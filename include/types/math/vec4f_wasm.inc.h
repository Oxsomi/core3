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

//types/math/vec4f_wasm.inc.h

#ifndef VEC4F_WASM_GUARD
	#error Vec4f wasm guard was undefined, this likely indicates include of vec4f_wasm.h was attempted instead of vec4f.h
#endif

#include <wasm_simd128.h>

//Loads

static inline F32x4 F32x4_fromI32x4(I32x4 a) { return wasm_f32x4_convert_i32x4(a); }

//Swizzles

static inline F32 F32x4_x(F32x4 a) { return wasm_f32x4_extract_lane(a, 0); }
static inline F32 F32x4_y(F32x4 a) { return wasm_f32x4_extract_lane(a, 1); }
static inline F32 F32x4_z(F32x4 a) { return wasm_f32x4_extract_lane(a, 2); }
static inline F32 F32x4_w(F32x4 a) { return wasm_f32x4_extract_lane(a, 3); }

static inline F32x4 F32x4_setXCopy(F32x4 a, F32 v) { return wasm_f32x4_replace_lane(a, 0, v); }
static inline F32x4 F32x4_setYCopy(F32x4 a, F32 v) { return wasm_f32x4_replace_lane(a, 1, v); }
static inline F32x4 F32x4_setZCopy(F32x4 a, F32 v) { return wasm_f32x4_replace_lane(a, 2, v); }
static inline F32x4 F32x4_setWCopy(F32x4 a, F32 v) { return wasm_f32x4_replace_lane(a, 3, v); }

//Trunc & reduce

static inline F32x4 F32x4_trunc2(F32x4 a) { return wasm_f32x4_make(F32x4_x(a), F32x4_y(a), 0, 0); }

static inline F32x4 F32x4_trunc3(F32x4 a) {
	return wasm_v128_and(a, wasm_i32x4_make(-1, -1, -1, 0));
}

//No horizontal add; sum lanewise through two shuffles, matching I32x4_reduce.

static inline F32 F32x4_reduce(F32x4 a) {
	const F32x4 pairs = wasm_f32x4_add(a, wasm_i32x4_shuffle(a, a, 2, 3, 0, 1));
	return wasm_f32x4_extract_lane(wasm_f32x4_add(pairs, wasm_i32x4_shuffle(pairs, pairs, 1, 0, 3, 2)), 0);
}

//Arithmetic

static inline F32x4 F32x4_add(F32x4 a, F32x4 b) { return wasm_f32x4_add(a, b); }
static inline F32x4 F32x4_sub(F32x4 a, F32x4 b) { return wasm_f32x4_sub(a, b); }
static inline F32x4 F32x4_mul(F32x4 a, F32x4 b) { return wasm_f32x4_mul(a, b); }
static inline F32x4 F32x4_div(F32x4 a, F32x4 b) { return wasm_f32x4_div(a, b); }

//SIMD128 has no dot product instruction (_mm_dp_ps), so these multiply and then reduce.
//Truncating the second operand is what makes the 2 and 3 component forms ignore the unused lanes.
//The SSE backend truncates the same way and always passes 0xFF to dpps.

static inline F32 F32x4_dot2(F32x4 a, F32x4 b) { return F32x4_reduce(F32x4_mul(a, F32x4_trunc2(b))); }
static inline F32 F32x4_dot3(F32x4 a, F32x4 b) { return F32x4_reduce(F32x4_mul(a, F32x4_trunc3(b))); }
static inline F32 F32x4_dot4(F32x4 a, F32x4 b) { return F32x4_reduce(F32x4_mul(a, b)); }

static inline F32x4 F32x4_negate(F32x4 a) { return wasm_f32x4_neg(a); }

//Clamps
//wasm_f32x4_min/max are the IEEE forms and propagate NaN, where SSE's minps/maxps return the second
// operand on NaN. OxC3 does not rely on that difference, and pmin/pmax exist if it ever needs to.

static inline F32x4 F32x4_min(F32x4 a, F32x4 b) { return wasm_f32x4_min(a, b); }
static inline F32x4 F32x4_max(F32x4 a, F32x4 b) { return wasm_f32x4_max(a, b); }

//Rounding

static inline F32x4 F32x4_ceil(F32x4 a) { return wasm_f32x4_ceil(a); }
static inline F32x4 F32x4_floor(F32x4 a) { return wasm_f32x4_floor(a); }
static inline F32x4 F32x4_round(F32x4 a) { return wasm_f32x4_nearest(a); }

//Transcendentals

static inline F32x4 F32x4_sqrt(F32x4 a) { return wasm_f32x4_sqrt(a); }

//No reciprocal square root estimate in SIMD128; the exact form is the only option.

static inline F32x4 F32x4_rsqrt(F32x4 a) { return wasm_f32x4_div(wasm_f32x4_splat(1), wasm_f32x4_sqrt(a)); }

//No fused multiply add in the SIMD128 MVP either, so this is a multiply then an add.
//That is not a true FMA: it rounds twice. OxC3's callers use it for throughput rather than for the extra
// precision a fused form gives, and the scalar fallback rounds twice as well.

static inline F32x4 F32x4_fma(F32x4 a, F32x4 b, F32x4 c) { return wasm_f32x4_add(wasm_f32x4_mul(a, b), c); }

//Comparison
//SIMD128 comparisons give an all ones mask per true lane, same as SSE, so these reuse the same
// recast-then-negate shape to turn that mask into a 1.0f/0.0f float vector.

//I32x4 and F32x4 are both v128_t here, so the SSE version's bit cast through a pointer is not needed:
// the lanes are reinterpreted by the conversion itself.

static inline F32x4 F32x4_recastI32x4Internal(F32x4 a) { return F32x4_fromI32x4(a); }
static inline F32x4 F32x4_negateRecastiInternal(F32x4 a) { return F32x4_negate(F32x4_recastI32x4Internal(a)); }

static inline F32x4 F32x4_eqExact(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(wasm_f32x4_eq(a, b)); }
static inline F32x4 F32x4_neqExact(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(wasm_f32x4_ne(a, b)); }
static inline F32x4 F32x4_geq(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(wasm_f32x4_ge(a, b)); }
static inline F32x4 F32x4_gt(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(wasm_f32x4_gt(a, b)); }
static inline F32x4 F32x4_leq(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(wasm_f32x4_le(a, b)); }
static inline F32x4 F32x4_lt(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(wasm_f32x4_lt(a, b)); }

//4x4 transpose.
//Sits here rather than in mat.h because it's the one matrix operation with a genuine per-SIMD
// implementation, and per-SIMD code belongs in these files.
//Safe when in == out.
//SIMD128 has no unpack/movelh pair, so each output row is one shuffle across two inputs: lanes 0-3 index
// the first vector and 4-7 the second.

static inline void F32x4_transpose4(const F32x4 *in, F32x4 *out) {

	const F32x4 t0 = wasm_i32x4_shuffle(in[0], in[1], 0, 4, 1, 5);        //x0 x1 y0 y1
	const F32x4 t1 = wasm_i32x4_shuffle(in[0], in[1], 2, 6, 3, 7);        //z0 z1 w0 w1
	const F32x4 t2 = wasm_i32x4_shuffle(in[2], in[3], 0, 4, 1, 5);        //x2 x3 y2 y3
	const F32x4 t3 = wasm_i32x4_shuffle(in[2], in[3], 2, 6, 3, 7);        //z2 z3 w2 w3

	out[0] = wasm_i32x4_shuffle(t0, t2, 0, 1, 4, 5);
	out[1] = wasm_i32x4_shuffle(t0, t2, 2, 3, 6, 7);
	out[2] = wasm_i32x4_shuffle(t1, t3, 0, 1, 4, 5);
	out[3] = wasm_i32x4_shuffle(t1, t3, 2, 3, 6, 7);
}
