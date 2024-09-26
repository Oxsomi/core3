/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

impl Error BLASRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, BLASRef *pending);
impl Error TLASRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, TLASRef *pending);

//RTVs and DSVs are temporary in DirectX.

D3D12_CPU_DESCRIPTOR_HANDLE createTempRTV(
	const DxGraphicsDevice *deviceExt,
	const U64 relativeLoc,
	const D3D12_CPU_DESCRIPTOR_HANDLE start,
	const DxHeap heap,
	RefPtr *image
) {

	ID3D12Resource *resource = NULL;
	D3D12_RENDER_TARGET_VIEW_DESC rtv;

	if(!image)
		rtv = (D3D12_RENDER_TARGET_VIEW_DESC) {
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D
		};

	else {

		const UnifiedTexture tex = TextureRef_getUnifiedTexture(image, NULL);
		const DxUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(image, Dx, 0);

		resource = imageExt->image;

		rtv = (D3D12_RENDER_TARGET_VIEW_DESC) {
			.Format = ETextureFormatId_toDXFormat(tex.textureFormatId)
		};

		switch(tex.type) {

			default:
				rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				rtv.Texture2D = (D3D12_TEX2D_RTV) { 0 };						//No mip and plane slice
				break;

			case ETextureType_Cube:
				rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				rtv.Texture2DArray = (D3D12_TEX2D_ARRAY_RTV) { .ArraySize = 6 };		//No mip, array off and plane slice
				break;

			case ETextureType_3D:
				rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
				rtv.Texture3D = (D3D12_TEX3D_RTV) { .WSize = tex.length };				//No mip and array offset
				break;
		}
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE location = (D3D12_CPU_DESCRIPTOR_HANDLE) {
		.ptr = start.ptr + relativeLoc * heap.cpuIncrement
	};

	deviceExt->device->lpVtbl->CreateRenderTargetView(deviceExt->device, resource, &rtv, location);
	return location;
}

D3D12_CPU_DESCRIPTOR_HANDLE createTempDSV(
	const DxGraphicsDevice *deviceExt,
	const U64 relativeLoc,
	const D3D12_CPU_DESCRIPTOR_HANDLE start,
	EStartRenderFlags flags,
	const DxHeap heap,
	RefPtr *image
) {

	ID3D12Resource *resource = NULL;
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv;

	if(!image)
		dsv = (D3D12_DEPTH_STENCIL_VIEW_DESC) {
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D
		};

	else {

		const UnifiedTexture tex = TextureRef_getUnifiedTexture(image, NULL);
		const DxUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(image, Dx, 0);

		resource = imageExt->image;

		dsv = (D3D12_DEPTH_STENCIL_VIEW_DESC) {
			.Format = EDepthStencilFormat_toDXFormat(tex.depthFormat),
			.Flags =
				(flags & EStartRenderFlags_DepthReadOnly ? D3D12_DSV_FLAG_READ_ONLY_DEPTH : 0) |
				(flags & EStartRenderFlags_StencilReadOnly ? D3D12_DSV_FLAG_READ_ONLY_STENCIL : 0)
		};

		switch(tex.type) {

			default:
				dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				dsv.Texture2D = (D3D12_TEX2D_DSV) { 0 };							//No mip
				break;

			case ETextureType_Cube:
				dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				dsv.Texture2DArray = (D3D12_TEX2D_ARRAY_DSV) { .ArraySize = 6 };			//No mip or array off
				break;

			case ETextureType_3D:
				dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				dsv.Texture2DArray = (D3D12_TEX2D_ARRAY_DSV) { .ArraySize = tex.length };	//No mip and array offset
				break;
		}
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE location = (D3D12_CPU_DESCRIPTOR_HANDLE) {
		.ptr = start.ptr + relativeLoc * heap.cpuIncrement
	};

	deviceExt->device->lpVtbl->CreateDepthStencilView(deviceExt->device, resource, &dsv, location);
	return location;
}

void CommandList_process(
	CommandList *commandList,
	GraphicsDeviceRef *deviceRef,
	ECommandOp op,
	const U8 *data,
	void *commandListExt
) {

	(void) commandList;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	(void) deviceExt;

	DxCommandBufferState *temp = (DxCommandBufferState*) commandListExt;
	DxCommandBuffer *buffer = temp->buffer;

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

			//Prepare attachments

			DxHeap heap = deviceExt->heaps[EDescriptorHeapType_RTV];

			D3D12_CPU_DESCRIPTOR_HANDLE cpuDesc = (D3D12_CPU_DESCRIPTOR_HANDLE) { .ptr = heap.cpuHandle.ptr };

			for (U8 i = 0; i < imageClearCount; ++i) {

				ClearImageCmd image = ((const ClearImageCmd*) (data + sizeof(U32)))[i];
				RefPtr *active = image.image;

				//Reuse the same descriptor, it's useless.

				D3D12_CPU_DESCRIPTOR_HANDLE curr = createTempRTV(deviceExt, 0, cpuDesc, heap, active);
				buffer->lpVtbl->ClearRenderTargetView(buffer, curr, image.color.colorf, 0, NULL);
			}

			break;
		}

		case ECommandOp_CopyImage: {

			CopyImageCmd copyImage = *(const CopyImageCmd*) data;
			const CopyImageRegion *copyImageRegions = (const CopyImageRegion*) (data + sizeof(copyImage));

			UnifiedTexture src = TextureRef_getUnifiedTexture(copyImage.src, NULL);
			U8 planes = 1;
			U8 planeOffset = 0;

			if(src.depthFormat) {

				if(copyImage.copyType == ECopyType_DepthOnly)
					planeOffset = 0;

				else if(copyImage.copyType == ECopyType_StencilOnly)
					planeOffset = 1;

				else planes = 2;
			}

			DxUnifiedTexture *srcExt = TextureRef_getCurrImgExtT(copyImage.src, Dx, 0);
			DxUnifiedTexture *dstExt = TextureRef_getCurrImgExtT(copyImage.dst, Dx, 0);

			for(U64 i = 0; i < copyImage.regionCount; ++i) {

				CopyImageRegion image = copyImageRegions[i];

				if(!image.width)
					image.width = src.width - image.srcX;

				if(!image.height)
					image.height = src.height - image.srcY;

				if(!image.length)
					image.length = src.length - image.srcZ;

				D3D12_BOX srcBox = (D3D12_BOX) {
					.left   = image.srcX,
					.top    = image.srcY,
					.front  = image.srcZ,
					.right  = image.srcX + image.width,
					.bottom	= image.srcY + image.height,
					.back	= image.srcZ + image.length
				};

				for(U8 j = planeOffset; j < planeOffset + planes; ++j) {

					D3D12_TEXTURE_COPY_LOCATION srcLocation = (D3D12_TEXTURE_COPY_LOCATION) {
						.pResource = srcExt->image,
						.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
						.SubresourceIndex = j
					};

					D3D12_TEXTURE_COPY_LOCATION dstLocation = (D3D12_TEXTURE_COPY_LOCATION) {
						.pResource = dstExt->image,
						.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
						.SubresourceIndex = j
					};

					buffer->lpVtbl->CopyTextureRegion(
						buffer,
						&dstLocation,
						image.dstX,
						image.dstY,
						image.dstZ,
						&srcLocation,
						&srcBox
					);
				}
			}

			break;
		}

		//Dynamic rendering / direct rendering

		case ECommandOp_StartRenderingExt: {

			const StartRenderCmdExt *startRender = (const StartRenderCmdExt*) data;
			const AttachmentInfoInternal *attachments = (const AttachmentInfoInternal*) (startRender + 1);

			//Prepare attachments

			DxHeap heap = deviceExt->heaps[EDescriptorHeapType_RTV];

			D3D12_CPU_DESCRIPTOR_HANDLE cpuDesc = heap.cpuHandle;
			U8 j = 0;

			D3D12_RECT rect = (D3D12_RECT) {
				.left = I32x2_x(startRender->offset),
				.top = I32x2_y(startRender->offset),
				.right = I32x2_x(startRender->offset) + I32x2_x(startRender->size),
				.bottom = I32x2_y(startRender->offset) + I32x2_y(startRender->size)
			};

			for (U8 i = 0; i < startRender->colorCount; ++i) {

				const AttachmentInfoInternal *attachmentsj = &attachments[j];
				RefPtr *active = NULL;

				if((startRender->activeMask >> i) & 1) {

					active = attachmentsj->image;

					if(attachmentsj->resolveImage) {
						temp->boundTargets[i] = (ImageAndRange) { .range = attachmentsj->range, .image = active };
						temp->resolveTargets[i] = (ImageAndRange) {
							.range = attachmentsj->resolveRange,
							.image = attachmentsj->resolveImage
						};
					}
				}

				D3D12_CPU_DESCRIPTOR_HANDLE curr = createTempRTV(deviceExt, i, cpuDesc, heap, active);

				if(((startRender->clearMask & startRender->activeMask) >> i) & 1)
					buffer->lpVtbl->ClearRenderTargetView(buffer, curr, attachmentsj->color.colorf, 1, &rect);

				if(active)
					++j;
			}

			for (U8 i = startRender->colorCount; i < 9; ++i)
				temp->boundTargets[i] = temp->resolveTargets[i] = (ImageAndRange) { 0 };

			DxHeap dsvHeap = deviceExt->heaps[EDescriptorHeapType_DSV];
			D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuDesc = dsvHeap.cpuHandle;
			D3D12_CPU_DESCRIPTOR_HANDLE dsv = createTempDSV(
				deviceExt, 0, dsvCpuDesc, startRender->flags, dsvHeap, startRender->depthStencil
			);

			if(startRender->flags & EStartRenderFlags_ClearDepthStencil)
				buffer->lpVtbl->ClearDepthStencilView(
					buffer, dsv,
					(startRender->flags & EStartRenderFlags_ClearDepth ? D3D12_CLEAR_FLAG_DEPTH : 0) |
					(startRender->flags & EStartRenderFlags_ClearStencil ? D3D12_CLEAR_FLAG_STENCIL : 0),
					startRender->clearDepth, startRender->clearStencil,
					1, &rect
				);

			if(startRender->resolveDepthStencil) {

				temp->boundTargets[8] = (ImageAndRange) {
					.range = startRender->depthStencilRange,
					.image = startRender->depthStencil
				};

				temp->resolveTargets[8] = (ImageAndRange) {
					.range = startRender->resolveDepthStencilRange,
					.image = startRender->resolveDepthStencil
				};
			}

			buffer->lpVtbl->OMSetRenderTargets(
				buffer, j,
				&cpuDesc, true,
				&dsv
			);

			temp->inRender = true;
			break;
		}

		//No-Op, except for MSAA resolving

		case ECommandOp_EndRenderingExt: {

			D3D12_BARRIER_GROUP deps = { .Type = D3D12_BARRIER_TYPE_TEXTURE };

			//All DSVs and RTVs to the correct state

			for(U8 i = 0; i < 9; ++i) {

				ImageAndRange input = temp->boundTargets[i];

				if(!input.image)		//Only there if it's really required
					continue;

				const UnifiedTexture utex = TextureRef_getUnifiedTexture(input.image, NULL);
				DxUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(input.image, Dx, 0);

				D3D12_BARRIER_SUBRESOURCE_RANGE range = (D3D12_BARRIER_SUBRESOURCE_RANGE) {
					.NumMipLevels = 1,
					.NumArraySlices = 1,
					.NumPlanes = 1
				};

				if (i == 8) {		//Depth stencil

					if(utex.depthFormat != EDepthStencilFormat_S8Ext)				//Take both planes (depth & stencil)
						++range.NumPlanes;

					else if(utex.depthFormat >= EDepthStencilFormat_StencilStart)	//Take only the stencil plane
						++range.FirstPlane;
				}

				//Only transition source, because dest was already transitioned in startScope

				Error err = DxUnifiedTexture_transition(
					imageExt,
					D3D12_BARRIER_SYNC_RESOLVE,
					D3D12_BARRIER_ACCESS_RESOLVE_SOURCE,
					D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE,
					&range,
					&deviceExt->imageTransitions,
					&deps
				);

				Error_printx(err, ELogLevel_Error, ELogOptions_Default);
			}

			if(deps.NumBarriers)
				buffer->lpVtbl->Barrier(buffer, 1, &deps);

			ListD3D12_TEXTURE_BARRIER_clear(&deviceExt->imageTransitions);

			if(deps.NumBarriers)
				for(U8 i = 0; i < 9; ++i) {

					ImageAndRange input = temp->boundTargets[i];
					ImageAndRange output = temp->resolveTargets[i];

					if(!input.image)		//Only there if it's really required
						continue;

					const DxUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(input.image, Dx, 0);
					const DxUnifiedTexture *resolveExt = TextureRef_getCurrImgExtT(output.image, Dx, 0);
					const UnifiedTexture utex = TextureRef_getUnifiedTexture(input.image, NULL);

					DXGI_FORMAT format = 0;

					if(utex.depthFormat)
						format = EDepthStencilFormat_toDXFormat(utex.depthFormat);

					else format = ETextureFormatId_toDXFormat(utex.textureFormatId);

					buffer->lpVtbl->ResolveSubresource(
						buffer,
						imageExt->image, 0,
						resolveExt->image, 0,
						format
					);
				}

			temp->inRender = false;
			break;
		}

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

			PipelineGraphicsInfo *graphicsShader = Pipeline_info(PipelineRef_ptr(temp->pipeline), PipelineGraphicsInfo);
			D3D12_PRIMITIVE_TOPOLOGY topology = 0;

			if(graphicsShader->patchControlPoints)
				topology = D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + graphicsShader->patchControlPoints - 1;

			else switch(graphicsShader->topologyMode) {

				default:								topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;			break;
				case ETopologyMode_TriangleStrip:		topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;		break;

				case ETopologyMode_LineList:			topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;				break;
				case ETopologyMode_LineStrip:			topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;			break;

				case ETopologyMode_PointList:			topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;			break;

				case ETopologyMode_TriangleListAdj:		topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;		break;
				case ETopologyMode_TriangleStripAdj:	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;	break;

				case ETopologyMode_LineListAdj:			topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;			break;
				case ETopologyMode_LineStripAdj:		topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;		break;
			}

			if (temp->boundPrimitiveTopology != topology) {
				buffer->lpVtbl->IASetPrimitiveTopology(buffer, topology);
				temp->boundPrimitiveTopology = (U8) topology;
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
					.SizeInBytes = (U32) indexBuffer->resource.size,
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

					DeviceBuffer *buf = DeviceBufferRef_ptr(bufferRef);

					vertexBuffers[i] = (D3D12_VERTEX_BUFFER_VIEW) {
						.BufferLocation = getDxDeviceAddress((DeviceData) { .buffer = bufferRef }),
						.SizeInBytes = (U32) buf->resource.size,
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
						draw.indexOffset, (I32)draw.vertexOffset,
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

					EExecuteIndirectCommand cmd =
						drawIndirect.isIndexed ? EExecuteIndirectCommand_DrawIndexed : EExecuteIndirectCommand_Draw;

					buffer->lpVtbl->ExecuteIndirect(
						buffer,
						deviceExt->commandSigs[cmd],
						drawIndirect.drawCalls,
						bufferExt->buffer, drawIndirect.bufferOffset,
						counterExt->buffer, drawIndirect.countOffsetExt
					);
				}

				//Indirect draw (non count)

				else {

					EExecuteIndirectCommand cmd =
						drawIndirect.isIndexed ? EExecuteIndirectCommand_DrawIndexed : EExecuteIndirectCommand_Draw;

					buffer->lpVtbl->ExecuteIndirect(
						buffer,
						deviceExt->commandSigs[cmd],
						drawIndirect.drawCalls,
						bufferExt->buffer, drawIndirect.bufferOffset,
						NULL, 0
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

				ID3D12CommandSignature *dispatchIndirect = deviceExt->commandSigs[EExecuteIndirectCommand_Dispatch];

				buffer->lpVtbl->ExecuteIndirect(
					buffer,
					dispatchIndirect,
					1,
					bufferExt->buffer, dispatch.offset,
					NULL, 0
				);
			}

			break;

		//JIT RTAS updates in case they are on the GPU (e.g. compute updates)

		case ECommandOp_UpdateBLASExt:
			BLASRef_flush(temp, deviceRef, *(BLASRef**)data);
			break;

		case ECommandOp_UpdateTLASExt:
			TLASRef_flush(temp, deviceRef, *(TLASRef**)data);
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
				.StartAddress = getDxDeviceAddress((DeviceData) { .buffer = info.shaderBindingTable }),
				.SizeInBytes = (U64)raytracingShaderAlignment * info.groups.length,
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

			if(!info.groups.length)
				hit = (D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE) { 0 };

			if(!info.missCount)
				miss = (D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE) { 0 };

			if(!info.callableCount)
				callable = (D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE) { 0 };

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

			/*else {

				DispatchRaysIndirectExt dispatch = *(const DispatchRaysIndirectExt*)data;
				raygen.SizeInBytes += raytracingShaderAlignment * dispatch.raygenId;

				DxDeviceBuffer *bufferExt = DeviceBuffer_ext(DeviceBufferRef_ptr(dispatch.buffer), Dx);

				ID3D12CommandSignature *dispatchIndirect = deviceExt->commandSigs[EExecuteIndirectCommand_DispatchRays];

				//TODO: Copy to intermediate buffer bufferExt->buffer, because we don't allow dynamic SBT
				//https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#d3d12_dispatch_rays_desc
				//&raygen, &miss, &hit, &callable,

				buffer->lpVtbl->ExecuteIndirect(
					buffer,
					dispatchIndirect,
					1,
					bufferExt->buffer, dispatch.offset,
					NULL, 0
				);
			}*/

			break;
		}

		case ECommandOp_StartScope: {

			D3D12_BARRIER_GROUP dep[2] = {
				(D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_TEXTURE },
				(D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_BUFFER }
			};

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

					case EPipelineStage_GeometryExt:
					case EPipelineStage_Domain:
					case EPipelineStage_Hull:
						pipelineStage = D3D12_BARRIER_SYNC_NON_PIXEL_SHADING;
						break;

					case EPipelineStage_RaygenExt:
					case EPipelineStage_CallableExt:
					case EPipelineStage_MissExt:
					case EPipelineStage_ClosestHitExt:
					case EPipelineStage_AnyHitExt:
						pipelineStage = D3D12_BARRIER_SYNC_RAYTRACING;
						break;

					case EPipelineStage_RTASBuild:
						pipelineStage = D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;
						break;
				}

				//If it's on the GPU then we have to rely on manual RTAS transitions

				Bool isTLAS = transition.resource->typeId == (ETypeId)EGraphicsTypeId_TLASExt;

				if (isTLAS || transition.resource->typeId == (ETypeId)EGraphicsTypeId_BLASExt) {

					RTAS rtas = isTLAS ? TLASRef_ptr(transition.resource)->base : BLASRef_ptr(transition.resource)->base;

					gotoIfError(nextTransition, DxDeviceBuffer_transition(

						DeviceBuffer_ext(DeviceBufferRef_ptr(rtas.asBuffer), Dx),

						pipelineStage,

						transition.type == ETransitionType_ShaderWrite ?
							D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE | 
							D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ :
							D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,

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
								isDepthStencil ? D3D12_BARRIER_SYNC_DEPTH_STENCIL : D3D12_BARRIER_SYNC_RENDER_TARGET;

							access =
								isDepthStencil ? D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ : D3D12_BARRIER_ACCESS_RENDER_TARGET;

							layout =
								isDepthStencil ? D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ : D3D12_BARRIER_LAYOUT_RENDER_TARGET;

							break;

						case ETransitionType_RenderTargetWrite:

							pipelineStage =
								isDepthStencil ? D3D12_BARRIER_SYNC_DEPTH_STENCIL : D3D12_BARRIER_SYNC_RENDER_TARGET;

							access =
								isDepthStencil ? D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE : D3D12_BARRIER_ACCESS_RENDER_TARGET;

							layout =
								isDepthStencil ? D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE : D3D12_BARRIER_LAYOUT_RENDER_TARGET;

							break;

						case ETransitionType_ResolveTargetWrite:
							pipelineStage = D3D12_BARRIER_SYNC_RESOLVE;
							access = D3D12_BARRIER_ACCESS_RESOLVE_DEST;
							layout = D3D12_BARRIER_LAYOUT_RESOLVE_DEST;
							break;

						case ETransitionType_Clear:
							pipelineStage = D3D12_BARRIER_SYNC_RENDER_TARGET;
							access = D3D12_BARRIER_ACCESS_RENDER_TARGET;
							layout = D3D12_BARRIER_LAYOUT_RENDER_TARGET;
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
							pipelineStage = D3D12_BARRIER_SYNC_VERTEX_SHADING;
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
					!!dep[0].NumBarriers + !!dep[1].NumBarriers,
					dep[0].NumBarriers ? &dep[0] : &dep[1]
				);

			ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions);
			ListD3D12_TEXTURE_BARRIER_clear(&deviceExt->imageTransitions);
			break;
		}

		//Debug markers
		//We're not going to include and port pix to C for this...

		case ECommandOp_EndRegionDebugExt:
			buffer->lpVtbl->EndEvent(buffer);
			break;

		case ECommandOp_AddMarkerDebugExt:
		case ECommandOp_StartRegionDebugExt: {

			const F32x4 colf = F32x4_round(F32x4_mul(F32x4_saturate(F32x4_load4((F32*)data)), F32x4_xxxx4(255)));
			const I32x4 v = I32x4_mul(I32x4_fromF32x4(colf), I32x4_create4(1 << 16, 1 << 8, 1, 0));

			const I32x2 reduc2 = I32x2_or(I32x4_xy(v), I32x4_zw(v));
			const I32 reduc = I32x2_x(reduc2) | I32x2_y(reduc2);

			U64 encoded[62] = { 0 };
			encoded[0] = (op == ECommandOp_AddMarkerDebugExt ? 0x018 : 0x002) << 10;
			encoded[1] = 0xFF000000 | (*(const U32*)&reduc);

			const U32 strLen = (U32) CharString_calcStrLen((const C8*)data + sizeof(I32x4), sizeof(encoded) - 16);
			const U32 len = U32_min((U32) sizeof(encoded), 16 + strLen);

			Buffer_copy(
				Buffer_createRef((C8*)encoded + 16, sizeof(encoded) - 16),
				Buffer_createRefConst((const C8*)data + sizeof(I32x4), strLen)
			);

			if(op == ECommandOp_AddMarkerDebugExt)
				buffer->lpVtbl->SetMarker(buffer, 2, encoded, len);

			else buffer->lpVtbl->BeginEvent(buffer, 2, encoded, len);

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
