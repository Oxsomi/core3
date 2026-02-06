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
					clmul00_8[0] = I32x8_xor(clmul00_8[0], clmul00_8[8]);
					clmul11_8[0] = I32x8_xor(clmul11_8[0], clmul11_8[8]);
					clmulFused_8[0] = I32x8_xor(clmulFused_8[0], clmulFused_8[8]);
				}
			}
		}
	}

	clmuls[0] = I32x4_xor(I32x8_getI32x4(clmul00_8[0], 0), I32x8_getI32x4(clmul00_8[0], 1));
	clmuls[1] = I32x4_xor(I32x8_getI32x4(clmulFused_8[0], 0), I32x8_getI32x4(clmulFused_8[0], 1));
	clmuls[2] = I32x4_xor(I32x8_getI32x4(clmul11_8[0], 0), I32x8_getI32x4(clmul11_8[0], 1));
}

static inline I32x8 AESEncryptionContext_blockHash2(
	I32x8 block, const I32x4 *restrict k/*[15]*/, const EBufferEncryptionType type
) {

	block = I32x8_xor(block, I32x8_xx4(k[0]));

	const U8 rounds = type == EBufferEncryptionType_AES128GCM ? 10 : 14;

	for(U8 i = 1; i < rounds; ++i)
		block = I32x8_aesEnc(block, I32x8_xx4(k[i]));

	return I32x8_aesEncLast(block, I32x8_xx4(k[rounds]));
}

void AESEncryptionContext_processBlockN2(
	AESEncryptionContext *restrict ctx,
	I32x4 *restrict io,
	const U32 id,
	const U8 N,
	Bool isEncrypt
) {

	I32x4 iv = ctx->iv;

	I32x8 v[32];
	I32x8 ivi[32];

	U8 N2 = N >> 1;

	for (U32 i = 0; i < N2; ++i)
		v[i] = I32x8_load(&io[i << 1]);

	if (!isEncrypt)
		AESEncryptionContext_updateTagN(ctx, (const I32x4 *restrict) v, N, 1);

	for (U32 i = 0; i < N; i += 2) {

		I32x4 ivi0 = iv;
		I32x4 ivi1 = iv;

		I32x4_setWRef(&ivi0, (I32)U32_swapEndianness(id + i + 2));
		I32x4_setWRef(&ivi1, (I32)U32_swapEndianness(id + i + 3));

		ivi[i >> 1] = I32x8_create4_4(ivi0, ivi1);
	}

	for (U32 i = 0; i < N2; ++i)
		v[i] = I32x8_xor(v[i], AESEncryptionContext_blockHash2(ivi[i], ctx->key, ctx->encryptionType));

	if (isEncrypt)
		AESEncryptionContext_updateTagN(ctx, (const I32x4 *restrict) v, N, 1);

	for (U32 i = 0; i < N2; ++i)
		I32x8_store(&io[i << 1], v[i]);
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
