/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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

	typedef struct BMPColorHeader {
		U32 redMask, greenMask, blueMask, alphaMask;
		U32 colorSpaceType;
		U32 unused[16];
	} BMPColorHeader;

#pragma pack(pop)

const U16 BMP_MAGIC = 0x4D42;
const U32 BMP_SRGB_MAGIC = 0x73524742;

const U32 reqHeadersSize = (U32) (sizeof(BMPHeader) +  sizeof(BMPInfoHeader));

Error BMP_write(Buffer buf, BMPInfo info, Allocator allocator, Buffer *result) {

	if(!result)
		return Error_nullPointer(5, "BMP_write()::result is required");

	if(result->ptr)
		return Error_invalidParameter(5, 0, "BMP_write()::result isn't empty, indicating possible memleak");

	if(!info.w || !info.h)
		return Error_invalidParameter(!info.w ? 1 : 2, 0, "BMP_write()::w and h are required");

	if((info.w >> 31) || (info.h >> 31))
		return Error_invalidParameter((info.w >> 31) ? 1 : 2, 0, "BMP_write()::w and h can't exceed I32_MAX");

	if(info.textureFormatId != ETextureFormatId_RGBA8 && info.textureFormatId != ETextureFormatId_BGRA8)
		return Error_invalidParameter(1, 3, "BMP_write()::textureFormatId is only supported for RGBA8/BGRA8 currently");

	U64 bufLen = Buffer_length(buf);

	if(bufLen + reqHeadersSize + sizeof(BMPColorHeader) > I32_MAX || bufLen != (U64)info.w * info.h * 4)
		return Error_invalidParameter(0, 0, "BMP_write() BMP has an image limit of 2GiB");

	BMPHeader header = (BMPHeader) {
		.fileType = BMP_MAGIC,
		.fileSize = (U32)bufLen + reqHeadersSize,
		.offsetData = reqHeadersSize
	};

	BMPInfoHeader infoHeader = (BMPInfoHeader) {
		.headerSize = sizeof(BMPInfoHeader),
		.width = (I32) info.w,
		.height = (I32) info.h * (info.isFlipped ? -1 : 1),
		.planes = 1,
		.bitCount = 32
	};

	BMPColorHeader colorHeader = (BMPColorHeader) {

		.redMask	= (U32) 0xFF << 16,
		.greenMask	= (U32) 0xFF << 8,
		.blueMask	= (U32) 0xFF,
		.alphaMask	= (U32) 0xFF << 24,

		.colorSpaceType = BMP_SRGB_MAGIC
	};

	Bool isBGR = info.textureFormatId == ETextureFormatId_BGRA8;

	if (isBGR) {		//Swizzle

		U32 blueMask = colorHeader.blueMask;
		colorHeader.blueMask = colorHeader.redMask;
		colorHeader.redMask = blueMask;

		header.offsetData += sizeof(BMPColorHeader);
	}

	Buffer file = Buffer_createNull();

	Error err = Buffer_createUninitializedBytes(
		reqHeadersSize + (isBGR ? sizeof(BMPColorHeader) : 0) + bufLen,
		allocator,
		&file
	);

	if(err.genericError)
		return err;

	Buffer fileAppend = Buffer_createRefFromBuffer(file, false);

	_gotoIfError(clean, Buffer_append(&fileAppend, &header, sizeof(header)));
	_gotoIfError(clean, Buffer_append(&fileAppend, &infoHeader, sizeof(infoHeader)));

	if(isBGR)
		_gotoIfError(clean, Buffer_append(&fileAppend, &colorHeader, sizeof(colorHeader)));

	U64 stride = (U64)info.w * 4;

	if(!info.isFlipped)
		_gotoIfError(clean, Buffer_appendBuffer(&fileAppend, buf))

	else for (U64 j = info.h - 1, k = 0; j != U64_MAX; --j, ++k)
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
	_gotoIfError(clean, Buffer_consume(&buf, &header, sizeof(header)));

	if(
		header.fileType != BMP_MAGIC || 
		header.offsetData < reqHeadersSize || 
		header.fileSize != ogLength ||
		header.reserved
	)
		_gotoIfError(clean, Error_invalidParameter(0, 0, "BMP_read()::buf didn't contain valid header"));

	U64 len = header.fileSize;

	if(header.offsetData >= len)
		_gotoIfError(clean, Error_invalidParameter(0, 0, "BMP_read()::buf was out of bounds"));

	len -= header.offsetData;

	//Validate info header

	BMPInfoHeader bmpInfo;
	_gotoIfError(clean, Buffer_consume(&buf, &bmpInfo, sizeof(bmpInfo)));

	if(
		bmpInfo.planes != 1 ||
		bmpInfo.width <= 0 ||
		!bmpInfo.height ||
		bmpInfo.headerSize != sizeof(BMPInfoHeader) ||
		bmpInfo.colorsUsed ||
		bmpInfo.colorsImportant
	)
		_gotoIfError(clean, Error_invalidParameter(0, 0, "BMP_read()::buf didn't contain valid header"));

	info->isFlipped = bmpInfo.height < 0;

	if(bmpInfo.height < 0)
		bmpInfo.height *= -1;

	if(
		bmpInfo.bitCount != 32 || 
		(bmpInfo.compression != 3 && bmpInfo.compression) || 
		(bmpInfo.compressedSize != len && bmpInfo.compressedSize)
	)
		_gotoIfError(clean, Error_invalidParameter(
			0, 0, "BMP_read()::buf contained unsupported format (only RGBA8/BGRA8 supported)"
		));

	info->w = (U32) bmpInfo.width;
	info->h = (U32) bmpInfo.height;

	if(len != (U64)info->w * info->h * 4)
		_gotoIfError(clean, Error_invalidParameter(0, 0, "BMP_read() BMP has an image limit of 2GiB"));

	//Validate color header if present
	
	Bool isBGR = false;

	if(header.offsetData != reqHeadersSize) {

		if(header.offsetData < reqHeadersSize + sizeof(BMPColorHeader))
			_gotoIfError(clean, Error_invalidParameter(0, 0, "BMP_read()::buf didn't contain BMPColorHeader"));

		BMPColorHeader colorHeader;
		_gotoIfError(clean, Buffer_consume(&buf, &colorHeader, sizeof(colorHeader)));

		if(
			colorHeader.colorSpaceType != BMP_SRGB_MAGIC || 
			colorHeader.blueMask != (U32) 0xFF << 8 || 
			colorHeader.alphaMask != (U32) 0xFF << 24
		)
			_gotoIfError(clean, Error_invalidParameter(0, 0, "BMP_read()::buf contained unsupported BMPColorHeader"));

		isBGR = colorHeader.redMask == 0xFF;

		if(isBGR && colorHeader.greenMask != (U32) 0xFF << 16)
			_gotoIfError(clean, Error_invalidParameter(0, 0, "BMP_read()::buf contained invalid BMPColorHeader"));

		if(!isBGR && (colorHeader.greenMask != 0xFF || colorHeader.redMask != (U32) 0xFF << 16))
			_gotoIfError(clean, Error_invalidParameter(0, 0, "BMP_read()::buf contained invalid BMPColorHeader"));
	}

	info->textureFormatId = isBGR ? ETextureFormatId_BGRA8 : ETextureFormatId_RGBA8;

	//If it's not flipped, we don't need to do anything

	const U8 *dataStart = start + header.offsetData;

	if (!info->isFlipped) {
		*result = Buffer_createRefConst(dataStart, len);
		return Error_none();
	}

	//Otherwise we need to flip it manually and allocate a new buffer

	_gotoIfError(clean, Buffer_createEmptyBytes(len, allocator, result));

	U64 stride = (U64)info->w * 4;

	for (U64 j = info->h - 1, k = 0; j != U64_MAX; --j, ++k)
		Buffer_copy(
			Buffer_createRef((U8*)result->ptr + stride * k, stride),
			Buffer_createRefConst(dataStart + stride * j, stride)
		);

	return Error_none();

clean:
	Buffer_free(result, allocator);
	return err;
}
