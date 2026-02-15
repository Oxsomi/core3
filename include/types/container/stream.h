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
#include "types/math/vec4.h"
#include "types/base/buffer.h"
#include "types/container/ref_ptr.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;

typedef struct Stream Stream;
typedef Bool (*StreamFunc)(Stream *stream, U64 offset, U64 length, Buffer buf, const Allocator *alloc, Error *e_rr);

typedef Bool (*StreamReserveFunc)(Stream *stream, U64 length, const Allocator *alloc, Error *e_rr);
typedef void (*StreamCloseFunc)(Stream *stream, const Allocator *alloc);

typedef enum EStreamType {
	EStreamType_Memory			= 1 << 0,
	EStreamType_File			= 1 << 1,
	EStreamType_ArchiveEntry	= 1 << 2,
	EStreamType_Compressed		= 1 << 3,		//Unsupported
	EStreamType_Encrypted		= 1 << 4,
	EStreamType_Resizable		= 1 << 5
} EStreamType;

typedef struct Stream {
	StreamFunc read;
	StreamFunc write;
	StreamReserveFunc reserve;
	StreamCloseFunc close;
	U64 size;
	U64 streamType;			//EStreamType, except extendible
} Stream;

typedef RefPtr StreamRef;

typedef struct StreamCursor {

	StreamRef *stream;

	Buffer cacheData;			//Temporary cache

	U64 lastLocation;			//Last location the cache was fetched from
	U64 lastWriteLocation;		//If writable only, TODO: Maybe lastReadLocation to allow switching mode? (reduce overhead)

	Bool readOnly;				//Only allowed to read or write once at a time
	Bool allowRead, allowWrite;
	U8 pad[7];

} StreamCursor;

typedef struct CryptoChunk {		//chunkId can be computed, iv is also computed from root iv + chunkId.
	I32x4 tag;
} CryptoChunk;

static inline Bool StreamCursor_canRead(const StreamCursor *cursor) { return cursor && cursor->stream && cursor->readOnly; }
static inline Bool StreamCursor_canWrite(const StreamCursor *cursor) { return cursor && cursor->stream && !cursor->readOnly; }

static inline Bool StreamCursor_contains(const StreamCursor *cursor, U64 offset, U64 length) {
	return cursor &&
		offset >= cursor->lastLocation &&
		offset + length <= cursor->lastLocation + Buffer_length(cursor->cacheData);
}

RefPtrType Stream_inheritType(const Allocator *alloc, U32 extraSize);

//Readwrite streams start in write mode.
Bool Stream_create(
	StreamFunc read,
	StreamFunc write,
	StreamReserveFunc reserve,
	StreamCloseFunc close,
	U64 streamSize,
	EStreamType streamType,
	const RefPtrType *type,		//Also passes the allocator
	StreamRef **stream,
	Error *e_rr
);

Bool StreamCursor_create(
	StreamRef *stream,
	U64 cacheSize,
	Bool writeOnly,				//By default a RW stream is readonly, needs to be made writeOnly
	const Allocator *alloc,
	StreamCursor *cursor,
	Error *e_rr
);

Bool StreamCursor_createWithCache(
	StreamRef *stream,
	Buffer *cache,				//Takes ownership of cache
	Bool writeOnly,				//By default a RW stream is readonly, needs to be made writeOnly
	StreamCursor *cursor,
	Error *e_rr
);

Bool StreamCursor_flush(StreamCursor *cursor, const Allocator *alloc, Error *e_rr);

//Turns a readwrite (or readonly) stream into a readonly stream until setWritable is enabled again.
//Returns false if not readable.
Bool StreamCursor_setReadOnly(StreamCursor *cursor, const Allocator *alloc, Error *e_rr);

//Turns a readwrite (or writeonly) stream into a writeonly stream until setReadOnly is enabled again.
//Returns false if not writable.
Bool StreamCursor_setWritable(StreamCursor *cursor, Error *e_rr);

void StreamCursor_close(StreamCursor *cursor, const Allocator *alloc);
Bool StreamCursor_closeAndKeepCache(StreamCursor *cursor, const Allocator *alloc, Buffer *cache, Error *e_rr);

//disableFlush is to disable flushing the previously written bytes to output if readwrite
Bool StreamCursor_resize(StreamCursor *cursor, U64 newSize, Bool disableFlush, const Allocator *alloc, Error *e_rr);

//Copy a stream into antoher
Bool StreamCursor_copyStream(
	StreamCursor *output,
	StreamCursor *input,
	U64 srcOff,
	U64 dstOff,
	U64 length,				//length = 0: all remaining bytes from src
	const Allocator *alloc,
	Error *e_rr
);

Bool StreamCursor_write(
	StreamCursor *cursor,
	Buffer buf,
	U64 srcOff,
	U64 dstOff,
	U64 length,
	Bool bypassCache,
	const Allocator *alloc,
	Error *e_rr
);

//buf.length == 0 && length indicates; load to cache only
Bool StreamCursor_read(
	StreamCursor *cursor,
	Buffer buf,
	U64 srcOff,
	U64 dstOff,
	U64 length,
	Bool bypassCache,
	const Allocator *alloc,
	Error *e_rr
);

//Consumes stream at *it into cursor (and copies into v) and increments *it.
static inline Bool StreamCursor_consume(
	StreamCursor *cursor,
	U64 *it,
	void *v,
	U64 length,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	if (!it)
		retError(clean, Error_nullPointer(1, "StreamCursor_consume()::it is required"));

	gotoIfError3(clean, StreamCursor_read(cursor, Buffer_createRef(v, length), *it, 0, length, false, alloc, e_rr));
	*it += length;

clean:
	return s_uccess;
}

//Appends to stream/cursor at *it from v and increments *it.
static inline Bool StreamCursor_append(
	StreamCursor *cursor,
	U64 *it,
	const void *v,
	U64 length,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	if (!it)
		retError(clean, Error_nullPointer(1, "StreamCursor_append()::it is required"));

	gotoIfError3(clean, StreamCursor_write(cursor, Buffer_createRefConst(v, length), 0, *it, length, false, alloc, e_rr));
	*it += length;

clean:
	return s_uccess;
}

#define STREAM_CURSOR_OP_IMPL(T)																						\
static inline Bool StreamCursor_consume##T(StreamCursor *cursor, U64 *it, T *t, const Allocator *alloc, Error *e_rr) {	\
	return StreamCursor_consume(cursor, it, t, sizeof(T), alloc, e_rr);													\
}																														\
																														\
static inline Bool StreamCursor_append##T(StreamCursor *cursor, U64 *it, T t, const Allocator *alloc, Error *e_rr) {	\
	return StreamCursor_append(cursor, it, &t, sizeof(T), alloc, e_rr);													\
}

STREAM_CURSOR_OP_IMPL(U64);
STREAM_CURSOR_OP_IMPL(U32);
STREAM_CURSOR_OP_IMPL(U16);
STREAM_CURSOR_OP_IMPL(U8);

STREAM_CURSOR_OP_IMPL(I64);
STREAM_CURSOR_OP_IMPL(I32);
STREAM_CURSOR_OP_IMPL(I16);
STREAM_CURSOR_OP_IMPL(I8);

STREAM_CURSOR_OP_IMPL(F64);
STREAM_CURSOR_OP_IMPL(F32);

/* TODO: Requires chunking, important: each chunk stores iv + tag and iv is unique.
*	Include chunkId per chunk into additional data to ensure no chunk reordering.
*	Root (header) defines chunk count and final file size and stores the AAD with the chunk count.
*	Load into cache first, then decrypt and verify. If fail, clear buffer.
*	Also allows >64GiB files as long as cache data size <= 64GiB.
typedef struct EncryptedStream {
	Stream parent;
	U32 key[8];
} EncryptedStream;
*/

#ifdef __cplusplus
	}
#endif
