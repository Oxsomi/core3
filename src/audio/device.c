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
#include "audio/device.h"
#include "audio/stream.h"
#include "audio/source.h"
#include "audio/interface.h"
#include "types/container/ref_ptr.h"
#include "platforms/log.h"
#include "platforms/platform.h"
#include "types/base/time.h"
#include "types/base/thread.h"

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
impl Bool AudioDevice_createExt(Bool isDebug, AudioDevice *dev, Error *e_rr);
impl Bool AudioDevice_freeExt(AudioDevice *dev, Allocator alloc);

Bool AudioDevice_free(AudioDevice *dev, Allocator alloc) {

	if(!dev)
		return true;

	ListWeakRefPtr_free(&dev->streams, alloc);
	ListWeakRefPtr_free(&dev->pendingSources, alloc);

	Bool s_uccess = AudioDevice_freeExt(dev, alloc);
	s_uccess &= !AudioInterfaceRef_dec(&dev->interf).genericError;

	return s_uccess;
}

Bool AudioDeviceRef_create(
	AudioInterfaceRef *interfRef,
	const AudioDeviceInfo *info,
	Bool isDebug,
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

	AudioDevice *audioDev = AudioDeviceRef_ptr(*device);

	*audioDev = (AudioDevice) {
		.interf = interfRef,
		.info = *info
	};

	if(F32x4_eq4(audioDev->info.up, F32x4_zero()))
		audioDev->info.up = F32x4_create3(0, 1, 0);

	if(F32x4_eq4(audioDev->info.forward, F32x4_zero()))
		audioDev->info.forward = F32x4_create3(0, 0, 1);

	gotoIfError3(clean, AudioDevice_createExt(isDebug, audioDev, e_rr))

clean:

	if(allocated && !s_uccess)
		RefPtr_dec(device);

	return s_uccess;
}

void AudioDeviceInfo_print(AudioDeviceInfo info, Allocator alloc) {
	Log_debugLn(
		alloc,
		"%"PRIu64": %s (debug: %s, mainOutput: %s, F32: %s, F64: %s, U24: %s)",
		info.id,
		info.name,
		info.flags & EAudioDeviceFlags_Debug ? "true" : "false",
		info.flags & EAudioDeviceFlags_MainOutput ? "true" : "false",
		info.flags & EAudioDeviceFlags_HasF32Ext ? "true" : "false",
		info.flags & EAudioDeviceFlags_HasF64Ext ? "true" : "false",
		info.flags & EAudioDeviceFlags_HasU24Ext ? "true" : "false"
	);
}

Bool AudioDeviceRef_createx(
	AudioInterfaceRef *interfRef,
	const AudioDeviceInfo *info,
	Bool isDebug,
	AudioDeviceRef **device,
	Error *e_rr
) {
	return AudioDeviceRef_create(interfRef, info, isDebug, Platform_instance->alloc, device, e_rr);
}

void AudioDeviceInfo_printx(AudioDeviceInfo info) {
	AudioDeviceInfo_print(info, Platform_instance->alloc);
}

typedef struct AudioSource AudioSource;

impl Bool AudioDevice_updateListenerTransformExt(AudioDevice *device, Error *e_rr);
impl Bool AudioSource_update(AudioSource *source, Error *e_rr);
impl Bool AudioStream_update(AudioStream *stream, U64 index, Allocator alloc, Error *e_rr);

//TODO: On step: gotoIfError3(clean, AudioDeviceRef_updateListenerTransformExt(dev, dirtyMask, e_rr))
//				 then reset dirtyMask
//		Also call dirty sources

Bool AudioDeviceRef_updateListenerTransform(
	AudioDeviceRef *devRef,
	F32x4 position,
	F32x4 forward,
	F32x4 up,
	F32x4 velocity,
	U8 dirtyMask,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!devRef || devRef->typeId != (ETypeId) EAudioTypeId_AudioDevice)
		retError(clean, Error_nullPointer(0, "AudioDeviceRef_updateListenerTransform()::dev is required"))

	AudioDevice *dev = AudioDeviceRef_ptr(devRef);

	if (dirtyMask & 1) {
		dirtyMask &= F32x4_neq4(position, dev->info.position) ? 1 : 0;
		dev->info.position = position;
	}

	if (dirtyMask & 2) {
		dirtyMask &= F32x4_neq4(forward, dev->info.forward) ? 2 : 0;
		dev->info.forward = forward;
	}

	if (dirtyMask & 4) {
		dirtyMask &= F32x4_neq4(up, dev->info.up) ? 4 : 0;
		dev->info.up = up;
	}

	if (dirtyMask & 8) {
		dirtyMask &= F32x4_neq4(velocity, dev->info.velocity) ? 8 : 0;
		dev->info.velocity = velocity;
	}

	dev->pendingDirtyMask |= dirtyMask;

clean:
	return s_uccess;
}

Bool AudioDeviceRef_updateListenerPosition(AudioDeviceRef *dev, F32x4 pos, Error *e_rr) {
	return AudioDeviceRef_updateListenerTransform(dev, pos, F32x4_zero(), F32x4_zero(), F32x4_zero(), 1, e_rr);
}

Bool AudioDeviceRef_updateListenerForward(AudioDeviceRef *dev, F32x4 fwd, Error *e_rr) {
	return AudioDeviceRef_updateListenerTransform(dev, F32x4_zero(), fwd, F32x4_zero(), F32x4_zero(), 2, e_rr);
}

Bool AudioDeviceRef_updateListenerUp(AudioDeviceRef *dev, F32x4 up, Error *e_rr) {
	return AudioDeviceRef_updateListenerTransform(dev, F32x4_zero(), F32x4_zero(), up, F32x4_zero(), 4, e_rr);
}

Bool AudioDeviceRef_updateListenerOrientation(AudioDeviceRef *dev, F32x4 fwd, F32x4 up, Error *e_rr) {
	return AudioDeviceRef_updateListenerTransform(dev, F32x4_zero(), fwd, up, F32x4_zero(), 6, e_rr);
}

Bool AudioDeviceRef_updateListenerVelocity(AudioDeviceRef *dev, F32x4 vel, Error *e_rr) {
	return AudioDeviceRef_updateListenerTransform(dev, F32x4_zero(), F32x4_zero(), F32x4_zero(), vel, 8, e_rr);
}

Bool AudioDeviceRef_update(AudioDeviceRef *devRef, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;
	AudioDevice *dev = NULL;

	if(!devRef || devRef->typeId != (ETypeId) EAudioTypeId_AudioDevice)
		retError(clean, Error_nullPointer(0, "AudioDeviceRef_update()::devRef is required"))

	dev = AudioDeviceRef_ptr(devRef);
	
	acq = SpinLock_lock(&dev->pendingUpdateLock, SECOND);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "AudioDeviceRef_update() couldn't acquire device lock in time"))
		
	//Update listener

	gotoIfError3(clean, AudioDevice_updateListenerTransformExt(dev, e_rr))

	//Update sources

	for(U64 i = 0; i < dev->pendingSources.length; ++i)
		gotoIfError3(clean, AudioSource_update(AudioSourceRef_ptr(dev->pendingSources.ptr[i]), e_rr))
			
	ListWeakRefPtr_clear(&dev->pendingSources);

	//Update streams

	for(U64 i = dev->streams.length - 1; i != U64_MAX; --i)
		gotoIfError3(clean, AudioStream_update(AudioStreamRef_ptr(dev->streams.ptr[i]), i, alloc, e_rr))

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&dev->pendingUpdateLock);

	return s_uccess;
}

Bool AudioDeviceRef_wait(AudioDeviceRef *devRef, Bool waitForLoopingStream, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;
	AudioDevice *dev = NULL;

	if(!devRef || devRef->typeId != (ETypeId) EAudioTypeId_AudioDevice)
		retError(clean, Error_nullPointer(0, "AudioDeviceRef_wait()::devRef is required"))

	dev = AudioDeviceRef_ptr(devRef);

	while (true) {

		Ns lastTime = Time_now();

		gotoIfError3(clean, AudioDeviceRef_update(devRef, alloc, e_rr))
		
		acq = SpinLock_lock(&dev->pendingUpdateLock, SECOND);

		if(acq < ELockAcquire_Success)
			retError(clean, Error_invalidState(0, "AudioDeviceRef_wait() couldn't acquire device lock in time"))

		Bool anyPlayingResource = false;

		for(U64 i = 0; i < dev->streams.length; ++i) {

			AudioStream *stream = AudioStreamRef_ptr(dev->streams.ptr[i]);

			//Infinite loop until the stream is manually paused somewhere

			if (stream->info.isLoop && stream->isPlaying && waitForLoopingStream) {
				anyPlayingResource = true;
				break;
			}

			if (!stream->info.isLoop && stream->isPlaying) {
				anyPlayingResource = true;
				break;
			}
		}

		if(!anyPlayingResource)		//Done
			goto clean;
			
		SpinLock_unlock(&dev->pendingUpdateLock);
		acq = ELockAcquire_Invalid;

		Ns targetTime = 100 * MU;			//Avoid looping too aggressively
		Ns diff = Time_now() - lastTime;

		if(diff < targetTime)
			Thread_sleep(targetTime - diff);
	}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&dev->pendingUpdateLock);

	return s_uccess;
}

Bool AudioDeviceRef_updatex(AudioDeviceRef *dev, Error *e_rr) {
	return AudioDeviceRef_update(dev, Platform_instance->alloc, e_rr);
}

Bool AudioDeviceRef_waitx(AudioDeviceRef *dev, Bool waitForLoopingStream, Error *e_rr) {
	return AudioDeviceRef_wait(dev, waitForLoopingStream, Platform_instance->alloc, e_rr);
}
