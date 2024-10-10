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

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

//DDS headers
//Combo between
// http://doc.51windows.net/Directx9_SDK/graphics/reference/ddsfilereference/ddsfileformat.htm#surface_format_header
// https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-header
// https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide

typedef enum EDDSFlag {

	EDDSFlag_Caps						= 1 <<  0,
	EDDSFlag_Height						= 1 <<  1,
	EDDSFlag_Width						= 1 <<  2,
	EDDSFlag_Pitch						= 1 <<  3,
	EDDSFlag_PixelFormat				= 1 << 12,
	EDDSFlag_Mips						= 1 << 17,
	EDDSFlag_LinearSize					= 1 << 19,
	EDDSFlag_Depth						= 1 << 23,

	EDDSFlag_Default					= EDDSFlag_Caps | EDDSFlag_Height | EDDSFlag_Width | EDDSFlag_PixelFormat,
	EDDSFlags_Supported					=
		EDDSFlag_Default | EDDSFlag_Pitch | EDDSFlag_Mips | EDDSFlag_LinearSize | EDDSFlag_Depth

} EDDSFlag;

typedef enum EDDSCapsFlags1 {
	EDDSCapsFlags1_Complex				= 1 << 3,
	EDDSCapsFlags1_Texture				= 1 << 12,
	EDDSCapsFlags1_Mips					= 1 << 22,
	EDDSCapsFlags1_Supported			= EDDSCapsFlags1_Complex | EDDSCapsFlags1_Texture | EDDSCapsFlags1_Mips
} EDDSCapsFlags1;

typedef enum EDDSCapsFlags2 {
	EDDSCapsFlags2_Cubemap				= 0xFE00,		//Contains the following bits: Cubemap, +x, -x, +y, -y, +z, -z
	EDDSCapsFlags2_Volume				= 1 << 21,
	EDDSCapsFlags2_Supported			= EDDSCapsFlags2_Cubemap | EDDSCapsFlags2_Volume
} EDDSCapsFlags2;

typedef struct DDSCaps2 {
	EDDSCapsFlags1 flag1;
	EDDSCapsFlags2 flag2;
	U32 padding[3];
} DDSCaps2;

typedef enum EDDSPixelFormatFlag {

	EDDSPixelFormatFlag_AlphaPixels		= 1 << 0,
	//EDDSPixelFormatFlag_Alpha			= 1 << 1,		//Unsupported
	EDDSPixelFormatFlag_MagicNumber		= 1 << 2,
	EDDSPixelFormatFlag_RGB				= 1 << 6,
	//EDDSPixelFormatFlag_YUV			= 1 << 9,		//Unsupported
	//EDDSPixelFormatFlag_Luminance		= 1 << 17,		//Unsupported

	EDDSPixelFormatFlag_Supported		=
		EDDSPixelFormatFlag_AlphaPixels | EDDSPixelFormatFlag_MagicNumber | EDDSPixelFormatFlag_RGB,

} EDDSPixelFormatFlag;

typedef enum EDDSFormatMagic {
	EDDSFormatMagic_DX10				= C8x4('D', 'X', '1', '0'),
	EDDSFormatMagic_BC4					= C8x4('B', 'C', '4', 'U'),
	EDDSFormatMagic_BC4s				= C8x4('B', 'C', '4', 'S'),
	EDDSFormatMagic_BC5					= C8x4('A', 'T', 'I', '2'),
	EDDSFormatMagic_BC5s				= C8x4('B', 'C', '5', 'S'),
	EDDSFormatMagic_RGBA16				= 36,
	EDDSFormatMagic_RGBA16s				= 110,
	EDDSFormatMagic_R16f				= 111,
	EDDSFormatMagic_RG16f				= 112,
	EDDSFormatMagic_RGBA16f				= 113,
	EDDSFormatMagic_R32f				= 114,
	EDDSFormatMagic_RG32f				= 115,
	EDDSFormatMagic_RGBA32f				= 116
} EDDSFormatMagic;

typedef struct DDSPixelFormat {

	U32 size;					//sizeof(DDSPixelFormat) = 32
	EDDSPixelFormatFlag flags;
	U32 magicNumber;			//EDDSFormatMagic or DXFormat if EDDSPixelFormatFlag_MagicNumber
	U32 rgbBitCount;

	U32 masks[4];

} DDSPixelFormat;

typedef struct DDSHeader {

	U32 magicNumber;			//"DDS "

	U32 size;					//sizeof(DDSHeader) - sizeof(magicNumber) = 128 - 4 = 124
	EDDSFlag flags;
	U32 height, width;
	U32 pitchOrLinearSize;		//flags & EDDSFlag_Pitch or LinearSize
	U32 depth;					//flags & EDDSFlag_Depth
	U32 mips;					//flags & EDDSFlag_Mips

	U32 pad0[11];

	DDSPixelFormat format;		//flags & EDDSFlag_PixelFormat

	DDSCaps2 caps;				//flags & EDDSFlag_Caps

} DDSHeader;

typedef enum EDX10Dim {
	EDX10Dim_1D = 2,
	EDX10Dim_2D,
	EDX10Dim_3D
} EDX10Dim;

typedef enum EDX10Misc {
	EDX10Misc_IsCube		= 1 << 2,
	EDX10Misc_Supported		= EDX10Misc_IsCube
} EDX10Misc;

typedef enum EDX10AlphaMode {
	EDX10AlphaMode_Unknown,
	EDX10AlphaMode_Normal,
	EDX10AlphaMode_Premul,
	EDX10AlphaMode_Opaque,
	EDX10AlphaMode_Custom,
	EDX10AlphaMode_Count
} EDX10AlphaMode;

typedef struct DDSHeaderDXT10 {
	DXFormat format;
	EDX10Dim dim;
	EDX10Misc miscFlag;
	U32 arraySize;
	EDX10AlphaMode miscFlags2;
} DDSHeaderDXT10;

static const U32 ddsMagic = C8x4('D', 'D', 'S', ' ');

#ifdef __cplusplus
	}
#endif
