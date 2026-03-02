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

#include "types/container/list_impl.h"
#include "types/container/types.h"
#include "types/container/ref_ptr.h"
#include "types/container/stream.h"
#include "formats/dds/dds_file.h"
#include "formats/dds/dds_headers.h"
#include "types/base/constants.h"
#include "types/base/mathi.h"
#include "types/base/mathf.h"

Bool DDS_read(
	StreamRef *streamRef,
	U64 *streamOff,
	DDSInfo *info,
	const Allocator *alloc,
	ListSubResourceData *result,
	Error *e_rr
) {

	Bool s_uccess = true;
	StreamCursor cursor = (StreamCursor) { 0 };

	if(!info || !result)
		retError(clean, Error_nullPointer(!info ? 1 : 3, "DDS_read()::info and result are required"));

	if(result->ptr)
		retError(clean, Error_invalidParameter(3, 0, "DDS_read()::result isn't empty, potential memleak"));

	if (!streamRef || !streamOff)
		retError(clean, Error_nullPointer(!streamRef ? 0 : 1, "DDS_read()::streamRef and streamOff are required"));

	if (streamRef->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream)
		retError(clean, Error_invalidParameter(3, 0, "DDS_read()::streamRef is of invalid type"));

	Stream *stream = RefPtr_data(streamRef, Stream);

	if (!stream->read)
		retError(clean, Error_invalidParameter(3, 0, "DDS_read()::streamRef is not readable"));

	gotoIfError3(clean, StreamCursor_create(streamRef, 0, false, alloc, &cursor, e_rr));

	DDSHeader header = (DDSHeader) { 0 };
	gotoIfError3(clean, StreamCursor_read(
		&cursor, Buffer_createRef(&header, sizeof(header)), *streamOff, 0, 0, false, alloc, e_rr
	));

	if(header.magicNumber != ddsMagic)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid header magic"));

	if(header.size != sizeof(header) - sizeof(header.magicNumber))
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid header size"));

	if(header.format.size != sizeof(DDSPixelFormat))
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid header pixel format size"));

	if(header.format.flags &~ EDDSPixelFormatFlag_Supported)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid header pixel flag"));

	if((header.flags & EDDSFlag_Pitch) && (header.flags & EDDSFlag_LinearSize))
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid pitch or linearSize"));

	if(header.flags &~ EDDSFlags_Supported)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::flags had an unsupported header flag"));

	if(header.caps.flag1 &~ EDDSCapsFlags1_Supported)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::flags had an unsupported capability flag1"));

	if(header.caps.flag2 &~ EDDSCapsFlags2_Supported)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::flags had an unsupported capability flag2"));

	if(!header.width || !header.height)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid width or height"));

	if(!(header.flags & EDDSFlag_Depth))
		header.depth = 1;

	if(!header.depth || !header.mips)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid depth or mips"));

	U32 biggestSize2 = (U32) U64_max(U64_max(header.width, header.height), header.depth);
	const U32 mips = U32_max(1, (U32) F64_floor(F64_log2((F64)biggestSize2)) + 1);

	if(header.mips > mips)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf mip count exceeded available mip count"));

	//Here we force DXT10 format so we don't have to handle anything else

	Bool useMagic = header.format.flags & EDDSPixelFormatFlag_MagicNumber;
	Bool hasDXT10 = false;
	U32 arraySize = 1;

	ETextureType type = ETextureType_2D;
	ETextureFormatId formatId = ETextureFormatId_Undefined;
	ETextureFormat format = ETextureFormat_Undefined;

	if(useMagic && header.format.magicNumber == EDDSFormatMagic_DX10) {

		hasDXT10 = true;
		DDSHeaderDXT10 header10 = (DDSHeaderDXT10) { 0 };

		gotoIfError3(clean, StreamCursor_read(
			&cursor, Buffer_createRef(&header10, sizeof(header10)), *streamOff + sizeof(DDSHeader), 0, 0, false, alloc, e_rr
		));

		formatId = DXFormat_toTextureFormatId(header10.format);
		format = ETextureFormatId_unpack[formatId];
		EDepthStencilFormat depthFormat = DXFormat_toDepthStencilFormat(header10.format);

		if(format == ETextureFormat_Undefined && depthFormat == EDepthStencilFormat_None)
			retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf had an unsupported OxC3 format"));

		switch (header10.dim) {

			case EDX10Dim_1D:
				header.height = 1;

			case EDX10Dim_2D:		//Both 2D and 1D are treated as 2D textures. 1D is just 2D tex with height 1
				break;

			case EDX10Dim_3D:
				type = ETextureType_3D;
				break;

			default:
				retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid texture type"));
		}

		if(!header10.arraySize || (header10.arraySize > 1 && type == ETextureType_3D))
			retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf had invalid arraySize (either 0 or 3D[]"));

		if(header10.miscFlag &~ EDX10Misc_Supported)
			retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf had unsupported misc flag"));

		if((header.caps.flag2 & EDDSCapsFlags2_Volume) && header10.dim != EDX10Dim_3D)
			retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf had volume flag but had invalid state"));

		if(
			(header10.miscFlag & EDX10Misc_IsCube) && (
				header10.dim != EDX10Dim_2D || (header.caps.flag2 & EDDSCapsFlags2_Cubemap) != EDDSCapsFlags2_Cubemap
			)
		)
			retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf had cubemap flag but had invalid state"));

		if(header10.miscFlag & EDX10Misc_IsCube) {
			type = ETextureType_Cube;
			header10.arraySize = 6;
		}

		if(header10.miscFlags2 >= EDX10AlphaMode_Count)
			retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf had invalid alpha mode"));

		if(depthFormat)
			retError(clean, Error_invalidParameter(0, 0, "DDS_read() depth textures aren't currently supported"));

		arraySize = header10.arraySize;
	}

	//DXT10 isn't present, we have to detect format, cube and volume the old way

	else {

		//Detect cubemap and volume

		if(header.caps.flag2 & EDDSCapsFlags2_Cubemap) {

			if((header.caps.flag2 & EDDSCapsFlags2_Cubemap) != EDDSCapsFlags2_Cubemap)
				retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf didn't have all cubemap bits set"));

			type = ETextureType_Cube;
			arraySize = 6;
		}

		if ((header.caps.flag2 & EDDSCapsFlags2_Volume)) {

			if(type != ETextureType_2D)
				retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf had both cubemap and volume bits set"));

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
			retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf couldn't detect format"));

		format = ETextureFormatId_unpack[formatId];
	}

	U64 len = ETextureFormat_getSize(format, header.width, header.height, header.depth);

	//Calculate expected length, because header.pitch is not even close to being reliable.

	U64 expectedLength = len;
	U32 currW = U32_max(1, header.width >> 1);
	U32 currH = U32_max(1, header.height >> 1);
	U32 currL = U32_max(1, header.depth >> 1);

	for (U64 i = 1; i < header.mips; ++i) {
		expectedLength += ETextureFormat_getSize(format, currW, currH, currL);
		currW = U32_max(1, currW >> 1);
		currH = U32_max(1, currH >> 1);
		currL = U32_max(1, currL >> 1);
	}

	expectedLength *= arraySize;

	U64 headerSize = sizeof(DDSHeader) + (hasDXT10 ? sizeof(DDSHeaderDXT10) : 0);

	if(*streamOff + headerSize + expectedLength > stream->size)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::stream had invalid size"));

	currL = header.depth;
	U64 totalSubResources = 0;

	for (U32 j = 0; j < header.mips; ++j) {
		totalSubResources += currL;
		currL = U32_max(1, currL >> 1);
	}

	totalSubResources *= arraySize;

	//Output parsed result

	gotoIfError3(clean, ListSubResourceData_resize(result, totalSubResources, alloc, e_rr));

	for (U32 i = 0, l = 0; i < arraySize; ++i) {

		currW = header.width;
		currH = header.height;
		currL = header.depth;

		for (U32 j = 0; j < header.mips; ++j) {

			for (U32 k = 0; k < currL; ++k) {

				len = ETextureFormat_getSize(format, currW, currH, 1);

				RefPtr_inc(streamRef);

				result->ptrNonConst[l++] = (SubResourceData) {
					.layerId = i,
					.mipId = j,
					.z = k,
					.stream = streamRef,
					.streamOff = *streamOff,
					.streamLen = len
				};
				
				*streamOff += len;
			}

			currW = U32_max(1, currW >> 1);
			currH = U32_max(1, currH >> 1);
			currL = U32_max(1, currL >> 1);
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

clean:
	StreamCursor_close(&cursor, alloc);
	return s_uccess;
}
