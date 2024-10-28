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
#include "audio/interface.h"
#include "audio/device.h"
#include "audio/openal_soft/openal_soft.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "platforms/ext/stringx.h"
#include "types/container/string.h"

#include "platforms/log.h"

//Audio interface data itself is mostly unused for OpenAL, because we don't have any functions we need to load.
//Maybe in the future with extensions.
//Currently only useful for querying devices.

U32 AudioInterface_sizeExt = sizeof(ALAudioInterface);

Bool AudioInterface_createExt(AudioInterface *interf, Error *e_rr) {

	Bool s_uccess = true;

	ALCint maj = 1, min = 1;

	AL_PROCESS_ERROR(NULL, alcGetIntegerv(NULL, ALC_MAJOR_VERSION, 1, &maj))
	AL_PROCESS_ERROR(NULL, alcGetIntegerv(NULL, ALC_MINOR_VERSION, 1, &min))

	if(maj < 0 || min < 0 || (maj >> 10) || (min >> 10))
		retError(clean, Error_invalidState(0, "AudioInterface_createExt() OpenAL version is invalid"))

	*interf = (AudioInterface) {
		.api = EAudioApi_OpenAL,
		.apiVersion = OXC3_MAKE_VERSION((U32)maj, (U32)min, 0)
	};

clean:
	return s_uccess;
}

Bool AudioInterface_freeExt(AudioInterface *interface, Allocator alloc) {
	(void) interface; (void) alloc;
	return true;
}

Bool AudioInterface_getDeviceInfos(
	const AudioInterface *interf,
	Allocator alloc,
	ListAudioDeviceInfo *infos,
	Error *e_rr
) {

	(void) interf;

	Bool s_uccess = true;
	const ALCchar *devicesStr = NULL;
	const ALCchar *extensionsStr = NULL;
	ALCdevice *device = NULL;

	ListConstC8 devices = (ListConstC8) { 0 };
	ListCharString strings = (ListCharString) { 0 };

	AL_PROCESS_ERROR(NULL, extensionsStr = alcGetString(NULL, ALC_EXTENSIONS))

	CharString extensions = CharString_createRefCStrConst(extensionsStr);

	gotoIfError2(clean, CharString_splitSensitive(extensions, ' ', alloc, &strings))

	CharString enumAll = CharString_createRefCStrConst("ALC_ENUMERATE_ALL_EXT");
	Bool containsEnumAll = false;

	for(U64 i = 0; i < strings.length; ++i)
		if (CharString_equalsStringSensitive(strings.ptr[i], enumAll)) {
			containsEnumAll = true;
			break;
		}

	if(containsEnumAll)
		AL_PROCESS_ERROR(NULL, devicesStr = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER))

	else AL_PROCESS_ERROR(NULL, devicesStr = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER))

	ListCharString_free(&strings, alloc);

	const C8 *ptr = devicesStr;

	U64 i = U64_MAX, prev = 0;
	Bool prevNull = false;
	C8 c = 0;

	U64 j = 0;

	CharString debug = CharString_createRefCStrConst("ALC_EXT_debug");

	while ((c = ptr[++i]) != '\0' || !prevNull) {

		prevNull = c == '\0';

		if (prevNull) {

			CharString str = CharString_createRefSizedConst(ptr + prev, i - prev, true);
			prev = i + 1;

			if (CharString_length(str) >= 255) {
				Log_warnLn(alloc, "OpenAL: Skipping device \"%s\", strings are limited to 256", str.ptr);
				++j;
				continue;
			}

			AL_PROCESS_ERROR(NULL, device = alcOpenDevice(str.ptr))
			AL_PROCESS_ERROR(device, extensionsStr = alcGetString(device, ALC_EXTENSIONS))

			extensions = CharString_createRefCStrConst(extensionsStr);

			gotoIfError2(clean, CharString_splitSensitive(extensions, ' ', alloc, &strings))
			
			Bool containsDebug = false;

			for(U64 k = 0; k < strings.length; ++k)
				if (CharString_equalsStringSensitive(strings.ptr[k], debug)) {
					containsDebug = true;
					break;
				}

			ListCharString_free(&strings, alloc);

			AudioDeviceInfo info = (AudioDeviceInfo) {
				.id = j,
				.flags = (!j ? EAudioDeviceFlags_MainOutput : 0) | (containsDebug ? EAudioDeviceFlags_Debug : 0)
			};

			Buffer_copy(Buffer_createRef(info.name, sizeof(info.name)), CharString_bufferConst(str));

			gotoIfError2(clean, ListAudioDeviceInfo_pushBack(infos, info, alloc))

			AL_PROCESS_ERROR(device, alcCloseDevice(device))
			device = NULL;
			++j;
		}
	}

clean:

	if(device)
		alcCloseDevice(device);

	ListCharString_free(&strings, alloc);
	ListConstC8_free(&devices, alloc);
	return s_uccess;
}
