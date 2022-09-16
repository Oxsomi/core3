#pragma once
#include "types/types.h"

enum TexturePrimitive {
	TexturePrimitive_Undefined,
	TexturePrimitive_Unorm,
	TexturePrimitive_Snorm,
	TexturePrimitive_Uint,
	TexturePrimitive_Int,
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
	TextureCompressionType_Unorm,
	TextureCompressionType_Snorm,
	TextureCompressionType_Float,
	TextureCompressionType_sRGB,
	TextureCompressionType_Invalid
};

//Format of a texture
// 
//Structured as the following;
//(in octal): 0'<nibbleSize>'<primitive>'<redBits>'<greenBits>'<blueBits>'<alphaBits>
// 
//Bit count is a multiple of 2 (so shifted by 1)
//Nibble size up to 5 bits (2 octanibbles where the upper bit is unused, shifted by 1).
//  The size of each pixel in nibbles
// 
//If compressed or extended, the get<X>Bits don't have to return a valid number.
//However, it should be non zero if the channel is present, so the has<X> functions are still available.
// 
//Compression used the following;
//(in octal): 0'<uint2Size>'<primitive>'<alignmentX>'<hasRed>'<alignmentY>'<hasGreen>'<type>'<hasBlue>'0'<hasAlpha>
//Type is TextureAlignment
//Spaces is TextureCompressionType
//booleans of >1 are also invalid
//For compression, the nibbleSize is replaced with size in uint2s (8-byte)
//E.g. 0 = 8 byte, 1 = 16 byte
//
enum TextureFormat {

	//8-bit

	TextureFormat_r8				= 0'01'1'04'00'00'00,
	TextureFormat_rg8				= 0'03'1'04'04'00'00,
	TextureFormat_rgba8				= 0'07'1'04'04'04'04,

	TextureFormat_r8s				= 0'01'2'04'00'00'00,
	TextureFormat_rg8s				= 0'03'2'04'04'00'00,
	TextureFormat_rgba8s			= 0'07'2'04'04'04'04,

	TextureFormat_r8u				= 0'00'3'04'00'00'00,
	TextureFormat_rg8u				= 0'01'3'04'04'00'00,
	TextureFormat_rgba8u			= 0'03'3'04'04'04'04,

	TextureFormat_r8i				= 0'00'4'04'00'00'00,
	TextureFormat_rg8i				= 0'01'4'04'04'00'00,
	TextureFormat_rgba8i			= 0'03'4'04'04'04'04,

	//16-bit

	TextureFormat_r16				= 0'03'1'10'00'00'00,
	TextureFormat_rg16				= 0'07'1'10'10'00'00,
	TextureFormat_rgba16			= 0'17'1'10'10'10'10,

	TextureFormat_r16s				= 0'03'2'10'00'00'00,
	TextureFormat_rg16s				= 0'07'2'10'10'00'00,
	TextureFormat_rgba16s			= 0'17'2'10'10'10'10,

	TextureFormat_r16u				= 0'03'3'10'00'00'00,
	TextureFormat_rg16u				= 0'07'3'10'10'00'00,
	TextureFormat_rgba16u			= 0'17'3'10'10'10'10,

	TextureFormat_r16i				= 0'03'4'10'00'00'00,
	TextureFormat_rg16i				= 0'07'4'10'10'00'00,
	TextureFormat_rgba16i			= 0'17'4'10'10'10'10,

	TextureFormat_r16f				= 0'03'5'10'00'00'00,
	TextureFormat_rg16f				= 0'07'5'10'10'00'00,
	TextureFormat_rgba16f			= 0'17'5'10'10'10'10,

	//32-bit

	TextureFormat_r32				= 0'07'1'20'00'00'00,
	TextureFormat_rg32				= 0'17'1'20'20'00'00,
	TextureFormat_rgba32			= 0'37'1'20'20'20'20,

	TextureFormat_r32s				= 0'07'2'20'00'00'00,
	TextureFormat_rg32s				= 0'17'2'20'20'00'00,
	TextureFormat_rgba32s			= 0'37'2'20'20'20'20,

	TextureFormat_r32u				= 0'07'3'20'00'00'00,
	TextureFormat_rg32u				= 0'17'3'20'20'00'00,
	TextureFormat_rgba32u			= 0'37'3'20'20'20'20,

	TextureFormat_r32i				= 0'07'4'20'00'00'00,
	TextureFormat_rg32i				= 0'17'4'20'20'00'00,
	TextureFormat_rgba32i			= 0'37'4'20'20'20'20,

	TextureFormat_r32f				= 0'07'5'20'00'00'00,
	TextureFormat_rg32f				= 0'17'5'20'20'00'00,
	TextureFormat_rgba32f			= 0'37'5'20'20'20'20,

	//Special purpose formats

	TextureFormat_rgb10a2			= 0'07'1'05'05'05'01,

	//Compression formats

	//Desktop

	TextureFormat_BC4				= 0'00'6'0'1'0'0'0'0'0'0,
	TextureFormat_BC5				= 0'01'6'0'1'0'1'0'0'0'0,
	TextureFormat_BC6H				= 0'01'6'0'1'0'1'2'1'0'0,
	TextureFormat_BC7				= 0'01'6'0'1'0'1'0'1'0'1,

	TextureFormat_BC4s				= 0'00'6'0'1'0'0'1'0'0'0,
	TextureFormat_BC5s				= 0'01'6'0'1'0'1'1'0'0'0,
	TextureFormat_BC7srgb			= 0'01'6'0'1'0'1'3'1'0'1,

	//Mobile / console

	TextureFormat_ASTC_4x4			= 0'01'6'0'1'0'1'0'1'0'0,
	TextureFormat_ASTC_4x4_sRGB		= 0'01'6'0'1'0'1'3'1'0'0,

	TextureFormat_ASTC_5x4			= 0'01'6'1'1'0'1'0'1'0'0,
	TextureFormat_ASTC_5x4_sRGB		= 0'01'6'1'1'0'1'3'1'0'1,

	TextureFormat_ASTC_5x5			= 0'01'6'1'1'1'1'0'1'0'0,
	TextureFormat_ASTC_5x5_sRGB		= 0'01'6'1'1'1'1'3'1'0'1,

	TextureFormat_ASTC_6x5			= 0'01'6'2'1'1'1'0'1'0'0,
	TextureFormat_ASTC_6x5_sRGB		= 0'01'6'2'1'1'1'3'1'0'1,

	TextureFormat_ASTC_6x6			= 0'01'6'2'1'2'1'0'1'0'0,
	TextureFormat_ASTC_6x6_sRGB		= 0'01'6'2'1'2'1'3'1'0'1,

	TextureFormat_ASTC_8x5			= 0'01'6'3'1'1'1'0'1'0'0,
	TextureFormat_ASTC_8x5_sRGB		= 0'01'6'3'1'1'1'3'1'0'1,

	TextureFormat_ASTC_8x6			= 0'01'6'3'1'2'1'0'1'0'0,
	TextureFormat_ASTC_8x6_sRGB		= 0'01'6'3'1'2'1'3'1'0'1,

	TextureFormat_ASTC_8x8			= 0'01'6'3'1'3'1'0'1'0'0,
	TextureFormat_ASTC_8x8_sRGB		= 0'01'6'3'1'3'1'3'1'0'1,

	TextureFormat_ASTC_10x5			= 0'01'6'4'1'1'1'0'1'0'0,
	TextureFormat_ASTC_10x5_sRGB	= 0'01'6'4'1'1'1'3'1'0'1,

	TextureFormat_ASTC_10x6			= 0'01'6'4'1'2'1'0'1'0'0,
	TextureFormat_ASTC_10x6_sRGB	= 0'01'6'4'1'2'1'3'1'0'1,

	TextureFormat_ASTC_10x8			= 0'01'6'4'1'3'1'0'1'0'0,
	TextureFormat_ASTC_10x8_sRGB	= 0'01'6'4'1'3'1'3'1'0'1,

	TextureFormat_ASTC_10x10		= 0'01'6'4'1'4'1'0'1'0'0,
	TextureFormat_ASTC_10x10_sRGB	= 0'01'6'4'1'4'1'3'1'0'1,

	TextureFormat_ASTC_12x10		= 0'01'6'5'1'4'1'0'1'0'0,
	TextureFormat_ASTC_12x10_sRGB	= 0'01'6'5'1'4'1'3'1'0'1,

	TextureFormat_ASTC_12x12		= 0'01'6'5'1'5'1'0'1'0'0,
	TextureFormat_ASTC_12x12_sRGB	= 0'01'6'5'1'5'1'3'1'0'1
};

inline enum TexturePrimitive TextureFormat_getPrimitive(enum TextureFormat f) { 
	return (enum TexturePrimitive)((f >> 24) & 7); 
}

inline bool TextureFormat_getIsCompressed(enum TextureFormat f) { 
	return TextureFormat_getPrimitive(f) == TexturePrimitive_Compressed; 
}

inline u64 TextureFormat_getBits(enum TextureFormat f) { 
	u64 siz = (f >> 27) + 1;
	return siz << (TextureFormat_getIsCompressed(f) ? 6 : 2);
}

inline u64 TextureFormat_getAlphaBits(enum TextureFormat f) { 
	return TextureFormat_getIsCompressed(f) ? f & 7 : (f & 077) << 1; 
}

inline u64 TextureFormat_getBlueBits(enum TextureFormat f) { 
	return TextureFormat_getIsCompressed(f) ? (f >> 6) & 7 : ((f >> 3) & 077) << 1; 
}

inline u64 TextureFormat_getGreenBits(enum TextureFormat f) { 
	return TextureFormat_getIsCompressed(f) ? (f >> 12) & 7 : ((f >> 6) & 077) << 1; 
}

inline u64 TextureFormat_getRedBits(enum TextureFormat f) { 
	return TextureFormat_getIsCompressed(f) ? (f >> 18) & 7 : ((f >> 9) & 077) << 1; 
}

inline bool TextureFormat_hasRed(enum TextureFormat f) { return TextureFormat_getRedBits(f); }
inline bool TextureFormat_hasGreen(enum TextureFormat f) { return TextureFormat_getGreenBits(f); }
inline bool TextureFormat_hasBlue(enum TextureFormat f) { return TextureFormat_getBlueBits(f); }
inline bool TextureFormat_hasAlpha(enum TextureFormat f) { return TextureFormat_getAlphaBits(f); }

inline u64 TextureFormat_getChannels(enum TextureFormat f) {
	return 
		(u64)TextureFormat_hasRed(f)  + (u64)TextureFormat_hasGreen(f) + 
		(u64)TextureFormat_hasBlue(f) + (u64)TextureFormat_hasAlpha(f);
}

inline enum TextureCompressionType TextureFormat_getCompressionType(enum TextureFormat f) {

	if(!TextureFormat_getIsCompressed(f))
		return TextureCompressionType_Invalid;

	return (enum TextureCompressionType)((f >> 9) & 7);
}

//Get the alignments (x, y) of a texture format
//Returns false if it doesn't need alignment (so alignment = 1x1)

inline bool TextureFormat_getAlignment(enum TextureFormat f, u8 *x, u8 *y) {

	if(!TextureFormat_getIsCompressed(f))
		return false;

	*x = (f >> 15) & 7;
	*y = (f >> 21) & 7;

	return true;
}

//Get texture's size in bytes
//Returns u64_MAX if misaligned (compressed formats)

inline u64 TextureFormat_getSize(enum TextureFormat f, u64 w, u64 h) {

	u8 alignW = 1, alignH = 1;

	//If compressed; the size of the texture format is specified per blocks

	if (TextureFormat_getAlignment(f, &alignW, &alignH)) {

		if(w % alignW || h % alignH)
			return u64_MAX;
		
		w /= alignW;
		h /= alignH;
	}

	return ((w * h * TextureFormat_getBits(f)) + 7) >> 3;
}
