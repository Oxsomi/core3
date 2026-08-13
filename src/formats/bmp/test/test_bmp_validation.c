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

//formats/bmp/test/test_bmp_validation.c

#include "test_bmp_shared.h"
#include "formats/bmp/bmp_file.h"
#include "formats/bmp/bmp_headers.h"
#include "types/container/memory_stream.h"
#include "types/container/texture_format.h"
#include "types/base/mathi.h"

#include <stddef.h>

//Helper: build a minimal valid BMPHeadersCombined for a 2x2 BGRA8 image,
// write it into a writable MemoryStream, and return the stream.
//The caller can then corrupt specific fields before passing to BMP_read.
//Caller owns *sr (RefPtr_dec).
//Returns false on allocation failure.
static Bool makeRawBMPStream(
	Test *t,
	StreamRef **sr,
	BMPHeadersCombined *outHeader,
	const RefPtrType *type
) {
	//2x2 BGRA8: stride = 8, pixel data = 16 bytes
	const U32 w = 2, h = 2;
	const U64 pixelStride = 4;
	const U64 stride      = ((U64)w * pixelStride + 3) & ~3;
	const U64 pixelData   = stride * h;
	const U64 totalSize   = sizeof(BMPHeadersCombined) + pixelData;

	BMPHeadersCombined hdr = {
		.header = {
			.fileType   = BMP_MAGIC,
			.fileSize   = (U32)totalSize,
			.reserved   = 0,
			.offsetData = sizeof(BMPHeadersCombined)
		},
		.info = {
			.headerSize      = sizeof(BMPInfoHeader),
			.width           = (I32)w,
			.height          = (I32)h,        //positive -> isFlipped=true
			.planes          = 1,
			.bitCount        = 32,
			.compression     = 0,
			.compressedSize  = 0,
			.xPixPerM        = 0,
			.yPixPerM        = 0,
			.colorsUsed      = 0,
			.colorsImportant = 0
		}
	};

	StreamRef *stream = NULL;

	if (!MemoryStream_create(totalSize, EMemoryStreamFlags_IsWritable, type, &stream, &t->err))
		return false;

	OxStream *s = RefPtr_data(stream, OxStream);

	if (!s->write(s, 0, sizeof(hdr), Buffer_createRefConst(&hdr, sizeof(hdr)), t->alloc, &t->err)) {
		RefPtr_dec(&stream);
		return false;
	}

	if (outHeader)
		*outHeader = hdr;

	*sr = stream;
	return true;
}

//BMP_write must fail when width or height is zero.
void Test_BMPWriteZeroDimensions(Test *t) {

	Test_setModule(t, "BMP write: zero width/height rejected");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *outSr  = NULL;
		StreamRef *dataSr = NULL;

		if (
			!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &outSr, &t->err) ||
			!MemoryStream_create(0, EMemoryStreamFlags_IsWritable,  &type, &dataSr, &t->err)
		) {
			Test_assert(t, "create streams zero dim", false);
			goto doneZeroDim;
		}

		U64 off = 0;

		//Zero width
		BMPInfo zeroW = { .w = 0, .h = 4, .isFlipped = true, .textureFormatId = ETextureFormatId_BGRA8 };
		Test_assert(t, "zero w fails", !BMP_write(outSr, &off, &zeroW, t->alloc, dataSr, 0, true, NULL));

		//Zero height
		off = 0;
		BMPInfo zeroH = { .w = 4, .h = 0, .isFlipped = true, .textureFormatId = ETextureFormatId_BGRA8 };
		Test_assert(t, "zero h fails", !BMP_write(outSr, &off, &zeroH, t->alloc, dataSr, 0, true, NULL));

	doneZeroDim:
		RefPtr_dec(&outSr);
		RefPtr_dec(&dataSr);
	}
}

//BMP_write must fail when xPixPerM or yPixPerM is negative.
void Test_BMPWriteNegativePixelsPerMetre(Test *t) {

	Test_setModule(t, "BMP write: negative pixels-per-metre rejected");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *outSr  = NULL;
		StreamRef *dataSr = NULL;

		if (
			!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &outSr, &t->err) ||
			!MemoryStream_create(16, EMemoryStreamFlags_IsWritable,  &type, &dataSr, &t->err)
		) {
			Test_assert(t, "create streams neg ppm", false);
			goto doneNegPPM;
		}

		U64 off = 0;

		BMPInfo negX = {
			.w = 2, .h = 2, .isFlipped = true, .textureFormatId = ETextureFormatId_BGRA8, .xPixPerM = -1, .yPixPerM = 0
		};

		Test_assert(t, "negative xPixPerM fails", !BMP_write(outSr, &off, &negX, t->alloc, dataSr, 0, true, NULL));

		off = 0;
		BMPInfo negY = {
			.w = 2, .h = 2, .isFlipped = true, .textureFormatId = ETextureFormatId_BGRA8, .xPixPerM = 0, .yPixPerM = -1
		};

		Test_assert(t, "negative yPixPerM fails", !BMP_write(outSr, &off, &negY, t->alloc, dataSr, 0, true, NULL));

	doneNegPPM:
		RefPtr_dec(&outSr);
		RefPtr_dec(&dataSr);
	}
}

//BMP_write must fail for any format other than BGRA8.
void Test_BMPWriteWrongFormat(Test *t) {

	Test_setModule(t, "BMP write: non-BGRA8 format rejected");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *outSr  = NULL;
		StreamRef *dataSr = NULL;

		if (
			!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &outSr, &t->err) ||
			!MemoryStream_create(16, EMemoryStreamFlags_IsWritable,  &type, &dataSr, &t->err)
		) {
			Test_assert(t, "create streams wrong fmt", false);
			goto doneWrongFmt;
		}

		U64 off = 0;
		BMPInfo info = { .w = 2, .h = 2, .isFlipped = true, .textureFormatId = ETextureFormatId_RGBA8 };
		Test_assert(t, "RGBA8 format rejected", !BMP_write(outSr, &off, &info, t->alloc, dataSr, 0, true, NULL));

	doneWrongFmt:
		RefPtr_dec(&outSr);
		RefPtr_dec(&dataSr);
	}
}

//BMP_read must fail when the magic number is wrong.
void Test_BMPReadInvalidMagic(Test *t) {

	Test_setModule(t, "BMP read: invalid magic rejected");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *sr = NULL;
		BMPHeadersCombined hdr;
		BMPInfo info     = { 0 };
		U64 dataOffset   = 0;

		if (!makeRawBMPStream(t, &sr, &hdr, &type)) {
			Test_assert(t, "make raw BMP stream magic", false);
			goto doneInvalidMagic;
		}

		//Corrupt the magic
		U16 badMagic = 0x0000;
		OxStream *s = RefPtr_data(sr, OxStream);
		s->write(s, 0, sizeof(badMagic), Buffer_createRefConst(&badMagic, sizeof(badMagic)), t->alloc, NULL);

		U64 off = 0;
		Test_assert(t, "bad magic rejected", !BMP_read(sr, &off, &dataOffset, &info, t->alloc, NULL));

	doneInvalidMagic:
		RefPtr_dec(&sr);
	}
}

//BMP_read must fail when width is zero.
void Test_BMPReadZeroWidth(Test *t) {

	Test_setModule(t, "BMP read: zero width rejected");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *sr = NULL;
		BMPHeadersCombined hdr;
		BMPInfo info   = { 0 };
		U64 dataOffset = 0;

		if (!makeRawBMPStream(t, &sr, &hdr, &type)) {
			Test_assert(t, "make raw BMP stream zero w", false);
			goto doneZeroW;
		}

		//Write width = 0 into the info header (offset of width field = 14 + 4 = 18)
		I32 zero = 0;
		OxStream *s = RefPtr_data(sr, OxStream);
		s->write(
			s,
			offsetof(BMPHeadersCombined, info.width),
			sizeof(zero),
			Buffer_createRefConst(&zero, sizeof(zero)),
			t->alloc,
			NULL
		);

		U64 off = 0;
		Test_assert(t, "zero width rejected", !BMP_read(sr, &off, &dataOffset, &info, t->alloc, NULL));

	doneZeroW:
		RefPtr_dec(&sr);
	}
}

//BMP_read must fail when height is zero.
void Test_BMPReadZeroHeight(Test *t) {

	Test_setModule(t, "BMP read: zero height rejected");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *sr = NULL;
		BMPHeadersCombined hdr;
		BMPInfo info   = { 0 };
		U64 dataOffset = 0;

		if (!makeRawBMPStream(t, &sr, &hdr, &type)) {
			Test_assert(t, "make raw BMP stream zero h", false);
			goto doneZeroH;
		}

		I32 zero = 0;
		OxStream *s = RefPtr_data(sr, OxStream);
		s->write(
			s,
			offsetof(BMPHeadersCombined, info.height),
			sizeof(zero),
			Buffer_createRefConst(&zero, sizeof(zero)),
			t->alloc,
			NULL
		);

		U64 off = 0;
		Test_assert(t, "zero height rejected", !BMP_read(sr, &off, &dataOffset, &info, t->alloc, NULL));

	doneZeroH:
		RefPtr_dec(&sr);
	}
}

//BMP_read must fail for unsupported bit depths (e.g. 8bpp).
void Test_BMPReadUnsupportedBitCount(Test *t) {

	Test_setModule(t, "BMP read: unsupported bit depth (8bpp) rejected");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *sr = NULL;
		BMPHeadersCombined hdr;
		BMPInfo info   = { 0 };
		U64 dataOffset = 0;

		if (!makeRawBMPStream(t, &sr, &hdr, &type)) {
			Test_assert(t, "make raw BMP stream bad bpp", false);
			goto doneBitCount;
		}

		U16 bpp8 = 8;
		OxStream *s = RefPtr_data(sr, OxStream);
		s->write(
			s,
			offsetof(BMPHeadersCombined, info.bitCount),
			0,
			Buffer_createRefConst(&bpp8, sizeof(bpp8)),
			t->alloc,
			NULL
		);

		U64 off = 0;
		Test_assert(t, "8bpp rejected", !BMP_read(sr, &off, &dataOffset, &info, t->alloc, NULL));

	doneBitCount:
		RefPtr_dec(&sr);
	}
}

//BMP_read must fail for unsupported compression modes (e.g. RLE8 = 1).
void Test_BMPReadUnsupportedCompression(Test *t) {

	Test_setModule(t, "BMP read: unsupported compression (RLE8) rejected");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *sr = NULL;
		BMPHeadersCombined hdr;
		BMPInfo info   = { 0 };
		U64 dataOffset = 0;

		if (!makeRawBMPStream(t, &sr, &hdr, &type)) {
			Test_assert(t, "make raw BMP stream bad comp", false);
			goto doneCompression;
		}

		U32 rle8 = 1;
		OxStream *s = RefPtr_data(sr, OxStream);
		s->write(
			s,
			offsetof(BMPHeadersCombined, info.compression),
			sizeof(rle8),
			Buffer_createRefConst(&rle8, sizeof(rle8)),
			t->alloc,
			NULL
		);

		U64 off = 0;
		Test_assert(t, "RLE8 compression rejected", !BMP_read(sr, &off, &dataOffset, &info, t->alloc, NULL));

	doneCompression:
		RefPtr_dec(&sr);
	}
}

//BMP_read must fail for RLE4 compression (value 2), not just RLE8 (value 1).
//Guards against an off-by-one in the compression check.
void Test_BMPReadUnsupportedCompressionRLE4(Test *t) {

	Test_setModule(t, "BMP read: unsupported compression (RLE4=2) rejected");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *sr = NULL;
		BMPHeadersCombined hdr;
		BMPInfo info   = { 0 };
		U64 dataOffset = 0;

		if (!makeRawBMPStream(t, &sr, &hdr, &type)) {
			Test_assert(t, "make raw BMP stream RLE4", false);
			goto doneRLE4;
		}

		U32 rle4 = 2;
		OxStream *s = RefPtr_data(sr, OxStream);
		s->write(
			s,
			offsetof(BMPHeadersCombined, info.compression),
			sizeof(rle4),
			Buffer_createRefConst(&rle4, sizeof(rle4)),
			t->alloc,
			NULL
		);

		U64 off = 0;
		Test_assert(t, "RLE4 compression rejected", !BMP_read(sr, &off, &dataOffset, &info, t->alloc, NULL));

	doneRLE4:
		RefPtr_dec(&sr);
	}
}
