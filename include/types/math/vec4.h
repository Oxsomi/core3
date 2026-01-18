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
#include "types/math/vec_base.h"
#include "types/base/buffer.h"
#include "types/base/platform_types.h"
#include <stdalign.h>

#ifdef __cplusplus
	extern "C" {
#endif

//Helper function to expand switch case

#define FUNC_EXPAND2(offset, func, var)						\
		case offset:		return func(var, offset);		\
		case offset + 1:	return func(var, offset + 1)

#define FUNC_EXPAND4(offset, func, var) FUNC_EXPAND2(offset, func, var); FUNC_EXPAND2((offset) + 2, func, var)
#define FUNC_EXPAND8(offset, func, var) FUNC_EXPAND4(offset, func, var); FUNC_EXPAND4((offset) + 4, func, var)
#define FUNC_EXPAND16(offset, func, var) FUNC_EXPAND8(offset, func, var); FUNC_EXPAND8((offset) + 8, func, var)
#define FUNC_EXPAND32(offset, func, var) FUNC_EXPAND16(offset, func, var); FUNC_EXPAND16((offset) + 16, func, var)
#define FUNC_EXPAND64(offset, func, var) FUNC_EXPAND32(offset, func, var); FUNC_EXPAND32((offset) + 32, func, var)

#if _SIMD == SIMD_NEON
	
	#include <arm_neon.h>

	typedef int32x4_t   I32x4;
	typedef float32x4_t F32x4;

#elif _SIMD == SIMD_SSE

	#include <emmintrin.h>

	//vec3 and vec4 can be represented using 4-element vectors,
	//These are a lot faster than just doing them manually.

	typedef __m128i I32x4;
	typedef __m128  F32x4;

	#define vecShufflei(a, x, y, z, w) _mm_shuffle_epi32(a, _MM_SHUFFLE(w, z, y, x))
	#define vecShufflef(a, x, y, z, w) _mm_shuffle_ps(a, a, _MM_SHUFFLE(w, z, y, x))

#else

	#include <stdalign.h>

	typedef struct I32x4_t {
		alignas(16) I32 v[4];
	} I32x4;

	typedef struct F32x4_t {
		alignas(16) F32 v[4];
	} F32x4;

	#define vecShufflei(a, x, y, z, w) (I32x4){ { a.v[x], a.v[y], a.v[z], a.v[w] } }
	#define vecShufflef(a, x, y, z, w) (F32x4){ { a.v[x], a.v[y], a.v[z], a.v[w] } }

#endif

#ifdef __cplusplus
		}
#endif
