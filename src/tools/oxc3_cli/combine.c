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

//tools/oxc3_cli/combine.c

#include "formats/oiSH/sh_file.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_combine.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"
#include "types/container/buffer.h"
#include "types/container/memory_stream.h"
#include "types/container/encryption_stream.h"
#include "types/container/log.h"
#include "types/base/error.h"
#include "types/base/string_read_helper.h"
#include "types/base/string_read.h"
#include "types/base/time.h"
#include "types/base/c8.h"
#include "types/math/vec4.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "platforms/file.h"
#include "tools/oxc3_cli/cli.h"
#include "types/base/constants.h"

Bool CLI_fileCombine(const ParsedArgs *args) {

	if(!args) return false;

	Ns start = Time_now();

	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;
	Buffer buf[3] = { 0 };
	U32 *encryptionKey = NULL;            //Only if we have aes should encryption key be set.

	const Allocator *alloc = Platform_instance->alloc;

	const RefPtrType fileHandleType = FileHandle_makeType(alloc);
	const RefPtrType memoryStreamType = MemoryStream_makeType(alloc);
	const RefPtrType encryptionStreamType = EncryptionStream_makeType(alloc);

	if (args->parameters & EOperationHasParameter_SplitBy) {
		Log_debugLnx("CLI_fileCombine() failed, -split can't be used");
		s_uccess = false;
		goto clean;
	}

	if (!(args->parameters & EOperationHasParameter_Input2)) {
		Log_debugLnx("CLI_fileCombine() failed, -input2 is required");
		s_uccess = false;
		goto clean;
	}

	//TODO: Multiple files

	//Get inputs and output

	CharString inputArg = CharString_createNull();

	gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &inputArg, e_rr));

	CharString outputArg = CharString_createNull();
	CharString inputArg2 = CharString_createNull();

	gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &outputArg, e_rr));

	//Parse encryption key (-aes / -aes-file / -aes-stdin)

	U32 encryptionKeyV[8] = { 0 };
	Bool hasKey = false;

	gotoIfError3(clean, CLI_getAesKey(args, encryptionKeyV, &hasKey, e_rr));

	if(hasKey)
		encryptionKey = encryptionKeyV;

	//Parse input2

	gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_Input2Shift, &inputArg2, e_rr));

	//Read input buffers

	if (!File_read(&inputArg, 100 * MS, 0, 0, &fileHandleType, &buf[0], e_rr)) {
		Log_debugLnx("CLI_fileCombine() missing input (1)");
		goto clean;
	}

	if (!File_read(&inputArg2, 100 * MS, 0, 0, &fileHandleType, &buf[1], e_rr)) {
		Log_debugLnx("CLI_fileCombine() missing input (2)");
		goto clean;
	}

	switch (args->format) {

		case EFormat_oiSH: {

			SHFile tmp[3] = { 0 };

			MemoryStreamRef *readStream[2] = { 0 };
			MemoryStreamRef *writeStream = NULL;
			U64 readOff[2] = { 0 };
			U64 writeOff = 0;

			if (
				!MemoryStream_createFromBuffer(&buf[0], EMemoryStreamFlags_None, &memoryStreamType, &readStream[0], e_rr) ||
				!MemoryStream_createFromBuffer(&buf[1], EMemoryStreamFlags_None, &memoryStreamType, &readStream[1], e_rr) ||
				!SHFile_read((StreamRef*)readStream[0], &readOff[0], false, alloc, &tmp[0], e_rr) ||
				!SHFile_read((StreamRef*)readStream[1], &readOff[1], false, alloc, &tmp[1], e_rr)
			) {
				Log_warnLnx("CLI_fileCombine() one of two SHFile couldn't be parsed");
				goto cleanSH;
			}

			if(!SHFile_combine(&tmp[0], &tmp[1], alloc, &tmp[2], e_rr)) {
				Log_warnLnx("CLI_fileCombine() SHFile can't be merged");
				goto cleanSH;
			}

			if (
				!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &memoryStreamType, &writeStream, e_rr) ||
				!SHFile_write((StreamRef*)writeStream, &writeOff, &tmp[2], alloc, e_rr) ||
				!MemoryStream_move(&writeStream, &buf[2], e_rr)
			) {
				Log_warnLnx("CLI_fileCombine() SHFile can't be serialized");
				goto cleanSH;
			}

		cleanSH:

			RefPtr_dec(&readStream[0]);
			RefPtr_dec(&readStream[1]);
			RefPtr_dec(&writeStream);

			for(U8 i = 0; i < sizeof(tmp) / sizeof(tmp[0]); ++i)
				SHFile_free(&tmp[i], alloc);

			if(!err.genericError)        //Branch failures populate err (via e_rr); only break to write on success
				break;

			goto clean;
		}

		case EFormat_oiCA: {

			CAFile tmp[3] = { 0 };

			MemoryStreamRef *readStream[2] = { 0 };
			MemoryStreamRef *writeStream = NULL;
			U64 writeOff = 0;

			if (
				!MemoryStream_createFromBuffer(&buf[0], EMemoryStreamFlags_None, &memoryStreamType, &readStream[0], e_rr) ||
				!MemoryStream_createFromBuffer(&buf[1], EMemoryStreamFlags_None, &memoryStreamType, &readStream[1], e_rr) ||
				!CAFile_read(
					(StreamRef*)readStream[0], encryptionKey ? &encryptionStreamType : NULL, 0, encryptionKey, alloc,
					&tmp[0], e_rr
				) ||
				!CAFile_read(
					(StreamRef*)readStream[1], encryptionKey ? &encryptionStreamType : NULL, 0, encryptionKey, alloc,
					&tmp[1], e_rr
				)
			) {
				Log_warnLnx("CLI_fileCombine() one of two CAFile couldn't be parsed");
				goto cleanCA;
			}

			if(!CAFile_combine(
				&tmp[0], &tmp[1], EArchiveCombineMode_RequireSame, EArchiveCombineFlags_None, alloc, &tmp[2], e_rr
			)) {
				Log_warnLnx("CLI_fileCombine() CAFile can't be merged");
				goto cleanCA;
			}

			if(encryptionKey)
				Buffer_memcpy(
					Buffer_createRef(tmp[2].settings.encryptionKey, sizeof(tmp[2].settings.encryptionKey)),
					Buffer_createRefConst(encryptionKey, sizeof(encryptionKeyV))
				);

			if (
				!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &memoryStreamType, &writeStream, e_rr) ||
				!CAFile_write(
					&tmp[2], encryptionKey ? &encryptionStreamType : NULL, (StreamRef*)writeStream, &writeOff, alloc, e_rr
				) ||
				!MemoryStream_move(&writeStream, &buf[2], e_rr)
			) {

				if(encryptionKey)
					Buffer_clearAllSecure(
						Buffer_createRef(tmp[2].settings.encryptionKey, sizeof(tmp[2].settings.encryptionKey))
					);

				Log_warnLnx("CLI_fileCombine() CAFile can't be serialized");
				goto cleanCA;
			}

			if(encryptionKey)
				Buffer_clearAllSecure(
					Buffer_createRef(tmp[2].settings.encryptionKey, sizeof(tmp[2].settings.encryptionKey))
				);

		cleanCA:

			RefPtr_dec(&readStream[0]);
			RefPtr_dec(&readStream[1]);
			RefPtr_dec(&writeStream);

			for(U8 i = 0; i < sizeof(tmp) / sizeof(tmp[0]); ++i)
				CAFile_free(&tmp[i], alloc);

			if(!err.genericError)        //Branch failures populate err (via e_rr); only break to write on success
				break;

			goto clean;
		}

		case EFormat_oiDL: {

			DLFile tmp[3] = { 0 };

			MemoryStreamRef *readStream[2] = { 0 };
			MemoryStreamRef *writeStream = NULL;
			U64 readOff[2] = { 0 };
			U64 writeOff = 0;

			if (
				!MemoryStream_createFromBuffer(&buf[0], EMemoryStreamFlags_None, &memoryStreamType, &readStream[0], e_rr) ||
				!MemoryStream_createFromBuffer(&buf[1], EMemoryStreamFlags_None, &memoryStreamType, &readStream[1], e_rr) ||
				!DLFile_read(
					(StreamRef*)readStream[0], &readOff[0], encryptionKey, I32x4_zero(), false, false, alloc,
					encryptionKey ? &encryptionStreamType : NULL, &tmp[0], e_rr
				) ||
				!DLFile_read(
					(StreamRef*)readStream[1], &readOff[1], encryptionKey, I32x4_zero(), false, false, alloc,
					encryptionKey ? &encryptionStreamType : NULL, &tmp[1], e_rr
				)
			) {
				Log_warnLnx("CLI_fileCombine() one of two DLFile couldn't be parsed");
				goto cleanDL;
			}

			if(!DLFile_combine(&tmp[0], &tmp[1], alloc, &tmp[2], e_rr)) {
				Log_warnLnx("CLI_fileCombine() DLFile can't be merged");
				goto cleanDL;
			}

			if(encryptionKey)
				Buffer_memcpy(
					Buffer_createRef(tmp[2].settings.encryptionKey, sizeof(tmp[2].settings.encryptionKey)),
					Buffer_createRefConst(encryptionKey, sizeof(encryptionKeyV))
				);

			if (
				!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &memoryStreamType, &writeStream, e_rr) ||
				!DLFile_write(
					&tmp[2], alloc, (StreamRef*)writeStream, encryptionKey ? &encryptionStreamType : NULL, I32x4_zero(),
					&writeOff, e_rr
				) ||
				!MemoryStream_move(&writeStream, &buf[2], e_rr)
			) {

				if(encryptionKey)
					Buffer_clearAllSecure(
						Buffer_createRef(tmp[2].settings.encryptionKey, sizeof(tmp[2].settings.encryptionKey))
					);

				Log_warnLnx("CLI_fileCombine() DLFile can't be serialized");
				goto cleanDL;
			}

			if(encryptionKey)
				Buffer_clearAllSecure(
					Buffer_createRef(tmp[2].settings.encryptionKey, sizeof(tmp[2].settings.encryptionKey))
				);

		cleanDL:

			RefPtr_dec(&readStream[0]);
			RefPtr_dec(&readStream[1]);
			RefPtr_dec(&writeStream);

			for(U8 i = 0; i < sizeof(tmp) / sizeof(tmp[0]); ++i)
				DLFile_free(&tmp[i], alloc);

			if(!err.genericError)        //Branch failures populate err (via e_rr); only break to write on success
				break;

			goto clean;
		}

		default:
			Log_errorLnx("CLI_fileCombine() doesn't support the specified format");
			s_uccess = false;
			goto clean;
	}

	if (!File_write(&buf[2], &outputArg, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr)) {
		Log_warnLnx("CLI_fileCombine() can't write to output file");
		goto clean;
	}

	Log_debugLnx("Combined oiXX files in %"PRIu64"ms", (Time_now() - start + MS - 1) / MS);

clean:

	if(encryptionKey)
		Buffer_clearAllSecure(Buffer_createRef(encryptionKeyV, sizeof(encryptionKeyV)));

	for(U8 i = 0; i < sizeof(buf) / sizeof(buf[0]); ++i)
		Buffer_free(&buf[i], alloc);

	Error_print(alloc, &err, ELogLevel_Error, ELogOptions_NewLine);
	return s_uccess && !err.genericError;
}
