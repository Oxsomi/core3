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

#pragma once
#include "types/container/stream.h"

#ifdef __cplusplus
	extern "C" {
#endif
		
typedef struct EncryptionStream {

	Stream parent;

	StreamRef *dataStream;		//The physical stream represented by this virtual one.
	U64 startOffset;

	U32 encryptionKey[8];

	U32 rootIv[3];

	U32 chunkSize;

	U8 chunkSizeShift;
	U8 pad[7];

	Buffer internalCache;		//sizeof(CryptoChunk) + chunkSize

} EncryptionStream;

static inline U64 EncryptionStream_underlyingSize(U64 chunkSize, U64 size) {		//chunkSize must be base2 and not 0

	U64 underlyingSize = (size / chunkSize) * (sizeof(CryptoChunk) + chunkSize);
	U64 leftOver = size & (chunkSize - 1);

	if (leftOver)
		underlyingSize += sizeof(CryptoChunk) + leftOver;

	return underlyingSize;
}

typedef RefPtr EncryptionStreamRef;

RefPtrType EncryptionStream_makeType(const Allocator *alloc);

Bool EncryptionStream_create(
	StreamRef *dataStream,
	U64 streamOffset,				//Must be 16 byte aligned
	const U32 encryptionKey[8],
	I32x4 rootIV,
	U64 chunkSize,					//Must be power of 2
	U64 size,						//Size of already encrypted data at streamOffset (in virtual bytes)
	const RefPtrType *type,
	EncryptionStreamRef **encStream,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
