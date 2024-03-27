/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/directx12/dx_buffer.h"
#include "graphics/directx12/dx_device.h"
#include "platforms/ext/bufferx.h"
#include "platforms/log.h"

U64 DeviceBufferExt_size = sizeof(DxDeviceBuffer);

Error DxDeviceBuffer_transition(
	DxDeviceBuffer *buffer,
	D3D12_BARRIER_SYNC sync,
	D3D12_BARRIER_ACCESS access,
	U64 offset,
	U64 size,
	ListD3D12_BUFFER_BARRIER *bufferBarriers,
	D3D12_BARRIER_GROUP *dependency
) {
	(void) buffer; (void)sync; (void)access; (void)offset; (void)size; (void)bufferBarriers; (void)dependency;
	return Error_unimplemented(0, "DxDeviceBuffer_transition() unimplemented");
}

/*
Error VkDeviceBuffer_transition(
	VkDeviceBuffer *buffer,
	VkPipelineStageFlags2 stage,
	VkAccessFlagBits2 access,
	U32 graphicsQueueId,
	U64 offset,
	U64 size,
	ListVkBufferMemoryBarrier2 *bufferBarriers,
	VkDependencyInfo *dependency
) {

	//No-op

	if(buffer->lastStage == stage && buffer->lastAccess == access)
		return Error_none();

	//Handle buffer barrier

	const VkBufferMemoryBarrier2 bufferBarrier = (VkBufferMemoryBarrier2) {

		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,

		.srcStageMask = buffer->lastStage,
		.srcAccessMask = buffer->lastAccess,

		.dstStageMask = stage,
		.dstAccessMask = access,

		.srcQueueFamilyIndex = graphicsQueueId,
		.dstQueueFamilyIndex = graphicsQueueId,

		.buffer = buffer->buffer,
		.offset = offset,
		.size = !size ? VK_WHOLE_SIZE : size
	};

	const Error err = ListVkBufferMemoryBarrier2_pushBackx(bufferBarriers, bufferBarrier);

	if(err.genericError)
		return err;

	buffer->lastStage = bufferBarrier.dstStageMask;
	buffer->lastAccess = bufferBarrier.dstAccessMask;

	dependency->pBufferMemoryBarriers = bufferBarriers->ptr;
	dependency->bufferMemoryBarrierCount = (U32) bufferBarriers->length;

	return Error_none();
}
*/

Bool DeviceBuffer_freeExt(DeviceBuffer *buffer) {
	(void) buffer;
	return false;
}

/*
	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(GraphicsDeviceRef_ptr(buffer->resource.device), Vk);
	VkDeviceBuffer *bufferExt = DeviceBuffer_ext(buffer, Vk);

	if(bufferExt->buffer) {
		vkDestroyBuffer(deviceExt->device, bufferExt->buffer, NULL);
		bufferExt->buffer = NULL;
	}

	return true;
}*/

Error GraphicsDeviceRef_createBufferExt(GraphicsDeviceRef *dev, DeviceBuffer *buf, CharString name) {

	(void)dev;
	(void)buf;
	(void)name;

	return Error_unimplemented(0, "GraphicsDeviceRef_createBufferExt() unimplemented");
}
/*

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	VkDeviceBuffer *bufExt = DeviceBuffer_ext(buf, Vk);

	VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	if(buf->resource.flags & EGraphicsResourceFlag_ShaderRW)
		usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	if(buf->usage & EDeviceBufferUsage_Vertex)
		usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	if(buf->usage & EDeviceBufferUsage_Index)
		usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	if(buf->usage & EDeviceBufferUsage_Indirect)
		usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

	if(buf->usage & EDeviceBufferUsage_ScratchExt)
		usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	if(buf->usage & EDeviceBufferUsage_ASExt)
		usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

	if(buf->usage & EDeviceBufferUsage_ASReadExt)
		usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

	if(buf->usage & EDeviceBufferUsage_SBTExt)
		usage |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;

	if(buf->resource.flags & EGraphicsResourceFlag_CPUAllocatedBit)		//Only for internal usage
		usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	VkBufferCreateInfo bufferInfo = (VkBufferCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = buf->resource.size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	Error err = Error_none();

	VkDeviceBufferMemoryRequirementsKHR bufferReq = (VkDeviceBufferMemoryRequirementsKHR) {
		.sType = VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS_KHR,
		.pCreateInfo = &bufferInfo
	};

	VkMemoryDedicatedRequirements dedicatedReq = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS
	};

	VkMemoryRequirements2 requirements = (VkMemoryRequirements2) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
		.pNext = &dedicatedReq
	};

	instanceExt->getDeviceBufferMemoryRequirements(deviceExt->device, &bufferReq, &requirements);

	gotoIfError(clean, DeviceMemoryAllocator_allocate(
		&device->allocator,
		&requirements,
		buf->resource.flags & EGraphicsResourceFlag_CPUAllocatedBit,
		&buf->resource.blockId,
		&buf->resource.blockOffset,
		EResourceType_DeviceBuffer,
		name
	))

	buf->resource.allocated = true;

	DeviceMemoryBlock block = device->allocator.blocks.ptr[buf->resource.blockId];

	//Bind memory

	gotoIfError(clean, vkCheck(vkCreateBuffer(deviceExt->device, &bufferInfo, NULL, &bufExt->buffer)))
	gotoIfError(clean, vkCheck(vkBindBufferMemory(
		deviceExt->device, bufExt->buffer, (VkDeviceMemory) block.ext, buf->resource.blockOffset
	)))

	if (block.mappedMemory)
		buf->resource.mappedMemoryExt = block.mappedMemory + buf->resource.blockOffset;

	//Grab GPU location

	VkBufferDeviceAddressInfo address = (VkBufferDeviceAddressInfo) {
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = bufExt->buffer
	};

	buf->resource.deviceAddress = vkGetBufferDeviceAddress(deviceExt->device, &address);

	if(!buf->resource.deviceAddress)
		gotoIfError(clean, Error_invalidState(0, "GraphicsDeviceRef_createBufferExt() Couldn't obtain GPU address"))

	//Fill relevant descriptor sets if shader accessible

	EGraphicsResourceFlag flags = buf->resource.flags;

	if(flags & EGraphicsResourceFlag_ShaderRW || (buf->usage & EDeviceBufferUsage_ASExt)) {

		//Create readonly buffer

		VkDescriptorBufferInfo bufferDesc = (VkDescriptorBufferInfo) { .buffer = bufExt->buffer, .range = buf->resource.size };

		VkWriteDescriptorSet descriptors[2] = {
			(VkWriteDescriptorSet) {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &bufferDesc
			}
		};

		U32 counter = 0;

		if (flags & EGraphicsResourceFlag_ShaderRead) {
			descriptors[counter].dstBinding = EDescriptorType_Buffer;
			descriptors[counter].dstArrayElement = ResourceHandle_getId(buf->readHandle);
			descriptors[counter].dstSet = deviceExt->sets[EDescriptorSetType_Resources];
			++counter;
		}

		if (flags & EGraphicsResourceFlag_ShaderWrite) {
			descriptors[counter] = descriptors[0];
			descriptors[counter].dstBinding = EDescriptorType_RWBuffer;
			descriptors[counter].dstArrayElement = ResourceHandle_getId(buf->writeHandle);
			descriptors[counter].dstSet = deviceExt->sets[EDescriptorSetType_Resources];
			++counter;
		}

		vkUpdateDescriptorSets(deviceExt->device, counter, descriptors, 0, NULL);
	}

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && CharString_length(name) && instanceExt->debugSetName) {

		VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = VK_OBJECT_TYPE_BUFFER,
			.pObjectName = name.ptr,
			.objectHandle = (U64) bufExt->buffer
		};

		gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName)))
	}

clean:
	return err;
}*/

Error DeviceBufferRef_flush(void *commandBufferExt, GraphicsDeviceRef *deviceRef, DeviceBufferRef *pending) {
	(void) commandBufferExt; (void) deviceRef; (void) pending;
	return Error_unimplemented(0, "DeviceBufferRef_flush() unimplemented");
}

/*
	VkCommandBuffer commandBuffer = (VkCommandBuffer) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	U32 graphicsQueueId = deviceExt->queues[EVkCommandQueue_Graphics].queueId;

	DeviceBuffer *buffer = DeviceBufferRef_ptr(pending);
	VkDeviceBuffer *bufferExt = DeviceBuffer_ext(buffer, Vk);

	Error err = Error_none();
	ListVkMappedMemoryRange tempMappedMemRanges = { 0 };
	ListVkBufferMemoryBarrier2 tempBufferBarriers = { 0 };

	Bool isInFlight = false;
	ListRefPtr *currentFlight = &device->resourcesInFlight[device->submitId % 3];
	DeviceBufferRef *tempStagingResource = NULL;
	ListVkBufferCopy pendingCopies = { 0 };

	for(U64 j = 0; j < sizeof(device->resourcesInFlight) / sizeof(device->resourcesInFlight[0]); ++j) {

		ListRefPtr inFlight = device->resourcesInFlight[j];

		for(U64 i = 0; i < inFlight.length; ++i)
			if (inFlight.ptr[i] == pending) {
				isInFlight = true;
				break;
			}

		if(isInFlight)
			break;
	}

	if(!isInFlight && buffer->resource.mappedMemoryExt) {

		DeviceMemoryBlock block = device->allocator.blocks.ptr[buffer->resource.blockId];
		Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		if(incoherent)
			gotoIfError(clean, ListVkMappedMemoryRange_resizex(&tempMappedMemRanges, buffer->pendingChanges.length + 1))

		for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {

			DevicePendingRange range = buffer->pendingChanges.ptr[j];

			U64 start = range.buffer.startRange;
			U64 len = range.buffer.endRange - range.buffer.startRange;

			Buffer dst = Buffer_createRef((U8*)buffer->resource.mappedMemoryExt + start, len);
			Buffer src = Buffer_createRefConst(buffer->cpuData.ptr + start, len);

			Buffer_copy(dst, src);

			if(incoherent)
				tempMappedMemRanges.ptrNonConst[j] = (VkMappedMemoryRange) {
					.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
					.memory = (VkDeviceMemory) block.ext,
					.offset = start + buffer->resource.blockOffset,
					.size = len
				};
		}

		if(incoherent)
			gotoIfError(clean, vkCheck(vkFlushMappedMemoryRanges(
				deviceExt->device, (U32) tempMappedMemRanges.length, tempMappedMemRanges.ptr
			)))
	}

	else {

		//TODO: Copy queue

		U64 allocRange = 0;

		for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {
			const BufferRange bufferj = buffer->pendingChanges.ptr[j].buffer;
			allocRange += bufferj.endRange - bufferj.startRange;
		}

		device->pendingBytes += allocRange;

		gotoIfError(clean, ListVkBufferCopy_resizex(&pendingCopies, buffer->pendingChanges.length))

		VkDependencyInfo dependency = (VkDependencyInfo) { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		gotoIfError(clean, ListVkBufferMemoryBarrier2_reservex(&tempBufferBarriers, 2 + buffer->pendingChanges.length))

		if (allocRange >= 16 * MIBI) {		//Resource is too big, allocate dedicated staging resource

			gotoIfError(clean, GraphicsDeviceRef_createBuffer(
				deviceRef,
				EDeviceBufferUsage_None, EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit,
				CharString_createRefCStrConst("Dedicated staging buffer"),
				allocRange, &tempStagingResource
			))

			DeviceBuffer *stagingResource = DeviceBufferRef_ptr(tempStagingResource);
			VkDeviceBuffer *stagingResourceExt = DeviceBuffer_ext(stagingResource, Vk);
			U8 *location = stagingResource->resource.mappedMemoryExt;

			DeviceMemoryBlock block = device->allocator.blocks.ptr[stagingResource->resource.blockId];
			Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			//Copy into our buffer

			allocRange = 0;

			for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {

				const BufferRange bufferj = buffer->pendingChanges.ptr[j].buffer;
				U64 len = bufferj.endRange - bufferj.startRange;

				Buffer_copy(
					Buffer_createRef(location + allocRange, len),
					Buffer_createRefConst(buffer->cpuData.ptr + bufferj.startRange, len)
				);

				gotoIfError(clean, VkDeviceBuffer_transition(
					bufferExt,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT,
					graphicsQueueId,
					bufferj.startRange,
					len,
					&tempBufferBarriers,
					&dependency
				))

				pendingCopies.ptrNonConst[j] = (VkBufferCopy) {
					.srcOffset = allocRange,
					.dstOffset = bufferj.startRange,
					.size = len
				};

				allocRange += len;
			}

			if(incoherent) {

				VkMappedMemoryRange memoryRange = (VkMappedMemoryRange) {
					.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
					.memory = (VkDeviceMemory) block.ext,
					.offset = stagingResource->resource.blockOffset,
					.size = allocRange
				};

				vkFlushMappedMemoryRanges(deviceExt->device, 1, &memoryRange);
			}

			gotoIfError(clean, VkDeviceBuffer_transition(
				stagingResourceExt,
				VK_PIPELINE_STAGE_2_COPY_BIT,
				VK_ACCESS_2_TRANSFER_READ_BIT,
				graphicsQueueId,
				0,
				allocRange,
				&tempBufferBarriers,
				&dependency
			))

			if(dependency.bufferMemoryBarrierCount)
				instanceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

			vkCmdCopyBuffer(
				commandBuffer,
				stagingResourceExt->buffer,
				bufferExt->buffer,
				(U32) pendingCopies.length,
				pendingCopies.ptr
			);

			//When staging resource is committed to current in flight then we can relinquish ownership.

			gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, tempStagingResource))
			tempStagingResource = NULL;
		}

		//Use staging buffer

		else {

			gotoIfError(clean, ListVkBufferCopy_resizex(&pendingCopies, buffer->pendingChanges.length))

			AllocationBuffer *stagingBuffer = &device->stagingAllocations[device->submitId % 3];
			DeviceBuffer *staging = DeviceBufferRef_ptr(device->staging);
			VkDeviceBuffer *stagingExt = DeviceBuffer_ext(staging, Vk);

			U8 *defaultLocation = (U8*) 1, *location = defaultLocation;
			Error temp = AllocationBuffer_allocateBlockx(stagingBuffer, allocRange, 4, (const U8**) &location);

			if(temp.genericError && location == defaultLocation)		//Something else went wrong
				gotoIfError(clean, temp)

			//We re-create the staging buffer to fit the new allocation.

			if (temp.genericError) {

				U64 prevSize = DeviceBufferRef_ptr(device->staging)->resource.size;

				//Allocate new staging buffer.

				U64 newSize = prevSize * 2 + allocRange * 3;
				gotoIfError(clean, GraphicsDeviceRef_resizeStagingBuffer(deviceRef, newSize))
				gotoIfError(clean, AllocationBuffer_allocateBlockx(stagingBuffer, allocRange, 4, (const U8**) &location))

				staging = DeviceBufferRef_ptr(device->staging);
				stagingExt = DeviceBuffer_ext(staging, Vk);
			}

			DeviceMemoryBlock block = device->allocator.blocks.ptr[staging->resource.blockId];
			Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			//Copy into our buffer

			allocRange = 0;

			for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {

				const BufferRange bufferj = buffer->pendingChanges.ptr[j].buffer;
				U64 len = bufferj.endRange - bufferj.startRange;

				Buffer_copy(
					Buffer_createRef(location + allocRange, len),
					Buffer_createRefConst(buffer->cpuData.ptr + bufferj.startRange, len)
				);

				pendingCopies.ptrNonConst[j] = (VkBufferCopy) {
					.srcOffset = allocRange + (location - stagingBuffer->buffer.ptr),
					.dstOffset = bufferj.startRange,
					.size = len
				};

				gotoIfError(clean, VkDeviceBuffer_transition(
					bufferExt,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT,
					graphicsQueueId,
					bufferj.startRange,
					len,
					&tempBufferBarriers,
					&dependency
				))

				allocRange += len;
			}

			if (incoherent) {

				VkMappedMemoryRange memoryRange = (VkMappedMemoryRange) {
					.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
					.memory = (VkDeviceMemory) block.ext,
					.offset = location - block.mappedMemory,
					.size = allocRange
				};

				vkFlushMappedMemoryRanges(deviceExt->device, 1, &memoryRange);
			}

			if(!ListRefPtr_contains(*currentFlight, device->staging, 0, NULL)) {

				gotoIfError(clean, VkDeviceBuffer_transition(						//Ensure resource is transitioned
					stagingExt,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_READ_BIT,
					graphicsQueueId,
					(device->submitId % 3) * (staging->resource.size / 3),
					staging->resource.size / 3,
					&tempBufferBarriers,
					&dependency
				))

				RefPtr_inc(device->staging);
				gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, device->staging))		//Add to in flight
			}

			if(dependency.bufferMemoryBarrierCount)
				instanceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

			vkCmdCopyBuffer(
				commandBuffer,
				stagingExt->buffer,
				bufferExt->buffer,
				(U32) buffer->pendingChanges.length,
				pendingCopies.ptr
			);
		}
	}

	if(!(buffer->resource.flags & EGraphicsResourceFlag_CPUBacked))
		Buffer_freex(&buffer->cpuData);

	buffer->isFirstFrame = buffer->isPending = buffer->isPendingFullCopy = false;
	gotoIfError(clean, ListDevicePendingRange_clear(&buffer->pendingChanges))

	if(RefPtr_inc(pending))
		gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, pending))

	if (device->pendingBytes >= device->flushThreshold)
		VkGraphicsDevice_flush(deviceRef, commandBuffer);

clean:
	DeviceBufferRef_dec(&tempStagingResource);
	ListVkMappedMemoryRange_freex(&tempMappedMemRanges);
	ListVkBufferMemoryBarrier2_freex(&tempBufferBarriers);
	ListVkBufferCopy_freex(&pendingCopies);
	return err;
}
*/