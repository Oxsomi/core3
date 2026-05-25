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
#include "types/container/test/stream.h"
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
			const U64 *v64  = (const U64*)ms->data.ptr;

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
		buf2.ptrNonConst[i] = (U8)i;

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
			if (buf2.ptr[i] != (U8)i) {
				ok = false;
				break;
			}

		Test_assert(t, "Data correct", ok);
	}
	
	else Test_assert(t, "MemoryStream_move()", false);

	RefPtr_dec(&stream);
}

static Bool MemStream_harnessCreate(const StreamHarness *h, U64 size, Bool isResizable, RefPtr **out, Test *t) {

	(void)h;
	EMemoryStreamFlags flags = EMemoryStreamFlags_IsWritable;

	if (isResizable)
		flags |= EMemoryStreamFlags_IsResizable;

	if (!MemoryStream_create(size, flags, h->type, out, &t->err)) {
		Test_assert(t, "MemStream_harnessCreate", false);
		return false;
	}

	return true;
}

void Test_memoryStream(Test *t) {

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamHarness h = {
		.create = MemStream_harnessCreate,
		.type = &type,
		.name = "MemoryStream"
	};

	Test_memoryStreamCreate(t, &type);
	StreamHarness_testStream(&h, t);
	Test_memoryStreamMove(t, &type);

	StreamHarness_testCursor(&h, t);

	Test_setModule(t, NULL);
}
