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

#include "graphics/generic/device_allocator.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_internal.h"
#include "graphics/generic/instance.h"
#include "graphics/vulkan/vk_device.h"
#include "graphics/vulkan/vk_instance.h"
#include "graphics/vulkan/vk_interface.h"
#include "platforms/logx.h"
#include "types/base/error.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"

static const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
static const VkMemoryPropertyFlags coherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
static const VkMemoryPropertyFlags local = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
static const VkMemoryPropertyFlags cached = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

static Bool VkDeviceMemoryAllocator_findMemory(
	VkGraphicsDevice *deviceExt,
	Bool cpuSided,
	Bool readback,
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

	//CPU reads from uncached (write combined) memory are many times slower, so readback looks for cached first.
	//Coherent only: readback never invalidates, so a non coherent cached type would read stale data.

	if (readback) {
		all |= cached;
		properties[0] = host | coherent | cached;
		properties[1] = host | coherent;
	}

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
	const CharString *objectName,
	DeviceMemoryBlock *resultBlock,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = allocator ? GraphicsDevice_getAlloc(allocator->device) : NULL;

	ELockAcquire acq = ELockAcquire_Invalid;

	VkDeviceMemory mem = NULL;
	DeviceMemoryBlock block = (DeviceMemoryBlock) { 0 };
	CharString temp = CharString_createNull();
	VkGraphicsDevice *deviceExt = allocator ? GraphicsDevice_ext(allocator->device, Vk) : NULL;

	if(!allocator || !requirementsExt || !blockId || !blockOffset)
		retError(clean, Error_nullPointer(
			!allocator ? 0 : (!requirementsExt ? 1 : (!blockId ? 2 : 3)),
			"VkDeviceMemoryAllocator_allocate()::allocator, requirementsExt, blockId and blockOffset are required"
		));

	VkGraphicsInstance *instanceExt = GraphicsInstance_ext(GraphicsInstanceRef_ptr(allocator->device->instance), Vk);

	const VkBlockRequirements req = *(const VkBlockRequirements*) requirementsExt;
	const VkMemoryRequirements memReq = req.memory;
	const Bool requiresDedicated = !!(req.flags & EVkBlockFlags_RequiresDedicated);
	const Bool prefersDedicated = !!(req.flags & EVkBlockFlags_PrefersDedicated);
	U64 maxAllocationSize = allocator->device->info.capabilities.maxAllocationSize;

	if(memReq.size > maxAllocationSize)
		retError(clean, Error_outOfBounds(
			2, memReq.size, maxAllocationSize,
			"VkDeviceMemoryAllocator_allocate() allocation length exceeds max allocation size"
		));

	//We lock this early to avoid other mem alloc from allocating too many memory blocks at once.
	//Maybe what we end up allocating now can be used for the next.

	acq = SpinLock_lock(&allocator->lock, U64_MAX);

	U32 memoryId = 0;
	VkMemoryPropertyFlags prop = 0;

	//When block count hits 1999 that means there were at least 2000 memory objects (+1 UBO)
	//After that, the allocator should be more conservative for dedicating separate memory blocks.
	//Most devices only support up to 4000 memory objects.

	Bool isDedicated = requiresDedicated;
	isDedicated |= prefersDedicated && allocator->blocks.length < 2000;

	const Bool requestedCpu = cpuSided;                                  //Before the device type can force it

	if(allocator->device->info.type != EGraphicsDeviceType_Dedicated)    //Ensure everything gets placed in cpu space
		cpuSided = true;

	//A resource over half a block gets a block of exactly its own size.
	//A shared block could hold at most one more like it, and sizing one to twice the request instead reserves
	// slack that a few large resources (acceleration structures, big meshes) turn into gigabytes of nothing.
	//Below that it shares a standard block, which keeps the number of memory objects down.

	const U64 blockSize = cpuSided ? allocator->device->blockSizeCpu : allocator->device->blockSizeGpu;
	isDedicated |= memReq.size > blockSize / 2;

	if(
		(allocator->device->flags & EGraphicsDeviceFlags_IsDebug) &&
		prefersDedicated &&
		!requiresDedicated &&
		allocator->blocks.length >= 2000 &&
		GraphicsDevice_logOnce(allocator->device, EGraphicsDeviceMessage_TooManyMemoryBlocks)
	)
		Log_performanceLnx(
			"VkDeviceMemoryAllocator_allocate() Memory allocation prefers dedicated allocation, "
			"but there are already >= 2000 memory blocks. Falling back to shared allocations to prevent "
			"reaching 4096 (only logged once)"
		);

	//Find an existing allocation, or create a block. A device local block past the OS budget is retried once in host
	// memory before going over it: the OS would page something out from under the device to make room.

	const Bool readback = !!(req.flags & EVkBlockFlags_Readback);
	const Bool canFallBack = !cpuSided && allocator->device->info.type == EGraphicsDeviceType_Dedicated;

	U64 usedMem = U64_MAX, budget = U64_MAX, maxAlloc = 0;

	VkMemoryAllocateFlagsInfo next = (VkMemoryAllocateFlagsInfo) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
		.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
	};

	VkMemoryDedicatedAllocateInfo dedicatedInfo = (VkMemoryDedicatedAllocateInfo) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
		.image = req.image,
		.buffer = req.buffer
	};

	VkMemoryAllocateInfo memAlloc = (VkMemoryAllocateInfo) { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };

	for(U8 attempt = 0; attempt < 2; ++attempt) {        //The device local pick, then at most one host memory retry

		gotoIfError3(clean, VkDeviceMemoryAllocator_findMemory(
			deviceExt, cpuSided, readback, memReq.memoryTypeBits, &memoryId, &prop, e_rr
		));

		U32 blocksOfKind = 0;

		for(U64 i = 0; i < allocator->blocks.length && !isDedicated; ++i) {

			DeviceMemoryBlock *blocki = &allocator->blocks.ptrNonConst[i];

			if(
				!blocki->ext ||
				blocki->isDedicated ||
				blocki->typeExt != memoryId ||
				!!(blocki->allocationTypeExt & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != !cpuSided
			)
				continue;

			++blocksOfKind;

			U64 tempAlignment = memReq.alignment;

			if(!(blocki->allocationTypeExt & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))        //Adhere to memory requirements
				tempAlignment = U64_max(deviceExt->atomSize, tempAlignment);

			const U8 *allocated = NULL;
			Error err1 = Error_none();

			AllocationBufferAllocate allocation = (AllocationBufferAllocate) {
				.allocationBuffer = &blocki->allocations,
				.alignment = tempAlignment,
				.isNonLinearResource = resourceType != EResourceType_DeviceBuffer,
				.alloc = GraphicsDevice_getAlloc(allocator->device)
			};

			Bool didAllocate = AllocationBuffer_allocateBlock(&allocation, memReq.size, &allocated, &err1);

			if(!didAllocate)
				continue;

			//Verbose rather than debug: one line per allocation buries everything else a run has to say, and a
			//validation message is exactly what gets lost. Debug keeps the rare new block line and the stack
			// traces; tracing every allocation is asked for separately.

			if(allocator->device->flags & EGraphicsDeviceFlags_IsVerbose)
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

		//A shared block is halved while it would go past the budget, down to what the request needs

		const U64 newBlockSize = cpuSided ? allocator->device->blockSizeCpu : allocator->device->blockSizeGpu;
		memAlloc.allocationSize = isDedicated ? memReq.size :
			DeviceMemoryAllocator_newBlockSize(U64_min(newBlockSize, maxAllocationSize), blocksOfKind, memReq.size);
		memAlloc.memoryTypeIndex = memoryId;

		usedMem = VkGraphicsDevice_getMemoryUsage(allocator->device, !cpuSided, &budget);
		maxAlloc = cpuSided ?
			allocator->device->info.capabilities.sharedMemory : allocator->device->info.capabilities.dedicatedMemory;

		const Bool known = usedMem != U64_MAX && budget != U64_MAX;

		while(
			known && !isDedicated &&
			usedMem + memAlloc.allocationSize > budget && (memAlloc.allocationSize >> 1) >= memReq.size
		)
			memAlloc.allocationSize >>= 1;

		const Bool overBudget = known && usedMem + memAlloc.allocationSize > budget;

		if(overBudget && canFallBack && !attempt) {

			U32 hostId = 0;
			VkMemoryPropertyFlags hostProp = 0;
			Error err1 = Error_none();

			if(VkDeviceMemoryAllocator_findMemory(
				deviceExt, true, readback, memReq.memoryTypeBits, &hostId, &hostProp, &err1
			)) {

				if(GraphicsDevice_logOnce(allocator->device, EGraphicsDeviceMessage_OverBudget))
					Log_performanceLnx(
						"VkDeviceMemoryAllocator_allocate() device local memory is over the OS budget, "
						"so new blocks are placed in host memory (only logged once)"
					);

				//Host blocks can be smaller than device local ones, so what was shared there may need its own here

				cpuSided = true;
				isDedicated |= memReq.size > allocator->device->blockSizeCpu / 2;
				continue;
			}
		}

		if(overBudget && GraphicsDevice_logOnce(allocator->device, EGraphicsDeviceMessage_OverBudget))
			Log_performanceLnx(
				"VkDeviceMemoryAllocator_allocate() a memory block goes past the OS budget for its heap, "
				"so the OS may page memory out (only logged once)"
			);

		break;
	}

	if(usedMem != U64_MAX && usedMem + memAlloc.allocationSize > maxAlloc)
		retError(clean, Error_outOfMemory(0, "Memory block allocation would exceed available memory"));

	//Tell the driver which resource a dedicated block is for; it asked for one so it can optimize for it

	if(isDedicated && (req.image || req.buffer))
		memAlloc.pNext = &dedicatedInfo;

	//CPU side memory (staging) is paged out first and dedicated high priority blocks last.
	//A shared block holds whatever lands in it, so it stays normal unless staging can't share it: on an integrated
	// GPU everything is CPU side, so staging and device data use the same memory type there.

	const Bool lowPriority =
		requestedCpu && (isDedicated || allocator->device->info.type == EGraphicsDeviceType_Dedicated);

	VkMemoryPriorityAllocateInfoEXT priority = (VkMemoryPriorityAllocateInfoEXT) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT,
		.pNext = memAlloc.pNext,
		.priority = lowPriority ? 0.25f : (isDedicated && (req.flags & EVkBlockFlags_HighPriority) ? 0.75f : 0.5f)
	};

	if(allocator->device->info.capabilities.featuresExt & EVkGraphicsFeatures_MemoryPriority)
		memAlloc.pNext = &priority;

	if(allocator->device->info.capabilities.featuresExt & EVkGraphicsFeatures_BufferDeviceAddress) {
		next.pNext = memAlloc.pNext;
		memAlloc.pNext = &next;
	}

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
			deviceExt->mapMemory(deviceExt->device, mem, 0, memAlloc.allocationSize, 0, &mappedMem), e_rr
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

	AllocationBufferCreate allocationCreate = (AllocationBufferCreate) {
		.size = memAlloc.allocationSize,
		.nonLinearAlignment = deviceExt->nonLinearAlignment,
		.alloc = GraphicsDevice_getAlloc(allocator->device),
		.allocationBuffer = &block.allocations
	};

	gotoIfError3(clean, AllocationBuffer_create(&allocationCreate, true, e_rr));

	if(allocator->device->flags & EGraphicsDeviceFlags_IsDebug)
		Error_captureStackTrace(block.stackTrace, (U8)(sizeof(block.stackTrace) / sizeof(void*)), 1);

	//Find a spot in the blocks list

	U64 i = 0;

	for(; i < allocator->blocks.length; ++i)
		if (!allocator->blocks.ptr[i].isActive)
			break;

	AllocationBufferAllocate allocation = (AllocationBufferAllocate) {
		.allocationBuffer = &block.allocations,
		.alignment = memReq.alignment,
		.isNonLinearResource = resourceType != EResourceType_DeviceBuffer,
		.alloc = GraphicsDevice_getAlloc(allocator->device)
	};

	const U8 *allocLoc = NULL;
	gotoIfError3(clean, AllocationBuffer_allocateBlock(&allocation, memReq.size, &allocLoc, e_rr));

	//Named before the block is listed, so a failure here leaves nothing pointing at the memory clean frees

	if(
		(allocator->device->flags & EGraphicsDeviceFlags_IsDebug) &&
		objectName && CharString_length(*objectName) && instanceExt->debugSetName
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
			objectName->ptr
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

	if(i == allocator->blocks.length) {

		if(i == U32_MAX)
			retError(clean, Error_outOfBounds(0, i, U32_MAX, "VkDeviceMemoryAllocator_allocate() block out of bounds"));

		gotoIfError3(clean, ListDeviceMemoryBlock_pushBack(&allocator->blocks, block, alloc, e_rr));
	}

	else allocator->blocks.ptrNonConst[i] = block;

	*blockId = (U32) i;
	*blockOffset = (U64) allocLoc;
	*resultBlock = block;

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&allocator->lock);

	CharString_free(&temp, alloc);

	if(!s_uccess && deviceExt) {

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
