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

//graphics/d3d12/generic/dx_command_list.c

#include "graphics/d3d12/dx_device.h"
#include "graphics/d3d12/dx_buffer.h"
#include "graphics/generic/interface.h"
#include "graphics/d3d12/dx_interface.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/opacity_micromap.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/blas.h"
#include "types/container/buffer.h"
#include "types/math/vec4i_swizzle.h"
#include "types/container/log.h"
#include "platforms/logx.h"
#include "types/container/buffer.h"
#include "types/base/error.h"
#include "types/base/mathi.h"

//RTVs and DSVs are temporary in DirectX.

D3D12_CPU_DESCRIPTOR_HANDLE createTempRTV(
	const DxGraphicsDevice *deviceExt,
	const U64 relativeLoc,
	const D3D12_CPU_DESCRIPTOR_HANDLE start,
	const DxDescriptorHeapSingle *heap,
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

				if (tex.sampleCount) {
					rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
					rtv.Texture2DMS = (D3D12_TEX2DMS_RTV) { 0 };
					break;
				}

				rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				rtv.Texture2D = (D3D12_TEX2D_RTV) { 0 };                                //No mip and plane slice
				break;

			case ETextureType_Cube:
				rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				rtv.Texture2DArray = (D3D12_TEX2D_ARRAY_RTV) { .ArraySize = 6 };        //No mip, array off and plane slice
				break;

			case ETextureType_3D:
				rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
				rtv.Texture3D = (D3D12_TEX3D_RTV) { .WSize = tex.length };                //No mip and array offset
				break;
		}
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE location = (D3D12_CPU_DESCRIPTOR_HANDLE) {
		.ptr = start.ptr + relativeLoc * heap->cpuIncrement
	};

	deviceExt->device->lpVtbl->CreateRenderTargetView(deviceExt->device, resource, &rtv, location);
	return location;
}

D3D12_CPU_DESCRIPTOR_HANDLE createTempDSV(
	const DxGraphicsDevice *deviceExt,
	const U64 relativeLoc,
	const D3D12_CPU_DESCRIPTOR_HANDLE start,
	EStartRenderFlags flags,
	const DxDescriptorHeapSingle *heap,
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

				if (tex.sampleCount) {
					dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
					dsv.Texture2DMS = (D3D12_TEX2DMS_DSV) { 0 };
					break;
				}

				dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				dsv.Texture2D = (D3D12_TEX2D_DSV) { 0 };                            //No mip
				break;

			case ETextureType_Cube:
				dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				dsv.Texture2DArray = (D3D12_TEX2D_ARRAY_DSV) { .ArraySize = 6 };            //No mip or array off
				break;

			case ETextureType_3D:
				dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				dsv.Texture2DArray = (D3D12_TEX2D_ARRAY_DSV) { .ArraySize = tex.length };    //No mip and array offset
				break;
		}
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE location = (D3D12_CPU_DESCRIPTOR_HANDLE) {
		.ptr = start.ptr + relativeLoc * heap->cpuIncrement
	};

	deviceExt->device->lpVtbl->CreateDepthStencilView(deviceExt->device, resource, &dsv, location);
	return location;
}

void GraphicsDevice_rebindDescriptors(GraphicsDevice *device, DxCommandBuffer *commandBuffer);

//Bindful: descriptors are emitted lazily at the work op, where the pipeline (and so the root
// signature) is known; binds only set state.
//A custom layout switches the root signature, which drops EVERY root argument, so the default layout state
// is marked dirty and a later default layout work op re-runs the frame rebind.

static void DxCommandBufferState_bindDescriptors(
	DxCommandBufferState *temp,
	GraphicsDevice *device,
	PipelineRef *pipelineRef,
	Bool isCompute
) {

	PipelineLayoutRef *layoutRef = PipelineRef_ptr(pipelineRef)->layout;
	DxCommandBuffer *buffer = temp->buffer;

	if (layoutRef == device->defaultPipelineLayout) {

		if (!temp->defaultDescriptorsBound) {

			temp->defaultDescriptorsBound = true;
			GraphicsDevice_rebindDescriptors(device, buffer);

			//The rebind switched the heap and root signatures back to the defaults, so the custom path's
			// trackers no longer describe what is bound.

			temp->lastBoundHeap = NULL;
			temp->lastBoundTable[0] = temp->lastBoundTable[1] = NULL;
			temp->lastRootSig[0] = temp->lastRootSig[1] = NULL;
		}

		return;
	}

	const PipelineLayout *layout = PipelineLayoutRef_ptr(layoutRef);
	DxPipelineLayout *layoutExt = PipelineLayout_ext((PipelineLayout*)layout, Dx);

	const U8 bindPoint = isCompute ? 1 : 0;

	//Tracked separately like the other command buffer state: the root signature only re-emits when it truly
	// changed, because setting one (even the same one) drops every root argument on D3D12.

	if (temp->lastRootSig[bindPoint] != layoutExt->rootSig) {

		if(isCompute)
			buffer->lpVtbl->SetComputeRootSignature(buffer, layoutExt->rootSig);

		else buffer->lpVtbl->SetGraphicsRootSignature(buffer, layoutExt->rootSig);

		temp->lastRootSig[bindPoint] = layoutExt->rootSig;
		temp->lastBoundTable[bindPoint] = NULL;        //The switch dropped every root argument
		temp->pushConstantsEmitted[bindPoint] = false;
		temp->pushDescriptorsEmitted[bindPoint] = false;
		temp->defaultDescriptorsBound = false;

		//A custom layout whose bindings are the device's own bindless set gets the device's heap and table
		// bound against ITS root signature; the root params come from the same DescriptorLayout, so they sit
		// at the same indices the default root signature puts them at.

		if (PipelineLayout_usesRuntimeBindless(layout)) {

			DxDescriptorHeap *bindlessHeap =
				DescriptorHeap_ext(DescriptorHeapRef_ptr(device->defaultDescriptorHeaps), Dx);

			DxDescriptorTable *bindlessTable =
				DescriptorTable_ext(DescriptorTableRef_ptr(device->defaultDescriptorTable), Dx);

			ID3D12DescriptorHeap *heaps[2] = { bindlessHeap->resourcesHeap.heap, bindlessHeap->samplerHeap.heap };

			//Sampler descriptors are not the same size as CBV/SRV/UAV ones on every adapter, so each offset
			// scales by its own heap's increment.

			const D3D12_GPU_DESCRIPTOR_HANDLE tables[2] = {
				{
					bindlessHeap->samplerHeap.gpuHandle.ptr +
					bindlessTable->allocationLocations[1] * bindlessHeap->samplerHeap.gpuIncrement
				},
				{
					bindlessHeap->resourcesHeap.gpuHandle.ptr +
					bindlessTable->allocationLocations[0] * bindlessHeap->resourcesHeap.gpuIncrement
				}
			};

			if(temp->lastBoundHeap != device->defaultDescriptorHeaps) {
				buffer->lpVtbl->SetDescriptorHeaps(buffer, 2, heaps);
				temp->lastBoundHeap = device->defaultDescriptorHeaps;
			}

			for(U32 i = 0; i < 2; ++i) {

				if(isCompute)
					buffer->lpVtbl->SetComputeRootDescriptorTable(buffer, i, tables[i]);

				else buffer->lpVtbl->SetGraphicsRootDescriptorTable(buffer, i, tables[i]);
			}
		}

		//A custom layout that declares OxC3's per frame globals gets them bound here, since the default
		// layout's own bind never runs for it and the root signature switch just dropped every argument.

		if (PipelineLayout_hasRuntimeGlobals(layout)) {

			const DeviceBuffer *frameData = DeviceBufferRef_ptr(device->frameData[device->fifId]);
			const D3D12_GPU_VIRTUAL_ADDRESS cbvLoc = frameData->resource.deviceAddress;

			if(isCompute)
				buffer->lpVtbl->SetComputeRootConstantBufferView(
					buffer, layoutExt->rootParamPushDescriptors, cbvLoc
				);

			else buffer->lpVtbl->SetGraphicsRootConstantBufferView(
				buffer, layoutExt->rootParamPushDescriptors, cbvLoc
			);
		}
	}

	//Emitted before the table work below, which returns early for a layout that has push constants but no
	// bindings at all; the root signature above is current either way by this point.

	if (temp->pushConstantSize && !temp->pushConstantsEmitted[bindPoint] && layout->info.pushConstants.count) {

		const U32 num32Bit = temp->pushConstantSize >> 2;

		if(isCompute)
			buffer->lpVtbl->SetComputeRoot32BitConstants(
				buffer, layoutExt->rootParamPushConstants, num32Bit, temp->pushConstantData, 0
			);

		else buffer->lpVtbl->SetGraphicsRoot32BitConstants(
			buffer, layoutExt->rootParamPushConstants, num32Bit, temp->pushConstantData, 0
		);

		temp->pushConstantsEmitted[bindPoint] = true;
	}

	//Push descriptors are root descriptors: one raw GPU virtual address per binding. That is also why only
	//buffer class resources can be one, since a texture's format, mip and swizzle have nowhere to live in an
	//address; createDescriptorLayout refuses the rest up front.
	//Emitted here for the same reason as the constants: the table work below returns early for a layout that
	//has no ordinary bindings at all.

	if (temp->pushDescriptorCount && !temp->pushDescriptorsEmitted[bindPoint] && layout->info.pushDescriptors) {

		const DescriptorLayout *pushLayout = DescriptorLayoutRef_ptr(layout->info.pushDescriptors);
		DxDescriptorLayout *pushExt = DescriptorLayout_ext((DescriptorLayout*)pushLayout, Dx);

		for (U8 i = 0; i < temp->pushDescriptorCount && i < pushLayout->info.bindings.length; ++i) {

			const Descriptor d = temp->pushDescriptors[i];
			const DescriptorBinding binding = pushLayout->info.bindings.ptr[i];
			const ESHRegisterType type = (ESHRegisterType)(binding.registerType & ESHRegisterType_TypeMask);

			const U32 rootParam = layoutExt->rootParamPushDescriptors + pushExt->rootParamOffsets.ptr[i];

			D3D12_GPU_VIRTUAL_ADDRESS addr =
				type == ESHRegisterType_AccelerationStructure ?
				DeviceBufferRef_ptr(TLASRef_ptr(d.resource)->base.asBuffer)->resource.deviceAddress :
				DeviceBufferRef_ptr(d.resource)->resource.deviceAddress + Descriptor_startBuffer(&d);

			if (type == ESHRegisterType_ConstantBuffer) {

				if(isCompute)
					buffer->lpVtbl->SetComputeRootConstantBufferView(buffer, rootParam, addr);

				else buffer->lpVtbl->SetGraphicsRootConstantBufferView(buffer, rootParam, addr);
			}

			else if (binding.registerType & ESHRegisterType_IsWrite) {

				if(isCompute)
					buffer->lpVtbl->SetComputeRootUnorderedAccessView(buffer, rootParam, addr);

				else buffer->lpVtbl->SetGraphicsRootUnorderedAccessView(buffer, rootParam, addr);
			}

			else {

				if(isCompute)
					buffer->lpVtbl->SetComputeRootShaderResourceView(buffer, rootParam, addr);

				else buffer->lpVtbl->SetGraphicsRootShaderResourceView(buffer, rootParam, addr);
			}
		}

		temp->pushDescriptorsEmitted[bindPoint] = true;
	}

	if(!temp->boundDescriptorTable || !layout->info.bindings)
		return;

	const DescriptorTable *table = DescriptorTableRef_ptr(temp->boundDescriptorTable);

	//A table stays bound across a pipeline switch, so it can outlive the layout it was bound for.
	//Emitting it against a root signature built from a DIFFERENT DescriptorLayout writes root parameters
	// that signature never declared, which is what interleaving a bindful dispatch with a bindless one does.

	if(table->layout != layout->info.bindings)
		return;

	//Root arguments persist while the root signature does, so an unchanged table has nothing to re-emit

	if(temp->lastBoundTable[bindPoint] == temp->boundDescriptorTable)
		return;

	temp->lastBoundTable[bindPoint] = temp->boundDescriptorTable;

	DxDescriptorTable *tableExt = DescriptorTable_ext((DescriptorTable*)table, Dx);

	DxDescriptorHeap *heap = DescriptorHeap_ext(DescriptorHeapRef_ptr(table->parent), Dx);

	//The table's heap has to be current; when it differs from the default heap this switches it, and the
	// default rebind switches back through the dirty flag above.

	if (temp->boundDescriptorHeap && temp->lastBoundHeap != temp->boundDescriptorHeap) {

		ID3D12DescriptorHeap *descriptorHeaps[2] = { heap->resourcesHeap.heap, heap->samplerHeap.heap };
		buffer->lpVtbl->SetDescriptorHeaps(buffer, heap->samplerHeap.heap ? 2 : 1, descriptorHeaps);

		temp->lastBoundHeap = temp->boundDescriptorHeap;
	}

	//The layout has at most two root tables (resources, samplers), created in encounter order; each binding's
	// root param is in rootParamOffsets, so the first binding of each class names its table's param.

	const DescriptorLayout *descLayout = DescriptorLayoutRef_ptr(layout->info.bindings);
	DxDescriptorLayout *descLayoutExt = DescriptorLayout_ext((DescriptorLayout*)descLayout, Dx);

	U8 resourceParam = U8_MAX, samplerParam = U8_MAX;

	for (U64 i = 0; i < descLayout->info.bindings.length; ++i) {

		const ESHRegisterType type =
			(ESHRegisterType)(descLayout->info.bindings.ptr[i].registerType & ESHRegisterType_TypeMask);

		const Bool isSampler = type == ESHRegisterType_Sampler || type == ESHRegisterType_SamplerComparisonState;

		if(isSampler && samplerParam == U8_MAX)
			samplerParam = descLayoutExt->rootParamOffsets.ptr[i];

		else if(!isSampler && resourceParam == U8_MAX)
			resourceParam = descLayoutExt->rootParamOffsets.ptr[i];
	}

	if (resourceParam != U8_MAX) {

		const D3D12_GPU_DESCRIPTOR_HANDLE handle = (D3D12_GPU_DESCRIPTOR_HANDLE) {
			.ptr = heap->resourcesHeap.gpuHandle.ptr + tableExt->allocationLocations[0] * heap->resourcesHeap.gpuIncrement
		};

		if(isCompute)
			buffer->lpVtbl->SetComputeRootDescriptorTable(buffer, resourceParam, handle);

		else buffer->lpVtbl->SetGraphicsRootDescriptorTable(buffer, resourceParam, handle);
	}

	if (samplerParam != U8_MAX) {

		const D3D12_GPU_DESCRIPTOR_HANDLE handle = (D3D12_GPU_DESCRIPTOR_HANDLE) {
			.ptr = heap->samplerHeap.gpuHandle.ptr + tableExt->allocationLocations[1] * heap->samplerHeap.gpuIncrement
		};

		if(isCompute)
			buffer->lpVtbl->SetComputeRootDescriptorTable(buffer, samplerParam, handle);

		else buffer->lpVtbl->SetGraphicsRootDescriptorTable(buffer, samplerParam, handle);
	}
}

//A declared stage stands for itself AND every later stage that could reach the resource, because a scope
//names only the FIRST stage that accesses it. D3D12 groups the graphics shader stages into two scopes, so a
//pre pixel stage means both of them, while compute stays a scope of its own rather than being folded in
//through NON_PIXEL_SHADING as it used to be.

//The acceleration structure access bits accept only a short list of sync scopes, and the per stage graphics
//scopes are not on it: a graphics stage reaches an RTAS through inline raytracing (RayQuery), for which
//ALL_SHADING is the narrowest legal scope. Pairing VERTEX_SHADING or PIXEL_SHADING with an RTAS access bit
//is rejected outright by the debug layer, so the generic mapper below can't be reused here.

static D3D12_BARRIER_SYNC DxBarrierSyncRtas_fromMask(U32 stageMask) {

	D3D12_BARRIER_SYNC sync = D3D12_BARRIER_SYNC_NONE;

	//ALL_SHADING already subsumes compute and raytracing, so those only matter on their own

	if(stageMask & (EPipelineStageMask_PrePixel | ((U32)1 << EPipelineStage_Pixel)))
		sync |= D3D12_BARRIER_SYNC_ALL_SHADING;

	else {

		if(stageMask & ((U32)1 << EPipelineStage_Compute))
			sync |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;

		if(stageMask & EPipelineStageMask_RtAny)
			sync |= D3D12_BARRIER_SYNC_RAYTRACING;
	}

	if(stageMask & EPipelineStageMask_RTASBuild)
		sync |= D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;

	//An empty scope would pair SYNC_NONE with an access bit that isn't NO_ACCESS, which isn't legal either

	if(!sync)
		sync = D3D12_BARRIER_SYNC_ALL_SHADING;

	return sync;
}

static D3D12_BARRIER_SYNC DxBarrierSync_fromMask(U32 stageMask) {

	D3D12_BARRIER_SYNC sync = D3D12_BARRIER_SYNC_NONE;

	if(stageMask & EPipelineStageMask_PrePixel)
		sync |= D3D12_BARRIER_SYNC_VERTEX_SHADING | D3D12_BARRIER_SYNC_PIXEL_SHADING;

	if(stageMask & ((U32)1 << EPipelineStage_Pixel))
		sync |= D3D12_BARRIER_SYNC_PIXEL_SHADING;

	if(stageMask & ((U32)1 << EPipelineStage_Compute))
		sync |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;

	if(stageMask & EPipelineStageMask_RtAny)
		sync |= D3D12_BARRIER_SYNC_RAYTRACING;

	if(stageMask & EPipelineStageMask_RTASBuild)
		sync |= D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;

	return sync;
}

//Writes one timestamp to the frame's query heap at the current cursor when timing is active, then advances it.
//The cursor advances even where the heap is absent, so it stays in lockstep with GraphicsDevice_buildTimings.

static void dxTimestampWrite(DxGraphicsDevice *deviceExt, U8 fifId, DxCommandBuffer *buffer) {

	if(deviceExt->timestampHeap[fifId] && deviceExt->timestampCursor < deviceExt->timestampCapacity[fifId])
		buffer->lpVtbl->EndQuery(
			buffer, deviceExt->timestampHeap[fifId], D3D12_QUERY_TYPE_TIMESTAMP, deviceExt->timestampCursor
		);

	++deviceExt->timestampCursor;
}

void DX_WRAP_FUNC(CommandList_process)(
	CommandList *commandList,
	GraphicsDeviceRef *deviceRef,
	ECommandOp op,
	const U8 *data,
	void *commandListExt
) {

	//CommandList_process can't fail upward; errors are printed and the op is skipped.

	Bool s_uccess = true;
	Error err = Error_none();
	Error *e_rr = &err;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

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
					.MinDepth = 0,
					.MaxDepth = 1
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

			Buffer_memcpy(
				Buffer_createRef(&temp->tempBlendConstants, sizeof(F32x4)),
				Buffer_createRefConst(data, sizeof(F32x4))
			);

			break;

		//Clear and copy commands

		case ECommandOp_ClearImages: {

			U64 imageClearCount = *(const U64*) data;

			//Prepare attachments

			const DxDescriptorHeapSingle *heap = &deviceExt->cpuHeaps[ECPUDescriptorHeapType_RTV];

			D3D12_CPU_DESCRIPTOR_HANDLE cpuDesc = (D3D12_CPU_DESCRIPTOR_HANDLE) { .ptr = heap->cpuHandle.ptr };

			for (U8 i = 0; i < imageClearCount; ++i) {

				ClearImageCmd image = ((const ClearImageCmd*) (data + sizeof(U64) * 2))[i];
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
					.bottom    = image.srcY + image.height,
					.back    = image.srcZ + image.length
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

			const DxDescriptorHeapSingle *heap = &deviceExt->cpuHeaps[ECPUDescriptorHeapType_RTV];

			D3D12_CPU_DESCRIPTOR_HANDLE cpuDesc = heap->cpuHandle;
			U8 j = 0;

			D3D12_RECT rect = (D3D12_RECT) {
				.left = I32x2_x(startRender->offset),
				.top = I32x2_y(startRender->offset),
				.right = I32x2_x(startRender->offset) + I32x2_x(startRender->size),
				.bottom = I32x2_y(startRender->offset) + I32x2_y(startRender->size)
			};

			Bool anyResolve = false;

			//Every slot is cleared BEFORE the attachments fill it, not just the ones past colorCount.
			//A slot that is active but carries no resolveImage is never written below, so clearing only the
			//tail left it pointing at whatever the PREVIOUS render pass resolved into: a later pass that
			//resolves any other attachment sets anyResolve and would then resolve this one too, into an
			//image it no longer names. Slot 8 is the depth stencil one and is re-armed further down.

			for (U8 i = 0; i < 9; ++i) {
				temp->boundTargets[i] = temp->resolveTargets[i] = (ImageAndRange) { 0 };
				temp->resolveModes[i] = EMSAAResolveMode_Average;
			}

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

						temp->resolveModes[i] = attachmentsj->resolveMode;
						anyResolve = true;
					}
				}

				D3D12_CPU_DESCRIPTOR_HANDLE curr = createTempRTV(deviceExt, i, cpuDesc, heap, active);

				UnifiedTexture utex = TextureRef_getUnifiedTexture(active, NULL);
				Bool hasRect = I32x2_neq2(startRender->size, I32x2_create2(utex.width, utex.height));

				if(((startRender->clearMask & startRender->activeMask) >> i) & 1)
					buffer->lpVtbl->ClearRenderTargetView(
						buffer, curr, attachmentsj->color.colorf, hasRect ? 1 : 0, hasRect ? &rect : NULL
					);

				else if(!(((startRender->preserveMask & startRender->activeMask) >> i) & 1)) {

					DxUnifiedTexture *tex = TextureRef_getCurrImgExtT(active, Dx, 0);

					D3D12_DISCARD_REGION region = (D3D12_DISCARD_REGION) {
						.NumRects = hasRect ? 1 : 0,
						.pRects = hasRect ? &rect : NULL,
						.FirstSubresource = 0,                //TODO:
						.NumSubresources = 1
					};

					buffer->lpVtbl->DiscardResource(buffer, tex->image, hasRect ? &region : NULL);
				}

				if(active)
					++j;
			}

			const DxDescriptorHeapSingle *dsvHeap = &deviceExt->cpuHeaps[ECPUDescriptorHeapType_DSV];
			D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuDesc = dsvHeap->cpuHandle;
			D3D12_CPU_DESCRIPTOR_HANDLE dsv =
				!startRender->depthStencil ? (D3D12_CPU_DESCRIPTOR_HANDLE) { 0 } :
				createTempDSV(deviceExt, 0, dsvCpuDesc, startRender->flags, dsvHeap, startRender->depthStencil);

			//TODO: Respect EStartRenderFlags_StencilReadOnly and EStartRenderFlags_DepthReadOnly

			UnifiedTexture utex = TextureRef_getUnifiedTexture(startRender->depthStencil, NULL);
			Bool hasRect = I32x2_neq2(startRender->size, I32x2_create2(utex.width, utex.height));

			EStartRenderFlags preserveDepthStencil = EStartRenderFlags_PreserveStencil | EStartRenderFlags_PreserveDepth;

			if(startRender->flags & EStartRenderFlags_ClearDepthStencil)
				buffer->lpVtbl->ClearDepthStencilView(
					buffer, dsv,
					(startRender->flags & EStartRenderFlags_ClearDepth ? D3D12_CLEAR_FLAG_DEPTH : 0) |
					(startRender->flags & EStartRenderFlags_ClearStencil ? D3D12_CLEAR_FLAG_STENCIL : 0),
					startRender->clearDepth, startRender->clearStencil,
					hasRect ? 1 : 0, hasRect ? &rect : NULL
				);

			else if (startRender->depthStencil && (startRender->flags & preserveDepthStencil) != preserveDepthStencil) {

				DxUnifiedTexture *tex = TextureRef_getCurrImgExtT(startRender->depthStencil, Dx, 0);

				D3D12_DISCARD_REGION region = (D3D12_DISCARD_REGION) {
					.NumRects = hasRect ? 1 : 0,
					.pRects = hasRect ? &rect : NULL,
					.FirstSubresource = 0,        //TODO: Ensure all are valid
					.NumSubresources = 1        //TODO: ^
				};

				Bool preserveDepth = startRender->flags & EStartRenderFlags_PreserveDepth;
				Bool preserveStencil = startRender->flags & EStartRenderFlags_PreserveStencil;

				Bool hasStencil = utex.depthFormat >= EDepthStencilFormat_StencilStart;

				Bool isPartial = hasStencil && preserveDepth && !preserveStencil;

				if(!isPartial && hasStencil && !preserveStencil)
					++region.NumSubresources;

				Bool hasRegion = !isPartial || hasRect;

				if(!preserveDepth)
					buffer->lpVtbl->DiscardResource(buffer, tex->image, hasRegion ? &region : NULL);

				if(!preserveStencil && isPartial) {
					++region.FirstSubresource;
					buffer->lpVtbl->DiscardResource(buffer, tex->image, &region);
				}
			}

			if(startRender->resolveDepthStencil) {

				temp->boundTargets[8] = (ImageAndRange) {
					.range = startRender->depthStencilRange,
					.image = startRender->depthStencil
				};

				temp->resolveTargets[8] = (ImageAndRange) {
					.range = startRender->resolveDepthStencilRange,
					.image = startRender->resolveDepthStencil
				};

				temp->resolveModes[8] = startRender->resolveDepthStencilMode;
				anyResolve = true;
			}

			buffer->lpVtbl->OMSetRenderTargets(
				buffer, j,
				&cpuDesc, true,
				!startRender->depthStencil ? NULL : &dsv
			);

			temp->inRender = true;
			temp->anyResolve = anyResolve;
			temp->size = startRender->size;
			temp->offset = startRender->offset;
			temp->colorCount = startRender->colorCount | (startRender->depthStencil ? 0x80 : 0);
			break;
		}

		//No-Op, except for MSAA resolving

		case ECommandOp_EndRenderingExt: {

			temp->inRender = false;

			if(!temp->anyResolve)
				break;

			D3D12_BARRIER_GROUP deps = { .Type = D3D12_BARRIER_TYPE_TEXTURE };

			//All DSVs and RTVs to the correct state (resolve source)

			U32 count = temp->colorCount & 0x7F;
			U32 forCount = count + (temp->colorCount >> 7);

			D3D12_RECT rect = (D3D12_RECT) {
				.left = I32x2_x(temp->offset),
				.top = I32x2_y(temp->offset),
				.right = I32x2_x(temp->offset) + I32x2_x(temp->size),
				.bottom = I32x2_y(temp->offset) + I32x2_y(temp->size)
			};

			for(U8 i = 0; i < forCount; ++i) {

				ImageAndRange input = temp->boundTargets[i == count ? 8 : i];
				ImageAndRange output = temp->resolveTargets[i == count ? 8 : i];

				if(!input.image || !output.image)        //Only there if it's really required
					continue;

				//Discard destination and transition to correct state

				{
					const DxUnifiedTexture *resolveExt = TextureRef_getCurrImgExtT(output.image, Dx, 0);
					const UnifiedTexture utex = TextureRef_getUnifiedTexture(output.image, NULL);
					DxUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(output.image, Dx, 0);

					Bool hasRect = I32x2_neq2(temp->size, I32x2_create2(utex.width, utex.height));

					D3D12_DISCARD_REGION region = (D3D12_DISCARD_REGION) {
						.NumRects = hasRect ? 1 : 0,
						.pRects = hasRect ? &rect : NULL,
						.FirstSubresource = 0,        //TODO: Ensure all are valid
						.NumSubresources = 1        //TODO: ^
					};

					buffer->lpVtbl->DiscardResource(buffer, resolveExt->image, &region);

					D3D12_BARRIER_SUBRESOURCE_RANGE range = (D3D12_BARRIER_SUBRESOURCE_RANGE) {
						.NumMipLevels = 1,
						.NumArraySlices = 1,
						.NumPlanes = 1
					};

					if (i == count) {        //Depth stencil

						//A depth ONLY format (D16, D32) has a single plane, so claiming two is out of bounds.
						//Only a combined format has a stencil plane to add, and a stencil only format IS the
						// stencil plane, which sits after the depth one.

						if(utex.depthFormat == EDepthStencilFormat_S8X24Ext)                //Take only the stencil plane
							++range.FirstPlane;

						else if(utex.depthFormat >= EDepthStencilFormat_StencilStart)        //Take depth & stencil
							++range.NumPlanes;
					}

					if(!DxUnifiedTexture_transition(
						imageExt,
						D3D12_BARRIER_SYNC_RESOLVE,
						D3D12_BARRIER_ACCESS_RESOLVE_DEST,
						D3D12_BARRIER_LAYOUT_RESOLVE_DEST,
						&range,
						&deviceExt->imageTransitions,
						&deps, alloc, e_rr
					))
						Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);
				}

				//Transition both source and destination;
				//Source wasn't transitioned, but destination was only transitioned for discard

				const UnifiedTexture utex = TextureRef_getUnifiedTexture(input.image, NULL);
				DxUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(input.image, Dx, 0);

				D3D12_BARRIER_SUBRESOURCE_RANGE range = (D3D12_BARRIER_SUBRESOURCE_RANGE) {
					.NumMipLevels = 1,
					.NumArraySlices = 1,
					.NumPlanes = 1
				};

				if (i == count) {        //Depth stencil

					//Same plane rule as the discard above: a depth only format is one plane

					if(utex.depthFormat == EDepthStencilFormat_S8X24Ext)                //Take only the stencil plane
						++range.FirstPlane;

					else if(utex.depthFormat >= EDepthStencilFormat_StencilStart)        //Take depth & stencil
						++range.NumPlanes;
				}

				if(!DxUnifiedTexture_transition(
					imageExt,
					D3D12_BARRIER_SYNC_RESOLVE,
					D3D12_BARRIER_ACCESS_RESOLVE_SOURCE,
					D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE,
					&range,
					&deviceExt->imageTransitions,
					&deps, alloc, e_rr
				))
					Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);
			}

			if(deps.NumBarriers)
				buffer->lpVtbl->Barrier(buffer, 1, &deps);

			ListD3D12_TEXTURE_BARRIER_clear(&deviceExt->imageTransitions, e_rr);

			//Resolve

			for(U8 i = 0; i < forCount; ++i) {

				ImageAndRange input = temp->boundTargets[i == count ? 8 : i];
				ImageAndRange output = temp->resolveTargets[i == count ? 8 : i];

				if(!input.image || !output.image)        //Only there if it's really required
					continue;

				const DxUnifiedTexture *imageExt = TextureRef_getCurrImgExtT(input.image, Dx, 0);
				const DxUnifiedTexture *resolveExt = TextureRef_getCurrImgExtT(output.image, Dx, 0);
				const UnifiedTexture utex = TextureRef_getUnifiedTexture(input.image, NULL);

				DXGI_FORMAT format = 0;

				//ResolveSubresource refuses depth formats outright: it takes only fully typed non integer
				// non stencil formats, plus the two depth readable typeless ones it names in its own
				// diagnostic. Those are exactly the formats a depth SRV uses, so the resolve borrows that
				// mapping rather than handing it the DSV format, which is what made depth resolve fail.

				if(i == count)
					switch(utex.depthFormat) {

						default:
						case EDepthStencilFormat_D32:            format = DXGI_FORMAT_R32_FLOAT;               break;
						case EDepthStencilFormat_D16:            format = DXGI_FORMAT_R16_UNORM;               break;
						case EDepthStencilFormat_D32S8X24Ext:    format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS; break;
						case EDepthStencilFormat_D24S8Ext:       format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;   break;
					}

				else format = ETextureFormatId_toDXFormat(utex.textureFormatId);

				//ResolveSubresourceRegion rather than ResolveSubresource, which has no mode parameter at all
				// and always averages. A NULL rect resolves the whole subresource, so with AVERAGE the two are
				// the same call; MIN and MAX are only reachable through this one.

				D3D12_RESOLVE_MODE resolveMode = D3D12_RESOLVE_MODE_AVERAGE;

				switch(temp->resolveModes[i == count ? 8 : i]) {
					default:                    break;
					case EMSAAResolveMode_Min:  resolveMode = D3D12_RESOLVE_MODE_MIN; break;
					case EMSAAResolveMode_Max:  resolveMode = D3D12_RESOLVE_MODE_MAX; break;
				}

				buffer->lpVtbl->ResolveSubresourceRegion(
					buffer,
					resolveExt->image, 0, 0, 0,
					imageExt->image, 0,
					NULL,
					format,
					resolveMode
				);
			}

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

			if (F32x4_neqExact4(temp->tempBlendConstants, temp->blendConstants)) {
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

			DxCommandBufferState_bindDescriptors(temp, device, temp->pipeline, false);

			PipelineGraphicsInfo *graphicsShader = Pipeline_info(PipelineRef_ptr(temp->pipeline), PipelineGraphicsInfo);
			D3D12_PRIMITIVE_TOPOLOGY topology = 0;

			if(graphicsShader->patchControlPoints)
				topology = D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + graphicsShader->patchControlPoints - 1;

			else switch(graphicsShader->topologyMode) {

				default:                                topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;            break;
				case ETopologyMode_TriangleStrip:        topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;        break;

				case ETopologyMode_LineList:            topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;                break;
				case ETopologyMode_LineStrip:            topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;            break;

				case ETopologyMode_PointList:            topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;            break;

				case ETopologyMode_TriangleListAdj:        topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;        break;
				case ETopologyMode_TriangleStripAdj:    topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;    break;

				case ETopologyMode_LineListAdj:            topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;            break;
				case ETopologyMode_LineStripAdj:        topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;        break;
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

			DxCommandBufferState_bindDescriptors(temp, device, temp->pipeline, true);

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

			if(!(DX_WRAP_FUNC(BLASRef_flush))(temp, deviceRef, *(BLASRef**)data, e_rr))
				Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);

			break;

		case ECommandOp_UpdateTLASExt:

			if(!(DX_WRAP_FUNC(TLASRef_flush))(temp, deviceRef, *(TLASRef**)data, e_rr))
				Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);

			break;

		//An OMM array is an acceleration structure on D3D12, so it flushes the same way

		case ECommandOp_BindDescriptorTable:
			temp->boundDescriptorTable = *(RefPtr* const*) data;
			break;

		case ECommandOp_BindDescriptorHeap:
			temp->boundDescriptorHeap = *(RefPtr* const*) data;
			break;

		//The bytes travel with the command, so nothing here depends on the recorder's buffer still existing

		case ECommandOp_SetPushConstants: {

			const SetPushConstantsCmd *push = (const SetPushConstantsCmd*) data;

			temp->pushConstantSize = (U8) push->size;

			Buffer_memcpy(
				Buffer_createRef(temp->pushConstantData, push->size),
				Buffer_createRefConst(push->data, push->size)
			);

			//A fresh write has to reach every bind point, since each carries its own copy

			temp->pushConstantsEmitted[0] = temp->pushConstantsEmitted[1] = false;

			break;
		}

		case ECommandOp_SetPushDescriptors: {

			const SetPushDescriptorsCmd *push = (const SetPushDescriptorsCmd*) data;
			const Descriptor *descriptors = (const Descriptor*)(push + 1);

			temp->pushDescriptorCount = (U8) push->count;

			Buffer_memcpy(
				Buffer_createRef(temp->pushDescriptors, push->count * sizeof(Descriptor)),
				Buffer_createRefConst(descriptors, push->count * sizeof(Descriptor))
			);

			temp->pushDescriptorsEmitted[0] = temp->pushDescriptorsEmitted[1] = false;

			break;
		}

		case ECommandOp_UpdateOmmExt:

			if(!(DX_WRAP_FUNC(OpacityMicromapRef_flush))(temp, deviceRef, *(OpacityMicromapRef**)data, e_rr))
				Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);

			break;

		case ECommandOp_DispatchRaysIndirect:
		case ECommandOp_DispatchRaysExt: {

			Pipeline *raytracingPipeline = PipelineRef_ptr(temp->tempPipelines[EPipelineType_RaytracingExt]);

			if(temp->pipeline != temp->tempPipelines[EPipelineType_RaytracingExt]) {

				temp->pipeline = temp->tempPipelines[EPipelineType_RaytracingExt];

				buffer->lpVtbl->SetPipelineState1(
					temp->buffer,
					Pipeline_ext(raytracingPipeline, Dx)->stateObject
				);
			}

			//Ray tracing dispatch reads through the compute root signature on D3D12

			DxCommandBufferState_bindDescriptors(
				temp, device, temp->tempPipelines[EPipelineType_RaytracingExt], true
			);

			PipelineRaytracingInfo info = *Pipeline_info(raytracingPipeline, PipelineRaytracingInfo);

			//The shader binding table is read by the trace itself rather than by anything the scope declared,
			// so no transition ever names it and its tracked state stays where the upload copy left it.
			//Ordering it here is what makes the trace's reads visible to that copy; after the first trace the
			// buffer already sits in this state and the helper adds no barrier.

			if (info.shaderBindingTable) {

				D3D12_BARRIER_GROUP sbtDependency = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_BUFFER };
				DeviceBuffer *sbtBuffer = DeviceBufferRef_ptr(info.shaderBindingTable);

				if(!DxDeviceBuffer_transition(
					DeviceBuffer_ext(sbtBuffer, Dx),
					D3D12_BARRIER_SYNC_RAYTRACING,
					D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
					&deviceExt->bufferTransitions,
					&sbtDependency, alloc, e_rr
				))
					Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);

				else if(sbtDependency.NumBarriers)
					buffer->lpVtbl->Barrier(temp->buffer, 1, &sbtDependency);

				ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions, e_rr);
			}

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

			else {

				DispatchRaysIndirectExt dispatch = *(const DispatchRaysIndirectExt*)data;
				raygen.StartAddress += raytracingShaderAlignment * dispatch.raygenId;

				//ExecuteIndirect wants the whole D3D12_DISPATCH_RAYS_DESC in the argument buffer, yet the shader binding
				// table is not dynamic, so only the three thread counts come from the caller. The pipeline's known SBT
				// ranges are written into a per frame intermediate slot and the counts copied in beside them, leaving a
				// conformant desc. Vulkan reads the caller buffer directly and has no such copy, which is why the caller
				// buffer becomes a copy source here at runtime rather than through the scope.

				D3D12_DISPATCH_RAYS_DESC desc = (D3D12_DISPATCH_RAYS_DESC) {
					.RayGenerationShaderRecord = raygen,
					.MissShaderTable = miss,
					.HitGroupTable = hit,
					.CallableShaderTable = callable
				};

				DeviceBufferRef *argsRef = deviceExt->dispatchRaysIndirect[device->fifId];
				const U64 stride = sizeof(D3D12DispatchRaysIndirect);
				const U32 capacity = argsRef ? (U32) (DeviceBufferRef_ptr(argsRef)->resource.size / stride) : 0;
				U32 slot = deviceExt->dispatchRaysIndirectCursor;

				if(!argsRef || slot >= capacity)
					Log_errorLnx("DispatchRaysIndirect argument buffer could not grow (out of memory), dropping one dispatch");

				else {

					++deviceExt->dispatchRaysIndirectCursor;

					const U64 argOffset = (U64) slot * stride;
					const Bool wbi = !!(device->info.capabilities.featuresExt & EDxGraphicsFeatures_WriteBufferImmediate);

					DxDeviceBuffer *argsBuf = DeviceBuffer_ext(DeviceBufferRef_ptr(argsRef), Dx);
					DxDeviceBuffer *callerBuf = DeviceBuffer_ext(DeviceBufferRef_ptr(dispatch.buffer), Dx);

					//Group A: the caller becomes a copy source (the scope left it an indirect argument for Vulkan, which reads
					// it directly; this copy is the D3D12 only runtime divergence) and the slot a copy destination. Both go
					// through DxDeviceBuffer_transition, which tracks each buffer's prior state so the barriers are automatic.

					D3D12_BARRIER_GROUP depA = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_BUFFER };

					if(
						!DxDeviceBuffer_transition(
							callerBuf, D3D12_BARRIER_SYNC_COPY, D3D12_BARRIER_ACCESS_COPY_SOURCE,
							&deviceExt->bufferTransitions, &depA, alloc, e_rr
						) ||
						!DxDeviceBuffer_transition(
							argsBuf, D3D12_BARRIER_SYNC_COPY, D3D12_BARRIER_ACCESS_COPY_DEST,
							&deviceExt->bufferTransitions, &depA, alloc, e_rr
						)
					)
						Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);

					if(depA.NumBarriers)
						buffer->lpVtbl->Barrier(buffer, 1, &depA);

					ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions, e_rr);

					//The 88-byte SBT ranges are CPU known. On a direct queue that supports WriteBufferImmediate they are poked
					// straight into the slot (no staging buffer); otherwise they are written into the mapped UPLOAD staging
					// slot at record time and copied in. The three thread counts always arrive by copy from the caller.

					const U32 sbtBytes =
						(U32)(sizeof(D3D12_GPU_VIRTUAL_ADDRESS_RANGE) + 3 * sizeof(D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE));

					if(wbi) {

						const U32 *sbtWords = (const U32*) &desc;
						const D3D12_GPU_VIRTUAL_ADDRESS argAddress =
							DeviceBufferRef_ptr(argsRef)->resource.deviceAddress + argOffset;

						D3D12_WRITEBUFFERIMMEDIATE_PARAMETER params[sizeof(D3D12_DISPATCH_RAYS_DESC) / sizeof(U32)];

						for(U32 i = 0; i < sbtBytes / (U32) sizeof(U32); ++i)
							params[i] = (D3D12_WRITEBUFFERIMMEDIATE_PARAMETER) {
								.Dest = argAddress + (U64) i * sizeof(U32), .Value = sbtWords[i]
							};

						buffer->lpVtbl->WriteBufferImmediate(buffer, sbtBytes / (U32) sizeof(U32), params, NULL);

						//WriteBufferImmediate DEFAULT mode's completion is not clearly covered by SYNC_COPY, so widen the
						// slot's next drain to SYNC_ALL by nudging its tracked sync; the ->INDIRECT_ARGUMENT transition below
						// then emits SyncBefore = SYNC_ALL while keeping DxDeviceBuffer's state consistent.

						argsBuf->lastSync = D3D12_BARRIER_SYNC_ALL;
					}

					else {

						DeviceBufferRef *stagingRef = deviceExt->dispatchRaysIndirectStaging[device->fifId];
						DxDeviceBuffer *stagingBuf = DeviceBuffer_ext(DeviceBufferRef_ptr(stagingRef), Dx);
						U8 *stagingMap = (U8*) DeviceBufferRef_ptr(stagingRef)->resource.mappedMemoryExt;

						Buffer_memcpy(
							Buffer_createRef(stagingMap + argOffset, sbtBytes),
							Buffer_createRefConst(&desc, sbtBytes)
						);

						buffer->lpVtbl->CopyBufferRegion(
							buffer, argsBuf->buffer, argOffset, stagingBuf->buffer, argOffset, sbtBytes
						);
					}

					buffer->lpVtbl->CopyBufferRegion(
						buffer, argsBuf->buffer, argOffset + sbtBytes,
						callerBuf->buffer, dispatch.offset, sizeof(U32) * 3
					);

					//Group B: the slot flips to an indirect argument. SyncBefore comes from its tracked state, SYNC_COPY for
					// the copy path and SYNC_ALL after the WBI nudge, so both the SBT write and the counts copy are drained.

					D3D12_BARRIER_GROUP depB = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_BUFFER };

					if(!DxDeviceBuffer_transition(
						argsBuf, D3D12_BARRIER_SYNC_EXECUTE_INDIRECT, D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT,
						&deviceExt->bufferTransitions, &depB, alloc, e_rr
					))
						Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);

					if(depB.NumBarriers)
						buffer->lpVtbl->Barrier(buffer, 1, &depB);

					ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions, e_rr);

					buffer->lpVtbl->ExecuteIndirect(
						buffer, deviceExt->commandSigs[EExecuteIndirectCommand_DispatchRays],
						1, argsBuf->buffer, argOffset, NULL, 0
					);
				}
			}

			break;
		}

		case ECommandOp_StartScope: {

			//Stencil and blend constants belong to the scope that set them, so a scope that doesn't set them starts
			// from the default rather than inheriting whatever the previous one left.
			//Recording already forces the pipeline, viewport and scissor to be re-declared per scope; without this
			// these two would be the only state that leaks across, which makes a scope depend on whether an
			// unrelated earlier one happened to be hidden.
			//Only the requested values reset, since stencilRef and blendConstants track what the GPU actually has;
			// the flush before the next draw then sets them again only if they really differ.

			temp->tempStencilRef = 0;
			temp->tempBlendConstants = F32x4_zero();

			D3D12_BARRIER_GROUP dep[2] = {
				(D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_TEXTURE },
				(D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_BUFFER }
			};

			CommandScope scope = commandList->activeScopes.ptr[temp->scopeCounter];
			++temp->scopeCounter;

			//A scope may bracket its work in a timestamp pair and/or a named debug region;
			// the flags carry both decisions to EndScope.
			//A DebugRegion scope has its name in the StartScope payload (data).

			temp->curScopeFlags = (U8) scope.flags;

			if((temp->curScopeFlags & ECommandScopeInternalFlags_DebugRegion)) {

				U64 encoded[62] = { 0 };
				encoded[0] = (U64) 0x002 << 10;
				encoded[1] = 0xFF000000 | 0x6699E6;
				encoded[2] = ((U64) 8 << 55) | ((U64) 1 << 54);        //Address alignment = 8 and indicate "ansi"

				const C8 *scopeName = (const C8*) data + sizeof(CommandScopePredicate);
				const U32 strLen = (U32) CharString_calcStrLen(scopeName, sizeof(encoded) - sizeof(U64) * 3 - 1);
				const U32 len = (U32) sizeof(U64) * 3 + strLen;

				Buffer_memcpy(
					Buffer_createRef(&encoded[3], sizeof(encoded) - sizeof(U64) * 3),
					Buffer_createRefConst(scopeName, strLen)
				);

				buffer->lpVtbl->BeginEvent(buffer, 2, encoded, (len + 7) &~ 7);
			}

			if((temp->curScopeFlags & ECommandScopeInternalFlags_Timed))
				dxTimestampWrite(deviceExt, device->fifId, buffer);

			for (U64 i = scope.transitionOffset; i < scope.transitionOffset + scope.transitionCount; ++i) {

				TransitionInternal transition = commandList->transitions.ptr[i];

				if(transition.type == ETransitionType_KeepAlive)        //TODO: Residency management
					continue;

				D3D12_BARRIER_SYNC pipelineStage = DxBarrierSync_fromMask(transition.stageMask);

				//If it's on the GPU then we have to rely on manual RTAS transitions

				Bool isTLAS = transition.resource->refPtrType->typeId == (TypeId)EGraphicsTypeId_TLASExt;
				Bool isOMM = transition.resource->refPtrType->typeId == (TypeId)EGraphicsTypeId_OpacityMicromapExt;

				//An OMM array is an acceleration structure on D3D12, so it transitions exactly like one

				if (isTLAS || isOMM || transition.resource->refPtrType->typeId == (TypeId)EGraphicsTypeId_BLASExt) {

					RTAS rtas =
						isTLAS ? TLASRef_ptr(transition.resource)->base : (
							isOMM ? OpacityMicromapRef_ptr(transition.resource)->base :
							BLASRef_ptr(transition.resource)->base
						);

					gotoIfError3(nextTransition, DxDeviceBuffer_transition(

						DeviceBuffer_ext(DeviceBufferRef_ptr(rtas.asBuffer), Dx),

						DxBarrierSyncRtas_fromMask(transition.stageMask),

						transition.type == ETransitionType_ShaderWrite ?
							D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE |
							D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ :
							D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,

						&deviceExt->bufferTransitions,
						&dep[1], alloc, e_rr));

					continue;
				}

				//Grab transition type

				Bool isImage = TextureRef_isTexture(transition.resource);
				Bool isDepthStencil = TextureRef_isDepthStencil(transition.resource);
				Bool isShaderRead = transition.type == ETransitionType_ShaderRead;

				D3D12_BARRIER_LAYOUT layout = D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
				D3D12_BARRIER_ACCESS access = 0;

				access = isShaderRead ? D3D12_BARRIER_ACCESS_SHADER_RESOURCE : D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;

				//A readable uniform buffer is read through CONSTANT_BUFFER rather than SHADER_RESOURCE;
				// both can apply when a buffer carries ShaderRead and the uniform usage at once

				if (
					isShaderRead && !isImage &&
					transition.resource->refPtrType->typeId == (TypeId) EGraphicsTypeId_DeviceBuffer &&
					(DeviceBufferRef_ptr(transition.resource)->usage & EDeviceBufferUsage_Uniform)
				) {

					access |= D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;

					if(!(DeviceBufferRef_ptr(transition.resource)->resource.flags & EGraphicsResourceFlag_ShaderRead))
						access &=~ D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
				}

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

						//A depth resolve target is discarded and resolved as a depth stencil resource, not as a
						// render target: RENDER_TARGET access and layout aren't legal on a texture that was never
						// created with the render target flag, and this scope is what the next barrier uses as its
						// source, so declaring the wrong one lets the resolve race the discard that precedes it.
						//Clear only ever reaches render textures and swapchains (clearImages refuses anything
						// else), so its resource is never depth stencil and the ternaries collapse for it.

						case ETransitionType_ResolveTargetWrite:        //We handle a 'secret' transition after (needs discard first)
						case ETransitionType_Clear:

							pipelineStage =
								isDepthStencil ? D3D12_BARRIER_SYNC_DEPTH_STENCIL : D3D12_BARRIER_SYNC_RENDER_TARGET;

							access =
								isDepthStencil ? D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE : D3D12_BARRIER_ACCESS_RENDER_TARGET;

							layout =
								isDepthStencil ? D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE : D3D12_BARRIER_LAYOUT_RENDER_TARGET;

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

						case ETransitionType_Predicate:
							pipelineStage = D3D12_BARRIER_SYNC_PREDICATION;
							access = D3D12_BARRIER_ACCESS_PREDICATION;
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

					D3D12_BARRIER_SUBRESOURCE_RANGE range = (D3D12_BARRIER_SUBRESOURCE_RANGE) {        //TODO:
						.NumMipLevels = 1,
						.NumArraySlices = 1,
						.NumPlanes = 1
					};

					if(isDepthStencil && unif.depthFormat >= EDepthStencilFormat_StencilStart)
						++range.NumPlanes;

					gotoIfError3(nextTransition, DxUnifiedTexture_transition(

						imageExt,
						pipelineStage,
						access,
						layout,
						&range,

						&deviceExt->imageTransitions,
						&dep[0], alloc, e_rr));
				}

				else {

					DeviceBuffer *devBuffer = DeviceBufferRef_ptr(transition.resource);

					gotoIfError3(nextTransition, DxDeviceBuffer_transition(
						DeviceBuffer_ext(devBuffer, Dx),
						pipelineStage,
						access,
						&deviceExt->bufferTransitions,
						&dep[1], alloc, e_rr));
				}

			nextTransition:

				if(!s_uccess)
					Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
			}

			if(dep[0].NumBarriers || dep[1].NumBarriers)
				buffer->lpVtbl->Barrier(
					buffer,
					(Bool) dep[0].NumBarriers + (Bool) dep[1].NumBarriers,
					dep[0].NumBarriers ? &dep[0] : &dep[1]
				);

			ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions, e_rr);
			ListD3D12_TEXTURE_BARRIER_clear(&deviceExt->imageTransitions, e_rr);

			//AFTER the barriers, so the predicate write this scope waits on is visible; EQUAL_ZERO skips
			// while the U64 reads zero, the same polarity Vulkan's nonzero-runs convention lands on.

			if(temp->curScopeFlags & ECommandScopeInternalFlags_Predicated) {

				const CommandScopePredicate pred = *(const CommandScopePredicate*) data;

				buffer->lpVtbl->SetPredication(
					buffer,
					DeviceBuffer_ext(DeviceBufferRef_ptr(pred.buffer), Dx)->buffer,
					pred.offset, D3D12_PREDICATION_OP_EQUAL_ZERO
				);
			}

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
			const I32x4 v = I32x4_mul(I32x4_fromF32x4(colf), I32x4_create4(1 << 16, 1 << 8, 1 << 0, 0));

			const I32x2 reduc2 = I32x2_or(I32x4_xy(v), I32x4_zw(v));
			const I32 reduc = I32x2_x(reduc2) | I32x2_y(reduc2);

			U64 encoded[62] = { 0 };
			encoded[0] = (op == ECommandOp_AddMarkerDebugExt ? 0x008 : 0x002) << 10;
			encoded[1] = 0xFF000000 | (*(const U32*)&reduc);
			encoded[2] = ((U64)8 << 55) | ((U64)1 << 54);        //Address alignment = 8 and indicate "ansi"

			const U32 strLen = (U32) CharString_calcStrLen(
				(const C8*)data + sizeof(F32x4),
				sizeof(encoded) - sizeof(U64) * 3 - 1
			);

			const U32 len = (U32) sizeof(U64) * 3 + strLen;

			Buffer_memcpy(
				Buffer_createRef(&encoded[3], sizeof(encoded) - sizeof(U64) * 3),
				Buffer_createRefConst((const C8*)data + sizeof(F32x4), strLen)
			);

			if(op == ECommandOp_AddMarkerDebugExt)
				buffer->lpVtbl->SetMarker(buffer, 2, encoded, (len + 7) &~ 7);

			else buffer->lpVtbl->BeginEvent(buffer, 2, encoded, (len + 7) &~ 7);

			break;
		}

		//Emits nothing itself; the end timestamp write lands here in the measurement pass.

		case ECommandOp_StartTimingRegion:
		case ECommandOp_EndTimingRegion:
		case ECommandOp_InsertTiming:
			dxTimestampWrite(deviceExt, device->fifId, buffer);
			break;

		//The end timestamp and end debug region of a scope; the barriers ran at StartScope.

		case ECommandOp_EndScope:

			if(temp->curScopeFlags & ECommandScopeInternalFlags_Predicated)
				buffer->lpVtbl->SetPredication(buffer, NULL, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);

			if((temp->curScopeFlags & ECommandScopeInternalFlags_Timed))
				dxTimestampWrite(deviceExt, device->fifId, buffer);

			if((temp->curScopeFlags & ECommandScopeInternalFlags_DebugRegion))
				buffer->lpVtbl->EndEvent(buffer);

			break;

		//Unsupported

		default:
			Log_errorLnx("Unsupported command issued.");
			break;
	}
}
