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

#include "graphics/generic/interface.h"
#include "graphics/generic/device.h"

const GraphicsObjectSizes *GraphicsDeviceRef_getObjectSizes(GraphicsDeviceRef *deviceRef) {

	if(!deviceRef || deviceRef->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return NULL;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	return GraphicsInterface_getObjectSizes(GraphicsInstanceRef_ptr(device->instance)->api);
}

#ifdef GRAPHICS_API_DYNAMIC

	#include "graphics/generic/blas.h"
	#include "graphics/generic/tlas.h"
	#include "graphics/generic/pipeline.h"
	#include "graphics/generic/sampler.h"
	#include "graphics/generic/swapchain.h"
	#include "platforms/file.h"
	#include "platforms/log.h"
	#include "platforms/dynamic_library.h"
	#include "platforms/platform.h"
	#include "formats/oiSH.h"

	GraphicsInterface GraphicsInterface_instance = { 0 };

	Bool GraphicsInterface_register(FileInfo info, void *dat, Error *e_rr) {

		(void) dat;

		if(info.type != EFileType_File)
			return true;

		if(!DynamicLibrary_isValidPath(info.path))
			return true;

		DynamicLibrary library = (DynamicLibrary) { 0 };
		Bool s_uccess = true;
		gotoIfError3(clean, DynamicLibrary_load(info.path, &library, e_rr))

		void *tableFunc = NULL;
		if(!DynamicLibrary_loadSymbol(library, CharString_createRefCStrConst("GraphicsInterface_getTable"), &tableFunc, NULL))
			goto clean;

		GraphicsInterfaceTable table = ((GraphicsInterface_getTableImpl)tableFunc)(Platform_instance);

		if (table.api >= EGraphicsApi_Count) {
			Log_warnLnx("GraphicsInterface_register() found \"%s\" but was unable to initialize it", info.path.ptr);
			goto clean;
		}

		if(GraphicsInterface_instance.tables[table.api].instanceCreate)
			retError(clean, Error_invalidState(0, "GraphicsInterface_register() dynamic library was already initialized"))

		GraphicsInterface_instance.tables[table.api] = table;
		Log_debugLnx("-- Dynamically loaded %s", info.path.ptr);

	clean:
		if(!s_uccess)
			DynamicLibrary_free(&library);

		return s_uccess;
	}

	Bool GraphicsInterface_init(Error *e_rr) {

		ELockAcquire acq = SpinLock_lock(&GraphicsInterface_instance.lock, U64_MAX);
		Bool s_uccess = true;

		if(acq < ELockAcquire_Success)
			retError(clean, Error_invalidState(0, "GraphicsInterface_init() couldn't acquire lock"))

		if(GraphicsInterface_instance.init)		//Already initialized
			goto clean;

		gotoIfError3(clean, File_foreach(
			CharString_createRefCStrConst("."), GraphicsInterface_register, NULL, false, e_rr
		))

		GraphicsInterface_instance.init = 1;

	clean:
		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(&GraphicsInterface_instance.lock);

		return s_uccess;
	}

	Bool GraphicsInterface_supports(EGraphicsApi api) {
		return api <= EGraphicsApi_Count && GraphicsInterface_instance.tables[api].instanceCreate;
	}

	//These are kept for ease of use, these are just a wrapper for the interface

	#define WrapperFunction(device, func) \
	((GraphicsInterface_instance.tables[GraphicsInstanceRef_ptr(GraphicsDeviceRef_ptr(device)->instance)->api]).func)

	//RTAS

	Bool BLAS_freeExt(BLAS *blas) { return WrapperFunction(blas->base.device, blasFree)(blas); }
	Error BLAS_initExt(BLAS *blas) { return WrapperFunction(blas->base.device, blasInit)(blas); }

	Error BLASRef_flushExt(void *commandBuffer, GraphicsDeviceRef *deviceRef, BLASRef *pending) {
		return WrapperFunction(deviceRef, blasFlush)(commandBuffer, deviceRef, pending);
	}

	Bool TLAS_freeExt(TLAS *tlas) { return WrapperFunction(tlas->base.device, tlasFree)(tlas); }
	Error TLAS_initExt(TLAS *tlas) { return WrapperFunction(tlas->base.device, tlasInit)(tlas); }

	Error TLASRef_flushExt(void *commandBuffer, GraphicsDeviceRef *deviceRef, TLASRef *pending) {
		return WrapperFunction(deviceRef, tlasFlush)(commandBuffer, deviceRef, pending);
	}

	//Pipelines

	Bool GraphicsDevice_createPipelineComputeExt(
		GraphicsDevice *device,
		CharString name,
		Pipeline *pipeline,
		SHBinaryInfo buf,
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
		ListSHFile binaries,
		CharString name,
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

	Bool Pipeline_freeExt(Pipeline *pipeline, Allocator alloc) {
		return WrapperFunction(pipeline->device, pipelineFree)(pipeline, alloc);
	}

	//Sampler

	Error GraphicsDeviceRef_createSamplerExt(GraphicsDeviceRef *dev, Sampler *sampler, CharString name) {
		return WrapperFunction(dev, samplerCreate)(dev, sampler, name);
	}

	Bool Sampler_freeExt(Sampler *sampler) { return WrapperFunction(sampler->device, samplerFree)(sampler); }

	//Device buffer

	Error GraphicsDeviceRef_createBufferExt(GraphicsDeviceRef *dev, DeviceBuffer *buf, CharString name) {
		return WrapperFunction(dev, bufferCreate)(dev, buf, name);
	}

	Bool DeviceBuffer_freeExt(DeviceBuffer *buffer) {
		return WrapperFunction(buffer->resource.device, bufferFree)(buffer);
	}

	Error DeviceBufferRef_flushExt(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceBufferRef *pending) {
		return WrapperFunction(deviceRef, bufferFlush)(commandBuffer, deviceRef, pending);
	}

	//Device texture

	Error UnifiedTexture_createExt(TextureRef *texture, CharString name) {
		return WrapperFunction(TextureRef_getUnifiedTexture(texture, NULL).resource.device, textureCreate)(texture, name);
	}

	Error DeviceTextureRef_flushExt(void *commandBuffer, GraphicsDeviceRef *deviceRef, DeviceTextureRef *pending) {
		return WrapperFunction(deviceRef, textureFlush)(commandBuffer, deviceRef, pending);
	}

	Bool UnifiedTexture_freeExt(TextureRef *texture) {
		return WrapperFunction(TextureRef_getUnifiedTexture(texture, NULL).resource.device, textureFree)(texture);
	}

	//Swapchain

	Error GraphicsDeviceRef_createSwapchainExt(GraphicsDeviceRef *dev, SwapchainRef *swapchain) {
		return WrapperFunction(dev, swapchainCreate)(dev, swapchain);
	}

	Bool Swapchain_freeExt(Swapchain *data, Allocator alloc) {
		return WrapperFunction(data->base.resource.device, swapchainFree)(data, alloc);
	}

	//Allocator
	
	Error DeviceMemoryAllocator_allocateExt(
		DeviceMemoryAllocator *allocator,
		void *requirementsExt,
		Bool cpuSided,
		U32 *blockId,
		U64 *blockOffset,
		EResourceType resourceType,
		CharString objectName
	) {
		GraphicsDevice *dev = GraphicsDeviceRef_ptr(allocator->device);
		return GraphicsInterface_instance.tables[GraphicsInstanceRef_ptr(dev->instance)->api].memoryAllocate(
			allocator, requirementsExt, cpuSided, blockId, blockOffset, resourceType, objectName
		);
	}

	Bool DeviceMemoryAllocator_freeAllocationExt(GraphicsDevice *device, void *ext) {
		return GraphicsInterface_instance.tables[GraphicsInstanceRef_ptr(device->instance)->api].memoryFree(device, ext);
	}

	//Device

	Error GraphicsDevice_initExt(
		const GraphicsInstance *instance,
		const GraphicsDeviceInfo *deviceInfo,
		GraphicsDeviceRef **deviceRef
	) {
		return GraphicsInterface_instance.tables[instance->api].deviceInit(instance, deviceInfo, deviceRef);
	}

	void GraphicsDevice_postInitExt(GraphicsDevice *device) {
		GraphicsInterface_instance.tables[GraphicsInstanceRef_ptr(device->instance)->api].devicePostInit(device);
	}

	Bool GraphicsDevice_freeExt(const GraphicsInstance *instance, void *ext) {
		return GraphicsInterface_instance.tables[instance->api].deviceFree(instance, ext);
	}

	Error GraphicsDeviceRef_waitExt(GraphicsDeviceRef *deviceRef) {
		return WrapperFunction(deviceRef, deviceWait)(deviceRef);
	}

	Error GraphicsDevice_submitCommandsExt(
		GraphicsDeviceRef *deviceRef,
		ListCommandListRef commandLists,
		ListSwapchainRef swapchains,
		CBufferData data
	) {
		return WrapperFunction(deviceRef, deviceSubmitCommands)(deviceRef, commandLists, swapchains, data);
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

	Error GraphicsInstance_createExt(GraphicsApplicationInfo info, GraphicsInstanceRef **instanceRef) {
		return GraphicsInterface_instance.tables[GraphicsInstanceRef_ptr(*instanceRef)->api].instanceCreate(info, instanceRef);
	}
	
	Bool GraphicsInstance_freeExt(GraphicsInstance *inst, Allocator alloc) {
		return GraphicsInterface_instance.tables[inst->api].instanceFree(inst, alloc);
	}

	Error GraphicsInstance_getDeviceInfosExt(const GraphicsInstance *inst, ListGraphicsDeviceInfo *infos) {
		return GraphicsInterface_instance.tables[inst->api].instanceGetDevices(inst, infos);
	}

	const GraphicsObjectSizes *GraphicsInterface_getObjectSizes(EGraphicsApi api) {
		return api >= EGraphicsApi_Count ? NULL : &GraphicsInterface_instance.tables[api].objectSizes;
	}

#endif
