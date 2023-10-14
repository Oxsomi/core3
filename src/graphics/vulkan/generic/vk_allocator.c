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

#include "graphics/generic/allocator.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/generic/device.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/listx.h"
#include "types/error.h"
#include "types/buffer.h"

Error GPUAllocator_allocate(GPUAllocator *allocator, void *requirementsExt, U32 *blockId, U64 *blockOffset) {
	
	U64 blockSize = 64 * MIBI;

	if(!allocator || !requirementsExt || !blockId || !blockOffset)
		return Error_nullPointer(!allocator ? 0 : (!requirementsExt ? 1 : (!blockId ? 2 : 3)));

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(allocator->device, Vk);

	VkMemoryRequirements2 req = *(VkMemoryRequirements2*) requirementsExt;
	VkMemoryRequirements memReq = req.memoryRequirements;
	VkMemoryDedicatedRequirements dedicated = *(VkMemoryDedicatedRequirements*) req.pNext;

	//When block count hits 2047 that means there were at least 2048 memory objects (+1 UBO)
	//After that, the allocator should be more conservative for dedicating separate memory blocks.
	//Most devices only support up to 4096 memory objects.

	Bool isDedicated = dedicated.requiresDedicatedAllocation;
	isDedicated |= dedicated.prefersDedicatedAllocation && allocator->blocks.length < 2048;

	//Find an existing allocation

	if (!isDedicated) {

		for(U64 i = 0; i < allocator->blocks.length; ++i) {

			GPUBlock *block = (GPUBlock*)List_ptr(allocator->blocks, i);

			if(
				!block->ext ||
				block->isDedicated || 
				(block->typeExt & memReq.memoryTypeBits) != memReq.memoryTypeBits
			)
				continue;

			const U8 *alloc = NULL; 
			Error err = AllocationBuffer_allocateBlockx(&block->allocations, memReq.size, memReq.alignment, &alloc);

			if(err.genericError)
				continue;

			*blockId = (U32) i;
			*blockOffset = (U64) alloc;
			return Error_none();
		}
	}

	//Find suitable memory type
	
	VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	VkMemoryPropertyFlags coherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	VkMemoryPropertyFlags local = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	VkMemoryPropertyFlags properties[3] = {
		local | host | coherent,
		local | host,
		local
	};

	U32 memoryId = U32_MAX;
	U32 propertyId = 3;

	for (U32 i = 0; i < deviceExt->memoryProperties.memoryTypeCount; ++i) {

		if(!(memReq.memoryTypeBits & (1 << i)))
			continue;

		VkMemoryPropertyFlags propFlag = deviceExt->memoryProperties.memoryTypes[i].propertyFlags;

		for(U32 j = 0; j < propertyId; ++j) {

			if ((propFlag & properties[j]) == properties[j]) {
				propertyId = j;
				memoryId = i;
				break;
			}
		}

		if(!propertyId)
			break;
	}

	if (memoryId == U32_MAX)
		return Error_invalidState(0);

	//Allocate memory
		
	U64 realBlockSize = (U64_max(blockSize, memReq.size * 2) + blockSize - 1) / blockSize * blockSize;

	VkMemoryAllocateInfo alloc = (VkMemoryAllocateInfo) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = isDedicated ? memReq.size : realBlockSize,
		.memoryTypeIndex = memoryId
	};

	VkDeviceMemory mem = NULL;
	GPUBlock block = (GPUBlock) { 0 };
	Error err = vkCheck(vkAllocateMemory(deviceExt->device, &alloc, NULL, &mem));

	if(err.genericError)
		return err;

	void *mappedMem = NULL;

	if(properties[propertyId] & host)
		_gotoIfError(clean, vkCheck(vkMapMemory(deviceExt->device, mem, 0, alloc.allocationSize, 0, &mappedMem)));

	//Initialize block

	block = (GPUBlock) {
		.typeExt = memReq.memoryTypeBits,
		.isDedicated = isDedicated,
		.mappedMemory = mappedMem,
		.ext = mem
	};

	_gotoIfError(clean, AllocationBuffer_createx(alloc.allocationSize, true, &block.allocations));

	//Find a spot in the blocks list

	U64 i = 0;

	for(; i < allocator->blocks.length; ++i)
		if (!((GPUBlock*)List_ptr(allocator->blocks, i))->ext)
			break;

	const U8 *allocLoc = NULL;
	_gotoIfError(clean, AllocationBuffer_allocateBlockx(&block.allocations, memReq.size, memReq.alignment, &allocLoc));

	if(i == allocator->blocks.length) {
		
		if(i == U32_MAX)
			_gotoIfError(clean, Error_outOfBounds(0, i, U32_MAX));

		_gotoIfError(clean, List_pushBackx(&allocator->blocks, Buffer_createConstRef(&block, sizeof(block))))
	}

	else *((GPUBlock*)List_ptr(allocator->blocks, i)) = block;

	*blockId = (U32) i;
	*blockOffset = (U64) allocLoc;

clean:

	if(err.genericError) {
		AllocationBuffer_freex(&block.allocations);
		vkFreeMemory(deviceExt->device, mem, NULL);
	}

	return err;
}

Bool GPUAllocator_freeAllocationExt(GraphicsDevice *device, void *ext) {
	
	if(!device || !ext)
		return false;
		
	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	vkFreeMemory(deviceExt->device, (VkDeviceMemory) ext, NULL);
	return true;
}
