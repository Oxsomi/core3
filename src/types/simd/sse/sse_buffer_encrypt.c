/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/type_cast.h"
#include "types/vec.h"

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

I32x4 AES_encodeBlock(I32x4 a, I32x4 b, Bool isLast) {
	return isLast ? _mm_aesenclast_si128(a, b) : _mm_aesenc_si128(a, b);
}

void AESEncryptionContext_ghashPrepare(I32x4 H, I32x4 ghashLut[17]) {
	ghashLut[0] = I32x4_swapEndianness(H);
}

//Refactored from https://www.intel.com/content/dam/develop/external/us/en/documents/clmul-wp-rev-2-02-2014-04-20.pdf

I32x4 AESEncryptionContext_ghash(I32x4 a, const I32x4 ghashLut[17]) {

	a = I32x4_swapEndianness(a);
	I32x4 b = ghashLut[0];

	I32x4 tmp[8];

	tmp[0] = _mm_clmulepi64_si128(a, b, 0x00);		//TODO: Abstract this so this can be generalized

	tmp[3] = I32x4_xor(
		_mm_clmulepi64_si128(a, b, 0x10),
		_mm_clmulepi64_si128(a, b, 0x01)
	);

	tmp[2] = _mm_clmulepi64_si128(a, b, 0x11);

	tmp[1] = I32x4_lshByte(tmp[3], 8);
	tmp[3] = I32x4_rshByte(tmp[3], 8);

	for(U8 i = 0; i < 2; ++i) {
		I32x4 t = I32x4_xor(tmp[i << 1], tmp[(i << 1) + 1]);
		tmp[i << 1] = I32x4_lsh32(t, 1);
		tmp[4 + (i << 1)] = I32x4_rsh32(t, 31);
	}

	tmp[7] = I32x4_rshByte(tmp[4], 12);

	for(U8 i = 0; i < 2; ++i)
		tmp[6 - i] = I32x4_lshByte(tmp[6 - (i << 1)], 4);

	const U8 v0[3] = { 31, 30, 25 };

	for(U8 i = 0; i < 3; ++i) {
		tmp[i << 1] = I32x4_or(tmp[i ? 2 : 0], tmp[5 + i]);
		tmp[5 + i] = I32x4_lsh32(tmp[0], v0[i]);
	}

	for(U8 i = 0; i < 2; ++i)
		tmp[5] = I32x4_xor(tmp[5], tmp[6 + i]);

	tmp[3] = I32x4_rshByte(tmp[5], 4);
	tmp[5] = I32x4_xor(tmp[0], I32x4_lshByte(tmp[5], 12));

	const U8 v1[3] = { 1, 2, 7 };

	for(U8 i = 0; i < 3; ++i)
		tmp[i] = I32x4_rsh32(tmp[5], v1[i]);

	for(U8 i = 1; i < 6; ++i)
		tmp[0] = I32x4_xor(tmp[0], tmp[i]);

	return I32x4_swapEndianness(tmp[0]);
}
