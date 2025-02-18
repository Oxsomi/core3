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

#pragma once
#include "types/base/types.h"
#include "types/base/error.h"
#include "types/base/atomic.h"
#include "types/container/ref_ptr.h"
#include "graphics/generic/resource.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum EDescriptorTableFlags {
	EDescriptorTableFlags_None
} EDescriptorTableFlags;

typedef RefPtr GraphicsDeviceRef;
typedef RefPtr DescriptorTableRef;
typedef RefPtr DescriptorHeapRef;
typedef RefPtr DescriptorLayoutRef;

typedef RefPtr TextureRef;
typedef RefPtr DeviceBufferRef;
typedef RefPtr TLASRef;
typedef RefPtr SamplerRef;

typedef struct DescriptorTable {

	DescriptorHeapRef *parent;
	DescriptorLayoutRef *layout;

	EDescriptorTableFlags flags;
	Bool acquiredAtomic;
	U8 padding[11];

} DescriptorTable;

#define DescriptorTable_ext(ptr, T) (!ptr ? NULL : (T##DescriptorTable*)(ptr + 1))		//impl
#define DescriptorTableRef_ptr(ptr) RefPtr_data(ptr, DescriptorTable)

Error DescriptorTableRef_dec(DescriptorTableRef **table);
Error DescriptorTableRef_inc(DescriptorTableRef *table);

Error DescriptorHeapRef_createDescriptorTable(
	DescriptorHeapRef *parent,
	DescriptorLayoutRef *layout,
	EDescriptorTableFlags flags,
	CharString name,
	DescriptorTableRef **table
);

typedef struct TextureDescriptorRange {

	U8 mipId, mipCount;
	U8 planeId, imageId;

	U16 arrayId, arrayCount;

	U64 padding[2];

} TextureDescriptorRange;

typedef struct DescriptorCounterOffset {
	U16 padding[3], counterOffset16;
} DescriptorCounterOffset;

typedef union DescriptorRegionAndCounter {
	U64 region48;
	DescriptorCounterOffset counter16;
} DescriptorRegionAndCounter;

typedef struct BufferDescriptorRange {
	DescriptorRegionAndCounter startRegionAndCounterOffset;
	DescriptorRegionAndCounter endRegionAndCounterOffset;
	DeviceBufferRef *counter;
} BufferDescriptorRange;

typedef struct Descriptor {

	RefPtr *resource;

	union {
		BufferDescriptorRange buffer;
		TextureDescriptorRange texture;
		U64 data[3];
	};

} Descriptor;

Descriptor Descriptor_texture(
	TextureRef *texture, U8 mipId, U8 mipCount, U8 planeId, U8 imageId, U16 arrayId, U16 arrayCount
);

//startRegion or endRegion >= U48_MAX it'll return a null descriptor
Descriptor Descriptor_buffer(
	DeviceBufferRef *buffer, U64 startRegion, U64 endRegion, DeviceBufferRef *counterResource, U32 counterOffset
);

Descriptor Descriptor_tlas(TLASRef *tlas);
Descriptor Descriptor_sampler(SamplerRef *sampler);

U64 Descriptor_startBuffer(Descriptor d);
U64 Descriptor_endBuffer(Descriptor d);
U64 Descriptor_bufferLength(Descriptor d);
U32 Descriptor_counterOffset(Descriptor d);

Bool DescriptorTableRef_setDescriptor(
	DescriptorTableRef *table,
	U64 i,					//ListDescriptorBinding[i]
	U64 arrayId,			//arrayId into descriptor
	Descriptor d,
	Error *e_rr
);

//Resolves register name to binding index, returns U64_MAX if invalid
U64 DescriptorTableRef_resolveRegisterName(DescriptorTableRef *table, CharString registerName);

Bool DescriptorTableRef_setDescriptorByName(
	DescriptorTableRef *table,
	CharString registerName,
	U64 arrayId,			//arrayId into descriptor
	Descriptor d,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
