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

#pragma once
#include "types/base/constants.h"
#include "types/base/buffer.h"
#include "types/math/vec4.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Error Error;
typedef struct Allocator Allocator;

//All these functions allocate, so Buffer_free them later

Bool Buffer_createCopy(const Buffer buf, const Allocator *alloc, Buffer *result, Error *e_rr);

//Guaranteed to be 16-byte aligned
Bool Buffer_createZeroBits(U64 length, const Allocator *alloc, Buffer *result, Error *e_rr);

//Guaranteed to be 16-byte aligned
Bool Buffer_createOneBits(U64 length, const Allocator *alloc, Buffer *result, Error *e_rr);

static inline Bool Buffer_createBits(U64 length, Bool value, const Allocator *alloc, Buffer *result, Error *e_rr) {
	return !value ? Buffer_createZeroBits(length, alloc, result, e_rr) : Buffer_createOneBits(length, alloc, result, e_rr);
}

void Buffer_free(Buffer *buf, const Allocator *alloc);

static inline Bool Buffer_createEmptyBytes(U64 length, const Allocator *alloc, Buffer *output, Error *e_rr) {
	return Buffer_createZeroBits(length >> 61 ? U64_MAX : length << 3, alloc, output, e_rr);
}

Bool Buffer_createUninitializedBytes(U64 length, const Allocator *alloc, Buffer *result, Error *e_rr);
Bool Buffer_createSubset(Buffer buf, U64 offset, U64 length, Bool isConst, Buffer *output, Error *e_rr);

Bool Buffer_resize(
	Buffer *buf, U64 newLen, Bool preserveContents, Bool clearUnsetContents, const Allocator *alloc, Error *e_rr
);

//Writing data

Bool Buffer_combine(const Buffer *a, const Buffer *b, const Allocator *alloc, Buffer *output, Error *e_rr);

//UTF-8 helpers

typedef U32 UnicodeCodePoint;

typedef struct UnicodeCodePointInfo {
	U8 chars, bytes, padding[2];
	UnicodeCodePoint index;
} UnicodeCodePointInfo;

Bool Buffer_readAsUTF8(const Buffer buf, U64 i, UnicodeCodePointInfo *codepoint, Error *e_rr);
Bool Buffer_writeAsUTF8(const Buffer buf, U64 i, UnicodeCodePoint codepoint, U8 *bytes, Error *e_rr);
Bool Buffer_readAsUTF16(const Buffer buf, U64 i, UnicodeCodePointInfo *codepoint, Error *e_rr);
Bool Buffer_writeAsUTF16(const Buffer buf, U64 i, UnicodeCodePoint codepoint, U8 *bytes, Error *e_rr);

//If the threshold (%) is met, it is identified as (mostly) unicode.
//If isUTF16 is set, it will try to find 2-byte width encoding (UTF16),
//otherwise it uses 1-byte width (UTF8).
//Both UTF16 and UTF8 can be variable length per codepoint.

Bool Buffer_isUnicode(const Buffer buf, F32 threshold, Bool isUTF16);

static inline Bool Buffer_isUTF8(const Buffer buf, F32 threshold) { return Buffer_isUnicode(buf, threshold, false); }
static inline Bool Buffer_isUTF16(const Buffer buf, F32 threshold) { return Buffer_isUnicode(buf, threshold, true); }

//What hash & encryption functions are good for:
//
//argon2id (Unsupported):
//	Passwords (limit size (not too low) to avoid DDOS and use pepper if applicable)			TODO:
//	https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html
//	https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html
//
//hash/crc32c: Hashmaps / performance critical hashing /
//	fast data integrity (encryption checksum / compression) when *NOT* dealing with adversaries
//
//hash/sha256: data integrity (encryption checksum / compression) when dealing with adversaries
//
//encryption/aes256: If you want to recover data that is essential (NOT PASSWORDS) but needs a key
//
//For more info:
//	https://cheatsheetseries.owasp.org/cheatsheets/Cryptographic_Storage_Cheat_Sheet.html

//CRC32 Castagnoli / iSCSI polynomial (0x82f63b78) not for ethernet/zip (0xedb88320)!
U32 Buffer_crc32c(const Buffer buf);
U32 Buffer_crc32cFallback(const Buffer buf);		//In case of no native CRC32C, but don't manually call

//Fowler-Noll-Vo-1A 64-bit (fast non HW accelerated hashes)

static const U64 Buffer_fnv1a64Offset = 0xCBF29CE484222325;
static const U64 Buffer_fnv1a64Prime = 0x00000100000001B3;

U64 Buffer_fnv1a64Single(U64 a, U64 hash);
U64 Buffer_fnv1a64(const Buffer buf, U64 hash);		//Put hash as Buffer_fnv1a64Offset if none

//MD5

I32x4 Buffer_md5(const Buffer buf);

//SHA256

void Buffer_sha256(const Buffer buf, U32 output[8]);
void Buffer_sha256Fallback(const Buffer buf, U32 *output);		//In case of no native SHA256, but don't manually call

//Encryption

//AES256GCM encryption with auto generated iv.
//Encrypt function encrypts target into target (in place).
//Be careful about the following:
//- Don't use the key too often (suggested <2^16 times)
//- Don't discard iv or key if any of them are generated
//- Don't discard tag or cut off too many bytes
//- additionalData and target must be 16-byte aligned
Bool Buffer_encryptAuto(
	Buffer *target,
	const Buffer *additionalData,
	Bool generateKey,
	U32 key[8],
	I32x4 *tag,
	I32x4 *iv,
	Error *e_rr
);

//AES256GCM decryption.
//Decrypt functions decrypt ciphertext from target into target (in place).
//Will return an error if the tag can't be verified
//Will clear decrypted result if the function was successful.
//When decrypting, be sure of the following:
//- Don't use the data if the function returns false (s_ucceeded = false)!
//- additionalData and target must be 16-byte aligned
Bool Buffer_decryptAuto(
	Buffer *target,
	const Buffer *additionalData,
	const U32 key[8],
	I32x4 tag,
	I32x4 iv,
	Error *e_rr
);

typedef enum EBufferEncryptionType {
	EBufferEncryptionType_AES256GCM,		//Additional data; IV (96 bits), TAG (128 bits)
	EBufferEncryptionType_AES128GCM,		//^
	EBufferEncryptionType_Count
} EBufferEncryptionType;

typedef enum EBufferEncryptionFlags {

	EBufferEncryptionFlags_None			= 0,
	EBufferEncryptionFlags_GenerateKey	= 1 << 0,
	EBufferEncryptionFlags_StopCreateIv	= 1 << 1,		//Only use if you know what you're doing, feed unique IVs here only

	EBufferEncryptionFlags_Count		= 2,

	EBufferEncryptionFlags_Invalid		= 0xFFFFFFFF << EBufferEncryptionFlags_Count

} EBufferEncryptionFlags;

typedef struct BufferEncrypt {

	//"Plaintext"/"Cyphertext" aka data to encrypt/decrypt (does so in place). Leave empty to authenticate with AES.
	//16-byte alignment required.
	Buffer *target;

	//Data that was/is supplied to verify integrity of the data. 16-byte alignment required
	const Buffer *additionalData;

	//Only AES is currently supported (but both 256 and 128 is, though 128 only for legacy reasons).
	EBufferEncryptionType type;

	//Whether to use supplied keys or generate new ones (currently encryption only).
	EBufferEncryptionFlags flags;

	union {

		struct {
			const U32 *key;			//Secret key; used to en/decrypt (AES256: U32[8], AES128: U32[4]).
			const I32x4 *tag;		//Tag that was generated to verify integrity of encrypted data.
			const I32x4 *iv;		//Iv was the 12-byte random number that was used to encrypt the data.
		} constDecrypt;

		//For Buffer_encryptAdvanced can be accessed only if the Generate flag is true.
		//Tag is always generated.
		struct NonConstEncrypt {
			U32 *key;				//& GenerateKey: Secret key; used to en/decrypt (AES256: U32[8], AES128: U32[4]).
			I32x4 *tag;				//Tag is always generated if encryption type supports it (non zero).
			I32x4 *iv;				//!(& StopCreateIv): Iv should be random 12 bytes. Generated unless flag is set.
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
Bool Buffer_encryptAdvanced(const BufferEncrypt *encrypt, Error *e_rr);

//Advanced encryption function, be very careful using this the wrong way.
//Decrypt functions decrypt ciphertext from target into target (in place).
//Will return an error if the tag can't be verified
//Will clear the target if the function was unsuccessful.
//When decrypting, be sure of the following:
//- Don't use the data if the function returns false (s_ucceeded = false)!
//- additionalData and target must be 16-byte aligned
Bool Buffer_decryptAdvanced(const BufferEncrypt *decrypt, Error *e_rr);

//Performs AES sbox in constant time (no lookup tables).
U8 AES_sbox(U8 x);

//Performs AES subWord in constant time (no lookup tables).
U32 AES_subWord(U32 w);

//Expert AES functions; these are way more sensitive than the Auto and Advanced functions.
//They assume that:
//- You adhere to the basic rules of AES
//- You init, update and finalize a context before using the result (correctly)
//The context of important AES variables.
//And encrypting/decrypting blocks and verifying tags.
//These functions don't do any parameter checks since they're internal helper functions
typedef struct AESEncryptionContext {

	I32x4 key[15];

	I32x4 H[16];

	I32x4 EKY0;

	I32x4 tag;

	I32x4 iv;

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
	I64 blockSizeHint,
	U8 *blockSizeMax,		//Outputs block size if requested

	AESEncryptionContext *ctx,
	Error *e_rr
);

//Buffer's addr must be 16-byte aligned
//There's a (theoretical) limit of U64_MAX / 8 bytes for all combined data.
//blockSizeMax: See aesExpertCreate's blockSizeHint
void Buffer_aesExpertUpdateAAD(AESEncryptionContext *ctx, Buffer data, U8 blockSizeMax);

//Buffer's addr must be 16-byte aligned
//There's a 64GiB data limit that should be respected (needs key reroll)
void Buffer_aesExpertEncUpdate(
	AESEncryptionContext *ctx,
	Buffer data,
	U32 offsetInBlocks,
	U8 blockSizeMax,
	EBufferEncryptionType type
);

//Buffer's addr must be 16-byte aligned
//There's a 64GiB data limit that should be respected (needs key reroll)
void Buffer_aesExpertDecUpdate(
	AESEncryptionContext *ctx,
	Buffer data,
	U32 offsetInBlocks,
	U8 blockSizeMax,
	EBufferEncryptionType type
);

//Don't use the data if the function returns false!
//Clear or remove the generated data if encryption failed, or risk exposing sensitive data.
//expectTag is the tag you expect. It's OK to ignore the result for encryption, since there's no valid tag yet.
Bool Buffer_aesExpertFinalize(AESEncryptionContext *ctx, U64 aadLen, U64 dataLen, I32x4 expectTag);

//Cryptographically secure random on a sized buffer

Bool Buffer_csprng(const Buffer target);

/*
//Compression

typedef enum EBufferCompressionType {
	EBufferCompressionType_Brotli11,
	//EBufferCompressionType_Brotli1,
	EBufferCompressionType_Count
} EBufferCompressionType;

typedef enum EBufferCompressionHint {
	EBufferCompressionHint_None,
	EBufferCompressionHint_UTF8,
	EBufferCompressionHint_Font,
	EBufferCompressionHint_Count	//Auto detect
} EBufferCompressionHint;

typedef struct BufferCompress {
	Buffer *target;
	EBufferCompressionType type;
	const Allocator *allocator;
	Buffer *output;
} BufferCompress;

Bool Buffer_compress(const BufferCompress *compress, EBufferCompressionHint hint, Error *e_rr);
Bool Buffer_decompress(const BufferCompress *compress, Error *e_rr);*/

#ifdef __cplusplus
	}
#endif
