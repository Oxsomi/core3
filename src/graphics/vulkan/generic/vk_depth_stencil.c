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
#include "graphics/generic/depth_stencil.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_device.h"
#include "platforms/ref_ptr.h"
#include "platforms/ext/stringx.h"

const U64 DepthStencilExt_size = sizeof(VkManagedImage);

Error GraphicsDeviceRef_createDepthStencilExt(
	GraphicsDeviceRef *deviceRef, 
	I32x2 size,
	EDepthStencilFormat format, 
	Bool allowShaderRead,
	CharString name,
	DepthStencil *depthStencil
) {

	name;

	//Prepare temporary free-ables and extended data.

	Error err = Error_none();
	CharString temp = CharString_createNull();
	ELockAcquire acq = ELockAcquire_Invalid;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkManagedImage *depthStencilExt = (VkManagedImage*) DepthStencil_ext(depthStencil, );
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instance = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	depthStencilExt->readHandle = depthStencilExt->writeHandle = U32_MAX;

	VkFormat vkFormat = VK_FORMAT_S8_UINT;

	switch(format) {
		case EDepthStencilFormat_D16:		vkFormat = VK_FORMAT_D16_UNORM;					break;
		case EDepthStencilFormat_D32:		vkFormat = VK_FORMAT_D32_SFLOAT;				break;
		case EDepthStencilFormat_D24S8Ext:	vkFormat = VK_FORMAT_D24_UNORM_S8_UINT;			break;
		case EDepthStencilFormat_D32S8:		vkFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;		break;
	}

	VkImageCreateInfo imageInfo = (VkImageCreateInfo) {

		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = vkFormat,
		.extent = (VkExtent3D) { .width = I32x2_x(size), .height = I32x2_y(size), .depth = 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,

		.usage = 
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | 
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			(allowShaderRead ? VK_IMAGE_USAGE_SAMPLED_BIT : 0),

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
		&depthStencilExt->blockId, 
		&depthStencilExt->blockOffset,
		EResourceType_RenderTargetOrDepthStencil,
		name
	));

	DeviceMemoryBlock block = device->allocator.blocks.ptr[depthStencilExt->blockId];

	VkImage *image = &depthStencilExt->image;
	_gotoIfError(clean, vkCheck(vkCreateImage(deviceExt->device, &imageInfo, NULL, image)));

	_gotoIfError(clean, vkCheck(vkBindImageMemory(
		deviceExt->device, *image, (VkDeviceMemory) block.ext, depthStencilExt->blockOffset
	)));

	//Image views

	VkImageView *view = &depthStencilExt->view;

	VkImageViewCreateInfo viewCreate = (VkImageViewCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = depthStencilExt->image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = vkFormat,
		.subresourceRange = (VkImageSubresourceRange) {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.layerCount = 1,
			.levelCount = 1
		}
	};

	_gotoIfError(clean, vkCheck(vkCreateImageView(deviceExt->device, &viewCreate, NULL, view)));

	//Return our handle

	GraphicsDeviceRef_inc(deviceRef);

	*depthStencil = (DepthStencil) {
		.device = deviceRef,
		.size = size,
		.format = format,
		.allowShaderRead = allowShaderRead,
		.readLocation = U32_MAX
	};

	//Allocate in descriptors

	if(allowShaderRead) {

		acq = Lock_lock(&deviceExt->descriptorLock, U64_MAX);

		if(acq < ELockAcquire_Success)
			_gotoIfError(clean, Error_invalidState(
				0, "GraphicsDeviceRef_createDepthStencilExt() couldn't acquire descriptor lock"
			));

		//Create readonly image

		U32 locationRead = VkGraphicsDevice_allocateDescriptor(deviceExt, EDescriptorType_Texture2D);

		if(locationRead == U32_MAX)
			_gotoIfError(clean, Error_outOfMemory(
				0, "GraphicsDeviceRef_createDepthStencilExt() couldn't allocate image descriptor"
			));

		depthStencil->readLocation = depthStencilExt->readHandle = locationRead;

		VkDescriptorImageInfo descriptorImageInfo = (VkDescriptorImageInfo) {
			.imageView = *view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		VkWriteDescriptorSet writeDescriptorSet = (VkWriteDescriptorSet) {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = deviceExt->sets[EDescriptorSetType_Resources],
			.dstBinding = EDescriptorType_Texture2D - 1,
			.dstArrayElement = locationRead & ((1 << 20) - 1),
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.pImageInfo = &descriptorImageInfo
		};

		vkUpdateDescriptorSets(deviceExt->device, 1, &writeDescriptorSet, 0, NULL);

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(&deviceExt->descriptorLock);

		acq = ELockAcquire_Invalid;
	}
	
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

			debugName = (VkDebugUtilsObjectNameInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VK_OBJECT_TYPE_IMAGE,
				.pObjectName = name.ptr,
				.objectHandle =  (U64) *image
			};

			_gotoIfError(clean, vkCheck(instance->debugSetName(deviceExt->device, &debugName)));
		}

	#endif

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&deviceExt->descriptorLock);
	
	CharString_freex(&temp);
	return err;
}

Bool GraphicsDevice_freeDepthStencilExt(DepthStencil *depthStencil, Allocator alloc) {

	alloc;

	VkManagedImage *image = (VkManagedImage*) DepthStencil_ext(depthStencil, );
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(GraphicsDeviceRef_ptr(depthStencil->device), Vk);

	if(image->readHandle != U32_MAX) {

		ELockAcquire acq = Lock_lock(&deviceExt->descriptorLock, U64_MAX);

		if(acq >= ELockAcquire_Success) {
			ListU32 allocationList = (ListU32) { 0 };
			ListU32_createRefConst(&image->readHandle, 1, &allocationList);
			VkGraphicsDevice_freeAllocations(deviceExt, &allocationList);
		}

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(&deviceExt->descriptorLock);
	}

	if(image->view)
		vkDestroyImageView(deviceExt->device, image->view, NULL);

	if(image->image)
		vkDestroyImage(deviceExt->device, image->image, NULL);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(depthStencil->device);
	DeviceMemoryAllocator_freeAllocation(&device->allocator, image->blockId, image->blockOffset);

	return true;
}
