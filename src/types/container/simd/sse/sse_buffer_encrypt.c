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

#include <wmmintrin.h>

I32x4 AES_keyGenAssist(I32x4 a, U8 i) {

	if(i >= 11)
		return I32x4_zero();

	switch (i) {
		case 0:		return _mm_aeskeygenassist_si128(a, 0x00);
		case 1:		return _mm_aeskeygenassist_si128(a, 0x01);
		case 2:		return _mm_aeskeygenassist_si128(a, 0x02);
		case 3:		return _mm_aeskeygenassist_si128(a, 0x04);
		case 4:		return _mm_aeskeygenassist_si128(a, 0x08);
		case 5:		return _mm_aeskeygenassist_si128(a, 0x10);
		case 6:		return _mm_aeskeygenassist_si128(a, 0x20);
		case 7:		return _mm_aeskeygenassist_si128(a, 0x40);
		case 8:		return _mm_aeskeygenassist_si128(a, 0x80);
		case 9:		return _mm_aeskeygenassist_si128(a, 0x1B);
		default:	return _mm_aeskeygenassist_si128(a, 0x36);
	}
}

I32x4 AES_encodeBlock(I32x4 a, I32x4 b) { return _mm_aesenc_si128(a, b); }
I32x4 AES_encodeBlockLast(I32x4 a, I32x4 b) { return _mm_aesenclast_si128(a, b); }
