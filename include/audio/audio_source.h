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
#include "types/math/vec2.h"
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct AudioModifier {
	F32 gain, pitch;
} AudioModifier;

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

	AudioDeviceRef *device;
	AudioStreamRef *stream;

	AudioPoint3D point;

	AudioModifier modifier;

	Bool spatialAudio;
	U8 dirtyMask, padding[6];

} AudioSource;

typedef RefPtr AudioSourceRef;

static inline AudioSource *AudioSourceRef_ptr(AudioSourceRef *ptr) { return RefPtr_data(ptr, AudioSource); }
static inline void *AudioSource_extVoid(AudioSource *src) { return !src ? NULL : (src + 1); }

#define AudioSource_ext(ptr, T) (T##AudioSource*)(AudioSource_extVoid(ptr))

RefPtrType AudioSource_makeType(const Allocator *alloc);

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

Bool AudioSourceRef_updateGain(AudioSourceRef *source, F32 gain, const Allocator *alloc, Error *e_rr);

Bool AudioSourceRef_updatePosition2D(AudioSourceRef *source, F32x2 pos, const Allocator *alloc, Error *e_rr);
Bool AudioSourceRef_updatePosition3D(AudioSourceRef *source, F32x4 pos, const Allocator *alloc, Error *e_rr);

Bool AudioSourceRef_updateVelocity2D(AudioSourceRef *source, F32x2 velocity, const Allocator *alloc, Error *e_rr);
Bool AudioSourceRef_updateVelocity3D(AudioSourceRef *source, F32x4 velocity, const Allocator *alloc, Error *e_rr);

Bool AudioSourceRef_updatePoint2D(AudioSourceRef *source, AudioPoint2D point, const Allocator *alloc, Error *e_rr);
Bool AudioSourceRef_updatePoint3D(AudioSourceRef *source, AudioPoint3D point, const Allocator *alloc, Error *e_rr);

//Pitch and thus modifier is only available for AudioBuffer sources, but aren't supported for AudioStreams.
//This is because AudioStream is the one that manages their own pitch.

Bool AudioSourceRef_updatePitchExt(AudioSourceRef *source, F32 pitch, const Allocator *alloc, Error *e_rr);
Bool AudioSourceRef_updateModifierExt(AudioSourceRef *source, AudioModifier modifier, const Allocator *alloc, Error *e_rr);

#ifdef __cplusplus
	}
#endif
