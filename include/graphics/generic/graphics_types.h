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

//graphics/generic/graphics_types.h

#pragma once
#include "types/base/type_id.h"

#ifdef __cplusplus
	extern "C" {
#endif

//TypeId but for graphics objects.

typedef enum EGraphicsTypeId {

	EGraphicsTypeId_GraphicsInstance             = makeObjectId(0x1C34,  0, 0),
	EGraphicsTypeId_GraphicsDevice               = makeObjectId(0x1C34,  1, 0),

	EGraphicsTypeId_Swapchain                    = makeObjectId(0x1C34,  2, 0),
	EGraphicsTypeId_CommandList                  = makeObjectId(0x1C34,  3, 0),

	EGraphicsTypeId_RenderTexture                = makeObjectId(0x1C34,  4, 0),
	EGraphicsTypeId_RenderPass                   = makeObjectId(0x1C34,  5, 0),

	EGraphicsTypeId_DeviceTexture                = makeObjectId(0x1C34,  6, 0),
	EGraphicsTypeId_DeviceBuffer                 = makeObjectId(0x1C34,  7, 0),
	EGraphicsTypeId_Pipeline                     = makeObjectId(0x1C34,  8, 0),
	EGraphicsTypeId_Sampler                      = makeObjectId(0x1C34,  9, 0),

	EGraphicsTypeId_DepthStencil                 = makeObjectId(0x1C34, 10, 0),

	EGraphicsTypeId_DescriptorLayout             = makeObjectId(0x1C34, 11, 0),
	EGraphicsTypeId_PipelineLayout               = makeObjectId(0x1C34, 12, 0),
	EGraphicsTypeId_DescriptorTable              = makeObjectId(0x1C34, 13, 0),
	EGraphicsTypeId_DescriptorHeap               = makeObjectId(0x1C34, 14, 0),

	//Requires EGraphicsFeatures_Raytracing

	EGraphicsTypeId_BLASExt                      = makeObjectId(0x1C34, 15, 0),
	EGraphicsTypeId_TLASExt                      = makeObjectId(0x1C34, 16, 0),

	EGraphicsTypeId_Count                        = 17

} EGraphicsTypeId;

extern EGraphicsTypeId EGraphicsTypeId_all[EGraphicsTypeId_Count];

//How big a backend's extension struct is, together with the alignment that struct needs.
//The two travel as one value because anything appending such a struct has to honour both, and taking the size
// while forgetting the alignment is exactly what used to leave every VkUnifiedTexture (which holds a SpinLock,
// so 64) sitting on a 32 byte boundary.
//It's a struct rather than a bare U32 so that arithmetic on it doesn't compile at all; the alternative was
// renaming every field and hoping nobody reintroduced a raw byte offset later.
//The size is in the low 24 bits (16 MiB, and every entry is a plain sizeof of a handful of pointers) and the
// alignment itself is in the top byte (a power of two, never above 128).
//This lives here rather than next to GraphicsObjectSizes in interface.h because texture.h needs it and is
// included from underneath that header.

typedef struct GraphicsObjectSize {
	U32 sizeAndAlignment;
} GraphicsObjectSize;

#define GraphicsObjectSize_create(T) { (U32) sizeof(T) | ((U32) alignof(T) << 24) }

//For the entries that pad past their own size so whatever follows them lands right; the alignment stays the
//struct's own, since that is what the struct itself requires.

#define GraphicsObjectSize_createPadded(T, extra) { (U32) (sizeof(T) + (extra)) | ((U32) alignof(T) << 24) }

//A backend without the object at all still has to report an alignment its consumers can align to.

#define GraphicsObjectSize_createRaw(size, alignment) { (U32) (size) | ((U32) (alignment) << 24) }

static inline U64 GraphicsObjectSize_size(GraphicsObjectSize s) { return s.sizeAndAlignment & 0xFFFFFF; }
static inline U64 GraphicsObjectSize_alignment(GraphicsObjectSize s) { return s.sizeAndAlignment >> 24; }

static inline U64 GraphicsObjectSize_alignUp(U64 offset, U64 alignment) {
	return alignment <= 1 ? offset : ((offset + alignment - 1) & ~(alignment - 1));
}

//What one block costs when they're laid out back to back: every block after the first has to start aligned
//too, so the size is rounded up to the struct's own alignment.

static inline U64 GraphicsObjectSize_stride(GraphicsObjectSize s) {
	return GraphicsObjectSize_alignUp(GraphicsObjectSize_size(s), GraphicsObjectSize_alignment(s));
}

#ifdef __cplusplus
	}
#endif
