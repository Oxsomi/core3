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

//types/math/vec4f_sse.inc.h

#ifndef VEC4F_SSE_GUARD
	#error Vec4f SSE guard was undefined, this likely indicates include of vec4f_sse.h was attempted instead of vec4f.h
#endif

#include <smmintrin.h>

//Loads

static inline F32x4 F32x4_fromI32x4(I32x4 a) { return _mm_cvtepi32_ps(a); }

//Swizzles

static inline F32 F32x4_x(F32x4 a) { return _mm_cvtss_f32(a); }
static inline F32 F32x4_y(F32x4 a) { return _mm_cvtss_f32(_mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 1, 1, 1))); }
static inline F32 F32x4_z(F32x4 a) { return _mm_cvtss_f32(_mm_movehl_ps(a, a)); }
static inline F32 F32x4_w(F32x4 a) { return _mm_cvtss_f32(_mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 3, 3, 3))); }

static inline F32x4 F32x4_setXCopy(F32x4 a, F32 v) { return _mm_blend_ps(a, _mm_set_ps1(v), 0x1); }
static inline F32x4 F32x4_setYCopy(F32x4 a, F32 v) { return _mm_blend_ps(a, _mm_set_ps1(v), 0x2); }
static inline F32x4 F32x4_setZCopy(F32x4 a, F32 v) { return _mm_blend_ps(a, _mm_set_ps1(v), 0x4); }
static inline F32x4 F32x4_setWCopy(F32x4 a, F32 v) { return _mm_blend_ps(a, _mm_set_ps1(v), 0x8); }

//Trunc & reduce

static inline F32x4 F32x4_trunc2(F32x4 a) { return _mm_movelh_ps(a, F32x4_zero()); }

static inline F32x4 F32x4_trunc3(F32x4 a) {
	const F32x4 mask = _mm_castsi128_ps(_mm_set_epi32(0, -1, -1, -1));
	return _mm_and_ps(a, mask);
}

static inline F32 F32x4_reduce(F32x4 a) {
	__m128 shuf = _mm_movehdup_ps(a);
	__m128 sums = _mm_add_ps(a, shuf);
	shuf = _mm_movehl_ps(shuf, sums);
	sums = _mm_add_ss(sums, shuf);
	return _mm_cvtss_f32(sums);
}

//Arithmetic

static inline F32x4 F32x4_add(F32x4 a, F32x4 b) { return _mm_add_ps(a, b); }
static inline F32x4 F32x4_sub(F32x4 a, F32x4 b) { return _mm_sub_ps(a, b); }
static inline F32x4 F32x4_mul(F32x4 a, F32x4 b) { return _mm_mul_ps(a, b); }
static inline F32x4 F32x4_div(F32x4 a, F32x4 b) { return _mm_div_ps(a, b); }

static inline F32 F32x4_dot2(F32x4 a, F32x4 b) { return F32x4_x(_mm_dp_ps(a, F32x4_trunc2(b), 0xFF)); }
static inline F32 F32x4_dot3(F32x4 a, F32x4 b) { return F32x4_x(_mm_dp_ps(a, F32x4_trunc3(b), 0xFF)); }
static inline F32 F32x4_dot4(F32x4 a, F32x4 b) { return F32x4_x(_mm_dp_ps(a, b, 0xFF)); }

static inline F32x4 F32x4_negate(F32x4 a) { return F32x4_sub(F32x4_zero(), a); }

//Clamps

static inline F32x4 F32x4_min(F32x4 a, F32x4 b) { return _mm_min_ps(a, b); }
static inline F32x4 F32x4_max(F32x4 a, F32x4 b) { return _mm_max_ps(a, b); }

//Rounding

static inline F32x4 F32x4_ceil(F32x4 a) { return _mm_ceil_ps(a); }
static inline F32x4 F32x4_floor(F32x4 a) { return _mm_floor_ps(a); }
static inline F32x4 F32x4_round(F32x4 a) { return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT); }

//Transcendentals

static inline F32x4 F32x4_sqrt(F32x4 a) { return _mm_sqrt_ps(a); }

//Clearing the sign bit, which gets -0 right where multiplying by a sign does not.
//AND against a positive mask rather than ANDNOT against the sign bit: ANDNOT is not commutative, so the mask
// has to be its first operand and the value gets moved out of the way first. Same cost in a loop, where the
// constant hoists and folds as a memory operand either way, one instruction fewer everywhere else.

static inline F32x4 F32x4_abs(F32x4 a) { return _mm_and_ps(a, _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF))); }

//_mm_rsqrt_ps is a ~12 bit estimate, so it hands back 0.99987793 for the reciprocal root of one. Correctly
//rounded is what every other backend gives and what a value written to a file has to be, so the plain name
// is the divide and the estimate is named for what it is. See F32x4_rsqrtFast in vec4f.h for when to take it.

static inline F32x4 F32x4_rsqrt(F32x4 a) { return _mm_div_ps(_mm_set1_ps(1), _mm_sqrt_ps(a)); }
static inline F32x4 F32x4_rsqrtFast(F32x4 a) { return _mm_rsqrt_ps(a); }

//These are intel extended instructions
// TODO: Should add a proper SIMD fallback, for now this is actually handled later with naive fallback

#if _SIMD_HAS_SVML

	#include <immintrin.h>

	//The header is already in, so the intrinsic costs nothing here that the SVML calls below have not paid.
	//It is defined in BOTH halves of this branch: fma is not an SVML function and a compiler that has SVML
	// still needs it.

	static inline F32x4 F32x4_fma(F32x4 a, F32x4 b, F32x4 c) { return _mm_fmadd_ps(a, b, c); }    //a * b + c

	static inline F32x4 F32x4_pow(F32x4 v, F32x4 e) { return _mm_pow_ps(v, e); }
	static inline F32x4 F32x4_loge(F32x4 v) { return _mm_log_ps(v); }
	static inline F32x4 F32x4_log10(F32x4 v) { return _mm_log10_ps(v); }
	static inline F32x4 F32x4_log2(F32x4 v) { return _mm_log2_ps(v); }

	static inline F32x4 F32x4_exp(F32x4 v) { return _mm_exp_ps(v); }
	static inline F32x4 F32x4_exp10(F32x4 v) { return _mm_exp10_ps(v); }
	static inline F32x4 F32x4_exp2(F32x4 v) { return _mm_exp2_ps(v); }

	static inline F32x4 F32x4_acos(F32x4 v) { return _mm_acos_ps(v); }
	static inline F32x4 F32x4_cos(F32x4 v) { return _mm_cos_ps(v); }
	static inline F32x4 F32x4_asin(F32x4 v) { return _mm_asin_ps(v); }
	static inline F32x4 F32x4_sin(F32x4 v) { return _mm_sin_ps(v); }
	static inline F32x4 F32x4_atan(F32x4 v) { return _mm_atan_ps(v); }
	static inline F32x4 F32x4_atan2(F32x4 y, F32x4 x) { return _mm_atan2_ps(y, x); }
	static inline F32x4 F32x4_tan(F32x4 v) { return _mm_tan_ps(v); }

#else

	//The BUILTIN, deliberately: _mm_fmadd_ps is declared in immintrin.h, and including that header costs
	//enough compile time to be worth avoiding wherever it is not already pulled in for SVML above. The
	// builtin is the same instruction with no header at all.
	//_SIMD_HAS_SVML is 0 exactly when the compiler is gcc or clang, both of which have it, so this branch
	// never reaches a compiler that does not. It does need -mfma, which this build passes.

	static inline F32x4 F32x4_fma(F32x4 a, F32x4 b, F32x4 c) {    //a * b + c (FMA required)
		return (__m128) __builtin_ia32_vfmaddps((__v4sf)a, (__v4sf)b, (__v4sf)c);
	}
#endif

//The FULL width only, where the caller's sixteen bytes are in bounds by the contract and the instruction is
//the explicitly unaligned one, so nothing here assumes an alignment the interface does not promise. The
// partial widths stay a byte copy in vec4f.h: there is no partial load that is both in bounds and unaligned.

static inline F32x4 F32x4_load4(const void *arr) { return arr ? _mm_loadu_ps((const F32*) arr) : _mm_setzero_ps(); }
static inline void F32x4_store4(void *arr, F32x4 a) { if(arr) _mm_storeu_ps((F32*) arr, a); }

//Boolean
		
static inline F32x4 F32x4_recastI32x4Internal(F32x4 a) { return F32x4_fromI32x4(*(const I32x4*) &a); }
static inline F32x4 F32x4_negateRecastiInternal(F32x4 a) { return F32x4_negate(F32x4_recastI32x4Internal(a)); }
static inline F32x4 F32x4_eqExact(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(_mm_cmpeq_ps(a, b)); }
static inline F32x4 F32x4_neqExact(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(_mm_cmpneq_ps(a, b)); }
static inline F32x4 F32x4_geq(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(_mm_cmpge_ps(a, b)); }
static inline F32x4 F32x4_gt(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(_mm_cmpgt_ps(a, b)); }
static inline F32x4 F32x4_leq(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(_mm_cmple_ps(a, b)); }
static inline F32x4 F32x4_lt(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(_mm_cmplt_ps(a, b)); }

//4x4 transpose.
//Sits here rather than in mat.h because it's the one matrix operation with a genuine per-SIMD implementation,
// and per-SIMD code belongs in these files.
//Safe when in == out.

static inline void F32x4_transpose4(const F32x4 *in, F32x4 *out) {

	const F32x4 t0 = _mm_unpacklo_ps(in[0], in[1]);        //x0 x1 y0 y1
	const F32x4 t1 = _mm_unpackhi_ps(in[0], in[1]);        //z0 z1 w0 w1
	const F32x4 t2 = _mm_unpacklo_ps(in[2], in[3]);        //x2 x3 y2 y3
	const F32x4 t3 = _mm_unpackhi_ps(in[2], in[3]);        //z2 z3 w2 w3

	out[0] = _mm_movelh_ps(t0, t2);
	out[1] = _mm_movehl_ps(t2, t0);
	out[2] = _mm_movelh_ps(t1, t3);
	out[3] = _mm_movehl_ps(t3, t1);
}
