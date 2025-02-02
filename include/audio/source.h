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

#pragma once
#include "types/base/types.h"
#include "types/math/vec.h"

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

typedef struct RefPtr RefPtr;
typedef struct Allocator Allocator;

typedef RefPtr AudioDeviceRef;
typedef RefPtr AudioStreamRef;

typedef struct AudioSource {

	AudioDeviceRef *device;
	AudioStreamRef *stream;
	AudioPoint3D point;

	Bool spatialAudio;
	U8 dirtyMask, padding[6];
	AudioModifier modifier;

} AudioSource;

typedef RefPtr AudioSourceRef;

#define AudioSource_ext(ptr, T) (!ptr ? NULL : (T##AudioSource*)(ptr + 1))		//impl
#define AudioSourceRef_ptr(ptr) RefPtr_data(ptr, AudioSource)

Error AudioSourceRef_dec(AudioSourceRef **source);
Error AudioSourceRef_inc(AudioSourceRef *source);

Bool AudioDeviceRef_createSource(
	AudioDeviceRef *device,
	AudioStreamRef *sound,
	AudioModifier modifier,
	Allocator alloc,
	AudioSourceRef **source,
	Error *e_rr
);

Bool AudioDeviceRef_createSourcex(
	AudioDeviceRef *device,
	AudioStreamRef *sound,
	AudioModifier modifier,
	AudioSourceRef **source,
	Error *e_rr
);

Bool AudioDeviceRef_createSource2D(
	AudioDeviceRef *device,
	AudioStreamRef *sound,
	AudioModifier modifier,
	AudioPoint2D point,
	Allocator alloc,
	AudioSourceRef **source,
	Error *e_rr
);

Bool AudioDeviceRef_createSource2Dx(
	AudioDeviceRef *device,
	AudioStreamRef *sound,
	AudioModifier modifier,
	AudioPoint2D point,
	AudioSourceRef **source,
	Error *e_rr
);

Bool AudioDeviceRef_createSource3D(
	AudioDeviceRef *device,
	AudioStreamRef *sound,
	AudioModifier modifier,
	AudioPoint3D point,
	Allocator alloc,
	AudioSourceRef **source,
	Error *e_rr
);

Bool AudioDeviceRef_createSource3Dx(
	AudioDeviceRef *device,
	AudioStreamRef *sound,
	AudioModifier modifier,
	AudioPoint3D point,
	AudioSourceRef **source,
	Error *e_rr
);

Bool AudioSourceRef_updateGain(AudioSourceRef *source, F32 gain, Allocator alloc, Error *e_rr);
Bool AudioSourceRef_updateGainx(AudioSourceRef *source, F32 gain, Error *e_rr);

Bool AudioSourceRef_updatePosition2D(AudioSourceRef *source, F32x2 pos, Allocator alloc, Error *e_rr);
Bool AudioSourceRef_updatePosition3D(AudioSourceRef *source, F32x4 pos, Allocator alloc, Error *e_rr);

Bool AudioSourceRef_updateVelocity2D(AudioSourceRef *source, F32x2 velocity, Allocator alloc, Error *e_rr);
Bool AudioSourceRef_updateVelocity3D(AudioSourceRef *source, F32x4 velocity, Allocator alloc, Error *e_rr);

Bool AudioSourceRef_updatePoint2D(AudioSourceRef *source, AudioPoint2D point, Allocator alloc, Error *e_rr);
Bool AudioSourceRef_updatePoint3D(AudioSourceRef *source, AudioPoint3D point, Allocator alloc, Error *e_rr);

Bool AudioSourceRef_updatePosition2Dx(AudioSourceRef *source, F32x2 pos, Error *e_rr);
Bool AudioSourceRef_updatePosition3Dx(AudioSourceRef *source, F32x4 pos, Error *e_rr);

Bool AudioSourceRef_updateVelocity2Dx(AudioSourceRef *source, F32x2 velocity, Error *e_rr);
Bool AudioSourceRef_updateVelocity3Dx(AudioSourceRef *source, F32x4 velocity, Error *e_rr);

Bool AudioSourceRef_updatePoint2Dx(AudioSourceRef *source, AudioPoint2D point, Error *e_rr);
Bool AudioSourceRef_updatePoint3Dx(AudioSourceRef *source, AudioPoint3D point, Error *e_rr);

//Pitch and thus modifier is only available for AudioBuffer sources, but aren't supported for AudioStreams.
//This is because AudioStream is the one that manages their own pitch.

Bool AudioSourceRef_updatePitchExt(AudioSourceRef *source, F32 pitch, Allocator alloc, Error *e_rr);
Bool AudioSourceRef_updateModifierExt(AudioSourceRef *source, AudioModifier modifier, Allocator alloc, Error *e_rr);
Bool AudioSourceRef_updatePitchExtx(AudioSourceRef *source, F32 pitch, Error *e_rr);
Bool AudioSourceRef_updateModifierExtx(AudioSourceRef *source, AudioModifier modifier, Error *e_rr);

#ifdef __cplusplus
	}
#endif
