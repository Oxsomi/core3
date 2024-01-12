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

#include "platforms/ext/listx_impl.h"
#include "graphics/generic/render_texture.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_device.h"
#include "platforms/ref_ptr.h"
#include "platforms/ext/stringx.h"

const U64 RenderTextureExt_size = sizeof(VkManagedImage);

Error GraphicsDeviceRef_createRenderTextureExt(
	GraphicsDeviceRef *deviceRef, 
	ERenderTextureType type,
	I32x4 size,
	ETextureFormat format, 
	ERenderTextureUsage usage,
	CharString name,
	RenderTexture *renderTexture
) {

	name;

	//Prepare temporary free-ables and extended data.

	Error err = Error_none();
	CharString temp = CharString_createNull();
	ELockAcquire acq = ELockAcquire_Invalid;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkManagedImage *renderTextureExt = (VkManagedImage*) RenderTexture_ext(renderTexture, );
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instance = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	renderTextureExt->readHandle = renderTextureExt->writeHandle = U32_MAX;

	VkFormat vkFormat = mapVkFormat(format);
	Bool is3D = type != ERenderTextureType_2D;
	U32 depth = type == ERenderTextureType_3D ? (U32)I32x4_z(size) : (type == ERenderTextureType_Cube ? 6 : 1);

	VkImageCreateInfo imageInfo = (VkImageCreateInfo) {

		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = is3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
		.format = vkFormat,
		.extent = (VkExtent3D) { .width = I32x4_x(size), .height = I32x4_y(size), .depth = depth },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,

		.usage = 
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | 
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			(usage & ERenderTextureUsage_ShaderRead ? VK_IMAGE_USAGE_SAMPLED_BIT : 0) |
			(usage & ERenderTextureUsage_ShaderWrite ? VK_IMAGE_USAGE_STORAGE_BIT : 0),

		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	//Allocate memory

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
		false, 
		&renderTextureExt->blockId, 
		&renderTextureExt->blockOffset,
		EResourceType_RenderTargetOrDepthStencil,
		name
	));

	DeviceMemoryBlock block = device->allocator.blocks.ptr[renderTextureExt->blockId];

	VkImage *image = &renderTextureExt->image;
	_gotoIfError(clean, vkCheck(vkCreateImage(deviceExt->device, &imageInfo, NULL, image)));

	_gotoIfError(clean, vkCheck(vkBindImageMemory(
		deviceExt->device, *image, (VkDeviceMemory) block.ext, renderTextureExt->blockOffset
	)));
	
	#ifndef NDEBUG

		if(instance->debugSetName) {

			VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VK_OBJECT_TYPE_IMAGE,
				.pObjectName = name.ptr,
				.objectHandle =  (U64) *image
			};

			_gotoIfError(clean, vkCheck(instance->debugSetName(deviceExt->device, &debugName)));
		}

	#endif

	//Image views

	//TODO: 1 UAV and SRV per face for cubemap (2D), 3D won't to avoid allocating too many (and doesn't need it).

	VkImageView *view = &renderTextureExt->view;

	VkImageViewCreateInfo viewCreate = (VkImageViewCreateInfo) {

		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = renderTextureExt->image,

		.viewType = 
			type == ERenderTextureType_Cube ? VK_IMAGE_VIEW_TYPE_CUBE : 
			(is3D ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D),

		.format = vkFormat,

		.subresourceRange = (VkImageSubresourceRange) {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1,
			.levelCount = 1
		}
	};

	_gotoIfError(clean, vkCheck(vkCreateImageView(deviceExt->device, &viewCreate, NULL, view)));
	
	#ifndef NDEBUG

		if(instance->debugSetName) {

			_gotoIfError(clean, CharString_formatx(&temp, "%.*s view", CharString_length(temp), temp.ptr));

			VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
				.pObjectName = temp.ptr,
				.objectHandle =  (U64) *view
			};

			_gotoIfError(clean, vkCheck(instance->debugSetName(deviceExt->device, &debugName)));

			CharString_freex(&temp);
		}

	#endif

	//Return our handle

	GraphicsDeviceRef_inc(deviceRef);

	*renderTexture = (RenderTexture) {
		.device = deviceRef,
		.size = size,
		.format = format,
		.usage = usage,
		.type = type
	};

	//Allocate in descriptors

	//TODO: Allocate descriptors (read & write) per face if cubemap

	if(usage & ERenderTextureUsage_ShaderRW) {

		acq = Lock_lock(&deviceExt->descriptorLock, U64_MAX);

		if(acq < ELockAcquire_Success)
			_gotoIfError(clean, Error_invalidState(
				0, "GraphicsDeviceRef_createRenderTextureExt() couldn't acquire descriptor lock"
			));

		//Create readonly image

		VkDescriptorImageInfo descriptorImageInfo[2] = {
			(VkDescriptorImageInfo) {
				.imageView = *view,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			},
			(VkDescriptorImageInfo) {
				.imageView = *view,
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL
			}
		};

		VkWriteDescriptorSet writeDescriptorSet[2] = {
			(VkWriteDescriptorSet) {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = deviceExt->sets[EDescriptorSetType_Resources],
				.descriptorCount = 1,
				.pImageInfo = &descriptorImageInfo[0]
			}
		};

		U32 counter = 0;

		if(usage & ERenderTextureUsage_ShaderRead) {

			EDescriptorType descType = is3D ? EDescriptorType_Texture3D : EDescriptorType_Texture2D;
			U32 locationRead = VkGraphicsDevice_allocateDescriptor(deviceExt, descType);

			if(locationRead == U32_MAX)
				_gotoIfError(clean, Error_outOfMemory(
					0, "GraphicsDeviceRef_createRenderTextureExt() couldn't allocate image descriptor"
				));

			renderTextureExt->readHandle = locationRead;

			writeDescriptorSet[0].dstBinding = descType - 1;
			writeDescriptorSet[0].dstArrayElement = locationRead & ((1 << 20) - 1);
			writeDescriptorSet[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			++counter;
		}

		if(usage & ERenderTextureUsage_ShaderWrite) {

			EDescriptorType descType = 0;
			ETexturePrimitive prim = ETextureFormat_getPrimitive(format);

			if (is3D)
				switch (prim) {
					default:						descType = EDescriptorType_RWTexture3D;		break;
					case ETexturePrimitive_SNorm:	descType = EDescriptorType_RWTexture3Ds;	break;
					case ETexturePrimitive_UInt:	descType = EDescriptorType_RWTexture3Du;	break;
					case ETexturePrimitive_SInt:	descType = EDescriptorType_RWTexture3Di;	break;
					case ETexturePrimitive_Float:	descType = EDescriptorType_RWTexture3Df;	break;
				}

			else switch (prim) {
				default:							descType = EDescriptorType_RWTexture2D;		break;
				case ETexturePrimitive_SNorm:		descType = EDescriptorType_RWTexture2Ds;	break;
				case ETexturePrimitive_UInt:		descType = EDescriptorType_RWTexture2Du;	break;
				case ETexturePrimitive_SInt:		descType = EDescriptorType_RWTexture2Di;	break;
				case ETexturePrimitive_Float:		descType = EDescriptorType_RWTexture2Df;	break;
			}

			U32 locationWrite = VkGraphicsDevice_allocateDescriptor(deviceExt, descType);

			if(locationWrite == U32_MAX)
				_gotoIfError(clean, Error_outOfMemory(
					0, "GraphicsDeviceRef_createRenderTextureExt() couldn't allocate image descriptor"
				));

			renderTextureExt->writeHandle = locationWrite;

			writeDescriptorSet[counter] = writeDescriptorSet[0];
			writeDescriptorSet[counter].dstBinding = descType - 1;
			writeDescriptorSet[counter].dstArrayElement = locationWrite & ((1 << 20) - 1);
			writeDescriptorSet[counter].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			writeDescriptorSet[counter].pImageInfo = &descriptorImageInfo[1];
			++counter;
		}

		vkUpdateDescriptorSets(deviceExt->device, counter, writeDescriptorSet, 0, NULL);

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(&deviceExt->descriptorLock);

		acq = ELockAcquire_Invalid;
	}

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&deviceExt->descriptorLock);
	
	CharString_freex(&temp);
	return err;
}

Bool GraphicsDevice_freeRenderTextureExt(RenderTexture *renderTexture, Allocator alloc) {

	alloc;

	VkManagedImage *image = (VkManagedImage*) RenderTexture_ext(renderTexture, );
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(GraphicsDeviceRef_ptr(renderTexture->device), Vk);

	if(image->readHandle != U32_MAX || image->writeHandle != U32_MAX) {

		ELockAcquire acq = Lock_lock(&deviceExt->descriptorLock, U64_MAX);

		if(acq >= ELockAcquire_Success) {
			U32 handles[2] = { image->readHandle, image->writeHandle };
			ListU32 allocationList = (ListU32) { 0 };
			ListU32_createRefConst(handles, 2, &allocationList);
			VkGraphicsDevice_freeAllocations(deviceExt, &allocationList);
		}

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(&deviceExt->descriptorLock);
	}

	if(image->view)
		vkDestroyImageView(deviceExt->device, image->view, NULL);

	if(image->image)
		vkDestroyImage(deviceExt->device, image->image, NULL);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(renderTexture->device);
	DeviceMemoryAllocator_freeAllocation(&device->allocator, image->blockId, image->blockOffset);

	return true;
}
