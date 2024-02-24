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

#include "formats/texture.h"

ETexturePrimitive ETextureFormat_getPrimitive(ETextureFormat f) {
	return (ETexturePrimitive)((f >> 24) & 7);
}

Bool ETextureFormat_getIsCompressed(ETextureFormat f) {
	return ETextureFormat_getPrimitive(f) == ETexturePrimitive_Compressed;
}

U64 ETextureFormat_getBits(ETextureFormat f) {
	Bool isCompressed = ETextureFormat_getIsCompressed(f);
	U64 length = ((f >> 27) & (isCompressed ? 7 : 0x1F)) + 1;
	return length << (isCompressed ? 6 : 2);
}

U64 ETextureFormat_getAlphaBits(ETextureFormat f) {
	return ETextureFormat_getIsCompressed(f) ? f & 7 : (f & 077) << 1;
}

U64 ETextureFormat_getBlueBits(ETextureFormat f) {
	return ETextureFormat_getIsCompressed(f) ? (f >> 6) & 7 : ((f >> 3) & 077) << 1;
}

U64 ETextureFormat_getGreenBits(ETextureFormat f) {
	return ETextureFormat_getIsCompressed(f) ? (f >> 12) & 7 : ((f >> 6) & 077) << 1;
}

U64 ETextureFormat_getRedBits(ETextureFormat f) {
	return ETextureFormat_getIsCompressed(f) ? (f >> 18) & 7 : ((f >> 9) & 077) << 1;
}

Bool ETextureFormat_hasRed(ETextureFormat f) { return ETextureFormat_getRedBits(f); }
Bool ETextureFormat_hasGreen(ETextureFormat f) { return ETextureFormat_getGreenBits(f); }
Bool ETextureFormat_hasBlue(ETextureFormat f) { return ETextureFormat_getBlueBits(f); }
Bool ETextureFormat_hasAlpha(ETextureFormat f) { return ETextureFormat_getAlphaBits(f); }

U8 ETextureFormat_getChannels(ETextureFormat f) {
	return
		(U8)ETextureFormat_hasRed(f)  + (U8)ETextureFormat_hasGreen(f) +
		(U8)ETextureFormat_hasBlue(f) + (U8)ETextureFormat_hasAlpha(f);
}

ETextureCompressionType ETextureFormat_getCompressionType(ETextureFormat f) {

	if(!ETextureFormat_getIsCompressed(f))
		return ETextureCompressionType_Invalid;

	return (ETextureCompressionType)((f >> 9) & 7);
}

ETextureCompressionAlgo ETextureFormat_getCompressionAlgo(ETextureFormat f) {

	if(!ETextureFormat_getIsCompressed(f))
		return ETextureCompressionAlgo_None;

	return (ETextureCompressionAlgo)((f >> 30) & 7);
}

Bool ETextureFormat_getAlignment(ETextureFormat f, U8 *x, U8 *y) {

	if(!ETextureFormat_getIsCompressed(f))
		return false;

	if(x)
		*x = ETextureAlignment_toAlignment[(f >> 15) & 7];

	if(y)
		*y = ETextureAlignment_toAlignment[(f >> 21) & 7];

	return true;
}

U64 ETextureFormat_getSize(ETextureFormat f, U32 w, U32 h, U32 l) {

	U8 alignW = 1, alignH = 1;

	//If compressed; the size of the texture format is specified per blocks

	if (ETextureFormat_getAlignment(f, &alignW, &alignH)) {
		w += alignW - 1;	h += alignH - 1;
		w /= alignW;		h /= alignH;
	}

	return ((((U64)w * h * ETextureFormat_getBits(f)) + 7) >> 3) * l;
}

DXFormat ETextureFormatId_toDXFormat(ETextureFormatId format) {

	switch (format) {

		case ETextureFormatId_RGBA32f:		return 2;
		case ETextureFormatId_RGBA32u:		return 3;
		case ETextureFormatId_RGBA32i:		return 4;

		case ETextureFormatId_RGB32f:		return 6;
		case ETextureFormatId_RGB32u:		return 7;
		case ETextureFormatId_RGB32i:		return 8;

		case ETextureFormatId_RGBA16f:		return 10;
		case ETextureFormatId_RGBA16:		return 11;
		case ETextureFormatId_RGBA16u:		return 12;
		case ETextureFormatId_RGBA16s:		return 13;
		case ETextureFormatId_RGBA16i:		return 14;

		case ETextureFormatId_RG32f:		return 16;
		case ETextureFormatId_RG32u:		return 17;
		case ETextureFormatId_RG32i:		return 18;

		case ETextureFormatId_BGR10A2:		return 24;

		case ETextureFormatId_RGBA8:		return 28;
		case ETextureFormatId_RGBA8u:		return 30;
		case ETextureFormatId_RGBA8s:		return 31;
		case ETextureFormatId_RGBA8i:		return 32;

		case ETextureFormatId_RG16f:		return 34;
		case ETextureFormatId_RG16:			return 35;
		case ETextureFormatId_RG16u:		return 36;
		case ETextureFormatId_RG16s:		return 37;
		case ETextureFormatId_RG16i:		return 38;

		case ETextureFormatId_R32f:			return 41;
		case ETextureFormatId_R32u:			return 42;
		case ETextureFormatId_R32i:			return 43;

		case ETextureFormatId_RG8:			return 49;
		case ETextureFormatId_RG8u:			return 50;
		case ETextureFormatId_RG8s:			return 51;
		case ETextureFormatId_RG8i:			return 52;

		case ETextureFormatId_R16f:			return 54;
		case ETextureFormatId_R16:			return 56;
		case ETextureFormatId_R16u:			return 57;
		case ETextureFormatId_R16s:			return 58;
		case ETextureFormatId_R16i:			return 59;

		case ETextureFormatId_R8:			return 61;
		case ETextureFormatId_R8u:			return 62;
		case ETextureFormatId_R8s:			return 63;
		case ETextureFormatId_R8i:			return 64;

		case ETextureFormatId_BC4:			return 80;
		case ETextureFormatId_BC4s:			return 81;

		case ETextureFormatId_BC5:			return 83;
		case ETextureFormatId_BC5s:			return 84;

		case ETextureFormatId_BGRA8:		return 91;

		case ETextureFormatId_BC6H:			return 95;

		case ETextureFormatId_BC7:			return 98;
		case ETextureFormatId_BC7_sRGB:		return 99;

		default:							return 0;
	}
}

ETextureFormatId DXFormat_toTextureFormatId(DXFormat format) {

	switch (format) {

		case 2:								return ETextureFormatId_RGBA32f;
		case 3:								return ETextureFormatId_RGBA32u;
		case 4:								return ETextureFormatId_RGBA32i;

		case 6:								return ETextureFormatId_RGB32f;
		case 7:								return ETextureFormatId_RGB32u;
		case 8:								return ETextureFormatId_RGB32i;

		case 10:							return ETextureFormatId_RGBA16f;
		case 11:							return ETextureFormatId_RGBA16;
		case 12:							return ETextureFormatId_RGBA16u;
		case 13:							return ETextureFormatId_RGBA16s;
		case 14:							return ETextureFormatId_RGBA16i;

		case 16:							return ETextureFormatId_RG32f;
		case 17:							return ETextureFormatId_RG32u;
		case 18:							return ETextureFormatId_RG32i;

		case 24:							return ETextureFormatId_BGR10A2;

		case 28:							return ETextureFormatId_RGBA8;
		case 30:							return ETextureFormatId_RGBA8u;
		case 31:							return ETextureFormatId_RGBA8s;
		case 32:							return ETextureFormatId_RGBA8i;

		case 34:							return ETextureFormatId_RG16f;
		case 35:							return ETextureFormatId_RG16;
		case 36:							return ETextureFormatId_RG16u;
		case 37:							return ETextureFormatId_RG16s;
		case 38:							return ETextureFormatId_RG16i;

		case 41:							return ETextureFormatId_R32f;
		case 42:							return ETextureFormatId_R32u;
		case 43:							return ETextureFormatId_R32i;

		case 49:							return ETextureFormatId_RG8;
		case 50:							return ETextureFormatId_RG8u;
		case 51:							return ETextureFormatId_RG8s;
		case 52:							return ETextureFormatId_RG8i;

		case 54:							return ETextureFormatId_R16f;
		case 56:							return ETextureFormatId_R16;
		case 57:							return ETextureFormatId_R16u;
		case 58:							return ETextureFormatId_R16s;
		case 59:							return ETextureFormatId_R16i;

		case 61:							return ETextureFormatId_R8;
		case 62:							return ETextureFormatId_R8u;
		case 63:							return ETextureFormatId_R8s;
		case 64:							return ETextureFormatId_R8i;

		case 80:							return ETextureFormatId_BC4;
		case 81:							return ETextureFormatId_BC4s;

		case 83:							return ETextureFormatId_BC5;
		case 84:							return ETextureFormatId_BC5s;

		case 91:							return ETextureFormatId_BGRA8;

		case 95:							return ETextureFormatId_BC6H;

		case 98:							return ETextureFormatId_BC7;
		case 99:							return ETextureFormatId_BC7_sRGB;

		default:							return ETextureFormatId_Undefined;
	}
}

EDepthStencilFormat DXFormat_toDepthStencilFormat(DXFormat format) {
	switch (format) {
		case 20:							return EDepthStencilFormat_D32S8;
		case 40:							return EDepthStencilFormat_D32;
		case 45:							return EDepthStencilFormat_D24S8Ext;
		case 55:							return EDepthStencilFormat_D16;
		default:							return EDepthStencilFormat_None;
	}
}

DXFormat EDepthStencilFormat_toDXFormat(EDepthStencilFormat format) {
	switch (format) {
		case EDepthStencilFormat_D32S8:		return 20;
		case EDepthStencilFormat_D32:		return 40;
		case EDepthStencilFormat_D24S8Ext:	return 45;
		case EDepthStencilFormat_D16:		return 55;
		default:							return 0;
	}
}
