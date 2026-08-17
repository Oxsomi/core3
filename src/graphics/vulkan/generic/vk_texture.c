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

//graphics/vulkan/generic/vk_texture.c

#include "types/container/list.h"
#include "graphics/generic/interface.h"
#include "graphics/vulkan/vk_interface.h"
#include "graphics/vulkan/vulkan.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "types/container/string.h"
#include "types/container/texture_format.h"
#include "formats/oiSH/sh_registers.h"
#include "types/base/constants.h"

void VK_WRAP_FUNC(UnifiedTexture_free)(TextureRef *textureRef) {

	const UnifiedTexture utex = TextureRef_getUnifiedTexture(textureRef, NULL);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(utex.resource.device);
	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	const Allocator *alloc = GraphicsDevice_getAlloc(device);

	for(U8 i = 0; i < utex.images; ++i) {

		VkUnifiedTexture *image = TextureRef_getImgExtT(textureRef, Vk, 0, i);

		for (U64 j = 0; j < image->views.length; ++j)
			deviceExt->destroyImageView(deviceExt->device, image->views.ptr[j].view, NULL);

		ListVkImageViewMapping_free(&image->views, alloc);

		if(image->image && utex.resource.type != EResourceType_Swapchain)
			deviceExt->destroyImage(deviceExt->device, image->image, NULL);
	}
}

UnifiedTexture *TextureRef_getUnifiedTextureIntern(TextureRef *tex, DeviceResourceVersion *version);

Bool VK_WRAP_FUNC(UnifiedTexture_create)(TextureRef *textureRef, const CharString *name, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(TextureRef_getUnifiedTexture(textureRef, NULL).resource.device);

	UnifiedTexture *texture = TextureRef_getUnifiedTextureIntern(textureRef, NULL);

	//Prepare temporary free-ables and extended data.

	CharString temp = CharString_createNull();

	GraphicsDevice *device = GraphicsDeviceRef_ptr(texture->resource.device);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(device->instance), Vk);

	VkFormat vkFormat = VK_FORMAT_UNDEFINED;

	switch(texture->depthFormat) {
		case EDepthStencilFormat_D16:         vkFormat = VK_FORMAT_D16_UNORM;          break;
		case EDepthStencilFormat_D32:         vkFormat = VK_FORMAT_D32_SFLOAT;         break;
		case EDepthStencilFormat_D24S8Ext:    vkFormat = VK_FORMAT_D24_UNORM_S8_UINT;  break;
		case EDepthStencilFormat_D32S8X24Ext: vkFormat = VK_FORMAT_D32_SFLOAT_S8_UINT; break;
		case EDepthStencilFormat_S8X24Ext:    vkFormat = VK_FORMAT_S8_UINT;            break;
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
		gotoIfError3(clean, checkVkError(
			deviceExt->createImage(deviceExt->device, &imageInfo, NULL, &managedImageExt->image),
			e_rr
		));

		VkImageMemoryRequirementsInfo2 imageReq = (VkImageMemoryRequirementsInfo2) {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
			.image = managedImageExt->image
		};

		deviceExt->getImageMemoryRequirements2(deviceExt->device, &imageReq, &requirements);

		DeviceMemoryBlock block;

		gotoIfError3(clean, VK_WRAP_FUNC(DeviceMemoryAllocator_allocate)(
			&device->allocator,
			&requirements,
			texture->resource.flags & EGraphicsResourceFlag_CPUAllocatedBit,
			&texture->resource.blockId,
			&texture->resource.blockOffset,
			texture->resource.type,
			name,
			&block, e_rr
		));

		texture->resource.allocated = true;

		gotoIfError3(clean, checkVkError(deviceExt->bindImageMemory(
			deviceExt->device, managedImageExt->image, (VkDeviceMemory) block.ext, texture->resource.blockOffset
		), e_rr));
	}

	for(U8 i = 0; i < texture->images; ++i) {

		VkUnifiedTexture *managedImageExt = TextureRef_getImgExtT(textureRef, Vk, 0, i);

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && name && CharString_length(*name) && instanceExt->debugSetName) {

			VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = VK_OBJECT_TYPE_IMAGE,
				.pObjectName = name->ptr,
				.objectHandle =  (U64) managedImageExt->image
			};

			gotoIfError3(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName), e_rr));
		}
	}

clean:
	CharString_free(&temp, alloc);
	return s_uccess;
}

Bool VkUnifiedTexture_transition(
	VkUnifiedTexture *imageExt,
	VkPipelineStageFlags2 stage,
	VkAccessFlagBits2 access,
	VkImageLayout layout,
	U32 graphicsQueueId,
	const VkImageSubresourceRange *range,
	ListVkImageMemoryBarrier2 *imageBarriers,
	VkDependencyInfo *dependency,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	//Avoid duplicate barriers except in one case:
	//Barriers for write->write, which always need to be inserted in-between two calls.
	//Otherwise, it's not synchronized correctly.

	if(imageExt->lastStage == stage && imageExt->lastAccess == access && !(access & VkAccessFlagBits2_WRITE))
		return s_uccess;

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

	gotoIfError3(clean, ListVkImageMemoryBarrier2_pushBack(imageBarriers, imageBarrier, alloc, e_rr));

	imageExt->lastLayout = imageBarrier.newLayout;
	imageExt->lastStage = imageBarrier.dstStageMask;
	imageExt->lastAccess = imageBarrier.dstAccessMask;

	dependency->pImageMemoryBarriers = imageBarriers->ptr;
	dependency->imageMemoryBarrierCount = (U32) imageBarriers->length;

clean:
	return s_uccess;
}

Bool VkUnifiedTexture_getView(Descriptor d, ESHRegisterType type, VkImageView *view, U32 *viewIdOutput, Error *e_rr) {

	Bool s_uccess = true;

	if(!d.resource)
		return false;

	const Allocator *alloc = GraphicsDeviceRef_getAlloc(TextureRef_getUnifiedTexture(d.resource, NULL).resource.device);

	if(!d.texture.arrayCount)
		d.texture.arrayCount = 1;

	if(!d.texture.mipCount)
		d.texture.mipCount = 1;

	VkImageView tmp = NULL;

	UnifiedTexture tex = TextureRef_getUnifiedTexture(d.resource, NULL);
	VkUnifiedTexture *texExt = TextureRef_getImgExtT(d.resource, Vk, 0, d.texture.imageId);

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(GraphicsDeviceRef_ptr(tex.resource.device), Vk);

	//A view's format has to be the IMAGE's format.
	//Picking a plane of a depth stencil image is the aspect mask's job, never the format's.
	//The per plane formats this used to select (S8_UINT for plane 1 and so on) are only legal on images
	// created with MUTABLE_FORMAT, which these aren't,
	// so validation rejected every view of a combined depth stencil image outright.

	VkFormat vkFormat;

	if(tex.depthFormat)
		switch(tex.depthFormat) {

			default:
			case EDepthStencilFormat_D32:         vkFormat = VK_FORMAT_D32_SFLOAT;         break;
			case EDepthStencilFormat_D16:         vkFormat = VK_FORMAT_D16_UNORM;          break;
			case EDepthStencilFormat_D32S8X24Ext: vkFormat = VK_FORMAT_D32_SFLOAT_S8_UINT; break;
			case EDepthStencilFormat_D24S8Ext:    vkFormat = VK_FORMAT_D24_UNORM_S8_UINT;  break;
			case EDepthStencilFormat_S8X24Ext:    vkFormat = VK_FORMAT_S8_UINT;            break;
		}

	else vkFormat = mapVkFormat(ETextureFormatId_unpack[tex.textureFormatId]);

	SpinLock *lock = &texExt->lock;
	ELockAcquire acq = SpinLock_lock(lock, 1 * SECOND);

	if(acq < ELockAcquire_Success)
		goto clean;

	U32 viewId = 0;
	U32 firstEmptyViewId = U32_MAX;

	for (; viewId < texExt->views.length; ++viewId)

		if (texExt->views.ptr[viewId].textureDescU64 == d.data[0]) {

			*view = texExt->views.ptr[viewId].view;
			*viewIdOutput = viewId;

			++texExt->views.ptrNonConst[viewId].refCount;
			goto clean;
		}

		else if(!texExt->views.ptr[viewId].view && firstEmptyViewId == U32_MAX)
			firstEmptyViewId = viewId;

	//planeId picks the aspect: 0 = the format's first plane (depth, or stencil for a stencil only format),
	// 1 = the stencil plane of a combined format.
	//0xFF is the ATTACHMENT sentinel: dynamic rendering requires the depth and the stencil attachment to carry the SAME view,
	// so the render path asks for one view spanning every aspect the format has.
	//A sampled view can never use that, since sampling allows exactly one aspect,
	// which is also why the sentinel gets its own dedup slot rather than sharing plane 0's.

	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;

	if (tex.depthFormat) {

		const EDepthStencilFormat dsFormat = (EDepthStencilFormat) tex.depthFormat;

		if(d.texture.planeId == 0xFF)
			aspect =
				(EDepthStencilFormat_hasDepth(dsFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
				(EDepthStencilFormat_hasStencil(dsFormat) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

		else if(d.texture.planeId)
			aspect = VK_IMAGE_ASPECT_STENCIL_BIT;

		else aspect = EDepthStencilFormat_hasDepth(dsFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	VkImageViewCreateInfo viewCreate = (VkImageViewCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = texExt->image,
		.format = vkFormat,
		.subresourceRange = (VkImageSubresourceRange) {
			.aspectMask = aspect,
			.layerCount = d.texture.arrayCount,
			.levelCount = d.texture.mipCount,
			.baseMipLevel = d.texture.mipId,
			.baseArrayLayer = d.texture.arrayId
		}
	};

	Bool isArray = type & ESHRegisterType_IsArray;
	type = type & ESHRegisterType_TypeMask;

	switch (type) {

		default:
		case ESHRegisterType_Texture2D:
		case ESHRegisterType_Texture2DMS:
			viewCreate.viewType = isArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
			break;

		case ESHRegisterType_TextureCube:
			viewCreate.viewType = isArray ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
			break;

		case ESHRegisterType_Texture3D:
			viewCreate.viewType = VK_IMAGE_VIEW_TYPE_3D;
			break;
	}

	gotoIfError3(clean, checkVkError(deviceExt->createImageView(deviceExt->device, &viewCreate, NULL, &tmp), e_rr));

		//Deposit the tmp into our list of views and ensure we don't free it without a reason

	VkImageViewMapping mapping = (VkImageViewMapping) {
		.textureDesc = d.texture,
		.view = tmp,
		.refCount = 1
	};

	if(firstEmptyViewId == U32_MAX) {
		gotoIfError3(clean, ListVkImageViewMapping_pushBack(&texExt->views, mapping, alloc, e_rr));
	}

	else {
		texExt->views.ptrNonConst[firstEmptyViewId] = mapping;
		viewId = firstEmptyViewId;
	}

	*viewIdOutput = viewId;
	*view = tmp;
	tmp = NULL;

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(lock);

	lock = NULL;
	acq = ELockAcquire_Invalid;

	if(tmp)
		deviceExt->destroyImageView(deviceExt->device, tmp, NULL);

	return s_uccess;
}
