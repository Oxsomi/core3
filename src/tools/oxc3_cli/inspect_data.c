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

//tools/oxc3_cli/inspect_data.c

#include "types/container/list_basic_types.h"
#include "types/base/c8.h"
#include "types/base/error.h"
#include "types/base/mathi.h"
#include "types/base/string_read.h"
#include "types/base/string_read_helper.h"
#include "types/math/vec4i.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/base/string_mut.h"
#include "types/container/string_unicode.h"
#include "types/container/memory_stream.h"
#include "types/container/encryption_stream.h"
#include "types/container/ref_ptr.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_headers.h"
#include "formats/oiCA/ca_lookup.h"
#include "formats/oiCA/ca_props.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_headers.h"
#include "formats/oiSH/sh_file.h"
#include "formats/oiSH/sh_headers.h"
#include "formats/oiSB/sb_file.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "tools/oxc3_cli/cli.h"
#include "types/base/constants.h"

#ifdef CLI_SHADER_COMPILER
	#include "shader_compiler/compiler.h"
#endif

//Printing an entry

Bool collectArchiveEntries(const FileInfo *info, ListCharString *arg, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString tmp = CharString_createNull();

	gotoIfError3(clean, CharString_createCopy(info->path, alloc, &tmp, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(arg, tmp, alloc, e_rr));

	tmp = CharString_createNull();        //Belongs to list now

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

//Printing an entry

typedef struct OutputFolderToDisk {
	CharString base, output;
	const CAFile *sourceArchive;
} OutputFolderToDisk;

Bool writeToDisk(const FileInfo *info, OutputFolderToDisk *output, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString subDir = CharString_createNull();
	CharString tmp = CharString_createNull();

	RefPtrType fileHandleType = FileHandle_makeType(alloc);

	const U64 start = CharString_length(output->base) == 1 && output->base.ptr[0] == '.' ? 0 : CharString_length(output->base);

	if (!CharString_cut(&info->path, start, 0, &subDir))
		retError(clean, Error_invalidOperation(0, "writeToDisk()::info.path cut failed"));

	gotoIfError3(clean, CharString_createCopy(output->output, alloc, &tmp, e_rr));
	gotoIfError3(clean, CharString_appendString(&tmp, &subDir, alloc, e_rr));

	if (info->type == EFileType_File) {

		Bool isValid = false;
		Buffer data = CAFile_getDataConst(output->sourceArchive, CAFile_resolve(output->sourceArchive, info->path), &isValid);

		if (!isValid)
			retError(clean, Error_invalidState(0, "writeToDisk()::info.path file data lookup failed"));

		gotoIfError3(clean, File_write(&data, &tmp, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr));
	}

	else gotoIfError3(clean, File_add(&tmp, EFileType_Folder, false, alloc, e_rr));

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

//Showing the entire file or a part to disk or to log

Bool CLI_showFile(const ParsedArgs *args, Buffer b, U64 start, U64 length, Bool isUTF8, Bool showEntireFile) {

	if(!args) return false;

	//Validate offset

	if (start + (!!Buffer_length(b)) > Buffer_length(b)) {
		Log_debugLnx("Section out of bounds.");
		return false;
	}

	//Output it to a folder on disk was requested

	Error *e_rr = NULL;
	CharString tmp = CharString_createNull();
	CharString tmp1 = CharString_createNull();
	Bool s_uccess = false;

	const Allocator *alloc = Platform_instance->alloc;
	RefPtrType fileHandleType = FileHandle_makeType(alloc);

	if (args->parameters & EOperationHasParameter_Output) {

		if(!length)
			length = Buffer_length(b) - start;

		if (start + length > Buffer_length(b)) {
			Log_debugLnx("Section out of bounds.");
			goto clean;
		}

		CharString out = CharString_createNull();

		if (ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &out).genericError) {
			Log_errorLnx("Invalid argument -output <string>.");
			goto clean;
		}

		Buffer subBuffer = Buffer_createRefConst(b.ptr + start, length);
		gotoIfError3(clean, File_write(&subBuffer, &out, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr));
	}

	//More info about a single entry

	else {

		if (!Buffer_length(b)) {
			Log_debugLnx("Section is empty.");
			goto clean;
		}

		Log_debugLnx("Section has %"PRIu64" bytes.", Buffer_length(b));

		//Get length

		U64 max = 32 * 32;

		if(!length)
			length = showEntireFile ? Buffer_length(b) - start : U64_min(max, Buffer_length(b) - start);

		else length = U64_min(max * 2, length);

		if (start + length > Buffer_length(b)) {
			Log_debugLnx("Section out of bounds.");
			goto clean;
		}

		//Show what offset is being displayed

		Log_debugLnx("Showing offset #%"PRIx64" with size %"PRIu64, start, length);
		Log_debugLnx(isUTF8 ? "File contents: (utf8)" : "File contents: (binary)");

		//UTF8 can be directly output to log

		if (isUTF8) {
			tmp = CharString_createRefSizedConst((const C8*)b.ptr + start, length, false);
			Log_debugLnx("%.*s", CharString_length(tmp), tmp.ptr);
			tmp = CharString_createNull();
		}

		//Binary needs to be formatted first

		else {

			const CharString newLine = CharString_newLine();

			for (U64 i = start, j = i + length, k = 0; i < j; ++i, ++k) {

				gotoIfError3(clean, CharString_createHex(&(CharStringCreateNumber) {
					.v = b.ptr[i], .leadingZeros = 2, .allocator = alloc, .result = &tmp1
				}, e_rr));
				gotoIfError3(clean, CharString_popFrontCount(&tmp1, 2, e_rr));
				gotoIfError3(clean, CharString_appendString(&tmp, &tmp1, alloc, e_rr));
				gotoIfError3(clean, CharString_append(&tmp, ' ', alloc, e_rr));

				if (!((k + 1) & 31))
					gotoIfError3(clean, CharString_appendString(&tmp, &newLine, alloc, e_rr));

				CharString_free(&tmp1, alloc);
			}

			Log_debugLnx("%.*s", CharString_length(tmp), tmp.ptr);
			CharString_free(&tmp, alloc);
		}
	}

	s_uccess = true;

clean:
	CharString_free(&tmp1, alloc);
	CharString_free(&tmp, alloc);
	return s_uccess;
}

//Storing file or folder on disk

Bool CLI_storeFileOrFolder(const ParsedArgs *args, const CAFile *a, CAHandle handle, Bool *madeFile, U64 start, U64 len) {

	if(!args) return false;

	CharString tmp = CharString_createNull();
	FileInfo info = (FileInfo) { 0 };
	Bool s_uccess = false;
	Error *e_rr = NULL;

	const Allocator *alloc = Platform_instance->alloc;
	RefPtrType fileHandleType = FileHandle_makeType(alloc);
	(void) fileHandleType;

	//Save folder

	if (CAHandle_isFolder(handle)) {

		if(start || (len && len != CAFile_fileObjectCount(a, CAHandle_Root, true)))
			Log_warnLnx("Folder output to disk ignores offset and/or count.");

		CharString out = CharString_createNull();

		if (ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &out).genericError) {
			Log_errorLnx("Invalid argument -output <string>.");
			goto clean;
		}

		gotoIfError3(clean, File_add(&out, EFileType_Folder, true, alloc, e_rr));
		*madeFile = true;

		gotoIfError3(clean, CharString_createCopy(out, alloc, &tmp, e_rr));
		gotoIfError3(clean, CharString_append(&tmp, '/', alloc, e_rr));

		gotoIfError3(clean, CAFile_getInfo(a, handle, &info, alloc, e_rr));

		OutputFolderToDisk output = (OutputFolderToDisk) {
			.base = info.path,
			.output = tmp,
			.sourceArchive = a
		};

		gotoIfError3(clean, CAFile_foreach(
			a,
			handle,
			(FileCallback) writeToDisk,
			&output,
			true,
			alloc,
			e_rr
		));

		CharString_free(&tmp, alloc);
		madeFile = false;            //We successfully wrote, so keep it from deleting the folder
		s_uccess = true;

		goto clean;
	}

	//Save file

	else {

		Bool isValid = false;
		Buffer data = CAFile_getDataConst(a, handle, &isValid);

		CLI_showFile(args, data, start, len, false, false);
		s_uccess = true;
		goto clean;
	}

clean:
	FileInfo_free(&info, alloc);
	CharString_free(&tmp, alloc);
	return s_uccess;
}

//Handle inspection of individual data.
//Also handles info about the file in general.

Bool CLI_inspectData(const ParsedArgs *args) {

	if(!args) return false;

	Buffer buf = Buffer_createNull();
	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = false;

	CharString path = CharString_createNull();
	CharString tmp = CharString_createNull();
	CharString tmp1 = CharString_createNull();

	const Allocator *alloc = Platform_instance->alloc;
	RefPtrType fileHandleType = FileHandle_makeType(alloc);
	RefPtrType memoryStreamType = MemoryStream_makeType(alloc);
	RefPtrType encStreamType = EncryptionStream_makeType(alloc);
	StreamRef *stream = NULL;

	//Get file

	if ((err = ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &path)).genericError) {
		Log_errorLnx("Invalid argument -input <string>.");
		goto clean;
	}

	if (!File_read(&path, 100 * MS, 0, 0, &fileHandleType, &buf, e_rr)) {
		Log_errorLnx("Invalid file path.");
		goto clean;
	}

	if (Buffer_length(buf) < 4) {
		Log_errorLnx("File has to start with magic number.");
		goto clean;
	}

	//Parse entry if available

	CharString entry = CharString_createNull();

	if (args->parameters & EOperationHasParameter_Entry)
		if ((err = ParsedArgs_getArg(args, EOperationHasParameter_EntryShift, &entry)).genericError) {
			Log_errorLnx("Invalid argument -entry <string or uint>.");
			goto clean;
		}

	//Parse start if available

	CharString starts = CharString_createNull();
	U64 start = 0;

	if (args->parameters & EOperationHasParameter_StartOffset)
		if (
			(err = ParsedArgs_getArg(args, EOperationHasParameter_StartOffsetShift, &starts)).genericError ||
			!CharString_parseU64(starts, &start) ||
			(start >> 32)
		) {
			Log_errorLnx("Invalid argument -start <uint>.");
			goto clean;
		}

	//Parse end if available

	CharString lengths = CharString_createNull();
	U64 length = 0;

	if (args->parameters & EOperationHasParameter_Length)
		if (
			(err = ParsedArgs_getArg(args, EOperationHasParameter_LengthShift, &lengths)).genericError ||
			!CharString_parseU64(lengths, &length) ||
			(length >> 32)
		) {
			Log_errorLnx("Invalid argument -length <uint>.");
			goto clean;
		}

	//Parse encryption key

	U32 encryptionKeyV[8] = { 0 };
	U32 *encryptionKey = NULL;            //Only if we have aes should encryption key be set.

	if (args->parameters & EOperationHasParameter_AES) {

		CharString key = CharString_createNull();

		if (
			(ParsedArgs_getArg(args, EOperationHasParameter_AESShift, &key)).genericError ||
			!CharString_isHex(key)
		) {
			Log_errorLnx("Invalid parameter sent to -aes. Expecting key in hex (32 bytes)");
			return false;
		}

		const CharString prefix0x = CharString_createRefCStrConst("0x");
		U64 off = CharString_startsWithStringInsensitive(&key, &prefix0x, 0) ? 2 : 0;

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

	U32 magic = *(const U32*)buf.ptr;

	if(args->flags & (EOperationFlags_Bin | EOperationFlags_Includes) && magic != SHHeader_MAGIC) {
		Log_errorLnx("--bin and --includes flag can only be used with an oiSH file");
		return false;
	}

	ESHBinaryType binaryType = ESHBinaryType_Count;

	if (args->parameters & EOperationHasParameter_ShaderOutputMode) {

		if(magic != SHHeader_MAGIC) {
			Log_errorLnx("-compile-output argument can only be used with an oiSH file");
			return false;
		}

		CharString shaderOutputMode = CharString_createNull();
		if ((err = ParsedArgs_getArg(args, EOperationHasParameter_ShaderOutputModeShift, &shaderOutputMode)).genericError) {
			Log_errorLnx("Missing argument -compile-output");
			goto clean;
		}

		if(CharString_equalsCStringInsensitive(&shaderOutputMode, "DXIL"))
			binaryType = ESHBinaryType_DXIL;

		else if(CharString_equalsCStringInsensitive(&shaderOutputMode, "SPV"))
			binaryType = ESHBinaryType_SPIRV;

		else {
			Log_errorLnx("Invalid argument. Expected: -compile-output <spv/dxil>.");
			goto clean;
		}
	}

	switch (magic) {

		//oiCA header

		case CAHeader_MAGIC: {

			CAFile file = (CAFile) { 0 };
			ListCharString strings = { 0 };
			U64 baseCount = 0;

			Bool madeFile = false;
			CharString out = CharString_createNull();

			gotoIfError3(cleanCa, MemoryStream_createFromBufferRegion(
				Buffer_createRefFromBuffer(buf, true), 0, Buffer_length(buf),
				EMemoryStreamFlags_None, &memoryStreamType, &stream, e_rr
			));

			gotoIfError3(cleanCa, CAFile_read(stream, &encStreamType, 0, encryptionKey, alloc, &file, e_rr));

			if(encryptionKey)
				Buffer_clearAllSecure(Buffer_createRef(encryptionKeyV, sizeof(encryptionKeyV)));

			//Specific entry was requested

			if (args->parameters & EOperationHasParameter_Entry) {

				//Get handle of entry (by path, otherwise fall back to a numeric index into root's objects)

				CAHandle handle = CAFile_resolve(&file, entry);

				if (handle == CAHandle_Invalid) {

					U64 index = 0;

					if (!CharString_parseU64(entry, &index)) {
						Log_errorLnx("Invalid argument -entry <uint> or <valid path> expected.");
						goto cleanCa;
					}

					handle = CAFile_fileObjectAt(&file, CAHandle_Root, index);

					if (handle == CAHandle_Invalid) {
						Log_errorLnx("Index out of bounds, max is %"PRIu64".", CAFile_fileObjectCount(&file, CAHandle_Root, false));
						goto cleanCa;
					}
				}

				//Output it to a folder on disk was requested

				if (args->parameters & EOperationHasParameter_Output) {
					CLI_storeFileOrFolder(args, &file, handle, &madeFile, start, length);
					goto cleanCa;
				}

				//Want to output to log

				else {

					//Simply print the archive folder

					if (CAHandle_isFolder(handle)) {

						CharString entryPath = CharString_createNull();
						gotoIfError3(cleanCa, CAFile_getFullName(&file, handle, alloc, &entryPath, e_rr));

						baseCount = CharString_countAllSensitive(&entryPath, '/', 0) + 1;
						CharString_free(&entryPath, alloc);

						gotoIfError3(cleanCa, CAFile_foreach(
							&file,
							handle,
							(FileCallback) collectArchiveEntries,
							&strings,
							true,
							alloc,
							e_rr
						));
					}

					//Print the subsection of the file

					else {

						Bool isValid = false;
						Buffer data = CAFile_getDataConst(&file, handle, &isValid);

						Bool isUTF8 = CharString_isValidUTF8(
							CharString_createRefSizedConst((const C8*) data.ptr, Buffer_length(data), false)
						);

						CharString entryPath = CharString_createNull();
						gotoIfError3(cleanCa, CAFile_getFullName(&file, handle, alloc, &entryPath, e_rr));
						Log_debugLnx("%.*s", CharString_length(entryPath), entryPath.ptr);
						CharString_free(&entryPath, alloc);

						CLI_showFile(args, data, start, length, isUTF8, false);
						goto cleanCa;
					}

				}
			}

			//General info was requested

			else {

				if (args->parameters & EOperationHasParameter_Output) {
					CLI_storeFileOrFolder(args, &file, CAHandle_Root, &madeFile, start, length);
					goto cleanCa;
				}

				gotoIfError3(cleanCa, CAFile_foreach(
					&file,
					CAHandle_Root,
					(FileCallback) collectArchiveEntries,
					&strings,
					true,
					alloc,
					e_rr
				));
			}

			//Sort to ensure the subdirectories are correct

			if(!ListCharString_sortInsensitive(strings))
				retError(cleanCa, Error_invalidOperation(0, "CLI_inspectData() sort strings (oiCA) failed"));

			//Process all and print

			if(!length && start < strings.length)
				length = U64_min(64, strings.length - start);

			U64 end = start + length;

			Log_debugLnx(
				"Showing offset #%"PRIx64" with count %"PRIu64" in selected folder (File contains %"PRIu64" entries)",
				start, length, CAFile_fileObjectCount(&file, CAHandle_Root, true)
			);

			for(U64 i = start; i < end && i < strings.length; ++i) {

				CharString pathi = strings.ptr[i];
				U64 parentCount = CharString_countAllSensitive(&pathi, '/', 0);

				CAHandle v = CAFile_resolve(&file, pathi);

				//000: self
				//001:   child (indented by 2)

				if (v == CAHandle_Invalid)
					retError(cleanCa, Error_notFound(0, 0, "CLI_inspectData() couldn't find archive entry (oiCA)"));

				gotoIfError3(cleanCa, CharString_createDec(&(CharStringCreateNumber) {
					.v = CAHandle_getId(v), .leadingZeros = 3, .allocator = alloc, .result = &tmp
				}, e_rr));
				gotoIfError3(cleanCa, CharString_create(' ', 2 * (parentCount - baseCount), alloc, &tmp1, e_rr));
				gotoIfError3(cleanCa, CharString_append(&tmp, ':', alloc, e_rr));
				gotoIfError3(cleanCa, CharString_append(&tmp, ' ', alloc, e_rr));
				gotoIfError3(cleanCa, CharString_appendString(&tmp, &tmp1, alloc, e_rr));
				CharString_free(&tmp1, alloc);

				CharString sub = CharString_createNull();
				if(!CharString_cutBeforeLastSensitive(&pathi, '/', &sub))
					sub = CharString_createRefSizedConst(pathi.ptr, CharString_length(pathi), false);

				gotoIfError3(cleanCa, CharString_appendString(&tmp, &sub, alloc, e_rr));

				//Log and free temp

				Log_debugLnx("%.*s", CharString_length(tmp), tmp.ptr);
				CharString_free(&tmp, alloc);
			}

			if(!strings.length)
				Log_debugLnx("Folder is empty.");

		cleanCa:

			if(madeFile)
				File_remove(&out, 1 * SECOND, alloc, NULL);

			CAFile_free(&file, alloc);
			ListCharString_freeUnderlying(&strings, alloc);
			CharString_free(&tmp, alloc);

			RefPtr_dec(&stream);

			if(err.genericError)
				goto clean;

			break;
		}

		//oiDL header

		case DLHeader_MAGIC: {

			DLFile file = (DLFile) { 0 };

			gotoIfError3(cleanDl, MemoryStream_createFromBufferRegion(
				Buffer_createRefFromBuffer(buf, true), 0, Buffer_length(buf),
				EMemoryStreamFlags_None, &memoryStreamType, &stream, e_rr
			));

			U64 dlOff = 0;
			gotoIfError3(cleanDl, DLFile_read(
				stream, &dlOff, encryptionKey, I32x4_zero(), false, false, alloc, &encStreamType, &file, e_rr
			));

			if(encryptionKey)
				Buffer_clearAllSecure(Buffer_createRef(encryptionKeyV, sizeof(encryptionKeyV)));

			U64 end = 0;

			if (!(args->parameters & EOperationHasParameter_Entry)) {

				if(!length && start < DLFile_entryCount(&file))
					length = U64_min(64, DLFile_entryCount(&file) - start);

				end = start + length;
			}

			if (args->parameters & EOperationHasParameter_Entry) {

				//Grab entry

				U64 entryI = 0;

				if (!CharString_parseU64(entry, &entryI)) {
					Log_errorLnx("Invalid argument -entry <uint> expected.");
					goto cleanDl;
				}

				if (entryI >= DLFile_entryCount(&file)) {
					Log_errorLnx("Index out of bounds, max is %"PRIu64, DLFile_entryCount(&file));
					goto cleanDl;
				}

				Bool isAscii = file.settings.dataType == EDLDataType_String;
				Buffer b =
					isAscii ? CharString_bufferConst(file.entryStrings.ptr[entryI]) :
					file.entryBuffers.ptr[entryI];

				if(!CLI_showFile(args, b, start, length, isAscii, false))
					goto cleanDl;
			}

			else {

				Log_debugLnx("oiDL Entries:");

				for (U64 i = start; i < end && i < DLFile_entryCount(&file); ++i) {

					U64 entrySize =
						file.settings.dataType == EDLDataType_String ? CharString_length(file.entryStrings.ptr[i]) :
						Buffer_length(file.entryBuffers.ptr[i]);

					const CharString colLen = CharString_createRefCStrConst(": length = ");

					gotoIfError3(cleanDl, CharString_createDec(&(CharStringCreateNumber) {
						.v = i, .leadingZeros = 3, .allocator = alloc, .result = &tmp
					}, e_rr));
					gotoIfError3(cleanDl, CharString_appendString(&tmp, &colLen, alloc, e_rr));

					gotoIfError3(cleanDl, CharString_createDec(&(CharStringCreateNumber) {
						.v = entrySize, .leadingZeros = 0, .allocator = alloc, .result = &tmp1
					}, e_rr));
					gotoIfError3(cleanDl, CharString_appendString(&tmp, &tmp1, alloc, e_rr));

					Log_debugLnx("%s", tmp.ptr);
					CharString_free(&tmp, alloc);
					CharString_free(&tmp1, alloc);
				}
			}

		cleanDl:

			CharString_free(&tmp, alloc);
			CharString_free(&tmp1, alloc);
			DLFile_free(&file, alloc);

			RefPtr_dec(&stream);

			if(err.genericError)
				goto clean;

			break;
		}

		//oiSH header

		case SHHeader_MAGIC: {

			if(encryptionKey)
				retError(clean, Error_invalidState(0, "CLI_inspectData() oiSH doesn't have aes support!"));

			SHFile file = (SHFile) { 0 };

			#ifdef CLI_SHADER_COMPILER
				Compiler comp = (Compiler) { 0 };
			#endif

			gotoIfError3(cleanSh, MemoryStream_createFromBufferRegion(
				Buffer_createRefFromBuffer(buf, true), 0, Buffer_length(buf),
				EMemoryStreamFlags_None, &memoryStreamType, &stream, e_rr
			));

			U64 shOff = 0;
			gotoIfError3(cleanSh, SHFile_read(stream, &shOff, false, alloc, &file, e_rr));

			Bool binaryMode = args->flags & EOperationFlags_Bin;
			Bool includesMode = args->flags & EOperationFlags_Includes;
			Bool isVerbose = args->flags & EOperationFlags_Verbose;

			if (binaryMode && includesMode) {
				Log_errorLnx("oiSH file data can't use --bin and --includes at the same time");
				goto cleanSh;
			}

			if((args->parameters & EOperationHasParameter_Output) && (
				binaryType == ESHBinaryType_Count ||
				!binaryMode ||
				!(args->parameters & EOperationHasParameter_Entry)
			)) {
				Log_errorLnx("oiSH file data can't use --output if -entry, --bin and -compile-output aren't defined");
				goto cleanSh;
			}

			U64 count = includesMode ? file.includes.length : (binaryMode ? file.binaries.length : file.entries.length);
			U64 end = 0;

			if (!(args->parameters & EOperationHasParameter_Entry)) {

				if(!length && start < count)
					length = U64_min(64, count - start);

				end = start + length;
			}

			if (args->parameters & EOperationHasParameter_Entry) {

				//Grab entry

				U64 entryI = 0;

				if (includesMode) {
					Log_errorLnx("oiSH file data includes mode doesn't support -entry, since all data is already displayed");
					goto cleanSh;
				}

				if (!CharString_parseU64(entry, &entryI)) {
					Log_errorLnx("Invalid argument -entry <uint> expected.");
					goto cleanSh;
				}

				if (entryI >= count) {
					Log_errorLnx("Index out of bounds, max is %"PRIu64, count);
					goto cleanSh;
				}

				if(!binaryMode)
					SHEntry_print(&file.entries.ptr[entryI], true, alloc);

				else {

					//Compile mode was selected

					if (binaryType != ESHBinaryType_Count) {

						Buffer binary = file.binaries.ptr[entryI].binaries[binaryType];

						if (!Buffer_length(binary)) {
							Log_errorLnx("%s binary is missing at index %"PRIu64, ESHBinaryType_names[binaryType], entryI);
							goto cleanSh;
						}

						//Show as disassembly (DXIL or SPIRV disassembly) unless not available

						#ifdef CLI_SHADER_COMPILER

							if(!(args->parameters & EOperationHasParameter_Output)) {

								gotoIfError3(cleanSh, Compiler_create(alloc, &comp, e_rr));

								if (!Compiler_disassemble(&comp, binaryType, binary, alloc, &tmp, e_rr)) {
									Log_errorLnx("%s disassembly failed at index %"PRIu64, ESHBinaryType_names[binaryType], entryI);
									goto cleanSh;
								}

								if(!CLI_showFile(args, CharString_bufferConst(tmp), start, length, true, true))
									goto cleanSh;

								goto cleanSh;
							}

						#endif

						if(!CLI_showFile(args, binary, start, length, false, false))
							goto cleanSh;
					}

					else SHBinaryInfo_print(&file.binaries.ptr[entryI], true, alloc);
				}
			}

			else {

				if (includesMode) {

					Log_debugLnx("oiSH includes:");

					for (U64 i = start; i < end && i < count; ++i) {

						SHInclude inc = file.includes.ptr[i];

						Log_debugLnx(
							"Include %"PRIu64" (%.*s) with CRC32C %"PRIX32,
							i,
							(int) CharString_length(inc.relativePath),
							inc.relativePath.ptr,
							inc.crc32c
						);
					}
				}

				else if (binaryMode) {

					Log_debugLnx("oiSH binaries:");

					for (U64 i = start; i < end && i < count; ++i)
						SHBinaryInfo_print(&file.binaries.ptr[i], isVerbose, alloc);
				}

				else {

					if (!(args->parameters & (EOperationHasParameter_Entry | EOperationHasParameter_StartOffset)))
						SHFile_print(&file, isVerbose, alloc);

					else {

						Log_debugLnx("oiSH entries:");

						for (U64 i = start; i < end && i < count; ++i) {

							SHEntry shEntry = file.entries.ptr[i];
							const C8 *name = SHEntry_stageName(&shEntry);

							Log_debugLnx(
								"Entry %"PRIu64" (%s): %.*s", i, name, (int) CharString_length(shEntry.name), shEntry.name.ptr
							);
						}
					}
				}
			}

		cleanSh:

			SHFile_free(&file, alloc);

			#ifdef CLI_SHADER_COMPILER
				Compiler_free(&comp, alloc);
			#endif

			RefPtr_dec(&stream);

			if(!s_uccess)
				goto clean;

			break;
		}

		//oiSB file

		case SBHeader_MAGIC: {

			if(encryptionKey)
				retError(clean, Error_invalidState(0, "CLI_inspectData() oiSH doesn't have aes support!"));

			SBFile file = (SBFile) { 0 };

			gotoIfError3(cleanSb, MemoryStream_createFromBufferRegion(
				Buffer_createRefFromBuffer(buf, true), 0, Buffer_length(buf),
				EMemoryStreamFlags_None, &memoryStreamType, &stream, e_rr
			));

			U64 sbOff = 0;
			gotoIfError3(cleanSb, SBFile_read(stream, &sbOff, false, alloc, &file, e_rr));

			SBFile_print(&file, 0, U16_MAX, true, alloc);        //TODO: parent

		cleanSb:

			SBFile_free(&file, alloc);

			RefPtr_dec(&stream);

			if(!s_uccess)
				goto clean;

			break;
		}

		//Invalid

		default:
			Log_errorLnx("File had unrecognized magic number.");
			goto clean;
	}

	s_uccess = true;

clean:

	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_NewLine);

	RefPtr_dec(&stream);
	CharString_free(&tmp, alloc);
	Buffer_free(&buf, alloc);
	return s_uccess;
}
