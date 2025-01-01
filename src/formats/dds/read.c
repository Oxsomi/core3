/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "formats/dds/dds.h"
#include "formats/dds/headers.h"
#include "types/math/math.h"

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
