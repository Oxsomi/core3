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

#pragma once
#include "types/container/list.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Error Error;
typedef struct RefPtr RefPtr;
typedef RefPtr AudioInterfaceRef;

typedef enum EAudioDeviceFlags {
	EAudioDeviceFlags_None			= 0,
	EAudioDeviceFlags_MainOutput	= 1 << 0,
	EAudioDeviceFlags_Debug			= 1 << 1
} EAudioDeviceFlags;

typedef struct AudioDeviceInfo {

	C8 name[256];

	U64 id;

	void *ext;

	EAudioDeviceFlags flags;
	U32 padding;

} AudioDeviceInfo;

TList(AudioDeviceInfo);

typedef struct AudioDevice {

	AudioInterfaceRef *interf;

	AudioDeviceInfo info;

	//TODO: SpinLock
	//TODO: ListAudioStreams

} AudioDevice;

typedef RefPtr AudioDeviceRef;

#define AudioDevice_ext(ptr, T) (!ptr ? NULL : (T##AudioDevice*)(ptr + 1))		//impl
#define AudioDeviceRef_ptr(ptr) RefPtr_data(ptr, AudioDevice)

Error AudioDeviceRef_dec(AudioDeviceRef **device);
Error AudioDeviceRef_inc(AudioDeviceRef *device);

void AudioDeviceInfo_print(AudioDeviceInfo info, Allocator alloc);

Bool AudioDeviceRef_create(
	AudioInterfaceRef *interfRef,
	const AudioDeviceInfo *info,
	Allocator alloc,
	AudioDeviceRef **device,
	Error *e_rr
);

Bool AudioDeviceRef_createx(
	AudioInterfaceRef *interfRef,
	const AudioDeviceInfo *info,
	AudioDeviceRef **device,
	Error *e_rr
);

void AudioDeviceInfo_printx(AudioDeviceInfo info);

#ifdef __cplusplus
	}
#endif
