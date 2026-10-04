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

//graphics/vulkan/generic/vk_pipeline_cache.c

#include "graphics/generic/pipeline.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_internal.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "platforms/logx.h"
#include "types/container/buffer.h"
#include "types/base/error.h"

static VkResult VkGraphicsDevice_buildPipelineWith(
	VkGraphicsDevice *deviceExt,
	EPipelineType type,
	VkPipelineCache cache,
	const void *createInfo,
	VkPipeline *result
) {
	switch(type) {

		case EPipelineType_Compute:
			return deviceExt->createComputePipelines(deviceExt->device, cache, 1, createInfo, NULL, result);

		case EPipelineType_Graphics:
			return deviceExt->createGraphicsPipelines(deviceExt->device, cache, 1, createInfo, NULL, result);

		default:
			return deviceExt->createRaytracingPipelines(deviceExt->device, NULL, cache, 1, createInfo, NULL, result);
	}
}

//The bytes a pipeline cache holds; 0 when the driver can't say

static U64 VkGraphicsDevice_cacheBytes(VkGraphicsDevice *deviceExt, VkPipelineCache cache) {
	size_t size = 0;
	return deviceExt->getPipelineCacheData(deviceExt->device, cache, &size, NULL) == VK_SUCCESS ? (U64) size : 0;
}

Bool VkGraphicsDevice_buildPipeline(
	GraphicsDevice *device,
	Pipeline *pipeline,
	const void *createInfo,
	U64 key,
	U64 irBytes,
	VkPipeline *result,
	Error *e_rr
) {

	Bool s_uccess = true;
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkPipelineCache own = VK_NULL_HANDLE;
	ELockAcquire acq = ELockAcquire_Invalid;

	//Built before: the device's cache has it, and the size was recorded when it was measured.
	//Under the device lock, since a merge into the device's cache may not overlap anything else using it.

	U64 known = 0;

	if(deviceExt->pipelineCache && GraphicsDevice_findPipelineSize(device, key, &known)) {

		acq = SpinLock_lock(&device->lock, U64_MAX);

		gotoIfError3(clean, VkGraphicsDevice_check(deviceExt, VkGraphicsDevice_buildPipelineWith(
			deviceExt, pipeline->type, deviceExt->pipelineCache, createInfo, result
		), e_rr));

		Pipeline_trackMemory(pipeline, known, irBytes, false);
		goto clean;
	}

	//New: built into a cache of its own, whose growth is what the driver keeps of it, then merged into the device's

	const VkPipelineCacheCreateInfo ownInfo = (VkPipelineCacheCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
	};

	if(!deviceExt->pipelineCache || deviceExt->createPipelineCache(deviceExt->device, &ownInfo, NULL, &own) != VK_SUCCESS)
		own = VK_NULL_HANDLE;

	const U64 empty = own ? VkGraphicsDevice_cacheBytes(deviceExt, own) : 0;

	gotoIfError3(clean, VkGraphicsDevice_check(deviceExt, VkGraphicsDevice_buildPipelineWith(
		deviceExt, pipeline->type, own, createInfo, result
	), e_rr));

	const U64 full = own ? VkGraphicsDevice_cacheBytes(deviceExt, own) : 0;

	if(full <= empty) {        //The driver keeps nothing it can say the size of
		Pipeline_trackMemory(pipeline, irBytes, irBytes, true);
		goto clean;
	}

	Pipeline_trackMemory(pipeline, full - empty, irBytes, false);
	gotoIfError3(clean, GraphicsDevice_recordPipelineSize(device, key, full - empty, e_rr));

	acq = SpinLock_lock(&device->lock, U64_MAX);

	if(deviceExt->mergePipelineCaches(deviceExt->device, deviceExt->pipelineCache, 1, &own) != VK_SUCCESS)
		Log_debugLnx("VkGraphicsDevice_buildPipeline() couldn't merge into the device's pipeline cache");

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	if(own)
		deviceExt->destroyPipelineCache(deviceExt->device, own, NULL);

	return s_uccess;
}

Bool VK_WRAP_FUNC(GraphicsDevice_loadPipelineCache)(GraphicsDevice *device, Buffer driverData, Bool *accepted, Error *e_rr) {

	Bool s_uccess = true;
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	VkPipelineCache loaded = VK_NULL_HANDLE;
	ELockAcquire acq = ELockAcquire_Invalid;

	*accepted = false;

	if(!Buffer_length(driverData)) {
		*accepted = true;
		goto clean;
	}

	if(!deviceExt->pipelineCache)
		goto clean;

	//Checked here rather than left to the driver: the spec has it ignore data it doesn't recognise, but a driver that
	// misreads another's data is the one place that would hurt.

	VkPipelineCacheHeaderVersionOne header = (VkPipelineCacheHeaderVersionOne) { 0 };

	if(Buffer_length(driverData) < sizeof(header))
		goto clean;

	Buffer_memcpy(Buffer_createRef(&header, sizeof(header)), Buffer_createRefConst(driverData.ptr, sizeof(header)));

	const VkPipelineCacheHeaderVersionOne *expected = &deviceExt->pipelineCacheHeader;

	if(
		header.headerVersion != expected->headerVersion ||
		header.headerSize < sizeof(header) ||
		header.vendorID != expected->vendorID ||
		header.deviceID != expected->deviceID ||
		Buffer_neq(
			Buffer_createRefConst(header.pipelineCacheUUID, VK_UUID_SIZE),
			Buffer_createRefConst(expected->pipelineCacheUUID, VK_UUID_SIZE)
		)
	)
		goto clean;

	const VkPipelineCacheCreateInfo info = (VkPipelineCacheCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
		.initialDataSize = (size_t) Buffer_length(driverData),
		.pInitialData = driverData.ptr
	};

	gotoIfError3(clean, checkVkError(deviceExt->createPipelineCache(deviceExt->device, &info, NULL, &loaded), e_rr));

	acq = SpinLock_lock(&device->lock, U64_MAX);

	gotoIfError3(clean, checkVkError(
		deviceExt->mergePipelineCaches(deviceExt->device, deviceExt->pipelineCache, 1, &loaded), e_rr
	));

	*accepted = true;

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	if(loaded)
		deviceExt->destroyPipelineCache(deviceExt->device, loaded, NULL);

	return s_uccess;
}

Bool VK_WRAP_FUNC(GraphicsDevice_savePipelineCache)(
	GraphicsDevice *device,
	const Allocator *alloc,
	Buffer *driverData,
	Error *e_rr
) {

	Bool s_uccess = true;
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	ELockAcquire acq = ELockAcquire_Invalid;

	if(!deviceExt->pipelineCache)
		goto clean;

	acq = SpinLock_lock(&device->lock, U64_MAX);

	size_t size = 0;
	gotoIfError3(clean, checkVkError(
		deviceExt->getPipelineCacheData(deviceExt->device, deviceExt->pipelineCache, &size, NULL), e_rr
	));

	if(!size)
		goto clean;

	gotoIfError3(clean, Buffer_createUninitializedBytes(size, alloc, driverData, e_rr));

	//VK_INCOMPLETE can't happen with nothing else using the cache, but it would only mean a shorter, still valid blob

	gotoIfError3(clean, checkVkError(deviceExt->getPipelineCacheData(
		deviceExt->device, deviceExt->pipelineCache, &size, (void*) driverData->ptr
	), e_rr));

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	if(!s_uccess)
		Buffer_free(driverData, alloc);

	return s_uccess;
}
