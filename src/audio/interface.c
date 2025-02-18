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

#include "platforms/ext/listx_impl.h"
#include "audio/interface.h"
#include "audio/device.h"
#include "types/container/ref_ptr.h"
#include "types/base/error.h"
#include "platforms/platform.h"

const C8 *EAudioApi_name[EAudioApi_Count] = {
	"OpenAL"
};

Error AudioInterfaceRef_dec(AudioInterfaceRef **interf) {
	return !RefPtr_dec(interf) ?
		Error_invalidOperation(0, "AudioInterfaceRef_dec()::interf is invalid") : Error_none();
}

Error AudioInterfaceRef_inc(AudioInterfaceRef *interf) {
	return !RefPtr_inc(interf) ?
		Error_invalidOperation(0, "AudioInterfaceRef_inc()::interf is invalid") : Error_none();
}

impl extern U32 AudioInterface_sizeExt;
impl Bool AudioInterface_createExt(AudioInterface *interf, Error *e_rr);
impl Bool AudioInterface_freeExt(AudioInterface *interf, Allocator alloc);

Bool AudioInterface_create(AudioInterfaceRef **interf, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;

	gotoIfError2(clean, RefPtr_create(
		(U32)(sizeof(AudioInterface) + AudioInterface_sizeExt),
		alloc,
		(ObjectFreeFunc) AudioInterface_freeExt,
		(ETypeId) EAudioTypeId_AudioInterface,
		interf
	))

	*AudioInterfaceRef_ptr(*interf) = (AudioInterface) {
		.api = EAudioApi_Count
	};

	allocated = true;
	gotoIfError3(clean, AudioInterface_createExt(AudioInterfaceRef_ptr(*interf), e_rr))

clean:

	if(allocated && !s_uccess)
		RefPtr_dec(interf);

	return s_uccess;
}

Bool AudioInterface_getPreferredDevice(
	const AudioInterface *interf,
	EAudioDeviceFlags requiredCapabilities,
	Allocator alloc,
	AudioDeviceInfo *deviceInfo,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListAudioDeviceInfo devices = (ListAudioDeviceInfo) { 0 };

	if(!deviceInfo)
		retError(clean, Error_nullPointer(0, "AudioInterface_getPreferredDevice()::deviceInfo is required"))

	gotoIfError3(clean, AudioInterface_getDeviceInfos(interf, alloc, &devices, e_rr))

	U64 firstSupported = U64_MAX;

	for (U64 i = 0; i < devices.length; ++i) {

		AudioDeviceInfo dev = devices.ptr[i];

		if((dev.flags & requiredCapabilities) != requiredCapabilities)
			continue;

		if (dev.flags & EAudioDeviceFlags_MainOutput) {
			*deviceInfo = dev;
			goto clean;
		}

		firstSupported = i;
	}

	if(firstSupported == U64_MAX)
		retError(clean, Error_invalidState(0, "AudioInterface_getPreferredDevice() no device with req capabilities found"))

	*deviceInfo = devices.ptr[firstSupported];

clean:
	ListAudioDeviceInfo_free(&devices, alloc);
	return s_uccess;
}

Bool AudioInterface_createx(AudioInterfaceRef **interf, Error *e_rr) {
	return AudioInterface_create(interf, Platform_instance->alloc, e_rr);
}

Bool AudioInterface_getDeviceInfosx(
	const AudioInterface *interf,
	ListAudioDeviceInfo *infos,
	Error *e_rr
) {
	return AudioInterface_getDeviceInfos(interf, Platform_instance->alloc, infos, e_rr);
}

Bool AudioInterface_getPreferredDevicex(
	const AudioInterface *interf,
	EAudioDeviceFlags requiredCapabilities,
	AudioDeviceInfo *deviceInfo,
	Error *e_rr
) {
	return AudioInterface_getPreferredDevice(interf, requiredCapabilities, Platform_instance->alloc, deviceInfo, e_rr);
}
