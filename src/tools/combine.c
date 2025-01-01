/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "formats/oiSH/sh_file.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiDL/dl_file.h"
#include "types/container/buffer.h"
#include "types/base/time.h"
#include "types/base/c8.h"
#include "platforms/log.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/file.h"
#include "tools/cli.h"

Bool CLI_fileCombine(ParsedArgs args) {

	Ns start = Time_now();

	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;
	Buffer buf[3] = { 0 };
	U32 *encryptionKey = NULL;			//Only if we have aes should encryption key be set.

	if (args.parameters & EOperationHasParameter_SplitBy) {
		Log_debugLnx("CLI_fileCombine() failed, -split can't be used");
		s_uccess = false;
		goto clean;
	}

	if (!(args.parameters & EOperationHasParameter_Input2)) {
		Log_debugLnx("CLI_fileCombine() failed, -input2 is required");
		s_uccess = false;
		goto clean;
	}

	//TODO: Multiple files

	//Get inputs and output

	CharString inputArg = CharString_createNull();

	gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &inputArg))

	CharString outputArg = CharString_createNull();
	CharString inputArg2 = CharString_createNull();

	gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &outputArg))

	//Parse encryption key

	U32 encryptionKeyV[8] = { 0 };

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

	//Parse input2

	gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_Input2Shift, &inputArg2))

	//Read input buffers

	if (!File_readx(inputArg, 100 * MS, 0, 0, &buf[0], e_rr)) {
		Log_debugLnx("CLI_fileCombine() missing input (1)");
		goto clean;
	}

	if (!File_readx(inputArg2, 100 * MS, 0, 0, &buf[1], e_rr)) {
		Log_debugLnx("CLI_fileCombine() missing input (2)");
		goto clean;
	}

	switch (args.format) {

		case EFormat_oiSH: {

			SHFile tmp[3] = { 0 };

			if (!SHFile_readx(buf[0], false, &tmp[0], e_rr) || !SHFile_readx(buf[1], false, &tmp[1], e_rr)) {
				Log_warnLnx("CLI_fileCombine() one of two SHFile couldn't be parsed");
				goto cleanSH;
			}

			if(!SHFile_combinex(tmp[0], tmp[1], &tmp[2], e_rr)) {
				Log_warnLnx("CLI_fileCombine() SHFile can't be merged");
				goto cleanSH;
			}

			if (!SHFile_writex(tmp[2], &buf[2], e_rr)) {
				Log_warnLnx("CLI_fileCombine() SHFile can't be serialized");
				goto cleanSH;
			}

		cleanSH:

			for(U8 i = 0; i < sizeof(tmp) / sizeof(tmp[0]); ++i)
				SHFile_freex(&tmp[i]);

			if(s_uccess)
				break;

			goto clean;
		}

		case EFormat_oiCA: {

			CAFile tmp[3] = { 0 };

			if (!CAFile_readx(buf[0], encryptionKey, &tmp[0], e_rr) || !CAFile_readx(buf[1], encryptionKey, &tmp[1], e_rr)) {
				Log_warnLnx("CLI_fileCombine() one of two CAFile couldn't be parsed");
				goto cleanCA;
			}

			if(!CAFile_combinex(tmp[0], tmp[1], &tmp[2], e_rr)) {
				Log_warnLnx("CLI_fileCombine() CAFile can't be merged");
				goto cleanCA;
			}

			if(encryptionKey)
				Buffer_copy(
					Buffer_createRef(tmp[2].settings.encryptionKey, sizeof(tmp[2].settings.encryptionKey)),
					Buffer_createRefConst(encryptionKey, sizeof(encryptionKeyV))
				);

			if (!CAFile_writex(tmp[2], &buf[2], e_rr)) {

				if(encryptionKey)
					Buffer_unsetAllBits(Buffer_createRef(tmp[2].settings.encryptionKey, sizeof(tmp[2].settings.encryptionKey)));

				Log_warnLnx("CLI_fileCombine() CAFile can't be serialized");
				goto cleanCA;
			}

			if(encryptionKey)
				Buffer_unsetAllBits(Buffer_createRef(tmp[2].settings.encryptionKey, sizeof(tmp[2].settings.encryptionKey)));

		cleanCA:

			for(U8 i = 0; i < sizeof(tmp) / sizeof(tmp[0]); ++i)
				CAFile_freex(&tmp[i]);

			if(s_uccess)
				break;

			goto clean;
		}

		case EFormat_oiDL: {

			DLFile tmp[3] = { 0 };

			if (
				!DLFile_readx(buf[0], encryptionKey, false, &tmp[0], e_rr) ||
				!DLFile_readx(buf[1], encryptionKey, false, &tmp[1], e_rr)
			) {
				Log_warnLnx("CLI_fileCombine() one of two DLFile couldn't be parsed");
				goto cleanDL;
			}

			if(!DLFile_combinex(tmp[0], tmp[1], &tmp[2], e_rr)) {
				Log_warnLnx("CLI_fileCombine() DLFile can't be merged");
				goto cleanDL;
			}

			if(encryptionKey)
				Buffer_copy(
					Buffer_createRef(tmp[2].settings.encryptionKey, sizeof(tmp[2].settings.encryptionKey)),
					Buffer_createRefConst(encryptionKey, sizeof(encryptionKeyV))
				);

			if (!DLFile_writex(tmp[2], &buf[2], e_rr)) {

				if(encryptionKey)
					Buffer_unsetAllBits(Buffer_createRef(tmp[2].settings.encryptionKey, sizeof(tmp[2].settings.encryptionKey)));

				Log_warnLnx("CLI_fileCombine() DLFile can't be serialized");
				goto cleanDL;
			}

			if(encryptionKey)
				Buffer_unsetAllBits(Buffer_createRef(tmp[2].settings.encryptionKey, sizeof(tmp[2].settings.encryptionKey)));

		cleanDL:

			for(U8 i = 0; i < sizeof(tmp) / sizeof(tmp[0]); ++i)
				DLFile_freex(&tmp[i]);

			if(s_uccess)
				break;

			goto clean;
		}

		default:
			Log_errorLnx("CLI_fileCombine() doesn't support the specified format");
			goto clean;
	}

	if (!File_writex(buf[2], outputArg, 0, 0, 1 * SECOND, true, e_rr)) {
		Log_warnLnx("CLI_fileCombine() can't write to output file");
		goto clean;
	}

	Log_debugLnx("Combined oiXX files in %"PRIu64"ms", (Time_now() - start + MS - 1) / MS);

clean:

	if(encryptionKey)
		Buffer_unsetAllBits(Buffer_createRef(encryptionKeyV, sizeof(encryptionKeyV)));

	for(U8 i = 0; i < sizeof(buf) / sizeof(buf[0]); ++i)
		Buffer_freex(&buf[i]);

	Error_printx(err, ELogLevel_Error, ELogOptions_NewLine);
	return false;
}
