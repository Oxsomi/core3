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

//types/math/vec4i_sse.inc.h

#pragma once
#include <immintrin.h>
#include <smmintrin.h>
#include <tmmintrin.h>

#ifndef VEC4I_SSE_GUARD
	#error Vec4i SSE guard was undefined, this likely indicates include of vec4i_sse.h was attempted instead of vec4i.h
#endif

//Loads

static inline I32x4 I32x4_fromF32x4(F32x4 a) { return _mm_cvttps_epi32(a); }

static inline I32x4 I32x4_createFromU64x2(U64 i0, U64 i1) { return _mm_set_epi64x(i1, i0); }

//Swizzles

static inline I32 I32x4_x(I32x4 a) { return _mm_extract_epi32(a, 0); }
static inline I32 I32x4_y(I32x4 a) { return _mm_extract_epi32(a, 1); }
static inline I32 I32x4_z(I32x4 a) { return _mm_extract_epi32(a, 2); }
static inline I32 I32x4_w(I32x4 a) { return _mm_extract_epi32(a, 3); }

static inline I32x4 I32x4_setXCopy(I32x4 a, I32 v) { return _mm_insert_epi32(a, v, 0); }
static inline I32x4 I32x4_setYCopy(I32x4 a, I32 v) { return _mm_insert_epi32(a, v, 1); }
static inline I32x4 I32x4_setZCopy(I32x4 a, I32 v) { return _mm_insert_epi32(a, v, 2); }
static inline I32x4 I32x4_setWCopy(I32x4 a, I32 v) { return _mm_insert_epi32(a, v, 3); }

//Trunc & reduce

static inline I32x4 I32x4_trunc2(I32x4 a) { return _mm_and_si128(a, _mm_set_epi32(0, 0, -1, -1)); }
static inline I32x4 I32x4_trunc3(I32x4 a)  { return _mm_and_si128(a, _mm_set_epi32(0, -1, -1, -1)); }

static inline I32 I32x4_reduce(I32x4 a) {
	return I32x4_x(_mm_hadd_epi32(_mm_hadd_epi32(a, I32x4_zero()), I32x4_zero()));
}

//Arithmetic

static inline I32x4 I32x4_add(I32x4 a, I32x4 b) { return _mm_add_epi32(a, b); }
static inline I32x4 I32x4_sub(I32x4 a, I32x4 b) { return _mm_sub_epi32(a, b); }
static inline I32x4 I32x4_mul(I32x4 a, I32x4 b) { return _mm_mullo_epi32(a, b); }

#if _PLATFORM_TYPE == PLATFORM_WINDOWS
	static inline I32x4 I32x4_div(I32x4 a, I32x4 b) { return _mm_div_epi32(a, b); }
#endif

static inline I32x4 I32x4_addI64x2(I32x4 a, I32x4 b) { return _mm_add_epi64(a, b); }
static inline I32x4 I32x4_mulU32x2AsU64x2(I32x4 a, I32x4 b) { return _mm_mul_epu32(a, b); }

static inline I32x4 I32x4_negate(I32x4 a) { return I32x4_sub(I32x4_zero(), a); }

//Clamps

static inline I32x4 I32x4_min(I32x4 a, I32x4 b) { return _mm_min_epi32(a, b); }
static inline I32x4 I32x4_max(I32x4 a, I32x4 b) { return _mm_max_epi32(a, b); }

//Comparison

static inline I32x4 I32x4_one() { return I32x4_xxxx4(1); }

static inline I32x4 I32x4_eq(I32x4 a, I32x4 b) { return I32x4_negate(_mm_cmpeq_epi32(a, b)); }
static inline I32x4 I32x4_neq(I32x4 a, I32x4 b) { return I32x4_add(I32x4_one(), _mm_cmpeq_epi32(a, b)); }
static inline I32x4 I32x4_geq(I32x4 a, I32x4 b) { return I32x4_add(I32x4_one(), _mm_cmplt_epi32(a, b)); }
static inline I32x4 I32x4_gt(I32x4 a, I32x4 b) { return I32x4_negate(_mm_cmpgt_epi32(a, b)); }
static inline I32x4 I32x4_leq(I32x4 a, I32x4 b) { return I32x4_add(I32x4_one(), _mm_cmpgt_epi32(a, b)); }
static inline I32x4 I32x4_lt(I32x4 a, I32x4 b) { return I32x4_negate(_mm_cmplt_epi32(a, b)); }

//Bitwise

static inline I32x4 I32x4_or(I32x4 a, I32x4 b) { return _mm_or_si128(a, b); }
static inline I32x4 I32x4_and(I32x4 a, I32x4 b) { return _mm_and_si128(a, b); }
static inline I32x4 I32x4_andnot(I32x4 a, I32x4 b) { return _mm_andnot_si128(a, b); }
static inline I32x4 I32x4_xor(I32x4 a, I32x4 b) { return _mm_xor_si128(a, b); }
static inline I32x4 I32x4_not(I32x4 a) { return _mm_xor_si128(a, I32x4_xxxx4((I32)U32_MAX)); }
	
static inline I32x4 I32x4_lshElements(I32x4 a, U8 elementCount) {
	switch (elementCount) {
		case 0:        return a;
		case 1:        return _mm_slli_si128(a,  4);
		case 2:        return _mm_slli_si128(a,  8);
		case 3:        return _mm_slli_si128(a, 12);
		default:    return I32x4_zero();
	}
}

static inline I32x4 I32x4_rshElements(I32x4 a, U8 elementCount) {
	switch (elementCount) {
		case 0:        return a;
		case 1:        return _mm_srli_si128(a,  4);
		case 2:        return _mm_srli_si128(a,  8);
		case 3:        return _mm_srli_si128(a, 12);
		default:    return I32x4_zero();
	}
}

I32x4 I32x4_lsh32(I32x4 a, U8 bits);
I32x4 I32x4_rsh32(I32x4 a, U8 bits);
I32x4 I32x4_lsh64(I32x4 a, U8 bits);
I32x4 I32x4_rsh64(I32x4 a, U8 bits);

//SHA256 helper functions

static inline I32x4 I32x4_shuffleBytes(I32x4 a, I32x4 b) { return _mm_shuffle_epi8(a, b); }

static inline I32x4 I32x4_blend(I32x4 a, I32x4 b, U8 xyzw) {

	switch (xyzw & 0xF) {

		default:        return a;
		case 0b1111:    return b;

		case 0b0001:    return _mm_blend_epi16(a, b, 0x03);
		case 0b0010:    return _mm_blend_epi16(a, b, 0x0C);
		case 0b0011:    return _mm_blend_epi16(a, b, 0x0F);

		case 0b0100:    return _mm_blend_epi16(a, b, 0x30);
		case 0b0101:    return _mm_blend_epi16(a, b, 0x33);
		case 0b0110:    return _mm_blend_epi16(a, b, 0x3C);
		case 0b0111:    return _mm_blend_epi16(a, b, 0x3F);

		case 0b1000:    return _mm_blend_epi16(a, b, 0xC0);
		case 0b1001:    return _mm_blend_epi16(a, b, 0xC3);
		case 0b1010:    return _mm_blend_epi16(a, b, 0xCC);
		case 0b1011:    return _mm_blend_epi16(a, b, 0xCF);

		case 0b1100:    return _mm_blend_epi16(a, b, 0xF0);
		case 0b1101:    return _mm_blend_epi16(a, b, 0xF3);
		case 0b1110:    return _mm_blend_epi16(a, b, 0xFC);
	}
}

static inline I32x4 I32x4_combineRightShift(I32x4 a, I32x4 b, U8 v) {
	switch (v) {
		case 0:        return b;
		case 1:        return _mm_alignr_epi8(a, b, 4);
		case 2:        return _mm_alignr_epi8(a, b, 8);
		case 3:        return _mm_alignr_epi8(a, b, 12);
		default:    return I32x4_zero();
	}
}

static inline I32x4 I32x4_swapEndianness(I32x4 v) {
	return _mm_shuffle_epi8(v, _mm_set_epi8(
		0, 1, 2, 3,
		4, 5, 6, 7,
		8, 9, 10, 11,
		12, 13, 14, 15
	));
}
