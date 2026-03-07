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

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct Error Error;
typedef struct Buffer Buffer;
typedef struct RefPtr RefPtr;
typedef RefPtr StreamRef;

typedef struct BMPInfo {

	U32 w, h;

	Bool isFlipped;			//Flipped only on pre-load or write time. After load it will allocate
	U8 textureFormatId;		//ETextureFormatId
	Bool discardAlpha;		//Allows storing RGBA8 as RGB8, at the runtime cost of re-allocation and copying
	U8 padding;

	I32 xPixPerM, yPixPerM;

} BMPInfo;

Bool BMP_write(
	StreamRef *stream,
	U64 *off,
	const BMPInfo *info,
	const Allocator *allocator,
	StreamRef *inputStream,			//A stream that needs to match the input's format (info->discardAlpha ? BGR8 : BGRA8)
	U64 inputOffset,
	Bool inputIsFlipped,
	Error *e_rr
);

//Read advances *off to dataOffset + dataLength, the data remains flipped in memory if isFlipped is true.
//This requires the texture copy to copy each row individually to still allow streaming.
//NOTE: If discardAlpha is true, the real format is BGR8, but that format doesn't exist.
Bool BMP_read(StreamRef *stream, U64 *off, U64 *dataOffset, BMPInfo *info, const Allocator *alloc, Error *e_rr);

#ifdef __cplusplus
	}
#endif
