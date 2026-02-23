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

#include "types/container/test/stream.h"
#include "types/container/memory_stream.h"
#include "types/test/test.h"

static void Test_streamCreate(Test *t, StreamHarness *h) {

	Test_setModuleH(t, h, "streamCreate");

	RefPtr *stream = NULL;

	//Empty

	if (h->create(h, 0, false, &stream, t))
		RefPtr_dec(&stream);

	else Test_assert(t, "Create empty", false);

	//Sized

	if (h->create(h, 1024, false, &stream, t)) {
		const Stream *s = RefPtr_data(stream, Stream);
		Test_assert(t, "Sized: size", s->size == 1024);
		RefPtr_dec(&stream);
	}

	else Test_assert(t, "Create sized", false);
}

static void Test_streamReadWrite(Test *t, StreamHarness *h) {

	Test_setModuleH(t, h, "readWrite");

	RefPtr *stream = NULL;

	if (!h->create(h, 100, false, &stream, t)) {
		Test_assert(t, "Create stream", false);
		return;
	}

	Stream *s = RefPtr_data(stream, Stream);

	if (!s->write)
		goto clean;

	U8 src[100];

	for (U8 i = 0; i < 100; ++i)
		src[i] = i;

	if (!s->write(s, 0, 100, Buffer_createRefConst(src, 100), t->alloc, &t->err)) {
		Test_assert(t, "Write [0,100)", false);
		goto clean;
	}

	//Read back [10,60) through the stream's own read function
	if (s->read) {

		U8 readBuf[50];

		if (s->read(s, 10, 50, Buffer_createRef(readBuf, 50), t->alloc, &t->err)) {
			Bool ok = true;

			for (U8 i = 0; i < 50; ++i)
				if (readBuf[i] != 10 + i) {
					ok = false;
					break;
				}

			Test_assert(t, "Read [10,60) contents", ok);
		}

		else Test_assert(t, "Read [10,60)", false);
	}

	//Overwrite [10,60) with [0..49]; verify entire buffer through harness

	for (U8 i = 0; i < 50; ++i)
		src[i] = i;

	if (s->write(s, 10, 50, Buffer_createRefConst(src, 50), t->alloc, &t->err)) {
		if (s->read) {
			U8 full[100];

			if (s->read(s, 0, 100, Buffer_createRef(full, 100), t->alloc, &t->err)) {
				Bool ok = true;

				for (U8 i = 0; i < 100 && ok; ++i)
					if (full[i] != (i >= 10 && i < 60 ? (U8)(i - 10) : i))
						ok = false;

				Test_assert(t, "Write [10,60) verify full buffer", ok);
			}

			else Test_assert(t, "Read full buffer after write", false);
		}
	}

	else Test_assert(t, "Write [10,60)", false);

clean:
	RefPtr_dec(&stream);
}

static void Test_streamResize(Test *t, StreamHarness *h) {

	Test_setModuleH(t, h, "resize");

	RefPtr *stream = NULL;

	if (!h->create(h, 100, true, &stream, t)) {
		Test_assert(t, "Create resizable", false);
		return;
	}

	Stream *s = RefPtr_data(stream, Stream);

	if (!s->write) {
		RefPtr_dec(&stream);
		return;
	}

	U8 src[50];

	for (U64 i = 0; i < 50; ++i)
		src[i] = (U8)i;

	//Write beyond size, grow to 170

	if (s->write(s, 120, 50, Buffer_createRefConst(src, 50), t->alloc, &t->err)) {

		Test_assert(t, "Grown to 170", s->size == 170);

		if (s->read) {

			//Check new region [120,170)

			U8 newBuf[50];

			if (s->read(s, 120, 50, Buffer_createRef(newBuf, 50), t->alloc, &t->err)) {
				Bool newOk = true;

				for (U8 i = 0; i < 50 && newOk; ++i)
					if (newBuf[i] != i)
						newOk = false;

				Test_assert(t, "New region [120,170) correct", newOk);
			}

			else Test_assert(t, "Read new region [120,170)", false);

			//Check original region [0,100) is still zero

			U8 origBuf[100];

			if (s->read(s, 0, 100, Buffer_createRef(origBuf, 100), t->alloc, &t->err)) {
				Bool origOk = true;

				for (U8 i = 0; i < 100 && origOk; ++i)
					if (origBuf[i] != 0)
						origOk = false;

				Test_assert(t, "Original region [0,100) untouched", origOk);
			}

			else Test_assert(t, "Read original region [0,100)", false);

			//Check gap [100,120) is zero-filled

			U8 gapBuf[20];

			if (s->read(s, 100, 20, Buffer_createRef(gapBuf, 20), t->alloc, &t->err)) {
				Bool gapOk = true;

				for (U8 i = 0; i < 20 && gapOk; ++i)
					if (gapBuf[i] != 0)
						gapOk = false;

				Test_assert(t, "Gap [100,120) zero", gapOk);
			}

			else Test_assert(t, "Read gap [100,120)", false);
		}
	}

	else Test_assert(t, "Write beyond size (120 + 50 = 170)", false);

	RefPtr_dec(&stream);

	//Non-resizable: write beyond end rejected

	if (h->create(h, 100, false, &stream, t)) {
		s = RefPtr_data(stream, Stream);
		Test_assert(t, "Non-resizable: rejected",     !s->write(s, 120, 50, Buffer_createRefConst(src, 50), t->alloc, NULL));
		Test_assert(t, "Non-resizable: size unchanged", s->size == 100);
		RefPtr_dec(&stream);
	}

	else Test_assert(t, "Create non-resizable", false);
}

static void Test_streamReserve(Test *t, StreamHarness *h) {

	Test_setModuleH(t, h, "reserve");

	RefPtr *stream = NULL;

	if (!h->create(h, 100, true, &stream, t)) {
		Test_assert(t, "Create resizable", false);
		return;
	}

	Stream *s = RefPtr_data(stream, Stream);
	Bool isMemoryStream = s->streamType == EStreamType_Memory;

	if (!s->reserve) {
		RefPtr_dec(&stream);
		return;
	}

	//Reserve 5000, then do 10 x 500-byte writes

	if (s->reserve(s, 5000, t->alloc, &t->err)) {

		Test_assert(t, "Reserve: size unchanged", s->size == 100);

		//For MemoryStream: record ptr + capacity so we can verify no reallocation occurs

		const MemoryStream *ms       = isMemoryStream ? RefPtr_data(stream, MemoryStream) : NULL;
		const void         *ptrBefore = ms ? ms->data.ptr              : NULL;
		const U64           capBefore = ms ? Buffer_length(ms->data)   : 0;

		U8     data[500];
		Buffer buf2 = Buffer_createRef(data, 500);

		for (U64 i = 0; i < 500; ++i)
			buf2.ptrNonConst[i] = (U8)i;

		Bool writesOk = true;

		for (U64 i = 0; i < 10; ++i)
			if (!s->write(s, i * 500, 500, buf2, t->alloc, &t->err)) {
				writesOk = false;
				break;
			}

		Test_assert(t, "Reserve: 10x 500B writes ok", writesOk);

		if (writesOk) {
			Test_assert(t, "Reserve: size 5000", s->size == 5000);

			if (ms)
				Test_assert(t, "Reserve: no reallocation", ms->data.ptr == ptrBefore && Buffer_length(ms->data) == capBefore);

			U8     data1[5000];
			Buffer readBuf = Buffer_createRef(data1, 5000);

			if (s->read(s, 0, 5000, readBuf, t->alloc, &t->err)) {

				Bool dataOk = true;

				for (U64 i = 0; i < 5000 && dataOk; ++i)
					if (readBuf.ptr[i] != (U8)(i % 500))
						dataOk = false;

				Test_assert(t, "Reserve: data correct", dataOk);
			}

			else Test_assert(t, "Reserve: read back", false);
		}
	}

	else Test_assert(t, "Reserve(5000)", false);

	RefPtr_dec(&stream);

	//Non-resizable must reject reserve

	if (h->create(h, 100, false, &stream, t)) {
		s = RefPtr_data(stream, Stream);
		Test_assert(t, "Non-resizable: reserve rejected", !s->reserve(s, 5000, t->alloc, NULL));
		RefPtr_dec(&stream);
	}

	else Test_assert(t, "Create non-resizable", false);

	//Reserve <= current size must not shrink

	if (h->create(h, 1000, true, &stream, t)) {
		s = RefPtr_data(stream, Stream);

		//For MemoryStream: pointer must not change

		const MemoryStream *ms       = isMemoryStream ? RefPtr_data(stream, MemoryStream) : NULL;
		const void         *ptrBefore = ms ? ms->data.ptr : NULL;

		Test_assert(t, "Reserve < size: succeeds",       s->reserve(s, 500,  t->alloc, &t->err));
		Test_assert(t, "Reserve < size: size unchanged",  s->size == 1000);
		Test_assert(t, "Reserve == size: succeeds",       s->reserve(s, 1000, t->alloc, &t->err));
		Test_assert(t, "Reserve == size: size unchanged", s->size == 1000);

		if (ms)
			Test_assert(t, "Reserve <= size: no realloc", ms->data.ptr == ptrBefore);

		RefPtr_dec(&stream);
	}

	else Test_assert(t, "Create 1000-byte stream", false);
}

void StreamHarness_testStream(StreamHarness *h, Test *t) {
	Test_streamCreate(t, h);
	Test_streamReadWrite(t, h);
	Test_streamResize(t, h);
	Test_streamReserve(t, h);
}

void Test_setModuleH(Test *t, StreamHarness *h, ShortString str) {

	Test_setModule(t, NULL);		//End last, before overwriting

	U64 baseLen = CharString_calcStrLen(h->name, sizeof(ShortString));
	Buffer_memcpy(
		Buffer_createRef(h->currModule, baseLen),
		Buffer_createRefConst(h->name, baseLen)
	);

	h->currModule[baseLen] = '_';

	U64 appendLen = CharString_calcStrLen(str, sizeof(ShortString));
	Buffer_memcpy(
		Buffer_createRef(h->currModule + baseLen + 1, sizeof(ShortString) - 1),
		Buffer_createRefConst(str, appendLen)
	);

	if (baseLen + appendLen + 1 != sizeof(LongString))
		h->currModule[baseLen + appendLen + 1] = '\0';

	Test_setModule(t, h->currModule);
}
