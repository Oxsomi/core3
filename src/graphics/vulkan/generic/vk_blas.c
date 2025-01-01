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

Error VK_WRAP_FUNC(BLAS_init)(BLAS *blas) {

	GraphicsDeviceRef *deviceRef = blas->base.device;
	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	CharString tmp = CharString_createNull();
	VkBLAS *blasExt = BLAS_ext(blas, Vk);

	if(blas->base.asConstructionType == EBLASConstructionType_Serialized)
		return Error_unsupportedOperation(0, "VkBLAS_init()::serialized not supported yet");		//TODO:

	Error err = Error_none();
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
		gotoIfError(clean, Error_outOfBounds(
			0, primitives, U32_MAX, "VkBLAS_init() only primitive count of <U32_MAX is supported"
		))

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

		VkAccelerationStructureGeometryTrianglesDataKHR *tri = &geometry.geometry.triangles;
		*tri = (VkAccelerationStructureGeometryTrianglesDataKHR) {
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
			.vertexFormat = mapVkFormat(ETextureFormatId_unpack[blas->positionFormatId]),
			.vertexData = getVkLocation(blas->positionBuffer, blas->positionOffset),
			.vertexStride = blas->positionBufferStride,
			.maxVertex = (U32) vertexCount
		};

		if (blas->indexFormatId) {
			tri->indexType = blas->indexFormatId == ETextureFormatId_R32u ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
			tri->indexData = getVkLocation(blas->indexBuffer, 0);
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

	blasExt->geometries = (VkAccelerationStructureBuildGeometryInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		.flags = flags,
		.geometryCount = 1,
		.pGeometries = &blasExt->geometry
	};

	if(blas->base.parent)
		blasExt->geometries.srcAccelerationStructure = BLAS_ext(BLASRef_ptr(blas->base.parent), Vk)->as;

	if(blas->base.flags & ERTASBuildFlags_IsUpdate)
		blasExt->geometries.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;

	//Get build size to allocate scratch and final buffer

	U32 primitivesU32 = (U32) primitives;
	VkAccelerationStructureBuildSizesInfoKHR sizes = (VkAccelerationStructureBuildSizesInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
	};

	instanceExt->getAccelerationStructureBuildSizes(
		deviceExt->device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&blasExt->geometries,
		&primitivesU32,
		&sizes
	);

	//Allocate scratch and final buffer

	gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_ASExt,
		EGraphicsResourceFlag_None,
		blas->base.name,
		sizes.accelerationStructureSize,
		&blas->base.asBuffer
	))

	gotoIfError(clean, CharString_formatx(
		&tmp, "%.*s scratch buffer", CharString_length(blas->base.name), blas->base.name.ptr
	))

	gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_ScratchExt,
		EGraphicsResourceFlag_None,
		tmp,
		blas->base.flags & ERTASBuildFlags_IsUpdate ? sizes.updateScratchSize : sizes.buildScratchSize,
		&blas->base.tempScratchBuffer
	))

	VkAccelerationStructureCreateInfoKHR createInfo = (VkAccelerationStructureCreateInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
		.buffer = DeviceBuffer_ext(DeviceBufferRef_ptr(blas->base.asBuffer), Vk)->buffer,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		.size = sizes.accelerationStructureSize
	};

	gotoIfError(clean, vkCheck(instanceExt->createAccelerationStructure(deviceExt->device, &createInfo, NULL, &blasExt->as)))
	blasExt->geometries.dstAccelerationStructure = blasExt->as;

	blasExt->geometries.scratchData = (VkDeviceOrHostAddressKHR) {
		.deviceAddress = DeviceBufferRef_ptr(blas->base.tempScratchBuffer)->resource.deviceAddress
	};

clean:
	CharString_freex(&tmp);
	return err;
}

Bool VK_WRAP_FUNC(BLAS_free)(BLAS *blas) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(blas->base.device);
	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	const VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	const VkAccelerationStructureKHR as = BLAS_ext(blas, Vk)->as;

	if(as)
		instanceExt->destroyAccelerationStructure(deviceExt->device, as, NULL);

	return true;
}

Error VK_WRAP_FUNC(BLASRef_flush)(void *commandBufferExt, GraphicsDeviceRef *deviceRef, BLASRef *pending) {

	VkCommandBufferState *commandBuffer = (VkCommandBufferState*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	BLAS *blas = BLASRef_ptr(pending);
	VkBLAS *blasExt = BLAS_ext(blas, Vk);

	ListRefPtr *currentFlight = &device->resourcesInFlight[(device->submitId - 1) % 3];

	Error err = Error_none();

	if(blas->base.isCompleted && !(blas->base.flags & ERTASBuildFlags_AllowUpdate))		//Done
		return Error_none();

	const VkAccelerationStructureBuildRangeInfoKHR *range = &blasExt->range;

	instanceExt->cmdBuildAccelerationStructures(
		commandBuffer->buffer,
		1,
		&blasExt->geometries,
		&range
	);

	//Add as flight and ensure flushes are done if too many ASes are queued this frame

	device->pendingPrimitives += blasExt->range.primitiveCount;

	gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, pending))
	RefPtr_inc(pending);

	//We mark scratch buffer as delete, we do this by pushing it as a current flight resource
	//And losing the reference from our object

	if(!(blas->base.flags & ERTASBuildFlags_AllowUpdate)) {
		gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, blas->base.tempScratchBuffer))
		blas->base.tempScratchBuffer = NULL;
	}

	//Ensure we don't exceed a maximum amount of time spent on the GPU

	if (device->pendingPrimitives >= device->flushThresholdPrimitives)
		gotoIfError(clean, VkGraphicsDevice_flush(deviceRef, commandBuffer))

	blas->base.isCompleted = true;

clean:
	return err;
}
