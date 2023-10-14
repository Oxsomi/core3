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
#include "types/types.h"
#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>

typedef struct VkManagedImage {

	VkImage image;
	VkImageView view;

	VkPipelineStageFlagBits2 lastStage;
	VkAccessFlagBits2 lastAccess;

	VkImageLayout lastLayout;
	U32 padding;

} VkManagedImage;

typedef struct VkGPUBuffer {

	VkBuffer buffer;

	U64 blockOffset;

	void *mappedMemory;						//Only accessible if not in flight

	VkPipelineStageFlagBits2 lastStage;
	VkAccessFlagBits2 lastAccess;

	VkImageLayout lastLayout;
	U32 blockId;

	U32 readDescriptor, writeDescriptor;

} VkGPUBuffer;

typedef struct CharString CharString;
typedef struct GraphicsDevice GraphicsDevice;
typedef struct Error Error;
typedef enum ETextureFormat ETextureFormat;
typedef enum EGraphicsDataTypes EGraphicsDataTypes;

Error vkCheck(VkResult result);

//Pass types as non NULL to allow validating if the texture format is supported.
//Sometimes you don't want this, for example when serializing.
Error mapVkFormat(
	EGraphicsDataTypes *types, 
	ETextureFormat format, 
	VkFormat *output, 
	Bool isForRenderTarget,
	Bool supportsUndefined			//Is format undefined supported?
);
