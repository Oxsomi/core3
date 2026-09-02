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

//types/math/vec4i.h

#pragma once
#include "types/base/buffer_base.h"
#include "types/math/vec4.h"

#ifdef __cplusplus
	extern "C" {
#endif

BUFFER_OP_IMPL(I32x4)

#if _SIMD == SIMD_SSE
	#define VEC4I_SSE_GUARD
	#include "types/math/vec4i_sse.inc.h"
#elif _SIMD == SIMD_WASM
	#define VEC4I_WASM_GUARD
	#include "types/math/vec4i_wasm.inc.h"
#elif _SIMD == SIMD_NEON
	#define VEC4I_NEON_GUARD
	#include "types/math/vec4i_neon.inc.h"
#else
	#define VEC4I_NONE_GUARD
	#include "types/math/vec4i_none.inc.h"
#endif

#if _SIMD != SIMD_SSE && _SIMD != SIMD_WASM
	static inline I32x4 I32x4_negate(I32x4 a) { return I32x4_sub(I32x4_zero(), a); }
	static inline I32x4 I32x4_one() { return I32x4_xxxx4(1); }
#endif

static inline I32x4 I32x4_bitsF32x4(F32x4 a) {
	const void *aptr = &a;
	return *(const I32x4*) aptr;
}

//Constants

static inline I32x4 I32x4_two() { return I32x4_xxxx4(2); }
static inline I32x4 I32x4_negOne() { return I32x4_xxxx4(-1); }
static inline I32x4 I32x4_negTwo() { return I32x4_xxxx4(-2); }

//Swizzles and shizzle

#if _SIMD != SIMD_NONE

	static inline void I32x4_setXRef(I32x4 *a, I32 v) { if (a) *a = I32x4_create4(v, I32x4_y(*a), I32x4_z(*a), I32x4_w(*a)); }
	static inline void I32x4_setYRef(I32x4 *a, I32 v) { if (a) *a = I32x4_create4(I32x4_x(*a), v, I32x4_z(*a), I32x4_w(*a)); }
	static inline void I32x4_setZRef(I32x4 *a, I32 v) { if (a) *a = I32x4_create4(I32x4_x(*a), I32x4_y(*a), v, I32x4_w(*a)); }
	static inline void I32x4_setWRef(I32x4 *a, I32 v) { if (a) *a = I32x4_create4(I32x4_x(*a), I32x4_y(*a), I32x4_z(*a), v); }

	static inline I32x4 I32x4_setCopy(I32x4 a, U8 i, I32 v) {
		switch (i & 3) {
			case 0:        return I32x4_setXCopy(a, v);
			case 1:        return I32x4_setYCopy(a, v);
			case 2:        return I32x4_setZCopy(a, v);
			default:    return I32x4_setWCopy(a, v);
		}
	}

	static inline void I32x4_setRef(I32x4 *a, U8 i, I32 v) {
		switch (i & 3) {
			case 0:        I32x4_setXRef(a, v);    break;
			case 1:        I32x4_setYRef(a, v);    break;
			case 2:        I32x4_setZRef(a, v);    break;
			default:    I32x4_setWRef(a, v);
		}
	}

	static inline I32 I32x4_get(I32x4 a, U8 i) {
		switch (i & 3) {
			case 0:        return I32x4_x(a);
			case 1:        return I32x4_y(a);
			case 2:        return I32x4_z(a);
			default:    return I32x4_w(a);
		}
	}

#endif

#if !_SIMD_HAS_SVML
	static inline I32x4 I32x4_div(I32x4 a, I32x4 b) { NONE_OP4I(I32x4_get(a, i) / I32x4_get(b, i)); }
#endif

//Arithmetic

static inline I32x4 I32x4_complement(I32x4 a) { return I32x4_sub(I32x4_one(), a); }

//Rotate left (<<<)
static inline I32x4 I32x4_rol(I32x4 a, U8 bits) {
	bits &= 31;
	return I32x4_or(I32x4_lsh32(a, bits), I32x4_rsh32(a, 32 - bits));
}

//Rotate right (>>>)
static inline I32x4 I32x4_ror(I32x4 a, U8 bits) {
	bits &= 31;
	return I32x4_or(I32x4_rsh32(a, bits), I32x4_lsh32(a, 32 - bits));
}

static inline I32x4 I32x4_pow2(I32x4 a) { return I32x4_mul(a, a); }

static inline I32x4 I32x4_mod(I32x4 v, I32x4 d) {                //UB for any d[x] == 0
	I32x4 r = I32x4_sub(v, I32x4_mul(I32x4_div(v, d), d));
	I32x4 mask = I32x4_lt(r, I32x4_zero());
	return I32x4_add(r, I32x4_mul(mask, d));
}

static inline I32x4 I32x4_clamp(I32x4 a, I32x4 mi, I32x4 ma) { return I32x4_max(mi, I32x4_min(ma, a)); }

static inline I32x4 I32x4_sign(I32x4 v) {
	return I32x4_add(
		I32x4_mul(I32x4_lt(v, I32x4_zero()), I32x4_negTwo()),
		I32x4_one()
	);
}

static inline I32x4 I32x4_abs(I32x4 v) { return I32x4_mul(I32x4_sign(v), v); }

//Boolean

static inline Bool I32x4_all(I32x4 a) { return I32x4_reduce(I32x4_neq(a, I32x4_zero())) == 4; }
static inline Bool I32x4_any(I32x4 a) { return I32x4_reduce(I32x4_neq(a, I32x4_zero())); }

static inline Bool I32x4_eq4(I32x4 a, I32x4 b) { return I32x4_all(I32x4_eq(a, b)); }
static inline Bool I32x4_neq4(I32x4 a, I32x4 b) { return !I32x4_eq4(a, b); }

//Generic helper functions
//Adapted from https://stackoverflow.com/questions/17610696/shift-a-m128i-of-n-bits

static inline I32x4 I32x4_lsh128(I32x4 a, U8 bits) {

	const I32x4 b = a;
	a = I32x4_lshElements(a, 2);

	if (bits >= 64)
		return I32x4_lsh64(a, bits - 64);

	a = I32x4_rsh64(a, 64 - bits);
	return I32x4_or(I32x4_lsh64(b, bits), a);
}

static inline I32x4 I32x4_rsh128(I32x4 a, U8 bits) {

	const I32x4 b = a;
	a = I32x4_rshElements(a, 2);

	if (bits >= 64)
		return I32x4_rsh64(a, bits - 64);

	a = I32x4_lsh64(a, 64 - bits);
	return I32x4_or(I32x4_rsh64(b, bits), a);
}

//Construction

static inline I32x4 I32x4_load1(const void *arr) {
	I32x4 result = I32x4_zero();
	if (arr) Buffer_memcpy(Buffer_createRef(&result, sizeof(I32)), Buffer_createRefConst(arr, sizeof(I32)));
	return result;
}

static inline I32x4 I32x4_load2(const void *arr) {
	I32x4 result = I32x4_zero();
	if (arr) Buffer_memcpy(Buffer_createRef(&result, sizeof(I32) * 2), Buffer_createRefConst(arr, sizeof(I32) * 2));
	return result;
}

static inline I32x4 I32x4_load3(const void *arr) {
	I32x4 result = I32x4_zero();
	if (arr) Buffer_memcpy(Buffer_createRef(&result, sizeof(I32) * 3), Buffer_createRefConst(arr, sizeof(I32) * 3));
	return result;
}

static inline I32x4 I32x4_load4(const void *arr) {
	I32x4 result = I32x4_zero();
	if (arr) Buffer_memcpy(Buffer_createRef(&result, sizeof(I32) * 4), Buffer_createRefConst(arr, sizeof(I32) * 4));
	return result;
}

#ifdef __cplusplus
	}
#endif
