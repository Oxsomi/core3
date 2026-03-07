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

#include "formats/bmp/bmp_file.h"
#include "formats/bmp/bmp_headers.h"
#include "types/container/texture_format.h"
#include "types/container/buffer.h"
#include "types/container/stream.h"
#include "types/container/ref_ptr.h"
#include "types/container/types.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/constants.h"

Bool BMP_read(StreamRef *streamRef, U64 *off, U64 *dataOffset, BMPInfo *info, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!streamRef || !off || !dataOffset || !info)
		retError(clean, Error_nullPointer(
			(!streamRef ? 0 : (!off ? 1 : (!dataOffset ? 2 : 3))),
			"BMP_read()::stream, off ,dataOffset and info are required")
		);

	if(streamRef->refPtrType->typeId != (ETypeId) EContainerTypeId_Stream)
		retError(clean, Error_invalidParameter(0, 0, "BMP_read()::stream needs to be the right type"));

	Stream *stream = RefPtr_data(streamRef, Stream);

	if(!stream->read)
		retError(clean, Error_invalidParameter(1, 0, "BMP_read()::stream needs to be readable"));

	BMPHeadersCombined bmp;
	gotoIfError3(clean, stream->read(stream, *off, sizeof(bmp), Buffer_createRef(&bmp, sizeof(bmp)), alloc, e_rr));

	if(
		bmp.info.planes != 1 ||
		bmp.info.width <= 0 ||
		!bmp.info.height ||
		bmp.info.headerSize != sizeof(BMPInfoHeader) ||
		bmp.info.colorsUsed ||
		bmp.info.colorsImportant ||
		bmp.info.xPixPerM < 0 ||
		bmp.info.yPixPerM < 0
	)
		retError(clean, Error_invalidParameter(0, 0, "BMP_read()::stream didn't contain valid header"));

	if(bmp.info.height == I32_MIN)
		retError(clean, Error_invalidParameter(0, 0, "BMP_read()::stream had an invalid height value"));

	Bool isFlipped = bmp.info.height > 0;

	if(bmp.info.height < 0)
		bmp.info.height *= -1;

	if((bmp.info.bitCount != 32 && bmp.info.bitCount != 24) || (bmp.info.compression != 3 && bmp.info.compression))
		retError(clean, Error_invalidParameter(
			0, 0, "BMP_read()::stream contained unsupported format (only RGBA8/BGRA8 supported)"
		));

	U64 pixelStride = bmp.info.bitCount >> 3;
	U64 stride = ((U64)bmp.info.width * pixelStride + 3) & ~3;		//Every line needs to be 4-byte aligned

	U64 expectedLength = stride * bmp.info.height;

	if (
		bmp.header.fileType != BMP_MAGIC ||
		bmp.header.offsetData < sizeof(BMPHeadersCombined) ||
		bmp.header.fileSize != sizeof(bmp) + expectedLength ||
		bmp.header.reserved
	)
		retError(clean, Error_invalidParameter(0, 0, "BMP_read()::stream didn't contain valid header"));

	if(bmp.info.compressedSize != expectedLength && bmp.info.compressedSize)
		retError(clean, Error_invalidParameter(
			0, 0, "BMP_read()::buf had unexpected 'compressedSize'"
		));

	if (bmp.header.offsetData + expectedLength > bmp.header.fileSize)
		retError(clean, Error_invalidParameter(0, 0, "BMP_read()::stream was out of bounds"));

	if(*off + bmp.header.fileSize > stream->size)
		retError(clean, Error_invalidParameter(0, 0, "BMP_read()::*off + fileSize out of bounds for stream"));

	info->isFlipped = isFlipped;
	info->xPixPerM = bmp.info.xPixPerM;
	info->yPixPerM = bmp.info.yPixPerM;
	info->discardAlpha = bmp.info.bitCount == 24;
	info->w = (U32) bmp.info.width;
	info->h = (U32) bmp.info.height;
	info->textureFormatId = ETextureFormatId_BGRA8;

	*dataOffset = *off + bmp.header.offsetData;
	*off += bmp.header.fileSize;

clean:
	return s_uccess;
}
