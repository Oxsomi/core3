#pragma once
#include "types/types.h"

enum TexturePrimitive {
	TexturePrimitive_Undefined,
	TexturePrimitive_UNorm,
	TexturePrimitive_SNorm,
	TexturePrimitive_UInt,
	TexturePrimitive_SInt,
	TexturePrimitive_Float,
	TexturePrimitive_Compressed,
	TexturePrimitive_Extended
};

enum TextureAlignment {
	TextureAlignment_4,
	TextureAlignment_5,
	TextureAlignment_6,
	TextureAlignment_8,
	TextureAlignment_10,
	TextureAlignment_12
};

enum TextureCompressionType {
	TextureCompressionType_UNorm,
	TextureCompressionType_SNorm,
	TextureCompressionType_Float,
	TextureCompressionType_sRGB,
	TextureCompressionType_Invalid
};

#define _TextureFormat(primitive, redBits, greenBits, blueBits, alphaBits) \
((alphaBits >> 1) | ((blueBits >> 1) << 6) | ((greenBits >> 1) << 12) | ((redBits >> 1) << 18) | (primitive << 24) | \
(((redBits + greenBits + blueBits + alphaBits) >> 2) - 1) << 27)

#define _TextureFormatCompressed(blockSizeBits, alignmentX, alignmentY, compType, hasRed, hasGreen, hasBlue, hasAlpha) \
((hasAlpha ? 1 : 0) | ((hasBlue ? 1 : 0) << 6) | (compType << 9) | ((hasGreen ? 1 : 0) << 12) | (alignmentY << 16) | \
 ((hasRed ? 1 : 0) << 18) | (alignmentX << 21) | (TexturePrimitive_Compressed << 24) | (((blockSizeBits >> 6) - 1) << 27))

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
//Type is TextureAlignment
//Spaces is TextureCompressionType
//Booleans of >1 are also invalid
//For compression, the nibbleSize is replaced with size in uint2s (8-byte)
//E.g. 0 = 8 byte, 1 = 16 byte
//
enum TextureFormat {

	//8-bit

	TextureFormat_r8				= _TextureFormat(TexturePrimitive_UNorm, 8, 0, 0, 0),
	TextureFormat_rg8				= _TextureFormat(TexturePrimitive_UNorm, 8, 8, 0, 0),
	TextureFormat_rgba8				= _TextureFormat(TexturePrimitive_UNorm, 8, 8, 8, 8),

	TextureFormat_r8s				= _TextureFormat(TexturePrimitive_SNorm, 8, 0, 0, 0),
	TextureFormat_rg8s				= _TextureFormat(TexturePrimitive_SNorm, 8, 8, 0, 0),
	TextureFormat_rgba8s			= _TextureFormat(TexturePrimitive_SNorm, 8, 8, 8, 8),

	TextureFormat_r8u				= _TextureFormat(TexturePrimitive_UInt, 8, 0, 0, 0),
	TextureFormat_rg8u				= _TextureFormat(TexturePrimitive_UInt, 8, 8, 0, 0),
	TextureFormat_rgba8u			= _TextureFormat(TexturePrimitive_UInt, 8, 8, 8, 8),

	TextureFormat_r8i				= _TextureFormat(TexturePrimitive_SInt, 8, 0, 0, 0),
	TextureFormat_rg8i				= _TextureFormat(TexturePrimitive_SInt, 8, 8, 0, 0),
	TextureFormat_rgba8i			= _TextureFormat(TexturePrimitive_SInt, 8, 8, 8, 8),

	//16-bit

	TextureFormat_r16				= _TextureFormat(TexturePrimitive_UNorm, 16, 0, 0, 0),
	TextureFormat_rg16				= _TextureFormat(TexturePrimitive_UNorm, 16, 16, 0, 0),
	TextureFormat_rgba16			= _TextureFormat(TexturePrimitive_UNorm, 16, 16, 16, 16),

	TextureFormat_r16s				= _TextureFormat(TexturePrimitive_SNorm, 16, 0, 0, 0),
	TextureFormat_rg16s				= _TextureFormat(TexturePrimitive_SNorm, 16, 16, 0, 0),
	TextureFormat_rgba16s			= _TextureFormat(TexturePrimitive_SNorm, 16, 16, 16, 16),

	TextureFormat_r16u				= _TextureFormat(TexturePrimitive_UInt, 16, 0, 0, 0),
	TextureFormat_rg16u				= _TextureFormat(TexturePrimitive_UInt, 16, 16, 0, 0),
	TextureFormat_rgba16u			= _TextureFormat(TexturePrimitive_UInt, 16, 16, 16, 16),

	TextureFormat_r16i				= _TextureFormat(TexturePrimitive_SInt, 16, 0, 0, 0),
	TextureFormat_rg16i				= _TextureFormat(TexturePrimitive_SInt, 16, 16, 0, 0),
	TextureFormat_rgba16i			= _TextureFormat(TexturePrimitive_SInt, 16, 16, 16, 16),

	TextureFormat_r16f				= _TextureFormat(TexturePrimitive_Float, 16, 0, 0, 0),
	TextureFormat_rg16f				= _TextureFormat(TexturePrimitive_Float, 16, 16, 0, 0),
	TextureFormat_rgba16f			= _TextureFormat(TexturePrimitive_Float, 16, 16, 16, 16),

	//32-bit

	TextureFormat_r32u				= _TextureFormat(TexturePrimitive_UInt, 32, 0, 0, 0),
	TextureFormat_rg32u				= _TextureFormat(TexturePrimitive_UInt, 32, 32, 0, 0),
	TextureFormat_rgba32u			= _TextureFormat(TexturePrimitive_UInt, 32, 32, 32, 32),

	TextureFormat_r32i				= _TextureFormat(TexturePrimitive_SInt, 32, 0, 0, 0),
	TextureFormat_rg32i				= _TextureFormat(TexturePrimitive_SInt, 32, 32, 0, 0),
	TextureFormat_rgba32i			= _TextureFormat(TexturePrimitive_SInt, 32, 32, 32, 32),

	TextureFormat_r32f				= _TextureFormat(TexturePrimitive_Float, 32, 0, 0, 0),
	TextureFormat_rg32f				= _TextureFormat(TexturePrimitive_Float, 32, 32, 0, 0),
	TextureFormat_rgba32f			= _TextureFormat(TexturePrimitive_Float, 32, 32, 32, 32),

	//Special purpose formats

	TextureFormat_rgb10a2			= _TextureFormat(TexturePrimitive_UNorm, 10, 10, 10, 2),

	//Compression formats

	//Desktop

	TextureFormat_BC4				= _TextureFormatCompressed(
		64, TextureAlignment_4, TextureAlignment_4, 
		TextureCompressionType_UNorm, 1, 0, 0, 0
	),

	TextureFormat_BC5				= _TextureFormatCompressed(
		128, TextureAlignment_4, TextureAlignment_4, 
		TextureCompressionType_UNorm, 1, 1, 0, 0
	),

	TextureFormat_BC6H				= _TextureFormatCompressed(
		128, TextureAlignment_4, TextureAlignment_4, 
		TextureCompressionType_Float, 1, 1, 1, 0
	),

	TextureFormat_BC7				= _TextureFormatCompressed(
		128, TextureAlignment_4, TextureAlignment_4, 
		TextureCompressionType_UNorm, 1, 1, 1, 1
	),

	TextureFormat_BC4s				= _TextureFormatCompressed(
		64, TextureAlignment_4, TextureAlignment_4, 
		TextureCompressionType_SNorm, 1, 0, 0, 0
	),

	TextureFormat_BC5s				= _TextureFormatCompressed(
		128, TextureAlignment_4, TextureAlignment_4, 
		TextureCompressionType_SNorm, 1, 1, 0, 0
	),

	TextureFormat_BC7srgb			= _TextureFormatCompressed(
		128, TextureAlignment_4, TextureAlignment_4, 
		TextureCompressionType_sRGB, 1, 1, 1, 1
	),

	//Mobile / console

	TextureFormat_ASTC_4x4			= _TextureFormatCompressed(
		128, TextureAlignment_4, TextureAlignment_4, 
		TextureCompressionType_UNorm, 1, 1, 1, 1
	),

	TextureFormat_ASTC_4x4_sRGB		= _TextureFormatCompressed(
		128, TextureAlignment_4, TextureAlignment_4, 
		TextureCompressionType_sRGB, 1, 1, 1, 1
	),


	TextureFormat_ASTC_5x4			= _TextureFormatCompressed(
		128, TextureAlignment_5, TextureAlignment_4, 
		TextureCompressionType_UNorm, 1, 1, 1, 1
	),

	TextureFormat_ASTC_5x4_sRGB		= _TextureFormatCompressed(
		128, TextureAlignment_5, TextureAlignment_4, 
		TextureCompressionType_sRGB, 1, 1, 1, 1
	),


	TextureFormat_ASTC_5x5			= _TextureFormatCompressed(
		128, TextureAlignment_5, TextureAlignment_5, 
		TextureCompressionType_UNorm, 1, 1, 1, 1
	),

	TextureFormat_ASTC_5x5_sRGB		= _TextureFormatCompressed(
		128, TextureAlignment_5, TextureAlignment_5, 
		TextureCompressionType_sRGB, 1, 1, 1, 1
	),


	TextureFormat_ASTC_6x5			= _TextureFormatCompressed(
		128, TextureAlignment_6, TextureAlignment_5, 
		TextureCompressionType_UNorm, 1, 1, 1, 1
	),

	TextureFormat_ASTC_6x5_sRGB		= _TextureFormatCompressed(
		128, TextureAlignment_6, TextureAlignment_5, 
		TextureCompressionType_sRGB, 1, 1, 1, 1
	),


	TextureFormat_ASTC_6x6			= _TextureFormatCompressed(
		128, TextureAlignment_6, TextureAlignment_6, 
		TextureCompressionType_UNorm, 1, 1, 1, 1
	),

	TextureFormat_ASTC_6x6_sRGB		= _TextureFormatCompressed(
		128, TextureAlignment_6, TextureAlignment_6, 
		TextureCompressionType_sRGB, 1, 1, 1, 1
	),


	TextureFormat_ASTC_8x5			= _TextureFormatCompressed(
		128, TextureAlignment_8, TextureAlignment_5, 
		TextureCompressionType_UNorm, 1, 1, 1, 1
	),

	TextureFormat_ASTC_8x5_sRGB		= _TextureFormatCompressed(
		128, TextureAlignment_8, TextureAlignment_5, 
		TextureCompressionType_sRGB, 1, 1, 1, 1
	),


	TextureFormat_ASTC_8x6			= _TextureFormatCompressed(
		128, TextureAlignment_8, TextureAlignment_6, 
		TextureCompressionType_UNorm, 1, 1, 1, 1
	),

	TextureFormat_ASTC_8x6_sRGB		= _TextureFormatCompressed(
		128, TextureAlignment_8, TextureAlignment_6, 
		TextureCompressionType_sRGB, 1, 1, 1, 1
	),


	TextureFormat_ASTC_8x8			= _TextureFormatCompressed(
		128, TextureAlignment_8, TextureAlignment_8, 
		TextureCompressionType_UNorm, 1, 1, 1, 1
	),

	TextureFormat_ASTC_8x8_sRGB		= _TextureFormatCompressed(
		128, TextureAlignment_8, TextureAlignment_8, 
		TextureCompressionType_sRGB, 1, 1, 1, 1
	),


	TextureFormat_ASTC_10x5			= _TextureFormatCompressed(
		128, TextureAlignment_10, TextureAlignment_5, 
		TextureCompressionType_UNorm, 1, 1, 1, 1
	),

	TextureFormat_ASTC_10x5_sRGB	= _TextureFormatCompressed(
		128, TextureAlignment_10, TextureAlignment_5, 
		TextureCompressionType_sRGB, 1, 1, 1, 1
	),


	TextureFormat_ASTC_10x6			= _TextureFormatCompressed(
		128, TextureAlignment_10, TextureAlignment_6, 
		TextureCompressionType_UNorm, 1, 1, 1, 1
	),

	TextureFormat_ASTC_10x6_sRGB	= _TextureFormatCompressed(
		128, TextureAlignment_10, TextureAlignment_6, 
		TextureCompressionType_sRGB, 1, 1, 1, 1
	),


	TextureFormat_ASTC_10x8			= _TextureFormatCompressed(
		128, TextureAlignment_10, TextureAlignment_8, 
		TextureCompressionType_UNorm, 1, 1, 1, 1
	),

	TextureFormat_ASTC_10x8_sRGB	= _TextureFormatCompressed(
		128, TextureAlignment_10, TextureAlignment_8, 
		TextureCompressionType_sRGB, 1, 1, 1, 1
	),


	TextureFormat_ASTC_10x10		= _TextureFormatCompressed(
		128, TextureAlignment_10, TextureAlignment_10, 
		TextureCompressionType_UNorm, 1, 1, 1, 1
	),

	TextureFormat_ASTC_10x10_sRGB	= _TextureFormatCompressed(
		128, TextureAlignment_10, TextureAlignment_10, 
		TextureCompressionType_sRGB, 1, 1, 1, 1
	),


	TextureFormat_ASTC_12x10		= _TextureFormatCompressed(
		128, TextureAlignment_12, TextureAlignment_10, 
		TextureCompressionType_UNorm, 1, 1, 1, 1
	),

	TextureFormat_ASTC_12x10_sRGB	= _TextureFormatCompressed(
		128, TextureAlignment_12, TextureAlignment_10, 
		TextureCompressionType_sRGB, 1, 1, 1, 1
	),


	TextureFormat_ASTC_12x12		= _TextureFormatCompressed(
		128, TextureAlignment_12, TextureAlignment_12, 
		TextureCompressionType_UNorm, 1, 1, 1, 1
	),

	TextureFormat_ASTC_12x12_sRGB	= _TextureFormatCompressed(
		128, TextureAlignment_12, TextureAlignment_12, 
		TextureCompressionType_sRGB, 1, 1, 1, 1
	),
};

inline enum TexturePrimitive TextureFormat_getPrimitive(enum TextureFormat f) { 
	return (enum TexturePrimitive)((f >> 24) & 7); 
}

inline Bool TextureFormat_getIsCompressed(enum TextureFormat f) { 
	return TextureFormat_getPrimitive(f) == TexturePrimitive_Compressed; 
}

inline U64 TextureFormat_getBits(enum TextureFormat f) { 
	U64 siz = (f >> 27) + 1;
	return siz << (TextureFormat_getIsCompressed(f) ? 6 : 2);
}

inline U64 TextureFormat_getAlphaBits(enum TextureFormat f) { 
	return TextureFormat_getIsCompressed(f) ? f & 7 : (f & 077) << 1; 
}

inline U64 TextureFormat_getBlueBits(enum TextureFormat f) { 
	return TextureFormat_getIsCompressed(f) ? (f >> 6) & 7 : ((f >> 3) & 077) << 1; 
}

inline U64 TextureFormat_getGreenBits(enum TextureFormat f) { 
	return TextureFormat_getIsCompressed(f) ? (f >> 12) & 7 : ((f >> 6) & 077) << 1; 
}

inline U64 TextureFormat_getRedBits(enum TextureFormat f) { 
	return TextureFormat_getIsCompressed(f) ? (f >> 18) & 7 : ((f >> 9) & 077) << 1; 
}

inline Bool TextureFormat_hasRed(enum TextureFormat f) { return TextureFormat_getRedBits(f); }
inline Bool TextureFormat_hasGreen(enum TextureFormat f) { return TextureFormat_getGreenBits(f); }
inline Bool TextureFormat_hasBlue(enum TextureFormat f) { return TextureFormat_getBlueBits(f); }
inline Bool TextureFormat_hasAlpha(enum TextureFormat f) { return TextureFormat_getAlphaBits(f); }

inline U8 TextureFormat_getChannels(enum TextureFormat f) {
	return 
		(U8)TextureFormat_hasRed(f)  + (U8)TextureFormat_hasGreen(f) + 
		(U8)TextureFormat_hasBlue(f) + (U8)TextureFormat_hasAlpha(f);
}

inline enum TextureCompressionType TextureFormat_getCompressionType(enum TextureFormat f) {

	if(!TextureFormat_getIsCompressed(f))
		return TextureCompressionType_Invalid;

	return (enum TextureCompressionType)((f >> 9) & 7);
}

//Get the alignments (x, y) of a texture format
//Returns false if it doesn't need alignment (so alignment = 1x1)

inline Bool TextureFormat_getAlignment(enum TextureFormat f, U8 *x, U8 *y) {

	if(!TextureFormat_getIsCompressed(f))
		return false;

	*x = (f >> 15) & 7;
	*y = (f >> 21) & 7;

	return true;
}

//Get texture's size in bytes
//Returns U64_MAX if misaligned (compressed formats)

inline U64 TextureFormat_getSize(enum TextureFormat f, U64 w, U64 h) {

	U8 alignW = 1, alignH = 1;

	//If compressed; the size of the texture format is specified per blocks

	if (TextureFormat_getAlignment(f, &alignW, &alignH)) {

		if(w % alignW || h % alignH)
			return U64_MAX;
		
		w /= alignW;
		h /= alignH;
	}

	return ((w * h * TextureFormat_getBits(f)) + 7) >> 3;
}
