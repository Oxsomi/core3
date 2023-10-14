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

#include "graphics/generic/buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vulkan.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "types/error.h"

U64 GPUBufferExt_size = sizeof(VkGPUBuffer);

Bool GPUBuffer_freeExt(GPUBuffer *buffer) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(buffer->device);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	VkGPUBuffer *bufferExt = GPUBuffer_ext(buffer, Vk);

	if(bufferExt->buffer) {

		Lock_lock(&deviceExt->descriptorLock, U64_MAX);

		U32 allocations[2] = { bufferExt->readDescriptor, bufferExt->writeDescriptor };
		List allocationList = (List) { 0 };
		List_createConstRef((const U8*) allocations, 2, sizeof(U32), &allocationList);
		VkGraphicsDevice_freeAllocations(deviceExt, &allocationList);

		Lock_unlock(&deviceExt->descriptorLock);

		GPUAllocator_freeAllocation(&device->allocator, bufferExt->blockId, bufferExt->blockOffset);
		vkDestroyBuffer(deviceExt->device, bufferExt->buffer, NULL);
	}

	return true;
}

Error GraphicsDeviceRef_createBufferExt(GraphicsDeviceRef *dev, GPUBuffer *buf) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Vk);

	VkBuffer tempBuffer = NULL;

	VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	if(buf->usage & (EGPUBufferUsage_ShaderRead | EGPUBufferUsage_ShaderWrite))
		usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	if(buf->usage & EGPUBufferUsage_Vertex)
		usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	if(buf->usage & EGPUBufferUsage_Index)
		usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	if(buf->usage & EGPUBufferUsage_Indirect)
		usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

	VkBufferCreateInfo bufferInfo = (VkBufferCreateInfo) {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = buf->length,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_CONCURRENT,
		.queueFamilyIndexCount = deviceExt->resolvedQueues,
		.pQueueFamilyIndices = deviceExt->uniqueQueues
	};

	Error err = Error_none();

	U32 blockId = U32_MAX;
	U64 blockOffset = 0;

	Bool lock = false;
	U32 locationRead = U32_MAX;
	U32 locationWrite = U32_MAX;

	_gotoIfError(clean, vkCheck(vkCreateBuffer(deviceExt->device, &bufferInfo, NULL, &tempBuffer)));

	VkBufferMemoryRequirementsInfo2 bufferReq = (VkBufferMemoryRequirementsInfo2) { 
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
		.buffer = tempBuffer
	};

	VkMemoryDedicatedRequirements dedicatedReq = { 
		.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS 
	};

	VkMemoryRequirements2 requirements = (VkMemoryRequirements2) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
		.pNext = &dedicatedReq
	};

	instanceExt->getBufferMemoryRequirements2(deviceExt->device, &bufferReq, &requirements);

	_gotoIfError(clean, GPUAllocator_allocate(&device->allocator, &requirements, &blockId, &blockOffset));

	GPUBlock *block = (GPUBlock*)List_ptr(device->allocator.blocks, blockId);

	_gotoIfError(clean, vkCheck(vkBindBufferMemory(deviceExt->device, tempBuffer, (VkDeviceMemory) block->ext, 0)));
	
	//Fill relevant descriptor sets if shader accessible

	if(buf->usage & (EGPUBufferUsage_ShaderRead | EGPUBufferUsage_ShaderWrite)) {

		if(!lock && !Lock_lock(&deviceExt->descriptorLock, U64_MAX))
			_gotoIfError(clean, Error_invalidState(0));

		lock = true;

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

		if (usage & EGPUBufferUsage_ShaderRead) {

			locationRead = VkGraphicsDevice_allocateDescriptor(deviceExt, EDescriptorType_Buffer);

			if(locationRead == U32_MAX)
				_gotoIfError(clean, Error_outOfMemory(0));

			descriptors[0].dstArrayElement = locationRead & ((1 << 20) - 1);
			descriptors[0].dstSet = deviceExt->sets[EDescriptorType_Buffer];

			++counter;
		}

		if (usage & EGPUBufferUsage_ShaderWrite) {

			locationWrite = VkGraphicsDevice_allocateDescriptor(deviceExt, EDescriptorType_RWBuffer);

			if(locationWrite == U32_MAX)
				_gotoIfError(clean, Error_outOfMemory(0));

			descriptors[counter].dstArrayElement = locationWrite & ((1 << 20) - 1);
			descriptors[counter].dstSet = deviceExt->sets[EDescriptorType_RWBuffer];

			++counter;
		}

		vkUpdateDescriptorSets(deviceExt->device, counter, descriptors, 0, NULL);
	}

	U8 *memoryLocation = (U8*)block->mappedMemory;

	if (memoryLocation)
		memoryLocation += blockOffset;

	*GPUBuffer_ext(buf, Vk) = (VkGPUBuffer) {
		.buffer = tempBuffer,
		.blockOffset = blockOffset,
		.mappedMemory = memoryLocation,
		.blockId = blockId,
		.readDescriptor = locationRead,
		.writeDescriptor = locationWrite
	};

clean:

	if(err.genericError) {

		U32 allocations[2] = { locationRead, locationWrite };
		List allocationList = (List) { 0 };
		List_createConstRef((const U8*) allocations, 2, sizeof(U32), &allocationList);
		VkGraphicsDevice_freeAllocations(deviceExt, &allocationList);

		vkDestroyBuffer(deviceExt->device, tempBuffer, NULL);

		if(blockId != U32_MAX)
			GPUAllocator_freeAllocation(&device->allocator, blockId, blockOffset);
	}

	if(lock)
		Lock_unlock(&deviceExt->descriptorLock);

	return err;
}
