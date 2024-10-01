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

#include "platforms/ext/listx_impl.h"
#include "platforms/ext/stringx.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_buffer.h"
#include "graphics/vulkan/vulkan.h"

const U64 TLASExt_size = sizeof(VkTLAS);

Bool TLAS_getInstanceDataCpuInternal(const TLAS *tlas, U64 i, TLASInstanceData **result);

Error TLAS_initExt(TLAS *tlas) {

	GraphicsDeviceRef *deviceRef = tlas->base.device;
	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	VkTLAS *tlasExt = TLAS_ext(tlas, Vk);

	Error err = Error_none();
	CharString tmp = CharString_createNull();

	if(tlas->base.asConstructionType == ETLASConstructionType_Serialized)
		return Error_unsupportedOperation(0, "TLAS_initExt()::serialized not supported yet");		//TODO:

	U64 instancesU64 = 0;
	U64 stride = tlas->base.isMotionBlurExt ? sizeof(TLASInstanceMotion) : sizeof(TLASInstanceStatic);

	if (tlas->useDeviceMemory)
		instancesU64 = tlas->deviceData.len / stride;

	else instancesU64 = tlas->cpuInstancesStatic.length;		//Both static and motion length are at the same loc

	if(instancesU64 >> 24)
		gotoIfError(clean, Error_outOfBounds(
			0, instancesU64, 1 << 24, "TLAS_initExt() only instance count of <U24_MAX is supported"
		))

	//Convert to Vulkan dependent version

	VkAccelerationStructureGeometryKHR geometry = (VkAccelerationStructureGeometryKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR
	};

	if(tlas->base.asConstructionType == ETLASConstructionType_Instances) {

		DeviceData instances = tlas->deviceData;

		//We have to temporarily allocate a device mem buffer

		if (!tlas->useDeviceMemory) {

			gotoIfError(clean, CharString_formatx(
				&tmp, "%.*s instances buffer", CharString_length(tlas->base.name), tlas->base.name.ptr
			))

			gotoIfError(clean, GraphicsDeviceRef_createBuffer(
				deviceRef,
				EDeviceBufferUsage_ASReadExt,
				EGraphicsResourceFlag_CPUAllocatedBit,
				tmp,
				stride * instancesU64,
				&tlas->tempInstanceBuffer
			))

			CharString_freex(&tmp);

			//Directly copy the data is allowed, because it's not in flight and it's on the CPU

			DeviceBuffer *tempInstanceBuf = DeviceBufferRef_ptr(tlas->tempInstanceBuffer);
			void *mem = tempInstanceBuf->resource.mappedMemoryExt;

			Buffer_copy(
				Buffer_createRef(mem, stride * instancesU64),
				Buffer_createRefConst(tlas->cpuInstancesStatic.ptr, stride * instancesU64)	//static, motion same pos
			);

			//We have to transform the CPU-sided buffer to a GPU buffer address

			for (U64 i = 0; i < instancesU64; ++i) {

				TLASInstanceData *dat = NULL;
				TLAS_getInstanceDataCpuInternal(tlas, i, &dat);

				if(!dat->blasCpu)
					continue;

				const U64 *blasAddress = &dat->blasDeviceAddress;
				U64 offset = (const U8*)blasAddress - (((const U8*)tlas->cpuInstancesStatic.ptr) + stride * i);

				*(U64*)((U8*)mem + offset) = getVkDeviceAddress((DeviceData) {
					.buffer = BLASRef_ptr(dat->blasCpu)->base.asBuffer
				});
			}

			DeviceMemoryBlock block = device->allocator.blocks.ptr[tempInstanceBuf->resource.blockId];
			Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			if(incoherent) {

				VkMappedMemoryRange mappedRange = (VkMappedMemoryRange) {
					.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
					.memory = (VkDeviceMemory) block.ext,
					.offset = tempInstanceBuf->resource.blockOffset,
					.size = stride * instancesU64
				};

				gotoIfError(clean, vkCheck(vkFlushMappedMemoryRanges(deviceExt->device, 1, &mappedRange)))
			}

			instances = (DeviceData) { .buffer = tlas->tempInstanceBuffer, .len = stride * instancesU64 };
		}

		geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;

		geometry.geometry.instances = (VkAccelerationStructureGeometryInstancesDataKHR) {
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
			.data = getVkLocation(instances, 0)
		};
	}

	tlasExt->geometry = geometry;

	VkBuildAccelerationStructureFlagsKHR flags = (VkBuildAccelerationStructureFlagsKHR) 0;

	if(tlas->base.flags & ERTASBuildFlags_AllowUpdate)
		flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;

	if(tlas->base.flags & ERTASBuildFlags_AllowCompaction)
		flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;

	if(tlas->base.flags & ERTASBuildFlags_FastTrace)
		flags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

	if(tlas->base.flags & ERTASBuildFlags_FastBuild)
		flags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;

	if(tlas->base.flags & ERTASBuildFlags_MinimizeMemory)
		flags |= VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR;

	if(tlas->base.isMotionBlurExt)
		flags |= VK_BUILD_ACCELERATION_STRUCTURE_MOTION_BIT_NV;

	tlasExt->geometries = (VkAccelerationStructureBuildGeometryInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
		.flags = flags,
		.geometryCount = 1,
		.pGeometries = &tlasExt->geometry
	};

	//Get build size to allocate scratch and final buffer

	U32 instancesU32 = (U32) instancesU64;
	VkAccelerationStructureBuildSizesInfoKHR sizes = (VkAccelerationStructureBuildSizesInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
	};

	tlasExt->range = (VkAccelerationStructureBuildRangeInfoKHR) { .primitiveCount = instancesU32 };

	instanceExt->getAccelerationStructureBuildSizes(
		deviceExt->device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&tlasExt->geometries,
		&instancesU32,
		&sizes
	);

	//Allocate scratch and final buffer

	gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_ASExt,
		EGraphicsResourceFlag_None,
		tlas->base.name,
		sizes.accelerationStructureSize,
		&tlas->base.asBuffer
	))

	gotoIfError(clean, CharString_formatx(
		&tmp, "%.*s scratch buffer", CharString_length(tlas->base.name), tlas->base.name.ptr
	))

	gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_ScratchExt,
		EGraphicsResourceFlag_None,
		tmp,
		tlas->base.flags & ERTASBuildFlags_IsUpdate ? sizes.updateScratchSize : sizes.buildScratchSize,
		&tlas->base.tempScratchBuffer
	))

	CharString_freex(&tmp);

	VkAccelerationStructureCreateInfoKHR createInfo = (VkAccelerationStructureCreateInfoKHR) {
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
		.buffer = DeviceBuffer_ext(DeviceBufferRef_ptr(tlas->base.asBuffer), Vk)->buffer,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
		.size = sizes.accelerationStructureSize
	};

	gotoIfError(clean, vkCheck(instanceExt->createAccelerationStructure(deviceExt->device, &createInfo, NULL, &tlasExt->as)))

	//Queue build

	if(tlas->base.parent)
		tlasExt->geometries.srcAccelerationStructure = TLAS_ext(TLASRef_ptr(tlas->base.parent), Vk)->as;

	tlasExt->geometries.dstAccelerationStructure = tlasExt->as;

	tlasExt->geometries.scratchData = (VkDeviceOrHostAddressKHR) {
		.deviceAddress = DeviceBufferRef_ptr(tlas->base.tempScratchBuffer)->resource.deviceAddress
	};

	if(tlas->base.flags & ERTASBuildFlags_IsUpdate)
		tlasExt->geometries.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;

	//Add as descriptor

	VkWriteDescriptorSetAccelerationStructureKHR tlasDesc = (VkWriteDescriptorSetAccelerationStructureKHR) {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
		.accelerationStructureCount = 1,
		.pAccelerationStructures = &tlasExt->geometries.dstAccelerationStructure
	};

	VkWriteDescriptorSet descriptor = (VkWriteDescriptorSet) {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstBinding = EDescriptorType_TLASExt - 1,				//Sampler is skipped
		.dstArrayElement = ResourceHandle_getId(tlas->handle),
		.dstSet = deviceExt->sets[EDescriptorSetType_Resources],
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
		.pNext = &tlasDesc
	};

	vkUpdateDescriptorSets(deviceExt->device, 1, &descriptor, 0, NULL);

clean:
	return err;
}

Bool TLAS_freeExt(TLAS *tlas) {

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(tlas->base.device);
	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	const GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	const VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	const VkAccelerationStructureKHR as = TLAS_ext(tlas, Vk)->as;

	if(as)
		instanceExt->destroyAccelerationStructure(deviceExt->device, as, NULL);

	return true;
}

Error TLASRef_flush(void *commandBufferExt, GraphicsDeviceRef *deviceRef, TLASRef *pending) {

	VkCommandBufferState *commandBuffer = (VkCommandBufferState*) commandBufferExt;

	TLAS *tlas = TLASRef_ptr(pending);
	VkTLAS *tlasExt = TLAS_ext(tlas, Vk);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	ListRefPtr *currentFlight = &device->resourcesInFlight[(device->submitId - 1) % 3];

	if(tlas->base.isCompleted && !(tlas->base.flags & ERTASBuildFlags_AllowUpdate))		//Done
		return Error_none();

	VkAccelerationStructureBuildRangeInfoKHR *range = &tlasExt->range;

	instanceExt->cmdBuildAccelerationStructures(
		commandBuffer->buffer,
		1,
		&tlasExt->geometries,
		&range
	);

	//Add as flight (keep alive extra)

	Error err = Error_none();
	gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, pending))
	RefPtr_inc(pending);

	//We mark scratch buffer as delete, we do this by pushing it as a current flight resource
	//And losing the reference from our object
	//We do the same thing on the tempInstances, since it's CPU mem only

	if(!(tlas->base.flags & ERTASBuildFlags_AllowUpdate)) {

		gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, tlas->base.tempScratchBuffer))
		tlas->base.tempScratchBuffer = NULL;

		if(tlas->tempInstanceBuffer) {
			gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, tlas->tempInstanceBuffer))
			tlas->tempInstanceBuffer = NULL;
		}
	}

	tlas->base.isCompleted = true;

clean:
	return err;
}
