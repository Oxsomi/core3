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

#include "types/buffer.h"
#include "types/time.h"
#include "platforms/log.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/file.h"
#include "formats/oiSH.h"
#include "cli.h"

Bool CLI_fileCombine(ParsedArgs args) {

	Ns start = Time_now();

	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;
	CharString inputArg = CharString_createNull();
	Buffer buf[3] = { 0 };
	SHFile tmpSHFile[3] = { 0 };

	//Get inputs and output

	U64 offset = 0;
	gotoIfError2(clean, ListCharString_get(args.args, offset++, &inputArg))
	
	CharString outputArg = CharString_createNull();
	CharString inputArg2 = CharString_createNull();

	gotoIfError2(clean, ListCharString_get(args.args, offset++, &outputArg))
	gotoIfError2(clean, ListCharString_get(args.args, offset++, &inputArg2))

	//Read input buffers

	if ((err = File_read(inputArg, 1 * SECOND, &buf[0])).genericError) {
		Log_debugLnx("CLI_fileCombine() missing input (1)");
		goto clean;
	}

	if ((err = File_read(inputArg2, 1 * SECOND, &buf[1])).genericError) {
		Log_debugLnx("CLI_fileCombine() missing input (2)");
		goto clean;
	}

	switch (args.format) {

		case EFormat_oiSH:

			if (!SHFile_readx(buf[0], false, &tmpSHFile[0], e_rr) || !SHFile_readx(buf[1], false, &tmpSHFile[1], e_rr)) {
				Log_warnLnx("CLI_fileCombine() one of two SHFile couldn't be parsed");
				goto clean;
			}

			if(!SHFile_combinex(tmpSHFile[0], tmpSHFile[1], &tmpSHFile[2], e_rr)) {
				Log_warnLnx("CLI_fileCombine() SHFile can't be merged");
				goto clean;
			}

			if (!SHFile_writex(tmpSHFile[2], &buf[2], e_rr)) {
				Log_warnLnx("CLI_fileCombine() SHFile can't be serialized");
				goto clean;
			}

			break;

		//TODO: Combine oiCA and oiDL

		default:
			Log_errorLnx("CLI_fileCombine() doesn't support the specified format");
			goto clean;
	}

	if ((err = File_write(buf[2], outputArg, 1 * SECOND)).genericError) {
		Log_warnLnx("CLI_fileCombine() can't write to output file");
		goto clean;
	}

	Log_debugLnx("Combined oiXX files in %"PRIu64"ms", (Time_now() - start + MS - 1) / MS);

clean:

	for(U8 i = 0; i < sizeof(buf) / sizeof(buf[0]); ++i) {
		Buffer_freex(&buf[i]);
		SHFile_freex(&tmpSHFile[i]);
	}

	Error_printx(err, ELogLevel_Error, ELogOptions_NewLine);
	return false;
}
