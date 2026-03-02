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

typedef struct RefPtr RefPtr;
typedef RefPtr StreamRef;

typedef struct SubResourceData {
	U32 mipId, layerId, z, padding;
	StreamRef *stream;
	U64 streamOff;
	U64 streamLen;
} SubResourceData;

typedef struct DDSInfo {

	U32 w, h, l;						//Dimensions; these decrease by ~2 for each mip
	U32 mips, layers;

	TextureFormatId textureFormatId;
	TextureType type;
	U16 pad;

} DDSInfo;

TList(SubResourceData);

//buf's contents are sorted based on subresource index, so needs a non const buf
Bool DDS_write(
	StreamRef *stream,
	U64 *streamOffset,
	ListSubResourceData *buf,
	const DDSInfo *info,
	const Allocator *alloc,
	Error *e_rr
);

Bool DDS_read(
	StreamRef *stream,
	U64 *streamOff,
	DDSInfo *info,
	const Allocator *alloc,
	ListSubResourceData *result,
	Error *e_rr
);

void ListSubResourceData_freeUnderlying(ListSubResourceData *buf, const Allocator *alloc);

#ifdef __cplusplus
	}
#endif
