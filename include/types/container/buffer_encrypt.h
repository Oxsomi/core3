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

//types/container/buffer_encrypt.h

#pragma once
#include "types/math/vec4.h"
#include "types/base/buffer_base.h"

#include <stdalign.h>

#ifdef __cplusplus
	extern "C" {
#endif

//Key derivation, so one raw key can protect any number of files without two of them sharing an AES key.
//AES256GCM is broken once a key sees the same iv twice, and random 96-bit ivs keep that unlikely for only about
// 2^32 encryptions per key, across every file that key ever protects.
//Each output instead encrypts under keys derived from the raw key and its own random root iv, one per use,
// so that budget belongs to one output and a header's tag can't collide with its first chunk.
//See docs/file/oiDL.md for the key schedule as stored.

//What a derived key is for; each domain is its own HKDF info string, so one raw key and root iv give unrelated
// keys for an oiDL header, an oiCA header and the encrypted stream data.
typedef enum EEncryptionKeyDomain {
	EEncryptionKeyDomain_StreamData,
	EEncryptionKeyDomain_DLHeader,
	EEncryptionKeyDomain_CAHeader,
	EEncryptionKeyDomain_Count
} EEncryptionKeyDomain;

//The AES256 key for one domain of one output: HKDF-SHA256 over the raw key with rootIv's 12 bytes as salt.
//The key has to be raw key material (32 random bytes), not a password: HKDF does no stretching.
//rootIv has to be unique per output or embedded stream, since it is all that tells their keys apart.
Bool Buffer_deriveEncryptionKey(
	const U32 key[8], I32x4 rootIv, EEncryptionKeyDomain domain, U32 output[8], Error *e_rr
);

//AES256GCM encryption with auto generated iv.
//Encrypt function encrypts target into target (in place).
//Be careful about the following:
//- Don't use the key too often (suggested <2^16 times)
//- Don't discard iv or key if any of them are generated
//- Don't discard tag or cut off too many bytes
//- additionalData and target must be 16-byte aligned
Bool Buffer_encryptAuto(
	Buffer *restrict target,
	const Buffer *restrict additionalData,
	Bool generateKey,
	U32 *restrict key,            //U32[8]
	I32x4 *restrict tag,
	I32x4 *restrict iv,
	Error *restrict e_rr
);

//AES256GCM decryption.
//Decrypt functions decrypt ciphertext from target into target (in place).
//Will return an error if the tag can't be verified
//Will clear decrypted result if the function was successful.
//When decrypting, be sure of the following:
//- Don't use the data if the function returns false (s_ucceeded = false)!
//- additionalData and target must be 16-byte aligned
Bool Buffer_decryptAuto(
	Buffer *restrict target,
	const Buffer *restrict additionalData,
	const U32 *restrict key,    //U32[8]
	I32x4 tag,
	I32x4 iv,
	Error *restrict e_rr
);

typedef enum EBufferEncryptionType {
	EBufferEncryptionType_AES256GCM,        //Additional data; IV (96 bits), TAG (128 bits)
	EBufferEncryptionType_AES128GCM,        //^
	EBufferEncryptionType_Count
} EBufferEncryptionType;

typedef enum EBufferEncryptionFlags {

	EBufferEncryptionFlags_None            = 0,
	EBufferEncryptionFlags_GenerateKey    = 1 << 0,
	EBufferEncryptionFlags_StopCreateIv    = 1 << 1,        //Only use if you know what you're doing, feed unique IVs here only

	EBufferEncryptionFlags_Count        = 2,

	EBufferEncryptionFlags_Invalid        = 0xFFFFFFFF << EBufferEncryptionFlags_Count

} EBufferEncryptionFlags;

typedef struct BufferEncrypt {

	//"Plaintext"/"Cyphertext" aka data to encrypt/decrypt (does so in place).
	//Leave empty to authenticate with AES.
	//16-byte alignment required.
	Buffer *restrict target;

	//Data that was/is supplied to verify integrity of the data.
	//16-byte alignment required
	const Buffer *restrict additionalData;

	//Only AES is currently supported (but both 256 and 128 is, though 128 only for legacy reasons).
	EBufferEncryptionType type;

	//Whether to use supplied keys or generate new ones (currently encryption only).
	EBufferEncryptionFlags flags;

	union {

		struct {
			const U32 *restrict key;          //Secret key; used to en/decrypt (AES256: U32[8], AES128: U32[4]).
			const I32x4 *restrict tag;        //Tag that was generated to verify integrity of encrypted data.
			const I32x4 *restrict iv;         //Iv was the 12-byte random number that was used to encrypt the data.
		} constDecrypt;

		//For Buffer_encryptAdvanced can be accessed only if the Generate flag is true.
		//Tag is always generated.
		struct NonConstEncrypt {
			U32 *restrict key;                 //& GenerateKey: Secret key; used to en/decrypt (AES256: U32[8], AES128: U32[4]).
			I32x4 *restrict tag;               //Tag is always generated if encryption type supports it (non zero).
			I32x4 *restrict iv;                //!(& StopCreateIv): Iv should be random 12 bytes. Generated unless flag is set.
		} nonConstEncrypt;
	};

} BufferEncrypt;

//Advanced encryption function, be very careful using this the wrong way.
//Encrypt function encrypts target into target (in place).
//Be careful about the following if iv and key are manually generated:
//- Don't reuse iv if supplied
//- Don't use the key too often (suggested <2^16 times)
//- Don't discard iv or key if any of them are generated
//- Don't discard tag or cut off too many bytes
//- additionalData and target must be 16-byte aligned
Bool Buffer_encryptAdvanced(const BufferEncrypt *restrict encrypt, Error *restrict e_rr);

//Advanced encryption function, be very careful using this the wrong way.
//Decrypt functions decrypt ciphertext from target into target (in place).
//Will return an error if the tag can't be verified
//Will clear the target if the function was unsuccessful.
//When decrypting, be sure of the following:
//- Don't use the data if the function returns false (s_ucceeded = false)!
//- additionalData and target must be 16-byte aligned
Bool Buffer_decryptAdvanced(const BufferEncrypt *restrict decrypt, Error *restrict e_rr);

//Expert AES functions; these are way more sensitive than the Auto and Advanced functions.
//They assume that:
//- You adhere to the basic rules of AES
//- You init, update and finalize a context before using the result (correctly)
//The context of important AES variables.
//And encrypting/decrypting blocks and verifying tags.
//The updates refuse what would silently break GCM (see Buffer_aesExpertEncUpdate); a refusal poisons the context, so its
// Finalize fails and leaves a zero tag. Everything else, such as IV uniqueness and key use, is up to the caller.
typedef struct AESEncryptionContext {

	I32x4 key[15];
	I32x4 tag;

	I32x4 H[16];

	I32x4 EKY0;

	I32x4 iv;

	EBufferEncryptionType encryptionType;

	U8 blockSizeMax;        //What Buffer_aesExpertCreate chose: H is expanded this far, so updates are clamped to it
	U8 use256Or512;
	U8 state;               //What the updates have seen so far, and whether one was refused
	U8 padding0;
	U32 padding1[2];

} AESEncryptionContext;

typedef union AESEncryptionKey {
	I32x4 aes256[2];
	I32x4 aes128;
	U32 u32x8[8];
} AESEncryptionKey;

//Be careful about the following:
//- Don't use the key too often (suggested <2^16 times)
//- Don't discard iv or key if any of them are generated
//- Don't discard tag or cut off too many bytes
Bool Buffer_aesExpertCreate(

	I32x4 iv,
	EBufferEncryptionType type,
	AESEncryptionKey key,

	//How many bytes are handled by ghash if positive
	// 0 means 'prefer large streams'
	// <0 is hardcoding block size (e.g. -1, -2, -4, -8, -16)
	I64 streamSizeHint,

	//If the decrypt is a one time, define this to non zero.
	//This will try to take into account the time the create takes as well (but requires streamSizeHint of >=0)
	// while streamSizeHint assumes the create overhead is nothing since a lot of data will be streamed.
	U64 oneTimeHint,

	U8 use256Or512Override,            //Put to 0xFF to detect
	U8 *restrict blockSizeMax,         //Outputs block size if requested
	U8 *restrict use256Or512,

	AESEncryptionContext *restrict ctx,
	Error *restrict e_rr
);

//Buffer's addr must be 16-byte aligned
//There's a (theoretical) limit of U64_MAX / 8 bytes for all combined data.
//blockSizeMax and use256Or512: what Buffer_aesExpertCreate output; larger values are clamped to them.
//Refused (false, and the context is poisoned) when misaligned, after any data, or after AAD that ended on a partial block:
// all AAD comes first, and only its last update may be a partial block.
Bool Buffer_aesExpertUpdateAAD(AESEncryptionContext *restrict ctx, Buffer data, U8 blockSizeMax, U8 use256Or512);

//Buffer's addr must be 16-byte aligned
//There's a 64GiB data limit that should be respected (needs key reroll)
//blockSizeMax and use256Or512: what Buffer_aesExpertCreate output; larger values are clamped to them.
//Refused (false, and the context is poisoned) when misaligned, after data that ended on a partial block (only the last
// update may be one), or when offsetInBlocks plus the data's blocks pass the 2^32 - 2 blocks GCM allows: the counter
// would wrap into the one that masks the tag.
Bool Buffer_aesExpertEncUpdate(
	AESEncryptionContext *restrict ctx, Buffer data, U32 offsetInBlocks, U8 blockSizeMax, U8 use256Or512
);

//Same rules as Buffer_aesExpertEncUpdate.
Bool Buffer_aesExpertDecUpdate(
	AESEncryptionContext *restrict ctx, Buffer data, U32 offsetInBlocks, U8 blockSizeMax, U8 use256Or512
);

//Don't use the data if the function returns false!
//Clear or remove the generated data if encryption failed, or risk exposing sensitive data.
//expectTag is the tag you expect.
//It's OK to ignore the result for encryption, since there's no valid tag yet: a context an update refused finalizes to a
// zero tag, which nothing verifies.
Bool Buffer_aesExpertFinalize(AESEncryptionContext *restrict ctx, U64 aadLen, U64 dataLen, I32x4 expectTag);

#ifdef __cplusplus
	}
#endif
