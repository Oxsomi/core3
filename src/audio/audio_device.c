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

#include "types/container/list_impl.h"
#include "audio/audio_device.h"
#include "audio/audio_stream.h"
#include "audio/audio_source.h"
#include "audio/audio_interface.h"
#include "types/container/ref_ptr.h"
#include "types/container/log.h"
#include "types/math/vec4f.h"
#include "types/base/time.h"
#include "types/base/thread.h"
#include "types/base/constants.h"

TListImpl(AudioDeviceInfo);

impl extern U32 AudioDevice_sizeExt;
impl Bool AudioDevice_createExt(Bool isDebug, AudioDevice *dev, Error *e_rr);
impl void AudioDevice_freeExt(AudioDevice *dev, const Allocator *alloc);

void AudioDevice_free(AudioDevice *dev, const Allocator *alloc) {

	if(!dev)
		return;

	ListWeakRefPtr_free(&dev->streams, alloc);
	ListWeakRefPtr_free(&dev->pendingSources, alloc);

	AudioDevice_freeExt(dev, alloc);
	RefPtr_dec(&dev->interf);
}

RefPtrType AudioDevice_makeType(const Allocator *alloc) {
	return (RefPtrType) {
		.typeId = (ETypeId) EAudioTypeId_AudioDevice,
		.length = (U32)(sizeof(AudioDevice) + AudioDevice_sizeExt),
		.alloc = alloc,
		.free = (ObjectFreeFunc)AudioDevice_free
	};
}

Bool AudioDeviceRef_create(
	AudioInterfaceRef *interfRef,
	const AudioDeviceInfo *info,
	Bool isDebug,
	const Allocator *alloc,
	const RefPtrType *type,
	AudioDeviceRef **device,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocated = false;

	if (!info || !type)
		retError(clean, Error_nullPointer(!info ? 1 : 4, "AudioDeviceRef_create()::info and type are required"));

	if(
		type->typeId != (ETypeId)EAudioTypeId_AudioDevice ||
		type->length != sizeof(AudioDevice) + AudioDevice_sizeExt ||
		type->free != (ObjectFreeFunc)AudioDevice_free ||
		type->alloc != alloc
	)
		retError(clean, Error_invalidParameter(4, 0, "AudioDeviceRef_create()::type is invalid"));

	gotoIfError3(clean, RefPtr_create(type, device, e_rr));
	allocated = true;

	if(!interfRef || interfRef->refPtrType->typeId != (ETypeId)EAudioTypeId_AudioInterface || !RefPtr_inc(interfRef))
		retError(clean, Error_invalidParameter(0, 0, "AudioDeviceRef_create()::interfRef is invalid"));

	AudioDevice *audioDev = AudioDeviceRef_ptr(*device);

	*audioDev = (AudioDevice) {
		.interf = interfRef,
		.info = *info
	};

	if (F32x4_eqExact4(audioDev->info.up, F32x4_zero()))
		audioDev->info.up = F32x4_create3(0, 1, 0);

	if (F32x4_eqExact4(audioDev->info.forward, F32x4_zero()))
		audioDev->info.forward = F32x4_create3(0, 0, -1);

	gotoIfError3(clean, AudioDevice_createExt(isDebug, audioDev, e_rr));

clean:

	if(allocated && !s_uccess)
		RefPtr_dec(device);

	return s_uccess;
}

void AudioDeviceInfo_print(AudioDeviceInfo info, const Allocator *alloc) {
	Log_debugLn(
		alloc,
		"%"PRIu64": %s (debug: %s, mainOutput: %s, F32: %s, F64: %s, U24: %s, U32: %s)",
		info.id,
		info.name,
		info.flags & EAudioDeviceFlags_Debug ? "true" : "false",
		info.flags & EAudioDeviceFlags_MainOutput ? "true" : "false",
		info.flags & EAudioDeviceFlags_HasF32Ext ? "true" : "false",
		info.flags & EAudioDeviceFlags_HasF64Ext ? "true" : "false",
		info.flags & EAudioDeviceFlags_HasU24Ext ? "true" : "false",
		info.flags & EAudioDeviceFlags_HasU32Ext ? "true" : "false"
	);
}

typedef struct AudioSource AudioSource;

impl Bool AudioDevice_updateListenerTransformExt(AudioDevice *device, Error *e_rr);
impl Bool AudioSource_update(AudioSource *source, Error *e_rr);
impl Bool AudioStream_update(AudioStream *stream, U64 index, const Allocator *alloc, Error *e_rr);

//TODO: On step: gotoIfError3(clean, AudioDeviceRef_updateListenerTransformExt(dev, dirtyMask, e_rr))
//				 then reset dirtyMask
//		Also call dirty sources

Bool AudioDeviceRef_updateListenerTransform(
	AudioDeviceRef *devRef,
	F32x4 position,
	F32x4 forward,
	F32x4 up,
	F32x4 velocity,
	F32 gain,
	U8 dirtyMask,
	Error *e_rr
) {

	Bool s_uccess = true;

	if (!devRef || devRef->refPtrType->typeId != (ETypeId)EAudioTypeId_AudioDevice)
		retError(clean, Error_nullPointer(0, "AudioDeviceRef_updateListenerTransform()::dev is required"));

	AudioDevice *dev = AudioDeviceRef_ptr(devRef);

	if (dirtyMask & 1) {
		dirtyMask &= F32x4_eqApprox4(position, dev->info.position) ? ~1 : U8_MAX;
		dev->info.position = position;
	}

	if (dirtyMask & 2) {
		dirtyMask &= F32x4_eqApprox4(forward, dev->info.forward) ? ~2 : U8_MAX;
		dev->info.forward = forward;
	}

	if (dirtyMask & 4) {
		dirtyMask &= F32x4_eqApprox4(up, dev->info.up) ? ~4 : U8_MAX;
		dev->info.up = up;
	}

	if (dirtyMask & 8) {
		dirtyMask &= F32x4_eqApprox4(velocity, dev->info.velocity) ? ~8 : U8_MAX;
		dev->info.velocity = velocity;
	}

	if (dirtyMask & 16) {
		dirtyMask &= F32_approxEq(gain, dev->info.gain, 0.01f) ? ~16 : U8_MAX;
		dev->info.gain = gain;
	}

	dev->pendingDirtyMask |= dirtyMask;

clean:
	return s_uccess;
}

Bool AudioDeviceRef_update(AudioDeviceRef *devRef, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;
	AudioDevice *dev = NULL;

	if (!devRef || devRef->refPtrType->typeId != (ETypeId)EAudioTypeId_AudioDevice)
		retError(clean, Error_nullPointer(0, "AudioDeviceRef_update()::devRef is required"));

	dev = AudioDeviceRef_ptr(devRef);
	
	acq = SpinLock_lock(&dev->pendingUpdateLock, SECOND);

	if (acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "AudioDeviceRef_update() couldn't acquire device lock in time"));
		
	//Update listener

	gotoIfError3(clean, AudioDevice_updateListenerTransformExt(dev, e_rr));

	//Update sources

	for(U64 i = 0; i < dev->pendingSources.length; ++i)
		gotoIfError3(clean, AudioSource_update(AudioSourceRef_ptr(dev->pendingSources.ptr[i]), e_rr));
			
	gotoIfError3(clean, ListWeakRefPtr_clear(&dev->pendingSources, e_rr));

	//Update streams

	for (U64 i = dev->streams.length - 1; i != U64_MAX; --i)
		gotoIfError3(clean, AudioStream_update(AudioStreamRef_ptr(dev->streams.ptr[i]), i, alloc, e_rr));

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&dev->pendingUpdateLock);

	return s_uccess;
}

Bool AudioDeviceRef_wait(AudioDeviceRef *devRef, Bool waitForLoopingStream, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;
	AudioDevice *dev = NULL;

	if (!devRef || devRef->refPtrType->typeId != (ETypeId)EAudioTypeId_AudioDevice)
		retError(clean, Error_nullPointer(0, "AudioDeviceRef_wait()::devRef is required"));

	dev = AudioDeviceRef_ptr(devRef);

	while (true) {

		Ns lastTime = Time_now();

		gotoIfError3(clean, AudioDeviceRef_update(devRef, alloc, e_rr));
		
		acq = SpinLock_lock(&dev->pendingUpdateLock, SECOND);

		if(acq < ELockAcquire_Success)
			retError(clean, Error_invalidState(0, "AudioDeviceRef_wait() couldn't acquire device lock in time"));

		Bool anyPlayingResource = false;

		for(U64 i = 0; i < dev->streams.length; ++i) {

			AudioStream *stream = AudioStreamRef_ptr(dev->streams.ptr[i]);

			Bool isInfiniteLoop = AudioStreamInfo_isInfiniteLoop(&stream->info);

			//Infinite loop until the stream is manually paused somewhere

			if (isInfiniteLoop && stream->isPlaying && waitForLoopingStream) {
				anyPlayingResource = true;
				break;
			}

			if (!isInfiniteLoop && stream->isPlaying) {
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
