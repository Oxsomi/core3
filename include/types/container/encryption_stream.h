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

//types/container/encryption_stream.h

#pragma once
#include "types/container/stream.h"

#ifdef __cplusplus
	extern "C" {
#endif
		
typedef struct EncryptionStream {

	OxStream parent;

	StreamRef *dataStream;        //The physical stream represented by this virtual one.
	U64 startOffset;

	U32 encryptionKey[8];

	U32 rootIv[3];

	U32 chunkSize;

	U64 encryptionCount56ChunkSizeShift8;        //Low 8 bits: chunk-size shift. High 56 bits: encryption attempts.

	Buffer internalCache;        //sizeof(CryptoChunk) + chunkSize

} EncryptionStream;

static inline U8 EncryptionStream_chunkSizeShift(const EncryptionStream *encStream) {
	return (U8) encStream->encryptionCount56ChunkSizeShift8;
}

static inline U64 EncryptionStream_encryptionCount(const EncryptionStream *encStream) {
	return encStream->encryptionCount56ChunkSizeShift8 >> 8;
}

//Returns U64_MAX for an invalid chunk size or a logical size outside the 48-bit stream limit.
static inline U64 EncryptionStream_underlyingSize(U64 chunkSize, U64 size) {

	if (!chunkSize || (chunkSize & (chunkSize - 1)) || (chunkSize >> 32) || (size >> 48))
		return U64_MAX;

	U64 underlyingSize = (size / chunkSize) * (sizeof(CryptoChunk) + chunkSize);
	U64 leftOver = size & (chunkSize - 1);

	if (leftOver)
		underlyingSize += sizeof(CryptoChunk) + leftOver;

	return underlyingSize;
}

typedef RefPtr EncryptionStreamRef;

RefPtrType EncryptionStream_makeType(const Allocator *alloc);

//rootIV must be unique for each new output under the same raw encryption key.
//Each chunk encryption generates and stores a fresh random nonce and authenticates its position and length.
//The stream rejects more than 2^32 encryption attempts per handle. The caller must enforce the same aggregate
// limit across reopened handles and copies sharing a key and rootIV, then reencrypt under a fresh rootIV.
//Reads and writes of one encryption stream must be serialized, including against reserve.
//
//What the stream does NOT protect, which a format built on it has to:
//- Its size. Opened with a smaller size that ends on a chunk boundary, every chunk before it still verifies, so
//  dropping trailing chunks goes unnoticed. Open it with a size that was itself authenticated (oiDL and oiCA keep it
//  in their authenticated header).
//- Versions. A chunk is bound to its position, its length and its output, not to when it was written: after an
//  in-place rewrite under the same rootIV, the older ciphertext of a chunk with the same position and length still
//  verifies. A format that rewrites in place needs its own per-chunk version or hash to detect a rollback.
//- Weak keys. encryptionKey must be raw key material (32 random bytes): it is only expanded with HKDF, never
//  stretched, so a password has to go through a password KDF (Argon2, scrypt, PBKDF2) first.
Bool EncryptionStream_create(
	StreamRef *dataStream,
	U64 streamOffset,                //Must be 16 byte aligned
	const U32 encryptionKey[8],
	I32x4 rootIV,
	U64 chunkSize,                    //A power of two above 32 KiB and below 4 GiB
	U64 size,                         //Size of already encrypted data at streamOffset (in virtual bytes)
	const RefPtrType *type,
	EncryptionStreamRef **encStream,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
