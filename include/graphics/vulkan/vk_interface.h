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

	Bool  VkBLAS_free(BLAS *blas);
	Error VkBLAS_init(BLAS *blas);
	Error VkBLASRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, BLASRef *pending);

	Bool  VkTLAS_free(TLAS *tlas);
	Error VkTLAS_init(TLAS *tlas);
	Error VkTLASRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, TLASRef *pending);

	//Pipeline

	Bool VkGraphicsDevice_createPipelineCompute(
		GraphicsDevice *device,
		CharString name,
		Pipeline *pipeline,
		SHBinaryInfo buf,
		Error *e_rr
	);

	Bool VkGraphicsDevice_createPipelineGraphics(
		GraphicsDevice *dev,
		ListSHFile binaries,
		CharString name,
		Pipeline *pipeline,
		Error *e_rr
	);

	Bool VkGraphicsDevice_createPipelineRaytracingInternal(
		GraphicsDeviceRef *deviceRef,
		ListSHFile binaries,
		CharString name,
		U8 maxPayloadSize,
		U8 maxAttributeSize,
		ListU32 binaryIndices,
		Pipeline *pipeline,
		Error *e_rr
	);

	Bool VkPipeline_free(Pipeline *pipeline, Allocator alloc);

	//Sampler

	Error VkGraphicsDeviceRef_createSampler(GraphicsDeviceRef *dev, Sampler *sampler, CharString name);
	Bool  VkSampler_free(Sampler *sampler);

	//Device buffer

	Error VkGraphicsDeviceRef_createBuffer(GraphicsDeviceRef *dev, DeviceBuffer *buf, CharString name);
	Bool  VkDeviceBuffer_free(DeviceBuffer *buffer);
	Error VkDeviceBufferRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceBufferRef *pending);

	//Device texture

	Error VkUnifiedTexture_create(TextureRef *textureRef, CharString name);
	Error VkDeviceTextureRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceTextureRef *pending);
	Bool  VkUnifiedTexture_free(TextureRef *textureRef);

	//Swapchain

	Error VkGraphicsDeviceRef_createSwapchain(GraphicsDeviceRef *dev, SwapchainRef *swapchain);
	Bool  VkSwapchain_free(Swapchain *data, Allocator alloc);

	//DescriptorHeap

	Error VkGraphicsDeviceRef_createDescriptorHeap(GraphicsDeviceRef *dev, DescriptorHeap *heap, CharString name);
	Bool VkDescriptorHeap_free(DescriptorHeap *heap, Allocator alloc);

	//DescriptorLayout

	Error VkGraphicsDeviceRef_createDescriptorLayout(GraphicsDeviceRef *dev, DescriptorLayout *layout, CharString name);
	Bool VkDescriptorLayout_free(DescriptorLayout *layout, Allocator alloc);

	//Allocator

	//Needs explicit lock, because allocator is accessed after.
	Error VkDeviceMemoryAllocator_allocate(
		DeviceMemoryAllocator *allocator,
		void *requirementsExt,
		Bool cpuSided,
		U32 *blockId,
		U64 *blockOffset,
		EResourceType resourceType,
		CharString objectName,				//Name of the object that allocates (for dedicated allocations)
		DeviceMemoryBlock *resultBlock
	);

	Bool VkDeviceMemoryAllocator_freeAllocation(GraphicsDevice *device, void *ext);

	//Device

	Error VkGraphicsDevice_init(
		const GraphicsInstance *instance,
		const GraphicsDeviceInfo *deviceInfo,
		GraphicsDeviceRef **deviceRef
	);

	void VkGraphicsDevice_postInit(GraphicsDevice *device);
	U64 VkGraphicsDevice_getMemoryBudget(GraphicsDevice *device, Bool isDeviceLocal);

	Bool VkGraphicsDevice_free(const GraphicsInstance *instance, void *ext);

	Error VkGraphicsDeviceRef_wait(GraphicsDeviceRef *deviceRef);

	Error VkGraphicsDevice_submitCommands(
		GraphicsDeviceRef *deviceRef,
		ListCommandListRef commandLists,
		ListSwapchainRef swapchains,
		CBufferData data
	);

	void VkCommandList_process(
		CommandList *commandList,
		GraphicsDeviceRef *deviceRef,
		ECommandOp op,
		const U8 *data,
		void *commandListExt
	);

	//Interface

	Error VkGraphicsInstance_create(GraphicsApplicationInfo info, GraphicsInstanceRef **instanceRef);
	Bool  VkGraphicsInstance_free(GraphicsInstance *inst, Allocator alloc);
	Error VkGraphicsInstance_getDeviceInfos(const GraphicsInstance *inst, ListGraphicsDeviceInfo *infos);

#endif
