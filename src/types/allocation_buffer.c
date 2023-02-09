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

#include "types/allocation_buffer.h"
#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"

Error AllocationBuffer_create(U64 size, Allocator alloc, AllocationBuffer *allocationBuffer) {

	if(!allocationBuffer || !size)
		return Error_nullPointer(!size ? 0 : 2, 0);

	if(allocationBuffer->buffer.ptr || allocationBuffer->allocations.ptr)
		return Error_invalidOperation(0);

	if(size >> 48)
		return Error_invalidParameter(0, 0, 0);

	Error err = Buffer_createEmptyBytes(size, alloc, &allocationBuffer->buffer);

	if(err.genericError)
		return err;
		
	allocationBuffer->allocations = List_createEmpty(sizeof(U64) * 2);
	err = List_reserve(&allocationBuffer->allocations, 16, alloc);

	if(err.genericError) {
		Buffer_free(&allocationBuffer->buffer, alloc);
		*allocationBuffer = (AllocationBuffer) { 0 };
		return err;
	}

	return Error_none();
}

Error AllocationBuffer_createRefFromRegion(
	Buffer origin,
	U64 offset,
	U64 size,
	Allocator alloc,
	AllocationBuffer *allocationBuffer
) {

	if(!allocationBuffer || !size)
		return Error_nullPointer(!size ? 2 : 3, 0);

	if(allocationBuffer->buffer.ptr || allocationBuffer->allocations.ptr)
		return Error_invalidOperation(0);

	Error err = Buffer_createSubset(origin, offset, size, false, &allocationBuffer->buffer);

	if(err.genericError)
		return err;

	allocationBuffer->allocations = List_createEmpty(sizeof(U64) * 2);
	err = List_reserve(&allocationBuffer->allocations, 16, alloc);

	if(err.genericError) {
		*allocationBuffer = (AllocationBuffer) { 0 };
		return err;
	}

	return Error_none();
}

Bool AllocationBuffer_free(AllocationBuffer *allocationBuffer, Allocator alloc) {

	if (!allocationBuffer)
		return true;

	Bool success = Buffer_free(&allocationBuffer->buffer, alloc);
	success &= List_free(&allocationBuffer->allocations, alloc);
	*allocationBuffer = (AllocationBuffer) { 0 };
	return success;
}

typedef struct AllocationBufferBlock {
	U64 start, end, alignment;
} AllocationBufferBlock;

inline U64 AllocationBufferBlock_getStart(AllocationBufferBlock block) {
	return block.start << 1 >> 1;
}

inline U64 AllocationBufferBlock_size(AllocationBufferBlock block) {
	return block.end - AllocationBufferBlock_getStart(block);
}

inline U64 AllocationBufferBlock_isFree(AllocationBufferBlock block) {
	return block.start >> 63;
}

inline U64 AllocationBufferBlock_getCenter(AllocationBufferBlock block) {
	return (AllocationBufferBlock_getStart(block) + block.end) >> 1;
}

inline U64 AllocationBufferBlock_alignTo(U64 a, U64 alignment) {
	return alignment ? (a + alignment - 1) / alignment * alignment : a;
}

inline U64 AllocationBufferBlock_alignToBackwards(U64 a, U64 alignment) {
	return alignment ? a / alignment * alignment : a;
}

inline U64 AllocationBufferBlock_getAligned(AllocationBufferBlock block) {
	return AllocationBufferBlock_alignTo(AllocationBufferBlock_getStart(block), block.alignment);
}

inline Bool AllocationBufferBlock_isSame(AllocationBufferBlock block, U8 *start, U8 *ptr) {
	U64 blockStart = AllocationBufferBlock_getStart(block);
	U64 aligned = AllocationBufferBlock_getAligned(block);
	return ptr == start + blockStart || ptr == start + aligned;
}

U8 *AllocationBuffer_allocateAndFillBlock(AllocationBuffer *allocationBuffer, Buffer data, U64 alignment, Allocator alloc) {

	U8 *ptr = AllocationBuffer_allocateBlock(allocationBuffer, Buffer_length(data), alignment, alloc);

	if(!ptr)
		return NULL;

	Buffer_copy(Buffer_createRef(ptr, Buffer_length(data)), data);
	return ptr;
}

U8 *AllocationBuffer_allocateBlock(AllocationBuffer *allocationBuffer, U64 size, U64 alignment, Allocator alloc) {

	if (!allocationBuffer || !allocationBuffer->allocations.ptr || !size || (size >> 48) || (alignment >> 48))
		return NULL;

	U64 len = Buffer_length(allocationBuffer->buffer);

	if(size > len || alignment > len)
		return NULL;

	//No allocations? We start at the front, it's always aligned

	AllocationBufferBlock v;
	Buffer vb = Buffer_createRef(&v, sizeof(v));

	if (List_empty(allocationBuffer->allocations)) {

		v = (AllocationBufferBlock) { .end = size };

		if(List_pushBack(&allocationBuffer->allocations, vb, alloc).genericError)
			return NULL;

		return allocationBuffer->buffer.ptr + v.start;
	}

	//Grab area behind last allocation to see if there's still space

	AllocationBufferBlock last = *(const AllocationBufferBlock*) List_lastConst(allocationBuffer->allocations);
	U64 lastAlign = AllocationBufferBlock_alignTo(last.end, alignment);

	if (lastAlign + size <= len) {

		v = (AllocationBufferBlock) { .start = last.end, .end = lastAlign + size, .alignment = alignment };

		if(List_pushBack(&allocationBuffer->allocations, vb, alloc).genericError)
			return NULL;

		return allocationBuffer->buffer.ptr + lastAlign;
	}

	//Grab area before first allocation to see if there's still space

	AllocationBufferBlock first = *(const AllocationBufferBlock*) List_beginConst(allocationBuffer->allocations);

	if (size <= AllocationBufferBlock_getStart(first)) {

		v = (AllocationBufferBlock) { 
			.start = AllocationBufferBlock_alignToBackwards(AllocationBufferBlock_getStart(first) - size, alignment), 
			.end = AllocationBufferBlock_getStart(first) ,
			.alignment = alignment
		};

		if(List_pushFront(&allocationBuffer->allocations, vb, alloc).genericError)
			return NULL;

		return allocationBuffer->buffer.ptr + v.start;
	}

	//Try to find an empty spot in between.
	//This technically makes it not a ring buffer, but it mostly functions like one

	Buffer buf = Buffer_createNull();

	for (U64 i = 0; i < allocationBuffer->allocations.length; ++i) {

		if(List_get(allocationBuffer->allocations, i, &buf).genericError)
			return NULL;

		AllocationBufferBlock *b = (AllocationBufferBlock*)buf.ptr;
		v = *b;

		if(!AllocationBufferBlock_isFree(v))
			continue;

		if (size <= AllocationBufferBlock_size(v)) {

			//See if the buffer can hold this aligned as well

			U64 aligned = AllocationBufferBlock_alignTo(AllocationBufferBlock_getStart(v), alignment);

			if(aligned + size > v.end)
				continue;

			//We only split if >50% is left over.
			//Otherwise we scoop up the entire block.
			//This is to avoid tiny areas left over, causing the ring buffer portion to become slower.

			if (size * 3 / 2 >= AllocationBufferBlock_size(v)) {
				b->start &= ~((U64)1 << 63);
				b->alignment = alignment;
				return allocationBuffer->buffer.ptr + aligned;
			}

			//Splitting the buffer, ideally if we're near the back of the buffer 
			//we want to put the empty buffer at the back too.
			//This will make it easier for blocks at the end to merge.

			if (AllocationBufferBlock_getCenter(v) >= (len / 2)) {

				if(aligned + size != v.end) {

					AllocationBufferBlock empty = (AllocationBufferBlock) { 
						.start = aligned + size, 
						.end = v.end 
					};

					Buffer emptyb = Buffer_createRef(&empty, sizeof(empty));

					if(List_insert(&allocationBuffer->allocations, i + 1, emptyb, alloc).genericError)
						return NULL;
				}

				v.start &= ~((U64)1 << 63);		//Occupied
				v.end = aligned + size;
				v.alignment = alignment;

				((AllocationBufferBlock*)allocationBuffer->allocations.ptr)[i] = v;

				return allocationBuffer->buffer.ptr + aligned;
			}

			aligned = AllocationBufferBlock_alignToBackwards(v.end - size, alignment);

			if(aligned < AllocationBufferBlock_getStart(v))
				continue;

			if(aligned != AllocationBufferBlock_getStart(v)) {

				//Try to split near the front 

				AllocationBufferBlock empty = (AllocationBufferBlock) { 
					.start = AllocationBufferBlock_getStart(v), 
					.end = aligned
				};

				Buffer emptyb = Buffer_createRef(&empty, sizeof(empty));

				if(List_insert(&allocationBuffer->allocations, i, emptyb, alloc).genericError)
					return NULL;
			}

			Bool spaceLeft = aligned != AllocationBufferBlock_getStart(v);

			v.start = aligned;
			v.end = aligned + size;

			((AllocationBufferBlock*)allocationBuffer->allocations.ptr)[i + spaceLeft] = v;

			return allocationBuffer->buffer.ptr + aligned;
		}
	}

	return NULL;
}

Bool AllocationBuffer_freeBlock(AllocationBuffer *allocationBuffer, U8 *ptr) {

	if(!allocationBuffer || !ptr)
		return true;

	if(
		ptr < allocationBuffer->buffer.ptr || 
		ptr >= allocationBuffer->buffer.ptr + Buffer_length(allocationBuffer->buffer) ||
		!allocationBuffer->allocations.length
	)
		return false;

	//Middle block somewhere possibly, we have to find it first

	for (U64 i = 0, j = allocationBuffer->allocations.length; i < j; ++i) {

		AllocationBufferBlock *p = (AllocationBufferBlock*) List_ptr(allocationBuffer->allocations, i);

		if (!AllocationBufferBlock_isSame(*p, allocationBuffer->buffer.ptr, ptr)) 
			continue;

		p->start |= (U64)1 << 63;		//Free up
		U64 self = i;

		//Merge freed right blocks until they're gone

		AllocationBufferBlock *tmp;

		while (
			self + 1 < allocationBuffer->allocations.length && 
			AllocationBufferBlock_isFree(
				*(tmp = (AllocationBufferBlock*) List_ptr(allocationBuffer->allocations, self + 1))
			)
		) {
			p->end = tmp->end;
			List_popLocation(&allocationBuffer->allocations, self + 1, Buffer_createNull());
		}

		//Merge freed left blocks until they're gone

		while (
			self &&
			AllocationBufferBlock_isFree(
				*(tmp = (AllocationBufferBlock *)List_ptr(allocationBuffer->allocations, self - 1))
			)
		) {
			tmp->end = p->end;
			List_popLocation(&allocationBuffer->allocations, self, Buffer_createNull());
			--self;
		}

		//Check if it's the first block, so it can be popped

		if (
			allocationBuffer->allocations.length &&
			AllocationBufferBlock_isFree(*(AllocationBufferBlock*)List_begin(allocationBuffer->allocations))
		) {
			List_popFront(&allocationBuffer->allocations, Buffer_createNull());
			return true;
		}

		//Check if it's the last block, so it can be popped

		if (
			allocationBuffer->allocations.length &&
			AllocationBufferBlock_isFree(*(AllocationBufferBlock*)List_last(allocationBuffer->allocations))
		) {
			List_popBack(&allocationBuffer->allocations, Buffer_createNull());
			return true;
		}

		return true;
	}

	return false;
}
