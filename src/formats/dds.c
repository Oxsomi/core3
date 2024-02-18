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
	EDDSPixelFormatFlag_Alpha			= 1 << 1,
	EDDSPixelFormatFlag_MagicNumber		= 1 << 2,
	EDDSPixelFormatFlag_RGB				= 1 << 6,
	//EDDSPixelFormatFlag_YUV			= 1 << 9,
	EDDSPixelFormatFlag_Luminance		= 1 << 17,

	EDDSPixelFormatFlag_Supported		= 
		EDDSPixelFormatFlag_AlphaPixels | EDDSPixelFormatFlag_Alpha | EDDSPixelFormatFlag_MagicNumber |
		EDDSPixelFormatFlag_RGB | EDDSPixelFormatFlag_Luminance,

} EDDSPixelFormatFlag;

typedef enum EDDSFormatMagic {
	EDDSFormatMagic_DX10				= C8x4('D', 'X', '1', '0'),
	//EDDSFormatMagic_DXT1				= C8x4('D', 'X', 'T', '1'),
	//EDDSFormatMagic_DXT2				= C8x4('D', 'X', 'T', '2'),
	//EDDSFormatMagic_DXT3				= C8x4('D', 'X', 'T', '3'),
	//EDDSFormatMagic_DXT4				= C8x4('D', 'X', 'T', '4'),
	//EDDSFormatMagic_DXT5				= C8x4('D', 'X', 'T', '5')
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

	union {
		I32 pitch;				//flags & EDDSFlag_Pitch
		U32 linearSize;			//flags & EDDSFlag_LinearSize
	};

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

//DDS serialization

Error DDS_read(Buffer buf, DDSInfo *info, Allocator allocator, ListSubResourceData *result) {

	if(!info || !result)
		return Error_nullPointer(!info ? 1 : 3, "DDS_read()::info and result are required");

	if(result->ptr)
		return Error_invalidParameter(3, 0, "DDS_read()::result isn't empty, potential memleak");

	if(Buffer_length(buf) < sizeof(DDSHeader))
		return Error_invalidParameter(0, 0, "DDS_read()::buf had a missing header");

	DDSHeader header = *(const DDSHeader*) buf.ptr;

	if(header.magicNumber != C8x4('D', 'D', 'S', ' '))
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

	if(header.format.magicNumber != EDDSFormatMagic_DX10 || !(header.format.flags & EDDSPixelFormatFlag_MagicNumber))
		return Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid header pixel format magic");

	if(Buffer_length(buf) < sizeof(DDSHeader) + sizeof(DDSHeaderDXT10))
		return Error_invalidParameter(0, 0, "DDS_read()::buf had a missing DXT10 header");

	DDSHeaderDXT10 header10 = *(const DDSHeaderDXT10*)((const DDSHeader*) buf.ptr + 1);

	ETextureFormatId formatId = DXFormat_toTextureFormatId(header10.format);
	ETextureFormat format = ETextureFormatId_unpack[formatId];
	EDepthStencilFormat depthFormat = DXFormat_toDepthStencilFormat(header10.format);

	if(format == ETextureFormat_Undefined && depthFormat == EDepthStencilFormat_None)
		return Error_invalidParameter(0, 0, "DDS_read()::buf had an unsupported OxC3 format");

	ETextureType type = ETextureType_2D;

	switch (header10.dim) {

		case EDX10Dim_1D: case EDX10Dim_2D:		//Both 2D and 1D are treated as 2D textures. 1D is just 2D tex with height 1
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

	if(
		(header.caps.flag2 & EDDSCapsFlags2_Volume) && (
			!(header.caps.flag1 & EDDSCapsFlags1_Complex) ||
			header10.dim != EDX10Dim_3D 
		) 
	)
		return Error_invalidParameter(0, 0, "DDS_read()::buf had volume flag but had invalid state");

	if(
		(header10.miscFlag & EDX10Misc_IsCube) && (
			header10.dim != EDX10Dim_2D || 
			(header.caps.flag2 & EDDSCapsFlags2_Cubemap) != EDDSCapsFlags2_Cubemap ||
			!(header.caps.flag1 & EDDSCapsFlags1_Complex)
		) 
	)
		return Error_invalidParameter(0, 0, "DDS_read()::buf had cubemap flag but had invalid state");

	if(header10.miscFlag & EDX10Misc_IsCube)
		type = ETextureType_Cube;

	if(header10.miscFlags2 >= EDX10AlphaMode_Count)
		return Error_invalidParameter(0, 0, "DDS_read()::buf had invalid alpha mode");

	if(depthFormat)
		return Error_invalidParameter(0, 0, "DDS_read() depth textures aren't currently supported");

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

	expectedLength *= header10.arraySize;

	if(Buffer_length(buf) != sizeof(DDSHeader) + sizeof(DDSHeaderDXT10) + expectedLength)
		return Error_invalidParameter(0, 0, "DDS_read()::buf had invalid size");

	U64 totalSubResources = (U64)header.mips * header.depth * header10.arraySize;

	if(totalSubResources >> 32)
		return Error_invalidParameter(0, 0, "DDS_read()::buf had invalid header sub resource count");

	Error err = ListSubResourceData_resize(result, totalSubResources, allocator);

	if(err.genericError)
		return err;

	const U8 *ptr = buf.ptr + sizeof(DDSHeader) + sizeof(DDSHeaderDXT10);

	for (U32 i = 0, l = 0; i < header10.arraySize; ++i) {

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

	//Output real format

	*info = (DDSInfo) {
		.w = header.width,
		.h = header.height,
		.l = header.depth,
		.mips = header.mips,
		.layers = header10.arraySize,
		.type = type,
		.textureFormatId = formatId
	};

	allocator;
	result;
	return Error_none();
}

Error DDS_write(ListSubResourceData buf, DDSInfo info, Allocator allocator, Buffer *result) {
	buf;
	info;
	allocator;
	result;
	//TODO:
	return Error_none();
}

Bool ListSubResourceData_freeAll(ListSubResourceData *buf, Allocator allocator) {

	if(!buf)
		return false;

	Bool success = true;

	for(U64 i = 0; i < buf->length; ++i)
		success &= Buffer_free(&buf->ptrNonConst[i].data, allocator);

	success &= ListSubResourceData_free(buf, allocator);
	return true;
}
