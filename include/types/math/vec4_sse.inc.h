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

//types/math/vec4_sse.inc.h

#pragma once
#ifndef VEC4_SSE_GUARD
	#error Vec4 SSE guard was undefined, this likely indicates include of vec4_sse.h was attempted instead of vec4.h
#endif

#include <emmintrin.h>

//vec3 and vec4 can be represented using 4-element vectors,
//These are a lot faster than just doing them manually.

typedef __m128i I32x4;
typedef __m128  F32x4;

#define vecShufflei(a, x, y, z, w) _mm_shuffle_epi32(a, _MM_SHUFFLE(w, z, y, x))
#define vecShufflef(a, x, y, z, w) _mm_shuffle_ps(a, a, _MM_SHUFFLE(w, z, y, x))

static inline F32x4 F32x4_zero() { return _mm_setzero_ps(); }
static inline F32x4 F32x4_xxxx4(F32 x) { return _mm_set_ps1(x); }

static inline F32x4 F32x4_create1(F32 x) { return _mm_set_ps(0, 0, 0, x); }
static inline F32x4 F32x4_create2(F32 x, F32 y) { return _mm_set_ps(0, 0, y, x); }
static inline F32x4 F32x4_create3(F32 x, F32 y, F32 z) { return _mm_set_ps(0, z, y, x); }
static inline F32x4 F32x4_create4(F32 x, F32 y, F32 z, F32 w) { return _mm_set_ps(w, z, y, x); }

static inline I32x4 I32x4_zero() { return _mm_setzero_si128(); }
static inline I32x4 I32x4_xxxx4(I32 x) { return _mm_set1_epi32(x); }

static inline I32x4 I32x4_create1(I32 x) { return _mm_set_epi32(0, 0, 0, x); }
static inline I32x4 I32x4_create2(I32 x, I32 y) { return _mm_set_epi32(0, 0, y, x); }
static inline I32x4 I32x4_create3(I32 x, I32 y, I32 z) { return _mm_set_epi32(0, z, y, x); }
static inline I32x4 I32x4_create4(I32 x, I32 y, I32 z, I32 w) { return _mm_set_epi32(w, z, y, x); }
