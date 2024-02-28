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
#include "graphics/generic/allocator.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/string.h"

static const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
static const VkMemoryPropertyFlags coherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
static const VkMemoryPropertyFlags local = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

Error VkDeviceMemoryAllocator_findMemory(
	VkGraphicsDevice *deviceExt,
	Bool cpuSided,
	U32 memoryBits,
	U32 *outMemoryId,
	VkMemoryPropertyFlags *outPropertyFlags,
	U64 *size
) {

	//Find suitable memory type

	VkMemoryPropertyFlags all = local | host | coherent;

	VkMemoryPropertyFlags properties[3] = {			//Contains local if force cpu sided is turned off
		host | coherent,
		host,
		0
	};

	U32 memoryId = U32_MAX;
	U32 propertyId = 2;

	U64 maxHeapSizes[2] = { 0 };
	U32 heapIds[2] = { 0 };

	for (U32 i = 0; i < deviceExt->memoryProperties.memoryHeapCount; ++i) {

		VkMemoryHeap heap = deviceExt->memoryProperties.memoryHeaps[i];
		heap.flags &= 1;														//OOB

		if (heap.size > maxHeapSizes[heap.flags] && heap.size > 256 * MIBI) {	//Ignore 256MB to allow AMD APU to work.
			maxHeapSizes[heap.flags] = heap.size;
			heapIds[heap.flags] = i;
		}
	}

	Bool distinct = true;
	Bool hasLocal = true;

 	if (!maxHeapSizes[0]) {						//If there's only local heaps then we know we're on mobile. Use local heap.
		maxHeapSizes[0] = maxHeapSizes[1];
		heapIds[0] = heapIds[1];
		distinct = false;
	}

	else if (!maxHeapSizes[1]) {				//If there's only host heaps then we know we're on AMD APU. Use host heap.
		maxHeapSizes[1] = maxHeapSizes[0];
		heapIds[1] = heapIds[0];
		distinct = false;
		hasLocal = false;
	}

	if (!cpuSided) {

		if(hasLocal)
			for (U32 i = 0; i < 3; ++i)
				properties[i] |= local;

		++propertyId;
	}

	if (!maxHeapSizes[0] || !maxHeapSizes[1])
		return Error_notFound(0, 0, "VkDeviceMemoryAllocator_findMemory() failed, no heaps found");

	if (size) {
		*size = (cpuSided ? maxHeapSizes[0] : maxHeapSizes[1]) | ((U64)distinct << 63);
		return Error_none();
	}

	//Allocate from the heaps we selected

	for (U32 i = 0; i < deviceExt->memoryProperties.memoryTypeCount; ++i) {

		if(!((memoryBits >> i) & 1))
			continue;

		VkMemoryType type = deviceExt->memoryProperties.memoryTypes[i];

		if(type.heapIndex != heapIds[0] && type.heapIndex != heapIds[1])
			continue;

		VkMemoryPropertyFlags propFlag = type.propertyFlags;

		for(U32 j = 0; j < propertyId; ++j) {

			if ((propFlag & all) == properties[j]) {
				propertyId = j;
				memoryId = i;
				break;
			}
		}

		if(!propertyId)
			break;
	}

	if (memoryId == U32_MAX)
		return Error_notFound(1, 0, "VkDeviceMemoryAllocator_findMemory() found no suitable memoryId");

	*outMemoryId = memoryId;
	*outPropertyFlags = properties[propertyId];

	return Error_none();
}

Error DeviceMemoryAllocator_allocate(
	DeviceMemoryAllocator *allocator,
	void *requirementsExt,
	Bool cpuSided,
	U32 *blockId,
	U64 *blockOffset,
	EResourceType resourceType,
	CharString objectName
) {

	U64 blockSize = DeviceMemoryBlock_defaultSize;

	if(!allocator || !requirementsExt || !blockId || !blockOffset)
		return Error_nullPointer(
			!allocator ? 0 : (!requirementsExt ? 1 : (!blockId ? 2 : 3)),
			"DeviceMemoryAllocator_allocate()::allocator, requirementsExt, blockId and blockOffset are required"
		);

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(allocator->device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(allocator->device->instance), Vk);

	(void)instanceExt;

	VkMemoryRequirements2 req = *(VkMemoryRequirements2*) requirementsExt;
	VkMemoryRequirements memReq = req.memoryRequirements;
	VkMemoryDedicatedRequirements dedicated = *(VkMemoryDedicatedRequirements*) req.pNext;

	//When block count hits 1999 that means there were at least 2000 memory objects (+1 UBO)
	//After that, the allocator should be more conservative for dedicating separate memory blocks.
	//Most devices only support up to 4000 memory objects.

	Bool isDedicated = dedicated.requiresDedicatedAllocation;
	isDedicated |= dedicated.prefersDedicatedAllocation && allocator->blocks.length < 2000;

	//Find an existing allocation

	if (!isDedicated) {

		for(U64 i = 0; i < allocator->blocks.length; ++i) {

			DeviceMemoryBlock *block = &allocator->blocks.ptrNonConst[i];

			if(
				!block->ext ||
				block->isDedicated ||
				(block->typeExt & memReq.memoryTypeBits) != memReq.memoryTypeBits ||
				(Bool)(block->allocationTypeExt & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != !cpuSided ||
				block->resourceType != resourceType
			)
				continue;

			U64 tempAlignment = memReq.alignment;

			if(!(block->allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))		//Adhere to memory requirements
				tempAlignment = U64_max(256, tempAlignment);

			const U8 *alloc = NULL;
			Error err = AllocationBuffer_allocateBlockx(&block->allocations, memReq.size, tempAlignment, &alloc);

			if(err.genericError)
				continue;

			*blockId = (U32) i;
			*blockOffset = (U64) alloc;
			return Error_none();
		}
	}

	//Allocate memory

	U32 memoryId = 0;
	VkMemoryPropertyFlags prop = 0;
	Error err = VkDeviceMemoryAllocator_findMemory(deviceExt, cpuSided, memReq.memoryTypeBits, &memoryId, &prop, NULL);

	if(err.genericError)
		return err;

	U64 realBlockSize = (U64_max(blockSize, memReq.size * 2) + blockSize - 1) / blockSize * blockSize;

	VkMemoryAllocateFlagsInfo allocNext = (VkMemoryAllocateFlagsInfo) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
		.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
	};

	VkMemoryAllocateInfo alloc = (VkMemoryAllocateInfo) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = resourceType == EResourceType_DeviceBuffer ? &allocNext : NULL,
		.allocationSize = isDedicated ? memReq.size : realBlockSize,
		.memoryTypeIndex = memoryId
	};

	VkDeviceMemory mem = NULL;
	DeviceMemoryBlock block = (DeviceMemoryBlock) { 0 };
	CharString temp = CharString_createNull();

	if((err = vkCheck(vkAllocateMemory(deviceExt->device, &alloc, NULL, &mem))).genericError)
		return err;

	void *mappedMem = NULL;

	if(prop & host)
		_gotoIfError(clean, vkCheck(vkMapMemory(deviceExt->device, mem, 0, alloc.allocationSize, 0, &mappedMem)));

	//Initialize block

	block = (DeviceMemoryBlock) {
		.typeExt = memReq.memoryTypeBits,
		.allocationTypeExt = (U16) prop,
		.isDedicated = isDedicated,
		.mappedMemory = mappedMem,
		.ext = mem,
		.resourceType = resourceType
	};

	_gotoIfError(clean, AllocationBuffer_createx(alloc.allocationSize, true, &block.allocations));

	#ifndef NDEBUG
		Log_captureStackTracex(block.stackTrace, sizeof(block.stackTrace) / sizeof(void*), 1);
	#endif

	//Find a spot in the blocks list

	U64 i = 0;

	for(; i < allocator->blocks.length; ++i)
		if (!allocator->blocks.ptr[i].ext)
			break;

	const U8 *allocLoc = NULL;
	_gotoIfError(clean, AllocationBuffer_allocateBlockx(&block.allocations, memReq.size, memReq.alignment, &allocLoc));

	if(i == allocator->blocks.length) {

		if(i == U32_MAX)
			_gotoIfError(clean, Error_outOfBounds(0, i, U32_MAX, "DeviceMemoryAllocator_allocate() block out of bounds"));

		_gotoIfError(clean, ListDeviceMemoryBlock_pushBackx(&allocator->blocks, block))
	}

	else allocator->blocks.ptrNonConst[i] = block;

	*blockId = (U32) i;
	*blockOffset = (U64) allocLoc;

	if(CharString_length(objectName)) {

		#ifndef NDEBUG

			if(instanceExt->debugSetName) {

				_gotoIfError(clean, CharString_formatx(
					&temp,
					isDedicated ? "Memory block %"PRIu32" (host: %s, coherent: %s, device: %s): %s" :
					"Memory block %"PRIu32" (host: %s, coherent: %s, device: %s)",
					(U32) i,
					prop & host ? "true" : "false",
					prop & coherent ? "true" : "false",
					prop & local ? "true" : "false",
					objectName.ptr
				));

				VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
					.objectType = VK_OBJECT_TYPE_DEVICE_MEMORY,
					.pObjectName = temp.ptr,
					.objectHandle = (U64) mem
				};

				_gotoIfError(clean, vkCheck(instanceExt->debugSetName(deviceExt->device, &debugName)));
				CharString_freex(&temp);
			}

		#endif
	}

clean:

	CharString_freex(&temp);

	if(err.genericError) {
		AllocationBuffer_freex(&block.allocations);
		vkFreeMemory(deviceExt->device, mem, NULL);
	}

	return err;
}

Bool DeviceMemoryAllocator_freeAllocationExt(GraphicsDevice *device, void *ext) {

	if(!device || !ext)
		return false;

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	vkFreeMemory(deviceExt->device, (VkDeviceMemory) ext, NULL);
	return true;
}
