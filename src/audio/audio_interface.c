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

//audio/audio_interface.c

#include "types/container/list_impl.h"
#include "audio/audio_interface.h"
#include "audio/audio_device.h"
#include "types/container/ref_ptr.h"
#include "types/container/log.h"
#include "types/base/error.h"
#include "types/base/constants.h"

const C8 *EAudioApi_name[EAudioApi_Count] = { "OpenAL" };

impl extern U32 AudioInterface_sizeExt;
impl Bool AudioInterface_createExt(AudioInterface *interf, Error *e_rr);
impl void AudioInterface_freeExt(AudioInterface *interf, const Allocator *alloc);

RefPtrType AudioInterface_makeType(const Allocator *alloc) {
	return (RefPtrType) {
		.typeId = (TypeId)EAudioTypeId_AudioInterface,
		.length = (U32)(sizeof(AudioInterface) + AudioInterface_sizeExt),
		.alloc = alloc,
		.free = (ObjectFreeFunc)AudioInterface_freeExt
	};
}

Bool AudioInterface_create(AudioInterfaceRef **interf, const Allocator *alloc, const RefPtrType *type, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;

	if(
		!type ||
		type->typeId != (TypeId)EAudioTypeId_AudioInterface ||
		type->length != (U32)(sizeof(AudioInterface) + AudioInterface_sizeExt) ||
		type->free != (ObjectFreeFunc)AudioInterface_freeExt ||
		type->alloc != alloc
	)
		retError(clean, Error_invalidParameter(4, 0, "AudioInterface_create()::type is invalid"));

	gotoIfError3(clean, RefPtr_create(type, interf, e_rr));

	*AudioInterfaceRef_ptr(*interf) = (AudioInterface) {
		.api = EAudioApi_Count
	};

	allocated = true;
	gotoIfError3(clean, AudioInterface_createExt(AudioInterfaceRef_ptr(*interf), e_rr));

clean:

	if(allocated && !s_uccess)
		RefPtr_dec(interf);

	return s_uccess;
}

Bool AudioInterface_getPreferredDevice(
	const AudioInterface *interf,
	EAudioDeviceFlags requiredCapabilities,
	const Allocator *alloc,
	AudioDeviceInfo *deviceInfo,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListAudioDeviceInfo devices = (ListAudioDeviceInfo) { 0 };

	if (!deviceInfo)
		retError(clean, Error_nullPointer(0, "AudioInterface_getPreferredDevice()::deviceInfo is required"));

	gotoIfError3(clean, AudioInterface_getDeviceInfos(interf, alloc, &devices, e_rr));

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

	if (firstSupported == U64_MAX)
		retError(clean, Error_invalidState(
			0, "AudioInterface_getPreferredDevice() no device with req capabilities found"
		));

	*deviceInfo = devices.ptr[firstSupported];

clean:

	if(s_uccess)
		Log_debugLn(alloc, "-- Audio: Default device: %s\n", deviceInfo->name);

	ListAudioDeviceInfo_free(&devices, alloc);
	return s_uccess;
}
