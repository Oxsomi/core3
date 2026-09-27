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

//types/mesh/test/test_mesh_internal.c

#include "test_mesh_shared.h"
#include "../mesh_internal.h"
#include "types/container/memory_stream.h"
#include "types/container/ref_ptr.h"
#include "types/base/string_read_helper.h"
#include "types/base/error.h"
#include "types/base/mathf.h"

//A readable stream over the caller's bytes, which is the shape a file read hands a reader.
//The type is REFERENCED by every stream made from it, so it is the caller's to hold for as long as the
// stream lives rather than something these make on the spot.

static MemoryStreamRef *readable(Test *t, const RefPtrType *type, const void *bytes, U64 length) {

	MemoryStreamRef *stream = NULL;

	if(!MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(bytes, length), 0, length, EMemoryStreamFlags_None, type, &stream, &t->err
	))
		return NULL;

	return stream;
}

static MemoryStreamRef *writable(Test *t, const RefPtrType *type) {

	MemoryStreamRef *stream = NULL;

	if(!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &stream, &t->err))
		return NULL;

	return stream;
}

void Test_meshSourceWindow(Test *t) {

	Test_setModule(t, "mesh/source");

	//Held for the whole test, since every stream below references it.

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	//A file larger than the window, so every read below crosses at least one refill. The lines are numbered
	// so a boundary that swallowed or duplicated one shows up as the wrong text rather than the wrong count.

	const U64 lines = MESH_WINDOW / 8 + 64;
	Buffer text = Buffer_createNull();

	if(!Test_assert(t, "alloc", Buffer_createUninitializedBytes(lines * 8, t->alloc, &text, &t->err)))
		return;

	C8 *chars = (C8*) text.ptrNonConst;

	for(U64 i = 0; i < lines; ++i) {

		C8 *at = chars + i * 8;

		for(U8 j = 0; j < 7; ++j)
			at[j] = (C8) ('0' + ((i >> ((6 - j) * 3)) & 7));

		at[7] = '\n';
	}

	MemoryStreamRef *stream = readable(t, &type, chars, lines * 8);
	MeshSource src = (MeshSource) { 0 };

	if(!Test_assert(t, "source", stream && MeshSource_create((StreamRef*) stream, 0, t->alloc, &src, &t->err)))
		goto clean;

	//Every line comes back in order and intact, the last one included.

	Bool intact = true;
	C8 line[64];

	for(U64 i = 0; i < lines && intact; ++i) {

		U64 got = 0;
		Bool any = false;

		if(!MeshSource_readLine(&src, line, sizeof(line), &got, &any, &t->err) || !any || got != 7) {
			intact = false;
			break;
		}

		for(U8 j = 0; j < 7; ++j)
			if(line[j] != (C8) ('0' + ((i >> ((6 - j) * 3)) & 7)))
				intact = false;
	}

	Test_assert(t, "everyLine", intact);

	//The end of the stream is not an error, it is simply no line.

	U64 got = 0;
	Bool any = true;
	Test_assert(t, "end", MeshSource_readLine(&src, line, sizeof(line), &got, &any, &t->err) && !any);

	MeshSource_free(&src);

	//Bytes and skips cross the same boundary. Reading the byte right after a skip that lands past the window
	// is what proves the refill kept the cursor rather than the buffer position.

	if(!Test_assert(t, "source2", MeshSource_create((StreamRef*) stream, 0, t->alloc, &src, &t->err)))
		goto clean;

	U8 head[8] = { 0 };
	Test_assert(t, "readBytes", MeshSource_readBytes(&src, head, sizeof(head), &t->err) && head[7] == '\n');

	const U64 skip = MESH_WINDOW + 13;
	Test_assert(t, "skip", MeshSource_skip(&src, skip, &t->err));
	Test_assert(t, "offset", MeshSource_offset(&src) == sizeof(head) + skip);

	U8 after = 0;
	Test_assert(t, "afterSkip", MeshSource_readBytes(&src, &after, 1, &t->err) && after == (U8) chars[sizeof(head) + skip]);

	//Reading past the end is an error, since a binary body is sized by its header.

	Test_assert(t, "skipToEnd", MeshSource_skip(&src, lines * 8 - MeshSource_offset(&src) - 2, &t->err));

	Test_assert(t, "pastEnd", !MeshSource_readBytes(&src, head, sizeof(head), NULL));

	MeshSource_free(&src);

	//A line that does not fit the caller's buffer is refused rather than split.

	if(!Test_assert(t, "source3", MeshSource_create((StreamRef*) stream, 0, t->alloc, &src, &t->err)))
		goto clean;

	C8 tiny[4];
	Test_assert(t, "lineCap", !MeshSource_readLine(&src, tiny, sizeof(tiny), &got, &any, NULL));

clean:

	MeshSource_free(&src);
	RefPtr_dec((RefPtr**) &stream);
	Buffer_free(&text, t->alloc);
}

void Test_meshSink(Test *t) {

	Test_setModule(t, "mesh/sink");

	//Held for the whole test, since every stream below references it.

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	//A sink with no stream discards, which is what an output the caller left NULL is. Nothing may allocate
	// and nothing may fail.

	MeshSink discard = (MeshSink) { 0 };
	const U32 dummy = 0;

	Test_assert(t, "discardCreate", MeshSink_create(NULL, 0, t->alloc, &discard, &t->err));
	Test_assert(t, "discardInactive", !MeshSink_active(&discard));
	Test_assert(t, "discardWrite", MeshSink_write(&discard, &dummy, sizeof(dummy), &t->err));
	Test_assert(t, "discardFlush", MeshSink_flush(&discard, &t->err));
	MeshSink_free(&discard);

	//Records that together cross several chunks come out contiguous and in order, which is the only thing
	// the buffering is allowed to change.

	const U64 count = MESH_CHUNK / sizeof(U32) * 3 + 7;
	MemoryStreamRef *stream = writable(t, &type);
	MeshSink sink = (MeshSink) { 0 };
	Buffer out = Buffer_createNull();

	if(!Test_assert(t, "sink", stream && MeshSink_create((StreamRef*) stream, 0, t->alloc, &sink, &t->err)))
		goto clean;

	Bool wrote = true;

	for(U64 i = 0; i < count && wrote; ++i) {
		const U32 value = (U32) (i * 2654435761u);
		wrote = MeshSink_write(&sink, &value, sizeof(value), &t->err);
	}

	Test_assert(t, "writes", wrote);
	Test_assert(t, "flush", MeshSink_flush(&sink, &t->err));
	MeshSink_free(&sink);

	if(!Test_assert(t, "move", MemoryStream_move(&stream, &out, &t->err)))
		goto clean;

	if(!Test_assert(t, "length", Buffer_length(out) == count * sizeof(U32)))
		goto clean;

	Bool ordered = true;

	for(U64 i = 0; i < count && ordered; ++i)
		ordered = ((const U32*) out.ptr)[i] == (U32) (i * 2654435761u);

	Test_assert(t, "ordered", ordered);

clean:

	MeshSink_free(&sink);
	RefPtr_dec((RefPtr**) &stream);
	Buffer_free(&out, t->alloc);
}

void Test_meshPositions(Test *t) {

	Test_setModule(t, "mesh/positions");

	//Held for the whole test, since every stream below references it.

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	//Three positions spanning a box that is flat on z, so the flat axis exercises the no extent arm.

	const F32 corners[3][3] = {
		{ -2, 0, 5 },
		{  6, 4, 5 },
		{  2, 2, 5 }
	};

	//Unquantized, the positions leave as they came and only the bounds are computed.

	MemoryStreamRef *stream = writable(t, &type);
	MeshSink sink = (MeshSink) { 0 };
	MeshPositions positions = (MeshPositions) { 0 };
	Buffer out = Buffer_createNull();
	MeshInfo info = (MeshInfo) { 0 };

	if(!Test_assert(t, "sink", stream && MeshSink_create((StreamRef*) stream, 0, t->alloc, &sink, &t->err)))
		goto clean;

	MeshPositions_create(EMeshFlags_None, &sink, &positions);

	for(U8 i = 0; i < 3; ++i)
		if(!Test_assert(t, "push", MeshPositions_push(&positions, corners[i], t->alloc, &t->err)))
			goto clean;

	Test_assert(t, "finish", MeshPositions_finish(&positions, &info, &t->err));
	Test_assert(t, "flush", MeshSink_flush(&sink, &t->err));

	Test_assert(t, "aabbMin", Test_near(info.aabbMin[0], -2) && Test_near(info.aabbMin[1], 0) &&
		Test_near(info.aabbMin[2], 5));

	Test_assert(t, "aabbMax", Test_near(info.aabbMax[0], 6) && Test_near(info.aabbMax[1], 4) &&
		Test_near(info.aabbMax[2], 5));

	MeshPositions_free(&positions, t->alloc);
	MeshSink_free(&sink);

	if(!Test_assert(t, "move", MemoryStream_move(&stream, &out, &t->err)))
		goto clean;

	Test_assert(t, "plainLength", Buffer_length(out) == 3 * 3 * sizeof(F32));
	Test_assert(t, "plainValue", Test_near(((const F32*) out.ptr)[3], 6));

	Buffer_free(&out, t->alloc);

	//Quantized, the records are held until the bounds are complete and leave as four snorm16. The extremes
	// land on the ends of the range exactly and the center of a span lands at zero.

	stream = writable(t, &type);

	if(!Test_assert(t, "sink2", stream && MeshSink_create((StreamRef*) stream, 0, t->alloc, &sink, &t->err)))
		goto clean;

	MeshPositions_create(EMeshFlags_QuantizePositions, &sink, &positions);

	for(U8 i = 0; i < 3; ++i)
		if(!Test_assert(t, "pushQuant", MeshPositions_push(&positions, corners[i], t->alloc, &t->err)))
			goto clean;

	info = (MeshInfo) { 0 };
	Test_assert(t, "finishQuant", MeshPositions_finish(&positions, &info, &t->err));
	Test_assert(t, "flushQuant", MeshSink_flush(&sink, &t->err));

	MeshPositions_free(&positions, t->alloc);
	MeshSink_free(&sink);

	if(!Test_assert(t, "moveQuant", MemoryStream_move(&stream, &out, &t->err)))
		goto clean;

	if(!Test_assert(t, "quantLength", Buffer_length(out) == 3 * 4 * sizeof(I16)))
		goto clean;

	const I16 *quantized = (const I16*) out.ptr;

	Test_assert(t, "lowEnd", quantized[0] == -32767 && quantized[1] == -32767);
	Test_assert(t, "highEnd", quantized[4] == 32767 && quantized[5] == 32767);

	//x = 2 is the middle of -2..6 and y = 2 the middle of 0..4, so both land on zero.

	Test_assert(t, "center", quantized[8] == 0 && quantized[9] == 0);

	//z has no extent, so every vertex quantizes to the center rather than dividing by zero.

	Test_assert(t, "flatAxis", !quantized[2] && !quantized[6] && !quantized[10]);

clean:

	MeshPositions_free(&positions, t->alloc);
	MeshSink_free(&sink);
	RefPtr_dec((RefPtr**) &stream);
	Buffer_free(&out, t->alloc);
}
