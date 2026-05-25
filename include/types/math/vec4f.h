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

//types/math/vec4f.h

#pragma once
#include "types/base/buffer.h"
#include "types/base/mathf.h"
#include "types/math/vec4.h"
#include "types/math/type_cast.h"

#ifdef __cplusplus
	extern "C" {
#endif

BUFFER_OP_IMPL(F32x4);

#if _SIMD == SIMD_SSE
	#define VEC4F_SSE_GUARD
	#include "types/math/vec4f_sse.h"
#elif _SIMD == SIMD_NEON
	#define VEC4F_NEON_GUARD
	#include "types/math/vec4f_neon.h"
#else
	#define VEC4F_NONE_GUARD
	#include "types/math/vec4f_none.h"
#endif

static inline F32x4 F32x4_bitsI32x4(I32x4 a) {
	const void *aptr = &a;
	return *(const F32x4*) aptr;
}

static inline F32 F32x4_get(F32x4 a, U8 i) {
	switch (i & 3) {
		case 0:		return F32x4_x(a);
		case 1:		return F32x4_y(a);
		case 2:		return F32x4_z(a);
		default:	return F32x4_w(a);
	}
}

//Swizzles and shizzle

#if _SIMD != SIMD_NONE

	static inline void F32x4_setXRef(F32x4 *a, F32 v) { if (a) *a = F32x4_setXCopy(*a, v); }
	static inline void F32x4_setYRef(F32x4 *a, F32 v) { if (a) *a = F32x4_setYCopy(*a, v); }
	static inline void F32x4_setZRef(F32x4 *a, F32 v) { if (a) *a = F32x4_setZCopy(*a, v); }
	static inline void F32x4_setWRef(F32x4 *a, F32 v) { if (a) *a = F32x4_setWCopy(*a, v); }

	static inline F32x4 F32x4_setCopy(F32x4 a, U8 i, F32 v) {
		switch (i & 3) {
			case 0:		return F32x4_setXCopy(a, v);
			case 1:		return F32x4_setYCopy(a, v);
			case 2:		return F32x4_setZCopy(a, v);
			default:	return F32x4_setWCopy(a, v);
		}
	}

	static inline void F32x4_setRef(F32x4 *a, U8 i, F32 v) {
		switch (i & 3) {
			case 0:		F32x4_setXRef(a, v);	break;
			case 1:		F32x4_setYRef(a, v);	break;
			case 2:		F32x4_setZRef(a, v);	break;
			default:	F32x4_setWRef(a, v);
		}
	}

#endif

//Fallbacks for transcendentals

#if _PLATFORM_TYPE != PLATFORM_WINDOWS || _SIMD != SIMD_SSE

	static inline F32x4 F32x4_pow(F32x4 a, F32x4 e) { NONE_OP4F(F32_pow(F32x4_get(a, i), F32x4_get(e, i))); }
	static inline F32x4 F32x4_loge(F32x4 a) { NONE_OP4F(F32_loge(F32x4_get(a, i))); }
	static inline F32x4 F32x4_log10(F32x4 a) { NONE_OP4F(F32_log10(F32x4_get(a, i))); }
	static inline F32x4 F32x4_log2(F32x4 a) { NONE_OP4F(F32_log2(F32x4_get(a, i))); }

	static inline F32x4 F32x4_exp(F32x4 a) { NONE_OP4F(F32_expe(F32x4_get(a, i))); }
	static inline F32x4 F32x4_exp10(F32x4 a) { NONE_OP4F(F32_exp10(F32x4_get(a, i))); }
	static inline F32x4 F32x4_exp2(F32x4 a) { NONE_OP4F(F32_exp2(F32x4_get(a, i))); }

	static inline F32x4 F32x4_acos(F32x4 a) { NONE_OP4F(F32_acos(F32x4_get(a, i))); }
	static inline F32x4 F32x4_cos(F32x4 a) { NONE_OP4F(F32_cos(F32x4_get(a, i))); }
	static inline F32x4 F32x4_asin(F32x4 a) { NONE_OP4F(F32_asin(F32x4_get(a, i))); }
	static inline F32x4 F32x4_sin(F32x4 a) { NONE_OP4F(F32_sin(F32x4_get(a, i))); }
	static inline F32x4 F32x4_atan(F32x4 a) { NONE_OP4F(F32_atan(F32x4_get(a, i))); }
	static inline F32x4 F32x4_atan2(F32x4 a, F32x4 x) { NONE_OP4F(F32_atan2(F32x4_get(a, i), F32x4_get(x, i))); }
	static inline F32x4 F32x4_tan(F32x4 a) { NONE_OP4F(F32_tan(F32x4_get(a, i))); }

#endif

//Constants

static inline F32x4 F32x4_one() { return F32x4_xxxx4(1); }
static inline F32x4 F32x4_two() { return F32x4_xxxx4(2); }
static inline F32x4 F32x4_negOne() { return F32x4_xxxx4(-1); }
static inline F32x4 F32x4_negTwo() { return F32x4_xxxx4(-2); }

//Clamp (standardized)

static inline F32x4 F32x4_clamp(F32x4 a, F32x4 mi, F32x4 ma) { return F32x4_max(mi, F32x4_min(ma, a)); }
static inline F32x4 F32x4_saturate(F32x4 a) { return F32x4_clamp(a, F32x4_zero(), F32x4_one()); }

//Math (standardized)

static inline F32x4 F32x4_fract(F32x4 v) { return F32x4_sub(v, F32x4_floor(v)); }
static inline F32x4 F32x4_mod(F32x4 v, F32x4 d) { return F32x4_mul(F32x4_fract(F32x4_div(v, d)), d); }

static inline F32x4 F32x4_complement(F32x4 a) { return F32x4_sub(F32x4_one(), a); }
static inline F32x4 F32x4_inverse(F32x4 a) { return F32x4_div(F32x4_one(), a); }

#if _SIMD != SIMD_SSE
	static inline F32x4 F32x4_negate(F32x4 a) { return F32x4_sub(F32x4_zero(), a); }
#endif

static inline F32x4 F32x4_pow2(F32x4 a) { return F32x4_mul(a, a); }

static inline F32 F32x4_sqLen2(F32x4 v) { return F32x4_dot2(v, v); }
static inline F32 F32x4_sqLen3(F32x4 v) { return F32x4_dot3(v, v); }
static inline F32 F32x4_sqLen4(F32x4 v) { return F32x4_dot4(v, v); }

static inline F32 F32x4_len2(F32x4 v) { return F32_sqrt(F32x4_sqLen2(v)); }
static inline F32 F32x4_len3(F32x4 v) { return F32_sqrt(F32x4_sqLen3(v)); }
static inline F32 F32x4_len4(F32x4 v) { return F32_sqrt(F32x4_sqLen4(v)); }

static inline F32x4 F32x4_normalize2(F32x4 v) { return F32x4_mul(v, F32x4_rsqrt(F32x4_xxxx4(F32x4_sqLen2(v)))); }
static inline F32x4 F32x4_normalize3(F32x4 v) { return F32x4_mul(v, F32x4_rsqrt(F32x4_xxxx4(F32x4_sqLen3(v)))); }
static inline F32x4 F32x4_normalize4(F32x4 v) { return F32x4_mul(v, F32x4_rsqrt(F32x4_xxxx4(F32x4_sqLen4(v)))); }

static inline F32x4 F32x4_sign(F32x4 v) {
	return F32x4_add(F32x4_mul(F32x4_lt(v, F32x4_zero()), F32x4_negTwo()), F32x4_one());
}

static inline F32x4 F32x4_abs(F32x4 v) { return F32x4_mul(F32x4_sign(v), v); }

static inline F32 F32x4_satDot2(F32x4 x, F32x4 y) { return F32_saturate(F32x4_dot2(x, y)); }
static inline F32 F32x4_satDot3(F32x4 x, F32x4 y) { return F32_saturate(F32x4_dot3(x, y)); }
static inline F32 F32x4_satDot4(F32x4 x, F32x4 y) { return F32_saturate(F32x4_dot4(x, y)); }

static inline F32x4 F32x4_lerp(F32x4 a, F32x4 b, F32 perc) {
	b = F32x4_sub(b, a);
	return F32x4_add(a, F32x4_mul(b, F32x4_xxxx4(perc)));
}

//Texture packing

static inline F32x4 F32x4_rgb8Unpack(U32 v) {
	const F32x4 rgb8 = F32x4_floor(F32x4_div(F32x4_trunc3(F32x4_xxxx4((F32)v)), F32x4_create4(0x10000, 0x100, 0x1, 0x1)));
	return F32x4_div(F32x4_floor(F32x4_mod(rgb8, F32x4_xxxx4(0x100))), F32x4_xxxx4(0xFF));
}

static inline U32 F32x4_rgb8Pack(F32x4 v) {
	const F32x4 v8 = F32x4_floor(F32x4_mul(v, F32x4_xxxx4(0xFF)));
	const F32x4 preShift = F32x4_mul(v8, F32x4_create3(0x10000, 0x100, 0x1));
	return (U32)F32x4_reduce(preShift);
}

//https://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html

static inline F32x4 F32x4_srgbToLinear(F32x4 x) {
	return F32x4_mul(x, F32x4_add(
		F32x4_xxxx4(0.012522878f),
		F32x4_mul(x, F32x4_add(
			F32x4_xxxx4(0.682171111f),
			F32x4_mul(x, F32x4_xxxx4(0.305306011f))
		))
	));
}

static inline F32x4 F32x4_srgb8Unpack(U32 v) {
	return F32x4_srgbToLinear(F32x4_rgb8Unpack(v));
}

static inline F32x4 F32x4_linearToSrgb(F32x4 x) {

	F32x4 S1 = F32x4_sqrt(x);
	F32x4 S2 = F32x4_sqrt(S1);
	F32x4 S3 = F32x4_sqrt(S2);

	S1 = F32x4_mul(S1, F32x4_xxxx4(0.662002687f));
	S2 = F32x4_mul(S2, F32x4_xxxx4(0.684122060f));
	S3 = F32x4_mul(S3, F32x4_xxxx4(-0.323583601f));
	x = F32x4_mul(x, F32x4_xxxx4(-0.0225411470f));

	return F32x4_add(F32x4_add(S1, S2), F32x4_add(S3, x));
}

static inline U32 F32x4_srgb8Pack(F32x4 v) { return F32x4_rgb8Pack(F32x4_linearToSrgb(v)); }

//Reflect incident direction around normal
//https://registry.khronos.org/OpenGL-Refpages/gl4/html/reflect.xhtml
static inline F32x4 F32x4_reflect2(F32x4 i, F32x4 n) {
	return F32x4_sub(i, F32x4_mul(n, F32x4_xxxx4(2 * F32x4_dot2(n, i))));
}

//Reflect incident direction around normal
//https://registry.khronos.org/OpenGL-Refpages/gl4/html/reflect.xhtml
static inline F32x4 F32x4_reflect3(F32x4 i, F32x4 n) {
	return F32x4_sub(i, F32x4_mul(n, F32x4_xxxx4(2 * F32x4_dot3(n, i))));
}

//Boolean

static inline Bool F32x4_all(F32x4 a) { return F32x4_reduce(F32x4_neqExact(a, F32x4_zero())) == 4; }
static inline Bool F32x4_any(F32x4 a) { return F32x4_reduce(F32x4_neqExact(a, F32x4_zero())); }

//For diffs that aren't exact with floats (reasonable relEpsilon = 1e-5, absEpsilon = 1e-6)
static inline F32x4 F32x4_epsilonDiff(F32x4 a, F32x4 b, F32 relEpsilon, F32 absEpsilon) {
	return F32x4_max(F32x4_mul(F32x4_max(F32x4_abs(a), F32x4_abs(b)), F32x4_xxxx4(relEpsilon)), F32x4_xxxx4(absEpsilon));
}
static inline F32x4 F32x4_eqApproxAdv(F32x4 a, F32x4 b, F32 relEpsilon, F32 absEpsilon) {
	F32x4 eps = F32x4_epsilonDiff(a, b, relEpsilon, absEpsilon);
	return F32x4_leq(F32x4_abs(F32x4_sub(a, b)), eps);
}

static inline F32x4 F32x4_neqApproxAdv(F32x4 a, F32x4 b, F32 relEpsilon, F32 absEpsilon) {
	F32x4 eps = F32x4_epsilonDiff(a, b, relEpsilon, absEpsilon);
	return F32x4_gt(F32x4_abs(F32x4_sub(a, b)), eps);
}

static inline F32x4 F32x4_eqApprox(F32x4 a, F32x4 b) { return F32x4_eqApproxAdv(a, b, 1e-5f, 1e-6f); }
static inline F32x4 F32x4_neqApprox(F32x4 a, F32x4 b) { return F32x4_neqApproxAdv(a, b, 1e-5f, 1e-6f); }

static inline Bool F32x4_eqExact4(F32x4 a, F32x4 b) { return F32x4_all(F32x4_eqExact(a, b)); }
static inline Bool F32x4_neqExact4(F32x4 a, F32x4 b) { return !F32x4_eqExact4(a, b); }

static inline Bool F32x4_eqApprox4(F32x4 a, F32x4 b) { return F32x4_all(F32x4_eqApprox(a, b)); }
static inline Bool F32x4_neqApprox4(F32x4 a, F32x4 b) { return !F32x4_eqApprox4(a, b); }

static inline Bool F32x4_eqApproxAdv4(F32x4 a, F32x4 b, F32 relEpsilon, F32 absEpsilon) {
	return F32x4_all(F32x4_eqApproxAdv(a, b, relEpsilon, absEpsilon));
}

static inline Bool F32x4_neqApproxAdv4(F32x4 a, F32x4 b, F32 relEpsilon, F32 absEpsilon) {
	return !F32x4_eqApproxAdv4(a, b, relEpsilon, absEpsilon);
}

//Construction

static inline F32x4 F32x4_load1(const void *arr) {
	F32x4 result = F32x4_zero();
	if (arr) Buffer_memcpy(Buffer_createRef(&result, sizeof(F32)), Buffer_createRefConst(arr, sizeof(F32)));
	return result;
}

static inline F32x4 F32x4_load2(const void *arr) {
	F32x4 result = F32x4_zero();
	if (arr) Buffer_memcpy(Buffer_createRef(&result, sizeof(F32) * 2), Buffer_createRefConst(arr, sizeof(F32) * 2));
	return result;
}

static inline F32x4 F32x4_load3(const void *arr) {
	F32x4 result = F32x4_zero();
	if (arr) Buffer_memcpy(Buffer_createRef(&result, sizeof(F32) * 3), Buffer_createRefConst(arr, sizeof(F32) * 3));
	return result;
}

static inline F32x4 F32x4_load4(const void *arr) {
	F32x4 result = F32x4_zero();
	if (arr) Buffer_memcpy(Buffer_createRef(&result, sizeof(F32) * 4), Buffer_createRefConst(arr, sizeof(F32) * 4));
	return result;
}

#ifdef __cplusplus
	}
#endif
