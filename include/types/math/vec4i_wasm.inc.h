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

//types/math/vec4i_wasm.inc.h

#pragma once
#include <wasm_simd128.h>

#ifndef VEC4I_WASM_GUARD
	#error Vec4i wasm guard was undefined, this likely indicates include of vec4i_wasm.h was attempted instead of vec4i.h
#endif

//Loads

static inline I32x4 I32x4_fromF32x4(F32x4 a) { return wasm_i32x4_trunc_sat_f32x4(a); }

static inline I32x4 I32x4_createFromU64x2(U64 i0, U64 i1) { return wasm_u64x2_make(i0, i1); }

//Swizzles

static inline I32 I32x4_x(I32x4 a) { return wasm_i32x4_extract_lane(a, 0); }
static inline I32 I32x4_y(I32x4 a) { return wasm_i32x4_extract_lane(a, 1); }
static inline I32 I32x4_z(I32x4 a) { return wasm_i32x4_extract_lane(a, 2); }
static inline I32 I32x4_w(I32x4 a) { return wasm_i32x4_extract_lane(a, 3); }

static inline I32x4 I32x4_setXCopy(I32x4 a, I32 v) { return wasm_i32x4_replace_lane(a, 0, v); }
static inline I32x4 I32x4_setYCopy(I32x4 a, I32 v) { return wasm_i32x4_replace_lane(a, 1, v); }
static inline I32x4 I32x4_setZCopy(I32x4 a, I32 v) { return wasm_i32x4_replace_lane(a, 2, v); }
static inline I32x4 I32x4_setWCopy(I32x4 a, I32 v) { return wasm_i32x4_replace_lane(a, 3, v); }

//Trunc & reduce

static inline I32x4 I32x4_trunc2(I32x4 a) { return wasm_v128_and(a, wasm_i32x4_make(-1, -1, 0, 0)); }
static inline I32x4 I32x4_trunc3(I32x4 a) { return wasm_v128_and(a, wasm_i32x4_make(-1, -1, -1, 0)); }

//SIMD128 has no horizontal add, so this sums lanewise through two shuffles rather than two hadds.

static inline I32 I32x4_reduce(I32x4 a) {
	const I32x4 pairs = wasm_i32x4_add(a, wasm_i32x4_shuffle(a, a, 2, 3, 0, 1));
	return wasm_i32x4_extract_lane(wasm_i32x4_add(pairs, wasm_i32x4_shuffle(pairs, pairs, 1, 0, 3, 2)), 0);
}

//Arithmetic

static inline I32x4 I32x4_add(I32x4 a, I32x4 b) { return wasm_i32x4_add(a, b); }
static inline I32x4 I32x4_sub(I32x4 a, I32x4 b) { return wasm_i32x4_sub(a, b); }
static inline I32x4 I32x4_mul(I32x4 a, I32x4 b) { return wasm_i32x4_mul(a, b); }

static inline I32x4 I32x4_addI64x2(I32x4 a, I32x4 b) { return wasm_i64x2_add(a, b); }

//No widening 32x32->64 multiply of the even lanes, so the two products are formed explicitly.

static inline I32x4 I32x4_mulU32x2AsU64x2(I32x4 a, I32x4 b) {
	const U64 a0 = (U32) wasm_i32x4_extract_lane(a, 0), b0 = (U32) wasm_i32x4_extract_lane(b, 0);
	const U64 a1 = (U32) wasm_i32x4_extract_lane(a, 2), b1 = (U32) wasm_i32x4_extract_lane(b, 2);
	return wasm_u64x2_make(a0 * b0, a1 * b1);
}

static inline I32x4 I32x4_negate(I32x4 a) { return wasm_i32x4_neg(a); }

//Clamps

static inline I32x4 I32x4_min(I32x4 a, I32x4 b) { return wasm_i32x4_min(a, b); }
static inline I32x4 I32x4_max(I32x4 a, I32x4 b) { return wasm_i32x4_max(a, b); }

//Comparison
//SIMD128 comparisons give all ones per true lane, same as SSE, so the 0/1 forms negate that mask.

static inline I32x4 I32x4_one() { return I32x4_xxxx4(1); }

static inline I32x4 I32x4_eq(I32x4 a, I32x4 b) { return wasm_i32x4_neg(wasm_i32x4_eq(a, b)); }
static inline I32x4 I32x4_neq(I32x4 a, I32x4 b) { return wasm_i32x4_neg(wasm_i32x4_ne(a, b)); }
static inline I32x4 I32x4_geq(I32x4 a, I32x4 b) { return wasm_i32x4_neg(wasm_i32x4_ge(a, b)); }
static inline I32x4 I32x4_gt(I32x4 a, I32x4 b) { return wasm_i32x4_neg(wasm_i32x4_gt(a, b)); }
static inline I32x4 I32x4_leq(I32x4 a, I32x4 b) { return wasm_i32x4_neg(wasm_i32x4_le(a, b)); }
static inline I32x4 I32x4_lt(I32x4 a, I32x4 b) { return wasm_i32x4_neg(wasm_i32x4_lt(a, b)); }

//Bitwise

static inline I32x4 I32x4_or(I32x4 a, I32x4 b) { return wasm_v128_or(a, b); }
static inline I32x4 I32x4_and(I32x4 a, I32x4 b) { return wasm_v128_and(a, b); }
static inline I32x4 I32x4_andnot(I32x4 a, I32x4 b) { return wasm_v128_and(wasm_v128_not(a), b); }
static inline I32x4 I32x4_xor(I32x4 a, I32x4 b) { return wasm_v128_xor(a, b); }
static inline I32x4 I32x4_not(I32x4 a) { return wasm_v128_not(a); }

//Byte-wise element shifts. wasm_i8x16_shuffle indexes 0-15 into a and 16-31 into b, so shifting in zeros
// means selecting lanes out of a zero vector rather than a dedicated slli_si128.

static inline I32x4 I32x4_lshElements(I32x4 a, U8 elementCount) {
	const I32x4 z = I32x4_zero();
	switch (elementCount) {
		case 0:     return a;
		case 1:     return wasm_i8x16_shuffle(z, a, 0, 1, 2, 3, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27);
		case 2:     return wasm_i8x16_shuffle(z, a, 0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23);
		case 3:     return wasm_i8x16_shuffle(z, a, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 16, 17, 18, 19);
		default:    return z;
	}
}

static inline I32x4 I32x4_rshElements(I32x4 a, U8 elementCount) {
	const I32x4 z = I32x4_zero();
	switch (elementCount) {
		case 0:     return a;
		case 1:     return wasm_i8x16_shuffle(a, z, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19);
		case 2:     return wasm_i8x16_shuffle(a, z, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23);
		case 3:     return wasm_i8x16_shuffle(a, z, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27);
		default:    return z;
	}
}

//Defined inline rather than out of line as the SSE backend does: SIMD128's shifts take a runtime count,
// so there is no need for the switch over every immediate that sse_vec.c has to spell out.

static inline I32x4 I32x4_lsh32(I32x4 a, U8 bits) { return wasm_i32x4_shl(a, bits); }
static inline I32x4 I32x4_rsh32(I32x4 a, U8 bits) { return wasm_u32x4_shr(a, bits); }
static inline I32x4 I32x4_lsh64(I32x4 a, U8 bits) { return wasm_i64x2_shl(a, bits); }
static inline I32x4 I32x4_rsh64(I32x4 a, U8 bits) { return wasm_u64x2_shr(a, bits); }

//SHA256 helper functions

static inline I32x4 I32x4_shuffleBytes(I32x4 a, I32x4 b) { return wasm_i8x16_swizzle(a, b); }

//wasm_v128_bitselect picks from a where the mask bit is set, so the per element mask is built directly
// instead of enumerating every blend immediate the way _mm_blend_epi16 requires.

static inline I32x4 I32x4_blend(I32x4 a, I32x4 b, U8 xyzw) {
	const I32x4 mask = wasm_i32x4_make(
		(xyzw & 1) ? -1 : 0,
		(xyzw & 2) ? -1 : 0,
		(xyzw & 4) ? -1 : 0,
		(xyzw & 8) ? -1 : 0
	);
	return wasm_v128_bitselect(b, a, mask);
}

static inline I32x4 I32x4_combineRightShift(I32x4 a, I32x4 b, U8 v) {
	switch (v) {
		case 0:     return b;
		case 1:     return wasm_i8x16_shuffle(b, a, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19);
		case 2:     return wasm_i8x16_shuffle(b, a, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23);
		case 3:     return wasm_i8x16_shuffle(b, a, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27);
		default:    return I32x4_zero();
	}
}

//Reverses all 16 bytes, not just the bytes within each lane: the scalar reference swaps the four 32 bit
// lanes to wzyx AND byte swaps inside each, and the SSE mask (_mm_set_epi8 takes its arguments highest
// byte first) is a full reversal too. A per lane swap alone leaves the lane order wrong, which shows up
// as a GHASH tag mismatch in aes128gcm rather than anywhere obvious.

static inline I32x4 I32x4_swapEndianness(I32x4 v) {
	return wasm_i8x16_shuffle(v, v, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
}
