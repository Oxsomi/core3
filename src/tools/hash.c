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

#include "cli.h"
#include "types/buffer.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/file.h"
#include "platforms/log.h"
#include "operations.h"

Bool CLI_hash(CharString str, Bool isFile, EFormat format, Error *e_rr) {

	Buffer buf = Buffer_createNull();
	CharString tmp = CharString_createNull();
	CharString tmpi = CharString_createNull();
	Bool s_uccess = true;

	if(!isFile)
		buf = CharString_bufferConst(str);

	else gotoIfError3(clean, File_read(str, 1 * SECOND, &buf, e_rr))

	switch(format) {

		case EFormat_SHA256: {

			U32 output[8] = { 0 };
			Buffer_sha256(buf, output);

			//Stringify

			for (U8 i = 0; i < 8; ++i) {
				gotoIfError2(clean, CharString_createHexx(output[i], 8, &tmpi))
				gotoIfError2(clean, CharString_popFrontCount(&tmpi, 2))
				gotoIfError2(clean, CharString_appendStringx(&tmp, tmpi))
				CharString_freex(&tmpi);
			}

			break;
		}

		case EFormat_CRC32C: {
			const U32 output = Buffer_crc32c(buf);
			gotoIfError2(clean, CharString_createHexx(output, 8, &tmp))
			gotoIfError2(clean, CharString_popFrontCount(&tmp, 2))
			break;
		}

		case EFormat_FNV1A64: {
			const U64 output = Buffer_fnv1a64(buf, Buffer_fnv1a64Offset);
			gotoIfError2(clean, CharString_createHexx(output, 16, &tmp))
			gotoIfError2(clean, CharString_popFrontCount(&tmp, 2))
			break;
		}

		case EFormat_MD5: {

			const I32x4 output = Buffer_md5(buf);

			for (U8 i = 0; i < 4; ++i) {
				gotoIfError2(clean, CharString_createHexx((U32)I32x4_get(output, i), 8, &tmpi))
				gotoIfError2(clean, CharString_popFrontCount(&tmpi, 2))
				gotoIfError2(clean, CharString_appendStringx(&tmp, tmpi))
				CharString_freex(&tmpi);
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
			Error_printx(*e_rr, ELogLevel_Error, ELogOptions_Default);
	}

	Buffer_freex(&buf);
	CharString_freex(&tmp);
	CharString_freex(&tmpi);
	return s_uccess;
}

Bool CLI_hashAllTheFiles(FileInfo info, EFormat *format, Error *e_rr) {

	Bool s_uccess = true;

	if(info.type == EFileType_File)
		gotoIfError3(clean, CLI_hash(info.path, true, *format, e_rr))

clean:
	return s_uccess;
}

Bool CLI_hashFile(ParsedArgs args) {

	CharString str = CharString_createNull();
	Error err = ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &str);

	if(err.genericError)
		return false;

	//If it's a folder, then we have to find all files in it and hash them
	//Otherwise we just go to the file directly

	if (File_hasFolder(str))
		return File_foreach(str, (FileCallback) CLI_hashAllTheFiles, &args.format, true, NULL);

	return CLI_hash(str, true, args.format, NULL);
}

Bool CLI_hashString(ParsedArgs args) {

	CharString str = CharString_createNull();
	Error err = ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &str);

	if(err.genericError)
		return false;

	return CLI_hash(str, false, args.format, NULL);
}
