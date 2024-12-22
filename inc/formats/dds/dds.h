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

#pragma once
#include "types/base/types.h"
#include "types/container/list.h"
#include "types/container/texture_format.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct Error Error;
typedef enum EDepthStencilFormat EDepthStencilFormat;

typedef struct SubResourceData {
	U32 mipId, layerId, z, padding;
	U64 dataOffset, dataLength;			//Points either into input file or resourceData
} SubResourceData;

typedef struct DDSInfo {

	U32 w, h, l;						//Dimensions; these decrease by ~2 for each mip
	U32 mips, layers;

	ETextureFormatId textureFormatId;
	ETextureType type;

} DDSInfo;

TList(SubResourceData);

typedef struct Stream Stream;

Bool DDS_write(Stream *resourceData, ListSubResourceData bufMutable, DDSInfo info, Stream **result, Error *e_rr);
Bool DDS_read(Stream *stream, DDSInfo *info, Allocator allocator, ListSubResourceData *result, Error *e_rr);
Bool ListSubResourceData_freeAll(ListSubResourceData *buf, Allocator allocator);

Bool DDS_readx(Stream *stream, DDSInfo *info, ListSubResourceData *result, Error *e_rr);
Bool ListSubResourceData_freeAllx(ListSubResourceData *buf);

#ifdef __cplusplus
	}
#endif
