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

#include "types/container/texture_format.h"
#include "shared.h"

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
}
