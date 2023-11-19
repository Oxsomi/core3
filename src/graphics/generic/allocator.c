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
#include "platforms/ext/bufferx.h"
#include "types/buffer.h"
#include "types/error.h"

impl Bool DeviceMemoryAllocator_freeAllocationExt(GraphicsDevice *device, void *ext);

Bool DeviceMemoryAllocator_freeAllocation(DeviceMemoryAllocator *allocator, U32 blockId, U64 blockOffset) {

	if(!allocator || blockId >= allocator->blocks.length)
		return false;

	ELockAcquire acq = Lock_lock(&allocator->lock, U64_MAX);

	if (acq < ELockAcquire_Success)
		return false;		//Can't free.

	DeviceMemoryBlock *block = (DeviceMemoryBlock*) List_ptr(allocator->blocks, blockId);

	Bool success = AllocationBuffer_freeBlock(&block->allocations, (const U8*) blockOffset);

	if (!block->allocations.allocations.length) {
		AllocationBuffer_freex(&block->allocations);
		DeviceMemoryAllocator_freeAllocationExt(allocator->device, block->ext);
	}

	if(blockId + 1 == allocator->blocks.length)
		while(allocator->blocks.length && !((DeviceMemoryBlock*) List_last(allocator->blocks))->ext)
			success &= !List_popBack(&allocator->blocks, Buffer_createNull()).genericError;

	if(acq == ELockAcquire_Acquired)	//Only release if it wasn't already active
		Lock_unlock(&allocator->lock);

	return success;
}