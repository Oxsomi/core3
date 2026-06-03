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

//formats/dds/dds_read.c

#include "types/container/list_impl.h"
#include "types/container/container_types.h"
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

	if(!info || !result)
		retError(clean, Error_nullPointer(!info ? 1 : 3, "DDS_read()::info and result are required"));

	if(result->ptr)
		retError(clean, Error_invalidParameter(3, 0, "DDS_read()::result isn't empty, potential memleak"));

	if (!streamRef || !streamOff)
		retError(clean, Error_nullPointer(!streamRef ? 0 : 1, "DDS_read()::streamRef and streamOff are required"));

	if (streamRef->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream)
		retError(clean, Error_invalidParameter(3, 0, "DDS_read()::streamRef is of invalid type"));

	OxStream *stream = RefPtr_data(streamRef, OxStream);

	if (!stream->read)
		retError(clean, Error_invalidParameter(3, 0, "DDS_read()::streamRef is not readable"));

	DDSHeaderMax dds;

	Bool canHaveDXT10 = stream->size >= sizeof(dds);

	gotoIfError3(clean, stream->read(
		stream,
		*streamOff,
		canHaveDXT10 ? sizeof(dds) : sizeof(DDSHeader),
		Buffer_createRef(&dds, sizeof(dds)),
		alloc,
		e_rr
	));

	if(dds.header.magicNumber != ddsMagic)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds had an invalid header magic"));

	if(dds.header.size != sizeof(dds.header) - sizeof(dds.header.magicNumber))
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds had an invalid header size"));

	if(dds.header.format.size != sizeof(DDSPixelFormat))
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds had an invalid header pixel format size"));

	if(dds.header.format.flags &~ EDDSPixelFormatFlag_Supported)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds had an invalid header pixel flag"));

	if((dds.header.flags & EDDSFlag_Pitch) && (dds.header.flags & EDDSFlag_LinearSize))
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf had an invalid pitch or linearSize"));

	if(dds.header.flags &~ EDDSFlags_Supported)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::flags had an unsupported header flag"));

	if(dds.header.caps.flag1 &~ EDDSCapsFlags1_Supported)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::flags had an unsupported capability flag1"));

	if(dds.header.caps.flag2 &~ EDDSCapsFlags2_Supported)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::flags had an unsupported capability flag2"));

	if(!dds.header.width || !dds.header.height)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds had an invalid width or height"));

	if(!(dds.header.flags & EDDSFlag_Depth))
		dds.header.depth = 1;

	if(!dds.header.depth || !dds.header.mips)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds had an invalid depth or mips"));

	U32 biggestSize2 = (U32) U64_max(U64_max(dds.header.width, dds.header.height), dds.header.depth);
	const U32 mips = U32_max(1, (U32) F64_floor(F64_log2((F64)biggestSize2)) + 1);

	if(dds.header.mips > mips)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds mip count exceeded available mip count"));

	//Here we force DXT10 format so we don't have to handle anything else

	Bool useMagic = dds.header.format.flags & EDDSPixelFormatFlag_MagicNumber;
	U32 arraySize = 1;

	ETextureType type = ETextureType_2D;
	ETextureFormatId formatId = ETextureFormatId_Undefined;
	ETextureFormat format = ETextureFormat_Undefined;
	Bool hasDXT10 = false;

	if(useMagic && dds.header.format.magicNumber == EDDSFormatMagic_DX10) {

		if(!canHaveDXT10)
			retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds didn't contain DXT10 even though it was requested"));

		hasDXT10 = true;
		*streamOff += sizeof(dds);

		formatId = DXFormat_toTextureFormatId(dds.header10.format);
		format = ETextureFormatId_unpack[formatId];
		EDepthStencilFormat depthFormat = DXFormat_toDepthStencilFormat(dds.header10.format);

		if(format == ETextureFormat_Undefined && depthFormat == EDepthStencilFormat_None)
			retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds had an unsupported OxC3 format"));

		switch (dds.header10.dim) {

			case EDX10Dim_1D:
				dds.header.height = 1;

			case EDX10Dim_2D:        //Both 2D and 1D are treated as 2D textures. 1D is just 2D tex with height 1
				break;

			case EDX10Dim_3D:
				type = ETextureType_3D;
				break;

			default:
				retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds had an invalid texture type"));
		}

		if(!dds.header10.arraySize || (dds.header10.arraySize > 1 && type == ETextureType_3D))
			retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds had invalid arraySize (either 0 or 3D[]"));

		if(dds.header10.miscFlag &~ EDX10Misc_Supported)
			retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds had unsupported misc flag"));

		if((dds.header.caps.flag2 & EDDSCapsFlags2_Volume) && dds.header10.dim != EDX10Dim_3D)
			retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds had volume flag but had invalid state"));

		if(
			(dds.header10.miscFlag & EDX10Misc_IsCube) && (
				dds.header10.dim != EDX10Dim_2D || (dds.header.caps.flag2 & EDDSCapsFlags2_Cubemap) != EDDSCapsFlags2_Cubemap
			)
		)
			retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds had cubemap flag but had invalid state"));

		if(dds.header10.miscFlag & EDX10Misc_IsCube) {
			type = ETextureType_Cube;
			dds.header10.arraySize = 6;
		}

		if(dds.header10.miscFlags2 >= EDX10AlphaMode_Count)
			retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds had invalid alpha mode"));

		if(depthFormat)
			retError(clean, Error_invalidParameter(0, 0, "DDS_read() depth textures aren't currently supported"));

		arraySize = dds.header10.arraySize;
	}

	//DXT10 isn't present, we have to detect format, cube and volume the old way

	else {

		*streamOff += sizeof(dds.header);

		//Detect cubemap and volume

		if(dds.header.caps.flag2 & EDDSCapsFlags2_Cubemap) {

			if((dds.header.caps.flag2 & EDDSCapsFlags2_Cubemap) != EDDSCapsFlags2_Cubemap)
				retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds didn't have all cubemap bits set"));

			type = ETextureType_Cube;
			arraySize = 6;
		}

		if ((dds.header.caps.flag2 & EDDSCapsFlags2_Volume)) {

			if(type != ETextureType_2D)
				retError(clean, Error_invalidParameter(0, 0, "DDS_read()::dds had both cubemap and volume bits set"));

			type = ETextureType_3D;
		}

		//Detect format

		formatId = ETextureFormatId_Undefined;

		//Detect from magic number

		if (useMagic)
			switch (dds.header.format.magicNumber) {

				case EDDSFormatMagic_BC4:            formatId = ETextureFormatId_BC4;        break;
				case EDDSFormatMagic_BC4s:            formatId = ETextureFormatId_BC4s;        break;
				case EDDSFormatMagic_BC5:            formatId = ETextureFormatId_BC5;        break;
				case EDDSFormatMagic_BC5s:            formatId = ETextureFormatId_BC5s;        break;

				case EDDSFormatMagic_RGBA16:        formatId = ETextureFormatId_RGBA16;        break;
				case EDDSFormatMagic_RGBA16s:        formatId = ETextureFormatId_RGBA16s;    break;

				case EDDSFormatMagic_R16f:            formatId = ETextureFormatId_R16f;        break;
				case EDDSFormatMagic_RG16f:            formatId = ETextureFormatId_RG16f;        break;
				case EDDSFormatMagic_RGBA16f:        formatId = ETextureFormatId_RGBA16f;    break;

				case EDDSFormatMagic_R32f:            formatId = ETextureFormatId_R32f;        break;
				case EDDSFormatMagic_RG32f:            formatId = ETextureFormatId_RG32f;        break;
				case EDDSFormatMagic_RGBA32f:        formatId = ETextureFormatId_RGBA32f;    break;
			}

		//Detect from pixel format

		if (!formatId && (dds.header.format.flags & EDDSPixelFormatFlag_RGB) && dds.header.format.rgbBitCount == 32) {

			if(dds.header.format.masks[0] == U16_MAX && dds.header.format.masks[1] == ((U32)U16_MAX << 16))
				formatId = ETextureFormatId_RG16;

			else if(
				dds.header.format.masks[0] == 0x3FF &&
				dds.header.format.masks[1] == (0x3FF << 10) &&
				dds.header.format.masks[2] == (0x3FF << 20)
			)
				formatId = ETextureFormatId_BGR10A2;

			else if (
				dds.header.format.masks[0] == U8_MAX &&
				dds.header.format.masks[1] == ((U32)U8_MAX << 8) &&
				dds.header.format.masks[2] == ((U32)U8_MAX << 16) &&
				dds.header.format.masks[3] == ((U32)U8_MAX << 24)
			)
				formatId = ETextureFormatId_RGBA8;
		}

		if(formatId == ETextureFormatId_Undefined)
			retError(clean, Error_invalidParameter(0, 0, "DDS_read()::buf couldn't detect format"));

		format = ETextureFormatId_unpack[formatId];
	}

	U64 len = ETextureFormat_getSize(format, dds.header.width, dds.header.height, dds.header.depth);

	//Calculate expected length, because header.pitch is not even close to being reliable.

	U64 expectedLength = len;
	U32 currW = U32_max(1, dds.header.width >> 1);
	U32 currH = U32_max(1, dds.header.height >> 1);
	U32 currL = U32_max(1, dds.header.depth >> 1);

	for (U64 i = 1; i < dds.header.mips; ++i) {
		expectedLength += ETextureFormat_getSize(format, currW, currH, currL);
		currW = U32_max(1, currW >> 1);
		currH = U32_max(1, currH >> 1);
		currL = U32_max(1, currL >> 1);
	}

	expectedLength *= arraySize;

	if(*streamOff + expectedLength > stream->size)
		retError(clean, Error_invalidParameter(0, 0, "DDS_read()::stream had invalid size"));

	currL = dds.header.depth;
	U64 totalSubResources = 0;

	for (U32 j = 0; j < dds.header.mips; ++j) {
		totalSubResources += currL;
		currL = U32_max(1, currL >> 1);
	}

	totalSubResources *= arraySize;

	//Output parsed result

	gotoIfError3(clean, ListSubResourceData_resize(result, totalSubResources, alloc, e_rr));

	for (U32 i = 0, l = 0; i < arraySize; ++i) {

		currW = dds.header.width;
		currH = dds.header.height;
		currL = dds.header.depth;

		for (U32 j = 0; j < dds.header.mips; ++j) {

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
		.w = dds.header.width,
		.h = dds.header.height,
		.l = dds.header.depth,
		.mips = dds.header.mips,
		.layers = arraySize,
		.type = type,
		.textureFormatId = formatId
	};

clean:
	return s_uccess;
}
