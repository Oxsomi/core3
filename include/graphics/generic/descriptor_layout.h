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
#include "platforms/ext/listx.h"
#include "types/base/error.h"
#include "types/container/ref_ptr.h"
#include "formats/oiSH/registers.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct SHFile SHFile;
typedef RefPtr GraphicsDeviceRef;

typedef enum EDescriptorLayoutFlags {

	EDescriptorLayoutFlags_None						= 0,
	EDescriptorLayoutFlags_AllowBindlessOnArrays	= 1 << 0,		//Required to use bindless, only enables bindless on desc[]
	EDescriptorLayoutFlags_AllowBindlessEverywhere	= 1 << 1,		//Potentially slower, will assume all registers are dynamic
	EDescriptorLayoutFlags_InternalWeakDeviceRef	= 1 << 2,
	EDescriptorLayoutFlags_HasPushDescriptors		= 1 << 3,		//Push descriptor set, no other descriptors allowed

	EDescriptorLayoutFlags_AllowBindlessAny			=
		EDescriptorLayoutFlags_AllowBindlessOnArrays | EDescriptorLayoutFlags_AllowBindlessEverywhere

} EDescriptorLayoutFlags;

typedef U16 DescriptorBindingFlags;

typedef struct DescriptorBinding {

	ESHRegisterType registerType;

	U32 count;

	SHBinding binding;

	U32 visibility;			//Bit mask of ESHPipelineStage

	union {
		U32 structedBufferStride;
		U32 constantBufferSize;
		SHTextureFormat textureFormat;
		U32 data;
	};

} DescriptorBinding;

Bool DescriptorBinding_overlaps(
	DescriptorBinding binding,
	ESHRegisterType regType,
	SHBinding b,
	U32 bcount,
	ESHBinaryType type,
	Bool isPushConstant
);

TList(DescriptorBinding);

typedef struct DescriptorLayoutInfo {

	EDescriptorLayoutFlags flags;
	U32 padding;

	ListDescriptorBinding bindings;
	ListCharString bindingNames;

} DescriptorLayoutInfo;

typedef enum EDetectDescriptorLayoutFlags {

	EDetectDescriptorLayoutFlags_None					= 0,

	//These should only be used if the contents are so frequently changing that allocating a descriptor set makes no sense.
	//For example a copy image or mip map shader, otherwise use None.
	//These only apply on non arrays, since those could be used for bindless for example.
	//Samplers need static samplers, doesn't work with push descriptors.

	EDetectDescriptorLayoutFlags_AssumePushConstants	= 1 << 0,		//First buffer (<128 bytes) receives a push constant
	EDetectDescriptorLayoutFlags_AssumePushDescriptors	= 1 << 1		//Assume non push constant buffer as push descriptors

} EDetectDescriptorLayoutFlags;

Bool GraphicsDeviceRef_detectLayoutFromEntries(
	GraphicsDeviceRef *dev,
	SHFile tmpBinary,
	ListU32 entrypoints,				//U32 (U16 entryId, binaryId)
	EDescriptorLayoutFlags flags,
	EDetectDescriptorLayoutFlags detectFlags,
	ListCharString pushDescriptors,		//Empty if no push descriptors or if AssumePushConstants
	CharString pushConstantName,		//Empty if no push constants or AssumePushConstants
	DescriptorBinding *pushConstantOut,
	DescriptorLayoutInfo *info,
	DescriptorLayoutInfo *pushDescriptorInfo,
	Error *e_rr
);

Bool GraphicsDeviceRef_detectLayoutFromEntry(
	GraphicsDeviceRef *dev,
	SHFile binary,
	U32 entrypoint,						//U32 (U16 entryId, binaryId)
	EDescriptorLayoutFlags flags,
	EDetectDescriptorLayoutFlags detectFlags,
	ListCharString pushDescriptors,		//Empty if no push descriptors or if AssumePushDescriptors
	CharString pushConstantName,		//Empty if no push constants or AssumePushConstants
	DescriptorBinding *pushConstantOut,
	DescriptorLayoutInfo *info,
	DescriptorLayoutInfo *pushDescriptorInfo,
	Error *e_rr
);

void DescriptorLayoutInfo_free(DescriptorLayoutInfo *info, Allocator alloc);

typedef RefPtr GraphicsDeviceRef;
typedef RefPtr DescriptorLayoutRef;

typedef struct DescriptorLayout {

	GraphicsDeviceRef *device;
	DescriptorLayoutInfo info;

	ListU16 bindlessTypeToBinding;
	ListU8 bindingToBindlessType;		//U8_MAX indicates "none"

	Bool anySampler, anyResource;
	U8 padding[14];

} DescriptorLayout;

#define DescriptorLayout_ext(ptr, T) (!ptr ? NULL : (T##DescriptorLayout*)(ptr + 1))		//impl
#define DescriptorLayoutRef_ptr(ptr) RefPtr_data(ptr, DescriptorLayout)

void DescriptorLayoutRef_dec(DescriptorLayoutRef **layout);
Error DescriptorLayoutRef_inc(DescriptorLayoutRef *layout);

Error GraphicsDeviceRef_createDescriptorLayout(
	GraphicsDeviceRef *dev,
	DescriptorLayoutInfo *info,		//Moves info
	CharString name,
	DescriptorLayoutRef **layout
);

#ifdef __cplusplus
	}
#endif
