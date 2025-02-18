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
#include "graphics/generic/allocator.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/generic/interface.h"
#include "graphics/vulkan/vk_interface.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "types/base/error.h"
#include "types/math/math.h"
#include "types/container/string.h"

static const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
static const VkMemoryPropertyFlags coherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
static const VkMemoryPropertyFlags local = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

Error VkDeviceMemoryAllocator_findMemory(
	VkGraphicsDevice *deviceExt,
	Bool cpuSided,
	U32 memoryBits,
	U32 *outMemoryId,
	VkMemoryPropertyFlags *outPropertyFlags
) {

	VkMemoryPropertyFlags all = local | host | coherent;
	U32 propertyId = 2;

	U32 memoryId = U32_MAX;

	if(!deviceExt->hasDistinctMemory)
		all &=~ local;

	VkMemoryPropertyFlags properties[3] = {			//Contains local if force cpu sided is turned off
		host | coherent,
		host,
		0
	};

	if (!cpuSided && deviceExt->hasLocalMemory) {

		for (U32 i = 0; i < 3; ++i)
			properties[i] |= local;

		++propertyId;
	}

	//Allocate from the heaps we selected

	for (U32 i = 0; i < deviceExt->memoryProperties.memoryTypeCount; ++i) {

		const VkMemoryType type = deviceExt->memoryProperties.memoryTypes[i];

		if(!((memoryBits >> i) & 1))
			continue;

		if(type.heapIndex != deviceExt->heapIds[0] && type.heapIndex != deviceExt->heapIds[1])
			continue;

		const VkMemoryPropertyFlags propFlag = type.propertyFlags;

		for(U32 j = 0; j < propertyId; ++j)
			if ((propFlag & all) == properties[j]) {
				propertyId = j;
				memoryId = i;
				break;
			}

		if(!propertyId)		//Stop if the most ideal property is found
			break;
	}
	if (memoryId == U32_MAX)
		return Error_notFound(1, 0, "VkDeviceMemoryAllocator_findMemory() found no suitable memoryId");

	*outMemoryId = memoryId;
	*outPropertyFlags = properties[propertyId];

	return Error_none();
}

Error VK_WRAP_FUNC(DeviceMemoryAllocator_allocate)(
	DeviceMemoryAllocator *allocator,
	void *requirementsExt,
	Bool cpuSided,
	U32 *blockId,
	U64 *blockOffset,
	EResourceType resourceType,
	CharString objectName,
	DeviceMemoryBlock *resultBlock
) {

	if(!allocator || !requirementsExt || !blockId || !blockOffset)
		return Error_nullPointer(
			!allocator ? 0 : (!requirementsExt ? 1 : (!blockId ? 2 : 3)),
			"VkDeviceMemoryAllocator_allocate()::allocator, requirementsExt, blockId and blockOffset are required"
		);

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(allocator->device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(allocator->device->instance), Vk);

	(void)instanceExt;

	VkMemoryRequirements2 req = *(VkMemoryRequirements2*) requirementsExt;
	VkMemoryRequirements memReq = req.memoryRequirements;
	VkMemoryDedicatedRequirements dedicated = *(VkMemoryDedicatedRequirements*) req.pNext;
	U64 maxAllocationSize = allocator->device->info.capabilities.maxAllocationSize;

	if(memReq.size > maxAllocationSize)
		return Error_outOfBounds(
			2, memReq.size, maxAllocationSize,
			"VkDeviceMemoryAllocator_allocate() allocation length exceeds max allocation size"
		);

	VkDeviceMemory mem = NULL;
	DeviceMemoryBlock block = (DeviceMemoryBlock) { 0 };
	CharString temp = CharString_createNull();

	//We lock this early to avoid other mem alloc from allocating too many memory blocks at once.
	//Maybe what we end up allocating now can be used for the next.

	ELockAcquire acq = SpinLock_lock(&allocator->lock, U64_MAX);

	U32 memoryId = 0;
	VkMemoryPropertyFlags prop = 0;
	Error err = Error_none();

	//When block count hits 1999 that means there were at least 2000 memory objects (+1 UBO)
	//After that, the allocator should be more conservative for dedicating separate memory blocks.
	//Most devices only support up to 4000 memory objects.

	Bool isDedicated = dedicated.requiresDedicatedAllocation;
	isDedicated |= dedicated.prefersDedicatedAllocation && allocator->blocks.length < 2000;

	if(allocator->device->info.type != EGraphicsDeviceType_Dedicated)	//Ensure everything gets placed in cpu space
		cpuSided = true;

	//Log_debugLnx("Searching for %"PRIu64" bytes", memReq.size);

	//Find an existing allocation
	
	gotoIfError(clean, VkDeviceMemoryAllocator_findMemory(
		deviceExt, cpuSided, memReq.memoryTypeBits, &memoryId, &prop
	))

	if (!isDedicated) {

		for(U64 i = 0; i < allocator->blocks.length; ++i) {

			DeviceMemoryBlock *blocki = &allocator->blocks.ptrNonConst[i];

			if(
				!blocki->ext ||
				blocki->isDedicated ||
				blocki->typeExt != memoryId ||
				!!(blocki->allocationTypeExt & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != !cpuSided
			) {

				/*Log_debugLnx(
					"Skipping block %"PRIu64" because of: %s",
					i,
					!block->ext ? "ext" : (
						block->isDedicated ? "dedicated" : (
							((block->typeExt & memReq.memoryTypeBits) != memReq.memoryTypeBits) ? "memoryTypeBits" : (
								!!(block->allocationTypeExt & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != !cpuSided ? "cpu sided" :
								"resourceType"
							)
						)		
					)
				);*/

				continue;
			}

			U64 tempAlignment = memReq.alignment;

			if(!(blocki->allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))		//Adhere to memory requirements
				tempAlignment = U64_max(deviceExt->atomSize, tempAlignment);

			const U8 *alloc = NULL;
			Error err1 = AllocationBuffer_allocateBlockx(
				&blocki->allocations,
				memReq.size,
				tempAlignment,
				resourceType != EResourceType_DeviceBuffer,
				&alloc
			);

			if(err1.genericError) {
				//Log_debugLnx("Skipping block %"PRIu64" because of: no memory", i);
				continue;
			}

			//Log_debugLnx("Found block %"PRIu64, i);

			if(allocator->device->flags & EGraphicsDeviceFlags_IsDebug)
				Log_debugLnx(
					"-- Graphics: Allocating into existing memory block "
					"(%"PRIu64" from allocation of size %"PRIu64" at offset %"PRIx64" and alignment %"PRIu64")",
					i,
					memReq.size,
					(U64) alloc,
					tempAlignment
				);

			*blockId = (U32) i;
			*blockOffset = (U64) alloc;
			*resultBlock = *blocki;

			goto clean;
		}
	}

	//Allocate memory

	U64 blockSize = cpuSided ? allocator->device->blockSizeCpu : allocator->device->blockSizeGpu;
	U64 realBlockSize = U64_min(
		(U64_max(blockSize, memReq.size * 2) + blockSize - 1) / blockSize * blockSize,
		maxAllocationSize
	);

	VkMemoryAllocateFlagsInfo next = (VkMemoryAllocateFlagsInfo) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
		.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
	};

	VkMemoryAllocateInfo alloc = (VkMemoryAllocateInfo) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = allocator->device->info.capabilities.featuresExt & EVkGraphicsFeatures_BufferDeviceAddress ? &next : NULL,
		.allocationSize = isDedicated ? memReq.size : realBlockSize,
		.memoryTypeIndex = memoryId
	};

	U64 usedMem = VK_WRAP_FUNC(GraphicsDevice_getMemoryBudget)(allocator->device, !cpuSided);
	U64 maxAlloc = 
		cpuSided ? allocator->device->info.capabilities.sharedMemory :
		allocator->device->info.capabilities.dedicatedMemory;

	if(usedMem != U64_MAX && usedMem + alloc.allocationSize > maxAlloc)
		gotoIfError(clean, Error_outOfMemory(0, "Memory block allocation would exceed available memory"))

	if(allocator->device->flags & EGraphicsDeviceFlags_IsDebug)
		Log_debugLnx(
			"-- Graphics: Allocating new memory block (%"PRIu64" with size %"PRIu64" from allocation with size %"PRIu64")\n"
			"\t%s (memory id: %"PRIu32", available memory: %"PRIu64")",
			allocator->blocks.length,
			alloc.allocationSize,
			memReq.size,
			cpuSided ? "Cpu sided allocation" : "Gpu sided allocation",
			memoryId,
			usedMem == U64_MAX ? U64_MAX : maxAlloc - usedMem
		);

	gotoIfError(clean, checkVkError(deviceExt->allocateMemory(deviceExt->device, &alloc, NULL, &mem)))
	
	void *mappedMem = NULL;

	if(prop & host)
		gotoIfError(clean, checkVkError(deviceExt->mapMemory(deviceExt->device, mem, 0, alloc.allocationSize, 0, &mappedMem)))
		
	//Initialize block

	if(allocator->device->info.type != EGraphicsDeviceType_Dedicated)
		prop &=~ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	block = (DeviceMemoryBlock) {
		.isActive = true,
		.typeExt = memoryId,
		.allocationTypeExt = (U16) prop,
		.isDedicated = isDedicated,
		.mappedMemoryExt = mappedMem,
		.ext = mem
	};

	gotoIfError(clean, AllocationBuffer_createx(alloc.allocationSize, true, deviceExt->nonLinearAlignment, &block.allocations))

	if(allocator->device->flags & EGraphicsDeviceFlags_IsDebug)
		Log_captureStackTracex(block.stackTrace, sizeof(block.stackTrace) / sizeof(void*), 1);

	//Find a spot in the blocks list

	U64 i = 0;

	for(; i < allocator->blocks.length; ++i)
		if (!allocator->blocks.ptr[i].isActive)
			break;

	const U8 *allocLoc = NULL;
	gotoIfError(clean, AllocationBuffer_allocateBlockx(
		&block.allocations,
		memReq.size,
		memReq.alignment,
		resourceType != EResourceType_DeviceBuffer,
		&allocLoc
	))

	if(i == allocator->blocks.length) {

		if(i == U32_MAX)
			gotoIfError(clean, Error_outOfBounds(0, i, U32_MAX, "VkDeviceMemoryAllocator_allocate() block out of bounds"))

		gotoIfError(clean, ListDeviceMemoryBlock_pushBackx(&allocator->blocks, block))
	}

	else allocator->blocks.ptrNonConst[i] = block;

	*blockId = (U32) i;
	*blockOffset = (U64) allocLoc;
	*resultBlock = block;

	if(
		(allocator->device->flags & EGraphicsDeviceFlags_IsDebug) &&
		CharString_length(objectName) && instanceExt->debugSetName
	) {

		gotoIfError(clean, CharString_formatx(
			&temp,
			isDedicated ? "Memory block %"PRIu32" (host: %s, coherent: %s, device: %s): %s" :
			"Memory block %"PRIu32" (host: %s, coherent: %s, device: %s)",
			(U32) i,
			prop & host ? "true" : "false",
			prop & coherent ? "true" : "false",
			prop & local ? "true" : "false",
			objectName.ptr
		))

		VkDebugUtilsObjectNameInfoEXT debugName = (VkDebugUtilsObjectNameInfoEXT) {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = VK_OBJECT_TYPE_DEVICE_MEMORY,
			.pObjectName = temp.ptr,
			.objectHandle = (U64) mem
		};

		gotoIfError(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName)))
		CharString_freex(&temp);
	}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&allocator->lock);

	CharString_freex(&temp);

	if(err.genericError) {

		AllocationBuffer_freex(&block.allocations);

		if(mem)
			deviceExt->freeMemory(deviceExt->device, mem, NULL);
	}

	return err;
}

Bool VK_WRAP_FUNC(DeviceMemoryAllocator_freeAllocation)(GraphicsDevice *device, void *ext) {

	if(!device || !ext)
		return false;

	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	deviceExt->freeMemory(deviceExt->device, (VkDeviceMemory) ext, NULL);
	return true;
}
