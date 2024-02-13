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

#pragma once
#include "types/types.h"

typedef enum ETexturePrimitive {
	ETexturePrimitive_Undefined,
	ETexturePrimitive_UNorm,
	ETexturePrimitive_SNorm,
	ETexturePrimitive_UInt,
	ETexturePrimitive_SInt,
	ETexturePrimitive_Float,
	ETexturePrimitive_Compressed,
	ETexturePrimitive_UNormBGR				//Special kind of formats
} ETexturePrimitive;

typedef enum ETextureType {
	ETextureType_2D,
	ETextureType_3D,
	ETextureType_Cube,
	ETextureType_Count
} ETextureType;

typedef enum ETextureAlignment {
	ETextureAlignment_4,
	ETextureAlignment_5,
	ETextureAlignment_6,
	ETextureAlignment_8,
	ETextureAlignment_10,
	ETextureAlignment_12
} ETextureAlignment;

typedef enum ETextureCompressionType {
	ETextureCompressionType_UNorm,
	ETextureCompressionType_SNorm,
	ETextureCompressionType_Float,
	ETextureCompressionType_sRGB,
	ETextureCompressionType_Invalid
} ETextureCompressionType;

typedef enum ETextureCompressionAlgo {
	ETextureCompressionAlgo_ASTC,
	ETextureCompressionAlgo_BCn,
	ETextureCompressionAlgo_None			//Don't use in _ETextureFormatCompressed
} ETextureCompressionAlgo;

#define _ETextureFormat(primitive, redBits, greenBits, blueBits, alphaBits)												\
((alphaBits >> 1) | ((blueBits >> 1) << 6) | ((greenBits >> 1) << 12) | ((redBits >> 1) << 18) | (primitive << 24) |	\
(((redBits + greenBits + blueBits + alphaBits) >> 2) - 1) << 27)

#define _ETextureFormatCompressed(algo, blockSizeBits, alignmentX, alignmentY, compType, hasRed, hasGreen, hasBlue, hasAlpha) \
((hasAlpha ? 1 : 0) | ((hasBlue ? 1 : 0) << 6) | (compType << 9) | ((hasGreen ? 1 : 0) << 12) | (alignmentY << 15) |	\
 ((hasRed ? 1 : 0) << 18) | (alignmentX << 21) |																		\
 (ETexturePrimitive_Compressed << 24) | (((blockSizeBits >> 6) - 1) << 27) | (algo << 30))

//Format of a texture; a bitflag of the properties of the format.
//
//A normal texture format contains the following information:
//(in octal): 0'<nibbleSize>'<primitive>'<redBits>'<greenBits>'<blueBits>'<alphaBits>
//
//Bit count is a multiple of 2 (so shifted by 1)
//Nibble size up to 5 bits (2 octanibbles where the upper bit is unused, dec by 1).
//  The size of each pixel in nibbles
//
//If compressed or extended, the get<X>Bits don't have to return a valid number.
//However, it should be non zero if the channel is present, so the has<X> functions are still available.
//
//Compression used the following;
//(in octal):
// 0'<compressionType>'<uint2Size>'<primitive>'<alignmentX>'<hasRed>'<alignmentY>'<hasGreen>'<type>'<hasBlue>'0'<hasAlpha>
//Type is ETextureAlignment
//Spaces is ETextureCompressionType
//Booleans of >1 are also invalid
//For compression, the nibbleSize is replaced with size in uint2s (8-byte)
//E.g. 0 = 8 byte, 1 = 16 byte
//If compression is enabled: Compression algo: ASTC (0), BCn (1)
//
typedef enum ETextureFormat {

	ETextureFormat_Undefined		= _ETextureFormat(ETextureCompressionType_Invalid, 4, 0, 0, 0),

	//8-bit

	ETextureFormat_R8				= _ETextureFormat(ETexturePrimitive_UNorm, 8, 0, 0, 0),
	ETextureFormat_RG8				= _ETextureFormat(ETexturePrimitive_UNorm, 8, 8, 0, 0),
	ETextureFormat_RGBA8			= _ETextureFormat(ETexturePrimitive_UNorm, 8, 8, 8, 8),
	ETextureFormat_BGRA8			= _ETextureFormat(ETexturePrimitive_UNormBGR, 8, 8, 8, 8),		//Swapchain format

	ETextureFormat_R8s				= _ETextureFormat(ETexturePrimitive_SNorm, 8, 0, 0, 0),
	ETextureFormat_RG8s				= _ETextureFormat(ETexturePrimitive_SNorm, 8, 8, 0, 0),
	ETextureFormat_RGBA8s			= _ETextureFormat(ETexturePrimitive_SNorm, 8, 8, 8, 8),

	ETextureFormat_R8u				= _ETextureFormat(ETexturePrimitive_UInt, 8, 0, 0, 0),
	ETextureFormat_RG8u				= _ETextureFormat(ETexturePrimitive_UInt, 8, 8, 0, 0),
	ETextureFormat_RGBA8u			= _ETextureFormat(ETexturePrimitive_UInt, 8, 8, 8, 8),

	ETextureFormat_R8i				= _ETextureFormat(ETexturePrimitive_SInt, 8, 0, 0, 0),
	ETextureFormat_RG8i				= _ETextureFormat(ETexturePrimitive_SInt, 8, 8, 0, 0),
	ETextureFormat_RGBA8i			= _ETextureFormat(ETexturePrimitive_SInt, 8, 8, 8, 8),

	//16-bit

	ETextureFormat_R16				= _ETextureFormat(ETexturePrimitive_UNorm, 16, 0, 0, 0),
	ETextureFormat_RG16				= _ETextureFormat(ETexturePrimitive_UNorm, 16, 16, 0, 0),
	ETextureFormat_RGBA16			= _ETextureFormat(ETexturePrimitive_UNorm, 16, 16, 16, 16),

	ETextureFormat_R16s				= _ETextureFormat(ETexturePrimitive_SNorm, 16, 0, 0, 0),
	ETextureFormat_RG16s			= _ETextureFormat(ETexturePrimitive_SNorm, 16, 16, 0, 0),
	ETextureFormat_RGBA16s			= _ETextureFormat(ETexturePrimitive_SNorm, 16, 16, 16, 16),

	ETextureFormat_R16u				= _ETextureFormat(ETexturePrimitive_UInt, 16, 0, 0, 0),
	ETextureFormat_RG16u			= _ETextureFormat(ETexturePrimitive_UInt, 16, 16, 0, 0),
	ETextureFormat_RGBA16u			= _ETextureFormat(ETexturePrimitive_UInt, 16, 16, 16, 16),

	ETextureFormat_R16i				= _ETextureFormat(ETexturePrimitive_SInt, 16, 0, 0, 0),
	ETextureFormat_RG16i			= _ETextureFormat(ETexturePrimitive_SInt, 16, 16, 0, 0),
	ETextureFormat_RGBA16i			= _ETextureFormat(ETexturePrimitive_SInt, 16, 16, 16, 16),

	ETextureFormat_R16f				= _ETextureFormat(ETexturePrimitive_Float, 16, 0, 0, 0),
	ETextureFormat_RG16f			= _ETextureFormat(ETexturePrimitive_Float, 16, 16, 0, 0),
	ETextureFormat_RGBA16f			= _ETextureFormat(ETexturePrimitive_Float, 16, 16, 16, 16),

	//32-bit

	ETextureFormat_R32u				= _ETextureFormat(ETexturePrimitive_UInt, 32, 0, 0, 0),
	ETextureFormat_RG32u			= _ETextureFormat(ETexturePrimitive_UInt, 32, 32, 0, 0),
	ETextureFormat_RGB32u			= _ETextureFormat(ETexturePrimitive_UInt, 32, 32, 32, 0),
	ETextureFormat_RGBA32u			= _ETextureFormat(ETexturePrimitive_UInt, 32, 32, 32, 32),

	ETextureFormat_R32i				= _ETextureFormat(ETexturePrimitive_SInt, 32, 0, 0, 0),
	ETextureFormat_RG32i			= _ETextureFormat(ETexturePrimitive_SInt, 32, 32, 0, 0),
	ETextureFormat_RGB32i			= _ETextureFormat(ETexturePrimitive_SInt, 32, 32, 32, 0),
	ETextureFormat_RGBA32i			= _ETextureFormat(ETexturePrimitive_SInt, 32, 32, 32, 32),

	ETextureFormat_R32f				= _ETextureFormat(ETexturePrimitive_Float, 32, 0, 0, 0),
	ETextureFormat_RG32f			= _ETextureFormat(ETexturePrimitive_Float, 32, 32, 0, 0),
	ETextureFormat_RGB32f			= _ETextureFormat(ETexturePrimitive_Float, 32, 32, 32, 0),
	ETextureFormat_RGBA32f			= _ETextureFormat(ETexturePrimitive_Float, 32, 32, 32, 32),

	//Special purpose formats

	ETextureFormat_BGR10A2			= _ETextureFormat(ETexturePrimitive_UNormBGR, 10, 10, 10, 2),

	//Compression formats

	//Desktop

	ETextureFormat_BC4				= _ETextureFormatCompressed(
		ETextureCompressionAlgo_BCn, 64, ETextureAlignment_4, ETextureAlignment_4,
		ETextureCompressionType_UNorm, 1, 0, 0, 0
	),

	ETextureFormat_BC5				= _ETextureFormatCompressed(
		ETextureCompressionAlgo_BCn, 128, ETextureAlignment_4, ETextureAlignment_4,
		ETextureCompressionType_UNorm, 1, 1, 0, 0
	),

	ETextureFormat_BC6H				= _ETextureFormatCompressed(
		ETextureCompressionAlgo_BCn, 128, ETextureAlignment_4, ETextureAlignment_4,
		ETextureCompressionType_Float, 1, 1, 1, 0
	),

	ETextureFormat_BC7				= _ETextureFormatCompressed(
		ETextureCompressionAlgo_BCn, 128, ETextureAlignment_4, ETextureAlignment_4,
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_BC4s				= _ETextureFormatCompressed(
		ETextureCompressionAlgo_BCn, 64, ETextureAlignment_4, ETextureAlignment_4,
		ETextureCompressionType_SNorm, 1, 0, 0, 0
	),

	ETextureFormat_BC5s				= _ETextureFormatCompressed(
		ETextureCompressionAlgo_BCn, 128, ETextureAlignment_4, ETextureAlignment_4,
		ETextureCompressionType_SNorm, 1, 1, 0, 0
	),

	ETextureFormat_BC7_sRGB			= _ETextureFormatCompressed(
		ETextureCompressionAlgo_BCn, 128, ETextureAlignment_4, ETextureAlignment_4,
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),

	//Mobile / console

	ETextureFormat_ASTC_4x4			= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_4, ETextureAlignment_4,
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_4x4_sRGB	= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_4, ETextureAlignment_4,
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_5x4			= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_5, ETextureAlignment_4,
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_5x4_sRGB	= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_5, ETextureAlignment_4,
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_5x5			= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_5, ETextureAlignment_5,
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_5x5_sRGB	= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_5, ETextureAlignment_5,
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_6x5			= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_6, ETextureAlignment_5,
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_6x5_sRGB	= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_6, ETextureAlignment_5,
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_6x6			= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_6, ETextureAlignment_6,
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_6x6_sRGB	= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_6, ETextureAlignment_6,
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_8x5			= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_8, ETextureAlignment_5,
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_8x5_sRGB	= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_8, ETextureAlignment_5,
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_8x6			= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_8, ETextureAlignment_6,
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_8x6_sRGB	= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_8, ETextureAlignment_6,
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_8x8			= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_8, ETextureAlignment_8,
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_8x8_sRGB	= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_8, ETextureAlignment_8,
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_10x5		= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_10, ETextureAlignment_5,
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_10x5_sRGB	= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_10, ETextureAlignment_5,
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_10x6		= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_10, ETextureAlignment_6,
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_10x6_sRGB	= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_10, ETextureAlignment_6,
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_10x8		= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_10, ETextureAlignment_8,
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_10x8_sRGB	= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_10, ETextureAlignment_8,
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_10x10		= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_10, ETextureAlignment_10,
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_10x10_sRGB	= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_10, ETextureAlignment_10,
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_12x10		= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_12, ETextureAlignment_10,
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_12x10_sRGB	= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_12, ETextureAlignment_10,
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_12x12		= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_12, ETextureAlignment_12,
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_12x12_sRGB	= _ETextureFormatCompressed(
		ETextureCompressionAlgo_ASTC, 128, ETextureAlignment_12, ETextureAlignment_12,
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	)

} ETextureFormat;

ETexturePrimitive ETextureFormat_getPrimitive(ETextureFormat f);

Bool ETextureFormat_getIsCompressed(ETextureFormat f);

U64 ETextureFormat_getBits(ETextureFormat f);
U64 ETextureFormat_getAlphaBits(ETextureFormat f);
U64 ETextureFormat_getBlueBits(ETextureFormat f);
U64 ETextureFormat_getGreenBits(ETextureFormat f);
U64 ETextureFormat_getRedBits(ETextureFormat f);

Bool ETextureFormat_hasRed(ETextureFormat f);
Bool ETextureFormat_hasGreen(ETextureFormat f);
Bool ETextureFormat_hasBlue(ETextureFormat f);
Bool ETextureFormat_hasAlpha(ETextureFormat f);

U8 ETextureFormat_getChannels(ETextureFormat f);

ETextureCompressionType ETextureFormat_getCompressionType(ETextureFormat f);
ETextureCompressionAlgo ETextureFormat_getCompressionAlgo(ETextureFormat f);

//Get the alignments (x, y) of a texture format
//Returns false if it doesn't need alignment (so alignment = 1x1)

Bool ETextureFormat_getAlignment(ETextureFormat f, U8 *x, U8 *y);

//Get texture's size in bytes
//Returns U64_MAX if misaligned (compressed formats)

U64 ETextureFormat_getSize(ETextureFormat f, U32 w, U32 h, U32 l);

//Map ETextureFormat to simplified id for storing in a more compact manner
//ETextureFormatId = U8 while ETextureFormat is U32.

typedef enum ETextureFormatId {

	ETextureFormatId_Undefined,

	//Standard formats

	ETextureFormatId_R8,			ETextureFormatId_RG8,		ETextureFormatId_RGBA8,		ETextureFormatId_BGRA8,
	ETextureFormatId_R8s,			ETextureFormatId_RG8s,		ETextureFormatId_RGBA8s,
	ETextureFormatId_R8u,			ETextureFormatId_RG8u,		ETextureFormatId_RGBA8u,
	ETextureFormatId_R8i,			ETextureFormatId_RG8i,		ETextureFormatId_RGBA8i,

	ETextureFormatId_R16,			ETextureFormatId_RG16,		ETextureFormatId_RGBA16,
	ETextureFormatId_R16s,			ETextureFormatId_RG16s,		ETextureFormatId_RGBA16s,
	ETextureFormatId_R16u,			ETextureFormatId_RG16u,		ETextureFormatId_RGBA16u,
	ETextureFormatId_R16i,			ETextureFormatId_RG16i,		ETextureFormatId_RGBA16i,
	ETextureFormatId_R16f,			ETextureFormatId_RG16f,		ETextureFormatId_RGBA16f,

	ETextureFormatId_R32u,			ETextureFormatId_RG32u,		ETextureFormatId_RGBA32u,
	ETextureFormatId_R32i,			ETextureFormatId_RG32i,		ETextureFormatId_RGBA32i,
	ETextureFormatId_R32f,			ETextureFormatId_RG32f,		ETextureFormatId_RGBA32f,

	//Special format

	ETextureFormatId_RGB10A2,
	ETextureFormatId_BGR10A2,

	//BCn

	ETextureFormatId_BC4,			ETextureFormatId_BC5,		ETextureFormatId_BC6H,		ETextureFormatId_BC7,
	ETextureFormatId_BC4s,			ETextureFormatId_BC5s,									ETextureFormatId_BC7_sRGB,

	//ASTC

	ETextureFormatId_ASTC_4x4,		ETextureFormatId_ASTC_4x4_sRGB,

	ETextureFormatId_ASTC_5x4,		ETextureFormatId_ASTC_5x4_sRGB,
	ETextureFormatId_ASTC_5x5,		ETextureFormatId_ASTC_5x5_sRGB,

	ETextureFormatId_ASTC_6x5,		ETextureFormatId_ASTC_6x5_sRGB,
	ETextureFormatId_ASTC_6x6,		ETextureFormatId_ASTC_6x6_sRGB,

	ETextureFormatId_ASTC_8x5,		ETextureFormatId_ASTC_8x5_sRGB,
	ETextureFormatId_ASTC_8x6,		ETextureFormatId_ASTC_8x6_sRGB,
	ETextureFormatId_ASTC_8x8,		ETextureFormatId_ASTC_8x8_sRGB,

	ETextureFormatId_ASTC_10x5,		ETextureFormatId_ASTC_10x5_sRGB,
	ETextureFormatId_ASTC_10x6,		ETextureFormatId_ASTC_10x6_sRGB,
	ETextureFormatId_ASTC_10x8,		ETextureFormatId_ASTC_10x8_sRGB,
	ETextureFormatId_ASTC_10x10,	ETextureFormatId_ASTC_10x10_sRGB,

	ETextureFormatId_ASTC_12x10,	ETextureFormatId_ASTC_12x10_sRGB,
	ETextureFormatId_ASTC_12x12,	ETextureFormatId_ASTC_12x12_sRGB,

	ETextureFormatId_Count

} ETextureFormatId;

static const ETextureFormat ETextureFormatId_unpack[] = {

	ETextureFormat_Undefined,

	//Standard formats

	ETextureFormat_R8,			ETextureFormat_RG8,			ETextureFormat_RGBA8,		ETextureFormat_BGRA8,
	ETextureFormat_R8s,			ETextureFormat_RG8s,		ETextureFormat_RGBA8s,
	ETextureFormat_R8u,			ETextureFormat_RG8u,		ETextureFormat_RGBA8u,
	ETextureFormat_R8i,			ETextureFormat_RG8i,		ETextureFormat_RGBA8i,

	ETextureFormat_R16,			ETextureFormat_RG16,		ETextureFormat_RGBA16,
	ETextureFormat_R16s,		ETextureFormat_RG16s,		ETextureFormat_RGBA16s,
	ETextureFormat_R16u,		ETextureFormat_RG16u,		ETextureFormat_RGBA16u,
	ETextureFormat_R16i,		ETextureFormat_RG16i,		ETextureFormat_RGBA16i,
	ETextureFormat_R16f,		ETextureFormat_RG16f,		ETextureFormat_RGBA16f,

	ETextureFormat_R32u,		ETextureFormat_RG32u,		ETextureFormat_RGBA32u,
	ETextureFormat_R32i,		ETextureFormat_RG32i,		ETextureFormat_RGBA32i,
	ETextureFormat_R32f,		ETextureFormat_RG32f,		ETextureFormat_RGBA32f,

	//Special format

	ETextureFormat_BGR10A2,

	//BCn

	ETextureFormat_BC4,			ETextureFormat_BC5,			ETextureFormat_BC6H,		ETextureFormat_BC7,
	ETextureFormat_BC4s,		ETextureFormat_BC5s,									ETextureFormat_BC7_sRGB,

	//ASTC

	ETextureFormat_ASTC_4x4,	ETextureFormat_ASTC_4x4_sRGB,

	ETextureFormat_ASTC_5x4,	ETextureFormat_ASTC_5x4_sRGB,
	ETextureFormat_ASTC_5x5,	ETextureFormat_ASTC_5x5_sRGB,

	ETextureFormat_ASTC_6x5,	ETextureFormat_ASTC_6x5_sRGB,
	ETextureFormat_ASTC_6x6,	ETextureFormat_ASTC_6x6_sRGB,

	ETextureFormat_ASTC_8x5,	ETextureFormat_ASTC_8x5_sRGB,
	ETextureFormat_ASTC_8x6,	ETextureFormat_ASTC_8x6_sRGB,
	ETextureFormat_ASTC_8x8,	ETextureFormat_ASTC_8x8_sRGB,

	ETextureFormat_ASTC_10x5,	ETextureFormat_ASTC_10x5_sRGB,
	ETextureFormat_ASTC_10x6,	ETextureFormat_ASTC_10x6_sRGB,
	ETextureFormat_ASTC_10x8,	ETextureFormat_ASTC_10x8_sRGB,
	ETextureFormat_ASTC_10x10,	ETextureFormat_ASTC_10x10_sRGB,

	ETextureFormat_ASTC_12x10,	ETextureFormat_ASTC_12x10_sRGB,
	ETextureFormat_ASTC_12x12,	ETextureFormat_ASTC_12x12_sRGB
};
