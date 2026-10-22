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

//types/container/test/test_types_container_range_stream.c

#include "test_types_container_shared.h"
#include "types/container/memory_stream.h"
#include "types/container/buffer.h"

void Test_rangeStream(Test *t) {

	Test_setModule(t, "Stream_createRange");

	const RefPtrType memType = MemoryStream_makeType(t->alloc);
	const RefPtrType rangeType = Stream_rangeType(t->alloc);

	U8 bytes[256];

	for (U32 i = 0; i < 256; ++i)
		bytes[i] = (U8) i;

	Buffer src = Buffer_createRefConst(bytes, sizeof(bytes));
	StreamRef *source = NULL, *range = NULL;

	if (!MemoryStream_createFromBuffer(&src, EMemoryStreamFlags_None, &memType, &source, &t->err)) {
		Test_assert(t, "Source", false);
		return;
	}

	//A window from 37 to the end: every read lands 37 bytes further into the source.

	if (Stream_createRange(source, 37, 0, &rangeType, &range, &t->err)) {

		OxStream *str = RefPtr_data(range, OxStream);
		Test_assert(t, "Size is the remainder", str->size == 256 - 37);
		Test_assert(t, "Readonly", !str->write);
		Test_assert(t, "ConcurrentRead forwarded", !!(str->streamType & EStreamType_ConcurrentRead));

		U8 out[16] = { 0 };
		const Bool read = str->read(str, 3, sizeof(out), Buffer_createRef(out, sizeof(out)), t->alloc, &t->err);
		Test_assert(t, "Read", read);

		Bool same = read;

		for (U8 i = 0; i < sizeof(out); ++i)
			same &= out[i] == (U8) (37 + 3 + i);

		Test_assert(t, "Read is offset by the window", same);

		//The last byte is readable and one past it is not, which is the window's end rather than the source's.

		Test_assert(t, "Last byte", str->read(str, str->size - 1, 1, Buffer_createRef(out, 1), t->alloc, &t->err));
		Test_assert(t, "Last byte value", out[0] == 255);
		Test_assert(t, "Past the end refused", !str->read(str, str->size, 1, Buffer_createRef(out, 1), t->alloc, NULL));

		//The window holds the source: dropping the caller's reference first still leaves it readable.

		RefPtr_dec(&source);
		Test_assert(t, "Read after source dropped", str->read(str, 0, 1, Buffer_createRef(out, 1), t->alloc, &t->err));
		Test_assert(t, "Read after source dropped value", out[0] == 37);

		RefPtr_dec(&range);
	}

	else Test_assert(t, "Create", false);

	if (source)
		RefPtr_dec(&source);

	src = Buffer_createRefConst(bytes, sizeof(bytes));

	if (!MemoryStream_createFromBuffer(&src, EMemoryStreamFlags_None, &memType, &source, &t->err)) {
		Test_assert(t, "Source again", false);
		return;
	}

	//An explicit length bounds the window below the source's end.

	if (Stream_createRange(source, 16, 32, &rangeType, &range, &t->err)) {

		OxStream *str = RefPtr_data(range, OxStream);
		U8 out[1];

		Test_assert(t, "Length", str->size == 32);
		Test_assert(t, "Inside the length", str->read(str, 31, 1, Buffer_createRef(out, 1), t->alloc, &t->err));
		Test_assert(t, "Past the length refused", !str->read(str, 32, 1, Buffer_createRef(out, 1), t->alloc, NULL));

		RefPtr_dec(&range);
	}

	else Test_assert(t, "Create with length", false);

	Test_assert(t, "Offset past the end refused", !Stream_createRange(source, 257, 0, &rangeType, &range, NULL));
	Test_assert(t, "Length past the end refused", !Stream_createRange(source, 200, 57, &rangeType, &range, NULL));
	Test_assert(t, "Refused leaves no stream", !range);

	//A source that decodes per block can only be windowed on a block boundary.

	Stream_setBlock(source, 16, EStreamReadCost_Memcpy);

	Test_assert(t, "Offset inside a block refused", !Stream_createRange(source, 8, 0, &rangeType, &range, NULL));

	if (Stream_createRange(source, 32, 0, &rangeType, &range, &t->err)) {
		Test_assert(t, "Block size forwarded", RefPtr_data(range, OxStream)->blockSize == 16);
		RefPtr_dec(&range);
	}

	else Test_assert(t, "Offset on a block accepted", false);

	RefPtr_dec(&source);
	Test_setModule(t, NULL);
}
