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
#include "graphics/generic/pipeline.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/vulkan/vulkan.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_swapchain.h"
#include "graphics/vulkan/vk_buffer.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/listx.h"
#include "formats/texture.h"
#include "types/buffer.h"
#include "types/error.h"

Bool VkCommandBufferState_imageRangeConflicts(RefPtr *image1, ImageRange range1, RefPtr *image2, ImageRange range2) {

	range2; range1;		//TODO:

	return image1 == image2;
}

Bool VkCommandBufferState_bufferRangeConflicts(RefPtr *buffer1, BufferRange range1, RefPtr *buffer2, BufferRange range2) {

	range2; range1;		//TODO:

	return buffer1 == buffer2;
}

Bool VkCommandBufferState_isImageBound(VkCommandBufferState *state, RefPtr *image, ImageRange range) {

	if(!state)
		return false;

	for(U64 i = 0; i < state->boundImageCount; ++i)
		if(VkCommandBufferState_imageRangeConflicts(image, range, state->boundImages[i].ref, state->boundImages[i].range))
			return true;

	return false;
}

Bool VkCommandBufferState_isBufferBound(VkCommandBufferState *state, RefPtr *image, BufferRange range) {

	if(!state)
		return false;

	for(U64 i = 0; i < 17; ++i)
		if(VkCommandBufferState_bufferRangeConflicts(image, range, state->boundBuffers[i], (BufferRange){ 0 }))	//TODO: range
			return true;

	return false;
}

Error CommandList_validateGraphicsPipeline(
	Pipeline *pipeline, 
	VkImageRange images[8], 
	U8 imageCount, 
	EDepthStencilFormat depthFormat
) {

	PipelineGraphicsInfo *info = (PipelineGraphicsInfo*)pipeline->extraInfo;

	//Depth stencil state can be set to None to ignore writing to depth stencil

	if (info->depthFormatExt != EDepthStencilFormat_None && depthFormat != info->depthFormatExt)
		return Error_invalidState(1);

	//Validate attachments

	if (info->attachmentCountExt != imageCount)
		return Error_invalidState(0);

	for (U8 i = 0; i < imageCount && i < 8; ++i) {

		//Undefined is used to ignore the currently bound slot and to avoid writing to it

		if (!info->attachmentFormatsExt[i])
			continue;

		//Validate if formats are the same

		RefPtr *ref = images[i].ref;

		if (!ref)
			return Error_nullPointer(1);

		if (ref->typeId != EGraphicsTypeId_Swapchain)
			return Error_invalidParameter(1, i);

		Swapchain *swapchain = SwapchainRef_ptr(ref);

		if (swapchain->format != ETextureFormatId_unpack[info->attachmentFormatsExt[i]])
			return Error_invalidState(i + 2);
	}

	return Error_none();
}

Error CommandList_process(GraphicsDevice *device, ECommandOp op, const U8 *data, void *commandListExt) {

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	deviceExt;

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	VkCommandBufferState *temp = (VkCommandBufferState*) commandListExt;
	VkCommandBuffer buffer = temp->buffer;

	Error err = Error_none();

	switch (op) {
	
		case ECommandOp_SetViewport:
		case ECommandOp_SetScissor:
		case ECommandOp_SetViewportAndScissor: {

			//TODO: We probably wanna cache this if there's no currently bound target and apply it later
			//		We probably also wanna store what size the viewport is so when re-binding a target we can
			//		re-bind it without any problem.

			if(I32x2_eq2(temp->currentSize, I32x2_zero()))
				return Error_invalidOperation(0);

			I32x4 results;
			Buffer_copy(Buffer_createRef(&results, sizeof(results)), Buffer_createConstRef(data, sizeof(results)));

			I32x2 offset = I32x4_xy(results);
			I32x2 size = I32x4_zw(results);

			if(I32x2_any(I32x2_geq(offset, temp->currentSize)))
				return Error_invalidOperation(1);

			if(I32x2_eq2(size, I32x2_zero()))
				size = I32x2_sub(temp->currentSize, offset);

			Bool setViewport = op & 1;
			Bool setScissor = op & 2;

			if(setViewport) {

				VkViewport viewport = (VkViewport) {
					.x = (F32) I32x2_x(offset),
					.y = (F32) I32x2_y(offset),
					.width = (F32) I32x2_x(size),
					.height = (F32) I32x2_y(size),
					.minDepth = 1,
					.maxDepth = 0
				};

				vkCmdSetViewport(buffer, 0, 1, &viewport);

				temp->anyViewport = true;
			}

			if (setScissor) {

				VkRect2D scissor = (VkRect2D) {

					.offset = (VkOffset2D) {
						.x = I32x2_x(offset),
						.y = I32x2_y(offset),
					},

					.extent = (VkExtent2D) {
						.width = I32x2_x(size),
						.height = I32x2_y(size),
					}
				};

				vkCmdSetScissor(buffer, 0, 1, &scissor);

				temp->anyScissor = true;
			}

			break;
		}

		case ECommandOp_SetStencil:
			vkCmdSetStencilReference(buffer, VK_STENCIL_FACE_FRONT_AND_BACK, *(const U8*) data);
			break;

		case ECommandOp_SetBlendConstants:
			vkCmdSetBlendConstants(buffer, (const float*) data);
			break;

		//Clear commands

		case ECommandOp_ClearImages: {

			U64 imageClearCount = *(const U32*) data;

			List imageBarriers = List_createEmpty(sizeof(VkImageMemoryBarrier2));
			_gotoIfError(cleanClearImages, List_reservex(&imageBarriers, imageClearCount));

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

				if(VkCommandBufferState_isImageBound(temp, image.image, image.range))
					return Error_invalidOperation(0);

				VkManagedImage *imageExt = &((VkManagedImage*)swapchainExt->images.ptr)[swapchainExt->currentIndex];

				//Validate subresource range

				if(swapchain) {

					if (image.range.layerId != U32_MAX && image.range.layerId >= 1)
						_gotoIfError(cleanClearImages, Error_outOfBounds(1, image.range.layerId, 1));

					if (image.range.levelId != U32_MAX && image.range.levelId >= 1)
						_gotoIfError(cleanClearImages, Error_outOfBounds(2, image.range.levelId, 1));
				}

				//Handle transition

				VkImageSubresourceRange range = (VkImageSubresourceRange) {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = 1,
					.layerCount = 1
				};

				_gotoIfError(cleanClearImages, VkSwapchain_transition(
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

				ClearImage image = ((const ClearImage*) (data + sizeof(U32)))[i];

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
					(const VkClearColorValue*) &image.color,
					1,
					&range
				);
			}

		cleanClearImages:
			List_freex(&imageBarriers);
			return err;
		}

		/*
		case ECommandOp_clearDepth: {

			ClearDepthStencil *clear = (const ClearDepthStencil*) data;
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
					(temp->flags & EVkCommandBufferFlags_HasDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
					(temp->flags & EVkCommandBufferFlags_HasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0),

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

		//Dynamic rendering / direct rendering

		case ECommandOp_StartRenderingExt: {

			if(!I32x2_eq2(temp->currentSize, I32x2_zero()))
				return Error_invalidOperation(0);

			const StartRenderExt *startRender = (const StartRenderExt*) data;
			const AttachmentInfo *attachments = (const AttachmentInfo*) (startRender + 1);

			//Prepare transitions

			VkDependencyInfo dependency = (VkDependencyInfo) {
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.dependencyFlags = 0
			};

			U32 graphicsQueueId = deviceExt->queues[EVkCommandQueue_Graphics].queueId;

			VkRenderingAttachmentInfoKHR attachmentsExt[8] = { 0 };
			U8 j = U8_MAX;

			I32x2 lastSize = I32x2_zero();

			VkImageRange ranges[8] = { 0 };

			for (U8 i = 0; i < startRender->colorCount; ++i) {

				if(!((startRender->activeMask >> i) & 1)) {

					attachmentsExt[i] = (VkRenderingAttachmentInfoKHR) {
						.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
						.imageView = NULL,
						.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
						.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE
					};

					continue;
				}

				++j;

				//Grab attachment

				const AttachmentInfo *attachmentsj = &attachments[j];

				ranges->range = attachmentsj->range;
				ranges->ref = attachmentsj->image;

				Swapchain *swapchain = SwapchainRef_ptr(attachmentsj->image);
				VkSwapchain *swapchainExt = Swapchain_ext(swapchain, Vk);

				if(!i)
					lastSize = swapchain->size;

				else if(!I32x2_eq2(lastSize, swapchain->size))
					_gotoIfError(cleanStartRendering, Error_invalidOperation(0));

				VkManagedImage *imageExt = &((VkManagedImage*)swapchainExt->images.ptr)[swapchainExt->currentIndex];

				VkImageSubresourceRange range = (VkImageSubresourceRange) {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = 1,
					.layerCount = 1
				};

				//Transition resource

				_gotoIfError(cleanStartRendering, VkSwapchain_transition(

					imageExt, 

					attachmentsj->readOnly ? VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT : 
					VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,

					VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | 
					(attachmentsj->readOnly ? 0 : VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT),

					VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
					graphicsQueueId,
					&range,

					&deviceExt->imageTransitions,
					&dependency
				));

				VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

				switch(attachmentsj->load) {
					case ELoadAttachmentType_Clear:		loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;		break;
					case ELoadAttachmentType_Any:		loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	break;
				}

				attachmentsExt[i] = (VkRenderingAttachmentInfoKHR) {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = imageExt->view,
					.imageLayout = imageExt->lastLayout,
					.loadOp = loadOp,
					.storeOp = attachmentsj->readOnly ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = *(const VkClearValue*) &attachmentsj->color
				};
			}

			if(dependency.imageMemoryBarrierCount)
				instanceExt->cmdPipelineBarrier2(buffer, &dependency);

			//Send begin render command

			I32x2 offset = startRender->offset;

			if(I32x2_any(I32x2_geq(offset, lastSize)))
				_gotoIfError(cleanStartRendering, Error_invalidOperation(1));

			I32x2 size = startRender->size;

			if(I32x2_eq2(size, I32x2_zero()))
				size = I32x2_sub(lastSize, offset);

			temp->currentSize = size;

			VkRenderingInfoKHR renderInfo = (VkRenderingInfoKHR) {
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
				.renderArea = (VkRect2D) {
					.offset = (VkOffset2D) {
						.x = I32x2_x(offset),
						.y = I32x2_y(offset)
					},
					.extent = (VkExtent2D) {
						.width = I32x2_x(size),
						.height = I32x2_y(size)
					}
				},
				.layerCount = 1,
				.colorAttachmentCount = startRender->colorCount,
				.pColorAttachments = attachmentsExt
			};

			instanceExt->cmdBeginRendering(buffer, &renderInfo);

			temp->boundImageCount = startRender->colorCount;

			U64 siz = sizeof(VkImageRange) * startRender->colorCount;
			Buffer_copy(Buffer_createRef(temp->boundImages, siz), Buffer_createRef(ranges, siz));

		cleanStartRendering:
			List_clear(&deviceExt->imageTransitions);
			return err;
		}

		case ECommandOp_EndRenderingExt:

			if(I32x2_eq2(temp->currentSize, I32x2_zero()))
				return Error_invalidOperation(0);

			instanceExt->cmdEndRendering(buffer);
			temp->currentSize = I32x2_zero();
			temp->anyScissor = temp->anyViewport = false;

			for(U8 i = 0; i < 8; ++i)
				temp->boundImages[i].ref = NULL;

			for(U8 i = 0; i < 17; ++i)
				temp->boundBuffers[i] = NULL;

			break;

		//Draws

		case ECommandOp_SetPipeline: {

			PipelineRef *setPipeline = *(PipelineRef* const*) data;

			Pipeline *pipeline = PipelineRef_ptr(setPipeline);
			VkPipeline *pipelineExt = Pipeline_ext(pipeline, Vk);

			vkCmdBindPipeline(
				temp->buffer,
				pipeline->type == EPipelineType_Compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
				*pipelineExt
			);

			temp->boundPipelines[pipeline->type] = setPipeline;

			break;
		}

		case ECommandOp_SetPrimitiveBuffers: {

			if(I32x2_eq2(temp->currentSize, I32x2_zero()))
				return Error_invalidOperation(0);

			PrimitiveBuffers prim = *(const PrimitiveBuffers*) data;

			VkBuffer vertexBuffers[16] = { 0 };
			VkDeviceSize vertexBufferOffsets[16] = { 0 };

			U32 start = 16, end = 0;

			Bool transitionedIndex = false;
			U32 graphicsQueueId = deviceExt->queues[EVkCommandQueue_Graphics].queueId;

			VkDependencyInfo dependency = (VkDependencyInfo) {
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.dependencyFlags = 0
			};

			//Fill vertexBuffers and find start/end range.
			//And ensure bound buffers can't be accidentally transitioned while render hasn't ended yet.

			for(U32 i = 0; i < 16; ++i) {

				DeviceBufferRef *bufferRef = prim.vertexBuffers[i];

				if (bufferRef) {

					DeviceBuffer *buf = DeviceBufferRef_ptr(bufferRef);
					VkDeviceBuffer *bufExt = DeviceBuffer_ext(buf, Vk);

					if(start == 16)
						start = i;

					end = i + 1;

					VkPipelineStageFlagBits2 flags = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
					VkAccessFlagBits2 access = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;

					if (bufferRef == prim.indexBuffer) {
						flags |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
						access |= VK_ACCESS_2_INDEX_READ_BIT;
						transitionedIndex = true;
					}

					_gotoIfError(cleanSetPrimitiveBuffers, VkDeviceBuffer_transition(
						bufExt,
						flags,
						access,
						graphicsQueueId,
						0,
						buf->length,
						&deviceExt->bufferTransitions,
						&dependency
					));

					vertexBuffers[i] = bufExt->buffer;
				}

				temp->boundBuffers[i] = prim.vertexBuffers[i];
			}

			if(!transitionedIndex && prim.indexBuffer) {
			
				DeviceBuffer *buf = DeviceBufferRef_ptr(prim.indexBuffer);
				VkDeviceBuffer *bufExt = DeviceBuffer_ext(buf, Vk);

				_gotoIfError(cleanSetPrimitiveBuffers, VkDeviceBuffer_transition(
					bufExt,
					VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
					VK_ACCESS_2_INDEX_READ_BIT,
					graphicsQueueId,
					0,
					buf->length,
					&deviceExt->bufferTransitions,
					&dependency
				));
			}

			if(dependency.imageMemoryBarrierCount)
				instanceExt->cmdPipelineBarrier2(buffer, &dependency);

			//Bind vertex and index buffer

			if(end > start)
				vkCmdBindVertexBuffers(
					temp->buffer,
					start,
					end - start, 
					&(vertexBuffers)[start], 
					&(vertexBufferOffsets)[start]
				);

			if(prim.indexBuffer) {

				vkCmdBindIndexBuffer(
					temp->buffer,
					DeviceBuffer_ext(DeviceBufferRef_ptr(prim.indexBuffer), Vk)->buffer, 
					0,
					prim.isIndex32Bit ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16
				);

			}

			temp->boundBuffers[16] = prim.indexBuffer;
		
		cleanSetPrimitiveBuffers:
			List_clear(&deviceExt->bufferTransitions);
			return err;
		}

		case ECommandOp_DrawIndirect:
		case ECommandOp_DrawIndirectCount:
		case ECommandOp_Draw: {

			if(I32x2_eq2(temp->currentSize, I32x2_zero()))
				return Error_invalidOperation(0);

			if (!temp->boundPipelines[0])
				return Error_invalidOperation(1);

			if(!temp->anyScissor || !temp->anyViewport)
				return Error_invalidOperation(2);

			Pipeline *pipeline = PipelineRef_ptr(temp->boundPipelines[0]);

			err = CommandList_validateGraphicsPipeline(
				pipeline, temp->boundImages, temp->boundImageCount, (EDepthStencilFormat) temp->boundDepthFormat
			);

			if(err.genericError)
				return err;

			//Otherwise we have a valid draw

			if(op == ECommandOp_Draw) {

				Draw draw = *(const Draw*)data;

				if(draw.isIndexed)
					vkCmdDrawIndexed(
						buffer, 
						draw.count, draw.instanceCount, 
						draw.indexOffset, draw.vertexOffset, 
						draw.instanceOffset
					);

				else vkCmdDraw(
					buffer,
					draw.count, draw.instanceCount, 
					draw.indexOffset, draw.vertexOffset
				);
			}

			else {

				List bufferBarriers = List_createEmpty(sizeof(VkBufferMemoryBarrier2));
				_gotoIfError(cleanDrawIndirect, List_reservex(&bufferBarriers, 2));

				U32 graphicsQueueId = deviceExt->queues[EVkCommandQueue_Graphics].queueId;

				VkDependencyInfo dependency = (VkDependencyInfo) {
					.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
					.dependencyFlags = 0
				};

				DrawIndirect drawIndirect = *(const DrawIndirect*)data;
				DeviceBuffer *dispatchBuffer = DeviceBufferRef_ptr(drawIndirect.buffer);
				VkDeviceBuffer *bufferExt = DeviceBuffer_ext(dispatchBuffer, Vk);

				if(VkCommandBufferState_isBufferBound(temp, drawIndirect.buffer, (BufferRange) { 0 }))			//TODO: range
					_gotoIfError(cleanDrawIndirect, Error_invalidOperation(0));

				if(VkCommandBufferState_isBufferBound(temp, drawIndirect.countBuffer, (BufferRange) { 0 }))		//TODO: range
					_gotoIfError(cleanDrawIndirect, Error_invalidOperation(1));

				_gotoIfError(cleanDrawIndirect, VkDeviceBuffer_transition(
					bufferExt,
					VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
					VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
					graphicsQueueId,
					drawIndirect.bufferOffset,
					(U64)drawIndirect.bufferStride * drawIndirect.drawCalls,
					&bufferBarriers,
					&dependency
				));

				if (drawIndirect.countBuffer) {

					DeviceBuffer *counterBuffer = DeviceBufferRef_ptr(drawIndirect.countBuffer);
					VkDeviceBuffer *counterExt = DeviceBuffer_ext(counterBuffer, Vk);

					_gotoIfError(cleanDrawIndirect, VkDeviceBuffer_transition(
						counterExt,
						VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
						VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
						graphicsQueueId,
						drawIndirect.countOffset,
						sizeof(U32),
						&bufferBarriers,
						&dependency
					));

					if(dependency.bufferMemoryBarrierCount)
						instanceExt->cmdPipelineBarrier2(buffer, &dependency);

					if(drawIndirect.isIndexed)
						vkCmdDrawIndexedIndirectCount(
							buffer, 
							bufferExt->buffer, drawIndirect.bufferOffset, 
							counterExt->buffer, drawIndirect.countOffset, 
							drawIndirect.drawCalls, drawIndirect.bufferStride
						);

					else vkCmdDrawIndirectCount(
						buffer,
						bufferExt->buffer, drawIndirect.bufferOffset,
						counterExt->buffer, drawIndirect.countOffset, 
						drawIndirect.drawCalls, drawIndirect.bufferStride
					);
				}

				else {

					if(dependency.bufferMemoryBarrierCount)
						instanceExt->cmdPipelineBarrier2(buffer, &dependency);

					if(drawIndirect.isIndexed)
						vkCmdDrawIndexedIndirect(
							buffer,
							bufferExt->buffer, drawIndirect.bufferOffset,
							drawIndirect.drawCalls, drawIndirect.bufferStride
						);

					else vkCmdDrawIndirect(
						buffer, bufferExt->buffer, drawIndirect.bufferOffset,
						drawIndirect.drawCalls, drawIndirect.bufferStride
					);
				}

			cleanDrawIndirect:
				List_freex(&bufferBarriers);
				return err;
			}

			break;
		}

		case ECommandOp_DispatchIndirect:
		case ECommandOp_Dispatch: {

			if (!temp->boundPipelines[1])
				return Error_invalidOperation(0);

			if(op == ECommandOp_Dispatch) {
				Dispatch dispatch = *(const Dispatch*)data;
				vkCmdDispatch(buffer, dispatch.groups[0], dispatch.groups[1], dispatch.groups[2]);
			}

			else {

				DispatchIndirect dispatch = *(const DispatchIndirect*)data;

				DeviceBuffer *dispatchBuffer = DeviceBufferRef_ptr(dispatch.buffer);
				VkDeviceBuffer *bufferExt = DeviceBuffer_ext(dispatchBuffer, Vk);

				if(VkCommandBufferState_isBufferBound(temp, dispatch.buffer, (BufferRange) { 0 }))		//TODO: range
					_gotoIfError(cleanDrawIndirect, Error_invalidOperation(0));

				List bufferBarriers = List_createEmpty(sizeof(VkBufferMemoryBarrier2));
				_gotoIfError(cleanDispatchIndirect, List_reservex(&bufferBarriers, 2));

				U32 graphicsQueueId = deviceExt->queues[EVkCommandQueue_Graphics].queueId;

				VkDependencyInfo dependency = (VkDependencyInfo) {
					.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
					.dependencyFlags = 0
				};

				_gotoIfError(cleanDispatchIndirect, VkDeviceBuffer_transition(
					bufferExt,
					VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
					VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
					graphicsQueueId,
					dispatch.offset,
					sizeof(U32) * 3,
					&bufferBarriers,
					&dependency
				));

				if(dependency.bufferMemoryBarrierCount)
					instanceExt->cmdPipelineBarrier2(buffer, &dependency);

				vkCmdDispatchIndirect(buffer, bufferExt->buffer, dispatch.offset);

			cleanDispatchIndirect:
				List_freex(&bufferBarriers);
				return err;
			}

			break;
		}

		case ECommandOp_Transition: {

			U32 transitionCount = *(const U32*) data;
			const Transition *transitions = (const Transition*)(data + sizeof(U32));

			VkDependencyInfo dependency = (VkDependencyInfo) {
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.dependencyFlags = 0
			};

			U32 graphicsQueueId = deviceExt->queues[EVkCommandQueue_Graphics].queueId;

			for (U64 i = 0; i < transitionCount; ++i) {

				Transition transition = transitions[i];
				
				VkPipelineStageFlags2 pipelineStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

				switch (transition.stage) {

					case EPipelineStage_Compute:		break;
					case EPipelineStage_Vertex:			pipelineStage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;		break;
					case EPipelineStage_Pixel:			pipelineStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;	break;
					case EPipelineStage_GeometryExt:	pipelineStage = VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;	break;

					case EPipelineStage_HullExt:		
						pipelineStage = VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT;
						break;

					case EPipelineStage_DomainExt:
						pipelineStage = VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT;
						break;

					default:
						_gotoIfError(cleanTransition, Error_unsupportedOperation(0));
				}

				if(transition.resource->typeId == EGraphicsTypeId_Swapchain) {

					Swapchain *swapchain = SwapchainRef_ptr(transition.resource);
					VkSwapchain *swapchainExt = Swapchain_ext(swapchain, Vk);

					VkManagedImage *imageExt = &((VkManagedImage*)swapchainExt->images.ptr)[swapchainExt->currentIndex];

					if(VkCommandBufferState_isImageBound(temp, transition.resource, transition.range.image))
						return Error_invalidOperation(0);

					VkImageSubresourceRange range = (VkImageSubresourceRange) {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.levelCount = 1,
						.layerCount = 1
					};

					//Transition resource

					VkAccessFlags2 accessFlags =
						!transition.isWrite ? VK_ACCESS_2_SHADER_SAMPLED_READ_BIT : VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

					_gotoIfError(cleanTransition, VkSwapchain_transition(

						imageExt, 
						pipelineStage,
						accessFlags,
						!transition.isWrite ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL,
						graphicsQueueId,
						&range,

						&deviceExt->imageTransitions,
						&dependency
					));
				}

				else {

					DeviceBuffer *devBuffer = DeviceBufferRef_ptr(transition.resource);
					VkDeviceBuffer *bufferExt = DeviceBuffer_ext(devBuffer, Vk);

					if(VkCommandBufferState_isBufferBound(temp, transition.resource, transition.range.buffer))
						_gotoIfError(cleanTransition, Error_invalidOperation(0));

					//Transition resource

					VkAccessFlags2 accessFlags =
						!transition.isWrite ? VK_ACCESS_2_SHADER_STORAGE_READ_BIT : VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

					_gotoIfError(cleanTransition, VkDeviceBuffer_transition(
						bufferExt,
						pipelineStage,
						accessFlags,
						graphicsQueueId,
						0,						//TODO: range
						devBuffer->length,
						&deviceExt->bufferTransitions,
						&dependency
					));
				}
			}

			if(dependency.imageMemoryBarrierCount || dependency.bufferMemoryBarrierCount)
				instanceExt->cmdPipelineBarrier2(buffer, &dependency);

		cleanTransition:
			List_clear(&deviceExt->bufferTransitions);
			List_clear(&deviceExt->imageTransitions);
			return err;
		}

		//Debug markers

		case ECommandOp_EndRegionDebugExt:

			if(!temp->debugRegionStack)
				return Error_invalidOperation(0);

			instanceExt->cmdDebugMarkerEnd(buffer);
			--temp->debugRegionStack;
			break;

		case ECommandOp_AddMarkerDebugExt:
		case ECommandOp_StartRegionDebugExt: {

			VkDebugMarkerMarkerInfoEXT markerInfo = (VkDebugMarkerMarkerInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT,
				.pMarkerName = (const char*) data + sizeof(F32x4),
			};

			Buffer_copy(Buffer_createRef(&markerInfo.color, sizeof(F32x4)), Buffer_createConstRef(data, sizeof(F32x4)));
			
			if(op == ECommandOp_AddMarkerDebugExt)
				instanceExt->cmdDebugMarkerInsert(buffer, &markerInfo);
			
			else if(op == ECommandOp_StartRegionDebugExt) {

				if(temp->debugRegionStack == U32_MAX)
					return Error_invalidOperation(0);

				instanceExt->cmdDebugMarkerBegin(buffer, &markerInfo);
				++temp->debugRegionStack;
			}

			break;
		}

		//NOP or unsupported

		case ECommandOp_Nop:
			break;

		default:
			return Error_unsupportedOperation(0);
	}

	return Error_none();
}
