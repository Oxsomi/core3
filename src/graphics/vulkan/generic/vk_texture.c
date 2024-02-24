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

#include "graphics/vulkan/vulkan.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "platforms/ext/stringx.h"
#include "formats/texture.h"

const U32 UnifiedTextureImageExt_size = sizeof(VkUnifiedTexture);

Bool UnifiedTexture_freeExt(TextureRef *textureRef) {

	UnifiedTexture utex = TextureRef_getUnifiedTexture(textureRef, NULL);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(GraphicsDeviceRef_ptr(utex.resource.device), Vk);

	for(U8 i = 0; i < utex.images; ++i) {

		VkUnifiedTexture *image = TextureRef_getImgExtT(textureRef, Vk, 0, i);

		if(image->view)
			vkDestroyImageView(deviceExt->device, image->view, NULL);

		if(image->image && utex.resource.type != EResourceType_Swapchain)
			vkDestroyImage(deviceExt->device, image->image, NULL);
	}

	return true;
}

UnifiedTexture *TextureRef_getUnifiedTextureIntern(TextureRef *tex, DeviceResourceVersion *version);

Error UnifiedTexture_createExt(TextureRef *textureRef, CharString name) {

	UnifiedTexture *texture = TextureRef_getUnifiedTextureIntern(textureRef, NULL);

	//Prepare temporary free-ables and extended data.

	Error err = Error_none();
	CharString temp = CharString_createNull();

	GraphicsDevice *device = GraphicsDeviceRef_ptr(texture->resource.device);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instance = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	VkFormat vkFormat = VK_FORMAT_UNDEFINED;

	switch(texture->depthFormat) {
		case EDepthStencilFormat_D16:		vkFormat = VK_FORMAT_D16_UNORM;					break;
		case EDepthStencilFormat_D32:		vkFormat = VK_FORMAT_D32_SFLOAT;				break;
		case EDepthStencilFormat_D24S8Ext:	vkFormat = VK_FORMAT_D24_UNORM_S8_UINT;			break;
		case EDepthStencilFormat_D32S8:		vkFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;		break;
		case EDepthStencilFormat_S8Ext:		vkFormat = VK_FORMAT_S8_UINT;					break;
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

		VkDeviceImageMemoryRequirementsKHR imageReq = (VkDeviceImageMemoryRequirementsKHR) {
			.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS_KHR,
			.pCreateInfo = &imageInfo
		};

		VkMemoryDedicatedRequirements dedicatedReq = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS
		};

		VkMemoryRequirements2 requirements = (VkMemoryRequirements2) {
			.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
			.pNext = &dedicatedReq
		};

		instance->getDeviceImageMemoryRequirements(deviceExt->device, &imageReq, &requirements);

		_gotoIfError(clean, DeviceMemoryAllocator_allocate(
			&device->allocator,
			&requirements,
			texture->resource.flags & EGraphicsResourceFlag_CPUAllocatedBit,
			&texture->resource.blockId,
			&texture->resource.blockOffset,
			texture->resource.type,
			name
		));

		texture->resource.allocated = true;

		DeviceMemoryBlock block = device->allocator.blocks.ptr[texture->resource.blockId];

		//TODO: versioned image

		VkUnifiedTexture *managedImageExt = TextureRef_getImgExtT(textureRef, Vk, 0, 0);
		_gotoIfError(clean, vkCheck(vkCreateImage(deviceExt->device, &imageInfo, NULL, &managedImageExt->image)));

		_gotoIfError(clean, vkCheck(vkBindImageMemory(
			deviceExt->device, managedImageExt->image, (VkDeviceMemory) block.ext, texture->resource.blockOffset
		)));
	}

	//Image views

	for(U8 i = 0; i < texture->images; ++i) {

		VkUnifiedTexture *managedImageExt = TextureRef_getImgExtT(textureRef, Vk, 0, i);
		UnifiedTextureImage managedImage = TextureRef_getImage(textureRef, 0, i);

		VkImageViewCreateInfo viewCreate = (VkImageViewCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = managedImageExt->image,
			.viewType = texture->type == ETextureType_2D ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_3D,
			.format = vkFormat,
			.subresourceRange = (VkImageSubresourceRange) {
				.aspectMask = texture->depthFormat ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
				.layerCount = texture->length,
				.levelCount = texture->levels
		}
		};

		_gotoIfError(clean, vkCheck(vkCreateImageView(deviceExt->device, &viewCreate, NULL, &managedImageExt->view)));

		if(texture->resource.flags & EGraphicsResourceFlag_ShaderRW) {

			VkDescriptorImageInfo descriptorImageInfos[2] = {
				(VkDescriptorImageInfo) {
					.imageView = managedImageExt->view,
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				},
				(VkDescriptorImageInfo) {
					.imageView = managedImageExt->view,
					.imageLayout = VK_IMAGE_LAYOUT_GENERAL
				}
			};

			VkWriteDescriptorSet writeDescriptorSet[2] = {
				(VkWriteDescriptorSet) {
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = deviceExt->sets[EDescriptorSetType_Resources],
					.descriptorCount = 1
				}
			};

			U32 counter = 0;

			if (texture->resource.flags & EGraphicsResourceFlag_ShaderRead) {
				writeDescriptorSet[0].dstBinding = EDescriptorType_Texture2D - 1;
				writeDescriptorSet[0].dstArrayElement = managedImage.readHandle & ((1 << 20) - 1);
				writeDescriptorSet[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				writeDescriptorSet[0].pImageInfo = &descriptorImageInfos[0];
				++counter;
			}

			if (texture->resource.flags & EGraphicsResourceFlag_ShaderWrite) {

				if(counter)
					writeDescriptorSet[1] = writeDescriptorSet[0];

				writeDescriptorSet[counter].dstBinding = UnifiedTexture_getWriteDescriptorType(*texture) - 1;
				writeDescriptorSet[counter].dstArrayElement = managedImage.writeHandle & ((1 << 20) - 1);
				writeDescriptorSet[counter].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				writeDescriptorSet[counter].pImageInfo = &descriptorImageInfos[1];
				++counter;
			}

			vkUpdateDescriptorSets(deviceExt->device, counter, writeDescriptorSet, 0, NULL);
		}

		if (CharString_length(name)) {
			#ifndef NDEBUG
				if(instance->debugSetName && CharString_length(name)) {

					_gotoIfError(clean, CharString_formatx(
						&temp, "%.*s view (#%"PRIu32")", CharString_length(name), name.ptr, (U32)i
					));

					VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
						.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
						.objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
						.pObjectName = temp.ptr,
						.objectHandle =  (U64) managedImageExt->view
					};

					_gotoIfError(clean, vkCheck(instance->debugSetName(deviceExt->device, &debugName)));

					CharString_freex(&temp);

					debugName = (VkDebugUtilsObjectNameInfoEXT) {
						.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
						.objectType = VK_OBJECT_TYPE_IMAGE,
						.pObjectName = name.ptr,
						.objectHandle =  (U64) managedImageExt->image
					};

					_gotoIfError(clean, vkCheck(instance->debugSetName(deviceExt->device, &debugName)));
				}
			#endif
		}
	}

clean:
	CharString_freex(&temp);
	return err;
}
