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

//tools/oxc3_cli/hash.c

#include "tools/oxc3_cli/cli.h"
#include "types/container/buffer.h"
#include "types/math/vec4i.h"
#include "types/container/string.h"
#include "types/base/string_mut.h"
#include "platforms/platform.h"
#include "platforms/file.h"
#include "platforms/logx.h"
#include "tools/oxc3_cli/operations.h"
#include "types/base/constants.h"

Bool CLI_hash(CharString str, Bool isFile, EFormat format, Error *e_rr) {

	const Allocator *alloc = Platform_instance->alloc;
	const RefPtrType fileHandleType = FileHandle_makeType(alloc);

	Buffer buf = Buffer_createNull();
	CharString tmp = CharString_createNull();
	CharString tmpi = CharString_createNull();
	Bool s_uccess = true;

	if(!isFile)
		buf = CharString_bufferConst(str);

	else gotoIfError3(clean, File_read(&str, 100 * MS, 0, 0, &fileHandleType, &buf, e_rr));

	switch(format) {

		case EFormat_SHA256: {

			U32 output[8] = { 0 };
			Buffer_sha256(buf, output);

			//Stringify

			for (U8 i = 0; i < 8; ++i) {
				const CharStringCreateNumber number = (CharStringCreateNumber) {
					.v = output[i], .leadingZeros = 8, .allocator = alloc, .result = &tmpi
				};
				gotoIfError3(clean, CharString_createHex(&number, e_rr));
				gotoIfError3(clean, CharString_popFrontCount(&tmpi, 2, e_rr));
				gotoIfError3(clean, CharString_appendString(&tmp, &tmpi, alloc, e_rr));
				CharString_free(&tmpi, alloc);
			}

			break;
		}

		case EFormat_CRC32C: {
			const U32 output = Buffer_crc32c(buf);
			const CharStringCreateNumber number = (CharStringCreateNumber) {
				.v = output, .leadingZeros = 8, .allocator = alloc, .result = &tmp
			};
			gotoIfError3(clean, CharString_createHex(&number, e_rr));
			gotoIfError3(clean, CharString_popFrontCount(&tmp, 2, e_rr));
			break;
		}

		case EFormat_FNV1A64: {
			const U64 output = Buffer_fnv1a64(buf, Buffer_fnv1a64Offset);
			const CharStringCreateNumber number = (CharStringCreateNumber) {
				.v = output, .leadingZeros = 16, .allocator = alloc, .result = &tmp
			};
			gotoIfError3(clean, CharString_createHex(&number, e_rr));
			gotoIfError3(clean, CharString_popFrontCount(&tmp, 2, e_rr));
			break;
		}

		case EFormat_MD5: {

			const MD5Hash output = Buffer_md5(buf);

			for (U8 i = 0; i < 4; ++i) {
				const CharStringCreateNumber number = (CharStringCreateNumber) {
					.v = output.v[i], .leadingZeros = 8, .allocator = alloc, .result = &tmpi
				};
				gotoIfError3(clean, CharString_createHex(&number, e_rr));
				gotoIfError3(clean, CharString_popFrontCount(&tmpi, 2, e_rr));
				gotoIfError3(clean, CharString_appendString(&tmp, &tmpi, alloc, e_rr));
				CharString_free(&tmpi, alloc);
			}

			break;
		}

		default:
			Log_errorLnx("Unsupported format");
			goto clean;
	}

	Log_debugLnx("Hash: 0x%.*s: \t\t%.*s", CharString_length(tmp), tmp.ptr, (int) CharString_length(str), str.ptr);

clean:

	if(!s_uccess) {

		Log_errorLnx("Failed to convert hash to string!");

		if(e_rr)
			Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);
	}

	Buffer_free(&buf, alloc);
	CharString_free(&tmp, alloc);
	CharString_free(&tmpi, alloc);
	return s_uccess;
}

Bool CLI_hashAllTheFiles(const FileInfo *info, void *formatGeneric, const Allocator *alloc, Error *e_rr) {

	EFormat *format = (EFormat*) formatGeneric;

	(void) alloc;

	Bool s_uccess = true;

	if(info->type == EFileType_File)
		gotoIfError3(clean, CLI_hash(info->path, true, *format, e_rr));

clean:
	return s_uccess;
}

Bool CLI_hashFile(const ParsedArgs *args) {

	if(!args) return false;

	const Allocator *alloc = Platform_instance->alloc;

	CharString str = CharString_createNull();

	if(!ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &str, NULL))
		return false;

	//If it's a folder, then we have to find all files in it and hash them
	//Otherwise we just go to the file directly

	if (File_hasFolder(&str, alloc)) {
		EFormat format = args->format;        //Local non-const copy (File_foreach's userData is a void*)
		return File_foreach(&str, false, CLI_hashAllTheFiles, &format, true, alloc, NULL);
	}

	return CLI_hash(str, true, args->format, NULL);
}

Bool CLI_hashString(const ParsedArgs *args) {

	if(!args) return false;

	CharString str = CharString_createNull();

	if(!ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &str, NULL))
		return false;

	return CLI_hash(str, false, args->format, NULL);
}
