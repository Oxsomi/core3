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

#include "graphics/generic/swapchain.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_swapchain.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_device.h"
#include "platforms/window.h"
#include "platforms/monitor.h"
#include "platforms/platform.h"
#include "platforms/ref_ptr.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/stringx.h"
#include "types/buffer.h"
#include "types/error.h"

Bool GraphicsDevice_freeSwapchain(Swapchain *data, Allocator alloc);

Error GraphicsDeviceRef_createSwapchainInternal(GraphicsDeviceRef *deviceRef, SwapchainInfo info, Swapchain *swapchain) {

	if(!info.presentModePriorities[0]) {
		info.presentModePriorities[0] = ESwapchainPresentMode_Mailbox;
		info.presentModePriorities[1] = ESwapchainPresentMode_Fifo;
		info.presentModePriorities[2] = ESwapchainPresentMode_FifoRelaxed;
		info.presentModePriorities[3] = ESwapchainPresentMode_Immediate;
	}

	//Prepare temporary free-ables and extended data.

	Error err = Error_none();
	CharString temp = CharString_createNull();
	List list = List_createEmpty(sizeof(VkSurfaceFormatKHR));
	Bool lock = false;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkSwapchain *swapchainExt = Swapchain_ext(swapchain, Vk);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instance = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	VkPhysicalDevice physicalDevice = (VkPhysicalDevice) device->info.ext;

	const Window *window = info.window;

	//Since this function is called for both resize and init, it's possible our surface already exists.

	if(!swapchainExt->surface)
		_gotoIfError(clean, VkSurface_create(device, window, &swapchainExt->surface));

	VkBool32 support = false;

	_gotoIfError(clean, vkCheck(instance->getPhysicalDeviceSurfaceSupport(
		(VkPhysicalDevice) device->info.ext,
		deviceExt->queues[EVkCommandQueue_Graphics].queueId, swapchainExt->surface, &support
	)));

	if(!support)
		_gotoIfError(clean, Error_unsupportedOperation(0));

	//It's possible that format has changed when calling Swapchain_resize.
	//So we can't skip this.

	U32 formatCount = 0;

	_gotoIfError(clean, vkCheck(instance->getPhysicalDeviceSurfaceFormats(
		physicalDevice, swapchainExt->surface, &formatCount, NULL
	)));

	if(!formatCount)
		_gotoIfError(clean, Error_invalidState(0));

	_gotoIfError(clean, List_resizex(&list, formatCount));

	_gotoIfError(clean, vkCheck(instance->getPhysicalDeviceSurfaceFormats(
		physicalDevice, swapchainExt->surface, &formatCount, (VkSurfaceFormatKHR*) list.ptr
	)));

	VkSurfaceFormatKHR searchFormat = (VkSurfaceFormatKHR) { 0 };

	switch(window->format) {

		case EWindowFormat_BGRA8:
			
			searchFormat = (VkSurfaceFormatKHR) { 
				.format = VK_FORMAT_B8G8R8A8_UNORM, 
				.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR 
			};

			break;

		//TODO: HDR10_ST2084_EXT?

		case EWindowFormat_BGR10A2:
			
			searchFormat = (VkSurfaceFormatKHR) { 
				.format = VK_FORMAT_A2B10G10R10_UNORM_PACK32, 
				.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR
			};

			break;

		case EWindowFormat_RGBA16f:

			searchFormat = (VkSurfaceFormatKHR) { 
				.format = VK_FORMAT_R16G16B16A16_SFLOAT, 
				.colorSpace = VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
			};

			break;

		default:
			_gotoIfError(clean, Error_invalidParameter(2, 0));
	}

	for (U32 j = 0; j < formatCount; ++j) {

		VkSurfaceFormatKHR fj = ((VkSurfaceFormatKHR*)list.ptr)[j];

		if(fj.colorSpace != searchFormat.colorSpace || fj.format != searchFormat.format)
			continue;

		swapchainExt->format = fj;
		break;
	}

	if(swapchainExt->format.format == VK_FORMAT_UNDEFINED)
		_gotoIfError(clean, Error_unsupportedOperation(0));

	List_freex(&list);

	VkSurfaceCapabilitiesKHR capabilities = (VkSurfaceCapabilitiesKHR) { 0 };

	_gotoIfError(clean, vkCheck(instance->getPhysicalDeviceSurfaceCapabilities(
		physicalDevice, swapchainExt->surface, &capabilities
	)));

	I32x2 size = I32x2_create2(capabilities.currentExtent.width, capabilities.currentExtent.height);

	//Validate if it's compatible with the OxC3_platforms window

	if(I32x2_neq2(window->size, size) || !capabilities.maxImageArrayLayers)
		_gotoIfError(clean, Error_invalidOperation(0));

	if(!(capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR))
		_gotoIfError(clean, Error_invalidOperation(1));

	VkFlags requiredUsageFlags = 
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT |
		(info.usage & ESwapchainUsage_AllowCompute ? VK_IMAGE_USAGE_STORAGE_BIT : 0) |
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	if((capabilities.supportedUsageFlags & requiredUsageFlags) != requiredUsageFlags)
		_gotoIfError(clean, Error_invalidOperation(2));

	if(!(capabilities.supportedCompositeAlpha & (VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR | VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)))
		_gotoIfError(clean, Error_invalidOperation(3));

	if(capabilities.minImageCount > 2 || capabilities.maxImageCount < 3)
		_gotoIfError(clean, Error_invalidOperation(4));

	VkFlags anyRotate = 
		VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR |
		VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR |
		VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR;

	info.requiresManualComposite = capabilities.supportedTransforms & anyRotate;

	if(info.requiresManualComposite) {

		if(window->monitors.length > 1)
			_gotoIfError(clean, Error_invalidOperation(5));

		MonitorOrientation current = EMonitorOrientation_Landscape;

		switch (capabilities.currentTransform) {
			case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:	current = EMonitorOrientation_Portrait;				break;
			case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:	current = EMonitorOrientation_FlippedLandscape;		break;
			case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:	current = EMonitorOrientation_FlippedPortrait;		break;
		}

		MonitorOrientation target = ((Monitor*)window->monitors.ptr)->orientation;

		if(current != target)
			_gotoIfError(clean, Error_invalidOperation(6));
	}

	//Get present mode

	U32 modes = 0;

	_gotoIfError(clean, vkCheck(instance->getPhysicalDeviceSurfacePresentModes(
		physicalDevice, swapchainExt->surface, &modes, NULL
	)));

	list = List_createEmpty(sizeof(VkPresentModeKHR));
	_gotoIfError(clean, List_resizex(&list, modes));

	_gotoIfError(clean, vkCheck(instance->getPhysicalDeviceSurfacePresentModes(
		physicalDevice, swapchainExt->surface, &modes, (VkPresentModeKHR*) list.ptr
	)));

	Bool supports[ESwapchainPresentMode_Count - 1] = { 0 };

	//supportsMailbox ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR

	for (U32 i = 0; i < modes; ++i) {

		VkPresentModeKHR modei = ((VkPresentModeKHR*)list.ptr)[i];

		switch(modei) {

			case VK_PRESENT_MODE_IMMEDIATE_KHR:		supports[ESwapchainPresentMode_Immediate - 1] = true;		break;
			case VK_PRESENT_MODE_FIFO_KHR:			supports[ESwapchainPresentMode_Fifo - 1] = true;			break;
			case VK_PRESENT_MODE_FIFO_RELAXED_KHR:	supports[ESwapchainPresentMode_FifoRelaxed - 1] = true;		break;

			//Mailbox can allocate additional images on Android, 
			//we don't want to deal with versioning 4x.

			case VK_PRESENT_MODE_MAILBOX_KHR:
				supports[ESwapchainPresentMode_Mailbox - 1] = _PLATFORM_TYPE != EPlatform_Android;
				break;
		}
	}

	List_freex(&list);

	VkPresentModeKHR presentMode = -1;

	for(U8 i = 0; i < ESwapchainPresentMode_Count - 1; ++i) {

		ESwapchainPresentMode mode = info.presentModePriorities[i];

		if(!mode)
			break;

		if(supports[mode - 1]) {

			swapchain->presentMode = mode;

			switch(mode) {
				case ESwapchainPresentMode_Immediate:		presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;		break;
				case ESwapchainPresentMode_Fifo:			presentMode = VK_PRESENT_MODE_FIFO_KHR;				break;
				case ESwapchainPresentMode_FifoRelaxed:		presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;		break;
				case ESwapchainPresentMode_Mailbox:			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;			break;
				default:
					_gotoIfError(clean, Error_unsupportedOperation(0));
			}

			break;
		}
	}

	if(presentMode == -1)
		_gotoIfError(clean, Error_invalidOperation(7));

	//Turn it into a swapchain
	
	U32 images = 3;

	Bool hasMultiQueue = deviceExt->resolvedQueues > 1;

	VkSwapchainKHR prevSwapchain = swapchainExt->swapchain;

	VkSwapchainCreateInfoKHR swapchainInfo = (VkSwapchainCreateInfoKHR) {

		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = swapchainExt->surface,
		.minImageCount = images,
		.imageFormat = swapchainExt->format.format,
		.imageColorSpace = swapchainExt->format.colorSpace,
		.imageExtent = capabilities.currentExtent,
		.imageArrayLayers = 1,
		.imageUsage = requiredUsageFlags,
		.imageSharingMode = hasMultiQueue ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,

		.queueFamilyIndexCount = hasMultiQueue ? deviceExt->resolvedQueues : 0,
		.pQueueFamilyIndices = hasMultiQueue ? deviceExt->uniqueQueues : NULL,

		.preTransform = capabilities.currentTransform,

		.compositeAlpha = 
			capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR ? 
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,

		.presentMode = presentMode,
		.clipped = true,
		.oldSwapchain = prevSwapchain
	};

	_gotoIfError(clean, vkCheck(instance->createSwapchain(deviceExt->device, &swapchainInfo, NULL, &swapchainExt->swapchain)));

	if(prevSwapchain) {

		instance->destroySwapchain(deviceExt->device, prevSwapchain, NULL);

		if(!Lock_lock(&deviceExt->descriptorLock, U64_MAX))
			_gotoIfError(clean, Error_invalidOperation(0));

		lock = true;

		if(!VkGraphicsDevice_freeAllocations(deviceExt, &swapchainExt->descriptorAllocations))
			_gotoIfError(clean, Error_invalidOperation(1));
	}

	//Acquire images

	U32 imageCount = 0;

	_gotoIfError(clean, vkCheck(instance->getSwapchainImages(
		deviceExt->device, swapchainExt->swapchain, &imageCount, NULL
	)));

	if(imageCount != images)
		_gotoIfError(clean, Error_invalidState(1));

	//Only recreate semaphores if needed.

	Bool createSemaphores = false;

	if(swapchainExt->semaphores.length != imageCount) {
		List_freex(&swapchainExt->semaphores);
		swapchainExt->semaphores = List_createEmpty(sizeof(VkSemaphore));
		_gotoIfError(clean, List_resizex(&swapchainExt->semaphores, imageCount));
		createSemaphores = true;
	}

	//Destroy image views

	for(U64 i = 0; i < swapchainExt->images.length; ++i) {

		VkManagedImage *managedImage = &((VkManagedImage*) swapchainExt->images.ptr)[i];

		managedImage->lastAccess = 0;
		managedImage->lastStage = 0;
		managedImage->lastLayout = 0;

		vkDestroyImageView(deviceExt->device, managedImage->view, NULL);
	}

	//Get images

	if(swapchainExt->images.length != imageCount) {
		List_freex(&swapchainExt->images);
		swapchainExt->images = List_createEmpty(sizeof(VkManagedImage));
		_gotoIfError(clean, List_resizex(&swapchainExt->images, imageCount));
	}

	VkImage vkImages[3];		//Temp alloc, we only allow up to 3 images.

	_gotoIfError(clean, vkCheck(instance->getSwapchainImages(
		deviceExt->device, swapchainExt->swapchain, &imageCount, vkImages
	)));

	for(U64 i = 0; i < swapchainExt->images.length; ++i)
		((VkManagedImage*) swapchainExt->images.ptr)[i].image = vkImages[i];

	//Grab semaphores

	for (U32 i = 0; i < images; ++i) {

		if(createSemaphores) {

			VkSemaphoreCreateInfo semaphoreInfo = (VkSemaphoreCreateInfo) { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			VkSemaphore *semaphore = (VkSemaphore*)swapchainExt->semaphores.ptr + i;

			_gotoIfError(clean, vkCheck(vkCreateSemaphore(deviceExt->device, &semaphoreInfo, NULL, semaphore)));

			if(instance->debugSetName) {

				CharString_freex(&temp);
				_gotoIfError(clean, CharString_formatx(&temp, "Swapchain semaphore %i", i));

				VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_SEMAPHORE,
					.objectHandle = (U64) *semaphore,
					.pObjectName = temp.ptr
				};

				_gotoIfError(clean, vkCheck(instance->debugSetName(deviceExt->device, &debugName)));
			}
		}

		//Image views

		VkImageView *view = &((VkManagedImage*) swapchainExt->images.ptr + i)->view;

		VkImageViewCreateInfo viewCreate = (VkImageViewCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = ((VkManagedImage*) swapchainExt->images.ptr)[i].image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = swapchainExt->format.format,
			.subresourceRange = (VkImageSubresourceRange) {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.layerCount = 1,
				.levelCount = 1
			}
		};

		_gotoIfError(clean, vkCheck(vkCreateImageView(deviceExt->device, &viewCreate, NULL, view)));
	}

	//Return our handle

	if(!prevSwapchain) {

		GraphicsDeviceRef_inc(deviceRef);

		*swapchain = (Swapchain) {
			.info = info,
			.device = deviceRef
		};
	}

	swapchain->format = window->format;
	swapchain->size = size;
	++swapchain->versionId;

	//Allocate in descriptors

	U64 descriptors = info.usage & ESwapchainUsage_AllowCompute ? 2 : 1;

	if(!swapchainExt->descriptorAllocations.stride)
		swapchainExt->descriptorAllocations = List_createEmpty(sizeof(U32));

	_gotoIfError(clean, List_resizex(&swapchainExt->descriptorAllocations, descriptors * imageCount));

	U32 *allocPtr = (U32*) swapchainExt->descriptorAllocations.ptr;

	list = List_createEmpty(sizeof(VkWriteDescriptorSet));
	_gotoIfError(clean, List_resizex(&list, swapchainExt->descriptorAllocations.length));

	if(!lock && !Lock_lock(&deviceExt->descriptorLock, U64_MAX))
		_gotoIfError(clean, Error_invalidState(0));

	lock = true;

	VkDescriptorImageInfo imageInfo[6];

	for(U64 i = 0; i < imageCount; ++i) {

		//Create readonly image

		U32 locationRead = VkGraphicsDevice_allocateDescriptor(deviceExt, EDescriptorType_Texture2D);

		if(locationRead == U32_MAX)
			_gotoIfError(clean, Error_outOfMemory(0));

		allocPtr[i * descriptors] = locationRead;

		imageInfo[i * descriptors] = (VkDescriptorImageInfo) {
			.imageView = ((VkManagedImage*)swapchainExt->images.ptr)[i].view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		((VkWriteDescriptorSet*)list.ptr)[i * descriptors + 0] = (VkWriteDescriptorSet) {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = deviceExt->sets[EDescriptorType_Texture2D],
			.dstBinding = 0,
			.dstArrayElement = locationRead & ((1 << 20) - 1),
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.pImageInfo = &imageInfo[i * descriptors]
		};

		//Create read write image
		//Default is RWTexture2D (unorm) otherwise float.

		EDescriptorType textureWriteType = EDescriptorType_RWTexture2D;

		switch (swapchain->format) {

			case EWindowFormat_RGBA32f:
			case EWindowFormat_RGBA16f:
				textureWriteType = EDescriptorType_RWTexture2Df;
				break;
		}

		if(info.usage & ESwapchainUsage_AllowCompute) {

			U32 locationWrite = VkGraphicsDevice_allocateDescriptor(deviceExt, textureWriteType);

			if(locationWrite == U32_MAX)
				_gotoIfError(clean, Error_outOfMemory(1));

			allocPtr[i * descriptors + 1] = locationWrite;

			imageInfo[i * descriptors + 1] = imageInfo[i * descriptors];
			imageInfo[i * descriptors + 1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

			((VkWriteDescriptorSet*)list.ptr)[i * descriptors + 1] = (VkWriteDescriptorSet) {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = deviceExt->sets[textureWriteType],
				.dstBinding = 0,
				.dstArrayElement = locationWrite & ((1 << 20) - 1),
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.pImageInfo = &imageInfo[i * descriptors + 1]
			};
		}
	}

	vkUpdateDescriptorSets(deviceExt->device, (U32) list.length, (const VkWriteDescriptorSet*) list.ptr, 0, NULL);
	List_freex(&list);

	Lock_unlock(&deviceExt->descriptorLock);
	lock = false;
	
	#ifndef NDEBUG

		if(instance->debugSetName) {

			CharString_freex(&temp);

			_gotoIfError(clean, CharString_formatx(
				&temp, "Swapchain (%.*s)", CharString_length(info.window->title), info.window->title.ptr
			));

			VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR,
				.pObjectName = temp.ptr,
				.objectHandle = (U64) swapchainExt->swapchain
			};

			_gotoIfError(clean, vkCheck(instance->debugSetName(deviceExt->device, &debugName)));

			for (U64 i = 0; i < imageCount; ++i) {

				CharString_freex(&temp);

				_gotoIfError(clean, CharString_formatx(
					&temp, "Swapchain image view #%u (%.*s)", 
					(U32) i, CharString_length(info.window->title), info.window->title.ptr
				));

				debugName = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
					.pObjectName = temp.ptr,
					.objectHandle =  (U64) ((VkManagedImage*) swapchainExt->images.ptr)[i].view
				};

				_gotoIfError(clean, vkCheck(instance->debugSetName(deviceExt->device, &debugName)));

				CharString_freex(&temp);

				_gotoIfError(clean, CharString_formatx(
					&temp, "Swapchain image #%u (%.*s)", 
					(U32) i, CharString_length(info.window->title), info.window->title.ptr
				));

				debugName = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_IMAGE,
					.pObjectName = temp.ptr,
					.objectHandle =  (U64) ((VkManagedImage*) swapchainExt->images.ptr)[i].image
				};

				_gotoIfError(clean, vkCheck(instance->debugSetName(deviceExt->device, &debugName)));
			}
		}

	#endif

clean:

	if(lock)
		Lock_unlock(&deviceExt->descriptorLock);
	
	CharString_freex(&temp);
	List_freex(&list);
	return err;
}

Error Swapchain_resize(Swapchain *swapchain) {

	if(!swapchain)
		return Error_nullPointer(0);

	//Resize with same format and same size is a NOP

	if(I32x2_eq2(swapchain->info.window->size, swapchain->size) && swapchain->info.window->format == swapchain->format)
		return Error_none();

	//Otherwise, we properly resize

	return GraphicsDeviceRef_createSwapchainInternal(swapchain->device, swapchain->info, swapchain);
}

Error GraphicsDeviceRef_createSwapchain(GraphicsDeviceRef *deviceRef, SwapchainInfo info, SwapchainRef **swapchainRef) {

	if(!deviceRef || !info.window || !info.window->nativeHandle)
		return Error_nullPointer(!deviceRef ? 1 : 0);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(!(device->info.capabilities.features & EGraphicsFeatures_Swapchain))
		return Error_unsupportedOperation(0);

	Error err = RefPtr_createx(
		(U32)(sizeof(Swapchain) + sizeof(VkSwapchain)), 
		(ObjectFreeFunc) GraphicsDevice_freeSwapchain, 
		EGraphicsTypeId_Swapchain, 
		swapchainRef
	);

	if(err.genericError)
		return err;

	err = GraphicsDeviceRef_createSwapchainInternal(deviceRef, info, SwapchainRef_ptr(*swapchainRef));

	if(err.genericError) {
		RefPtr_dec(swapchainRef);
		return err;
	}

	return Error_none();
}

Bool GraphicsDevice_freeSwapchain(Swapchain *swapchain, Allocator alloc) {

	alloc;

	if(!swapchain || !swapchain->device)
		return false;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(swapchain->device);
	VkSwapchain *swapchainExt = Swapchain_ext(swapchain, Vk);

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instance = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	for(U64 i = 0; i < swapchainExt->semaphores.length; ++i) {
	
		VkSemaphore semaphore = ((VkSemaphore*)swapchainExt->semaphores.ptr)[i];

		if(semaphore)
			vkDestroySemaphore(deviceExt->device, semaphore, NULL);
	}

	for (U64 i = 0; i < swapchainExt->images.length; ++i) {

		VkImageView view = ((VkManagedImage*)swapchainExt->images.ptr)[i].view;

		if(view)
			vkDestroyImageView(deviceExt->device, view, NULL);
	}

	List_freex(&swapchainExt->semaphores);
	List_freex(&swapchainExt->images);

	List_freex(&swapchainExt->descriptorAllocations);

	if(swapchainExt->swapchain)
		vkDestroySwapchainKHR(deviceExt->device, swapchainExt->swapchain, NULL);

	if(swapchainExt->surface)
		vkDestroySurfaceKHR(instance->instance, swapchainExt->surface, NULL);

	GraphicsDeviceRef_dec(&swapchain->device);

	return true;
}

Error VkSwapchain_transition(
	VkManagedImage *imageExt, 
	VkPipelineStageFlags2 stage, 
	VkAccessFlagBits2 access,
	VkImageLayout layout,
	U32 graphicsQueueId,
	const VkImageSubresourceRange *range,
	List *imageBarriers,
	VkDependencyInfo *dependency
) {

	//No-op

	if(imageExt->lastStage == stage && imageExt->lastAccess == access && imageExt->lastLayout == layout)
		return Error_none();

	//Handle image barrier

	VkImageMemoryBarrier2 imageBarrier = (VkImageMemoryBarrier2) {

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

	Error err = List_pushBackx(imageBarriers, Buffer_createConstRef(&imageBarrier, sizeof(imageBarrier)));

	if(err.genericError)
		return err;

	imageExt->lastLayout = imageBarrier.newLayout;
	imageExt->lastStage = imageBarrier.dstStageMask;
	imageExt->lastAccess = imageBarrier.dstAccessMask;

	dependency->pImageMemoryBarriers = (VkImageMemoryBarrier2*) imageBarriers->ptr;
	dependency->imageMemoryBarrierCount = (U32) imageBarriers->length;

	return Error_none();
}
