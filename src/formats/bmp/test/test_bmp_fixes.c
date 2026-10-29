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

//formats/bmp/test/test_bmp_fixes.c

#include "test_bmp_shared.h"
#include "formats/bmp/bmp_file.h"
#include "formats/bmp/bmp_headers.h"
#include "types/container/memory_stream.h"
#include "types/container/texture_format.h"
#include "types/container/buffer.h"
#include "types/container/test/failing_stream.h"
#include "types/container/test/basic_alloc.h"
#include "types/base/mathi.h"

//The stream type has to outlive every stream made from it
static RefPtrType streamTypeStorage;

static const RefPtrType *streamType(Test *t) {
	streamTypeStorage = MemoryStream_makeType(t->alloc);
	return &streamTypeStorage;
}

//Failing each write in turn: a lost write must not be reported as a success, nor move the offset.
//Small images are written by the one flush that closing the cursor does, so that flush is checked as well.

static Bool writeImage(Test *t, StreamRef *out, Bool discardAlpha, U32 size, U64 *off, Error *err) {

	StreamRef *input = NULL;
	Bool ok = false;

	const U64 stride = ((U64) size * (discardAlpha ? 3 : 4) + 3) & ~(U64) 3;
	const U64 inputLen = stride * size;

	if(!MemoryStream_create(inputLen, EMemoryStreamFlags_IsWritable, streamType(t), &input, &t->err))
		return false;

	BMPInfo info = (BMPInfo) {
		.w = size, .h = size, .isFlipped = true, .discardAlpha = discardAlpha, .textureFormatId = ETextureFormatId_BGRA8
	};

	*off = 0;
	ok = BMP_write(out, off, &info, t->alloc, input, 0, true, err);
	RefPtr_dec(&input);
	return ok;
}

void Test_BMPWriteFailedWrites(Test *t) {

	Test_setModule(t, "BMP_write: failed writes are reported");

	const U32 sizes[] = { 1, 3, 600 };

	for (U64 sz = 0; sz < sizeof(sizes) / sizeof(sizes[0]); ++sz)
		for (U32 discard = 0; discard < 2; ++discard) {

			StreamRef *out = NULL;
			U64 off = 0;

			Test_assert(t, "create", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, streamType(t), &out, &t->err));
			FailingStream_arm(out, 0);
			Test_assert(t, "write ok", writeImage(t, out, discard, sizes[sz], &off, &t->err));

			const U64 writes = FailingStream_writes();

			Test_assert(t, "wrote something", writes > 0);
			RefPtr_dec(&out);

			for (U64 i = 1; i <= writes; ++i) {

				Test_assert(t, "create", MemoryStream_create(
					0, EMemoryStreamFlags_WriteResize, streamType(t), &out, &t->err
				));

				FailingStream_arm(out, i);

				Error err = (Error) { 0 };

				Test_assert(t, "failed write is reported", !writeImage(t, out, discard, sizes[sz], &off, &err));
				Test_assert(t, "the write was reached", FailingStream_failed() && err.genericError);
				Test_assert(t, "a failed write leaves the offset", !off);
				RefPtr_dec(&out);
			}
		}
}

//The 2 GiB limit applies before anything is reserved, and the input bounds can't wrap

void Test_BMPWriteLimits(Test *t) {

	Test_setModule(t, "BMP_write: size limit and input bounds");

	StreamRef *out = NULL, *input = NULL;

	if (
		!Test_assert(t, "create out", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, streamType(t), &out, &t->err)) ||
		!Test_assert(t, "create in", MemoryStream_create(64, EMemoryStreamFlags_IsWritable, streamType(t), &input, &t->err))
	)
		goto clean;

	OxStream *outStream = RefPtr_data(out, OxStream);

	//Over 2 GiB: refused before the output is reserved, so the output stays empty (no gigabyte allocation)

	BMPInfo big = (BMPInfo) { .w = 30000, .h = 30000, .isFlipped = true, .textureFormatId = ETextureFormatId_BGRA8 };
	U64 off = 0;

	Test_assert(t, "over 2GiB refused", !BMP_write(out, &off, &big, t->alloc, input, 0, true, NULL));
	Test_assert(t, "over 2GiB reserved nothing", outStream->size == 0 && off == 0);

	//The size query refuses it too, instead of reporting a length no BMP can have

	off = 0;
	Test_assert(t, "over 2GiB length query refused", !BMP_write(NULL, &off, &big, t->alloc, NULL, 0, true, NULL));
	Test_assert(t, "over 2GiB length query unchanged", off == 0);

	//inputOffset + length wraps to a small number

	BMPInfo small = (BMPInfo) { .w = 2, .h = 2, .isFlipped = true, .textureFormatId = ETextureFormatId_BGRA8 };

	off = 0;
	Test_assert(t, "input offset wrap refused", !BMP_write(out, &off, &small, t->alloc, input, U64_MAX - 3, true, NULL));
	Test_assert(t, "input offset past end refused", !BMP_write(out, &off, &small, t->alloc, input, 64, true, NULL));
	Test_assert(t, "input offset too late refused", !BMP_write(out, &off, &small, t->alloc, input, 56, true, NULL));
	Test_assert(t, "input offset fits", BMP_write(out, &off, &small, t->alloc, input, 48, true, &t->err));

clean:
	RefPtr_dec(&input);
	RefPtr_dec(&out);
}

//BI_BITFIELDS: three masks sit between the header and the pixels. Only BGRA masks are accepted.

static Bool readBitfields(Test *t, U32 bitCount, const U32 *masks, U32 fileExtra, Error *err, Bool *isFlipped) {

	const U32 w = 2, h = 2;
	const U64 pixelData = (U64) w * 4 * h;
	const U64 maskBytes = masks ? 12 : 0;

	U8 file[sizeof(BMPHeadersCombined) + 12 + 16 + 8] = { 0 };

	BMPHeadersCombined hdr = {
		.header = {
			.fileType = BMP_MAGIC,
			.fileSize = (U32) (sizeof(BMPHeadersCombined) + maskBytes + pixelData) + fileExtra,
			.offsetData = (U32) (sizeof(BMPHeadersCombined) + maskBytes)
		},
		.info = {
			.headerSize = sizeof(BMPInfoHeader), .width = (I32) w, .height = (I32) h, .planes = 1,
			.bitCount = (U16) bitCount, .compression = 3
		}
	};

	Buffer_memcpy(Buffer_createRef(file, sizeof(hdr)), Buffer_createRefConst(&hdr, sizeof(hdr)));

	if (masks)
		Buffer_memcpy(Buffer_createRef(file + sizeof(hdr), 12), Buffer_createRefConst(masks, 12));

	StreamRef *sr = NULL;

	if(!MemoryStream_create(sizeof(file), EMemoryStreamFlags_IsWritable, streamType(t), &sr, &t->err))
		return false;

	OxStream *s = RefPtr_data(sr, OxStream);

	s->write(s, 0, sizeof(file), Buffer_createRefConst(file, sizeof(file)), t->alloc, &t->err);

	BMPInfo info = (BMPInfo) { 0 };
	U64 off = 0, dataOffset = 0;
	Bool ok = BMP_read(sr, &off, &dataOffset, &info, t->alloc, err);

	if(ok) {

		if(isFlipped)
			*isFlipped = info.isFlipped;

		ok = dataOffset == sizeof(BMPHeadersCombined) + maskBytes && off == hdr.header.fileSize;
	}

	RefPtr_dec(&sr);
	return ok;
}

void Test_BMPReadBitfields(Test *t) {

	Test_setModule(t, "BMP read: BITFIELDS needs BGRA masks");

	const U32 bgra[3] = { 0x00FF0000, 0x0000FF00, 0x000000FF };
	const U32 rgba[3] = { 0x000000FF, 0x0000FF00, 0x00FF0000 };
	const U32 rgb565[3] = { 0xF800, 0x07E0, 0x001F };

	Bool flipped = false;
	Error err = (Error) { 0 };

	Test_assert(t, "BGRA masks accepted", readBitfields(t, 32, bgra, 0, &t->err, &flipped) && flipped);
	Test_assert(t, "RGBA masks refused", !readBitfields(t, 32, rgba, 0, &err, NULL) && err.genericError);
	Test_assert(t, "565 masks refused", !readBitfields(t, 32, rgb565, 0, NULL, NULL));

	//Declared BITFIELDS with no masks in the file used to be read as plain BGRA

	Test_assert(t, "no masks refused", !readBitfields(t, 32, NULL, 0, NULL, NULL));

	//Masks on a 24 bit image mean nothing

	Test_assert(t, "24 bit BITFIELDS refused", !readBitfields(t, 24, bgra, 0, NULL, NULL));

	//A declared file can include bytes after the pixel array.

	Test_assert(t, "file size past the data accepted", readBitfields(t, 32, bgra, 4, NULL, NULL));
}

void Test_BMPReadExtendedHeaders(Test *t) {

	Test_setModule(t, "BMP read: extended headers and pixel offsets");

	const U32 headerSizes[] = { 40, 108, 124 };

	for(U32 h = 0; h < 3; ++h)
		for(U32 bitfields = 0; bitfields < 2; ++bitfields)
			for(U32 noFileSize = 0; noFileSize < 2; ++noFileSize) {

				U8 file[192] = { 0 };
				const U32 prefix = 7;
				const U32 data = 14 + headerSizes[h] + (bitfields && !h ? 12 : 0) + 8;
				const U32 length = data + 4;
				BMPHeadersCombined bmp = {
					.header = { .fileType = BMP_MAGIC, .fileSize = noFileSize ? 0 : length, .offsetData = data },
					.info = {
						.headerSize = headerSizes[h], .width = 1, .height = -1, .planes = 1,
						.bitCount = 32, .compression = bitfields ? 3 : 0
					}
				};

				Buffer_memcpy(Buffer_createRef(file + prefix, sizeof(bmp)), Buffer_createRefConst(&bmp, sizeof(bmp)));

				if(bitfields) {

					const U32 masks[] = { 0xFF0000, 0xFF00, 0xFF, 0xFF000000 };
					const U64 maskBytes = h ? sizeof(masks) : 12;

					Buffer_memcpy(Buffer_createRef(file + prefix + 54, maskBytes), Buffer_createRefConst(masks, maskBytes));
				}

				file[prefix + data] = 37;

				StreamRef *sr = NULL;

				if (!Test_assert(t, "create fixture", MemoryStream_create(
					prefix + length + 3, EMemoryStreamFlags_IsWritable, streamType(t), &sr, &t->err
				)))
					continue;

				OxStream *s = RefPtr_data(sr, OxStream);

				Test_assert(t, "write fixture", s->write(
					s, 0, s->size, Buffer_createRefConst(file, s->size), t->alloc, &t->err
				));

				BMPInfo info = (BMPInfo) { 0 };
				U64 off = prefix, dataOffset = 0;
				Bool ok = BMP_read(sr, &off, &dataOffset, &info, t->alloc, NULL);

				Test_assert(t, "header accepted", ok);

				if(ok) {
					Test_assert(t, "correct dimensions and orientation", info.w == 1 && info.h == 1 && !info.isFlipped);
					Test_assert(t, "correct offsets", dataOffset == prefix + data && off == prefix + length);
					Test_assert(t, "pixel preserved", RefPtr_data(sr, MemoryStream)->data.ptr[dataOffset] == 37);
				}

				RefPtr_dec(&sr);
			}
}
