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

//types/container/test/test_types_container_texture_format.c

#include "test_types_container_shared.h"
#include "types/container/texture_format.h"
#include "types/math/type_cast.h"
#include "types/base/mathf.h"

void Test_textureFormat(Test *t) {

	Test_setModule(t, "ETextureFormat DXFormat");

	DXFormat dx = ETextureFormatId_toDXFormat(ETextureFormatId_RGBA8);
	Test_assert(t, "RGBA8 DXFormat mapping",         dx == 28);
	Test_assert(t, "RGBA8 DXFormat reverse mapping", DXFormat_toTextureFormatId(dx) == ETextureFormatId_RGBA8);

	dx = ETextureFormatId_toDXFormat(ETextureFormatId_BC7);
	Test_assert(t, "BC7 DXFormat mapping",           dx == 98);
	Test_assert(t, "BC7 DXFormat reverse mapping",   DXFormat_toTextureFormatId(dx) == ETextureFormatId_BC7);

	Test_setModule(t, "ETextureFormat Unpack");

	Test_assert(t, "RGBA8 unpack",      ETextureFormatId_unpack[ETextureFormatId_RGBA8]      == ETextureFormat_RGBA8);
	Test_assert(t, "BC7 unpack",        ETextureFormatId_unpack[ETextureFormatId_BC7]         == ETextureFormat_BC7);
	Test_assert(t, "ASTC_10x10 unpack", ETextureFormatId_unpack[ETextureFormatId_ASTC_10x10] == ETextureFormat_ASTC_10x10);

	Test_setModule(t, "ETextureFormat RGBA8");

	ETextureFormat fmt = ETextureFormat_RGBA8;

	Test_assert(t, "RGBA8 primitive",     ETextureFormat_getPrimitive(fmt) == ETexturePrimitive_UNorm);
	Test_assert(t, "RGBA8 R bits",        ETextureFormat_getRedBits(fmt)   == 8);
	Test_assert(t, "RGBA8 G bits",        ETextureFormat_getGreenBits(fmt) == 8);
	Test_assert(t, "RGBA8 B bits",        ETextureFormat_getBlueBits(fmt)  == 8);
	Test_assert(t, "RGBA8 A bits",        ETextureFormat_getAlphaBits(fmt) == 8);
	Test_assert(t, "RGBA8 not compressed",!ETextureFormat_getIsCompressed(fmt));
	Test_assert(t, "RGBA8 channels",      ETextureFormat_getChannels(fmt)  == 4);
	Test_assert(t, "RGBA8 size 1x1x1",    ETextureFormat_getSize(fmt, 1, 1, 1) == 4);

	//Compressed BCn format

	Test_setModule(t, "ETextureFormat/BC7");

	fmt = ETextureFormat_BC7;

	Test_assert(t, "BC7 primitive",      ETextureFormat_getPrimitive(fmt)        == ETexturePrimitive_Compressed);
	Test_assert(t, "BC7 compress type",  ETextureFormat_getCompressionType(fmt)  == ETextureCompressionType_UNorm);
	Test_assert(t, "BC7 compress algo",  ETextureFormat_getCompressionAlgo(fmt)  == ETextureCompressionAlgo_BCn);
	Test_assert(t, "BC7 has R",          ETextureFormat_hasRed(fmt));
	Test_assert(t, "BC7 has G",          ETextureFormat_hasGreen(fmt));
	Test_assert(t, "BC7 has B",          ETextureFormat_hasBlue(fmt));
	Test_assert(t, "BC7 has A",          ETextureFormat_hasAlpha(fmt));
	Test_assert(t, "BC7 channels",       ETextureFormat_getChannels(fmt) == 4);

	U8 alignX = 0, alignY = 0;
	Test_assert(t, "BC7 alignment query", ETextureFormat_getAlignment(fmt, &alignX, &alignY));
	Test_assert(t, "BC7 alignX",          alignX == 4);
	Test_assert(t, "BC7 alignY",          alignY == 4);

	U64 expectedSize = (((8 + alignX - 1) / alignX) * ((8 + alignY - 1) / alignY) * ETextureFormat_getBits(fmt) + 7) >> 3;
	Test_assert(t, "BC7 size 8x8x1",      ETextureFormat_getSize(fmt, 8, 8, 1) == expectedSize);

	//Compressed ASTC format

	Test_setModule(t, "ETextureFormat/ASTC_8x8_sRGB");

	fmt = ETextureFormat_ASTC_8x8_sRGB;

	Test_assert(t, "ASTC 8x8 sRGB primitive",     ETextureFormat_getPrimitive(fmt)       == ETexturePrimitive_Compressed);
	Test_assert(t, "ASTC 8x8 sRGB algo",          ETextureFormat_getCompressionAlgo(fmt) == ETextureCompressionAlgo_ASTC);
	Test_assert(t, "ASTC 8x8 sRGB compress type", ETextureFormat_getCompressionType(fmt) == ETextureCompressionType_sRGB);
	Test_assert(t, "ASTC 8x8 sRGB has R",         ETextureFormat_hasRed(fmt));
	Test_assert(t, "ASTC 8x8 sRGB has G",         ETextureFormat_hasGreen(fmt));
	Test_assert(t, "ASTC 8x8 sRGB has B",         ETextureFormat_hasBlue(fmt));
	Test_assert(t, "ASTC 8x8 sRGB has A",         ETextureFormat_hasAlpha(fmt));

	Test_assert(t, "ASTC 8x8 sRGB alignment query", ETextureFormat_getAlignment(fmt, &alignX, &alignY));
	Test_assert(t, "ASTC 8x8 sRGB alignX",          alignX == 8);
	Test_assert(t, "ASTC 8x8 sRGB alignY",          alignY == 8);

	expectedSize = (((16 + alignX - 1) / alignX) * ((16 + alignY - 1) / alignY) * ETextureFormat_getBits(fmt) + 7) >> 3;
	Test_assert(t, "ASTC 8x8 sRGB size 16x16x1", ETextureFormat_getSize(fmt, 16, 16, 1) == expectedSize);

	//Depth/stencil formats

	Test_setModule(t, "EDepthStencilFormat");

	static const EDepthStencilFormat depthFormats[] = {
		EDepthStencilFormat_D16,
		EDepthStencilFormat_D32,
		EDepthStencilFormat_D24S8Ext,
		EDepthStencilFormat_D32S8X24Ext
	};

	for (U32 i = 0; i < (U32)(sizeof(depthFormats) / sizeof(depthFormats[0])); ++i) {
		const EDepthStencilFormat f  = depthFormats[i];
		dx = EDepthStencilFormat_toDXFormat(f);
		Test_assert(t, "Depth DXFormat mapping",         dx != 0);
		Test_assert(t, "Depth DXFormat reverse mapping", DXFormat_toDepthStencilFormat(dx) == f);
	}

	Test_assert(t, "D24S8 DXFormat value", EDepthStencilFormat_toDXFormat(EDepthStencilFormat_D24S8Ext) == 45);
	Test_assert(t, "D32 DXFormat value",   EDepthStencilFormat_toDXFormat(EDepthStencilFormat_D32)      == 40);

	//RGB9E5 is the one format the encoding can't describe literally: 9 bit channels don't survive the >>1
	// storage and the exponent rides in the alpha field, so the per channel accessors special case it.
	//What is pinned here is that everything SIZE related stays exact anyway, since the size field holds the
	// sum of the four counts rather than any single one, and that the odd width case doesn't round; getting
	// that wrong is what ruled out encoding this as a block format.

	Test_setModule(t, "ETextureFormat/RGB9E5");

	{
		const ETextureFormat f = ETextureFormat_RGB9E5;

		Test_assert(t, "RGB9E5 bits",           ETextureFormat_getBits(f) == 32);
		Test_assert(t, "RGB9E5 size",           ETextureFormat_getSize(f, 4096, 2048, 1) == (U64) 4096 * 2048 * 4);
		Test_assert(t, "RGB9E5 odd width size", ETextureFormat_getSize(f, 4097, 1, 1) == (U64) 4097 * 4);
		Test_assert(t, "RGB9E5 primitive",      ETextureFormat_getPrimitive(f) == ETexturePrimitive_Float);
		Test_assert(t, "RGB9E5 not compressed", !ETextureFormat_getIsCompressed(f));

		Test_assert(t, "RGB9E5 exponential",    ETextureFormat_isExponentialEncode(f));
		Test_assert(t, "RGB9E5 exponent bits",  ETextureFormat_getExponentBits(f) == 5);
		Test_assert(t, "RGB9E5 R bits",         ETextureFormat_getRedBits(f) == 9);
		Test_assert(t, "RGB9E5 G bits",         ETextureFormat_getGreenBits(f) == 9);
		Test_assert(t, "RGB9E5 B bits",         ETextureFormat_getBlueBits(f) == 9);
		Test_assert(t, "RGB9E5 no alpha",       !ETextureFormat_getAlphaBits(f));
		Test_assert(t, "RGB9E5 channels",       ETextureFormat_getChannels(f) == 3);

		Test_assert(t, "RGB9E5 id unpack",      ETextureFormatId_unpack[ETextureFormatId_RGB9E5] == f);

		//The encoding is only safe while nothing else lands on the same value.

		U64 collisions = 0;

		for(U64 i = 0; i < ETextureFormatId_Count; ++i)
			if(i != ETextureFormatId_RGB9E5 && ETextureFormatId_unpack[i] == f)
				++collisions;

		Test_assert(t, "RGB9E5 encoding unique", !collisions);

		//Nothing else may claim to be exponentially encoded, or the accessor special cases above would
		// start rewriting formats they don't describe.

		U64 exponential = 0;

		for(U64 i = 0; i < ETextureFormatId_Count; ++i)
			if(ETextureFormat_isExponentialEncode(ETextureFormatId_unpack[i]))
				++exponential;

		Test_assert(t, "RGB9E5 sole exponential", exponential == 1);
	}
}

typedef struct CodecCase {
	ETextureFormatId format;
	const C8 *name;
	U8 count;
	F32 value[4];
} CodecCase;

static const CodecCase codecCases[] = {

	//SIGNED integers, which are what a sign extension bug hides in: the stored bits are two's complement and
	// a decode that reads them as unsigned turns every negative into a large positive.

	{ ETextureFormatId_R8i,     "R8i",     1, { -5, 0, 0, 0 } },
	{ ETextureFormatId_R8i,     "R8i min", 1, { -128, 0, 0, 0 } },
	{ ETextureFormatId_RG16i,   "RG16i",   2, { -32768, 32767, 0, 0 } },
	{ ETextureFormatId_RGBA32i, "RGBA32i", 4, { -8388608, -1, 0, 8388607 } },

	//UNSIGNED integers, so the same arm with no extension at all

	{ ETextureFormatId_R8u,     "R8u",     1, { 255, 0, 0, 0 } },
	{ ETextureFormatId_RG16u,   "RG16u",   2, { 0, 65535, 0, 0 } },
	{ ETextureFormatId_RGBA32u, "RGBA32u", 4, { 0, 1, 16777215, 42 } },

	//Floats, exact at both widths for values an F16 holds

	{ ETextureFormatId_R16f,    "R16f",    1, { 0.5f, 0, 0, 0 } },
	{ ETextureFormatId_RGBA32f, "RGBA32f", 4, { -1.25f, 0, 1e18f, 3.5f } }
};

static void Test_textureCodecRoundTrip(Test *t) {

	Test_setModule(t, "texture/codec round trip");

	for(U64 i = 0; i < sizeof(codecCases) / sizeof(codecCases[0]); ++i) {

		const CodecCase c = codecCases[i];

		U8 record[16] = { 0 };
		F32 back[4] = { 0, 0, 0, 0 };

		ETextureFormatId_encode(record, c.format, c.value, c.count);
		ETextureFormatId_decode(record, c.format, back, c.count);

		Bool same = true;

		for(U8 k = 0; k < c.count; ++k)
			same &= back[k] == c.value[k];

		Test_assert(t, c.name, same);
	}
}

static void Test_textureCodecNormalized(Test *t) {

	Test_setModule(t, "texture/codec normalized");

	U8 record[16] = { 0 };
	F32 back[4] = { 0, 0, 0, 0 };

	const F32 snorm[4] = { -1, -0.5f, 0, 1 };
	ETextureFormatId_encode(record, ETextureFormatId_RGBA8s, snorm, 4);
	ETextureFormatId_decode(record, ETextureFormatId_RGBA8s, back, 4);

	Bool near = true;

	for(U8 k = 0; k < 4; ++k)
		near &= F32_abs(back[k] - snorm[k]) < 0.01f;

	Test_assert(t, "RGBA8s round trip", near);
	Test_assert(t, "SNorm endpoints exact", back[0] == -1 && back[3] == 1);

	const F32 unorm[4] = { 0, 0.25f, 0.75f, 1 };
	ETextureFormatId_encode(record, ETextureFormatId_RGBA8, unorm, 4);
	ETextureFormatId_decode(record, ETextureFormatId_RGBA8, back, 4);

	near = true;

	for(U8 k = 0; k < 4; ++k)
		near &= F32_abs(back[k] - unorm[k]) < 0.01f;

	Test_assert(t, "RGBA8 round trip", near);
	Test_assert(t, "UNorm endpoints exact", back[0] == 0 && back[3] == 1);
}

static void Test_textureCodecRange(Test *t) {

	Test_setModule(t, "texture/codec range");

	U8 record[16] = { 0 };
	F32 back[4] = { 0, 0, 0, 0 };

	const F32 overSigned[2] = { 1e9f, -1e9f };
	ETextureFormatId_encode(record, ETextureFormatId_RG16i, overSigned, 2);
	ETextureFormatId_decode(record, ETextureFormatId_RG16i, back, 2);

	Test_assert(t, "SInt saturates high", back[0] == 32767);
	Test_assert(t, "SInt saturates low", back[1] == -32768);

	const F32 overUnsigned[2] = { 1e9f, -1e9f };
	ETextureFormatId_encode(record, ETextureFormatId_RG16u, overUnsigned, 2);
	ETextureFormatId_decode(record, ETextureFormatId_RG16u, back, 2);

	Test_assert(t, "UInt saturates high", back[0] == 65535);
	Test_assert(t, "UInt saturates low", back[1] == 0);

	//A NaN has no integer to convert to and converting one is undefined, so every arm but the float one
	// treats it as zero rather than as whatever the hardware happened to produce.

	const F32 q = F32_fromU32Bits(0x7FC00000);
	const F32 nan[4] = { q, q, q, q };

	ETextureFormatId_encode(record, ETextureFormatId_RGBA8, nan, 4);
	ETextureFormatId_decode(record, ETextureFormatId_RGBA8, back, 4);
	Test_assert(t, "UNorm NaN is zero", !back[0] && !back[1] && !back[2] && !back[3]);

	ETextureFormatId_encode(record, ETextureFormatId_RGBA32i, nan, 4);
	ETextureFormatId_decode(record, ETextureFormatId_RGBA32i, back, 4);
	Test_assert(t, "SInt NaN is zero", !back[0] && !back[1] && !back[2] && !back[3]);
}

static void Test_textureCodecComponents(Test *t) {

	Test_setModule(t, "texture/codec components");

	U8 record[16] = { 0xFF };
	F32 back[4] = { 1, 1, 1, 1 };

	for(U8 i = 0; i < 16; ++i)
		record[i] = 0xFF;

	const F32 three[3] = { 1, 2, 3 };
	ETextureFormatId_encode(record, ETextureFormatId_RGBA32i, three, 3);
	ETextureFormatId_decode(record, ETextureFormatId_RGBA32i, back, 4);

	Test_assert(t, "supplied components kept", back[0] == 1 && back[1] == 2 && back[2] == 3);
	Test_assert(t, "unsupplied component is zero", back[3] == 0);

	const F32 four[4] = { 7, 8, 9, 10 };
	F32 two[2] = { 0, 0 };

	ETextureFormatId_encode(record, ETextureFormatId_RG16i, four, 4);
	ETextureFormatId_decode(record, ETextureFormatId_RG16i, two, 2);

	Test_assert(t, "components past the format dropped", two[0] == 7 && two[1] == 8);

	//A decode asked for more than the format holds zeroes what it cannot fill rather than reading past it.

	for(U8 i = 0; i < 4; ++i)
		back[i] = 1;

	ETextureFormatId_decode(record, ETextureFormatId_RG16i, back, 4);
	Test_assert(t, "decode past the format is zero", back[2] == 0 && back[3] == 0);
}

//The texel codec: one round trip per primitive at every width, since that is what it is written over.

void Test_textureCodec(Test *t) {
	Test_textureCodecRoundTrip(t);
	Test_textureCodecNormalized(t);
	Test_textureCodecRange(t);
	Test_textureCodecComponents(t);
}
