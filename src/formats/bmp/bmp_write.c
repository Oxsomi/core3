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

//formats/bmp/bmp_write.c

#include "formats/bmp/bmp_file.h"
#include "formats/bmp/bmp_headers.h"
#include "types/container/texture_format.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/container/types.h"
#include "types/container/stream.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/constants.h"

Bool BMP_write(
	StreamRef *streamRef,
	U64 *off,
	const BMPInfo *info,
	const Allocator *alloc,
	StreamRef *inputStreamRef,
	U64 inputOffset,
	Bool inputIsFlipped,
	Error *e_rr
) {

	Bool s_uccess = true;
	StreamCursor cursor = (StreamCursor) { 0 };
	StreamCursor cursorInput = (StreamCursor) { 0 };

	if(!off || !info)
		retError(clean, Error_nullPointer(!off ? 1 : 2, "BMP_write()::off and info are required"));

	if (!info->w || !info->h)
		retError(clean, Error_invalidParameter(2, 0, "BMP_write()::info->w and h are required"));

	if (info->xPixPerM < 0 || info->yPixPerM < 0)
		retError(clean, Error_invalidParameter(
			info->xPixPerM < 0 ? 5 : 6, 0, "BMP_write()::xPixPerM and yPixPerM have to be >0"
		));

	if ((info->w >> 31) || (info->h >> 31))
		retError(clean, Error_invalidParameter(
			(info->w >> 31) ? 1 : 2, 0, "BMP_write()::w and h can't exceed I32_MAX"
		));

	if (info->textureFormatId != ETextureFormatId_BGRA8)
		retError(clean, Error_invalidParameter(
			1, 3, "BMP_write()::textureFormatId is only supported for BGRA8 currently"
		));

	const U64 pixelStride = info->discardAlpha ? 3 : 4;
	const U64 stride = ((U64)info->w * pixelStride + 3) & ~3;        //Every line needs to be 4-byte aligned

	U64 expectedLen = stride * info->h;

	if (!streamRef) {
		*off += expectedLen + sizeof(BMPHeadersCombined);
		goto clean;
	}

	gotoIfError3(clean, StreamCursor_create(streamRef, 0, true, alloc, &cursor, e_rr));
	gotoIfError3(clean, StreamCursor_create(inputStreamRef, 0, false, alloc, &cursorInput, e_rr));

	OxStream *stream = RefPtr_data(streamRef, OxStream);
	OxStream *inputStream = RefPtr_data(inputStreamRef, OxStream);

	if(inputOffset + expectedLen > inputStream->size)
		retError(clean, Error_outOfBounds(1, inputOffset + expectedLen, inputStream->size, "BMP_write()::off out of bounds"));

	if (stream->reserve)
		gotoIfError3(clean, stream->reserve(stream, *off + sizeof(BMPHeadersCombined) + expectedLen, alloc, e_rr));

	if(expectedLen + sizeof(BMPHeadersCombined) > (U64)I32_MAX)
		retError(clean, Error_invalidParameter(0, 0, "BMP_write() BMP has an image limit of 2GiB"));

	const BMPHeadersCombined header = (BMPHeadersCombined) {
		.header = (BMPHeader) {
			.fileType = BMP_MAGIC,
			.fileSize = (U32)(info->h * stride + sizeof(BMPHeadersCombined)),
			.offsetData = sizeof(BMPHeadersCombined)
		},
		.info = (BMPInfoHeader) {
			.headerSize = sizeof(BMPInfoHeader),
			.width = (I32) info->w,
			.height = (I32) info->h * (info->isFlipped ? 1 : -1),
			.planes = 1,
			.bitCount = info->discardAlpha ? 24 : 32,
			.compression = 3,
			.compressedSize = (U32) expectedLen,
			.xPixPerM = info->xPixPerM,
			.yPixPerM = info->yPixPerM
		}
	};

	gotoIfError3(clean, StreamCursor_write(
		&cursor, Buffer_createRefConst(&header, sizeof(header)), 0, *off, 0, false, alloc, e_rr
	));

	*off += sizeof(header);

	//Flip hasn't changed, so we can go into the fast path
	if (inputIsFlipped == info->isFlipped) {
		gotoIfError3(clean, StreamCursor_copyStream(&cursor, &cursorInput, inputOffset, *off, expectedLen, alloc, e_rr));
	}

	//We need to flip the image's y
	else {
		for (U64 j = (U64)info->h - 1, k = 0; j != U64_MAX; --j, ++k)
			gotoIfError3(clean, StreamCursor_copyStream(
				&cursor,
				&cursorInput,
				inputOffset + stride * j,
				*off + stride * k,
				stride,
				alloc,
				e_rr
			));
	}

	*off += expectedLen;

clean:
	StreamCursor_close(&cursor, alloc);
	StreamCursor_close(&cursorInput, alloc);
	return s_uccess;
}
