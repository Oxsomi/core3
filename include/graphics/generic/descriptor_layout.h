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
#include "platforms/ext/listx.h"
#include "types/base/error.h"
#include "types/container/ref_ptr.h"
#include "formats/oiSH/registers.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum EDescriptorLayoutFlags {

	EDescriptorLayoutFlags_None						= 0,
	EDescriptorLayoutFlags_AllowBindlessOnArrays	= 1 << 0,		//Required to use bindless, only enables bindless on desc[]
	EDescriptorLayoutFlags_AllowBindlessEverywhere	= 1 << 1,		//Potentially slower, will assume all registers are dynamic
	EDescriptorLayoutFlags_InternalWeakDeviceRef	= 1 << 2,

	EDescriptorLayoutFlags_AllowBindlessAny			=
		EDescriptorLayoutFlags_AllowBindlessOnArrays | EDescriptorLayoutFlags_AllowBindlessEverywhere

} EDescriptorLayoutFlags;

typedef struct DescriptorBinding {
	ESHRegisterType registerType;
	U32 count;
	U32 space;
	U32 id;
	U32 visibility;			//Bit mask of ESHPipelineStage
	U32 strideOrLength;		//Constant buffers; length, structured buffers: stride
} DescriptorBinding;

TList(DescriptorBinding);

typedef struct DescriptorLayoutInfo {

	EDescriptorLayoutFlags flags;
	U32 padding;

	ListDescriptorBinding bindings;
	ListCharString bindingNames;

} DescriptorLayoutInfo;

void DescriptorLayoutInfo_free(DescriptorLayoutInfo *info, Allocator alloc);

typedef RefPtr GraphicsDeviceRef;
typedef RefPtr DescriptorLayoutRef;

typedef struct DescriptorLayout {
	GraphicsDeviceRef *device;
	DescriptorLayoutInfo info;
} DescriptorLayout;

#define DescriptorLayout_ext(ptr, T) (!ptr ? NULL : (T##DescriptorLayout*)(ptr + 1))		//impl
#define DescriptorLayoutRef_ptr(ptr) RefPtr_data(ptr, DescriptorLayout)

Error DescriptorLayoutRef_dec(DescriptorLayoutRef **layout);
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
