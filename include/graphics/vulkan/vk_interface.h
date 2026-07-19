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

//graphics/vulkan/vk_interface.h

#pragma once
#ifdef GRAPHICS_API_DYNAMIC

	//Needs the generic graphics type forward declarations (BLAS, TLAS, Pipeline, ...);
	//included here so this header is order-independent for its includers.

	#include "graphics/generic/interface.h"

	void  VkBLAS_free(BLAS *blas);
	Bool VkBLAS_init(BLAS *blas, Error *e_rr);
	Bool VkBLASRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, BLASRef *pending, Error *e_rr);

	void  VkTLAS_free(TLAS *tlas);
	Bool VkTLAS_init(TLAS *tlas, Error *e_rr);
	Bool VkTLASRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, TLASRef *pending, Error *e_rr);

	//Pipeline

	Bool VkGraphicsDevice_createPipelineCompute(
		GraphicsDevice *device,
		const CharString *name,
		Pipeline *pipeline,
		const SHBinaryInfo *buf,
		Error *e_rr
	);

	Bool VkGraphicsDevice_createPipelineGraphics(
		GraphicsDevice *dev,
		const ListSHFile *binaries,
		const CharString *name,
		Pipeline *pipeline,
		Error *e_rr
	);

	Bool VkGraphicsDevice_createPipelineRaytracingInternal(
		GraphicsDeviceRef *deviceRef,
		const ListSHFile *binaries,
		const CharString *name,
		U8 maxPayloadSize,
		U8 maxAttributeSize,
		const ListU32 *binaryIndices,
		Pipeline *pipeline,
		Error *e_rr
	);

	void VkPipeline_free(Pipeline *pipeline, const Allocator *alloc);

	//Sampler

	Bool VkGraphicsDeviceRef_createSampler(GraphicsDeviceRef *dev, Sampler *sampler, const CharString *name, Error *e_rr);
	void  VkSampler_free(Sampler *sampler);

	//Device buffer

	Bool VkGraphicsDeviceRef_createBuffer(GraphicsDeviceRef *dev, DeviceBuffer *buf, const CharString *name, Error *e_rr);
	void  VkDeviceBuffer_free(DeviceBuffer *buffer);
	Bool VkDeviceBufferRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceBufferRef *pending, Error *e_rr);

	//Device texture

	Bool VkUnifiedTexture_create(TextureRef *textureRef, const CharString *name, Error *e_rr);
	Bool VkDeviceTextureRef_flush(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceTextureRef *pending, Error *e_rr);
	void  VkUnifiedTexture_free(TextureRef *textureRef);

	//Swapchain

	Bool VkGraphicsDeviceRef_createSwapchain(GraphicsDeviceRef *dev, SwapchainRef *swapchain, Error *e_rr);
	void  VkSwapchain_free(Swapchain *data, const Allocator *alloc);

	//DescriptorHeap

	Bool VkGraphicsDeviceRef_createDescriptorHeap(
		GraphicsDeviceRef *dev, DescriptorHeap *heap, const CharString *name, Error *e_rr
	);

	void VkDescriptorHeap_free(DescriptorHeap *heap, const Allocator *alloc);

	//DescriptorTable

	Bool VkDescriptorHeap_createDescriptorTable(
		DescriptorHeapRef *heap, DescriptorTable *table, const CharString *name, Error *e_rr
	);

	void VkDescriptorTable_free(DescriptorTable *table, const Allocator *alloc);

	Bool VkDescriptorTable_setDescriptors(
		DescriptorTable *table,
		U64 bindId,
		U64 arrayId,
		const ListDescriptor *darr,
		Error *e_rr
	);

	Bool VkDescriptorTable_unsetDescriptors(
		DescriptorTable *table,
		U64 bindId,
		U64 arrayId,
		U64 count,
		Error *e_rr
	);

	//DescriptorLayout

	Bool VkGraphicsDeviceRef_createDescriptorLayout(
		GraphicsDeviceRef *dev,
		DescriptorLayout *layout,
		const CharString *name,
		Error *e_rr
	);

	void VkDescriptorLayout_free(DescriptorLayout *layout, const Allocator *alloc);

	//PipelineLayout

	Bool VkGraphicsDeviceRef_createPipelineLayout(
		GraphicsDeviceRef *dev, PipelineLayout *layout, const CharString *name, Error *e_rr
	);

	void VkPipelineLayout_free(PipelineLayout *layout, const Allocator *alloc);

	//Allocator

	//Needs explicit lock, because allocator is accessed after.
	Bool VkDeviceMemoryAllocator_allocate(
		DeviceMemoryAllocator *allocator,
		void *requirementsExt,
		Bool cpuSided,
		U32 *blockId,
		U64 *blockOffset,
		EResourceType resourceType,
		const CharString *objectName,          //Name of the object that allocates (for dedicated allocations)
		DeviceMemoryBlock *resultBlock,
		Error *e_rr
	);

	Bool VkDeviceMemoryAllocator_freeAllocation(GraphicsDevice *device, void *ext);

	//Device

	Bool VkGraphicsDevice_init(
		const GraphicsInstance *instance,
		const GraphicsDeviceInfo *deviceInfo,
		GraphicsDeviceRef **deviceRef,
		Error *e_rr
	);

	U64 VkGraphicsDevice_getMemoryBudget(GraphicsDevice *device, Bool isDeviceLocal);

	void VkGraphicsDevice_free(const GraphicsInstance *instance, void *ext);

	Bool VkGraphicsDeviceRef_wait(GraphicsDeviceRef *deviceRef, Error *e_rr);

	Bool VkGraphicsDevice_submitCommands(
		GraphicsDeviceRef *deviceRef,
		const ListCommandListRef *commandLists,
		const ListSwapchainRef *swapchains,
		CBufferData *data,
		Error *e_rr
	);

	void VkCommandList_process(
		CommandList *commandList,
		GraphicsDeviceRef *deviceRef,
		ECommandOp op,
		const U8 *data,
		void *commandListExt
	);

	//Interface

	Bool VkGraphicsInstance_create(const GraphicsApplicationInfo *info, GraphicsInstanceRef **instanceRef, Error *e_rr);
	void  VkGraphicsInstance_free(GraphicsInstance *inst, const Allocator *alloc);
	Bool VkGraphicsInstance_getDeviceInfos(const GraphicsInstance *inst, ListGraphicsDeviceInfo *infos, Error *e_rr);

#endif
