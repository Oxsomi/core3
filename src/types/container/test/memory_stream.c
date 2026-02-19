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
#include "shared.h"

static void Test_memoryStreamCreate(Test *t, const RefPtrType *type) {

	Test_setModule(t, "MemoryStream create");

	RefPtr *stream = NULL;
	Buffer  full   = Buffer_createNull();

	//Empty

	if (MemoryStream_create(0, EMemoryStreamFlags_IsWritable | EMemoryStreamFlags_IsResizable, type, &stream, &t->err))
		RefPtr_dec(&stream);

	else Test_assert(t, "Create empty", false);

	//Sized

	if (MemoryStream_create(1024, EMemoryStreamFlags_IsWritable | EMemoryStreamFlags_IsResizable, type, &stream, &t->err)) {
		MemoryStream *ms = RefPtr_data(stream, MemoryStream);
		Test_assert(t, "Sized: size",   ms->parent.size == 1024);
		Test_assert(t, "Sized: length", Buffer_length(ms->data) == 1024);
		RefPtr_dec(&stream);
	}
	else Test_assert(t, "Create sized", false);

	//From buffer (owned)

	const Bool allocOk = Buffer_createUninitializedBytes(256, t->alloc, &full, &t->err);
	Test_assert(t, "FromBuffer owned: uninit alloc", allocOk);

	if (allocOk) {

		Buffer_setAllToU8(full, 0xAB, NULL);

		if (MemoryStream_createFromBuffer(&full, EMemoryStreamFlags_None, type, &stream, &t->err)) {
			MemoryStream *ms = RefPtr_data(stream, MemoryStream);
			Test_assert(t, "FromBuffer: moves buffer", !full.ptr);
			Test_assert(t, "FromBuffer: size",         ms->parent.size == 256);

			const U64 *v64  = (const U64*) ms->data.ptr;
			Bool contentsOk = true;

			for (U8 i = 0; i < 256 / 8; ++i)
				if (v64[i] != 0xABABABABABABABAB) {
					contentsOk = false;
					break;
				}

			Test_assert(t, "FromBuffer: contents", contentsOk);
			RefPtr_dec(&stream);
		}
		else Test_assert(t, "FromBuffer owned: create", false);
	}

	//From buffer (const) — writable must be rejected, readonly must succeed

	const U8 constData[100] = { 0 };
	Buffer bufConst = Buffer_createRefConst(constData, 100);

	Test_assert(t, "FromBuffer: reject const + writable",
		!MemoryStream_createFromBuffer(&bufConst, EMemoryStreamFlags_IsWritable, type, &stream, NULL)
	);

	if (MemoryStream_createFromBuffer(&bufConst, EMemoryStreamFlags_None, type, &stream, &t->err))
		RefPtr_dec(&stream);

	else Test_assert(t, "FromBuffer: accept const + readonly", false);
}

static void Test_memoryStreamReadWrite(Test *t, const RefPtrType *type) {

	Test_setModule(t, "MemoryStream readWrite");

	RefPtr *stream = NULL;
	Buffer  full   = Buffer_createNull();

	const Bool allocOk = Buffer_createUninitializedBytes(100, t->alloc, &full, &t->err);
	Test_assert(t, "Alloc 100 bytes", allocOk);

	if (!allocOk)
		return;

	for (U8 i = 0; i < 100; ++i)
		full.ptrNonConst[i] = i;

	if (!MemoryStream_createFromBuffer(&full, EMemoryStreamFlags_IsWritable, type, &stream, &t->err)) {
		Test_assert(t, "Create stream", false);
		Buffer_free(&full, t->alloc);
		return;
	}

	Stream      *s  = RefPtr_data(stream, Stream);
	MemoryStream *ms = RefPtr_data(stream, MemoryStream);

	U8     data[50];
	Buffer buf2 = Buffer_createRef(data, 50);

	if (s->read(s, 10, 50, buf2, t->alloc, &t->err)) {

		Bool ok = true;

		for (U8 i = 10; i < 60; ++i)
			if (data[i - 10] != i) {
				ok = false;
				break;
			}

		Test_assert(t, "Read [10, 60) contents", ok);
	}
	
	else Test_assert(t, "Read [10, 60)", false);

	for (U8 i = 0; i < 50; ++i)
		data[i] = i;

	if (s->write(s, 10, 50, buf2, t->alloc, &t->err)) {

		Bool ok = true;

		for (U8 i = 0; i < 100; ++i)
			if (ms->data.ptr[i] != (i >= 10 && i < 60 ? (U8)(i - 10) : i)) {
				ok = false;
				break;
			}

		Test_assert(t, "Write [10, 60) contents", ok);
	}
	
	else Test_assert(t, "Write [10, 60)", false);

	RefPtr_dec(&stream);
}

static void Test_memoryStreamResize(Test *t, const RefPtrType *type) {

	Test_setModule(t, "MemoryStream resize");

	RefPtr *stream = NULL;
	Buffer  full   = Buffer_createNull();

	if (!MemoryStream_create(100, EMemoryStreamFlags_IsWritable | EMemoryStreamFlags_IsResizable, type, &stream, &t->err)) {
		Test_assert(t, "Create resizable", false);
		return;
	}

	Stream      *s  = RefPtr_data(stream, Stream);
	MemoryStream *ms = RefPtr_data(stream, MemoryStream);

	const Bool allocOk = Buffer_createUninitializedBytes(50, t->alloc, &full, &t->err);
	Test_assert(t, "Alloc 50 bytes", allocOk);

	if (!allocOk) {
		RefPtr_dec(&stream);
		return;
	}

	for (U64 i = 0; i < 50; ++i)
		full.ptrNonConst[i] = (U8)i;

	if (s->write(s, 120, 50, full, t->alloc, &t->err)) {
		Test_assert(t, "Grown to 170", s->size == 170);

		Bool ok = true;
		for (U8 i = 0; i < 50; ++i)
			if (ms->data.ptr[120 + i] != i) {
				ok = false;
				break;
			}

		Test_assert(t, "Data beyond original size correct", ok);
	}

	else Test_assert(t, "Write beyond size (120+50=170)", false);

	RefPtr_dec(&stream);

	//Non-resizable must not grow

	if (MemoryStream_create(100, EMemoryStreamFlags_IsWritable, type, &stream, &t->err)) {
		s = RefPtr_data(stream, Stream);
		Test_assert(t, "Non-resizable: write beyond rejected", !s->write(s, 120, 50, full, t->alloc, NULL));
		Test_assert(t, "Non-resizable: size unchanged",          s->size == 100);
		RefPtr_dec(&stream);
	}
	
	else Test_assert(t, "Create non-resizable", false);

	Buffer_free(&full, t->alloc);
}

static void Test_memoryStreamReserve(Test *t, const RefPtrType *type) {

	Test_setModule(t, "MemoryStream reserve");

	RefPtr *stream = NULL;

	//Reserve 5000 on resizable stream

	if (!MemoryStream_create(100, EMemoryStreamFlags_IsWritable | EMemoryStreamFlags_IsResizable, type, &stream, &t->err)) {
		Test_assert(t, "Create resizable", false);
		return;
	}

	Stream      *s  = RefPtr_data(stream, Stream);
	MemoryStream *ms = RefPtr_data(stream, MemoryStream);

	if (s->reserve(s, 5000, t->alloc, &t->err)) {

		Test_assert(t, "Reserve: capacity >= 5000", Buffer_length(ms->data) >= 5000);
		Test_assert(t, "Reserve: size unchanged",   s->size == 100);

		const void *addrBefore     = ms->data.ptr;
		const U64   capacityBefore = Buffer_length(ms->data);

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

		Test_assert(t, "Reserve: 10x 500B writes succeeded", writesOk);

		if (writesOk) {
			Test_assert(t, "Reserve: no reallocation", Buffer_length(ms->data) == capacityBefore && ms->data.ptr == addrBefore);
			Test_assert(t, "Reserve: size is 5000",    s->size == 5000);

			Bool dataOk = true;
			for (U64 i = 0; i < 5000; ++i)
				if (ms->data.ptr[i] != (U8)(i % 500)) {
					dataOk = false;
					break;
				}

			Test_assert(t, "Reserve: data correct after writes", dataOk);
		}
	}
	
	else Test_assert(t, "Reserve(5000)", false);

	RefPtr_dec(&stream);

	//Reserve on non-resizable must fail

	if (MemoryStream_create(100, EMemoryStreamFlags_IsWritable, type, &stream, &t->err)) {
		s = RefPtr_data(stream, Stream);
		Test_assert(t, "Non-resizable: reserve rejected", !s->reserve(s, 5000, t->alloc, NULL));
		RefPtr_dec(&stream);
	}

	else Test_assert(t, "Create non-resizable", false);

	//Reserve with amount <= current capacity must not shrink or reallocate

	if (!MemoryStream_create(1000, EMemoryStreamFlags_IsWritable | EMemoryStreamFlags_IsResizable, type, &stream, &t->err)) {
		Test_assert(t, "Create 1000-byte stream", false);
		return;
	}

	s                = RefPtr_data(stream, Stream);
	ms               = RefPtr_data(stream, MemoryStream);
	const void *addr = ms->data.ptr;

	if (s->reserve(s, 500, t->alloc, &t->err)) {
		Test_assert(t, "Reserve < size: no shrink",     Buffer_length(ms->data) == 1000);
		Test_assert(t, "Reserve < size: ptr unchanged", ms->data.ptr == addr);
	}
	
	else Test_assert(t, "Reserve(500) on 1000-byte stream", false);

	if (s->reserve(s, 1000, t->alloc, &t->err)) {
		Test_assert(t, "Reserve == size: no realloc",    Buffer_length(ms->data) == 1000);
		Test_assert(t, "Reserve == size: ptr unchanged", ms->data.ptr == addr);
	}
	
	else Test_assert(t, "Reserve(1000) on 1000-byte stream", false);

	RefPtr_dec(&stream);
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
		Test_assert(t, "Stream is null after move", !stream);
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

static void Test_streamCursorBasic(Test *t, const RefPtrType *type) {

	Test_setModule(t, "StreamCursor basic");

	RefPtr      *stream = NULL;
	StreamCursor cursor = { 0 };

	if (!MemoryStream_create(1024, EMemoryStreamFlags_IsWritable, type, &stream, &t->err)) {
		Test_assert(t, "Create stream", false);
		return;
	}

	MemoryStream *ms = RefPtr_data(stream, MemoryStream);

	//Default (readonly) cursor

	if (StreamCursor_create((StreamRef*)stream, 0, false, t->alloc, &cursor, &t->err)) {
		Test_assert(t, "Create: correct stream ref",   cursor.stream == stream);
		Test_assert(t, "Create: default cache 128KiB", Buffer_length(cursor.cacheData) == 128 * KIBI);
		Test_assert(t, "Create: readonly by default",  StreamCursor_canRead(&cursor));
		StreamCursor_close(&cursor, t->alloc);
	}
	
	else Test_assert(t, "Create readonly cursor", false);

	//Writable cursor

	if (!StreamCursor_create((StreamRef*)stream, 0, true, t->alloc, &cursor, &t->err)) {
		Test_assert(t, "Create writable cursor", false);
		RefPtr_dec(&stream);
		return;
	}

	Test_assert(t, "Create: writable if requested", StreamCursor_canWrite(&cursor));

	//Write 128 U64s

	U64    magic    = 0xDEADBEEFCAFEBABE;
	Buffer magicBuf = Buffer_createRef(&magic, sizeof(magic));

	Bool writesOk = true;
	for (U64 i = 0; i < 1024 / 8; ++i) {

		if (!StreamCursor_write(&cursor, magicBuf, 0, i * sizeof(magic), sizeof(magic), false, t->alloc, &t->err)) {
			writesOk = false;
			break;
		}

		++magic;
	}
	Test_assert(t, "Writes (simple)", writesOk);

	//Writeonly cursor must reject reads

	Test_assert(t, "Writeonly rejects read", !StreamCursor_read(&cursor, magicBuf, 0, 0, 1, false, t->alloc, NULL));

	//Cache contents

	const U64 *cachev64 = (const U64*) cursor.cacheData.ptr;
	magic = 0xDEADBEEFCAFEBABE;

	Bool cacheOk = true;
	for (U64 i = 0; i < 1024 / 8; ++i)
		if (cachev64[i] != magic + i) {
			cacheOk = false;
			break;
		}

	Test_assert(t, "Cache contents after writes", cacheOk);

	//Flush

	if (StreamCursor_flush(&cursor, t->alloc, &t->err)) {

		const U64 *streamv64 = (const U64*) ms->data.ptr;
		Bool flushOk = true;
		for (U64 i = 0; i < 1024 / 8; ++i)
			if (streamv64[i] != magic + i) {
				flushOk = false;
				break;
			}

		Test_assert(t, "Flush: data in stream", flushOk);
	}
	
	else Test_assert(t, "Flush", false);

	//Switch to readonly

	if (StreamCursor_setReadOnly(&cursor, t->alloc, &t->err)) {
		Test_assert(t, "SetReadOnly: canRead",   StreamCursor_canRead(&cursor));
		Test_assert(t, "Readonly rejects write", !StreamCursor_write(&cursor, magicBuf, 0, 0, 1, false, t->alloc, NULL));
	}
	
	else Test_assert(t, "SetReadOnly", false);

	//Readback

	Bool readOk = true;
	for (U64 i = 0; i < 1024 / 8; ++i) {

		if (!StreamCursor_read(&cursor, magicBuf, i * sizeof(magic), 0, sizeof(magic), false, t->alloc, &t->err)) {
			readOk = false;
			break;
		}

		if (magic != 0xDEADBEEFCAFEBABE + i) {
			readOk = false;
			break;
		}
	}
	Test_assert(t, "Readback after flush", readOk);

	StreamCursor_close(&cursor, t->alloc);
	RefPtr_dec(&stream);
}

static void Test_streamCursorCaching(Test *t, const RefPtrType *type) {

	Test_setModule(t, "StreamCursor caching");

	RefPtr      *stream = NULL;
	StreamCursor cursor = { 0 };
	Buffer       full   = Buffer_createNull();

	if (!MemoryStream_create(96 * KIBI, EMemoryStreamFlags_IsWritable, type, &stream, &t->err)) {
		Test_assert(t, "Create stream", false);
		return;
	}

	MemoryStream *ms = RefPtr_data(stream, MemoryStream);

	if (!StreamCursor_create((StreamRef*)stream, 32 * KIBI, true, t->alloc, &cursor, &t->err)) {
		Test_assert(t, "Create cursor", false);
		RefPtr_dec(&stream);
		return;
	}

	U64    magic    = 0xDEADBEEFCAFEBABE;
	Buffer magicBuf = Buffer_createRef(&magic, sizeof(magic));

	//Sequential writes across all 96KiB, checking eviction boundaries

	Bool writeOk = true;

	for (U64 i = 0; i < 96 * KIBI / 8; ++i) {

		if (!StreamCursor_write(&cursor, magicBuf, 0, i * sizeof(magic), sizeof(magic), false, t->alloc, &t->err)) {
			writeOk = false;
			break;
		}

		++magic;

		if (!((i * 8) & (32 * KIBI - 1)))
			if (cursor.lastWriteLocation != (i + 1) * 8 || cursor.lastLocation != i * 8) {
				writeOk = false;
				break;
			}
	}

	Test_assert(t, "Sequential writes + eviction boundaries", writeOk);

	//0->64KiB should have been flushed to the stream; 64KiB->96KiB still in cursor cache

	const U64 *streamv64 = (const U64*) ms->data.ptr;
	magic = 0xDEADBEEFCAFEBABE;

	Bool flushedOk = true;

	for (U64 i = 0; i < 64 * KIBI / 8; ++i)
		if (streamv64[i] != magic + i) {
			flushedOk = false;
			break;
		}

	Test_assert(t, "Flushed region (0->64KiB) in stream", flushedOk);

	const U64 *cachev64 = (const U64*) cursor.cacheData.ptr;
	Bool cachedOk = true;

	for (U64 i = 0; i < 32 * KIBI / 8; ++i)
		if (cachev64[i] != magic + 64 * KIBI / 8 + i) {
			cachedOk = false;
			break;
		}

	Test_assert(t, "Live region (64KiB->96KiB) in cache", cachedOk);

	Test_assert(t, "lastLocation / lastWriteLocation",
		cursor.lastLocation == 64 * KIBI && cursor.lastWriteLocation == 96 * KIBI
	);

	if (!StreamCursor_setReadOnly(&cursor, t->alloc, &t->err)) {
		Test_assert(t, "SetReadOnly", false);
		goto clean;
	}

	//Sequential readback

	Bool seqReadOk = true;

	for (U64 i = 0; i < 96 * KIBI / 8; ++i) {

		if (!StreamCursor_read(&cursor, magicBuf, i * sizeof(magic), 0, sizeof(magic), false, t->alloc, &t->err)) {
			seqReadOk = false;
			break;
		}

		if (magic != 0xDEADBEEFCAFEBABE + i) {
			seqReadOk = false;
			break;
		}

		if (!((i * 8) & (32 * KIBI - 1)) && cursor.lastLocation != i * 8) {
			seqReadOk = false;
			break;
		}
	}

	Test_assert(t, "Sequential readback", seqReadOk);

	//Random access (readonly)

	{
		const U64 offsets[] = { 0, 32 * KIBI, 64 * KIBI };
		const U64 offsetLen = sizeof(offsets) / sizeof(offsets[0]);

		Bool randReadOk = true;

		for (U64 i = 0; i < offsetLen; ++i) {

			if (!StreamCursor_read(&cursor, magicBuf, offsets[i], 0, sizeof(magic), false, t->alloc, &t->err)) {
				randReadOk = false;
				break;
			}

			if (magic != 0xDEADBEEFCAFEBABE + offsets[i] / 8) {
				randReadOk = false;
				break;
			}
		}

		Test_assert(t, "Random read (0 / 32KiB / 64KiB)", randReadOk);

		//Random access (writable) — write i at each offset

		if (!StreamCursor_setWritable(&cursor, &t->err)) {
			Test_assert(t, "SetWritable", false);
			goto clean;
		}

		Bool randWriteOk = true;

		for (U64 i = 0; i < offsetLen; ++i) {
			magic = i;

			if (!StreamCursor_write(&cursor, magicBuf, 0, offsets[i], sizeof(magic), false, t->alloc, &t->err)) {
				randWriteOk = false;
				break;
			}
		}

		Test_assert(t, "Random write (0 / 32KiB / 64KiB)", randWriteOk);

		if (StreamCursor_flush(&cursor, t->alloc, &t->err)) {

			Bool spotOk = true;

			for (U64 i = 0; i < offsetLen; ++i)
				if (streamv64[offsets[i] / 8] != i) {
					spotOk = false;
					break;
				}

			Test_assert(t, "Random write: spot values correct", spotOk);

			Bool surroundOk = true;

			for (U64 i = 0; i < 96 * KIBI / 8; ++i) {

				U64 j = 0;

				for (; j < offsetLen; ++j)
					if (i * 8 == offsets[j])
						break;

				if (j != offsetLen)
					continue;

				if (streamv64[i] != 0xDEADBEEFCAFEBABE + i) {
					surroundOk = false;
					break;
				}
			}

			Test_assert(t, "Random write: surrounding data undisturbed", surroundOk);
		}

		else
			Test_assert(t, "Random write: flush", false);

		//Write at offset+8 — must not disturb offset+0

		Bool plusEightOk = true;

		for (U64 i = 0; i < offsetLen; ++i) {
			magic = i | 0x1000;

			if (!StreamCursor_write(&cursor, magicBuf, 0, offsets[i] + 8, sizeof(magic), false, t->alloc, &t->err)) {
				plusEightOk = false;
				break;
			}
		}

		Test_assert(t, "Write at offset+8", plusEightOk);

		if (StreamCursor_flush(&cursor, t->alloc, &t->err)) {

			Bool base0Ok = true, base8Ok = true;

			for (U64 i = 0; i < offsetLen; ++i) {
				if (streamv64[offsets[i] / 8]     != i)             base0Ok = false;
				if (streamv64[offsets[i] / 8 + 1] != (i | 0x1000)) base8Ok = false;
			}

			Test_assert(t, "offset+8: base values intact", base0Ok);
			Test_assert(t, "offset+8: +8 values correct",  base8Ok);

			Bool restOk = true;

			for (U64 i = 0; i < 96 * KIBI / 8; ++i) {

				U64 j = 0;

				for (; j < offsetLen; ++j)
					if (i * 8 == offsets[j] || i * 8 == offsets[j] + 8)
						break;

				if (j != offsetLen)
					continue;

				if (streamv64[i] != 0xDEADBEEFCAFEBABE + i) {
					restOk = false;
					break;
				}
			}

			Test_assert(t, "offset+8: rest undisturbed", restOk);
		}

		else
			Test_assert(t, "offset+8: flush", false);
	}

	//Buffer exceeding cache size (write zeros, no bypass)

	{
		const Bool largeAllocOk = Buffer_createEmptyBytes(96 * KIBI, t->alloc, &full, NULL);
		Test_assert(t, "Alloc 96KiB", largeAllocOk);

		if (largeAllocOk) {

			if (StreamCursor_write(&cursor, full, 0, 0, 0, false, t->alloc, &t->err)) {

				if (StreamCursor_flush(&cursor, t->alloc, &t->err)) {

					Bool zeroOk = true;

					for (U64 i = 0; i < 96 * KIBI / 8; ++i)
						if (streamv64[i]) {
							zeroOk = false;
							break;
						}

					Test_assert(t, "Large write (> cache): zeros flushed", zeroOk);
				}

				else
					Test_assert(t, "Large write: flush", false);
			}

			else
				Test_assert(t, "Large write (> cache)", false);

			//Bypass cache — write 0xCA without flush

			Buffer_setAllToU8(full, 0xCA, NULL);

			if (StreamCursor_write(&cursor, full, 0, 0, 0, true, t->alloc, &t->err)) {

				Bool bypassOk = true;

				for (U64 i = 0; i < 96 * KIBI / 8; ++i)
					if (streamv64[i] != 0xCACACACACACACACA) {
						bypassOk = false;
						break;
					}

				Test_assert(t, "Bypass cache write", bypassOk);
			}

			else
				Test_assert(t, "Bypass cache write", false);

			Buffer_free(&full, t->alloc);
		}
	}

	//Partial writes — only block boundaries and block-boundary-minus-8 slots

	magic = 0xBABABABABABABABA;

	StreamCursor_write(&cursor, magicBuf, 0, 0,              0, false, t->alloc, &t->err);
	StreamCursor_write(&cursor, magicBuf, 0, 32 * KIBI - 8, 0, false, t->alloc, &t->err);
	StreamCursor_write(&cursor, magicBuf, 0, 32 * KIBI,     0, false, t->alloc, &t->err);
	StreamCursor_write(&cursor, magicBuf, 0, 64 * KIBI - 8, 0, false, t->alloc, &t->err);
	StreamCursor_write(&cursor, magicBuf, 0, 64 * KIBI,     0, false, t->alloc, &t->err);
	StreamCursor_write(&cursor, magicBuf, 0, 96 * KIBI - 8, 0, false, t->alloc, &t->err);

	if (StreamCursor_flush(&cursor, t->alloc, &t->err)) {

		Bool partialOk = true;

		for (U64 i = 0; i < 96 * KIBI / 8; ++i) {
			const U64 off  = (i * 8) & (32 * KIBI - 1);
			const U64 want = (!off || off == 32 * KIBI - 8) ? magic : 0xCACACACACACACACA;

			if (streamv64[i] != want) {
				partialOk = false;
				break;
			}
		}

		Test_assert(t, "Partial writes at cache boundaries", partialOk);
	}

	else
		Test_assert(t, "Partial writes: flush", false);

	//Writes spanning two cache blocks

	magic = 0xBADA55B17C412345;
	const U64 magicOld = magic;

	StreamCursor_write(&cursor, magicBuf, 0, 32 * KIBI - 4, 0, false, t->alloc, &t->err);
	StreamCursor_write(&cursor, magicBuf, 0, 64 * KIBI - 4, 0, false, t->alloc, &t->err);

	Test_assert(t, "OOB write rejected",
		!StreamCursor_write(&cursor, magicBuf, 0, 96 * KIBI - 4, 0, false, t->alloc, NULL)
	);

	if (!StreamCursor_setReadOnly(&cursor, t->alloc, &t->err)) {
		Test_assert(t, "SetReadOnly for span reads", false);
		goto clean;
	}

	magic = 0;

	if (StreamCursor_read(&cursor, magicBuf, 32 * KIBI - 4, 0, 0, false, t->alloc, &t->err))
		Test_assert(t, "Span read at 32KiB-4", magic == magicOld);

	else
		Test_assert(t, "Span read at 32KiB-4", false);

	magic = 0;

	if (StreamCursor_read(&cursor, magicBuf, 64 * KIBI - 4, 0, 0, false, t->alloc, &t->err))
		Test_assert(t, "Span read at 64KiB-4", magic == magicOld);

	else
		Test_assert(t, "Span read at 64KiB-4", false);

	Test_assert(t, "OOB read rejected",
		!StreamCursor_read(&cursor, magicBuf, 96 * KIBI - 4, 0, 0, false, t->alloc, NULL)
	);

clean:
	StreamCursor_close(&cursor, t->alloc);
	RefPtr_dec(&stream);
}

static void Test_streamCursorCopyStream(Test *t, const RefPtrType *type) {

	Test_setModule(t, "StreamCursor copyStream");

	RefPtr      *stream  = NULL;
	RefPtr      *stream1 = NULL;
	StreamCursor cursor  = { 0 };
	StreamCursor cursor1 = { 0 };

	const Bool streamsOk =
		MemoryStream_create(96 * KIBI, EMemoryStreamFlags_None,      type, &stream,  NULL) &&
		MemoryStream_create(96 * KIBI, EMemoryStreamFlags_IsWritable, type, &stream1, NULL);

	Test_assert(t, "Create streams", streamsOk);

	if (!streamsOk)
		goto clean;

	MemoryStream *ms  = RefPtr_data(stream,  MemoryStream);
	MemoryStream *ms1 = RefPtr_data(stream1, MemoryStream);

	Buffer_setAllToU8(ms->data, 0xAA, NULL);

	const Bool cursorsOk =
		StreamCursor_create((StreamRef*)stream,  32 * KIBI, false, t->alloc, &cursor,  NULL) &&
		StreamCursor_create((StreamRef*)stream1, 32 * KIBI, true,  t->alloc, &cursor1, NULL);

	Test_assert(t, "Create cursors", cursorsOk);

	if (!cursorsOk)
		goto clean;

	//Full copy

	if (
		StreamCursor_copyStream(&cursor1, &cursor, 0, 0, 0, t->alloc, &t->err) &&
		StreamCursor_flush(&cursor1, t->alloc, &t->err)
	) {
		const U64 *v64 = (const U64*) ms1->data.ptr;
		Bool ok = true;
		for (U64 i = 0; i < 96 * KIBI / 8; ++i)
			if (v64[i] != 0xAAAAAAAAAAAAAAAA) {
				ok = false;
				break;
			}

		Test_assert(t, "Full copy", ok);
	}
	
	else Test_assert(t, "Full copy", false);

	//Partial copy — first 48KiB only; second half must retain 0xAA

	Buffer_setAllToU8(ms->data, 0xBB, NULL);
	StreamCursor_flush(&cursor, t->alloc, &t->err);

	if (
		StreamCursor_copyStream(&cursor1, &cursor, 48 * KIBI, 0, 0, t->alloc, &t->err) &&
		StreamCursor_flush(&cursor1, t->alloc, &t->err)
	) {
		const U64 *v64 = (const U64*) ms1->data.ptr;
		Bool ok = true;
		for (U64 i = 0; i < 96 * KIBI / 8; ++i) {
			const U64 want = i < 48 * KIBI / 8 ? 0xBBBBBBBBBBBBBBBB : 0xAAAAAAAAAAAAAAAA;
			if (v64[i] != want) {
				ok = false;
				break;
			}
		}

		Test_assert(t, "Partial copy (48KiB)", ok);
	}
	
	else Test_assert(t, "Partial copy (48KiB)", false);

clean:
	StreamCursor_close(&cursor,  t->alloc);
	StreamCursor_close(&cursor1, t->alloc);
	RefPtr_dec(&stream);
	RefPtr_dec(&stream1);
}

void Test_memoryStream(Test *t) {

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	Test_memoryStreamCreate(t,        &type);
	Test_memoryStreamReadWrite(t,     &type);
	Test_memoryStreamResize(t,        &type);
	Test_memoryStreamReserve(t,       &type);
	Test_memoryStreamMove(t,          &type);
	Test_streamCursorBasic(t,         &type);
	Test_streamCursorCaching(t,       &type);
	Test_streamCursorCopyStream(t,    &type);
}
