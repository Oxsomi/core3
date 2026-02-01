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
#include "types/container/buffer.h"
#include "types/math/u128.h"
#include "types/math/vec4i_swizzle.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"
#include "types/math/type_cast.h"
#include "types/base/endianness.h"

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

impl I32x4 AES_keyGenAssist(I32x4 a, U8 i);
impl I32x4 AES_encodeBlock(I32x4 a, I32x4 b);
impl I32x4 AES_encodeBlockLast(I32x4 a, I32x4 b);

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

U8 AES_sbox(U8 x) {
	return AES_affine(AES_gfInv(x));
}

U32 AES_subWord(U32 w) {
	return
		((U32)AES_sbox((U8)(w >>  0)) <<  0) |
		((U32)AES_sbox((U8)(w >>  8)) <<  8) |
		((U32)AES_sbox((U8)(w >> 16)) << 16) |
		((U32)AES_sbox((U8)(w >> 24)) << 24);
}

//The context of important AES variables.
//And encrypting/decrypting blocks and verifying tags.
//These functions don't do any parameter checks since they're internal helper functions
typedef struct AESEncryptionContext {

	I32x4 key[15];

	I32x4 H[4];

	I32x4 EKY0;

	I32x4 tag;

	I32x4 iv;

	EBufferEncryptionType encryptionType;
	U32 padding[3];

} AESEncryptionContext;

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

static inline void AESEncryptionContext_expandKey(const U32 *key, I32x4 k[15], const EBufferEncryptionType encryptionType) {

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
static inline I32x4 AESEncryptionContext_blockHash(I32x4 block, const I32x4 k[15], const EBufferEncryptionType type) {

	block = I32x4_xor(block, k[0]);

	const U8 rounds = type == EBufferEncryptionType_AES128GCM ? 10 : 14;

	for(U8 i = 1; i < rounds; ++i)
		block = AES_encodeBlock(block, k[i]);

	return AES_encodeBlockLast(block, k[rounds]);
}

//Refactored from https://www.intel.com/content/dam/develop/external/us/en/documents/clmul-wp-rev-2-02-2014-04-20.pdf

static inline I32x4 AESEncryptionContext_ghash(I32x4 a, I32x4 H) {

	a = I32x4_swapEndianness(a);
	const I32x4 b = I32x4_swapEndianness(H);

	I32x4 tmp[8];

	tmp[0] = I32x4_clmul64(a, b, 0x00);
	tmp[3] = I32x4_xor(I32x4_clmul64(a, b, 0x10), I32x4_clmul64(a, b, 0x01));
	tmp[2] = I32x4_clmul64(a, b, 0x11);

	tmp[1] = I32x4_lshElements(tmp[3], 2);
	tmp[3] = I32x4_rshElements(tmp[3], 2);

	for(U8 i = 0; i < 2; ++i) {
		I32x4 t = I32x4_xor(tmp[i << 1], tmp[(i << 1) + 1]);
		tmp[i << 1] = I32x4_lsh32(t, 1);
		tmp[4 + (i << 1)] = I32x4_rsh32(t, 31);
	}

	tmp[7] = I32x4_rshElements(tmp[4], 3);

	for(U8 i = 0; i < 2; ++i)
		tmp[6 - i] = I32x4_lshElements(tmp[6 - (i << 1)], 1);

	const U8 v0[3] = { 31, 30, 25 };

	for(U8 i = 0; i < 3; ++i) {
		tmp[i << 1] = I32x4_or(tmp[i ? 2 : 0], tmp[5 + i]);
		tmp[5 + i] = I32x4_lsh32(tmp[0], v0[i]);
	}

	for(U8 i = 0; i < 2; ++i)
		tmp[5] = I32x4_xor(tmp[5], tmp[6 + i]);

	tmp[3] = I32x4_rshElements(tmp[5], 1);
	tmp[5] = I32x4_xor(tmp[0], I32x4_lshElements(tmp[5], 3));

	const U8 v1[3] = { 1, 2, 7 };

	for(U8 i = 0; i < 3; ++i)
		tmp[i] = I32x4_rsh32(tmp[5], v1[i]);

	for(U8 i = 1; i < 6; ++i)
		tmp[0] = I32x4_xor(tmp[0], tmp[i]);

	return I32x4_swapEndianness(tmp[0]);
}

//Safe fetch a block (even if <16 bytes are left)
static inline I32x4 AESEncryptionContext_fetchBlockTail(const void *dat, const U64 leftOver) {
	I32x4 v = I32x4_zero();
	Buffer_memcpy(Buffer_createRef(&v, sizeof(v)), Buffer_createRefConst(dat, leftOver));
	return v;
}

//Hash in the additional data
//This could be something like sender + receiver ip address
//This data could allow the dev to discard invalid packets for example
//And verify that this is the data the original message was signed with
static inline I32x4 AESEncryptionContext_initTag(const Buffer *additionalData, I32x4 H) {

	I32x4 tag = I32x4_zero();

	if (!additionalData)
		return tag;

	const U64 len = Buffer_length(*additionalData);
	const U8 *ptr = additionalData->ptr;

	//TODO: Speed this up

	for (U64 i = 0, j = (len + 15) >> 4; i < j; ++i) {
		const I32x4 ADi = AESEncryptionContext_fetchBlockTail((const I32*)ptr + ((U64)i << 2), len - (i << 4));
		tag = AESEncryptionContext_ghash(I32x4_xor(tag, ADi), H);
	}

	return tag;
}

static inline Bool AESEncryptionContext_create(const BufferEncrypt *encrypt, AESEncryptionContext *ctx, Error *e_rr) {

	Bool s_uccess = true;

	if (!encrypt->target)
		retError(clean, Error_nullPointer(0, "AESEncryptionContext_create()::decrypt->target must be non zero"));

	if ((U64)encrypt->type >= EBufferEncryptionType_Count)
		retError(clean, Error_invalidEnum(
			1, (U64)encrypt->type, EBufferEncryptionType_Count, "AESEncryptionContext_create()::encrypt->type is out of bounds"
		));

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

	//Get key that's gonna be used for aes blocks

	ctx->encryptionType = encrypt->type;
	AESEncryptionContext_expandKey(encrypt->constDecrypt.key, ctx->key, ctx->encryptionType);

	//Prepare ghash

	ctx->H[0] = AESEncryptionContext_blockHash(I32x4_zero(), ctx->key, ctx->encryptionType);
	ctx->H[1] = AESEncryptionContext_ghash(ctx->H[0], ctx->H[0]);
	ctx->H[2] = AESEncryptionContext_ghash(ctx->H[1], ctx->H[0]);
	ctx->H[3] = AESEncryptionContext_ghash(ctx->H[2], ctx->H[0]);

	//Compute final tag xor

	I32x4 Y0 = *encrypt->constDecrypt.iv;
	ctx->iv = Y0;

	I32x4_setWRef(&Y0, I32_swapEndianness(1));

	ctx->EKY0 = AESEncryptionContext_blockHash(Y0, ctx->key, ctx->encryptionType);
	ctx->tag = AESEncryptionContext_initTag(encrypt->additionalData, ctx->H[0]);

clean:
	return s_uccess;
}

typedef union AESEncryptionContextLengths {
	I32x4 vec;
	U64 arr[2];
} AESEncryptionContextLengths;

static inline void AESEncryptionContext_finish(AESEncryptionContext *ctx, const Buffer *additionalData, const Buffer *target) {

	//Add length of inputs into the message too (lengths are in bits)

	AESEncryptionContextLengths lengths = { 0 };

	if(additionalData)
		lengths.arr[0] = U64_swapEndianness(Buffer_length(*additionalData) << 3);

	if(target)
		lengths.arr[1] = U64_swapEndianness(Buffer_length(*target) << 3);

	ctx->tag = AESEncryptionContext_ghash(I32x4_xor(ctx->tag, lengths.vec), ctx->H[0]);

	//Finish up by adding the iv into the key (this already has blockId 1 in it)

	ctx->tag = I32x4_xor(ctx->tag, ctx->EKY0);
}

static inline void AESEncryptionContext_updateTagN(AESEncryptionContext *ctx, const I32x4 CTi[4], const U8 N) {
	
	I32x4 v[4];

	v[0] = AESEncryptionContext_ghash(I32x4_xor(CTi[0], ctx->tag), ctx->H[N - 1]);

	for (U8 i = 1; i < N; ++i)
		v[i] = AESEncryptionContext_ghash(CTi[i], ctx->H[N - 1 - i]);

	I32x4 t;

	switch (N) {
		default:	t = v[0];														break;
		case 2:		t = I32x4_xor(v[0], v[1]);										break;
		case 4:		t = I32x4_xor(I32x4_xor(v[0], v[1]), I32x4_xor(v[2], v[3]));	break;
	}

	ctx->tag = t;
}

static inline void AESEncryptionContext_updateTagTail(AESEncryptionContext *ctx, I32x4 CTi, const U8 leftOver) {
	Buffer_unsetAllBits(Buffer_createRef(((U8*)&CTi + leftOver), 16 - leftOver), NULL);
	ctx->tag = AESEncryptionContext_ghash(I32x4_xor(CTi, ctx->tag), ctx->H[0]);
}

static inline void AESEncryptionContext_storeBlockTail(I32 *io, const U64 leftOver, void *v) {
	Buffer_memcpy(Buffer_createRef(io, sizeof(I32x4)), Buffer_createRefConst(v, leftOver));
}

static inline void AESEncryptionContext_processBlockTail(
	AESEncryptionContext *ctx,
	I32 *io,
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
	AESEncryptionContext *ctx,
	I32x4 *io,
	const U32 id,
	const U8 N,
	Bool isEncrypt
) {

	I32x4 v[4];

	for (U32 i = 0; i < N; ++i)
		v[i] = io[i];

	//Update tag for the ciphertext (before decryption)

	if (!isEncrypt)
		AESEncryptionContext_updateTagN(ctx, v, N);

	//Encrypt IV + blockId to use to encrypt

	for (U32 i = 0; i < N; ++i) {

		I32x4 ivi = ctx->iv;
		I32x4_setWRef(&ivi, (I32)U32_swapEndianness(id + i + 2));

		v[i] = I32x4_xor(v[i], AESEncryptionContext_blockHash(ivi, ctx->key, ctx->encryptionType));
	}

	//Update tag for the ciphertext (after encryption)

	if(isEncrypt)
		AESEncryptionContext_updateTagN(ctx, v, N);

	//Store

	for (U32 i = 0; i < N; ++i)
		io[i] = v[i];
}

static inline void AESEncryptionContext_processBlock1(AESEncryptionContext *ctx, I32x4 *io, const U32 id, Bool isEncrypt) {
	AESEncryptionContext_processBlockN(ctx, io, id, 1, isEncrypt);
}

static inline void AESEncryptionContext_processBlock2(AESEncryptionContext *ctx, I32x4 *io, const U32 id, Bool isEncrypt) {
	AESEncryptionContext_processBlockN(ctx, io, id, 2, isEncrypt);
}

static inline void AESEncryptionContext_processBlock4(AESEncryptionContext *ctx, I32x4 *io, const U32 id, Bool isEncrypt) {
	AESEncryptionContext_processBlockN(ctx, io, id, 4, isEncrypt);
}

//TODO: fetchBlock4, 2, tail this
static inline void AESEncryptionContext_fetchAndUpdateTag(AESEncryptionContext *ctx, const void *data, const U64 leftOver) {
	I32x4 v[4];
	v[0] = AESEncryptionContext_fetchBlockTail(data, leftOver);
	AESEncryptionContext_updateTagN(ctx, v, 1);
}

//This ensures no expanded key, iv or anything else is leaked on the stack,
//which might be possible to obtain after execution through for example a buffer overflow.
static inline void AESEncryptionContext_clear(AESEncryptionContext *ctx) {
	Buffer_unsetAllBits(Buffer_createRef(ctx->key, sizeof(ctx->key)), NULL);
	Buffer_unsetAllBits(Buffer_createRef(ctx->H, sizeof(ctx->H)), NULL);
	ctx->iv = ctx->tag = ctx->EKY0 = I32x4_zero();
	ctx->encryptionType = 0;
}

static inline void AESEncryptionContext_handleBlocks(AESEncryptionContext *ctx, U8 *targetPtr, U64 targetLen, Bool isEncrypt) {

	//Handle blocks
	//TODO: We might wanna multithread this if we ever get big enough data
	//		For now, we're dealing with small enough files that spinning up threads would be slower
	//		(Without a job system)

	//4 blocks at a time, this handles only fully aligned blocks.
	//This improves performance because it allows better scheduling
	// (4 can run in parallel, instead of being blocked every instruction)

	for (U32 i = 0; i < targetLen >> 6; ++i)
		AESEncryptionContext_processBlock4(
			ctx,
			(I32x4*)(targetPtr + ((U64)i << 6)),
			i << 2,
			isEncrypt
		);

	U64 next = targetLen & ~63;

	if (next + 32 <= targetLen) {
		AESEncryptionContext_processBlock2(
			ctx,
			(I32x4*)(targetPtr + next),
			(U32)(next >> 4),
			isEncrypt
		);
		next += 32;
	}

	if (next + 16 <= targetLen) {
		AESEncryptionContext_processBlock1(
			ctx,
			(I32x4*)(targetPtr + next),
			(U32)(next >> 4),
			isEncrypt
		);
		next += 16;
	}

	if (next < targetLen)
		AESEncryptionContext_processBlockTail(
			ctx,
			(I32*)(targetPtr + next),
			(U8)(targetLen & 15),
			(U32)(targetLen >> 4),
			isEncrypt
		);
}

static inline Bool AESEncryptionContext_encrypt(const BufferEncrypt *encrypt, Error *e_rr) {

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

	AESEncryptionContext ctx;
	gotoIfError3(clean, AESEncryptionContext_create(encrypt, &ctx, e_rr));

	U8 *targetPtr = encrypt->target->ptrNonConst;
	const U64 targetLen = Buffer_length(*encrypt->target);
	AESEncryptionContext_handleBlocks(&ctx, targetPtr, targetLen, true);

	//Finish encryption by appending tag for authentication / verification that the data isn't messed with

	AESEncryptionContext_finish(&ctx, encrypt->additionalData, encrypt->target);
	*encrypt->nonConstEncrypt.tag = ctx.tag;

	AESEncryptionContext_clear(&ctx);

clean:
	return s_uccess;
}

Bool Buffer_encryptAuto(
	Buffer *target,
	const Buffer *additionalData,
	Bool generateKey,
	U32 key[8],
	I32x4 *tag,
	I32x4 *iv,
	Error *e_rr
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
	Buffer *target,
	const Buffer *additionalData,
	const U32 key[8],
	I32x4 tag,
	I32x4 iv,
	Error *e_rr
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

Bool Buffer_encryptAdvanced(const BufferEncrypt *encrypt, Error *e_rr) {

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

static inline Bool AESEncryptionContext_decrypt(const BufferEncrypt *decrypt, Error *e_rr) {

	Bool s_uccess = true;

	//Create context

	AESEncryptionContext ctx;
	gotoIfError3(clean, AESEncryptionContext_create(decrypt, &ctx, e_rr));

	U8 *targetPtr = decrypt->target->ptrNonConst;
	const U64 targetLen = Buffer_length(*decrypt->target);

	AESEncryptionContext_handleBlocks(&ctx, targetPtr, targetLen, false);
	AESEncryptionContext_finish(&ctx, decrypt->additionalData, decrypt->target);

	//Check if the tag is the same, if not, then it has been tempered with

	if (I32x4_neq4(ctx.tag, *decrypt->constDecrypt.tag)) {

		AESEncryptionContext_clear(&ctx);

		if(decrypt->target)
			Buffer_unsetAllBits(*decrypt->target, NULL);

		retError(clean, Error_invalidState(0, "AESEncryptionContext_decrypt() GMAC tag is invalid"));
	}

	AESEncryptionContext_clear(&ctx);

clean:
	return s_uccess;
}

Bool Buffer_decryptAdvanced(const BufferEncrypt *decrypt, Error *e_rr) {

	Bool s_uccess = true;

	if (!decrypt)
		retError(clean, Error_nullPointer(0, "Buffer_decryptAdvanced()::decrypt must be non zero"));

	if (decrypt->flags)
		retError(clean, Error_invalidParameter(3, 0, "Buffer_decryptAdvanced()::flags are invalid"));

	gotoIfError3(clean, AESEncryptionContext_decrypt(decrypt, e_rr));

clean:
	return s_uccess;
}
