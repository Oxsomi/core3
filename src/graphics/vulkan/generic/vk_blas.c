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

//graphics/vulkan/generic/vk_blas.c

#include "graphics/generic/device.h"
#include "types/base/mathi.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_buffer.h"
#include "graphics/vulkan/vulkan.h"
#include "types/container/string.h"
#include "types/base/constants.h"

Bool VK_WRAP_FUNC(BLAS_init)(BLAS *blas, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = blas ? GraphicsDeviceRef_getAlloc(blas->base.device) : NULL;

	GraphicsDeviceRef *deviceRef = blas->base.device;
	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	CharString tmp = CharString_createNull();
	VkBLAS *blasExt = BLAS_ext(blas, Vk);

	if(blas->base.asConstructionType == EBLASConstructionType_Serialized)
		retError(clean, Error_unsupportedOperation(0, "VkBLAS_init()::serialized not supported yet"));        //TODO:

	U64 primitives = 0;
	EBLASConstructionType type = (EBLASConstructionType) blas->base.asConstructionType;

	U64 vertexCount = 0;

	switch (type) {

		case EBLASConstructionType_Serialized:
			primitives = Buffer_length(blas->cpuData) / 12;        //Conservative estimate
			break;

		case EBLASConstructionType_Procedural:
			primitives = blas->aabbBuffer.len / (sizeof(F32) * 3 * 2);
			break;

		default: {

			vertexCount = blas->positionBuffer.len / blas->positionBufferStride;
			U8 stride = blas->indexFormatId == ETextureFormatId_R32u ? 4 : 2;

			if(blas->indexFormatId != ETextureFormatId_Undefined)
				primitives = blas->indexBuffer.len / stride / 3;

			else primitives = vertexCount / 3;

			break;
		}
	}

	if(primitives >> 32)
		retError(clean, Error_outOfBounds(
			0, primitives, U32_MAX, "VkBLAS_init() only primitive count of <U32_MAX is supported"
		));

	blasExt->range = (VkAccelerationStructureBuildRangeInfoKHR) { .primitiveCount = (U32) primitives };

	//Convert to Vulkan dependent version

	VkAccelerationStructureGeometryKHR geometry = (VkAccelerationStructureGeometryKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR
	};

	if(blas->base.flagsExt & EBLASFlag_DisableAnyHit)
		geometry.flags |= VK_GEOMETRY_OPAQUE_BIT_KHR;

	if(blas->base.flagsExt & EBLASFlag_AvoidDuplicateAnyHit)
		geometry.flags |= VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;

	if(blas->base.asConstructionType == EBLASConstructionType_Geometry) {

		geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;

		//RGBA32f is only optionally supported as an AS vertex format, RGB32f support is mandatory;
		// the w is padding the stride already covers, so the three component format reads the same memory.

		VkFormat vertexFormat = mapVkFormat(ETextureFormatId_unpack[blas->positionFormatId]);

		if(blas->positionFormatId == ETextureFormatId_RGBA32f)
			vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;

		VkAccelerationStructureGeometryTrianglesDataKHR *tri = &geometry.geometry.triangles;
		*tri = (VkAccelerationStructureGeometryTrianglesDataKHR) {
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
			.vertexFormat = vertexFormat,
			.vertexData = getVkLocation(blas->positionBuffer, blas->positionOffset),
			.vertexStride = blas->positionBufferStride,
			.maxVertex = (U32) vertexCount,
			.indexType = VK_INDEX_TYPE_NONE_KHR        //Zero initialized would read as UINT16 without an index buffer
		};

		if (blas->indexFormatId) {
			tri->indexType = blas->indexFormatId == ETextureFormatId_R32u ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
			tri->indexData = getVkLocation(blas->indexBuffer, 0);
		}

		//Opacity micromaps, stage 1.
		//The micromap handle stays null on purpose: that is what tells the driver the index buffer holds only
		// special indices (fully opaque or fully transparent) rather than referencing a built micromap.
		//usageCounts stays empty for the same reason, since there are no micromap entries to describe.

		//It lives on the ext object rather than on this stack: the geometry desc only CHAINS it, and the build
		// that reads it runs at flush time, long after this function returned.

		if (blas->ommIndexFormatId) {

			blasExt->ommTriangles = (VkAccelerationStructureTrianglesOpacityMicromapEXT) {

				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT,

				.indexType =
					blas->ommIndexFormatId == ETextureFormatId_R32u ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16,

				.indexBuffer = getVkLocation(blas->ommIndexBuffer, 0),
				.indexStride = blas->ommIndexFormatId == ETextureFormatId_R32u ? 4 : 2
			};

			tri->pNext = &blasExt->ommTriangles;
		}
	}

	else {
		geometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
		geometry.geometry.aabbs = (VkAccelerationStructureGeometryAabbsDataKHR) {
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR,
			.data = getVkLocation(blas->aabbBuffer, blas->aabbOffset),
			.stride = blas->aabbStride
		};
	}

	blasExt->geometry = geometry;

	VkBuildAccelerationStructureFlagsKHR flags = (VkBuildAccelerationStructureFlagsKHR) 0;

	if(blas->base.flags & ERTASBuildFlags_AllowUpdate)
		flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;

	if(blas->base.flags & ERTASBuildFlags_AllowCompaction)
		flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;

	if(blas->base.flags & ERTASBuildFlags_FastTrace)
		flags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

	if(blas->base.flags & ERTASBuildFlags_FastBuild)
		flags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;

	if(blas->base.flags & ERTASBuildFlags_MinimizeMemory)
		flags |= VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR;

	if(blas->base.flags & ERTASBuildFlags_AllowDataAccessExt)
		flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_BIT_KHR;

	blasExt->geometries = (VkAccelerationStructureBuildGeometryInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		.flags = flags,
		.geometryCount = 1,
		.pGeometries = &blasExt->geometry
	};

	//mode and srcAccelerationStructure are left at build here and decided per build in flush instead, since
	// whether a build refits depends on whether this structure has been built before.

	//Get build size to allocate scratch and final buffer

	U32 primitivesU32 = (U32) primitives;
	VkAccelerationStructureBuildSizesInfoKHR sizes = (VkAccelerationStructureBuildSizesInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
	};

	deviceExt->getAccelerationStructureBuildSizes(
		deviceExt->device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&blasExt->geometries,
		&primitivesU32,
		&sizes
	);

	//Allocate scratch and final buffer

	gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_ASExt,
		EGraphicsResourceFlag_None,
		NULL,
		&blas->base.name,
		sizes.accelerationStructureSize,
		&blas->base.asBuffer, e_rr
	));

	gotoIfError3(clean, CharString_format(
		alloc,
		&tmp,
		e_rr,
		"%.*s scratch buffer",
		CharString_length(blas->base.name),
		blas->base.name.ptr
	));

	gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_ScratchExt,
		EGraphicsResourceFlag_None,
		NULL,
		&tmp,
		//One scratch buffer serves both, since the same object now does the full build and every refit after
		// it; the update size is not required to be the smaller of the two, so neither is assumed.

		blas->base.flags & ERTASBuildFlags_AllowUpdate ?
			U64_max(sizes.buildScratchSize, sizes.updateScratchSize) : sizes.buildScratchSize,
		&blas->base.tempScratchBuffer, e_rr
	));

	VkAccelerationStructureCreateInfoKHR createInfo = (VkAccelerationStructureCreateInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
		.buffer = DeviceBuffer_ext(DeviceBufferRef_ptr(blas->base.asBuffer), Vk)->buffer,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		.size = sizes.accelerationStructureSize
	};

	gotoIfError3(clean, checkVkError(
		deviceExt->createAccelerationStructure(deviceExt->device, &createInfo, NULL, &blasExt->as),
		e_rr
	));

	blasExt->geometries.dstAccelerationStructure = blasExt->as;

	blasExt->geometries.scratchData = (VkDeviceOrHostAddressKHR) {
		.deviceAddress = DeviceBufferRef_ptr(blas->base.tempScratchBuffer)->resource.deviceAddress
	};

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

void VK_WRAP_FUNC(BLAS_free)(BLAS *blas) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(blas->base.device);
	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	const VkAccelerationStructureKHR as = BLAS_ext(blas, Vk)->as;

	if(as)
		deviceExt->destroyAccelerationStructure(deviceExt->device, as, NULL);
}

Bool VK_WRAP_FUNC(BLASRef_flush)(void *commandBufferExt, GraphicsDeviceRef *deviceRef, BLASRef *pending, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	VkCommandBufferState *commandBuffer = (VkCommandBufferState*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	BLAS *blas = BLASRef_ptr(pending);
	VkBLAS *blasExt = BLAS_ext(blas, Vk);

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->fifId];

	if(blas->base.isCompleted && !(blas->base.flags & ERTASBuildFlags_AllowUpdate))        //Done
		return s_uccess;

	const VkAccelerationStructureBuildRangeInfoKHR *range = &blasExt->range;

	//A structure that was already built refits itself in place, which both APIs allow and which is what keeps
	// its device address and every instance descriptor pointing at it valid across an update.

	if (blas->base.isCompleted) {
		blasExt->geometries.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
		blasExt->geometries.srcAccelerationStructure = blasExt->as;
	}

	deviceExt->cmdBuildAccelerationStructures(
		commandBuffer->buffer,
		1,
		&blasExt->geometries,
		&range
	);

	//Add as flight and ensure flushes are done if too many ASes are queued this frame

	device->pendingPrimitives += blasExt->range.primitiveCount;

	if(!ListRefPtr_contains(*currentFlight, pending, 0, NULL)) {
		gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, pending, alloc, e_rr));
		RefPtr_inc(pending);
	}

	//We mark scratch buffer as delete, we do this by pushing it as a current flight resource,
	// and losing the reference from our object.
	//However that's only if allow update is false.

	if(!ListRefPtr_contains(*currentFlight, blas->base.tempScratchBuffer, 0, NULL)) {

		gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, blas->base.tempScratchBuffer, alloc, e_rr));

		if(!(blas->base.flags & ERTASBuildFlags_AllowUpdate))
			blas->base.tempScratchBuffer = NULL;

		else RefPtr_inc(blas->base.tempScratchBuffer);
	}

	//Ensure we don't exceed a maximum amount of time spent on the GPU

	if (device->pendingPrimitives >= device->flushThresholdPrimitives)
		gotoIfError3(clean, VkGraphicsDevice_flush(deviceRef, commandBuffer, e_rr));

	blas->base.isCompleted = true;

clean:
	return s_uccess;
}
