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
#include "types/base/types.h"
#include "types/base/type_id.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum EAudioTypeId {
	EAudioTypeId_AudioInterface			= makeObjectId(0x1C34, 0, 0),
	EAudioTypeId_AudioDevice			= makeObjectId(0x1C34, 1, 0),
	EAudioTypeId_AudioSource			= makeObjectId(0x1C34, 2, 0),
	EAudioTypeId_AudioListener			= makeObjectId(0x1C34, 3, 0),
	EAudioTypeId_AudioBuffer			= makeObjectId(0x1C34, 4, 0),
	EAudioTypeId_AudioStream			= makeObjectId(0x1C34, 5, 0)
} EAudioTypeId;

typedef enum EAudioApi {
	EAudioApi_OpenAL,
	EAudioApi_Count
} EAudioApi;

extern const C8 *EAudioApi_name[EAudioApi_Count];

typedef struct AudioInterface {
	EAudioApi api;
	U32 apiVersion;		//OXC3_MAKE_VERSION
} AudioInterface;

typedef struct Allocator Allocator;
typedef struct Error Error;

typedef struct RefPtr RefPtr;
typedef RefPtr AudioInterfaceRef;

typedef struct ListAudioDeviceInfo ListAudioDeviceInfo;
typedef enum EAudioDeviceFlags EAudioDeviceFlags;
typedef struct AudioDeviceInfo AudioDeviceInfo;

#define AudioInterface_ext(ptr, T) (!ptr ? NULL : (T##AudioInterface*)(ptr + 1))		//impl
#define AudioInterfaceRef_ptr(ptr) RefPtr_data(ptr, AudioInterface)

Error AudioInterfaceRef_dec(AudioInterfaceRef **interf);
Error AudioInterfaceRef_inc(AudioInterfaceRef *interf);

Bool AudioInterface_create(AudioInterfaceRef **interf, Allocator alloc, Error *e_rr);
Bool AudioInterface_createx(AudioInterfaceRef **interf, Error *e_rr);

impl Bool AudioInterface_getDeviceInfos(
	const AudioInterface *interf,
	Allocator alloc,
	ListAudioDeviceInfo *infos,
	Error *e_rr
);

Bool AudioInterface_getPreferredDevice(
	const AudioInterface *interf,
	EAudioDeviceFlags requiredCapabilities,
	Allocator alloc,
	AudioDeviceInfo *deviceInfo,
	Error *e_rr
);

Bool AudioInterface_getDeviceInfosx(
	const AudioInterface *interf,
	ListAudioDeviceInfo *infos,
	Error *e_rr
);

Bool AudioInterface_getPreferredDevicex(
	const AudioInterface *interf,
	EAudioDeviceFlags requiredCapabilities,
	AudioDeviceInfo *deviceInfo,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif