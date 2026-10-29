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

//types/container/test/test_types_container_memory_stream.c

#include "test_types_container_shared.h"
#include "types/container/test/stream_harness.h"
#include "types/container/memory_stream.h"
#include "types/container/buffer.h"

static void Test_memoryStreamCreate(Test *t, const RefPtrType *type) {

	Test_setModule(t, "MemoryStream_create");

	RefPtr *stream = NULL;
	Buffer  full   = Buffer_createNull();

	//Check matching size
	
	if (MemoryStream_create(1024, EMemoryStreamFlags_WriteResize, type, &stream, &t->err)) {
		const MemoryStream *ms = RefPtr_data(stream, MemoryStream);
		Test_assert(t, "Sized: size",          ms->parent.size == 1024);
		Test_assert(t, "Sized: buffer length", Buffer_length(ms->data) == 1024);
		RefPtr_dec(&stream);
	}

	else Test_assert(t, "Create sized resizable", false);

	//Ensure write func is NULL for readonly
	
	if (MemoryStream_create(256, EMemoryStreamFlags_None, type, &stream, &t->err)) {
		const OxStream *s = RefPtr_data(stream, OxStream);
		Test_assert(t, "Readonly: write fn is NULL", !s->write);
		RefPtr_dec(&stream);
	}

	else Test_assert(t, "Create readonly", false);

	//createFromBuffer (owned): buffer is moved, size correct, contents preserved.

	const Bool allocOk = Buffer_createUninitializedBytes(256, t->alloc, &full, &t->err);
	Test_assert(t, "FromBuffer owned: alloc", allocOk);

	if (allocOk) {

		Buffer_setAllToU8(full, 0xAB, NULL);

		if (MemoryStream_createFromBuffer(&full, EMemoryStreamFlags_None, type, &stream, &t->err)) {

			const MemoryStream *ms = RefPtr_data(stream, MemoryStream);
			Test_assert(t, "FromBuffer: buffer moved (ptr null)", !full.ptr);
			Test_assert(t, "FromBuffer: size",                    ms->parent.size == 256);

			Bool contentsOk = true;
			const U64 *v64  = (const U64*) ms->data.ptr;

			for (U8 i = 0; i < 256 / 8; ++i)
				if (v64[i] != 0xABABABABABABABAB) {
					contentsOk = false;
					break;
				}

			Test_assert(t, "FromBuffer: contents correct", contentsOk);
			RefPtr_dec(&stream);

		} else {
			Test_assert(t, "FromBuffer owned: create", false);
			Buffer_free(&full, t->alloc);
		}
	}

	//createFromBuffer (const): writable must be rejected, readonly must succeed.

	const U8 constData[100] = { 0 };
	Buffer bufConst = Buffer_createRefConst(constData, 100);

	Test_assert(t, "FromBuffer const: writable rejected",
		!MemoryStream_createFromBuffer(&bufConst, EMemoryStreamFlags_IsWritable, type, &stream, NULL)
	);

	if (MemoryStream_createFromBuffer(&bufConst, EMemoryStreamFlags_None, type, &stream, &t->err))
		RefPtr_dec(&stream);

	else Test_assert(t, "FromBuffer const: readonly accepted", false);
}

static void Test_memoryStreamMove(Test *t, const RefPtrType *type) {

	Test_setModule(t, "MemoryStream move");

	RefPtr *stream = NULL;

	U8     data[200];
	Buffer buf2 = Buffer_createRef(data, 200);

	for (U64 i = 0; i < 200; ++i)
		buf2.ptrNonConst[i] = (U8) i;

	if (!MemoryStream_createFromBuffer(&buf2, EMemoryStreamFlags_None, type, &stream, &t->err)) {
		Test_assert(t, "Create from buffer", false);
		return;
	}

	if (MemoryStream_move(&stream, &buf2, &t->err)) {
		Test_assert(t, "OxStream is null after move", !stream);
		Test_assert(t, "Buffer length preserved",   Buffer_length(buf2) == 200);
		Test_assert(t, "Buffer pointer preserved",  buf2.ptr == data);

		Bool ok = true;

		for (U64 i = 0; i < 200; ++i)
			if (buf2.ptr[i] != (U8) i) {
				ok = false;
				break;
			}

		Test_assert(t, "Data correct", ok);
	}
	
	else Test_assert(t, "MemoryStream_move()", false);

	RefPtr_dec(&stream);
}

static Bool MemStream_harnessCreate(const StreamHarness *h, U64 size, Bool isResizable, RefPtr **out, Test *t) {

	(void) h;
	EMemoryStreamFlags flags = EMemoryStreamFlags_IsWritable;

	if (isResizable)
		flags |= EMemoryStreamFlags_IsResizable;

	if (!MemoryStream_create(size, flags, h->type, out, &t->err)) {
		Test_assert(t, "MemStream_harnessCreate", false);
		return false;
	}

	return true;
}

typedef struct MemoryStreamCountAlloc {
	Allocator inner;
	U64 allocations;
} MemoryStreamCountAlloc;

static Bool MemoryStream_countAlloc(void *context, U64 length, Buffer *output, Error *e_rr) {

	MemoryStreamCountAlloc *counter = context;

	++counter->allocations;
	return counter->inner.alloc(counter->inner.ptr, length, output, e_rr);
}

static void MemoryStream_countFree(void *context, Buffer buffer) {
	MemoryStreamCountAlloc *counter = context;
	counter->inner.free(counter->inner.ptr, buffer);
}

static void Test_memoryStreamGrowth(Test *t) {

	Test_setModule(t, "MemoryStream amortized growth preserves size and content");

	MemoryStreamCountAlloc counter = { .inner = *t->alloc };
	const Allocator counting = { .ptr = &counter, .alloc = MemoryStream_countAlloc, .free = MemoryStream_countFree };
	const RefPtrType type = MemoryStream_makeType(&counting);

	for(U8 explicitReserve = 0; explicitReserve < 2; ++explicitReserve) {

		StreamRef *stream = NULL;
		Buffer moved = Buffer_createNull();

		if (!Test_assert(t, "Create growing stream", MemoryStream_create(
			0, EMemoryStreamFlags_WriteResize, &type, &stream, &t->err
		)))
			continue;

		OxStream *s = RefPtr_data(stream, OxStream);
		Bool writesOk = true, sizesOk = true;

		counter.allocations = 0;

		for(U64 i = 0; i < 4097; ++i) {

			U8 value = (U8) (i * 17);

			if(explicitReserve) {
				writesOk = s->reserve(s, i + 1, &counting, &t->err);
				sizesOk &= s->size == i;
			}

			writesOk = writesOk && s->write(s, i, 1, Buffer_createRefConst(&value, 1), &counting, &t->err);
			sizesOk &= s->size == i + 1;

			if(!writesOk)
				break;
		}

		Test_assert(t, "All appends succeed", writesOk);
		Test_assert(t, "Logical size excludes spare capacity", sizesOk);
		Test_assert(t, "Thousands of appends take at most 32 allocations", counter.allocations <= 32);
		Test_assert(t, "Move growing stream", MemoryStream_move(&stream, &moved, &t->err));
		Test_assert(t, "Moved buffer has exact logical length", Buffer_length(moved) == 4097);

		Bool contentsOk = Buffer_length(moved) == 4097;

		for(U64 i = 0; contentsOk && i < 4097; ++i)
			contentsOk = moved.ptr[i] == (U8) (i * 17);

		Test_assert(t, "Data survives reallocations and move", contentsOk);

		Buffer_free(&moved, &counting);
		RefPtr_dec(&stream);
	}

	U8 storage[8] = { 0 };
	U8 value = 42;
	Buffer ref = Buffer_createRef(storage, sizeof(storage));
	StreamRef *fixed = NULL;

	if (Test_assert(t, "Create fixed reference", MemoryStream_createFromBuffer(
		&ref, EMemoryStreamFlags_IsWritable, &type, &fixed, &t->err
	))) {

		OxStream *s = RefPtr_data(fixed, OxStream);

		counter.allocations = 0;

		Test_assert(t, "Write caller storage", s->write(s, 3, 1, Buffer_createRefConst(&value, 1), &counting, &t->err));
		Test_assert(t, "Fixed reference cannot grow", !s->write(s, 8, 1, Buffer_createRefConst(&value, 1), &counting, NULL));
		Test_assert(t, "Reference contract preserved", storage[3] == 42 && s->size == 8 && !s->reserve && !counter.allocations);
	}

	RefPtr_dec(&fixed);
}

static void Test_memoryStreamBounds(Test *t, const RefPtrType *type) {

	Test_setModule(t, "MemoryStream bounds");

	RefPtr *stream = NULL;

	if (!MemoryStream_create(4, EMemoryStreamFlags_WriteResize, type, &stream, &t->err)) {
		Test_assert(t, "Create stream for bounds", false);
		return;
	}

	OxStream *s = RefPtr_data(stream, OxStream);
	U8 data[8] = { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };
	Buffer buf = Buffer_createRef(data, sizeof(data));

	Test_assert(t, "Reserve capacity beyond logical size", s->reserve(s, 32, t->alloc, &t->err));
	Test_assert(t, "Read to end uses logical size", s->read(s, 0, 0, buf, t->alloc, &t->err));
	Test_assert(t, "Read leaves reserved tail untouched", data[0] == 0 && data[3] == 0 && data[4] == 0xCC);
	Test_assert(
		t, "Read rejects const destination",
		!s->read(s, 0, 1, Buffer_createRefConst(data, sizeof(data)), t->alloc, NULL)
	);

	Test_assert(t, "Read rejects overflowing range", !s->read(s, U64_MAX, 1, buf, t->alloc, NULL));
	Test_assert(t, "Read rejects overflowing length", !s->read(s, 1, U64_MAX, buf, t->alloc, NULL));
	Test_assert(t, "Write rejects overflowing range", !s->write(s, U64_MAX, 1, buf, t->alloc, NULL));
	Test_assert(t, "Write rejects range beyond 48 bits", !s->write(s, U64_MAX >> 16, 1, buf, t->alloc, NULL));
	Test_assert(t, "Read at end accepts empty buffer", s->read(s, 4, 0, Buffer_createNull(), t->alloc, &t->err));
	Test_assert(t, "Failed writes preserve logical size", s->size == 4);

	RefPtr_dec(&stream);
}

static void Test_memoryStreamCursorWrites(Test *t, const RefPtrType *type) {

	Test_setModule(t, "StreamCursor write bounds and ordering");

	RefPtr *stream = NULL;
	StreamCursor cursor = (StreamCursor) { 0 };
	Buffer large = Buffer_createNull();

	if (!MemoryStream_create(64 * KIBI, EMemoryStreamFlags_IsWritable, type, &stream, &t->err)) {
		Test_assert(t, "Create stream for cursor writes", false);
		goto clean;
	}

	if (!StreamCursor_create(stream, 32 * KIBI, true, t->alloc, &cursor, &t->err)) {
		Test_assert(t, "Create cursor for write bounds", false);
		goto clean;
	}

	U8 data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	Buffer shortBuf = Buffer_createRefConst(data, 2);

	Test_assert(t, "Empty cache contains no wrapped range", !StreamCursor_contains(&cursor, U64_MAX, 1));
	Test_assert(
		t, "Write rejects source length beyond buffer",
		!StreamCursor_write(&cursor, shortBuf, 0, 0, 4, false, t->alloc, NULL)
	);

	Test_assert(
		t, "Write rejects source offset beyond buffer",
		!StreamCursor_write(&cursor, shortBuf, 3, 0, 1, false, t->alloc, NULL)
	);

	Test_assert(
		t, "Write rejects range beyond 48 bits",
		!StreamCursor_write(&cursor, shortBuf, 0, U64_MAX >> 16, 1, true, t->alloc, NULL)
	);

	Test_assert(t, "Invalid writes leave cache empty", cursor.lastLocation == U64_MAX);
	Test_assert(t, "Flush after source bounds checks", StreamCursor_flush(&cursor, t->alloc, &t->err));

	Test_assert(t, "Cache first write", StreamCursor_write(&cursor, shortBuf, 0, 0, 2, false, t->alloc, &t->err));
	Test_assert(t, "Cache contains valid range", StreamCursor_contains(&cursor, 0, 2));
	Test_assert(t, "Cache contains no wrapped range", !StreamCursor_contains(&cursor, U64_MAX, 1));
	Test_assert(
		t, "Bypass newer write",
		StreamCursor_write(&cursor, Buffer_createRefConst(data + 2, 2), 0, 0, 2, true, t->alloc, &t->err)
	);

	Test_assert(t, "Flush after bypass write", StreamCursor_flush(&cursor, t->alloc, &t->err));

	const MemoryStream *mem = RefPtr_data(stream, MemoryStream);

	Test_assert(t, "Newest bypass write survives flush", mem->data.ptr[0] == 3 && mem->data.ptr[1] == 4);
	Test_assert(
		t, "Zero length writes remaining source bytes",
		StreamCursor_write(&cursor, shortBuf, 1, 16, 0, false, t->alloc, &t->err)
	);

	Test_assert(t, "Flush remaining source bytes", StreamCursor_flush(&cursor, t->alloc, &t->err));
	Test_assert(t, "Only remaining source byte was written", mem->data.ptr[16] == 2 && mem->data.ptr[17] == 0);

	if (!Buffer_createEmptyBytes(64 * KIBI, t->alloc, &large, &t->err)) {
		Test_assert(t, "Create large write buffer", false);
		goto clean;
	}

	Test_assert(
		t, "Cache write inside large range",
		StreamCursor_write(&cursor, shortBuf, 0, 8, 2, false, t->alloc, &t->err)
	);

	Test_assert(t, "Write overlapping large range", StreamCursor_write(&cursor, large, 0, 0, 0, false, t->alloc, &t->err));
	Test_assert(t, "Flush after large write", StreamCursor_flush(&cursor, t->alloc, &t->err));
	Test_assert(t, "Newest large write survives flush", mem->data.ptr[8] == 0 && mem->data.ptr[9] == 0);

clean:
	Buffer_free(&large, t->alloc);
	StreamCursor_close(&cursor, t->alloc);
	RefPtr_dec(&stream);
}

static U64 failedReadCalls;

static Bool Test_failedCacheRead(OxStream *stream, U64 offset, U64 length, Buffer buf, const Allocator *alloc, Error *e_rr) {

	(void) stream; (void) offset; (void) length; (void) alloc; (void) e_rr;
	++failedReadCalls;
	Buffer_unsetAllBits(buf, NULL);
	return false;
}

static void Test_memoryStreamFailedRead(Test *t, const RefPtrType *type) {

	Test_setModule(t, "StreamCursor failed reads never populate cache");

	StreamRef *stream = NULL;
	StreamCursor cursor = { 0 };
	U8 output[4] = { 0 };

	failedReadCalls = 0;

	if (!MemoryStream_create(64, EMemoryStreamFlags_IsWritable, type, &stream, &t->err)) {
		Test_assert(t, "Create stream", false);
		goto clean;
	}

	if (!StreamCursor_create(stream, 0, false, t->alloc, &cursor, &t->err)) {
		Test_assert(t, "Create cursor", false);
		goto clean;
	}

	RefPtr_data(stream, OxStream)->read = Test_failedCacheRead;

	Test_assert(
		t, "First failed read",
		!StreamCursor_read(&cursor, Buffer_createRef(output, sizeof(output)), 0, 0, 4, false, t->alloc, NULL)
	);

	Test_assert(t, "Failure leaves no cached bytes", !StreamCursor_contains(&cursor, 0, 4));
	Test_assert(
		t, "Retry also fails",
		!StreamCursor_read(&cursor, Buffer_createRef(output, sizeof(output)), 0, 0, 4, false, t->alloc, NULL)
	);

	Test_assert(t, "Retry reaches the stream", failedReadCalls == 2);

clean:
	StreamCursor_close(&cursor, t->alloc);
	RefPtr_dec(&stream);
}

static void Test_memoryStreamEmptyCopy(Test *t, const RefPtrType *type) {

	Test_setModule(t, "StreamCursor empty copies");

	for(U64 size = 0; size <= 4; size += 4) {

		StreamRef *input = NULL, *output = NULL;
		StreamCursor reader = { 0 }, writer = { 0 };

		if (
			Test_assert(
				t, "Create copy streams",
				MemoryStream_create(size, EMemoryStreamFlags_None, type, &input, &t->err) &&
				MemoryStream_create(8, EMemoryStreamFlags_WriteResize, type, &output, &t->err)
			) &&
			Test_assert(
				t, "Create copy cursors",
				StreamCursor_create(input, 0, false, t->alloc, &reader, &t->err) &&
				StreamCursor_create(output, 0, true, t->alloc, &writer, &t->err)
			)
		) {

			Test_assert(t, "Copy from the end succeeds", StreamCursor_copyStream(
				&writer, &reader, size, 0, 0, t->alloc, &t->err
			));

			Test_assert(
				t, "Empty copy leaves output untouched", RefPtr_data(output, OxStream)->size == 8 &&
				writer.lastLocation == U64_MAX
			);
		}

		StreamCursor_close(&writer, t->alloc);
		StreamCursor_close(&reader, t->alloc);
		RefPtr_dec(&output);
		RefPtr_dec(&input);
	}
}

void Test_memoryStream(Test *t) {

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamHarness h = {
		.create = MemStream_harnessCreate,
		.type = &type,
		.name = "MemoryStream"
	};

	Test_memoryStreamCreate(t, &type);
	Test_memoryStreamGrowth(t);
	Test_memoryStreamEmptyCopy(t, &type);
	StreamHarness_testStream(&h, t);
	Test_memoryStreamMove(t, &type);

	StreamHarness_testCursor(&h, t);
	Test_memoryStreamBounds(t, &type);
	Test_memoryStreamCursorWrites(t, &type);
	Test_memoryStreamFailedRead(t, &type);

	Test_setModule(t, NULL);
}
