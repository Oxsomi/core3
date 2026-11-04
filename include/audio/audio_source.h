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

//audio/audio_source.h

#pragma once
#include "types/container/ref_ptr.h"
#include "types/math/vec4.h"
#include "types/math/vec2.h"
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

//gain is a linear factor on top of the listener's, 1 is unchanged and 0 is silent. pitch is a playback speed factor.
//Creating a source takes pitch 0 as the stream's own pitch, but gain 0 stays 0, so a zeroed modifier is silent.

typedef struct AudioModifier {
	F32 gain, pitch;
} AudioModifier;

//Positions are world space, the same space as the listener. 2D points sit at z = 0 and are still positioned in 3D.
//Velocity is world units per second and only feeds Doppler.

typedef struct AudioPoint2D {
	F32x2 pos, velocity;
} AudioPoint2D;

typedef struct AudioPoint3D {
	F32x4 pos, velocity;
} AudioPoint3D;

//createSource(2D/3D/)(x/) keeps the same stream running at the position.
//If the sounds need to have unique starting points, they should manually be created.

typedef struct Allocator Allocator;

typedef RefPtr AudioDeviceRef;
typedef RefPtr AudioStreamRef;

typedef struct AudioSource {

	AudioDeviceRef *device;          //Reference held by the source
	AudioStreamRef *stream;          //Reference held by the source, keeps the stream alive

	AudioPoint3D point;

	AudioModifier modifier;

	Bool spatialAudio;
	U8 dirtyMask, padding[6];        //Not yet sent to the backend: 1 gain, 2 pitch, 4 position, 8 velocity. Cleared by update

} AudioSource;

typedef RefPtr AudioSourceRef;

static inline AudioSource *AudioSourceRef_ptr(AudioSourceRef *ptr) { return RefPtr_data(ptr, AudioSource); }
static inline void *AudioSource_extVoid(AudioSource *src) { return !src ? NULL : (src + 1); }

#define AudioSource_ext(ptr, T) (T##AudioSource*)(AudioSource_extVoid(ptr))

RefPtrType AudioSource_makeType(const Allocator *alloc);

//sound must have been created on device, one made on another device is refused. *source must be NULL on entry.
// The source adds a reference to both.
//A sound is silent until a source is made for it, and releasing a source mutes the sound without stopping it.
//All sources of one sound drive the same backend voice, so the gain, position and velocity applied last win.
//modifier.pitch must be 0 for a sound, which keeps the stream's pitch. modifier.gain must be finite and at least 0 and
// point must be finite, or the call is refused with an invalid parameter error.
//createSource is not spatial, so its position and velocity setters fail and it plays at the listener, wherever the
// listener is. The 2D and 3D variants are spatial, which needs a mono stream or one made with flattenSound.
//On failure *source is NULL and the sound may have been muted.
Bool AudioDeviceRef_createSource(
	AudioDeviceRef *device,
	AudioStreamRef *sound,
	AudioModifier modifier,
	const Allocator *alloc,
	const RefPtrType *type,
	AudioSourceRef **source,
	Error *e_rr
);

Bool AudioDeviceRef_createSource2D(
	AudioDeviceRef *device,
	AudioStreamRef *sound,
	AudioModifier modifier,
	AudioPoint2D point,
	const Allocator *alloc,
	const RefPtrType *type,
	AudioSourceRef **source,
	Error *e_rr
);

Bool AudioDeviceRef_createSource3D(
	AudioDeviceRef *device,
	AudioStreamRef *sound,
	AudioModifier modifier,
	AudioPoint3D point,
	const Allocator *alloc,
	const RefPtrType *type,
	AudioSourceRef **source,
	Error *e_rr
);

//Setters store the value at once and apply it on the next AudioDeviceRef_update, queueing the source with alloc.
// A value exactly equal to the stored one queues nothing. Any other value does, however small the change.
//They do not lock, so call them from the thread that calls update, or serialize them with it.
//Gain must be finite and at least 0, and positions and velocities must be finite. Anything else is refused with an
// invalid parameter error and nothing is stored, so the backend never gets a value it would refuse.
//Composite setters (modifier, point) check all of their parts first and then apply them in order, so an earlier part
// stays stored if a later one can't be queued.
//Position and velocity setters fail on a source that is not spatial.
Bool AudioSourceRef_updateGain(AudioSourceRef *source, F32 gain, const Allocator *alloc, Error *e_rr);

Bool AudioSourceRef_updatePosition2D(AudioSourceRef *source, F32x2 pos, const Allocator *alloc, Error *e_rr);
Bool AudioSourceRef_updatePosition3D(AudioSourceRef *source, F32x4 pos, const Allocator *alloc, Error *e_rr);

Bool AudioSourceRef_updateVelocity2D(AudioSourceRef *source, F32x2 velocity, const Allocator *alloc, Error *e_rr);
Bool AudioSourceRef_updateVelocity3D(AudioSourceRef *source, F32x4 velocity, const Allocator *alloc, Error *e_rr);

Bool AudioSourceRef_updatePoint2D(AudioSourceRef *source, AudioPoint2D point, const Allocator *alloc, Error *e_rr);
Bool AudioSourceRef_updatePoint3D(AudioSourceRef *source, AudioPoint3D point, const Allocator *alloc, Error *e_rr);

//Every source plays an AudioStream, which owns its pitch from its creation. Setting any other pitch than the
// source's own is refused with an invalid parameter error and changes nothing, in both setters. Setting the same
// pitch is a no-op.

Bool AudioSourceRef_updatePitchExt(AudioSourceRef *source, F32 pitch, const Allocator *alloc, Error *e_rr);
Bool AudioSourceRef_updateModifierExt(AudioSourceRef *source, AudioModifier modifier, const Allocator *alloc, Error *e_rr);

#ifdef __cplusplus
	}
#endif
