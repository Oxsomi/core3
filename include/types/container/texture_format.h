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

typedef enum EDepthStencilFormat {

	EDepthStencilFormat_None,
	EDepthStencilFormat_D16,			//Prefer this if stencil isn't needed and precision is no concern on mobile
	EDepthStencilFormat_D32,			//Prefer on desktop if stencil isn't needed
	EDepthStencilFormat_D24S8Ext,		//On AMD this is unsupported, use D32S8 instead
	EDepthStencilFormat_D32S8X24Ext,	//Some older systems might not support stencil at all
	EDepthStencilFormat_S8X24Ext,		//Some APIs and devices might support this, but unlikely

	EDepthStencilFormat_Count,			//Ensure < 0x10

	EDepthStencilFormat_StencilStart = EDepthStencilFormat_D24S8Ext,
	EDepthStencilFormat_StencilEnd = EDepthStencilFormat_Count

} EDepthStencilFormat;

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

typedef U8 TextureType;

typedef enum ETextureAlignment {
	ETextureAlignment_4,
	ETextureAlignment_5,
	ETextureAlignment_6,
	ETextureAlignment_8,
	ETextureAlignment_10,
	ETextureAlignment_12
} ETextureAlignment;

static const U8 ETextureAlignment_toAlignment[] = { 4, 5, 6, 8, 10, 12 };

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

#define _ETextureFormat(primitive, redBits, greenBits, blueBits, alphaBits)											\
	((alphaBits >> 1) | ((blueBits >> 1) << 6) | ((greenBits >> 1) << 12) | ((redBits >> 1) << 18) |				\
	(primitive << 24) | (((redBits + greenBits + blueBits + alphaBits) >> 2) - 1) << 27)

#define _ETextureFormatCompressed(algo, blockSizeBits, alignmentX, alignmentY, compType, hasR, hasG, hasB, hasA)	\
	((hasA ? 1 : 0) | ((hasB ? 1 : 0) << 6) | (compType << 9) | ((hasG ? 1 : 0) << 12) | (alignmentY << 15) |		\
	((hasR ? 1 : 0) << 18) | (alignmentX << 21) |																	\
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

static inline ETexturePrimitive ETextureFormat_getPrimitive(ETextureFormat f) {
	return (ETexturePrimitive)((f >> 24) & 7);
}

static inline Bool ETextureFormat_getIsCompressed(ETextureFormat f) {
	return ETextureFormat_getPrimitive(f) == ETexturePrimitive_Compressed;
}

static inline U64 ETextureFormat_getBits(ETextureFormat f) {
	const Bool isCompressed = ETextureFormat_getIsCompressed(f);
	const U64 length = ((f >> 27) & (isCompressed ? 7 : 0x1F)) + 1;
	return length << (isCompressed ? 6 : 2);
}

static inline U64 ETextureFormat_getAlphaBits(ETextureFormat f) {
	return ETextureFormat_getIsCompressed(f) ? f & 7 : (f & 077) << 1;
}

static inline U64 ETextureFormat_getBlueBits(ETextureFormat f) {
	return ETextureFormat_getIsCompressed(f) ? (f >> 6) & 7 : ((f >> 6) & 077) << 1;
}

static inline U64 ETextureFormat_getGreenBits(ETextureFormat f) {
	return ETextureFormat_getIsCompressed(f) ? (f >> 12) & 7 : ((f >> 12) & 077) << 1;
}

static inline U64 ETextureFormat_getRedBits(ETextureFormat f) {
	return ETextureFormat_getIsCompressed(f) ? (f >> 18) & 7 : ((f >> 18) & 077) << 1;
}

static inline Bool ETextureFormat_hasRed(ETextureFormat f) { return ETextureFormat_getRedBits(f); }
static inline Bool ETextureFormat_hasGreen(ETextureFormat f) { return ETextureFormat_getGreenBits(f); }
static inline Bool ETextureFormat_hasBlue(ETextureFormat f) { return ETextureFormat_getBlueBits(f); }
static inline Bool ETextureFormat_hasAlpha(ETextureFormat f) { return ETextureFormat_getAlphaBits(f); }

static inline U8 ETextureFormat_getChannels(ETextureFormat f) {
	return
		(U8)ETextureFormat_hasRed(f)  + (U8)ETextureFormat_hasGreen(f) +
		(U8)ETextureFormat_hasBlue(f) + (U8)ETextureFormat_hasAlpha(f);
}

static inline ETextureCompressionType ETextureFormat_getCompressionType(ETextureFormat f) {

	if(!ETextureFormat_getIsCompressed(f))
		return ETextureCompressionType_Invalid;

	return (ETextureCompressionType)((f >> 9) & 7);
}

static inline ETextureCompressionAlgo ETextureFormat_getCompressionAlgo(ETextureFormat f) {

	if(!ETextureFormat_getIsCompressed(f))
		return ETextureCompressionAlgo_None;

	return (ETextureCompressionAlgo)((f >> 30) & 7);
}

//Get the alignments (x, y) of a texture format
//Returns false if it doesn't need alignment (so alignment = 1x1)
static inline Bool ETextureFormat_getAlignment(ETextureFormat f, U8 *x, U8 *y) {

	if(!ETextureFormat_getIsCompressed(f))
		return false;

	if(x)
		*x = ETextureAlignment_toAlignment[(f >> 15) & 7];

	if(y)
		*y = ETextureAlignment_toAlignment[(f >> 21) & 7];

	return true;
}

//Get texture's size in bytes, if misaligned it will align to next block count
static inline U64 ETextureFormat_getSize(ETextureFormat f, U32 w, U32 h, U32 l) {

	U8 alignW = 1, alignH = 1;

	//If compressed; the size of the texture format is specified per blocks

	if (ETextureFormat_getAlignment(f, &alignW, &alignH)) {
		w += alignW - 1;	h += alignH - 1;
		w /= alignW;		h /= alignH;
	}

	return ((((U64)w * h * ETextureFormat_getBits(f)) + 7) >> 3) * l;
}

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

	ETextureFormatId_R32u,			ETextureFormatId_RG32u,		ETextureFormatId_RGB32u,	ETextureFormatId_RGBA32u,
	ETextureFormatId_R32i,			ETextureFormatId_RG32i,		ETextureFormatId_RGB32i,	ETextureFormatId_RGBA32i,
	ETextureFormatId_R32f,			ETextureFormatId_RG32f,		ETextureFormatId_RGB32f,	ETextureFormatId_RGBA32f,

	//Special format

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

	ETextureFormatId_Count		//Ensure Count < 0xF0, because depth stencil might occupy it.

} ETextureFormatId;

typedef U8 TextureFormatId;

extern const C8 *ETextureFormatId_name[ETextureFormatId_Count];

static const ETextureFormat ETextureFormatId_unpack[] = {

	ETextureFormat_Undefined,

	//Standard formats

	ETextureFormat_R8,				ETextureFormat_RG8,			ETextureFormat_RGBA8,		ETextureFormat_BGRA8,
	ETextureFormat_R8s,				ETextureFormat_RG8s,		ETextureFormat_RGBA8s,
	ETextureFormat_R8u,				ETextureFormat_RG8u,		ETextureFormat_RGBA8u,
	ETextureFormat_R8i,				ETextureFormat_RG8i,		ETextureFormat_RGBA8i,

	ETextureFormat_R16,				ETextureFormat_RG16,		ETextureFormat_RGBA16,
	ETextureFormat_R16s,			ETextureFormat_RG16s,		ETextureFormat_RGBA16s,
	ETextureFormat_R16u,			ETextureFormat_RG16u,		ETextureFormat_RGBA16u,
	ETextureFormat_R16i,			ETextureFormat_RG16i,		ETextureFormat_RGBA16i,
	ETextureFormat_R16f,			ETextureFormat_RG16f,		ETextureFormat_RGBA16f,

	ETextureFormat_R32u,			ETextureFormat_RG32u,		ETextureFormat_RGB32u,		ETextureFormat_RGBA32u,
	ETextureFormat_R32i,			ETextureFormat_RG32i,		ETextureFormat_RGB32i,		ETextureFormat_RGBA32i,
	ETextureFormat_R32f,			ETextureFormat_RG32f,		ETextureFormat_RGB32f,		ETextureFormat_RGBA32f,

	//Special format

	ETextureFormat_BGR10A2,

	//BCn

	ETextureFormat_BC4,				ETextureFormat_BC5,			ETextureFormat_BC6H,		ETextureFormat_BC7,
	ETextureFormat_BC4s,			ETextureFormat_BC5s,									ETextureFormat_BC7_sRGB,

	//ASTC

	ETextureFormat_ASTC_4x4,		ETextureFormat_ASTC_4x4_sRGB,

	ETextureFormat_ASTC_5x4,		ETextureFormat_ASTC_5x4_sRGB,
	ETextureFormat_ASTC_5x5,		ETextureFormat_ASTC_5x5_sRGB,

	ETextureFormat_ASTC_6x5,		ETextureFormat_ASTC_6x5_sRGB,
	ETextureFormat_ASTC_6x6,		ETextureFormat_ASTC_6x6_sRGB,

	ETextureFormat_ASTC_8x5,		ETextureFormat_ASTC_8x5_sRGB,
	ETextureFormat_ASTC_8x6,		ETextureFormat_ASTC_8x6_sRGB,
	ETextureFormat_ASTC_8x8,		ETextureFormat_ASTC_8x8_sRGB,

	ETextureFormat_ASTC_10x5,		ETextureFormat_ASTC_10x5_sRGB,
	ETextureFormat_ASTC_10x6,		ETextureFormat_ASTC_10x6_sRGB,
	ETextureFormat_ASTC_10x8,		ETextureFormat_ASTC_10x8_sRGB,
	ETextureFormat_ASTC_10x10,		ETextureFormat_ASTC_10x10_sRGB,

	ETextureFormat_ASTC_12x10,		ETextureFormat_ASTC_12x10_sRGB,
	ETextureFormat_ASTC_12x12,		ETextureFormat_ASTC_12x12_sRGB
};

//Mapping between DXGI_FORMAT and ETextureFormat

typedef U32 DXFormat;			//DXGI_FORMAT (Can't use Windows.h, bloated and not cross platform)

static const U8 DXFormat_toTextureFormatIdArr[100] = {
	ETextureFormatId_Undefined,				// 0
	ETextureFormatId_Undefined,				// 1
	ETextureFormatId_RGBA32f,				// 2
	ETextureFormatId_RGBA32u,				// 3
	ETextureFormatId_RGBA32i,				// 4
	ETextureFormatId_Undefined,				// 5
	ETextureFormatId_RGB32f,				// 6
	ETextureFormatId_RGB32u,				// 7
	ETextureFormatId_RGB32i,				// 8
	ETextureFormatId_Undefined,				// 9
	ETextureFormatId_RGBA16f,				// 10
	ETextureFormatId_RGBA16,				// 11
	ETextureFormatId_RGBA16u,				// 12
	ETextureFormatId_RGBA16s,				// 13
	ETextureFormatId_RGBA16i,				// 14
	ETextureFormatId_Undefined,				// 15
	ETextureFormatId_RG32f,					// 16
	ETextureFormatId_RG32u,					// 17
	ETextureFormatId_RG32i,					// 18
	ETextureFormatId_Undefined,				// 19
	EDepthStencilFormat_D32S8X24Ext + 0xF0,	// 20 (DepthStencil)
	ETextureFormatId_Undefined,				// 21
	ETextureFormatId_Undefined,				// 22
	ETextureFormatId_Undefined,				// 23
	ETextureFormatId_BGR10A2,				// 24
	ETextureFormatId_Undefined,				// 25
	ETextureFormatId_Undefined,				// 26
	ETextureFormatId_Undefined,				// 27
	ETextureFormatId_RGBA8,					// 28
	ETextureFormatId_Undefined,				// 29
	ETextureFormatId_RGBA8u,				// 30
	ETextureFormatId_RGBA8s,				// 31
	ETextureFormatId_RGBA8i,				// 32
	ETextureFormatId_Undefined,				// 33
	ETextureFormatId_RG16f,					// 34
	ETextureFormatId_RG16,					// 35
	ETextureFormatId_RG16u,					// 36
	ETextureFormatId_RG16s,					// 37
	ETextureFormatId_RG16i,					// 38
	ETextureFormatId_Undefined,				// 39
	EDepthStencilFormat_D32 + 0xF0,			// 40 (DepthStencil)
	ETextureFormatId_R32f,					// 41
	ETextureFormatId_R32u,					// 42
	ETextureFormatId_R32i,					// 43
	ETextureFormatId_Undefined,				// 44
	EDepthStencilFormat_D24S8Ext + 0xF0,	// 45 (DepthStencil)
	ETextureFormatId_Undefined,				// 46
	ETextureFormatId_Undefined,				// 47
	ETextureFormatId_Undefined,				// 48
	ETextureFormatId_RG8,					// 49
	ETextureFormatId_RG8u,					// 50
	ETextureFormatId_RG8s,					// 51
	ETextureFormatId_RG8i,					// 52
	ETextureFormatId_Undefined,				// 53
	ETextureFormatId_R16f,					// 54
	EDepthStencilFormat_D16 + 0xF0,			// 55 (DepthStencil)
	ETextureFormatId_R16,					// 56
	ETextureFormatId_R16u,					// 57
	ETextureFormatId_R16s,					// 58
	ETextureFormatId_R16i,					// 59
	ETextureFormatId_Undefined,				// 60
	ETextureFormatId_R8,					// 61
	ETextureFormatId_R8u,					// 62
	ETextureFormatId_R8s,					// 63
	ETextureFormatId_R8i,					// 64
	ETextureFormatId_Undefined,				// 65
	ETextureFormatId_Undefined,				// 66
	ETextureFormatId_Undefined,				// 67
	ETextureFormatId_Undefined,				// 68
	ETextureFormatId_Undefined,				// 69
	ETextureFormatId_Undefined,				// 70
	ETextureFormatId_Undefined,				// 71
	ETextureFormatId_Undefined,				// 72
	ETextureFormatId_Undefined,				// 73
	ETextureFormatId_Undefined,				// 74
	ETextureFormatId_Undefined,				// 75
	ETextureFormatId_Undefined,				// 76
	ETextureFormatId_Undefined,				// 77
	ETextureFormatId_Undefined,				// 78
	ETextureFormatId_Undefined,				// 79
	ETextureFormatId_BC4,					// 80
	ETextureFormatId_BC4s,					// 81
	ETextureFormatId_Undefined,				// 82
	ETextureFormatId_BC5,					// 83
	ETextureFormatId_BC5s,					// 84
	ETextureFormatId_Undefined,				// 85
	ETextureFormatId_Undefined,				// 86
	ETextureFormatId_BGRA8,					// 87
	ETextureFormatId_Undefined,				// 88
	ETextureFormatId_Undefined,				// 89
	ETextureFormatId_Undefined,				// 90
	ETextureFormatId_Undefined,				// 91
	ETextureFormatId_Undefined,				// 92
	ETextureFormatId_Undefined,				// 93
	ETextureFormatId_Undefined,				// 94
	ETextureFormatId_BC6H,					// 95
	ETextureFormatId_Undefined,				// 96
	ETextureFormatId_Undefined,				// 97
	ETextureFormatId_BC7,					// 98
	ETextureFormatId_BC7_sRGB				// 99
};

static inline ETextureFormatId DXFormat_toTextureFormatId(DXFormat format) {

	if ((U64)format >= sizeof(DXFormat_toTextureFormatIdArr))
		return ETextureFormatId_Undefined;

	U8 i = DXFormat_toTextureFormatIdArr[format];

	if(i >= 0xF0)		//Can't convert depth stencil to ETextureFormatId
		return ETextureFormatId_Undefined;

	return (ETextureFormatId) i;
}

static const U8 ETextureFormatId_toDXFormatArr[ETextureFormatId_Count] = {

	0,	// ETextureFormatId_Undefined

	// R8..BGRA8
	61, 49, 28, 87,		// R8, RG8, RGBA8, BGRA8
	63, 51, 31,			// R8s, RG8s, RGBA8s
	62, 50, 30,			// R8u, RG8u, RGBA8u
	64, 52, 32,			// R8i, RG8i, RGBA8i

	// R16..RGBA16f
	56, 35, 11,			// R16, RG16, RGBA16
	58, 37, 13,			// R16s, RG16s, RGBA16s
	57, 36, 12,			// R16u, RG16u, RGBA16u
	59, 38, 14,			// R16i, RG16i, RGBA16i
	54, 34, 10,			// R16f, RG16f, RGBA16f

	// R32..RGBA32f
	42, 17, 7, 3,		// R32u, RG32u, RGB32u, RGBA32u
	43, 18, 8, 4,		// R32i, RG32i, RGB32i, RGBA32i
	41, 16, 6, 2,		// R32f, RG32f, RGB32f, RGBA32f

	// Special
	24,					// BGR10A2

	// BCn
	80, 83, 95, 98,		// BC4, BC5, BC6H, BC7
	81, 84, 99,			// BC4s, BC5s, -, BC7_sRGB

	// ASTC (skip mapping to DXFormat, 0)
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0
};

static inline DXFormat ETextureFormatId_toDXFormat(ETextureFormatId format) {
	return ETextureFormatId_toDXFormatArr[format];
}

static const U8 EDepthStencilFormat_toDXFormatArr[] = { 0, 55, 40, 45, 20, 0 };

static inline DXFormat EDepthStencilFormat_toDXFormat(EDepthStencilFormat format) {

	if ((U64)format >= sizeof(EDepthStencilFormat_toDXFormatArr))
		return 0;

	return EDepthStencilFormat_toDXFormatArr[(U64)format];
}

static inline EDepthStencilFormat DXFormat_toDepthStencilFormat(DXFormat format) {

	if ((U64)format >= sizeof(DXFormat_toTextureFormatIdArr))
		return EDepthStencilFormat_None;

	U8 i = DXFormat_toTextureFormatIdArr[format];

	if (i < 0xF0)		//Can't convert color format to depth stencil
		return EDepthStencilFormat_None;

	return (EDepthStencilFormat)(i - 0xF0);
}

#ifdef __cplusplus
	}
#endif
