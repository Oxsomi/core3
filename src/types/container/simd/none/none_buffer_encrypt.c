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

#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/container/buffer.h"
#include "types/math/type_cast.h"
#include "types/math/vec.h"

//Implemented from
//https://www.intel.com/content/dam/doc/white-paper/advanced-encryption-standard-new-instructions-set-paper.pdf
//https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.197.pdf

static const U8 AES_SBOX[256] = {
	0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
	0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0, 0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
	0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
	0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
	0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0, 0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
	0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
	0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
	0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5, 0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
	0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
	0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
	0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C, 0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
	0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
	0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
	0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E, 0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
	0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
	0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16
};

U32 AES_subWord(U32 a) {

	U32 res = 0;

	for(U8 i = 0; i < 4; ++i)
		res |= (U32)(AES_SBOX[(a >> (i * 8)) & 0xFF]) << (i * 8);

	return res;
}

U32 AES_rotWord(U32 a) {
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

U8x4x4 U8x4x4_transpose(const U8x4x4 *r) {

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

I32x4 AES_shiftRows(I32x4 a) {

	I32x4_U8x4x4 ap = (I32x4_U8x4x4) { .v = a };
	I32x4_U8x4x4 res = ap;

	for(U64 j = 0; j < 4; ++j)
		for(U64 i = 1; i < 4; ++i)
			res.v4x4.v[j][i] = ap.v4x4.v[(i + j) & 3][i];

	return res.v;
}

I32x4 AES_subBytes(I32x4 a) {

	I32x4 res = a;
	U8 *ptr = (U8*)&res;

	for(U8 i = 0; i < 16; ++i)
		ptr[i] = AES_SBOX[ptr[i]];

	return res;
}

U8 AES_g2_8(U8 v, U8 mul) {
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

I32x4 AES_mixColumns(I32x4 vvv) {

	I32x4_U8x4x4 v = (I32x4_U8x4x4) { .v = vvv };

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

I32x4 AES_encodeBlock(I32x4 a, I32x4 b, Bool isLast) {

	I32x4 t = AES_shiftRows(a);
	t = AES_subBytes(t);

	if(!isLast)
		t = AES_mixColumns(t);

	return I32x4_xor(t, b);
}

I32x4 AESEncryptionContext_rsh(I32x4 v, U8 shift) {

	U64 *a = (U64*) &v;
	U64 *b = a + 1;

	*a = (*a >> shift) | (*b << (64 - shift));
	*b >>= shift;

	return v;
}

//ghash computes the Galois field multiplication GF(2^128) with the current H (hash of AES256 encrypted zero block)
//for AES256 GCM + GMAC

//LUT creation from https://github.com/mko-x/SharedAES-GCM/blob/master/Sources/gcm.c#L207

void AESEncryptionContext_ghashPrepare(I32x4 H, I32x4 ghashLut[17]) {

	H = I32x4_swapEndianness(H);

	ghashLut[16] = H;

	ghashLut[0] = I32x4_zero();		//0 = 0 in GF(2^128)
	ghashLut[8] = H;				//8 (0b1000) corresponds to 1 in GF (2^128)

	for (U8 i = 4; i > 0; i >>= 1) {

		const I32x4 T = I32x4_create4(0, 0, 0, I32x4_x(H) & 1 ? 0xE1000000 : 0);
		H = AESEncryptionContext_rsh(H, 1);
		H = I32x4_xor(H, T);

		ghashLut[i] = H;
	}

	for (U8 i = 2; i < 16; i <<= 1) {

		H = ghashLut[i];

		for(U8 j = 1; j < i; ++j)
			ghashLut[j + i] = I32x4_xor(H, ghashLut[j]);
	}
}

//Implemented and optimized to SSE from https://github.com/mko-x/SharedAES-GCM/blob/master/Sources/gcm.c#L131
//(Of course we don't necessarily use SSE here, only if relax float is disabled)

const U16 GHASH_LAST4[16] = {
	0x0000, 0x1C20, 0x3840, 0x2460, 0x7080, 0x6CA0, 0x48C0, 0x54E0,
	0xE100, 0xFD20, 0xD940, 0xC560, 0x9180, 0x8DA0, 0xA9C0, 0xB5E0
};

I32x4 AESEncryptionContext_ghash(I32x4 aa, const I32x4 ghashLut[17]) {

	I32x4 zlZh = ghashLut[((U8*)&aa)[15] & 0xF];

	for (U8 i = 30; i != U8_MAX; --i) {

		const U8 rem = (U8)I32x4_x(zlZh) & 0xF;
		const U8 ind = (((U8*)&aa)[i / 2] >> (4 * (1 - (i & 1)))) & 0xF;

		zlZh = AESEncryptionContext_rsh(zlZh, 4);
		zlZh = I32x4_xor(zlZh, I32x4_create4(0, 0, 0, (U32)GHASH_LAST4[rem] << 16));
		zlZh = I32x4_xor(zlZh, ghashLut[ind]);
	}

	return I32x4_swapEndianness(zlZh);
}
