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
#include "types/container/stream.h"
#include "types/container/buffer.h"
#include "types/test/test.h"

static void Test_streamCursorBasic(Test *t, StreamHarness *h) {

	Test_setModuleH(t, h, "cursorBasic");

	RefPtr      *stream = NULL;
	StreamCursor cursor = { 0 };

	if (!h->create(h, 1024, false, &stream, t)) {
		Test_assert(t, "Create stream", false);
		return;
	}

	OxStream *s = RefPtr_data(stream, OxStream);

	// Default (readonly) cursor, tests stream ref, default cache size, canRead flag.

	if (s->read) {

		if (StreamCursor_create((StreamRef*)stream, 0, false, t->alloc, &cursor, &t->err)) {
			Test_assert(t, "Default cursor: stream ref",   cursor.stream == stream);
			Test_assert(t, "Default cursor: 128KiB cache", Buffer_length(cursor.cacheData) == 128 * KIBI);
			Test_assert(t, "Default cursor: canRead",      StreamCursor_canRead(&cursor));
			StreamCursor_close(&cursor, t->alloc);
		}

		else Test_assert(t, "Create default (readonly) cursor", false);
	}

	if (!s->write)
		goto clean;

	//Writable cursor

	if (!StreamCursor_create((StreamRef*)stream, 0, true, t->alloc, &cursor, &t->err)) {
		Test_assert(t, "Create writable cursor", false);
		goto clean;
	}

	Test_assert(t, "Writable cursor: canWrite", StreamCursor_canWrite(&cursor));

	//Write 128 sequential U64s.

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
	Test_assert(t, "Writeonly rejects read", !StreamCursor_read(&cursor, magicBuf, 0, 0, 1, false, t->alloc, NULL));

	//Flush; read back entire 1KiB at once and verify

	if (StreamCursor_flush(&cursor, t->alloc, &t->err)) {

		if (s->read) {
			U64 readBuf[128];

			if (s->read(s, 0, 1024, Buffer_createRef(readBuf, 1024), t->alloc, &t->err)) {
				Bool flushOk = true;

				for (U64 i = 0; i < 128 && flushOk; ++i)
					if (readBuf[i] != 0xDEADBEEFCAFEBABE + i)
						flushOk = false;

				Test_assert(t, "Flush: all data correct", flushOk);
			}

			else Test_assert(t, "Flush: read back", false);
		}
	}

	else Test_assert(t, "Flush", false);

	if (!s->read)
		goto clean;

	//setReadOnly: canRead true, write rejected

	if (StreamCursor_setReadOnly(&cursor, t->alloc, &t->err)) {

		Test_assert(t, "SetReadOnly: canRead",   StreamCursor_canRead(&cursor));
		Test_assert(t, "Readonly rejects write", !StreamCursor_write(&cursor, magicBuf, 0, 0, 1, false, t->alloc, NULL));

		//Read all 1KiB bytes through the regular way

		Bool readOk = true;
		magic = 0;

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
	}

	else Test_assert(t, "SetReadOnly", false);

	//setWritable: canWrite true, read rejected.
	if (StreamCursor_setWritable(&cursor, &t->err)) {
		Test_assert(t, "SetWritable: canWrite", StreamCursor_canWrite(&cursor));
		Test_assert(t, "Writable rejects read", !StreamCursor_read(&cursor, magicBuf, 0, 0, 1, false, t->alloc, NULL));
	}

	else Test_assert(t, "SetWritable", false);

clean:
	StreamCursor_close(&cursor, t->alloc);
	RefPtr_dec(&stream);
}

static void Test_streamCursorCaching(Test *t, StreamHarness *h) {

	Test_setModuleH(t, h, "cursorCaching");

	RefPtr      *stream = NULL;
	StreamCursor cursor = { 0 };

	U64 dat[98304 / sizeof(U64)] = {0};
	Buffer full   = Buffer_createRef(dat, sizeof(dat));

	U64 dat1[98304 / sizeof(U64)] = { 0 };
	Buffer read   = Buffer_createRef(dat1, sizeof(dat1));

	if (!h->create(h, 96 * KIBI, false, &stream, t)) {
		Test_assert(t, "Create stream", false);
		return;
	}

	OxStream *s = RefPtr_data(stream, OxStream);

	if (!s->write || !s->read)
		goto clean;

	if (!StreamCursor_create((StreamRef*)stream, 32 * KIBI, true, t->alloc, &cursor, &t->err)) {
		Test_assert(t, "Create cursor (32KiB cache)", false);
		goto clean;
	}

	U64    magic    = 0xDEADBEEFCAFEBABE;
	Buffer magicBuf = Buffer_createRef(&magic, sizeof(magic));

	//Sequential writes across all 96 KiB

	Bool writeOk = true;

	for (U64 i = 0; i < 96 * KIBI / 8; ++i) {

		if (!StreamCursor_write(&cursor, magicBuf, 0, i * sizeof(magic), sizeof(magic), false, t->alloc, &t->err)) {
			writeOk = false;
			break;
		}

		++magic;
	}

	Test_assert(t, "Sequential writes", writeOk);

	if (!StreamCursor_flush(&cursor, t->alloc, &t->err)) {
		Test_assert(t, "Flush after sequential writes", false);
		goto clean;
	}

	//Verify entire 96 KiB

	if (s->read(s, 0, 96 * KIBI, full, t->alloc, &t->err)) {

		Bool verifyOk = true;
		const U64 *v64 = (const U64*)full.ptr;

		for (U64 i = 0; i < 96 * KIBI / 8 && verifyOk; ++i)
			if (v64[i] != 0xDEADBEEFCAFEBABE + i)
				verifyOk = false;

		Test_assert(t, "All 96KiB correct after flush", verifyOk);
	}

	if (!StreamCursor_setReadOnly(&cursor, t->alloc, &t->err)) {
		Test_assert(t, "SetReadOnly", false);
		goto clean;
	}

	//Sequential cursor read

	{
		Bool seqOk = true;
		magic = 0;

		for (U64 i = 0; i < 96 * KIBI / 8; ++i) {

			if (!StreamCursor_read(&cursor, magicBuf, i * sizeof(magic), 0, sizeof(magic), false, t->alloc, &t->err)) {
				seqOk = false;
				break;
			}

			if (magic != 0xDEADBEEFCAFEBABE + i) {
				seqOk = false;
				break;
			}
		}

		Test_assert(t, "Sequential readback", seqOk);
	}

	//Random read at cache block boundaries

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

		//Random write: write index i at offsets[i].

		if (!StreamCursor_setWritable(&cursor, &t->err)) {
			Test_assert(t, "SetWritable for random write", false);
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

		if (!StreamCursor_flush(&cursor, t->alloc, &t->err)) {
			Test_assert(t, "Random write: flush", false);
			goto clean;
		}

		//Read back all 96KiB at once; check written slots and undisturbed locations in one pass

		if (s->read(s, 0, 96 * KIBI, full, t->alloc, &t->err)) {
			const U64 *v64 = (const U64*)full.ptr;
			Bool spotOk = true, surroundOk = true;

			for (U64 i = 0; i < 96 * KIBI / 8; ++i) {

				U64 j = 0;

				for (; j < offsetLen; ++j)
					if (i * 8 == offsets[j])
						break;

				if (j < offsetLen) { if (v64[i] != j)                          spotOk     = false; }
				else               { if (v64[i] != 0xDEADBEEFCAFEBABE + i)     surroundOk = false; }
			}

			Test_assert(t, "Random write: values correct",               spotOk);
			Test_assert(t, "Random write: surrounding data undisturbed", surroundOk);
		}

		else Test_assert(t, "Random write: read back", false);

		//Write at offset + 8 must not corrupt offset + 0.

		Bool plusEightOk = true;

		for (U64 i = 0; i < offsetLen; ++i) {
			magic = i | 0x1000;

			if (!StreamCursor_write(&cursor, magicBuf, 0, offsets[i] + 8, sizeof(magic), false, t->alloc, &t->err)) {
				plusEightOk = false;
				break;
			}
		}

		Test_assert(t, "Write at offset+8", plusEightOk);

		if (!StreamCursor_flush(&cursor, t->alloc, &t->err)) {
			Test_assert(t, "offset+8: flush", false);
			goto clean;
		}
		
		if (s->read(s, 0, 96 * KIBI, full, t->alloc, &t->err)) {
			const U64 *v64 = (const U64*)full.ptr;
			Bool base0Ok = true, base8Ok = true, restOk = true;

			for (U64 i = 0; i < 96 * KIBI / 8; ++i) {

				U64 j = 0;

				for (; j < offsetLen; ++j)
					if (i * 8 == offsets[j] || i * 8 == offsets[j] + 8)
						break;

				if (j < offsetLen) {
					if (i * 8 == offsets[j]) { if (v64[i] != j)              base0Ok = false; }
					else                      { if (v64[i] != (j | 0x1000))  base8Ok = false; }
				}
				else { if (v64[i] != 0xDEADBEEFCAFEBABE + i) restOk = false; }
			}

			Test_assert(t, "offset+8: base values intact", base0Ok);
			Test_assert(t, "offset+8: +8 values correct",  base8Ok);
			Test_assert(t, "offset+8: rest undisturbed",    restOk);
		}

		else Test_assert(t, "offset+8: read back", false);
	}

	//Large write (> cache) and bypass

	Buffer_setAllToU8(full, 0, NULL);

	if (
		StreamCursor_write(&cursor, full, 0, 0, 0, false, t->alloc, &t->err) &&
		StreamCursor_flush(&cursor, t->alloc, &t->err)
	) {
		if (s->read(s, 0, 96 * KIBI, read, t->alloc, &t->err)) {

			Bool zeroOk = true;
			const U64 *v64 = (const U64*)read.ptr;

			for (U64 i = 0; i < 96 * KIBI / sizeof(U64) && zeroOk; ++i)
				if (v64[i] != 0)
					zeroOk = false;

			Test_assert(t, "Large write: zeros flushed", zeroOk);
		}

		else Test_assert(t, "Large write: read back", false);
	}

	else Test_assert(t, "Large write (> cache)", false);

	//Bypass cache, write 0xCA directly

	Buffer_setAllToU8(full, 0xCA, NULL);

	if (StreamCursor_write(&cursor, full, 0, 0, 0, true, t->alloc, &t->err)) {

		if (s->read(s, 0, 96 * KIBI, read, t->alloc, &t->err)) {

			Bool zeroOk = true;
			const U64 *v64 = (const U64*)read.ptr;

			for (U64 i = 0; i < 96 * KIBI / sizeof(U64) && zeroOk; ++i)
				if (v64[i] != 0xCACACACACACACACA)
					zeroOk = false;

			Test_assert(t, "Large write (bypass cache): flushed", zeroOk);
		}

		else Test_assert(t, "Large write: read back", false);
	}

	else Test_assert(t, "Bypass cache write", false);

	//Partial writes at cache block boundaries and boundary - 8.
	//Background is 0xCACA� from the bypass write above.

	{
		magic = 0xBABABABABABABABA;

		StreamCursor_write(&cursor, magicBuf, 0, 0,             sizeof(magic), false, t->alloc, &t->err);
		StreamCursor_write(&cursor, magicBuf, 0, 32 * KIBI - 8, sizeof(magic), false, t->alloc, &t->err);
		StreamCursor_write(&cursor, magicBuf, 0, 32 * KIBI,     sizeof(magic), false, t->alloc, &t->err);
		StreamCursor_write(&cursor, magicBuf, 0, 64 * KIBI - 8, sizeof(magic), false, t->alloc, &t->err);
		StreamCursor_write(&cursor, magicBuf, 0, 64 * KIBI,     sizeof(magic), false, t->alloc, &t->err);
		StreamCursor_write(&cursor, magicBuf, 0, 96 * KIBI - 8, sizeof(magic), false, t->alloc, &t->err);

		if (!StreamCursor_flush(&cursor, t->alloc, &t->err)) {
			Test_assert(t, "Partial writes: flush", false);
			goto clean;
		}

		if (s->read(s, 0, 96 * KIBI, full, t->alloc, &t->err)) {
			Bool partialOk = true;
			const U64 *v64 = (const U64*)full.ptr;

			for (U64 i = 0; i < 96 * KIBI / 8 && partialOk; ++i) {
				const U64 off = (i * 8) & (32 * KIBI - 1);
				const U64 want = (!off || off == 32 * KIBI - 8) ? magic : 0xCACACACACACACACA;

				if (v64[i] != want)
					partialOk = false;
			}

			Test_assert(t, "Partial writes at cache boundaries", partialOk);
		}

		else Test_assert(t, "Partial writes: read back", false);
	}

	//Span writes: single write straddling two cache blocks

	{
		const U64 magicSpan = 0xBADA55B17C412345;

		magic = magicSpan;
		StreamCursor_write(&cursor, magicBuf, 0, 32 * KIBI - 4, sizeof(magic), false, t->alloc, &t->err);
		magic = magicSpan;
		StreamCursor_write(&cursor, magicBuf, 0, 64 * KIBI - 4, sizeof(magic), false, t->alloc, &t->err);

		Test_assert(t, "OOB span write rejected",
			!StreamCursor_write(&cursor, magicBuf, 0, 96 * KIBI - 4, sizeof(magic), false, t->alloc, NULL));

		if (!StreamCursor_setReadOnly(&cursor, t->alloc, &t->err)) {
			Test_assert(t, "SetReadOnly for span reads", false);
			goto clean;
		}

		magic = 0;
		Test_assert(t, "Span read at 32KiB-4",
			StreamCursor_read(&cursor, magicBuf, 32 * KIBI - 4, 0, sizeof(magic), false, t->alloc, &t->err) &&
			magic == magicSpan);

		magic = 0;
		Test_assert(t, "Span read at 64KiB-4",
			StreamCursor_read(&cursor, magicBuf, 64 * KIBI - 4, 0, sizeof(magic), false, t->alloc, &t->err) &&
			magic == magicSpan);

		Test_assert(t, "OOB span read rejected",
			!StreamCursor_read(&cursor, magicBuf, 96 * KIBI - 4, 0, sizeof(magic), false, t->alloc, NULL));
	}

clean:
	StreamCursor_close(&cursor, t->alloc);
	RefPtr_dec(&stream);
}

static void Test_streamCursorCopyStream(Test *t, StreamHarness *h) {

	Test_setModuleH(t, h, "cursorCopyStream");

	RefPtr       *stream  = NULL;
	RefPtr       *stream1 = NULL;
	StreamCursor cursor   = { 0 };
	StreamCursor cursor1  = { 0 };
	Buffer       fill     = Buffer_createNull();

	if (!Buffer_createEmptyBytes(96 * KIBI, t->alloc, &fill, &t->err)) {
		Test_assert(t, "Buffer_createEmptyBytes", false);
		return;
	}

	if (!h->create(h, 96 * KIBI, false, &stream, t)) {		//Creates zeroed stream
		Test_assert(t, "Create src stream", false);
		goto clean;
	}

	{
		Buffer_setAllToU8(fill, 0xAA, NULL);

		OxStream *s = RefPtr_data(stream, OxStream);

		if (!s->write(s, 0, 0, fill, t->alloc, &t->err)) {
			Test_assert(t, "Fill src write", false);
			goto clean;
		}
	}

	if (!h->create(h, 96 * KIBI, false, &stream1, t)) {
		Test_assert(t, "Create dst stream", false);
		goto clean;
	}

	const Bool cursorsOk =
		StreamCursor_create((StreamRef*)stream,  32 * KIBI, false, t->alloc, &cursor,  NULL) &&
		StreamCursor_create((StreamRef*)stream1, 32 * KIBI, true,  t->alloc, &cursor1, NULL);

	Test_assert(t, "Create cursors", cursorsOk);

	if (!cursorsOk)
		goto clean;

	//Copy and test

	if (
		StreamCursor_copyStream(&cursor1, &cursor, 0, 0, 0, t->alloc, &t->err) &&
		StreamCursor_flush(&cursor1, t->alloc, &t->err)
	) {
		OxStream *s0 = RefPtr_data(stream, OxStream);
		OxStream *s1 = RefPtr_data(stream1, OxStream);

		//Check dst

		if (s1->read(s1, 0, 96 * KIBI, fill, t->alloc, &t->err)) {
			Bool dstOk = true;
			const U64 *v64 = (const U64*)fill.ptr;

			for (U64 i = 0; i < 96 * KIBI / 8 && dstOk; ++i)
				if (v64[i] != 0xAAAAAAAAAAAAAAAA)
					dstOk = false;

			Test_assert(t, "Full copy: dst correct", dstOk);
		}

		else Test_assert(t, "Full copy: dst read", false);

		//Check src

		if (s0->read(s0, 0, 96 * KIBI, fill, t->alloc, &t->err)) {
			Bool srcOk = true;
			const U64 *v64 = (const U64*)fill.ptr;

			for (U64 i = 0; i < 96 * KIBI / 8 && srcOk; ++i)
				if (v64[i] != 0xAAAAAAAAAAAAAAAA)
					srcOk = false;

			Test_assert(t, "Full copy: src unmodified", srcOk);
		}

		else Test_assert(t, "Full copy: src read", false);
	}

	else Test_assert(t, "Full copy", false);

	//Partial copy: copy first 48 KiB only

	{
		OxStream *s = RefPtr_data(stream, OxStream);
		Buffer_setAllToU8(fill, 0xBB, NULL);

		if (!s->write(s, 0, 0, fill, t->alloc, &t->err)) {
			Test_assert(t, "Overwrite src with 0xBB", false);
			goto clean;
		}

		StreamCursor_close(&cursor,  t->alloc);
		StreamCursor_close(&cursor1, t->alloc);

		const Bool resetOk =
			StreamCursor_create((StreamRef*)stream,  32 * KIBI, false, t->alloc, &cursor,  NULL) &&
			StreamCursor_create((StreamRef*)stream1, 32 * KIBI, true,  t->alloc, &cursor1, NULL);

		if (
			resetOk &&
			StreamCursor_copyStream(&cursor1, &cursor, 48 * KIBI, 0, 0, t->alloc, &t->err) &&
			StreamCursor_flush(&cursor1, t->alloc, &t->err)
		) {

			OxStream *s0 = RefPtr_data(stream, OxStream);
			OxStream *s1 = RefPtr_data(stream1, OxStream);

			//First 48KiB of src must be 0xBB, second 48KiB retains 0xAA from full copy

			if (s1->read(s1, 0, 96 * KIBI, fill, t->alloc, &t->err)) {
				Bool partialOk = true;
				const U64 *v64 = (const U64 *)fill.ptr;

				for (U64 i = 0; i < 96 * KIBI / 8 && partialOk; ++i) {
					const U64 want = i < 48 * KIBI / 8 ? 0xBBBBBBBBBBBBBBBB : 0xAAAAAAAAAAAAAAAA;

					if (v64[i] != want)
						partialOk = false;
				}

				Test_assert(t, "Partial copy (48KiB): dst correct", partialOk);
			}

			else Test_assert(t, "Partial copy (48KiB): dst read", false);

			//Src must be entirely 0xBB

			if (s0->read(s0, 0, 96 * KIBI, fill, t->alloc, &t->err)) {
				Bool srcOk = true;
				const U64 *v64 = (const U64 *)fill.ptr;

				for (U64 i = 0; i < 96 * KIBI / 8 && srcOk; ++i)
					if (v64[i] != 0xBBBBBBBBBBBBBBBB)
						srcOk = false;

				Test_assert(t, "Partial copy (48KiB): src unmodified", srcOk);
			}

			else Test_assert(t, "Partial copy (48KiB): src read", false);
		}

		else Test_assert(t, "Partial copy (48KiB)", false);
	}

clean:
	StreamCursor_close(&cursor,  t->alloc);
	StreamCursor_close(&cursor1, t->alloc);
	Buffer_free(&fill, t->alloc);
	RefPtr_dec(&stream);
	RefPtr_dec(&stream1);
}

void StreamHarness_testCursor(StreamHarness *h, Test *t) {
	Test_streamCursorBasic(t, h);
	Test_streamCursorCaching(t, h);
	Test_streamCursorCopyStream(t, h);
}
