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

#pragma once
#include "types/container/ref_ptr.h"
#include "types/math/vec4.h"
#include "types/base/lock.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Error Error;
typedef RefPtr AudioInterfaceRef;

typedef enum EAudioDeviceFlags {
	EAudioDeviceFlags_None			= 0,
	EAudioDeviceFlags_MainOutput	= 1 << 0,
	EAudioDeviceFlags_Debug			= 1 << 1,
	EAudioDeviceFlags_HasF32Ext		= 1 << 2,
	EAudioDeviceFlags_HasF64Ext		= 1 << 3,
	EAudioDeviceFlags_HasU24Ext		= 1 << 4
} EAudioDeviceFlags;

typedef struct AudioDeviceInfo {

	C8 name[96];

	F32x4 position;
	F32x4 forward;
	F32x4 up;
	F32x4 velocity;

	U64 id;
	void *ext;					//External handle from underlying API

	EAudioDeviceFlags flags;
	F32 gain;

} AudioDeviceInfo;

TList(AudioDeviceInfo);

typedef struct AudioDevice {

	AudioDeviceInfo info;

	AudioInterfaceRef *interf;

	U8 pendingDirtyMask;
	U8 padding[7];

	//Pending updates

	SpinLock pendingUpdateLock;
	ListWeakRefPtr streams;
	ListWeakRefPtr pendingSources;

} AudioDevice;

typedef RefPtr AudioDeviceRef;

static inline AudioDevice *AudioDeviceRef_ptr(AudioDeviceRef *ptr) { return RefPtr_data(ptr, AudioDevice); }
static inline void *AudioDevice_extVoid(AudioDevice *device) { return !device ? NULL : (device + 1); }

#define AudioDevice_ext(ptr, T) (T##AudioDevice*)(AudioDevice_extVoid(ptr))

void AudioDeviceInfo_print(AudioDeviceInfo info, const Allocator *alloc);

RefPtrType AudioDevice_makeType(const Allocator *alloc);

Bool AudioDeviceRef_create(
	AudioInterfaceRef *interfRef,
	const AudioDeviceInfo *info,
	Bool isDebug,
	const Allocator *alloc,
	const RefPtrType *type,
	AudioDeviceRef **device,
	Error *e_rr
);

Bool AudioDeviceRef_updateListenerTransform(
	AudioDeviceRef *dev,
	F32x4 position,
	F32x4 forward,
	F32x4 up,
	F32x4 velocity,
	F32 gain,
	U8 dirtyMask,		//[pos, fwd, up, vel, gain], e.g. pos = & 1, all = 31
	Error *e_rr
);

static inline Bool AudioDeviceRef_updateListenerPosition(AudioDeviceRef *dev, F32x4 pos, Error *e_rr) {
	return AudioDeviceRef_updateListenerTransform(dev, pos, F32x4_zero(), F32x4_zero(), F32x4_zero(), 0, 1, e_rr);
}

static inline Bool AudioDeviceRef_updateListenerForward(AudioDeviceRef *dev, F32x4 fwd, Error *e_rr) {
	return AudioDeviceRef_updateListenerTransform(dev, F32x4_zero(), fwd, F32x4_zero(), F32x4_zero(), 0, 2, e_rr);
}

static inline Bool AudioDeviceRef_updateListenerUp(AudioDeviceRef *dev, F32x4 up, Error *e_rr) {
	return AudioDeviceRef_updateListenerTransform(dev, F32x4_zero(), F32x4_zero(), up, F32x4_zero(), 0, 4, e_rr);
}

static inline Bool AudioDeviceRef_updateListenerOrientation(AudioDeviceRef *dev, F32x4 fwd, F32x4 up, Error *e_rr) {
	return AudioDeviceRef_updateListenerTransform(dev, F32x4_zero(), fwd, up, F32x4_zero(), 0, 6, e_rr);
}

static inline Bool AudioDeviceRef_updateListenerVelocity(AudioDeviceRef *dev, F32x4 vel, Error *e_rr) {
	return AudioDeviceRef_updateListenerTransform(dev, F32x4_zero(), F32x4_zero(), F32x4_zero(), vel, 0, 8, e_rr);
}

static inline Bool AudioDeviceRef_updateListenerGain(AudioDeviceRef *dev, F32 gain, Error *e_rr) {
	return AudioDeviceRef_updateListenerTransform(dev, F32x4_zero(), F32x4_zero(), F32x4_zero(), F32x4_zero(), gain, 16, e_rr);
}

Bool AudioDeviceRef_update(AudioDeviceRef *dev, const Allocator *alloc, Error *e_rr);
Bool AudioDeviceRef_wait(AudioDeviceRef *dev, Bool waitForLoopingStream, const Allocator *alloc, Error *e_rr);

#ifdef __cplusplus
	}
#endif
