/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/ext/listx.h"
#include "types/container/buffer.h"
#include "types/base/time.h"
#include "platforms/file.h"
#include "platforms/log.h"
#include "platforms/ext/archivex.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/formatx.h"
#include "formats/oiCA/ca_file.h"
#include "cli.h"

#ifdef CLI_SHADER_COMPILER
	#include "shader_compiler/compiler.h"
#endif

typedef struct CAFileRecursion {
	Archive *archive;
	CharString root;
} CAFileRecursion;

Bool packageFile(FileInfo file, CAFileRecursion *caFile, Error *e_rr) {

	Bool s_uccess = true;
	CharString subPath = CharString_createNull();

	if(!CharString_cut(file.path, CharString_length(caFile->root), 0, &subPath))
		retError(clean, Error_invalidState(0, "packageFile()::file.path cut failed"))

	ArchiveEntry entry = (ArchiveEntry) {
		.path = subPath,
		.type = file.type
	};

	if (entry.type == EFileType_File)
		gotoIfError3(clean, File_read(file.path, 1 * SECOND, &entry.data, e_rr))

	if (file.type == EFileType_File) {

		if (
			CharString_endsWithStringSensitive(entry.path, CharString_createRefCStrConst(".hlsl"), 0) ||
			CharString_endsWithStringSensitive(entry.path, CharString_createRefCStrConst(".hlsli"), 0)
		)
			goto clean;

		//We have to detect file type and process it here to a custom type.
		//We don't have a custom file yet, so for now
		//this will just be identical to addFileToCAFile.

		gotoIfError3(clean, Archive_addFilex(caFile->archive, entry.path, &entry.data, 0, e_rr))
	}

	else gotoIfError3(clean, Archive_addDirectoryx(caFile->archive, entry.path, e_rr))

clean:
	Buffer_freex(&entry.data);
	return s_uccess;
}

Bool CLI_package(ParsedArgs args) {

	//Parse encryption key

	U32 encryptionKeyV[8] = { 0 };
	U32 *encryptionKey = NULL;			//Only if we have aes should encryption key be set.
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	Archive archive = (Archive) { 0 };
	CharString resolved = CharString_createNull();
	CAFile file = (CAFile) { 0 };
	Buffer res = Buffer_createNull();
	Bool isVirtual = false;

	ListCharString allFiles = (ListCharString) { 0 };
	ListCharString allShaderText = (ListCharString) { 0 };
	ListCharString allOutputs = (ListCharString) { 0 };
	ListU8 allCompileOutputs = (ListU8) { 0 };
	ListBuffer allBuffers = (ListBuffer) { 0 };

	Ns start = Time_now();

	if (args.parameters & EOperationHasParameter_AES) {

		CharString key = CharString_createNull();

		if (
			(ParsedArgs_getArg(args, EOperationHasParameter_AESShift, &key)).genericError ||
			!CharString_isHex(key)
		) {
			Log_errorLnx("Invalid parameter sent to -aes. Expecting key in hex (32 bytes)");
			return false;
		}

		U64 off = CharString_startsWithStringInsensitive(key, CharString_createRefCStrConst("0x"), 0) ? 2 : 0;

		if (CharString_length(key) - off != 64) {
			Log_errorLnx("Invalid parameter sent to -aes. Expecting key in hex (32 bytes)");
			return false;
		}

		for (U64 i = off; i + 1 < CharString_length(key); ++i) {

			U8 v0 = C8_hex(key.ptr[i]);
			U8 v1 = C8_hex(key.ptr[++i]);

			v0 = (v0 << 4) | v1;
			*((U8*)encryptionKeyV + ((i - off) >> 1)) = v0;
		}

		encryptionKey = encryptionKeyV;
	}

	CASettings settings = (CASettings) { .compressionType = EXXCompressionType_None };

	if(args.parameters & EOperationHasParameter_AES)
		settings.encryptionType = EXXEncryptionType_AES256GCM;

	//Copying encryption key

	if(settings.encryptionType)
		Buffer_copy(
			Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)),
			Buffer_createRef(encryptionKey, sizeof(settings.encryptionKey))
		);

	//Get input

	CharString input = (CharString) { 0 };
	gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &input))

	//Check if output is valid

	CharString output = (CharString) { 0 };
	gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &output))

	//Get compile settings

	#ifdef CLI_SHADER_COMPILER

		//Get compile types

		Bool multipleModes = false;
		U64 compileModeU64 = 0;
		gotoIfError3(clean, CLI_parseCompileTypes(args, &compileModeU64, &multipleModes))

		//Check thread count

		U64 threadCount = 0;
		gotoIfError3(clean, CLI_parseThreads(args, &threadCount, 1))

		//Additional includeDir

		CharString includeDir = (CharString) { 0 };

		if (args.parameters & EOperationHasParameter_IncludeDir)
			gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_IncludeDirShift, &includeDir))

		//Grab all files that need compilation

		gotoIfError3(clean, CLI_getCompileTargetsFromFile(
			input,
			ECompileType_Compile,
			compileModeU64,
			multipleModes,
			!(args.flags & EOperationFlags_Split),
			NULL,
			NULL,		//Don't write to output, write to Buffer[] instead
			&allFiles,
			&allShaderText,
			&allOutputs,
			&allCompileOutputs
		))

		//Grab info about extra detailed compiler warnings

		ECompilerWarning extraWarnings = CLI_getExtraWarnings(args);

	#endif

	//Make archive

	gotoIfError3(clean, Archive_createx(&archive, e_rr))
	gotoIfError3(clean, File_resolvex(input, &isVirtual, 0, &resolved, e_rr))

	gotoIfError2(clean, CharString_appendx(&resolved, '/'))

	CAFileRecursion caFileRecursion = (CAFileRecursion) {
		.archive = &archive,
		.root = resolved
	};

	gotoIfError3(clean, File_foreach(
		caFileRecursion.root,
		(FileCallback) packageFile,
		&caFileRecursion,
		true,
		e_rr
	))

	//Convert shaders

	#ifdef CLI_SHADER_COMPILER

		if(allFiles.length)
			gotoIfError3(clean, CLI_compileShaders(
				allFiles, allShaderText, allOutputs, allCompileOutputs,
				threadCount,
				args.flags & EOperationFlags_Debug,
				extraWarnings,
				args.flags & EOperationFlags_IgnoreEmptyFiles,
				ECompileType_Compile,
				includeDir,
				CharString_createNull(),
				&allBuffers,
				e_rr
			))

		for(U64 i = 0; i < allOutputs.length; ++i)
			if(Buffer_length(allBuffers.ptrNonConst[i]))
				gotoIfError3(clean, Archive_addFilex(&archive, allOutputs.ptr[i], &allBuffers.ptrNonConst[i], 0, e_rr))

	#endif

	//Convert to CAFile and write to file

	gotoIfError3(clean, CAFile_create(settings, &archive, &file, e_rr))
	gotoIfError3(clean, CAFile_writex(file, &res, e_rr))

	gotoIfError3(clean, File_add(output, EFileType_File, 1 * SECOND, true, e_rr))		//Ensure subdirs are created
	gotoIfError3(clean, File_write(res, output, 1 * SECOND, e_rr))

clean:

	F64 dt = (F64)(Time_now() - start) / SECOND;

	if(s_uccess)
		Log_debugLnx("-- Packaging %s success in %fs!", resolved.ptr, dt);

	else Log_errorLnx("-- Packaging %s failed in %fs!!", resolved.ptr, dt);

	Error_printx(err, ELogLevel_Error, ELogOptions_NewLine);

	ListBuffer_freeUnderlyingx(&allBuffers);
	ListCharString_freeUnderlyingx(&allFiles);
	ListCharString_freeUnderlyingx(&allShaderText);
	ListCharString_freeUnderlyingx(&allOutputs);
	ListU8_freex(&allCompileOutputs);

	Buffer_freex(&res);
	CAFile_freex(&file);
	Archive_freex(&archive);
	CharString_freex(&resolved);
	return s_uccess;
}
