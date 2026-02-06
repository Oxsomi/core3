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
#include "types/math/vec16i_sse.h"
#include "types/base/endianness.h"

void AESEncryptionContext_updateTagN(
	AESEncryptionContext *restrict ctx, const I32x4 *restrict CTi, const U8 N, U8 use256Or512
);

static inline I32x16 AESEncryptionContext_ghashReduceClMul4(I32x16 clmul00, I32x16 clmulFused, I32x16 clmul11) {
		
	I32x16 tmp[8];

	tmp[0] = clmul00;
	tmp[3] = clmulFused;
	tmp[2] = clmul11;

	tmp[1] = I32x16_lshElements(tmp[3], 2);
	tmp[3] = I32x16_rshElements(tmp[3], 2);

	for (U8 i = 0; i < 2; ++i) {
		I32x16 t = I32x16_xor(tmp[i << 1], tmp[(i << 1) + 1]);
		tmp[i << 1] = I32x16_lsh32(t, 1);
		tmp[4 + (i << 1)] = I32x16_rsh32(t, 31);
	}

	tmp[7] = I32x16_rshElements(tmp[4], 3);

	for (U8 i = 0; i < 2; ++i)
		tmp[6 - i] = I32x16_lshElements(tmp[6 - (i << 1)], 1);

	const U8 v0[3] = { 31, 30, 25 };

	for (U8 i = 0; i < 3; ++i) {
		tmp[i << 1] = I32x16_or(tmp[i ? 2 : 0], tmp[5 + i]);
		tmp[5 + i] = I32x16_lsh32(tmp[0], v0[i]);
	}

	for (U8 i = 0; i < 2; ++i)
		tmp[5] = I32x16_xor(tmp[5], tmp[6 + i]);

	tmp[3] = I32x16_rshElements(tmp[5], 1);
	tmp[5] = I32x16_xor(tmp[0], I32x16_lshElements(tmp[5], 3));

	const U8 v1[3] = { 1, 2, 7 };

	for (U8 i = 0; i < 3; ++i)
		tmp[i] = I32x16_rsh32(tmp[5], v1[i]);

	for (U8 i = 1; i < 6; ++i)
		tmp[0] = I32x16_xor(tmp[0], tmp[i]);

	return I32x16_swapEndianness(tmp[0]);
}

void AESEncryptionContext_ghashN4(I32x4 *restrict a, const I32x4 *restrict H, U8 N, I32x4 *restrict clmuls) {

	I32x16 clmul00_16[16];
	I32x16 clmul11_16[16];
	I32x16 clmulFused_16[16];

	I32x16 a16[16];

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

static inline I32x16 AESEncryptionContext_blockHash4(
	I32x16 block, const I32x4 *restrict k/*[15]*/, const EBufferEncryptionType type
) {

	block = I32x16_xor(block, I32x16_xxxx4(k[0]));

	const U8 rounds = type == EBufferEncryptionType_AES128GCM ? 10 : 14;

	for(U8 i = 1; i < rounds; ++i)
		block = I32x16_aesEnc(block, I32x16_xxxx4(k[i]));

	return I32x16_aesEncLast(block, I32x16_xxxx4(k[rounds]));
}

	void AESEncryptionContext_processBlockN4(
	AESEncryptionContext *restrict ctx,
	I32x4 *restrict io,
	const U32 id,
	const U8 N,
	Bool isEncrypt
) {

	I32x4 iv = ctx->iv;

	I32x16 v[16];
	I32x16 ivi[16];

	U8 N4 = N >> 2;

	for (U32 i = 0; i < N4; ++i)
		v[i] = I32x16_load(&io[i << 2]);

	if (!isEncrypt)
		AESEncryptionContext_updateTagN(ctx, (const I32x4 *restrict) v, N, 3);

	for (U32 i = 0; i < N; i += 4) {

		I32x4 ivi0 = iv;
		I32x4 ivi1 = iv;
		I32x4 ivi2 = iv;
		I32x4 ivi3 = iv;

		I32x4_setWRef(&ivi0, (I32)U32_swapEndianness(id + i + 2));
		I32x4_setWRef(&ivi1, (I32)U32_swapEndianness(id + i + 3));
		I32x4_setWRef(&ivi2, (I32)U32_swapEndianness(id + i + 4));
		I32x4_setWRef(&ivi3, (I32)U32_swapEndianness(id + i + 5));

		ivi[i >> 2] = I32x16_create4_4_4_4(ivi0, ivi1, ivi2, ivi3);
	}

	for (U32 i = 0; i < N4; ++i)
		v[i] = I32x16_xor(v[i], AESEncryptionContext_blockHash4(ivi[i], ctx->key, ctx->encryptionType));

	if (isEncrypt)
		AESEncryptionContext_updateTagN(ctx, (const I32x4 *restrict) v, N, 3);

	for (U32 i = 0; i < N4; ++i)
		I32x16_store(&io[i << 2], v[i]);
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
