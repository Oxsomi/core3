/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

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

