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

#include "types/math/vec4i.h"
#include "types/math/vec8i_sse.h"
#include "types/math/vec16i_sse.h"

I32x4 I32x4_lsh32(I32x4 a, U8 bits) {
	switch (bits) {
		case 0:		return a;
		FUNC_EXPAND16(1, _mm_slli_epi32, a);
		FUNC_EXPAND8(17, _mm_slli_epi32, a);
		FUNC_EXPAND4(25, _mm_slli_epi32, a);
		FUNC_EXPAND2(29, _mm_slli_epi32, a);
		case 31:	return _mm_slli_epi32(a, 31);
		default:	return I32x4_zero();
	}
}

I32x4 I32x4_rsh32(I32x4 a, U8 bits) {
	switch (bits) {
		case 0:		return a;
		FUNC_EXPAND16(1, _mm_srli_epi32, a);
		FUNC_EXPAND8(17, _mm_srli_epi32, a);
		FUNC_EXPAND4(25, _mm_srli_epi32, a);
		FUNC_EXPAND2(29, _mm_srli_epi32, a);
		case 31:	return _mm_srli_epi32(a, 31);
		default:	return I32x4_zero();
	}
}

I32x4 I32x4_lsh64(I32x4 a, U8 bits) {
	switch (bits) {
		case 0:		return a;
		FUNC_EXPAND32(1, _mm_slli_epi64, a);
		FUNC_EXPAND16(33, _mm_slli_epi64, a);
		FUNC_EXPAND8(49, _mm_slli_epi64, a);
		FUNC_EXPAND4(57, _mm_slli_epi64, a);
		FUNC_EXPAND2(61, _mm_slli_epi64, a);
		case 63:	return _mm_slli_epi64(a, 63);
		default:	return I32x4_zero();
	}
}

I32x4 I32x4_rsh64(I32x4 a, U8 bits) {
	switch (bits) {
		case 0:		return a;
		FUNC_EXPAND32(1, _mm_srli_epi64, a);
		FUNC_EXPAND16(33, _mm_srli_epi64, a);
		FUNC_EXPAND8(49, _mm_srli_epi64, a);
		FUNC_EXPAND4(57, _mm_srli_epi64, a);
		FUNC_EXPAND2(61, _mm_srli_epi64, a);
		case 63:	return _mm_srli_epi64(a, 63);
		default:	return I32x4_zero();
	}
}

I32x8 I32x8_lsh32(I32x8 a, U8 bits) {
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

I32x8 I32x8_rsh32(I32x8 a, U8 bits) {
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

I32x16 I32x16_lsh32(I32x16 a, U8 bits) {
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

I32x16 I32x16_rsh32(I32x16 a, U8 bits) {
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
