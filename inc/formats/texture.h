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
	ETexturePrimitive_Extended
} ETexturePrimitive;

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

#define _ETextureFormat(primitive, redBits, greenBits, blueBits, alphaBits) \
((alphaBits >> 1) | ((blueBits >> 1) << 6) | ((greenBits >> 1) << 12) | ((redBits >> 1) << 18) | (primitive << 24) | \
(((redBits + greenBits + blueBits + alphaBits) >> 2) - 1) << 27)

#define _ETextureFormatCompressed(blockSizeBits, alignmentX, alignmentY, compType, hasRed, hasGreen, hasBlue, hasAlpha) \
((hasAlpha ? 1 : 0) | ((hasBlue ? 1 : 0) << 6) | (compType << 9) | ((hasGreen ? 1 : 0) << 12) | (alignmentY << 16) | \
 ((hasRed ? 1 : 0) << 18) | (alignmentX << 21) | (ETexturePrimitive_Compressed << 24) | (((blockSizeBits >> 6) - 1) << 27))

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
//(in octal): 0'<uint2Size>'<primitive>'<alignmentX>'<hasRed>'<alignmentY>'<hasGreen>'<type>'<hasBlue>'0'<hasAlpha>
//Type is ETextureAlignment
//Spaces is ETextureCompressionType
//Booleans of >1 are also invalid
//For compression, the nibbleSize is replaced with size in uint2s (8-byte)
//E.g. 0 = 8 byte, 1 = 16 byte
//
typedef enum ETextureFormat {

	//8-bit

	ETextureFormat_r8				= _ETextureFormat(ETexturePrimitive_UNorm, 8, 0, 0, 0),
	ETextureFormat_rg8				= _ETextureFormat(ETexturePrimitive_UNorm, 8, 8, 0, 0),
	ETextureFormat_rgba8				= _ETextureFormat(ETexturePrimitive_UNorm, 8, 8, 8, 8),

	ETextureFormat_r8s				= _ETextureFormat(ETexturePrimitive_SNorm, 8, 0, 0, 0),
	ETextureFormat_rg8s				= _ETextureFormat(ETexturePrimitive_SNorm, 8, 8, 0, 0),
	ETextureFormat_rgba8s			= _ETextureFormat(ETexturePrimitive_SNorm, 8, 8, 8, 8),

	ETextureFormat_r8u				= _ETextureFormat(ETexturePrimitive_UInt, 8, 0, 0, 0),
	ETextureFormat_rg8u				= _ETextureFormat(ETexturePrimitive_UInt, 8, 8, 0, 0),
	ETextureFormat_rgba8u			= _ETextureFormat(ETexturePrimitive_UInt, 8, 8, 8, 8),

	ETextureFormat_r8i				= _ETextureFormat(ETexturePrimitive_SInt, 8, 0, 0, 0),
	ETextureFormat_rg8i				= _ETextureFormat(ETexturePrimitive_SInt, 8, 8, 0, 0),
	ETextureFormat_rgba8i			= _ETextureFormat(ETexturePrimitive_SInt, 8, 8, 8, 8),

	//16-bit

	ETextureFormat_r16				= _ETextureFormat(ETexturePrimitive_UNorm, 16, 0, 0, 0),
	ETextureFormat_rg16				= _ETextureFormat(ETexturePrimitive_UNorm, 16, 16, 0, 0),
	ETextureFormat_rgba16			= _ETextureFormat(ETexturePrimitive_UNorm, 16, 16, 16, 16),

	ETextureFormat_r16s				= _ETextureFormat(ETexturePrimitive_SNorm, 16, 0, 0, 0),
	ETextureFormat_rg16s				= _ETextureFormat(ETexturePrimitive_SNorm, 16, 16, 0, 0),
	ETextureFormat_rgba16s			= _ETextureFormat(ETexturePrimitive_SNorm, 16, 16, 16, 16),

	ETextureFormat_r16u				= _ETextureFormat(ETexturePrimitive_UInt, 16, 0, 0, 0),
	ETextureFormat_rg16u				= _ETextureFormat(ETexturePrimitive_UInt, 16, 16, 0, 0),
	ETextureFormat_rgba16u			= _ETextureFormat(ETexturePrimitive_UInt, 16, 16, 16, 16),

	ETextureFormat_r16i				= _ETextureFormat(ETexturePrimitive_SInt, 16, 0, 0, 0),
	ETextureFormat_rg16i				= _ETextureFormat(ETexturePrimitive_SInt, 16, 16, 0, 0),
	ETextureFormat_rgba16i			= _ETextureFormat(ETexturePrimitive_SInt, 16, 16, 16, 16),

	ETextureFormat_r16f				= _ETextureFormat(ETexturePrimitive_Float, 16, 0, 0, 0),
	ETextureFormat_rg16f				= _ETextureFormat(ETexturePrimitive_Float, 16, 16, 0, 0),
	ETextureFormat_rgba16f			= _ETextureFormat(ETexturePrimitive_Float, 16, 16, 16, 16),

	//32-bit

	ETextureFormat_r32u				= _ETextureFormat(ETexturePrimitive_UInt, 32, 0, 0, 0),
	ETextureFormat_rg32u				= _ETextureFormat(ETexturePrimitive_UInt, 32, 32, 0, 0),
	ETextureFormat_rgba32u			= _ETextureFormat(ETexturePrimitive_UInt, 32, 32, 32, 32),

	ETextureFormat_r32i				= _ETextureFormat(ETexturePrimitive_SInt, 32, 0, 0, 0),
	ETextureFormat_rg32i				= _ETextureFormat(ETexturePrimitive_SInt, 32, 32, 0, 0),
	ETextureFormat_rgba32i			= _ETextureFormat(ETexturePrimitive_SInt, 32, 32, 32, 32),

	ETextureFormat_r32f				= _ETextureFormat(ETexturePrimitive_Float, 32, 0, 0, 0),
	ETextureFormat_rg32f				= _ETextureFormat(ETexturePrimitive_Float, 32, 32, 0, 0),
	ETextureFormat_rgba32f			= _ETextureFormat(ETexturePrimitive_Float, 32, 32, 32, 32),

	//Special purpose formats

	ETextureFormat_rgb10a2			= _ETextureFormat(ETexturePrimitive_UNorm, 10, 10, 10, 2),

	//Compression formats

	//Desktop

	ETextureFormat_BC4				= _ETextureFormatCompressed(
		64, ETextureAlignment_4, ETextureAlignment_4, 
		ETextureCompressionType_UNorm, 1, 0, 0, 0
	),

	ETextureFormat_BC5				= _ETextureFormatCompressed(
		128, ETextureAlignment_4, ETextureAlignment_4, 
		ETextureCompressionType_UNorm, 1, 1, 0, 0
	),

	ETextureFormat_BC6H				= _ETextureFormatCompressed(
		128, ETextureAlignment_4, ETextureAlignment_4, 
		ETextureCompressionType_Float, 1, 1, 1, 0
	),

	ETextureFormat_BC7				= _ETextureFormatCompressed(
		128, ETextureAlignment_4, ETextureAlignment_4, 
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_BC4s				= _ETextureFormatCompressed(
		64, ETextureAlignment_4, ETextureAlignment_4, 
		ETextureCompressionType_SNorm, 1, 0, 0, 0
	),

	ETextureFormat_BC5s				= _ETextureFormatCompressed(
		128, ETextureAlignment_4, ETextureAlignment_4, 
		ETextureCompressionType_SNorm, 1, 1, 0, 0
	),

	ETextureFormat_BC7srgb			= _ETextureFormatCompressed(
		128, ETextureAlignment_4, ETextureAlignment_4, 
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),

	//Mobile / console

	ETextureFormat_ASTC_4x4			= _ETextureFormatCompressed(
		128, ETextureAlignment_4, ETextureAlignment_4, 
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_4x4_sRGB		= _ETextureFormatCompressed(
		128, ETextureAlignment_4, ETextureAlignment_4, 
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_5x4			= _ETextureFormatCompressed(
		128, ETextureAlignment_5, ETextureAlignment_4, 
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_5x4_sRGB		= _ETextureFormatCompressed(
		128, ETextureAlignment_5, ETextureAlignment_4, 
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_5x5			= _ETextureFormatCompressed(
		128, ETextureAlignment_5, ETextureAlignment_5, 
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_5x5_sRGB		= _ETextureFormatCompressed(
		128, ETextureAlignment_5, ETextureAlignment_5, 
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_6x5			= _ETextureFormatCompressed(
		128, ETextureAlignment_6, ETextureAlignment_5, 
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_6x5_sRGB		= _ETextureFormatCompressed(
		128, ETextureAlignment_6, ETextureAlignment_5, 
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_6x6			= _ETextureFormatCompressed(
		128, ETextureAlignment_6, ETextureAlignment_6, 
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_6x6_sRGB		= _ETextureFormatCompressed(
		128, ETextureAlignment_6, ETextureAlignment_6, 
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_8x5			= _ETextureFormatCompressed(
		128, ETextureAlignment_8, ETextureAlignment_5, 
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_8x5_sRGB		= _ETextureFormatCompressed(
		128, ETextureAlignment_8, ETextureAlignment_5, 
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_8x6			= _ETextureFormatCompressed(
		128, ETextureAlignment_8, ETextureAlignment_6, 
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_8x6_sRGB		= _ETextureFormatCompressed(
		128, ETextureAlignment_8, ETextureAlignment_6, 
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_8x8			= _ETextureFormatCompressed(
		128, ETextureAlignment_8, ETextureAlignment_8, 
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_8x8_sRGB		= _ETextureFormatCompressed(
		128, ETextureAlignment_8, ETextureAlignment_8, 
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_10x5			= _ETextureFormatCompressed(
		128, ETextureAlignment_10, ETextureAlignment_5, 
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_10x5_sRGB	= _ETextureFormatCompressed(
		128, ETextureAlignment_10, ETextureAlignment_5, 
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_10x6			= _ETextureFormatCompressed(
		128, ETextureAlignment_10, ETextureAlignment_6, 
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_10x6_sRGB	= _ETextureFormatCompressed(
		128, ETextureAlignment_10, ETextureAlignment_6, 
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_10x8			= _ETextureFormatCompressed(
		128, ETextureAlignment_10, ETextureAlignment_8, 
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_10x8_sRGB	= _ETextureFormatCompressed(
		128, ETextureAlignment_10, ETextureAlignment_8, 
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_10x10		= _ETextureFormatCompressed(
		128, ETextureAlignment_10, ETextureAlignment_10, 
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_10x10_sRGB	= _ETextureFormatCompressed(
		128, ETextureAlignment_10, ETextureAlignment_10, 
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_12x10		= _ETextureFormatCompressed(
		128, ETextureAlignment_12, ETextureAlignment_10, 
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_12x10_sRGB	= _ETextureFormatCompressed(
		128, ETextureAlignment_12, ETextureAlignment_10, 
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	),


	ETextureFormat_ASTC_12x12		= _ETextureFormatCompressed(
		128, ETextureAlignment_12, ETextureAlignment_12, 
		ETextureCompressionType_UNorm, 1, 1, 1, 1
	),

	ETextureFormat_ASTC_12x12_sRGB	= _ETextureFormatCompressed(
		128, ETextureAlignment_12, ETextureAlignment_12, 
		ETextureCompressionType_sRGB, 1, 1, 1, 1
	)

} ETextureFormat;

inline ETexturePrimitive ETextureFormat_getPrimitive(ETextureFormat f) { 
	return (ETexturePrimitive)((f >> 24) & 7); 
}

inline Bool ETextureFormat_getIsCompressed(ETextureFormat f) { 
	return ETextureFormat_getPrimitive(f) == ETexturePrimitive_Compressed; 
}

inline U64 ETextureFormat_getBits(ETextureFormat f) { 
	U64 length = (f >> 27) + 1;
	return length << (ETextureFormat_getIsCompressed(f) ? 6 : 2);
}

inline U64 ETextureFormat_getAlphaBits(ETextureFormat f) { 
	return ETextureFormat_getIsCompressed(f) ? f & 7 : (f & 077) << 1; 
}

inline U64 ETextureFormat_getBlueBits(ETextureFormat f) { 
	return ETextureFormat_getIsCompressed(f) ? (f >> 6) & 7 : ((f >> 3) & 077) << 1; 
}

inline U64 ETextureFormat_getGreenBits(ETextureFormat f) { 
	return ETextureFormat_getIsCompressed(f) ? (f >> 12) & 7 : ((f >> 6) & 077) << 1; 
}

inline U64 ETextureFormat_getRedBits(ETextureFormat f) { 
	return ETextureFormat_getIsCompressed(f) ? (f >> 18) & 7 : ((f >> 9) & 077) << 1; 
}

inline Bool ETextureFormat_hasRed(ETextureFormat f) { return ETextureFormat_getRedBits(f); }
inline Bool ETextureFormat_hasGreen(ETextureFormat f) { return ETextureFormat_getGreenBits(f); }
inline Bool ETextureFormat_hasBlue(ETextureFormat f) { return ETextureFormat_getBlueBits(f); }
inline Bool ETextureFormat_hasAlpha(ETextureFormat f) { return ETextureFormat_getAlphaBits(f); }

inline U8 ETextureFormat_getChannels(ETextureFormat f) {
	return 
		(U8)ETextureFormat_hasRed(f)  + (U8)ETextureFormat_hasGreen(f) + 
		(U8)ETextureFormat_hasBlue(f) + (U8)ETextureFormat_hasAlpha(f);
}

inline ETextureCompressionType ETextureFormat_getCompressionType(ETextureFormat f) {

	if(!ETextureFormat_getIsCompressed(f))
		return ETextureCompressionType_Invalid;

	return (ETextureCompressionType)((f >> 9) & 7);
}

//Get the alignments (x, y) of a texture format
//Returns false if it doesn't need alignment (so alignment = 1x1)

inline Bool ETextureFormat_getAlignment(ETextureFormat f, U8 *x, U8 *y) {

	if(!ETextureFormat_getIsCompressed(f))
		return false;

	*x = (f >> 15) & 7;
	*y = (f >> 21) & 7;

	return true;
}

//Get texture's size in bytes
//Returns U64_MAX if misaligned (compressed formats)

inline U64 ETextureFormat_getSize(ETextureFormat f, U32 w, U32 h) {

	U8 alignW = 1, alignH = 1;

	//If compressed; the size of the texture format is specified per blocks

	if (ETextureFormat_getAlignment(f, &alignW, &alignH)) {

		if(w % alignW || h % alignH)
			return U64_MAX;
		
		w /= alignW;
		h /= alignH;
	}

	return (((U64)w * h * ETextureFormat_getBits(f)) + 7) >> 3;
}
