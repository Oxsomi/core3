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

//types/container/buffer_encrypt.c

#include "types/base/error.h"
#include "types/container/buffer_encrypt.h"
#include "types/container/buffer.h"
#include "types/container/simd/aes_encryption_helpers.h"
#include "types/math/u128_base.h"
#include "types/math/vec4i_swizzle.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"
#include "types/math/type_cast.h"
#include "types/base/endianness.h"

#if _ARCH == ARCH_X86_64
	#include <immintrin.h>
#endif

//-1: Uninitialized
//0: No support (No fallback yet though)
//1: Support for AES-NI (or AESE with NEON)
//2: Support for AES-NI, VAES, AVX2, VPCLMUL, AVX512VL, AVX512BM
//3: Support for AES-NI, VAES, AVX2, VPCLMUL, AVX512VL, AVX512BM, AVX512F, AVX512DQ
static I8 cryptoState = -1;

#if _SIMD == SIMD_SSE
	void SIMD_createCryptoState() {

		if (cryptoState >= 0)
			return;

		U32 cpuInfo[4];

		Platform_getCPUId(1, cpuInfo);

		Bool hasAES = (cpuInfo[2] & (1 << 25)) != 0;
		Bool hasPCLMUL = (cpuInfo[2] & (1 << 1)) != 0;
		Bool hasOSXSave = (cpuInfo[2] & (1 << 27)) != 0;

		if (!hasAES || !hasPCLMUL) {
			cryptoState = 0;
			return;
		}

		if (!hasOSXSave || (_xgetbv(0) & 0x6) != 0x6) {
			cryptoState = 1;    // AES-NI only
			return;
		}

		Platform_getCPUId(7, cpuInfo);

		Bool hasAVX2 = (cpuInfo[1] & (1 << 5)) != 0;
		Bool hasAVX512F = (cpuInfo[1] & (1 << 16)) != 0;
		Bool hasAVX512BW = (cpuInfo[1] & (1 << 30)) != 0;
		Bool hasAVX512DQ = (cpuInfo[1] & (1 << 17)) != 0;
		Bool hasAVX512VL = (cpuInfo[1] & (1u << 31)) != 0;
		Bool hasVAES = (cpuInfo[2] & (1 << 9)) != 0;
		Bool hasVPCLMUL = (cpuInfo[2] & (1 << 10)) != 0;

		Bool osHasZMM = (_xgetbv(0) & 0xE0) == 0xE0;

		if (!(hasVAES && hasAVX2 && hasVPCLMUL && hasAVX512VL && osHasZMM && hasAVX512BW)) {
			cryptoState = 1;                //AES-NI only
			return;
		}

		if (hasAVX512F && hasAVX512DQ) {
			cryptoState = 3;                //512-bit path
			return;
		}

		cryptoState = 2;                    //256-bit VAES path
	}
#else
	void SIMD_createCryptoState() { }        //TODO: Use this for presence of crypto in the first place
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
//    - tag = GHASH(tag XOR additional data block)
//
//- IV (Initial vector) = Generate CSPRNG of 12-bytes (if not provided)
//- Store iv in result
//
//- Foreach plaindata block at i padded to 16-byte with 0s:
//    - Eki = encrypt(IV append U32BE(i + 2))
//    - store (cyphertext[i] = plainText[i] XOR Eki) in result
//    - tag = GHASH(tag XOR cyphertext[i])
//
//- tag = GHASH(combine(U64BE(additionalDataBits), U64BE(plainTextBits)) XOR tag)
//- tag = tag XOR aes256(IV with U32BE(1) appended)
//
//- Store tag in result
//
//For "encrypt" we use AES CTR as explained by the intel paper:
//https://www.intel.com/content/dam/doc/white-paper/advanced-encryption-standard-new-instructions-set-paper.pdf

//AES_subWord can be used by as fallback for encryption.
//No lookup tables, those are unsafe.
//TODO: Speedup this fallback, it's made to be a reference & to be secure.
//        Since most devices have AES-NI, no time was spent optimizing this.
//        If you're encrypting a lot of data... goodluck and see you in 2039
//          (Don't use for perf critical without AES-NI support)
//        Investigate Bitslicing, Boyar-Peralta or something like it.

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

MIGHT_BE_UNUSED static inline U32 AES_subWord(U32 w) {
	return
		((U32)AES_sbox((U8)(w >>  0)) <<  0) |
		((U32)AES_sbox((U8)(w >>  8)) <<  8) |
		((U32)AES_sbox((U8)(w >> 16)) << 16) |
		((U32)AES_sbox((U8)(w >> 24)) << 24);
}

#if _SIMD == SIMD_NEON
	#include "types/container/simd/neon/neon_buffer_encrypt.inc.h"
#elif _SIMD == SIMD_SSE
	#include "types/container/simd/sse/sse_buffer_encrypt.inc.h"
	#define HAS_AESx2
	#define HAS_AESx4
	#define HAS_CLMUL64x2
	#define HAS_CLMUL64x4
#else
	#include "types/container/simd/none/none_buffer_encrypt.inc.h"
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

static inline void AESEncryptionContext_expandKey(
	const U32 *restrict key, I32x4 *restrict k/*[15]*/, const EBufferEncryptionType encryptionType
) {

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

//AES block encryption.
//Don't use this plainly, it's a part of the larger AES256-CTR algorithm
__forceinline__ static I32x4 AESEncryptionContext_blockHash(
	I32x4 a,
	const I32x4 *restrict k/*[15]*/,
	const EBufferEncryptionType type
) {

	a = I32x4_xor(a, k[0]);
	a = AES_encodeBlock(a, k[1]);
	a = AES_encodeBlock(a, k[2]);
	a = AES_encodeBlock(a, k[3]);
	a = AES_encodeBlock(a, k[4]);
	a = AES_encodeBlock(a, k[5]);
	a = AES_encodeBlock(a, k[6]);
	a = AES_encodeBlock(a, k[7]);
	a = AES_encodeBlock(a, k[8]);
	a = AES_encodeBlock(a, k[9]);

	if (type == EBufferEncryptionType_AES256GCM) {
		a = AES_encodeBlock(a, k[10]);
		a = AES_encodeBlock(a, k[11]);
		a = AES_encodeBlock(a, k[12]);
		a = AES_encodeBlock(a, k[13]);
		return AES_encodeBlockLast(a, k[14]);
	}

	return AES_encodeBlockLast(a, k[10]);
}

typedef struct I32x4x2 {
	I32x4 a, b;
} I32x4x2;

typedef struct I32x4x3 {
	I32x4 a, b, c;
} I32x4x3;

typedef struct I32x4x4 {
	I32x4 a, b, c, d;
} I32x4x4;

typedef struct I32x4x5 {
	I32x4 a, b, c, d, e;
} I32x4x5;

__forceinline__ static I32x4x2 AESEncryptionContext_blockHash2(
	I32x4 a, I32x4 b, const I32x4 *restrict k/*[15]*/, const EBufferEncryptionType type
) {

	a = I32x4_xor(a, k[0]);
	b = I32x4_xor(b, k[0]);

	a = AES_encodeBlock(a, k[1]);
	b = AES_encodeBlock(b, k[1]);

	a = AES_encodeBlock(a, k[2]);
	b = AES_encodeBlock(b, k[2]);

	a = AES_encodeBlock(a, k[3]);
	b = AES_encodeBlock(b, k[3]);

	a = AES_encodeBlock(a, k[4]);
	b = AES_encodeBlock(b, k[4]);

	a = AES_encodeBlock(a, k[5]);
	b = AES_encodeBlock(b, k[5]);

	a = AES_encodeBlock(a, k[6]);
	b = AES_encodeBlock(b, k[6]);

	a = AES_encodeBlock(a, k[7]);
	b = AES_encodeBlock(b, k[7]);

	a = AES_encodeBlock(a, k[8]);
	b = AES_encodeBlock(b, k[8]);

	a = AES_encodeBlock(a, k[9]);
	b = AES_encodeBlock(b, k[9]);

	if (type == EBufferEncryptionType_AES256GCM) {

		a = AES_encodeBlock(a, k[10]);
		b = AES_encodeBlock(b, k[10]);

		a = AES_encodeBlock(a, k[11]);
		b = AES_encodeBlock(b, k[11]);

		a = AES_encodeBlock(a, k[12]);
		b = AES_encodeBlock(b, k[12]);

		a = AES_encodeBlock(a, k[13]);
		b = AES_encodeBlock(b, k[13]);

		I32x4x2 res = { AES_encodeBlockLast(a, k[14]), AES_encodeBlockLast(b, k[14]) };
		return res;
	}

	I32x4x2 res = { AES_encodeBlockLast(a, k[10]), AES_encodeBlockLast(b, k[10]) };
	return res;
}

__forceinline__ static I32x4x4 AESEncryptionContext_blockHash4(
	I32x4 a, I32x4 b, I32x4 c, I32x4 d, const I32x4 *restrict k/*[15]*/, const EBufferEncryptionType type
) {

	a = I32x4_xor(a, k[0]);
	b = I32x4_xor(b, k[0]);
	c = I32x4_xor(c, k[0]);
	d = I32x4_xor(d, k[0]);

	a = AES_encodeBlock(a, k[1]);
	b = AES_encodeBlock(b, k[1]);
	c = AES_encodeBlock(c, k[1]);
	d = AES_encodeBlock(d, k[1]);

	a = AES_encodeBlock(a, k[2]);
	b = AES_encodeBlock(b, k[2]);
	c = AES_encodeBlock(c, k[2]);
	d = AES_encodeBlock(d, k[2]);

	a = AES_encodeBlock(a, k[3]);
	b = AES_encodeBlock(b, k[3]);
	c = AES_encodeBlock(c, k[3]);
	d = AES_encodeBlock(d, k[3]);

	a = AES_encodeBlock(a, k[4]);
	b = AES_encodeBlock(b, k[4]);
	c = AES_encodeBlock(c, k[4]);
	d = AES_encodeBlock(d, k[4]);

	a = AES_encodeBlock(a, k[5]);
	b = AES_encodeBlock(b, k[5]);
	c = AES_encodeBlock(c, k[5]);
	d = AES_encodeBlock(d, k[5]);

	a = AES_encodeBlock(a, k[6]);
	b = AES_encodeBlock(b, k[6]);
	c = AES_encodeBlock(c, k[6]);
	d = AES_encodeBlock(d, k[6]);

	a = AES_encodeBlock(a, k[7]);
	b = AES_encodeBlock(b, k[7]);
	c = AES_encodeBlock(c, k[7]);
	d = AES_encodeBlock(d, k[7]);

	a = AES_encodeBlock(a, k[8]);
	b = AES_encodeBlock(b, k[8]);
	c = AES_encodeBlock(c, k[8]);
	d = AES_encodeBlock(d, k[8]);

	a = AES_encodeBlock(a, k[9]);
	b = AES_encodeBlock(b, k[9]);
	c = AES_encodeBlock(c, k[9]);
	d = AES_encodeBlock(d, k[9]);

	if (type == EBufferEncryptionType_AES256GCM) {

		a = AES_encodeBlock(a, k[10]);
		b = AES_encodeBlock(b, k[10]);
		c = AES_encodeBlock(c, k[10]);
		d = AES_encodeBlock(d, k[10]);

		a = AES_encodeBlock(a, k[11]);
		b = AES_encodeBlock(b, k[11]);
		c = AES_encodeBlock(c, k[11]);
		d = AES_encodeBlock(d, k[11]);

		a = AES_encodeBlock(a, k[12]);
		b = AES_encodeBlock(b, k[12]);
		c = AES_encodeBlock(c, k[12]);
		d = AES_encodeBlock(d, k[12]);

		a = AES_encodeBlock(a, k[13]);
		b = AES_encodeBlock(b, k[13]);
		c = AES_encodeBlock(c, k[13]);
		d = AES_encodeBlock(d, k[13]);

		I32x4x4 res = {
			AES_encodeBlockLast(a, k[14]),
			AES_encodeBlockLast(b, k[14]),
			AES_encodeBlockLast(c, k[14]),
			AES_encodeBlockLast(d, k[14])
		};

		return res;
	}

	I32x4x4 res = {
		AES_encodeBlockLast(a, k[10]),
		AES_encodeBlockLast(b, k[10]),
		AES_encodeBlockLast(c, k[10]),
		AES_encodeBlockLast(d, k[10])
	};

	return res;
}

__forceinline__ static I32x4x2 AESEncryptionContext_blockHashAndGhash(
	I32x4 a,        //ivi
	I32x4 H,
	I32x4 b,        //prevState
	I32x4 tag,
	const I32x4 *restrict k,
	const EBufferEncryptionType type
) {
	//Manual interleaving because you can't trust MSVC to generate proper code unfortunately.

	b = I32x4_xor(b, tag);
	a = I32x4_xor(a, k[0]);

	b = I32x4_swapEndianness(b);
	a = AES_encodeBlock(a, k[1]);
	a = AES_encodeBlock(a, k[2]);
	I32x4 clmul01 = I32x4_clmul64(b, H, 0x01);
	a = AES_encodeBlock(a, k[3]);
	a = AES_encodeBlock(a, k[4]);
	I32x4 clmul10 = I32x4_clmul64(b, H, 0x10);
	a = AES_encodeBlock(a, k[5]);
	a = AES_encodeBlock(a, k[6]);
	I32x4 clmul00 = I32x4_clmul64(b, H, 0x00);
	a = AES_encodeBlock(a, k[7]);
	a = AES_encodeBlock(a, k[8]);
	I32x4 clmul11 = I32x4_clmul64(b, H, 0x11);
	a = AES_encodeBlock(a, k[9]);

	if (type == EBufferEncryptionType_AES256GCM) {

		a = AES_encodeBlock(a, k[10]);

		clmul01 = I32x4_xor(clmul01, clmul10);
		a = AES_encodeBlock(a, k[11]);

		I32x4 tmp1 = I32x4_lshElements(clmul01, 2);
		I32x4 tmp3 = I32x4_rshElements(clmul01, 2);
		a = AES_encodeBlock(a, k[12]);

		I32x4 t0 = I32x4_xor(clmul00, tmp1);
		I32x4 t1 = I32x4_xor(clmul11, tmp3);
		a = AES_encodeBlock(a, k[13]);

		I32x4 tmp0 = I32x4_lsh32(t0, 1);
		I32x4 tmp4 = I32x4_rsh32(t0, 31);
		I32x4 tmp2 = I32x4_lsh32(t1, 1);
		I32x4 tmp6 = I32x4_rsh32(t1, 31);
		a = AES_encodeBlockLast(a, k[14]);

		I32x4 tmp7 = I32x4_rshElements(tmp4, 3);
		tmp6 = I32x4_lshElements(tmp6, 1);
		I32x4 tmp5 = I32x4_lshElements(tmp4, 1);

		tmp0 = I32x4_or(tmp0, tmp5);
		tmp2 = I32x4_or(tmp2, tmp6);
		tmp5 = I32x4_lsh32(tmp0, 31);
		tmp6 = I32x4_lsh32(tmp0, 30);
		tmp4 = I32x4_or(tmp2, tmp7);
		tmp7 = I32x4_lsh32(tmp0, 25);

		tmp5 = I32x4_xor(tmp5, tmp6);
		tmp5 = I32x4_xor(tmp5, tmp7);

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
		a = AES_encodeBlockLast(a, k[10]);

		clmul01 = I32x4_xor(clmul01, clmul10);
		b = AESEncryptionContext_ghashReduceClMul(clmul00, clmul01, clmul11);
	}

	I32x4x2 ab = { a, b };
	return ab;
}

__forceinline__ static I32x4x3 AESEncryptionContext_blockHashAndGhash2(
	I32x4 a0,
	I32x4 a1,
	I32x4 H,
	I32x4 H2,
	I32x4 b0,
	I32x4 b1,
	I32x4 tag,
	const I32x4 *restrict k,
	const EBufferEncryptionType type
) {
	//Manual interleaving because you can't trust MSVC to generate proper code unfortunately.

	b0 = I32x4_xor(b0, tag);

	a0 = I32x4_xor(a0, k[0]);
	a1 = I32x4_xor(a1, k[0]);

	b0 = I32x4_swapEndianness(b0);
	b1 = I32x4_swapEndianness(b1);

	a0 = AES_encodeBlock(a0, k[1]);
	a1 = AES_encodeBlock(a1, k[1]);

	I32x4 clmul01_0 = I32x4_clmul64(b0, H2, 0x01);
	I32x4 clmul10_0 = I32x4_clmul64(b0, H2, 0x10);
	clmul01_0 = I32x4_xor(clmul01_0, clmul10_0);

	a0 = AES_encodeBlock(a0, k[2]);
	a1 = AES_encodeBlock(a1, k[2]);

	I32x4 clmul01_1 = I32x4_clmul64(b1, H, 0x01);
	I32x4 clmul10_1 = I32x4_clmul64(b1, H, 0x10);
	clmul01_1 = I32x4_xor(clmul01_1, clmul10_1);

	a0 = AES_encodeBlock(a0, k[3]);
	a1 = AES_encodeBlock(a1, k[3]);

	I32x4 clmul00_0 = I32x4_clmul64(b0, H2, 0x00);
	I32x4 clmul00_1 = I32x4_clmul64(b1, H, 0x00);

	a0 = AES_encodeBlock(a0, k[4]);
	a1 = AES_encodeBlock(a1, k[4]);

	I32x4 clmul11_0 = I32x4_clmul64(b0, H2, 0x11);
	I32x4 clmul11_1 = I32x4_clmul64(b1, H, 0x11);

	a0 = AES_encodeBlock(a0, k[5]);
	a1 = AES_encodeBlock(a1, k[5]);
	a0 = AES_encodeBlock(a0, k[6]);
	a1 = AES_encodeBlock(a1, k[6]);

	a0 = AES_encodeBlock(a0, k[7]);
	a1 = AES_encodeBlock(a1, k[7]);
	a0 = AES_encodeBlock(a0, k[8]);
	a1 = AES_encodeBlock(a1, k[8]);

	a0 = AES_encodeBlock(a0, k[9]);
	a1 = AES_encodeBlock(a1, k[9]);

	I32x4 b;

	if (type == EBufferEncryptionType_AES256GCM) {

		a0 = AES_encodeBlock(a0, k[10]);
		a1 = AES_encodeBlock(a1, k[10]);

		clmul01_0 = I32x4_xor(clmul01_0, clmul01_1);
		clmul00_0 = I32x4_xor(clmul00_0, clmul00_1);

		a0 = AES_encodeBlock(a0, k[11]);
		a1 = AES_encodeBlock(a1, k[11]);

		clmul11_0 = I32x4_xor(clmul11_0, clmul11_1);
		I32x4 tmp1 = I32x4_lshElements(clmul01_0, 2);
		a0 = AES_encodeBlock(a0, k[12]);
		a1 = AES_encodeBlock(a1, k[12]);

		I32x4 tmp3 = I32x4_rshElements(clmul01_0, 2);
		I32x4 t0 = I32x4_xor(clmul00_0, tmp1);
		a0 = AES_encodeBlock(a0, k[13]);
		a1 = AES_encodeBlock(a1, k[13]);

		I32x4 t1 = I32x4_xor(clmul11_0, tmp3);
		I32x4 tmp0 = I32x4_lsh32(t0, 1);
		I32x4 tmp4 = I32x4_rsh32(t0, 31);
		I32x4 tmp2 = I32x4_lsh32(t1, 1);
		a0 = AES_encodeBlockLast(a0, k[14]);
		a1 = AES_encodeBlockLast(a1, k[14]);

		I32x4 tmp6 = I32x4_rsh32(t1, 31);
		I32x4 tmp7 = I32x4_rshElements(tmp4, 3);
		tmp6 = I32x4_lshElements(tmp6, 1);
		I32x4 tmp5 = I32x4_lshElements(tmp4, 1);

		tmp0 = I32x4_or(tmp0, tmp5);
		tmp2 = I32x4_or(tmp2, tmp6);
		tmp5 = I32x4_lsh32(tmp0, 31);
		tmp6 = I32x4_lsh32(tmp0, 30);
		tmp4 = I32x4_or(tmp2, tmp7);
		tmp7 = I32x4_lsh32(tmp0, 25);

		tmp5 = I32x4_xor(tmp5, tmp6);
		tmp5 = I32x4_xor(tmp5, tmp7);

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

		a0 = AES_encodeBlockLast(a0, k[10]);
		a1 = AES_encodeBlockLast(a1, k[10]);

		clmul01_0 = I32x4_xor(clmul01_0, clmul01_1);
		clmul00_0 = I32x4_xor(clmul00_0, clmul00_1);
		clmul11_0 = I32x4_xor(clmul11_0, clmul11_1);
		
		b = AESEncryptionContext_ghashReduceClMul(clmul00_0, clmul01_0, clmul11_0);
	}

	I32x4x3 abc = { a0, a1, b };
	return abc;
}

__forceinline__ static I32x4x5 AESEncryptionContext_blockHashAndGhash4(
	I32x4 a0,
	I32x4 a1,
	I32x4 a2,
	I32x4 a3,
	I32x4 H,
	I32x4 H2,
	I32x4 H3,
	I32x4 H4,
	I32x4 b0,
	I32x4 b1,
	I32x4 b2,
	I32x4 b3,
	I32x4 tag,
	const I32x4 *restrict k,
	const EBufferEncryptionType type
) {
	//Manual interleaving because you can't trust MSVC to generate proper code unfortunately.

	b0 = I32x4_xor(b0, tag);

	a0 = I32x4_xor(a0, k[0]);
	a1 = I32x4_xor(a1, k[0]);
	a2 = I32x4_xor(a2, k[0]);
	a3 = I32x4_xor(a3, k[0]);

	b0 = I32x4_swapEndianness(b0);
	b1 = I32x4_swapEndianness(b1);
	b2 = I32x4_swapEndianness(b2);
	b3 = I32x4_swapEndianness(b3);

	a0 = AES_encodeBlock(a0, k[1]);
	a1 = AES_encodeBlock(a1, k[1]);

	I32x4 clmul01_0 = I32x4_clmul64(b0, H4, 0x01);
	I32x4 clmul10_0 = I32x4_clmul64(b0, H4, 0x10);
	clmul01_0 = I32x4_xor(clmul01_0, clmul10_0);

	a2 = AES_encodeBlock(a2, k[1]);
	a3 = AES_encodeBlock(a3, k[1]);

	I32x4 clmul01_1 = I32x4_clmul64(b1, H3, 0x01);
	I32x4 clmul10_1 = I32x4_clmul64(b1, H3, 0x10);
	clmul01_1 = I32x4_xor(clmul01_1, clmul10_1);

	a0 = AES_encodeBlock(a0, k[2]);
	a1 = AES_encodeBlock(a1, k[2]);

	I32x4 clmul01_2 = I32x4_clmul64(b2, H2, 0x01);
	I32x4 clmul10_2 = I32x4_clmul64(b2, H2, 0x10);
	clmul01_2 = I32x4_xor(clmul01_2, clmul10_2);

	a2 = AES_encodeBlock(a2, k[2]);
	a3 = AES_encodeBlock(a3, k[2]);

	I32x4 clmul01_3 = I32x4_clmul64(b3, H, 0x01);
	I32x4 clmul10_3 = I32x4_clmul64(b3, H, 0x10);
	clmul01_3 = I32x4_xor(clmul01_3, clmul10_3);

	a0 = AES_encodeBlock(a0, k[3]);
	a1 = AES_encodeBlock(a1, k[3]);

	I32x4 clmul00_0 = I32x4_clmul64(b0, H4, 0x00);
	I32x4 clmul00_1 = I32x4_clmul64(b1, H3, 0x00);

	a2 = AES_encodeBlock(a2, k[3]);
	a3 = AES_encodeBlock(a3, k[3]);

	I32x4 clmul00_2 = I32x4_clmul64(b2, H2, 0x00);
	I32x4 clmul00_3 = I32x4_clmul64(b3, H, 0x00);

	a0 = AES_encodeBlock(a0, k[4]);
	a1 = AES_encodeBlock(a1, k[4]);

	I32x4 clmul11_0 = I32x4_clmul64(b0, H4, 0x11);
	I32x4 clmul11_1 = I32x4_clmul64(b1, H3, 0x11);

	a2 = AES_encodeBlock(a2, k[4]);
	a3 = AES_encodeBlock(a3, k[4]);

	I32x4 clmul11_2 = I32x4_clmul64(b2, H2, 0x11);
	I32x4 clmul11_3 = I32x4_clmul64(b3, H, 0x11);

	a0 = AES_encodeBlock(a0, k[5]);
	a1 = AES_encodeBlock(a1, k[5]);
	a2 = AES_encodeBlock(a2, k[5]);
	a3 = AES_encodeBlock(a3, k[5]);

	a0 = AES_encodeBlock(a0, k[6]);
	a1 = AES_encodeBlock(a1, k[6]);
	a2 = AES_encodeBlock(a2, k[6]);
	a3 = AES_encodeBlock(a3, k[6]);

	a0 = AES_encodeBlock(a0, k[7]);
	a1 = AES_encodeBlock(a1, k[7]);
	a2 = AES_encodeBlock(a2, k[7]);
	a3 = AES_encodeBlock(a3, k[7]);

	a0 = AES_encodeBlock(a0, k[8]);
	a1 = AES_encodeBlock(a1, k[8]);
	a2 = AES_encodeBlock(a2, k[8]);
	a3 = AES_encodeBlock(a3, k[8]);

	a0 = AES_encodeBlock(a0, k[9]);
	a1 = AES_encodeBlock(a1, k[9]);
	a2 = AES_encodeBlock(a2, k[9]);
	a3 = AES_encodeBlock(a3, k[9]);

	I32x4 b;

	if (type == EBufferEncryptionType_AES256GCM) {

		a0 = AES_encodeBlock(a0, k[10]);
		a1 = AES_encodeBlock(a1, k[10]);
		a2 = AES_encodeBlock(a2, k[10]);
		a3 = AES_encodeBlock(a3, k[10]);

		clmul01_0 = I32x4_xor(clmul01_0, clmul01_1);
		clmul00_0 = I32x4_xor(clmul00_0, clmul00_1);
		clmul01_2 = I32x4_xor(clmul01_2, clmul01_3);
		clmul00_2 = I32x4_xor(clmul00_2, clmul00_3);

		a0 = AES_encodeBlock(a0, k[11]);
		a1 = AES_encodeBlock(a1, k[11]);
		a2 = AES_encodeBlock(a2, k[11]);
		a3 = AES_encodeBlock(a3, k[11]);

		clmul01_0 = I32x4_xor(clmul01_0, clmul01_2);
		clmul00_0 = I32x4_xor(clmul00_0, clmul00_2);
		clmul11_0 = I32x4_xor(clmul11_0, clmul11_1);
		clmul11_2 = I32x4_xor(clmul11_2, clmul11_3);

		I32x4 tmp1 = I32x4_lshElements(clmul01_0, 2);
		a0 = AES_encodeBlock(a0, k[12]);
		a1 = AES_encodeBlock(a1, k[12]);

		clmul11_0 = I32x4_xor(clmul11_0, clmul11_2);
		I32x4 tmp3 = I32x4_rshElements(clmul01_0, 2);
		I32x4 t0 = I32x4_xor(clmul00_0, tmp1);
		a2 = AES_encodeBlock(a2, k[12]);
		a3 = AES_encodeBlock(a3, k[12]);

		I32x4 t1 = I32x4_xor(clmul11_0, tmp3);
		I32x4 tmp0 = I32x4_lsh32(t0, 1);
		I32x4 tmp4 = I32x4_rsh32(t0, 31);
		I32x4 tmp2 = I32x4_lsh32(t1, 1);
		a0 = AES_encodeBlock(a0, k[13]);
		a1 = AES_encodeBlock(a1, k[13]);

		I32x4 tmp6 = I32x4_rsh32(t1, 31);
		I32x4 tmp7 = I32x4_rshElements(tmp4, 3);
		tmp6 = I32x4_lshElements(tmp6, 1);
		I32x4 tmp5 = I32x4_lshElements(tmp4, 1);
		a2 = AES_encodeBlock(a2, k[13]);
		a3 = AES_encodeBlock(a3, k[13]);

		tmp0 = I32x4_or(tmp0, tmp5);
		tmp2 = I32x4_or(tmp2, tmp6);
		tmp5 = I32x4_lsh32(tmp0, 31);
		tmp6 = I32x4_lsh32(tmp0, 30);
		tmp4 = I32x4_or(tmp2, tmp7);
		tmp7 = I32x4_lsh32(tmp0, 25);

		a0 = AES_encodeBlockLast(a0, k[14]);
		a1 = AES_encodeBlockLast(a1, k[14]);

		tmp5 = I32x4_xor(tmp5, tmp6);
		tmp5 = I32x4_xor(tmp5, tmp7);

		a2 = AES_encodeBlockLast(a2, k[14]);
		a3 = AES_encodeBlockLast(a3, k[14]);

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

		a0 = AES_encodeBlockLast(a0, k[10]);
		a1 = AES_encodeBlockLast(a1, k[10]);
		a2 = AES_encodeBlockLast(a2, k[10]);
		a3 = AES_encodeBlockLast(a3, k[10]);

		clmul01_0 = I32x4_xor(clmul01_0, clmul01_1);
		clmul00_0 = I32x4_xor(clmul00_0, clmul00_1);
		clmul11_0 = I32x4_xor(clmul11_0, clmul11_1);

		clmul01_2 = I32x4_xor(clmul01_2, clmul01_3);
		clmul00_2 = I32x4_xor(clmul00_2, clmul00_3);
		clmul11_2 = I32x4_xor(clmul11_2, clmul11_3);

		clmul01_0 = I32x4_xor(clmul01_0, clmul01_2);
		clmul00_0 = I32x4_xor(clmul00_0, clmul00_2);
		clmul11_0 = I32x4_xor(clmul11_0, clmul11_2);
		
		b = AESEncryptionContext_ghashReduceClMul(clmul00_0, clmul01_0, clmul11_0);
	}

	I32x4x5 abcde = { a0, a1, a2, a3, b };
	return abcde;
}

#ifdef HAS_CLMUL64x2
	void AESEncryptionContext_ghashN2(I32x4 *restrict a, const I32x4 *restrict H, U8 N, I32x4 *restrict clmuls);
	void AESEncryptionContext_blocks8(
		U32 *restrict counterForIv,
		I32x4 *restrict *restrict next,
		I32x4 *restrict end,
		I32x4 iv,
		I32x4 *restrict H,
		I32x4 *restrict k,
		I32x4 *restrict tag,
		const Bool isEncrypt,
		const EBufferEncryptionType encryptionType
	);
	void AESEncryptionContext_ghashTable2(I32x4 *restrict H);
	void AESEncryptionContext_ghashTable2_4(I32x4 *restrict H, I32x4 H2, I32x4 H3, I32x4 H4);
#endif

#ifdef HAS_CLMUL64x4
	void AESEncryptionContext_ghashN4(I32x4 *restrict a, const I32x4 *restrict H, U8 N, I32x4 *restrict clmuls);
	void AESEncryptionContext_blocks16(
		U32 *restrict counterForIv,
		I32x4 *restrict *restrict next,
		I32x4 *restrict end,
		I32x4 iv,
		I32x4 *restrict H,
		I32x4 *restrict k,
		I32x4 *restrict tag,
		const Bool isEncrypt,
		const EBufferEncryptionType encryptionType
	);
	void AESEncryptionContext_ghashTable4(I32x4 *restrict H, I32x4 H2, I32x4 H3, I32x4 H4);
#endif

static inline I32x4 AESEncryptionContext_ghashN(I32x4 *restrict a, const I32x4 *restrict H, U8 N, U8 use256Or512) {

	(void)use256Or512;

	I32x4 clmuls[3];

	#ifdef HAS_CLMUL64x4
		//cryptoState >= 3: has cmul64x2 + clmul64x4
		if(cryptoState >= 3 && N >= 4 && (use256Or512 & 2))
			AESEncryptionContext_ghashN4(a, H, N, clmuls);
		else
	#endif

	#ifdef HAS_CLMUL64x2
	
		//cryptoState >= 2: has AVX2, AVX512VL, VAES, VPCLMULQDQ
		if(cryptoState >= 2 && N >= 2 && (use256Or512 & 1))
			AESEncryptionContext_ghashN2(a, H, N, clmuls);

		else

	#endif

	{
		for (U32 i = 0; i < N; ++i)
			a[i] = I32x4_swapEndianness(a[i]);

		I32x4 clmul00[16];
		I32x4 clmul11[16];

		//Not using fused here because it seems the dependency chain will be too short otherwise to hide latency
		//Maybe after merging ghash and aes?
		I32x4 clmul01[16];
		I32x4 clmul10[16];

		//Looks a bit odd, but it's to allow multiple clmuls to run in parallel.
		//Then, it'll be xored later.
		//If we do clmulNN[i] ^= it creates a dependency, stalling everything.

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

		clmuls[0] = clmul00[0];
		clmuls[1] = I32x4_xor(clmul01[0], clmul10[0]);
		clmuls[2] = clmul11[0];
	}

	return AESEncryptionContext_ghashReduceClMul(clmuls[0], clmuls[1], clmuls[2]);
}

static inline void AESEncryptionContext_updateTagN(
	AESEncryptionContext *restrict ctx, const I32x4 *restrict CTi, const U8 N, U8 use256Or512
) {
	
	I32x4 v[16];

	v[0] = I32x4_xor(CTi[0], ctx->tag);

	for (U8 i = 1; i < N; ++i)
		v[i] = CTi[i];

	ctx->tag = AESEncryptionContext_ghashN(v, ctx->H, N, use256Or512);
}

//Safe fetch a block (even if <16 bytes are left)
static inline I32x4 AESEncryptionContext_fetchBlockTail(const void *restrict dat, const U64 leftOver) {
	I32x4 v = I32x4_zero();
	Buffer_memcpy(Buffer_createRef(&v, sizeof(v)), Buffer_createRefConst(dat, leftOver));
	return v;
}

static inline void AESEncryptionContext_updateTagTail(AESEncryptionContext *restrict ctx, I32x4 CTi, const U8 leftOver) {
	Buffer_unsetAllBits(Buffer_createRef(((U8*)&CTi + leftOver), 16 - leftOver), NULL);
	CTi = I32x4_xor(CTi, ctx->tag);
	ctx->tag = AESEncryptionContext_ghashN(&CTi, ctx->H, 1, 0);
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

	//TODO: Re-enable this block size only for AAD (Need to update the size in ctx->H and AESEncryptionContext_updateTagN)
	//        Would also need to update the switch case that points to Buffer_aesExpertExpandHash,
	//        AESEncryptionContext_ghashN2 and AESEncryptionContext_ghashN4
	//This giant block size actually helps because we have no other work to schedule unlike a real AES+GHASH pipeline.

	/*
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
	}*/

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

static inline void Buffer_aesExpertExpandHash(AESEncryptionContext *restrict ctx, U8 blockSizeMax, U8 use256Or512) {

	(void) use256Or512;

	//Compute H^2

	I32x4 H = I32x4_swapEndianness(ctx->H[0]);
	ctx->H[1] = AESEncryptionContext_ghashN(&H, ctx->H, 1, 0);
	ctx->H[1] = I32x4_swapEndianness(ctx->H[1]);

	if (blockSizeMax == 2)
		return;

	//Compute H^3 and H^4 which are computed by doing:
	//ghashParallel2((H, H2), (H2, H2)) = (H3, H4)
	//This is !NOT! the same as ghashN, as that reduces multiple blocks into one, where here we want to keep them parallel.
	
	#ifdef HAS_CLMUL64x2
		if (cryptoState >= 2 && use256Or512)
			AESEncryptionContext_ghashTable2(ctx->H);
		else
	#endif

	{
		I32x4 clmul00[2];
		I32x4 clmul01[2];
		I32x4 clmul10[2];
		I32x4 clmul11[2];

		I32x4 H1 = ctx->H[0];
		I32x4 H2 = ctx->H[1];

		clmul01[0] = I32x4_clmul64(H1, H2, 0x01);
		clmul01[1] = I32x4_clmul64(H2, H2, 0x01);
		clmul10[0] = I32x4_clmul64(H1, H2, 0x10);
		clmul10[1] = I32x4_clmul64(H2, H2, 0x10);

		clmul00[0] = I32x4_clmul64(H1, H2, 0x00);
		clmul00[1] = I32x4_clmul64(H2, H2, 0x00);
		clmul11[0] = I32x4_clmul64(H1, H2, 0x11);
		clmul11[1] = I32x4_clmul64(H2, H2, 0x11);

		clmul01[0] = I32x4_xor(clmul01[0], clmul10[0]);
		clmul01[1] = I32x4_xor(clmul01[1], clmul10[1]);

		ctx->H[2] = I32x4_swapEndianness(AESEncryptionContext_ghashReduceClMul(clmul00[0], clmul01[0], clmul11[0]));
		ctx->H[3] = I32x4_swapEndianness(AESEncryptionContext_ghashReduceClMul(clmul00[1], clmul01[1], clmul11[1]));
	}
				
	if (blockSizeMax == 4)
		return;

	//Compute H^5, H^6, H^7, H^8 which are computed using:
	//ghashParallel4((H2, H3, H3, H4), (H3, H3, H4, H4)) = (H5, H6, H7, H8)
	//ghashParallel4((H4, H5, H5, H6), (H5, H5, H6, H6)) = (H9, H10, H11, H12)
	//ghashParallel4((H6, H7, H7, H8), (H7, H7, H8, H8)) = (H13, H14, H15, H16)
	//It follows the same pattern for all others, so this loop is easy.

	for (U8 i = 4, j = 1; i < blockSizeMax; i += 4, j += 2) {

		I32x4 H2 = ctx->H[j + 0];        //[1] = H2, [3] = H4, [5] = H6
		I32x4 H3 = ctx->H[j + 1];        //[2] = H3, [4] = H5, [6] = H7
		I32x4 H4 = ctx->H[j + 2];        //[3] = H4, [5] = H6, [7] = H8

		//This is one of the only ones that needs rshElements/lshElements which needs AVX512BW
		//TODO: This seems to currently underperform the 256-bit version.

		#if defined(HAS_CLMUL64x4) && false
			if (cryptoState >= 3 && (use256Or512 & 2))
				AESEncryptionContext_ghashTable4(&ctx->H[i], H2, H3, H4);
			else
		#endif

		//Two 256-bit clmuls
		
		#ifdef HAS_CLMUL64x2
			if (cryptoState >= 2 && use256Or512)
				AESEncryptionContext_ghashTable2_4(&ctx->H[i], H2, H3, H4);
			else
		#endif

		//Four 128-bit muls

		{
			I32x4 clmul00[4];
			I32x4 clmul01[4];
			I32x4 clmul10[4];
			I32x4 clmul11[4];

			clmul01[0] = I32x4_clmul64(H2, H3, 0x01);
			clmul10[0] = I32x4_clmul64(H2, H3, 0x10);
			clmul01[1] = I32x4_clmul64(H3, H3, 0x01);
			clmul10[1] = I32x4_clmul64(H3, H3, 0x10);
			clmul01[2] = I32x4_clmul64(H3, H4, 0x01);
			clmul10[2] = I32x4_clmul64(H3, H4, 0x10);
			clmul01[3] = I32x4_clmul64(H4, H4, 0x01);
			clmul10[3] = I32x4_clmul64(H4, H4, 0x10);

			clmul00[0] = I32x4_clmul64(H2, H3, 0x00);
			clmul11[0] = I32x4_clmul64(H2, H3, 0x11);
			clmul00[1] = I32x4_clmul64(H3, H3, 0x00);
			clmul11[1] = I32x4_clmul64(H3, H3, 0x11);
			clmul00[2] = I32x4_clmul64(H3, H4, 0x00);
			clmul11[2] = I32x4_clmul64(H3, H4, 0x11);
			clmul00[3] = I32x4_clmul64(H4, H4, 0x00);
			clmul11[3] = I32x4_clmul64(H4, H4, 0x11);

			clmul01[0] = I32x4_xor(clmul01[0], clmul10[0]);
			clmul01[1] = I32x4_xor(clmul01[1], clmul10[1]);
			clmul01[2] = I32x4_xor(clmul01[2], clmul10[2]);
			clmul01[3] = I32x4_xor(clmul01[3], clmul10[3]);

			ctx->H[i + 0] = I32x4_swapEndianness(AESEncryptionContext_ghashReduceClMul(clmul00[0], clmul01[0], clmul11[0]));
			ctx->H[i + 1] = I32x4_swapEndianness(AESEncryptionContext_ghashReduceClMul(clmul00[1], clmul01[1], clmul11[1]));
			ctx->H[i + 2] = I32x4_swapEndianness(AESEncryptionContext_ghashReduceClMul(clmul00[2], clmul01[2], clmul11[2]));
			ctx->H[i + 3] = I32x4_swapEndianness(AESEncryptionContext_ghashReduceClMul(clmul00[3], clmul01[3], clmul11[3]));
		}
	}
}

Bool Buffer_aesExpertCreate(
	I32x4 iv,
	EBufferEncryptionType type,
	AESEncryptionKey key,
	I64 streamSizeHint,
	U64 oneTimeHint,
	U8 use256Or512Override,
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

	//Here we detect what block size we should use and if 256-bit or 512-bit should be used if available.

	U8 blockSize = 16;
	Bool detect = false;

	U8 use256Or512Real = use256Or512Override;

	#if _SIMD != SIMD_SSE
		use256Or512Real = 0;
	#endif

	switch (streamSizeHint) {

		case -16:
		case -8:
		case -4:
		case -2:
		case -1:
			blockSize = (U8)-streamSizeHint;
			break;

		case 0:
		default:

			if(streamSizeHint < 0)
				retError(clean, Error_invalidEnum(
					1, (U64)-streamSizeHint, 16,
					"Buffer_aesExpertCreate()::blockSizeHint must be -16, -8, -4, -2, -1 or a positive number"
				));

			//We'll try to find the optimal size, this is mostly just the largest batch size available
			// but might need certain crypto flags to be set properly.
			detect = true;

			//As for the stream size hint, we always assume we want to do the most blocks we can as it has no negative effect

			break;
	}

	if (cryptoState <= 1 || !use256Or512Real) {
		blockSize = U8_min(blockSize, 4);
		use256Or512Real = 0;
	}

	else if (cryptoState <= 2 || use256Or512Real < 2) {
		blockSize = U8_min(blockSize, 8);
		use256Or512Real &= 1;
	}

	else {
		blockSize = U8_min(blockSize, 16);
		use256Or512Real &= 3;
	}

	//oneTimeHint to determine efficient blockSize from what's available.
	//Basically we try to minimize all of the setup here, from expandKey, blockHashes, ghashes, etc.
	//NOTE: This needs to be kept up to date probably.

	if (detect && streamSizeHint >= 0 && oneTimeHint) {

		if (oneTimeHint <= 128)        //Frequency penality for using 256-bit or 512-bit vectors
			use256Or512Real = 0;

		//Here are the optimal sizes:
		//NEO: 2, 2, 2, 2, 4...
		//
		//SSE: 1, 1, 2, 2, 4...
		//256: 1, 1, 2, 2, 4, 8...
		//512: 1, 1, 2, 2, 4, 8, 8, 16...

		blockSize = oneTimeHint <= 32 ? 1 : (oneTimeHint <= 128 ? 2 : 4);

		if (!use256Or512Real) {
			#if _SIMD == SIMD_NEON
				blockSize = oneTimeHint <= 128 ? 2 : 4;
			#endif
		}

		else if(use256Or512Real & 2)
			blockSize = oneTimeHint <= 256 ? blockSize : (oneTimeHint <= 1024 ? 8 : 16);

		else if(use256Or512Real & 1)
			blockSize = oneTimeHint <= 256 ? blockSize : 8;
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
		case 2:        Buffer_aesExpertExpandHash(ctx,  2, use256Or512Real);    break;
		case 4:        Buffer_aesExpertExpandHash(ctx,  4, use256Or512Real);    break;
		case 8:        Buffer_aesExpertExpandHash(ctx,  8, use256Or512Real);    break;
		case 16:    Buffer_aesExpertExpandHash(ctx, 16, use256Or512Real);    break;
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

	if (encrypt->target && Buffer_isConstRef(*encrypt->target))
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

	const U64 targetLen = !encrypt->target ? 0 : Buffer_length(*encrypt->target);

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
		*encrypt->constDecrypt.iv,
		encrypt->type,
		key,
		0, blockHint,
		U8_MAX,
		blockSize,
		use256Or512,
		ctx,
		e_rr
	));

	if (encrypt->additionalData)
		switch (*blockSize) {
			case 1:        Buffer_aesExpertUpdateAADFast(ctx, *encrypt->additionalData,   1, *use256Or512);    break;
			case 2:        Buffer_aesExpertUpdateAADFast(ctx, *encrypt->additionalData,   2, *use256Or512);    break;
			case 4:        Buffer_aesExpertUpdateAADFast(ctx, *encrypt->additionalData,   4, *use256Or512);    break;
			case 8:        Buffer_aesExpertUpdateAADFast(ctx, *encrypt->additionalData,   8, *use256Or512);    break;
			case 16:    Buffer_aesExpertUpdateAADFast(ctx, *encrypt->additionalData,  16, *use256Or512);    break;
		}

clean:
	Buffer_clearAllSecure(Buffer_createRef(&key, sizeof(key)));
	return s_uccess;
}

typedef union AESEncryptionContextLengths {
	I32x4 vec;
	U64 arr[2];
} AESEncryptionContextLengths;

//This ensures no expanded key, iv or anything else is leaked on the stack,
// which might be possible to obtain after execution through for example a buffer overflow.
static inline void AESEncryptionContext_clear(AESEncryptionContext *restrict ctx) {
	Buffer_clearAllSecure(Buffer_createRef(ctx->key, sizeof(ctx->key)));
	Buffer_clearAllSecure(Buffer_createRef(ctx->H, sizeof(ctx->H)));
	Buffer_clearAllSecure(Buffer_createRef(&ctx->iv, sizeof(ctx->iv)));
	Buffer_clearAllSecure(Buffer_createRef(&ctx->tag, sizeof(ctx->tag)));
	Buffer_clearAllSecure(Buffer_createRef(&ctx->EKY0, sizeof(ctx->EKY0)));
	ctx->encryptionType = 0;        //Not important enough to securely clear.
}

Bool Buffer_aesExpertFinalize(AESEncryptionContext *restrict ctx, U64 aadLen, U64 dataLen, I32x4 expectTag) {

	//Add length of inputs into the message too (lengths are in bits)

	AESEncryptionContextLengths lengths = { 0 };

	lengths.arr[0] = U64_swapEndianness(aadLen << 3);
	lengths.arr[1] = U64_swapEndianness(dataLen << 3);

	I32x4 tag = I32x4_xor(ctx->tag, lengths.vec);
	ctx->tag = AESEncryptionContext_ghashN(&tag, ctx->H, 1, 0);

	//Finish up by adding the iv into the key (this already has blockId 1 in it)

	ctx->tag = I32x4_xor(ctx->tag, ctx->EKY0);

	tag = ctx->tag;
	AESEncryptionContext_clear(ctx);
	ctx->tag = tag;

	return I32x4_eq4(tag, expectTag);
}

__forceinline__ static void AESEncryptionContext_handleBlocks(
	AESEncryptionContext *restrict ctx,
	U8 *restrict targetPtr,
	U64 targetLen,
	Bool isEncrypt,
	U32 offsetInBlocks,
	U8 blockSizeMax,
	U8 use256Or512
) {

	(void)use256Or512;

	if (!targetLen)
		return;

	I32x4 *restrict next = (I32x4 *restrict)targetPtr;
	I32x4 *restrict end = (I32x4 *restrict)(targetPtr + targetLen);        //Can be misaligned

	U32 counterForIv = offsetInBlocks + 2;

	I32x4 iv = ctx->iv;

	//Batch 16

	#if defined(HAS_CLMUL64x4) && defined(HAS_AESx4)
		if (blockSizeMax > 8 && (use256Or512 & 2) && next + 16 <= end)
			AESEncryptionContext_blocks16(&counterForIv, &next, end, iv, ctx->H, ctx->key, &ctx->tag, isEncrypt, ctx->encryptionType);
	#endif

	if (next >= end)
		return;

	//Batch 8

	#if defined(HAS_CLMUL64x2) && defined(HAS_AESx2)
		if (blockSizeMax > 4 && (use256Or512 & 1) && next + 8 <= end)
			AESEncryptionContext_blocks8(&counterForIv, &next, end, iv, ctx->H, ctx->key, &ctx->tag, isEncrypt, ctx->encryptionType);
	#endif

	if (next >= end)
		return;
	
	//Batch 4
	
	if(blockSizeMax > 2 && next + 4 <= end) {

		//Prologue

		I32x4 prevState[4];
		I32x4 H = ctx->H[0];
		I32x4 H2 = ctx->H[1];
		I32x4 H3 = ctx->H[2];
		I32x4 H4 = ctx->H[3];

		//Don't waste time on a ghash that's not gonna be used
		//I manually unrolled this and optimized the instruction order, because MSVC really can't compile well at alll.

		{
			I32x4 ivi0 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counterForIv));
			I32x4 ivi1 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counterForIv + 1));
			I32x4 ivi2 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counterForIv + 2));
			I32x4 ivi3 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counterForIv + 3));
			counterForIv += 4;

			I32x4x4 ab = AESEncryptionContext_blockHash4(ivi0, ivi1, ivi2, ivi3, ctx->key, ctx->encryptionType);

			I32x4 a = I32x4_xor(ab.a, next[0]);
			I32x4 b = I32x4_xor(ab.b, next[1]);
			I32x4 c = I32x4_xor(ab.c, next[2]);
			I32x4 d = I32x4_xor(ab.d, next[3]);

			if (!isEncrypt) {            //Decryption, we use the input (ciphertext)
				prevState[0] = next[0];
				prevState[1] = next[1];
				prevState[2] = next[2];
				prevState[3] = next[3];
			}

			else {                        //Encryption, we use the output (ciphertext)
				prevState[0] = a;
				prevState[1] = b;
				prevState[2] = c;
				prevState[3] = d;
			}

			next[0] = a;
			next[1] = b;
			next[2] = c;
			next[3] = d;
			next += 4;
		}

		//Contents

		while (next + 4 <= end) {

			I32x4 ivi0 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counterForIv));
			I32x4 ivi1 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counterForIv + 1));
			I32x4 ivi2 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counterForIv + 2));
			I32x4 ivi3 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counterForIv + 3));
			counterForIv += 4;

			I32x4x5 ab = AESEncryptionContext_blockHashAndGhash4(
				ivi0, ivi1, ivi2, ivi3,
				H, H2, H3, H4,
				prevState[0], prevState[1], prevState[2], prevState[3],
				ctx->tag,
				ctx->key,
				ctx->encryptionType
			);

			ctx->tag = ab.e;

			I32x4 a = I32x4_xor(ab.a, next[0]);
			I32x4 b = I32x4_xor(ab.b, next[1]);
			I32x4 c = I32x4_xor(ab.c, next[2]);
			I32x4 d = I32x4_xor(ab.d, next[3]);

			if (!isEncrypt) {                //Decryption, we use the input (ciphertext)
				prevState[0] = next[0];
				prevState[1] = next[1];
				prevState[2] = next[2];
				prevState[3] = next[3];
			}

			else {                            //Encryption, we use the output (ciphertext)
				prevState[0] = a;
				prevState[1] = b;
				prevState[2] = c;
				prevState[3] = d;
			}

			next[0] = a;
			next[1] = b;
			next[2] = c;
			next[3] = d;
			next += 4;
		}

		//Epilogue

		{
			I32x4 a = I32x4_xor(prevState[0], ctx->tag);
			I32x4 b = prevState[1];
			I32x4 c = prevState[2];
			I32x4 d = prevState[3];

			a = I32x4_swapEndianness(a);
			b = I32x4_swapEndianness(b);
			c = I32x4_swapEndianness(c);
			d = I32x4_swapEndianness(d);

			I32x4 clmul01a = I32x4_clmul64(a, H4, 0x01);
			I32x4 clmul10a = I32x4_clmul64(a, H4, 0x10);
			clmul01a = I32x4_xor(clmul01a, clmul10a);

			I32x4 clmul01b = I32x4_clmul64(b, H3, 0x01);
			I32x4 clmul10b = I32x4_clmul64(b, H3, 0x10);
			clmul01b = I32x4_xor(clmul01b, clmul10b);

			I32x4 clmul01c = I32x4_clmul64(c, H2, 0x01);
			I32x4 clmul10c = I32x4_clmul64(c, H2, 0x10);
			clmul01c = I32x4_xor(clmul01c, clmul10c);

			I32x4 clmul01d = I32x4_clmul64(d, H, 0x01);
			I32x4 clmul10d = I32x4_clmul64(d, H, 0x10);
			clmul01d = I32x4_xor(clmul01d, clmul10d);

			I32x4 clmul00a = I32x4_clmul64(a, H4, 0x00);
			I32x4 clmul00b = I32x4_clmul64(b, H3, 0x00);
			I32x4 clmul00c = I32x4_clmul64(c, H2, 0x00);
			I32x4 clmul00d = I32x4_clmul64(d, H, 0x00);

			I32x4 clmul11a = I32x4_clmul64(a, H4, 0x11);
			I32x4 clmul11b = I32x4_clmul64(b, H3, 0x11);
			I32x4 clmul11c = I32x4_clmul64(c, H2, 0x11);
			I32x4 clmul11d = I32x4_clmul64(d, H, 0x11);

			clmul11a = I32x4_xor(clmul11a, clmul11b);
			clmul00a = I32x4_xor(clmul00a, clmul00b);
			clmul01a = I32x4_xor(clmul01a, clmul01b);

			clmul11c = I32x4_xor(clmul11c, clmul11d);
			clmul00c = I32x4_xor(clmul00c, clmul00d);
			clmul01c = I32x4_xor(clmul01c, clmul01d);

			clmul11a = I32x4_xor(clmul11a, clmul11c);
			clmul00a = I32x4_xor(clmul00a, clmul00c);
			clmul01a = I32x4_xor(clmul01a, clmul01c);

			ctx->tag = AESEncryptionContext_ghashReduceClMul(clmul00a, clmul01a, clmul11a);
		}
	}

	if (next >= end)
		return;
	
	//Batch 2
	
	if(blockSizeMax > 1 && next + 2 <= end) {

		//Prologue

		I32x4 prevState[2];
		I32x4 H = ctx->H[0];
		I32x4 H2 = ctx->H[1];

		//Don't waste time on a ghash that's not gonna be used
		//I manually unrolled this and optimized the instruction order, because MSVC really can't compile well at alll.

		{

			I32x4 ivi0 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counterForIv));
			I32x4 ivi1 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counterForIv + 1));
			counterForIv += 2;

			I32x4x2 ab = AESEncryptionContext_blockHash2(ivi0, ivi1, ctx->key, ctx->encryptionType);

			I32x4 a = I32x4_xor(ab.a, next[0]);
			I32x4 b = I32x4_xor(ab.b, next[1]);

			if (!isEncrypt) {            //Decryption, we use the input (ciphertext)
				prevState[0] = next[0];
				prevState[1] = next[1];
			}

			else {                        //Encryption, we use the output (ciphertext)
				prevState[0] = a;
				prevState[1] = b;
			}

			next[0] = a;
			next[1] = b;
			next += 2;
		}

		//Contents

		while (next + 2 <= end) {

			I32x4 ivi0 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counterForIv));
			I32x4 ivi1 = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counterForIv + 1));
			counterForIv += 2;

			I32x4x3 ab = AESEncryptionContext_blockHashAndGhash2(
				ivi0, ivi1,
				H, H2,
				prevState[0], prevState[1],
				ctx->tag,
				ctx->key,
				ctx->encryptionType
			);

			ctx->tag = ab.c;

			I32x4 a = I32x4_xor(ab.a, next[0]);
			I32x4 b = I32x4_xor(ab.b, next[1]);

			if (!isEncrypt) {                //Decryption, we use the input (ciphertext)
				prevState[0] = next[0];
				prevState[1] = next[1];
			}

			else {                            //Encryption, we use the output (ciphertext)
				prevState[0] = a;
				prevState[1] = b;
			}

			next[0] = a;
			next[1] = b;
			next += 2;
		}

		//Epilogue

		{
			I32x4 a = I32x4_xor(prevState[0], ctx->tag);
			I32x4 b = prevState[1];

			a = I32x4_swapEndianness(a);
			b = I32x4_swapEndianness(b);

			I32x4 clmul01a = I32x4_clmul64(a, H2, 0x01);
			I32x4 clmul10a = I32x4_clmul64(a, H2, 0x10);
			clmul01a = I32x4_xor(clmul01a, clmul10a);

			I32x4 clmul01b = I32x4_clmul64(b, H, 0x01);
			I32x4 clmul10b = I32x4_clmul64(b, H, 0x10);
			clmul01b = I32x4_xor(clmul01b, clmul10b);

			I32x4 clmul00b = I32x4_clmul64(b, H, 0x00);
			I32x4 clmul00a = I32x4_clmul64(a, H2, 0x00);
			I32x4 clmul11b = I32x4_clmul64(b, H, 0x11);
			I32x4 clmul11a = I32x4_clmul64(a, H2, 0x11);

			clmul11a = I32x4_xor(clmul11a, clmul11b);
			clmul00a = I32x4_xor(clmul00a, clmul00b);
			clmul01a = I32x4_xor(clmul01a, clmul01b);

			ctx->tag = AESEncryptionContext_ghashReduceClMul(clmul00a, clmul01a, clmul11a);
		}
	}

	if (next >= end)
		return;

	//Batch 1

	I32x4 H = ctx->H[0];
	I32x4 prevState = I32x4_zero();
	U8 prevBlock = 0;

	//Don't waste time on a ghash that's not gonna be used
	//I manually unrolled this and optimized the instruction order, because MSVC really can't compile well at alll.

	if (next + 1 <= end) {

		//Prologue

		I32x4 ivi = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counterForIv++));
		I32x4 a = AESEncryptionContext_blockHash(ivi, ctx->key, ctx->encryptionType);
		a = I32x4_xor(a, next[0]);

		if (!isEncrypt)                //Decryption, we use the input (ciphertext)
			prevState = next[0];

		else prevState = a;        //Encryption, we use the output (ciphertext)

		next[0] = a;
		++next;
		prevBlock = 1;

		//Contents

		while (next + 1 <= end) {

			ivi = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counterForIv++));
			I32x4x2 ab = AESEncryptionContext_blockHashAndGhash(ivi, H, prevState, ctx->tag, ctx->key, ctx->encryptionType);

			ctx->tag = ab.b;

			a = I32x4_xor(ab.a, next[0]);

			if (!isEncrypt)                    //Decryption, we use the input (ciphertext)
				prevState = next[0];

			else prevState = a;            //Encryption, we use the output (ciphertext)

			next[0] = a;
			++next;
			prevBlock = 1;
		}

		//(No need to handle ghash yet, still have the tail)
	}

	//Tail (needs slower copy)

	if (next != end) {

		U8 leftOver = (U8)(targetLen & 15);

		I32x4 a;
		I32x4 v;
		
		if (prevBlock) {

			I32x4 ivi = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counterForIv++));
			I32x4x2 ab = AESEncryptionContext_blockHashAndGhash(ivi, H, prevState, ctx->tag, ctx->key, ctx->encryptionType);

			ctx->tag = ab.b;
			a = ab.a;

			v = I32x4_zero();
			Buffer_memcpy(Buffer_createRef(&v, leftOver), Buffer_createRefConst(next, leftOver));

			a = I32x4_xor(a, v);

		} else {

			I32x4 ivi = I32x4_setWCopy(iv, (I32)U32_swapEndianness(counterForIv++));
			a = AESEncryptionContext_blockHash(ivi, ctx->key, ctx->encryptionType);

			Buffer_memcpy(Buffer_createRef(&v, leftOver), Buffer_createRefConst(next, leftOver));
			a = I32x4_xor(a, v);
		}

		if (!isEncrypt)                    //Decryption, we use the input (ciphertext)
			prevState = v;

		else prevState = a;            //Encryption, we use the output (ciphertext)

		Buffer_memcpy(Buffer_createRef(next, leftOver), Buffer_createRefConst(&a, leftOver));
		prevBlock = 1;

		Buffer_unsetAllBits(Buffer_createRef(((U8*)&prevState + leftOver), 16 - leftOver), NULL);
	}

	if (prevBlock) {
		I32x4 b = I32x4_xor(prevState, ctx->tag);
		b = I32x4_swapEndianness(b);
		I32x4 clmul01 = I32x4_clmul64(b, H, 0x01);
		I32x4 clmul10 = I32x4_clmul64(b, H, 0x10);
		I32x4 clmul00 = I32x4_clmul64(b, H, 0x00);
		I32x4 clmul11 = I32x4_clmul64(b, H, 0x11);
		clmul01 = I32x4_xor(clmul01, clmul10);
		ctx->tag = AESEncryptionContext_ghashReduceClMul(clmul00, clmul01, clmul11);
	}
}

static inline void Buffer_prefetch(Buffer data) {
	(void) data;
	#if _SIMD == SIMD_SSE
		if (data.ptr) {
			if (Buffer_length(data) > 256)
				_mm_prefetch((const char*)data.ptr, _MM_HINT_T0);
		}
	#endif
}

static inline void Buffer_aesExpertEncUpdateFast(
	AESEncryptionContext *restrict ctx, Buffer data, U32 offsetInBlocks, U8 blockSizeMax, U8 use256Or512
) {
	Buffer_prefetch(data);
	AESEncryptionContext_handleBlocks(
		ctx, data.ptrNonConst, Buffer_length(data), true, offsetInBlocks, blockSizeMax, use256Or512
	);
}

static inline void Buffer_aesExpertDecUpdateFast(
	AESEncryptionContext *restrict ctx, Buffer data, U32 offsetInBlocks, U8 blockSizeMax, U8 use256Or512
) {
	Buffer_prefetch(data);
	AESEncryptionContext_handleBlocks(
		ctx, data.ptrNonConst, Buffer_length(data), false, offsetInBlocks, blockSizeMax, use256Or512
	);
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
			case 1:        Buffer_aesExpertEncUpdateFast(&ctx, *encrypt->target, 0,   1, use256Or512);    break;
			case 2:        Buffer_aesExpertEncUpdateFast(&ctx, *encrypt->target, 0,   2, use256Or512);    break;
			case 4:        Buffer_aesExpertEncUpdateFast(&ctx, *encrypt->target, 0,   4, use256Or512);    break;
			case 8:        Buffer_aesExpertEncUpdateFast(&ctx, *encrypt->target, 0,   8, use256Or512);    break;
			case 16:    Buffer_aesExpertEncUpdateFast(&ctx, *encrypt->target, 0,  16, use256Or512);    break;
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
			case 1:        Buffer_aesExpertDecUpdateFast(&ctx, *decrypt->target, 0,   1, use256Or512);    break;
			case 2:        Buffer_aesExpertDecUpdateFast(&ctx, *decrypt->target, 0,   2, use256Or512);    break;
			case 4:        Buffer_aesExpertDecUpdateFast(&ctx, *decrypt->target, 0,   4, use256Or512);    break;
			case 8:        Buffer_aesExpertDecUpdateFast(&ctx, *decrypt->target, 0,   8, use256Or512);    break;
			case 16:    Buffer_aesExpertDecUpdateFast(&ctx, *decrypt->target, 0,  16, use256Or512);    break;
		}

	U64 aadLen = decrypt->additionalData ? Buffer_length(*decrypt->additionalData) : 0;
	U64 dataLen = decrypt->target ? Buffer_length(*decrypt->target) : 0;

	//Check if the tag is the same, if not, then it has been tempered with
	if(!Buffer_aesExpertFinalize(&ctx, aadLen, dataLen, *decrypt->constDecrypt.tag)) {

		ctx.tag = I32x4_zero();

		if(decrypt->target)
			Buffer_clearAllSecure(*decrypt->target);

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
