/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "math/vec.h"

typedef struct Buffer Buffer;
typedef struct Allocator Allocator;

typedef struct BitRef {
	U8 *ptr, off, isConst;
} BitRef;

Bool BitRef_get(BitRef b);
void BitRef_set(BitRef b);
void BitRef_reset(BitRef b);

void BitRef_setTo(BitRef b, Bool v);

Error Buffer_getBit(Buffer buf, U64 offset, Bool *output);

Bool Buffer_copy(Buffer dst, Buffer src);
Bool Buffer_revCopy(Buffer dst, Buffer src);		//Copies bytes from range backwards; useful if ranges overlap

Error Buffer_setBit(Buffer buf, U64 offset);
Error Buffer_resetBit(Buffer buf, U64 offset);

Error Buffer_setBitTo(Buffer buf, U64 offset, Bool value);

Error Buffer_bitwiseOr(Buffer dst, Buffer src);
Error Buffer_bitwiseAnd(Buffer dst, Buffer src);
Error Buffer_bitwiseXor(Buffer dst, Buffer src);
Error Buffer_bitwiseNot(Buffer dst);

Error Buffer_setBitRange(Buffer dst, U64 dstOff, U64 bits);
Error Buffer_unsetBitRange(Buffer dst, U64 dstOff, U64 bits);

Error Buffer_setAllBits(Buffer dst);
Error Buffer_unsetAllBits(Buffer dst);

Error Buffer_setAllBitsTo(Buffer buf, Bool isOn);

//Comparison

Error Buffer_eq(Buffer buf0, Buffer buf1, Bool *result);		//Also compares size
Error Buffer_neq(Buffer buf0, Buffer buf1, Bool *result);		//Also compares size

//These should never be Buffer_free-d because Buffer doesn't know if it's allocated

Buffer Buffer_createNull();

Buffer Buffer_createRef(void *v, U64 length);
Buffer Buffer_createConstRef(const void *v, U64 length);

//All these functions allocate, so Buffer_free them later

Error Buffer_createCopy(Buffer buf, Allocator alloc, Buffer *output);
Error Buffer_createZeroBits(U64 length, Allocator alloc, Buffer *output);
Error Buffer_createOneBits(U64 length, Allocator alloc, Buffer *output);

Error Buffer_createBits(U64 length, Bool value, Allocator alloc, Buffer *result);

Bool Buffer_free(Buffer *buf, Allocator alloc);
Error Buffer_createEmptyBytes(U64 length, Allocator alloc, Buffer *output);

Error Buffer_createUninitializedBytes(U64 length, Allocator alloc, Buffer *output);
Error Buffer_createSubset(Buffer buf, U64 offset, U64 length, Bool isConst, Buffer *output);

//Writing data

Error Buffer_offset(Buffer *buf, U64 length);

Error Buffer_append(Buffer *buf, const void *v, U64 length);
Error Buffer_appendBuffer(Buffer *buf, Buffer append);

Error Buffer_consume(Buffer *buf, void *v, U64 length);

Error Buffer_combine(Buffer a, Buffer b, Allocator alloc, Buffer *output);

#define _BUFFER_OP(T)						\
Error Buffer_append##T(Buffer *buf, T v);	\
Error Buffer_consume##T(Buffer *buf, T *v);

_BUFFER_OP(U64);
_BUFFER_OP(U32);
_BUFFER_OP(U16);
_BUFFER_OP(U8);

_BUFFER_OP(I64);
_BUFFER_OP(I32);
_BUFFER_OP(I16);
_BUFFER_OP(I8);

_BUFFER_OP(F64);
_BUFFER_OP(F32);

_BUFFER_OP(I32x2);
_BUFFER_OP(F32x2);
//_BUFFER_OP(F64x2);		TODO:

_BUFFER_OP(I32x4);
_BUFFER_OP(F32x4);
//_BUFFER_OP(F64x4);		TODO:

//UTF-8 helpers

typedef U32 UTF8CodePoint;

typedef struct UTF8CodePointInfo {
	U8 size;
	UTF8CodePoint index;
} UTF8CodePointInfo;

Error Buffer_readAsUTF8(Buffer buf, U64 i, UTF8CodePointInfo *codepoint);
Error Buffer_writeAsUTF8(Buffer buf, U64 i, UTF8CodePoint codepoint);

Bool Buffer_isUTF8(Buffer buf, F32 threshold);		//If the threshold (%) is met, it is identified as (mostly) UTF8

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
//encryption/aes256: If you wanna recover data that is essential (NOT PASSWORDS) but needs a key
//
//For more info: 
//	https://cheatsheetseries.owasp.org/cheatsheets/Cryptographic_Storage_Cheat_Sheet.html

//CRC32 Castagnoli / iSCSI polynomial (0x82f63b78) not for ethernet/zip (0xedb88320)!

U32 Buffer_crc32c(Buffer buf);

void Buffer_sha256(Buffer buf, U32 output[8]);

//Encryption

typedef enum EBufferEncryptionType {
	EBufferEncryptionType_AES256GCM,		//Additional data; IV (96 bits), TAG (128 bits)
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
//- Don't use the key too often (e.g. >2^32 times)
//- Don't discard iv or key if any of them are generated
//- Don't discard tag or cut off too many bytes

Error Buffer_encrypt(
	Buffer target,					//"Plaintext" aka data to encrypt. Leave empty to authenticate with AES256GCM
	Buffer additionalData,			//Non secret data. Data which can't be modified after enc w/o key
	EBufferEncryptionType type,		//Only AES256GCM is currently supported
	EBufferEncryptionFlags flags,	//Whether or not to use supplied keys or generate new ones
	U32 key[8],						//Secret key; used for encryption and decryption
	I32x4 *iv,						//Iv should be random 12 bytes. Can be generated if flag is set
	I32x4 *tag						//Tag should be non zero if encryption type supports it. 
);

//Decrypt functions decrypt ciphertext from target into target
//Will return an error if the tag can't be verified
//Will only return decrypted result into target if the function was successful
//When decrypting, be sure of the following:
//- Don't use the data if Error_none() wasn't returned!

Error Buffer_decrypt(
	Buffer target,						//"Cyphertext" aka data to decrypt. Leave empty to verify with AES256GCM
	Buffer additionalData,				//Data that was supplied to verify integrity of the data
	EBufferEncryptionType type,			//Only AES256GCM is currently supported
	const U32 key[8],					//Secret key used to encrypt; used for encryption and decryption
	I32x4 tag,							//Tag that was generated to verify integrity of encrypted data
	I32x4 iv							//Iv was the 12-byte random number that was used to encrypt the data
);

//Cryptographically secure random on a sized buffer

Bool Buffer_csprng(Buffer target);

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
);
