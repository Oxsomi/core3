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

//types/container/simd/sse/buffer_encrypt_avx512.c

#include "types/container/buffer_encrypt.h"
#include "types/container/simd/sse/sse_buffer_encrypt.h"
#include "types/container/simd/aes_encryption_helpers.h"
#include "types/math/vec8i_sse.h"
#include "types/math/vec16i_sse.h"
#include "types/base/endianness.h"

__forceinline__ static I32x16 AESEncryptionContext_ghashReduceClMul4(I32x16 clmul00, I32x16 clmulFused, I32x16 clmul11) {

	I32x16 tmp1 = I32x16_lshElements(clmulFused, 2);
	I32x16 tmp3 = I32x16_rshElements(clmulFused, 2);

	I32x16 t0 = I32x16_xor(clmul00, tmp1);
	I32x16 t1 = I32x16_xor(clmul11, tmp3);

	I32x16 tmp0 = I32x16_lsh32(t0, 1);
	I32x16 tmp4 = I32x16_rsh32(t0, 31);
	I32x16 tmp2 = I32x16_lsh32(t1, 1);
	I32x16 tmp6 = I32x16_rsh32(t1, 31);

	I32x16 tmp7 = I32x16_rshElements(tmp4, 3);
	tmp6 = I32x16_lshElements(tmp6, 1);
	I32x16 tmp5 = I32x16_lshElements(tmp4, 1);

	tmp0 = I32x16_or(tmp0, tmp5);
	tmp2 = I32x16_or(tmp2, tmp6);
	tmp5 = I32x16_lsh32(tmp0, 31);
	tmp6 = I32x16_lsh32(tmp0, 30);
	tmp4 = I32x16_or(tmp2, tmp7);
	tmp7 = I32x16_lsh32(tmp0, 25);

	tmp5 = I32x16_xor(tmp5, tmp6);
	tmp5 = I32x16_xor(tmp5, tmp7);

	tmp6 = I32x16_lshElements(tmp5, 3);
	tmp3 = I32x16_rshElements(tmp5, 1);
	tmp5 = I32x16_xor(tmp0, tmp6);

	tmp0 = I32x16_rsh32(tmp5, 1);
	tmp1 = I32x16_rsh32(tmp5, 2);
	tmp2 = I32x16_rsh32(tmp5, 7);

	tmp0 = I32x16_xor(tmp0, tmp5);        //0 ^ 5
	tmp1 = I32x16_xor(tmp1, tmp2);        //1 ^ 2
	tmp3 = I32x16_xor(tmp3, tmp4);        //3 ^ 4
	tmp0 = I32x16_xor(tmp0, tmp1);        //0 ^ 1 ^ 2 ^ 5
	tmp0 = I32x16_xor(tmp0, tmp3);        //0 ^ 1 ^ 2 ^ 3 ^ 4 ^ 5

	return I32x16_swapEndianness(tmp0);
}

void AESEncryptionContext_ghashN4(I32x4 *restrict a, const I32x4 *restrict H, U8 N, I32x4 *restrict clmuls) {

	I32x16 clmul00_16[4];
	I32x16 clmul11_16[4];
	I32x16 clmulFused_16[4];

	I32x16 a16[4];

	const U8 N4 = N >> 2;

	for (U32 i = 0; i < N4; ++i)
		a16[i] = I32x16_swapEndianness(I32x16_load(&a[i << 2]));

	//Looks a bit odd, but it's to allow multiple clmuls to run in parallel.
	//Then, it'll be xored later. If we do clmulNN[i] ^= it creates a dependency, stalling everything.

	for (U32 i = 0; i < N4; ++i) {

		I32x16 Hi = I32x16_wzyxI32x4(I32x16_load(&H[N - 4 - (i << 2)]));

		I32x16 clmul01 = I32x16_clmul64(a16[i], Hi, 0x01);
		I32x16 clmul10 = I32x16_clmul64(a16[i], Hi, 0x10);
		clmul00_16[i] = I32x16_clmul64(a16[i], Hi, 0x00);
		clmul11_16[i] = I32x16_clmul64(a16[i], Hi, 0x11);
		clmulFused_16[i] = I32x16_xor(clmul01, clmul10);
	}

	if (N4 > 1) {

		for (U32 i = 0; i < (U32)(N4 >> 1); ++i) {
			U32 left = i << 1;
			clmul00_16[left] = I32x16_xor(clmul00_16[left], clmul00_16[left | 1]);
			clmul11_16[left] = I32x16_xor(clmul11_16[left], clmul11_16[left | 1]);
			clmulFused_16[left] = I32x16_xor(clmulFused_16[left], clmulFused_16[left | 1]);
		}

		if (N4 > 2) {

			for (U32 i = 0; i < (U32)(N4 >> 2); ++i) {
				U32 left = i << 2;
				clmul00_16[left] = I32x16_xor(clmul00_16[left], clmul00_16[left | 2]);
				clmul11_16[left] = I32x16_xor(clmul11_16[left], clmul11_16[left | 2]);
				clmulFused_16[left] = I32x16_xor(clmulFused_16[left], clmulFused_16[left | 2]);
			}

			if (N4 > 4) {

				for (U32 i = 0; i < (U32)(N4 >> 3); ++i) {
					U32 left = i << 3;
					clmul00_16[left] = I32x16_xor(clmul00_16[left], clmul00_16[left | 4]);
					clmul11_16[left] = I32x16_xor(clmul11_16[left], clmul11_16[left | 4]);
					clmulFused_16[left] = I32x16_xor(clmulFused_16[left], clmulFused_16[left | 4]);
				}

				if (N4 > 8) {
					clmul00_16[0] = I32x16_xor(clmul00_16[0], clmul00_16[8]);
					clmul11_16[0] = I32x16_xor(clmul11_16[0], clmul11_16[8]);
					clmulFused_16[0] = I32x16_xor(clmulFused_16[0], clmulFused_16[8]);
				}
			}
		}
	}

	I32x8 clmul00_8 = I32x8_xor(I32x16_getI32x8(clmul00_16[0], 0), I32x16_getI32x8(clmul00_16[0], 1));
	I32x8 clmulFused = I32x8_xor(I32x16_getI32x8(clmulFused_16[0], 0), I32x16_getI32x8(clmulFused_16[0], 1));
	I32x8 clmul11_8 = I32x8_xor(I32x16_getI32x8(clmul11_16[0], 0), I32x16_getI32x8(clmul11_16[0], 1));

	clmuls[0] = I32x4_xor(I32x8_getI32x4(clmul00_8, 0), I32x8_getI32x4(clmul00_8, 1));
	clmuls[1] = I32x4_xor(I32x8_getI32x4(clmulFused, 0), I32x8_getI32x4(clmulFused, 1));
	clmuls[2] = I32x4_xor(I32x8_getI32x4(clmul11_8, 0), I32x8_getI32x4(clmul11_8, 1));
}

void AESEncryptionContext_ghashTable4(I32x4 *restrict H, I32x4 H2, I32x4 H3, I32x4 H4) {

	I32x16 a = I32x16_create4_4_4_4(H2, H3, H3, H4);
	I32x16 b = I32x16_create4_4_4_4(H3, H3, H4, H4);
	I32x16 clmul01 = I32x16_clmul64(a, b, 0x01);
	I32x16 clmul10 = I32x16_clmul64(a, b, 0x10);
	I32x16 clmul00 = I32x16_clmul64(a, b, 0x00);
	I32x16 clmul11 = I32x16_clmul64(a, b, 0x11);

	I32x16 clmulFused = I32x16_xor(clmul01, clmul10);

	a = AESEncryptionContext_ghashReduceClMul4(clmul00, clmulFused, clmul11);
	a = I32x16_swapEndiannessI32x4(a);
	I32x16_store(&H[0], a);
}

//Almost a copy paste from the I32x4 blockHash4 version, but I32x16 and duplicating k

typedef struct I32x16x4 { I32x16 a, b, c, d; } I32x16x4;

__forceinline__ static I32x16x4 AESEncryptionContext_blockHash16(
	I32x16 a, I32x16 b, I32x16 c, I32x16 d, const I32x4 *restrict k/*[15]*/, const EBufferEncryptionType type
) {

	I32x16 k0 = I32x16_xxxx4(k[0]);
	a = I32x16_xor(a, k0);
	b = I32x16_xor(b, k0);
	c = I32x16_xor(c, k0);
	d = I32x16_xor(d, k0);

	I32x16 k1 = I32x16_xxxx4(k[1]);
	a = I32x16_aesEnc(a, k1);
	b = I32x16_aesEnc(b, k1);
	c = I32x16_aesEnc(c, k1);
	d = I32x16_aesEnc(d, k1);

	I32x16 k2 = I32x16_xxxx4(k[2]);
	a = I32x16_aesEnc(a, k2);
	b = I32x16_aesEnc(b, k2);
	c = I32x16_aesEnc(c, k2);
	d = I32x16_aesEnc(d, k2);

	I32x16 k3 = I32x16_xxxx4(k[3]);
	a = I32x16_aesEnc(a, k3);
	b = I32x16_aesEnc(b, k3);
	c = I32x16_aesEnc(c, k3);
	d = I32x16_aesEnc(d, k3);

	I32x16 k4 = I32x16_xxxx4(k[4]);
	a = I32x16_aesEnc(a, k4);
	b = I32x16_aesEnc(b, k4);
	c = I32x16_aesEnc(c, k4);
	d = I32x16_aesEnc(d, k4);

	I32x16 k5 = I32x16_xxxx4(k[5]);
	a = I32x16_aesEnc(a, k5);
	b = I32x16_aesEnc(b, k5);
	c = I32x16_aesEnc(c, k5);
	d = I32x16_aesEnc(d, k5);

	I32x16 k6 = I32x16_xxxx4(k[6]);
	a = I32x16_aesEnc(a, k6);
	b = I32x16_aesEnc(b, k6);
	c = I32x16_aesEnc(c, k6);
	d = I32x16_aesEnc(d, k6);

	I32x16 k7 = I32x16_xxxx4(k[7]);
	a = I32x16_aesEnc(a, k7);
	b = I32x16_aesEnc(b, k7);
	c = I32x16_aesEnc(c, k7);
	d = I32x16_aesEnc(d, k7);

	I32x16 k8 = I32x16_xxxx4(k[8]);
	a = I32x16_aesEnc(a, k8);
	b = I32x16_aesEnc(b, k8);
	c = I32x16_aesEnc(c, k8);
	d = I32x16_aesEnc(d, k8);

	I32x16 k9 = I32x16_xxxx4(k[9]);
	a = I32x16_aesEnc(a, k9);
	b = I32x16_aesEnc(b, k9);
	c = I32x16_aesEnc(c, k9);
	d = I32x16_aesEnc(d, k9);

	I32x16 k10 = I32x16_xxxx4(k[10]);

	if (type == EBufferEncryptionType_AES256GCM) {

		a = I32x16_aesEnc(a, k10);
		b = I32x16_aesEnc(b, k10);
		c = I32x16_aesEnc(c, k10);
		d = I32x16_aesEnc(d, k10);

		I32x16 k11 = I32x16_xxxx4(k[11]);
		a = I32x16_aesEnc(a, k11);
		b = I32x16_aesEnc(b, k11);
		c = I32x16_aesEnc(c, k11);
		d = I32x16_aesEnc(d, k11);

		I32x16 k12 = I32x16_xxxx4(k[12]);
		a = I32x16_aesEnc(a, k12);
		b = I32x16_aesEnc(b, k12);
		c = I32x16_aesEnc(c, k12);
		d = I32x16_aesEnc(d, k12);

		I32x16 k13 = I32x16_xxxx4(k[13]);
		a = I32x16_aesEnc(a, k13);
		b = I32x16_aesEnc(b, k13);
		c = I32x16_aesEnc(c, k13);
		d = I32x16_aesEnc(d, k13);

		I32x16 k14 = I32x16_xxxx4(k[14]);
		I32x16x4 res = {
			I32x16_aesEncLast(a, k14), I32x16_aesEncLast(b, k14),
			I32x16_aesEncLast(c, k14), I32x16_aesEncLast(d, k14)
		};

		return res;
	}

	I32x16x4 res = {
		I32x16_aesEncLast(a, k10), I32x16_aesEncLast(b, k10),
		I32x16_aesEncLast(c, k10), I32x16_aesEncLast(d, k10)
	};

	return res;
}

typedef struct I32x16x4AndTag {
	I32x16 a, b, c, d;
	I32x4 tag;
} I32x16x4AndTag;

__forceinline__ static I32x16x4AndTag AESEncryptionContext_blockHashAndGhash16(
	I32x16 a0,
	I32x16 a1,
	I32x16 a2,
	I32x16 a3,
	I32x16 H,
	I32x16 H2,
	I32x16 H3,
	I32x16 H4,
	I32x16 b0,
	I32x16 b1,
	I32x16 b2,
	I32x16 b3,
	I32x4 tag,
	const I32x4 *restrict k,
	const EBufferEncryptionType type
) {
	//Manual interleaving because you can't trust MSVC to generate proper code unfortunately.

	b0 = I32x16_xor(b0, I32x16_create4_4_4_4(tag, I32x4_zero(), I32x4_zero(), I32x4_zero()));

	I32x16 k0 = I32x16_xxxx4(k[0]);

	a0 = I32x16_xor(a0, k0);
	a1 = I32x16_xor(a1, k0);
	a2 = I32x16_xor(a2, k0);
	a3 = I32x16_xor(a3, k0);

	I32x16 k1 = I32x16_xxxx4(k[1]);

	b0 = I32x16_swapEndianness(b0);
	b1 = I32x16_swapEndianness(b1);
	b2 = I32x16_swapEndianness(b2);
	b3 = I32x16_swapEndianness(b3);

	a0 = I32x16_aesEnc(a0, k1);
	a1 = I32x16_aesEnc(a1, k1);

	I32x16 k2 = I32x16_xxxx4(k[2]);

	I32x16 clmul01_0 = I32x16_clmul64(b0, H4, 0x01);
	I32x16 clmul10_0 = I32x16_clmul64(b0, H4, 0x10);
	clmul01_0 = I32x16_xor(clmul01_0, clmul10_0);

	a2 = I32x16_aesEnc(a2, k1);
	a3 = I32x16_aesEnc(a3, k1);

	I32x16 clmul01_1 = I32x16_clmul64(b1, H3, 0x01);
	I32x16 clmul10_1 = I32x16_clmul64(b1, H3, 0x10);
	clmul01_1 = I32x16_xor(clmul01_1, clmul10_1);

	a0 = I32x16_aesEnc(a0, k2);
	a1 = I32x16_aesEnc(a1, k2);

	I32x16 k3 = I32x16_xxxx4(k[3]);

	I32x16 clmul01_2 = I32x16_clmul64(b2, H2, 0x01);
	I32x16 clmul10_2 = I32x16_clmul64(b2, H2, 0x10);
	clmul01_2 = I32x16_xor(clmul01_2, clmul10_2);

	a2 = I32x16_aesEnc(a2, k2);
	a3 = I32x16_aesEnc(a3, k2);

	I32x16 clmul01_3 = I32x16_clmul64(b3, H, 0x01);
	I32x16 clmul10_3 = I32x16_clmul64(b3, H, 0x10);
	clmul01_3 = I32x16_xor(clmul01_3, clmul10_3);

	a0 = I32x16_aesEnc(a0, k3);
	a1 = I32x16_aesEnc(a1, k3);

	I32x16 k4 = I32x16_xxxx4(k[4]);

	I32x16 clmul00_0 = I32x16_clmul64(b0, H4, 0x00);
	I32x16 clmul00_1 = I32x16_clmul64(b1, H3, 0x00);

	a2 = I32x16_aesEnc(a2, k3);
	a3 = I32x16_aesEnc(a3, k3);

	I32x16 clmul00_2 = I32x16_clmul64(b2, H2, 0x00);
	I32x16 clmul00_3 = I32x16_clmul64(b3, H, 0x00);

	a0 = I32x16_aesEnc(a0, k4);
	a1 = I32x16_aesEnc(a1, k4);

	I32x16 k5 = I32x16_xxxx4(k[5]);

	I32x16 clmul11_0 = I32x16_clmul64(b0, H4, 0x11);
	I32x16 clmul11_1 = I32x16_clmul64(b1, H3, 0x11);

	a2 = I32x16_aesEnc(a2, k4);
	a3 = I32x16_aesEnc(a3, k4);

	I32x16 clmul11_2 = I32x16_clmul64(b2, H2, 0x11);
	I32x16 clmul11_3 = I32x16_clmul64(b3, H, 0x11);

	a0 = I32x16_aesEnc(a0, k5);
	a1 = I32x16_aesEnc(a1, k5);
	I32x16 k6 = I32x16_xxxx4(k[6]);
	a2 = I32x16_aesEnc(a2, k5);
	a3 = I32x16_aesEnc(a3, k5);

	a0 = I32x16_aesEnc(a0, k6);
	a1 = I32x16_aesEnc(a1, k6);
	I32x16 k7 = I32x16_xxxx4(k[7]);
	a2 = I32x16_aesEnc(a2, k6);
	a3 = I32x16_aesEnc(a3, k6);

	a0 = I32x16_aesEnc(a0, k7);
	a1 = I32x16_aesEnc(a1, k7);
	I32x16 k8 = I32x16_xxxx4(k[8]);
	a2 = I32x16_aesEnc(a2, k7);
	a3 = I32x16_aesEnc(a3, k7);

	a0 = I32x16_aesEnc(a0, k8);
	a1 = I32x16_aesEnc(a1, k8);
	I32x16 k9 = I32x16_xxxx4(k[9]);
	a2 = I32x16_aesEnc(a2, k8);
	a3 = I32x16_aesEnc(a3, k8);

	a0 = I32x16_aesEnc(a0, k9);
	a1 = I32x16_aesEnc(a1, k9);
	I32x16 k10 = I32x16_xxxx4(k[10]);
	a2 = I32x16_aesEnc(a2, k9);
	a3 = I32x16_aesEnc(a3, k9);

	I32x4 b;

	if (type == EBufferEncryptionType_AES256GCM) {

		a0 = I32x16_aesEnc(a0, k10);
		a1 = I32x16_aesEnc(a1, k10);
		I32x16 k11 = I32x16_xxxx4(k[11]);
		a2 = I32x16_aesEnc(a2, k10);
		a3 = I32x16_aesEnc(a3, k10);

		clmul01_0 = I32x16_xor(clmul01_0, clmul01_1);
		clmul00_0 = I32x16_xor(clmul00_0, clmul00_1);
		clmul01_2 = I32x16_xor(clmul01_2, clmul01_3);
		clmul00_2 = I32x16_xor(clmul00_2, clmul00_3);

		a0 = I32x16_aesEnc(a0, k11);
		a1 = I32x16_aesEnc(a1, k11);
		I32x16 k12 = I32x16_xxxx4(k[12]);
		a2 = I32x16_aesEnc(a2, k11);
		a3 = I32x16_aesEnc(a3, k11);

		clmul01_0 = I32x16_xor(clmul01_0, clmul01_2);
		clmul00_0 = I32x16_xor(clmul00_0, clmul00_2);
		I32x8 clmul01_8 = I32x8_xor(I32x16_getI32x8(clmul01_0, 0), I32x16_getI32x8(clmul01_0, 1));
		clmul11_0 = I32x16_xor(clmul11_0, clmul11_1);
		clmul11_2 = I32x16_xor(clmul11_2, clmul11_3);
		I32x4 clmul01 = I32x4_xor(I32x8_getI32x4(clmul01_8, 0), I32x8_getI32x4(clmul01_8, 1));

		I32x4 tmp1 = I32x4_lshElements(clmul01, 2);
		a0 = I32x16_aesEnc(a0, k12);
		a1 = I32x16_aesEnc(a1, k12);

		I32x8 clmul00_8 = I32x8_xor(I32x16_getI32x8(clmul00_0, 0), I32x16_getI32x8(clmul00_0, 1));

		clmul11_0 = I32x16_xor(clmul11_0, clmul11_2);
		I32x4 tmp3 = I32x4_rshElements(clmul01, 2);
		I32x4 clmul00 = I32x4_xor(I32x8_getI32x4(clmul00_8, 0), I32x8_getI32x4(clmul00_8, 1));
		I32x4 t0 = I32x4_xor(clmul00, tmp1);
		a2 = I32x16_aesEnc(a2, k12);
		a3 = I32x16_aesEnc(a3, k12);

		I32x8 clmul11_8 = I32x8_xor(I32x16_getI32x8(clmul11_0, 0), I32x16_getI32x8(clmul11_0, 1));

		I32x16 k13 = I32x16_xxxx4(k[13]);

		I32x4 clmul11 = I32x4_xor(I32x8_getI32x4(clmul11_8, 0), I32x8_getI32x4(clmul11_8, 1));
		I32x4 t1 = I32x4_xor(clmul11, tmp3);
		I32x4 tmp0 = I32x4_lsh32(t0, 1);
		I32x4 tmp4 = I32x4_rsh32(t0, 31);
		I32x4 tmp2 = I32x4_lsh32(t1, 1);
		a0 = I32x16_aesEnc(a0, k13);
		a1 = I32x16_aesEnc(a1, k13);

		I32x16 k14 = I32x16_xxxx4(k[14]);

		I32x4 tmp6 = I32x4_rsh32(t1, 31);
		I32x4 tmp7 = I32x4_rshElements(tmp4, 3);
		tmp6 = I32x4_lshElements(tmp6, 1);
		I32x4 tmp5 = I32x4_lshElements(tmp4, 1);
		a2 = I32x16_aesEnc(a2, k13);
		a3 = I32x16_aesEnc(a3, k13);

		tmp0 = I32x4_or(tmp0, tmp5);
		tmp2 = I32x4_or(tmp2, tmp6);
		tmp5 = I32x4_lsh32(tmp0, 31);
		tmp6 = I32x4_lsh32(tmp0, 30);
		tmp4 = I32x4_or(tmp2, tmp7);
		tmp7 = I32x4_lsh32(tmp0, 25);

		a0 = I32x16_aesEncLast(a0, k14);
		a1 = I32x16_aesEncLast(a1, k14);

		tmp5 = I32x4_xor(tmp5, tmp6);
		tmp5 = I32x4_xor(tmp5, tmp7);

		a2 = I32x16_aesEncLast(a2, k14);
		a3 = I32x16_aesEncLast(a3, k14);

		tmp6 = I32x4_lshElements(tmp5, 3);
		tmp3 = I32x4_rshElements(tmp5, 1);
		tmp5 = I32x4_xor(tmp0, tmp6);

		tmp0 = I32x4_rsh32(tmp5, 1);
		tmp1 = I32x4_rsh32(tmp5, 2);
		tmp2 = I32x4_rsh32(tmp5, 7);

		tmp0 = I32x4_xor(tmp0, tmp5);        //0 ^ 5
		tmp1 = I32x4_xor(tmp1, tmp2);        //1 ^ 2
		tmp3 = I32x4_xor(tmp3, tmp4);        //3 ^ 4
		tmp0 = I32x4_xor(tmp0, tmp1);        //0 ^ 1 ^ 2 ^ 5
		tmp0 = I32x4_xor(tmp0, tmp3);        //0 ^ 1 ^ 2 ^ 3 ^ 4 ^ 5

		b = I32x4_swapEndianness(tmp0);

	} else {

		a0 = I32x16_aesEncLast(a0, k10);
		a1 = I32x16_aesEncLast(a1, k10);
		a2 = I32x16_aesEncLast(a2, k10);
		a3 = I32x16_aesEncLast(a3, k10);

		clmul01_0 = I32x16_xor(clmul01_0, clmul01_1);
		clmul00_0 = I32x16_xor(clmul00_0, clmul00_1);
		clmul11_0 = I32x16_xor(clmul11_0, clmul11_1);

		clmul01_2 = I32x16_xor(clmul01_2, clmul01_3);
		clmul00_2 = I32x16_xor(clmul00_2, clmul00_3);
		clmul11_2 = I32x16_xor(clmul11_2, clmul11_3);

		clmul01_0 = I32x16_xor(clmul01_0, clmul01_2);
		clmul00_0 = I32x16_xor(clmul00_0, clmul00_2);
		clmul11_0 = I32x16_xor(clmul11_0, clmul11_2);

		I32x8 clmul01_8 = I32x8_xor(I32x16_getI32x8(clmul01_0, 0), I32x16_getI32x8(clmul01_0, 1));
		I32x8 clmul00_8 = I32x8_xor(I32x16_getI32x8(clmul00_0, 0), I32x16_getI32x8(clmul00_0, 1));
		I32x8 clmul11_8 = I32x8_xor(I32x16_getI32x8(clmul11_0, 0), I32x16_getI32x8(clmul11_0, 1));

		I32x4 clmul01 = I32x4_xor(I32x8_getI32x4(clmul01_8, 0), I32x8_getI32x4(clmul01_8, 1));
		I32x4 clmul00 = I32x4_xor(I32x8_getI32x4(clmul00_8, 0), I32x8_getI32x4(clmul00_8, 1));
		I32x4 clmul11 = I32x4_xor(I32x8_getI32x4(clmul11_8, 0), I32x8_getI32x4(clmul11_8, 1));
		
		b = AESEncryptionContext_ghashReduceClMul(clmul00, clmul01, clmul11);
	}

	I32x16x4AndTag abcde = { a0, a1, a2, a3, b };
	return abcde;
}

void AESEncryptionContext_blocks16(
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

	I32x16 prevState[4];
	I32x16 H12 = I32x16_wzyxI32x4(I32x16_load(H));
	I32x16 H34 = I32x16_wzyxI32x4(I32x16_load(H + 4));
	I32x16 H56 = I32x16_wzyxI32x4(I32x16_load(H + 8));
	I32x16 H78 = I32x16_wzyxI32x4(I32x16_load(H + 12));

	//Don't waste time on a ghash that's not gonna be used
	//I manually unrolled this and optimized the instruction order, because MSVC really can't compile well at alll.

	{
		U32 counter = *counterForIv;

		I32x4 ivi0 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter));
		I32x4 ivi1 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 1));
		I32x4 ivi2 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 2));
		I32x4 ivi3 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 3));
		I32x16 ivi0123 = I32x16_create4_4_4_4(ivi0, ivi1, ivi2, ivi3);

		I32x4 ivi4 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 4));
		I32x4 ivi5 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 5));
		I32x4 ivi6 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 6));
		I32x4 ivi7 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 7));
		I32x16 ivi4567 = I32x16_create4_4_4_4(ivi4, ivi5, ivi6, ivi7);

		I32x4 ivi8 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 8));
		I32x4 ivi9 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 9));
		I32x4 iviA = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 10));
		I32x4 iviB = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 11));
		I32x16 ivi89AB = I32x16_create4_4_4_4(ivi8, ivi9, iviA, iviB);

		I32x4 iviC = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 12));
		I32x4 iviD = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 13));
		I32x4 iviE = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 14));
		I32x4 iviF = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 15));
		I32x16 iviCDEF = I32x16_create4_4_4_4(iviC, iviD, iviE, iviF);

		*counterForIv += 16;

		I32x16x4 ab = AESEncryptionContext_blockHash16(ivi0123, ivi4567, ivi89AB, iviCDEF, k, encryptionType);

		I32x4 *restrict nextAddr = *next;
		I32x16 data = I32x16_load(nextAddr + 0);
		I32x16 datb = I32x16_load(nextAddr + 4);
		I32x16 datc = I32x16_load(nextAddr + 8);
		I32x16 datd = I32x16_load(nextAddr + 12);

		I32x16 a = I32x16_xor(ab.a, data);
		I32x16 b = I32x16_xor(ab.b, datb);
		I32x16 c = I32x16_xor(ab.c, datc);
		I32x16 d = I32x16_xor(ab.d, datd);

		if (!isEncrypt) {            //Decryption, we use the input (ciphertext)
			prevState[0] = data;
			prevState[1] = datb;
			prevState[2] = datc;
			prevState[3] = datd;
		}

		else {                        //Encryption, we use the output (ciphertext)
			prevState[0] = a;
			prevState[1] = b;
			prevState[2] = c;
			prevState[3] = d;
		}

		I32x16_store(nextAddr + 0, a);
		I32x16_store(nextAddr + 4, b);
		I32x16_store(nextAddr + 8, c);
		I32x16_store(nextAddr + 12, d);
		*next += 16;
	}

	I32x4 tag = *tagPtr;

	//Contents

	while (*next + 8 <= end) {

		U32 counter = *counterForIv;

		I32x4 ivi0 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter));
		I32x4 ivi1 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 1));
		I32x4 ivi2 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 2));
		I32x4 ivi3 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 3));
		I32x16 ivi0123 = I32x16_create4_4_4_4(ivi0, ivi1, ivi2, ivi3);

		I32x4 ivi4 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 4));
		I32x4 ivi5 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 5));
		I32x4 ivi6 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 6));
		I32x4 ivi7 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 7));
		I32x16 ivi4567 = I32x16_create4_4_4_4(ivi4, ivi5, ivi6, ivi7);

		I32x4 ivi8 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 8));
		I32x4 ivi9 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 9));
		I32x4 iviA = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 10));
		I32x4 iviB = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 11));
		I32x16 ivi89AB = I32x16_create4_4_4_4(ivi8, ivi9, iviA, iviB);

		I32x4 iviC = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 12));
		I32x4 iviD = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 13));
		I32x4 iviE = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 14));
		I32x4 iviF = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counter + 15));
		I32x16 iviCDEF = I32x16_create4_4_4_4(iviC, iviD, iviE, iviF);

		*counterForIv += 16;

		I32x16x4AndTag ab = AESEncryptionContext_blockHashAndGhash16(
			ivi0123, ivi4567, ivi89AB, iviCDEF,
			H12, H34, H56, H78,
			prevState[0], prevState[1], prevState[2], prevState[3],
			tag,
			k,
			encryptionType
		);

		tag = ab.tag;

		I32x4 *restrict nextAddr = *next;
		I32x16 data = I32x16_load(nextAddr + 0);
		I32x16 datb = I32x16_load(nextAddr + 4);
		I32x16 datc = I32x16_load(nextAddr + 8);
		I32x16 datd = I32x16_load(nextAddr + 12);

		I32x16 a = I32x16_xor(ab.a, data);
		I32x16 b = I32x16_xor(ab.b, datb);
		I32x16 c = I32x16_xor(ab.c, datc);
		I32x16 d = I32x16_xor(ab.d, datd);

		if (!isEncrypt) {                //Decryption, we use the input (ciphertext)
			prevState[0] = data;
			prevState[1] = datb;
			prevState[2] = datc;
			prevState[3] = datd;
		}

		else {                            //Encryption, we use the output (ciphertext)
			prevState[0] = a;
			prevState[1] = b;
			prevState[2] = c;
			prevState[3] = d;
		}

		I32x16_store(nextAddr + 0, a);
		I32x16_store(nextAddr + 4, b);
		I32x16_store(nextAddr + 8, c);
		I32x16_store(nextAddr + 12, d);
		*next += 16;
	}

	//Epilogue

	{
		I32x16 a = I32x16_xor(prevState[0], I32x16_create4_4_4_4(tag, I32x4_zero(), I32x4_zero(), I32x4_zero()));
		I32x16 b = prevState[1];
		I32x16 c = prevState[2];
		I32x16 d = prevState[3];

		a = I32x16_swapEndianness(a);
		b = I32x16_swapEndianness(b);
		c = I32x16_swapEndianness(c);
		d = I32x16_swapEndianness(d);

		I32x16 clmul01a = I32x16_clmul64(a, H78, 0x01);
		I32x16 clmul10a = I32x16_clmul64(a, H78, 0x10);
		clmul01a = I32x16_xor(clmul01a, clmul10a);

		I32x16 clmul01b = I32x16_clmul64(b, H56, 0x01);
		I32x16 clmul10b = I32x16_clmul64(b, H56, 0x10);
		clmul01b = I32x16_xor(clmul01b, clmul10b);

		I32x16 clmul01c = I32x16_clmul64(c, H34, 0x01);
		I32x16 clmul10c = I32x16_clmul64(c, H34, 0x10);
		clmul01c = I32x16_xor(clmul01c, clmul10c);

		I32x16 clmul01d = I32x16_clmul64(d, H12, 0x01);
		I32x16 clmul10d = I32x16_clmul64(d, H12, 0x10);
		clmul01d = I32x16_xor(clmul01d, clmul10d);

		I32x16 clmul00a = I32x16_clmul64(a, H78, 0x00);
		I32x16 clmul00b = I32x16_clmul64(b, H56, 0x00);
		I32x16 clmul00c = I32x16_clmul64(c, H34, 0x00);
		I32x16 clmul00d = I32x16_clmul64(d, H12, 0x00);

		I32x16 clmul11a = I32x16_clmul64(a, H78, 0x11);
		I32x16 clmul11b = I32x16_clmul64(b, H56, 0x11);
		I32x16 clmul11c = I32x16_clmul64(c, H34, 0x11);
		I32x16 clmul11d = I32x16_clmul64(d, H12, 0x11);

		clmul11a = I32x16_xor(clmul11a, clmul11b);
		clmul00a = I32x16_xor(clmul00a, clmul00b);
		clmul01a = I32x16_xor(clmul01a, clmul01b);

		clmul11c = I32x16_xor(clmul11c, clmul11d);
		clmul00c = I32x16_xor(clmul00c, clmul00d);
		clmul01c = I32x16_xor(clmul01c, clmul01d);

		clmul11a = I32x16_xor(clmul11a, clmul11c);
		clmul00a = I32x16_xor(clmul00a, clmul00c);
		clmul01a = I32x16_xor(clmul01a, clmul01c);

		I32x8 clmul11_8 = I32x8_xor(I32x16_getI32x8(clmul11a, 0), I32x16_getI32x8(clmul11a, 1));
		I32x8 clmul00_8 = I32x8_xor(I32x16_getI32x8(clmul00a, 0), I32x16_getI32x8(clmul00a, 1));
		I32x8 clmul01_8 = I32x8_xor(I32x16_getI32x8(clmul01a, 0), I32x16_getI32x8(clmul01a, 1));

		I32x4 clmul11 = I32x4_xor(I32x8_getI32x4(clmul11_8, 0), I32x8_getI32x4(clmul11_8, 1));
		I32x4 clmul00 = I32x4_xor(I32x8_getI32x4(clmul00_8, 0), I32x8_getI32x4(clmul00_8, 1));
		I32x4 clmul01 = I32x4_xor(I32x8_getI32x4(clmul01_8, 0), I32x8_getI32x4(clmul01_8, 1));

		*tagPtr = AESEncryptionContext_ghashReduceClMul(clmul00, clmul01, clmul11);
	}
}
