/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/base/allocator.h"
#include "types/container/buffer.h"
#include "types/base/error.h"

#ifndef DISALLOW_BMP_OXC3_PLATFORMS

	#include "platforms/platform.h"

	Error BMP_writex(Buffer buf, BMPInfo info, Buffer *result) {
		return BMP_write(buf, info, Platform_instance->alloc, result);
	}

#endif

Error BMP_write(Buffer buf, BMPInfo info, Allocator allocator, Buffer *result) {

	if(!result)
		return Error_nullPointer(5, "BMP_write()::result is required");

	if(result->ptr)
		return Error_invalidParameter(
			5, 0, "BMP_write()::result isn't empty, indicating possible memleak"
		);

	if(!info.w || !info.h)
		return Error_invalidParameter(!info.w ? 1 : 2, 0, "BMP_write()::w and h are required");

	if(info.xPixPerM < 0 || info.yPixPerM < 0)
		return Error_invalidParameter(
			info.xPixPerM < 0 ? 5 : 6, 0, "BMP_write()::xPixPerM and yPixPerM have to be >0"
		);

	if((info.w >> 31) || (info.h >> 31))
		return Error_invalidParameter(
			(info.w >> 31) ? 1 : 2, 0, "BMP_write()::w and h can't exceed I32_MAX"
		);

	if(info.textureFormatId != ETextureFormatId_BGRA8)
		return Error_invalidParameter(
			1, 3, "BMP_write()::textureFormatId is only supported for BGRA8 currently"
		);

	const U64 bufLen = Buffer_length(buf);

	const U64 pixelStride = info.discardAlpha ? 3 : 4;
	const U64 stride = (info.w * pixelStride + 3) &~ 3;		//Every line needs to be 4-byte aligned

	if(info.h * stride + BMP_reqHeadersSize > (U64)I32_MAX || bufLen != (U64)info.w * info.h * 4)
		return Error_invalidParameter(0, 0, "BMP_write() BMP has an image limit of 2GiB");

	const BMPHeader header = (BMPHeader) {
		.fileType = BMP_MAGIC,
		.fileSize = (U32)(info.h * stride + BMP_reqHeadersSize),
		.offsetData = BMP_reqHeadersSize
	};

	const BMPInfoHeader infoHeader = (BMPInfoHeader) {
		.headerSize = sizeof(BMPInfoHeader),
		.width = (I32) info.w,
		.height = (I32) info.h * (info.isFlipped ? 1 : -1),
		.planes = 1,
		.bitCount = info.discardAlpha ? 24 : 32,
		.xPixPerM = info.xPixPerM,
		.yPixPerM = info.yPixPerM
	};

	Buffer file = Buffer_createNull();

	Error err = Buffer_createUninitializedBytes(BMP_reqHeadersSize + info.h * stride, allocator, &file);

	if(err.genericError)
		return err;

	Buffer fileAppend = Buffer_createRefFromBuffer(file, false);

	gotoIfError(clean, Buffer_append(&fileAppend, &header, sizeof(header)))
	gotoIfError(clean, Buffer_append(&fileAppend, &infoHeader, sizeof(infoHeader)))

	if (pixelStride == 3) {		//Copy without alpha, since buffer lengths aren't the same

		//Unfortunately we have to copy per row, since image can be flipped or contain padding

		for (
			U64 j = info.isFlipped ? info.h - 1 : 0, k = 0;
			info.isFlipped ? j != U64_MAX : j < info.h;
			info.isFlipped ? --j : ++j, ++k
		) {

			//Write two pixels at a time through a U64,
			//We have to shift out the alpha though.

			U64 dstOff = 0;

			for (
				U64 srcOff = 0;
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
