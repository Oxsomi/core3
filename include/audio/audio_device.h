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

//audio/audio_device.h

#pragma once
#include "types/container/ref_ptr.h"
#include "types/math/vec4.h"
#include "types/base/lock.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Error Error;
typedef RefPtr AudioInterfaceRef;

//Has*Ext: the device takes that sample format natively, otherwise streams convert it to 16 bit on the CPU.
//Debug is informational; AudioDeviceRef_create goes by its isDebug argument.

typedef enum EAudioDeviceFlags {
	EAudioDeviceFlags_None             = 0,
	EAudioDeviceFlags_MainOutput       = 1 << 0,
	EAudioDeviceFlags_Debug            = 1 << 1,
	EAudioDeviceFlags_HasF32Ext        = 1 << 2,
	EAudioDeviceFlags_HasF64Ext        = 1 << 3,
	EAudioDeviceFlags_HasU24Ext        = 1 << 4,
	EAudioDeviceFlags_HasU32Ext        = 1 << 5
} EAudioDeviceFlags;

//position, forward, up and velocity are the listener's, in world space (right handed, default forward -Z and up +Y).
//Velocity is in world units per second and only feeds Doppler. Gain is a linear factor, 1 is unchanged.

typedef struct AudioDeviceInfo {

	C8 name[96];                       //Null terminated, at most 95 characters. The device is opened by this name

	F32x4 position;
	F32x4 forward;
	F32x4 up;
	F32x4 velocity;

	U64 id;                            //Index in the enumeration that produced it; the device is opened by name
	void *ext;                         //Always NULL, the OpenAL backend sets no handle

	EAudioDeviceFlags flags;
	F32 gain;

} AudioDeviceInfo;

TList(AudioDeviceInfo);

typedef struct AudioDevice {

	AudioDeviceInfo info;              //Also the current listener state, the setters write here

	AudioInterfaceRef *interf;         //Reference held by the device

	U8 pendingDirtyMask;               //Listener bits not yet sent to the backend, same bits as updateListenerTransform
	U8 padding[7];

	//Pending updates

	ListWeakRefPtr streams;            //Playing streams, no references held
	ListWeakRefPtr pendingSources;     //Sources with changes for the next update, no references held

	SpinLock pendingUpdateLock;        //Guards both lists, see AudioDeviceRef_update

} AudioDevice;

typedef RefPtr AudioDeviceRef;

static inline AudioDevice *AudioDeviceRef_ptr(AudioDeviceRef *ptr) { return RefPtr_data(ptr, AudioDevice); }
static inline void *AudioDevice_extVoid(AudioDevice *device) { return !device ? NULL : (device + 1); }

#define AudioDevice_ext(ptr, T) (T##AudioDevice*)(AudioDevice_extVoid(ptr))

void AudioDeviceInfo_print(AudioDeviceInfo info, const Allocator *alloc);

RefPtrType AudioDevice_makeType(const Allocator *alloc);

//Copies info and adds a reference to interfRef, released with the device. *device must be NULL on entry and stays
// NULL on failure. Zero up and forward become +Y and -Z; info.gain is ignored and the device starts at 1.
//isDebug fails the call if the device lacks the backend's debug extension.
//The backend's current context is process wide and each call switches it, so audio calls (create, update, play,
// source and stream creation) must not overlap on different threads, even for different devices.
Bool AudioDeviceRef_create(
	AudioInterfaceRef *interfRef,
	const AudioDeviceInfo *info,
	Bool isDebug,
	const Allocator *alloc,
	const RefPtrType *type,
	AudioDeviceRef **device,
	Error *e_rr
);

//Nothing is sent to the backend until AudioDeviceRef_update. These setters do not lock, so call them from the thread
// that calls update, or serialize them with it.
//Only the arguments selected by dirtyMask are read. Vectors are not normalized. They must be finite and gain must be
// finite and at least 0, or the call is refused with an invalid parameter error and nothing is stored.
//An argument exactly equal to the stored value queues nothing. Any other value does, however small the change.
Bool AudioDeviceRef_updateListenerTransform(
	AudioDeviceRef *dev,
	F32x4 position,
	F32x4 forward,
	F32x4 up,
	F32x4 velocity,
	F32 gain,
	U8 dirtyMask,        //[pos, fwd, up, vel, gain], e.g. pos = & 1, all = 31
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

//There is no audio thread: stream data is read and queued, and listener and source changes applied, only here and
// on the calling thread. Call it before the queued audio runs out (4 buffers of streamLength per stream): a stream that
// underruns stops and counts as finished.
//Takes the device lock (waits up to a second, then fails). Stops at the first failure, so the remaining sources and
// streams wait for the next call. alloc is scratch.
Bool AudioDeviceRef_update(AudioDeviceRef *dev, const Allocator *alloc, Error *e_rr);

//Blocks the calling thread, calling AudioDeviceRef_update about every 100 microseconds, until no stream is playing.
//Streams that loop forever are ignored unless waitForLoopingStream is set, which then only returns once another thread
// stops them (or on failure).
Bool AudioDeviceRef_wait(AudioDeviceRef *dev, Bool waitForLoopingStream, const Allocator *alloc, Error *e_rr);

#ifdef __cplusplus
	}
#endif
