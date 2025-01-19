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

#include "platforms/ext/listx_impl.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_swapchain.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_device.h"
#include "platforms/window.h"
#include "platforms/monitor.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "types/container/ref_ptr.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"

TList(VkSurfaceFormatKHR);
TList(VkPresentModeKHR);

TListImpl(VkSurfaceFormatKHR);
TListImpl(VkPresentModeKHR);

Error VK_WRAP_FUNC(GraphicsDeviceRef_createSwapchain)(GraphicsDeviceRef *deviceRef, SwapchainRef *swapchainRef) {

	Swapchain *swapchain = SwapchainRef_ptr(swapchainRef);
	SwapchainInfo *info = &swapchain->info;

	//Prepare temporary free-ables and extended data.

	Error err = Error_none();
	CharString temp = CharString_createNull();
	ListVkSurfaceFormatKHR surfaceFormats = (ListVkSurfaceFormatKHR) { 0 };
	ListVkPresentModeKHR presentModes = (ListVkPresentModeKHR) { 0 };

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkSwapchain *swapchainExt = TextureRef_getImplExtT(VkSwapchain, swapchainRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	VkPhysicalDevice physicalDevice = (VkPhysicalDevice) device->info.ext;
	const Window *window = info->window;

	//Since this function is called for both resize and init, it's possible our surface already exists.

	if(!swapchainExt->surface)
		gotoIfError(clean, VkSurface_create(device, window, &swapchainExt->surface))

	VkBool32 support = false;

	gotoIfError(clean, checkVkError(instanceExt->getPhysicalDeviceSurfaceSupport(
		(VkPhysicalDevice) device->info.ext,
		deviceExt->queues[EVkCommandQueue_Graphics].queueId, swapchainExt->surface, &support
	)))

	if(!support)
		gotoIfError(clean, Error_unsupportedOperation(0, "VkGraphicsDeviceRef_createSwapchain() has no queue support"))

	//It's possible that format has changed when calling Swapchain_resize.
	//So we can't skip this.

	U32 formatCount = 0;

	gotoIfError(clean, checkVkError(instanceExt->getPhysicalDeviceSurfaceFormats(
		physicalDevice, swapchainExt->surface, &formatCount, NULL
	)))

	if(!formatCount)
		gotoIfError(clean, Error_invalidState(0, "VkGraphicsDeviceRef_createSwapchain() format isn't supported"))

	gotoIfError(clean, ListVkSurfaceFormatKHR_resizex(&surfaceFormats, formatCount))

	gotoIfError(clean, checkVkError(instanceExt->getPhysicalDeviceSurfaceFormats(
		physicalDevice, swapchainExt->surface, &formatCount, surfaceFormats.ptrNonConst
	)))

	VkSurfaceFormatKHR searchFormat = (VkSurfaceFormatKHR) { 0 };

	switch(swapchain->base.textureFormatId) {

		case ETextureFormatId_RGBA8:

			searchFormat = (VkSurfaceFormatKHR) {
				.format = VK_FORMAT_R8G8B8A8_UNORM,
				.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR
			};

			break;

		case ETextureFormatId_BGRA8:

			searchFormat = (VkSurfaceFormatKHR) {
				.format = VK_FORMAT_B8G8R8A8_UNORM,
				.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR
			};

			break;

		//TODO: HDR10_ST2084_EXT?

		case ETextureFormatId_BGR10A2:

			searchFormat = (VkSurfaceFormatKHR) {
				.format = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
				.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR
			};

			break;

		case ETextureFormatId_RGBA16f:

			searchFormat = (VkSurfaceFormatKHR) {
				.format = VK_FORMAT_R16G16B16A16_SFLOAT,
				.colorSpace = VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
			};

			break;

		case ETextureFormatId_RGBA32f:

			searchFormat = (VkSurfaceFormatKHR) {
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.colorSpace = VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
			};

			break;

		default:
			break;
	}

	for (U32 j = 0; j < formatCount; ++j) {

		VkSurfaceFormatKHR fj = surfaceFormats.ptr[j];

		if(fj.colorSpace != searchFormat.colorSpace || fj.format != searchFormat.format)
			continue;

		swapchainExt->format = fj;
		break;
	}

	if(swapchainExt->format.format == VK_FORMAT_UNDEFINED)
		gotoIfError(clean, Error_unsupportedOperation(0, "VkGraphicsDeviceRef_createSwapchain() invalid format"))

	VkSurfaceCapabilitiesKHR capabilities = (VkSurfaceCapabilitiesKHR) { 0 };

	gotoIfError(clean, checkVkError(instanceExt->getPhysicalDeviceSurfaceCapabilities(
		physicalDevice, swapchainExt->surface, &capabilities
	)))

	I32x2 size = I32x2_create2(capabilities.currentExtent.width, capabilities.currentExtent.height);

	//Validate if it's compatible with the OxC3_platforms window
	//currentExtent can be -1 but only for Wayland, which means "do whatever you want" and in this case it won't match.

	if((I32x2_neq2(size, I32x2_create2(-1, -1)) && I32x2_neq2(window->size, size)) || !capabilities.maxImageArrayLayers)
		gotoIfError(clean, Error_invalidOperation(0, "VkGraphicsDeviceRef_createSwapchain() incompatible window size"))

	Bool isWritable = swapchain->base.resource.flags & EGraphicsResourceFlag_ShaderWrite;

	VkFlags requiredUsageFlags =
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT |
		(isWritable ? VK_IMAGE_USAGE_STORAGE_BIT : 0) |
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	if((capabilities.supportedUsageFlags & requiredUsageFlags) != requiredUsageFlags)
		gotoIfError(clean, Error_invalidOperation(2, "VkGraphicsDeviceRef_createSwapchain() doesn't have required flags"))

	if(!(capabilities.supportedCompositeAlpha & (VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR | VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)))
		gotoIfError(clean, Error_invalidOperation(
			3, "VkGraphicsDeviceRef_createSwapchain() doesn't have required composite alpha"
		))
		
	if(capabilities.minImageCount > 3 || (capabilities.maxImageCount < 3 && capabilities.maxImageCount))
		gotoIfError(clean, Error_invalidOperation(
			4, "VkGraphicsDeviceRef_createSwapchain() requires support for 2 and 3 images"
		))

	VkFlags anyRotate =
		VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR |
		VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR |
		VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR;

	swapchain->requiresManualComposite = !!(capabilities.supportedTransforms & anyRotate);

	if(swapchain->requiresManualComposite)
		switch (capabilities.currentTransform) {
			case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:	swapchain->orientation = 90;	break;
			case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:	swapchain->orientation = 180;	break;
			case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:	swapchain->orientation = 270;	break;
			default:										swapchain->orientation = 0;		break;
		}

	//Get present mode

	U32 modes = 0;

	gotoIfError(clean, checkVkError(instanceExt->getPhysicalDeviceSurfacePresentModes(
		physicalDevice, swapchainExt->surface, &modes, NULL
	)))

	gotoIfError(clean, ListVkPresentModeKHR_resizex(&presentModes, modes))

	gotoIfError(clean, checkVkError(instanceExt->getPhysicalDeviceSurfacePresentModes(
		physicalDevice, swapchainExt->surface, &modes, presentModes.ptrNonConst
	)))

	Bool supports[ESwapchainPresentMode_Count - 1] = { 0 };

	//supportsMailbox ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR

	for (U32 i = 0; i < modes; ++i) {

		VkPresentModeKHR modei = presentModes.ptr[i];

		switch(modei) {

			default:																							break;
			case VK_PRESENT_MODE_IMMEDIATE_KHR:		supports[ESwapchainPresentMode_Immediate - 1] = true;		break;
			case VK_PRESENT_MODE_FIFO_KHR:			supports[ESwapchainPresentMode_Fifo - 1] = true;			break;
			case VK_PRESENT_MODE_FIFO_RELAXED_KHR:	supports[ESwapchainPresentMode_FifoRelaxed - 1] = true;		break;

			//Mailbox can allocate additional images on Android,
			//we don't want to deal with versioning 4x.

			case VK_PRESENT_MODE_MAILBOX_KHR:
				supports[ESwapchainPresentMode_Mailbox - 1] = _PLATFORM_TYPE != PLATFORM_ANDROID;
				break;
		}
	}

	VkPresentModeKHR presentMode = -1;

	for(U8 i = 0; i < ESwapchainPresentMode_Count - 1; ++i) {

		ESwapchainPresentMode mode = info->presentModePriorities[i];

		if(!mode)
			break;

		if(supports[mode - 1]) {

			swapchain->presentMode = mode;

			switch(mode) {
				default:																						break;
				case ESwapchainPresentMode_Immediate:		presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;		break;
				case ESwapchainPresentMode_Fifo:			presentMode = VK_PRESENT_MODE_FIFO_KHR;				break;
				case ESwapchainPresentMode_FifoRelaxed:		presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;		break;
				case ESwapchainPresentMode_Mailbox:			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;			break;
			}

			break;
		}
	}

	if(presentMode == (VkPresentModeKHR) -1)
		gotoIfError(clean, Error_invalidOperation(7, "VkGraphicsDeviceRef_createSwapchain() unsupported present mode"))

	Log_debugLnx("Requested present mode: %"PRIu32, presentMode);

	//Turn it into a swapchain

	VkSwapchainKHR prevSwapchain = swapchainExt->swapchain;

	VkSwapchainCreateInfoKHR swapchainInfo = (VkSwapchainCreateInfoKHR) {

		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = swapchainExt->surface,
		.minImageCount = swapchain->base.images,
		.imageFormat = swapchainExt->format.format,
		.imageColorSpace = swapchainExt->format.colorSpace,
		.imageExtent = (VkExtent2D) { .width = I32x2_x(window->size), .height = I32x2_y(window->size) },
		.imageArrayLayers = 1,
		.imageUsage = requiredUsageFlags,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,

		.preTransform = capabilities.currentTransform,

		.compositeAlpha =
			capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR ?
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,

		.presentMode = presentMode,
		.clipped = true,
		.oldSwapchain = prevSwapchain
	};
	
	gotoIfError(clean, checkVkError(deviceExt->createSwapchain(deviceExt->device, &swapchainInfo, NULL, &swapchainExt->swapchain)))

	if(prevSwapchain)
		deviceExt->destroySwapchain(deviceExt->device, prevSwapchain, NULL);

	//Acquire images
	
	Log_debugLnx("Get images");

	U32 imageCount = 0;
	gotoIfError(clean, checkVkError(deviceExt->getSwapchainImages(deviceExt->device, swapchainExt->swapchain, &imageCount, NULL)))
	
	Log_debugLnx("Get images: %"PRIu32, imageCount);

	if(imageCount != swapchain->base.images)
		gotoIfError(clean, Error_invalidState(1, "VkGraphicsDeviceRef_createSwapchain() imageCount doesn't match"))

	//Only recreate semaphores if needed.

	Bool createSemaphores = false;

	if(swapchainExt->semaphores.length != imageCount) {
		ListVkSemaphore_freex(&swapchainExt->semaphores);
		gotoIfError(clean, ListVkSemaphore_resizex(&swapchainExt->semaphores, imageCount))
		createSemaphores = true;
	}

	//Destroy image views

	//Get images

	VkImage vkImages[3];		//Temp alloc, we only allow up to 3 images.

	gotoIfError(clean, checkVkError(deviceExt->getSwapchainImages(
		deviceExt->device, swapchainExt->swapchain, &imageCount, vkImages
	)))

	for(U8 i = 0; i < swapchain->base.images; ++i) {

		VkUnifiedTexture *managedImage = TextureRef_getImgExtT(swapchainRef, Vk, 0, i);
		managedImage->lastAccess = managedImage->lastLayout = 0;
		managedImage->lastStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		if(managedImage->view)
			deviceExt->destroyImageView(deviceExt->device, managedImage->view, NULL);

		managedImage->image = vkImages[i];
	}

	//Grab semaphores

	Log_debugLnx("Get semas");

	for (U8 i = 0; i < swapchain->base.images; ++i) {

		if(createSemaphores) {

			VkSemaphoreCreateInfo semaphoreInfo = (VkSemaphoreCreateInfo) { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			VkSemaphore *semaphore = swapchainExt->semaphores.ptrNonConst + i;

			gotoIfError(clean, checkVkError(deviceExt->createSemaphore(deviceExt->device, &semaphoreInfo, NULL, semaphore)))

			if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {

				CharString_freex(&temp);
				gotoIfError(clean, CharString_formatx(&temp, "Swapchain semaphore %"PRIu64, (U64)i))

				const VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_SEMAPHORE,
					.objectHandle = (U64) *semaphore,
					.pObjectName = temp.ptr
				};

				gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName)))
			}
		}

		//Image views

		if(device->flags & EGraphicsDeviceFlags_IsDebug) {

			VkUnifiedTexture *managedImage = TextureRef_getImgExtT(swapchainRef, Vk, 0, i);

			CharString_freex(&temp);

			gotoIfError(clean, CharString_formatx(
				&temp, "Swapchain image #%"PRIu32" (%.*s)",
				(U32) i, CharString_length(window->title), window->title.ptr
			))

			const VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VK_OBJECT_TYPE_IMAGE,
				.pObjectName = temp.ptr,
				.objectHandle =  (U64) managedImage->image
			};

			gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName)))
		}
	}

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {

		CharString_freex(&temp);

		gotoIfError(clean, CharString_formatx(
			&temp, "Swapchain (%.*s)", CharString_length(window->title), window->title.ptr
		))

		const VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR,
			.pObjectName = temp.ptr,
			.objectHandle = (U64) swapchainExt->swapchain
		};

		gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName)))
	}

	Log_debugLnx("Done");

clean:
	Log_debugLnx("Fail?");
	CharString_freex(&temp);
	ListVkSurfaceFormatKHR_freex(&surfaceFormats);
	ListVkPresentModeKHR_freex(&presentModes);
	return err;
}

Bool VK_WRAP_FUNC(Swapchain_free)(Swapchain *swapchain, Allocator alloc) {

	(void)alloc;

	SwapchainRef *swapchainRef = (RefPtr*) swapchain - 1;
	GraphicsDevice *device = GraphicsDeviceRef_ptr(swapchain->base.resource.device);
	VkSwapchain *swapchainExt = TextureRef_getImplExtT(VkSwapchain, swapchainRef);

	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	const VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	for(U8 i = 0; i < swapchain->base.images; ++i) {

		const VkSemaphore semaphore = swapchainExt->semaphores.ptr[i];

		if(semaphore)
			deviceExt->destroySemaphore(deviceExt->device, semaphore, NULL);
	}

	ListVkSemaphore_freex(&swapchainExt->semaphores);

	if(swapchainExt->swapchain)
		deviceExt->destroySwapchain(deviceExt->device, swapchainExt->swapchain, NULL);

	if(swapchainExt->surface)
		instanceExt->destroySurface(instanceExt->instance, swapchainExt->surface, NULL);

	return true;
}
