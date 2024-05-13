/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/type_cast.h"
#include "types/vec.h"

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
impl I32x4 AES_encodeBlock(I32x4 a, I32x4 b, Bool isLast);

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

I32x4 AESEncryptionContext_expandKeyN(I32x4 im1, const I32x4 im2) {

	I32x4 im4 = im1;

	for(U8 i = 0; i < 3; ++i) {
		im4 = I32x4_lshByte(im4, 4);
		im1 = I32x4_xor(im1, im4);
	}

	return I32x4_xor(im1, im2);
}

I32x4 AESEncryptionContext_expandKey1(const I32x4 im1, const I32x4 im2) {
	return AESEncryptionContext_expandKeyN(im1, I32x4_wwww(im2));
}

I32x4 AESEncryptionContext_expandKey2(const I32x4 im1, const I32x4 im3) {
	return AESEncryptionContext_expandKeyN(im3, I32x4_zzzz(AES_keyGenAssist(im1, 0)));
}

void AESEncryptionContext_expandKey(const U32 *key, I32x4 k[15], const EBufferEncryptionType encryptionType) {

	k[0] = I32x4_load4((const I32*)key);

	if(encryptionType == EBufferEncryptionType_AES256GCM)
		k[1] = I32x4_load4((const I32*)key + 4);

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

I32x4 AESEncryptionContext_blockHash(I32x4 block, const I32x4 k[15], const EBufferEncryptionType type) {

	block = I32x4_xor(block, k[0]);

	const U8 rounds = type == EBufferEncryptionType_AES128GCM ? 10 : 14;

	for(U8 i = 1; i < rounds; ++i)
		block = AES_encodeBlock(block, k[i], false);

	return AES_encodeBlock(block, k[rounds], true);
}

impl void AESEncryptionContext_ghashPrepare(I32x4 H, I32x4 ghashLut[17]);
impl I32x4 AESEncryptionContext_ghash(I32x4 a, const I32x4 ghashLut[17]);

//Safe fetch a block (even if <16 bytes are left)

I32x4 AESEncryptionContext_fetchBlock(const I32 *dat, const U64 leftOver) {

	//Avoid out of bounds read, by simply filling additional data by zero

	if (leftOver < sizeof(I32x4)) {

		I32x4 v = I32x4_zero();
		Buffer_copy(
			Buffer_createRef(&v, sizeof(v)),
			Buffer_createRefConst(dat, leftOver)
		);

		return v;
	}

	return I32x4_load4(dat);
}

//Hash in the additional data
//This could be something like sender + receiver ip address
//This data could allow the dev to discard invalid packets for example
//And verify that this is the data the original message was signed with

I32x4 AESEncryptionContext_initTag(Buffer additionalData, const I32x4 ghashLut[17]) {

	I32x4 tag = I32x4_zero();
	const U64 len = Buffer_length(additionalData);

	for (U64 i = 0, j = (len + 15) >> 4; i < j; ++i) {

		const I32x4 ADi = AESEncryptionContext_fetchBlock(
			(const I32*)additionalData.ptr + ((U64)i << 2), len - (i << 4)
		);

		tag = AESEncryptionContext_ghash(I32x4_xor(tag, ADi), ghashLut);
	}

	return tag;
}

AESEncryptionContext AESEncryptionContext_create(
	const U32 *realKey,
	const I32x4 iv,
	const Buffer additionalData,
	const EBufferEncryptionType encryptionType
) {

	AESEncryptionContext ctx = (AESEncryptionContext) { .encryptionType = encryptionType };

	//Get key that's gonna be used for aes blocks

	AESEncryptionContext_expandKey(realKey, ctx.key, ctx.encryptionType);

	//Prepare ghash

	ctx.H = AESEncryptionContext_blockHash(I32x4_zero(), ctx.key, ctx.encryptionType);

	AESEncryptionContext_ghashPrepare(ctx.H, ctx.ghashLut);

	//Compute final tag xor

	I32x4 Y0 = iv;
	I32x4_setW(&Y0, I32_swapEndianness(1));

	ctx.iv = iv;
	ctx.EKY0 = AESEncryptionContext_blockHash(Y0, ctx.key, ctx.encryptionType);
	ctx.tag = AESEncryptionContext_initTag(additionalData, ctx.ghashLut);

	return ctx;
}

void AESEncryptionContext_finish(AESEncryptionContext *ctx, const Buffer additionalData, const Buffer target) {

	//Add length of inputs into the message too (lengths are in bits)

	I32x4 lengths = I32x4_zero();
	*(U64*)&lengths = U64_swapEndianness(Buffer_length(additionalData) << 3);
	*((U64*)&lengths + 1) = U64_swapEndianness(Buffer_length(target) << 3);

	ctx->tag = AESEncryptionContext_ghash(I32x4_xor(ctx->tag, lengths), ctx->ghashLut);

	//Finish up by adding the iv into the key (this is already has blockId 1 in it)

	ctx->tag = I32x4_xor(ctx->tag, ctx->EKY0);
}

void AESEncryptionContext_updateTag(AESEncryptionContext *ctx, const I32x4 CTi) {
	ctx->tag = AESEncryptionContext_ghash(I32x4_xor(CTi, ctx->tag), ctx->ghashLut);
}

void AESEncryptionContext_storeBlock(I32 *io, const U64 leftOver, const I32 *v) {

	//A special property of unaligned blocks is that the bytes that are added as padding
	//shouldn't be stored, and so they have to be zero-ed in CTi, otherwise the tag will mess up

	if (leftOver < sizeof(I32x4)) {

		Buffer_unsetAllBits(Buffer_createRef((U8*)v + leftOver, sizeof(I32x4) - leftOver));

		Buffer_copy(
			Buffer_createRef(io, leftOver),
			Buffer_createRefConst(v, leftOver)
		);
	}

	//Full blocks

	else for(U64 i = 0; i < 4; ++i)
		io[i] = v[i];
}

void AESEncryptionContext_processBlock(
	AESEncryptionContext *ctx,
	I32 *io,
	const U64 leftOver,
	const U32 i,
	const Bool updateTag
) {

	I32x4 v = AESEncryptionContext_fetchBlock(io, leftOver);

	//Encrypt IV + blockId to use to encrypt

	I32x4 ivi = ctx->iv;
	I32x4_setW(&ivi, I32_swapEndianness((U32)i + 2));

	v = I32x4_xor(v, AESEncryptionContext_blockHash(ivi, ctx->key, ctx->encryptionType));

	AESEncryptionContext_storeBlock(io, leftOver, (const I32*) &v);

	//Continue tag

	if(updateTag)
		AESEncryptionContext_updateTag(ctx, v);
}

void AESEncryptionContext_fetchAndUpdateTag(AESEncryptionContext *ctx, const I32 *data, const U64 leftOver) {
	AESEncryptionContext_updateTag(ctx, AESEncryptionContext_fetchBlock(data, leftOver));
}

U64 EBufferEncryptionType_getAdditionalData(const EBufferEncryptionType type) {
	switch (type) {
		case EBufferEncryptionType_AES128GCM:
		case EBufferEncryptionType_AES256GCM:	return 16 + 12;
		default:								return 0;
	}
}

Error AESEncryptionContext_encrypt(
	const Buffer target,
	const Buffer additionalData,
	const EBufferEncryptionFlags flags,
	U32 *realKey,
	I32x4 *ivPtr,
	I32x4 *tag,
	const EBufferEncryptionType encryptionType
) {

	//Since we have a 12-byte IV, we have a 4-byte block counter.
	//This block counter runs out in (4Gi - 3) * sizeof(Block) aka ~4Gi * 16 = ~64GiB.
	//When the IV block counter runs out it would basically repeat the same block xor pattern again.
	//-3 because we start at 2 since 1 is used at the end for verification (and 0 is skipped).

	if(Buffer_length(target) > (4 * GIBI - 3) * sizeof(I32x4))
		return Error_unsupportedOperation(
			0,
			"AESEncryptionContext_encrypt()::target has a limit of 64GB - 48 bytes to avoid block counter re-use.\n"
			"If file size exceeds 64GB encrypt in blocks with a unique IV each 64GB block"
		);

	//Generate iv & context

	I32x4_setW(ivPtr, 0);

	if(flags & EBufferEncryptionFlags_GenerateIv) {

		if(!Buffer_csprng(Buffer_createRef(ivPtr, 12)))
			return Error_invalidState(0, "AESEncryptionContext_encrypt() couldn't generate iv");
	}

	if(flags & EBufferEncryptionFlags_GenerateKey) {

		const U8 len = encryptionType == EBufferEncryptionType_AES128GCM ? 4 : 8;

		if(!Buffer_csprng(Buffer_createRef(realKey, sizeof(U32) * len)))
			return Error_invalidState(1, "AESEncryptionContext_encrypt() couldn't generate key");
	}

	AESEncryptionContext ctx = AESEncryptionContext_create(realKey, *ivPtr, additionalData, encryptionType);

	//Handle blocks
	//TODO: We might wanna multithread this if we ever get big enough data
	//		For now, we're dealing with small enough files that spinning up threads would be slower
	//		(Without a job system)

	const U64 targetLen = Buffer_length(target);

	const U32 j = (U32)((targetLen + 15) >> 4);

	for (U32 i = 0; i < j; ++i)
		AESEncryptionContext_processBlock(
			&ctx,
			(I32*)target.ptr + ((U64)i << 2),
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
	U32 *key,
	I32x4 *iv,
	I32x4 *tag
) {

	if(Buffer_isConstRef(target))
		return Error_constData(0, 0, "Buffer_encrypt()::target must be writable");

	if(type >= EBufferEncryptionType_Count)
		return Error_invalidEnum(
			2, (U64)type, (U64)EBufferEncryptionType_Count, "Buffer_encrypt()::type out of bounds"
		);

	if(flags & EBufferEncryptionFlags_Invalid)
		return Error_invalidEnum(
			3, (U64)flags, ((U64)1 << EBufferEncryptionFlags_Count) - 1,
			"Buffer_encrypt()::flags are invalid"
		);

	if(!key || !iv || !tag)
		return Error_nullPointer(!key ? 4 : (!iv ? 5 : 6), "Buffer_encrypt()::key, iv and tag are required");

	return AESEncryptionContext_encrypt(target, additionalData, flags, key, iv, tag, type);
}

Error AESEncryptionContext_decrypt(
	const Buffer target,
	const Buffer additionalData,
	const U32 *realKey,
	const I32x4 tag,
	const I32x4 iv,
	const EBufferEncryptionType type
) {

	//Validate inputs

	const U64 targetLen = Buffer_length(target);

	//Since we have a 12-byte IV, we have a 4-byte block counter.
	//This block counter runs out in (4Gi - 3) * sizeof(Block) aka ~4Gi * 16 = ~64GiB.
	//When the IV block counter runs out it would basically repeat the same block xor pattern again.
	//-3 because we start at 2 since 1 is used at the end for verification (and 0 is skipped).

	if(targetLen > (4 * GIBI - 3) * sizeof(I32x4))
		return Error_unsupportedOperation(
			0,
			"AESEncryptionContext_decrypt()::target has a limit of 64GB - 48 bytes to avoid block counter re-use.\n"
			"If file size exceeds 64GB encrypt in blocks with a unique IV each 64GB block"
		);

	//Create context

	AESEncryptionContext ctx = AESEncryptionContext_create(realKey, iv, additionalData, type);

	//Verify tegridy before we continue decryption. This does mean we're reading twice,
	//but it's against the spec to start decrypting while still unsure if it's valid.

	const U32 j = (U32)((targetLen + 15) >> 4);

	for (U32 i = 0; i < j; ++i)
		AESEncryptionContext_fetchAndUpdateTag(
			&ctx,
			(const I32*)target.ptr + ((U64)i << 2),
			targetLen - ((U64)i << 4)
		);

	//Check if the tag is the same, if not, then it has been tempered with

	AESEncryptionContext_finish(&ctx, additionalData, target);

	if(I32x4_any(I32x4_neq(ctx.tag, tag)))
		return Error_invalidState(0, "AESEncryptionContext_decrypt() GMAC tag is invalid");

	//Decrypt blocks
	//TODO: We might wanna multi-thread this if we ever get big enough data
	//		For now, we're dealing with small enough files that spinning up threads would be slower
	//		(Without a job system)

	for (U32 i = 0; i < j; ++i)
		AESEncryptionContext_processBlock(
			&ctx,
			(I32*)target.ptr + ((U64)i << 2),
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
	const U32 *key,						//Secret key; used for encryption and decryption
	I32x4 tag,							//Tag to verify integrity of encrypted data
	I32x4 iv							//Iv is a 12-byte random number that was used to encrypt the data
) {

	if(Buffer_isConstRef(target))
		return Error_constData(0, 0, "Buffer_decrypt()::target needs to be writable");

	if(type >= EBufferEncryptionType_Count)
		return Error_invalidEnum(1, (U64) type, EBufferEncryptionType_Count, "Buffer_decrypt()::type is out of bounds");

	if(!key)
		return Error_nullPointer(3, "Buffer_decrypt()::key is required");

	return AESEncryptionContext_decrypt(target, additionalData, key, tag, iv, type);
}
