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
#include "graphics/generic/opacity_micromap.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_buffer.h"
#include "graphics/vulkan/vulkan.h"
#include "types/container/string.h"
#include "types/base/constants.h"
#include "types/container/list_impl.h"

TListNamedImpl(ListVkAccelerationStructureGeometryKHR);
TListNamedImpl(ListVkAccelerationStructureBuildRangeInfoKHR);
TListNamedImpl(ListVkBLASOmmTriangles);

//EBLASGeometryFlag is per geometry on both APIs, so this runs once per geometry rather than once per BLAS.

static VkGeometryFlagsKHR mapVkGeometryFlags(U8 flags) {

	VkGeometryFlagsKHR result = 0;

	if(flags & EBLASGeometryFlag_DisableAnyHit)
		result |= VK_GEOMETRY_OPAQUE_BIT_KHR;

	if(flags & EBLASGeometryFlag_AvoidDuplicateAnyHit)
		result |= VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;

	return result;
}

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
	const EBLASConstructionType type = (EBLASConstructionType) blas->base.asConstructionType;

	//One geometry desc per BLASGeometry, in that order, because a shader reads the index back as
	// GeometryIndex(); AABBs stay a single desc.
	//Sized before anything points into them, since the build info keeps those pointers for its lifetime.

	const U64 geometryCount = type == EBLASConstructionType_Geometry ? blas->geometries.length : 1;

	gotoIfError3(clean, ListVkAccelerationStructureGeometryKHR_resize(
		&blasExt->geometries, geometryCount, alloc, e_rr
	));

	gotoIfError3(clean, ListVkAccelerationStructureBuildRangeInfoKHR_resize(
		&blasExt->ranges, geometryCount, alloc, e_rr
	));

	gotoIfError3(clean, ListU32_resize(&blasExt->maxPrimitiveCounts, geometryCount, alloc, e_rr));

	//Convert to Vulkan dependent version

	if(type == EBLASConstructionType_Geometry) {

		//Allocated for every geometry as soon as one of them carries a micromap, so the chained addresses
		// taken below stay valid for the BLAS's lifetime.

		Bool anyOmm = false;

		for(U64 i = 0; i < geometryCount; ++i)
			if(blas->geometries.ptr[i].ommIndexFormatId)
				anyOmm = true;

		if(anyOmm)
			gotoIfError3(clean, ListVkBLASOmmTriangles_resize(&blasExt->ommTriangles, geometryCount, alloc, e_rr));

		for(U64 i = 0; i < geometryCount; ++i) {

			const BLASGeometry geom = blas->geometries.ptr[i];

			const U64 vertexCount = geom.positionBuffer.len / geom.positionBufferStride;
			const U8 indexStride = geom.indexFormatId == ETextureFormatId_R32u ? 4 : 2;

			const U64 geometryPrimitives =
				geom.indexFormatId != ETextureFormatId_Undefined ? geom.indexBuffer.len / indexStride / 3 :
				vertexCount / 3;

			if(geometryPrimitives >> 32)
				retError(clean, Error_outOfBounds(
					0, geometryPrimitives, U32_MAX, "VkBLAS_init() only primitive count of <U32_MAX is supported"
				));

			primitives += geometryPrimitives;

			//RGBA32f is only optionally supported as an AS vertex format, RGB32f support is mandatory;
			// the w is padding the stride already covers, so the three component format reads the same memory.

			VkFormat vertexFormat = mapVkFormat(ETextureFormatId_unpack[geom.positionFormatId]);

			if(geom.positionFormatId == ETextureFormatId_RGBA32f)
				vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;

			VkAccelerationStructureGeometryKHR *geometry = &blasExt->geometries.ptrNonConst[i];

			*geometry = (VkAccelerationStructureGeometryKHR) {
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
				.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
				.flags = mapVkGeometryFlags(geom.flags)
			};

			VkAccelerationStructureGeometryTrianglesDataKHR *tri = &geometry->geometry.triangles;
			*tri = (VkAccelerationStructureGeometryTrianglesDataKHR) {
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
				.vertexFormat = vertexFormat,
				.vertexData = getVkLocation(geom.positionBuffer, geom.positionOffset),
				.vertexStride = geom.positionBufferStride,
				.maxVertex = (U32) (vertexCount - 1),        //The highest index a triangle may name, not the count
				.indexType = VK_INDEX_TYPE_NONE_KHR        //Zero initialized would read as UINT16 without an index buffer
			};

			if (geom.indexFormatId) {
				tri->indexType = geom.indexFormatId == ETextureFormatId_R32u ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
				tri->indexData = getVkLocation(geom.indexBuffer, 0);
			}

			blasExt->ranges.ptrNonConst[i] = (VkAccelerationStructureBuildRangeInfoKHR) {
				.primitiveCount = (U32) geometryPrimitives
			};

			blasExt->maxPrimitiveCounts.ptrNonConst[i] = (U32) geometryPrimitives;

			//Opacity micromaps, stage 1.
			//The micromap handle stays null on purpose: that is what tells the driver the index buffer holds only
			// special indices (fully opaque or fully transparent) rather than referencing a built micromap.
			//usageCounts stays empty for the same reason, since there are no micromap entries to describe.

			//It lives on the ext object rather than on this stack: the geometry desc only CHAINS it, and the build
			// that reads it runs at flush time, long after this function returned.

			if (geom.ommIndexFormatId) {

				VkBLASOmmTriangles *ommTriangles = &blasExt->ommTriangles.ptrNonConst[i];

				VkIndexType indexType = VK_INDEX_TYPE_UINT16;
				U8 ommIndexStride = 2;

				switch (geom.ommIndexFormatId) {
					case ETextureFormatId_R32u:    indexType = VK_INDEX_TYPE_UINT32;    ommIndexStride = 4;    break;
					case ETextureFormatId_R8u:     indexType = VK_INDEX_TYPE_UINT8;     ommIndexStride = 1;    break;
					default:                                                                                   break;
				}

				//Which of the two extensions the device runs decides the struct: they are not layout compatible and
				// the driver only accepts its own.
				//R8u can't get here at all today: no Vulkan device claims RayMicromapOpacityU8 (that waits on the
				// KHR path being implemented) and BLAS create validation rejects R8u without it.

				if(device->info.capabilities.featuresExt & EVkGraphicsFeatures_OpacityMicromapKHR) {

					ommTriangles->khr = (VkAccelerationStructureTrianglesOpacityMicromapKHR) {
						.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_KHR,
						.indexType = indexType,
						.indexBuffer = getVkDeviceAddress(geom.ommIndexBuffer),
						.indexStride = ommIndexStride
					};

					tri->pNext = &ommTriangles->khr;
				}

				else {

					ommTriangles->ext = (VkAccelerationStructureTrianglesOpacityMicromapEXT) {
						.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT,
						.indexType = indexType,
						.indexBuffer = getVkLocation(geom.ommIndexBuffer, 0),
						.indexStride = ommIndexStride
					};

					//A linked micromap brings its handle and its usage counts; the EXT extension wants the same
					// counts the array was built with restated in every BLAS that links it.

					if (geom.ommMicromap) {

						VkOpacityMicromap *micromapExt = OpacityMicromap_ext(OpacityMicromapRef_ptr(geom.ommMicromap), Vk);

						ommTriangles->ext.micromap = micromapExt->micromap;
						ommTriangles->ext.usageCountsCount = (U32) micromapExt->usages.length;
						ommTriangles->ext.pUsageCounts = micromapExt->usages.ptr;
					}

					tri->pNext = &ommTriangles->ext;
				}
			}
		}
	}

	else {

		primitives = blas->aabbBuffer.len / blas->aabbStride;        //One per stride, which may exceed the 24 byte box

		if(primitives >> 32)
			retError(clean, Error_outOfBounds(
				0, primitives, U32_MAX, "VkBLAS_init() only primitive count of <U32_MAX is supported"
			));

		blasExt->geometries.ptrNonConst[0] = (VkAccelerationStructureGeometryKHR) {
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
			.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR,
			.flags = mapVkGeometryFlags(blas->aabbFlags),
			.geometry = {
				.aabbs = (VkAccelerationStructureGeometryAabbsDataKHR) {
					.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR,
					.data = getVkLocation(blas->aabbBuffer, blas->aabbOffset),
					.stride = blas->aabbStride
				}
			}
		};

		blasExt->ranges.ptrNonConst[0] = (VkAccelerationStructureBuildRangeInfoKHR) {
			.primitiveCount = (U32) primitives
		};

		blasExt->maxPrimitiveCounts.ptrNonConst[0] = (U32) primitives;
	}

	blasExt->primitives = primitives;

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

	blasExt->build = (VkAccelerationStructureBuildGeometryInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		.flags = flags,
		.geometryCount = (U32) geometryCount,
		.pGeometries = blasExt->geometries.ptr
	};

	//mode and srcAccelerationStructure are left at build here and decided per build in flush instead, since
	// whether a build refits depends on whether this structure has been built before.

	//Get build size to allocate scratch and final buffer

	VkAccelerationStructureBuildSizesInfoKHR sizes = (VkAccelerationStructureBuildSizesInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
	};

	deviceExt->getAccelerationStructureBuildSizes(
		deviceExt->device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&blasExt->build,
		blasExt->maxPrimitiveCounts.ptr,
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

	//TODO: cache the scratch buffer on the device rather than allocating one per structure.
	// Scratch is only live between a build and the submit that runs it, so structures built in the same
	// submit could share one buffer sized to the largest of them, and a build that is not a refit could
	// hand it straight back. That is a real saving on a scene of many structures, where the scratch can
	// rival the structures themselves in peak footprint.

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

	//No query claimed yet; the build claims one only if this structure may be compacted.

	blas->base.compactionQuery = U32_MAX;
	blas->base.isCompacted = false;

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

	blasExt->build.dstAccelerationStructure = blasExt->as;

	blasExt->build.scratchData = (VkDeviceOrHostAddressKHR) {
		.deviceAddress = DeviceBufferRef_ptr(blas->base.tempScratchBuffer)->resource.deviceAddress
	};

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

void VK_WRAP_FUNC(BLAS_free)(BLAS *blas) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(blas->base.device);
	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	VkBLAS *blasExt = BLAS_ext(blas, Vk);

	if(blasExt->as)
		deviceExt->destroyAccelerationStructure(deviceExt->device, blasExt->as, NULL);

	//A compaction whose copy was prepared but never recorded owns a destination structure that nothing
	// else will ever adopt, since only BLASRef_compact moves it into as.
	//The buffer it sits in is released by BLAS_free, which calls this before dropping that reference, so
	// the structure always goes before its memory.

	if(blasExt->pendingAs)
		deviceExt->destroyAccelerationStructure(deviceExt->device, blasExt->pendingAs, NULL);

	const Allocator *alloc = GraphicsDeviceRef_getAlloc(blas->base.device);

	ListVkAccelerationStructureGeometryKHR_free(&blasExt->geometries, alloc);
	ListVkAccelerationStructureBuildRangeInfoKHR_free(&blasExt->ranges, alloc);
	ListU32_free(&blasExt->maxPrimitiveCounts, alloc);
	ListVkBLASOmmTriangles_free(&blasExt->ommTriangles, alloc);
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

	const VkAccelerationStructureBuildRangeInfoKHR *range = blasExt->ranges.ptr;

	//A structure that was already built refits itself in place, which both APIs allow and which is what keeps
	// its device address and every instance descriptor pointing at it valid across an update.

	if (blas->base.isCompleted) {
		blasExt->build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
		blasExt->build.srcAccelerationStructure = blasExt->as;
	}

	deviceExt->cmdBuildAccelerationStructures(
		commandBuffer->buffer,
		1,
		&blasExt->build,
		&range
	);

	//The compacted size is a property of the BUILT structure, so the query is recorded right behind the
	//build and read back once the submit containing both has completed.

	const Bool wantsCompaction =
		(blas->base.flags & ERTASBuildFlags_AllowCompaction) &&
		!blas->base.isCompacted &&
		blas->base.compactionQuery == U32_MAX &&
		deviceExt->writeAccelerationStructuresProperties;

	if(wantsCompaction) {

		//The slot itself comes from the shared allocator; only the pool behind it is Vulkan's.

		U32 query = U32_MAX;
		Bool needsPool = false;

		const U32 poolSize = GraphicsDevice_compactionPoolSize(device, VK_COMPACTION_QUERIES_BASE);

		gotoIfError3(clean, GraphicsDevice_claimCompactionQuery(
			device, VK_COMPACTION_QUERIES_BASE, &query, &needsPool, e_rr
		));

		if(needsPool) {

			const VkQueryPoolCreateInfo queryInfo = (VkQueryPoolCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
				.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
				.queryCount = poolSize
			};

			VkQueryPool nextPool = NULL;

			gotoIfError3(clean, checkVkError(
				deviceExt->createQueryPool(deviceExt->device, &queryInfo, NULL, &nextPool), e_rr
			));

			gotoIfError3(clean, ListVkQueryPool_pushBack(&deviceExt->compactionPools, nextPool, alloc, e_rr));
		}

		U32 poolId = 0, slot = 0;
		GraphicsDevice_compactionQueryPool(query, VK_COMPACTION_QUERIES_BASE, &poolId, &slot);

		const VkQueryPool pool = deviceExt->compactionPools.ptr[poolId];

		//Reset just this slot. That covers both a slot in a brand new pool, whose queries start undefined,
		// and a recycled slot still holding the previous structure's size.

		deviceExt->cmdResetQueryPool(commandBuffer->buffer, pool, slot, 1);

		//The query reads the structure this same command buffer is still writing. Transitioned rather than
		// barriered by hand, so the structure's tracked stage and access stay in step with every other
		// transition on it.

		VkDependencyInfo dependency = (VkDependencyInfo) { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

		gotoIfError3(clean, VkDeviceBuffer_transition(
			DeviceBuffer_ext(DeviceBufferRef_ptr(blas->base.asBuffer), Vk),
			VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
			deviceExt->queues[EVkCommandQueue_Graphics].queueId,
			0, 0,
			&deviceExt->bufferTransitions,
			&dependency, alloc, e_rr
		));

		if(dependency.bufferMemoryBarrierCount)
			deviceExt->cmdPipelineBarrier2(commandBuffer->buffer, &dependency);

		gotoIfError3(clean, ListVkBufferMemoryBarrier2_clear(&deviceExt->bufferTransitions, e_rr));

		blas->base.compactionQuery = query;
		blas->base.compactionSubmitId = device->submitId;

		deviceExt->writeAccelerationStructuresProperties(
			commandBuffer->buffer, 1, &blasExt->as,
			VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, pool, slot
		);
	}

	//Add as flight and ensure flushes are done if too many ASes are queued this frame

	device->pendingPrimitives += blasExt->primitives;

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

//Compaction, in the two halves the interface splits it into.
//
//prepareCompact is the CPU work: read the size the build produced and allocate the structure to copy into.
//The read is non-blocking, so a query that is somehow not ready is reported rather than waited on.

Bool VK_WRAP_FUNC(BLASRef_prepareCompact)(GraphicsDeviceRef *deviceRef, BLASRef *blasRef, Bool *recorded, Error *e_rr) {

	Bool s_uccess = true;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	BLAS *blas = BLASRef_ptr(blasRef);
	VkBLAS *blasExt = BLAS_ext(blas, Vk);

	U64 compactedSize = 0;

	if(!deviceExt->copyAccelerationStructure)
		retError(clean, Error_unsupportedOperation(
			0, "VkBLASRef_prepareCompact() device doesn't support acceleration structure copies"
		));

	const U32 query = blas->base.compactionQuery;

	U32 poolId = 0, slot = 0;
	GraphicsDevice_compactionQueryPool(query, VK_COMPACTION_QUERIES_BASE, &poolId, &slot);

	const VkQueryPool queryPool = deviceExt->compactionPools.ptr[poolId];

	const VkResult queryRes = deviceExt->getQueryPoolResults(
		deviceExt->device, queryPool, slot, 1,
		sizeof(compactedSize), &compactedSize, sizeof(compactedSize), VK_QUERY_RESULT_64_BIT
	);

	if(queryRes == VK_NOT_READY)
		retError(clean, Error_invalidState(
			0, "VkBLASRef_prepareCompact() the compacted size isn't available yet, the build's submit has to complete first"
		));

	gotoIfError3(clean, checkVkError(queryRes, e_rr));

	//The query has been consumed either way, so the slot goes back before any early out below can take it
	// out of circulation.

	blas->base.compactionQuery = U32_MAX;
	GraphicsDevice_releaseCompactionQuery(device, query, alloc);

	//A driver is allowed to report no saving. Leaving recorded false keeps a pointless copy out of the
	// command buffer entirely.

	//A driver that declines to compact reports the ORIGINAL size 1:1 (confirmed for WARP and lavapipe);
	// ZERO is not a size a conformant driver can produce, so reading one means the query was consumed
	// before the copy that fills it ran, and silently treating it as "no saving" would bury a sync bug.

	if(!compactedSize)
		retError(clean, Error_invalidState(
			1,
			"VkBLASRef_prepareCompact() the compacted size read back as 0, which no conformant driver "
			"returns; the query result has not been written yet and reading it raced the GPU"
		));

	//No saving is a legitimate driver answer. Leaving recorded false keeps a pointless copy out of the
	// command buffer entirely.

	if(compactedSize >= DeviceBufferRef_ptr(blas->base.asBuffer)->resource.size) {
		blas->base.isCompacted = true;
		goto clean;
	}

	gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_ASExt, EGraphicsResourceFlag_None, NULL,
		&blas->base.name, compactedSize, &blas->base.pendingCompactBuffer, e_rr
	));

	const VkAccelerationStructureCreateInfoKHR createInfo = (VkAccelerationStructureCreateInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
		.buffer = DeviceBuffer_ext(DeviceBufferRef_ptr(blas->base.pendingCompactBuffer), Vk)->buffer,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		.size = compactedSize
	};

	gotoIfError3(clean, checkVkError(
		deviceExt->createAccelerationStructure(deviceExt->device, &createInfo, NULL, &blasExt->pendingAs), e_rr
	));

	*recorded = true;

clean:

	if(!s_uccess) {

		if(blasExt->pendingAs) {
			deviceExt->destroyAccelerationStructure(deviceExt->device, blasExt->pendingAs, NULL);
			blasExt->pendingAs = NULL;
		}

		RefPtr_dec(&blas->base.pendingCompactBuffer);
	}

	return s_uccess;
}

//And the GPU half: one copy into the destination prepared above, then the swap. The structure being
//replaced cannot go back yet, since the copy reading it has only been RECORDED, so its buffer joins the
//frame's in-flight resources and its handle the retired list.

Bool VK_WRAP_FUNC(BLASRef_compact)(
	void *commandBufferExt, GraphicsDeviceRef *deviceRef, BLASRef *blasRef, Error *e_rr
) {

	Bool s_uccess = true;

	VkCommandBufferState *commandBuffer = (VkCommandBufferState*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	BLAS *blas = BLASRef_ptr(blasRef);
	VkBLAS *blasExt = BLAS_ext(blas, Vk);

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->fifId];

	if(!blasExt->pendingAs || !blas->base.pendingCompactBuffer)
		retError(clean, Error_invalidState(0, "VkBLASRef_compact() nothing was prepared for this structure"));

	const VkCopyAccelerationStructureInfoKHR copyInfo = (VkCopyAccelerationStructureInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
		.src = blasExt->as,
		.dst = blasExt->pendingAs,
		.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
	};

	//Everything that can fail happens BEFORE the copy is recorded. Once it is in the command buffer neither
	// the structure it reads nor the one it writes may be released, so there is no undo: a failure between
	// the two retirements below would leave the structure naming a handle already queued for destruction.

	ListVkAccelerationStructureKHR *retired = &deviceExt->retiredAs[device->fifId];

	gotoIfError3(clean, ListRefPtr_reserve(currentFlight, currentFlight->length + 1, alloc, e_rr));
	gotoIfError3(clean, ListVkAccelerationStructureKHR_reserve(retired, retired->length + 1, alloc, e_rr));

	deviceExt->copyAccelerationStructure(commandBuffer->buffer, &copyInfo);

	//Reserved above, so neither can fail here. The buffer reference MOVES to the flight list rather than
	// being dropped, so the memory outlives the copy reading it.

	ListVkAccelerationStructureKHR_pushBack(retired, blasExt->as, alloc, NULL);
	ListRefPtr_pushBack(currentFlight, blas->base.asBuffer, alloc, NULL);

	blasExt->as = blasExt->pendingAs;
	blasExt->build.dstAccelerationStructure = blasExt->pendingAs;
	blas->base.asBuffer = blas->base.pendingCompactBuffer;
	blas->base.isCompacted = true;

	blasExt->pendingAs = NULL;
	blas->base.pendingCompactBuffer = NULL;

	//The address REALLY changes here, so dependents are marked again: a pending TLAS build riding this same
	// submit may have resolved the old address and cleared the record time mark already.

	gotoIfError3(clean, GraphicsDeviceRef_markTlasesStaleForBLAS(deviceRef, blasRef, true, e_rr));

clean:
	return s_uccess;
}
