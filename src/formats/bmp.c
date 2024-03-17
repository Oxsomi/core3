/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "formats/bmp.h"
#include "formats/texture.h"
#include "types/buffer.h"
#include "types/allocator.h"
#include "types/error.h"

#pragma pack(push, 1)

	typedef struct BMPHeader {
		U16 fileType;
		U32 fileSize;
		U32 reserved;
		U32 offsetData;
	} BMPHeader;

	typedef struct BMPInfoHeader {
		U32 headerSize;
		I32 width, height;
		U16 planes, bitCount;
		U32 compression, compressedSize;
		I32 xPixPerM, yPixPerM;
		U32 colorsUsed, colorsImportant;
	} BMPInfoHeader;

#pragma pack(pop)

const U16 BMP_MAGIC = 0x4D42;

const U32 reqHeadersSize = (U32) (sizeof(BMPHeader) +  sizeof(BMPInfoHeader));

Error BMP_write(Buffer buf, BMPInfo info, Allocator allocator, Buffer *result) {

	if(!result)
		return Error_nullPointer(5, "BMP_write()::result is required");

	if(result->ptr)
		return Error_invalidParameter(5, 0, "BMP_write()::result isn't empty, indicating possible memleak");

	if(!info.w || !info.h)
		return Error_invalidParameter(!info.w ? 1 : 2, 0, "BMP_write()::w and h are required");

	if(info.xPixPerM < 0 || info.yPixPerM < 0)
		return Error_invalidParameter(info.xPixPerM < 0 ? 5 : 6, 0, "BMP_write()::xPixPerM and yPixPerM have to be >0");

	if((info.w >> 31) || (info.h >> 31))
		return Error_invalidParameter((info.w >> 31) ? 1 : 2, 0, "BMP_write()::w and h can't exceed I32_MAX");

	if(info.textureFormatId != ETextureFormatId_BGRA8)
		return Error_invalidParameter(1, 3, "BMP_write()::textureFormatId is only supported for BGRA8 currently");

	U64 bufLen = Buffer_length(buf);

	U64 pixelStride = info.discardAlpha ? 3 : 4;
	U64 stride = (info.w * pixelStride + 3) &~ 3;		//Every line needs to be 4-byte aligned

	if(info.h * stride + reqHeadersSize > (U64)I32_MAX || bufLen != (U64)info.w * info.h * 4)
		return Error_invalidParameter(0, 0, "BMP_write() BMP has an image limit of 2GiB");

	BMPHeader header = (BMPHeader) {
		.fileType = BMP_MAGIC,
		.fileSize = (U32)(info.h * stride + reqHeadersSize),
		.offsetData = reqHeadersSize
	};

	BMPInfoHeader infoHeader = (BMPInfoHeader) {
		.headerSize = sizeof(BMPInfoHeader),
		.width = (I32) info.w,
		.height = (I32) info.h * (info.isFlipped ? 1 : -1),
		.planes = 1,
		.bitCount = info.discardAlpha ? 24 : 32,
		.xPixPerM = info.xPixPerM,
		.yPixPerM = info.yPixPerM
	};

	Buffer file = Buffer_createNull();

	Error err = Buffer_createUninitializedBytes(reqHeadersSize + info.h * stride, allocator, &file);

	if(err.genericError)
		return err;

	Buffer fileAppend = Buffer_createRefFromBuffer(file, false);

	gotoIfError(clean, Buffer_append(&fileAppend, &header, sizeof(header)));
	gotoIfError(clean, Buffer_append(&fileAppend, &infoHeader, sizeof(infoHeader)));

	if (pixelStride == 3) {		//Copy without alpha, since buffer lengths aren't the same

		//Unfortunately we have to copy per row, since image can be flipped or contain padding

		for (
			U64 j = info.isFlipped ? info.h - 1 : 0, k = 0;
			info.isFlipped ? j != U64_MAX : j < info.h;
			info.isFlipped ? --j : ++j, ++k
		) {

			//Write two pixels at a time through a U64,
			//We have to shift out the alpha though.

			U64 srcOff = 0, dstOff = 0;

			for (
				;
				srcOff + sizeof(U64) <= 4 * info.w && dstOff + sizeof(U64) <= stride;
				srcOff += sizeof(U64), dstOff += 3 * 2
			) {

				U64 oldPix = *(const U64*)(buf.ptr + 4 * info.w * j + srcOff);
				oldPix = (oldPix & 0xFFFFFF) | (oldPix >> 32 << 24);					//Yeet alpha

				*(U64*)(fileAppend.ptr + stride * k + dstOff) = oldPix;
			}

			//If we're left with anything, we have to do slow copies
			//We ignore the padding in the stride because it won't be read anyways

			for(U64 i = dstOff; i < info.w * pixelStride; ++i)
				((U8*)fileAppend.ptr)[i + stride * k] = buf.ptr[((i / 3) << 2) + (i % 3) + 4 * info.w * j];
		}
	}

	//Buffer lengths are the same, we can efficiently copy

	else if(!info.isFlipped)
		gotoIfError(clean, Buffer_appendBuffer(&fileAppend, buf))

	else for (U64 j = (U64)info.h - 1, k = 0; j != U64_MAX; --j, ++k)
		Buffer_copy(
			Buffer_createRef((U8*)fileAppend.ptr + stride * k, stride),
			Buffer_createRefConst(buf.ptr + stride * j, stride)
		);

	*result = file;
	return Error_none();

clean:
	Buffer_free(&file, allocator);
	return err;
}

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
	gotoIfError(clean, Buffer_consume(&buf, &header, sizeof(header)));

	if(
		header.fileType != BMP_MAGIC ||
		header.offsetData < reqHeadersSize ||
		header.fileSize != ogLength ||
		header.reserved
	)
		gotoIfError(clean, Error_invalidParameter(0, 0, "BMP_read()::buf didn't contain valid header"));

	U64 len = header.fileSize;

	if(header.offsetData >= len)
		gotoIfError(clean, Error_invalidParameter(0, 0, "BMP_read()::buf was out of bounds"));

	len -= header.offsetData;

	//Validate info header

	BMPInfoHeader bmpInfo;
	gotoIfError(clean, Buffer_consume(&buf, &bmpInfo, sizeof(bmpInfo)));

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
		gotoIfError(clean, Error_invalidParameter(0, 0, "BMP_read()::buf didn't contain valid header"));

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
		));

	info->w = (U32) bmpInfo.width;
	info->h = (U32) bmpInfo.height;

	U64 pixelStride = bmpInfo.bitCount >> 3;
	U64 stride = (info->w * pixelStride + 3) &~ 3;		//Every line needs to be 4-byte aligned

	if(len != info->h * stride)
		gotoIfError(clean, Error_invalidParameter(0, 0, "BMP_read() BMP has an image limit of 2GiB"));

	//If it's not flipped, we don't need to do anything

	info->textureFormatId = ETextureFormatId_BGRA8;

	const U8 *dataStart = start + header.offsetData;

	if (!info->isFlipped && pixelStride == 4) {				//We can only make a raw reference if bits aren't modified
		*result = Buffer_createRefConst(dataStart, len);
		return Error_none();
	}

	//Otherwise we need to flip it manually and allocate a new buffer

	gotoIfError(clean, Buffer_createOneBits((U64)info->w * info->h * 32, allocator, result));

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

			U64 dstOff = 0, srcOff = 0;

			for (
				;
				dstOff + sizeof(U64) <= 4 * info->w && srcOff + sizeof(U64) <= stride;
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
