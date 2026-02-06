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
#include "types/math/vec4.h"
#include <immintrin.h>

#ifdef __cplusplus
	extern "C" {
#endif

//Even though there's no real fallback for this with either NEON or NONE, they're still abstracted just in case.

typedef __m256i I32x8;

static inline I32x8 I32x8_zero() { return _mm256_setzero_si256(); }
static inline I32x8 I32x8_load(const void *addr) { return _mm256_loadu_si256((const I32x8*)addr); }
static inline void I32x8_store(void *addr, I32x8 v) { _mm256_storeu_si256((I32x8*)addr, v); }
static inline I32x8 I32x8_create4_4(I32x4 a, I32x4 b) { return _mm256_inserti128_si256(_mm256_castsi128_si256(a), b, 1); }
static inline I32x8 I32x8_xx4(I32x4 a) { return I32x8_create4_4(a, a); }

static inline I32x8 I32x8_xor(I32x8 a, I32x8 b) { return _mm256_xor_si256(a, b); }
static inline I32x8 I32x8_or(I32x8 a, I32x8 b) { return _mm256_or_si256(a, b); }

static inline I32x8 I32x8_lshElements(I32x8 a, U8 elementCount) {
	switch (elementCount) {
		case 0:		return a;
		case 1:		return _mm256_slli_si256(a,  4);
		case 2:		return _mm256_slli_si256(a,  8);
		case 3:		return _mm256_slli_si256(a, 12);
		default:	return I32x8_zero();
	}
}

static inline I32x8 I32x8_rshElements(I32x8 a, U8 elementCount) {
	switch (elementCount) {
		case 0:		return a;
		case 1:		return _mm256_srli_si256(a,  4);
		case 2:		return _mm256_srli_si256(a,  8);
		case 3:		return _mm256_srli_si256(a, 12);
		default:	return I32x8_zero();
	}
}

static inline I32x4 I32x8_getI32x4(I32x8 v, U8 i) {
	switch (i) {
		case 1:		return _mm256_extracti128_si256(v, 1);
		default:	return _mm256_castsi256_si128(v);
	}
}

static inline I32x8 I32x8_aesEnc(I32x8 a, I32x8 b) { return _mm256_aesenc_epi128(a, b); }
static inline I32x8 I32x8_aesEncLast(I32x8 a, I32x8 b) { return _mm256_aesenclast_epi128(a, b); }

static inline I32x8 I32x8_clmul64(I32x8 a, I32x8 b, U8 imm) {
	switch (imm) {
		case 0x00:	return _mm256_clmulepi64_epi128(a, b, 0x00);
		case 0x01:	return _mm256_clmulepi64_epi128(a, b, 0x01);
		case 0x10:	return _mm256_clmulepi64_epi128(a, b, 0x10);
		default:	return _mm256_clmulepi64_epi128(a, b, 0x11);
	}
}

static inline I32x8 I32x8_swapEndianness(I32x8 v) {
	return _mm256_shuffle_epi8(v, _mm256_set_epi8(
		 0,  1,  2,  3,  4,  5,  6,  7,
		 8,  9, 10, 11, 12, 13, 14, 15,
		16, 17, 18, 19, 20, 21, 22, 23,
		24, 25, 26, 27, 28, 29, 30, 31
	));
}

//Swap endianness only in the I32x4[2]
static inline I32x8 I32x8_swapEndiannessI32x4(I32x8 v) {
	return _mm256_shuffle_epi8(v, _mm256_set_epi8(
		16, 17, 18, 19, 20, 21, 22, 23,
		24, 25, 26, 27, 28, 29, 30, 31,
		 0,  1,  2,  3,  4,  5,  6,  7,
		 8,  9, 10, 11, 12, 13, 14, 15
	));
}

//Swaps I32x4[2] into yx order, useful for hash loads.
static inline I32x8 I32x8_yxI32x4(I32x8 v) {
	return _mm256_shuffle_i32x4(v, v, _MM_SHUFFLE2(0, 1));
}

static inline I32x8 I32x8_lsh32(I32x8 a, U8 bits) {
	switch (bits) {
		case 0:		return a;
		FUNC_EXPAND16(1, _mm256_slli_epi32, a);
		FUNC_EXPAND8(17, _mm256_slli_epi32, a);
		FUNC_EXPAND4(25, _mm256_slli_epi32, a);
		FUNC_EXPAND2(29, _mm256_slli_epi32, a);
		case 31:	return _mm256_slli_epi32(a, 31);
		default:	return I32x8_zero();
	}
}

static inline I32x8 I32x8_rsh32(I32x8 a, U8 bits) {
	switch (bits) {
		case 0:		return a;
		FUNC_EXPAND16(1, _mm256_srli_epi32, a);
		FUNC_EXPAND8(17, _mm256_srli_epi32, a);
		FUNC_EXPAND4(25, _mm256_srli_epi32, a);
		FUNC_EXPAND2(29, _mm256_srli_epi32, a);
		case 31:	return _mm256_srli_epi32(a, 31);
		default:	return I32x8_zero();
	}
}

#define HAS_CLMUL64x2
#define HAS_AESx2

#ifdef __cplusplus
		}
#endif
