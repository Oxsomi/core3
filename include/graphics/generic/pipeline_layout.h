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

typedef enum EPipelineLayoutFlags {
	EPipelineLayoutFlags_None						= 0,
	EPipelineLayoutFlags_InternalWeakDeviceRef		= 1 << 1
} EPipelineLayoutFlags;

typedef RefPtr DescriptorLayoutRef;

typedef struct PipelineLayoutInfo {

	EPipelineLayoutFlags flags;
	U32 padding;

	DescriptorLayoutRef *bindings;

} PipelineLayoutInfo;

typedef RefPtr GraphicsDeviceRef;
typedef RefPtr PipelineLayoutRef;

typedef struct PipelineLayout {
	GraphicsDeviceRef *device;
	U64 padding;
	PipelineLayoutInfo info;
} PipelineLayout;

#define PipelineLayout_ext(ptr, T) (!ptr ? NULL : (T##PipelineLayout*)(ptr + 1))		//impl
#define PipelineLayoutRef_ptr(ptr) RefPtr_data(ptr, PipelineLayout)

Error PipelineLayoutRef_dec(PipelineLayoutRef **layout);
Error PipelineLayoutRef_inc(PipelineLayoutRef *layout);

Error GraphicsDeviceRef_createPipelineLayout(
	GraphicsDeviceRef *dev,
	PipelineLayoutInfo info,		//Moves info
	CharString name,
	PipelineLayoutRef **layout
);

#ifdef __cplusplus
	}
#endif
