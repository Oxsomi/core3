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
#include "graphics/generic/interface.h"
#include "graphics/vulkan/vk_interface.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/blas.h"
#include "graphics/vulkan/vulkan.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_buffer.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/errorx.h"
#include "platforms/log.h"
#include "types/container/buffer.h"
#include "types/base/error.h"

void addResolveImage(AttachmentInfoInternal attachment, VkRenderingAttachmentInfoKHR *result) {

	VkUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(attachment.resolveImage, Vk, 0);

	switch (attachment.resolveMode) {
		default:						result->resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;	break;
		case EMSAAResolveMode_Min:		result->resolveMode = VK_RESOLVE_MODE_MIN_BIT;		break;
		case EMSAAResolveMode_Max:		result->resolveMode = VK_RESOLVE_MODE_MAX_BIT;		break;
	}

	result->resolveImageView = imageExt->view;
	result->resolveImageLayout = imageExt->lastLayout;
}

void VK_WRAP_FUNC(CommandList_process)(
	CommandList *commandList,
	GraphicsDeviceRef *deviceRef,
	ECommandOp op,
	const U8 *data,
	void *commandListExt
) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
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

			U64 imageClearCount = *(const U64*) data;

			for(U64 i = 0; i < imageClearCount; ++i) {

				ClearImageCmd image = ((const ClearImageCmd*) (data + sizeof(U64)))[i];
				VkUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(image.image, Vk, 0);

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

			UnifiedTexture src = TextureRef_getUnifiedTexture(copyImage.src, NULL);

			if(src.depthFormat) {

				if(copyImage.copyType == ECopyType_DepthOnly)
					aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

				else if(copyImage.copyType == ECopyType_StencilOnly)
					aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;

				else aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			}

			for(U64 i = 0; i < copyImage.regionCount; ++i) {

				CopyImageRegion image = copyImageRegions[i];

				if(!image.width)
					image.width = src.width - image.srcX;

				if(!image.height)
					image.height = src.height - image.srcY;

				if(!image.length)
					image.length = src.length - image.srcZ;

				VkImageSubresourceLayers subResource = (VkImageSubresourceLayers) {
					.aspectMask = aspectFlags,
					.layerCount = 1
				};

				Error err = Error_none();

				gotoIfError(next, ListVkImageCopy_pushBackx(&deviceExt->imageCopyRanges, (VkImageCopy) {
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
				}))

			next:

				if(err.genericError) {
					Error_printx(err, ELogLevel_Error, ELogOptions_Default);
					break;
				}
			}

			VkUnifiedTexture *srcExt = TextureRef_getCurrImgExtT(copyImage.src, Vk, 0);
			VkUnifiedTexture *dstExt = TextureRef_getCurrImgExtT(copyImage.dst, Vk, 0);

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
			U8 j = 0;

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

				const AttachmentInfoInternal *attachmentsj = &attachments[j++];

				VkUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(attachmentsj->image, Vk, 0);
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

					.clearValue = (VkClearValue) {
						.color = (VkClearColorValue) {
							.float32 = {
								attachmentsj->color.colorf[0],
								attachmentsj->color.colorf[1],
								attachmentsj->color.colorf[2],
								attachmentsj->color.colorf[3]
							}
						}
					}
				};

				if(attachmentsj->resolveImage)
					addResolveImage(*attachmentsj, &attachmentsExt[i]);
			}

			//Send begin render command

			VkRenderingAttachmentInfoKHR depthAttachment;
			VkRenderingAttachmentInfoKHR stencilAttachment;

			if (startRender->flags & EStartRenderFlags_Depth) {

				VkUnifiedTexture *depthExt = TextureRef_getCurrImgExtT(startRender->depthStencil, Vk, 0);

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
					.clearValue = (VkClearValue) {
						.depthStencil = (VkClearDepthStencilValue) { .depth = startRender->clearDepth }
					}
				};

				if(startRender->resolveDepthStencil) {

					AttachmentInfoInternal tmp = (AttachmentInfoInternal) {
						.resolveImage = startRender->resolveDepthStencil,
						.resolveMode = startRender->resolveDepthStencilMode
					};

					addResolveImage(tmp, &depthAttachment);
				}
			}

			if (startRender->flags & EStartRenderFlags_Stencil) {

				VkUnifiedTexture *stencilExt = TextureRef_getCurrImgExtT(startRender->depthStencil, Vk, 0);

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
					.clearValue = (VkClearValue) {
						.depthStencil = (VkClearDepthStencilValue) { .stencil = startRender->clearStencil }
					}
				};

				if(startRender->resolveDepthStencil) {

					AttachmentInfoInternal tmp = (AttachmentInfoInternal) {
						.resolveImage = startRender->resolveDepthStencil,
						.resolveMode = startRender->resolveDepthStencilMode
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

		case ECommandOp_SetRaytracingPipelineExt:
			temp->tempPipelines[EPipelineType_RaytracingExt] = *(PipelineRef* const*) data;
			break;

		case ECommandOp_SetPrimitiveBuffers:
			temp->tempBoundBuffers = *(const SetPrimitiveBuffersCmd*) data;
			break;

		case ECommandOp_DrawIndirect:
		case ECommandOp_DrawIndirectCount:
		case ECommandOp_Draw: {

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
					draw.vertexOffset, draw.instanceOffset
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
							drawIndirect.drawCalls, sizeof(DrawCallIndexed)
						);

					else vkCmdDrawIndirectCount(
						buffer,
						bufferExt->buffer, drawIndirect.bufferOffset,
						counterExt->buffer, drawIndirect.countOffsetExt,
						drawIndirect.drawCalls, sizeof(DrawCallUnindexed)
					);
				}

				//Indirect draw (non count)

				else {

					if(drawIndirect.isIndexed)
						vkCmdDrawIndexedIndirect(
							buffer,
							bufferExt->buffer, drawIndirect.bufferOffset,
							drawIndirect.drawCalls, sizeof(DrawCallIndexed)
						);

					else vkCmdDrawIndirect(
						buffer, bufferExt->buffer, drawIndirect.bufferOffset,
						drawIndirect.drawCalls, sizeof(DrawCallUnindexed)
					);
				}
			}

			break;
		}

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
				vkCmdDispatch(
					buffer, dispatch.groups[0], dispatch.groups[1], dispatch.groups[2]
				);
			}

			else {
				DispatchIndirectCmd dispatch = *(const DispatchIndirectCmd*)data;
				VkDeviceBuffer *bufferExt = DeviceBuffer_ext(DeviceBufferRef_ptr(dispatch.buffer), Vk);
				vkCmdDispatchIndirect(buffer, bufferExt->buffer, dispatch.offset);
			}

			break;

		//JIT RTAS updates in case they are on the GPU (e.g. compute updates)

		case ECommandOp_UpdateBLASExt:
			(VK_WRAP_FUNC(BLASRef_flush))(temp, deviceRef, *(BLASRef**)data);
			break;

		case ECommandOp_UpdateTLASExt:
			(VK_WRAP_FUNC(TLASRef_flush))(temp, deviceRef, *(TLASRef**)data);
			break;

		//case ECommandOp_DispatchRaysIndirect:
		case ECommandOp_DispatchRaysExt: {

			Pipeline *raytracingPipeline = PipelineRef_ptr(temp->tempPipelines[EPipelineType_RaytracingExt]);

			if(temp->pipelines[EPipelineType_RaytracingExt] != temp->tempPipelines[EPipelineType_RaytracingExt]) {

				temp->pipelines[EPipelineType_RaytracingExt] = temp->tempPipelines[EPipelineType_RaytracingExt];

				vkCmdBindPipeline(
					temp->buffer,
					VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
					*Pipeline_ext(raytracingPipeline, Vk)
				);
			}

			PipelineRaytracingInfo info = *Pipeline_info(raytracingPipeline, PipelineRaytracingInfo);

			VkStridedDeviceAddressRegionKHR hit = (VkStridedDeviceAddressRegionKHR) {
				.deviceAddress = getVkDeviceAddress((DeviceData) { .buffer = info.shaderBindingTable }),
				.size = (U64)raytracingShaderAlignment * info.groups.length,
				.stride = raytracingShaderAlignment
			};

			VkStridedDeviceAddressRegionKHR miss = (VkStridedDeviceAddressRegionKHR) {
				.deviceAddress = hit.deviceAddress + hit.size,
				.size = (U64)raytracingShaderAlignment * info.missCount,
				.stride = raytracingShaderAlignment
			};

			VkStridedDeviceAddressRegionKHR raygen = (VkStridedDeviceAddressRegionKHR) {
				.deviceAddress = miss.deviceAddress + miss.size,
				.size = raytracingShaderAlignment,
				.stride = raytracingShaderAlignment
			};

			VkStridedDeviceAddressRegionKHR callable = (VkStridedDeviceAddressRegionKHR) {
				.deviceAddress = raygen.deviceAddress + raygen.size * info.raygenCount,
				.size = (U64)raytracingShaderAlignment * info.callableCount,
				.stride = raytracingShaderAlignment
			};

			if(!info.groups.length)
				hit = (VkStridedDeviceAddressRegionKHR) { 0 };

			if(!info.missCount)
				miss = (VkStridedDeviceAddressRegionKHR) { 0 };

			if(!info.callableCount)
				callable = (VkStridedDeviceAddressRegionKHR) { 0 };

			if(op == ECommandOp_DispatchRaysExt) {
				DispatchRaysExt dispatch = *(const DispatchRaysExt*)data;
				raygen.deviceAddress += raygen.stride * dispatch.raygenId;
				instanceExt->traceRays(
					buffer,
					&raygen, &miss, &hit, &callable,
					dispatch.x, dispatch.y, dispatch.z
				);
			}

			else {
				DispatchRaysIndirectExt dispatch = *(const DispatchRaysIndirectExt*)data;
				raygen.deviceAddress += raygen.stride * dispatch.raygenId;
				instanceExt->traceRaysIndirect(
					buffer,
					&raygen, &miss, &hit, &callable,
					DeviceBufferRef_ptr(dispatch.buffer)->resource.deviceAddress + dispatch.offset
				);
			}

			break;
		}

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

				if(transition.type == ETransitionType_KeepAlive)		//TODO: Residency management
					continue;

				VkPipelineStageFlags2 pipelineStage = 0;

				switch (transition.stage) {

					default:																						break;
					case EPipelineStage_Compute:		pipelineStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;		break;
					case EPipelineStage_Vertex:			pipelineStage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;		break;
					case EPipelineStage_Pixel:			pipelineStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;	break;
					case EPipelineStage_GeometryExt:	pipelineStage = VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;	break;

					case EPipelineStage_Hull:
						pipelineStage = VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT;
						break;

					case EPipelineStage_Domain:
						pipelineStage = VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT;
						break;

					case EPipelineStage_RaygenExt:
					case EPipelineStage_CallableExt:
					case EPipelineStage_MissExt:
					case EPipelineStage_ClosestHitExt:
					case EPipelineStage_AnyHitExt:
						pipelineStage = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
						break;

					case EPipelineStage_RTASBuild:
						pipelineStage = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
						break;
				}

				//If it's on the GPU then we have to rely on manual RTAS transitions

				Bool isTLAS = transition.resource->typeId == (ETypeId)EGraphicsTypeId_TLASExt;

				if (isTLAS || transition.resource->typeId == (ETypeId)EGraphicsTypeId_BLASExt) {

					RTAS rtas = isTLAS ? TLASRef_ptr(transition.resource)->base : BLASRef_ptr(transition.resource)->base;

					//Read to RTAS is illegal if it's not initialized yet.
					//However, if ShaderWrite is used on an RTAS then it will be written in the current scope.
					//In that case, we want to transition to write of course.
					if (!rtas.isCompleted && transition.type != ETransitionType_ShaderWrite)
						continue;

					gotoIfError(nextTransition, VkDeviceBuffer_transition(

						DeviceBuffer_ext(DeviceBufferRef_ptr(rtas.asBuffer), Vk),
						pipelineStage,

						transition.type == ETransitionType_ShaderWrite ? VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR :
						VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,

						graphicsQueueId,
						0, 0,
						&deviceExt->bufferTransitions,
						&dependency
					))

					continue;
				}

				//Grab transition type

				Bool isImage = TextureRef_isTexture(transition.resource);
				Bool isDepthStencil = TextureRef_isDepthStencil(transition.resource);
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

				if(transition.stage == EPipelineStage_RTASBuild && !isShaderRead)	//Scratch buffer is AS write
					access = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

				if(!pipelineStage)
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

						case ETransitionType_ResolveTargetWrite:		//No distinction in Vulkan
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

						default:
							break;
					}

				//Transition resource

				if(isImage) {

					const UnifiedTexture utex = TextureRef_getUnifiedTexture(transition.resource, NULL);
					VkUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(transition.resource, Vk, 0);

					VkImageSubresourceRange range = (VkImageSubresourceRange) {		//TODO:
						.aspectMask = isDepthStencil ? 0 : VK_IMAGE_ASPECT_COLOR_BIT,
						.levelCount = 1,
						.layerCount = 1
					};

					if(isDepthStencil) {

						if(utex.depthFormat >= EDepthStencilFormat_StencilStart)
							range.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

						if(utex.depthFormat != EDepthStencilFormat_S8X24Ext)
							range.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
					}

					gotoIfError(nextTransition, VkUnifiedTexture_transition(

						imageExt,
						pipelineStage,
						access,
						layout,
						graphicsQueueId,
						&range,

						&deviceExt->imageTransitions,
						&dependency
					))
				}

				else {

					DeviceBuffer *devBuffer = DeviceBufferRef_ptr(transition.resource);
					VkDeviceBuffer *bufferExt = DeviceBuffer_ext(devBuffer, Vk);

					gotoIfError(nextTransition, VkDeviceBuffer_transition(
						bufferExt,
						pipelineStage,
						access,
						graphicsQueueId,
						0,						//TODO: range
						devBuffer->resource.size,
						&deviceExt->bufferTransitions,
						&dependency
					))
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

			if(instanceExt->cmdDebugMarkerEnd)
				instanceExt->cmdDebugMarkerEnd(buffer);

			break;

		case ECommandOp_AddMarkerDebugExt:
		case ECommandOp_StartRegionDebugExt: {

			VkDebugMarkerMarkerInfoEXT markerInfo = (VkDebugMarkerMarkerInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT,
				.pMarkerName = (const char*) data + sizeof(F32x4),
			};

			Buffer_copy(
				Buffer_createRef(&markerInfo.color, sizeof(F32x4)),
				Buffer_createRefConst(data, sizeof(F32x4))
			);

			if(!instanceExt->cmdDebugMarkerInsert)		//No debug markers
				break;

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
