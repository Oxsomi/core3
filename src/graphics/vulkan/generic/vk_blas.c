/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/ext/listx_impl.h"
#include "platforms/ext/stringx.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_buffer.h"
#include "graphics/vulkan/vulkan.h"

const U64 BLASExt_size = sizeof(VkBLAS);

Bool BLAS_freeExt(BLAS *blas) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(blas->base.device);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	VkAccelerationStructureKHR as = BLAS_ext(blas, Vk)->as;

	if(as)
		instanceExt->destroyAccelerationStructure(deviceExt->device, as, NULL);

	return true;
}

Error BLASRef_flush(void *commandBufferExt, GraphicsDeviceRef *deviceRef, BLASRef *pending) {

	VkCommandBuffer commandBuffer = (VkCommandBuffer) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	DeviceBufferRef *tempScratch = NULL;

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->submitId % 3];

	BLAS *blas = BLASRef_ptr(pending);

	Error err = Error_none();
	CharString tmp = CharString_createNull();

	if(blas->base.asConstructionType == EBLASConstructionType_Serialized)
		return Error_unsupportedOperation(0, "BLASRef_flush()::serialized not supported yet");		//TODO:

	U64 primitives = 0;
	EBLASConstructionType type = (EBLASConstructionType) blas->base.asConstructionType;

	U64 vertexCount = 0;

	switch (type) {

		case EBLASConstructionType_Serialized:
			primitives = Buffer_length(blas->cpuData) / 12;		//Conservative estimate
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
		_gotoIfError(clean, Error_outOfBounds(
			0, primitives, U32_MAX, "BLASRef_flush() only primitive count of <U32_MAX is supported"
		));

	//Convert to Vulkan dependent version

	VkAccelerationStructureGeometryKHR geometry = (VkAccelerationStructureGeometryKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR
	};

	if(blas->base.flagsExt & EBLASFlag_DisableAnyHit)
		geometry.flags |= VK_GEOMETRY_OPAQUE_BIT_KHR;

	if(blas->base.flagsExt & EBLASFlag_AvoidDuplicateAnyHit)
		geometry.flags |= VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;

	VkDependencyInfo dep = (VkDependencyInfo){ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

	VkCommandQueue queue = deviceExt->queues[EVkCommandQueue_Graphics];
	U32 graphicsQueueId = queue.queueId;

	if(blas->base.asConstructionType == EBLASConstructionType_Geometry) {

		geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;

		VkAccelerationStructureGeometryTrianglesDataKHR *tri = &geometry.geometry.triangles;
		*tri = (VkAccelerationStructureGeometryTrianglesDataKHR) {
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
			.vertexFormat = mapVkFormat(ETextureFormatId_unpack[blas->positionFormatId]),
			.vertexData = getVkLocation(blas->positionBuffer, blas->positionOffset),
			.vertexStride = blas->positionBufferStride,
			.maxVertex = (U32) vertexCount
		};

		_gotoIfError(clean, VkDeviceBuffer_transition(
			DeviceBuffer_ext(DeviceBufferRef_ptr(blas->positionBuffer.buffer), Vk),
			VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
			graphicsQueueId,
			0, 0,
			&deviceExt->bufferTransitions,
			&dep
		));

		if (blas->indexFormatId) {

			tri->indexType = blas->indexFormatId == ETextureFormatId_R32u ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
			tri->indexData = getVkLocation(blas->indexBuffer, 0);

			_gotoIfError(clean, VkDeviceBuffer_transition(
				DeviceBuffer_ext(DeviceBufferRef_ptr(blas->indexBuffer.buffer), Vk),
				VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
				graphicsQueueId,
				0, 0,
				&deviceExt->bufferTransitions,
				&dep
			));
		}
	}

	else {

		geometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
		geometry.geometry.aabbs = (VkAccelerationStructureGeometryAabbsDataKHR) {
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR,
			.data = getVkLocation(blas->aabbBuffer, blas->aabbOffset),
			.stride = blas->aabbStride
		};

		_gotoIfError(clean, VkDeviceBuffer_transition(
			DeviceBuffer_ext(DeviceBufferRef_ptr(blas->aabbBuffer.buffer), Vk),
			VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
			graphicsQueueId,
			0, 0,
			&deviceExt->bufferTransitions,
			&dep
		));
	}

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

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = (VkAccelerationStructureBuildGeometryInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		.flags = flags,
		.geometryCount = 1,
		.pGeometries = &geometry
	};

	//Get build size to allocate scratch and final buffer

	U32 primitivesU32 = (U32) primitives;
	VkAccelerationStructureBuildSizesInfoKHR sizes = (VkAccelerationStructureBuildSizesInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
	};

	instanceExt->getAccelerationStructureBuildSizes(
		deviceExt->device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfo,
		&primitivesU32,
		&sizes
	);

	//Allocate scratch and final buffer

	_gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_ASExt,
		EGraphicsResourceFlag_None,
		blas->base.name,
		sizes.accelerationStructureSize,
		&blas->base.asBuffer
	));

	_gotoIfError(clean, CharString_formatx(
		&tmp, "%.*s scratch buffer", CharString_length(blas->base.name), blas->base.name.ptr
	));

	_gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_ScratchExt,
		EGraphicsResourceFlag_None,
		tmp,
		blas->base.flags & ERTASBuildFlags_IsUpdate ? sizes.updateScratchSize : sizes.buildScratchSize,
		&tempScratch
	));

	VkAccelerationStructureCreateInfoKHR createInfo = (VkAccelerationStructureCreateInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
		.buffer = DeviceBuffer_ext(DeviceBufferRef_ptr(blas->base.asBuffer), Vk)->buffer,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
	};

	_gotoIfError(clean, vkCheck(instanceExt->createAccelerationStructure(
		deviceExt->device, &createInfo, NULL, &BLAS_ext(BLASRef_ptr(pending), Vk)->as
	)));

	//Queue build

	if(blas->base.parent)
		buildInfo.srcAccelerationStructure = BLAS_ext(BLASRef_ptr(blas->base.parent), Vk)->as;

	buildInfo.dstAccelerationStructure = BLAS_ext(BLASRef_ptr(pending), Vk)->as;

	buildInfo.scratchData = (VkDeviceOrHostAddressKHR) {
		.deviceAddress = DeviceBufferRef_ptr(tempScratch)->resource.deviceAddress
	};

	if(blas->base.flags & ERTASBuildFlags_IsUpdate)
		buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;

	_gotoIfError(clean, VkDeviceBuffer_transition(
		DeviceBuffer_ext(DeviceBufferRef_ptr(blas->base.asBuffer), Vk),
		VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		graphicsQueueId,
		0, 0,
		&deviceExt->bufferTransitions,
		&dep
	));

	_gotoIfError(clean, VkDeviceBuffer_transition(
		DeviceBuffer_ext(DeviceBufferRef_ptr(tempScratch), Vk),
		VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		graphicsQueueId,
		0, 0,
		&deviceExt->bufferTransitions,
		&dep
	));

	if (dep.bufferMemoryBarrierCount)
		instanceExt->cmdPipelineBarrier2(commandBuffer, &dep);

	ListVkBufferMemoryBarrier2_clear(&deviceExt->bufferTransitions);

	VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo = (VkAccelerationStructureBuildRangeInfoKHR) {
		.primitiveCount = primitivesU32
	};

	const VkAccelerationStructureBuildRangeInfoKHR *buildRangeInfoPtr = &buildRangeInfo;

	instanceExt->cmdBuildAccelerationStructures(
		commandBuffer,
		1,
		&buildInfo,
		&buildRangeInfoPtr
	);

	//Add as flight and ensure flushes are done if too many ASes are queued this frame

	device->pendingBytes += primitives;

	_gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, pending));
	RefPtr_inc(pending);

	//We mark scratch buffer as delete, we do this by pushing it as a current flight resource
	//And losing the reference from our object

	_gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, tempScratch));
	tempScratch = NULL;

	//Ensure we don't exceed a maximum amount of time spent on the GPU

	if (device->pendingPrimitives >= device->flushThresholdPrimitives)
		VkGraphicsDevice_flush(deviceRef, commandBuffer);

	blas->base.isCompleted = true;

clean:
	DeviceBufferRef_dec(&tempScratch);
	CharString_freex(&tmp);
	return err;
}
