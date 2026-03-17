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

#include "test_bmp_shared.h"
#include "formats/bmp/bmp_file.h"
#include "formats/bmp/bmp_headers.h"
#include "types/container/memory_stream.h"
#include "types/container/texture_format.h"
#include "types/base/mathi.h"

//Helper: allocate a MemoryStream pre-filled with a solid colour pattern.
//Each pixel is written as BGRA8 (B=(U8)x, G=(U8)y, R=(U8)(x + y), A=0xFF).
//Caller owns *dataSr (RefPtr_dec).
static Bool makePixelStream(
	Test *t,
	U32 w,
	U32 h,
	Bool discardAlpha,
	StreamRef **dataSr,
	const RefPtrType *type
) {
	const U64 pixelStride = discardAlpha ? 3 : 4;
	const U64 stride      = ((U64)w * pixelStride + 3) & ~3;
	const U64 total       = stride * h;

	StreamRef *sr = NULL;

	if (!MemoryStream_create(total, EMemoryStreamFlags_IsWritable, type, &sr, &t->err))
		return false;

	OxStream *stream = RefPtr_data(sr, OxStream);

	for (U32 y = 0; y < h; ++y) {
		for (U32 x = 0; x < w; ++x) {
			U64 off = (U64)y * stride + (U64)x * pixelStride;
			U8 px[4] = { (U8)x, (U8)y, (U8)(x + y), 0xFF };
			if (!stream->write(stream, off, 0, Buffer_createRefConst(px, pixelStride), t->alloc, &t->err)) {
				RefPtr_dec(&sr);
				return false;
			}
		}
	}

	*dataSr = sr;
	return true;
}

//Helper: write a BMP then read its header back, verifying metadata.
//Caller owns *archiveSr (RefPtr_dec).
static Bool bmpRoundTripHeader(
	Test *t,
	const BMPInfo *writeInfo,
	StreamRef *dataSr,
	Bool inputIsFlipped,
	StreamRef **archiveSr,
	BMPInfo *readInfo,
	U64 *dataOffset,
	const RefPtrType *type
) {
	StreamRef *sr = NULL;

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &sr, &t->err))
		return false;

	U64 off = 0;

	if (!BMP_write(sr, &off, writeInfo, t->alloc, dataSr, 0, inputIsFlipped, &t->err)) {
		RefPtr_dec(&sr);
		return false;
	}

	U64 readOff = 0;

	if (!BMP_read(sr, &readOff, dataOffset, readInfo, t->alloc, &t->err)) {
		RefPtr_dec(&sr);
		return false;
	}

	*archiveSr = sr;
	return true;
}

//4x4 BGRA8, bottom-up (standard orientation). Verifies header fields survive round-trip.
void Test_BMPRoundTripBGRA8(Test *t) {

	Test_setModule(t, "BMP round-trip: 4x4 BGRA8 bottom-up");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;
		BMPInfo readInfo     = { 0 };
		U64 dataOffset       = 0;

		BMPInfo writeInfo = {
			.w               = 4,
			.h               = 4,
			.isFlipped       = true,
			.textureFormatId = ETextureFormatId_BGRA8,
			.discardAlpha    = false
		};

		if (!makePixelStream(t, 4, 4, false, &dataSr, &type)) {
			Test_assert(t, "make pixel stream", false);
			goto doneBGRA8;
		}

		Test_assert(t, "BGRA8 round-trip",
			bmpRoundTripHeader(t, &writeInfo, dataSr, true, &archiveSr, &readInfo, &dataOffset, &type)
		);

		Test_assert(t, "BGRA8 w",            readInfo.w               == 4);
		Test_assert(t, "BGRA8 h",            readInfo.h               == 4);
		Test_assert(t, "BGRA8 isFlipped",    readInfo.isFlipped       == true);
		Test_assert(t, "BGRA8 fmt",          readInfo.textureFormatId == ETextureFormatId_BGRA8);
		Test_assert(t, "BGRA8 discardAlpha", readInfo.discardAlpha    == false);
		Test_assert(t, "BGRA8 xPixPerM",     readInfo.xPixPerM        == 0);
		Test_assert(t, "BGRA8 yPixPerM",     readInfo.yPixPerM        == 0);

		//dataOffset must point past the combined header (54 bytes)
		Test_assert(t, "BGRA8 dataOffset", dataOffset == sizeof(BMPHeadersCombined));

	doneBGRA8:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
	}
}

//4x4 BGR8 (discardAlpha = true). Verifies 24-bit path and stride padding (4 * 3 = 12, already aligned).
void Test_BMPRoundTripBGR8(Test *t) {

	Test_setModule(t, "BMP round-trip: 4x4 BGR8 (discardAlpha)");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;
		BMPInfo readInfo     = { 0 };
		U64 dataOffset       = 0;

		BMPInfo writeInfo = {
			.w               = 4,
			.h               = 4,
			.isFlipped       = true,
			.textureFormatId = ETextureFormatId_BGRA8,
			.discardAlpha    = true
		};

		if (!makePixelStream(t, 4, 4, true, &dataSr, &type)) {
			Test_assert(t, "make BGR8 stream", false);
			goto doneBGR8;
		}

		Test_assert(t, "BGR8 round-trip",
			bmpRoundTripHeader(t, &writeInfo, dataSr, true, &archiveSr, &readInfo, &dataOffset, &type)
		);

		Test_assert(t, "BGR8 w",            readInfo.w            == 4);
		Test_assert(t, "BGR8 h",            readInfo.h            == 4);
		Test_assert(t, "BGR8 discardAlpha", readInfo.discardAlpha == true);

	doneBGR8:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
	}
}

//Write with isFlipped = true (bottom-up), read back; verify isFlipped preserved.
void Test_BMPRoundTripFlipped(Test *t) {

	Test_setModule(t, "BMP round-trip: isFlipped = true preserved");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;
		BMPInfo readInfo     = { 0 };
		U64 dataOffset       = 0;

		BMPInfo writeInfo = {
			.w = 2, .h = 2,
			.isFlipped       = true,
			.textureFormatId = ETextureFormatId_BGRA8,
			.discardAlpha    = false
		};

		if (!makePixelStream(t, 2, 2, false, &dataSr, &type)) {
			Test_assert(t, "make flipped stream", false);
			goto doneFlipped;
		}

		Test_assert(t, "flipped round-trip",
			bmpRoundTripHeader(t, &writeInfo, dataSr, true, &archiveSr, &readInfo, &dataOffset, &type)
		);

		//Bottom-up BMP has positive height in the info header -> isFlipped = true
		Test_assert(t, "isFlipped true", readInfo.isFlipped == true);

	doneFlipped:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
	}
}

//Write with isFlipped = false (top-down), read back; verify isFlipped = false preserved.
void Test_BMPRoundTripTopDown(Test *t) {

	Test_setModule(t, "BMP round-trip: isFlipped=false (top-down) preserved");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;
		BMPInfo readInfo     = { 0 };
		U64 dataOffset       = 0;

		BMPInfo writeInfo = {
			.w = 2, .h = 2,
			.isFlipped       = false,
			.textureFormatId = ETextureFormatId_BGRA8,
			.discardAlpha    = false
		};

		if (!makePixelStream(t, 2, 2, false, &dataSr, &type)) {
			Test_assert(t, "make top-down stream", false);
			goto doneTopDown;
		}

		Test_assert(t, "top-down round-trip",
			bmpRoundTripHeader(t, &writeInfo, dataSr, false, &archiveSr, &readInfo, &dataOffset, &type)
		);

		//Top-down BMP has negative height in the info header -> isFlipped = false
		Test_assert(t, "isFlipped false", readInfo.isFlipped == false);

	doneTopDown:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
	}
}

//3x3 BGR8: row width 9 bytes padded to 12. Verifies stride padding is applied correctly.
//If padding is missing the fileSize check will fail on read, catching the bug.
void Test_BMPRoundTripStridePadding(Test *t) {

	Test_setModule(t, "BMP round-trip: 3x3 BGR8 stride padding (9->12 bytes/row)");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;
		BMPInfo readInfo     = { 0 };
		U64 dataOffset       = 0;

		BMPInfo writeInfo = {
			.w               = 3,
			.h               = 3,
			.isFlipped       = true,
			.textureFormatId = ETextureFormatId_BGRA8,
			.discardAlpha    = true
		};

		if (!makePixelStream(t, 3, 3, true, &dataSr, &type)) {
			Test_assert(t, "make 3x3 stream", false);
			goto donePadding;
		}

		Test_assert(t, "3x3 stride round-trip",
			bmpRoundTripHeader(t, &writeInfo, dataSr, true, &archiveSr, &readInfo, &dataOffset, &type)
		);

		Test_assert(t, "3x3 w", readInfo.w == 3);
		Test_assert(t, "3x3 h", readInfo.h == 3);

	donePadding:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
	}
}

//Verify that BMP_write emits compression = 3 and compressedSize = expectedLen in the raw header.
//These fields are written by BMP_write but not surfaced through BMPInfo, so we inspect the
//raw stream bytes directly using offsetof rather than relying solely on the read-back path.
void Test_BMPRoundTripHeaderFields(Test *t) {

	Test_setModule(t, "BMP round-trip: raw header has compression=3 and compressedSize=expectedLen");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;

		BMPInfo writeInfo = {
			.w               = 4,
			.h               = 4,
			.isFlipped       = true,
			.textureFormatId = ETextureFormatId_BGRA8,
			.discardAlpha    = false
		};

		if (!makePixelStream(t, 4, 4, false, &dataSr, &type)) {
			Test_assert(t, "make pixel stream header fields", false);
			goto doneHeaderFields;
		}

		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &archiveSr, &t->err)) {
			Test_assert(t, "create archive stream header fields", false);
			goto doneHeaderFields;
		}

		U64 off = 0;
		Test_assert(t, "write succeeds",
			BMP_write(archiveSr, &off, &writeInfo, t->alloc, dataSr, 0, true, &t->err)
		);

		//Read the raw header back from the stream
		BMPHeadersCombined hdr = { 0 };
		OxStream *s = RefPtr_data(archiveSr, OxStream);
		Test_assert(t, "read raw header",
			s->read(s, 0, sizeof(hdr), Buffer_createRef(&hdr, sizeof(hdr)), t->alloc, &t->err)
		);

		//4x4 BGRA8: stride = 16, expectedLen = 64
		const U64 expectedLen = 16 * 4;

		Test_assert(t, "compression == 3",          hdr.info.compression    == 3);
		Test_assert(t, "compressedSize == 64",       hdr.info.compressedSize == (U32)expectedLen);
		Test_assert(t, "fileSize == header + data",  hdr.header.fileSize     == sizeof(BMPHeadersCombined) + expectedLen);

	doneHeaderFields:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
	}
}

//Verify xPixPerM and yPixPerM are preserved exactly through write and read.
void Test_BMPRoundTripPixelDensity(Test *t) {

	Test_setModule(t, "BMP round-trip: pixel density (xPixPerM, yPixPerM) preserved");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;
		BMPInfo readInfo     = { 0 };
		U64 dataOffset       = 0;

		BMPInfo writeInfo = {
			.w               = 2,
			.h               = 2,
			.isFlipped       = true,
			.textureFormatId = ETextureFormatId_BGRA8,
			.discardAlpha    = false,
			.xPixPerM        = 2835,	//72 DPI in px/m
			.yPixPerM        = 2835
		};

		if (!makePixelStream(t, 2, 2, false, &dataSr, &type)) {
			Test_assert(t, "make density stream", false);
			goto doneDensity;
		}

		Test_assert(t, "density round-trip",
			bmpRoundTripHeader(t, &writeInfo, dataSr, true, &archiveSr, &readInfo, &dataOffset, &type)
		);

		Test_assert(t, "xPixPerM", readInfo.xPixPerM == 2835);
		Test_assert(t, "yPixPerM", readInfo.yPixPerM == 2835);

	doneDensity:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
	}
}

//Helper: read one pixel at output position (x, outRow) from the written stream and compare
//it against the expected pattern from makePixelStream for the given original source row.
//makePixelStream pattern: B=(U8)x, G=(U8)origRow, R=(U8)(x+origRow), A=0xFF.
static Bool checkPixel(
	OxStream *s,
	U64 dataOffset,
	U32 x,
	U32 outRow,
	U64 stride,
	U64 pixelStride,
	U32 expectedOrigRow,
	const Allocator *alloc,
	Error *e_rr
) {
	U8 got[4] = { 0 };
	U64 off = dataOffset + (U64)outRow * stride + (U64)x * pixelStride;

	if (!s->read(s, off, pixelStride, Buffer_createRef(got, pixelStride), alloc, e_rr))
		return false;

	U8 exp[4] = { (U8)x, (U8)expectedOrigRow, (U8)(x + expectedOrigRow), 0xFF };

	for (U64 i = 0; i < pixelStride; ++i)
		if (got[i] != exp[i])
			return false;

	return true;
}

//Verify pixel data is preserved correctly when input and output orientation match (fast copy path).
//Uses a 4x4 BGRA8 bottom-up image written and read in the same orientation.
void Test_BMPPixelContentNoFlip(Test *t) {

	Test_setModule(t, "BMP pixel content: no flip, all pixels preserved");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;
		BMPInfo readInfo     = { 0 };
		U64 dataOffset       = 0;

		const U32 w = 4, h = 4;
		const U64 pixelStride = 4;
		const U64 stride      = ((U64)w * pixelStride + 3) & ~3;

		BMPInfo writeInfo = {
			.w               = w,
			.h               = h,
			.isFlipped       = true,
			.textureFormatId = ETextureFormatId_BGRA8,
			.discardAlpha    = false
		};

		if (!makePixelStream(t, w, h, false, &dataSr, &type)) {
			Test_assert(t, "make pixel stream no-flip", false);
			goto doneNoFlip;
		}

		//inputIsFlipped == writeInfo.isFlipped -> fast copy path, no row reorder
		if (!bmpRoundTripHeader(t, &writeInfo, dataSr, true, &archiveSr, &readInfo, &dataOffset, &type)) {
			Test_assert(t, "round-trip no-flip", false);
			goto doneNoFlip;
		}

		OxStream *s = RefPtr_data(archiveSr, OxStream);
		Bool allMatch = true;

		for (U32 y = 0; y < h && allMatch; ++y)
			for (U32 x = 0; x < w && allMatch; ++x)
				//No flip: output row y contains original row y
				if (!checkPixel(s, dataOffset, x, y, stride, pixelStride, y, t->alloc, &t->err))
					allMatch = false;

		Test_assert(t, "all pixels match (no flip)", allMatch);

	doneNoFlip:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
	}
}

//Verify pixel data is correct when write requires a row-order flip.
//Input is top-down (inputIsFlipped = false), output is bottom-up (isFlipped = true).
//Row 0 of the input must appear as row h-1 of the output, and so on.
void Test_BMPPixelContentFlipped(Test *t) {

	Test_setModule(t, "BMP pixel content: flip path, rows reversed correctly");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;
		BMPInfo readInfo     = { 0 };
		U64 dataOffset       = 0;

		const U32 w = 4, h = 4;
		const U64 pixelStride = 4;
		const U64 stride      = ((U64)w * pixelStride + 3) & ~3;

		BMPInfo writeInfo = {
			.w               = w,
			.h               = h,
			.isFlipped       = true,		//output: bottom-up
			.textureFormatId = ETextureFormatId_BGRA8,
			.discardAlpha    = false
		};

		//inputIsFlipped = false (top-down) != isFlipped = true -> flip path triggered
		if (!makePixelStream(t, w, h, false, &dataSr, &type)) {
			Test_assert(t, "make pixel stream flip", false);
			goto doneFlipContent;
		}

		if (!bmpRoundTripHeader(t, &writeInfo, dataSr, false, &archiveSr, &readInfo, &dataOffset, &type)) {
			Test_assert(t, "round-trip flip content", false);
			goto doneFlipContent;
		}

		OxStream *s = RefPtr_data(archiveSr, OxStream);
		Bool allMatch = true;

		for (U32 y = 0; y < h && allMatch; ++y)
			for (U32 x = 0; x < w && allMatch; ++x)
				//Input row 0 (top) maps to output row h-1 (bottom), etc.
				if (!checkPixel(s, dataOffset, x, y, stride, pixelStride, (h - 1) - y, t->alloc, &t->err))
					allMatch = false;

		Test_assert(t, "all pixels match (flipped)", allMatch);

	doneFlipContent:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
	}
}

//Verify that BMP_write with a null streamRef produces the same size as an actual write.
void Test_BMPWriteSizeConsistency(Test *t) {

	Test_setModule(t, "BMP write: null-stream size matches real write");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;

		BMPInfo writeInfo = {
			.w               = 4,
			.h               = 4,
			.isFlipped       = true,
			.textureFormatId = ETextureFormatId_BGRA8,
			.discardAlpha    = false
		};

		if (!makePixelStream(t, 4, 4, false, &dataSr, &type)) {
			Test_assert(t, "make pixel stream size", false);
			goto doneSizeConsistency;
		}

		//Null-stream pass: get predicted size
		U64 predictedSize = 0;
		Test_assert(t, "null write succeeds",
			BMP_write(NULL, &predictedSize, &writeInfo, t->alloc, dataSr, 0, true, &t->err)
		);

		//Real write pass
		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &archiveSr, &t->err)) {
			Test_assert(t, "create archive size", false);
			goto doneSizeConsistency;
		}

		U64 realSize = 0;
		Test_assert(t, "real write succeeds",
			BMP_write(archiveSr, &realSize, &writeInfo, t->alloc, dataSr, 0, true, &t->err)
		);

		Test_assert(t, "null size == real size", predictedSize == realSize);

	doneSizeConsistency:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
	}
}

//Same check for BGR8 (discardAlpha = true, different stride calculation).
void Test_BMPWriteSizeConsistencyBGR8(Test *t) {

	Test_setModule(t, "BMP write: null-stream size matches real write (BGR8)");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;

		BMPInfo writeInfo = {
			.w               = 3,
			.h               = 3,
			.isFlipped       = true,
			.textureFormatId = ETextureFormatId_BGRA8,
			.discardAlpha    = true
		};

		if (!makePixelStream(t, 3, 3, true, &dataSr, &type)) {
			Test_assert(t, "make BGR8 size stream", false);
			goto doneBGR8Size;
		}

		U64 predictedSize = 0;
		Test_assert(t, "BGR8 null write succeeds",
			BMP_write(NULL, &predictedSize, &writeInfo, t->alloc, dataSr, 0, true, &t->err)
		);

		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &archiveSr, &t->err)) {
			Test_assert(t, "create BGR8 archive size", false);
			goto doneBGR8Size;
		}

		U64 realSize = 0;
		Test_assert(t, "BGR8 real write succeeds",
			BMP_write(archiveSr, &realSize, &writeInfo, t->alloc, dataSr, 0, true, &t->err)
		);

		Test_assert(t, "BGR8 null size == real size", predictedSize == realSize);

	doneBGR8Size:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
	}
}
