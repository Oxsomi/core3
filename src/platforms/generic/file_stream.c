/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/ext/listx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "types/container/buffer.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/math/math.h"

Bool FileStream_read(Stream *stream, U64 offset, U64 length, Buffer buf, Allocator alloc, Error *e_rr) {
	
	Bool s_uccess = true;

	if(!stream)
		retError(clean, Error_nullPointer(0, "FileStream_read()::stream is required"))

	(void) alloc;

	gotoIfError3(clean, FileHandle_read(&stream->handle, offset, length, buf, e_rr))

clean:
	return s_uccess;
}

Bool FileStream_write(Stream *stream, U64 offset, U64 length, Buffer buf, Allocator alloc, Error *e_rr) {
	
	Bool s_uccess = true;

	if(!stream)
		retError(clean, Error_nullPointer(0, "FileStream_write()::stream is required"))

	(void) alloc;

	gotoIfError3(clean, FileHandle_write(&stream->handle, buf, offset, length, e_rr))

clean:
	return s_uccess;
}

Bool File_openStream(CharString loc, Ns timeout, EFileOpenType type, Bool create, U64 cache, Stream *output, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;

	if(!output)
		retError(clean, Error_nullPointer(5, "File_openStream()::output is required"))

	if(output->handle.filePath.ptr)
		retError(clean, Error_invalidParameter(5, 0, "File_openStream()::output already defined, might be a memleak"))

	output->lastLocation = output->lastWriteLocation = U64_MAX;

	if(type == EFileOpenType_Read)
		output->read = FileStream_read;

	else output->write = FileStream_write;

	gotoIfError3(clean, File_open(loc, timeout, type, create, &output->handle, e_rr))
	allocated = true;

	if(!cache)
		cache = 256 * KIBI;

	gotoIfError2(clean, Buffer_createUninitializedBytesx(cache, &output->cacheData))

clean:

	if(!s_uccess & allocated)
		Stream_close(output);

	return s_uccess;
}

Bool FileHandle_openStream(FileHandle *handle, U64 cache, Stream *stream, Error *e_rr) {

	Bool s_uccess = true;

	if(!stream)
		retError(clean, Error_nullPointer(5, "File_openStream()::stream is required"))

	if(stream->handle.filePath.ptr)
		retError(clean, Error_invalidParameter(5, 0, "File_openStream()::stream already defined, might be a memleak"))

	if(!cache)
		cache = 256 * KIBI;

	gotoIfError2(clean, Buffer_createUninitializedBytesx(cache, &stream->cacheData))

	if(stream->handle.type == EFileOpenType_Read)
		stream->read = FileStream_read;

	else stream->write = FileStream_write;

	stream->lastLocation = stream->lastWriteLocation = U64_MAX;
	stream->handle = *handle;
	*handle = (FileHandle) { 0 };

clean:
	return s_uccess;
}

Bool Stream_write(Stream *stream, Buffer buf, U64 srcOff, U64 dstOff, U64 length, Bool bypassCache, Error *e_rr) {
	
	Bool s_uccess = true;

	if(!stream)
		retError(clean, Error_nullPointer(0, "Stream_write()::stream is required"))

	if(stream->handle.type != EFileOpenType_Read)
		retError(clean, Error_invalidParameter(0, 0, "Stream_write()::stream must be readonly"))

	if((srcOff >> 48) || (dstOff >> 48) || (length >> 48))
		retError(clean, Error_invalidParameter(2, 0, "Stream_read()::dst/src off limited to 48b"))

	U64 dstEnd = dstOff + length;

	if(dstEnd > stream->handle.fileSize)
		retError(clean, Error_outOfBounds(
			2, dstEnd, stream->handle.fileSize, "Stream_read()::stream out of bounds"
		))
		
	U64 srcEnd = srcOff + length;

	if(srcEnd > Buffer_length(buf))
		retError(clean, Error_outOfBounds(
			3, srcEnd, Buffer_length(buf), "Stream_read()::buf out of bounds"
		))
		
	//Fill buffer as efficiently as possible to avoid writes

	U64 streamLen = Buffer_length(stream->cacheData);
	U64 streamEnd = 0;

	if (stream->lastLocation != U64_MAX)
		streamEnd = U64_min(stream->lastLocation + streamLen, stream->handle.fileSize);

	//If the writing dst is in our stream, we can write it in memory for the remaining bytes.
	//When the cache is full, we can flush it to disk / output.

	if (dstOff >= stream->lastLocation && dstOff < streamEnd && !bypassCache) {

		U64 dstRel = dstOff - stream->lastLocation;
		U64 bytesToCopy = U64_min(streamLen - dstRel, length);

		Buffer_copy(
			Buffer_createRef((U8*)stream->cacheData.ptr + dstOff, bytesToCopy),
			Buffer_createRefConst(buf.ptr + srcOff, bytesToCopy)
		);

		srcOff += bytesToCopy;
		dstOff += bytesToCopy;
		length -= bytesToCopy;

		stream->lastWriteLocation = dstOff;
	}

	if(!length)
		goto clean;

	//The part that can't fit in the stream but needs to go into the target, we can write directly

	if(length > streamLen || bypassCache) {

		U64 len = bypassCache ? length : length - streamLen;

		gotoIfError3(clean, stream->write(
			stream,
			dstOff,
			len,
			Buffer_createRef((U8*)buf.ptr + srcOff, len),
			Platform_instance->alloc,
			e_rr
		))

		dstOff += len;
		srcOff += len;
		length -= len;
	}

	if(!length)
		goto clean;

	//Flush cache (if present), as we clearly don't have it in memory.

	if(stream->lastLocation != U64_MAX) {

		stream->write(
			stream,
			stream->lastLocation,
			stream->lastWriteLocation - stream->lastLocation,
			stream->cacheData,
			Platform_instance->alloc,
			NULL
		);

		stream->lastLocation = stream->lastWriteLocation = U64_MAX;
	}

	//The remainder of the file that can fit in the stream is loaded in two places.
	//This should be avoided by using File_read/FileHandle_read if the file is only accessed once and linearly.
	//However, in some cases, the file isn't necessarily read like that and/or only parts are accessed.
	//In that case, you don't want to load the entire file, but you might want to keep the next info ready just in case.
	// It would minimize IO access at the cost of memory and potentially unnecessary copies.

	stream->lastLocation = dstOff;
	stream->lastWriteLocation = dstOff + length;

	Buffer_copy(stream->cacheData, Buffer_createRef((U8*)buf.ptr + srcOff, length));

clean:
	return s_uccess;
}

Bool Stream_read(Stream *stream, Buffer buf, U64 srcOff, U64 dstOff, U64 length, Bool bypassCache, Error *e_rr) {
	
	Bool s_uccess = true;

	if(!stream)
		retError(clean, Error_nullPointer(0, "Stream_read()::stream is required"))

	if(stream->handle.type != EFileOpenType_Read)
		retError(clean, Error_invalidParameter(0, 0, "Stream_read()::stream must be readonly"))

	if(Buffer_isConstRef(buf))
		retError(clean, Error_invalidParameter(1, 0, "Stream_read()::buf must be writable"))

	if(!length)
		length = Buffer_length(buf);

	if((srcOff >> 48) || (dstOff >> 48) || (length >> 48))
		retError(clean, Error_invalidParameter(2, 0, "Stream_read()::dst/src off limited to 48b"))

	U64 srcEnd = srcOff + length;

	if(srcEnd > stream->handle.fileSize)
		retError(clean, Error_outOfBounds(
			2, srcEnd, stream->handle.fileSize, "Stream_read()::stream out of bounds"
		))
		
	U64 dstEnd = dstOff + length;
	U64 bufLen = Buffer_length(buf);

	if(dstEnd > bufLen && bufLen)
		retError(clean, Error_outOfBounds(
			3, dstEnd, bufLen, "Stream_read()::buf out of bounds"
		))

	U64 streamLen = Buffer_length(stream->cacheData);

	if(!bufLen && length > streamLen)
		retError(clean, Error_outOfBounds(
			3, length, streamLen, "Stream_read() has empty buf, requires whole stream to be in memory at the end"
		))

	//Fill buffer as efficiently as possible to avoid copies

	U64 streamEnd = 0;

	if (stream->lastLocation != U64_MAX)
		streamEnd = U64_min(stream->lastLocation + streamLen, stream->handle.fileSize);

	//If the reading src is in our stream, we can read it directly for the remaining bytes

	if (srcOff >= stream->lastLocation && srcOff < streamEnd && !bypassCache) {

		U64 srcRel = srcOff - stream->lastLocation;
		U64 bytesToCopy = U64_min(streamEnd - srcOff, length);

		if(bufLen)
			Buffer_copy(
				Buffer_createRef((U8*)buf.ptr + dstOff, bytesToCopy),
				Buffer_createRefConst(stream->cacheData.ptr + srcRel, bytesToCopy)
			);

		//If bufLen isn't present, it will force the stream to start from this new position instead.
		if(bufLen || streamEnd - srcOff >= length) {
			srcOff += bytesToCopy;
			dstOff += bytesToCopy;
			length -= bytesToCopy;
		}
	}

	if(!length)
		goto clean;

	//The part that can't fit in the stream but needs to go into the target, we can load directly

	if(length > streamLen || bypassCache) {

		U64 len = bypassCache ? length : length - streamLen;

		if(bufLen)
			gotoIfError3(clean, stream->read(
				stream,
				srcOff,
				len,
				Buffer_createRef((U8*)buf.ptr + dstOff, len),
				Platform_instance->alloc,
				e_rr
			))

		dstOff += len;
		srcOff += len;
		length -= len;
	}

	if(!length)
		goto clean;

	//The remainder of the file that can fit in the stream is loaded in two places.
	//This should be avoided by using File_read/FileHandle_read if the file is only accessed once and linearly.
	//However, in some cases, the file isn't necessarily read like that and/or only parts are accessed.
	//In that case, you don't want to load the entire file, but you might want to keep the next info ready just in case.
	// It would minimize IO access at the cost of memory and potentially unnecessary copies.

	stream->lastLocation = srcOff;

	gotoIfError3(clean, stream->read(
		stream,
		srcOff,
		U64_min(streamLen, stream->handle.fileSize - srcOff),
		stream->cacheData,
		Platform_instance->alloc,
		e_rr
	))
	
	if(bufLen)
		Buffer_copy(Buffer_createRef((U8*)buf.ptr + dstOff, length), stream->cacheData);

clean:
	return s_uccess;
}

void Stream_close(Stream *stream) {

	if(!stream)
		return;

	if (stream->lastWriteLocation != U64_MAX)
		stream->write(
			stream,
			stream->lastLocation,
			stream->lastWriteLocation - stream->lastLocation,
			stream->cacheData,
			Platform_instance->alloc,
			NULL
		);

	Buffer_freex(&stream->cacheData);
	FileHandle_close(&stream->handle);
}
