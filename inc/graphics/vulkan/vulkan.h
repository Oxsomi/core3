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
#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>

typedef struct VkManagedImage {

	VkImage image;
	VkImageView view;

	VkPipelineStageFlagBits2 lastStage;
	VkAccessFlagBits2 lastAccess;

	VkImageLayout lastLayout;
	U32 readHandle;

	U32 writeHandle;
	U32 blockId;			//If specifically allocated, indicates which block this is present in

	U64 blockOffset;

} VkManagedImage;

typedef struct CharString CharString;
typedef struct GraphicsDevice GraphicsDevice;
typedef struct Error Error;
typedef enum ETextureFormat ETextureFormat;
typedef enum EGraphicsDataTypes EGraphicsDataTypes;
typedef enum ECompareOp ECompareOp;

TList(VkMappedMemoryRange);
TList(VkBufferCopy);
TList(VkImageCopy);
TList(VkBufferImageCopy);
TList(VkImageMemoryBarrier2);
TList(VkBufferMemoryBarrier2);

Error vkCheck(VkResult result);

//Pass types as non NULL to allow validating if the texture format is supported.
//Sometimes you don't want this, for example when serializing.
VkFormat mapVkFormat(ETextureFormat format);

VkCompareOp mapVkCompareOp(ECompareOp op);

//Transitions entire resource rather than subresources

typedef struct ListVkImageMemoryBarrier2 ListVkImageMemoryBarrier2;

Error VkManagedImage_transition(
	VkManagedImage *image,
	VkPipelineStageFlags2 stage,
	VkAccessFlagBits2 access,
	VkImageLayout layout,
	U32 graphicsQueueId,
	const VkImageSubresourceRange *range,
	ListVkImageMemoryBarrier2 *imageBarriers,
	VkDependencyInfo *dependency
);
