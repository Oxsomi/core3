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

#include "graphics/generic/command_list.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/swapchain.h"
#include "graphics/vulkan/vulkan.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_swapchain.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/listx.h"
#include "types/buffer.h"
#include "types/error.h"

Error CommandList_process(GraphicsDevice *device, ECommandOp op, const U8 *data, void *commandListExt) {

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	deviceExt;

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	VkCommandBufferState *temp = (VkCommandBufferState*) commandListExt;
	VkCommandBuffer buffer = temp->buffer;

	switch (op) {
	
		case ECommandOp_setViewport:
		case ECommandOp_setScissor:
		case ECommandOp_setViewportAndScissor: {

			I32x4 results;
			Buffer_copy(Buffer_createRef(&results, sizeof(results)), Buffer_createConstRef(data, sizeof(results)));

			I32x2 offset = I32x4_xy(results);
			I32x2 size = I32x4_zw(results);

			Bool setViewport = op & 1;
			Bool setScissor = op & 2;

			if(setViewport) {

				VkViewport viewport = (VkViewport) {
					.x = (F32) I32x2_x(offset),
					.y = (F32) I32x2_y(temp->currentSize) - I32x2_y(offset),
					.width = (F32) I32x2_x(size),
					.height = (F32) -I32x2_y(size),
					.minDepth = 1,
					.maxDepth = 0
				};

				vkCmdSetViewport(buffer, 0, 1, &viewport);
			}

			if (setScissor) {

				VkRect2D scissor = (VkRect2D) {

					.offset = (VkOffset2D) {
						.x = I32x2_x(offset),
						.y = I32x2_y(temp->currentSize) - I32x2_y(offset),
					},

					.extent = (VkExtent2D) {
						.width = I32x2_x(size),
						.height = -I32x2_y(size),
					}
				};

				vkCmdSetScissor(buffer, 0, 1, &scissor);
			}

			break;
		}

		case ECommandOp_setStencil:
			vkCmdSetStencilReference(buffer, VK_STENCIL_FACE_FRONT_AND_BACK, *(const U8*) data);
			break;

		//Clear commands

		case ECommandOp_clearImages: {

			U64 imageClearCount = *(const U32*) data;

			List imageBarriers = List_createEmpty(sizeof(VkImageMemoryBarrier2));
			Error err = Error_none();

			_gotoIfError(clean, List_reservex(&imageBarriers, imageClearCount));

			//Combine transitions into one call.

			VkDependencyInfo dependency = (VkDependencyInfo) {
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.dependencyFlags = 0
			};

			U32 graphicsQueueId = deviceExt->queues[EVkCommandQueue_Graphics].queueId;

			for(U64 i = 0; i < imageClearCount; ++i) {

				//Get image

				ClearImage image = ((const ClearImage*) (data + sizeof(U32)))[i];

				Swapchain *swapchain = SwapchainRef_ptr(image.image);
				VkSwapchain *swapchainExt = Swapchain_ext(swapchain, Vk);

				VkManagedImage *imageExt = &((VkManagedImage*)swapchainExt->images.ptr)[swapchainExt->currentIndex];

				//Validate subresource range

				if(swapchain) {

					if (image.range.layerId != U32_MAX && image.range.layerId >= 1)
						_gotoIfError(clean, Error_outOfBounds(1, image.range.layerId, 1));

					if (image.range.levelId != U32_MAX && image.range.levelId >= 1)
						_gotoIfError(clean, Error_outOfBounds(2, image.range.levelId, 1));
				}

				//Handle transition

				VkImageSubresourceRange range = (VkImageSubresourceRange) {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = 1,
					.layerCount = 1
				};

				_gotoIfError(clean, VkSwapchain_transition(
					imageExt,
					VK_PIPELINE_STAGE_2_CLEAR_BIT, 
					VK_ACCESS_2_TRANSFER_WRITE_BIT, 
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					graphicsQueueId,
					&range,
					&imageBarriers,
					&dependency
				));
			}

			if(dependency.imageMemoryBarrierCount)
				instanceExt->cmdPipelineBarrier2(buffer, &dependency);

			//Handle copies.
			//To do so, we will search for all images that are the same and have the same clear color.
			//If this is the case, we can combine a clear into 1 operation.
			//An example: clear layer 2 and layer 3 with 0.xxxx could be done in 1 op.

			for(U64 i = 0; i < imageClearCount; ++i) {

				//Get image

				ClearImage image = ((ClearImage*) (data + sizeof(U32)))[i];

				Swapchain *swapchain = SwapchainRef_ptr(image.image);
				VkSwapchain *swapchainExt = Swapchain_ext(swapchain, Vk);

				VkManagedImage *imageExt = &((VkManagedImage*)swapchainExt->images.ptr)[swapchainExt->currentIndex];

				//Clear

				VkImageSubresourceRange range = (VkImageSubresourceRange) {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = 1,
					.layerCount = 1
				};

				vkCmdClearColorImage(
					buffer, 
					imageExt->image, 
					imageExt->lastLayout,
					(const VkClearColorValue*) image.color,
					1,
					&range
				);
			}

		clean:
			List_freex(&imageBarriers);
			return err;
		}

		/*
		case ECommandOp_clearDepth: {

			ClearDepthStencil *clear = (ClearDepthStencil*) data;
			ImageRange image = clear->image;

			VkClearDepthStencilValue clearValue = (VkClearDepthStencilValue) {
				.depth = clear->depth,
				.stencil = clear->stencil
			};

			//Validate range

			Swapchain *swapchain = SwapchainRef_ptr(image.image);

			U32 layerCount = 1, levelCount = 1;

			//Validate subresource range

			if(swapchain) {

				if (image.layerId != U32_MAX && image.layerId >= 1)
					return Error_invalidParameter(4, 1);

				if (image.levelId != U32_MAX && image.levelId >= 1)
					return Error_invalidParameter(4, 2);
			}

			//TODO: Transition to dst optimal

			VkImageSubresourceRange range = (VkImageSubresourceRange) {
			
				.aspectMask = 
					(temp->flags & EVkCommandBufferFlags_hasDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
					(temp->flags & EVkCommandBufferFlags_hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0),

				.levelCount = image.levelId == U32_MAX ? levelCount : 1,
				.layerCount = image.layerId == U32_MAX ? layerCount : 1,
				.baseArrayLayer = image.layerId == U32_MAX ? image.layerId : 0,
				.baseMipLevel = image.levelId == U32_MAX ? image.levelId : 0
			};

			vkCmdClearDepthStencilImage(
				buffer, 
				temp->depthStencil, 
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				&clearValue,
				1,
				&range
			);

			break;
		}*/

		//Debug

		case ECommandOp_endRegionDebugExt:

			if(instanceExt->debugMarkerEnd)
				instanceExt->debugMarkerEnd(buffer);

			break;

		case ECommandOp_addMarkerDebugExt:
		case ECommandOp_startRegionDebugExt: {

			VkDebugMarkerMarkerInfoEXT markerInfo = (VkDebugMarkerMarkerInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT,
				.pMarkerName = (const char*) data + sizeof(F32x4),
			};

			Buffer_copy(Buffer_createRef(&markerInfo.color, sizeof(F32x4)), Buffer_createConstRef(data, sizeof(F32x4)));
			
			if(op == ECommandOp_addMarkerDebugExt && instanceExt->debugMarkerInsert)
				instanceExt->debugMarkerInsert(buffer, &markerInfo);
			
			if(op == ECommandOp_startRegionDebugExt && instanceExt->debugMarkerBegin)
				instanceExt->debugMarkerBegin(buffer, &markerInfo);

			break;
		}

		//NOP or unsupported

		case ECommandOp_nop:
			break;

		default:
			return Error_unsupportedOperation(0);
	}

	return Error_none();
}
