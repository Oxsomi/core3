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

#include "types/container/buffer_encrypt.h"
#include "types/container/simd/sse/sse_buffer_encrypt.h"
#include "types/container/simd/aes_encryption_helpers.h"
#include "types/math/vec8i_sse.h"
#include "types/base/endianness.h"

void AESEncryptionContext_updateTagN(
	AESEncryptionContext *restrict ctx, const I32x4 *restrict CTi, const U8 N, U8 use256Or512
);

static inline I32x8 AESEncryptionContext_ghashReduceClMul2(I32x8 clmul00, I32x8 clmulFused, I32x8 clmul11) {
		
	I32x8 tmp[8];

	tmp[0] = clmul00;
	tmp[3] = clmulFused;
	tmp[2] = clmul11;

	tmp[1] = I32x8_lshElements(tmp[3], 2);
	tmp[3] = I32x8_rshElements(tmp[3], 2);

	for (U8 i = 0; i < 2; ++i) {
		I32x8 t = I32x8_xor(tmp[i << 1], tmp[(i << 1) + 1]);
		tmp[i << 1] = I32x8_lsh32(t, 1);
		tmp[4 + (i << 1)] = I32x8_rsh32(t, 31);
	}

	tmp[7] = I32x8_rshElements(tmp[4], 3);

	for (U8 i = 0; i < 2; ++i)
		tmp[6 - i] = I32x8_lshElements(tmp[6 - (i << 1)], 1);

	const U8 v0[3] = { 31, 30, 25 };

	for (U8 i = 0; i < 3; ++i) {
		tmp[i << 1] = I32x8_or(tmp[i ? 2 : 0], tmp[5 + i]);
		tmp[5 + i] = I32x8_lsh32(tmp[0], v0[i]);
	}

	for (U8 i = 0; i < 2; ++i)
		tmp[5] = I32x8_xor(tmp[5], tmp[6 + i]);

	tmp[3] = I32x8_rshElements(tmp[5], 1);
	tmp[5] = I32x8_xor(tmp[0], I32x8_lshElements(tmp[5], 3));

	const U8 v1[3] = { 1, 2, 7 };

	for (U8 i = 0; i < 3; ++i)
		tmp[i] = I32x8_rsh32(tmp[5], v1[i]);

	for (U8 i = 1; i < 6; ++i)
		tmp[0] = I32x8_xor(tmp[0], tmp[i]);

	return I32x8_swapEndianness(tmp[0]);
}

void AESEncryptionContext_ghashN2(I32x4 *restrict a, const I32x4 *restrict H, U8 N, I32x4 *restrict clmuls) {

	I32x8 clmul00_8[32];
	I32x8 clmul11_8[32];
	I32x8 clmulFused_8[32];

	I32x8 a8[32];

	const U8 N2 = N >> 1;

	for (U32 i = 0; i < N2; ++i)
		a8[i] = I32x8_swapEndianness(I32x8_load(&a[i << 1]));

	//Looks a bit odd, but it's to allow multiple clmuls to run in parallel.
	//Then, it'll be xored later. If we do clmulNN[i] ^= it creates a dependency, stalling everything.

	for (U32 i = 0; i < N2; ++i) {
		I32x8 Hi = I32x8_create4_4(H[N - 1 - (i << 1)], H[N - 2 - (i << 1)]);
		I32x8 clmul01 = I32x8_clmul64(a8[i], Hi, 0x01);
		I32x8 clmul10 = I32x8_clmul64(a8[i], Hi, 0x10);
		clmul00_8[i] = I32x8_clmul64(a8[i], Hi, 0x00);
		clmul11_8[i] = I32x8_clmul64(a8[i], Hi, 0x11);
		clmulFused_8[i] = I32x8_xor(clmul01, clmul10);
	}

	if (N2 > 1) {

		for (U32 i = 0; i < (U32)(N2 >> 1); ++i) {
			U32 left = i << 1;
			clmul00_8[left] = I32x8_xor(clmul00_8[left], clmul00_8[left | 1]);
			clmul11_8[left] = I32x8_xor(clmul11_8[left], clmul11_8[left | 1]);
			clmulFused_8[left] = I32x8_xor(clmulFused_8[left], clmulFused_8[left | 1]);
		}

		if (N2 > 2) {

			for (U32 i = 0; i < (U32)(N2 >> 2); ++i) {
				U32 left = i << 2;
				clmul00_8[left] = I32x8_xor(clmul00_8[left], clmul00_8[left | 2]);
				clmul11_8[left] = I32x8_xor(clmul11_8[left], clmul11_8[left | 2]);
				clmulFused_8[left] = I32x8_xor(clmulFused_8[left], clmulFused_8[left | 2]);
			}

			if (N2 > 4) {

				for (U32 i = 0; i < (U32)(N2 >> 3); ++i) {
					U32 left = i << 3;
					clmul00_8[left] = I32x8_xor(clmul00_8[left], clmul00_8[left | 4]);
					clmul11_8[left] = I32x8_xor(clmul11_8[left], clmul11_8[left | 4]);
					clmulFused_8[left] = I32x8_xor(clmulFused_8[left], clmulFused_8[left | 4]);
				}

				if (N2 > 8) {

					for (U32 i = 0; i < (U32)(N2 >> 4); ++i) {
						U32 left = i << 4;
						clmul00_8[left] = I32x8_xor(clmul00_8[left], clmul00_8[left | 8]);
						clmul11_8[left] = I32x8_xor(clmul11_8[left], clmul11_8[left | 8]);
						clmulFused_8[left] = I32x8_xor(clmulFused_8[left], clmulFused_8[left | 8]);
					}

					if (N2 > 16) {
						clmul00_8[0] = I32x8_xor(clmul00_8[0], clmul00_8[16]);
						clmul11_8[0] = I32x8_xor(clmul11_8[0], clmul11_8[16]);
						clmulFused_8[0] = I32x8_xor(clmulFused_8[0], clmulFused_8[16]);
					}
				}
			}
		}
	}

	clmuls[0] = I32x4_xor(I32x8_getI32x4(clmul00_8[0], 0), I32x8_getI32x4(clmul00_8[0], 1));
	clmuls[1] = I32x4_xor(I32x8_getI32x4(clmulFused_8[0], 0), I32x8_getI32x4(clmulFused_8[0], 1));
	clmuls[2] = I32x4_xor(I32x8_getI32x4(clmul11_8[0], 0), I32x8_getI32x4(clmul11_8[0], 1));
}

void AESEncryptionContext_ghashTable2(I32x4 *restrict H) {

	I32x8 a = I32x8_load(H);
	I32x8 b = I32x8_xx4(H[1]);
	I32x8 clmul01 = I32x8_clmul64(a, b, 0x01);
	I32x8 clmul10 = I32x8_clmul64(a, b, 0x10);
	I32x8 clmul00 = I32x8_clmul64(a, b, 0x00);
	I32x8 clmul11 = I32x8_clmul64(a, b, 0x11);

	I32x8 clmulFused = I32x8_xor(clmul01, clmul10);

	a = AESEncryptionContext_ghashReduceClMul2(clmul00, clmulFused, clmul11);
	a = I32x8_swapEndiannessI32x4(a);
	I32x8_store(&H[2], a);
}

void AESEncryptionContext_ghashTable2_4(I32x4 *restrict H, I32x4 H2, I32x4 H3, I32x4 H4) {

	I32x8 a0 = I32x8_create4_4(H2, H3);
	I32x8 a1 = I32x8_create4_4(H3, H4);
	I32x8 b0 = I32x8_xx4(H3);
	I32x8 b1 = I32x8_xx4(H4);

	I32x8 clmul01_0 = I32x8_clmul64(a0, b0, 0x01);
	I32x8 clmul10_0 = I32x8_clmul64(a0, b0, 0x10);
	I32x8 clmul00_0 = I32x8_clmul64(a0, b0, 0x00);
	I32x8 clmul11_0 = I32x8_clmul64(a0, b0, 0x11);

	I32x8 clmul01_1 = I32x8_clmul64(a1, b1, 0x01);
	I32x8 clmul10_1 = I32x8_clmul64(a1, b1, 0x10);
	I32x8 clmul00_1 = I32x8_clmul64(a1, b1, 0x00);
	I32x8 clmul11_1 = I32x8_clmul64(a1, b1, 0x11);

	I32x8 clmulFused0 = I32x8_xor(clmul01_0, clmul10_0);
	I32x8 clmulFused1 = I32x8_xor(clmul01_1, clmul10_1);

	a0 = AESEncryptionContext_ghashReduceClMul2(clmul00_0, clmulFused0, clmul11_0);
	a1 = AESEncryptionContext_ghashReduceClMul2(clmul00_1, clmulFused1, clmul11_1);
	a0 = I32x8_swapEndiannessI32x4(a0);
	a1 = I32x8_swapEndiannessI32x4(a1);

	I32x8_store(&H[0], a0);
	I32x8_store(&H[2], a1);
}

//Almost a copy paste from the I32x4 blockHash4 version, but I32x8 and duplicating k

typedef struct I32x8x4 { I32x8 a, b, c, d; } I32x8x4;

__forceinline__ static I32x8x4 AESEncryptionContext_blockHash8(
	I32x8 a, I32x8 b, I32x8 c, I32x8 d, const I32x4 *restrict k/*[15]*/, const EBufferEncryptionType type
) {

	I32x8 k0 = I32x8_xx4(k[0]);
	a = I32x8_xor(a, k0);
	b = I32x8_xor(b, k0);
	c = I32x8_xor(c, k0);
	d = I32x8_xor(d, k0);

	I32x8 k1 = I32x8_xx4(k[1]);
	a = I32x8_aesEnc(a, k1);
	b = I32x8_aesEnc(b, k1);
	c = I32x8_aesEnc(c, k1);
	d = I32x8_aesEnc(d, k1);

	I32x8 k2 = I32x8_xx4(k[2]);
	a = I32x8_aesEnc(a, k2);
	b = I32x8_aesEnc(b, k2);
	c = I32x8_aesEnc(c, k2);
	d = I32x8_aesEnc(d, k2);

	I32x8 k3 = I32x8_xx4(k[3]);
	a = I32x8_aesEnc(a, k3);
	b = I32x8_aesEnc(b, k3);
	c = I32x8_aesEnc(c, k3);
	d = I32x8_aesEnc(d, k3);

	I32x8 k4 = I32x8_xx4(k[4]);
	a = I32x8_aesEnc(a, k4);
	b = I32x8_aesEnc(b, k4);
	c = I32x8_aesEnc(c, k4);
	d = I32x8_aesEnc(d, k4);

	I32x8 k5 = I32x8_xx4(k[5]);
	a = I32x8_aesEnc(a, k5);
	b = I32x8_aesEnc(b, k5);
	c = I32x8_aesEnc(c, k5);
	d = I32x8_aesEnc(d, k5);

	I32x8 k6 = I32x8_xx4(k[6]);
	a = I32x8_aesEnc(a, k6);
	b = I32x8_aesEnc(b, k6);
	c = I32x8_aesEnc(c, k6);
	d = I32x8_aesEnc(d, k6);

	I32x8 k7 = I32x8_xx4(k[7]);
	a = I32x8_aesEnc(a, k7);
	b = I32x8_aesEnc(b, k7);
	c = I32x8_aesEnc(c, k7);
	d = I32x8_aesEnc(d, k7);

	I32x8 k8 = I32x8_xx4(k[8]);
	a = I32x8_aesEnc(a, k8);
	b = I32x8_aesEnc(b, k8);
	c = I32x8_aesEnc(c, k8);
	d = I32x8_aesEnc(d, k8);

	I32x8 k9 = I32x8_xx4(k[9]);
	a = I32x8_aesEnc(a, k9);
	b = I32x8_aesEnc(b, k9);
	c = I32x8_aesEnc(c, k9);
	d = I32x8_aesEnc(d, k9);

	I32x8 k10 = I32x8_xx4(k[10]);

	if (type == EBufferEncryptionType_AES256GCM) {

		a = I32x8_aesEnc(a, k10);
		b = I32x8_aesEnc(b, k10);
		c = I32x8_aesEnc(c, k10);
		d = I32x8_aesEnc(d, k10);

		I32x8 k11 = I32x8_xx4(k[11]);
		a = I32x8_aesEnc(a, k11);
		b = I32x8_aesEnc(b, k11);
		c = I32x8_aesEnc(c, k11);
		d = I32x8_aesEnc(d, k11);

		I32x8 k12 = I32x8_xx4(k[12]);
		a = I32x8_aesEnc(a, k12);
		b = I32x8_aesEnc(b, k12);
		c = I32x8_aesEnc(c, k12);
		d = I32x8_aesEnc(d, k12);

		I32x8 k13 = I32x8_xx4(k[13]);
		a = I32x8_aesEnc(a, k13);
		b = I32x8_aesEnc(b, k13);
		c = I32x8_aesEnc(c, k13);
		d = I32x8_aesEnc(d, k13);

		I32x8 k14 = I32x8_xx4(k[14]);
		I32x8x4 res = {
			I32x8_aesEncLast(a, k14), I32x8_aesEncLast(b, k14),
			I32x8_aesEncLast(c, k14), I32x8_aesEncLast(d, k14)
		};

		return res;
	}

	I32x8x4 res = {
		I32x8_aesEncLast(a, k10), I32x8_aesEncLast(b, k10),
		I32x8_aesEncLast(c, k10), I32x8_aesEncLast(d, k10)
	};

	return res;
}

typedef struct I32x8x4AndTag {
	I32x8 a, b, c, d;
	I32x4 tag;
} I32x8x4AndTag;

__forceinline__ static I32x8x4AndTag AESEncryptionContext_blockHashAndGhash8(
	I32x8 a0,
	I32x8 a1,
	I32x8 a2,
	I32x8 a3,
	I32x8 H,
	I32x8 H2,
	I32x8 H3,
	I32x8 H4,
	I32x8 b0,
	I32x8 b1,
	I32x8 b2,
	I32x8 b3,
	I32x4 tag,
	const I32x4 *restrict k,
	const EBufferEncryptionType type
) {
	//Manual interleaving because you can't trust MSVC to generate proper code unfortunately.

	b0 = I32x8_xor(b0, I32x8_create4_4(tag, I32x4_zero()));

	I32x8 k0 = I32x8_xx4(k[0]);

	a0 = I32x8_xor(a0, k0);
	a1 = I32x8_xor(a1, k0);
	a2 = I32x8_xor(a2, k0);
	a3 = I32x8_xor(a3, k0);

	I32x8 k1 = I32x8_xx4(k[1]);

	b0 = I32x8_swapEndianness(b0);
	b1 = I32x8_swapEndianness(b1);
	b2 = I32x8_swapEndianness(b2);
	b3 = I32x8_swapEndianness(b3);

	a0 = I32x8_aesEnc(a0, k1);
	a1 = I32x8_aesEnc(a1, k1);

	I32x8 k2 = I32x8_xx4(k[2]);

	I32x8 clmul01_0 = I32x8_clmul64(b0, H4, 0x01);
	I32x8 clmul10_0 = I32x8_clmul64(b0, H4, 0x10);
	clmul01_0 = I32x8_xor(clmul01_0, clmul10_0);

	a2 = I32x8_aesEnc(a2, k1);
	a3 = I32x8_aesEnc(a3, k1);

	I32x8 clmul01_1 = I32x8_clmul64(b1, H3, 0x01);
	I32x8 clmul10_1 = I32x8_clmul64(b1, H3, 0x10);
	clmul01_1 = I32x8_xor(clmul01_1, clmul10_1);

	a0 = I32x8_aesEnc(a0, k2);
	a1 = I32x8_aesEnc(a1, k2);

	I32x8 k3 = I32x8_xx4(k[3]);

	I32x8 clmul01_2 = I32x8_clmul64(b2, H2, 0x01);
	I32x8 clmul10_2 = I32x8_clmul64(b2, H2, 0x10);
	clmul01_2 = I32x8_xor(clmul01_2, clmul10_2);

	a2 = I32x8_aesEnc(a2, k2);
	a3 = I32x8_aesEnc(a3, k2);

	I32x8 clmul01_3 = I32x8_clmul64(b3, H, 0x01);
	I32x8 clmul10_3 = I32x8_clmul64(b3, H, 0x10);
	clmul01_3 = I32x8_xor(clmul01_3, clmul10_3);

	a0 = I32x8_aesEnc(a0, k3);
	a1 = I32x8_aesEnc(a1, k3);

	I32x8 k4 = I32x8_xx4(k[4]);

	I32x8 clmul00_0 = I32x8_clmul64(b0, H4, 0x00);
	I32x8 clmul00_1 = I32x8_clmul64(b1, H3, 0x00);

	a2 = I32x8_aesEnc(a2, k3);
	a3 = I32x8_aesEnc(a3, k3);

	I32x8 clmul00_2 = I32x8_clmul64(b2, H2, 0x00);
	I32x8 clmul00_3 = I32x8_clmul64(b3, H, 0x00);

	a0 = I32x8_aesEnc(a0, k4);
	a1 = I32x8_aesEnc(a1, k4);

	I32x8 k5 = I32x8_xx4(k[5]);

	I32x8 clmul11_0 = I32x8_clmul64(b0, H4, 0x11);
	I32x8 clmul11_1 = I32x8_clmul64(b1, H3, 0x11);

	a2 = I32x8_aesEnc(a2, k4);
	a3 = I32x8_aesEnc(a3, k4);

	I32x8 clmul11_2 = I32x8_clmul64(b2, H2, 0x11);
	I32x8 clmul11_3 = I32x8_clmul64(b3, H, 0x11);

	a0 = I32x8_aesEnc(a0, k5);
	a1 = I32x8_aesEnc(a1, k5);
	I32x8 k6 = I32x8_xx4(k[6]);
	a2 = I32x8_aesEnc(a2, k5);
	a3 = I32x8_aesEnc(a3, k5);

	a0 = I32x8_aesEnc(a0, k6);
	a1 = I32x8_aesEnc(a1, k6);
	I32x8 k7 = I32x8_xx4(k[7]);
	a2 = I32x8_aesEnc(a2, k6);
	a3 = I32x8_aesEnc(a3, k6);

	a0 = I32x8_aesEnc(a0, k7);
	a1 = I32x8_aesEnc(a1, k7);
	I32x8 k8 = I32x8_xx4(k[8]);
	a2 = I32x8_aesEnc(a2, k7);
	a3 = I32x8_aesEnc(a3, k7);

	a0 = I32x8_aesEnc(a0, k8);
	a1 = I32x8_aesEnc(a1, k8);
	I32x8 k9 = I32x8_xx4(k[9]);
	a2 = I32x8_aesEnc(a2, k8);
	a3 = I32x8_aesEnc(a3, k8);

	a0 = I32x8_aesEnc(a0, k9);
	a1 = I32x8_aesEnc(a1, k9);
	I32x8 k10 = I32x8_xx4(k[10]);
	a2 = I32x8_aesEnc(a2, k9);
	a3 = I32x8_aesEnc(a3, k9);

	I32x4 b;

	if (type == EBufferEncryptionType_AES256GCM) {

		a0 = I32x8_aesEnc(a0, k10);
		a1 = I32x8_aesEnc(a1, k10);
		I32x8 k11 = I32x8_xx4(k[11]);
		a2 = I32x8_aesEnc(a2, k10);
		a3 = I32x8_aesEnc(a3, k10);

		clmul01_0 = I32x8_xor(clmul01_0, clmul01_1);
		clmul00_0 = I32x8_xor(clmul00_0, clmul00_1);
		clmul01_2 = I32x8_xor(clmul01_2, clmul01_3);
		clmul00_2 = I32x8_xor(clmul00_2, clmul00_3);

		a0 = I32x8_aesEnc(a0, k11);
		a1 = I32x8_aesEnc(a1, k11);
		I32x8 k12 = I32x8_xx4(k[12]);
		a2 = I32x8_aesEnc(a2, k11);
		a3 = I32x8_aesEnc(a3, k11);

		clmul01_0 = I32x8_xor(clmul01_0, clmul01_2);
		clmul00_0 = I32x8_xor(clmul00_0, clmul00_2);
		I32x4 clmul01 = I32x4_xor(I32x8_getI32x4(clmul01_0, 0), I32x8_getI32x4(clmul01_0, 1));
		clmul11_0 = I32x8_xor(clmul11_0, clmul11_1);
		clmul11_2 = I32x8_xor(clmul11_2, clmul11_3);

		I32x4 tmp1 = I32x4_lshElements(clmul01, 2);
		a0 = I32x8_aesEnc(a0, k12);
		a1 = I32x8_aesEnc(a1, k12);

		I32x4 clmul00 = I32x4_xor(I32x8_getI32x4(clmul00_0, 0), I32x8_getI32x4(clmul00_0, 1));

		clmul11_0 = I32x8_xor(clmul11_0, clmul11_2);
		I32x4 tmp3 = I32x4_rshElements(clmul01, 2);
		I32x4 t0 = I32x4_xor(clmul00, tmp1);
		a2 = I32x8_aesEnc(a2, k12);
		a3 = I32x8_aesEnc(a3, k12);

		I32x4 clmul11 = I32x4_xor(I32x8_getI32x4(clmul11_0, 0), I32x8_getI32x4(clmul11_0, 1));

		I32x8 k13 = I32x8_xx4(k[13]);

		I32x4 t1 = I32x4_xor(clmul11, tmp3);
		I32x4 tmp0 = I32x4_lsh32(t0, 1);
		I32x4 tmp4 = I32x4_rsh32(t0, 31);
		I32x4 tmp2 = I32x4_lsh32(t1, 1);
		a0 = I32x8_aesEnc(a0, k13);
		a1 = I32x8_aesEnc(a1, k13);

		I32x8 k14 = I32x8_xx4(k[14]);

		I32x4 tmp6 = I32x4_rsh32(t1, 31);
		I32x4 tmp7 = I32x4_rshElements(tmp4, 3);
		tmp6 = I32x4_lshElements(tmp6, 1);
		I32x4 tmp5 = I32x4_lshElements(tmp4, 1);
		a2 = I32x8_aesEnc(a2, k13);
		a3 = I32x8_aesEnc(a3, k13);

		tmp0 = I32x4_or(tmp0, tmp5);
		tmp2 = I32x4_or(tmp2, tmp6);
		tmp5 = I32x4_lsh32(tmp0, 31);
		tmp6 = I32x4_lsh32(tmp0, 30);
		tmp4 = I32x4_or(tmp2, tmp7);
		tmp7 = I32x4_lsh32(tmp0, 25);

		a0 = I32x8_aesEncLast(a0, k14);
		a1 = I32x8_aesEncLast(a1, k14);

		tmp5 = I32x4_xor(tmp5, tmp6);
		tmp5 = I32x4_xor(tmp5, tmp7);

		a2 = I32x8_aesEncLast(a2, k14);
		a3 = I32x8_aesEncLast(a3, k14);

		tmp6 = I32x4_lshElements(tmp5, 3);
		tmp3 = I32x4_rshElements(tmp5, 1);
		tmp5 = I32x4_xor(tmp0, tmp6);

		tmp0 = I32x4_rsh32(tmp5, 1);
		tmp1 = I32x4_rsh32(tmp5, 2);
		tmp2 = I32x4_rsh32(tmp5, 7);

		tmp0 = I32x4_xor(tmp0, tmp5);		//0 ^ 5
		tmp1 = I32x4_xor(tmp1, tmp2);		//1 ^ 2
		tmp3 = I32x4_xor(tmp3, tmp4);		//3 ^ 4
		tmp0 = I32x4_xor(tmp0, tmp1);		//0 ^ 1 ^ 2 ^ 5
		tmp0 = I32x4_xor(tmp0, tmp3);		//0 ^ 1 ^ 2 ^ 3 ^ 4 ^ 5

		b = I32x4_swapEndianness(tmp0);

	} else {

		a0 = I32x8_aesEncLast(a0, k10);
		a1 = I32x8_aesEncLast(a1, k10);
		a2 = I32x8_aesEncLast(a2, k10);
		a3 = I32x8_aesEncLast(a3, k10);

		clmul01_0 = I32x8_xor(clmul01_0, clmul01_1);
		clmul00_0 = I32x8_xor(clmul00_0, clmul00_1);
		clmul11_0 = I32x8_xor(clmul11_0, clmul11_1);

		clmul01_2 = I32x8_xor(clmul01_2, clmul01_3);
		clmul00_2 = I32x8_xor(clmul00_2, clmul00_3);
		clmul11_2 = I32x8_xor(clmul11_2, clmul11_3);

		clmul01_0 = I32x8_xor(clmul01_0, clmul01_2);
		clmul00_0 = I32x8_xor(clmul00_0, clmul00_2);
		clmul11_0 = I32x8_xor(clmul11_0, clmul11_2);

		I32x4 clmul01 = I32x4_xor(I32x8_getI32x4(clmul01_0, 0), I32x8_getI32x4(clmul01_0, 1));
		I32x4 clmul00 = I32x4_xor(I32x8_getI32x4(clmul00_0, 0), I32x8_getI32x4(clmul00_0, 1));
		I32x4 clmul11 = I32x4_xor(I32x8_getI32x4(clmul11_0, 0), I32x8_getI32x4(clmul11_0, 1));
		
		b = AESEncryptionContext_ghashReduceClMul(clmul00, clmul01, clmul11);
	}

	I32x8x4AndTag abcde = { a0, a1, a2, a3, b };
	return abcde;
}

void AESEncryptionContext_blocks8(
	U32 *restrict counterForIv,
	I32x4 *restrict *restrict next,
	I32x4 *restrict end,
	I32x4 iv,
	I32x4 *restrict H,
	I32x4 *restrict k,
	I32x4 *restrict tagPtr,
	const Bool isEncrypt,
	const EBufferEncryptionType encryptionType
) {
	(void)end;

	//Prologue

	I32x8 prevState[4];
	I32x8 H12 = I32x8_yxI32x4(I32x8_load(H));
	I32x8 H34 = I32x8_yxI32x4(I32x8_load(H + 2));
	I32x8 H56 = I32x8_yxI32x4(I32x8_load(H + 4));
	I32x8 H78 = I32x8_yxI32x4(I32x8_load(H + 6));

	//Don't waste time on a ghash that's not gonna be used
	//I manually unrolled this and optimized the instruction order, because MSVC really can't compile well at alll.

	{
		U32 counter = *counterForIv;

		I32x4 ivi0 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter));
		I32x4 ivi1 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 1));
		I32x8 ivi01 = I32x8_create4_4(ivi0, ivi1);

		I32x4 ivi2 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 2));
		I32x4 ivi3 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 3));
		I32x8 ivi23 = I32x8_create4_4(ivi2, ivi3);

		I32x4 ivi4 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 4));
		I32x4 ivi5 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 5));
		I32x8 ivi45 = I32x8_create4_4(ivi4, ivi5);

		I32x4 ivi6 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 6));
		I32x4 ivi7 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 7));
		I32x8 ivi67 = I32x8_create4_4(ivi6, ivi7);

		*counterForIv += 8;

		I32x8x4 ab = AESEncryptionContext_blockHash8(ivi01, ivi23, ivi45, ivi67, k, encryptionType);

		I32x4 *restrict nextAddr = *next;
		I32x8 data = I32x8_load(nextAddr + 0);
		I32x8 datb = I32x8_load(nextAddr + 2);
		I32x8 datc = I32x8_load(nextAddr + 4);
		I32x8 datd = I32x8_load(nextAddr + 6);

		I32x8 a = I32x8_xor(ab.a, data);
		I32x8 b = I32x8_xor(ab.b, datb);
		I32x8 c = I32x8_xor(ab.c, datc);
		I32x8 d = I32x8_xor(ab.d, datd);

		if (!isEncrypt) {			//Decryption, we use the input (ciphertext)
			prevState[0] = data;
			prevState[1] = datb;
			prevState[2] = datc;
			prevState[3] = datd;
		}

		else {						//Encryption, we use the output (ciphertext)
			prevState[0] = a;
			prevState[1] = b;
			prevState[2] = c;
			prevState[3] = d;
		}

		I32x8_store(nextAddr + 0, a);
		I32x8_store(nextAddr + 2, b);
		I32x8_store(nextAddr + 4, c);
		I32x8_store(nextAddr + 6, d);
		*next += 8;
	}

	I32x4 tag = *tagPtr;

	//Contents

	while (*next + 4 <= end) {

		U32 counter = *counterForIv;

		I32x4 ivi0 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter));
		I32x4 ivi1 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 1));
		I32x8 ivi01 = I32x8_create4_4(ivi0, ivi1);

		I32x4 ivi2 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 2));
		I32x4 ivi3 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 3));
		I32x8 ivi23 = I32x8_create4_4(ivi2, ivi3);

		I32x4 ivi4 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 4));
		I32x4 ivi5 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 5));
		I32x8 ivi45 = I32x8_create4_4(ivi4, ivi5);

		I32x4 ivi6 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 6));
		I32x4 ivi7 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 7));
		I32x8 ivi67 = I32x8_create4_4(ivi6, ivi7);

		*counterForIv += 8;

		I32x8x4AndTag ab = AESEncryptionContext_blockHashAndGhash8(
			ivi01, ivi23, ivi45, ivi67,
			H12, H34, H56, H78,
			prevState[0], prevState[1], prevState[2], prevState[3],
			tag,
			k,
			encryptionType
		);

		tag = ab.tag;

		I32x4 *restrict nextAddr = *next;
		I32x8 data = I32x8_load(nextAddr + 0);
		I32x8 datb = I32x8_load(nextAddr + 2);
		I32x8 datc = I32x8_load(nextAddr + 4);
		I32x8 datd = I32x8_load(nextAddr + 6);

		I32x8 a = I32x8_xor(ab.a, data);
		I32x8 b = I32x8_xor(ab.b, datb);
		I32x8 c = I32x8_xor(ab.c, datc);
		I32x8 d = I32x8_xor(ab.d, datd);

		if (!isEncrypt) {				//Decryption, we use the input (ciphertext)
			prevState[0] = data;
			prevState[1] = datb;
			prevState[2] = datc;
			prevState[3] = datd;
		}

		else {							//Encryption, we use the output (ciphertext)
			prevState[0] = a;
			prevState[1] = b;
			prevState[2] = c;
			prevState[3] = d;
		}

		I32x8_store(nextAddr + 0, a);
		I32x8_store(nextAddr + 2, b);
		I32x8_store(nextAddr + 4, c);
		I32x8_store(nextAddr + 6, d);
		*next += 8;
	}

	//Epilogue

	{
		I32x8 a = I32x8_xor(prevState[0], I32x8_create4_4(tag, I32x4_zero()));
		I32x8 b = prevState[1];
		I32x8 c = prevState[2];
		I32x8 d = prevState[3];

		a = I32x8_swapEndianness(a);
		b = I32x8_swapEndianness(b);
		c = I32x8_swapEndianness(c);
		d = I32x8_swapEndianness(d);

		I32x8 clmul01a = I32x8_clmul64(a, H78, 0x01);
		I32x8 clmul10a = I32x8_clmul64(a, H78, 0x10);
		clmul01a = I32x8_xor(clmul01a, clmul10a);

		I32x8 clmul01b = I32x8_clmul64(b, H56, 0x01);
		I32x8 clmul10b = I32x8_clmul64(b, H56, 0x10);
		clmul01b = I32x8_xor(clmul01b, clmul10b);

		I32x8 clmul01c = I32x8_clmul64(c, H34, 0x01);
		I32x8 clmul10c = I32x8_clmul64(c, H34, 0x10);
		clmul01c = I32x8_xor(clmul01c, clmul10c);

		I32x8 clmul01d = I32x8_clmul64(d, H12, 0x01);
		I32x8 clmul10d = I32x8_clmul64(d, H12, 0x10);
		clmul01d = I32x8_xor(clmul01d, clmul10d);

		I32x8 clmul00a = I32x8_clmul64(a, H78, 0x00);
		I32x8 clmul00b = I32x8_clmul64(b, H56, 0x00);
		I32x8 clmul00c = I32x8_clmul64(c, H34, 0x00);
		I32x8 clmul00d = I32x8_clmul64(d, H12, 0x00);

		I32x8 clmul11a = I32x8_clmul64(a, H78, 0x11);
		I32x8 clmul11b = I32x8_clmul64(b, H56, 0x11);
		I32x8 clmul11c = I32x8_clmul64(c, H34, 0x11);
		I32x8 clmul11d = I32x8_clmul64(d, H12, 0x11);

		clmul11a = I32x8_xor(clmul11a, clmul11b);
		clmul00a = I32x8_xor(clmul00a, clmul00b);
		clmul01a = I32x8_xor(clmul01a, clmul01b);

		clmul11c = I32x8_xor(clmul11c, clmul11d);
		clmul00c = I32x8_xor(clmul00c, clmul00d);
		clmul01c = I32x8_xor(clmul01c, clmul01d);

		clmul11a = I32x8_xor(clmul11a, clmul11c);
		clmul00a = I32x8_xor(clmul00a, clmul00c);
		clmul01a = I32x8_xor(clmul01a, clmul01c);

		I32x4 clmul11 = I32x4_xor(I32x8_getI32x4(clmul11a, 0), I32x8_getI32x4(clmul11a, 1));
		I32x4 clmul00 = I32x4_xor(I32x8_getI32x4(clmul00a, 0), I32x8_getI32x4(clmul00a, 1));
		I32x4 clmul01 = I32x4_xor(I32x8_getI32x4(clmul01a, 0), I32x8_getI32x4(clmul01a, 1));

		*tagPtr = AESEncryptionContext_ghashReduceClMul(clmul00, clmul01, clmul11);
	}
}
