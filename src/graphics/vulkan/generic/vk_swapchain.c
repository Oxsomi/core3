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

	//Prepare temporary free-ables and extended data.

	Error err = Error_none();
	CharString temp = CharString_createNull();
	List list = List_createEmpty(sizeof(VkSurfaceFormatKHR));

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkSwapchain *swapchainExt = Swapchain_ext(swapchain, Vk);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instance = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	VkPhysicalDevice physicalDevice = (VkPhysicalDevice) device->info.ext;

	const Window *window = info.window;

	//Since this function is called for both resize and init, it's possible our surface already exists.

	if(!swapchainExt->surface)
		_gotoIfError(clean, VkSurface_create(device, window, &swapchainExt->surface));

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

		case EWindowFormat_rgba8:
			
			searchFormat = (VkSurfaceFormatKHR) { 
				.format = VK_FORMAT_B8G8R8A8_UNORM, 
				.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR 
			};

			break;

		//TODO: HDR10_ST2084_EXT?

		case EWindowFormat_rgb10a2:
			
			searchFormat = (VkSurfaceFormatKHR) { 
				.format = VK_FORMAT_A2B10G10R10_UNORM_PACK32, 
				.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR
			};

			break;

		case EWindowFormat_rgba16f:

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
		VK_IMAGE_USAGE_STORAGE_BIT |
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

	Bool supportsMailbox = false, supportsFifo = false;

	for (U32 i = 0; i < modes; ++i) {

		VkPresentModeKHR mode = ((VkPresentModeKHR*)list.ptr)[i];

		if(mode == VK_PRESENT_MODE_FIFO_KHR)
			supportsFifo = true;

		//Mailbox can allocate additional images on Android, 
		//we don't want to deal with versioning 4x.

		else if(mode == VK_PRESENT_MODE_MAILBOX_KHR && _PLATFORM_TYPE != EPlatform_Android)
			supportsMailbox = true;
	}

	List_freex(&list);

	if(!supportsFifo && !supportsMailbox)
		_gotoIfError(clean, Error_invalidOperation(7));

	//Turn it into a swapchain
	
	U32 images = info.vsync ? 2 : 3;

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

		.presentMode = supportsMailbox ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR,
		.clipped = true,
		.oldSwapchain = prevSwapchain
	};

	_gotoIfError(clean, vkCheck(instance->createSwapchain(deviceExt->device, &swapchainInfo, NULL, &swapchainExt->swapchain)));

	if(prevSwapchain)
		instance->destroySwapchain(deviceExt->device, prevSwapchain, NULL);

	//Acquire images

	U32 imageCount = 0;

	_gotoIfError(clean, vkCheck(instance->getSwapchainImagesKHR(
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

	//Get images

	if(swapchainExt->images.length != imageCount) {
		List_freex(&swapchainExt->images);
		swapchainExt->images = List_createEmpty(sizeof(VkImage));
		_gotoIfError(clean, List_resizex(&swapchainExt->images, imageCount));
	}

	_gotoIfError(clean, vkCheck(instance->getSwapchainImagesKHR(
		deviceExt->device, swapchainExt->swapchain, &imageCount, (VkImage*) swapchainExt->images.ptr
	)));

	//Grab semaphores

	if(createSemaphores)
		for (U32 i = 0; i < images; ++i) {

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

	//Return our handle

	if(!prevSwapchain) {

		GraphicsDeviceRef_add(deviceRef);

		*swapchain = (Swapchain) {
			.info = info,
			.device = deviceRef
		};
	}

	swapchain->format = window->format;
	swapchain->size = size;
	++swapchain->versionId;

clean:
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

	List_freex(&swapchainExt->semaphores);
	List_freex(&swapchainExt->images);

	if(swapchainExt->swapchain)
		vkDestroySwapchainKHR(deviceExt->device, swapchainExt->swapchain, NULL);

	if(swapchainExt->surface)
		vkDestroySurfaceKHR(instance->instance, swapchainExt->surface, NULL);

	GraphicsDeviceRef_dec(&swapchain->device);

	return true;
}
