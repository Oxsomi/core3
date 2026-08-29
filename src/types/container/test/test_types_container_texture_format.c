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
