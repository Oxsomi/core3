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

#include "types/base/error.h"
#include "types/container/buffer_encrypt.h"
#include "types/container/buffer.h"
#include "types/math/u128.h"
#include "types/math/vec4i_swizzle.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"
#include "types/math/type_cast.h"
#include "types/base/endianness.h"

static I8 cryptoState = -1;

#if _SIMD == SIMD_SSE
	void SIMD_createCryptoState() {

		if (cryptoState >= 0)
			return;

		U32 cpuInfo[4];

		//Leaf 1 - AES + PCLMUL + OSXSAVE
		Platform_getCPUId(1, cpuInfo);

		Bool hasAES = (cpuInfo[2] & (1 << 25)) != 0;
		Bool hasPCLMUL = (cpuInfo[2] & (1 << 1)) != 0;
		Bool hasOSXSAVE = (cpuInfo[2] & (1 << 27)) != 0;

		if (!hasAES || !hasPCLMUL) {
			cryptoState = 0;
			return;
		}

		if (!hasOSXSAVE || (_xgetbv(0) & 0x6) != 0x6) {
			cryptoState = 1;	// AES-NI only
			return;
		}

		Platform_getCPUId(7, cpuInfo);

		Bool hasAVX2 = (cpuInfo[1] & (1 << 5)) != 0;
		Bool hasAVX512F = (cpuInfo[1] & (1 << 16)) != 0;
		Bool hasAVX512VL = (cpuInfo[1] & (1u << 31)) != 0;
		Bool hasVAES = (cpuInfo[2] & (1 << 9)) != 0;
		Bool hasVPCLMUL = (cpuInfo[2] & (1 << 10)) != 0;

		Bool osHasZMM = (_xgetbv(0) & 0xE0) == 0xE0;

		if (!(hasVAES && hasAVX2 && hasVPCLMUL && hasAVX512VL)) {
			cryptoState = 1;				//AES-NI only
			return;
		}

		if (hasAVX512F && osHasZMM) {		//512-bit path
			cryptoState = 3;
			return;
		}

		cryptoState = 2;					//256-bit VAES path
	}
#else
	void SIMD_createCryptoState() { }		//TODO: Use this for presence of crypto in the first place
#endif

//Explanation of algorithm; AES256 GCM + GMAC
//https://www.alexeyshmalko.com/20200319144641/
//https://www.youtube.com/watch?v=V2TlG3JbGp0
//https://www.youtube.com/watch?v=g_eY7JXOc8U
//
//The final algorithm is basically the following:
//
//- Init key using CSPRNG if not available
//- Init H: aes256(0, key)
//
//- Tag: 0
//- Foreach additional data block padded to 16-byte with 0s:
//	- tag = GHASH(tag XOR additional data block)
//
//- IV (Initial vector) = Generate CSPRNG of 12-bytes (if not provided)
//- Store iv in result
//
//- Foreach plaindata block at i padded to 16-byte with 0s:
//	- Eki = encrypt(IV append U32BE(i + 2))
//	- store (cyphertext[i] = plainText[i] XOR Eki) in result
//	- tag = GHASH(tag XOR cyphertext[i])
//
//- tag = GHASH(combine(U64BE(additionalDataBits), U64BE(plainTextBits)) XOR tag)
//- tag = tag XOR aes256(IV with U32BE(1) appended)
//
//- Store tag in result
//
//For "encrypt" we use AES CTR as explained by the intel paper:
//https://www.intel.com/content/dam/doc/white-paper/advanced-encryption-standard-new-instructions-set-paper.pdf

//AES_subWord can be used by either NEON or NONE for encryption.
//No lookup tables, those are unsafe.

static inline U8 AES_xtime(U8 x) {
	return (U8)((x << 1) ^ ((x >> 7) * 0x1B));
}

static inline U8 AES_gfMul(U8 a, U8 b) {
	U8 r = 0;
	for (int i = 0; i < 8; i++) {
		r ^= (U8)(-(b & 1) & a);
		a = AES_xtime(a);
		b >>= 1;
	}
	return r;
}

static inline U8 AES_gfInv(U8 x) {
	U8 x2   = AES_gfMul(x, x);
	U8 x4   = AES_gfMul(x2, x2);
	U8 x8   = AES_gfMul(x4, x4);	
	U8 x16  = AES_gfMul(x8, x8);
	U8 x32  = AES_gfMul(x16, x16);
	U8 x64  = AES_gfMul(x32, x32);
	U8 x128 = AES_gfMul(x64, x64);	
	U8 x192 = AES_gfMul(x128, x64);
	U8 x224 = AES_gfMul(x192, x32);
	U8 x240 = AES_gfMul(x224, x16);
	U8 x248 = AES_gfMul(x240, x8);
	U8 x252 = AES_gfMul(x248, x4);
	U8 x254 = AES_gfMul(x252, x2);
	return x254;
}

static inline U8 AES_affine(U8 x) {
	U8 y = x;
	y ^= (x << 1) | (x >> 7);
	y ^= (x << 2) | (x >> 6);
	y ^= (x << 3) | (x >> 5);
	y ^= (x << 4) | (x >> 4);
	return y ^ 0x63;
}

static inline U8 AES_sbox(U8 x) {
	return AES_affine(AES_gfInv(x));
}

static inline U32 AES_subWord(U32 w) {
	return
		((U32)AES_sbox((U8)(w >>  0)) <<  0) |
		((U32)AES_sbox((U8)(w >>  8)) <<  8) |
		((U32)AES_sbox((U8)(w >> 16)) << 16) |
		((U32)AES_sbox((U8)(w >> 24)) << 24);
}

#if _SIMD == SIMD_NEON
	#include "types/container/simd/neon/neon_buffer_encrypt.h"
#elif _SIMD == SIMD_SSE
	#include "types/container/simd/sse/sse_buffer_encrypt.h"
	#include "types/math/vec8i_sse.h"
	#include "types/math/vec16i_sse.h"
#else
	#include "types/container/simd/none/none_buffer_encrypt.h"
#endif

//Key expansion for AES256
//Implemented from the official intel AES-NI paper + Additional paper by S. Gueron appendix A
//https://link.springer.com/content/pdf/10.1007/978-3-642-03317-9_4.pdf
//https://www.samiam.org/key-schedule.html
static inline I32x4 AESEncryptionContext_expandKeyN(I32x4 im1, const I32x4 im2) {

	I32x4 im4 = im1;

	for(U8 i = 0; i < 3; ++i) {
		im4 = I32x4_lshElements(im4, 1);
		im1 = I32x4_xor(im1, im4);
	}

	return I32x4_xor(im1, im2);
}

static inline I32x4 AESEncryptionContext_expandKey1(const I32x4 im1, const I32x4 im2) {
	return AESEncryptionContext_expandKeyN(im1, I32x4_wwww(im2));
}

static inline I32x4 AESEncryptionContext_expandKey2(const I32x4 im1, const I32x4 im3) {
	return AESEncryptionContext_expandKeyN(im3, I32x4_zzzz(AES_keyGenAssist(im1, 0)));
}

static inline void AESEncryptionContext_expandKey(const U32 *restrict key, I32x4 *restrict k/*[15]*/, const EBufferEncryptionType encryptionType) {

	k[0] = I32x4_load4(key);

	if(encryptionType == EBufferEncryptionType_AES256GCM)
		k[1] = I32x4_load4(key + 4);

	//Only use AESEncryptionContext_expandKey1 for AES128,

	if(encryptionType == EBufferEncryptionType_AES128GCM) {

		I32x4 im1 = k[0];

		for (U8 i = 0; i < 10; ++i)
			k[i + 1] = (im1 = AESEncryptionContext_expandKey1(im1, AES_keyGenAssist(im1, i + 1)));

		return;
	}

	//AESEncryptionContext_expandKey2 and 1 are also used for AES256

	I32x4 im1 = k[0];
	I32x4 im3 = k[1];

	for (U8 i = 0, j = 2; i < 7; ++i, j += 2) {

		k[j] = (im1 = AESEncryptionContext_expandKey1(im1, AES_keyGenAssist(im3, i + 1)));

		if(j + 1 < 15)
			k[j + 1] = (im3 = AESEncryptionContext_expandKey2(im1, im3));
	}
}

//AES block encryption. Don't use this plainly, it's a part of the larger AES256-CTR algorithm
static inline I32x4 AESEncryptionContext_blockHash(I32x4 block, const I32x4 *restrict k/*[15]*/, const EBufferEncryptionType type) {

	block = I32x4_xor(block, k[0]);

	const U8 rounds = type == EBufferEncryptionType_AES128GCM ? 10 : 14;

	for(U8 i = 1; i < rounds; ++i)
		block = AES_encodeBlock(block, k[i]);

	return AES_encodeBlockLast(block, k[rounds]);
}

#ifdef HAS_AESx2
	static inline I32x8 AESEncryptionContext_blockHash2(
		I32x8 block, const I32x4 *restrict k/*[15]*/, const EBufferEncryptionType type
	) {

		block = I32x8_xor(block, I32x8_xx4(k[0]));

		const U8 rounds = type == EBufferEncryptionType_AES128GCM ? 10 : 14;

		for(U8 i = 1; i < rounds; ++i)
			block = I32x8_aesEnc(block, I32x8_xx4(k[i]));

		return I32x8_aesEncLast(block, I32x8_xx4(k[rounds]));
	}
#endif

#ifdef HAS_AESx4
	static inline I32x16 AESEncryptionContext_blockHash4(
		I32x16 block, const I32x4 *restrict k/*[15]*/, const EBufferEncryptionType type
	) {

		block = I32x16_xor(block, I32x16_xxxx4(k[0]));

		const U8 rounds = type == EBufferEncryptionType_AES128GCM ? 10 : 14;

		for(U8 i = 1; i < rounds; ++i)
			block = I32x16_aesEnc(block, I32x16_xxxx4(k[i]));

		return I32x16_aesEncLast(block, I32x16_xxxx4(k[rounds]));
	}
#endif

//Refactored from https://www.intel.com/content/dam/develop/external/us/en/documents/clmul-wp-rev-2-02-2014-04-20.pdf

static inline I32x4 AESEncryptionContext_ghashN(I32x4 *restrict a, const I32x4 *restrict H, U8 N, U8 use256Or512) {

	I32x4 clmul00_0;
	I32x4 clmul01_0;
	I32x4 clmul10_0;
	I32x4 clmul11_0;

	#ifdef HAS_CLMUL64x4
	
		//cryptoState >= 3: has cmul64x2 + clmul64x4
		if(cryptoState >= 3 && N >= 4 && (use256Or512 & 2)) {

			I32x16 clmul00_16[32];
			I32x16 clmul11_16[32];
			I32x16 clmul01_16[32];
			I32x16 clmul10_16[32];
	
			I32x16 a16[32];

			const U8 N4 = N >> 2;

			for (U32 i = 0; i < N4; ++i)
				a16[i] = I32x16_swapEndianness(I32x16_load(&a[i << 2]));

			//Looks a bit odd, but it's to allow multiple clmuls to run in parallel.
			//Then, it'll be xored later. If we do clmulNN[i] ^= it creates a dependency, stalling everything.

			for (U32 i = 0; i < N4; ++i) {

				I32x16 Hi = I32x16_wzyxI32x4(I32x16_load(&H[N - 4 - (i << 2)]));

				clmul00_16[i] = I32x16_clmul64(a16[i], Hi, 0x00);
				clmul01_16[i] = I32x16_clmul64(a16[i], Hi, 0x01);
				clmul10_16[i] = I32x16_clmul64(a16[i], Hi, 0x10);
				clmul11_16[i] = I32x16_clmul64(a16[i], Hi, 0x11);
			}

			if (N4 > 1) {

				for (U32 i = 0; i < (U32)(N4 >> 1); ++i) {
					U32 left = i << 1;
					clmul00_16[left] = I32x16_xor(clmul00_16[left], clmul00_16[left | 1]);
					clmul01_16[left] = I32x16_xor(clmul01_16[left], clmul01_16[left | 1]);
					clmul10_16[left] = I32x16_xor(clmul10_16[left], clmul10_16[left | 1]);
					clmul11_16[left] = I32x16_xor(clmul11_16[left], clmul11_16[left | 1]);
				}

				if (N4 > 2) {

					for (U32 i = 0; i < (U32)(N4 >> 2); ++i) {
						U32 left = i << 2;
						clmul00_16[left] = I32x16_xor(clmul00_16[left], clmul00_16[left | 2]);
						clmul01_16[left] = I32x16_xor(clmul01_16[left], clmul01_16[left | 2]);
						clmul10_16[left] = I32x16_xor(clmul10_16[left], clmul10_16[left | 2]);
						clmul11_16[left] = I32x16_xor(clmul11_16[left], clmul11_16[left | 2]);
					}

					if (N4 > 4) {

						for (U32 i = 0; i < (U32)(N4 >> 3); ++i) {
							U32 left = i << 3;
							clmul00_16[left] = I32x16_xor(clmul00_16[left], clmul00_16[left | 4]);
							clmul01_16[left] = I32x16_xor(clmul01_16[left], clmul01_16[left | 4]);
							clmul10_16[left] = I32x16_xor(clmul10_16[left], clmul10_16[left | 4]);
							clmul11_16[left] = I32x16_xor(clmul11_16[left], clmul11_16[left | 4]);
						}

						if (N4 > 8) {
							clmul00_16[0] = I32x16_xor(clmul00_16[0], clmul00_16[8]);
							clmul01_16[0] = I32x16_xor(clmul01_16[0], clmul01_16[8]);
							clmul10_16[0] = I32x16_xor(clmul10_16[0], clmul10_16[8]);
							clmul11_16[0] = I32x16_xor(clmul11_16[0], clmul11_16[8]);
						}
					}
				}
			}

			I32x8 clmul00_8 = I32x8_xor(I32x16_getI32x8(clmul00_16[0], 0), I32x16_getI32x8(clmul00_16[0], 1));
			I32x8 clmul01_8 = I32x8_xor(I32x16_getI32x8(clmul01_16[0], 0), I32x16_getI32x8(clmul01_16[0], 1));
			I32x8 clmul10_8 = I32x8_xor(I32x16_getI32x8(clmul10_16[0], 0), I32x16_getI32x8(clmul10_16[0], 1));
			I32x8 clmul11_8 = I32x8_xor(I32x16_getI32x8(clmul11_16[0], 0), I32x16_getI32x8(clmul11_16[0], 1));

			clmul00_0 = I32x4_xor(I32x8_getI32x4(clmul00_8, 0), I32x8_getI32x4(clmul00_8, 1));
			clmul01_0 = I32x4_xor(I32x8_getI32x4(clmul01_8, 0), I32x8_getI32x4(clmul01_8, 1));
			clmul10_0 = I32x4_xor(I32x8_getI32x4(clmul10_8, 0), I32x8_getI32x4(clmul10_8, 1));
			clmul11_0 = I32x4_xor(I32x8_getI32x4(clmul11_8, 0), I32x8_getI32x4(clmul11_8, 1));
		} else

	#endif

	#ifdef HAS_CLMUL64x2
	
		//cryptoState >= 2: has AVX2, AVX512VL, VAES, VPCLMULQDQ
		if(cryptoState >= 2 && N >= 2 && (use256Or512 & 1)) {

			I32x8 clmul00_8[32];
			I32x8 clmul11_8[32];
			I32x8 clmul01_8[32];
			I32x8 clmul10_8[32];
	
			I32x8 a8[32];

			const U8 N2 = N >> 1;

			for (U32 i = 0; i < N2; ++i)
				a8[i] = I32x8_swapEndianness(I32x8_load(&a[i << 1]));

			//Looks a bit odd, but it's to allow multiple clmuls to run in parallel.
			//Then, it'll be xored later. If we do clmulNN[i] ^= it creates a dependency, stalling everything.

			for (U32 i = 0; i < N2; ++i) {
				I32x8 Hi = I32x8_create4_4(H[N - 1 - (i << 1)], H[N - 2 - (i << 1)]);
				clmul00_8[i] = I32x8_clmul64(a8[i], Hi, 0x00);
				clmul01_8[i] = I32x8_clmul64(a8[i], Hi, 0x01);
				clmul10_8[i] = I32x8_clmul64(a8[i], Hi, 0x10);
				clmul11_8[i] = I32x8_clmul64(a8[i], Hi, 0x11);
			}

			if (N2 > 1) {

				for (U32 i = 0; i < (U32)(N2 >> 1); ++i) {
					U32 left = i << 1;
					clmul00_8[left] = I32x8_xor(clmul00_8[left], clmul00_8[left | 1]);
					clmul01_8[left] = I32x8_xor(clmul01_8[left], clmul01_8[left | 1]);
					clmul10_8[left] = I32x8_xor(clmul10_8[left], clmul10_8[left | 1]);
					clmul11_8[left] = I32x8_xor(clmul11_8[left], clmul11_8[left | 1]);
				}

				if (N2 > 2) {

					for (U32 i = 0; i < (U32)(N2 >> 2); ++i) {
						U32 left = i << 2;
						clmul00_8[left] = I32x8_xor(clmul00_8[left], clmul00_8[left | 2]);
						clmul01_8[left] = I32x8_xor(clmul01_8[left], clmul01_8[left | 2]);
						clmul10_8[left] = I32x8_xor(clmul10_8[left], clmul10_8[left | 2]);
						clmul11_8[left] = I32x8_xor(clmul11_8[left], clmul11_8[left | 2]);
					}

					if (N2 > 4) {

						for (U32 i = 0; i < (U32)(N2 >> 3); ++i) {
							U32 left = i << 3;
							clmul00_8[left] = I32x8_xor(clmul00_8[left], clmul00_8[left | 4]);
							clmul01_8[left] = I32x8_xor(clmul01_8[left], clmul01_8[left | 4]);
							clmul10_8[left] = I32x8_xor(clmul10_8[left], clmul10_8[left | 4]);
							clmul11_8[left] = I32x8_xor(clmul11_8[left], clmul11_8[left | 4]);
						}

						if (N2 > 8) {
							clmul00_8[0] = I32x8_xor(clmul00_8[0], clmul00_8[8]);
							clmul01_8[0] = I32x8_xor(clmul01_8[0], clmul01_8[8]);
							clmul10_8[0] = I32x8_xor(clmul10_8[0], clmul10_8[8]);
							clmul11_8[0] = I32x8_xor(clmul11_8[0], clmul11_8[8]);
						}
					}
				}
			}

			clmul00_0 = I32x4_xor(I32x8_getI32x4(clmul00_8[0], 0), I32x8_getI32x4(clmul00_8[0], 1));
			clmul01_0 = I32x4_xor(I32x8_getI32x4(clmul01_8[0], 0), I32x8_getI32x4(clmul01_8[0], 1));
			clmul10_0 = I32x4_xor(I32x8_getI32x4(clmul10_8[0], 0), I32x8_getI32x4(clmul10_8[0], 1));
			clmul11_0 = I32x4_xor(I32x8_getI32x4(clmul11_8[0], 0), I32x8_getI32x4(clmul11_8[0], 1));
		} else

	#endif

	{
		for (U32 i = 0; i < N; ++i)
			a[i] = I32x4_swapEndianness(a[i]);

		I32x4 clmul00[16];
		I32x4 clmul11[16];
		I32x4 clmul01[16];
		I32x4 clmul10[16];

		//Looks a bit odd, but it's to allow multiple clmuls to run in parallel.
		//Then, it'll be xored later. If we do clmulNN[i] ^= it creates a dependency, stalling everything.

		for (U32 i = 0; i < N; ++i) {
			I32x4 Hi = H[N - 1 - i];
			clmul00[i] = I32x4_clmul64(a[i], Hi, 0x00);
			clmul01[i] = I32x4_clmul64(a[i], Hi, 0x01);
			clmul10[i] = I32x4_clmul64(a[i], Hi, 0x10);
			clmul11[i] = I32x4_clmul64(a[i], Hi, 0x11);
		}

		if (N > 1) {

			for (U32 i = 0; i < (U32)(N >> 1); ++i) {
				U32 left = i << 1;
				clmul00[left] = I32x4_xor(clmul00[left], clmul00[left | 1]);
				clmul01[left] = I32x4_xor(clmul01[left], clmul01[left | 1]);
				clmul10[left] = I32x4_xor(clmul10[left], clmul10[left | 1]);
				clmul11[left] = I32x4_xor(clmul11[left], clmul11[left | 1]);
			}

			if (N > 2) {

				for (U32 i = 0; i < (U32)(N >> 2); ++i) {
					U32 left = i << 2;
					clmul00[left] = I32x4_xor(clmul00[left], clmul00[left | 2]);
					clmul01[left] = I32x4_xor(clmul01[left], clmul01[left | 2]);
					clmul10[left] = I32x4_xor(clmul10[left], clmul10[left | 2]);
					clmul11[left] = I32x4_xor(clmul11[left], clmul11[left | 2]);
				}

				if (N > 4) {

					for (U32 i = 0; i < (U32)(N >> 3); ++i) {
						U32 left = i << 3;
						clmul00[left] = I32x4_xor(clmul00[left], clmul00[left | 4]);
						clmul01[left] = I32x4_xor(clmul01[left], clmul01[left | 4]);
						clmul10[left] = I32x4_xor(clmul10[left], clmul10[left | 4]);
						clmul11[left] = I32x4_xor(clmul11[left], clmul11[left | 4]);
					}

					if (N > 8) {
						clmul00[0] = I32x4_xor(clmul00[0], clmul00[8]);
						clmul01[0] = I32x4_xor(clmul01[0], clmul01[8]);
						clmul10[0] = I32x4_xor(clmul10[0], clmul10[8]);
						clmul11[0] = I32x4_xor(clmul11[0], clmul11[8]);
					}
				}
			}
		}

		clmul00_0 = clmul00[0];
		clmul01_0 = clmul01[0];
		clmul10_0 = clmul10[0];
		clmul11_0 = clmul11[0];
	}

	I32x4 tmp[8];

	tmp[0] = clmul00_0;
	tmp[3] = I32x4_xor(clmul10_0, clmul01_0);
	tmp[2] = clmul11_0;

	tmp[1] = I32x4_lshElements(tmp[3], 2);
	tmp[3] = I32x4_rshElements(tmp[3], 2);

	for (U8 i = 0; i < 2; ++i) {
		I32x4 t = I32x4_xor(tmp[i << 1], tmp[(i << 1) + 1]);
		tmp[i << 1] = I32x4_lsh32(t, 1);
		tmp[4 + (i << 1)] = I32x4_rsh32(t, 31);
	}

	tmp[7] = I32x4_rshElements(tmp[4], 3);

	for (U8 i = 0; i < 2; ++i)
		tmp[6 - i] = I32x4_lshElements(tmp[6 - (i << 1)], 1);

	const U8 v0[3] = { 31, 30, 25 };

	for (U8 i = 0; i < 3; ++i) {
		tmp[i << 1] = I32x4_or(tmp[i ? 2 : 0], tmp[5 + i]);
		tmp[5 + i] = I32x4_lsh32(tmp[0], v0[i]);
	}

	for (U8 i = 0; i < 2; ++i)
		tmp[5] = I32x4_xor(tmp[5], tmp[6 + i]);

	tmp[3] = I32x4_rshElements(tmp[5], 1);
	tmp[5] = I32x4_xor(tmp[0], I32x4_lshElements(tmp[5], 3));

	const U8 v1[3] = { 1, 2, 7 };

	for (U8 i = 0; i < 3; ++i)
		tmp[i] = I32x4_rsh32(tmp[5], v1[i]);

	for (U8 i = 1; i < 6; ++i)
		tmp[0] = I32x4_xor(tmp[0], tmp[i]);

	return I32x4_swapEndianness(tmp[0]);
}

//Safe fetch a block (even if <16 bytes are left)
static inline I32x4 AESEncryptionContext_fetchBlockTail(const void *restrict dat, const U64 leftOver) {
	I32x4 v = I32x4_zero();
	Buffer_memcpy(Buffer_createRef(&v, sizeof(v)), Buffer_createRefConst(dat, leftOver));
	return v;
}


static inline void AESEncryptionContext_updateTagN(
	AESEncryptionContext *restrict ctx, const I32x4 *restrict CTi, const U8 N, U8 use256Or512
) {
	
	I32x4 v[128];

	v[0] = I32x4_xor(CTi[0], ctx->tag);

	for (U8 i = 1; i < N; ++i)
		v[i] = CTi[i];

	ctx->tag = AESEncryptionContext_ghashN(v, ctx->H, N, use256Or512);
}

static inline void AESEncryptionContext_updateTagTail(AESEncryptionContext *restrict ctx, I32x4 CTi, const U8 leftOver) {
	Buffer_unsetAllBits(Buffer_createRef(((U8*)&CTi + leftOver), 16 - leftOver), NULL);
	CTi = I32x4_xor(CTi, ctx->tag);
	ctx->tag = AESEncryptionContext_ghashN(&CTi, ctx->H, 1, false);
}

//Hash in the additional data
//This could be something like sender + receiver ip address
//This data could allow the dev to discard invalid packets for example
//And verify that this is the data the original message was signed with
void Buffer_aesExpertUpdateAADFast(AESEncryptionContext *restrict ctx, Buffer additionalData, U8 blockSize, U8 use256Or512) {

	const U64 len = Buffer_length(additionalData);

	if (!len)
		return;

	U64 next = 0;
	const I32x4 *restrict ptr = (const I32x4* restrict)additionalData.ptr;

	{
		if (blockSize >= 128 && (use256Or512 & 2)) {
			while (next + 2048 <= len) {

				I32x4 v[128];

				for (U32 i = 0; i < 128; ++i)
					v[i] = ptr[(next >> 4) | i];

				AESEncryptionContext_updateTagN(ctx, v, 128, use256Or512);
				next += 2048;
			}
		}

		if (blockSize >= 64 && use256Or512) {
			while (next + 1024 <= len) {

				I32x4 v[64];

				for (U32 i = 0; i < 64; ++i)
					v[i] = ptr[(next >> 4) | i];

				AESEncryptionContext_updateTagN(ctx, v, 64, use256Or512);
				next += 1024;
			}
		}

		if (blockSize >= 32 && use256Or512) {
			while (next + 512 <= len) {

				I32x4 v[32];

				for (U32 i = 0; i < 32; ++i)
					v[i] = ptr[(next >> 4) | i];

				AESEncryptionContext_updateTagN(ctx, v, 32, use256Or512);
				next += 512;
			}
		}

		if (blockSize >= 16) {
			while (next + 256 <= len) {

				I32x4 v[16];

				for (U32 i = 0; i < 16; ++i)
					v[i] = ptr[(next >> 4) | i];

				AESEncryptionContext_updateTagN(ctx, v, 16, use256Or512);
				next += 256;
			}
		}

		if (blockSize >= 8) {
			while (next + 128 <= len) {

				I32x4 v[8];

				for (U32 i = 0; i < 8; ++i)
					v[i] = ptr[(next >> 4) | i];

				AESEncryptionContext_updateTagN(ctx, v, 8, use256Or512);
				next += 128;
			}
		}

		if (blockSize >= 4) {
			while (next + 64 <= len) {

				I32x4 v[4];

				for (U32 i = 0; i < 4; ++i)
					v[i] = ptr[(next >> 4) | i];

				AESEncryptionContext_updateTagN(ctx, v, 4, use256Or512);
				next += 64;
			}
		}

		if (blockSize >= 2) {
			while (next + 32 <= len) {

				I32x4 v[2];

				for (U32 i = 0; i < 2; ++i)
					v[i] = ptr[(next >> 4) | i];

				AESEncryptionContext_updateTagN(ctx, v, 2, use256Or512);
				next += 32;
			}
		}
	}

	while (next + 16 <= len) {
		I32x4 v = ptr[next >> 4];
		AESEncryptionContext_updateTagN(ctx, &v, 1, false);
		next += 16;
	}

	if (next < len)
		AESEncryptionContext_updateTagTail(
			ctx, AESEncryptionContext_fetchBlockTail(ptr + (next >> 4), len & 15), (U8)(len & 15)
		);
}

void Buffer_aesExpertUpdateAAD(AESEncryptionContext *restrict ctx, Buffer data, U8 blockSizeMax, U8 use256Or512) {
	Buffer_aesExpertUpdateAADFast(ctx, data, blockSizeMax, use256Or512);
}

static inline void Buffer_aesExpertExpandHash(AESEncryptionContext *restrict ctx, U8 blockSizeMax) {
	for (U8 i = 1; i < blockSizeMax; ++i) {
		I32x4 Hi1 = I32x4_swapEndianness(ctx->H[i - 1]);
		ctx->H[i] = AESEncryptionContext_ghashN(&Hi1, ctx->H, 1, false);
		ctx->H[i] = I32x4_swapEndianness(ctx->H[i]);
	}
}

//TODO: Find a better way of doing this

#if _ARCH == ARCH_ARM64

	U8 AES_getOptimalBatchSize(U64 totalSize, U8 *use256Or512) {

		(void)use256Or512;

		if(totalSize <= 16)
			return 1;

		if(blockSizeHint <= 512)
			return 2;

		if(blockSizeHint <= 4096)
			return 8;

		return 16;
	}

#else

	//TODO:
	U8 AES_getOptimalBatchSize(U64 totalSize, U8 *use256Or512) {

		U8 batchSize;

		if (totalSize < 32) {
			*use256Or512 = false;
			batchSize = 1;
		}

		else if (totalSize <= 64) {
			batchSize = 2;
			*use256Or512 = true;  // AVX512 helps here (+6-12%)
		}
		else if (totalSize <= 512) {
			// Small-medium blocks: batch=8 sweet spot
			batchSize = 8;
			*use256Or512 = false;  // Avoid frequency penalty
		}
		else if (totalSize <= 4096) {
			// Medium-large blocks: batch=16 optimal
			batchSize = 16;
			*use256Or512 = true;  // Penalty amortized
		}
		else {
			// Very large blocks: batch=16 still optimal (32 gives no benefit)
			batchSize = 32;
			*use256Or512 = true;
		}

		return batchSize;
	}
#endif

Bool Buffer_aesExpertCreate(
	I32x4 iv,
	EBufferEncryptionType type,
	AESEncryptionKey key,
	I64 blockSizeHint,
	U8 *restrict blockSizeMax,
	U8 *restrict use256Or512,
	AESEncryptionContext *restrict ctx,
	Error *restrict e_rr
) {

	Bool s_uccess = true;

	//Initialize crypto state

	SIMD_createCryptoState();

	if ((U64)type >= EBufferEncryptionType_Count)
		retError(clean, Error_invalidEnum(
			1, (U64)type, EBufferEncryptionType_Count, "Buffer_aesExpertCreate()::type is out of bounds"
		));

	U8 blockSize = 128;

	switch (blockSizeHint) {

		case -128:
		case -64:
		case -32:
		case -16:
		case -8:
		case -4:
		case -2:
		case -1:
			blockSize = (U8)-blockSizeHint;
			break;

		case 0:
			blockSize = 64;
			break;

		default:

			if(blockSizeHint < 0)
				retError(clean, Error_invalidEnum(
					1, (U64)-blockSizeHint, 16,
					"Buffer_aesExpertCreate()::blockSizeHint must be -16, -8, -4, -2, -1 or a positive number"
				));

			blockSizeHint /= 16;
			blockSize = AES_getOptimalBatchSize(blockSizeHint, use256Or512);
			break;
	}

	blockSize = 128;
	U8 use256Or512Real = 3;

	if (cryptoState <= 1) {
		blockSize = U8_min(blockSize, 16);
		use256Or512Real = 0;
	}

	else if (cryptoState <= 2) {
		blockSize = U8_min(blockSize, 64);
		use256Or512Real &= 1;
	}

	else {
		blockSize = U8_min(blockSize, 128);
		use256Or512Real &= 3;
	}

	if (use256Or512) *use256Or512 = use256Or512Real;
	if (blockSizeMax) *blockSizeMax = blockSize;

	//Get key that's gonna be used for aes blocks

	ctx->encryptionType = type;
	AESEncryptionContext_expandKey(key.u32x8, ctx->key, ctx->encryptionType);

	//Prepare ghash

	ctx->H[0] = AESEncryptionContext_blockHash(I32x4_zero(), ctx->key, ctx->encryptionType);
	ctx->H[0] = I32x4_swapEndianness(ctx->H[0]);

	switch (blockSize) {
		case 2:		Buffer_aesExpertExpandHash(ctx, 2);		break;
		case 4:		Buffer_aesExpertExpandHash(ctx, 4);		break;
		case 8:		Buffer_aesExpertExpandHash(ctx, 8);		break;
		case 16:	Buffer_aesExpertExpandHash(ctx, 16);	break;
		case 32:	Buffer_aesExpertExpandHash(ctx, 32);	break;
		case 64:	Buffer_aesExpertExpandHash(ctx, 64);	break;
		case 128:	Buffer_aesExpertExpandHash(ctx, 128);	break;
	}

	//Compute final tag xor

	I32x4 Y0 = iv;
	ctx->iv = Y0;

	I32x4_setWRef(&Y0, I32_swapEndianness(1));

	ctx->EKY0 = AESEncryptionContext_blockHash(Y0, ctx->key, ctx->encryptionType);
	ctx->tag = I32x4_zero();

clean:
	return s_uccess;
}

static inline Bool AESEncryptionContext_create(
	const BufferEncrypt *restrict encrypt,
	AESEncryptionContext *restrict ctx,
	U8 *restrict blockSize,
	U8 *restrict use256Or512,
	Error *restrict e_rr
) {

	Bool s_uccess = true;

	if (!encrypt->target)
		retError(clean, Error_nullPointer(0, "AESEncryptionContext_create()::decrypt->target must be non zero"));

	if (Buffer_isConstRef(*encrypt->target))
		retError(clean, Error_constData(0, 0, "AESEncryptionContext_create()::decrypt->target needs to be writable"));

	if (!encrypt->constDecrypt.key || !encrypt->constDecrypt.iv || !encrypt->constDecrypt.tag)
		retError(clean, Error_nullPointer(
			!encrypt->constDecrypt.key ? 4 : (!encrypt->constDecrypt.iv ? 5 : 6),
			"AESEncryptionContext_create()::encrypt->key, iv and tag are required"
		));

	if(encrypt->additionalData && Buffer_length(*encrypt->additionalData) >= (U64_MAX >> 3))
		retError(clean, Error_unsupportedOperation(
			0,
			"AESEncryptionContext_create()::->additionalData has a limit of U32_MAX  bits to avoid bit length issues in GMAC"
		));

	if(encrypt->additionalData && (U64)(void*)encrypt->additionalData->ptr & 15)
		retError(clean, Error_unsupportedOperation(
			0, "AESEncryptionContext_create()::->additionalData was misaligned, expecting 16-byte alignment"
		));

	if(encrypt->target && (U64)(void*)encrypt->target->ptr & 15)
		retError(clean, Error_unsupportedOperation(
			0, "AESEncryptionContext_create()::->target was misaligned, expecting 16-byte alignment"
		));

	const U64 targetLen = Buffer_length(*encrypt->target);

	//Since we have a 12-byte IV, we have a 4-byte block counter.
	//This block counter runs out in (4Gi - 3) * sizeof(Block) aka ~4Gi * 16 = ~64GiB.
	//When the IV block counter runs out it would basically repeat the same block xor pattern again.
	//-3 because we start at 2 since 1 is used at the end for verification (and 0 is skipped).

	if(targetLen > (4 * GIBI - 3) * sizeof(I32x4))
		retError(clean, Error_unsupportedOperation(
			0,
			"AESEncryptionContext_create()::target has a limit of 64GB - 48 bytes to avoid block counter re-use.\n"
			"If file size exceeds 64GB encrypt in blocks with a unique IV each 64GB block"
		));

	AESEncryptionKey key;

	if (encrypt->type == EBufferEncryptionType_AES256GCM)
		Buffer_memcpy(
			Buffer_createRef(&key, sizeof(key)),
			Buffer_createRefConst(encrypt->constDecrypt.key, sizeof(key))
		);

	else Buffer_memcpy(
		Buffer_createRef(&key, sizeof(I32x4)),
		Buffer_createRefConst(encrypt->constDecrypt.key, sizeof(I32x4))
	);

	U64 blockHint = targetLen + (encrypt->additionalData ? Buffer_length(*encrypt->additionalData) : 0);

	if(blockHint >> 63)
		retError(clean, Error_unsupportedOperation(
			0, "AESEncryptionContext_create()::target + additional data is limited to 63-bit"
		));

	gotoIfError3(clean, Buffer_aesExpertCreate(
		*encrypt->constDecrypt.iv, encrypt->type, key, blockHint, blockSize, use256Or512, ctx, e_rr
	));

	if (encrypt->additionalData)
		switch (*blockSize) {
			case 1:		Buffer_aesExpertUpdateAADFast(ctx, *encrypt->additionalData,   1, *use256Or512);	break;
			case 2:		Buffer_aesExpertUpdateAADFast(ctx, *encrypt->additionalData,   2, *use256Or512);	break;
			case 4:		Buffer_aesExpertUpdateAADFast(ctx, *encrypt->additionalData,   4, *use256Or512);	break;
			case 8:		Buffer_aesExpertUpdateAADFast(ctx, *encrypt->additionalData,   8, *use256Or512);	break;
			case 16:	Buffer_aesExpertUpdateAADFast(ctx, *encrypt->additionalData,  16, *use256Or512);	break;
			case 32:	Buffer_aesExpertUpdateAADFast(ctx, *encrypt->additionalData,  32, *use256Or512);	break;
			case 64:	Buffer_aesExpertUpdateAADFast(ctx, *encrypt->additionalData,  64, *use256Or512);	break;
			case 128:	Buffer_aesExpertUpdateAADFast(ctx, *encrypt->additionalData, 128, *use256Or512);	break;
		}

clean:
	return s_uccess;
}

typedef union AESEncryptionContextLengths {
	I32x4 vec;
	U64 arr[2];
} AESEncryptionContextLengths;

//This ensures no expanded key, iv or anything else is leaked on the stack,
//which might be possible to obtain after execution through for example a buffer overflow.
static inline void AESEncryptionContext_clear(AESEncryptionContext *restrict ctx) {
	Buffer_unsetAllBits(Buffer_createRef(ctx->key, sizeof(ctx->key)), NULL);
	Buffer_unsetAllBits(Buffer_createRef(ctx->H, sizeof(ctx->H)), NULL);
	ctx->iv = ctx->tag = ctx->EKY0 = I32x4_zero();
	ctx->encryptionType = 0;
}

Bool Buffer_aesExpertFinalize(AESEncryptionContext *restrict ctx, U64 aadLen, U64 dataLen, I32x4 expectTag) {

	//Add length of inputs into the message too (lengths are in bits)

	AESEncryptionContextLengths lengths = { 0 };

	lengths.arr[0] = U64_swapEndianness(aadLen << 3);
	lengths.arr[1] = U64_swapEndianness(dataLen << 3);

	I32x4 tag = I32x4_xor(ctx->tag, lengths.vec);
	ctx->tag = AESEncryptionContext_ghashN(&tag, ctx->H, 1, false);

	//Finish up by adding the iv into the key (this already has blockId 1 in it)

	ctx->tag = I32x4_xor(ctx->tag, ctx->EKY0);

	tag = ctx->tag;
	AESEncryptionContext_clear(ctx);
	ctx->tag = tag;

	return I32x4_eq4(tag, expectTag);
}

static inline void AESEncryptionContext_storeBlockTail(I32 *restrict io, const U64 leftOver, void *restrict v) {
	Buffer_memcpy(Buffer_createRef(io, sizeof(I32x4)), Buffer_createRefConst(v, leftOver));
}

static inline void AESEncryptionContext_processBlockTail(
	AESEncryptionContext *restrict ctx,
	I32 *restrict io,
	const U8 leftOver,
	const U32 i,
	Bool isEncrypt
) {

	I32x4 v = AESEncryptionContext_fetchBlockTail(io, leftOver);

	//Update tag for the ciphertext (before decryption)

	if (!isEncrypt)
		AESEncryptionContext_updateTagTail(ctx, v, leftOver);

	//Encrypt IV + blockId to use to encrypt

	I32x4 ivi = ctx->iv;
	I32x4_setWRef(&ivi, (I32)U32_swapEndianness(i + 2));

	v = I32x4_xor(v, AESEncryptionContext_blockHash(ivi, ctx->key, ctx->encryptionType));

	//Update tag for the ciphertext (after encryption)

	if (isEncrypt)
		AESEncryptionContext_updateTagTail(ctx, v, leftOver);

	//Store

	AESEncryptionContext_storeBlockTail(io, leftOver, &v);
}

static inline void AESEncryptionContext_processBlockN(
	AESEncryptionContext *restrict ctx,
	I32x4 *restrict io,
	const U32 id,
	const U8 N,
	Bool isEncrypt,
	U8 use256Or512
) {
	I32x4 iv = ctx->iv;

	#ifdef HAS_AESx4
		//cryptoState >= 3: has everything needed for x2 + AVX512F
		if(cryptoState >= 3 && N >= 4 && (use256Or512 & 2)) {
		
			I32x16 v[32];		//TODO: Look into bigger blocks
			I32x16 ivi[32];

			U8 N4 = N >> 2;

			for (U32 i = 0; i < N4; ++i)
				v[i] = I32x16_load(&io[i << 2]);

			if (!isEncrypt)
				AESEncryptionContext_updateTagN(ctx, (const I32x4 *restrict) v, N, use256Or512);

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
				AESEncryptionContext_updateTagN(ctx, (const I32x4 *restrict) v, N, use256Or512);

			for (U32 i = 0; i < N4; ++i)
				I32x16_store(&io[i << 2], v[i]);

			return;
		}
	#endif

	#ifdef HAS_AESx2
		//cryptoState >= 2: has AVX2, AVX512VL, VAES, VPCLMULQDQ
		if(cryptoState >= 2 && N >= 2 && (use256Or512 & 1)) {
		
			I32x8 v[32];
			I32x8 ivi[32];

			U8 N2 = N >> 1;

			for (U32 i = 0; i < N2; ++i)
				v[i] = I32x8_load(&io[i << 1]);

			if (!isEncrypt)
				AESEncryptionContext_updateTagN(ctx, (const I32x4 *restrict) v, N, use256Or512);

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
				AESEncryptionContext_updateTagN(ctx, (const I32x4 *restrict) v, N, use256Or512);

			for (U32 i = 0; i < N2; ++i)
				I32x8_store(&io[i << 1], v[i]);

			return;
		}
	#endif

	I32x4 v[16];

	for (U32 i = 0; i < N; ++i)
		v[i] = io[i];

	//Update tag for the ciphertext (before decryption)

	if (!isEncrypt)
		AESEncryptionContext_updateTagN(ctx, v, N, false);

	//Encrypt IV + blockId to use to encrypt

	for (U32 i = 0; i < N; ++i) {
		I32x4 ivi = iv;
		I32x4_setWRef(&ivi, (I32)U32_swapEndianness(id + i + 2));

		v[i] = I32x4_xor(v[i], AESEncryptionContext_blockHash(ivi, ctx->key, ctx->encryptionType));
	}

	//Update tag for the ciphertext (after encryption)

	if(isEncrypt)
		AESEncryptionContext_updateTagN(ctx, v, N, false);

	//Store

	for (U32 i = 0; i < N; ++i)
		io[i] = v[i];
}

static inline void AESEncryptionContext_handleBlocks(
	AESEncryptionContext *restrict ctx,
	U8 *restrict targetPtr,
	U64 targetLen,
	Bool isEncrypt,
	U32 offsetInBlocks,
	U8 blockSizeMax,
	U8 use256Or512
) {

	U64 next = 0;

	//Handle blocks
	//TODO: We might wanna multithread this if we ever get big enough data
	//		For now, we're dealing with small enough files that spinning up threads would be slower
	//		(Without a job system)

	//32 blocks and higher is reserved for 256-bit vectors

	if (blockSizeMax >= 128 && cryptoState >= 3 && (use256Or512 & 2)) {
		while (next + 2048 <= targetLen) {
			AESEncryptionContext_processBlockN(
				ctx,
				(I32x4*)(targetPtr + next),
				(U32)(next >> 4) + offsetInBlocks,
				128,
				isEncrypt,
				use256Or512
			);
			next += 2048;
		}
	}

	if (blockSizeMax >= 64 && cryptoState >= 2 && (use256Or512 & 1)) {
		while (next + 1024 <= targetLen) {
			AESEncryptionContext_processBlockN(
				ctx,
				(I32x4*)(targetPtr + next),
				(U32)(next >> 4) + offsetInBlocks,
				64,
				isEncrypt,
				use256Or512
			);
			next += 1024;
		}
	}

	if (blockSizeMax >= 32) {
		while (next + 512 <= targetLen) {
			AESEncryptionContext_processBlockN(
				ctx,
				(I32x4*)(targetPtr + next),
				(U32)(next >> 4) + offsetInBlocks,
				32,
				isEncrypt,
				use256Or512
			);
			next += 512;
		}
	}

	//16 blocks at a time, this handles only fully aligned blocks.
	//This improves performance because it allows better scheduling
	// (16 can run in parallel, instead of being blocked every instruction)

	if (blockSizeMax >= 16) {

		while (next + 256 <= targetLen) {
			AESEncryptionContext_processBlockN(
				ctx,
				(I32x4*)(targetPtr + next),
				(U32)(next >> 4) + offsetInBlocks,
				16,
				isEncrypt,
				use256Or512
			);
			next += 256;
		}
	}

	if (blockSizeMax >= 8) {
		while (next + 128 <= targetLen) {
			AESEncryptionContext_processBlockN(
				ctx,
				(I32x4*)(targetPtr + next),
				(U32)(next >> 4) + offsetInBlocks,
				8,
				isEncrypt,
				use256Or512
			);
			next += 128;
		}
	}

	if (blockSizeMax >= 4) {
		while (next + 64 <= targetLen) {
			AESEncryptionContext_processBlockN(
				ctx,
				(I32x4*)(targetPtr + next),
				(U32)(next >> 4) + offsetInBlocks,
				4,
				isEncrypt,
				use256Or512
			);
			next += 64;
		}
	}


	if (blockSizeMax >= 2) {
		while (next + 32 <= targetLen) {
			AESEncryptionContext_processBlockN(
				ctx,
				(I32x4*)(targetPtr + next),
				(U32)(next >> 4) + offsetInBlocks,
				2,
				isEncrypt,
				use256Or512
			);
			next += 32;
		}
	}

	while (next + 16 <= targetLen) {
		AESEncryptionContext_processBlockN(
			ctx,
			(I32x4*)(targetPtr + next),
			(U32)(next >> 4) + offsetInBlocks,
			1,
			isEncrypt,
			false
		);
		next += 16;
	}

	if (next < targetLen)
		AESEncryptionContext_processBlockTail(
			ctx,
			(I32*)(targetPtr + next),
			(U8)(targetLen & 15),
			(U32)(targetLen >> 4) + offsetInBlocks,
			isEncrypt
		);
}

static inline void Buffer_prefetch(Buffer data) {
	#if _SIMD == SIMD_SSE
		if (data.ptr) {
			if (Buffer_length(data) > 256)
				_mm_prefetch((const char*)data.ptr, _MM_HINT_T0);
		}
	#endif
}

inline void Buffer_aesExpertEncUpdateFast(
	AESEncryptionContext *restrict ctx, Buffer data, U32 offsetInBlocks, U8 blockSizeMax, U8 use256Or512
) {
	Buffer_prefetch(data);
	AESEncryptionContext_handleBlocks(ctx, data.ptrNonConst, Buffer_length(data), true, offsetInBlocks, blockSizeMax, use256Or512);
}

inline void Buffer_aesExpertDecUpdateFast(
	AESEncryptionContext *restrict ctx, Buffer data, U32 offsetInBlocks, U8 blockSizeMax, U8 use256Or512
) {
	Buffer_prefetch(data);
	AESEncryptionContext_handleBlocks(ctx, data.ptrNonConst, Buffer_length(data), false, offsetInBlocks, blockSizeMax, use256Or512);
}

void Buffer_aesExpertEncUpdate(
	AESEncryptionContext *restrict ctx, Buffer data, U32 offsetInBlocks, U8 blockSizeMax, U8 use256Or512
) {
	Buffer_aesExpertEncUpdateFast(ctx, data, offsetInBlocks, blockSizeMax, use256Or512);
}

void Buffer_aesExpertDecUpdate(
	AESEncryptionContext *restrict ctx, Buffer data, U32 offsetInBlocks, U8 blockSizeMax, U8 use256Or512
) {
	Buffer_aesExpertDecUpdateFast(ctx, data, offsetInBlocks, blockSizeMax, use256Or512);
}

static inline Bool AESEncryptionContext_encrypt(const BufferEncrypt *restrict encrypt, Error *restrict e_rr) {

	Bool s_uccess = true;

	//Generate iv & context

	I32x4_setWRef(encrypt->nonConstEncrypt.iv, 0);

	if(!(encrypt->flags & EBufferEncryptionFlags_StopCreateIv)) {

		if(!Buffer_csprng(Buffer_createRef(encrypt->nonConstEncrypt.iv, 12)))
			retError(clean, Error_invalidState(0, "AESEncryptionContext_encrypt() couldn't generate iv"));
	}

	if(encrypt->flags & EBufferEncryptionFlags_GenerateKey) {

		const U8 len = encrypt->type == EBufferEncryptionType_AES128GCM ? 4 : 8;

		if(!Buffer_csprng(Buffer_createRef(encrypt->nonConstEncrypt.key, sizeof(U32) * len)))
			retError(clean, Error_invalidState(1, "AESEncryptionContext_encrypt() couldn't generate key"));
	}

	if (encrypt->target)
		Buffer_prefetch(*encrypt->target);

	AESEncryptionContext ctx;
	U8 blockSizeMax;
	U8 use256Or512;
	gotoIfError3(clean, AESEncryptionContext_create(encrypt, &ctx, &blockSizeMax, &use256Or512, e_rr));

	if(encrypt->target)
		switch (blockSizeMax) {
			case 1:		Buffer_aesExpertEncUpdateFast(&ctx, *encrypt->target, 0,   1, use256Or512);	break;
			case 2:		Buffer_aesExpertEncUpdateFast(&ctx, *encrypt->target, 0,   2, use256Or512);	break;
			case 4:		Buffer_aesExpertEncUpdateFast(&ctx, *encrypt->target, 0,   4, use256Or512);	break;
			case 8:		Buffer_aesExpertEncUpdateFast(&ctx, *encrypt->target, 0,   8, use256Or512);	break;
			case 16:	Buffer_aesExpertEncUpdateFast(&ctx, *encrypt->target, 0,  16, use256Or512);	break;
			case 32:	Buffer_aesExpertEncUpdateFast(&ctx, *encrypt->target, 0,  32, use256Or512);	break;
			case 64:	Buffer_aesExpertEncUpdateFast(&ctx, *encrypt->target, 0,  64, use256Or512);	break;
			case 128:	Buffer_aesExpertEncUpdateFast(&ctx, *encrypt->target, 0, 128, use256Or512);	break;
		}

	//Finish encryption by appending tag for authentication / verification that the data isn't messed with

	U64 aadLen = encrypt->additionalData ? Buffer_length(*encrypt->additionalData) : 0;
	U64 dataLen = encrypt->target ? Buffer_length(*encrypt->target) : 0;
	Buffer_aesExpertFinalize(&ctx, aadLen, dataLen, I32x4_zero());
	*encrypt->nonConstEncrypt.tag = ctx.tag;
	ctx.tag = I32x4_zero();

clean:
	return s_uccess;
}

Bool Buffer_encryptAuto(
	Buffer *restrict target,
	const Buffer *restrict additionalData,
	Bool generateKey,
	U32 *restrict key,
	I32x4 *restrict tag,
	I32x4 *restrict iv,
	Error *restrict e_rr
) {
	BufferEncrypt encrypt = (BufferEncrypt) {
		.target = target,
		.additionalData = additionalData,
		.type = EBufferEncryptionType_AES256GCM,
		.flags = generateKey ? EBufferEncryptionFlags_GenerateKey : 0,
		.nonConstEncrypt = {
			.key = key,
			.tag = tag,
			.iv = iv
		}
	};

	return Buffer_encryptAdvanced(&encrypt, e_rr);
}

Bool Buffer_decryptAuto(
	Buffer *restrict target,
	const Buffer *restrict additionalData,
	const U32 *restrict key,
	I32x4 tag,
	I32x4 iv,
	Error *restrict e_rr
) {
	BufferEncrypt decrypt = (BufferEncrypt) {
		.target = target,
		.additionalData = additionalData,
		.type = EBufferEncryptionType_AES256GCM,
		.flags = EBufferEncryptionFlags_None,
		.constDecrypt = {
			.key = key,
			.tag = &tag,
			.iv = &iv
		}
	};

	return Buffer_decryptAdvanced(&decrypt, e_rr);
}

Bool Buffer_encryptAdvanced(const BufferEncrypt *restrict encrypt, Error *restrict e_rr) {

	Bool s_uccess = true;

	if(!encrypt)
		retError(clean, Error_nullPointer(0, "Buffer_encryptAdvanced()::encrypt must be non zero"));

	if(encrypt->flags & EBufferEncryptionFlags_Invalid)
		retError(clean, Error_invalidEnum(
			3, (U64)encrypt->flags, ((U64)1 << EBufferEncryptionFlags_Count) - 1,
			"Buffer_encryptAdvanced()::flags are invalid"
		));

	gotoIfError3(clean, AESEncryptionContext_encrypt(encrypt, e_rr));

clean:
	return s_uccess;
}

static inline Bool AESEncryptionContext_decrypt(const BufferEncrypt *restrict decrypt, Error *restrict e_rr) {

	Bool s_uccess = true;

	//Create context

	if (decrypt->target)
		Buffer_prefetch(*decrypt->target);

	U8 blockSizeMax;
	AESEncryptionContext ctx;
	U8 use256Or512;
	gotoIfError3(clean, AESEncryptionContext_create(decrypt, &ctx, &blockSizeMax, &use256Or512, e_rr));

	if(decrypt->target)
		switch (blockSizeMax) {
			case 1:		Buffer_aesExpertDecUpdateFast(&ctx, *decrypt->target, 0,   1, use256Or512);	break;
			case 2:		Buffer_aesExpertDecUpdateFast(&ctx, *decrypt->target, 0,   2, use256Or512);	break;
			case 4:		Buffer_aesExpertDecUpdateFast(&ctx, *decrypt->target, 0,   4, use256Or512);	break;
			case 8:		Buffer_aesExpertDecUpdateFast(&ctx, *decrypt->target, 0,   8, use256Or512);	break;
			case 16:	Buffer_aesExpertDecUpdateFast(&ctx, *decrypt->target, 0,  16, use256Or512);	break;
			case 32:	Buffer_aesExpertDecUpdateFast(&ctx, *decrypt->target, 0,  32, use256Or512);	break;
			case 64:	Buffer_aesExpertDecUpdateFast(&ctx, *decrypt->target, 0,  64, use256Or512);	break;
			case 128:	Buffer_aesExpertDecUpdateFast(&ctx, *decrypt->target, 0, 128, use256Or512);	break;
		}

	U64 aadLen = decrypt->additionalData ? Buffer_length(*decrypt->additionalData) : 0;
	U64 dataLen = decrypt->target ? Buffer_length(*decrypt->target) : 0;

	//Check if the tag is the same, if not, then it has been tempered with
	if(!Buffer_aesExpertFinalize(&ctx, aadLen, dataLen, *decrypt->constDecrypt.tag)) {

		ctx.tag = I32x4_zero();

		if(decrypt->target)
			Buffer_unsetAllBits(*decrypt->target, NULL);

		retError(clean, Error_invalidState(0, "AESEncryptionContext_decrypt() GMAC tag is invalid"));
	}

	ctx.tag = I32x4_zero();

clean:
	return s_uccess;
}

Bool Buffer_decryptAdvanced(const BufferEncrypt *restrict decrypt, Error *restrict e_rr) {

	Bool s_uccess = true;

	if (!decrypt)
		retError(clean, Error_nullPointer(0, "Buffer_decryptAdvanced()::decrypt must be non zero"));

	if (decrypt->flags)
		retError(clean, Error_invalidParameter(3, 0, "Buffer_decryptAdvanced()::flags are invalid"));

	gotoIfError3(clean, AESEncryptionContext_decrypt(decrypt, e_rr));

clean:
	return s_uccess;
}
