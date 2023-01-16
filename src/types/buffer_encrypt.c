#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/type_cast.h"
#include "math/vec.h"

//Explanation of algorithm; AES256 GCM + GMAC
//https://www.alexeyshmalko.com/20200319144641/ / https://www.youtube.com/watch?v=V2TlG3JbGp0
//https://www.youtube.com/watch?v=g_eY7JXOc8U
//
//The final algorithm is basically the following:
//
//- Init key using CSPRNG if not available
//- Init H: aes256(0, key)
//- Init GHASH table
//
//- Tag: 0
//- Foreach additional data block padded to 16-byte with 0s:
//	- tag = GHASH(tag XOR additional data block)
// 
//- IV (Initial vector) = Generate CSPRNG of 12-bytes
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

#if _SIMD == SIMD_SSE

	I32x4 AES_keyGenAssist(I32x4 a, U8 i) {

		if(i >= 8)
			return I32x4_zero();

		switch (i) {
			case 0:		return _mm_aeskeygenassist_si128(a, 0x00);
			case 1:		return _mm_aeskeygenassist_si128(a, 0x01);
			case 2:		return _mm_aeskeygenassist_si128(a, 0x02);
			case 3:		return _mm_aeskeygenassist_si128(a, 0x04);
			case 4:		return _mm_aeskeygenassist_si128(a, 0x08);
			case 5:		return _mm_aeskeygenassist_si128(a, 0x10);
			case 6:		return _mm_aeskeygenassist_si128(a, 0x20);
			default:	return _mm_aeskeygenassist_si128(a, 0x40);
		}
	}

	I32x4 AES_encodeBlock(I32x4 a, I32x4 b, Bool isLast) {
		return isLast ? _mm_aesenclast_si128(a, b) : _mm_aesenc_si128(a, b);
	}

#else

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

	inline U32 AES_subWord(U32 a) {

		U32 res = 0;

		for(U8 i = 0; i < 4; ++i)
			res |= (U32)(AES_SBOX[(a >> (i * 8)) & 0xFF]) << (i * 8);

		return res;
	}

	inline U32 AES_rotWord(U32 a) { 
		return (a >> 8) | (a << 24);
	}

	I32x4 AES_keyGenAssist(I32x4 a, U8 i) {

		if(i >= 8)
			return I32x4_zero();

		//https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_aeskeygenassist_si128&ig_expand=6746,293

		U32 x1 = I32x4_y(a);
		U32 x3 = I32x4_w(a);
		U32 rcon = i ? 1 << (i - 1) : 0;

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

	inline U8x4x4 U8x4x4_transpose(const U8x4x4 *r) {

		U8x4x4 t = *r;

		for(U8 i = 0; i < 4; ++i)
			for(U8 j = 0; j < 4; ++j)
				t.v[i][j] = r->v[j][i];

		return t;
	}

	inline I32x4 AES_shiftRows(I32x4 a) {

		U8x4x4 *ap = (U8x4x4*) &a;

		U8x4x4 res = *ap;

		for(U64 j = 0; j < 4; ++j)
			for(U64 i = 1; i < 4; ++i)
				res.v[j][i] = ap->v[(i + j) & 3][i];

		return *(I32x4*)&res;
	}

	inline I32x4 AES_subBytes(I32x4 a) {

		I32x4 res = a;
		U8 *ptr = (U8*)&res;

		for(U8 i = 0; i < 16; ++i)
			ptr[i] = AES_SBOX[ptr[i]];

		return res;
	}

	inline U8 AES_g2_8(U8 v, U8 mul) {
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

	inline I32x4 AES_mixColumns(I32x4 vvv) { 

		U8x4x4 v = *(U8x4x4*)&vvv;

		v = U8x4x4_transpose(&v);

		U8x4x4 r = { 0 };

		for(U8 i = 0; i < 4; ++i) 
			for(U8 j = 0; j < 4; ++j) {

				for(U8 k = 0; k < 4; ++k)
					r.v[j][i] ^= AES_g2_8(v.v[k][i], AES_MIX_COLUMN[j][k]);
			}

		r = U8x4x4_transpose(&r);

		return *(const I32x4*)&r;
	}

	I32x4 AES_encodeBlock(I32x4 a, I32x4 b, Bool isLast) {

		I32x4 t = AES_shiftRows(a);
		t = AES_subBytes(t);

		if(!isLast)
			t = AES_mixColumns(t);

		return I32x4_xor(t, b);
	}

#endif

//The context of important AES variables.
//And encrypting/decrypting blocks and verifying tags.
//These functions don't do any parameter checks since they're internal helper functions

typedef struct AESEncryptionContext {

	I32x4 key[15];

	I32x4 H;

	I32x4 ghashLut[17];

	I32x4 EKY0;

	I32x4 tag;

	I32x4 iv;

} AESEncryptionContext;

//Key expansion for AES256
//Implemented from the official intel AES-NI paper + Additional paper by S. Gueron appendix A
//https://link.springer.com/content/pdf/10.1007/978-3-642-03317-9_4.pdf
//https://www.samiam.org/key-schedule.html

inline I32x4 AESEncryptionContext_expandKeyN(I32x4 im1, I32x4 im2) {

	I32x4 im4 = im1;

	for(U8 i = 0; i < 3; ++i) {
		im4 = I32x4_lsh32(im4);
		im1 = I32x4_xor(im1, im4);
	}

	return I32x4_xor(im1, im2);
}

inline I32x4 AESEncryptionContext_expandKey1(I32x4 im1, I32x4 im2) {
	return AESEncryptionContext_expandKeyN(im1, I32x4_wwww(im2));
}

inline I32x4 AESEncryptionContext_expandKey2(I32x4 im1, I32x4 im3) {
	return AESEncryptionContext_expandKeyN(im3, I32x4_zzzz(AES_keyGenAssist(im1, 0)));
}

inline void AESEncryptionContext_expandKey(const U32 key[8], I32x4 k[15]) {

	I32x4 im1 = (k[0] = *(const I32x4*) key);
	I32x4 im3 = (k[1] = ((const I32x4*) key)[1]);

	for (U8 i = 0, j = 2; i < 7; ++i, j += 2) {

		k[j] = (im1 = AESEncryptionContext_expandKey1(im1, AES_keyGenAssist(im3, i + 1)));

		if(j + 1 < 15)
			k[j + 1] = (im3 = AESEncryptionContext_expandKey2(im1, im3));
	}
}

//Aes block encryption. Don't use this plainly, it's a part of the larger AES256-CTR algorithm

inline I32x4 AESEncryptionContext_blockHash(I32x4 block, const I32x4 k[15]) {

	block = I32x4_xor(block, k[0]);

	for(U8 i = 1; i < 14; ++i)				//AES256 uses 14 rounds
		block = AES_encodeBlock(block, k[i], false);

	return AES_encodeBlock(block, k[14], true);
}

#if _SIMD == SIMD_SSE

	inline void AESEncryptionContext_ghashPrepare(I32x4 H, I32x4 ghashLut[17]) { 
		ghashLut[0] = I32x4_swapEndianness(H);
	}

	//Refactored from https://www.intel.com/content/dam/develop/external/us/en/documents/clmul-wp-rev-2-02-2014-04-20.pdf

	inline I32x4 AESEncryptionContext_ghash(I32x4 a, const I32x4 ghashLut[17]) {

		a = I32x4_swapEndianness(a);
		I32x4 b = ghashLut[0];

		I32x4 tmp[8];

		tmp[0] = _mm_clmulepi64_si128(a, b, 0x00);

		tmp[3] = I32x4_xor(
			_mm_clmulepi64_si128(a, b, 0x10),
			_mm_clmulepi64_si128(a, b, 0x01)
		);

		tmp[2] = _mm_clmulepi64_si128(a, b, 0x11);

		tmp[1] = I32x4_lsh64(tmp[3]);
		tmp[3] = _mm_srli_si128(tmp[3], 8);

		for(U8 i = 0; i < 2; ++i) {
			I32x4 t = I32x4_xor(tmp[i << 1], tmp[(i << 1) + 1]);
			tmp[i << 1] = _mm_slli_epi32(t, 1);
			tmp[4 + (i << 1)] = _mm_srli_epi32(t, 31);
		}

		tmp[7] = _mm_srli_si128(tmp[4], 12);

		for(U8 i = 0; i < 2; ++i)
			tmp[6 - i] = I32x4_lsh32(tmp[6 - (i << 1)]);

		const U8 v0[3] = { 31, 30, 25 };

		for(U8 i = 0; i < 3; ++i) {
			tmp[i << 1] = I32x4_or(tmp[i ? 2 : 0], tmp[5 + i]);
			tmp[5 + i] = _mm_slli_epi32(tmp[0], v0[i]);
		}

		for(U8 i = 0; i < 2; ++i)
			tmp[5] = I32x4_xor(tmp[5], tmp[6 + i]);

		tmp[3] = _mm_srli_si128(tmp[5], 4);
		tmp[5] = I32x4_xor(tmp[0], I32x4_lsh96(tmp[5]));

		const U8 v1[3] = { 1, 2, 7 };
		
		for(U8 i = 0; i < 3; ++i)
			tmp[i] = _mm_srli_epi32(tmp[5], v1[i]);

		for(U8 i = 1; i < 6; ++i)
			tmp[0] = I32x4_xor(tmp[0], tmp[i]);

		return I32x4_swapEndianness(tmp[0]);
	}

#elif _SIMD == SIMD_NEON

	#error Neon currently unsupported for native CRC32C, AES256 or SHA256 operation

#else

	//inline void aesExpandKey(U32 key[8], I32x4 k[15]);
	//inline I32x4 aesBlock(I32x4 block, I32x4 k[15]);

	inline I32x4 AESEncryptionContext_rsh(I32x4 v, U8 shift) {

		U64 *a = (U64*) &v;
		U64 *b = a + 1;

		*a = (*a >> shift) | (*b << (64 - shift));
		*b >>= shift;

		return v;
	}

	//ghash computes the Galois field multiplication GF(2^128) with the current H (hash of AES256 encrypted zero block)
	//for AES256 GCM + GMAC

	//LUT creation from https://github.com/mko-x/SharedAES-GCM/blob/master/Sources/gcm.c#L207

	inline void AESEncryptionContext_ghashPrepare(I32x4 H, I32x4 ghashLut[17]) {

		H = I32x4_swapEndianness(H);

		ghashLut[16] = H;

		ghashLut[0] = I32x4_zero();		//0 = 0 in GF(2^128)
		ghashLut[8] = H;				//8 (0b1000) corresponds to 1 in GF (2^128)

		for (U8 i = 4; i > 0; i >>= 1) {

			I32x4 T = I32x4_create4(0, 0, 0, I32x4_x(H) & 1 ? 0xE1000000 : 0);
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

	const U16 GHASH_LAST4[16] = {
		0x0000, 0x1C20, 0x3840, 0x2460, 0x7080, 0x6CA0, 0x48C0, 0x54E0,
		0xE100, 0xFD20, 0xD940, 0xC560, 0x9180, 0x8DA0, 0xA9C0, 0xB5E0
	};

	inline I32x4 AESEncryptionContext_ghash(I32x4 aa, const I32x4 ghashLut[17]) {

		I32x4 zlZh = ghashLut[((U8*)&aa)[15] & 0xF];

		for (U8 i = 30; i != U8_MAX; --i) {

			U8 rem = (U8)I32x4_x(zlZh) & 0xF;
			U8 ind = (((U8*)&aa)[i / 2] >> (4 * (1 - (i & 1)))) & 0xF;

			zlZh = AESEncryptionContext_rsh(zlZh, 4);
			zlZh = I32x4_xor(zlZh, I32x4_create4(0, 0, 0, (U32)GHASH_LAST4[rem] << 16));
			zlZh = I32x4_xor(zlZh, ghashLut[ind]);
		}

		return I32x4_swapEndianness(zlZh);
	}

#endif

//Safe fetch a block (even if <16 bytes are left)

I32x4 AESEncryptionContext_fetchBlock(const I32x4 *dat, U64 leftOver) {

	//Avoid out of bounds read, by simply filling additional data by zero

	if (leftOver < sizeof(I32x4)) {

		I32x4 v = I32x4_zero();
		Buffer_copy(
			Buffer_createRef(&v, sizeof(v)),
			Buffer_createConstRef(dat, leftOver)
		);

		return v;
	}

	return *dat;
}

//Hash in the additional data
//This could be something like sender + receiver ip address
//This data could allow the dev to discard invalid packets for example
//And verify that this is the data the original message was signed with

I32x4 AESEncryptionContext_initTag(Buffer additionalData, const I32x4 ghashLut[17]) {

	I32x4 tag = I32x4_zero();
	U64 len = Buffer_length(additionalData);

	for (U64 i = 0, j = (len + 15) >> 4; i < j; ++i) {
		I32x4 ADi = AESEncryptionContext_fetchBlock((const I32x4*)additionalData.ptr + i, len - (i << 4));
		tag = AESEncryptionContext_ghash(I32x4_xor(tag, ADi), ghashLut);
	}

	return tag;
}

AESEncryptionContext AESEncryptionContext_create(const U32 realKey[8], I32x4 iv, Buffer additionalData) {

	AESEncryptionContext ctx;

	//Get key that's gonna be used for aes blocks

	AESEncryptionContext_expandKey(realKey, ctx.key);

	//Prepare ghash

	ctx.H = AESEncryptionContext_blockHash(I32x4_zero(), ctx.key);

	AESEncryptionContext_ghashPrepare(ctx.H, ctx.ghashLut);

	//Compute final tag xor

	I32x4 Y0 = iv;
	I32x4_setW(&Y0, I32_swapEndianness(1));

	ctx.iv = iv;
	ctx.EKY0 = AESEncryptionContext_blockHash(Y0, ctx.key);
	ctx.tag = AESEncryptionContext_initTag(additionalData, ctx.ghashLut);

	return ctx;
}

void AESEncryptionContext_finish(AESEncryptionContext *ctx, Buffer additionalData, Buffer target) {

	//Add length of inputs into the message too (lengths are in bits)

	I32x4 lengths = I32x4_zero();
	*(U64*)&lengths = U64_swapEndianness(Buffer_length(additionalData) << 3);
	*((U64*)&lengths + 1) = U64_swapEndianness(Buffer_length(target) << 3);

	ctx->tag = AESEncryptionContext_ghash(I32x4_xor(ctx->tag, lengths), ctx->ghashLut);

	//Finish up by adding the iv into the key (this is already has blockId 1 in it)

	ctx->tag = I32x4_xor(ctx->tag, ctx->EKY0);
}

void AESEncryptionContext_updateTag(AESEncryptionContext *ctx, I32x4 CTi) {
	ctx->tag = AESEncryptionContext_ghash(I32x4_xor(CTi, ctx->tag), ctx->ghashLut);
}

void AESEncryptionContext_storeBlock(I32x4 *io, U64 leftOver, I32x4 *v) {

	//A special property of unaligned blocks is that the bytes that are added as padding
	//shouldn't be stored and so they have to be zero-ed in CTi, otherwise the tag will mess up

	if (leftOver < sizeof(I32x4)) {

		Buffer_unsetAllBits(Buffer_createRef((U8*)v + leftOver, sizeof(I32x4) - leftOver));

		Buffer_copy(
			Buffer_createRef(io, leftOver),
			Buffer_createConstRef(v, leftOver)
		);
	}

	//Aligned blocks

	else *io = *v;
}

void AESEncryptionContext_processBlock(
	AESEncryptionContext *ctx, 
	I32x4 *io,
	U64 leftOver,
	U32 i,
	Bool updateTag
) {

	I32x4 v = AESEncryptionContext_fetchBlock(io, leftOver);

	//Encrypt IV + blockId to use to encrypt

	I32x4 ivi = ctx->iv;
	I32x4_setW(&ivi, I32_swapEndianness((U32)i + 2));

	v = I32x4_xor(v, AESEncryptionContext_blockHash(ivi, ctx->key));

	AESEncryptionContext_storeBlock(io, leftOver, &v);

	//Continue tag

	if(updateTag)
		AESEncryptionContext_updateTag(ctx, v);
}

void AESEncryptionContext_fetchAndUpdateTag(AESEncryptionContext *ctx, const I32x4 *data, U64 leftOver) {
	AESEncryptionContext_updateTag(ctx, AESEncryptionContext_fetchBlock(data, leftOver));
}

U64 EBufferEncryptionType_getAdditionalData(EBufferEncryptionType type) {
	switch (type) {
		case EBufferEncryptionType_AES256GCM:	return 16 + 12;
		default:								return 0;
	}
}

inline Error AESEncryptionContext_encrypt(
	Buffer target, 
	Buffer additionalData,
	EBufferEncryptionFlags flags,
	U32 realKey[8], 
	I32x4 *ivPtr,
	I32x4 *tag
) {

	//Since we have a 12-byte IV, we have a 4-byte block counter.
	//This block counter runs out in (4Gi - 3) * sizeof(Block) aka ~4Gi * 16 = ~64GiB.
	//When the IV block counter runs out it would basically repeat the same block xor pattern again.
	//-3 because we start at 2 since 1 is used at the end for verification (and 0 is skipped).

	if(Buffer_length(target) > (4 * GIBI - 3) * sizeof(I32x4))
		return Error_unsupportedOperation(0);

	//Generate iv & context

	I32x4_setW(ivPtr, 0);

	if(flags & EBufferEncryptionFlags_GenerateIv)
		Buffer_csprng(Buffer_createRef(ivPtr, 12));

	if(flags & EBufferEncryptionFlags_GenerateKey)
		Buffer_csprng(Buffer_createRef(realKey, sizeof(U32) * 8));

	AESEncryptionContext ctx = AESEncryptionContext_create(realKey, *ivPtr, additionalData);

	//Handle blocks
	//TODO: We might wanna multithread this if we ever get big enough data
	//		For now, we're dealing with small enough files that spinning up threads would be slower
	//		(Without a job system)

	U64 targetLen = Buffer_length(target);

	U32 j = (U32)((targetLen + 15) >> 4);

	for (U32 i = 0; i < j; ++i)
		AESEncryptionContext_processBlock(
			&ctx, 
			(I32x4*)target.ptr + i,
			targetLen - ((U64)i << 4),
			i,
			true
		);

	//Finish encryption by appending tag for authentication / verification that the data isn't messed with

	AESEncryptionContext_finish(&ctx, additionalData, target);
	*tag = ctx.tag;

	return Error_none();
}

Error Buffer_encrypt(
	Buffer target,
	Buffer additionalData,
	EBufferEncryptionType type,
	EBufferEncryptionFlags flags,
	U32 key[8],
	I32x4 *iv,
	I32x4 *tag
) {

	if(Buffer_isConstRef(target))
		return Error_constData(0, 0);

	if(type >= EBufferEncryptionType_Count)
		return Error_invalidEnum(2, 0, (U64)type, (U64)EBufferEncryptionType_Count);

	if(flags & EBufferEncryptionFlags_Invalid)
		return Error_invalidEnum(3, 0, (U64)flags, ((U64)1 << EBufferEncryptionFlags_Count) - 1);

	if(!key || !iv || !tag)
		return Error_nullPointer(!key ? 4 : (!iv ? 5 : 6), 0);

	return AESEncryptionContext_encrypt(target, additionalData, flags, key, iv, tag);
}

inline Error AESEncryptionContext_decrypt(
	Buffer target,
	Buffer additionalData,
	const U32 realKey[8],
	I32x4 tag,
	I32x4 iv
) {

	//Validate inputs

	U64 targetLen = Buffer_length(target);

	//Since we have a 12-byte IV, we have a 4-byte block counter.
	//This block counter runs out in (4Gi - 3) * sizeof(Block) aka ~4Gi * 16 = ~64GiB.
	//When the IV block counter runs out it would basically repeat the same block xor pattern again.
	//-3 because we start at 2 since 1 is used at the end for verification (and 0 is skipped).

	if(targetLen > (4 * GIBI - 3) * sizeof(I32x4))
		return Error_unsupportedOperation(0);

	//Create context

	AESEncryptionContext ctx = AESEncryptionContext_create(realKey, iv, additionalData);

	//Verify tegridy before we continue decryption. This does mean we're reading twice, 
	//but it's against the spec to start decrypting while still unsure if it's valid.

	U32 j = (U32)((targetLen + 15) >> 4);

	for (U32 i = 0; i < j; ++i)
		AESEncryptionContext_fetchAndUpdateTag(
			&ctx,
			(const I32x4*)target.ptr + i,
			targetLen - ((U64)i << 4)
		);

	//Check if the tag is the same, if not, then it has been tempered with

	AESEncryptionContext_finish(&ctx, additionalData, target);

	if(I32x4_any(I32x4_neq(ctx.tag, tag)))
		return Error_invalidState(0);

	//Decrypt blocks blocks
	//TODO: We might wanna multithread this if we ever get big enough data
	//		For now, we're dealing with small enough files that spinning up threads would be slower
	//		(Without a job system)

	for (U32 i = 0; i < j; ++i)
		AESEncryptionContext_processBlock(
			&ctx,
			(I32x4*)target.ptr + i,
			targetLen - ((U64)i << 4),
			i,
			false
		);

	return Error_none();
}

Error Buffer_decrypt(
	Buffer target,						//"Cyphertext" aka data to decrypt. Leave empty to verify with AES256GCM
	Buffer additionalData,				//Data that was supplied to verify integrity of the data
	EBufferEncryptionType type,			//Only AES256GCM is currently supported
	const U32 key[8],					//Secret key; used for encryption and decryption
	I32x4 tag,							//Tag to verify integrity of encrypted data
	I32x4 iv							//Iv is a 12-byte random number that was used to encrypt the data
) {

	if(Buffer_isConstRef(target))
		return Error_constData(0, 0);

	if(type >= EBufferEncryptionType_Count)
		return Error_invalidEnum(1, 0, (U64) type, EBufferEncryptionType_Count);

	if(!key)
		return Error_nullPointer(3, 0);

	return AESEncryptionContext_decrypt(target, additionalData, key, tag, iv);
}
