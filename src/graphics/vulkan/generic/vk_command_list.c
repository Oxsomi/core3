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
#include "graphics/generic/command_list.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/depth_stencil.h"
#include "graphics/generic/render_texture.h"
#include "graphics/vulkan/vulkan.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_swapchain.h"
#include "graphics/vulkan/vk_buffer.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/errorx.h"
#include "platforms/log.h"
#include "formats/texture.h"
#include "types/buffer.h"
#include "types/error.h"

void addResolveImage(AttachmentInfoInternal attachment, VkRenderingAttachmentInfoKHR *result) {

	VkManagedImage *imageExt = NULL;

	if(attachment.resolveImage->typeId == EGraphicsTypeId_Swapchain) {
		VkSwapchain *swapchain = Swapchain_ext(SwapchainRef_ptr(attachment.resolveImage), Vk);
		imageExt = &swapchain->images.ptrNonConst[swapchain->currentIndex];
	}

	else if(attachment.resolveImage->typeId == EGraphicsTypeId_DepthStencil)
		imageExt = (VkManagedImage*) DepthStencil_ext(DepthStencilRef_ptr(attachment.resolveImage), );

	else imageExt = (VkManagedImage*) RenderTexture_ext(RenderTextureRef_ptr(attachment.resolveImage), );

	switch (attachment.resolveMode) {
		case EMSAAResolveMode_Average:	result->resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;	break;
		case EMSAAResolveMode_Min:		result->resolveMode = VK_RESOLVE_MODE_MIN_BIT;		break;
		case EMSAAResolveMode_Max:		result->resolveMode = VK_RESOLVE_MODE_MAX_BIT;		break;
	}

	result->resolveImageView = imageExt->view;
	result->resolveImageLayout = imageExt->lastLayout;
}

void CommandList_process(
	CommandList *commandList, 
	GraphicsDevice *device, 
	ECommandOp op, 
	const U8 *data, 
	void *commandListExt
) {

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	VkCommandBufferState *temp = (VkCommandBufferState*) commandListExt;
	VkCommandBuffer buffer = temp->buffer;

	switch (op) {
	
		case ECommandOp_SetViewport:
		case ECommandOp_SetScissor:
		case ECommandOp_SetViewportAndScissor: {

			I32x2 offset = ((const I32x2*) data)[0];
			I32x2 size = ((const I32x2*) data)[1];

			if((op - ECommandOp_SetViewport + 1) & 1)
				temp->tempViewport = (VkViewport) {
					.x = (F32) I32x2_x(offset),
					.y = (F32) I32x2_y(offset),
					.width = (F32) I32x2_x(size),
					.height = (F32) I32x2_y(size),
					.minDepth = 1,
					.maxDepth = 0
				};

			if ((op - ECommandOp_SetViewport + 1) & 2)
				temp->tempScissor = (VkRect2D) {
					.offset = (VkOffset2D) {
						.x = I32x2_x(offset),
						.y = I32x2_y(offset),
					},
					.extent = (VkExtent2D) {
						.width = I32x2_x(size),
						.height = I32x2_y(size),
					}
				};

			break;
		}

		case ECommandOp_SetStencil:
			temp->tempStencilRef = *(const U8*) data;
			break;

		case ECommandOp_SetBlendConstants:

			Buffer_copy(
				Buffer_createRef(&temp->tempBlendConstants, sizeof(F32x4)), 
				Buffer_createRefConst(data, sizeof(F32x4))
			);

			break;

		//Clear and copy commands

		case ECommandOp_ClearImages: {

			U32 imageClearCount = *(const U32*) data;

			for(U64 i = 0; i < imageClearCount; ++i) {

				//Get image

				ClearImageCmd image = ((const ClearImageCmd*) (data + sizeof(U32)))[i];

				VkManagedImage *imageExt = NULL;

				if(image.image->typeId == EGraphicsTypeId_Swapchain) {

					Swapchain *swapchain = SwapchainRef_ptr(image.image);
					VkSwapchain *swapchainExt = Swapchain_ext(swapchain, Vk);

					imageExt = &swapchainExt->images.ptrNonConst[swapchainExt->currentIndex];
				}

				else imageExt = (VkManagedImage*) RenderTexture_ext(RenderTextureRef_ptr(image.image), );

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

			break;
		}

		case ECommandOp_CopyImage: {

			CopyImageCmd copyImage = *(const CopyImageCmd*) data;
			const CopyImageRegion *copyImageRegions = (const CopyImageRegion*) (data + sizeof(copyImage));

			VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

			I32x4 srcSize = I32x4_zero();

			if(copyImage.src->typeId == EGraphicsTypeId_DepthStencil) {
				aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
				srcSize = I32x4_fromI32x2(DepthStencilRef_ptr(copyImage.src)->size);
			}

			else if(copyImage.src->typeId == EGraphicsTypeId_Swapchain)
				srcSize = I32x4_fromI32x2(SwapchainRef_ptr(copyImage.src)->size);

			else srcSize = RenderTextureRef_ptr(copyImage.src)->size;		//TODO: DeviceTexture

			if(copyImage.copyType == ECopyType_DepthOnly)
				aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

			else if(copyImage.copyType == ECopyType_StencilOnly)
				aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;

			for(U64 i = 0; i < copyImage.regionCount; ++i) {

				CopyImageRegion image = copyImageRegions[i];

				if(!image.width)
					image.width = (U32)I32x4_x(srcSize) - image.srcX;

				if(!image.height)
					image.height = (U32)I32x4_y(srcSize) - image.srcY;

				if(!image.length)
					image.length = (U32)U64_max((U32)I32x4_z(srcSize), 1) - image.srcZ;

				VkImageSubresourceLayers subResource = (VkImageSubresourceLayers) {
					.aspectMask = aspectFlags,
					.layerCount = 1
				};

				Error err = Error_none();

				_gotoIfError(next, ListVkImageCopy_pushBackx(&deviceExt->imageCopyRanges, (VkImageCopy) {
					.srcSubresource = subResource,
					.srcOffset = (VkOffset3D) {
						.x = (I32) image.srcX,
						.y = (I32) image.srcY,
						.z = (I32) image.srcZ
					},
					.dstSubresource = subResource,
					.dstOffset = (VkOffset3D) {
						.x = (I32) image.dstX,
						.y = (I32) image.dstY,
						.z = (I32) image.dstZ
					},
					.extent = (VkExtent3D) {
						.width = image.width,
						.height = image.height,
						.depth = image.length
					}
				}));

			next:

				if(err.genericError) {
					Error_printx(err, ELogLevel_Error, ELogOptions_Default);
					break;
				}
			}

			VkManagedImage *srcExt = NULL;

			if(copyImage.src->typeId == EGraphicsTypeId_Swapchain) {
				VkSwapchain *swapchainExt = Swapchain_ext(SwapchainRef_ptr(copyImage.src), Vk);
				srcExt = &swapchainExt->images.ptrNonConst[swapchainExt->currentIndex];
			}

			else if(copyImage.src->typeId == EGraphicsTypeId_DepthStencil)
				srcExt = (VkManagedImage*) DepthStencil_ext(DepthStencilRef_ptr(copyImage.src), );

			else srcExt = (VkManagedImage*) RenderTexture_ext(RenderTextureRef_ptr(copyImage.src), );

			VkManagedImage *dstExt = NULL;

			if(copyImage.dst->typeId == EGraphicsTypeId_Swapchain) {
				VkSwapchain *swapchainExt = Swapchain_ext(SwapchainRef_ptr(copyImage.dst), Vk);
				dstExt = &swapchainExt->images.ptrNonConst[swapchainExt->currentIndex];
			}

			else if(copyImage.src->typeId == EGraphicsTypeId_DepthStencil)
				dstExt = (VkManagedImage*) DepthStencil_ext(DepthStencilRef_ptr(copyImage.dst), );

			else dstExt = (VkManagedImage*) RenderTexture_ext(RenderTextureRef_ptr(copyImage.dst), );

			vkCmdCopyImage(
				buffer, 
				srcExt->image,
				srcExt->lastLayout,
				dstExt->image,
				dstExt->lastLayout,
				copyImage.regionCount,
				deviceExt->imageCopyRanges.ptr
			);

			ListVkImageCopy_clear(&deviceExt->imageCopyRanges);
			break;
		}

		//Dynamic rendering / direct rendering

		case ECommandOp_StartRenderingExt: {

			const StartRenderCmdExt *startRender = (const StartRenderCmdExt*) data;
			const AttachmentInfoInternal *attachments = (const AttachmentInfoInternal*) (startRender + 1);

			//Prepare attachments

			VkRenderingAttachmentInfoKHR attachmentsExt[8] = { 0 };
			U8 j = U8_MAX;

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

				const AttachmentInfoInternal *attachmentsj = &attachments[j];

				VkManagedImage *imageExt = NULL;

				if(attachmentsj->image->typeId == EGraphicsTypeId_Swapchain) {

					Swapchain *swapchain = SwapchainRef_ptr(attachmentsj->image);
					VkSwapchain *swapchainExt = Swapchain_ext(swapchain, Vk);

					imageExt = &swapchainExt->images.ptrNonConst[swapchainExt->currentIndex];
				}

				else imageExt = (VkManagedImage*) RenderTexture_ext(RenderTextureRef_ptr(attachmentsj->image), );

				VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

				if((startRender->clearMask >> i) & 1)
					loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

				else if((startRender->preserveMask >> i) & 1)
					loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

				attachmentsExt[i] = (VkRenderingAttachmentInfoKHR) {

					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = imageExt->view,
					.imageLayout = imageExt->lastLayout,
					.loadOp = loadOp,

					.storeOp = 
						!((startRender->unusedAfterRenderMask >> i) & 1) ? VK_ATTACHMENT_STORE_OP_STORE:
						VK_ATTACHMENT_STORE_OP_DONT_CARE,

					.clearValue = *(const VkClearValue*) &attachmentsj->color
				};

				if(attachmentsj->resolveImage)
					addResolveImage(*attachmentsj, &attachmentsExt[i]);
			}

			//Send begin render command

			VkRenderingAttachmentInfoKHR depthAttachment;
			VkRenderingAttachmentInfoKHR stencilAttachment;

			if (startRender->flags & EStartRenderFlags_Depth) {

				DepthStencil *depth = DepthStencilRef_ptr(startRender->depth);
				VkManagedImage *depthExt = (VkManagedImage*) DepthStencil_ext(depth, );

				Bool unusedAfterRender = startRender->flags & EStartRenderFlags_DepthUnusedAfterRender;

				VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

				if(startRender->flags & EStartRenderFlags_ClearDepth)
					loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

				else if(startRender->flags & EStartRenderFlags_PreserveDepth)
					loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				
				depthAttachment = (VkRenderingAttachmentInfoKHR) {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = depthExt->view,
					.imageLayout = depthExt->lastLayout,
					.loadOp = loadOp,
					.storeOp = unusedAfterRender ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = (VkClearColorValue) { .float32 = { startRender->clearDepth } }
				};

				if(startRender->resolveDepth) {

					AttachmentInfoInternal tmp = (AttachmentInfoInternal) { 
						.resolveImage = startRender->resolveDepth,
						.resolveMode = startRender->resolveDepthMode
					};

					addResolveImage(tmp, &depthAttachment);
				}
			}

			if (startRender->flags & EStartRenderFlags_Stencil) {

				DepthStencil *stencil = DepthStencilRef_ptr(startRender->stencil);
				VkManagedImage *stencilExt = (VkManagedImage*) DepthStencil_ext(stencil, );

				Bool unusedAfterRender = startRender->flags & EStartRenderFlags_StencilUnusedAfterRender;

				VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

				if(startRender->flags & EStartRenderFlags_ClearStencil)
					loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

				else if(startRender->flags & EStartRenderFlags_PreserveStencil)
					loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

				stencilAttachment = (VkRenderingAttachmentInfoKHR) {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = stencilExt->view,
					.imageLayout = stencilExt->lastLayout,
					.loadOp = loadOp,
					.storeOp = unusedAfterRender ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = (VkClearColorValue) { .uint32 = { startRender->clearStencil } }
				};

				if(startRender->resolveStencil) {

					AttachmentInfoInternal tmp = (AttachmentInfoInternal) { 
						.resolveImage = startRender->resolveStencil,
						.resolveMode = startRender->resolveStencilMode
					};

					addResolveImage(tmp, &stencilAttachment);
				}
			}

			VkRenderingInfoKHR renderInfo = (VkRenderingInfoKHR) {
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
				.renderArea = (VkRect2D) {
					.offset = (VkOffset2D) {
						.x = I32x2_x(startRender->offset),
						.y = I32x2_y(startRender->offset)
					},
					.extent = (VkExtent2D) {
						.width = I32x2_x(startRender->size),
						.height = I32x2_y(startRender->size)
					}
				},
				.layerCount = 1,
				.colorAttachmentCount = startRender->colorCount,
				.pColorAttachments = attachmentsExt,
				.pDepthAttachment = startRender->flags & EStartRenderFlags_Depth ? &depthAttachment : NULL,
				.pStencilAttachment = startRender->flags & EStartRenderFlags_Stencil ? &stencilAttachment : NULL
			};

			instanceExt->cmdBeginRendering(buffer, &renderInfo);
			break;
		}

		case ECommandOp_EndRenderingExt:
			instanceExt->cmdEndRendering(buffer);
			break;

		//Draws

		case ECommandOp_SetGraphicsPipeline:
			temp->tempPipelines[EPipelineType_Graphics] = *(PipelineRef* const*) data;
			break;

		case ECommandOp_SetComputePipeline:
			temp->tempPipelines[EPipelineType_Compute] = *(PipelineRef* const*) data;
			break;

		case ECommandOp_SetPrimitiveBuffers:
			temp->tempBoundBuffers = *(const SetPrimitiveBuffersCmd*) data;
			break;

		case ECommandOp_DrawIndirect:
		case ECommandOp_DrawIndirectCount:
		case ECommandOp_Draw:

			//Bind viewport and scissor

			Bool eq = Buffer_eq(
				Buffer_createRefConst(&temp->boundViewport, sizeof(VkViewport)),
				Buffer_createRefConst(&temp->tempViewport, sizeof(VkViewport))
			);

			if(!eq) {
				temp->boundViewport = temp->tempViewport;
				vkCmdSetViewport(buffer, 0, 1, &temp->boundViewport);
			}

			eq = Buffer_eq(
				Buffer_createRefConst(&temp->boundScissor, sizeof(VkRect2D)),
				Buffer_createRefConst(&temp->tempScissor, sizeof(VkRect2D))
			);

			if(!eq) {
				temp->boundScissor = temp->tempScissor;
				vkCmdSetScissor(buffer, 0, 1, &temp->boundScissor);
			}

			//Bind blend constants and/or stencil ref

			if (F32x4_neq4(temp->tempBlendConstants, temp->blendConstants)) {
				temp->blendConstants = temp->tempBlendConstants;
				vkCmdSetBlendConstants(buffer, (const float*) &temp->blendConstants);
			}

			if (temp->tempStencilRef != temp->stencilRef) {
				temp->stencilRef = temp->tempStencilRef;
				vkCmdSetStencilReference(buffer, VK_STENCIL_FACE_FRONT_AND_BACK, temp->stencilRef);
			}

			//Bind pipeline

			if(temp->pipelines[EPipelineType_Graphics] != temp->tempPipelines[EPipelineType_Graphics]) {

				temp->pipelines[EPipelineType_Graphics] = temp->tempPipelines[EPipelineType_Graphics];

				vkCmdBindPipeline(
					temp->buffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					*Pipeline_ext(PipelineRef_ptr(temp->pipelines[EPipelineType_Graphics]), Vk)
				);
			}

			//Bind index buffer

			if (
				temp->tempBoundBuffers.indexBuffer && 
				(
					temp->boundBuffers.indexBuffer != temp->tempBoundBuffers.indexBuffer || 
					temp->boundBuffers.isIndex32Bit != temp->tempBoundBuffers.isIndex32Bit
				)
			) {

				temp->boundBuffers.indexBuffer = temp->tempBoundBuffers.indexBuffer;
				temp->boundBuffers.isIndex32Bit = temp->tempBoundBuffers.isIndex32Bit;

				vkCmdBindIndexBuffer(
					temp->buffer,
					DeviceBuffer_ext(DeviceBufferRef_ptr(temp->boundBuffers.indexBuffer), Vk)->buffer, 
					0,
					temp->boundBuffers.isIndex32Bit ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16
				);
			}

			//Bind vertex buffers

			{
				VkBuffer vertexBuffers[16] = { 0 };
				VkDeviceSize vertexBufferOffsets[16] = { 0 };

				U32 start = 16, end = 0;

				//Fill vertexBuffers and find start/end range.
				//And ensure bound buffers can't be accidentally transitioned while render hasn't ended yet.

				for(U32 i = 0; i < 16; ++i) {

					DeviceBufferRef *bufferRef = temp->tempBoundBuffers.vertexBuffers[i];

					if (!bufferRef)
						continue;

					if(temp->boundBuffers.vertexBuffers[i] != bufferRef) {

						if(start == 16)
							start = i;

						end = i + 1;

						temp->boundBuffers.vertexBuffers[i] = bufferRef;
					}

					vertexBuffers[i] = DeviceBuffer_ext(DeviceBufferRef_ptr(bufferRef), Vk)->buffer;
				}

				if(end > start)
					vkCmdBindVertexBuffers(
						temp->buffer,
						start,
						end - start, 
						&(vertexBuffers)[start], 
						&(vertexBufferOffsets)[start]
					);
			}

			//Direct draws

			if(op == ECommandOp_Draw) {

				DrawCmd draw = *(const DrawCmd*)data;

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

			//Indirect draws

			else {

				DrawIndirectCmd drawIndirect = *(const DrawIndirectCmd*)data;
				VkDeviceBuffer *bufferExt = DeviceBuffer_ext(DeviceBufferRef_ptr(drawIndirect.buffer), Vk);

				//Indirect draw count

				if (drawIndirect.countBufferExt) {

					DeviceBuffer *counterBuffer = DeviceBufferRef_ptr(drawIndirect.countBufferExt);
					VkDeviceBuffer *counterExt = DeviceBuffer_ext(counterBuffer, Vk);

					if(drawIndirect.isIndexed)
						vkCmdDrawIndexedIndirectCount(
							buffer, 
							bufferExt->buffer, drawIndirect.bufferOffset, 
							counterExt->buffer, drawIndirect.countOffsetExt, 
							drawIndirect.drawCalls, drawIndirect.bufferStride
						);

					else vkCmdDrawIndirectCount(
						buffer,
						bufferExt->buffer, drawIndirect.bufferOffset,
						counterExt->buffer, drawIndirect.countOffsetExt, 
						drawIndirect.drawCalls, drawIndirect.bufferStride
					);
				}

				//Indirect draw (non count)

				else {

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
			}

			break;

		case ECommandOp_DispatchIndirect:
		case ECommandOp_Dispatch: 

			if(temp->pipelines[EPipelineType_Compute] != temp->tempPipelines[EPipelineType_Compute]) {

				temp->pipelines[EPipelineType_Compute] = temp->tempPipelines[EPipelineType_Compute];

				vkCmdBindPipeline(
					temp->buffer,
					VK_PIPELINE_BIND_POINT_COMPUTE,
					*Pipeline_ext(PipelineRef_ptr(temp->pipelines[EPipelineType_Compute]), Vk)
				);
			}

			if(op == ECommandOp_Dispatch) {
				DispatchCmd dispatch = *(const DispatchCmd*)data;
				vkCmdDispatch(buffer, dispatch.groups[0], dispatch.groups[1], dispatch.groups[2]);
			}

			else {
				DispatchIndirectCmd dispatch = *(const DispatchIndirectCmd*)data;
				VkDeviceBuffer *bufferExt = DeviceBuffer_ext(DeviceBufferRef_ptr(dispatch.buffer), Vk);
				vkCmdDispatchIndirect(buffer, bufferExt->buffer, dispatch.offset);
			}

			break;

		case ECommandOp_StartScope: {

			VkDependencyInfo dependency = (VkDependencyInfo) {
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.dependencyFlags = 0
			};

			U32 graphicsQueueId = deviceExt->queues[EVkCommandQueue_Graphics].queueId;

			CommandScope scope = commandList->activeScopes.ptr[temp->scopeCounter];
			++temp->scopeCounter;

			Error err = Error_none();

			for (U64 i = scope.transitionOffset; i < scope.transitionOffset + scope.transitionCount; ++i) {

				TransitionInternal transition = commandList->transitions.ptr[i];

				//Grab transition type
				
				Bool isSwapchain = transition.resource->typeId == EGraphicsTypeId_Swapchain;
				Bool isRenderTexture = transition.resource->typeId == EGraphicsTypeId_RenderTexture;
				Bool isDepthStencil = transition.resource->typeId == EGraphicsTypeId_DepthStencil;
				Bool isDeviceTexture = transition.resource->typeId == EGraphicsTypeId_DeviceTexture;
				Bool isImage = isSwapchain || isRenderTexture || isDepthStencil || isDeviceTexture;
				Bool isShaderRead = transition.type == ETransitionType_ShaderRead;

				VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;
				VkAccessFlags2 access = 0;

				if(isImage) {
					access = isShaderRead ? VK_ACCESS_2_SHADER_SAMPLED_READ_BIT : VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
					layout = isShaderRead ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
				}

				if (!isImage)
					access = 
						transition.type == ETransitionType_ShaderRead ? VK_ACCESS_2_SHADER_STORAGE_READ_BIT : 
						VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

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

					default:		//Indicates special usage

						switch ((ETransitionType) transition.type) {

							case ETransitionType_RenderTargetRead:

								pipelineStage = 
									isDepthStencil ? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT :
									VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

								access = 
									isDepthStencil ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT :
									VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;

								layout = 
									isDepthStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL :
									VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

								break;

							case ETransitionType_RenderTargetWrite:

								pipelineStage = 
									isDepthStencil ? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT :
									VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

								access = 
									isDepthStencil ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
									VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT :
									VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | 
									VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

								layout = 
									isDepthStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
									VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

								break;

							case ETransitionType_Clear:
								pipelineStage = VK_PIPELINE_STAGE_2_CLEAR_BIT;
								access = VK_ACCESS_2_TRANSFER_WRITE_BIT;
								layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
								break;

							case ETransitionType_CopyRead:
								pipelineStage =  VK_PIPELINE_STAGE_2_COPY_BIT;
								access = VK_ACCESS_2_TRANSFER_READ_BIT;
								layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
								break;

							case ETransitionType_CopyWrite:
								pipelineStage =  VK_PIPELINE_STAGE_2_COPY_BIT;
								access = VK_ACCESS_2_TRANSFER_WRITE_BIT;
								layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
								break;

							case ETransitionType_Indirect:
								pipelineStage = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
								access = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
								break;

							case ETransitionType_Index:
								pipelineStage = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
								access = VK_ACCESS_2_INDEX_READ_BIT;
								break;

							case ETransitionType_Vertex:
								pipelineStage = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
								access = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
								break;
						}

						break;
				}

				//Transition resource

				if(isImage) {

					VkManagedImage *imageExt = NULL;

					if(isSwapchain) {
						VkSwapchain *swapchainExt = Swapchain_ext(SwapchainRef_ptr(transition.resource), Vk);
						imageExt = &swapchainExt->images.ptrNonConst[swapchainExt->currentIndex];
					}

					else if(isRenderTexture)
						imageExt = (VkManagedImage*) RenderTexture_ext(RenderTextureRef_ptr(transition.resource), );

					else if(isDeviceTexture)
						imageExt = (VkManagedImage*) DeviceTexture_ext(DeviceTextureRef_ptr(transition.resource), );

					else imageExt = (VkManagedImage*) DepthStencil_ext(DepthStencilRef_ptr(transition.resource), );

					VkImageSubresourceRange range = (VkImageSubresourceRange) {		//TODO:
						.aspectMask = isDepthStencil ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
						.levelCount = 1,
						.layerCount = 1
					};

					_gotoIfError(nextTransition, VkManagedImage_transition(

						imageExt, 
						pipelineStage,
						access,
						layout,
						graphicsQueueId,
						&range,

						&deviceExt->imageTransitions,
						&dependency
					));
				}

				else {

					DeviceBuffer *devBuffer = DeviceBufferRef_ptr(transition.resource);
					VkDeviceBuffer *bufferExt = DeviceBuffer_ext(devBuffer, Vk);

					_gotoIfError(nextTransition, VkDeviceBuffer_transition(
						bufferExt,
						pipelineStage,
						access,
						graphicsQueueId,
						0,						//TODO: range
						devBuffer->length,
						&deviceExt->bufferTransitions,
						&dependency
					));
				}

			nextTransition:
				
				if(err.genericError)
					Error_printx(err, ELogLevel_Error, ELogOptions_Default);
			}

			if(dependency.imageMemoryBarrierCount || dependency.bufferMemoryBarrierCount)
				instanceExt->cmdPipelineBarrier2(buffer, &dependency);

			ListVkBufferMemoryBarrier2_clear(&deviceExt->bufferTransitions);
			ListVkImageMemoryBarrier2_clear(&deviceExt->imageTransitions);
			break;
		}

		//Debug markers

		case ECommandOp_EndRegionDebugExt:
			instanceExt->cmdDebugMarkerEnd(buffer);
			break;

		case ECommandOp_AddMarkerDebugExt:
		case ECommandOp_StartRegionDebugExt: {

			VkDebugMarkerMarkerInfoEXT markerInfo = (VkDebugMarkerMarkerInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT,
				.pMarkerName = (const char*) data + sizeof(F32x4),
			};

			Buffer_copy(Buffer_createRef(&markerInfo.color, sizeof(F32x4)), Buffer_createRefConst(data, sizeof(F32x4)));
			
			if(op == ECommandOp_AddMarkerDebugExt)
				instanceExt->cmdDebugMarkerInsert(buffer, &markerInfo);
			
			else instanceExt->cmdDebugMarkerBegin(buffer, &markerInfo);

			break;
		}

		//No-op, only used for virtual command list

		case ECommandOp_EndScope:
			break;

		//Unsupported

		default:
			Log_errorLnx("Unsupported command issued.");
			break;
	}
}
