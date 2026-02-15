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

#include "types/container/memory_stream.h"
#include "types/container/buffer.h"
#include "types/container/types.h"
#include "types/base/mathi.h"

//Implement Stream's functions

static Bool MemoryStream_readInternal(
	Stream *stream,
	U64 offset,
	U64 length,
	Buffer buf,
	const Allocator *alloc,
	Error *e_rr
) {
	(void)alloc;
	Bool s_uccess = true;

	MemoryStream *memStream = (MemoryStream*)stream;

	if (offset + length > memStream->parent.size)
		retError(clean, Error_outOfBounds(
			1, offset + length, memStream->parent.size,
			"MemoryStream_readInternal() out of bounds"
		));

	if (!length)
		length = Buffer_length(memStream->data) - offset;

	if (length > Buffer_length(buf))
		retError(clean, Error_outOfBounds(
			3, length, Buffer_length(buf),
			"MemoryStream_readInternal() buffer too small"
		));

	Buffer_memcpy(buf, Buffer_createRefConst(memStream->data.ptr + offset, length));

clean:
	return s_uccess;
}

static Bool MemoryStream_writeInternal(
	Stream *stream,
	U64 offset,
	U64 length,
	Buffer buf,
	const Allocator *alloc,
	Error *e_rr
) {
	Bool s_uccess = true;
	
	MemoryStream *memStream = (MemoryStream*)stream;

	if (Buffer_isConstRef(memStream->data))
		retError(clean, Error_unsupportedOperation(0, "MemoryStream_writeInternal() stream is readonly"));

	if (length > Buffer_length(buf))
		retError(clean, Error_outOfBounds(
			2, length, Buffer_length(buf),
			"MemoryStream_writeInternal() buffer too small"
		));

	if (!length)
		length = Buffer_length(buf);

	//Expand buffer if needed

	U64 requiredSize = offset + length;
	U64 currentSize = Buffer_length(memStream->data);

	if (requiredSize > currentSize) {

		if(!(memStream->parent.streamType & EStreamType_Resizable) || Buffer_isRef(memStream->data))
			retError(clean, Error_unsupportedOperation(
				0, "MemoryStream_writeInternal() stream is not resizable and out of bounds"
			));

		Buffer newBuf = Buffer_createNull();
		gotoIfError3(clean, Buffer_createUninitializedBytes(requiredSize, alloc, &newBuf, e_rr));

		if (currentSize)
			Buffer_memcpy(
				Buffer_createRef(newBuf.ptrNonConst, currentSize),
				Buffer_createRefConst(memStream->data.ptr, currentSize)
			);

		Buffer_free(&memStream->data, alloc);
		memStream->data = newBuf;
		stream->size = requiredSize;
	}

	//If the buffer was already reserved, we'll only need to update the size.

	if (requiredSize > stream->size)
		stream->size = requiredSize;

	//Copy

	Buffer_memcpy(
		Buffer_createRef(memStream->data.ptrNonConst + offset, length),
		Buffer_createRefConst(buf.ptr, length)
	);

clean:
	return s_uccess;


}

static Bool MemoryStream_reserveInternal(Stream *stream, U64 size, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	MemoryStream *memStream = (MemoryStream*)stream;

	if (!(memStream->parent.streamType & EStreamType_Resizable))
		retError(clean, Error_unsupportedOperation(0, "MemoryStream_reserveInternal() stream is not resizable"));

	if (Buffer_isRef(memStream->data))
		retError(clean, Error_unsupportedOperation(0, "MemoryStream_reserveInternal() stream uses ref buffer"));

	U64 currentSize = Buffer_length(memStream->data);

	if (currentSize >= size)
		goto clean;

	Buffer newBuf = Buffer_createNull();
	gotoIfError3(clean, Buffer_createUninitializedBytes(size, alloc, &newBuf, e_rr));

	if (currentSize)
		Buffer_memcpy(
			Buffer_createRef(newBuf.ptrNonConst, currentSize),
			Buffer_createRefConst(memStream->data.ptr, currentSize)
		);

	Buffer_free(&memStream->data, alloc);
	memStream->data = newBuf;

	//Don't update stream->size, only the capacity

clean:
	return s_uccess;
}

static void MemoryStream_closeInternal(Stream *stream, const Allocator *alloc) {
	Buffer_free(&((MemoryStream*)stream)->data, alloc);
}

//Create helpers

Bool MemoryStream_createFromBuffer(
	Buffer *buffer,
	EMemoryStreamFlags flags,
	const RefPtrType *type,
	MemoryStreamRef **memStream,
	Error *e_rr
) {
	Bool s_uccess = true;

	if (!buffer)
		retError(clean, Error_nullPointer(0, "MemoryStream_createFromBuffer()::buffer is required"));

	if((flags & EMemoryStreamFlags_IsWritable) && Buffer_isConstRef(*buffer))
		retError(clean, Error_constData(
			0, 0, "MemoryStream_createFromBuffer()::buffer is readonly but flags contained IsWritable"
		));

	if((flags & EMemoryStreamFlags_IsResizable) && Buffer_isRef(*buffer))
		retError(clean, Error_constData(
			0, 0, "MemoryStream_createFromBuffer()::buffer is a ref but flags contained IsResizable"
		));

	gotoIfError3(clean, Stream_create(
		MemoryStream_readInternal,
		(flags & EMemoryStreamFlags_IsWritable) ? MemoryStream_writeInternal : NULL,
		MemoryStream_reserveInternal,
		MemoryStream_closeInternal,
		Buffer_length(*buffer),
		EStreamType_Memory | (flags & EMemoryStreamFlags_IsResizable ? EStreamType_Resizable : 0),
		type,
		memStream,
		e_rr
	));

	MemoryStream *stream = RefPtr_data(*memStream, MemoryStream);

	stream->data = *buffer;
	*buffer = Buffer_createNull();

clean:
	return s_uccess;
}

RefPtrType MemoryStream_makeType(const Allocator *alloc) {
	return Stream_inheritType(alloc, sizeof(MemoryStream) - sizeof(Stream));
}

Bool MemoryStream_create(
	U64 size,
	EMemoryStreamFlags flags,
	const RefPtrType *type,
	MemoryStreamRef **stream,
	Error *e_rr
) {

	Bool s_uccess = true;
	Buffer buf = Buffer_createNull();

	if (!type || !type->alloc)
		retError(clean, Error_nullPointer(0, "MemoryStream_create()::type and type->alloc are required"));

	if (size)
		gotoIfError3(clean, Buffer_createUninitializedBytes(size, type->alloc, &buf, e_rr));

	gotoIfError3(clean, MemoryStream_createFromBuffer(&buf, flags, type, stream, e_rr));

clean:
	if(!s_uccess && buf.ptr)
		Buffer_free(&buf, type->alloc);

	return s_uccess;
}

Bool MemoryStream_createFromBufferRegion(
	Buffer buffer,
	U64 offset,
	U64 length,
	EMemoryStreamFlags flags,
	const RefPtrType *type,
	MemoryStreamRef **memStream,
	Error *e_rr
) {
	Bool s_uccess = true;

	gotoIfError3(clean, Buffer_offset(&buffer, offset, e_rr));

	U64 bufl = Buffer_length(buffer);

	if (length > bufl)
		retError(clean, Error_outOfBounds(
			1, length, bufl,
			"MemoryStream_createFromBufferRegion() buffer is too small"
		));

	buffer.lengthAndRefBits &= (U64)3 << 62;
	buffer.lengthAndRefBits |= length;

	gotoIfError3(clean, MemoryStream_createFromBuffer(&buffer, flags, type, memStream, e_rr));
		
clean:
	return s_uccess;
}

//Closing a stream & moving memory

Bool MemoryStream_move(MemoryStreamRef **streamRef, Buffer *output, Error *e_rr) {

	Bool s_uccess = true;

	if (!streamRef || !*streamRef || (*streamRef)->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream)
		retError(clean, Error_nullPointer(0, "MemoryStream_move()::streamRef is required"));

	if (!output)
		retError(clean, Error_nullPointer(1, "MemoryStream_move()::output is required"));

	if(output->ptr)
		retError(clean, Error_invalidParameter(1, 0, "MemoryStream_move()::output isn't empty, indicating memleak"));

	MemoryStream *stream = RefPtr_data(*streamRef, MemoryStream);

	if (!(stream->parent.streamType & EStreamType_Memory))
		retError(clean, Error_invalidParameter(0, 0, "MemoryStream_move()::streamRef must be a MemoryStream"));

	*output = stream->data;
	stream->data = Buffer_createNull();
	
	RefPtr_dec(streamRef);

clean:
	return s_uccess;
}
