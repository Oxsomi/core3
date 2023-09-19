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
#include "graphics/vulkan/vulkan.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
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

		case ECommandOp_clearColoru:
		case ECommandOp_clearColori:
		case ECommandOp_clearColorf: {

			VkClearColorValue color;
			Buffer_copy(Buffer_createRef(&color, sizeof(color)), Buffer_createConstRef(data, sizeof(color)));

			//TODO: Transition to dst optimal

			VkImageSubresourceRange range = (VkImageSubresourceRange) {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1
			};

			vkCmdClearColorImage(
				buffer, 
				temp->color, 
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				&color,
				1,
				&range
			);

			break;
		}

		case ECommandOp_clearDepthStencil: {

			VkClearDepthStencilValue clearValue = (VkClearDepthStencilValue) {
				.depth = *(const F32*) data,
				.stencil = *(const U8*) (data + sizeof(F32))
			};

			//TODO: Transition to dst optimal

			VkImageSubresourceRange range = (VkImageSubresourceRange) {
			
				.aspectMask = 
					(temp->flags & EVkCommandBufferFlags_hasDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
					(temp->flags & EVkCommandBufferFlags_hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0),

				.levelCount = 1,
				.layerCount = 1
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
		}

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
