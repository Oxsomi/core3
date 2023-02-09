/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#pragma once
#include "math/math.h"
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

//

#if _SIMD == SIMD_NEON

	#error Compiling NEON isn't supported

#elif _SIMD == SIMD_SSE

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
