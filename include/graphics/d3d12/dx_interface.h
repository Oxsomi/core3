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

#pragma once
#ifdef GRAPHICS_API_DYNAMIC

	Bool  D3D12BLAS_free(BLAS *blas);
	Error D3D12BLAS_init(BLAS *blas);
	Error D3D12BLASRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, BLASRef *pending);

	Bool  D3D12TLAS_free(TLAS *tlas);
	Error D3D12TLAS_init(TLAS *tlas);
	Error D3D12TLASRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, TLASRef *pending);

	//Pipeline

	Bool D3D12GraphicsDevice_createPipelineCompute(
		GraphicsDevice *device,
		CharString name,
		Pipeline *pipeline,
		SHBinaryInfo buf,
		Error *e_rr
	);

	Bool D3D12GraphicsDevice_createPipelineGraphics(
		GraphicsDevice *dev,
		ListSHFile binaries,
		CharString name,
		Pipeline *pipeline,
		Error *e_rr
	);

	Bool D3D12GraphicsDevice_createPipelineRaytracingInternal(
		GraphicsDeviceRef *deviceRef,
		ListSHFile binaries,
		CharString name,
		U8 maxPayloadSize,
		U8 maxAttributeSize,
		ListU32 binaryIndices,
		Pipeline *pipeline,
		Error *e_rr
	);

	Bool D3D12Pipeline_free(Pipeline *pipeline, Allocator alloc);

	//Sampler

	Error D3D12GraphicsDeviceRef_createSampler(GraphicsDeviceRef *dev, Sampler *sampler, CharString name);
	Bool  D3D12Sampler_free(Sampler *sampler);

	//Device buffer

	Error D3D12GraphicsDeviceRef_createBuffer(GraphicsDeviceRef *dev, DeviceBuffer *buf, CharString name);
	Bool  D3D12DeviceBuffer_free(DeviceBuffer *buffer);
	Error D3D12DeviceBufferRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceBufferRef *pending);

	//Device texture

	Error D3D12UnifiedTexture_create(TextureRef *textureRef, CharString name);
	Error D3D12DeviceTextureRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceTextureRef *pending);
	Bool  D3D12UnifiedTexture_free(TextureRef *textureRef);

	//Swapchain

	Error D3D12GraphicsDeviceRef_createSwapchain(GraphicsDeviceRef *dev, SwapchainRef *swapchain);
	Bool  D3D12Swapchain_free(Swapchain *data, Allocator alloc);

	//Allocator

	//Needs explicit lock, because allocator is accessed after.
	Error D3D12DeviceMemoryAllocator_allocate(
		DeviceMemoryAllocator *allocator,
		void *requirementsExt,
		Bool cpuSided,
		U32 *blockId,
		U64 *blockOffset,
		EResourceType resourceType,
		CharString objectName,				//Name of the object that allocates (for dedicated allocations)
		DeviceMemoryBlock *resultBlock
	);

	Bool D3D12DeviceMemoryAllocator_freeAllocation(GraphicsDevice *device, void *ext);

	//Device

	Error D3D12GraphicsDevice_init(
		const GraphicsInstance *instance,
		const GraphicsDeviceInfo *deviceInfo,
		GraphicsDeviceRef **deviceRef
	);

	void D3D12GraphicsDevice_postInit(GraphicsDevice *device);
	U64 D3D12GraphicsDevice_getMemoryBudget(GraphicsDevice *device, Bool isDeviceLocal);

	Bool D3D12GraphicsDevice_free(const GraphicsInstance *instance, void *ext);

	Error D3D12GraphicsDeviceRef_wait(GraphicsDeviceRef *deviceRef);

	Error D3D12GraphicsDevice_submitCommands(
		GraphicsDeviceRef *deviceRef,
		ListCommandListRef commandLists,
		ListSwapchainRef swapchains,
		CBufferData data
	);

	void D3D12CommandList_process(
		CommandList *commandList,
		GraphicsDeviceRef *deviceRef,
		ECommandOp op,
		const U8 *data,
		void *commandListExt
	);

	//Interface

	Error D3D12GraphicsInstance_create(GraphicsApplicationInfo info, GraphicsInstanceRef **instanceRef);
	Bool  D3D12GraphicsInstance_free(GraphicsInstance *inst, Allocator alloc);
	Error D3D12GraphicsInstance_getDeviceInfos(const GraphicsInstance *inst, ListGraphicsDeviceInfo *infos);

#endif
