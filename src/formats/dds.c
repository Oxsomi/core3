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

#include "types/list_impl.h"
#include "formats/dds.h"
#include "formats/texture.h"
#include "types/buffer.h"
#include "types/allocator.h"
#include "types/error.h"

TListImpl(SubResourceData);

//DDS headers
//Combo between
// http://doc.51windows.net/Directx9_SDK/graphics/reference/ddsfilereference/ddsfileformat.htm#surface_format_header
// https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-header
// https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide

typedef enum EDDSFlag {

	EDDSFlag_Caps						= 1 <<  0,
	EDDSFlag_Height						= 1 <<  1,
	EDDSFlag_Width						= 1 <<  2,
	EDDSFlag_Pitch						= 1 <<  3,
	EDDSFlag_PixelFormat				= 1 << 12,
	EDDSFlag_Mips						= 1 << 17,
	EDDSFlag_LinearSize					= 1 << 19,
	EDDSFlag_Depth						= 1 << 23,

	EDDSFlag_Default					= EDDSFlag_Caps | EDDSFlag_Height | EDDSFlag_Width | EDDSFlag_PixelFormat,
	EDDSFlags_Supported					=
		EDDSFlag_Default | EDDSFlag_Pitch | EDDSFlag_Mips | EDDSFlag_LinearSize | EDDSFlag_Depth

} EDDSFlag;

typedef enum EDDSCapsFlags1 {
	EDDSCapsFlags1_Complex				= 1 << 3,
	EDDSCapsFlags1_Texture				= 1 << 12,
	EDDSCapsFlags1_Mips					= 1 << 22,
	EDDSCapsFlags1_Supported			= EDDSCapsFlags1_Complex | EDDSCapsFlags1_Texture | EDDSCapsFlags1_Mips
} EDDSCapsFlags1;

typedef enum EDDSCapsFlags2 {
	EDDSCapsFlags2_Cubemap				= 0xFE00,		//Contains the following bits: Cubemap, +x, -x, +y, -y, +z, -z
	EDDSCapsFlags2_Volume				= 1 << 21,
	EDDSCapsFlags2_Supported			= EDDSCapsFlags2_Cubemap | EDDSCapsFlags2_Volume
} EDDSCapsFlags2;

typedef struct DDSCaps2 {
	EDDSCapsFlags1 flag1;
	EDDSCapsFlags2 flag2;
	U32 padding[3];
} DDSCaps2;

typedef enum EDDSPixelFormatFlag {

	EDDSPixelFormatFlag_AlphaPixels		= 1 << 0,
	//EDDSPixelFormatFlag_Alpha			= 1 << 1,		//Unsupported
	EDDSPixelFormatFlag_MagicNumber		= 1 << 2,
	EDDSPixelFormatFlag_RGB				= 1 << 6,
	//EDDSPixelFormatFlag_YUV			= 1 << 9,		//Unsupported
	//EDDSPixelFormatFlag_Luminance		= 1 << 17,		//Unsupported

	EDDSPixelFormatFlag_Supported		=
		EDDSPixelFormatFlag_AlphaPixels | EDDSPixelFormatFlag_MagicNumber | EDDSPixelFormatFlag_RGB,

} EDDSPixelFormatFlag;

typedef enum EDDSFormatMagic {
	EDDSFormatMagic_DX10				= C8x4('D', 'X', '1', '0'),
	EDDSFormatMagic_BC4					= C8x4('B', 'C', '4', 'U'),
	EDDSFormatMagic_BC4s				= C8x4('B', 'C', '4', 'S'),
	EDDSFormatMagic_BC5					= C8x4('A', 'T', 'I', '2'),
	EDDSFormatMagic_BC5s				= C8x4('B', 'C', '5', 'S'),
	EDDSFormatMagic_RGBA16				= 36,
	EDDSFormatMagic_RGBA16s				= 110,
	EDDSFormatMagic_R16f				= 111,
	EDDSFormatMagic_RG16f				= 112,
	EDDSFormatMagic_RGBA16f				= 113,
	EDDSFormatMagic_R32f				= 114,
	EDDSFormatMagic_RG32f				= 115,
	EDDSFormatMagic_RGBA32f				= 116
} EDDSFormatMagic;

typedef struct DDSPixelFormat {

	U32 size;					//sizeof(DDSPixelFormat) = 32
	EDDSPixelFormatFlag flags;
	U32 magicNumber;			//EDDSFormatMagic or DXFormat if EDDSPixelFormatFlag_MagicNumber
	U32 rgbBitCount;

	U32 masks[4];

} DDSPixelFormat;

typedef struct DDSHeader {

	U32 magicNumber;			//"DDS "

	U32 size;					//sizeof(DDSHeader) - sizeof(magicNumber) = 128 - 4 = 124
	EDDSFlag flags;
	U32 height, width;
	U32 pitchOrLinearSize;		//flags & EDDSFlag_Pitch or LinearSize
	U32 depth;					//flags & EDDSFlag_Depth
	U32 mips;					//flags & EDDSFlag_Mips

	U32 pad0[11];

	DDSPixelFormat format;		//flags & EDDSFlag_PixelFormat

	DDSCaps2 caps;				//flags & EDDSFlag_Caps

} DDSHeader;

typedef enum EDX10Dim {
	EDX10Dim_1D = 2,
	EDX10Dim_2D,
	EDX10Dim_3D
} EDX10Dim;

typedef enum EDX10Misc {
	EDX10Misc_IsCube		= 1 << 2,
	EDX10Misc_Supported		= EDX10Misc_IsCube
} EDX10Misc;

typedef enum EDX10AlphaMode {
	EDX10AlphaMode_Unknown,
	EDX10AlphaMode_Normal,
	EDX10AlphaMode_Premul,
	EDX10AlphaMode_Opaque,
	EDX10AlphaMode_Custom,
	EDX10AlphaMode_Count
} EDX10AlphaMode;

typedef struct DDSHeaderDXT10 {
	DXFormat format;
	EDX10Dim dim;
	EDX10Misc miscFlag;
	U32 arraySize;
	EDX10AlphaMode miscFlags2;
} DDSHeaderDXT10;

static const U32 ddsMagic = C8x4('D', 'D', 'S', ' ');

//DDS serialization

Error DDS_read(Buffer buf, DDSInfo *info, Allocator allocator, ListSubResourceData *result) {

	if(!info || !result)
		return Error_nullPointer(!info ? 1 : 3, "DDS_read()::info and result are required");

	if(result->ptr)
		return Error_invalidParameter(3, 0, "DDS_read()::result isn't empty, potential memleak");

	if(Buffer_length(buf) < sizeof(DDSHeader))
		return Error_invalidParameter(0, 0, "DDS_read()::buf had a missing header");

	DDSHeader header = *(const DDSHeader*) buf.ptr;

	if(header.magicNumber != ddsMagic)
		return Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid header magic");

	if(header.size != sizeof(header) - sizeof(header.magicNumber))
		return Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid header size");

	if(header.format.size != sizeof(DDSPixelFormat))
		return Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid header pixel format size");

	if(header.format.flags &~ EDDSPixelFormatFlag_Supported)
		return Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid header pixel flag");

	if((header.flags & EDDSFlag_Pitch) && (header.flags & EDDSFlag_LinearSize))
		return Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid pitch or linearSize");

	if(header.flags &~ EDDSFlags_Supported)
		return Error_invalidParameter(0, 0, "DDS_read()::flags had an unsupported header flag");

	if(header.caps.flag1 &~ EDDSCapsFlags1_Supported)
		return Error_invalidParameter(0, 0, "DDS_read()::flags had an unsupported capability flag1");

	if(header.caps.flag2 &~ EDDSCapsFlags2_Supported)
		return Error_invalidParameter(0, 0, "DDS_read()::flags had an unsupported capability flag2");

	if(!header.width || !header.height)
		return Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid width or height");

	if(!(header.flags & EDDSFlag_Depth))
		header.depth = 1;

	if(!header.depth || !header.mips)
		return Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid depth or mips");

	U32 biggestSize2 = (U32) U64_max(U64_max(header.width, header.height), header.depth);
	U32 mips = (U32) U64_max(1, (U64) F64_ceil(F64_log2((F64)biggestSize2)));

	if(header.mips > mips)
		return Error_invalidParameter(0, 0, "DDS_read()::buf mip count exceeded available mip count");

	//Here we force DXT10 format so we don't have to handle anything else

	Bool useMagic = header.format.flags & EDDSPixelFormatFlag_MagicNumber;
	Bool hasDXT10 = false;
	U32 arraySize = 1;

	ETextureType type = ETextureType_2D;
	ETextureFormatId formatId = ETextureFormatId_Undefined;
	ETextureFormat format = ETextureFormat_Undefined;

	if(useMagic && header.format.magicNumber == EDDSFormatMagic_DX10) {

		if(Buffer_length(buf) < sizeof(DDSHeader) + sizeof(DDSHeaderDXT10))
			return Error_invalidParameter(0, 0, "DDS_read()::buf had a missing DXT10 header");

		hasDXT10 = true;
		DDSHeaderDXT10 header10 = *(const DDSHeaderDXT10*)((const DDSHeader*) buf.ptr + 1);

		formatId = DXFormat_toTextureFormatId(header10.format);
		format = ETextureFormatId_unpack[formatId];
		EDepthStencilFormat depthFormat = DXFormat_toDepthStencilFormat(header10.format);

		if(format == ETextureFormat_Undefined && depthFormat == EDepthStencilFormat_None)
			return Error_invalidParameter(0, 0, "DDS_read()::buf had an unsupported OxC3 format");

		switch (header10.dim) {

			case EDX10Dim_1D:
				header.height = 1;

			case EDX10Dim_2D:		//Both 2D and 1D are treated as 2D textures. 1D is just 2D tex with height 1
				break;

			case EDX10Dim_3D:
				type = ETextureType_3D;
				break;

			default:
				return Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid texture type");
		}

		if(!header10.arraySize || (header10.arraySize > 1 && type == ETextureType_3D))
			return Error_invalidParameter(0, 0, "DDS_read()::buf had invalid arraySize (either 0 or 3D[]");

		if(header10.miscFlag &~ EDX10Misc_Supported)
			return Error_invalidParameter(0, 0, "DDS_read()::buf had unsupported misc flag");

		if((header.caps.flag2 & EDDSCapsFlags2_Volume) && header10.dim != EDX10Dim_3D)
			return Error_invalidParameter(0, 0, "DDS_read()::buf had volume flag but had invalid state");

		if(
			(header10.miscFlag & EDX10Misc_IsCube) && (
				header10.dim != EDX10Dim_2D || (header.caps.flag2 & EDDSCapsFlags2_Cubemap) != EDDSCapsFlags2_Cubemap
			)
		)
			return Error_invalidParameter(0, 0, "DDS_read()::buf had cubemap flag but had invalid state");

		if(header10.miscFlag & EDX10Misc_IsCube) {
			type = ETextureType_Cube;
			header10.arraySize = 6;
		}

		if(header10.miscFlags2 >= EDX10AlphaMode_Count)
			return Error_invalidParameter(0, 0, "DDS_read()::buf had invalid alpha mode");

		if(depthFormat)
			return Error_invalidParameter(0, 0, "DDS_read() depth textures aren't currently supported");

		arraySize = header10.arraySize;
	}

	//DXT10 isn't present, we have to detect format, cube and volume the old way

	else {

		//Detect cubemap and volume

		if(header.caps.flag2 & EDDSCapsFlags2_Cubemap) {

			if((header.caps.flag2 & EDDSCapsFlags2_Cubemap) != EDDSCapsFlags2_Cubemap)
				return Error_invalidParameter(0, 0, "DDS_read()::buf didn't have all cubemap bits set");

			type = ETextureType_Cube;
			arraySize = 6;
		}

		if ((header.caps.flag2 & EDDSCapsFlags2_Volume)) {

			if(type != ETextureType_2D)
				return Error_invalidParameter(0, 0, "DDS_read()::buf had both cubemap and volume bits set");

			type = ETextureType_3D;
		}

		//Detect format

		formatId = ETextureFormatId_Undefined;

		//Detect from magic number

		if (useMagic)
			switch (header.format.magicNumber) {

				case EDDSFormatMagic_BC4:			formatId = ETextureFormatId_BC4;		break;
				case EDDSFormatMagic_BC4s:			formatId = ETextureFormatId_BC4s;		break;
				case EDDSFormatMagic_BC5:			formatId = ETextureFormatId_BC5;		break;
				case EDDSFormatMagic_BC5s:			formatId = ETextureFormatId_BC5s;		break;

				case EDDSFormatMagic_RGBA16:		formatId = ETextureFormatId_RGBA16;		break;
				case EDDSFormatMagic_RGBA16s:		formatId = ETextureFormatId_RGBA16s;	break;

				case EDDSFormatMagic_R16f:			formatId = ETextureFormatId_R16f;		break;
				case EDDSFormatMagic_RG16f:			formatId = ETextureFormatId_RG16f;		break;
				case EDDSFormatMagic_RGBA16f:		formatId = ETextureFormatId_RGBA16f;	break;

				case EDDSFormatMagic_R32f:			formatId = ETextureFormatId_R32f;		break;
				case EDDSFormatMagic_RG32f:			formatId = ETextureFormatId_RG32f;		break;
				case EDDSFormatMagic_RGBA32f:		formatId = ETextureFormatId_RGBA32f;	break;
			}

		//Detect from pixel format

		if (!formatId && (header.format.flags & EDDSPixelFormatFlag_RGB) && header.format.rgbBitCount == 32) {

			if(header.format.masks[0] == U16_MAX && header.format.masks[1] == ((U32)U16_MAX << 16))
				formatId = ETextureFormatId_RG16;

			else if(
				header.format.masks[0] == 0x3FF &&
				header.format.masks[1] == (0x3FF << 10) &&
				header.format.masks[2] == (0x3FF << 20)
			)
				formatId = ETextureFormatId_BGR10A2;

			else if (
				header.format.masks[0] == U8_MAX &&
				header.format.masks[1] == ((U32)U8_MAX << 8) &&
				header.format.masks[2] == ((U32)U8_MAX << 16) &&
				header.format.masks[3] == ((U32)U8_MAX << 24)
			)
				formatId = ETextureFormatId_RGBA8;
		}

		if(formatId == ETextureFormatId_Undefined)
			return Error_invalidParameter(0, 0, "DDS_read()::buf couldn't detect format");

		format = ETextureFormatId_unpack[formatId];
	}

	U64 len = ETextureFormat_getSize(format, header.width, header.height, header.depth);

	//Calculate expected length, because header.pitch is not even close to being reliable.

	U64 expectedLength = len;
	U32 currW = (U32) U64_max(1, header.width >> 1);
	U32 currH = (U32) U64_max(1, header.height >> 1);
	U32 currL = (U32) U64_max(1, header.depth >> 1);

	for (U64 i = 1; i < header.mips; ++i) {
		expectedLength += ETextureFormat_getSize(format, currW, currH, currL);
		currW = (U32) U64_max(1, currW >> 1);
		currH = (U32) U64_max(1, currH >> 1);
		currL = (U32) U64_max(1, currL >> 1);
	}

	expectedLength *= arraySize;

	U64 headerSize = sizeof(DDSHeader) + (hasDXT10 ? sizeof(DDSHeaderDXT10) : 0);

	if(Buffer_length(buf) != headerSize + expectedLength)
		return Error_invalidParameter(0, 0, "DDS_read()::buf had invalid size");

	U64 totalSubResources = (U64)header.mips * header.depth * arraySize;

	//Output parsed result

	Error err = ListSubResourceData_resize(result, totalSubResources, allocator);

	if(err.genericError)
		return err;

	const U8 *ptr = buf.ptr + headerSize;

	for (U32 i = 0, l = 0; i < arraySize; ++i) {

		currW = header.width;
		currH = header.height;
		currL = header.depth;

		for (U32 j = 0; j < header.mips; ++j) {

			for (U32 k = 0; k < currL; ++k) {

				len = ETextureFormat_getSize(format, currW, currH, currL);

				result->ptrNonConst[l++] = (SubResourceData) {
					.layerId = i,
					.mipId = j,
					.z = k,
					.data = Buffer_createRefConst(ptr, len)
				};

				ptr += len;
			}

			currW = (U32) U64_max(1, currW >> 1);
			currH = (U32) U64_max(1, currH >> 1);
			currL = (U32) U64_max(1, currL >> 1);
		}
	}

	*info = (DDSInfo) {
		.w = header.width,
		.h = header.height,
		.l = header.depth,
		.mips = header.mips,
		.layers = arraySize,
		.type = type,
		.textureFormatId = formatId
	};

	return Error_none();
}

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

	U32 biggestSize2 = (U32) U64_max(U64_max(info.w, info.h), info.l);
	U32 mips = (U32) U64_max(1, (U64) F64_ceil(F64_log2((F64)biggestSize2)));

	if(info.mips > mips)
		return Error_invalidParameter(1, 0, "DDS_write()::info.mips out of bounds");

	U64 totalSubResources = (U64)info.mips * info.l * info.layers;

	if(totalSubResources != buf.length)
		return Error_invalidParameter(0, 0, "DDS_write()::info's subresource count and buf.length mismatched");

	if(info.type == ETextureType_Cube && info.layers != 6)
		return Error_invalidParameter(0, 0, "DDS_write()::info specifies a cubemap, but no 6 faces were found");

	if(info.l > 1 && info.type != ETextureType_3D)
		return Error_invalidParameter(0, 0, "DDS_write()::info specifies length of >1 but ETextureType_3D wasn't specified");

	if(info.layers > 1 && info.type == ETextureType_3D)
		return Error_invalidParameter(0, 0, "DDS_write()::info specifies layers of >1 but ETextureType_3D was used");

	DXFormat format = ETextureFormatId_toDXFormat(info.textureFormatId);

	if(!format)
		return Error_invalidParameter(0, 0, "DDS_write()::info.textureFormatId isn't supported as a DDS texture");

	ETextureFormat formatOxC = ETextureFormatId_unpack[info.textureFormatId];
	Bool isCompressed = ETextureFormat_getIsCompressed(formatOxC);

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

			U64 len = ETextureFormat_getSize(formatOxC, currW, currH, currL);

			for (U32 k = 0; k < currL; ++k) {

				SubResourceData dat = buf.ptr[l++];

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

	U64 stride = ETextureFormat_getSize(formatOxC, info.w, alignY, 1);

	if(stride >> 32)
		return Error_overflow(0, stride, U32_MAX, "DDS_write() pitch overflow");

	Error err = Buffer_createUninitializedBytes(bufLen, allocator, result);

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
