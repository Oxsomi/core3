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

#include "types/time.h"
#include "types/file.h"
#include "platforms/log.h"
#include "platforms/ext/errorx.h"
#include "platforms/file.h"
#include "cli.h"

Bool CLI_convert(ParsedArgs args, Bool isTo) {

	Ns start = Time_now();

	//Prepare for convert to/from

	Format f = Format_values[args.format];
	U64 offset = 0;

	CharString inputArg = CharString_createNull();
	FileInfo info = (FileInfo) { 0 };

	Error err;
	gotoIfError(clean, ListCharString_get(args.args, offset++, &inputArg))

	//Check if output is valid

	CharString outputArg = CharString_createNull();

	gotoIfError(clean, ListCharString_get(args.args, offset++, &outputArg))

	//TODO: Support multiple files

	//Check if input is valid

	//Check if input file and file type are valid

	gotoIfError(clean, File_getInfox(inputArg, &info))

	if (!(f.flags & EFormatFlags_SupportFiles) && info.type == EFileType_File)
		gotoIfError(clean, Error_invalidState(0, "CLI_convert() Invalid file passed to convertTo. Only accepting folders."))

	if (!(f.flags & EFormatFlags_SupportFolders) && info.type == EFileType_Folder)
		gotoIfError(clean, Error_invalidState(1, "CLI_convert() Invalid file passed to convertTo. Only accepting files."))

	//Parse encryption key

	U32 encryptionKeyV[8] = { 0 };
	U32 *encryptionKey = NULL;			//Only if we have aes should encryption key be set.

	if (args.parameters & EOperationHasParameter_AES) {

		CharString key = CharString_createNull();

		if (
			(ParsedArgs_getArg(args, EOperationHasParameter_AESShift, &key)).genericError ||
			!CharString_isHex(key)
		)
			gotoIfError(clean, Error_invalidState(
				2, "CLI_convert() Invalid parameter sent to -aes. Expecting key in hex (32 bytes)"
			))

		U64 off = CharString_startsWithStringInsensitive(key, CharString_createRefCStrConst("0x"), 0) ? 2 : 0;

		if (CharString_length(key) - off != 64)
			gotoIfError(clean, Error_invalidState(
				3, "CLI_convert() Invalid parameter sent to -aes. Expecting key in hex (32 bytes)"
			))

		for (U64 i = off; i + 1 < CharString_length(key); ++i) {

			U8 v0 = C8_hex(key.ptr[i]);
			U8 v1 = C8_hex(key.ptr[++i]);

			v0 = (v0 << 4) | v1;
			*((U8*)encryptionKeyV + ((i - off) >> 1)) = v0;
		}

		encryptionKey = encryptionKeyV;
	}

	//Now convert it

	switch (args.format) {

		case EFormat_oiDL:

			if(isTo)
				gotoIfError(clean, CLI_convertToDL(args, inputArg, info, outputArg, encryptionKey))

			else gotoIfError(clean, CLI_convertFromDL(args, inputArg, info, outputArg, encryptionKey))

			break;

		case EFormat_oiCA:

			if(isTo)
				gotoIfError(clean, CLI_convertToCA(args, inputArg, info, outputArg, encryptionKey))

			else gotoIfError(clean, CLI_convertFromCA(args, inputArg, info, outputArg, encryptionKey))

			break;

		default:
			gotoIfError(clean, Error_invalidOperation(0, "CLI_convert() Unsupported format"))
	}

	//Tell CLI users

	Log_debugLnx("Converted file oiXX format in %"PRIu64"ms", (Time_now() - start + MS - 1) / MS);

clean:

	if (err.genericError)
		Log_errorLnx("File conversion failed!");

	FileInfo_freex(&info);
	Error_printx(err, ELogLevel_Error, ELogOptions_NewLine);
	return !err.genericError;
}

Bool CLI_convertTo(ParsedArgs args) {
	return CLI_convert(args, true);
}

Bool CLI_convertFrom(ParsedArgs args) {
	return CLI_convert(args, false);
}