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

//formats/bmp/bmp_headers.h

#pragma once
#include "types/base/types.h"

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

	typedef struct BMPHeadersCombined {
		BMPHeader header;
		BMPInfoHeader info;
	} BMPHeadersCombined;

#pragma pack(pop)

static const U16 BMP_MAGIC = 0x4D42;
