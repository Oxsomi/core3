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
#ifndef VEC4_NEON_GUARD
	#error Vec4 NEON guard was undefined, this likely indicates include of vec4_neon.h was attempted instead of vec4.h
#endif

#include <arm_neon.h>

typedef int32x4_t   I32x4;
typedef float32x4_t F32x4;

static inline F32x4 F32x4_zero() { return vdupq_n_f32(0.0f); }
static inline F32x4 F32x4_xxxx4(F32 x) { return vdupq_n_f32(x); }

static inline F32x4 F32x4_create1(F32 x) {
	F32x4 v = F32x4_zero();
	return vsetq_lane_f32(x, v, 0);
}

static inline F32x4 F32x4_create2(F32 x, F32 y) {
	F32x4 v = F32x4_create1(x);
	return vsetq_lane_f32(y, v, 1);
}

static inline F32x4 F32x4_create3(F32 x, F32 y, F32 z) {
	F32x4 v = F32x4_create2(x, y);
	return vsetq_lane_f32(z, v, 2);
}

static inline F32x4 F32x4_create4(F32 x, F32 y, F32 z, F32 w) {
	F32x4 v = F32x4_create3(x, y, z);
	return vsetq_lane_f32(w, v, 3);
}

static inline I32x4 I32x4_zero() { return vdupq_n_s32(0); }
static inline I32x4 I32x4_xxxx4(I32 x) { return vdupq_n_s32(x); }

static inline I32x4 I32x4_create1(I32 x) {
	I32x4 v = I32x4_zero();
	return vsetq_lane_s32(x, v, 0);
}

static inline I32x4 I32x4_create2(I32 x, I32 y) {
	I32x4 v = I32x4_create1(x);
	return vsetq_lane_s32(y, v, 1);
}

static inline I32x4 I32x4_create3(I32 x, I32 y, I32 z) {
	I32x4 v = I32x4_create2(x, y);
	return vsetq_lane_s32(z, v, 2);
}

static inline I32x4 I32x4_create4(I32 x, I32 y, I32 z, I32 w) {
	I32x4 v = I32x4_create3(x, y, z);
	return vsetq_lane_s32(w, v, 3);
}

#define vecShufflei(a, x, y, z, w) I32x4_create4(vgetq_lane_s32(a, x), vgetq_lane_s32(a, y), vgetq_lane_s32(a, z), vgetq_lane_s32(a, w))
#define vecShufflef(a, x, y, z, w) F32x4_create4(vgetq_lane_f32(a, x), vgetq_lane_f32(a, y), vgetq_lane_f32(a, z), vgetq_lane_f32(a, w))
