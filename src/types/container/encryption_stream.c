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

#include "types/base/mathi.h"
#include "types/base/mathf.h"
#include "types/math/vec4i.h"
#include "types/container/encryption_stream.h"
#include "types/container/types.h"
#include "types/container/buffer_encrypt.h"
#include "types/container/buffer.h"

//Implement Stream's functions

static Bool EncryptionStream_readInternal(		//Decrypt
	Stream *stream,
	U64 offset,
	U64 length,
	Buffer buf,
	const Allocator *alloc,
	Error *e_rr
) {
	Bool s_uccess = true;
	StreamCursor underlyingCursor = (StreamCursor) { 0 };

	EncryptionStream *encStream = (EncryptionStream*)stream;
	Bool open = false;

	if (offset + length > stream->size)
		retError(clean, Error_outOfBounds(
			1, offset + length, stream->size,
			"EncryptionStream_readInternal() out of bounds"
		));

	if (!length)
		length = Buffer_length(buf);

	if (length > Buffer_length(buf))
		retError(clean, Error_outOfBounds(
			2, length, Buffer_length(buf),
			"EncryptionStream_readInternal() buffer too small"
		));

	gotoIfError3(clean, StreamCursor_createWithCache(
		encStream->dataStream,
		&encStream->internalCache,
		false,
		&underlyingCursor,
		e_rr
	));

	open = true;

	U64 dstOff = offset;

	while (length) {

		U64 chunkId = dstOff >> encStream->chunkSizeShift;
		U64 offsetInChunk = dstOff & (encStream->chunkSize - 1);
		U64 bytesInChunk = U64_min(encStream->chunkSize - offsetInChunk, length);

		U64 chunkStart = chunkId << encStream->chunkSizeShift;
		U64 chunkEnd = U64_min((chunkId + 1) << encStream->chunkSizeShift, stream->size);
		U64 actualChunkSize = chunkEnd - chunkStart;

		U64 underlyingOffset = chunkId * (encStream->chunkSize + sizeof(CryptoChunk)) + encStream->startOffset;

		U64 readSize = actualChunkSize + sizeof(CryptoChunk);
		gotoIfError3(clean, StreamCursor_read(
			&underlyingCursor,
			Buffer_createNull(),
			underlyingOffset,
			0,
			readSize,
			false,
			alloc,
			e_rr
		));

		//Decrypt and verify

		const CryptoChunk *cryptoChunk = (const CryptoChunk*)(
			underlyingCursor.cacheData.ptr + (underlyingOffset - underlyingCursor.lastLocation)
		);

		I32x4 tag = cryptoChunk->tag;

		Buffer encryptedData = Buffer_createRef(
			underlyingCursor.cacheData.ptrNonConst +
			(underlyingOffset - underlyingCursor.lastLocation) + sizeof(CryptoChunk),
			actualChunkSize
		);

		I32x4 chunkIv = I32x4_xor(I32x4_load3(encStream->rootIv), I32x4_createFromU64x2(chunkId, 0));

		gotoIfError3(clean, Buffer_decryptAuto(
			&encryptedData,
			NULL,
			encStream->encryptionKey,
			tag,
			chunkIv,
			e_rr
		));

		Buffer_memcpy(
			Buffer_createRef(buf.ptrNonConst + (dstOff - offset), bytesInChunk),
			Buffer_createRefConst(encryptedData.ptr + offsetInChunk, bytesInChunk)
		);

		dstOff += bytesInChunk;
		length -= bytesInChunk;
	}

clean:

	if(open)
		StreamCursor_closeAndKeepCache(&underlyingCursor, alloc, &encStream->internalCache, NULL);

	return s_uccess;
}

static Bool EncryptionStream_writeInternal(
	Stream *stream,
	U64 offset,
	U64 length,
	Buffer buf,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	StreamCursor underlyingCursor = (StreamCursor) { 0 };
	Bool open = false;

	EncryptionStream *encStream = (EncryptionStream*)stream;

	if (length > Buffer_length(buf))
		retError(clean, Error_outOfBounds(
			2, length, Buffer_length(buf),
			"EncryptionStream_writeInternal() buffer too small"
		));

	if (!length)
		length = Buffer_length(buf);

	U64 requiredSize = offset + length;

	if (requiredSize > stream->size)
		stream->size = requiredSize;

	gotoIfError3(clean, StreamCursor_createWithCache(
		encStream->dataStream,
		&encStream->internalCache,
		true,
		&underlyingCursor,
		e_rr
	));

	open = true;

	U64 srcOff = 0;

	while (length) {

		//Same as read; determine chunkId, etc.

		U64 chunkId = (offset + srcOff) >> encStream->chunkSizeShift;
		U64 offsetInChunk = (offset + srcOff) & (encStream->chunkSize - 1);
		U64 bytesInChunk = U64_min(encStream->chunkSize - offsetInChunk, length);

		U64 chunkStart = chunkId << encStream->chunkSizeShift;
		U64 chunkEnd = U64_min((chunkId + 1) << encStream->chunkSizeShift, stream->size);
		U64 actualChunkSize = chunkEnd - chunkStart;

		U64 underlyingOffset = chunkId * (encStream->chunkSize + sizeof(CryptoChunk)) + encStream->startOffset;

		//If partial chunk write, need to read & decrypt previous data
		Bool isPartialWrite = offsetInChunk || bytesInChunk != actualChunkSize;

		Buffer chunkData = Buffer_createRef(underlyingCursor.cacheData.ptrNonConst + sizeof(CryptoChunk), actualChunkSize);

		if (isPartialWrite) {

			U64 readSize = actualChunkSize + sizeof(CryptoChunk);

			gotoIfError3(clean, StreamCursor_read(
				&underlyingCursor,
				Buffer_createNull(),
				underlyingOffset,
				0,
				readSize,
				false,
				alloc,
				e_rr
			));

			//Decrypt existing data

			const CryptoChunk *existingChunk = (const CryptoChunk*)underlyingCursor.cacheData.ptr;
			I32x4 chunkIv = I32x4_xor(I32x4_load3(encStream->rootIv), I32x4_createFromU64x2(chunkId, 0));

			gotoIfError3(clean, Buffer_decryptAuto(
				&chunkData,
				NULL,
				encStream->encryptionKey,
				existingChunk->tag,
				chunkIv,
				e_rr
			));

			gotoIfError3(clean, StreamCursor_setWritable(&underlyingCursor, e_rr));
		}

		//Modify

		Buffer_memcpy(
			Buffer_createRef(chunkData.ptrNonConst + offsetInChunk, bytesInChunk),
			Buffer_createRefConst(buf.ptr + srcOff, bytesInChunk)
		);

		//Encrypt

		I32x4 chunkIv = I32x4_xor(I32x4_load3(encStream->rootIv), I32x4_createFromU64x2(chunkId, 0));
		I32x4 tag = I32x4_zero();

		BufferEncrypt encrypt = (BufferEncrypt) {
			.target = &chunkData,
			.additionalData = NULL,
			.type = EBufferEncryptionType_AES256GCM,
			.flags = EBufferEncryptionFlags_None | EBufferEncryptionFlags_StopCreateIv,
			.nonConstEncrypt = {
				.key = encStream->encryptionKey,
				.tag = &tag,
				.iv = &chunkIv
			}
		};

		gotoIfError3(clean, Buffer_encryptAdvanced(&encrypt, e_rr));

		//Write tag and chunk

		CryptoChunk *cryptoChunk = (CryptoChunk*)underlyingCursor.cacheData.ptrNonConst;
		cryptoChunk->tag = tag;

		U64 writeSize = actualChunkSize + sizeof(CryptoChunk);
		gotoIfError3(clean, StreamCursor_write(
			&underlyingCursor,
			underlyingCursor.cacheData,
			0,
			underlyingOffset,
			writeSize,
			false,
			alloc,
			e_rr
		));

		srcOff += bytesInChunk;
		length -= bytesInChunk;
	}

clean:

	if(open)
		StreamCursor_closeAndKeepCache(&underlyingCursor, alloc, &encStream->internalCache, NULL);

	return s_uccess;
}

static void EncryptionStream_closeInternal(Stream *stream, const Allocator *alloc) {

	(void)alloc;

	EncryptionStream *encStream = (EncryptionStream*)stream;

	Buffer_clearAllSecure(Buffer_createRef(&encStream->rootIv, sizeof(encStream->rootIv)));
	Buffer_clearAllSecure(Buffer_createRef(&encStream->encryptionKey, sizeof(encStream->encryptionKey)));

	RefPtr_dec(&encStream->dataStream);
	Buffer_free(&encStream->internalCache, alloc);
}

static Bool EncryptionStream_reserveInternal(
	Stream *stream,
	U64 size,
	const Allocator *alloc,
	Error *e_rr
) {
	Bool s_uccess = true;

	EncryptionStream *encStream = (EncryptionStream*)stream;
	Stream *underlying = RefPtr_data(encStream->dataStream, Stream);

	if (!underlying->reserve)
		retError(clean, Error_unsupportedOperation(0, "EncryptionStream_reserveInternal()::underlying stream doesn't support reserve"));

	//Calculate how much space we need in underlying stream

	U64 chunks = (size + encStream->chunkSize - 1) >> encStream->chunkSizeShift;
	U64 underlyingSize = encStream->startOffset + size + (chunks * sizeof(CryptoChunk));

	gotoIfError3(clean, underlying->reserve(underlying, underlyingSize, alloc, e_rr));

clean:
	return s_uccess;
}

//Public encryption stream functions

RefPtrType EncryptionStream_makeType(const Allocator *alloc) {
	return Stream_inheritType(alloc, sizeof(EncryptionStream) - sizeof(Stream));
}

Bool EncryptionStream_create(
	StreamRef *dataStream,
	U64 streamOffset,
	const U32 encryptionKey[8],
	I32x4 rootIV,
	U64 chunkSize,
	const RefPtrType *type,
	EncryptionStreamRef **encStream,
	Error *e_rr
) {
	Bool s_uccess = true;
	Bool inc = false;
	Bool hasStream = false;

	if (!encStream)
		retError(clean, Error_nullPointer(6, "EncryptionStream_create()::encStream is required"));

	if (*encStream)
		retError(clean, Error_invalidOperation(
			0, "EncryptionStream_create()::encStream already initialized, indicating memleak"
		));

	if(streamOffset & 15)
		retError(clean, Error_invalidOperation(
			1, "EncryptionStream_create()::streamOffset must be 16-byte aligned"
		));

	if(chunkSize <= 32 * KIBI)
		retError(clean, Error_invalidParameter(4, 0, "EncryptionStream_create()::chunkSize is too small"));

	if(chunkSize >> 32)
		retError(clean, Error_invalidParameter(4, 0, "EncryptionStream_create()::chunkSize is too big"));

	if (!dataStream || dataStream->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream)
		retError(clean, Error_nullPointer(0, "EncryptionStream_create()::underlyingStream is required"));

	if (!encryptionKey)
		retError(clean, Error_nullPointer(2, "EncryptionStream_create()::encryptionKey is required"));

	if (!type || type->typeId != (ETypeId)EContainerTypeId_Stream)
		retError(clean, Error_nullPointer(6, "EncryptionStream_create()::type is required"));

	U8 chunkSizeShift = (U8)F64_log2((F64)chunkSize);

	if (!chunkSizeShift || chunkSize != ((U64)1 << chunkSizeShift))
		retError(clean, Error_invalidParameter(4, 0, "EncryptionStream_create()::chunkSize invalid"));

	Stream *underlying = RefPtr_data(dataStream, Stream);

	if (streamOffset > underlying->size)
		retError(clean, Error_invalidParameter(
			1, 0, "EncryptionStream_create()::streamOffset out of bounds (not at stream back)"
		));

	RefPtr_inc(dataStream);
	inc = true;

	U64 underlyingDataSize = underlying->size - streamOffset;
	U64 decryptedSize = 0;

	if (underlyingDataSize) {

		U64 bytesPerChunk = chunkSize + sizeof(CryptoChunk);
		U64 chunks = underlyingDataSize / bytesPerChunk;
		U64 remainder = underlyingDataSize % bytesPerChunk;

		decryptedSize = chunks * chunkSize;

		if (remainder > sizeof(CryptoChunk))
			decryptedSize += (remainder - sizeof(CryptoChunk));
	}

	// Use Stream_create to properly initialize
	gotoIfError3(clean, Stream_create(
		underlying->read ? EncryptionStream_readInternal : NULL,
		underlying->write ? EncryptionStream_writeInternal : NULL,
		underlying->reserve ? EncryptionStream_reserveInternal : NULL,
		EncryptionStream_closeInternal,
		decryptedSize,
		EStreamType_Encrypted | underlying->streamType,
		type,
		encStream,
		e_rr
	));

	hasStream = true;

	//EncryptionStream-specific

	EncryptionStream *es = RefPtr_data(*encStream, EncryptionStream);

	gotoIfError3(clean, Buffer_createUninitializedBytes(
		chunkSize + sizeof(CryptoChunk), type->alloc, &es->internalCache, e_rr
	));

	es->dataStream = dataStream;
	es->startOffset = streamOffset;

	for(U8 i = 0; i < 3; ++i)
		es->rootIv[i] = I32x4_get(rootIV, i);

	es->chunkSize = (U32)chunkSize;
	es->chunkSizeShift = chunkSizeShift;

	Buffer_memcpy(
		Buffer_createRef(es->encryptionKey, sizeof(es->encryptionKey)),
		Buffer_createRefConst(encryptionKey, sizeof(encryptionKey))
	);

clean:

	if (hasStream && !s_uccess)
		RefPtr_dec(encStream);

	if (inc && !s_uccess)
		RefPtr_dec(&dataStream);

	return s_uccess;
}
