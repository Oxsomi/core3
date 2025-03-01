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
#include <stdalign.h>

//Helper function to insert a simple non SIMD operation
//Useful if there's no SIMD function that's faster than native

#define NONE_OP_SELF_T(T, N, ...) {			\
											\
	T res = (T) { 0 };					  	\
											\
	for (U8 i = 0; i < N; ++i)			  	\
		T##_set(&res, i, (__VA_ARGS__));	\
											\
	return res;							 	\
}

#define NONE_OP4I(...) NONE_OP_SELF_T(I32x4, 4, __VA_ARGS__)
#define NONE_OP4F(...) NONE_OP_SELF_T(F32x4, 4, __VA_ARGS__)
#define NONE_OP2I(...) NONE_OP_SELF_T(I32x2, 2, __VA_ARGS__)
#define NONE_OP2F(...) NONE_OP_SELF_T(F32x2, 2, __VA_ARGS__)

//Helper function to expand switch case

#define FUNC_EXPAND2(offset, func, var)						\
		case offset:		return func(var, offset);		\
		case offset + 1:	return func(var, offset + 1)

#define FUNC_EXPAND4(offset, func, var) FUNC_EXPAND2(offset, func, var); FUNC_EXPAND2((offset) + 2, func, var)
#define FUNC_EXPAND8(offset, func, var) FUNC_EXPAND4(offset, func, var); FUNC_EXPAND4((offset) + 4, func, var)
#define FUNC_EXPAND16(offset, func, var) FUNC_EXPAND8(offset, func, var); FUNC_EXPAND8((offset) + 8, func, var)
#define FUNC_EXPAND32(offset, func, var) FUNC_EXPAND16(offset, func, var); FUNC_EXPAND16((offset) + 16, func, var)
#define FUNC_EXPAND64(offset, func, var) FUNC_EXPAND32(offset, func, var); FUNC_EXPAND32((offset) + 32, func, var)

//

#if _SIMD == SIMD_NEON

	#error Compiling NEON isnt supported

#elif _SIMD == SIMD_SSE

	#include <immintrin.h>
	#include <xmmintrin.h>
	#include <smmintrin.h>
	#include <emmintrin.h>
	#include <nmmintrin.h>
	#include <stdalign.h>

	#ifdef __cplusplus
		extern "C" {
	#endif

	//vec3 and vec4 can be represented using 4-element vectors,
	//These are a lot faster than just doing them manually.

	typedef __m128i I32x4;
	typedef __m128  F32x4;

	//vec2s are just regular memory to us, because XMM is legacy and needs a clear everytime vec ops are finished.
	//That's not very user friendly.
	//Unpacking 2-element vectors to 4-element vectors, doing operation and packing them again is slower than normal.
	//This is because the latency of vector operations is bigger than 2x the normal operation.
	//And 2x normal operation might be pipelined better.
	//The only exceptions are expensive operations like trig operations, rounding and sqrt & rsqrt, etc..

	typedef struct I32x2_t {
		alignas(8) I32 v[2];
	} I32x2;

	typedef struct F32x2_t {
		alignas(8) F32 v[2];
	} F32x2;

	//

	#define vecShufflei(a, x, y, z, w) _mm_shuffle_epi32(a, _MM_SHUFFLE(w, z, y, x))
	#define vecShufflef(a, x, y, z, w) _mm_shuffle_ps(a, a, _MM_SHUFFLE(w, z, y, x))

	#ifdef __cplusplus
		}
	#endif

#else

	#ifdef __cplusplus
		extern "C" {
	#endif

	typedef struct I32x4_t {
		alignas(16) I32 v[4];
	} I32x4;

	typedef struct F32x4_t {
		alignas(16) F32 v[4];
	} F32x4;

	typedef struct I32x2_t {
		alignas(8) I32 v[2];
	} I32x2;

	typedef struct F32x2_t {
		alignas(8) F32 v[2];
	} F32x2;

	#define vecShufflei(a, x, y, z, w) (I32x4){ { a.v[x], a.v[y], a.v[z], a.v[w] } }
	#define vecShufflef(a, x, y, z, w) (F32x4){ { a.v[x], a.v[y], a.v[z], a.v[w] } }

	#ifdef __cplusplus
		}
	#endif

#endif

#include "types/math/vec4i.h"
#include "types/math/vec4f.h"
#include "types/math/vec2i.h"
#include "types/math/vec2f.h"
#include "types/base/buffer.h"

BUFFER_OP(I32x2);
BUFFER_OP(F32x2);
//BUFFER_OP(F64x2);		TODO:

BUFFER_OP(I32x4);
BUFFER_OP(F32x4);
//BUFFER_OP(F64x4);		TODO:
