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

#include "platforms/ext/listx.h"
#include "graphics/generic/interface.h"
#include "graphics/vulkan/vk_interface.h"
#include "graphics/vulkan/vulkan.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "platforms/ext/stringx.h"
#include "types/container/texture_format.h"

Bool VK_WRAP_FUNC(UnifiedTexture_free)(TextureRef *textureRef) {

	const UnifiedTexture utex = TextureRef_getUnifiedTexture(textureRef, NULL);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(utex.resource.device);
	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	for(U8 i = 0; i < utex.images; ++i) {

		VkUnifiedTexture *image = TextureRef_getImgExtT(textureRef, Vk, 0, i);

		for (U64 j = 0; j < image->views.length; ++j)
			deviceExt->destroyImageView(deviceExt->device, image->views.ptr[i].view, NULL);

		ListVkImageViewMapping_freex(&image->views);

		if(image->image && utex.resource.type != EResourceType_Swapchain)
			deviceExt->destroyImage(deviceExt->device, image->image, NULL);
	}

	return true;
}

UnifiedTexture *TextureRef_getUnifiedTextureIntern(TextureRef *tex, DeviceResourceVersion *version);

Error VK_WRAP_FUNC(UnifiedTexture_create)(TextureRef *textureRef, CharString name) {

	UnifiedTexture *texture = TextureRef_getUnifiedTextureIntern(textureRef, NULL);

	//Prepare temporary free-ables and extended data.

	Error err = Error_none();
	CharString temp = CharString_createNull();

	GraphicsDevice *device = GraphicsDeviceRef_ptr(texture->resource.device);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	VkFormat vkFormat = VK_FORMAT_UNDEFINED;

	switch(texture->depthFormat) {
		case EDepthStencilFormat_D16:			vkFormat = VK_FORMAT_D16_UNORM;					break;
		case EDepthStencilFormat_D32:			vkFormat = VK_FORMAT_D32_SFLOAT;				break;
		case EDepthStencilFormat_D24S8Ext:		vkFormat = VK_FORMAT_D24_UNORM_S8_UINT;			break;
		case EDepthStencilFormat_D32S8X24Ext:	vkFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;		break;
		case EDepthStencilFormat_S8X24Ext:		vkFormat = VK_FORMAT_S8_UINT;					break;
	}

	if(!vkFormat)
		vkFormat = mapVkFormat(ETextureFormatId_unpack[texture->textureFormatId]);

	Bool isDeviceTexture = texture->resource.type == EResourceType_DeviceTexture;

	VkImageCreateInfo imageInfo = (VkImageCreateInfo) {

		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = texture->type == ETextureType_2D ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D,
		.format = vkFormat,
		.extent = (VkExtent3D) { .width = texture->width, .height = texture->height, .depth = texture->length },
		.mipLevels = texture->levels,
		.arrayLayers = 1,
		.samples = (VkSampleCountFlagBits) (1 << texture->sampleCount),
		.tiling = VK_IMAGE_TILING_OPTIMAL,

		.usage =
			(texture->depthFormat ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0) |
			(texture->textureFormatId && !isDeviceTexture ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0) |
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			(texture->resource.flags & EGraphicsResourceFlag_ShaderRead ? VK_IMAGE_USAGE_SAMPLED_BIT : 0) |
			(texture->resource.flags & EGraphicsResourceFlag_ShaderWrite ? VK_IMAGE_USAGE_STORAGE_BIT : 0),

		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	//Allocate memory

	if(texture->resource.type != EResourceType_Swapchain) {

		VkMemoryDedicatedRequirements dedicatedReq = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS
		};

		VkMemoryRequirements2 requirements = (VkMemoryRequirements2) {
			.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
			.pNext = &dedicatedReq
		};

		//TODO: versioned image

		VkUnifiedTexture *managedImageExt = TextureRef_getImgExtT(textureRef, Vk, 0, 0);
		gotoIfError(clean, checkVkError(deviceExt->createImage(deviceExt->device, &imageInfo, NULL, &managedImageExt->image)))

		VkImageMemoryRequirementsInfo2 imageReq = (VkImageMemoryRequirementsInfo2) {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
			.image = managedImageExt->image
		};

		deviceExt->getImageMemoryRequirements2(deviceExt->device, &imageReq, &requirements);

		DeviceMemoryBlock block;

		gotoIfError(clean, VK_WRAP_FUNC(DeviceMemoryAllocator_allocate)(
			&device->allocator,
			&requirements,
			texture->resource.flags & EGraphicsResourceFlag_CPUAllocatedBit,
			&texture->resource.blockId,
			&texture->resource.blockOffset,
			texture->resource.type,
			name,
			&block
		))

		texture->resource.allocated = true;

		gotoIfError(clean, checkVkError(deviceExt->bindImageMemory(
			deviceExt->device, managedImageExt->image, (VkDeviceMemory) block.ext, texture->resource.blockOffset
		)))
	}

	for(U8 i = 0; i < texture->images; ++i) {

		VkUnifiedTexture *managedImageExt = TextureRef_getImgExtT(textureRef, Vk, 0, i);

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && CharString_length(name) && instanceExt->debugSetName) {

			VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VK_OBJECT_TYPE_IMAGE,
				.pObjectName = name.ptr,
				.objectHandle =  (U64) managedImageExt->image
			};

			gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName)))
		}
	}

clean:
	CharString_freex(&temp);
	return err;
}

Error VkUnifiedTexture_transition(
	VkUnifiedTexture *imageExt,
	VkPipelineStageFlags2 stage,
	VkAccessFlagBits2 access,
	VkImageLayout layout,
	U32 graphicsQueueId,
	const VkImageSubresourceRange *range,
	ListVkImageMemoryBarrier2 *imageBarriers,
	VkDependencyInfo *dependency
) {

	//Avoid duplicate barriers except in one case:
	//Barriers for write->write, which always need to be inserted in-between two calls.
	//Otherwise, it's not synchronized correctly.

	if(imageExt->lastStage == stage && imageExt->lastAccess == access && !(access & VkAccessFlagBits2_WRITE))
		return Error_none();

	//Handle image barrier

	const VkImageMemoryBarrier2 imageBarrier = (VkImageMemoryBarrier2) {

		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

		.srcStageMask = imageExt->lastStage,
		.srcAccessMask = imageExt->lastAccess,

		.dstStageMask = stage,
		.dstAccessMask = access,

		.oldLayout = imageExt->lastLayout,
		.newLayout = layout,

		.srcQueueFamilyIndex = graphicsQueueId,
		.dstQueueFamilyIndex = graphicsQueueId,

		.image = imageExt->image,
		.subresourceRange = *range
	};

	const Error err = ListVkImageMemoryBarrier2_pushBackx(imageBarriers, imageBarrier);

	if(err.genericError)
		return err;

	imageExt->lastLayout = imageBarrier.newLayout;
	imageExt->lastStage = imageBarrier.dstStageMask;
	imageExt->lastAccess = imageBarrier.dstAccessMask;

	dependency->pImageMemoryBarriers = imageBarriers->ptr;
	dependency->imageMemoryBarrierCount = (U32) imageBarriers->length;

	return Error_none();
}
