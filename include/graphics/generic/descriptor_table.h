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

//graphics/generic/descriptor_table.h

#pragma once
#include "types/base/types.h"
#include "types/base/error.h"
#include "types/base/atomic.h"
#include "types/base/lock.h"
#include "types/container/ref_ptr.h"
#include "graphics/generic/resource.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct CharString CharString;

typedef enum EDescriptorTableFlags {
	EDescriptorTableFlags_None                        = 0,
	EDescriptorTableFlags_InternalWeakDeviceRef       = 1 << 0
} EDescriptorTableFlags;

typedef U8 DescriptorTableFlags;

typedef RefPtr GraphicsDeviceRef;
typedef RefPtr DescriptorTableRef;
typedef RefPtr DescriptorHeapRef;
typedef RefPtr DescriptorLayoutRef;

typedef RefPtr TextureRef;
typedef RefPtr DeviceBufferRef;
typedef RefPtr TLASRef;
typedef RefPtr SamplerRef;

typedef enum ESHRegisterType ESHRegisterType;

typedef struct TextureDescriptorRange {

	U8 mipId, mipCount;
	U8 planeId, imageId;

	U16 arrayId, arrayCount;

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

TList(Descriptor);
TList(TextureDescriptorRange);
TList(BufferDescriptorRange);

typedef struct DescriptorStackTrace {

	#ifndef NDEBUG
		void *stackTrace[8];
	#else
		void *stackTrace[4];
	#endif

} DescriptorStackTrace;

TList(DescriptorStackTrace);

typedef struct DescriptorTableBindingMultiple {

	ListU64 activeList;         //Quicker than indexing descriptors
	ListU64 maintainRef;        //If the resource affects ListDescriptorTableResourceRef in DescriptorTable
	ListWeakRefPtr resources;
	ListDescriptorStackTrace stackTraces;

	union {
		ListBufferDescriptorRange buffers;
		ListTextureDescriptorRange textures;       //This is 3x less mem than buffer descs
	};

} DescriptorTableBindingMultiple;

typedef struct DescriptorTableBindingSingle {

	DescriptorStackTrace stackTrace;
	WeakRefPtr *resource;
	Bool maintainRef;
	U8 padding0[7];

	union {
		BufferDescriptorRange buffer;
		TextureDescriptorRange texture;            //This is 3x less mem than buffer descs
		U64 data[3];
	};

} DescriptorTableBindingSingle;

typedef struct DescriptorTableBinding {

	SpinLock lock;

	union {
		DescriptorTableBindingMultiple multiple;
		DescriptorTableBindingSingle single;
	};

} DescriptorTableBinding;

typedef struct DescriptorTableResourceRef {
	RefPtr *resource;
	U64 count;
} DescriptorTableResourceRef;

TList(DescriptorTableBinding);
TList(DescriptorTableResourceRef);

typedef struct DescriptorTable {

	DescriptorHeapRef *parent;
	DescriptorLayoutRef *layout;

	DescriptorTableFlags flags;
	Bool acquiredAtomic;
	U8 padding[6];

	SpinLock lock;                                   //To access resources

	ListDescriptorTableResourceRef resources;        //All resources that are bound by the table
	ListDescriptorTableBinding bindings;

} DescriptorTable;

#define DescriptorTable_ext(ptr, T) (!ptr ? NULL : (T##DescriptorTable*)(ptr + 1))        //impl
#define DescriptorTableRef_ptr(ptr) RefPtr_data(ptr, DescriptorTable)

void DescriptorTableRef_dec(DescriptorTableRef **table);
Error DescriptorTableRef_inc(DescriptorTableRef *table);

Error DescriptorHeapRef_createDescriptorTable(
	DescriptorHeapRef *parent,
	DescriptorLayoutRef *layout,
	EDescriptorTableFlags flags,
	CharString name,
	DescriptorTableRef **table
);

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

//Note: When setting descriptors, ensure all descriptors are valid (e.g. resource != NULL)
//        not all implementations support null descriptors, as such, setting all descriptors in a range to NULL
//        is unexpected behavior.
//        For telling our front end that descriptors are free use unsetDescriptor(s)(byName).

Bool DescriptorTableRef_setDescriptors(
	DescriptorTableRef *table,
	U64 bindId,             //ListDescriptorBinding[i]
	U64 arrayId,            //arrayId into descriptor
	Bool maintainRef,
	ListDescriptor d,
	Error *e_rr
);

Bool DescriptorTableRef_unsetDescriptors(
	DescriptorTableRef *table,
	U64 bindId,             //ListDescriptorBinding[i]
	U64 arrayId,            //arrayId into descriptor
	U64 count,
	Error *e_rr
);

Bool DescriptorTableRef_setDescriptor(
	DescriptorTableRef *table,
	U64 bindId,             //ListDescriptorBinding[i]
	U64 arrayId,            //arrayId into descriptor
	Bool maintainRef,
	Descriptor d,
	Error *e_rr
);

Bool DescriptorTableRef_allocDescriptor(
	DescriptorTableRef *table,
	U64 bindId,             //ListDescriptorBinding[i]
	U64 *arrayId,           //outputs arrayId into descriptor if success
	Bool maintainRef,
	Descriptor d,
	Error *e_rr
);

//Find a bindless register of the relevant type for the current format, descriptor type, etc.
//This will only try to find it if the register type matches.
Bool DescriptorTableRef_findBindlessRegister(
	DescriptorTableRef *table,
	ESHRegisterType type,
	U32 strideOrLength,     //Used to find a resource of this size (can be 0)
	U16 *bindId,
	U8 *bindlessTypeId,
	RefPtr *resource,
	U8 planeId,             //Useful for depth buffer (stencil read)
	Error *e_rr
);

//Finds a bindless descriptor by resource type and allocate into the array.
//allocDescriptor(ByName) can still be used if you know the binding but want to allocate into it.
//This even works with normal descriptor sets.
Bool DescriptorTableRef_allocDescriptorBindless(
	DescriptorTableRef *table,
	ESHRegisterType type,
	U32 strideOrLength,     //Used to find a resource of this size (can be 0)
	U16 *bindId,            //ListDescriptorBinding[i]
	U8 *bindlessTypeId,     //Index for bindless handle
	U64 *arrayId,           //outputs arrayId into descriptor if success
	Bool maintainRef,
	Descriptor d,
	Error *e_rr
);

//Resolves register name to binding index, returns U64_MAX if invalid
U64 DescriptorTableRef_resolveRegisterName(DescriptorTableRef *table, CharString registerName);

Bool DescriptorTableRef_setDescriptorByName(
	DescriptorTableRef *table,
	CharString registerName,
	U64 arrayId,            //arrayId into descriptor
	Bool maintainRef,
	Descriptor d,
	Error *e_rr
);

Bool DescriptorTableRef_setDescriptorsByName(
	DescriptorTableRef *table,
	CharString registerName,
	U64 arrayId,            //arrayId into descriptor
	Bool maintainRef,
	ListDescriptor d,
	Error *e_rr
);

Bool DescriptorTableRef_unsetDescriptorsByName(
	DescriptorTableRef *table,
	CharString registerName,
	U64 arrayId,
	U64 count,
	Error *e_rr
);

Bool DescriptorTableRef_allocDescriptorByName(
	DescriptorTableRef *table,
	CharString registerName,
	U64 *arrayId,           //outputs arrayId into descriptor if success
	Descriptor d,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
