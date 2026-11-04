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

//formats/dds/dds_file.h

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

//One depth slice of one mip of one layer. The pixels stay in the stream; nothing is copied out of it.
typedef struct SubResourceData {
	U32 mipId, layerId, z, padding;        //z is the slice within this mip, whose depth halves per mip. padding is ignored.
	StreamRef *stream;                     //Entries from DDS_read own a reference that ListSubResourceData_freeUnderlying drops
	U64 streamOff;                         //In bytes, from the start of stream
	U64 streamLen;                         //In bytes, exactly one slice: the size of the mip's width x height at depth 1
} SubResourceData;

typedef struct DDSInfo {

	U32 w, h, l;                        //Dimensions; these decrease by ~2 for each mip. l is depth, 1 if not 3D.
	U32 mips, layers;                  //For cubes, layers counts faces (a multiple of 6).

	TextureFormatId textureFormatId;
	TextureType type;
	U16 pad;                           //Ignored by DDS_write and left 0 by DDS_read

} DDSInfo;

TList(SubResourceData);

//buf must hold exactly one entry per (layer, mip, z) of info, each streamLen equal to the slice size. It is sorted in place
// (layer, then mip, then z) before validation, so it is reordered even when the call fails.
//The sources are only read: buf and its streams stay owned by the caller, and no reference is taken or dropped.
//*streamOffset is where the DDS starts and is advanced to its end only on success.
//Pass a NULL stream to only measure: nothing is written and *streamOffset advances by the file's size.
//A failure after the header started can leave the stream partially written and reserved.
Bool DDS_write(
	StreamRef *stream,
	U64 *streamOffset,        //Required, even when stream is NULL
	ListSubResourceData *buf,
	const DDSInfo *info,
	const Allocator *alloc,
	Error *e_rr
);

//*streamOff is where the DDS starts and is only advanced when the read succeeds (to the end of the last subresource).
//Sizes in the header are checked against the bytes in the stream before anything is allocated from them.
//A zero mip count denotes a single mip. Legacy RGBA8 and BGRA8 channel masks are accepted.
//result has to be empty (NULL ptr) on entry. Pixels are not read: each entry refs stream
// (one added reference each) and points into it, so the stream's bytes have to stay unchanged while the entries are used.
//On failure info, result and *streamOff are untouched.
Bool DDS_read(
	StreamRef *stream,
	U64 *streamOff,
	DDSInfo *info,
	const Allocator *alloc,
	ListSubResourceData *result,
	Error *e_rr
);

//Drops the stream reference of every entry, then frees the list. Every entry must own one reference (as DDS_read gives).
void ListSubResourceData_freeUnderlying(ListSubResourceData *buf, const Allocator *alloc);

#ifdef __cplusplus
	}
#endif
