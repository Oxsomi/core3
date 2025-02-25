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
#include "types/container/list.h"
#include "types/math/vec.h"

#ifdef __cplusplus
	extern "C" {
#endif

//The only thing defined is that the resource handle must be accessible on the device
//	using the allocated handle for the dedicated purpose.
//For example, device.allocateDescriptor(Texture2D) would do the following across APIs:
//	In D3D12, Texture2D is a specific SRV range into a descriptor heap.
//	In Vulkan, Texture2D is a specific descriptor range into a descriptor set.

typedef enum EDescriptorType {

	EDescriptorType_Invalid,		//Used for null descriptors (e.g. descriptor id 0)
	EDescriptorType_Texture2D,
	EDescriptorType_TextureCube,
	EDescriptorType_Texture3D,
	EDescriptorType_Buffer,

	EDescriptorType_RWBuffer,
	EDescriptorType_RWTexture3D,
	EDescriptorType_RWTexture3Di,
	EDescriptorType_RWTexture3Du,
	EDescriptorType_RWTexture2D,
	EDescriptorType_RWTexture2Di,
	EDescriptorType_RWTexture2Du,

	EDescriptorType_Sampler,
	EDescriptorType_TLASExt,

	EDescriptorType_Count

} EDescriptorType;

typedef enum EDescriptorTypeCount {
	EDescriptorTypeCount_Texture2D		= 131072,
	EDescriptorTypeCount_TextureCube	= 32768,
	EDescriptorTypeCount_Texture3D		= 32768,
	EDescriptorTypeCount_Buffer			= 131072,
	EDescriptorTypeCount_RWBuffer		= 131072,
	EDescriptorTypeCount_RWTexture3D	= 32768,
	EDescriptorTypeCount_RWTexture3Di	= 8192,
	EDescriptorTypeCount_RWTexture3Du	= 8192,
	EDescriptorTypeCount_RWTexture2D	= 131072,
	EDescriptorTypeCount_RWTexture2Di	= 16384,
	EDescriptorTypeCount_RWTexture2Du	= 16384,
	EDescriptorTypeCount_Sampler		= 2048,		//All samplers
	EDescriptorTypeCount_TLASExt		= 16
} EDescriptorTypeCount;

static const U32 descriptorTypeCount[] = {
	0,
	EDescriptorTypeCount_Texture2D,
	EDescriptorTypeCount_TextureCube,
	EDescriptorTypeCount_Texture3D,
	EDescriptorTypeCount_Buffer,
	EDescriptorTypeCount_RWBuffer,
	EDescriptorTypeCount_RWTexture3D,
	EDescriptorTypeCount_RWTexture3Di,
	EDescriptorTypeCount_RWTexture3Du,
	EDescriptorTypeCount_RWTexture2D,
	EDescriptorTypeCount_RWTexture2Di,
	EDescriptorTypeCount_RWTexture2Du,
	EDescriptorTypeCount_Sampler,
	EDescriptorTypeCount_TLASExt
};

typedef struct RefPtr RefPtr;
typedef RefPtr GraphicsDeviceRef;

//0 and U32_MAX are reserved and indicate "no allocation" and "invalid allocation" respectively
//Lower 17 bit: id
//4 bits (<<18): descriptor type, descriptor type 0 is invalid

U32 GraphicsDeviceRef_allocateDescriptor(GraphicsDeviceRef *device, EDescriptorType type);
Bool GraphicsDeviceRef_freeDescriptors(GraphicsDeviceRef *device, ListU32 *allocations);

U64 ResourceHandle_pack3(U32 a, U32 b, U32 c);
I32x4 ResourceHandle_unpack3(U64 v);

EDescriptorType ResourceHandle_getType(U32 handle);
U32 ResourceHandle_getId(U32 handle);
Bool ResourceHandle_isValid(U32 handle);

#ifdef __cplusplus
	}
#endif
