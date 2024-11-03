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

#include "types/container/list_impl.h"
#include "formats/wav/wav.h"
#include "formats/wav/headers.h"
#include "types/math/math.h"

static const U8 WAV_write = 0;

/*

TListImpl(SubResourceData);

ECompareResult SubResourceData_sort(const SubResourceData *a, const SubResourceData *b) {
	return a->layerId > b->layerId || (
		(a->layerId == b->layerId && a->z > b->z) || (
			a->layerId == b->layerId && a->z == b->z && a->mipId > b->mipId
		)
	);
}

Error DDS_write(ListSubResourceData buf, DDSInfo info, Allocator allocator, Buffer *result) {

	if(!result || !buf.length)
		return Error_nullPointer(!result ? 3 : 0, "DDS_write()::buf and result are required");

	if(result->ptr)
		return Error_invalidParameter(3, 0, "DDS_write()::result already contained data, possible memleak");

	if(!info.w || !info.h || !info.l || !info.mips || !info.layers || !info.textureFormatId)
		return Error_invalidParameter(1, 1, "DDS_write()::info.w, h, l, mips, layers and textureFormatId are required");

	if(info.type >= ETextureType_Count || info.textureFormatId >= ETextureFormatId_Count)
		return Error_invalidParameter(1, 1, "DDS_write()::info.type and textureFormatId have to be valid");

	const U32 biggestSize2 = (U32) U64_max(U64_max(info.w, info.h), info.l);
	const U32 mips = (U32) U64_max(1, (U64) F64_ceil(F64_log2((F64)biggestSize2)));

	if(info.mips > mips)
		return Error_invalidParameter(1, 0, "DDS_write()::info.mips out of bounds");

	const U64 totalSubResources = (U64)info.mips * info.l * info.layers;

	if(totalSubResources != buf.length)
		return Error_invalidParameter(0, 0, "DDS_write()::info's subresource count and buf.length mismatched");

	if(info.type == ETextureType_Cube && info.layers != 6)
		return Error_invalidParameter(0, 0, "DDS_write()::info specifies a cubemap, but no 6 faces were found");

	if(info.l > 1 && info.type != ETextureType_3D)
		return Error_invalidParameter(0, 0, "DDS_write()::info specifies length of >1 but ETextureType_3D wasn't specified");

	if(info.layers > 1 && info.type == ETextureType_3D)
		return Error_invalidParameter(0, 0, "DDS_write()::info specifies layers of >1 but ETextureType_3D was used");

	const DXFormat format = ETextureFormatId_toDXFormat(info.textureFormatId);

	if(!format)
		return Error_invalidParameter(0, 0, "DDS_write()::info.textureFormatId isn't supported as a DDS texture");

	const ETextureFormat formatOxC = ETextureFormatId_unpack[info.textureFormatId];
	const Bool isCompressed = ETextureFormat_getIsCompressed(formatOxC);

	//Calculate total size

	Bool requiresDXT10 = info.layers > 1;		//Layers are a DXT10 only feature

	if(!requiresDXT10)
		switch (info.textureFormatId) {

			//These formats are built in without DXT10

			case ETextureFormatId_RGBA8:
			case ETextureFormatId_RG16:
			case ETextureFormatId_BC4:		case ETextureFormatId_BC4s:
			case ETextureFormatId_BC5:		case ETextureFormatId_BC5s:
			case ETextureFormatId_RGBA16:	case ETextureFormatId_RGBA16s:
			case ETextureFormatId_R16f:		case ETextureFormatId_RG16f:		case ETextureFormatId_RGBA16f:
			case ETextureFormatId_R32f:		case ETextureFormatId_RG32f:		case ETextureFormatId_RGBA32f:
				break;

			//Normally we'd require DXT10

			default:
				requiresDXT10 = true;
				break;
		}

	U64 bufLen = sizeof(DDSHeader) + (requiresDXT10 ? sizeof(DDSHeaderDXT10) : 0);

	//Sort subresources to ensure we don't have missing or wrongly ordered SubResource data

	ListSubResourceData_sortCustom(buf, (CompareFunction) SubResourceData_sort);

	//Validate size of each subresource

	for (U32 i = 0, l = 0; i < info.layers; ++i) {

		U32 currW = info.w;
		U32 currH = info.h;
		U32 currL = info.l;

		for (U32 j = 0; j < info.mips; ++j) {

			const U64 len = ETextureFormat_getSize(formatOxC, currW, currH, currL);

			for (U32 k = 0; k < currL; ++k) {

				const SubResourceData dat = buf.ptr[l++];

				if(dat.layerId != i || dat.mipId != j || dat.z != k)
					return Error_invalidParameter(0, 0, "DDS_write()::buf contained duplicate data");

				if(Buffer_length(dat.data) != len)
					return Error_invalidParameter(0, 0, "DDS_write()::buf contained invalid sized data");

				if(bufLen + len < bufLen)
					return Error_overflow(0, bufLen + len, bufLen, "DDS_write() write failed, overflow!");

				bufLen += len;
			}

			currW = (U32) U64_max(1, currW >> 1);
			currH = (U32) U64_max(1, currH >> 1);
			currL = (U32) U64_max(1, currL >> 1);
		}
	}

	U8 alignY = 1;
	ETextureFormat_getAlignment(formatOxC, NULL, &alignY);

	const U64 stride = ETextureFormat_getSize(formatOxC, info.w, alignY, 1);

	if(stride >> 32)
		return Error_overflow(0, stride, U32_MAX, "DDS_write() pitch overflow");

	const Error err = Buffer_createUninitializedBytes(bufLen, allocator, result);

	if(err.genericError)
		return err;

	U8 *ptr = (U8*)result->ptr;

	//Write DDS Header

	DDSPixelFormat pixelFormat = (DDSPixelFormat) {
		.size = (U32) sizeof(DDSPixelFormat),
		.flags = EDDSPixelFormatFlag_MagicNumber,
		.magicNumber = EDDSFormatMagic_DX10
	};

	if (!requiresDXT10) {

		//These are special formats that don't have to use DXT10 for backwards compatibly reasons
		//We could force them, but we want to remain a little backwards compatible

		switch (formatOxC) {

			case ETextureFormat_RGBA8:

				pixelFormat = (DDSPixelFormat) {
					.size = pixelFormat.size,
					.flags = EDDSPixelFormatFlag_RGB | EDDSPixelFormatFlag_AlphaPixels,
					.rgbBitCount = 32,
					.masks = { 0xFF, 0xFF00, 0xFF0000, 0xFF000000 }
				};

				break;

			case ETextureFormat_RG16:

				pixelFormat = (DDSPixelFormat) {
					.size = pixelFormat.size,
					.flags = EDDSPixelFormatFlag_RGB,
					.rgbBitCount = 32,
					.masks = { 0xFFFF, 0xFFFF0000 }
				};

				break;

			case ETextureFormat_BC4:		pixelFormat.magicNumber = EDDSFormatMagic_BC4;		break;
			case ETextureFormat_BC4s:		pixelFormat.magicNumber = EDDSFormatMagic_BC4s;		break;
			case ETextureFormat_BC5:		pixelFormat.magicNumber = EDDSFormatMagic_BC5;		break;
			case ETextureFormat_BC5s:		pixelFormat.magicNumber = EDDSFormatMagic_BC5s;		break;

			case ETextureFormat_RGBA16:		pixelFormat.magicNumber = EDDSFormatMagic_RGBA16;	break;
			case ETextureFormat_RGBA16s:	pixelFormat.magicNumber = EDDSFormatMagic_RGBA16s;	break;

			case ETextureFormat_R16f:		pixelFormat.magicNumber = EDDSFormatMagic_R16f;		break;
			case ETextureFormat_RG16f:		pixelFormat.magicNumber = EDDSFormatMagic_RG16f;	break;
			case ETextureFormat_RGBA16f:	pixelFormat.magicNumber = EDDSFormatMagic_RGBA16f;	break;

			case ETextureFormat_R32f:		pixelFormat.magicNumber = EDDSFormatMagic_R32f;		break;
			case ETextureFormat_RG32f:		pixelFormat.magicNumber = EDDSFormatMagic_RG32f;	break;
			case ETextureFormat_RGBA32f:	pixelFormat.magicNumber = EDDSFormatMagic_RGBA32f;	break;

			default: 																			break;
		}

	}

	*(DDSHeader*)ptr = (DDSHeader) {
		.magicNumber = ddsMagic,
		.size = (U32)(sizeof(DDSHeader) - sizeof(((DDSHeader*)ptr)->magicNumber)),
		.flags =
			EDDSFlag_Default | (info.mips > 1 ? EDDSFlag_Mips : 0) | (info.type == ETextureType_3D ? EDDSFlag_Depth : 0) |
			(isCompressed ? EDDSFlag_LinearSize : EDDSFlag_Pitch),
		.width = info.w,
		.height = info.h,
		.pitchOrLinearSize = (U32) stride,
		.depth = info.l,
		.mips = info.mips,
		.format = pixelFormat,
		.caps = (DDSCaps2) {
			.flag1 = (EDDSCapsFlags1_Texture |
				(info.mips > 1 ? EDDSCapsFlags1_Mips : 0) |
				(info.type != ETextureType_2D ? EDDSCapsFlags1_Complex : 0)
			),
			.flag2 = (info.type == ETextureType_3D ? EDDSCapsFlags2_Volume :
				(info.type == ETextureType_Cube ? EDDSCapsFlags2_Cubemap : 0)
			)
		}
	};

	ptr += sizeof(DDSHeader);

	//Write DDS ext header

	if(requiresDXT10) {

		*(DDSHeaderDXT10*)ptr = (DDSHeaderDXT10) {
			.format = format,
			.dim = (info.type == ETextureType_3D ? EDX10Dim_3D : EDX10Dim_2D),
			.miscFlag = (info.type == ETextureType_Cube ? EDX10Misc_IsCube : 0),
			.arraySize = info.layers,
			.miscFlags2 = EDX10AlphaMode_Unknown
		};

		ptr += sizeof(DDSHeaderDXT10);
	}

	//Write subresources

	for (U64 i = 0; i < buf.length; ++i) {
		bufLen = Buffer_length(buf.ptr[i].data);
		Buffer_copy(Buffer_createRef(ptr, bufLen), buf.ptr[i].data);
		ptr += bufLen;
	}

	return Error_none();
}

Bool ListSubResourceData_freeAll(ListSubResourceData *buf, Allocator allocator) {

	if(!buf)
		return false;

	Bool success = true;

	for(U64 i = 0; i < buf->length; ++i)
		success &= Buffer_free(&buf->ptrNonConst[i].data, allocator);

	success &= ListSubResourceData_free(buf, allocator);
	return success;
}
*/
