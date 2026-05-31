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

//types/container/stream.c

#include "types/container/stream.h"
#include "types/container/buffer.h"
#include "types/container/types.h"
#include "types/base/mathi.h"

void Stream_close(OxStream *stream, const Allocator *alloc) {

	if (!stream)
		return;

	if (stream->close)
		stream->close(stream, alloc);

	*stream = (OxStream) { 0 };
}

RefPtrType Stream_inheritType(const Allocator *alloc, U32 extraSize) {

	if (sizeof(OxStream) + extraSize >= U32_MAX)
		return (RefPtrType) { 0 };

	return (RefPtrType) {
		.typeId = (ETypeId)EContainerTypeId_Stream,
		.length = (U32)(sizeof(OxStream) + extraSize),
		.alloc = alloc,
		.free = (ObjectFreeFunc) Stream_close
	};
}

Bool Stream_create(
	StreamFunc read,
	StreamFunc write,
	StreamReserveFunc reserve,
	StreamCloseFunc close,
	U64 streamSize,
	EStreamType streamType,
	const RefPtrType *type,
	StreamRef **streamRef,
	Error *e_rr
) {

	Bool s_uccess = true;

	if ((!write && !read) || !streamRef)
		retError(clean, Error_nullPointer(!streamRef ? 5 : 0, "Stream_create()::stream and read or write are required"));

	if (
		!type ||
		type->typeId != (ETypeId)EContainerTypeId_Stream ||
		type->length < sizeof(OxStream) ||
		type->free != (ObjectFreeFunc)Stream_close
	)
		retError(clean, Error_invalidParameter(4, 0, "Stream_create()::type is invalid"));

	if (streamSize >> 48)
		retError(clean, Error_invalidParameter(5, 0, "Stream_create()::streamSize too big"));

	gotoIfError3(clean, RefPtr_create(type, streamRef, e_rr));

	*RefPtr_data(*streamRef, OxStream) = (OxStream) {
		.read = read,
		.write = write,
		.reserve = reserve,
		.close = close,
		.size = streamSize,
		.streamType = streamType
	};

clean:
	return s_uccess;
}

Bool StreamCursor_create(
	StreamRef *stream,
	U64 cacheSize,
	Bool writeOnly,
	const Allocator *alloc,
	StreamCursor *cursor,
	Error *e_rr
) {

	Bool s_uccess = true;
	Buffer buf = Buffer_createNull();

	if (!cacheSize)                    //Default cache size 128KiB
		cacheSize = 128 * KIBI;

	gotoIfError3(clean, Buffer_createUninitializedBytes(cacheSize, alloc, &buf, e_rr));
	gotoIfError3(clean, StreamCursor_createWithCache(stream, &buf, writeOnly, cursor, e_rr));

clean:
	Buffer_free(&buf, alloc);
	return s_uccess;
}

Bool StreamCursor_createWithCache(
	StreamRef *stream,
	Buffer *cache,
	Bool writeOnly,
	StreamCursor *cursor,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool inc = false;

	if (!cursor || !cache)
		retError(clean, Error_nullPointer(!cache ? 1 : 4, "StreamCursor_createWithCache()::cursor is required"));

	if(!stream || stream->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream)
		retError(clean, Error_nullPointer(3, "StreamCursor_createWithCache()::stream is required"));

	OxStream *streamPtr = RefPtr_data(stream, OxStream);

	if (streamPtr->streamType & EStreamType_DisableSeek)
		retError(clean, Error_unsupportedOperation(0, "StreamCursor_createWithCache()::DisableSeek streams are not yet supported"));

	RefPtr_inc(stream);
	inc = true;

	if (cursor->cacheData.ptr)
		retError(clean, Error_invalidParameter(3, 0, "StreamCursor_createWithCache()::cursor already initialized, indicating memleak"));

	U64 cacheSize = Buffer_length(*cache);

	if (cacheSize < 32 * KIBI)
		retError(clean, Error_invalidParameter(3, 0, "StreamCursor_createWithCache()::cache->length too small"));

	if (cacheSize >> 48)
		retError(clean, Error_invalidParameter(5, 0, "StreamCursor_createWithCache()::cacheSize too big"));

	*cursor = (StreamCursor) {
		.stream = stream,
		.cacheData = *cache,
		.lastLocation = U64_MAX,
		.lastWriteLocation = U64_MAX,
		.readOnly = !(writeOnly && streamPtr->write),
		.allowRead = streamPtr->read,
		.allowWrite = streamPtr->write
	};

	*cache = Buffer_createNull();

clean:

	if (inc && !s_uccess)
		RefPtr_dec(&stream);

	return s_uccess;
}

void StreamCursor_close(StreamCursor *cursor, const Allocator *alloc) {

	if (!cursor)
		return;

	StreamCursor_flush(cursor, alloc, NULL);
	Buffer_free(&cursor->cacheData, alloc);
	RefPtr_dec(&cursor->stream);

	*cursor = (StreamCursor) { 0 };
}

Bool StreamCursor_closeAndKeepCache(StreamCursor *cursor, const Allocator *alloc, Buffer *cache, Error *e_rr) {

	Bool s_uccess = true;

	if (!cache)
		retError(clean, Error_nullPointer(2, "StreamCursor_closeAndKeepCache()::cache is required"));

	if(cache->ptr)
		retError(clean, Error_invalidParameter(
			2, 0, "StreamCursor_closeAndKeepCache()::cache is non empty, indicating memleak"
		));

	gotoIfError3(clean, StreamCursor_flush(cursor, alloc, e_rr));

	if (cursor) {                    //Move cache data for next cursor
		*cache = cursor->cacheData;
		cursor->cacheData = Buffer_createNull();
	}

	StreamCursor_close(cursor, alloc);

clean:
	return s_uccess;
}

Bool StreamCursor_flush(StreamCursor *cursor, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!cursor)
		retError(clean, Error_nullPointer(0, "StreamCursor_flush()::cursor is required"));
	
	OxStream *stream = RefPtr_data(cursor->stream, OxStream);

	if (!stream || cursor->stream->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream)
		retError(clean, Error_nullPointer(0, "StreamCursor_flush()::input/output->stream is required"));

	//Only flush if we have pending writes

	if (stream->write && StreamCursor_canWrite(cursor) && cursor->lastWriteLocation != U64_MAX)
		gotoIfError3(clean, stream->write(
			stream,
			cursor->lastLocation,
			cursor->lastWriteLocation - cursor->lastLocation,
			cursor->cacheData,
			alloc,
			e_rr
		));

	cursor->lastLocation = U64_MAX;
	cursor->lastWriteLocation = U64_MAX;

clean:
	return s_uccess;
}

Bool StreamCursor_setReadOnly(StreamCursor *cursor, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!cursor)
		retError(clean, Error_nullPointer(0, "StreamCursor_setReadOnly()::cursor is required"));

	if (!cursor->allowRead)
		retError(clean, Error_unsupportedOperation(0, "Stream_setReadOnly()::stream doesn't support reading"));

	if (!StreamCursor_canWrite(cursor))
		goto clean;

	gotoIfError3(clean, StreamCursor_flush(cursor, alloc, e_rr));
	cursor->readOnly = true;

clean:
	return s_uccess;
}

Bool StreamCursor_setWritable(StreamCursor *cursor, Error *e_rr) {

	Bool s_uccess = true;

	if (!cursor)
		retError(clean, Error_nullPointer(0, "StreamCursor_setWritable()::cursor is required"));

	if (!cursor->allowWrite)
		retError(clean, Error_unsupportedOperation(0, "StreamCursor_setWritable()::cursor doesn't support writing"));

	if (StreamCursor_canWrite(cursor))
		goto clean;

	cursor->lastLocation = U64_MAX;
	cursor->lastWriteLocation = U64_MAX;
	cursor->readOnly = false;

clean:
	return s_uccess;
}

Bool StreamCursor_resize(StreamCursor *cursor, U64 newSize, Bool disableFlush, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!cursor || !cursor->cacheData.ptr)
		retError(clean, Error_nullPointer(0, "StreamCursor_resize()::cursor is required"));

	if (newSize < 32 * KIBI)
		retError(clean, Error_invalidParameter(1, 0, "StreamCursor_resize()::newSize is too small"));

	U64 prevLen = Buffer_length(cursor->cacheData);

	if (prevLen == newSize)
		goto clean;

	//Flush to function first

	if(!disableFlush)
		gotoIfError3(clean, StreamCursor_flush(cursor, alloc, e_rr));

	Buffer_free(&cursor->cacheData, alloc);
	gotoIfError3(clean, Buffer_createUninitializedBytes(newSize, alloc, &cursor->cacheData, e_rr));
	cursor->lastLocation = cursor->lastWriteLocation = U64_MAX;

clean:
	return s_uccess;
}

Bool StreamCursor_copyStream(
	StreamCursor *output,
	StreamCursor *input,
	U64 srcOff,
	U64 dstOff,
	U64 length,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	//Validate

	if (!output || !input)
		retError(clean, Error_nullPointer(!output ? 0 : 1, "StreamCursor_copyStream()::input and output are required"));

	OxStream *streamIn = RefPtr_data(input->stream, OxStream);
	OxStream *streamOut = RefPtr_data(output->stream, OxStream);

	if (
		!streamIn || !streamOut ||
		input->stream->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream ||
		output->stream->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream
	)
		retError(clean, Error_nullPointer(0, "StreamCursor_copyStream()::input/output->stream is required"));

	if (!streamOut->write)
		retError(clean, Error_nullPointer(0, "StreamCursor_copyStream()::outputStream->write is required"));

	if (!streamIn->read)
		retError(clean, Error_nullPointer(1, "StreamCursor_copyStream()::inputStream->read is required"));

	if (!StreamCursor_canRead(input))
		retError(clean, Error_invalidParameter(1, 0, "StreamCursor_copyStream()::inputStream must be readonly"));

	if (!StreamCursor_canWrite(output))
		retError(clean, Error_invalidParameter(0, 0, "StreamCursor_copyStream()::outputStream must be writable"));

	if ((srcOff >> 48) || (dstOff >> 48) || (length >> 48))
		retError(clean, Error_invalidParameter(2, 0, "StreamCursor_copyStream()::dst/src off limited to 48b"));

	U64 inputSize = streamIn->size;

	if (!length) {

		if (srcOff >= inputSize)
			retError(clean, Error_outOfBounds(2, srcOff, inputSize, "StreamCursor_copyStream()::srcOff out of bounds"));

		length = inputSize - srcOff;
	}

	U64 srcEnd = srcOff + length;

	if (srcEnd > inputSize)
		retError(clean, Error_outOfBounds(2, srcOff, inputSize, "StreamCursor_copyStream()::srcOff out of bounds"));

	U64 cacheSiz = Buffer_length(input->cacheData);

	//Copy

	while (length) {

		U64 len = U64_min(cacheSiz, length);

		gotoIfError3(clean, StreamCursor_read(input, Buffer_createNull(), srcOff, 0, len, false, alloc, e_rr));
		gotoIfError3(clean, StreamCursor_write(output, input->cacheData, 0, dstOff, len, false, alloc, e_rr));

		srcOff += len;
		dstOff += len;
		length -= len;
	}

clean:
	return s_uccess;
}

Bool StreamCursor_write(
	StreamCursor *cursor,
	Buffer buf,
	U64 srcOff,
	U64 dstOff,
	U64 length,
	Bool bypassCache,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	if (!cursor)
		retError(clean, Error_nullPointer(0, "StreamCursor_write()::cursor is required"));

	OxStream *stream = RefPtr_data(cursor->stream, OxStream);

	if (!stream || cursor->stream->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream || !stream->write)
		retError(clean, Error_nullPointer(0, "StreamCursor_write()::cursor->stream is required"));

	if (!StreamCursor_canWrite(cursor))
		retError(clean, Error_invalidParameter(0, 0, "StreamCursor_write()::cursor must be writable"));

	if ((srcOff >> 48) || (dstOff >> 48) || (length >> 48))
		retError(clean, Error_invalidParameter(2, 0, "StreamCursor_write()::dst/src off limited to 48b"));

	if (!length)
		length = Buffer_length(buf);

	if (dstOff + length > stream->size && !(stream->streamType & EStreamType_Resizable))
		retError(clean, Error_outOfBounds(
			3, dstOff + length, stream->size, "StreamCursor_write() out of bounds and stream isn't resizable"
		));

	//Fill buffer as efficiently as possible to avoid writes

	U64 cursorLen = Buffer_length(cursor->cacheData);
	U64 cursorEnd = 0;

	if (cursor->lastLocation != U64_MAX)
		cursorEnd = cursor->lastLocation + cursorLen;

	//If the writing dst is in our stream, we can write it in memory for the remaining bytes.
	//When the cache is full, we can flush it to disk / output.

	if (dstOff >= cursor->lastLocation && dstOff < cursorEnd && !bypassCache) {

		U64 dstRel = dstOff - cursor->lastLocation;

		//Non sequential write, aka there's a gap.
		//When this happens, we can't be sure that our stream contains any valid data in between.
		//So just to be sure we flush first.

		if (cursor->lastWriteLocation != U64_MAX && dstOff > cursor->lastWriteLocation) {
			gotoIfError3(clean, StreamCursor_flush(cursor, alloc, e_rr));
		}

		else {

			U64 bytesToCopy = U64_min(cursorLen - dstRel, length);

			if(cursor->cacheData.ptr != buf.ptr)
				Buffer_memcpy(
					Buffer_createRef(cursor->cacheData.ptrNonConst + dstRel, bytesToCopy),
					Buffer_createRefConst(buf.ptr + srcOff, bytesToCopy)
				);

			cursor->lastWriteLocation = U64_max(cursor->lastWriteLocation, dstOff + bytesToCopy);

			srcOff += bytesToCopy;
			dstOff += bytesToCopy;
			length -= bytesToCopy;
		}
	}

	if (!length)
		goto clean;

	//The part that can't fit in the stream but needs to go into the target, we can write directly

	if (length > cursorLen || bypassCache) {

		U64 len = bypassCache ? length : length - cursorLen;

		gotoIfError3(clean, stream->write(
			stream,
			dstOff,
			len,
			Buffer_createRef(buf.ptrNonConst + srcOff, len),
			alloc,
			e_rr
		));

		dstOff += len;
		srcOff += len;
		length -= len;
	}

	if (!length)
		goto clean;

	//Flush cache (if present), as we clearly don't have it in memory.

	if (cursor->lastLocation != U64_MAX) {

		gotoIfError3(clean, stream->write(
			stream,
			cursor->lastLocation,
			cursor->lastWriteLocation - cursor->lastLocation,
			cursor->cacheData,
			alloc,
			e_rr
		));

		cursor->lastLocation = cursor->lastWriteLocation = U64_MAX;
	}

	//The remainder of the file that can fit in the stream is loaded in two places.
	//This should be avoided by using File_read/FileHandle_read if the file is only accessed once and linearly.
	//However, in some cases, the file isn't necessarily read like that and/or only parts are accessed.
	//In that case, you don't want to load the entire file, but you might want to keep the next info ready just in case.
	// It would minimize IO access at the cost of memory and potentially unnecessary copies.

	cursor->lastLocation = dstOff;
	cursor->lastWriteLocation = dstOff + length;

	if(cursor->cacheData.ptr != buf.ptr)
		Buffer_memcpy(cursor->cacheData, Buffer_createRef(buf.ptrNonConst + srcOff, length));

clean:
	return s_uccess;
}

Bool StreamCursor_read(
	StreamCursor *cursor,
	Buffer buf,
	U64 srcOff,
	U64 dstOff,
	U64 length,
	Bool bypassCache,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	if (!cursor)
		retError(clean, Error_nullPointer(0, "StreamCursor_read()::cursor is required"));

	OxStream *stream = RefPtr_data(cursor->stream, OxStream);

	if (!stream || cursor->stream->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream || !stream->read)
		retError(clean, Error_nullPointer(0, "StreamCursor_read()::cursor->stream is required"));

	if (!StreamCursor_canRead(cursor))
		retError(clean, Error_invalidParameter(0, 0, "StreamCursor_read()::cursor must be readable"));

	if (Buffer_isConstRef(buf))
		retError(clean, Error_invalidParameter(1, 0, "StreamCursor_read()::buf must be writable"));

	if (!length)
		length = Buffer_length(buf);

	if ((srcOff >> 48) || (dstOff >> 48) || (length >> 48))
		retError(clean, Error_invalidParameter(2, 0, "StreamCursor_read()::dst/src off limited to 48b"));

	U64 srcEnd = srcOff + length;

	if (srcEnd > stream->size)
		retError(clean, Error_outOfBounds(2, srcEnd, stream->size, "StreamCursor_read()::stream out of bounds"));

	U64 dstEnd = dstOff + length;
	U64 bufLen = Buffer_length(buf);

	if (dstEnd > bufLen && bufLen)
		retError(clean, Error_outOfBounds(
			3, dstEnd, bufLen, "StreamCursor_read()::buf out of bounds"
		));

	U64 streamLen = Buffer_length(cursor->cacheData);

	if (!bufLen && length > streamLen)
		retError(clean, Error_outOfBounds(
			3, length, streamLen, "StreamCursor_read() has empty buf, requires whole stream to be in memory at the end"
		));

	//Fill buffer as efficiently as possible to avoid copies

	U64 streamEnd = 0;

	if (cursor->lastLocation != U64_MAX)
		streamEnd = U64_min(cursor->lastLocation + streamLen, stream->size);

	//If the reading src is in our stream, we can read it directly for the remaining bytes

	if (srcOff >= cursor->lastLocation && srcOff < streamEnd && !bypassCache) {

		U64 srcRel = srcOff - cursor->lastLocation;
		U64 bytesToCopy = U64_min(streamEnd - srcOff, length);

		if (bufLen)
			Buffer_memcpy(
				Buffer_createRef(buf.ptrNonConst + dstOff, bytesToCopy),
				Buffer_createRefConst(cursor->cacheData.ptr + srcRel, bytesToCopy)
			);

		//If bufLen isn't present, it will force the stream to start from this new position instead.
		if (bufLen || streamEnd - srcOff >= length) {
			srcOff += bytesToCopy;
			dstOff += bytesToCopy;
			length -= bytesToCopy;
		}
	}

	if (!length)
		goto clean;

	//The part that can't fit in the stream but needs to go into the target, we can load directly

	if (length > streamLen || bypassCache) {

		U64 len = bypassCache ? length : length - streamLen;

		if (bufLen)
			gotoIfError3(clean, stream->read(
				stream,
				srcOff,
				len,
				Buffer_createRef(buf.ptrNonConst + dstOff, len),
				alloc,
				e_rr
			));

		dstOff += len;
		srcOff += len;
		length -= len;
	}

	if (!length)
		goto clean;

	//The remainder of the file that can fit in the stream is loaded in two places.
	//This should be avoided by using File_read/FileHandle_read if the file is only accessed once and linearly.
	//However, in some cases, the file isn't necessarily read like that and/or only parts are accessed.
	//In that case, you don't want to load the entire file, but you might want to keep the next info ready just in case.
	// It would minimize IO access at the cost of memory and potentially unnecessary copies.

	cursor->lastLocation = srcOff;

	gotoIfError3(clean, stream->read(
		stream,
		srcOff,
		U64_min(streamLen, stream->size - srcOff),
		cursor->cacheData,
		alloc,
		e_rr
	));

	if (bufLen)
		Buffer_memcpy(Buffer_createRef(buf.ptrNonConst + dstOff, length), cursor->cacheData);

clean:
	return s_uccess;
}

Bool Stream_compare(
	StreamRef *a,
	StreamRef *b,
	U64 aOff,
	U64 bOff,
	U64 length,
	U64 chunkSize,          // 0 = sensible default (e.g. 64KiB)
	const Allocator *alloc,
	ECompareResult *result,
	Error *e_rr
) {
	Bool s_uccess = true;
	StreamCursor aCursor = (StreamCursor) { 0 };
	StreamCursor bCursor = (StreamCursor) { 0 };

	if (!a || !b || !result)
		retError(clean, Error_nullPointer(!a ? 0 : (!b ? 1 : 7), "Stream_compare()::a, b and result are required"));

	OxStream *aRaw = RefPtr_data(a, OxStream);
	OxStream *bRaw = RefPtr_data(b, OxStream);

	if ((aRaw->streamType & EStreamType_DisableSeek) || (bRaw->streamType & EStreamType_DisableSeek))
		retError(clean, Error_invalidOperation(0, "Stream_compare()::cannot compare non-seekable streams"));

	*result = ECompareResult_Eq;

	//Streams of different lengths are comparable: shorter is Lt

	U64 aSize = aRaw->size;
	U64 bSize = bRaw->size;

	//Clamp to available data from offset

	U64 aAvail = aOff < aSize ? aSize - aOff : 0;
	U64 bAvail = bOff < bSize ? bSize - bOff : 0;

	U64 aLen = length ? U64_min(length, aAvail) : aAvail;
	U64 bLen = length ? U64_min(length, bAvail) : bAvail;

	if (aLen != bLen) {
		*result = aLen < bLen ? ECompareResult_Lt : ECompareResult_Gt;
		goto clean;
	}

	if (!aLen)      //Both empty in range
		goto clean;

	gotoIfError3(clean, StreamCursor_create(a, chunkSize, false, alloc, &aCursor, e_rr));
	gotoIfError3(clean, StreamCursor_create(b, chunkSize, false, alloc, &bCursor, e_rr));

	U64 remaining = aLen;
	U64 aIt = aOff;
	U64 bIt = bOff;

	while (remaining) {

		U64 readNow = U64_min(remaining, Buffer_length(aCursor.cacheData));

		//Read chunk from a into cursor cache, then directly compare against b's chunk

		gotoIfError3(clean, StreamCursor_read(&aCursor, Buffer_createNull(), aIt, 0, readNow, false, alloc, e_rr));
		gotoIfError3(clean, StreamCursor_read(&bCursor, Buffer_createNull(), bIt, 0, readNow, false, alloc, e_rr));

		//Both cursors now have the chunk in cache; compare the cache windows directly

		Buffer aCached = Buffer_createRefConst(aCursor.cacheData.ptr, readNow);
		Buffer bCached = Buffer_createRefConst(bCursor.cacheData.ptr, readNow);

		ECompareResult cmp = Buffer_cmp(aCached, bCached);   // or however your API spells memcmp

		if (cmp != ECompareResult_Eq) {
			*result = cmp;
			goto clean;
		}

		aIt += readNow;
		bIt += readNow;
		remaining -= readNow;
	}

clean:
	StreamCursor_close(&aCursor, alloc);
	StreamCursor_close(&bCursor, alloc);
	return s_uccess;
}
