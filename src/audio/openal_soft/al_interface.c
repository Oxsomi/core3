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

//audio/openal_soft/al_interface.c

#include "types/container/list_impl.h"
#include "audio/audio_interface.h"
#include "audio/audio_device.h"
#include "audio/openal_soft/openal_soft.h"
#include "types/container/log.h"
#include "types/container/string_helper.h"
#include "types/base/string_read_helper.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/constants.h"

//Audio interface data itself is mostly unused for OpenAL, because we don't have any functions we need to load.
//Maybe in the future with extensions.
//Currently only useful for querying devices.

U32 AudioInterface_sizeExt = sizeof(ALAudioInterface);

Bool AudioInterface_createExt(AudioInterface *interf, Error *e_rr) {

	Bool s_uccess = true;

	ALCint maj = 1, min = 1;

	ALC_PROCESS_ERROR(NULL, alcGetIntegerv(NULL, ALC_MAJOR_VERSION, 1, &maj));
	ALC_PROCESS_ERROR(NULL, alcGetIntegerv(NULL, ALC_MINOR_VERSION, 1, &min));

	if(maj < 0 || min < 0 || (maj >> 10) || (min >> 10))
		retError(clean, Error_invalidState(0, "AudioInterface_createExt() OpenAL version is invalid"));

	*interf = (AudioInterface) {
		.api = EAudioApi_OpenAL,
		.apiVersion = OXC3_MAKE_VERSION((U32)maj, (U32)min, 0)
	};

clean:
	return s_uccess;
}

void AudioInterface_freeExt(AudioInterface *interf, const Allocator *alloc) {
	(void) interf; (void) alloc;
	//Nothing to clean.
}

Bool AudioInterface_getDeviceInfos(
	const AudioInterface *interf,
	const Allocator *alloc,
	ListAudioDeviceInfo *infos,
	Error *e_rr
) {

	(void) interf;

	Bool s_uccess = true;
	const ALCchar *devicesStr = NULL;
	const ALCchar *extensionsStr = NULL;
	ALCdevice *device = NULL;

	ListCharString strings = (ListCharString) { 0 };

	ALC_PROCESS_ERROR(NULL, extensionsStr = alcGetString(NULL, ALC_EXTENSIONS));

	CharString extensions = CharString_createRefCStrConst(extensionsStr);

	CharStringSplit split = (CharStringSplit) {
		.s = &extensions,
		.allocator = alloc,
		.result = &strings
	};

	gotoIfError3(clean, CharString_splitSensitive(&split, ' ', e_rr));

	Bool containsEnumAll = false;

	for(U64 i = 0; i < strings.length; ++i)
		if (CharString_equalsCStringSensitive(&strings.ptr[i], "ALC_ENUMERATE_ALL_EXT")) {
			containsEnumAll = true;
			break;
		}

	if (containsEnumAll) {
		ALC_PROCESS_ERROR(NULL, devicesStr = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER));
	}

	else ALC_PROCESS_ERROR(NULL, devicesStr = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER));

	const C8 *mainOutput = NULL;
	ALC_PROCESS_ERROR(NULL, mainOutput = alcGetString(NULL, ALC_DEFAULT_ALL_DEVICES_SPECIFIER));

	ListCharString_free(&strings, alloc);

	const C8 *ptr = devicesStr;

	U64 i = U64_MAX, prev = 0;
	Bool prevNull = false;
	C8 c = 0;

	U64 j = 0;

	while ((c = ptr[++i]) != '\0' || !prevNull) {

		prevNull = c == '\0';

		if (prevNull) {

			CharString str = CharString_createRefSizedConst(ptr + prev, i - prev, true);
			prev = i + 1;

			if (CharString_length(str) > 95)
				Log_warnLn(alloc, "OpenAL: truncating device name \"%s\", strings are limited to 95", str.ptr);

			//We have to open and close a device to query its extensions

			ALC_PROCESS_ERROR(NULL, device = alcOpenDevice(str.ptr));
			ALC_PROCESS_ERROR(device, extensionsStr = alcGetString(device, ALC_EXTENSIONS));

			extensions = CharString_createRefCStrConst(extensionsStr);

			gotoIfError3(clean, CharString_splitSensitive(&split, ' ', e_rr));
			
			AudioDeviceInfo info = (AudioDeviceInfo) { .id = j };

			if (CharString_equalsCStringSensitive(&str, mainOutput) || (!mainOutput && !i))
				info.flags |= EAudioDeviceFlags_MainOutput;

			for(U64 k = 0; k < strings.length; ++k)

				if (CharString_equalsCStringSensitive(&strings.ptr[k], "ALC_EXT_debug"))
					info.flags |= EAudioDeviceFlags_Debug;

				else if (CharString_equalsCStringSensitive(&strings.ptr[k], "AL_EXT_double"))
					info.flags |= EAudioDeviceFlags_HasF64Ext;

				else if (CharString_equalsCStringSensitive(&strings.ptr[k], "AL_EXT_float32"))
					info.flags |= EAudioDeviceFlags_HasF32Ext;

			ListCharString_free(&strings, alloc);

			Buffer_memcpy(Buffer_createRef(info.name, sizeof(info.name)), CharString_bufferConst(str));

			if (CharString_length(str) > 95)
				info.name[95] = '\0';

			gotoIfError3(clean, ListAudioDeviceInfo_pushBack(infos, info, alloc, e_rr));

			if (alcCloseDevice(device) != ALC_TRUE)
				retError(clean, Error_invalidState(0, "AudioInterface_getDeviceInfos() alcCloseDevice failed"));

			device = NULL;
			++j;
		}
	}

clean:

	if(device)
		alcCloseDevice(device);

	ListCharString_free(&strings, alloc);
	return s_uccess;
}
