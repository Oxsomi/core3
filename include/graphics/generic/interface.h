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
#include "types/base/types.h"
#include "types/base/lock.h"
#include "graphics/generic/instance.h"

typedef enum ECommandOp ECommandOp;
typedef enum EResourceType EResourceType;

typedef union ResourceRange ResourceRange;
typedef struct Sampler Sampler;
typedef struct BLAS BLAS;
typedef struct TLAS TLAS;
typedef struct Pipeline Pipeline;
typedef struct Swapchain Swapchain;
typedef struct CommandList CommandList;
typedef struct DeviceBuffer DeviceBuffer;
typedef struct DescriptorLayout DescriptorLayout;
typedef struct DescriptorTable DescriptorTable;
typedef struct DescriptorHeap DescriptorHeap;
typedef struct PipelineLayout PipelineLayout;
typedef struct GraphicsDevice GraphicsDevice;
typedef struct SHBinaryInfo SHBinaryInfo;
typedef struct DeviceMemoryAllocator DeviceMemoryAllocator;
typedef struct CBufferData CBufferData;
typedef struct Descriptor Descriptor;

typedef struct ListDescriptor ListDescriptor;
typedef struct ListSHFile ListSHFile;
typedef struct ListCommandListRef ListCommandListRef;
typedef struct ListSwapchainRef ListSwapchainRef;
typedef struct DeviceMemoryBlock DeviceMemoryBlock;

typedef struct RefPtr RefPtr;

typedef RefPtr BLASRef;
typedef RefPtr TLASRef;
typedef RefPtr TextureRef;
typedef RefPtr DeviceTextureRef;
typedef RefPtr DeviceBufferRef;
typedef RefPtr GraphicsDeviceRef;
typedef RefPtr SwapchainRef;
typedef RefPtr DescriptorLayoutRef;
typedef RefPtr DescriptorTableRef;
typedef RefPtr DescriptorHeapRef;
typedef RefPtr PipelineLayoutRef;

typedef struct GraphicsObjectSizes {
	U64 blas, tlas, pipeline, sampler, buffer, image, swapchain, device, instance;
	U64 descriptorLayout, descriptorTable, descriptorHeap, pipelineLayout;
} GraphicsObjectSizes;

//Dynamic linking will load the dlls to generate the function tables.
//Static linking should be preferred for better inlining / reduced code size.
//While dynamic linking should be preferred for debugging and easily switching from Vulkan to D3D12 for example.
//All functions except creating a graphics instance expect
// that GraphicsInterface_init was called before, undefined behavior otherwise.
//GraphicsInstance automatically calls GraphicsInterface_init which guards against multiple inits.

#ifndef GRAPHICS_API_DYNAMIC

	Bool GraphicsInterface_init(Error *e_rr);
	Bool GraphicsInterface_supports(EGraphicsApi api);
	impl const GraphicsObjectSizes *GraphicsInterface_getObjectSizes(EGraphicsApi api);

#else

	//RTAS

	typedef void (*BLAS_freeImpl)(BLAS *blas);
	typedef Error (*BLAS_initImpl)(BLAS *blas);
	typedef Error (*BLASRef_flushImpl)(void *commandBuffer, GraphicsDeviceRef *deviceRef, BLASRef *pending);

	typedef Bool (*TLAS_freeImpl)(TLAS *tlas);
	typedef Error (*TLAS_initImpl)(TLAS *tlas);
	typedef Error (*TLASRef_flushImpl)(void *commandBuffer, GraphicsDeviceRef *deviceRef, TLASRef *pending);

	//Pipelines

	typedef Bool (*GraphicsDevice_createPipelineComputeImpl)(
		GraphicsDevice *device,
		CharString name,
		Pipeline *pipeline,
		SHBinaryInfo buf,
		Error *e_rr
	);

	typedef Bool (*GraphicsDevice_createPipelineGraphicsImpl)(
		GraphicsDevice *dev,
		ListSHFile binaries,
		CharString name,
		Pipeline *pipeline,
		Error *e_rr
	);

	typedef Bool (*GraphicsDevice_createPipelineRaytracingImpl)(
		GraphicsDeviceRef *deviceRef,
		ListSHFile binaries,
		CharString name,
		U8 maxPayloadSize,
		U8 maxAttributeSize,
		ListU32 binaryIndices,
		Pipeline *pipeline,
		Error *e_rr
	);

	typedef void (*Pipeline_freeImpl)(Pipeline *pipeline, Allocator alloc);

	//Sampler

	typedef Error (*GraphicsDeviceRef_createSamplerImpl)(GraphicsDeviceRef *dev, Sampler *sampler, CharString name);
	typedef void (*Sampler_freeImpl)(Sampler *sampler);

	//Device buffer

	typedef Error (*GraphicsDeviceRef_createBufferImpl)(GraphicsDeviceRef *dev, DeviceBuffer *buf, CharString name);
	typedef void (*DeviceBuffer_freeImpl)(DeviceBuffer *buffer);
	typedef Error (*DeviceBufferRef_flushImpl)(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceBufferRef *pending);

	//Device texture

	typedef Error (*UnifiedTexture_createImpl)(TextureRef *textureRef, CharString name);
	typedef Error (*DeviceTextureRef_flushImpl)(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceTextureRef *pending);
	typedef void (*UnifiedTexture_freeImpl)(TextureRef *textureRef);

	//Swapchain

	typedef Error (*GraphicsDeviceRef_createSwapchainImpl)(GraphicsDeviceRef *dev, SwapchainRef *swapchain);
	typedef void (*Swapchain_freeImpl)(Swapchain *data, Allocator alloc);

	//DescriptorLayout

	typedef Error (*GraphicsDeviceRef_createDescriptorLayoutImpl)(GraphicsDeviceRef *dev, DescriptorLayout *layout, CharString name);
	typedef void (*DescriptorLayout_freeImpl)(DescriptorLayout *layout, Allocator alloc);

	//PipelineLayout

	typedef Error (*GraphicsDeviceRef_createPipelineLayoutImpl)(GraphicsDeviceRef *dev, PipelineLayout *layout, CharString name);
	typedef void (*PipelineLayout_freeImpl)(PipelineLayout *layout, Allocator alloc);

	//DescriptorTable
	
	typedef Error (*DescriptorHeap_createDescriptorTableImpl)(DescriptorHeapRef *heap, DescriptorTable *table, CharString name);
	typedef void (*DescriptorTable_freeImpl)(DescriptorTable *table, Allocator alloc);

	typedef Bool (*DescriptorTable_setDescriptorsImpl)(
		DescriptorTable *table,
		U64 bindId,
		U64 arrayId,
		ListDescriptor darr,
		Error *e_rr
	);

	typedef Bool (*DescriptorTable_unsetDescriptorsImpl)(
		DescriptorTable *table,
		U64 bindId,
		U64 arrayId,
		U64 count,
		Error *e_rr
	);

	//DescriptorHeap

	typedef Error (*GraphicsDeviceRef_createDescriptorHeapImpl)(GraphicsDeviceRef *dev, DescriptorHeap *heap, CharString name);
	typedef void (*DescriptorHeap_freeImpl)(DescriptorHeap *heap, Allocator alloc);

	//Allocator

	typedef Error (*DeviceMemoryAllocator_allocateImpl)(
		DeviceMemoryAllocator *allocator,
		void *requirementsExt,
		Bool cpuSided,
		U32 *blockId,
		U64 *blockOffset,
		EResourceType resourceType,
		CharString objectName,
		DeviceMemoryBlock *resultBlock
	);

	typedef Bool (*DeviceMemoryAllocator_freeAllocationImpl)(GraphicsDevice *device, void *ext);

	//Device

	typedef Error (*GraphicsDevice_initImpl)(
		const GraphicsInstance *instance,
		const GraphicsDeviceInfo *deviceInfo,
		GraphicsDeviceRef **deviceRef
	);

	typedef U64 (*GraphicsDevice_getMemoryBudgetImpl)(GraphicsDevice *device, Bool isDeviceLocal);

	typedef void (*GraphicsDevice_freeImpl)(const GraphicsInstance *instance, void *ext);

	typedef Error (*GraphicsDeviceRef_waitImpl)(GraphicsDeviceRef *deviceRef);

	typedef Error (*GraphicsDevice_submitCommandsImpl)(
		GraphicsDeviceRef *deviceRef,
		ListCommandListRef commandLists,
		ListSwapchainRef swapchains,
		CBufferData data
	);

	typedef void (*CommandList_processImpl)(
		CommandList *commandList,
		GraphicsDeviceRef *deviceRef,
		ECommandOp op,
		const U8 *data,
		void *commandListExt
	);

	//Instance

	typedef Error (*GraphicsInstance_createImpl)(GraphicsApplicationInfo info, GraphicsInstanceRef **instanceRef);
	typedef void (*GraphicsInstance_freeImpl)(GraphicsInstance *inst, Allocator alloc);
	typedef Error (*GraphicsInstance_getDeviceInfosImpl)(const GraphicsInstance *inst, ListGraphicsDeviceInfo *infos);

	//Glue to define what a graphics interface requires

	typedef struct GraphicsInterfaceTable {

		EGraphicsApi									api;
		U32												padding;

		GraphicsObjectSizes								objectSizes;

		BLAS_initImpl									blasInit;
		BLASRef_flushImpl								blasFlush;
		BLAS_freeImpl									blasFree;

		TLAS_initImpl									tlasInit;
		TLASRef_flushImpl								tlasFlush;
		TLAS_freeImpl									tlasFree;

		GraphicsDevice_createPipelineGraphicsImpl		pipelineCreateGraphics;
		GraphicsDevice_createPipelineComputeImpl		pipelineCreateCompute;
		GraphicsDevice_createPipelineRaytracingImpl		pipelineCreateRt;
		Pipeline_freeImpl								pipelineFree;

		GraphicsDeviceRef_createSamplerImpl				samplerCreate;
		Sampler_freeImpl								samplerFree;

		GraphicsDeviceRef_createBufferImpl				bufferCreate;
		DeviceBufferRef_flushImpl						bufferFlush;
		DeviceBuffer_freeImpl							bufferFree;

		UnifiedTexture_createImpl						textureCreate;
		DeviceTextureRef_flushImpl						textureFlush;
		UnifiedTexture_freeImpl							textureFree;

		GraphicsDeviceRef_createSwapchainImpl			swapchainCreate;
		Swapchain_freeImpl								swapchainFree;

		GraphicsDeviceRef_createDescriptorLayoutImpl	descriptorLayoutCreate;
		DescriptorLayout_freeImpl						descriptorLayoutFree;

		GraphicsDeviceRef_createPipelineLayoutImpl		pipelineLayoutCreate;
		PipelineLayout_freeImpl							pipelineLayoutFree;

		DescriptorHeap_createDescriptorTableImpl		descriptorTableCreate;
		DescriptorTable_freeImpl						descriptorTableFree;
		DescriptorTable_setDescriptorsImpl				descriptorTableSet;
		DescriptorTable_unsetDescriptorsImpl			descriptorTableUnset;

		GraphicsDeviceRef_createDescriptorHeapImpl		descriptorHeapCreate;
		DescriptorHeap_freeImpl							descriptorHeapFree;

		DeviceMemoryAllocator_allocateImpl				memoryAllocate;
		DeviceMemoryAllocator_freeAllocationImpl		memoryFree;

		GraphicsDevice_initImpl							deviceInit;
		GraphicsDeviceRef_waitImpl						deviceWait;
		GraphicsDevice_freeImpl							deviceFree;
		GraphicsDevice_submitCommandsImpl				deviceSubmitCommands;
		GraphicsDevice_getMemoryBudgetImpl				deviceGetMemoryBudget;

		CommandList_processImpl							commandListProcess;

		GraphicsInstance_createImpl						instanceCreate;
		GraphicsInstance_freeImpl						instanceFree;
		GraphicsInstance_getDeviceInfosImpl				instanceGetDevices;

	} GraphicsInterfaceTable;

	typedef struct Platform Platform;
	typedef struct GraphicsInterface GraphicsInterface;

	typedef GraphicsInterfaceTable (*GraphicsInterface_getTableImpl)(Platform *instance, GraphicsInterface *interf);

	typedef struct GraphicsInterface {
		GraphicsInterfaceTable tables[EGraphicsApi_Count];
	} GraphicsInterface;

	extern GraphicsInterface *GraphicsInterface_instance;

	Bool GraphicsInterface_init(Error *e_rr);
	Bool GraphicsInterface_supports(EGraphicsApi api);

	const GraphicsObjectSizes *GraphicsInterface_getObjectSizes(EGraphicsApi api);

#endif

const GraphicsObjectSizes *GraphicsDeviceRef_getObjectSizes(GraphicsDeviceRef *device);

//These have two different meanings depending on how OxC3 graphics is linked:
//Dynamic linking: These are just wrappers that call the graphics table directly.
//Static linking:  These are direct callers to the implementation.

//RTAS

void BLAS_freeExt(BLAS *blas);
Error BLAS_initExt(BLAS *blas);
Error BLASRef_flushExt(void *commandBuffer, GraphicsDeviceRef *deviceRef, BLASRef *pending);

Bool TLAS_freeExt(TLAS *tlas);
Error TLAS_initExt(TLAS *tlas);
Error TLASRef_flushExt(void *commandBuffer, GraphicsDeviceRef *deviceRef, TLASRef *pending);

//Pipelines

Bool GraphicsDevice_createPipelineComputeExt(
	GraphicsDevice *device,
	CharString name,
	Pipeline *pipeline,
	SHBinaryInfo buf,
	Error *e_rr
);

Bool GraphicsDevice_createPipelineGraphicsExt(
	GraphicsDevice *dev,
	ListSHFile binaries,
	CharString name,
	Pipeline *pipeline,
	Error *e_rr
);

Bool GraphicsDevice_createPipelineRaytracingInternalExt(
	GraphicsDeviceRef *deviceRef,
	ListSHFile binaries,
	CharString name,
	U8 maxPayloadSize,
	U8 maxAttributeSize,
	ListU32 binaryIndices,
	Pipeline *pipeline,
	Error *e_rr
);

void Pipeline_freeExt(Pipeline *pipeline, Allocator alloc);

//Sampler

Error GraphicsDeviceRef_createSamplerExt(GraphicsDeviceRef *dev, Sampler *sampler, CharString name);
void Sampler_freeExt(Sampler *sampler);

//Device buffer

Error GraphicsDeviceRef_createBufferExt(GraphicsDeviceRef *dev, DeviceBuffer *buf, CharString name);
void DeviceBuffer_freeExt(DeviceBuffer *buffer);
Error DeviceBufferRef_flushExt(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceBufferRef *pending);

//Device texture

Error UnifiedTexture_createExt(TextureRef *textureRef, CharString name);
Error DeviceTextureRef_flushExt(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceTextureRef *pending);
void UnifiedTexture_freeExt(TextureRef *textureRef);

//Swapchain

Error GraphicsDeviceRef_createSwapchainExt(GraphicsDeviceRef *dev, SwapchainRef *swapchain);
void Swapchain_freeExt(Swapchain *data, Allocator alloc);

//DescriptorLayout

Error GraphicsDeviceRef_createDescriptorLayoutExt(GraphicsDeviceRef *dev, DescriptorLayout *layout, CharString name);
void DescriptorLayout_freeExt(DescriptorLayout *layout, Allocator alloc);

//PipelineLayout

Error GraphicsDeviceRef_createPipelineLayoutExt(GraphicsDeviceRef *dev, PipelineLayout *layout, CharString name);
void PipelineLayout_freeExt(PipelineLayout *layout, Allocator alloc);

//DescriptorTable

Error DescriptorHeap_createDescriptorTableExt(DescriptorHeapRef *heap, DescriptorTable *table, CharString name);
void DescriptorTable_freeExt(DescriptorTable *table, Allocator alloc);

Bool DescriptorTable_setDescriptorsExt(
	DescriptorTable *table,
	U64 bindingId,
	U64 arrayId,
	ListDescriptor darr,
	Error *e_rr
);

Bool DescriptorTable_unsetDescriptorsExt(
	DescriptorTable *table,
	U64 bindingId,
	U64 arrayId,
	U64 count,
	Error *e_rr
);

//DescriptorHeap

Error GraphicsDeviceRef_createDescriptorHeapExt(GraphicsDeviceRef *dev, DescriptorHeap *heap, CharString name);
void DescriptorHeap_freeExt(DescriptorHeap *heap, Allocator alloc);

//Allocator

//Needs explicit lock, because allocator is accessed after.
Error DeviceMemoryAllocator_allocateExt(
	DeviceMemoryAllocator *allocator,
	void *requirementsExt,
	Bool cpuSided,
	U32 *blockId,
	U64 *blockOffset,
	EResourceType resourceType,
	CharString objectName,						//Name of the object that allocates (for dedicated allocations)
	DeviceMemoryBlock *resultBlock
);

Bool DeviceMemoryAllocator_freeAllocationExt(GraphicsDevice *device, void *ext);

//Device

Error GraphicsDevice_initExt(
	const GraphicsInstance *instance,
	const GraphicsDeviceInfo *deviceInfo,
	GraphicsDeviceRef **deviceRef
);

U64 GraphicsDevice_getMemoryBudgetExt(GraphicsDevice *device, Bool isDeviceLocal);

void GraphicsDevice_freeExt(const GraphicsInstance *instance, void *ext);

Error GraphicsDeviceRef_waitExt(GraphicsDeviceRef *deviceRef);

Error GraphicsDevice_submitCommandsExt(
	GraphicsDeviceRef *deviceRef,
	ListCommandListRef commandLists,
	ListSwapchainRef swapchains,
	CBufferData data
);

void CommandList_processExt(
	CommandList *commandList,
	GraphicsDeviceRef *deviceRef,
	ECommandOp op,
	const U8 *data,
	void *commandListExt
);

//Instance

Error GraphicsInstance_createExt(GraphicsApplicationInfo info, GraphicsInstanceRef **instanceRef);
void GraphicsInstance_freeExt(GraphicsInstance *inst, Allocator alloc);
Error GraphicsInstance_getDeviceInfosExt(const GraphicsInstance *inst, ListGraphicsDeviceInfo *infos);
