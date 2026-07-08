/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

//graphics/vulkan/generic/vk_swapchain.c

#include "types/container/list_impl.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_swapchain.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_device.h"
#include "platforms/window.h"
#include "platforms/monitor.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "types/container/ref_ptr.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/base/constants.h"

TList(VkSurfaceFormatKHR);
TList(VkPresentModeKHR);

TListImpl(VkSurfaceFormatKHR);
TListImpl(VkPresentModeKHR);

Bool VK_WRAP_FUNC(GraphicsDeviceRef_createSwapchain)(GraphicsDeviceRef *deviceRef, SwapchainRef *swapchainRef, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	Swapchain *swapchain = SwapchainRef_ptr(swapchainRef);
	SwapchainInfo *info = &swapchain->info;

	//Prepare temporary free-ables and extended data.

	CharString temp = CharString_createNull();
	ListVkSurfaceFormatKHR surfaceFormats = (ListVkSurfaceFormatKHR) { 0 };
	ListVkPresentModeKHR presentModes = (ListVkPresentModeKHR) { 0 };

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkSwapchain *swapchainExt = TextureRef_getImplExtT(VkSwapchain, swapchainRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	VkPhysicalDevice physicalDevice = (VkPhysicalDevice) device->info.ext;
	const Window *window = info->window ? (const Window*)(info->window + 1) : NULL;

	//Since this function is called for both resize and init, it's possible our surface already exists.

	if(!swapchainExt->surface)
		gotoIfError3(clean, VkSurface_create(device, window, &swapchainExt->surface, e_rr));

	VkBool32 support = false;

	gotoIfError3(clean, checkVkError(instanceExt->getPhysicalDeviceSurfaceSupport(
		(VkPhysicalDevice) device->info.ext,
		deviceExt->queues[EVkCommandQueue_Graphics].queueId, swapchainExt->surface, &support
	), e_rr));

	if(!support)
		retError(clean, Error_unsupportedOperation(0, "VkGraphicsDeviceRef_createSwapchain() has no queue support"));

	//It's possible that format has changed when calling Swapchain_resize.
	//So we can't skip this.

	U32 formatCount = 0;

	gotoIfError3(clean, checkVkError(instanceExt->getPhysicalDeviceSurfaceFormats(
		physicalDevice, swapchainExt->surface, &formatCount, NULL
	), e_rr));

	if(!formatCount)
		retError(clean, Error_invalidState(0, "VkGraphicsDeviceRef_createSwapchain() format isn't supported"));

	gotoIfError3(clean, ListVkSurfaceFormatKHR_resize(&surfaceFormats, formatCount, alloc, e_rr));

	gotoIfError3(clean, checkVkError(instanceExt->getPhysicalDeviceSurfaceFormats(
		physicalDevice, swapchainExt->surface, &formatCount, surfaceFormats.ptrNonConst
	), e_rr));

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
		retError(clean, Error_unsupportedOperation(0, "VkGraphicsDeviceRef_createSwapchain() invalid format"));

	VkSurfaceCapabilitiesKHR capabilities = (VkSurfaceCapabilitiesKHR) { 0 };

	gotoIfError3(clean, checkVkError(instanceExt->getPhysicalDeviceSurfaceCapabilities(
		physicalDevice, swapchainExt->surface, &capabilities
	), e_rr));

	I32x2 size = I32x2_create2(capabilities.currentExtent.width, capabilities.currentExtent.height);

	switch (capabilities.currentTransform) {

		case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:    case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:        //Avoid compositor
			size = I32x2_yx(size);
			break;

		default:
			break;
	}

	//Validate if it's compatible with the OxC3_platforms window
	//currentExtent can be -1 but only for Wayland, which means "do whatever you want" and in this case it won't match.

	if((I32x2_neq2(size, I32x2_create2(-1, -1)) && I32x2_neq2(window->size, size)) || !capabilities.maxImageArrayLayers)
		retError(clean, Error_invalidOperation(0, "VkGraphicsDeviceRef_createSwapchain() incompatible window size"));

	Bool isWritable = swapchain->base.resource.flags & EGraphicsResourceFlag_ShaderWrite;

	VkFlags requiredUsageFlags =
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT |
		(isWritable ? VK_IMAGE_USAGE_STORAGE_BIT : 0) |
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	if((capabilities.supportedUsageFlags & requiredUsageFlags) != requiredUsageFlags)
		retError(clean, Error_invalidOperation(2, "VkGraphicsDeviceRef_createSwapchain() doesn't have required flags"));

	if(!(capabilities.supportedCompositeAlpha & (VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR | VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)))
		retError(clean, Error_invalidOperation(
			3, "VkGraphicsDeviceRef_createSwapchain() doesn't have required composite alpha"));

	//Don't use the already requested images, since we might get a different image count
	U32 requestedImages = SWAPCHAIN_VERSIONING;

	if(capabilities.minImageCount > requestedImages)
		++requestedImages;

	if(capabilities.minImageCount > requestedImages || (capabilities.maxImageCount < 3 && capabilities.maxImageCount))
		retError(clean, Error_invalidOperation(
			4, "VkGraphicsDeviceRef_createSwapchain() requires support for 3 or 4 images"));

	VkFlags anyRotate =
		VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR |
		VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR |
		VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR;

	swapchain->requiresManualComposite = !!(capabilities.supportedTransforms & anyRotate);

	U16 expectOrientation = 0;

	if(swapchain->requiresManualComposite)
		switch (capabilities.currentTransform) {
			case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:    expectOrientation = 90;        break;
			case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:    expectOrientation = 180;    break;
			case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:    expectOrientation = 270;    break;
			default:                                        expectOrientation = 0;        break;
		}

	if(window->orientation != expectOrientation) {

		Log_debugLnx(
			"-- Mismatching orientation %"PRIu16" and %"PRIu16" %"PRIi32,
			window->orientation,
			expectOrientation,
			capabilities.currentTransform
		);

		retError(clean, Error_invalidState(
			0, "VkGraphicsDeviceRef_createSwapchain() expected orientation didn't match real orientation"));
	}

	//Get present mode

	U32 modes = 0;

	gotoIfError3(clean, checkVkError(instanceExt->getPhysicalDeviceSurfacePresentModes(
		physicalDevice, swapchainExt->surface, &modes, NULL
	), e_rr));

	gotoIfError3(clean, ListVkPresentModeKHR_resize(&presentModes, modes, alloc, e_rr));

	gotoIfError3(clean, checkVkError(instanceExt->getPhysicalDeviceSurfacePresentModes(
		physicalDevice, swapchainExt->surface, &modes, presentModes.ptrNonConst
	), e_rr));

	Bool supports[ESwapchainPresentMode_Count - 1] = { 0 };

	for (U32 i = 0; i < modes; ++i) {

		VkPresentModeKHR modei = presentModes.ptr[i];

		switch(modei) {

			default:                                                                                            break;
			case VK_PRESENT_MODE_IMMEDIATE_KHR:        supports[ESwapchainPresentMode_Immediate - 1] = true;        break;
			case VK_PRESENT_MODE_FIFO_KHR:            supports[ESwapchainPresentMode_Fifo - 1] = true;            break;
			case VK_PRESENT_MODE_FIFO_RELAXED_KHR:    supports[ESwapchainPresentMode_FifoRelaxed - 1] = true;        break;

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
				default:                                                                                        break;
				case ESwapchainPresentMode_Immediate:        presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;        break;
				case ESwapchainPresentMode_Fifo:            presentMode = VK_PRESENT_MODE_FIFO_KHR;                break;
				case ESwapchainPresentMode_FifoRelaxed:        presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;        break;
				case ESwapchainPresentMode_Mailbox:            presentMode = VK_PRESENT_MODE_MAILBOX_KHR;            break;
			}

			break;
		}
	}

	if(presentMode == (VkPresentModeKHR) -1)
		retError(clean, Error_invalidOperation(7, "VkGraphicsDeviceRef_createSwapchain() unsupported present mode"));

	//Turn it into a swapchain

	VkSwapchainKHR prevSwapchain = swapchainExt->swapchain;

	VkSwapchainCreateInfoKHR swapchainInfo = (VkSwapchainCreateInfoKHR) {

		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = swapchainExt->surface,
		.minImageCount = requestedImages,
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

	gotoIfError3(clean, checkVkError(
		deviceExt->createSwapchain(deviceExt->device, &swapchainInfo, NULL, &swapchainExt->swapchain),
		e_rr
	));

	if(prevSwapchain)
		deviceExt->destroySwapchain(deviceExt->device, prevSwapchain, NULL);

	//Acquire images

	U32 imageCount = 0;
	gotoIfError3(clean, checkVkError(
		deviceExt->getSwapchainImages(deviceExt->device, swapchainExt->swapchain, &imageCount, NULL),
		e_rr
	));

	if(device->flags & EGraphicsDeviceFlags_IsDebug)
		Log_debugLnx(
			"Creating swapchain: %"PRIi32"x%"PRIi32"x%"PRIu32" and orientation: %"PRIu16,
			I32x2_x(window->size),
			I32x2_y(window->size),
			imageCount,
			expectOrientation
		);

	if(imageCount < SWAPCHAIN_VERSIONING || imageCount > SWAPCHAIN_MAX_IMAGES) {

		if(device->flags & EGraphicsDeviceFlags_IsDebug)
			Log_debugLnx("Swapchain: Invalid image count: %"PRIu32, imageCount);

		retError(clean, Error_invalidState(
			1,
			"VkGraphicsDeviceRef_createSwapchain() imageCount returned exceeds max or subseeds min images permitted by OxC3"
		));
	}

	swapchain->base.images = (U8) imageCount;

	//Only recreate semaphores once.
	//These semaphores are linked to the FIF (frames in flight), not to the swapchain images

	if(swapchainExt->semaphores.length != device->framesInFlight) {

		ListVkSemaphore_free(&swapchainExt->semaphores, alloc);
		gotoIfError3(clean, ListVkSemaphore_resize(&swapchainExt->semaphores, device->framesInFlight, alloc, e_rr));

		for (U8 i = 0; i < imageCount; ++i) {

			VkSemaphoreCreateInfo semaphoreInfo = (VkSemaphoreCreateInfo) { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			VkSemaphore *semaphore = swapchainExt->semaphores.ptrNonConst + i;

			gotoIfError3(clean, checkVkError(
				deviceExt->createSemaphore(deviceExt->device, &semaphoreInfo, NULL, semaphore),
				e_rr
			));

			if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {

				CharString_free(&temp, alloc);
				gotoIfError3(clean, CharString_format(alloc, &temp, e_rr, "Swapchain semaphore %"PRIu64, (U64)i));

				const VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_SEMAPHORE,
					.objectHandle = (U64) *semaphore,
					.pObjectName = temp.ptr
				};

				gotoIfError3(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName), e_rr));
			}
		}
	}

	//Get images

	VkImage vkImages[SWAPCHAIN_MAX_IMAGES];        //Temp alloc, we only allow up to 5 images.

	gotoIfError3(clean, checkVkError(deviceExt->getSwapchainImages(
		deviceExt->device, swapchainExt->swapchain, &imageCount, vkImages
	), e_rr));

	//Destroy image views

	for(U8 i = 0; i < imageCount; ++i) {

		VkUnifiedTexture *managedImage = TextureRef_getImgExtT(swapchainRef, Vk, 0, i);
		managedImage->lastAccess = managedImage->lastLayout = 0;
		managedImage->lastStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		managedImage->image = vkImages[i];

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {

			CharString_free(&temp, alloc);

			gotoIfError3(clean, CharString_format(
				alloc,
				&temp,
				e_rr,
				"Swapchain image #%"PRIu32" (%.*s)",
				(U32) i,
				CharString_length(window->title),
				window->title.ptr
			));

			const VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VK_OBJECT_TYPE_IMAGE,
				.pObjectName = temp.ptr,
				.objectHandle =  (U64) managedImage->image
			};

			gotoIfError3(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName), e_rr));
		}
	}

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && instanceExt->debugSetName) {

		CharString_free(&temp, alloc);

		gotoIfError3(clean, CharString_format(
			alloc,
			&temp,
			e_rr,
			"Swapchain (%.*s)",
			CharString_length(window->title),
			window->title.ptr
		));

		const VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR,
			.pObjectName = temp.ptr,
			.objectHandle = (U64) swapchainExt->swapchain
		};

		gotoIfError3(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName), e_rr));
	}

	swapchain->orientation = expectOrientation;

clean:
	CharString_free(&temp, alloc);
	ListVkSurfaceFormatKHR_free(&surfaceFormats, alloc);
	ListVkPresentModeKHR_free(&presentModes, alloc);
	return s_uccess;
}

void VK_WRAP_FUNC(Swapchain_free)(Swapchain *swapchain, const Allocator *alloc) {

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

	ListVkSemaphore_free(&swapchainExt->semaphores, alloc);

	if(swapchainExt->swapchain)
		deviceExt->destroySwapchain(deviceExt->device, swapchainExt->swapchain, NULL);

	if(swapchainExt->surface)
		instanceExt->destroySurface(instanceExt->instance, swapchainExt->surface, NULL);
}
