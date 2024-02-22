/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/list.h"

//The only thing defined is that the resource handle must be accessible on the device
//	using the allocated handle for the dedicated purpose.
//For example, device.allocateDescriptor(Texture2D) would do the following across APIs:
//	In DX12, Texture2D is a specific SRV range into a descriptor heap.
//	In Vulkan, Texture2D is a specific descriptor range into a descriptor set.

typedef enum EDescriptorType {

	EDescriptorType_Sampler,

	EDescriptorType_Texture2D,
	EDescriptorType_TextureCube,
	EDescriptorType_Texture3D,
	EDescriptorType_Buffer,

	EDescriptorType_RWBuffer,
	EDescriptorType_RWTexture3D,
	EDescriptorType_RWTexture3Ds,
	EDescriptorType_RWTexture3Df,
	EDescriptorType_RWTexture3Di,
	EDescriptorType_RWTexture3Du,
	EDescriptorType_RWTexture2D,
	EDescriptorType_RWTexture2Ds,
	EDescriptorType_RWTexture2Df,
	EDescriptorType_RWTexture2Di,
	EDescriptorType_RWTexture2Du,

	EDescriptorType_ResourceCount		//Count of totally accessible resources (without cbuffer)

} EDescriptorType;

typedef enum EDescriptorTypeCount {

	EDescriptorTypeCount_Sampler		= 2048,		//All samplers

	EDescriptorTypeCount_Texture2D		= 184464,	//~74% of textures (2D)
	EDescriptorTypeCount_TextureCube	= 32768,	//~13% of textures (Cube)
	EDescriptorTypeCount_Texture3D		= 32768,	//~13% of textures (3D)
	EDescriptorTypeCount_Buffer			= 249999,	//All buffers (readonly)
	EDescriptorTypeCount_RWBuffer		= 250000,	//All buffers (RW)
	EDescriptorTypeCount_RWTexture3D	= 6553,		//10% of 64Ki (3D + Cube) for RW 3D unorm
	EDescriptorTypeCount_RWTexture3Ds	= 4809,		//~7.3% of 64Ki (Remainder) (3D + Cube) for RW 3D snorm
	EDescriptorTypeCount_RWTexture3Df	= 43690,	//66% of 64Ki (3D + Cube) for RW 3D float
	EDescriptorTypeCount_RWTexture3Di	= 5242,		//8% of 64Ki (3D + Cube) for RW 3D int
	EDescriptorTypeCount_RWTexture3Du	= 5242,		//8% of 64Ki (3D + Cube) for RW 3D uint
	EDescriptorTypeCount_RWTexture2D	= 92232,	//50% of 250K - 64Ki (2D) for 2D unorm
	EDescriptorTypeCount_RWTexture2Ds	= 9224,		//5% of 250K - 64Ki (2D) for 2D snorm
	EDescriptorTypeCount_RWTexture2Df	= 61488,	//33% of 250K - 64Ki (2D) for 2D float
	EDescriptorTypeCount_RWTexture2Di	= 10760,	//5.8% of 250K - 64Ki (2D) for 2D int
	EDescriptorTypeCount_RWTexture2Du	= 10760,	//5.8% of 250K - 64Ki (2D) for 2D uint

	EDescriptorTypeCount_Textures		=
		EDescriptorTypeCount_Texture2D + EDescriptorTypeCount_Texture3D + EDescriptorTypeCount_TextureCube,

	EDescriptorTypeCount_RWTextures2D	=
		EDescriptorTypeCount_RWTexture2D  + EDescriptorTypeCount_RWTexture2Ds +	EDescriptorTypeCount_RWTexture2Df +
		EDescriptorTypeCount_RWTexture2Du + EDescriptorTypeCount_RWTexture2Di,

	EDescriptorTypeCount_RWTextures3D	=
		EDescriptorTypeCount_RWTexture3D  + EDescriptorTypeCount_RWTexture3Ds +	EDescriptorTypeCount_RWTexture3Df +
		EDescriptorTypeCount_RWTexture3Du + EDescriptorTypeCount_RWTexture3Di,

	EDescriptorTypeCount_RWTextures		= EDescriptorTypeCount_RWTextures2D + EDescriptorTypeCount_RWTextures3D,

	EDescriptorTypeCount_SSBO			= EDescriptorTypeCount_Buffer + EDescriptorTypeCount_RWBuffer

} EDescriptorTypeCount;

static const U32 descriptorTypeCount[] = {
	EDescriptorTypeCount_Sampler,
	EDescriptorTypeCount_Texture2D,
	EDescriptorTypeCount_TextureCube,
	EDescriptorTypeCount_Texture3D,
	EDescriptorTypeCount_Buffer,
	EDescriptorTypeCount_RWBuffer,
	EDescriptorTypeCount_RWTexture3D,
	EDescriptorTypeCount_RWTexture3Ds,
	EDescriptorTypeCount_RWTexture3Df,
	EDescriptorTypeCount_RWTexture3Di,
	EDescriptorTypeCount_RWTexture3Du,
	EDescriptorTypeCount_RWTexture2D,
	EDescriptorTypeCount_RWTexture2Ds,
	EDescriptorTypeCount_RWTexture2Df,
	EDescriptorTypeCount_RWTexture2Di,
	EDescriptorTypeCount_RWTexture2Du
};

typedef struct RefPtr RefPtr;
typedef RefPtr GraphicsDeviceRef;

//Lower 20 bit: id
//4 bit higher: descriptor type
U32 GraphicsDeviceRef_allocateDescriptor(GraphicsDeviceRef *device, EDescriptorType type);
Bool GraphicsDeviceRef_freeDescriptors(GraphicsDeviceRef *device, ListU32 *allocations);
