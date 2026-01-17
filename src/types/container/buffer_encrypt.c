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
#include "types/math/vec4i_swizzle.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"
#include "types/math/type_cast.h"
#include "types/math/endianness.h"

//Explanation of algorithm; AES256 GCM + GMAC
//https://www.alexeyshmalko.com/20200319144641/
//https://www.youtube.com/watch?v=V2TlG3JbGp0
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
		im4 = I32x4_lshByte(im4, 4);
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

impl void AESEncryptionContext_ghashPrepare(I32x4 H, I32x4 ghashLut[17]);
impl I32x4 AESEncryptionContext_ghash(I32x4 a, const I32x4 ghashLut[17]);

//Safe fetch a block (even if <16 bytes are left)
static inline I32x4 AESEncryptionContext_fetchBlock(const void *dat, const U64 leftOver) {

	I32x4 v = I32x4_zero();
	Buffer_memcpy(
		Buffer_createRef(&v, sizeof(v)),
		Buffer_createRefConst(dat, U64_min(leftOver, sizeof(v)))
	);

	return v;
}

//Hash in the additional data
//This could be something like sender + receiver ip address
//This data could allow the dev to discard invalid packets for example
//And verify that this is the data the original message was signed with
static inline I32x4 AESEncryptionContext_initTag(const Buffer *additionalData, const I32x4 ghashLut[17]) {

	I32x4 tag = I32x4_zero();

	if (!additionalData)
		return tag;

	const U64 len = Buffer_length(*additionalData);
	const U8 *ptr = additionalData->ptr;

	for (U64 i = 0, j = (len + 15) >> 4; i < j; ++i) {
		const I32x4 ADi = AESEncryptionContext_fetchBlock((const I32*)ptr + ((U64)i << 2), len - (i << 4));
		tag = AESEncryptionContext_ghash(I32x4_xor(tag, ADi), ghashLut);
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

	const U64 targetLen = Buffer_length(*encrypt->target);

	//Since we have a 12-byte IV, we have a 4-byte block counter.
	//This block counter runs out in (4Gi - 3) * sizeof(Block) aka ~4Gi * 16 = ~64GiB.
	//When the IV block counter runs out it would basically repeat the same block xor pattern again.
	//-3 because we start at 2 since 1 is used at the end for verification (and 0 is skipped).

	if(targetLen > (4 * GIBI - 3) * sizeof(I32x4))
		retError(clean, Error_unsupportedOperation(
			0,
			"AESEncryptionContext_decrypt()::target has a limit of 64GB - 48 bytes to avoid block counter re-use.\n"
			"If file size exceeds 64GB encrypt in blocks with a unique IV each 64GB block"
		));

	//Get key that's gonna be used for aes blocks

	ctx->encryptionType = encrypt->type;
	AESEncryptionContext_expandKey(encrypt->constDecrypt.key, ctx->key, ctx->encryptionType);

	//Prepare ghash

	ctx->H = AESEncryptionContext_blockHash(I32x4_zero(), ctx->key, ctx->encryptionType);

	AESEncryptionContext_ghashPrepare(ctx->H, ctx->ghashLut);

	//Compute final tag xor

	I32x4 Y0 = *encrypt->constDecrypt.iv;
	ctx->iv = Y0;

	I32x4_setWRef(&Y0, I32_swapEndianness(1));

	ctx->EKY0 = AESEncryptionContext_blockHash(Y0, ctx->key, ctx->encryptionType);
	ctx->tag = AESEncryptionContext_initTag(encrypt->additionalData, ctx->ghashLut);

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

	ctx->tag = AESEncryptionContext_ghash(I32x4_xor(ctx->tag, lengths.vec), ctx->ghashLut);

	//Finish up by adding the iv into the key (this is already has blockId 1 in it)

	ctx->tag = I32x4_xor(ctx->tag, ctx->EKY0);
}

static inline void AESEncryptionContext_updateTag(AESEncryptionContext *ctx, const I32x4 CTi) {
	ctx->tag = AESEncryptionContext_ghash(I32x4_xor(CTi, ctx->tag), ctx->ghashLut);
}

static inline void AESEncryptionContext_storeBlock(I32 *io, const U64 leftOver, void *v) {

	//A special property of unaligned blocks is that the bytes that are added as padding
	//shouldn't be stored, and so they have to be zero-ed in CTi, otherwise the tag will mess up

	if (leftOver < sizeof(I32x4))
		Buffer_unsetAllBits(Buffer_createRef((U8*)v + leftOver, sizeof(I32x4) - leftOver), NULL);

	Buffer_memcpy(
		Buffer_createRef(io, sizeof(I32x4)),
		Buffer_createRefConst(v, U64_min(sizeof(I32x4), leftOver))
	);
}

static inline void AESEncryptionContext_processBlock(
	AESEncryptionContext *ctx,
	I32 *io,
	const U64 leftOver,
	const U32 i,
	const Bool updateTag
) {

	I32x4 v = AESEncryptionContext_fetchBlock(io, leftOver);

	//Encrypt IV + blockId to use to encrypt

	I32x4 ivi = ctx->iv;
	I32x4_setWRef(&ivi, I32_swapEndianness((U32)i + 2));

	v = I32x4_xor(v, AESEncryptionContext_blockHash(ivi, ctx->key, ctx->encryptionType));

	AESEncryptionContext_storeBlock(io, leftOver, &v);

	//Continue tag

	if(updateTag)
		AESEncryptionContext_updateTag(ctx, v);
}

static inline void AESEncryptionContext_fetchAndUpdateTag(AESEncryptionContext *ctx, const void *data, const U64 leftOver) {
	AESEncryptionContext_updateTag(ctx, AESEncryptionContext_fetchBlock(data, leftOver));
}

//This ensures no expanded key, iv or anything else is leaked on the stack,
//which might be possible to obtain after execution through for example a buffer overflow.
static inline void AESEncryptionContext_clear(AESEncryptionContext *ctx) {
	Buffer_unsetAllBits(Buffer_createRef(ctx->key, sizeof(ctx->key)), NULL);
	ctx->iv = ctx->tag = ctx->EKY0 = ctx->H = I32x4_zero();
	ctx->encryptionType = 0;
}

static inline Bool AESEncryptionContext_encrypt(const BufferEncrypt *encrypt, Error *e_rr) {

	Bool s_uccess = true;

	//Generate iv & context

	I32x4_setWRef(encrypt->nonConstEncrypt.iv, 0);

	if(encrypt->flags & EBufferEncryptionFlags_GenerateIv) {

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

	const U8 *targetPtr = encrypt->target->ptr;
	const U64 targetLen = Buffer_length(*encrypt->target);

	//Handle blocks
	//TODO: We might wanna multithread this if we ever get big enough data
	//		For now, we're dealing with small enough files that spinning up threads would be slower
	//		(Without a job system)

	const U32 j = (U32)((targetLen + 15) >> 4);

	for (U32 i = 0; i < j; ++i)
		AESEncryptionContext_processBlock(
			&ctx,
			(I32*)targetPtr + ((U64)i << 2),
			targetLen - ((U64)i << 4),
			i,
			true
		);

	//Finish encryption by appending tag for authentication / verification that the data isn't messed with

	AESEncryptionContext_finish(&ctx, encrypt->additionalData, encrypt->target);
	*encrypt->nonConstEncrypt.tag = ctx.tag;

	AESEncryptionContext_clear(&ctx);

clean:
	return s_uccess;
}

Bool Buffer_encrypt(const BufferEncrypt *encrypt, Error *e_rr) {

	Bool s_uccess = true;

	if(!encrypt)
		retError(clean, Error_nullPointer(0, "Buffer_encrypt()::encrypt must be non zero"));

	if(encrypt->flags & EBufferEncryptionFlags_Invalid)
		retError(clean, Error_invalidEnum(
			3, (U64)encrypt->flags, ((U64)1 << EBufferEncryptionFlags_Count) - 1,
			"Buffer_encrypt()::flags are invalid"
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

	const U8 *targetPtr = decrypt->target->ptr;
	const U64 targetLen = Buffer_length(*decrypt->target);

	//Verify tegridy before we continue decryption. This does mean we're reading twice,
	//but it's against the spec to start decrypting while still unsure if it's valid.

	const U32 j = (U32)((targetLen + 15) >> 4);

	for (U32 i = 0; i < j; ++i)
		AESEncryptionContext_fetchAndUpdateTag(
			&ctx,
			(const I32*)targetPtr + ((U64)i << 2),
			targetLen - ((U64)i << 4)
		);

	//Check if the tag is the same, if not, then it has been tempered with

	AESEncryptionContext_finish(&ctx, decrypt->additionalData, decrypt->target);

	if(I32x4_any(I32x4_neq(ctx.tag, *decrypt->constDecrypt.tag))) {
		AESEncryptionContext_clear(&ctx);
		retError(clean, Error_invalidState(0, "AESEncryptionContext_decrypt() GMAC tag is invalid"));
	}

	//Decrypt blocks
	//TODO: We might wanna multi-thread this if we ever get big enough data
	//		For now, we're dealing with small enough files that spinning up threads would be slower
	//		(Without a job system)

	for (U32 i = 0; i < j; ++i)
		AESEncryptionContext_processBlock(
			&ctx,
			(I32*)targetPtr + ((U64)i << 2),
			targetLen - ((U64)i << 4),
			i,
			false
		);

	AESEncryptionContext_clear(&ctx);

clean:
	return s_uccess;
}

Bool Buffer_decrypt(const BufferEncrypt *decrypt, Error *e_rr) {

	Bool s_uccess = true;

	if (!decrypt)
		retError(clean, Error_nullPointer(0, "Buffer_decrypt()::decrypt must be non zero"));

	if (decrypt->flags)
		retError(clean, Error_invalidParameter(3, 0, "Buffer_encrypt()::flags are invalid"));

	gotoIfError3(clean, AESEncryptionContext_decrypt(decrypt, e_rr));

clean:
	return s_uccess;
}
