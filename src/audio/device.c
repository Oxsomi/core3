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
#include "audio/device.h"
#include "audio/interface.h"
#include "types/container/ref_ptr.h"
#include "platforms/log.h"
#include "platforms/platform.h"

TListImpl(AudioDeviceInfo);

Error AudioDeviceRef_dec(AudioDeviceRef **dev) {
	return !RefPtr_dec(dev) ?
		Error_invalidOperation(0, "AudioDeviceRef_dec()::dev is invalid") : Error_none();
}

Error AudioDeviceRef_inc(AudioDeviceRef *dev) {
	return !RefPtr_inc(dev) ?
		Error_invalidOperation(0, "AudioDeviceRef_inc()::dev is invalid") : Error_none();
}

impl extern U32 AudioDevice_sizeExt;
impl Bool AudioDevice_createExt(AudioDevice *dev, Error *e_rr);
impl Bool AudioDevice_freeExt(AudioDevice *dev, Allocator alloc);

Bool AudioDevice_free(AudioDevice *dev, Allocator alloc) {

	if(!dev)
		return true;

	Bool s_uccess = AudioDevice_freeExt(dev, alloc);
	s_uccess &= !AudioInterfaceRef_dec(&dev->interf).genericError;

	return s_uccess;
}

Bool AudioDeviceRef_create(
	AudioInterfaceRef *interfRef,
	const AudioDeviceInfo *info,
	Allocator alloc,
	AudioDeviceRef **device,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocated = false;

	if(!info)
		retError(clean, Error_nullPointer(0, "AudioDeviceRef_create()::info is required"))

	gotoIfError2(clean, RefPtr_create(
		(U32)(sizeof(AudioDevice) + AudioDevice_sizeExt),
		alloc,
		(ObjectFreeFunc) AudioDevice_free,
		(ETypeId) EAudioTypeId_AudioDevice,
		device
	))

	allocated = true;

	gotoIfError2(clean, AudioInterfaceRef_inc(interfRef))
	*AudioDeviceRef_ptr(device) = (AudioDevice) {
		.interf = interfRef,
		.info = *info
	};

	gotoIfError3(clean, AudioDevice_createExt(AudioDeviceRef_ptr(device), e_rr))

clean:

	if(allocated && !s_uccess)
		RefPtr_dec(device);

	return s_uccess;
}

void AudioDeviceInfo_print(AudioDeviceInfo info, Allocator alloc) {
	Log_debugLn(
		alloc,
		"%"PRIu64": %s (debug: %s, mainOutput: %s)",
		info.id,
		info.name,
		info.flags & EAudioDeviceFlags_Debug ? "true" : "false",
		info.flags & EAudioDeviceFlags_MainOutput ? "true" : "false"
	);
}

Bool AudioDeviceRef_createx(
	AudioInterfaceRef *interfRef,
	const AudioDeviceInfo *info,
	AudioDeviceRef **device,
	Error *e_rr
) {
	return AudioDeviceRef_create(interfRef, info, Platform_instance->alloc, device, e_rr);
}

void AudioDeviceInfo_printx(AudioDeviceInfo info) {
	AudioDeviceInfo_print(info, Platform_instance->alloc);
}
