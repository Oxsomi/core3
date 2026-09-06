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

//graphics/generic/bindless_descriptor.h

#pragma once
#include "types/container/list.h"
#include "types/math/vec4.h"
#include "types/math/pack.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Bindless descriptors are defined as follows:
//17 bit descriptor array index
// 4 bit descriptor index (0 = reserved)
//0 is reserved and indicates invalid.
//This allows packing 3 bindless descriptors in 1 U64 or
// even 4 if you know the descriptor binding already (know it's valid) and know the arraySize < 65536.
//The descriptor index addresses what binding it's in (indirectly).
//
//[[vk::image_format("Unknown")]]
//Texture2D test[128];                    //Binding id = 0, bindless type index: 1
//Texture2D test2;                        //Binding id = 1, bindless type index: N/A
//Texture2D test2[63];                    //Binding id = 2, bindless type index: 2
//
//As stated above, only descriptor arrays are considered to be a bindless type, there can only be 15 bindless arrays.
//Bindless type index 0 is reserved as invalid and U21_MAX indicates InvalidAllocation.

typedef struct RefPtr RefPtr;
typedef RefPtr GraphicsDeviceRef;
typedef RefPtr DescriptorTableRef;

typedef struct DescriptorBinding DescriptorBinding;
typedef struct Descriptor Descriptor;

typedef U32 BindlessDescriptor;
typedef enum EGfxRegisterType EGfxRegisterType;

static const BindlessDescriptor BindlessDescriptor_InvalidAllocation = (1 << 21) - 1;
static const BindlessDescriptor BindlessDescriptor_None    = 0;

Bool GraphicsDeviceRef_allocateDescriptorBindless(
	GraphicsDeviceRef *device,
	DescriptorTableRef *descTable,        //Can be NULL in case the device has a default bindless table
	EGfxRegisterType type,
	U32 strideOrLength,                   //For structured/storage buffers
	Bool maintainRef,                     //Only if the resource isn't in charge of managing the descriptor
	const Descriptor *desc,
	BindlessDescriptor *descriptorHandle,
	Error *e_rr
);

Bool GraphicsDeviceRef_freeDescriptorBindless(
	GraphicsDeviceRef *device,
	DescriptorTableRef *descTable,        //Can be NULL in case the device has a default bindless table
	BindlessDescriptor descriptor,
	Error *e_rr
);

static inline U64 BindlessDescriptor_pack3(BindlessDescriptor a, BindlessDescriptor b, BindlessDescriptor c) {
	return U64_pack21x3(a, b, c);
}

//Has to unpack 21 bits per handle, since that's what pack3 puts in.
//Unpacking 20 would return the first handle intact but shift the other two, so the round trip wouldn't hold.

static inline I32x4 BindlessDescriptor_unpack3(U64 v) {
	return I32x4_create3(
		(I32) U64_unpack21x3(v, 0), (I32) U64_unpack21x3(v, 1), (I32) U64_unpack21x3(v, 2)
	);
}

static inline U8 BindlessDescriptor_getBindlessType(BindlessDescriptor handle) {
	return (U8)(handle >> 17);
}

static inline U32 BindlessDescriptor_getId(BindlessDescriptor handle) {
	return handle & ((1 << 17) - 1);
}

Bool BindlessDescriptor_isValid(GraphicsDeviceRef *device, DescriptorTableRef *descTable, BindlessDescriptor handle);

#ifdef __cplusplus
	}
#endif
