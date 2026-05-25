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

//types/math/vec16i.h

#pragma once
#include "types/math/vec4.h"
#include <immintrin.h>

#ifdef __cplusplus
	extern "C" {
#endif

//Even though there's no real fallback for this with either NEON or NONE, they're still abstracted just in case.

typedef __m512i I32x16;

static inline I32x16 I32x16_zero() { return _mm512_setzero_si512(); }
static inline I32x16 I32x16_load(const void *addr) { return _mm512_loadu_si512(addr); }
static inline void I32x16_store(void *addr, I32x16 v) { _mm512_storeu_si512(addr, v); }

static inline I32x16 I32x16_create4_4_4_4(I32x4 a, I32x4 b, I32x4 c, I32x4 d) {
	I32x16 result = _mm512_castsi128_si512(a);
	result = _mm512_inserti32x4(result, b, 1);
	result = _mm512_inserti32x4(result, c, 2);
	result = _mm512_inserti32x4(result, d, 3);
	return result;
}

static inline I32x16 I32x16_xxxx4(I32x4 a) { return I32x16_create4_4_4_4(a, a, a, a); }

static inline I32x16 I32x16_xor(I32x16 a, I32x16 b) { return _mm512_xor_si512(a, b); }
static inline I32x16 I32x16_or(I32x16 a, I32x16 b) { return _mm512_or_si512(a, b); }

static inline I32x16 I32x16_lshElements(I32x16 a, U8 elementCount) {
	switch (elementCount) {
		case 0:		return a;
		case 1:		return _mm512_bslli_epi128(a, 4);
		case 2:		return _mm512_bslli_epi128(a, 8);
		case 3:		return _mm512_bslli_epi128(a, 12);
		default:	return I32x16_zero();
	}
}

static inline I32x16 I32x16_rshElements(I32x16 a, U8 elementCount) {
	switch (elementCount) {
		case 0:		return a;
		case 1:		return _mm512_bsrli_epi128(a, 4);
		case 2:		return _mm512_bsrli_epi128(a, 8);
		case 3:		return _mm512_bsrli_epi128(a, 12);
		default:	return I32x16_zero();
	}
}

static inline I32x4 I32x16_getI32x4(I32x16 v, U8 i) {
	switch (i) {
		case 1:		return _mm512_extracti32x4_epi32(v, 1);
		case 2:		return _mm512_extracti32x4_epi32(v, 2);
		case 3:		return _mm512_extracti32x4_epi32(v, 3);
		default:	return _mm512_castsi512_si128(v);
	}
}

static inline I32x8 I32x16_getI32x8(I32x16 v, U8 i) {
	switch (i) {
		case 1:		return _mm512_extracti32x8_epi32(v, 1);
		default:	return _mm512_castsi512_si256(v);
	}
}

static inline I32x16 I32x16_aesEnc(I32x16 a, I32x16 b) {
	return _mm512_aesenc_epi128(a, b);
}

static inline I32x16 I32x16_aesEncLast(I32x16 a, I32x16 b) {
	return _mm512_aesenclast_epi128(a, b);
}

static inline I32x16 I32x16_clmul64(I32x16 a, I32x16 b, U8 imm) {
	switch (imm) {
		case 0x00:	return _mm512_clmulepi64_epi128(a, b, 0x00);
		case 0x01:	return _mm512_clmulepi64_epi128(a, b, 0x01);
		case 0x10:	return _mm512_clmulepi64_epi128(a, b, 0x10);
		default:	return _mm512_clmulepi64_epi128(a, b, 0x11);
	}
}

static inline I32x16 I32x16_swapEndianness(I32x16 v) {
	return _mm512_shuffle_epi8(v, _mm512_set_epi8(
		 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
		16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
		32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
		48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
	));
}

static inline I32x16 I32x16_swapEndiannessI32x4(I32x16 v) {
	return _mm512_shuffle_epi8(v, _mm512_set_epi8(
		48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
		32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
		16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
		 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
	));
}

//Swaps I32x4[4] into wzyx order, useful for hash loads.
static inline I32x16 I32x16_wzyxI32x4(I32x16 v) {
	return _mm512_shuffle_i32x4(v, v, _MM_SHUFFLE(0, 1, 2, 3));
}

static inline I32x16 I32x16_lsh32(I32x16 a, U8 bits) {
	switch (bits) {
		case 0:		return a;
		FUNC_EXPAND16(1, _mm512_slli_epi32, a);
		FUNC_EXPAND8(17, _mm512_slli_epi32, a);
		FUNC_EXPAND4(25, _mm512_slli_epi32, a);
		FUNC_EXPAND2(29, _mm512_slli_epi32, a);
		case 31:	return _mm512_slli_epi32(a, 31);
		default:	return I32x16_zero();
	}
}

static inline I32x16 I32x16_rsh32(I32x16 a, U8 bits) {
	switch (bits) {
		case 0:		return a;
		FUNC_EXPAND16(1, _mm512_srli_epi32, a);
		FUNC_EXPAND8(17, _mm512_srli_epi32, a);
		FUNC_EXPAND4(25, _mm512_srli_epi32, a);
		FUNC_EXPAND2(29, _mm512_srli_epi32, a);
		case 31:	return _mm512_srli_epi32(a, 31);
		default:	return I32x16_zero();
	}
}

#define HAS_CLMUL64x4
#define HAS_AESx4

#ifdef __cplusplus
		}
#endif
