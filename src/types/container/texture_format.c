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

//types/container/texture_format.c

#include "types/container/texture_format.h"

const C8 *ETextureFormatId_name[ETextureFormatId_Count] = {

	"Undefined",

	"R8",            "RG8",        "RGBA8",        "BGRA8",
	"R8s",            "RG8s",        "RGBA8s",
	"R8u",            "RG8u",        "RGBA8u",
	"R8i",            "RG8i",        "RGBA8i",

	"R16",            "RG16",        "RGBA16",
	"R16s",            "RG16s",    "RGBA16s",
	"R16u",            "RG16u",    "RGBA16u",
	"R16i",            "RG16i",    "RGBA16i",
	"R16f",            "RG16f",    "RGBA16f",

	"R32u",            "RG32u",    "RGB32u",    "RGBA32u",
	"R32i",            "RG32i",    "RGB32i",    "RGBA32i",
	"R32f",            "RG32f",    "RGB32f",    "RGBA32f",

	"BGR10A2",

	"BC4",            "BC5",        "BC6H",        "BC7",
	"BC4s",            "BC5s",                    "BC7 sRGB",

	"ASTC 4x4",        "ASTC 4x4 sRGB",

	"ASTC 5x4",        "ASTC 5x4 sRGB",
	"ASTC 5x5",        "ASTC 5x5 sRGB",

	"ASTC 6x5",        "ASTC 6x5 sRGB",
	"ASTC 6x6",        "ASTC 6x6 sRGB",

	"ASTC 8x5",        "ASTC 8x5 sRGB",
	"ASTC 8x6",        "ASTC 8x6 sRGB",
	"ASTC 8x8",        "ASTC 8x8 sRGB",

	"ASTC 10x5",    "ASTC 10x5 sRGB",
	"ASTC 10x6",    "ASTC 10x6 sRGB",
	"ASTC 10x8",    "ASTC 10x8 sRGB",
	"ASTC 10x10",    "ASTC 10x10 sRGB",

	"ASTC 12x10",    "ASTC 12x10 sRGB",
	"ASTC 12x12",    "ASTC 12x12 sRGB"
};
