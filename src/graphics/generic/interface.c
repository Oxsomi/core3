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

//graphics/generic/interface.c

#ifdef GRAPHICS_API_DYNAMIC
	#include "formats/oiSH/sh_file.h"
	#include "graphics/generic/blas.h"
	#include "graphics/generic/tlas.h"
	#include "graphics/generic/pipeline.h"
	#include "graphics/generic/sampler.h"
	#include "graphics/generic/swapchain.h"
	#include "graphics/generic/descriptor_heap.h"
	#include "graphics/generic/descriptor_layout.h"
	#include "graphics/generic/descriptor_table.h"
	#include "graphics/generic/pipeline_layout.h"
	#include "graphics/generic/command_list.h"
	#include "platforms/file.h"
	#include "platforms/logx.h"
	#include "platforms/dynamic_library.h"
	#include "platforms/platform.h"
#endif

#include "graphics/generic/interface.h"
#include "graphics/generic/device.h"

const GraphicsObjectSizes *GraphicsDeviceRef_getObjectSizes(GraphicsDeviceRef *deviceRef) {

	if(!deviceRef || deviceRef->refPtrType->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return NULL;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	return GraphicsInterface_getObjectSizes(GraphicsInstanceRef_ptr(device->instance)->api);
}

#ifdef GRAPHICS_API_DYNAMIC

	GraphicsInterface *GraphicsInterface_instance = 0, graphicsInterfaceInstance = { 0 };

	Bool GraphicsInterface_register(const FileInfo *info, void *dat, const Allocator *alloc, Error *e_rr) {

		Bool s_uccess = true;

		(void) dat;
		(void) alloc;

		if(info->type != EFileType_File)
			return true;

		if(!DynamicLibrary_isValidPath(info->path))
			return true;

		DynamicLibrary library = (DynamicLibrary) { 0 };
		gotoIfError3(clean, DynamicLibrary_load(info->path, true, &library, e_rr));

		void *tableFunc = NULL;
		if(!DynamicLibrary_loadSymbol(library, CharString_createRefCStrConst("GraphicsInterface_getTable"), &tableFunc, NULL))
			goto clean;

		GraphicsInterfaceTable table = ((GraphicsInterface_getTableImpl)tableFunc)(
			Platform_instance, GraphicsInterface_instance
		);

		if (table.api >= EGraphicsApi_Count) {
			Log_warnLnx("GraphicsInterface_register() found \"%s\" but was unable to initialize it", info->path.ptr);
			goto clean;
		}

		if(GraphicsInterface_instance->tables[table.api].instanceCreate)
			retError(clean, Error_invalidState(0, "GraphicsInterface_register() dynamic library was already initialized"));

		GraphicsInterface_instance->tables[table.api] = table;
		Log_debugLnx("-- Dynamically loaded %s", info->path.ptr);

	clean:
		if(!s_uccess)
			DynamicLibrary_free(library);

		return s_uccess;
	}

	Bool GraphicsInterface_init(Error *e_rr) {

		Bool s_uccess = true;

		if(GraphicsInterface_instance)        //Already initialized
			goto clean;

		GraphicsInterface_instance = &graphicsInterfaceInstance;

		gotoIfError3(clean, File_foreach(
			&Platform_instance->appDirectory, true, GraphicsInterface_register, NULL, false,
			Platform_instance->alloc, e_rr
		));

	clean:
		return s_uccess;
	}

	Bool GraphicsInterface_supports(EGraphicsApi api) {
		return
			api <= EGraphicsApi_Count && GraphicsInterface_instance &&
			GraphicsInterface_instance->tables[api].instanceCreate;
	}

	//These are kept for ease of use, these are just a wrapper for the interface

	#define WrapperFunction(device, func) \
		((GraphicsInterface_instance->tables[GraphicsInstanceRef_ptr(GraphicsDeviceRef_ptr(device)->instance)->api]).func)

	//RTAS

	void BLAS_freeExt(BLAS *blas) { WrapperFunction(blas->base.device, blasFree)(blas); }
	Bool BLAS_initExt(BLAS *blas, Error *e_rr) { return WrapperFunction(blas->base.device, blasInit)(blas, e_rr); }

	Bool BLASRef_flushExt(void *commandBuffer, GraphicsDeviceRef *deviceRef, BLASRef *pending, Error *e_rr) {
		return WrapperFunction(deviceRef, blasFlush)(commandBuffer, deviceRef, pending, e_rr);
	}

	void TLAS_freeExt(TLAS *tlas) { WrapperFunction(tlas->base.device, tlasFree)(tlas); }
	Bool TLAS_initExt(TLAS *tlas, Error *e_rr) { return WrapperFunction(tlas->base.device, tlasInit)(tlas, e_rr); }

	Bool TLASRef_flushExt(void *commandBuffer, GraphicsDeviceRef *deviceRef, TLASRef *pending, Error *e_rr) {
		return WrapperFunction(deviceRef, tlasFlush)(commandBuffer, deviceRef, pending, e_rr);
	}

	//Pipelines

	Bool GraphicsDevice_createPipelineComputeExt(
		GraphicsDevice *device,
		const CharString *name,
		Pipeline *pipeline,
		const SHBinaryInfo *buf,
		Error *e_rr
	) {
		return WrapperFunction(pipeline->device, pipelineCreateCompute)(device, name, pipeline, buf, e_rr);
	}

	Bool GraphicsDevice_createPipelineGraphicsExt(
		GraphicsDevice *dev,
		ListSHFile binaries,
		CharString name,
		Pipeline *pipeline,
		Error *e_rr
	) {
		return WrapperFunction(pipeline->device, pipelineCreateGraphics)(dev, binaries, name, pipeline, e_rr);
	}

	Bool GraphicsDevice_createPipelineRaytracingInternalExt(
		GraphicsDeviceRef *deviceRef,
		const ListSHFile *binaries,
		const CharString *name,
		U8 maxPayloadSize,
		U8 maxAttributeSize,
		ListU32 binaryIndices,
		Pipeline *pipeline,
		Error *e_rr
	) {
		return WrapperFunction(pipeline->device, pipelineCreateRt)(
			deviceRef, binaries, name, maxPayloadSize, maxAttributeSize, binaryIndices, pipeline, e_rr
		);
	}

	void Pipeline_freeExt(Pipeline *pipeline, const Allocator *alloc) {
		WrapperFunction(pipeline->device, pipelineFree)(pipeline, alloc);
	}

	//Sampler

	Bool GraphicsDeviceRef_createSamplerExt(GraphicsDeviceRef *dev, Sampler *sampler, const CharString *name, Error *e_rr) {
		return WrapperFunction(dev, samplerCreate)(dev, sampler, name, e_rr);
	}

	void Sampler_freeExt(Sampler *sampler) { WrapperFunction(sampler->device, samplerFree)(sampler); }

	//Device buffer

	Bool GraphicsDeviceRef_createBufferExt(GraphicsDeviceRef *dev, DeviceBuffer *buf, const CharString *name, Error *e_rr) {
		return WrapperFunction(dev, bufferCreate)(dev, buf, name, e_rr);
	}

	void DeviceBuffer_freeExt(DeviceBuffer *buffer) { WrapperFunction(buffer->resource.device, bufferFree)(buffer); }

	Bool DeviceBufferRef_flushExt(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceBufferRef *pending, Error *e_rr) {
		return WrapperFunction(deviceRef, bufferFlush)(commandBuffer, deviceRef, pending, e_rr);
	}

	//Device texture

	Bool UnifiedTexture_createExt(TextureRef *texture, const CharString *name, Error *e_rr) {
		return WrapperFunction(TextureRef_getUnifiedTexture(texture, NULL).resource.device, textureCreate)(texture, name, e_rr);
	}

	Bool DeviceTextureRef_flushExt(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceTextureRef *pending, Error *e_rr) {
		return WrapperFunction(deviceRef, textureFlush)(commandBuffer, deviceRef, pending, e_rr);
	}

	void UnifiedTexture_freeExt(TextureRef *texture) {
		WrapperFunction(TextureRef_getUnifiedTexture(texture, NULL).resource.device, textureFree)(texture);
	}

	//Swapchain

	Bool GraphicsDeviceRef_createSwapchainExt(GraphicsDeviceRef *dev, SwapchainRef *swapchain, Error *e_rr) {
		return WrapperFunction(dev, swapchainCreate)(dev, swapchain, e_rr);
	}

	void Swapchain_freeExt(Swapchain *data, const Allocator *alloc) {
		WrapperFunction(data->base.resource.device, swapchainFree)(data, alloc);
	}

	//DescriptorLayout

	Bool GraphicsDeviceRef_createDescriptorLayoutExt(
		GraphicsDeviceRef *dev,
		DescriptorLayout *layout,
		const CharString *name,
		Error *e_rr
	) {
		return WrapperFunction(dev, descriptorLayoutCreate)(dev, layout, name, e_rr);
	}

	void DescriptorLayout_freeExt(DescriptorLayout *layout, const Allocator *alloc) {
		WrapperFunction(layout->device, descriptorLayoutFree)(layout, alloc);
	}

	//PipelineLayout

	Bool GraphicsDeviceRef_createPipelineLayoutExt(
		GraphicsDeviceRef *dev,
		PipelineLayout *layout,
		const CharString *name,
		Error *e_rr
	) {
		return WrapperFunction(dev, pipelineLayoutCreate)(dev, layout, name, e_rr);
	}

	void PipelineLayout_freeExt(PipelineLayout *layout, const Allocator *alloc) {
		WrapperFunction(layout->device, pipelineLayoutFree)(layout, alloc);
	}

	//DescriptorTable

	Bool DescriptorHeap_createDescriptorTableExt(
		DescriptorHeapRef *heap,
		DescriptorTable *table,
		const CharString *name,
		Error *e_rr
	) {
		return WrapperFunction(DescriptorHeapRef_ptr(heap)->device, descriptorTableCreate)(heap, table, name, e_rr);
	}

	void DescriptorTable_freeExt(DescriptorTable *table, const Allocator *alloc) {
		WrapperFunction(DescriptorHeapRef_ptr(table->parent)->device, descriptorTableFree)(table, alloc);
	}

	Bool DescriptorTable_setDescriptorsExt(
		DescriptorTable *table,
		U64 bindingId,
		U64 arrayId,
		const ListDescriptor *darr,
		Error *e_rr
	) {
		return WrapperFunction(DescriptorHeapRef_ptr(table->parent)->device, descriptorTableSet)(
			table, bindingId, arrayId, darr, e_rr
		);
	}

	Bool DescriptorTable_unsetDescriptorsExt(
		DescriptorTable *table,
		U64 bindingId,
		U64 arrayId,
		U64 count,
		Error *e_rr
	) {
		return WrapperFunction(DescriptorHeapRef_ptr(table->parent)->device, descriptorTableUnset)(
			table, bindingId, arrayId, count, e_rr
		);
	}

	//DescriptorHeap

	Bool GraphicsDeviceRef_createDescriptorHeapExt(
		GraphicsDeviceRef *dev, DescriptorHeap *heap, const CharString *name, Error *e_rr
	) {
		return WrapperFunction(dev, descriptorHeapCreate)(dev, heap, name, e_rr);
	}

	void DescriptorHeap_freeExt(DescriptorHeap *heap, const Allocator *alloc) {
		WrapperFunction(heap->device, descriptorHeapFree)(heap, alloc);
	}

	//Allocator

	Bool DeviceMemoryAllocator_allocateExt(
		DeviceMemoryAllocator *allocator,
		void *requirementsExt,
		Bool cpuSided,
		U32 *blockId,
		U64 *blockOffset,
		EResourceType resourceType,
		const CharString *objectName,
		DeviceMemoryBlock *resultBlock,
		Error *e_rr
	) {
		return GraphicsInterface_instance->tables[GraphicsInstanceRef_ptr(allocator->device->instance)->api].memoryAllocate(
			allocator, requirementsExt, cpuSided, blockId, blockOffset, resourceType, objectName, resultBlock, e_rr
		);
	}

	Bool DeviceMemoryAllocator_freeAllocationExt(GraphicsDevice *device, void *ext) {
		return GraphicsInterface_instance->tables[GraphicsInstanceRef_ptr(device->instance)->api].memoryFree(device, ext);
	}

	//Device

	Bool GraphicsDevice_initExt(
		const GraphicsInstance *instance,
		const GraphicsDeviceInfo *deviceInfo,
		GraphicsDeviceRef **deviceRef,
		Error *e_rr
	) {
		return GraphicsInterface_instance->tables[instance->api].deviceInit(instance, deviceInfo, deviceRef, e_rr);
	}

	U64 GraphicsDevice_getMemoryBudgetExt(GraphicsDevice *device, Bool isDeviceLocal) {
		return GraphicsInterface_instance->tables[GraphicsInstanceRef_ptr(device->instance)->api].deviceGetMemoryBudget(
			device, isDeviceLocal
		);
	}

	void GraphicsDevice_freeExt(const GraphicsInstance *instance, void *ext) {
		GraphicsInterface_instance->tables[instance->api].deviceFree(instance, ext);
	}

	Bool GraphicsDeviceRef_waitExt(GraphicsDeviceRef *deviceRef, Error *e_rr) {
		return WrapperFunction(deviceRef, deviceWait)(deviceRef, e_rr);
	}

	Bool GraphicsDevice_submitCommandsExt(
		GraphicsDeviceRef *deviceRef,
		const ListCommandListRef *commandLists,
		const ListSwapchainRef *swapchains,
		const CBufferData *data,
		Error *e_rr
	) {
		return WrapperFunction(deviceRef, deviceSubmitCommands)(deviceRef, commandLists, swapchains, data, e_rr);
	}

	void CommandList_processExt(
		CommandList *commandList,
		GraphicsDeviceRef *deviceRef,
		ECommandOp op,
		const U8 *data,
		void *commandListExt
	) {
		WrapperFunction(deviceRef, commandListProcess)(commandList, deviceRef, op, data, commandListExt);
	}

	//Instance

	Bool GraphicsInstance_createExt(const GraphicsApplicationInfo *info, GraphicsInstanceRef **instanceRef, Error *e_rr) {
		return GraphicsInterface_instance->tables[GraphicsInstanceRef_ptr(*instanceRef)->api].instanceCreate(info, instanceRef, e_rr);
	}

	void GraphicsInstance_freeExt(GraphicsInstance *inst, const Allocator *alloc) {
		GraphicsInterface_instance->tables[inst->api].instanceFree(inst, alloc);
	}

	Bool GraphicsInstance_getDeviceInfosExt(const GraphicsInstance *inst, ListGraphicsDeviceInfo *infos, Error *e_rr) {
		return GraphicsInterface_instance->tables[inst->api].instanceGetDevices(inst, infos, e_rr);
	}

	const GraphicsObjectSizes *GraphicsInterface_getObjectSizes(EGraphicsApi api) {
		return api >= EGraphicsApi_Count ? NULL : &GraphicsInterface_instance->tables[api].objectSizes;
	}

#else
	Bool GraphicsInterface_init(Error *e_rr) { (void) e_rr; return true; }

	Bool GraphicsInterface_supports(EGraphicsApi api) {
		return api == _GRAPHICS_API || api == EGraphicsApi_Count /* Indicates 'Default' */;
	}
#endif
