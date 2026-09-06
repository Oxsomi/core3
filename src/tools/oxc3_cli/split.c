/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

//tools/oxc3_cli/split.c

#include "tools/oxc3_cli/cli.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "platforms/file.h"
#include "formats/oiSH/sh_file.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/container/string_helper.h"
#include "types/container/memory_stream.h"
#include "types/base/error.h"
#include "types/base/string_read_helper.h"
#include "types/base/time.h"
#include "types/base/constants.h"

//The suffix each half is written with, the same one the shader compiler gives --split.
//Both routes producing the same names is what lets a split file stand in for a compile that was never merged.

static const C8 *CLI_splitSuffixes[EGfxBinaryType_Count] = {
	".spv.oiSH",
	".dxil.oiSH"
};

//Which binary types -compile-output asked for.
//Absent, it is every type, and *explicitTypes stays false so a type the file doesn't carry is skipped rather
// than treated as the user asking for something that isn't there.

static Bool CLI_splitTypes(const ParsedArgs *args, U8 *mask, Bool *explicitTypes, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = Platform_instance->alloc;
	ListCharString splits = (ListCharString) { 0 };
	CharString arg = (CharString) { 0 };

	*mask = (U8)((1 << EGfxBinaryType_Count) - 1);
	*explicitTypes = false;

	if(!(args->parameters & EOperationHasParameter_ShaderOutputMode))
		goto clean;

	gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_ShaderOutputModeShift, &arg, e_rr));

	CharStringSplit split = (CharStringSplit) { .s = &arg, .allocator = alloc, .result = &splits };
	gotoIfError3(clean, CharString_splitSensitive(&split, ',', e_rr));

	*mask = 0;
	*explicitTypes = true;

	for (U64 i = 0; i < splits.length; ++i) {

		if (CharString_equalsCStringInsensitive(&splits.ptr[i], "all")) {
			*mask = (U8)((1 << EGfxBinaryType_Count) - 1);
			*explicitTypes = false;
			break;
		}

		U8 j = 0;

		for(; j < EGfxBinaryType_Count; ++j)
			if(CharString_equalsCStringInsensitive(&splits.ptr[i], EGfxBinaryType_names[j]))
				break;

		if(j == EGfxBinaryType_Count)
			retError(clean, Error_invalidParameter(
				0, 0, "CLI_fileSplit() expected -compile-output <spv/dxil/all> (or for example spv,dxil)"
			));

		*mask |= (U8)(1 << j);
	}

clean:
	ListCharString_free(&splits, alloc);        //Elements are refs into arg, free the list only
	return s_uccess;
}

Bool CLI_fileSplit(const ParsedArgs *args) {

	if(!args) return false;

	const Ns start = Time_now();

	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;
	Buffer virtualBuf = Buffer_createNull(), outBuf = Buffer_createNull();
	SHFile file = (SHFile) { 0 }, half = (SHFile) { 0 };
	StreamRef *readStream = NULL;
	MemoryStreamRef *writeStream = NULL;
	CharString outPath = CharString_createNull();
	U8 written = 0;

	const Allocator *alloc = Platform_instance->alloc;
	const RefPtrType fileHandleType = FileHandle_makeType(alloc);
	const RefPtrType memoryStreamType = MemoryStream_makeType(alloc);
	const RefPtrType fileStreamType = FileStream_makeType(alloc);

	if (args->format != EFormat_oiSH) {
		Log_errorLnx("CLI_fileSplit() doesn't support the specified format");
		s_uccess = false;
		goto clean;
	}

	if (args->parameters & EOperationHasParameter_Input2) {
		Log_errorLnx("CLI_fileSplit() failed, -input2 can't be used");
		s_uccess = false;
		goto clean;
	}

	CharString inputArg = CharString_createNull(), outputArg = CharString_createNull();

	gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &inputArg, e_rr));
	gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &outputArg, e_rr));

	U8 mask = 0;
	Bool explicitTypes = false;
	gotoIfError3(clean, CLI_splitTypes(args, &mask, &explicitTypes, e_rr));

	//-output names the pair rather than one file, so a trailing .oiSH comes off before the per type suffix goes on

	const CharString oiSHSuffix = CharString_createRefCStrConst(".oiSH");
	U64 baseLen = CharString_length(outputArg);

	if(CharString_endsWithStringInsensitive(&outputArg, &oiSHSuffix, 0))
		baseLen -= CharString_length(oiSHSuffix);

	if(!baseLen)
		retError(clean, Error_invalidParameter(0, 0, "CLI_fileSplit()::-output has no name left once .oiSH comes off"));

	//Read the oiSH that is being taken apart.
	//File_openStream doesn't work on virtual files, so those are materialized and wrapped in a memory stream
	// while a physical one is read off disk as SHFile_read walks it.

	CLI_ensureVirtualLoaded(&inputArg);        //A "//section/..." path needs its section loaded before File_* resolves it

	if (File_isVirtual(inputArg)) {

		if (!File_read(&inputArg, 100 * MS, 0, 0, &fileHandleType, &virtualBuf, e_rr)) {
			Log_errorLnx("CLI_fileSplit() missing input");
			goto clean;
		}

		if(!MemoryStream_createFromBuffer(&virtualBuf, EMemoryStreamFlags_None, &memoryStreamType, &readStream, e_rr))
			goto clean;
	}

	else if (!File_openStream(
		&inputArg, 100 * MS, EFileOpenType_Read, false, &fileHandleType, &fileStreamType, &readStream, e_rr
	)) {
		Log_errorLnx("CLI_fileSplit() missing input");
		goto clean;
	}

	U64 readOff = 0;

	if(!SHFile_read(readStream, &readOff, false, alloc, &file, e_rr)) {
		Log_errorLnx("CLI_fileSplit() input couldn't be parsed as an oiSH");
		goto clean;
	}

	for (U8 i = 0; i < EGfxBinaryType_Count; ++i) {

		if(!((mask >> i) & 1))
			continue;

		//A type the file doesn't carry is refused when it was named and skipped when it wasn't.
		//That way splitting a SPIRV only oiSH without arguments writes the one half it has, rather than
		// failing over the one it never could have written.
		//A reflection only oiSH carries no code to go by and splits per register instead, so both halves
		// are always there to write.

		Bool hasType = (file.flags & ESHSettingsFlags_ReflectionOnly) != 0;

		for(U64 j = 0; j < file.binaries.length && !hasType; ++j)
			hasType = Buffer_length(file.binaries.ptr[j].binaries[i]) != 0;

		if (!hasType) {

			if(explicitTypes)
				retError(clean, Error_invalidState(
					0, "CLI_fileSplit() input carries no binary of a requested -compile-output type"
				));

			continue;
		}

		gotoIfError3(clean, SHFile_split(&file, (EGfxBinaryType) i, alloc, &half, e_rr));

		//Serialized into memory and written in one go, rather than straight into a file stream.
		//SHFile_write CRCs the body by reading back what it just wrote and patches the hash into the header
		// behind itself, so its target has to be readable, writable and seekable.
		//Opening a file that way is EFileOpenType_ReadWrite, and only EFileOpenType_Write truncates, on every
		// platform (O_RDWR|O_CREAT against O_WRONLY|O_CREAT|O_TRUNC, OPEN_ALWAYS against CREATE_ALWAYS), so a
		// smaller oiSH written over a bigger one would leave the tail of the old file behind it.
		//There is no truncate in the File api to pair with it, only File_remove, and removing the previous
		// output before knowing this one serializes is worse than holding a few KiB.

		U64 writeOff = 0;
		gotoIfError3(clean, MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &memoryStreamType, &writeStream, e_rr));
		gotoIfError3(clean, SHFile_write((StreamRef*)writeStream, &writeOff, &half, alloc, e_rr));
		gotoIfError3(clean, MemoryStream_move(&writeStream, &outBuf, e_rr));

		gotoIfError3(clean, CharString_format(
			alloc, &outPath, e_rr, "%.*s%s", (int) baseLen, outputArg.ptr, CLI_splitSuffixes[i]
		));

		gotoIfError3(clean, File_write(&outBuf, &outPath, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr));

		Log_debugLnx(
			"-- Split: %s -> %.*s (%"PRIu64" entrypoint(s), %"PRIu64" binary/binaries)",
			EGfxBinaryType_names[i],
			(int) CharString_length(outPath), outPath.ptr,
			half.entries.length, half.binaries.length
		);

		CharString_free(&outPath, alloc);
		Buffer_free(&outBuf, alloc);
		SHFile_free(&half, alloc);
		++written;
	}

	if(!written)
		retError(clean, Error_invalidState(0, "CLI_fileSplit() input carries no compiled binary at all"));

	Log_debugLnx("Split oiSH into %"PRIu8" file(s) in %"PRIu64"ms", written, (Time_now() - start + MS - 1) / MS);

clean:

	RefPtr_dec(&readStream);
	RefPtr_dec(&writeStream);

	SHFile_free(&half, alloc);
	SHFile_free(&file, alloc);
	CharString_free(&outPath, alloc);
	Buffer_free(&outBuf, alloc);
	Buffer_free(&virtualBuf, alloc);

	Error_print(alloc, &err, ELogLevel_Error, ELogOptions_NewLine);
	return s_uccess && !err.genericError;
}
