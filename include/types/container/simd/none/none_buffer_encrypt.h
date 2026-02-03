/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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

#include "types/container/buffer_encrypt.h"
#include "types/math/vec4i.h"

//Implemented from
//https://www.intel.com/content/dam/doc/white-paper/advanced-encryption-standard-new-instructions-set-paper.pdf
//https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.197.pdf

static inline U32 AES_rotWord(U32 a) {
	return (a >> 8) | (a << 24);
}

I32x4 AES_keyGenAssist(I32x4 a, U8 i) {

	if(i >= 11)
		return I32x4_zero();

	//https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_aeskeygenassist_si128&ig_expand=6746,293

	U32 x1 = I32x4_y(a);
	U32 x3 = I32x4_w(a);

	const U32 rcons[] = { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36 };
	const U32 rcon = rcons[i];

	x1 = AES_subWord(x1);
	x3 = AES_subWord(x3);

	return I32x4_create4(
		x1,
		AES_rotWord(x1) ^ rcon,
		x3,
		AES_rotWord(x3) ^ rcon
	);
}

typedef struct U8x4x4 {
	U8 v[4][4];
} U8x4x4;

static inline U8x4x4 U8x4x4_transpose(const U8x4x4 *r) {

	U8x4x4 t = *r;

	for(U8 i = 0; i < 4; ++i)
		for(U8 j = 0; j < 4; ++j)
			t.v[i][j] = r->v[j][i];

	return t;
}

typedef union I32x4_U8x4x4 {
	I32x4 v;
	U8x4x4 v4x4;
} I32x4_U8x4x4;

static inline I32x4 AES_shiftRows(I32x4 a) {

	I32x4_U8x4x4 ap;
	ap.v = a;

	I32x4_U8x4x4 res = ap;

	for(U64 j = 0; j < 4; ++j)
		for(U64 i = 1; i < 4; ++i)
			res.v4x4.v[j][i] = ap.v4x4.v[(i + j) & 3][i];

	return res.v;
}

static inline I32x4 AES_subBytes(I32x4 a) {

	I32x4 res = a;
	U8 *ptr = (U8*)&res;

	for(U8 i = 0; i < 16; ++i)
		ptr[i] = AES_sbox(ptr[i]);

	return res;
}

static inline U8 AES_g2_8(U8 v, U8 mul) {
	switch (mul) {
		case 2:		return (v << 1) ^ ((v >> 7) * 0x1B);
		case 3:		return v ^ AES_g2_8(v, 2);
		default:	return v;
	}
}

static U8 AES_MIX_COLUMN[4][4] = {
	{ 2, 3, 1, 1 },
	{ 1, 2, 3, 1 },
	{ 1, 1, 2, 3 },
	{ 3, 1, 1, 2 }
};

static inline I32x4 AES_mixColumns(I32x4 vvv) {

	I32x4_U8x4x4 v;
	v.v = vvv;

	v.v4x4 = U8x4x4_transpose(&v.v4x4);

	U8x4x4 r = { 0 };

	for(U8 i = 0; i < 4; ++i)
		for(U8 j = 0; j < 4; ++j)
			for(U8 k = 0; k < 4; ++k)
				r.v[j][i] ^= AES_g2_8(v.v4x4.v[k][i], AES_MIX_COLUMN[j][k]);

	r = U8x4x4_transpose(&r);

	I32x4 res = I32x4_zero();
	Buffer_memcpy(Buffer_createRef(&res, sizeof(res)), Buffer_createRefConst(&r, sizeof(r)));
	return res;
}

static inline I32x4 AES_encodeBlock(I32x4 a, I32x4 b) {
	I32x4 t = AES_shiftRows(a);
	t = AES_subBytes(t);
	t = AES_mixColumns(t);
	return I32x4_xor(t, b);
}

static inline I32x4 AES_encodeBlockLast(I32x4 a, I32x4 b) {
	I32x4 t = AES_shiftRows(a);
	t = AES_subBytes(t);
	return I32x4_xor(t, b);
}
