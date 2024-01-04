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

#pragma once
#include "types/list.h"

//An allocation buffer functions mostly like a ring buffer, but it falls back to a 
//normal block buffer if it can't allocate in O(1) (e.g. space at back or front is unavailable).
//This means it can be used for both purposes.

typedef struct AllocationBufferBlock {
	U64 start, end, alignment;
} AllocationBufferBlock;

TList(AllocationBufferBlock);

typedef struct AllocationBuffer {

	Buffer buffer;							//Our data buffer
	ListAllocationBufferBlock allocations;

} AllocationBuffer;

Error AllocationBuffer_create(U64 size, Bool isVirtual, Allocator alloc, AllocationBuffer *allocationBuffer);

Error AllocationBuffer_createRefFromRegion(
	Buffer origin,
	U64 offset,
	U64 size,
	Allocator alloc,
	AllocationBuffer *allocationBuffer
);

Bool AllocationBuffer_free(AllocationBuffer *allocationBuffer, Allocator alloc);

Bool AllocationBuffer_freeBlock(AllocationBuffer *allocationBuffer, const U8 *ptr);

Bool AllocationBuffer_freeAll(AllocationBuffer *allocationBuffer);						//Frees all blocks

//If !allocationBuffer->buffer.ptr the pointer shouldn't be dereferenced, it's just for offset tracking.
//Result doesn't get touched if validation of arguments failed or if out of memory is triggered by the list.
//If there's no available blocks then it will set *result to NULL and return out of memory.

Error AllocationBuffer_allocateBlock(
	AllocationBuffer *allocationBuffer,
	U64 size,
	U64 alignment,
	Allocator alloc,
	const U8 **result
);

Error AllocationBuffer_allocateAndFillBlock(
	AllocationBuffer *allocationBuffer,
	Buffer data,
	U64 alignment,
	Allocator alloc,
	U8 **result
);
