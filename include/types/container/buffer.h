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

//types/container/buffer.h

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
U32 Buffer_crc32cChained(const Buffer buf, U32 crcPrev);

U32 Buffer_crc32cFallbackChained(const Buffer buf, U32 crcPrev);		//In case of no native CRC32C, but don't manually call

static inline U32 Buffer_crc32cFallback(const Buffer buf) {		//In case of no native CRC32C, but don't manually call
	return Buffer_crc32cFallbackChained(buf, 0);
}

static inline U32 Buffer_crc32c(const Buffer buf) {
	return Buffer_crc32cChained(buf, 0);
}

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
