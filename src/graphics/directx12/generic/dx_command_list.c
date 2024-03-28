/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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
#include "graphics/generic/tlas.h"
#include "graphics/generic/blas.h"
#include "graphics/directx12/dx_device.h"
#include "graphics/directx12/dx_buffer.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/errorx.h"
#include "platforms/log.h"
#include "types/buffer.h"
#include "types/error.h"

#include "types/math.h"

void CommandList_process(
	CommandList *commandList,
	GraphicsDevice *device,
	ECommandOp op,
	const U8 *data,
	void *commandListExt
) {

	(void) commandList;

	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	(void) deviceExt;

	DxCommandBufferState *temp = (DxCommandBufferState*) commandListExt;
	DxCommandBuffer *buffer = temp->buffer;

	ID3D12GraphicsCommandList *graphicsCmd = NULL;
	if(FAILED(buffer->lpVtbl->QueryInterface(buffer, &IID_ID3D12GraphicsCommandList, (void**) &graphicsCmd))) {
		Log_errorLnx("Invalid graphics command list");
		return;
	}

	switch (op) {

		case ECommandOp_SetViewport:
		case ECommandOp_SetScissor:
		case ECommandOp_SetViewportAndScissor: {

			I32x2 offset = ((const I32x2*) data)[0];
			I32x2 size = ((const I32x2*) data)[1];

			if((op - ECommandOp_SetViewport + 1) & 1)
				temp->tempViewport = (D3D12_VIEWPORT) {
					.TopLeftX = (F32) I32x2_x(offset),
					.TopLeftY = (F32) I32x2_y(offset),
					.Width = (F32) I32x2_x(size),
					.Height = (F32) I32x2_y(size),
					.MinDepth = 1,
					.MaxDepth = 0
				};

			if ((op - ECommandOp_SetViewport + 1) & 2)
				temp->tempScissor = (D3D12_RECT) {
					.left = I32x2_x(offset),
					.top = I32x2_y(offset),
					.right = I32x2_x(offset) + I32x2_x(size),
					.bottom = I32x2_x(offset) + I32x2_y(size)
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

				ClearImageCmd image = ((const ClearImageCmd*) (data + sizeof(U32)))[i];
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

					.clearValue = *(const VkClearValue*) &attachmentsj->color
				};

				if(attachmentsj->resolveImage)
					addResolveImage(*attachmentsj, &attachmentsExt[i]);
			}

			//Send begin render command

			VkRenderingAttachmentInfoKHR depthAttachment;
			VkRenderingAttachmentInfoKHR stencilAttachment;

			if (startRender->flags & EStartRenderFlags_Depth) {

				VkUnifiedTexture *depthExt = TextureRef_getCurrImgExtT(startRender->depth, Vk, 0);

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

				if(startRender->resolveDepth) {

					AttachmentInfoInternal tmp = (AttachmentInfoInternal) {
						.resolveImage = startRender->resolveDepth,
						.resolveMode = startRender->resolveDepthMode
					};

					addResolveImage(tmp, &depthAttachment);
				}
			}

			if (startRender->flags & EStartRenderFlags_Stencil) {

				VkUnifiedTexture *stencilExt = TextureRef_getCurrImgExtT(startRender->stencil, Vk, 0);

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

		case ECommandOp_EndRenderingExt:		//No-Op, D3D12 has no such concept
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
				Buffer_createRefConst(&temp->boundViewport, sizeof(D3D12_VIEWPORT)),
				Buffer_createRefConst(&temp->tempViewport, sizeof(D3D12_VIEWPORT))
			);

			if(!eq) {
				temp->boundViewport = temp->tempViewport;
				buffer->lpVtbl->RSSetViewports(buffer, 1, &temp->boundViewport);
			}

			eq = Buffer_eq(
				Buffer_createRefConst(&temp->boundScissor, sizeof(D3D12_RECT)),
				Buffer_createRefConst(&temp->tempScissor, sizeof(D3D12_RECT))
			);

			if(!eq) {
				temp->boundScissor = temp->tempScissor;
				buffer->lpVtbl->RSSetScissorRects(buffer, 1, &temp->boundScissor);
			}

			//Bind blend constants and/or stencil ref

			if (F32x4_neq4(temp->tempBlendConstants, temp->blendConstants)) {
				temp->blendConstants = temp->tempBlendConstants;
				buffer->lpVtbl->OMSetBlendFactor(buffer, (const float*) &temp->blendConstants);
			}

			if (temp->tempStencilRef != temp->stencilRef) {
				temp->stencilRef = temp->tempStencilRef;
				buffer->lpVtbl->OMSetStencilRef(buffer, temp->stencilRef);
			}

			//Bind pipeline

			if(temp->pipeline != temp->tempPipelines[EPipelineType_Graphics]) {

				temp->pipeline = temp->tempPipelines[EPipelineType_Graphics];

				buffer->lpVtbl->SetPipelineState(
					temp->buffer,
					Pipeline_ext(PipelineRef_ptr(temp->pipeline), Dx)->pso
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

				DeviceBuffer *indexBuffer = DeviceBufferRef_ptr(temp->boundBuffers.indexBuffer);

				D3D12_INDEX_BUFFER_VIEW ibo = (D3D12_INDEX_BUFFER_VIEW) {
					.BufferLocation = getDxDeviceAddress((DeviceData) { .buffer = temp->boundBuffers.indexBuffer }),
					.SizeInBytes = indexBuffer->resource.size,
					.Format = temp->boundBuffers.isIndex32Bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT
				};

				buffer->lpVtbl->IASetIndexBuffer(temp->buffer, &ibo);
			}

			//Bind vertex buffers

			{
				D3D12_VERTEX_BUFFER_VIEW vertexBuffers[16] = { 0 };

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

					PipelineGraphicsInfo *graphicsShader = Pipeline_info(
						PipelineRef_ptr(temp->tempPipelines[EPipelineType_Graphics]), PipelineGraphicsInfo
					);

					DeviceBuffer *buf = DeviceBufferRef_ptr(bufferRef);

					vertexBuffers[i] = (D3D12_VERTEX_BUFFER_VIEW) {
						.BufferLocation = DeviceBuffer_ext(buf, Dx)->buffer,
						.SizeInBytes = buf->resource.size,
						.StrideInBytes = graphicsShader->vertexLayout.bufferStrides12_isInstance1[i] & 4095
					};
				}

				if(end > start)
					buffer->lpVtbl->IASetVertexBuffers(
						temp->buffer,
						start,
						end - start,
						&(vertexBuffers)[start]
					);
			}

			//Direct draws

			if(op == ECommandOp_Draw) {

				DrawCmd draw = *(const DrawCmd*)data;

				if(draw.isIndexed)
					buffer->lpVtbl->DrawIndexedInstanced(
						buffer,
						draw.count, draw.instanceCount,
						draw.indexOffset, draw.vertexOffset,
						draw.instanceOffset
					);

				else buffer->lpVtbl->DrawInstanced(
					buffer,
					draw.count, draw.instanceCount,
					draw.vertexOffset, draw.indexOffset
				);
			}

			//Indirect draws

			else {

				DrawIndirectCmd drawIndirect = *(const DrawIndirectCmd*)data;
				DxDeviceBuffer *bufferExt = DeviceBuffer_ext(DeviceBufferRef_ptr(drawIndirect.buffer), Dx);

				//Indirect draw count

				if (drawIndirect.countBufferExt) {

					DeviceBuffer *counterBuffer = DeviceBufferRef_ptr(drawIndirect.countBufferExt);
					DxDeviceBuffer *counterExt = DeviceBuffer_ext(counterBuffer, Dx);

					if(drawIndirect.isIndexed)
						buffer->lpVtbl->DrawIndexedIndirectCount(
							buffer,
							bufferExt->buffer, drawIndirect.bufferOffset,
							counterExt->buffer, drawIndirect.countOffsetExt,
							drawIndirect.drawCalls, drawIndirect.bufferStride
						);

					else buffer->lpVtbl->DrawIndirectCount(
						buffer,
						bufferExt->buffer, drawIndirect.bufferOffset,
						counterExt->buffer, drawIndirect.countOffsetExt,
						drawIndirect.drawCalls, drawIndirect.bufferStride
					);
				}

				//Indirect draw (non count)

				else {

					if(drawIndirect.isIndexed)
						buffer->lpVtbl->DrawIndexedIndirect(
							buffer,
							bufferExt->buffer, drawIndirect.bufferOffset,
							drawIndirect.drawCalls, drawIndirect.bufferStride
						);

					else buffer->lpVtbl->DrawIndirect(
						buffer, bufferExt->buffer, drawIndirect.bufferOffset,
						drawIndirect.drawCalls, drawIndirect.bufferStride
					);
				}
			}

			break;
		}

		case ECommandOp_DispatchIndirect:
		case ECommandOp_Dispatch:

			if(temp->pipeline != temp->tempPipelines[EPipelineType_Compute]) {

				temp->pipeline = temp->tempPipelines[EPipelineType_Compute];

				buffer->lpVtbl->SetPipelineState(
					temp->buffer,
					Pipeline_ext(PipelineRef_ptr(temp->pipeline), Dx)->pso
				);
			}

			if(op == ECommandOp_Dispatch) {
				DispatchCmd dispatch = *(const DispatchCmd*)data;
				buffer->lpVtbl->Dispatch(
					buffer, 
					dispatch.groups[0], dispatch.groups[1], dispatch.groups[2]
				);
			}

			else {
				DispatchIndirectCmd dispatch = *(const DispatchIndirectCmd*)data;
				DxDeviceBuffer *bufferExt = DeviceBuffer_ext(DeviceBufferRef_ptr(dispatch.buffer), Dx);
				buffer->lpVtbl->DispatchIndirect(buffer, bufferExt->buffer, dispatch.offset);
			}

			break;

		//case ECommandOp_DispatchRaysIndirect:
		case ECommandOp_DispatchRaysExt: {

			Pipeline *raytracingPipeline = PipelineRef_ptr(temp->tempPipelines[EPipelineType_RaytracingExt]);

			if(temp->pipeline != temp->tempPipelines[EPipelineType_RaytracingExt]) {

				temp->pipeline = temp->tempPipelines[EPipelineType_RaytracingExt];

				buffer->lpVtbl->SetPipelineState1(
					temp->buffer,
					Pipeline_ext(raytracingPipeline, Dx)->stateObject
				);
			}

			PipelineRaytracingInfo info = *Pipeline_info(raytracingPipeline, PipelineRaytracingInfo);

			D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE hit = (D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE) {
				.StartAddress = getDxDeviceAddress((DeviceData) {
					.buffer = info.shaderBindingTable, .offset = info.sbtOffset
				}),
				.SizeInBytes = (U64)raytracingShaderAlignment * info.groupCount,
				.StrideInBytes = raytracingShaderAlignment
			};

			D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE miss = (D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE) {
				.StartAddress = hit.StartAddress + hit.SizeInBytes,
				.SizeInBytes = (U64)raytracingShaderAlignment * info.missCount,
				.StrideInBytes = raytracingShaderAlignment
			};

			D3D12_GPU_VIRTUAL_ADDRESS_RANGE raygen = (D3D12_GPU_VIRTUAL_ADDRESS_RANGE) {
				.StartAddress = miss.StartAddress + miss.SizeInBytes,
				.SizeInBytes = raytracingShaderAlignment
			};

			D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE callable = (D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE) {
				.StartAddress = raygen.StartAddress + raygen.SizeInBytes * info.raygenCount,
				.SizeInBytes = (U64)raytracingShaderAlignment * info.callableCount,
				.StrideInBytes = raytracingShaderAlignment
			};

			if(op == ECommandOp_DispatchRaysExt) {

				DispatchRaysExt dispatch = *(const DispatchRaysExt*)data;
				raygen.StartAddress += raytracingShaderAlignment * dispatch.raygenId;

				D3D12_DISPATCH_RAYS_DESC dispatchRays = (D3D12_DISPATCH_RAYS_DESC) {
					.RayGenerationShaderRecord = raygen,
					.MissShaderTable = miss,
					.HitGroupTable = hit,
					.CallableShaderTable = callable,
					.Width = dispatch.x,
					.Height = dispatch.y,
					.Depth = dispatch.z
				};

				buffer->lpVtbl->DispatchRays(buffer, &dispatchRays);
			}

			else {

				//TODO: Copy to buffer that we already include addresses in, we don't want to expose that level of control.

				DispatchRaysIndirectExt dispatch = *(const DispatchRaysIndirectExt*)data;
				raygen.SizeInBytes += raytracingShaderAlignment * dispatch.raygenId;

				buffer->lpVtbl->DispatchRaysIndirect(
					buffer,
					&raygen, &miss, &hit, &callable,
					DeviceBufferRef_ptr(dispatch.buffer)->resource.deviceAddress + dispatch.offset
				);
			}

			break;
		}

		case ECommandOp_StartScope: {

			D3D12_BARRIER_GROUP dep[2] = { 0 };		//image, buffer

			CommandScope scope = commandList->activeScopes.ptr[temp->scopeCounter];
			++temp->scopeCounter;

			Error err = Error_none();

			for (U64 i = scope.transitionOffset; i < scope.transitionOffset + scope.transitionCount; ++i) {

				TransitionInternal transition = commandList->transitions.ptr[i];

				if(transition.type == ETransitionType_KeepAlive)		//TODO: Residency management
					continue;

				D3D12_BARRIER_SYNC pipelineStage = 0;

				switch (transition.stage) {

					default:																						break;
					case EPipelineStage_Compute:		pipelineStage = D3D12_BARRIER_SYNC_COMPUTE_SHADING;			break;
					case EPipelineStage_Vertex:			pipelineStage = D3D12_BARRIER_SYNC_VERTEX_SHADING;			break;
					case EPipelineStage_Pixel:			pipelineStage = D3D12_BARRIER_SYNC_PIXEL_SHADING;			break;
					case EPipelineStage_GeometryExt:	pipelineStage = D3D12_BARRIER_SYNC_NON_PIXEL_SHADING;		break;

					case EPipelineStage_Hull:			pipelineStage = D3D12_BARRIER_SYNC_NON_PIXEL_SHADING;		break;
					case EPipelineStage_Domain:			pipelineStage = D3D12_BARRIER_SYNC_NON_PIXEL_SHADING;		break;

					case EPipelineStage_RaygenExt:
					case EPipelineStage_CallableExt:
					case EPipelineStage_MissExt:
					case EPipelineStage_ClosestHitExt:
					case EPipelineStage_AnyHitExt:
						pipelineStage = D3D12_BARRIER_SYNC_RAYTRACING;
						break;
				}

				//TLAS, if it's on the CPU we already know the BLASes,
				//If it's on the GPU then we have to rely on manual BLAS transitions

				if (transition.resource->typeId == (ETypeId)EGraphicsTypeId_TLASExt) {

					TLAS *tlas = TLASRef_ptr(transition.resource);

					if (!tlas->base.isCompleted)
						continue;

					if(!tlas->useDeviceMemory)
						for (U64 j = 0; j < tlas->cpuInstancesStatic.length; ++j) {

							TLASInstanceData dat = (TLASInstanceData) { 0 };
							TLAS_getInstanceDataCpu(tlas, j, &dat);

							if (!dat.blasCpu)
								continue;

							BLAS *blas = BLASRef_ptr(dat.blasCpu);

							if (!blas->base.isCompleted)
								continue;

							gotoIfError(nextTransition, DxDeviceBuffer_transition(
								DeviceBuffer_ext(DeviceBufferRef_ptr(blas->base.asBuffer), Dx),
								pipelineStage,
								D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,
								0, 0,
								&deviceExt->bufferTransitions,
								&dep[1]
							))
						}

					gotoIfError(nextTransition, DxDeviceBuffer_transition(
						DeviceBuffer_ext(DeviceBufferRef_ptr(tlas->base.asBuffer), Dx),
						pipelineStage,
						D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,
						0, 0,
						&deviceExt->bufferTransitions,
						&dep[1]
					))

					continue;
				}

				if (transition.resource->typeId == (ETypeId)EGraphicsTypeId_BLASExt) {

					BLAS *blas = BLASRef_ptr(transition.resource);

					if (!blas->base.isCompleted)
						continue;

					gotoIfError(nextTransition, DxDeviceBuffer_transition(
						DeviceBuffer_ext(DeviceBufferRef_ptr(blas->base.asBuffer), Dx),
						pipelineStage,
						D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,
						0, 0,
						&deviceExt->bufferTransitions,
						&dep[1]
					))

					continue;
				}

				//Grab transition type

				Bool isImage = TextureRef_isTexture(transition.resource);
				Bool isDepthStencil = TextureRef_isDepthStencil(transition.resource);
				Bool isShaderRead = transition.type == ETransitionType_ShaderRead;

				D3D12_BARRIER_LAYOUT layout = D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
				D3D12_BARRIER_ACCESS access = 0;

				access = isShaderRead ? D3D12_BARRIER_ACCESS_SHADER_RESOURCE : D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;

				if(isImage)
					layout = isShaderRead ? D3D12_BARRIER_LAYOUT_SHADER_RESOURCE : D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;

				if(!pipelineStage)
					switch ((ETransitionType) transition.type) {

						case ETransitionType_RenderTargetRead:

							pipelineStage = 
								isDepthStencil ? D3D12_BARRIER_SYNC_DEPTH_STENCIL : D3D12_BARRIER_SYNC_PIXEL_SHADING;

							access =
								isDepthStencil ? D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ : D3D12_BARRIER_ACCESS_RENDER_TARGET;

							layout =
								isDepthStencil ? D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ : D3D12_BARRIER_LAYOUT_RENDER_TARGET;

							break;

						case ETransitionType_RenderTargetWrite:

							pipelineStage = 
								isDepthStencil ? D3D12_BARRIER_SYNC_DEPTH_STENCIL : D3D12_BARRIER_SYNC_PIXEL_SHADING;

							access =
								isDepthStencil ? D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE : D3D12_BARRIER_ACCESS_RENDER_TARGET;

							layout =
								isDepthStencil ? D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE : D3D12_BARRIER_LAYOUT_RENDER_TARGET;

							break;

						case ETransitionType_Clear:
							pipelineStage = D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW;
							access = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
							layout = D3D12_BARRIER_SYNC_RENDER_TARGET;
							break;

						case ETransitionType_CopyRead:
							pipelineStage =  D3D12_BARRIER_SYNC_COPY;
							access = D3D12_BARRIER_ACCESS_COPY_SOURCE;
							layout = D3D12_BARRIER_LAYOUT_COMMON;
							break;

						case ETransitionType_CopyWrite:
							pipelineStage =  D3D12_BARRIER_SYNC_COPY;
							access = D3D12_BARRIER_ACCESS_COPY_DEST;
							layout = D3D12_BARRIER_LAYOUT_COMMON;
							break;

						case ETransitionType_Indirect:
							pipelineStage = D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
							access = D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
							break;

						case ETransitionType_Index:
							pipelineStage = D3D12_BARRIER_SYNC_INDEX_INPUT;
							access = D3D12_BARRIER_ACCESS_INDEX_BUFFER;
							break;

						case ETransitionType_Vertex:
							pipelineStage = D3D12_BARRIER_SYNC_INDEX_INPUT;
							access = D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
							break;

						default:
							break;
					}

				//Transition resource

				if(isImage) {

					UnifiedTexture unif = TextureRef_getUnifiedTexture(transition.resource, NULL);
					DxUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(transition.resource, Dx, 0);

					D3D12_BARRIER_SUBRESOURCE_RANGE range = (D3D12_BARRIER_SUBRESOURCE_RANGE) {		//TODO:
						.NumMipLevels = 1,
						.NumArraySlices = 1,
						.NumPlanes = 1
					};

					if(isDepthStencil && unif.depthFormat >= EDepthStencilFormat_StencilStart)
						++range.NumPlanes;

					gotoIfError(nextTransition, DxUnifiedTexture_transition(

						imageExt,
						pipelineStage,
						access,
						layout,
						&range,

						&deviceExt->imageTransitions,
						&dep[0]
					))
				}

				else {

					DeviceBuffer *devBuffer = DeviceBufferRef_ptr(transition.resource);

					gotoIfError(nextTransition, DxDeviceBuffer_transition(
						DeviceBuffer_ext(devBuffer, Dx),
						pipelineStage,
						access,
						0,						//TODO: range
						devBuffer->resource.size,
						&deviceExt->bufferTransitions,
						&dep[1]
					))
				}

			nextTransition:

				if(err.genericError)
					Error_printx(err, ELogLevel_Error, ELogOptions_Default);
			}

			if(dep[0].NumBarriers || dep[1].NumBarriers)
				buffer->lpVtbl->Barrier(
					buffer, 
					dep[0].NumBarriers || dep[1].NumBarriers,
					dep[0].NumBarriers ? &dep[0] : dep[1]
				);

			ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions);
			ListD3D12_TEXTURE_BARRIER_clear(&deviceExt->imageTransitions);
			break;
		}

		//Debug markers
		//We're not going to include and port pix to C for this...

		case ECommandOp_EndRegionDebugExt:
			graphicsCmd->lpVtbl->EndEvent(graphicsCmd);
			break;

		case ECommandOp_AddMarkerDebugExt:
		case ECommandOp_StartRegionDebugExt: {

			const F32x4 colf = F32x4_round(F32x4_mul(F32x4_saturate(F32x4_load4((F32*)data)), F32x4_xxxx4(255)));
			const I32x4 v = I32x4_mul(I32x4_fromF32x4(colf), I32x4_create4(1 << 16, 1 << 8, 1, 0));

			const I32x2 reduc2 = I32x2_or(I32x4_xy(v), I32x4_zw(v));
			const I32 reduc = I32x2_x(reduc2) | I32x2_y(reduc2);

			U64 encoded[64];
			encoded[0] = (op == ECommandOp_AddMarkerDebugExt ? 0x017 : 0x001) << 10;
			encoded[1] = 0xFF000000 | (*(const U32*)&reduc);

			const U32 strLen = (U32) CharString_calcStrLen((const C8*)data + sizeof(I32x4), sizeof(encoded) - 16);
			const U32 len = U32_min((U32) sizeof(encoded), 16 + strLen);

			Buffer_copy(
				Buffer_createRef((C8*)encoded + 16, sizeof(encoded) - 16),
				Buffer_createRefConst((const C8*)data + sizeof(I32x4), strLen)
			);

			if(op == ECommandOp_AddMarkerDebugExt)
				graphicsCmd->lpVtbl->SetMarker(graphicsCmd, 2, encoded, len);

			else graphicsCmd->lpVtbl->BeginEvent(graphicsCmd, 2, encoded, len);

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
