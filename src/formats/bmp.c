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
#include "types/buffer.h"
#include "types/allocator.h"
#include "types/error.h"

#pragma pack(push, 1)

	typedef struct BMPHeader {
		U16 fileType;
		U32 fileSize;
		U16 r0, r1;
		U32 offsetData;
	} BMPHeader;

	typedef struct BMPInfoHeader {
		U32 size;
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

Error BMP_writeRGBA(
	Buffer buf,
	U16 w,
	U16 h,
	Bool isFlipped, 
	Allocator allocator,
	Buffer *result
) {

	if(!result)
		return Error_nullPointer(5);

	if(result->ptr)
		return Error_invalidParameter(5, 0);

	if(!w || !h)
		return Error_invalidParameter(!w ? 1 : 2, 0);

	U64 bufLen = Buffer_length(buf);

	if(bufLen > I32_MAX || bufLen != (U64)w * h * 4)
		return Error_invalidParameter(0, 0);

	U32 headersSize = (U32) (
		sizeof(BMPHeader) + 
		sizeof(BMPInfoHeader) + 
		sizeof(BMPColorHeader)
	);

	BMPHeader header = (BMPHeader) {
		.fileType = BMP_MAGIC,
		.fileSize = ((I32) bufLen) * (isFlipped ? -1 : 1),
		.offsetData = headersSize
	};

	BMPInfoHeader infoHeader = (BMPInfoHeader) {
		.size = sizeof(BMPInfoHeader),
		.width = w,
		.height = h,
		.planes = 1,
		.bitCount = 32,
		.compression = 3		//rgba8
	};

	BMPColorHeader colorHeader = (BMPColorHeader) {

		.redMask	= (U32) 0xFF << 16,
		.greenMask	= (U32) 0xFF << 8,
		.blueMask	= (U32) 0xFF,
		.alphaMask	= (U32) 0xFF << 24,

		.colorSpaceType = BMP_SRGB_MAGIC
	};

	Buffer file = Buffer_createNull();

	Error err = Buffer_createUninitializedBytes(
		headersSize + bufLen,
		allocator,
		&file
	);

	if(err.genericError)
		return err;

	Buffer fileAppend = Buffer_createRefFromBuffer(file, false);

	_gotoIfError(clean, Buffer_append(&fileAppend, &header, sizeof(header)));
	_gotoIfError(clean, Buffer_append(&fileAppend, &infoHeader, sizeof(infoHeader)));
	_gotoIfError(clean, Buffer_append(&fileAppend, &colorHeader, sizeof(colorHeader)));
	_gotoIfError(clean, Buffer_appendBuffer(&fileAppend, buf));

	*result = file;
	return Error_none();

clean:
	Buffer_free(&file, allocator);
	return err;
}
