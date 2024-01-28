/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/buffer.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/file.h"
#include "platforms/log.h"
#include "operations.h"
#include "cli.h"

Bool CLI_hash(ParsedArgs args, Bool isFile) {

	CharString str = CharString_createNull();
	Buffer buf = Buffer_createNull();
	CharString tmp = CharString_createNull();
	CharString tmpi = CharString_createNull();
	Bool success = false;

	Error err = ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &str);

	if (err.genericError)
		goto clean;

	if(!isFile)
		buf = CharString_bufferConst(str);

	else _gotoIfError(clean, File_read(str, 1 * SECOND, &buf));

	switch(args.format) {

		case EFormat_SHA256: {

			U32 output[8] = { 0 };
			Buffer_sha256(buf, output);

			//Stringify

			for (U8 i = 0; i < 8; ++i) {
				_gotoIfError(clean, CharString_createHexx(output[i], 8, &tmpi));
				_gotoIfError(clean, CharString_popFrontCount(&tmpi, 2));
				_gotoIfError(clean, CharString_appendStringx(&tmp, tmpi));
				CharString_freex(&tmpi);
			}

			break;
		}

		case EFormat_CRC32C: {
			U32 output = Buffer_crc32c(buf);
			_gotoIfError(clean, CharString_createHexx(output, 8, &tmp));
			_gotoIfError(clean, CharString_popFrontCount(&tmp, 2));
			break;
		}

		default:
			Log_errorLnx("Unsupported format");
			goto clean;
	}

	Log_debugLnx("Hash: 0x%.*s", CharString_length(tmp), tmp.ptr);
	success = true;

clean:

	if(err.genericError) {
		Log_errorLnx("Failed to convert hash to string!");
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
	}

	Buffer_freex(&buf);
	CharString_freex(&tmp);
	CharString_freex(&tmpi);
	return success;
}

Bool CLI_hashFile(ParsedArgs args) {
	return CLI_hash(args, true);
}

Bool CLI_hashString(ParsedArgs args) {
	return CLI_hash(args, false);
}
