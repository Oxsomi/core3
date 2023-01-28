#pragma once
#include "types/list.h"

//An allocation buffer functions mostly like a ring buffer, but it falls back to a 
//normal block buffer if it can't allocate in O(1) (e.g. space at back or front is unavailable).
//This means it can be used for both purposes.

typedef struct AllocationBuffer {

	Buffer buffer;			//Our data buffer
	List allocations;		//List<[U64 start, U64 end]>. Top bit of start indicates the block is free

} AllocationBuffer;

Error AllocationBuffer_create(U64 size, Allocator alloc, AllocationBuffer *allocationBuffer);

Error AllocationBuffer_createRefFromRegion(
	Buffer origin,
	U64 offset, U64 size,
	Allocator alloc,
	AllocationBuffer *allocationBuffer
);

Bool AllocationBuffer_free(AllocationBuffer *allocationBuffer, Allocator alloc);

U8 *AllocationBuffer_allocateBlock(AllocationBuffer *allocationBuffer, U64 size, U64 alignment, Allocator alloc);
U8 *AllocationBuffer_allocateAndFillBlock(AllocationBuffer *allocationBuffer, Buffer data, U64 alignment, Allocator alloc);

Bool AllocationBuffer_freeBlock(AllocationBuffer *allocationBuffer, U8 *ptr);

