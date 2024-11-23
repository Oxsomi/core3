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

#include "platforms/ext/listx_impl.h"
#include "tools/cli.h"
#include "audio/interface.h"
#include "audio/device.h"
#include "formats/wav/wav.h"
#include "platforms/file.h"
#include "platforms/log.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "types/container/ref_ptr.h"

Bool CLI_audioDevices(ParsedArgs args) {

	(void) args;

	Bool s_uccess = true;
	AudioInterfaceRef *ref = NULL;
	ListAudioDeviceInfo infos = (ListAudioDeviceInfo) { 0 };
	Error err = Error_none();

	gotoIfError3(clean, AudioInterface_createx(&ref, &err))
	gotoIfError3(clean, AudioInterface_getDeviceInfosx(AudioInterfaceRef_ptr(ref), &infos, &err))

	for(U64 i = 0; i < infos.length; ++i)
		AudioDeviceInfo_printx(infos.ptr[i]);

clean:

	ListAudioDeviceInfo_freex(&infos);

	if(ref)
		AudioInterfaceRef_dec(&ref);

	return s_uccess;
}

typedef struct CLIAudioConvertForeach {
	ListCharString *inputs, *outputs;
	CharString outputDir;
	CharString inputDir;
} CLIAudioConvertForeach;

Bool CLI_audioConvertFind(FileInfo info, CLIAudioConvertForeach *data, Error *e_rr) {

	CharString copy = CharString_createNull();
	Bool s_uccess = true;

	if (
		info.type == EFileType_File &&
		CharString_endsWithStringInsensitive(info.path, CharString_createRefCStrConst(".wav"), 0)
	) {

		CharString cut = CharString_createNull();
		if (!CharString_cut(info.path, CharString_length(data->inputDir), 0, &cut))
			retError(clean, Error_invalidState(0, "CLI_audioConvertFind() cut failed"))

		gotoIfError2(clean, CharString_createCopyx(data->outputDir, &copy))
		gotoIfError2(clean, CharString_appendStringx(&copy, cut))
		gotoIfError2(clean, ListCharString_pushBackx(data->outputs, copy))
		copy = CharString_createNull();

		gotoIfError2(clean, CharString_createCopyx(info.path, &copy))
		gotoIfError2(clean, ListCharString_pushBackx(data->inputs, copy))
		copy = CharString_createNull();
	}

clean:
	
	if(!s_uccess)
		Log_errorLnx("CLI_audioConvertFind() failed");

	CharString_freex(&copy);
	return s_uccess;
}

Bool CLI_audioConvert(ParsedArgs args) {

	Bool success = true;
	ESplitType splitType = ESplitType_Untouched;

	U8 bitPreferences[5] = { 0 };
	U8 bitPreferenceCount = 0;

	ListCharString inputs  = (ListCharString) { 0 };
	ListCharString outputs = (ListCharString) { 0 };

	if (args.parameters & EOperationHasParameter_SplitBy) {

		CharString str = CharString_createNull();
		if (ParsedArgs_getArg(args, EOperationHasParameter_SplitByShift, &str).genericError || CharString_length(str) <= 2) {
			Log_debugLnx("CLI_audioConvert() invalid -split argument. Expected left, right or average");
			success = false;
			goto clean2;
		}
		
		if(CharString_equalsStringInsensitive(str, CharString_createRefCStrConst("average")))
			splitType = ESplitType_Average;

		else if(CharString_equalsStringInsensitive(str, CharString_createRefCStrConst("left")))
			splitType = ESplitType_Left;

		else if(CharString_equalsStringInsensitive(str, CharString_createRefCStrConst("right")))
			splitType = ESplitType_Right;

		else if(CharString_equalsStringInsensitive(str, CharString_createRefCStrConst("untouched")))
			splitType = ESplitType_Untouched;

		else {
			Log_debugLnx("CLI_audioConvert() invalid -split argument. Expected left, right or average");
			success = false;
			goto clean2;
		}
	}
	
	if (args.parameters & EOperationHasParameter_Bit) {

		CharString str = CharString_createNull();
		if (ParsedArgs_getArg(args, EOperationHasParameter_BitShift, &str).genericError || !CharString_length(str)) {
			Log_debugLnx("CLI_audioConvert() invalid -bits argument. Expected 8, 16, 24, 32 or 64 or a combo split by ,");
			success = false;
			goto clean2;
		}
		
		ListCharString split = (ListCharString) { 0 };

		if (CharString_splitSensitivex(str, ',', &split).genericError || split.length > 5) {
			ListCharString_freex(&split);
			Log_debugLnx("CLI_audioConvert() invalid -bits argument. Expected 8, 16, 24, 32 or 64 or a combo split by ,");
			success = false;
			goto clean2;
		}

		for (U64 i = 0; i < split.length; ++i) {

			CharString stri = split.ptr[i];
			U64 num = 0;

			if (!CharString_parseDec(stri, &num) || !(num == 8 || num == 16 || num == 24 || num == 32 || num == 64)) {
				ListCharString_freex(&split);
				Log_debugLnx("CLI_audioConvert() invalid -bits argument. Expected unsigned integer (8, 16, 24, 32, 64)");
				success = false;
				goto clean2;
			}

			bitPreferences[bitPreferenceCount] = (U8) num;
			++bitPreferenceCount;
		}

		ListCharString_freex(&split);
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

	if (File_hasFolder(inputStr)) {

		CharString resolved = CharString_createNull();
		CharString resolved1 = CharString_createNull();

		Bool isVirtual = false;
		if(!File_resolvex(outputStr, &isVirtual, false, 0, &resolved, NULL)) {
			Log_debugLnx("CLI_audioConvert() invalid -output argument. Couldn't be resolved");
			success = false;
			goto clean2;
		}

		if(!File_resolvex(inputStr, &isVirtual, false, 0, &resolved1, NULL)) {
			CharString_freex(&resolved);
			Log_debugLnx("CLI_audioConvert() invalid -input argument. Couldn't be resolved");
			success = false;
			goto clean2;
		}

		if (CharString_appendx(&resolved, '/').genericError || CharString_appendx(&resolved1, '/').genericError) {
			CharString_freex(&resolved);
			CharString_freex(&resolved1);
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

		if(!File_foreach(inputStr, false, (FileCallback) CLI_audioConvertFind, &audioForeach, true, NULL)) {
			CharString_freex(&resolved);
			CharString_freex(&resolved1);
			Log_debugLnx("CLI_audioConvert() invalid -output argument. Couldn't query all files");
			success = false;
			goto clean2;
		}

		CharString_freex(&resolved);
		CharString_freex(&resolved1);
	}

	//Single input

	else {
		ListCharString_pushBackx(&inputs, inputStr);
		ListCharString_pushBackx(&outputs, outputStr);
	}

	switch (args.format) {

		case EFormat_WAV: {

			for (U64 i = 0; i < inputs.length; ++i) {

				CharString input = inputs.ptr[i];
				CharString output = outputs.ptr[i];

				Bool s_uccess = true;
				Error err = Error_none(), *e_rr = &err;
				WAVFile wav = (WAVFile) { 0 };
				Stream outputStream = (Stream) { 0 };
				Stream inputStream = (Stream) { 0 };

				Log_debugLnx("CLI_audioConvert() converting \"%.*s\"", (int) CharString_length(input), input.ptr);

				gotoIfError3(clean, File_openStreamx(input,  1 * SECOND, EFileOpenType_Read,  false, 1 * MIBI, &inputStream, e_rr))
				gotoIfError3(clean, WAV_readx(&inputStream, 0, 0, &wav, e_rr))

				if(wav.fmt.stride == 24)
					gotoIfError3(clean, Stream_resizex(&inputStream, (1 * MIBI) / 3 * 3, e_rr))
				
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
					retError(clean, Error_invalidState(0, "CLI_audioConvert() format wasn't supported to truncate to"))

				gotoIfError3(clean, File_openStreamx(output, 1 * SECOND, EFileOpenType_Write, true,  0, &outputStream, e_rr))

				if(wav.fmt.channels == 1)
					splitType = ESplitType_Untouched;

				WAVConversionInfo info = (WAVConversionInfo) {
					.format = EAudioFormat_WAV,
					.splitType = (SplitType) splitType,
					.oldByteCount = (U8) (wav.fmt.stride >> 3) | (wav.fmt.channels == 2 ? 0x80 : 0),
					.newByteCount = (U8) (newBitCount >> 3)
				};

				Bool isStereo = splitType == ESplitType_Untouched && wav.fmt.channels == 2;

				if(splitType == ESplitType_Untouched && wav.fmt.stride == newBitCount)
					gotoIfError3(clean, WAV_write(
						&outputStream,
						&inputStream,
						0,
						wav.dataStart,
						wav.dataLength,
						isStereo,
						wav.fmt.frequency,
						wav.fmt.stride,
						false,
						e_rr
					))

				else gotoIfError3(clean, WAVFile_convertx(
					&inputStream,
					wav.dataStart,
					wav.dataLength,
					&outputStream,
					0,
					info,
					wav.fmt.frequency,
					true,
					e_rr
				))

				Log_debugLnx("CLI_audioConvert() converted to \"%.*s\"", (int) CharString_length(output), output.ptr);
			
			clean:

				Bool hadStream = outputStream.handle.filePath.ptr;

				Stream_closex(&inputStream);

				if(hadStream)
					Stream_closex(&outputStream);

				if(!s_uccess) {

					if (hadStream)
						File_remove(output, 1 * SECOND, NULL);

					Error_printLnx(err);
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
	ListCharString_freeUnderlyingx(&inputs);
	ListCharString_freeUnderlyingx(&outputs);
	return success;
}
