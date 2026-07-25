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

//tools/oxc3_cli/audio.c

#include "tools/oxc3_cli/cli.h"
#include "audio/audio_interface.h"
#include "audio/audio_device.h"
#include "types/container/stream.h"
#include "formats/wav/wav_file.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "types/base/error.h"
#include "types/base/string_read.h"
#include "types/base/string_read_helper.h"
#include "types/base/constants.h"
#include "types/container/string.h"
#include "types/container/string_helper.h"
#include "types/container/ref_ptr.h"

Bool CLI_audioDevices(const ParsedArgs *args) {

	if(!args) return false;

	(void) args;

	const Allocator *alloc = Platform_instance->alloc;

	Bool s_uccess = true;
	AudioInterfaceRef *ref = NULL;
	ListAudioDeviceInfo infos = (ListAudioDeviceInfo) { 0 };
	RefPtrType type = AudioInterface_makeType(alloc);
	Error err = Error_none();

	gotoIfError3(clean, AudioInterface_create(&ref, alloc, &type, &err));
	gotoIfError3(clean, AudioInterface_getDeviceInfos(AudioInterfaceRef_ptr(ref), alloc, &infos, &err));

	for(U64 i = 0; i < infos.length; ++i)
		AudioDeviceInfo_print(infos.ptr[i], alloc);

clean:

	ListAudioDeviceInfo_free(&infos, alloc);

	if(ref)
		RefPtr_dec(&ref);

	return s_uccess;
}

typedef struct CLIAudioConvertForeach {
	ListCharString *inputs, *outputs;
	CharString outputDir;
	CharString inputDir;
} CLIAudioConvertForeach;

Bool CLI_audioConvertFind(const FileInfo *info, CLIAudioConvertForeach *data, const Allocator *alloc, Error *e_rr) {

	CharString copy = CharString_createNull();
	CharString wav = CharString_createRefCStrConst(".wav");
	Bool s_uccess = true;

	if (info->type == EFileType_File && CharString_endsWithStringInsensitive(&info->path, &wav, 0)) {

		CharString cut = CharString_createNull();
		if (!CharString_cut(&info->path, CharString_length(data->inputDir), 0, &cut))
			retError(clean, Error_invalidState(0, "CLI_audioConvertFind() cut failed"));

		gotoIfError3(clean, CharString_createCopy(data->outputDir, alloc, &copy, e_rr));
		gotoIfError3(clean, CharString_appendString(&copy, &cut, alloc, e_rr));
		gotoIfError3(clean, ListCharString_pushBack(data->outputs, copy, alloc, e_rr));
		copy = CharString_createNull();

		gotoIfError3(clean, CharString_createCopy(info->path, alloc, &copy, e_rr));
		gotoIfError3(clean, ListCharString_pushBack(data->inputs, copy, alloc, e_rr));
		copy = CharString_createNull();
	}

clean:

	if(!s_uccess)
		Log_errorLnx("CLI_audioConvertFind() failed");

	CharString_free(&copy, alloc);
	return s_uccess;
}

Bool CLI_audioConvert(const ParsedArgs *args) {

	if(!args) return false;

	const Allocator *alloc = Platform_instance->alloc;

	Bool success = true;
	ESplitType splitType = ESplitType_Untouched;

	U8 bitPreferences[5] = { 0 };
	U8 bitPreferenceCount = 0;

	ListCharString inputs  = (ListCharString) { 0 };
	ListCharString outputs = (ListCharString) { 0 };

	if (args->parameters & EOperationHasParameter_SplitBy) {

		CharString str = CharString_createNull();
		if (ParsedArgs_getArg(args, EOperationHasParameter_SplitByShift, &str).genericError || CharString_length(str) <= 2) {
			Log_debugLnx("CLI_audioConvert() invalid -split argument. Expected left, right or average");
			success = false;
			goto clean2;
		}
		
		if(CharString_equalsCStringInsensitive(&str, "average"))
			splitType = ESplitType_Average;

		else if(CharString_equalsCStringInsensitive(&str, "left"))
			splitType = ESplitType_Left;

		else if(CharString_equalsCStringInsensitive(&str, "right"))
			splitType = ESplitType_Right;

		else {
			Log_debugLnx("CLI_audioConvert() invalid -split argument. Expected left, right or average");
			success = false;
			goto clean2;
		}
	}
	
	if (args->parameters & EOperationHasParameter_Bit) {

		CharString str = CharString_createNull();
		if (ParsedArgs_getArg(args, EOperationHasParameter_BitShift, &str).genericError || !CharString_length(str)) {
			Log_debugLnx("CLI_audioConvert() invalid -bits argument. Expected 8, 16, 24, 32 or 64 or a combo split by ,");
			success = false;
			goto clean2;
		}
		
		ListCharString split = (ListCharString) { 0 };
		CharStringSplit splitInfo = (CharStringSplit) { .s = &str, .allocator = alloc, .result = &split };

		if (!CharString_splitSensitive(&splitInfo, ',', NULL) || split.length > 5) {
			ListCharString_free(&split, alloc);
			Log_debugLnx("CLI_audioConvert() invalid -bits argument. Expected 8, 16, 24, 32 or 64 or a combo split by ,");
			success = false;
			goto clean2;
		}

		for (U64 i = 0; i < split.length; ++i) {

			CharString stri = split.ptr[i];
			U64 num = 0;

			if (!CharString_parseDec(stri, &num) || !(num == 8 || num == 16 || num == 24 || num == 32 || num == 64)) {
				ListCharString_free(&split, alloc);
				Log_debugLnx("CLI_audioConvert() invalid -bits argument. Expected unsigned integer (8, 16, 24, 32, 64)");
				success = false;
				goto clean2;
			}

			bitPreferences[bitPreferenceCount++] = (U8) num;
		}

		ListCharString_free(&split, alloc);
	}

	CharString inputStr = CharString_createNull();
	if (ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &inputStr).genericError) {
		Log_debugLnx("CLI_audioConvert() invalid -input argument. Expected folder or file path");
		success = false;
		goto clean2;
	}

	CharString outputStr = CharString_createNull();
	if (ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &outputStr).genericError) {
		Log_debugLnx("CLI_audioConvert() invalid -output argument. Expected folder or file path");
		success = false;
		goto clean2;
	}

	//Multiple inputs

	if (File_hasFolder(&inputStr, alloc)) {

		CharString resolved = CharString_createNull();
		CharString resolved1 = CharString_createNull();

		Bool isVirtual = false;
		if(!File_resolve(&outputStr, &isVirtual, 0, &Platform_instance->defaultDir, alloc, &resolved, NULL)) {
			Log_debugLnx("CLI_audioConvert() invalid -output argument. Couldn't be resolved");
			success = false;
			goto clean2;
		}

		if(!File_resolve(&inputStr, &isVirtual, 0, &Platform_instance->defaultDir, alloc, &resolved1, NULL)) {
			CharString_free(&resolved, alloc);
			Log_debugLnx("CLI_audioConvert() invalid -input argument. Couldn't be resolved");
			success = false;
			goto clean2;
		}

		if (!CharString_append(&resolved, '/', alloc, NULL) || !CharString_append(&resolved1, '/', alloc, NULL)) {
			CharString_free(&resolved, alloc);
			CharString_free(&resolved1, alloc);
			Log_debugLnx("CLI_audioConvert() invalid -output argument. Couldn't allocate memory");
			success = false;
			goto clean2;
		}

		CLIAudioConvertForeach audioForeach = (CLIAudioConvertForeach) {
			.inputs = &inputs,
			.outputs = &outputs,
			.outputDir = resolved,
			.inputDir = resolved1
		};

		if(!File_foreach(&inputStr, false, (FileCallback) CLI_audioConvertFind, &audioForeach, true, alloc, NULL)) {
			CharString_free(&resolved, alloc);
			CharString_free(&resolved1, alloc);
			Log_debugLnx("CLI_audioConvert() invalid -output argument. Couldn't query all files");
			success = false;
			goto clean2;
		}

		CharString_free(&resolved, alloc);
		CharString_free(&resolved1, alloc);
	}

	//Single input

	else {
		ListCharString_pushBack(&inputs, inputStr, alloc, NULL);
		ListCharString_pushBack(&outputs, outputStr, alloc, NULL);
	}

	switch (args->format) {

		case EFormat_WAV: {

			for (U64 i = 0; i < inputs.length; ++i) {

				CharString input = inputs.ptr[i];
				CharString output = outputs.ptr[i];

				Bool s_uccess = true;
				Error err = Error_none(), *e_rr = &err;
				WAVFile wav = (WAVFile) { 0 };
				StreamRef *outputStream = NULL;
				StreamRef *inputStream = NULL;
				RefPtrType fileHandleType = FileHandle_makeType(alloc);
				RefPtrType streamType = FileStream_makeType(alloc);

				Log_debugLnx("CLI_audioConvert() converting \"%.*s\"", (int) CharString_length(input), input.ptr);

				gotoIfError3(clean, File_openStream(
					&input, 1 * SECOND, EFileOpenType_Read, false, &fileHandleType, &streamType, &inputStream, e_rr
				));
				gotoIfError3(clean, WAV_read(inputStream, 0, 0, alloc, &wav, e_rr));

				Bool hasBitPreference = bitPreferenceCount == 0;
				U16 newBitCount = wav.fmt.stride;

				if(!hasBitPreference)
					for (U64 j = 0; j < bitPreferenceCount; ++j)
						if(bitPreferences[j] <= wav.fmt.stride) {
							hasBitPreference = true;
							newBitCount = bitPreferences[j];
							break;
						}

				if(!hasBitPreference)
					retError(clean, Error_invalidState(0, "CLI_audioConvert() format wasn't supported to truncate to"));

				gotoIfError3(clean, File_openStream(
					&output, 1 * SECOND, EFileOpenType_Write, true, &fileHandleType, &streamType, &outputStream, e_rr
				));

				if(wav.fmt.channels == 1)
					splitType = ESplitType_Untouched;

				WAVConversionInfo info = (WAVConversionInfo) {
					.format = EAudioFormat_WAV,
					.splitType = (SplitType) splitType,
					.oldByteCountStereoPcm = (U8) (wav.fmt.stride >> 3) | (wav.fmt.channels == 2 ? 0x80 : 0),
					.newByteCountStereoPcm = (U8) (newBitCount >> 3)
				};

				Bool isStereo = splitType == ESplitType_Untouched && wav.fmt.channels == 2;

				if(splitType == ESplitType_Untouched && wav.fmt.stride == newBitCount) {
					gotoIfError3(clean, WAV_write(
						outputStream,
						inputStream,
						0,
						wav.dataStart,
						wav.dataLength,
						isStereo,
						wav.fmt.frequency,
						wav.fmt.stride,
						false,
						NULL,
						alloc,
						e_rr
					));
				}

				else gotoIfError3(clean, WAVFile_convert(
					inputStream,
					wav.dataStart,
					wav.dataLength,
					outputStream,
					0,
					info,
					wav.fmt.frequency,
					true,
					alloc,
					e_rr
				));

				Log_debugLnx("CLI_audioConvert() converted to \"%.*s\"", (int) CharString_length(output), output.ptr);
			
			clean:

				Bool hadStream = outputStream != NULL;

				RefPtr_dec(&inputStream);

				if(hadStream)
					RefPtr_dec(&outputStream);

				if(!s_uccess) {

					if (hadStream)
						File_remove(&output, 1 * SECOND, alloc, NULL);

					Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
					Log_errorLnx("CLI_audioConvert() couldn't be convert \"%.*s\"", (int) CharString_length(input), input.ptr);
				}

				success &= s_uccess;
			}
			
			if (success)
				Log_debugLnx("CLI_audioConvert() great success!");

			break;
		}

		default:
			Log_errorLnx("CLI_audioConvert() unrecognized format; expected \"WAV\"");
			success = false;
			break;
	}

clean2:
	ListCharString_freeUnderlying(&inputs, alloc);
	ListCharString_freeUnderlying(&outputs, alloc);
	return success;
}
