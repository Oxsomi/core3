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

#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vulkan.h"
#include "graphics/vulkan/vk_buffer.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/log.h"
#include "types/buffer.h"
#include "types/list.h"
#include "types/error.h"

U64 DeviceBufferExt_size = sizeof(VkDeviceBuffer);

Error VkDeviceBuffer_transition(
	VkDeviceBuffer *buffer,
	VkPipelineStageFlags2 stage,
	VkAccessFlagBits2 access,
	U32 graphicsQueueId,
	U64 offset,
	U64 size,
	List *bufferBarriers,
	VkDependencyInfo *dependency
) {

	//No-op

	if(buffer->lastStage == stage && buffer->lastAccess == access)
		return Error_none();

	//Handle buffer barrier

	VkBufferMemoryBarrier2 bufferBarrier = (VkBufferMemoryBarrier2) {

		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,

		.srcStageMask = buffer->lastStage,
		.srcAccessMask = buffer->lastAccess,

		.dstStageMask = stage,
		.dstAccessMask = access,

		.srcQueueFamilyIndex = graphicsQueueId,
		.dstQueueFamilyIndex = graphicsQueueId,

		.buffer = buffer->buffer,
		.offset = offset,
		.size = size
	};

	Error err = List_pushBackx(bufferBarriers, Buffer_createConstRef(&bufferBarrier, sizeof(bufferBarrier)));

	if(err.genericError)
		return err;

	buffer->lastStage = bufferBarrier.dstStageMask;
	buffer->lastAccess = bufferBarrier.dstAccessMask;

	dependency->pBufferMemoryBarriers = (const VkBufferMemoryBarrier2*) bufferBarriers->ptr;
	dependency->bufferMemoryBarrierCount = (U32) bufferBarriers->length;

	return Error_none();
}


Bool DeviceBuffer_freeExt(DeviceBuffer *buffer) {

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(GraphicsDeviceRef_ptr(buffer->device), Vk);

	if(buffer->writeHandle != U32_MAX || buffer->readHandle != U32_MAX) {

		ELockAcquire acq = Lock_lock(&deviceExt->descriptorLock, U64_MAX);

		if(acq >= ELockAcquire_Success) {
			U32 allocations[2] = { buffer->readHandle, buffer->writeHandle };
			List allocationList = (List) { 0 };
			List_createConstRef((const U8*) allocations, 2, sizeof(U32), &allocationList);
			VkGraphicsDevice_freeAllocations(deviceExt, &allocationList);
		}

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(&deviceExt->descriptorLock);
	}

	VkDeviceBuffer *bufferExt = DeviceBuffer_ext(buffer, Vk);

	if(bufferExt->buffer)
		vkDestroyBuffer(deviceExt->device, bufferExt->buffer, NULL);

	return true;
}

Error GraphicsDeviceRef_createBufferExt(GraphicsDeviceRef *dev, DeviceBuffer *buf, CharString name) {

	name;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	VkBuffer tempBuffer = NULL;

	VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	if(buf->usage & (EDeviceBufferUsage_ShaderRead | EDeviceBufferUsage_ShaderWrite))
		usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	if(buf->usage & EDeviceBufferUsage_Vertex)
		usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	if(buf->usage & EDeviceBufferUsage_Index)
		usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	if(buf->usage & EDeviceBufferUsage_Indirect)
		usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

	if(buf->usage & EDeviceBufferUsage_CPUAllocatedBit)		//Only for internal usage
		usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	VkBufferCreateInfo bufferInfo = (VkBufferCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = buf->length,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	Error err = Error_none();
	ELockAcquire acq = ELockAcquire_Invalid;

	U32 blockId = U32_MAX;
	U64 blockOffset = 0;

	U32 locationRead = U32_MAX;
	U32 locationWrite = U32_MAX;

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

	_gotoIfError(clean, DeviceMemoryAllocator_allocate(
		&device->allocator, 
		&requirements, 
		buf->usage & EDeviceBufferUsage_CPUAllocatedBit, 
		&blockId, 
		&blockOffset,
		EResourceType_Buffer,
		name
	));

	const DeviceMemoryBlock *block = (const DeviceMemoryBlock*)List_ptrConst(device->allocator.blocks, blockId);

	//Bind memory

	_gotoIfError(clean, vkCheck(vkCreateBuffer(deviceExt->device, &bufferInfo, NULL, &tempBuffer)));
	_gotoIfError(clean, vkCheck(vkBindBufferMemory(deviceExt->device, tempBuffer, (VkDeviceMemory) block->ext, blockOffset)));

	U8 *memoryLocation = block->mappedMemory;

	if (memoryLocation)
		memoryLocation += blockOffset;
	
	//Fill relevant descriptor sets if shader accessible

	if(buf->usage & (EDeviceBufferUsage_ShaderRead | EDeviceBufferUsage_ShaderWrite)) {

		acq = Lock_lock(&deviceExt->descriptorLock, U64_MAX);

		if(acq < ELockAcquire_Success)
			_gotoIfError(clean, Error_invalidState(0));

		//Create readonly buffer

		VkDescriptorBufferInfo bufferDesc = (VkDescriptorBufferInfo) { .buffer = tempBuffer, .range = buf->length };

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

		if (usage & EDeviceBufferUsage_ShaderRead) {

			locationRead = VkGraphicsDevice_allocateDescriptor(deviceExt, EDescriptorType_Buffer);

			if(locationRead == U32_MAX)
				_gotoIfError(clean, Error_outOfMemory(0));

			descriptors[0].dstBinding = EDescriptorType_Buffer - 1;
			descriptors[0].dstArrayElement = locationRead & ((1 << 20) - 1);
			descriptors[0].dstSet = deviceExt->sets[EDescriptorSetType_Resources];

			++counter;
		}

		if (usage & EDeviceBufferUsage_ShaderWrite) {

			locationWrite = VkGraphicsDevice_allocateDescriptor(deviceExt, EDescriptorType_RWBuffer);

			if(locationWrite == U32_MAX)
				_gotoIfError(clean, Error_outOfMemory(0));

			descriptors[counter] = descriptors[0];
			descriptors[counter].dstBinding = EDescriptorType_RWBuffer - 1;
			descriptors[counter].dstArrayElement = locationWrite & ((1 << 20) - 1);
			descriptors[counter].dstSet = deviceExt->sets[EDescriptorSetType_Resources];

			++counter;
		}

		vkUpdateDescriptorSets(deviceExt->device, counter, descriptors, 0, NULL);
	}

	if (name.ptr) {
	
		#ifndef NDEBUG

			if(instanceExt->debugSetName) {

				VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_BUFFER,
					.pObjectName = name.ptr,
					.objectHandle = (U64) tempBuffer
				};

				_gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName)));
			}

		#endif
	}

	*DeviceBuffer_ext(buf, Vk) = (VkDeviceBuffer) { .buffer = tempBuffer };
	
	buf->blockOffset = blockOffset;
	buf->blockId = blockId;
	buf->mappedMemory = memoryLocation;
	buf->readHandle = locationRead;
	buf->writeHandle = locationWrite;

clean:

	if(err.genericError) {

		U32 allocations[2] = { locationRead, locationWrite };
		List allocationList = (List) { 0 };
		List_createConstRef((const U8*) allocations, 2, sizeof(U32), &allocationList);
		VkGraphicsDevice_freeAllocations(deviceExt, &allocationList);

		vkDestroyBuffer(deviceExt->device, tempBuffer, NULL);

		if(blockId != U32_MAX)
			DeviceMemoryAllocator_freeAllocation(&device->allocator, blockId, blockOffset);
	}

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&deviceExt->descriptorLock);

	return err;
}

Error DeviceBufferRef_flush(void *commandBufferExt, GraphicsDeviceRef *deviceRef, DeviceBufferRef *pending) {

	VkCommandBuffer commandBuffer = (VkCommandBuffer) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	U32 graphicsQueueId = deviceExt->queues[EVkCommandQueue_Graphics].queueId;

	DeviceBuffer *buffer = DeviceBufferRef_ptr(pending);
	VkDeviceBuffer *bufferExt = DeviceBuffer_ext(buffer, Vk);

	Error err = Error_none();
	List tempList = (List) { 0 };

	Bool isInFlight = false;
	List *currentFlight = &device->resourcesInFlight[device->submitId % 3];
	DeviceBufferRef *tempStagingResource = NULL;
	List pendingCopies = List_createEmpty(sizeof(VkBufferCopy));

	for(U64 j = 0; j < sizeof(device->resourcesInFlight) / sizeof(device->resourcesInFlight[0]); ++j) {

		List inFlight = device->resourcesInFlight[j];

		for(U64 i = 0; i < inFlight.length; ++i)
			if (((RefPtr**)inFlight.ptr)[i] == pending) {
				isInFlight = true;
				break;
			}

		if(isInFlight)
			break;
	}

	if(!isInFlight && buffer->mappedMemory) {

		DeviceMemoryBlock block = *(const DeviceMemoryBlock*) List_ptr(device->allocator.blocks, buffer->blockId);
		Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		if(incoherent) {
			tempList = List_createEmpty(sizeof(VkMappedMemoryRange));
			_gotoIfError(clean, List_resizex(&tempList, buffer->pendingChanges.length + 1));
		}

		for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {

			DevicePendingRange range = *(DevicePendingRange*) List_ptr(buffer->pendingChanges, j);

			U64 start = range.buffer.startRange;
			U64 len = range.buffer.endRange - range.buffer.startRange;

			Buffer dst = Buffer_createRef((U8*)buffer->mappedMemory + start, len);
			Buffer src = Buffer_createConstRef(buffer->cpuData.ptr + start, len);

			Buffer_copy(dst, src);

			if(incoherent)
				*(VkMappedMemoryRange*)List_ptr(tempList, j) = (VkMappedMemoryRange) {
					.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
					.memory = (VkDeviceMemory) block.ext,
					.offset = start + buffer->blockOffset,
					.size = len
				};
		}

		if(incoherent)
			_gotoIfError(clean, vkCheck(vkFlushMappedMemoryRanges(
				deviceExt->device, (U32) tempList.length, (const VkMappedMemoryRange*) tempList.ptr
			)));
	}

	else {

		//TODO: Copy queue

		U64 allocRange = 0;

		for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {
			BufferRange bufferj = ((DevicePendingRange*) List_ptr(buffer->pendingChanges, j))->buffer;
			allocRange += bufferj.endRange - bufferj.startRange;
		}

		device->pendingBytes += allocRange;

		_gotoIfError(clean, List_resizex(&pendingCopies, buffer->pendingChanges.length));

		VkDependencyInfo dependency = (VkDependencyInfo) {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.dependencyFlags = 0
		};

		List_freex(&tempList);
		tempList = List_createEmpty(sizeof(VkBufferMemoryBarrier2));
		_gotoIfError(clean, List_reservex(&tempList, 2 + buffer->pendingChanges.length));

		if (allocRange >= 16 * MIBI) {		//Resource is too big, allocate dedicated staging resource

			_gotoIfError(clean, GraphicsDeviceRef_createBuffer(
				deviceRef, EDeviceBufferUsage_CPUAllocatedBit, CharString_createConstRefCStr("Dedicated staging buffer"),
				allocRange, &tempStagingResource
			));

			DeviceBuffer *stagingResource = DeviceBufferRef_ptr(tempStagingResource);
			VkDeviceBuffer *stagingResourceExt = DeviceBuffer_ext(stagingResource, Vk);
			U8 *location = stagingResource->mappedMemory;

			DeviceMemoryBlock block = *(DeviceMemoryBlock*) List_ptr(device->allocator.blocks, stagingResource->blockId);
			Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			//Copy into our buffer

			allocRange = 0;

			for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {

				BufferRange bufferj = ((const DevicePendingRange*) List_ptr(buffer->pendingChanges, j))->buffer;
				U64 len = bufferj.endRange - bufferj.startRange;

				Buffer_copy(
					Buffer_createRef(location + allocRange, len), 
					Buffer_createConstRef(buffer->cpuData.ptr + bufferj.startRange, len)
				);

				_gotoIfError(clean, VkDeviceBuffer_transition(
					bufferExt, 
					VK_PIPELINE_STAGE_2_COPY_BIT, 
					VK_ACCESS_2_TRANSFER_WRITE_BIT,
					graphicsQueueId,
					bufferj.startRange,
					len,
					&tempList,
					&dependency
				));

				((VkBufferCopy*)pendingCopies.ptr)[j] = (VkBufferCopy) {
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
					.offset = stagingResource->blockOffset,
					.size = allocRange
				};

				vkFlushMappedMemoryRanges(deviceExt->device, 1, &memoryRange);
			}

			_gotoIfError(clean, VkDeviceBuffer_transition(
				stagingResourceExt, 
				VK_PIPELINE_STAGE_2_COPY_BIT, 
				VK_ACCESS_2_TRANSFER_READ_BIT,
				graphicsQueueId,
				0,
				allocRange,
				&tempList,
				&dependency
			));

			if(dependency.bufferMemoryBarrierCount)
				instanceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

			vkCmdCopyBuffer(
				commandBuffer, 
				stagingResourceExt->buffer, 
				bufferExt->buffer, 
				(U32) pendingCopies.length, 
				(const VkBufferCopy*) pendingCopies.ptr
			);

			//When staging resource is commited to current in flight then we can relinguish ownership.

			_gotoIfError(clean, List_pushBackx(currentFlight, Buffer_createConstRef(&tempStagingResource, sizeof(RefPtr*))));
			tempStagingResource = NULL;
		}

		//Use staging buffer

		else {

			_gotoIfError(clean, List_resizex(&pendingCopies, buffer->pendingChanges.length));

			AllocationBuffer *stagingBuffer = &device->stagingAllocations[device->submitId % 3];
			DeviceBuffer *staging = DeviceBufferRef_ptr(device->staging);
			VkDeviceBuffer *stagingExt = DeviceBuffer_ext(staging, Vk);

			U8 *defaultLocation = (U8*) 1, *location = defaultLocation;
			Error temp = AllocationBuffer_allocateBlockx(stagingBuffer, allocRange, 4, (const U8**) &location);

			if(temp.genericError && location == defaultLocation)		//Something else went wrong
				_gotoIfError(clean, temp);

			//We re-create the staging buffer to fit the new allocation.

			if (temp.genericError) {

				U64 prevSize = DeviceBufferRef_ptr(device->staging)->length;

				//Allocate new staging buffer.

				U64 newSize = prevSize * 2 + allocRange * 3;
				_gotoIfError(clean, GraphicsDeviceRef_resizeStagingBuffer(deviceRef, newSize));
				_gotoIfError(clean, AllocationBuffer_allocateBlockx(stagingBuffer, allocRange, 4, (const U8**) &location));

				staging = DeviceBufferRef_ptr(device->staging);
				stagingExt = DeviceBuffer_ext(staging, Vk);
			}

			DeviceMemoryBlock block = *(const DeviceMemoryBlock*) List_ptr(device->allocator.blocks, staging->blockId);
			Bool incoherent = !(block.allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			//Copy into our buffer

			allocRange = 0;

			for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {

				BufferRange bufferj = ((DevicePendingRange*) List_ptr(buffer->pendingChanges, j))->buffer;
				U64 len = bufferj.endRange - bufferj.startRange;

				Buffer_copy(
					Buffer_createRef(location + allocRange, len), 
					Buffer_createConstRef(buffer->cpuData.ptr + bufferj.startRange, len)
				);

				((VkBufferCopy*)pendingCopies.ptr)[j] = (VkBufferCopy) {
					.srcOffset = allocRange + (location - stagingBuffer->buffer.ptr),
					.dstOffset = bufferj.startRange,
					.size = len
				};

				_gotoIfError(clean, VkDeviceBuffer_transition(
					bufferExt, 
					VK_PIPELINE_STAGE_2_COPY_BIT, 
					VK_ACCESS_2_TRANSFER_WRITE_BIT,
					graphicsQueueId,
					bufferj.startRange,
					len,
					&tempList,
					&dependency
				));

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

			Buffer stagingBuf = Buffer_createConstRef(&device->staging, sizeof(RefPtr*));
			if(!List_contains(*currentFlight, stagingBuf, 0)) {

				_gotoIfError(clean, VkDeviceBuffer_transition(						//Ensure resource is transitioned
					stagingExt, 
					VK_PIPELINE_STAGE_2_COPY_BIT, 
					VK_ACCESS_2_TRANSFER_READ_BIT,
					graphicsQueueId,
					(device->submitId % 3) * (staging->length / 3),
					staging->length / 3,
					&tempList,
					&dependency
				));

				RefPtr_inc(device->staging);
				_gotoIfError(clean, List_pushBackx(currentFlight, stagingBuf));		//Add to in flight
			}

			if(dependency.bufferMemoryBarrierCount)
				instanceExt->cmdPipelineBarrier2(commandBuffer, &dependency);

			vkCmdCopyBuffer(
				commandBuffer, 
				stagingExt->buffer,
				bufferExt->buffer, 
				(U32) buffer->pendingChanges.length, 
				(const VkBufferCopy*) pendingCopies.ptr
			);
		}
	}

	if(!(buffer->usage & EDeviceBufferUsage_CPUBacked))
		Buffer_freex(&buffer->cpuData);

	buffer->isFirstFrame = buffer->isPending = buffer->isPendingFullCopy = false;
	_gotoIfError(clean, List_clear(&buffer->pendingChanges));

	if(RefPtr_inc(pending))
		_gotoIfError(clean, List_pushBackx(currentFlight, Buffer_createConstRef(&pending, sizeof(pending))));

	if (device->pendingBytes >= device->flushThreshold) {

		//End current command list

		_gotoIfError(clean, vkCheck(vkEndCommandBuffer(commandBuffer)));

		//Submit only the copy command list

		U64 waitValue = device->submitId - 1;

		VkTimelineSemaphoreSubmitInfo timelineInfo = (VkTimelineSemaphoreSubmitInfo) {
			.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
			.waitSemaphoreValueCount = device->submitId > 0,
			.pWaitSemaphoreValues = device->submitId > 0 ? &waitValue : NULL,
		};

		VkSubmitInfo submitInfo = (VkSubmitInfo) {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = &timelineInfo,
			.waitSemaphoreCount = timelineInfo.waitSemaphoreValueCount,
			.pWaitSemaphores = (VkSemaphore*) &deviceExt->commitSemaphore,
			.pCommandBuffers = &commandBuffer,
			.commandBufferCount = 1
		};

		VkCommandQueue queue = deviceExt->queues[EVkCommandQueue_Graphics];
		_gotoIfError(clean, vkCheck(vkQueueSubmit(queue.queue, 1, &submitInfo, VK_NULL_HANDLE)));

		//Wait for the device

		_gotoIfError(clean, GraphicsDeviceRef_wait(deviceRef));

		//Reset command list

		U32 threadId = 0;

		VkCommandAllocator *allocator = VkGraphicsDevice_getCommandAllocator(
			deviceExt, queue.resolvedQueueId, threadId, (U8)(device->submitId % 3)
		);

		_gotoIfError(clean, vkCheck(vkResetCommandPool(
			deviceExt->device, allocator->pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT
		)));

		//Re-open

		VkCommandBufferBeginInfo beginInfo = (VkCommandBufferBeginInfo) {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
		};

		_gotoIfError(clean, vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo)));
	}

clean:
	DeviceBufferRef_dec(&tempStagingResource);
	List_freex(&tempList);
	List_freex(&pendingCopies);
	return err;
}
