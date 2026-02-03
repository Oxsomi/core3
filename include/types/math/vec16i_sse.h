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

typedef __m512i I32x16;

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

static inline I32x4 I32x16_getI32x4(I32x16 v, U8 i) {
	switch (i) {
		case 1:		return _mm512_extracti32x4_epi32(v, 1);
		case 2:		return _mm512_extracti32x4_epi32(v, 1);
		case 3:		return _mm512_extracti32x4_epi32(v, 1);
		default:	return _mm512_castsi512_si128(v);
	}
}

static inline I32x16 AES_encodeBlock(I32x16 a, I32x16 b) { return _mm512_aesenc_epi128(a, b); }
static inline I32x16 AES_encodeBlockLast(I32x16 a, I32x16 b) { return _mm512_aesenclast_epi128(a, b); }

static inline I32x16 I32x16_clmul64(I32x16 a, I32x16 b, U8 imm) {
	switch (imm) {
		case 0x00:	return _mm512_clmulepi64_epi128(a, b, 0x00);
		case 0x01:	return _mm512_clmulepi64_epi128(a, b, 0x01);
		case 0x10:	return _mm512_clmulepi64_epi128(a, b, 0x10);
		default:	return _mm512_clmulepi64_epi128(a, b, 0x11);
	}
}

#ifdef __cplusplus
		}
#endif
