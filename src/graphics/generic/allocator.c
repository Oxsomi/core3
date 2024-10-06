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
#include "graphics/generic/interface.h"
#include "graphics/generic/allocator.h"
#include "platforms/ext/bufferx.h"
#include "types/buffer.h"
#include "types/error.h"

TListImpl(DeviceMemoryBlock);

Bool DeviceMemoryAllocator_freeAllocation(DeviceMemoryAllocator *allocator, U32 blockId, U64 blockOffset) {

	if(!allocator || blockId >= allocator->blocks.length)
		return false;

	ELockAcquire acq = SpinLock_lock(&allocator->lock, U64_MAX);

	if (acq < ELockAcquire_Success)
		return false;		//Can't free.

	DeviceMemoryBlock *block = &allocator->blocks.ptrNonConst[blockId];
	Bool success = AllocationBuffer_freeBlock(&block->allocations, (const U8*) blockOffset);

	if (!block->allocations.allocations.length) {
		AllocationBuffer_freex(&block->allocations);
		DeviceMemoryAllocator_freeAllocationExt(allocator->device, block->ext);
		block->ext = NULL;
		block->mappedMemoryExt = NULL;
	}

	if(blockId + 1 == allocator->blocks.length)
		while(allocator->blocks.length && !ListDeviceMemoryBlock_last(allocator->blocks)->ext)
			success &= !ListDeviceMemoryBlock_popBack(&allocator->blocks, NULL).genericError;

	if(acq == ELockAcquire_Acquired)	//Only release if it wasn't already active
		SpinLock_unlock(&allocator->lock);

	return success;
}