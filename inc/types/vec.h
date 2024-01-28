/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/math.h"
#include <stdalign.h>

//Helper function to insert a simple non SIMD operation
//Useful if there's no SIMD function that's faster than native

#define _NONE_OP_SELF_T(T, N, ...) {        \
                                            \
	T res = (T) { 0 };                      \
	                                        \
	for (U8 i = 0; i < N; ++i)              \
		T##_set(&res, i, (__VA_ARGS__));    \
	                                        \
	return res;                             \
}

#define _NONE_OP4I(...) _NONE_OP_SELF_T(I32x4, 4, __VA_ARGS__)
#define _NONE_OP4F(...) _NONE_OP_SELF_T(F32x4, 4, __VA_ARGS__)
#define _NONE_OP2I(...) _NONE_OP_SELF_T(I32x2, 2, __VA_ARGS__)
#define _NONE_OP2F(...) _NONE_OP_SELF_T(F32x2, 2, __VA_ARGS__)

//Helper function to expand switch case

#define _expand2(offset, func, var)						\
		case offset:		return func(var, offset);		\
		case offset + 1:	return func(var, offset + 1)

#define _expand4(offset, func, var) _expand2(offset, func, var); _expand2(offset + 2, func, var)
#define _expand8(offset, func, var) _expand4(offset, func, var); _expand4(offset + 4, func, var)
#define _expand16(offset, func, var) _expand8(offset, func, var); _expand8(offset + 8, func, var)
#define _expand32(offset, func, var) _expand16(offset, func, var); _expand16(offset + 16, func, var)
#define _expand64(offset, func, var) _expand32(offset, func, var); _expand32(offset + 32, func, var)

//

#if _SIMD == SIMD_NEON

	#error Compiling NEON isn't supported

#elif _SIMD == SIMD_SSE

	#include <intrin.h>
	#include <immintrin.h>
	#include <xmmintrin.h>
	#include <emmintrin.h>
	#include <smmintrin.h>
	
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
	
	#define _shufflei(a, x, y, z, w) _mm_shuffle_epi32(a, _MM_SHUFFLE(w, z, y, x))
	#define _shufflef(a, x, y, z, w) _mm_shuffle_ps(a, a, _MM_SHUFFLE(w, z, y, x))

#else

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
	
	#define _shufflei(a, x, y, z, w) (I32x4){ { a.v[x], a.v[y], a.v[z], a.v[w] } }
	#define _shufflef(a, x, y, z, w) (F32x4){ { a.v[x], a.v[y], a.v[z], a.v[w] } }

#endif

#include "vec4i.h"
#include "vec4f.h"
#include "vec2i.h"
#include "vec2f.h"
