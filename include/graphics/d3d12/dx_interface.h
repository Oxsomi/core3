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

//graphics/d3d12/dx_interface.h

#pragma once
#ifdef GRAPHICS_API_DYNAMIC

	//Needs the generic graphics type forward declarations (BLAS, TLAS, Pipeline, ...);
	//included here so this header is order-independent for its includers.

	#include "graphics/generic/interface.h"

	void  D3D12BLAS_free(BLAS *blas);
	Bool D3D12BLAS_init(BLAS *blas, Error *e_rr);
	Bool D3D12BLASRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, BLASRef *pending, Error *e_rr);

	void  D3D12OpacityMicromap_free(OpacityMicromap *micromap);
	Bool D3D12OpacityMicromap_init(OpacityMicromap *micromap, Error *e_rr);

	Bool D3D12OpacityMicromapRef_flush(
		void *commandBuffer, GraphicsDeviceRef *deviceRef, OpacityMicromapRef *pending, Error *e_rr
	);

	void  D3D12TLAS_free(TLAS *tlas);
	Bool D3D12TLAS_init(TLAS *tlas, Error *e_rr);
	Bool D3D12TLASRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, TLASRef *pending, Error *e_rr);

	//Pipeline

	Bool D3D12GraphicsDevice_createPipelineCompute(
		GraphicsDevice *device,
		const CharString *name,
		const CharString *entryName,
		Pipeline *pipeline,
		const SHBinaryInfo *buf,
		Error *e_rr
	);

	Bool D3D12GraphicsDevice_createPipelineGraphics(
		GraphicsDevice *dev,
		const ListSHFile *binaries,
		const CharString *name,
		Pipeline *pipeline,
		Error *e_rr
	);

	Bool D3D12GraphicsDevice_createPipelineRaytracingInternal(
		GraphicsDeviceRef *deviceRef,
		const ListSHFile *binaries,
		const CharString *name,
		U8 maxPayloadSize,
		U8 maxAttributeSize,
		const ListU32 *binaryIndices,
		Pipeline *pipeline,
		Error *e_rr
	);

	void D3D12Pipeline_free(Pipeline *pipeline, const Allocator *alloc);

	Bool D3D12Pipeline_getExecutables(
		Pipeline *pipeline, const Allocator *alloc, ListPipelineExecutable *result, Error *e_rr
	);

	Bool D3D12GraphicsDeviceRef_listShaderTargets(
		GraphicsDeviceRef *deviceRef, const Allocator *alloc, ListCharString *result, Error *e_rr
	);

	//Sampler

	Bool D3D12GraphicsDeviceRef_createSampler(GraphicsDeviceRef *dev, Sampler *sampler, const CharString *name, Error *e_rr);
	void  D3D12Sampler_free(Sampler *sampler);

	//Device buffer

	Bool D3D12GraphicsDeviceRef_createBuffer(GraphicsDeviceRef *dev, DeviceBuffer *buf, const CharString *name, Error *e_rr);
	void  D3D12DeviceBuffer_free(DeviceBuffer *buffer);
	Bool D3D12DeviceBufferRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceBufferRef *pending, Error *e_rr);

	Bool D3D12DeviceBufferRef_pull(
		void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceBufferRef *resource,
		U64 offset, U64 len, U64 stagingOffset, Error *e_rr
	);

	//Device texture

	Bool D3D12UnifiedTexture_create(TextureRef *textureRef, const CharString *name, Error *e_rr);
	Bool D3D12DeviceTextureRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceTextureRef *pending, Error *e_rr);

	Bool D3D12DeviceTextureRef_pull(
		void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceTextureRef *resource,
		const TextureRange *range, U64 stagingOffset, U64 *rowPitch, Error *e_rr
	);
	void  D3D12UnifiedTexture_free(TextureRef *textureRef);

	//Swapchain

	Bool D3D12GraphicsDeviceRef_createSwapchain(GraphicsDeviceRef *dev, SwapchainRef *swapchain, Error *e_rr);
	void  D3D12Swapchain_free(Swapchain *data, const Allocator *alloc);

	//DescriptorHeap

	Bool D3D12GraphicsDeviceRef_createDescriptorHeap(
		GraphicsDeviceRef *dev,
		DescriptorHeap *heap,
		const CharString *name,
		Error *e_rr
	);

	void D3D12DescriptorHeap_free(DescriptorHeap *heap, const Allocator *alloc);

	//DescriptorTable

	Bool D3D12DescriptorHeap_createDescriptorTable(
		DescriptorHeapRef *heap,
		DescriptorTable *table,
		const CharString *name,
		Error *e_rr
	);
	void D3D12DescriptorTable_free(DescriptorTable *table, const Allocator *alloc);

	Bool D3D12DescriptorTable_setDescriptors(
		DescriptorTable *table,
		U64 bindId,
		U64 arrayId,
		const ListDescriptor *darr,
		Error *e_rr
	);

	Bool D3D12DescriptorTable_unsetDescriptors(
		DescriptorTable *table,
		U64 bindId,
		U64 arrayId,
		U64 count,
		Error *e_rr
	);

	//DescriptorLayout

	Bool D3D12GraphicsDeviceRef_createDescriptorLayout(
		GraphicsDeviceRef *dev,
		DescriptorLayout *layout,
		const CharString *name,
		Error *e_rr
	);
	void D3D12DescriptorLayout_free(DescriptorLayout *layout, const Allocator *alloc);

	//PipelineLayout

	Bool D3D12GraphicsDeviceRef_createPipelineLayout(
		GraphicsDeviceRef *dev,
		PipelineLayout *layout,
		const CharString *name,
		Error *e_rr
	);
	void D3D12PipelineLayout_free(PipelineLayout *layout, const Allocator *alloc);

	//Allocator

	//Needs explicit lock, because allocator is accessed after.
	Bool D3D12DeviceMemoryAllocator_allocate(
		DeviceMemoryAllocator *allocator,
		void *requirementsExt,
		Bool cpuSided,
		U32 *blockId,
		U64 *blockOffset,
		EResourceType resourceType,
		const CharString *objectName,            //Name of the object that allocates (for dedicated allocations)
		DeviceMemoryBlock *resultBlock,
		Error *e_rr
	);

	Bool D3D12DeviceMemoryAllocator_freeAllocation(GraphicsDevice *device, void *ext);

	//Device

	Bool D3D12GraphicsDevice_init(
		const GraphicsInstance *instance,
		const GraphicsDeviceInfo *deviceInfo,
		GraphicsDeviceRef **deviceRef,
		Error *e_rr
	);

	U64 D3D12GraphicsDevice_getMemoryBudget(GraphicsDevice *device, Bool isDeviceLocal);

	void D3D12GraphicsDevice_free(const GraphicsInstance *instance, void *ext);

	Bool D3D12GraphicsDeviceRef_wait(GraphicsDeviceRef *deviceRef, Error *e_rr);

	Bool D3D12GraphicsDevice_submitCommands(
		GraphicsDeviceRef *deviceRef,
		const ListCommandListRef *commandLists,
		const ListSwapchainRef *swapchains,
		CBufferData *data,
		Error *e_rr
	);

	void D3D12CommandList_process(
		CommandList *commandList,
		GraphicsDeviceRef *deviceRef,
		ECommandOp op,
		const U8 *data,
		void *commandListExt
	);

	//Interface

	Bool D3D12GraphicsInstance_create(const GraphicsApplicationInfo *info, GraphicsInstanceRef **instanceRef, Error *e_rr);
	void  D3D12GraphicsInstance_free(GraphicsInstance *inst, const Allocator *alloc);
	Bool D3D12GraphicsInstance_getDeviceInfos(const GraphicsInstance *inst, ListGraphicsDeviceInfo *infos, Error *e_rr);

#endif
