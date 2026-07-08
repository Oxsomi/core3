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

//graphics/vulkan/generic/vk_allocator.c

#include "types/container/list_impl.h"
#include "graphics/generic/device_allocator.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/generic/interface.h"
#include "graphics/vulkan/vk_interface.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "platforms/logx.h"
#include "types/base/error.h"
#include "types/base/mathi.h"
#include "types/base/mathf.h"
#include "types/container/string.h"
#include "types/base/constants.h"

static const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
static const VkMemoryPropertyFlags coherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
static const VkMemoryPropertyFlags local = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

Bool VkDeviceMemoryAllocator_findMemory(
	VkGraphicsDevice *deviceExt,
	Bool cpuSided,
	U32 memoryBits,
	U32 *outMemoryId,
	VkMemoryPropertyFlags *outPropertyFlags,
	Error *e_rr
) {

	Bool s_uccess = true;

	VkMemoryPropertyFlags all = local | host | coherent;
	U32 propertyId = 2;

	U32 memoryId = U32_MAX;

	if(!deviceExt->hasDistinctMemory && !deviceExt->hasOnlyLocalMemory)
		all &=~ local;

	VkMemoryPropertyFlags properties[3] = {            //Contains local if force cpu sided is turned off
		host | coherent,
		host,
		0
	};

	if (
		(!cpuSided && deviceExt->hasLocalMemory) ||
		(cpuSided && deviceExt->hasOnlyLocalMemory)
	) {

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

		if(!propertyId)        //Stop if the most ideal property is found
			break;
	}
	if (memoryId == U32_MAX)
		retError(clean, Error_notFound(1, 0, "VkDeviceMemoryAllocator_findMemory() found no suitable memoryId"));

	*outMemoryId = memoryId;
	*outPropertyFlags = properties[propertyId];

clean:
	return s_uccess;
}

Bool VK_WRAP_FUNC(DeviceMemoryAllocator_allocate)(
	DeviceMemoryAllocator *allocator,
	void *requirementsExt,
	Bool cpuSided,
	U32 *blockId,
	U64 *blockOffset,
	EResourceType resourceType,
	CharString objectName,
	DeviceMemoryBlock *resultBlock,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = allocator ? GraphicsDevice_getAlloc(allocator->device) : NULL;

	if(!allocator || !requirementsExt || !blockId || !blockOffset)
		retError(clean, Error_nullPointer(
			!allocator ? 0 : (!requirementsExt ? 1 : (!blockId ? 2 : 3)),
			"VkDeviceMemoryAllocator_allocate()::allocator, requirementsExt, blockId and blockOffset are required"
		));

	VkGraphicsDevice *deviceExt = GraphicsDevice_ext(allocator->device, Vk);
	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(allocator->device->instance), Vk);

	(void)instanceExt;

	VkMemoryRequirements2 req = *(VkMemoryRequirements2*) requirementsExt;
	VkMemoryRequirements memReq = req.memoryRequirements;
	VkMemoryDedicatedRequirements dedicated = *(VkMemoryDedicatedRequirements*) req.pNext;
	U64 maxAllocationSize = allocator->device->info.capabilities.maxAllocationSize;

	if(memReq.size > maxAllocationSize)
		retError(clean, Error_outOfBounds(
			2, memReq.size, maxAllocationSize,
			"VkDeviceMemoryAllocator_allocate() allocation length exceeds max allocation size"
		));

	VkDeviceMemory mem = NULL;
	DeviceMemoryBlock block = (DeviceMemoryBlock) { 0 };
	CharString temp = CharString_createNull();

	//We lock this early to avoid other mem alloc from allocating too many memory blocks at once.
	//Maybe what we end up allocating now can be used for the next.

	ELockAcquire acq = SpinLock_lock(&allocator->lock, U64_MAX);

	U32 memoryId = 0;
	VkMemoryPropertyFlags prop = 0;

	//When block count hits 1999 that means there were at least 2000 memory objects (+1 UBO)
	//After that, the allocator should be more conservative for dedicating separate memory blocks.
	//Most devices only support up to 4000 memory objects.

	Bool isDedicated = dedicated.requiresDedicatedAllocation;
	isDedicated |= dedicated.prefersDedicatedAllocation && allocator->blocks.length < 2000;

	if(allocator->device->info.type != EGraphicsDeviceType_Dedicated)    //Ensure everything gets placed in cpu space
		cpuSided = true;

	//Log_debugLnx("Searching for %"PRIu64" bytes", memReq.size);

	//Find an existing allocation

	gotoIfError3(clean, VkDeviceMemoryAllocator_findMemory(
		deviceExt, cpuSided, memReq.memoryTypeBits, &memoryId, &prop, e_rr));

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

			if(!(blocki->allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))        //Adhere to memory requirements
				tempAlignment = U64_max(deviceExt->atomSize, tempAlignment);

			const U8 *allocated = NULL;
			Error err1 = Error_none();

			Bool didAllocate = AllocationBuffer_allocateBlock(
				&(AllocationBufferAllocate) {
					.allocationBuffer = &blocki->allocations,
					.alignment = tempAlignment,
					.isNonLinearResource = resourceType != EResourceType_DeviceBuffer,
					.alloc = GraphicsDevice_getAlloc(allocator->device)
				},
				memReq.size,
				&allocated,
				&err1
			);

			if(!didAllocate) {
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
					(U64) allocated,
					tempAlignment
				);

			*blockId = (U32) i;
			*blockOffset = (U64) allocated;
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

	VkMemoryAllocateInfo memAlloc = (VkMemoryAllocateInfo) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = allocator->device->info.capabilities.featuresExt & EVkGraphicsFeatures_BufferDeviceAddress ? &next : NULL,
		.allocationSize = isDedicated ? memReq.size : realBlockSize,
		.memoryTypeIndex = memoryId
	};

	U64 usedMem = VK_WRAP_FUNC(GraphicsDevice_getMemoryBudget)(allocator->device, !cpuSided);
	U64 maxAlloc =
		cpuSided ? allocator->device->info.capabilities.sharedMemory :
		allocator->device->info.capabilities.dedicatedMemory;

	if(usedMem != U64_MAX && usedMem + memAlloc.allocationSize > maxAlloc)
		retError(clean, Error_outOfMemory(0, "Memory block allocation would exceed available memory"));

	if(allocator->device->flags & EGraphicsDeviceFlags_IsDebug)
		Log_debugLnx(
			"-- Graphics: Allocating new memory block (%"PRIu64" with size %"PRIu64" from allocation with size %"PRIu64")\n"
			"\t%s (memory id: %"PRIu32", available memory: %"PRIu64")",
			allocator->blocks.length,
			memAlloc.allocationSize,
			memReq.size,
			cpuSided ? "Cpu sided allocation" : "Gpu sided allocation",
			memoryId,
			usedMem == U64_MAX ? U64_MAX : maxAlloc - usedMem
		);

	gotoIfError3(clean, checkVkError(deviceExt->allocateMemory(deviceExt->device, &memAlloc, NULL, &mem), e_rr));

	void *mappedMem = NULL;

	if(prop & host)
		gotoIfError3(clean, checkVkError(
			deviceExt->mapMemory(deviceExt->device, mem, 0, memAlloc.allocationSize, 0, &mappedMem),
			e_rr
		));

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

	gotoIfError3(clean, AllocationBuffer_create(
		&(AllocationBufferCreate) {
			.size = memAlloc.allocationSize,
			.nonLinearAlignment = deviceExt->nonLinearAlignment,
			.alloc = GraphicsDevice_getAlloc(allocator->device),
			.allocationBuffer = &block.allocations
		},
		true, e_rr
	));

	if(allocator->device->flags & EGraphicsDeviceFlags_IsDebug)
		Error_captureStackTrace(block.stackTrace, (U8)(sizeof(block.stackTrace) / sizeof(void*)), 1);

	//Find a spot in the blocks list

	U64 i = 0;

	for(; i < allocator->blocks.length; ++i)
		if (!allocator->blocks.ptr[i].isActive)
			break;

	const U8 *allocLoc = NULL;
	gotoIfError3(clean, AllocationBuffer_allocateBlock(
		&(AllocationBufferAllocate) {
			.allocationBuffer = &block.allocations,
			.alignment = memReq.alignment,
			.isNonLinearResource = resourceType != EResourceType_DeviceBuffer,
			.alloc = GraphicsDevice_getAlloc(allocator->device)
		},
		memReq.size, &allocLoc, e_rr
	));

	if(i == allocator->blocks.length) {

		if(i == U32_MAX)
			retError(clean, Error_outOfBounds(0, i, U32_MAX, "VkDeviceMemoryAllocator_allocate() block out of bounds"));

		gotoIfError3(clean, ListDeviceMemoryBlock_pushBack(&allocator->blocks, block, alloc, e_rr));
	}

	else allocator->blocks.ptrNonConst[i] = block;

	*blockId = (U32) i;
	*blockOffset = (U64) allocLoc;
	*resultBlock = block;

	if(
		(allocator->device->flags & EGraphicsDeviceFlags_IsDebug) &&
		CharString_length(objectName) && instanceExt->debugSetName
	) {

		gotoIfError3(clean, CharString_format(
			alloc, &temp, e_rr,
			isDedicated ?
				"Memory block %"PRIu32" (host: %s, coherent: %s, device: %s): %s" :
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

		gotoIfError3(clean, checkVkError(instanceExt->debugSetName(deviceExt->device, &debugName), e_rr));
		CharString_free(&temp, alloc);
	}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&allocator->lock);

	CharString_free(&temp, alloc);

	if(!s_uccess) {

		AllocationBuffer_free(&block.allocations, alloc);

		if(mem)
			deviceExt->freeMemory(deviceExt->device, mem, NULL);
	}

	return s_uccess;
}

Bool VK_WRAP_FUNC(DeviceMemoryAllocator_freeAllocation)(GraphicsDevice *device, void *ext) {

	if(!device || !ext)
		return false;

	const VkGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Vk);
	deviceExt->freeMemory(deviceExt->device, (VkDeviceMemory) ext, NULL);
	return true;
}
