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

//types/container/encryption_stream.c

#include "types/base/mathi.h"
#include "types/math/vec4i.h"
#include "types/container/encryption_stream.h"
#include "types/container/container_types.h"
#include "types/container/buffer_encrypt.h"
#include "types/container/buffer.h"

//AAD is an explicitly serialized record, independent of C structure padding.
static void EncryptionStream_aad(const EncryptionStream *stream, U64 chunkId, U64 length, U8 aad[48]) {

	//The label fills aad[0, 16) exactly, the root iv starts right after it
	static const U8 DOMAIN[] = "OxC3 chunk data1";
	static_assert(sizeof(DOMAIN) - 1 == 16, "EncryptionStream_aad() expects a 16 byte domain label");

	Buffer_memcpy(Buffer_createRef(aad, 16), Buffer_createRefConst(DOMAIN, 16));
	Buffer_memcpy(Buffer_createRef(aad + 16, 12), Buffer_createRefConst(stream->rootIv, 12));

	for (U8 i = 0; i < 4; ++i)
		aad[28 + i] = (U8) (stream->chunkSize >> (8 * i));

	for (U8 i = 0; i < 8; ++i) {
		aad[32 + i] = (U8) (chunkId >> (8 * i));
		aad[40 + i] = (U8) (length >> (8 * i));
	}
}

static Bool EncryptionStream_loadChunk(
	EncryptionStream *stream, U64 chunkId, U64 length, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;

	alignas(16) U8 aad[48] = { 0 };

	const U64 offset = stream->startOffset + chunkId * ((U64) stream->chunkSize + sizeof(CryptoChunk));
	OxStream *underlying = RefPtr_data(stream->dataStream, OxStream);

	gotoIfError3(clean, Stream_read(underlying, offset, length + sizeof(CryptoChunk), stream->internalCache, alloc, e_rr));

	const CryptoChunk *chunk = (const CryptoChunk*) stream->internalCache.ptr;

	if (chunk->reserved)
		retError(clean, Error_invalidParameter(0, 0, "EncryptionStream_loadChunk()::reserved must be zero"));

	EncryptionStream_aad(stream, chunkId, length, aad);

	Buffer aadBuffer = Buffer_createRefConst(aad, sizeof(aad));
	Buffer data = Buffer_createRef(stream->internalCache.ptrNonConst + sizeof(CryptoChunk), length);

	gotoIfError3(clean, Buffer_decryptAuto(
		&data, &aadBuffer, stream->encryptionKey, chunk->tag, I32x4_load3(chunk->nonce), e_rr
	));

clean:
	return s_uccess;
}

static Bool EncryptionStream_readInternal(
	OxStream *stream, U64 offset, U64 length, Buffer buf, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;
	EncryptionStream *encStream = (EncryptionStream*) stream;
	const U8 chunkSizeShift = EncryptionStream_chunkSizeShift(encStream);

	if (!length)
		length = Buffer_length(buf);

	if (offset > stream->size || length > stream->size - offset)
		retError(clean, Error_outOfBounds(1, offset, stream->size, "EncryptionStream_readInternal()::range"));

	if (length > Buffer_length(buf) || (length && (!buf.ptr || Buffer_isConstRef(buf))))
		retError(clean, Error_invalidParameter(3, 0, "EncryptionStream_readInternal()::buffer must be writable and sized"));

	U64 dstOffset = 0;

	while (length) {

		const U64 chunkId = offset >> chunkSizeShift;
		const U64 chunkStart = chunkId << chunkSizeShift;
		const U64 chunkLength = U64_min(encStream->chunkSize, stream->size - chunkStart);
		const U64 inChunk = offset - chunkStart;
		const U64 copyLength = U64_min(chunkLength - inChunk, length);

		gotoIfError3(clean, EncryptionStream_loadChunk(encStream, chunkId, chunkLength, alloc, e_rr));

		Buffer_memcpy(
			Buffer_createRef(buf.ptrNonConst + dstOffset, copyLength),
			Buffer_createRefConst(encStream->internalCache.ptr + sizeof(CryptoChunk) + inChunk, copyLength)
		);

		offset += copyLength;
		dstOffset += copyLength;
		length -= copyLength;
	}

clean:
	Buffer_clearAllSecure(encStream->internalCache);
	return s_uccess;
}

static Bool EncryptionStream_writeInternal(
	OxStream *stream, U64 offset, U64 length, Buffer buf, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;
	EncryptionStream *encStream = (EncryptionStream*)stream;
	OxStream *underlying = RefPtr_data(encStream->dataStream, OxStream);

	alignas(16) U8 aad[48] = { 0 };

	if (!length)
		length = Buffer_length(buf);

	if ((offset >> 48) || length > (((U64) 1 << 48) - 1) - offset)
		retError(clean, Error_outOfBounds(1, offset, (U64) 1 << 48, "EncryptionStream_writeInternal()::range exceeds 48 bits"));

	if (length > Buffer_length(buf) || (length && !buf.ptr))
		retError(clean, Error_invalidParameter(3, 0, "EncryptionStream_writeInternal()::source buffer is too small"));

	if (!length)
		goto clean;

	const U64 previousSize = stream->size;
	const U64 requiredSize = offset + length;
	const U64 targetSize = U64_max(requiredSize, previousSize);
	const U64 physicalSize = EncryptionStream_underlyingSize(encStream->chunkSize, targetSize);

	if (physicalSize >= ((U64) 1 << 48) - encStream->startOffset)
		retError(clean, Error_outOfBounds(1, physicalSize, (U64) 1 << 48, "EncryptionStream_writeInternal()::physical size"));

	if (!(underlying->streamType & EStreamType_Resizable) && physicalSize > underlying->size - encStream->startOffset)
		retError(clean, Error_outOfBounds(2, targetSize, previousSize, "EncryptionStream_writeInternal()::stream cannot grow"));

	//A sparse extension must also authenticate the zero-filled gap and the enlarged final chunk.
	const U8 chunkSizeShift = EncryptionStream_chunkSizeShift(encStream);
	const U64 encryptionCount = EncryptionStream_encryptionCount(encStream);
	const U64 firstChunk = U64_min(offset, previousSize) >> chunkSizeShift;
	const U64 lastChunk = (requiredSize - 1) >> chunkSizeShift;
	const U64 attempts = lastChunk - firstChunk + 1;
	const U64 maxAttempts = (U64) 1 << 32;

	if (encryptionCount > maxAttempts || attempts > maxAttempts - encryptionCount)
		retError(clean, Error_invalidState(0, "EncryptionStream_writeInternal()::key usage exhausted, replace rootIV"));

	for (U64 chunkId = firstChunk; chunkId <= lastChunk; ++chunkId) {

		const U64 chunkStart = chunkId << chunkSizeShift;
		const U64 chunkLength = U64_min(encStream->chunkSize, targetSize - chunkStart);
		const U64 oldLength = previousSize > chunkStart ? U64_min(encStream->chunkSize, previousSize - chunkStart) : 0;
		const U64 copyStart = U64_max(offset, chunkStart);
		const U64 copyEnd = U64_min(requiredSize, chunkStart + chunkLength);
		const Bool fullWrite = copyStart == chunkStart && copyEnd == chunkStart + chunkLength;

		if (oldLength && !fullWrite)
			gotoIfError3(clean, EncryptionStream_loadChunk(encStream, chunkId, oldLength, alloc, e_rr));

		U8 *dataPtr = encStream->internalCache.ptrNonConst + sizeof(CryptoChunk);

		if (chunkLength > oldLength)
			Buffer_unsetAllBits(Buffer_createRef(dataPtr + oldLength, chunkLength - oldLength), NULL);

		if (copyEnd > copyStart)
			Buffer_memcpy(
				Buffer_createRef(dataPtr + copyStart - chunkStart, copyEnd - copyStart),
				Buffer_createRefConst(buf.ptr + copyStart - offset, copyEnd - copyStart)
			);

		EncryptionStream_aad(encStream, chunkId, chunkLength, aad);

		Buffer aadBuffer = Buffer_createRefConst(aad, sizeof(aad));
		Buffer data = Buffer_createRef(dataPtr, chunkLength);
		I32x4 nonce = I32x4_zero();
		I32x4 tag = I32x4_zero();

		//Count before encryption, since a failed write may already have exposed its ciphertext.
		encStream->encryptionCount56ChunkSizeShift8 += (U64) 1 << 8;
		gotoIfError3(clean, Buffer_encryptAuto(&data, &aadBuffer, false, encStream->encryptionKey, &tag, &nonce, e_rr));

		CryptoChunk *chunk = (CryptoChunk*) encStream->internalCache.ptrNonConst;

		I32x4_store3(chunk->nonce, nonce);
		chunk->reserved = 0;
		chunk->tag = tag;

		const U64 physicalOffset = encStream->startOffset + chunkId * ((U64) encStream->chunkSize + sizeof(CryptoChunk));

		gotoIfError3(clean, Stream_write(
			underlying, physicalOffset, chunkLength + sizeof(CryptoChunk), encStream->internalCache, alloc, e_rr
		));

		stream->size = U64_max(stream->size, chunkStart + chunkLength);
	}

clean:
	Buffer_clearAllSecure(encStream->internalCache);
	return s_uccess;
}

static void EncryptionStream_closeInternal(OxStream *stream, const Allocator *alloc) {

	EncryptionStream *encStream = (EncryptionStream*)stream;

	Buffer_clearAllSecure(Buffer_createRef(encStream->rootIv, sizeof(encStream->rootIv)));
	Buffer_clearAllSecure(Buffer_createRef(encStream->encryptionKey, sizeof(encStream->encryptionKey)));
	Buffer_clearAllSecure(encStream->internalCache);
	RefPtr_dec(&encStream->dataStream);
	Buffer_free(&encStream->internalCache, alloc);
}

static Bool EncryptionStream_reserveInternal(OxStream *stream, U64 size, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	EncryptionStream *encStream = (EncryptionStream*)stream;
	OxStream *underlying = RefPtr_data(encStream->dataStream, OxStream);
	const U64 physicalSize = EncryptionStream_underlyingSize(encStream->chunkSize, size);

	if (!underlying->reserve)
		retError(clean, Error_unsupportedOperation(0, "EncryptionStream_reserveInternal()::underlying stream cannot reserve"));

	if (physicalSize >= ((U64) 1 << 48) - encStream->startOffset)
		retError(clean, Error_outOfBounds(1, size, (U64) 1 << 48, "EncryptionStream_reserveInternal()::size"));

	gotoIfError3(clean, underlying->reserve(underlying, encStream->startOffset + physicalSize, alloc, e_rr));

clean:
	return s_uccess;
}

RefPtrType EncryptionStream_makeType(const Allocator *alloc) {
	return Stream_inheritType(alloc, sizeof(EncryptionStream) - sizeof(OxStream));
}

Bool EncryptionStream_create(
	StreamRef *dataStream,
	U64 streamOffset,
	const U32 encryptionKey[8],
	I32x4 rootIV,
	U64 chunkSize,
	U64 size,
	const RefPtrType *type,
	EncryptionStreamRef **encStream,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool hasStream = false;

	if (!encStream || !dataStream || !encryptionKey || !type)
		retError(clean, Error_nullPointer(0, "EncryptionStream_create()::stream, key, type and output are required"));

	if (*encStream)
		retError(clean, Error_invalidOperation(0, "EncryptionStream_create()::output is already initialized"));

	if (dataStream->refPtrType->typeId != (TypeId) EContainerTypeId_Stream)
		retError(clean, Error_invalidParameter(0, 0, "EncryptionStream_create()::underlying stream type"));

	if (type->typeId != (TypeId) EContainerTypeId_Stream || RefPtrType_length(type) < sizeof(EncryptionStream))
		retError(clean, Error_invalidParameter(6, 0, "EncryptionStream_create()::type is too small"));

	if ((streamOffset & 15) || (streamOffset >> 48))
		retError(clean, Error_invalidParameter(1, 0, "EncryptionStream_create()::offset must be aligned and fit 48 bits"));

	//Chunks are found by shifting, so their size has to be a power of two, and it has to fit the U32 the stream stores

	if (chunkSize <= 32 * KIBI || (chunkSize & (chunkSize - 1)) || (chunkSize >> 32))
		retError(clean, Error_invalidParameter(
			4, 0, "EncryptionStream_create()::chunkSize must be a power of two above 32 KiB and below 4 GiB"
		));

	const U64 physicalSize = EncryptionStream_underlyingSize(chunkSize, size);

	if (physicalSize == U64_MAX)
		retError(clean, Error_invalidParameter(5, 0, "EncryptionStream_create()::size doesn't fit 48 bits"));

	OxStream *underlying = RefPtr_data(dataStream, OxStream);

	if (streamOffset > underlying->size || physicalSize > underlying->size - streamOffset)
		retError(clean, Error_outOfBounds(1, streamOffset, underlying->size, "EncryptionStream_create()::underlying range"));

	gotoIfError3(clean, Stream_create(
		underlying->read ? EncryptionStream_readInternal : NULL,
		underlying->write ? EncryptionStream_writeInternal : NULL,
		underlying->reserve ? EncryptionStream_reserveInternal : NULL,
		EncryptionStream_closeInternal, size,
		(EStreamType) ((underlying->streamType | EStreamType_Encrypted) & ~(U64) EStreamType_ConcurrentRead),
		type, encStream, e_rr
	));

	hasStream = true;
	EncryptionStream *es = RefPtr_data(*encStream, EncryptionStream);

	if (!RefPtr_inc(dataStream))
		retError(clean, Error_invalidState(0, "EncryptionStream_create()::underlying stream cannot be referenced"));

	es->dataStream = dataStream;
	es->startOffset = streamOffset;
	es->chunkSize = (U32) chunkSize;

	U8 chunkSizeShift = 0;

	for (U64 value = chunkSize; value > 1; value >>= 1)
		++chunkSizeShift;

	es->encryptionCount56ChunkSizeShift8 = chunkSizeShift;

	I32x4_store3(es->rootIv, rootIV);
	gotoIfError3(clean, Buffer_deriveEncryptionKey(
		encryptionKey, rootIV, EEncryptionKeyDomain_StreamData, es->encryptionKey, e_rr
	));

	gotoIfError3(clean, Buffer_createUninitializedBytes(
		chunkSize + sizeof(CryptoChunk), type->alloc, &es->internalCache, e_rr
	));

	Stream_setBlock(*encStream, (U32) chunkSize, EStreamReadCost_Decode);

clean:
	if (hasStream && !s_uccess)
		RefPtr_dec(encStream);

	return s_uccess;
}
