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

//formats/hdr/test/test_hdr_validation.c

#include "test_hdr_shared.h"
#include "formats/hdr/hdr_file.h"
#include "types/container/memory_stream.h"
#include "types/container/buffer.h"

//Every case here must FAIL, so the refusal is passed a NULL Error rather than the test's own:
//a refusal is the expected outcome here, and routing it into t->err would report the right behaviour as a failure.

static void expectWriteRefused(Test *t, const C8 *name, U32 w, U32 h, U64 srcTexels) {

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	StreamRef *src = NULL, *sink = NULL;
	U64 off = 0;

	if(
		!MemoryStream_create(
			srcTexels * 4 * sizeof(F32) + 1, EMemoryStreamFlags_IsWritable, &type,
			(MemoryStreamRef**) &src, &t->err
		) ||
		!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, (MemoryStreamRef**) &sink, &t->err)
	) {
		Test_assert(t, name, false);
		goto clean;
	}

	Test_assert(t, name, !HDR_write(sink, &off, EHDRWriteFlags_None, w, h, t->alloc, src, 0, NULL));

clean:
	RefPtr_dec(&sink);
	RefPtr_dec(&src);
}

void Test_HDRWriteZeroDimensions(Test *t) {
	Test_setModule(t, "HDR write refuses a zero dimension");
	expectWriteRefused(t, "w = 0", 0, 4, 4);
	expectWriteRefused(t, "h = 0", 4, 0, 4);
}

void Test_HDRWriteOversizedDimensions(Test *t) {
	Test_setModule(t, "HDR write refuses a dimension past HDR_MAX_DIM");
	expectWriteRefused(t, "w too large", HDR_MAX_DIM + 1, 1, 1);
	expectWriteRefused(t, "h too large", 1, HDR_MAX_DIM + 1, 1);
}

//The source has to hold what the dimensions claim. Bounds-checking it here is what stops the encoder reading past a stream
// that a caller sized wrongly.

void Test_HDRWriteShortInput(Test *t) {
	Test_setModule(t, "HDR write refuses a source shorter than w * h");
	expectWriteRefused(t, "source too small", 16, 16, 4);
}

//Decoder refusals. Each feeds bytes that are wrong in exactly one way.

static void expectReadRefused(Test *t, const C8 *name, const C8 *bytes, U64 len) {

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	StreamRef *src = NULL;
	MemoryStreamRef *sink = NULL;
	HDRInfo info = { 0 };
	U64 off = 0;

	if(
		!MemoryStream_createFromBufferRegion(
			Buffer_createRefConst(bytes, len), 0, len, EMemoryStreamFlags_None, &type,
			(MemoryStreamRef**) &src, &t->err
		) ||
		!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &sink, &t->err)
	) {
		Test_assert(t, name, false);
		goto clean;
	}

	Test_assert(t, name, !HDR_read(src, &off, EHDRReadFlags_None, &info, (StreamRef*) sink, 0, t->alloc, NULL));

clean:
	RefPtr_dec((RefPtr**) &sink);
	RefPtr_dec(&src);
}

void Test_HDRReadInvalidMagic(Test *t) {
	Test_setModule(t, "HDR read refuses a file that is not Radiance");
	static const C8 notHdr[] = "BM\x36\x00\x00\x00 definitely a bitmap";
	expectReadRefused(t, "wrong magic", notHdr, sizeof(notHdr) - 1);
}

void Test_HDRReadMissingFormat(Test *t) {
	Test_setModule(t, "HDR read refuses a header with no FORMAT line");
	static const C8 noFormat[] = "#?RADIANCE\n\n-Y 4 +X 4\n";
	expectReadRefused(t, "no FORMAT", noFormat, sizeof(noFormat) - 1);
}

//XYZE decodes with identical RLE and exponent maths, so it would read as RGB without complaint.
//Refusing it is deliberate: a wrong colour space is worse than a failed load, because nothing downstream can see it.

void Test_HDRReadUnsupportedFormat(Test *t) {
	Test_setModule(t, "HDR read refuses XYZE rather than reading it as RGB");
	static const C8 xyze[] = "#?RADIANCE\nFORMAT=32-bit_rle_xyze\n\n-Y 4 +X 4\n";
	expectReadRefused(t, "XYZE", xyze, sizeof(xyze) - 1);
}

//The decoder writes rows into a stream it does not own, so there is no result buffer to arrive filled.
//What replaces that check is refusing a sink it cannot write to.

//A repeat of zero advances nothing, so a file made of them would spin while the shift that widens the count
// climbed past 32 and became undefined. Refused rather than survived.

void Test_HDRReadZeroRepeat(Test *t) {

	Test_setModule(t, "HDR read refuses a zero length repeat");

	//Four wide so the scanline takes the flat path, one texel, then a (1, 1, 1, 0) repeat triple.

	static const C8 spin[] =
		"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 4\n"
		"\x0a\x14\x1e\x80"
		"\x01\x01\x01\x00"
		"\x01\x01\x01\x00";

	expectReadRefused(t, "zero repeat", spin, sizeof(spin) - 1);
}

void Test_HDRReadUnwritableSink(Test *t) {

	Test_setModule(t, "HDR read refuses an unwritable sink");

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	static const C8 hdr[] = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 1\n";

	StreamRef *src = NULL;
	MemoryStreamRef *sink = NULL;
	HDRInfo info = { 0 };
	U64 off = 0;

	if(
		!MemoryStream_createFromBufferRegion(
			Buffer_createRefConst(hdr, sizeof(hdr) - 1), 0, sizeof(hdr) - 1,
			EMemoryStreamFlags_None, &type, (MemoryStreamRef**) &src, &t->err
		) ||
		!MemoryStream_create(4, EMemoryStreamFlags_None, &type, &sink, &t->err)
	) {
		Test_assert(t, "setup", false);
		goto clean;
	}

	Test_assert(t, "read-only sink refused",
		!HDR_read(src, &off, EHDRReadFlags_None, &info, (StreamRef*) sink, 0, t->alloc, NULL)
	);

clean:
	RefPtr_dec((RefPtr**) &sink);
	RefPtr_dec(&src);

}
