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
#include "types/math/vec.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Buffer Buffer;
typedef struct Allocator Allocator;

typedef struct BitRef {
	U8 *ptr;
	U8 off, isConst, padding[6];
} BitRef;

Bool BitRef_get(BitRef b);
void BitRef_set(BitRef b);
void BitRef_reset(BitRef b);

void BitRef_setTo(BitRef b, Bool v);

//All these functions allocate, so Buffer_free them later

Error Buffer_createCopy(Buffer buf, Allocator alloc, Buffer *result);
Error Buffer_createZeroBits(U64 length, Allocator alloc, Buffer *result);		//Guaranteed to be 16-byte aligned
Error Buffer_createOneBits(U64 length, Allocator alloc, Buffer *result);

Error Buffer_createBits(U64 length, Bool value, Allocator alloc, Buffer *result);

Bool Buffer_free(Buffer *buf, Allocator alloc);
Error Buffer_createEmptyBytes(U64 length, Allocator alloc, Buffer *output);

Error Buffer_createUninitializedBytes(U64 length, Allocator alloc, Buffer *result);
Error Buffer_createSubset(Buffer buf, U64 offset, U64 length, Bool isConst, Buffer *output);

Bool Buffer_resize(Buffer *buf, U64 newLen, Bool preserveContents, Bool clearUnsetContents, Allocator alloc, Error *e_rr);

//Writing data

Error Buffer_combine(Buffer a, Buffer b, Allocator alloc, Buffer *output);

//UTF-8 helpers

typedef U32 UnicodeCodePoint;

typedef struct UnicodeCodePointInfo {
	U8 chars, bytes, padding[2];
	UnicodeCodePoint index;
} UnicodeCodePointInfo;

Error Buffer_readAsUTF8(Buffer buf, U64 i, UnicodeCodePointInfo *codepoint);
Error Buffer_writeAsUTF8(Buffer buf, U64 i, UnicodeCodePoint codepoint, U8 *bytes);
Error Buffer_readAsUTF16(Buffer buf, U64 i, UnicodeCodePointInfo *codepoint);
Error Buffer_writeAsUTF16(Buffer buf, U64 i, UnicodeCodePoint codepoint, U8 *bytes);

//If the threshold (%) is met, it is identified as (mostly) unicode.
//If isUTF16 is set, it will try to find 2-byte width encoding (UTF16),
//otherwise it uses 1-byte width (UTF8).
//Both UTF16 and UTF8 can be variable length per codepoint.

Bool Buffer_isUnicode(Buffer buf, F32 threshold, Bool isUTF16);
Bool Buffer_isUTF8(Buffer buf, F32 threshold);
Bool Buffer_isUTF16(Buffer buf, F32 threshold);

Bool Buffer_isAscii(Buffer buf);

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
U32 Buffer_crc32c(Buffer buf);

//Fowler-Noll-Vo-1A 64-bit (fast non HW accelerated hashes)

static const U64 Buffer_fnv1a64Offset = 0xCBF29CE484222325;
static const U64 Buffer_fnv1a64Prime = 0x00000100000001B3;

U64 Buffer_fnv1a64Single(U64 a, U64 hash);
U64 Buffer_fnv1a64(Buffer buf, U64 hash);		//Put hash as Buffer_fnv1a64Offset if none

//MD5

I32x4 Buffer_md5(Buffer buf);

//SHA256

void Buffer_sha256(Buffer buf, U32 output[8]);

//Encryption

typedef enum EBufferEncryptionType {
	EBufferEncryptionType_AES256GCM,		//Additional data; IV (96 bits), TAG (128 bits)
	EBufferEncryptionType_AES128GCM,		//^
	EBufferEncryptionType_Count
} EBufferEncryptionType;

U64 EBufferEncryptionType_getAdditionalData(EBufferEncryptionType type);		//For example; IV (96 bits), TAG (128 bits)

typedef enum EBufferEncryptionFlags {

	EBufferEncryptionFlags_None			= 0,
	EBufferEncryptionFlags_GenerateKey	= 1 << 0,
	EBufferEncryptionFlags_GenerateIv	= 1 << 1,

	EBufferEncryptionFlags_Count		= 2,

	EBufferEncryptionFlags_Invalid		= 0xFFFFFFFF << EBufferEncryptionFlags_Count

} EBufferEncryptionFlags;

//Encrypt function encrypts target into target
//Be careful about the following if iv and key are manually generated:
//- Don't reuse iv if supplied
//- Don't use the key too often (suggested <2^16 times)
//- Don't discard iv or key if any of them are generated
//- Don't discard tag or cut off too many bytes

Error Buffer_encrypt(
	Buffer target,					//"Plaintext" aka data to encrypt. Leave empty to authenticate with AES
	Buffer additionalData,			//Non-secret data. Data which can't be modified after enc w/o key
	EBufferEncryptionType type,		//Only AES is currently supported
	EBufferEncryptionFlags flags,	//Whether to use supplied keys or generate new ones
	U32 *key,						//Secret key; used to en/decrypt (AES256: U32[8], AES128: U32[4])
	I32x4 *iv,						//Iv should be random 12 bytes. Can be generated if flag is set
	I32x4 *tag						//Tag should be non-NULL if encryption type supports it.
);

//Decrypt functions decrypt ciphertext from target into target
//Will return an error if the tag can't be verified
//Will only return decrypted result into target if the function was successful
//When decrypting, be sure of the following:
//- Don't use the data if Error_none() wasn't returned!

Error Buffer_decrypt(
	Buffer target,						//"Cyphertext" aka data to decrypt. Leave empty to verify with AES256GCM
	Buffer additionalData,				//Data that was supplied to verify integrity of the data
	EBufferEncryptionType type,			//Only AES is currently supported
	const U32 *key,						//Secret key used to en/decrypt (AES256: U32[8], AES128: U32[4])
	I32x4 tag,							//Tag that was generated to verify integrity of encrypted data
	I32x4 iv							//Iv was the 12-byte random number that was used to encrypt the data
);

//Cryptographically secure random on a sized buffer

Bool Buffer_csprng(Buffer target);

/*
//Compression

typedef enum EBufferCompressionType {
	EBufferCompressionType_Brotli11,
	EBufferCompressionType_Count
} EBufferCompressionType;

typedef enum EBufferCompressionHint {
	EBufferCompressionHint_None,
	EBufferCompressionHint_UTF8,
	EBufferCompressionHint_Font,
	EBufferCompressionHint_Count
} EBufferCompressionHint;

Error Buffer_compress(
	Buffer target,
	EBufferCompressionType type,
	EBufferCompressionHint hint,
	Allocator allocator,
	Buffer *output
);

Error Buffer_decompress(
	Buffer target,
	EBufferCompressionType type,
	Allocator allocator,
	Buffer *output
);*/

#ifdef __cplusplus
	}
#endif
