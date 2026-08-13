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
#include "types/base/buffer_base.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Error Error;
typedef struct Allocator Allocator;

//All these functions allocate, so Buffer_free them later

Bool Buffer_createCopy(const Buffer buf, const Allocator *alloc, Buffer *result, Error *e_rr);

//What every allocation below is aligned to without asking for it, matching what malloc promises on 64 bit.
#define BUFFER_DEFAULT_ALIGNMENT 16

//The most the aligned variants can hand out; it's a byte of bookkeeping and a byte of slack per allocation.
#define BUFFER_ALIGN_MAX 128

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

//The allocator only promises what malloc does, which is 16 bytes on 64 bit targets.
//That isn't enough for a type declaring more, and SpinLock declares alignas(64) to keep locks off each
// other's cache lines, so any struct embedding one by value needs this.
//Writing through a pointer that isn't aligned for its type is undefined rather than merely slow, and
// compilers act on it: clang turns a struct assignment into aligned AVX stores and faults on the first
// address that isn't a multiple of 32.
//alignment must be a power of two and at most 128.
//
//headerOffset aligns result->ptr + headerOffset instead of result->ptr itself.
//RefPtr needs that, since it puts its own bookkeeping first and the object that wants the alignment after
// it; pass 0 when the buffer itself is what has to be aligned.
//
//Buffer_free releases the result like any other buffer: the aligned bit it carries is what tells it to walk
// back to the real base first, so there is nothing for a caller to remember.
//The one case that needs care is throwing the Buffer away and keeping only the pointer.
//Rebuilding it later with Buffer_createManagedPtr loses the bit, so call Buffer_markAligned before freeing.

Bool Buffer_createUninitializedBytesAligned(
	U64 length, U64 alignment, U64 headerOffset, const Allocator *alloc, Buffer *result, Error *e_rr
);

Bool Buffer_createEmptyBytesAligned(
	U64 length, U64 alignment, U64 headerOffset, const Allocator *alloc, Buffer *result, Error *e_rr
);
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
//    Passwords (limit size (not too low) to avoid DDOS and use pepper if applicable)            TODO:
//    https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html
//    https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html
//
//hash/crc32c: Hashmaps / performance critical hashing /
//    fast data integrity (encryption checksum / compression) when *NOT* dealing with adversaries
//
//hash/sha256: data integrity (encryption checksum / compression) when dealing with adversaries
//
//encryption/aes256: If you want to recover data that is essential (NOT PASSWORDS) but needs a key
//
//For more info:
//    https://cheatsheetseries.owasp.org/cheatsheets/Cryptographic_Storage_Cheat_Sheet.html

//CRC32 Castagnoli / iSCSI polynomial (0x82f63b78) not for ethernet/zip (0xedb88320)!
U32 Buffer_crc32cChained(const Buffer buf, U32 crcPrev);

U32 Buffer_crc32cFallbackChained(const Buffer buf, U32 crcPrev);        //In case of no native CRC32C, but don't manually call

static inline U32 Buffer_crc32cFallback(const Buffer buf) {        //In case of no native CRC32C, but don't manually call
	return Buffer_crc32cFallbackChained(buf, 0);
}

static inline U32 Buffer_crc32c(const Buffer buf) {
	return Buffer_crc32cChained(buf, 0);
}

//Fowler-Noll-Vo-1A 64-bit (fast non HW accelerated hashes)

static const U64 Buffer_fnv1a64Offset = 0xCBF29CE484222325;
static const U64 Buffer_fnv1a64Prime = 0x00000100000001B3;

U64 Buffer_fnv1a64Single(U64 a, U64 hash);
U64 Buffer_fnv1a64(const Buffer buf, U64 hash);        //Put hash as Buffer_fnv1a64Offset if none

//MD5

typedef struct MD5Hash { U32 v[4]; } MD5Hash;

MD5Hash Buffer_md5(const Buffer buf);

//SHA256

void Buffer_sha256(const Buffer buf, U32 output[8]);
void Buffer_sha256Fallback(const Buffer buf, U32 *output);        //In case of no native SHA256, but don't manually call

//Cryptographically secure random on a sized buffer

Bool Buffer_csprng(const Buffer target);

#ifdef __cplusplus
	}
#endif
