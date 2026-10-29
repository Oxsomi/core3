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

//formats/hdr/test/test_hdr_fixes.c

#include "test_hdr_shared.h"
#include "formats/hdr/hdr_file.h"
#include "types/container/memory_stream.h"
#include "types/container/buffer.h"
#include "types/container/job_queue.h"
#include "types/container/test/basic_alloc.h"
#include "types/container/test/failing_stream.h"
#include "types/base/mathf.h"

#include <locale.h>

//The stream type has to outlive every stream made from it
static RefPtrType streamTypeStorage;

static const RefPtrType *streamType(Test *t) {
	streamTypeStorage = MemoryStream_makeType(t->alloc);
	return &streamTypeStorage;
}

//Allocator that refuses anything big and remembers the biggest request, so a regression that allocates from a header
// count fails cleanly instead of taking the machine down.

#define CAP_LIMIT (16 * 1024 * 1024)

static U64 capLargest;

static Bool capAlloc(void *allocator, U64 length, Buffer *output, Error *e_rr) {

	Bool s_uccess = true;

	(void) allocator;

	if(length > capLargest)
		capLargest = length;

	if(length > CAP_LIMIT)
		retError(clean, Error_outOfMemory(0, "capAlloc() over the cap"));

	gotoIfError3(clean, BasicAllocator_instance.alloc(BasicAllocator_instance.ptr, length, output, e_rr));

clean:
	return s_uccess;
}

static void capFree(void *allocator, Buffer buf) {
	(void) allocator;
	BasicAllocator_instance.free(BasicAllocator_instance.ptr, buf);
}

static const Allocator capAllocator = { .alloc = capAlloc, .free = capFree };

static Bool makeInput(Test *t, const void *bytes, U64 len, StreamRef **src) {
	return MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(bytes, len), 0, len, EMemoryStreamFlags_None, streamType(t), (MemoryStreamRef**) src, &t->err
	);
}

//A header that declares a 65536 x 65536 image over 53 bytes asked for 64 GiB before a single scanline was looked at

void Test_HDRReadHugeHeader(Test *t) {

	Test_setModule(t, "HDR read: header counts bounded by the data");

	static const C8 hdr[] = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 65536 +X 65536\n";
	const U64 len = sizeof(hdr) - 1;

	StreamRef *src = NULL;
	MemoryStreamRef *sink = NULL;
	JobQueue jobs = (JobQueue) { 0 };
	HDRInfo info = (HDRInfo) { 0 };
	U64 off = 0;

	if (
		!makeInput(t, hdr, len, &src) ||
		!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, streamType(t), &sink, &t->err) ||
		!JobQueue_create(HDR_TEST_THREADS, t->alloc, &jobs, &t->err)
	) {
		Test_assert(t, "setup", false);
		goto clean;
	}

	for (U32 keep = 0; keep < 2; ++keep) {

		const EHDRReadFlags flags = keep ? EHDRReadFlags_KeepRGBE : EHDRReadFlags_None;

		capLargest = 0;
		off = 0;
		Test_assert(t, "serial refused", !HDR_read(src, &off, flags, &info, (StreamRef*) sink, 0, &capAllocator, NULL));
		Test_assert(t, "serial allocation", capLargest < 1024 * 1024);
		Test_assert(t, "serial offset kept", off == 0);

		capLargest = 0;
		off = 0;
		Test_assert(
			t, "parallel refused",
			!HDR_readParallel(src, &off, flags, &info, (StreamRef*) sink, 0, &jobs, &capAllocator, NULL)
		);

		Test_assert(t, "parallel allocation", capLargest < 1024 * 1024);

		//Begin gets the buffer from the caller, so there the check is the one before the scan sizes its tables

		Buffer output = Buffer_createNull();
		HDRReadPending *pending = NULL;

		capLargest = 0;

		Test_assert(t, "begin output", Buffer_createEmptyBytes(4096, t->alloc, &output, &t->err));
		Test_assert(t, "begin refused", !HDR_readBegin(
			Buffer_createRefConst(hdr, len), flags, &info, output, NULL, &capAllocator, &pending, NULL
		));

		Test_assert(t, "begin allocation", capLargest < 1024 * 1024);
		Buffer_free(&output, t->alloc);
	}

	//Boundary: a one texel wide image has exactly four bytes per row

	{

		static const C8 body[] = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 3 +X 1\n";
		U8 file[sizeof(body) - 1 + 12] = { 0 };

		for (U64 i = 0; i < sizeof(body) - 1; ++i)
			file[i] = (U8) body[i];

		for (U64 i = sizeof(body) - 1; i < sizeof(file); i += 4) {

			file[i] = 0x80;
			file[i + 1] = 0x80;
			file[i + 2] = 0x80;
			file[i + 3] = 129;
		}

		StreamRef *three = NULL;
		MemoryStreamRef *sink3 = NULL;

		if (
			Test_assert(t, "setup 3 rows", makeInput(t, file, sizeof(file), &three)) &&
			Test_assert(t, "setup sink", MemoryStream_create(
				0, EMemoryStreamFlags_WriteResize, streamType(t), &sink3, &t->err
			))
		) {

			off = 0;
			Test_assert(t, "3 rows in 12 bytes", HDR_read(
				three, &off, EHDRReadFlags_None, &info, (StreamRef*) sink3, 0, t->alloc, &t->err
			));

			Test_assert(t, "3 rows info", info.w == 1 && info.h == 3 && off == sizeof(file));
		}

		RefPtr_dec((RefPtr**) &sink3);
		RefPtr_dec(&three);

		//The same header with a row missing

		StreamRef *two = NULL;
		MemoryStreamRef *sink2 = NULL;

		if (
			Test_assert(t, "setup 2 rows", makeInput(t, file, sizeof(file) - 4, &two)) &&
			Test_assert(t, "setup sink", MemoryStream_create(
				0, EMemoryStreamFlags_WriteResize, streamType(t), &sink2, &t->err
			))
		) {

			off = 0;
			Test_assert(t, "3 rows in 8 bytes refused", !HDR_read(
				two, &off, EHDRReadFlags_None, &info, (StreamRef*) sink2, 0, t->alloc, NULL
			));
		}

		RefPtr_dec((RefPtr**) &sink2);
		RefPtr_dec(&two);
	}

clean:
	JobQueue_free(&jobs);
	RefPtr_dec((RefPtr**) &sink);
	RefPtr_dec(&src);
}

//A CRLF header never saw its blank line, because the line held a \r

void Test_HDRReadCRLF(Test *t) {

	Test_setModule(t, "HDR read: CRLF header");

	static const C8 head[] =
		"#?RADIANCE\r\nFORMAT=32-bit_rle_rgbe\r\nEXPOSURE=2\r\n\r\n-Y 2 +X 2\r\n";

	U8 file[sizeof(head) - 1 + 16] = { 0 };

	for (U64 i = 0; i < sizeof(head) - 1; ++i)
		file[i] = (U8) head[i];

	for (U64 i = 0; i < 4; ++i) {

		U8 *px = file + sizeof(head) - 1 + i * 4;

		px[0] = (U8) (0x80 + i);
		px[1] = 0x40;
		px[2] = 0x20;
		px[3] = 129;
	}

	StreamRef *src = NULL;
	MemoryStreamRef *sink = NULL;
	HDRInfo info = (HDRInfo) { 0 };
	U64 off = 0;

	if (
		Test_assert(t, "setup", makeInput(t, file, sizeof(file), &src)) &&
		Test_assert(t, "setup sink", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, streamType(t), &sink, &t->err))
	) {

		Test_assert(t, "crlf read", HDR_read(
			src, &off, EHDRReadFlags_KeepRGBE, &info, (StreamRef*) sink, 0, t->alloc, &t->err
		));

		Test_assert(t, "crlf info", info.w == 2 && info.h == 2 && info.exposure == 2);
		Test_assert(t, "crlf offset", off == sizeof(file));

		const U8 *data = RefPtr_data(sink, MemoryStream)->data.ptr;

		Test_assert(
			t, "crlf pixels",
			RefPtr_data(sink, OxStream)->size == 16 && data[0] == 0x80 && data[4] == 0x81 && data[12] == 0x83
		);
	}

	RefPtr_dec((RefPtr**) &sink);
	RefPtr_dec(&src);

	//HDR_readHeader leaves the offset on the first scanline

	if(Test_assert(t, "setup header", makeInput(t, file, sizeof(file), &src))) {

		off = 0;
		Test_assert(t, "crlf header", HDR_readHeader(src, &off, EHDRReadFlags_None, &info, t->alloc, &t->err));
		Test_assert(t, "crlf header offset", off == sizeof(head) - 1);
	}

	RefPtr_dec(&src);
}

//What a write keeps is the nearest RGBE value, and writing what was read produces the very same file

void Test_HDRFloatRoundTripStable(Test *t) {

	Test_setModule(t, "HDR: float round trip is within half a step and stable");

	enum { W = 16, H = 3 };

	F32 pixels[W * H * 4];

	for (U32 i = 0; i < W * H; ++i) {

		const F32 base = (F32) (i + 1) * 0.37f;

		pixels[i * 4 + 0] = base * ((i & 1) ? 1.0f : 4.0f);
		pixels[i * 4 + 1] = base * 0.5f;
		pixels[i * 4 + 2] = i % 5 ? base * 0.013f : 0.0f;
		pixels[i * 4 + 3] = 1;
	}

	//Powers of two

	pixels[0] = 1.0f;
	pixels[1] = 0.5f;
	pixels[2] = 0.25f;
	pixels[4] = 1000.0f;
	pixels[5] = 1e-3f;
	pixels[6] = 0.0f;

	StreamRef *input = NULL, *first = NULL, *second = NULL;
	MemoryStreamRef *decoded = NULL;
	const RefPtrType *type = streamType(t);

	if (
		!Test_assert(t, "setup input", MemoryStream_create(
			sizeof(pixels), EMemoryStreamFlags_IsWritable, type, &input, &t->err
		)) ||
		!Test_assert(t, "setup first", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &first, &t->err)) ||
		!Test_assert(t, "setup second", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &second, &t->err)) ||
		!Test_assert(t, "setup decoded", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &decoded, &t->err))
	)
		goto clean;

	OxStream *in = RefPtr_data(input, OxStream);

	Test_assert(t, "fill", in->write(in, 0, sizeof(pixels), Buffer_createRefConst(pixels, sizeof(pixels)), t->alloc, &t->err));

	U64 off = 0;

	Test_assert(t, "write", HDR_write(first, &off, EHDRWriteFlags_None, W, H, t->alloc, input, 0, &t->err));

	const U64 firstLen = off;

	off = 0;

	HDRInfo info = (HDRInfo) { 0 };

	Test_assert(t, "read", HDR_read(first, &off, EHDRReadFlags_None, &info, (StreamRef*) decoded, 0, t->alloc, &t->err));

	const F32 *got = (const F32*) RefPtr_data(decoded, MemoryStream)->data.ptr;
	Bool close = RefPtr_data(decoded, OxStream)->size == sizeof(pixels);

	for (U32 i = 0; close && i < W * H; ++i) {

		const F32 *want = pixels + i * 4, *have = got + i * 4;
		const F32 brightest = F32_max(want[0], F32_max(want[1], want[2]));

		//Half a step of the brightest channel's eight bits, which the exponent places in [128, 256)
		const F32 bound = brightest * (0.5f / 128);

		for (U32 c = 0; c < 3; ++c)
			close = close && F32_abs(have[c] - want[c]) <= bound * 1.0001f;
	}

	Test_assert(t, "within half a step", close);
	Test_assert(t, "1.0 is not bit exact (documented)", got[0] > 1.0f && got[0] < 1.0079f);

	//Writing what was read changes nothing

	StreamRef *back2 = NULL;

	Test_assert(t, "reread input", MemoryStream_create(sizeof(pixels), EMemoryStreamFlags_IsWritable, type, &back2, &t->err));

	if(back2) {

		OxStream *b = RefPtr_data(back2, OxStream);

		Test_assert(t, "fill back", b->write(
			b, 0, sizeof(pixels), Buffer_createRefConst(got, sizeof(pixels)), t->alloc, &t->err
		));

		off = 0;
		Test_assert(t, "write again", HDR_write(second, &off, EHDRWriteFlags_None, W, H, t->alloc, back2, 0, &t->err));

		const MemoryStream *a = RefPtr_data(first, MemoryStream), *c = RefPtr_data(second, MemoryStream);

		Test_assert(t, "same length", off == firstLen);
		Test_assert(t, "same bytes", Buffer_eq(
			Buffer_createRefConst(a->data.ptr, firstLen), Buffer_createRefConst(c->data.ptr, off)
		));
	}

	RefPtr_dec(&back2);

clean:
	RefPtr_dec((RefPtr**) &decoded);
	RefPtr_dec(&second);
	RefPtr_dec(&first);
	RefPtr_dec(&input);
}

//The exposure is written the same under a locale with a decimal comma

void Test_HDRExposureLocale(Test *t) {

	Test_setModule(t, "HDR write: EXPOSURE ignores the locale's decimal separator");

	const C8 *candidates[] = { "de_DE.UTF-8", "de_DE.utf8", "de_DE", "German_Germany.1252", "German", "fr_FR.UTF-8", "fr_FR" };
	const C8 *picked = NULL;

	for (U64 i = 0; !picked && i < sizeof(candidates) / sizeof(candidates[0]); ++i)
		if(setlocale(LC_NUMERIC, candidates[i]))
			picked = candidates[i];

	F32 pixel[4] = { 0.5f, 0.25f, 0.125f, 1 };
	StreamRef *input = NULL, *out = NULL;
	const RefPtrType *type = streamType(t);

	if (
		Test_assert(t, "setup", MemoryStream_create(sizeof(pixel), EMemoryStreamFlags_IsWritable, type, &input, &t->err)) &&
		Test_assert(t, "setup out", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &out, &t->err))
	) {

		OxStream *in = RefPtr_data(input, OxStream);

		Test_assert(t, "fill", in->write(in, 0, sizeof(pixel), Buffer_createRefConst(pixel, sizeof(pixel)), t->alloc, &t->err));

		U64 off = 0;

		Test_assert(t, "write", HDR_writeExposed(out, &off, EHDRWriteFlags_None, 1, 1, 1.5f, t->alloc, input, 0, &t->err));

		static const C8 want[] = "EXPOSURE=1.5\n";
		const C8 *data = (const C8*) RefPtr_data(out, MemoryStream)->data.ptr;
		Bool found = false;

		for (U64 i = 0; !found && i + sizeof(want) - 1 <= off; ++i) {

			found = true;

			for (U64 j = 0; found && j < sizeof(want) - 1; ++j)
				found = data[i + j] == want[j];
		}

		Test_assert(t, "exposure uses a point", found);

		//And it reads back as the same exposure

		HDRInfo info = (HDRInfo) { 0 };
		U64 readOff = 0;

		Test_assert(t, "header", HDR_readHeader(out, &readOff, EHDRReadFlags_None, &info, t->alloc, &t->err));
		Test_assert(t, "exposure reads back", info.exposure == 1.5f);
	}

	setlocale(LC_NUMERIC, "C");

	RefPtr_dec(&out);
	RefPtr_dec(&input);

	if(!picked)
		Test_print(t, "HDR exposure locale test ran in the C locale only (no decimal comma locale installed)");
}

//Failing each write in turn: every failure is reported and leaves the caller's offset where it was

void Test_HDRWriteFailedWrites(Test *t) {

	Test_setModule(t, "HDR write: failed writes are reported and keep the offset");

	const U32 size = 16;
	const RefPtrType *type = streamType(t);
	StreamRef *input = NULL, *out = NULL;
	U64 off = 0;

	if (
		!Test_assert(t, "input", MemoryStream_create(
			(U64) size * size * 4 * sizeof(F32), EMemoryStreamFlags_IsWritable, type, &input, &t->err
		)) ||
		!Test_assert(t, "create", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &out, &t->err))
	)
		goto clean;

	FailingStream_arm(out, 0);
	Test_assert(t, "write ok", HDR_writeExposed(
		out, &off, EHDRWriteFlags_None, size, size, 1, t->alloc, input, 0, &t->err
	));

	const U64 writes = FailingStream_writes();

	Test_assert(t, "wrote something", writes > 0);
	RefPtr_dec(&out);

	for (U64 i = 1; i <= writes; ++i) {

		if(!Test_assert(t, "create", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &out, &t->err)))
			break;

		FailingStream_arm(out, i);

		Error err = (Error) { 0 };

		off = 0;
		Test_assert(t, "failed write is reported", !HDR_writeExposed(
			out, &off, EHDRWriteFlags_None, size, size, 1, t->alloc, input, 0, &err
		));

		Test_assert(t, "the write was reached", FailingStream_failed() && err.genericError);
		Test_assert(t, "a failed write leaves the offset", !off);
		RefPtr_dec(&out);
	}

clean:
	RefPtr_dec(&out);
	RefPtr_dec(&input);
}

//The output buffer a caller hands HDR_readBegin has no alignment promise

void Test_HDRReadUnalignedOutput(Test *t) {

	Test_setModule(t, "HDR read: output buffer at an odd address");

	enum { W = 16, H = 4 };

	F32 pixels[W * H * 4];

	for (U32 i = 0; i < W * H; ++i) {

		pixels[i * 4 + 0] = (F32) (i + 1) * 0.25f;
		pixels[i * 4 + 1] = 0.5f;
		pixels[i * 4 + 2] = 0.125f;
		pixels[i * 4 + 3] = 1;
	}

	StreamRef *input = NULL, *encodedStream = NULL;
	const RefPtrType *type = streamType(t);
	Buffer encoded = Buffer_createNull(), aligned = Buffer_createNull(), odd = Buffer_createNull();
	JobQueue jobs = (JobQueue) { 0 };

	if (
		!Test_assert(t, "setup", MemoryStream_create(sizeof(pixels), EMemoryStreamFlags_IsWritable, type, &input, &t->err)) ||
		!Test_assert(t, "setup out", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &encodedStream, &t->err)) ||
		!Test_assert(t, "setup jobs", JobQueue_create(HDR_TEST_THREADS, t->alloc, &jobs, &t->err))
	)
		goto clean;

	OxStream *in = RefPtr_data(input, OxStream);

	Test_assert(t, "fill", in->write(in, 0, sizeof(pixels), Buffer_createRefConst(pixels, sizeof(pixels)), t->alloc, &t->err));

	U64 off = 0;

	Test_assert(t, "write", HDR_write(encodedStream, &off, EHDRWriteFlags_None, W, H, t->alloc, input, 0, &t->err));

	const U64 encodedLen = off;
	const U64 rowsBytes = sizeof(pixels);

	Test_assert(t, "encoded copy", Buffer_createUninitializedBytes(encodedLen, t->alloc, &encoded, &t->err));
	Test_assert(t, "read encoded", Stream_read(
		RefPtr_data(encodedStream, OxStream), 0, encodedLen, encoded, t->alloc, &t->err
	));

	Test_assert(t, "aligned output", Buffer_createUninitializedBytes(rowsBytes + 8, t->alloc, &aligned, &t->err));
	Test_assert(t, "odd output", Buffer_createUninitializedBytes(rowsBytes + 8, t->alloc, &odd, &t->err));

	for (U32 withJobs = 0; withJobs < 2; ++withJobs) {

		HDRInfo info = (HDRInfo) { 0 };
		HDRReadPending *pending = NULL;

		Test_assert(t, "begin aligned", HDR_readBegin(
			encoded, EHDRReadFlags_None, &info, Buffer_createRef(aligned.ptrNonConst, rowsBytes), withJobs ? &jobs : NULL,
			t->alloc, &pending, &t->err
		));

		Test_assert(t, "end aligned", HDR_readEnd(&pending, NULL, &t->err));

		//One byte in: no float in this buffer is aligned

		Test_assert(t, "begin odd", HDR_readBegin(
			encoded, EHDRReadFlags_None, &info, Buffer_createRef(odd.ptrNonConst + 1, rowsBytes), withJobs ? &jobs : NULL,
			t->alloc, &pending, &t->err
		));

		Test_assert(t, "end odd", HDR_readEnd(&pending, NULL, &t->err));

		Test_assert(t, "same pixels", Buffer_eq(
			Buffer_createRefConst(aligned.ptr, rowsBytes), Buffer_createRefConst(odd.ptr + 1, rowsBytes)
		));

		const F32 *first = (const F32*) aligned.ptr;

		Test_assert(t, "pixel decoded", first[0] > 0.24f && first[0] < 0.26f && first[3] == 1);
	}

clean:
	JobQueue_free(&jobs);
	Buffer_free(&odd, t->alloc);
	Buffer_free(&aligned, t->alloc);
	Buffer_free(&encoded, t->alloc);
	RefPtr_dec(&encodedStream);
	RefPtr_dec(&input);
}
