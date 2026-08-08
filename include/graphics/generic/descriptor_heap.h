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

//graphics/generic/descriptor_heap.h

#pragma once
#include "types/base/types.h"
#include "types/base/atomic.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct CharString CharString;
typedef struct Error Error;

typedef enum EDescriptorHeapFlags {
	EDescriptorHeapFlags_None                     = 0,
	EDescriptorHeapFlags_AllowBindless            = 1 << 0,        //Required to use bindless
	EDescriptorHeapFlags_InternalWeakDeviceRef    = 1 << 1
} EDescriptorHeapFlags;

typedef struct DescriptorHeapInfo {

	EDescriptorHeapFlags flags;

	U16 maxAccelerationStructures;
	U16 maxSamplers;

	U16 maxInputAttachments;
	U16 maxCombinedSamplers;

	U32 maxTextures;

	U32 maxTexturesRW;

	U32 maxBuffersRW;

	U32 maxConstantBuffers;

	U32 maxDescriptorTables;

} DescriptorHeapInfo;

typedef struct RefPtr RefPtr;
typedef RefPtr GraphicsDeviceRef;
typedef RefPtr DescriptorHeapRef;

typedef struct DescriptorHeap {

	GraphicsDeviceRef *device;

	DescriptorHeapInfo info;
	U32 padding[3];

	AtomicI64 descriptorTableCount;

} DescriptorHeap;

#define DescriptorHeap_ext(ptr, T) (!ptr ? NULL : (T##DescriptorHeap*)(ptr + 1))        //impl
#define DescriptorHeapRef_ptr(ptr) RefPtr_data(ptr, DescriptorHeap)

Bool GraphicsDeviceRef_createDescriptorHeap(
	GraphicsDeviceRef *dev,
	const DescriptorHeapInfo *info,
	const CharString *name,
	DescriptorHeapRef **heap,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
