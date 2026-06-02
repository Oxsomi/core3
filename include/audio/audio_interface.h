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

//audio/audio_interface.h

#pragma once
#include "types/container/ref_ptr.h"
#include "types/base/types.h"
#include "types/base/type_id.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum EAudioTypeId {
	EAudioTypeId_AudioInterface         = makeObjectId(0x1C35, 0, 0),
	EAudioTypeId_AudioDevice            = makeObjectId(0x1C35, 1, 0),
	EAudioTypeId_AudioStream            = makeObjectId(0x1C35, 2, 0),
	EAudioTypeId_AudioSource            = makeObjectId(0x1C35, 3, 0)
} EAudioTypeId;

typedef enum EAudioApi {
	EAudioApi_OpenAL,
	EAudioApi_Count
} EAudioApi;

extern const C8 *EAudioApi_name[EAudioApi_Count];

typedef struct AudioInterface {
	EAudioApi api;
	U32 apiVersion;        //OXC3_MAKE_VERSION
	U64 padding;           //Ensures ext data (immediately following RefPtr) is 16-byte aligned regardless of implementation
} AudioInterface;

typedef struct Allocator Allocator;
typedef struct Error Error;

typedef RefPtr AudioInterfaceRef;

typedef struct ListAudioDeviceInfo ListAudioDeviceInfo;
typedef enum EAudioDeviceFlags EAudioDeviceFlags;
typedef struct AudioDeviceInfo AudioDeviceInfo;

static inline AudioInterface *AudioInterfaceRef_ptr(AudioInterfaceRef *ptr) { return RefPtr_data(ptr, AudioInterface); }
static inline void *AudioInterface_extVoid(AudioInterface *interf) { return !interf ? NULL : (interf + 1); }

#define AudioInterface_ext(ptr, T) (T##AudioInterface*)(AudioInterface_extVoid(ptr))

RefPtrType AudioInterface_makeType(const Allocator *alloc);

Bool AudioInterface_create(AudioInterfaceRef **interf, const Allocator *alloc, const RefPtrType *type, Error *e_rr);

impl Bool AudioInterface_getDeviceInfos(
	const AudioInterface *interf,
	const Allocator *alloc,
	ListAudioDeviceInfo *infos,
	Error *e_rr
);

Bool AudioInterface_getPreferredDevice(
	const AudioInterface *interf,
	EAudioDeviceFlags requiredCapabilities,
	const Allocator *alloc,
	AudioDeviceInfo *deviceInfo,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif