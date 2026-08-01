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

//audio/audio_source.c

#include "audio/audio_source.h"
#include "audio/audio_interface.h"
#include "audio/audio_device.h"
#include "audio/audio_stream.h"
#include "types/container/ref_ptr.h"
#include "types/math/vec_cvt.h"
#include "types/base/error.h"
#include "types/base/constants.h"

impl extern U32 AudioSource_sizeExt;
impl Bool AudioSource_createExt(AudioSource *source, const Allocator *alloc, Error *e_rr);
impl void AudioSource_freeExt(AudioSource *source, const Allocator *alloc);

void AudioSource_free(AudioSource *source, const Allocator *alloc) {

	if(!source)
		return;

	AudioSource_freeExt(source, alloc);
	RefPtr_dec(&source->stream);
	RefPtr_dec(&source->device);
}

RefPtrType AudioSource_makeType(const Allocator *alloc) {
	return (RefPtrType) {
		.typeId = (TypeId)EAudioTypeId_AudioSource,
		.length = (U32)(sizeof(AudioSource) + AudioSource_sizeExt),
		.alloc = alloc,
		.free = (ObjectFreeFunc)AudioSource_free
	};
}

Bool AudioDeviceRef_createSourceGeneric(
	AudioDeviceRef *deviceRef,
	AudioStreamRef *soundRef,
	AudioModifier modifier,
	AudioPoint3D point,
	Bool spatialAudio,
	const Allocator *alloc,
	const RefPtrType *type,
	AudioSourceRef **sourceRef,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool madeRef = false;
	ELockAcquire acq = ELockAcquire_Invalid;
	AudioDevice *dev = NULL;

	if(
		!type ||
		type->typeId != (TypeId)EAudioTypeId_AudioSource ||
		type->length != (U32)(sizeof(AudioSource) + AudioSource_sizeExt) ||
		type->free != (ObjectFreeFunc)AudioSource_free ||
		type->alloc != alloc
	)
		retError(clean, Error_invalidParameter(4, 0, "AudioDeviceRef_createSourceGeneric()::type is invalid"));

	if (!deviceRef || deviceRef->refPtrType->typeId != (TypeId)EAudioTypeId_AudioDevice)
		retError(clean, Error_nullPointer(0, "AudioDeviceRef_createSourceGeneric()::deviceRef is required"));

	dev = AudioDeviceRef_ptr(deviceRef);

	if(!soundRef || soundRef->refPtrType->typeId != (TypeId)EAudioTypeId_AudioStream)
		retError(clean, Error_nullPointer(1, "AudioDeviceRef_createSourceGeneric()::soundRef is required"));

	Bool isStream = soundRef && soundRef->refPtrType->typeId == (TypeId)EAudioTypeId_AudioStream;
	AudioStream *stream = AudioStreamRef_ptr(soundRef);

	if(!modifier.pitch)
		modifier.pitch = isStream ? stream->info.pitch : 1;

	else if(isStream)
		retError(clean, Error_invalidParameter(
			1, 0, "AudioDeviceRef_createSourceGeneric() specifying pitch is only allowed for AudioBuffer sources"
		));

	Bool flattenSound = AudioStreamInfo_flattenSound(&stream->info);
	EAudioStreamFormat format = AudioStreamInfo_format(&stream->info);

	if(spatialAudio && !flattenSound && EAudioStreamFormat_getChannels(format) != 1)
		retError(clean, Error_invalidParameter(
			1, 0,
			"AudioDeviceRef_createSourceGeneric() stereo + spatial audio is unsupported.\n"
			"Please enable 'flattenSound' on the stream to turn it into mono automatically or manually split it to two tracks."
		));
		
	gotoIfError3(clean, RefPtr_create(type, sourceRef, e_rr));

	madeRef = true;

	gotoIfError3(clean, RefPtr_inc(deviceRef));

	AudioSource *source = AudioSourceRef_ptr(*sourceRef);

	*source = (AudioSource) {
		.device = deviceRef,
		.modifier = modifier,
		.point = point,
		.spatialAudio = spatialAudio,
		.dirtyMask = 0xF
	};

	gotoIfError3(clean, RefPtr_inc(soundRef));
	source->stream = soundRef;

	gotoIfError3(clean, AudioSource_createExt(source, alloc, e_rr));

	acq = SpinLock_lock(&dev->pendingUpdateLock, SECOND);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "AudioDeviceRef_createSourceGeneric() couldn't acquire device lock in time"));

	gotoIfError3(clean, ListWeakRefPtr_pushBack(&dev->pendingSources, *sourceRef, alloc, e_rr));

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&dev->pendingUpdateLock);
		
	if(madeRef && !s_uccess)
		RefPtr_dec(sourceRef);

	return s_uccess;
}

Bool AudioDeviceRef_createSource(
	AudioDeviceRef *device,
	AudioStreamRef *sound,
	AudioModifier modifier,
	const Allocator *alloc,
	const RefPtrType *type,
	AudioSourceRef **source,
	Error *e_rr
) {
	AudioPoint3D point = (AudioPoint3D) { 0 };
	return AudioDeviceRef_createSourceGeneric(
		device,
		sound,
		modifier,
		point,
		false,
		alloc,
		type,
		source,
		e_rr
	);
}

Bool AudioDeviceRef_createSource3D(
	AudioDeviceRef *device,
	AudioStreamRef *sound,
	AudioModifier modifier,
	AudioPoint3D point,
	const Allocator *alloc,
	const RefPtrType *type,
	AudioSourceRef **source,
	Error *e_rr
) {
	return AudioDeviceRef_createSourceGeneric(
		device,
		sound,
		modifier,
		point,
		true,
		alloc,
		type,
		source,
		e_rr
	);
}

Bool AudioDeviceRef_createSource2D(
	AudioDeviceRef *device,
	AudioStreamRef *sound,
	AudioModifier modifier,
	AudioPoint2D point,
	const Allocator *alloc,
	const RefPtrType *type,
	AudioSourceRef **source,
	Error *e_rr
) {
	AudioPoint3D point3D =  (AudioPoint3D) {
		.pos = F32x4_create2_1(point.pos, 0),
		.velocity = F32x4_create2_1(point.velocity, 0)
	};

	return AudioDeviceRef_createSource3D(
		device,
		sound,
		modifier,
		point3D,
		alloc,
		type,
		source,
		e_rr
	);
}

Bool AudioSourceRef_onUpdate(AudioSource *source, AudioSourceRef *sourceRef, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	AudioDevice *dev = AudioDeviceRef_ptr(source->device);
	ELockAcquire acq = SpinLock_lock(&dev->pendingUpdateLock, SECOND);

	if (acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "AudioDeviceRef_createSourceGeneric() couldn't acquire device lock in time"));

	if(ListWeakRefPtr_contains(dev->pendingSources, sourceRef, 0, NULL))
		goto clean;

	gotoIfError3(clean, ListWeakRefPtr_pushBack(&dev->pendingSources, sourceRef, alloc, e_rr));

clean:
	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&dev->pendingUpdateLock);

	return s_uccess;
}

Bool AudioSourceRef_updateGain(AudioSourceRef *sourceRef, F32 gain, const Allocator *alloc, Error *e_rr) {
	
	Bool s_uccess = true;

	if (!sourceRef || sourceRef->refPtrType->typeId != (TypeId)EAudioTypeId_AudioSource)
		retError(clean, Error_nullPointer(0, "AudioSourceRef_updateGain()::sourceRef is required"));

	AudioSource *source = AudioSourceRef_ptr(sourceRef);

	if(source->modifier.gain != gain) {
		source->modifier.gain = gain;
		source->dirtyMask |= 1;
		gotoIfError3(clean, AudioSourceRef_onUpdate(source, sourceRef, alloc, e_rr));
	}

clean:
	return s_uccess;
}

Bool AudioSourceRef_updatePitchExt(AudioSourceRef *sourceRef, F32 pitch, const Allocator *alloc, Error *e_rr) {
	
	Bool s_uccess = true;

	if (!sourceRef || sourceRef->refPtrType->typeId != (TypeId)EAudioTypeId_AudioSource)
		retError(clean, Error_nullPointer(0, "AudioSourceRef_updatePitchExt()::sourceRef is required"));

	AudioSource *source = AudioSourceRef_ptr(sourceRef);

	//TODO: updating pitch on the stream itself
	if (sourceRef && sourceRef->refPtrType->typeId == (TypeId)EAudioTypeId_AudioStream)
		retError(clean, Error_nullPointer(
			0, "AudioSourceRef_updatePitchExt() is only allowed on sources with an AudioBuffer"
		));

	if(source->modifier.pitch != pitch) {
		source->modifier.pitch = pitch;
		source->dirtyMask |= 2;
		gotoIfError3(clean, AudioSourceRef_onUpdate(source, sourceRef, alloc, e_rr));
	}

clean:
	return s_uccess;
}

Bool AudioSourceRef_updateModifierExt(AudioSourceRef *source, AudioModifier modifier, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	gotoIfError3(clean, AudioSourceRef_updateGain(source, modifier.gain, alloc, e_rr));
	gotoIfError3(clean, AudioSourceRef_updatePitchExt(source, modifier.pitch, alloc, e_rr));

clean:
	return s_uccess;
}

Bool AudioSourceRef_updatePosition3D(AudioSourceRef *sourceRef, F32x4 pos, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!sourceRef || sourceRef->refPtrType->typeId != (TypeId)EAudioTypeId_AudioSource)
		retError(clean, Error_nullPointer(0, "AudioSourceRef_updatePosition3D()::sourceRef is required"));

	AudioSource *source = AudioSourceRef_ptr(sourceRef);

	if(!source->spatialAudio)
		retError(clean, Error_invalidParameter(0, 0, "AudioSourceRef_updatePosition3D()::source->spatialAudio is required"));

	if(F32x4_neqApprox4(source->point.pos, pos)) {
		source->point.pos = pos;
		source->dirtyMask |= 4;
		gotoIfError3(clean, AudioSourceRef_onUpdate(source, sourceRef, alloc, e_rr));
	}

clean:
	return s_uccess;
}

Bool AudioSourceRef_updateVelocity3D(AudioSourceRef *sourceRef, F32x4 velocity, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!sourceRef || sourceRef->refPtrType->typeId != (TypeId)EAudioTypeId_AudioSource)
		retError(clean, Error_nullPointer(0, "AudioSourceRef_updateVelocity3D()::sourceRef is required"));

	AudioSource *source = AudioSourceRef_ptr(sourceRef);

	if (!source->spatialAudio)
		retError(clean, Error_invalidParameter(0, 0, "AudioSourceRef_updateVelocity3D()::source->spatialAudio is required"));

	if(F32x4_neqApprox4(source->point.velocity, velocity)) {
		source->point.velocity = velocity;
		source->dirtyMask |= 8;
		gotoIfError3(clean, AudioSourceRef_onUpdate(source, sourceRef, alloc, e_rr));
	}

clean:
	return s_uccess;
}

Bool AudioSourceRef_updatePoint3D(AudioSourceRef *source, AudioPoint3D point, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	gotoIfError3(clean, AudioSourceRef_updatePosition3D(source, point.pos, alloc, e_rr));
	gotoIfError3(clean, AudioSourceRef_updateVelocity3D(source, point.velocity, alloc, e_rr));

clean:
	return s_uccess;
}

Bool AudioSourceRef_updatePosition2D(AudioSourceRef *source, F32x2 pos, const Allocator *alloc, Error *e_rr) {
	return AudioSourceRef_updatePosition3D(source, F32x4_create2_1(pos, 0), alloc, e_rr);
}

Bool AudioSourceRef_updateVelocity2D(AudioSourceRef *source, F32x2 velocity, const Allocator *alloc, Error *e_rr) {
	return AudioSourceRef_updateVelocity3D(source, F32x4_create2_1(velocity, 0), alloc, e_rr);
}

Bool AudioSourceRef_updatePoint2D(AudioSourceRef *source, AudioPoint2D point, const Allocator *alloc, Error *e_rr) {

	AudioPoint3D point3D = (AudioPoint3D) {
		.pos = F32x4_create2_1(point.pos, 0),
		.velocity = F32x4_create2_1(point.velocity, 0)
	};

	return AudioSourceRef_updatePoint3D(source, point3D, alloc, e_rr);
}
