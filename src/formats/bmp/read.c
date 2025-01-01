/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "formats/bmp/bmp.h"
#include "types/container/texture_format.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

#ifndef DISALLOW_BMP_OXC3_PLATFORMS

	#include "platforms/platform.h"

	Error BMP_readx(Buffer buf, BMPInfo *info, Buffer *result) {
		return BMP_read(buf, info, Platform_instance->alloc, result);
	}

#endif

Error BMP_read(Buffer buf, BMPInfo *info, Allocator allocator, Buffer *result) {

	if(!result || !info)
		return Error_nullPointer(1, "BMP_read()::result and info are required");

	if(result->ptr)
		return Error_invalidParameter(1, 0, "BMP_read()::result isn't empty, indicating possible memleak");

	buf = Buffer_createRefFromBuffer(buf, true);

	const U8 *start = buf.ptr;
	U64 ogLength = Buffer_length(buf);

	Error err = Error_none();
	BMPHeader header;
	gotoIfError(clean, Buffer_consume(&buf, &header, sizeof(header)))

	if(
		header.fileType != BMP_MAGIC ||
		header.offsetData < BMP_reqHeadersSize ||
		header.fileSize != ogLength ||
		header.reserved
	)
		gotoIfError(clean, Error_invalidParameter(0, 0, "BMP_read()::buf didn't contain valid header"))

	U64 len = header.fileSize;

	if(header.offsetData >= len)
		gotoIfError(clean, Error_invalidParameter(0, 0, "BMP_read()::buf was out of bounds"))

	len -= header.offsetData;

	//Validate info header

	BMPInfoHeader bmpInfo;
	gotoIfError(clean, Buffer_consume(&buf, &bmpInfo, sizeof(bmpInfo)))

	if(
		bmpInfo.planes != 1 ||
		bmpInfo.width <= 0 ||
		!bmpInfo.height ||
		bmpInfo.headerSize != sizeof(BMPInfoHeader) ||
		bmpInfo.colorsUsed ||
		bmpInfo.colorsImportant ||
		bmpInfo.xPixPerM < 0 ||
		bmpInfo.yPixPerM < 0
	)
		gotoIfError(clean, Error_invalidParameter(0, 0, "BMP_read()::buf didn't contain valid header"))

	info->isFlipped = bmpInfo.height > 0;
	info->xPixPerM = bmpInfo.xPixPerM;
	info->yPixPerM = bmpInfo.yPixPerM;
	info->discardAlpha = bmpInfo.bitCount == 24;

	if(bmpInfo.height < 0)
		bmpInfo.height *= -1;

	if(
		(bmpInfo.bitCount != 32 && bmpInfo.bitCount != 24) ||
		(bmpInfo.compression != 3 && bmpInfo.compression) ||
		(bmpInfo.compressedSize != len && bmpInfo.compressedSize)
	)
		gotoIfError(clean, Error_invalidParameter(
			0, 0, "BMP_read()::buf contained unsupported format (only RGBA8/BGRA8 supported)"
		))

	info->w = (U32) bmpInfo.width;
	info->h = (U32) bmpInfo.height;

	U64 pixelStride = bmpInfo.bitCount >> 3;
	U64 stride = (info->w * pixelStride + 3) &~ 3;		//Every line needs to be 4-byte aligned

	if(len != info->h * stride)
		gotoIfError(clean, Error_invalidParameter(0, 0, "BMP_read() BMP has an image limit of 2GiB"))

	//If it's not flipped, we don't need to do anything

	info->textureFormatId = ETextureFormatId_BGRA8;

	const U8 *dataStart = start + header.offsetData;

	if (!info->isFlipped && pixelStride == 4) {				//We can only make a raw reference if bits aren't modified
		*result = Buffer_createRefConst(dataStart, len);
		return Error_none();
	}

	//Otherwise we need to flip it manually and allocate a new buffer

	gotoIfError(clean, Buffer_createOneBits((U64)info->w * info->h * 32, allocator, result))

	for (
		U64 j = info->isFlipped ? info->h - 1 : 0, k = 0;
		info->isFlipped ? j != U64_MAX : j < info->h;
		info->isFlipped ? --j : ++j, ++k
	) {

		if(pixelStride == 4)		//Simple copy
			Buffer_copy(
				Buffer_createRef((U8*)result->ptr + stride * k, stride),
				Buffer_createRefConst(dataStart + stride * j, stride)
			);

		else {						//Unfortunately we have to copy per row

			//Write two pixels at a time through a U64,
			//We have to shift out the alpha though.

			U64 srcOff = 0;

			for (
				U64 dstOff = 0;
				dstOff + sizeof(U64) <= (U64)4 * info->w && srcOff + sizeof(U64) <= stride;
				dstOff += sizeof(U64), srcOff += 3 * 2
			) {

				U64 oldPix = *(const U64*)(dataStart + stride * j + srcOff);
				oldPix = (oldPix & 0xFFFFFF) | (oldPix >> 24 << 32) | 0xFF000000FF000000;		//Expand alpha

				*(U64*)(result->ptr + (U64)4 * info->w * k + dstOff) = oldPix;
			}

			//If we're left with anything, we have to do slow copies
			//We ignore the padding in the stride because it won't be read anyways

			for(U64 i = srcOff; i < info->w * pixelStride; ++i)
				((U8*)result->ptr)[(U64)4 * info->w * k + ((i / 3) << 2) + (i % 3)] = dataStart[stride * j + i];
		}
	}

	return Error_none();

clean:
	Buffer_free(result, allocator);
	return err;
}
