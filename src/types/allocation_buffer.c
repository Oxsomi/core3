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

#include "types/list_impl.h"
#include "types/allocation_buffer.h"
#include "types/allocator.h"

TListImpl(AllocationBufferBlock);

Error AllocationBuffer_create(U64 size, Bool isVirtual, Allocator alloc, AllocationBuffer *allocationBuffer) {

	if(!allocationBuffer || !size)
		return Error_nullPointer(!size ? 0 : 2, "AllocationBuffer_create()::size or allocationBuffer is NULL");

	if(allocationBuffer->allocations.ptr)
		return Error_invalidOperation(0, "AllocationBuffer_create()::allocationBuffer isn't NULL, might indicate memleak");

	if(size >> 48 && !isVirtual)
		return Error_invalidParameter(0, 0, "AllocationBuffer_create()::size is out of bounds (should be max 48-bit)");

	Error err = Error_none();

	if(!isVirtual) {

		err = Buffer_createEmptyBytes(size, alloc, &allocationBuffer->buffer);

		if(err.genericError)
			return err;
	}

	else allocationBuffer->buffer = (Buffer) { .lengthAndRefBits = ((U64)3 << 62) | size };

	if ((err = ListAllocationBufferBlock_reserve(&allocationBuffer->allocations, 16, alloc)).genericError) {
		Buffer_free(&allocationBuffer->buffer, alloc);		//Ignores if !ptr
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
		return Error_nullPointer(!size ? 2 : 3, "AllocationBuffer_createRefFromRegion()::size or allocationBuffer is NULL");

	if(allocationBuffer->allocations.ptr)
		return Error_invalidOperation(
			0, "AllocationBuffer_createRefFromRegion()::allocationBuffer isn't NULL, might indicate memleak"
		);

	Error err = Buffer_createSubset(origin, offset, size, false, &allocationBuffer->buffer);

	if(err.genericError)
		return err;

	if((err = ListAllocationBufferBlock_reserve(&allocationBuffer->allocations, 16, alloc)).genericError) {
		*allocationBuffer = (AllocationBuffer) { 0 };
		return err;
	}

	return Error_none();
}

Bool AllocationBuffer_free(AllocationBuffer *allocationBuffer, Allocator alloc) {

	if (!allocationBuffer)
		return true;

	Bool success = Buffer_free(&allocationBuffer->buffer, alloc);		//Ignores if !ptr
	success &= ListAllocationBufferBlock_free(&allocationBuffer->allocations, alloc);
	*allocationBuffer = (AllocationBuffer) { 0 };
	return success;
}

U64 AllocationBufferBlock_getStart(AllocationBufferBlock block) {
	return block.start << 1 >> 1;
}

U64 AllocationBufferBlock_size(AllocationBufferBlock block) {
	return block.end - AllocationBufferBlock_getStart(block);
}

U64 AllocationBufferBlock_isFree(AllocationBufferBlock block) {
	return block.start >> 63;
}

U64 AllocationBufferBlock_getCenter(AllocationBufferBlock block) {
	return (AllocationBufferBlock_getStart(block) + block.end) >> 1;
}

U64 AllocationBufferBlock_alignTo(U64 a, U64 alignment) {
	return alignment ? (a + alignment - 1) / alignment * alignment : a;
}

U64 AllocationBufferBlock_alignToBackwards(U64 a, U64 alignment) {
	return alignment ? a / alignment * alignment : a;
}

U64 AllocationBufferBlock_getAligned(AllocationBufferBlock block) {
	return AllocationBufferBlock_alignTo(AllocationBufferBlock_getStart(block), block.alignment);
}

Bool AllocationBufferBlock_isSame(AllocationBufferBlock block, const U8 *start, const U8 *ptr) {
	U64 blockStart = AllocationBufferBlock_getStart(block);
	U64 aligned = AllocationBufferBlock_getAligned(block);
	return ptr == start + blockStart || ptr == start + aligned;
}

Error AllocationBuffer_allocateAndFillBlock(
	AllocationBuffer *allocationBuffer,
	Buffer data,
	U64 alignment,
	Allocator alloc,
	U8 **result
) {

	if(!allocationBuffer || !allocationBuffer->buffer.ptr || !result)
		return Error_nullPointer(
			!allocationBuffer ? 0 : (!allocationBuffer->buffer.ptr ? 0 : 4),
			"AllocationBuffer_allocateAndFillBlock()::allocationBuffer is NULL"
		);

	const U8 *defaultPtr = (U8*)1, *ptr = defaultPtr;
	Error err = AllocationBuffer_allocateBlock(allocationBuffer, Buffer_length(data), alignment, alloc, &ptr);

	if (err.genericError && ptr != defaultPtr) {	//Touch pointer so it can be checked if blocks are all gone or not.
		*result = NULL;
		return err;
	}

	if(err.genericError)
		return err;

	Buffer_copy(Buffer_createRef((U8*)ptr, Buffer_length(data)), data);
	*result = (U8*)ptr;
	return Error_none();
}

Error AllocationBuffer_allocateBlock(
	AllocationBuffer *allocationBuffer,
	U64 size,
	U64 alignment,
	Allocator alloc,
	const U8 **result
) {

	if (!allocationBuffer || !size || !alignment || !result)
		return Error_nullPointer(
			!allocationBuffer ? 0 : (!size ? 1 : (!alignment ? 2 : 4)),
			"AllocationBuffer_allocateBlock()::allocationBuffer or result is NULL or size or alignment is 0"
		);

	if(*result && *result != (const U8*)1)
		return Error_invalidParameter(4, 0, "AllocationBuffer_allocateBlock()::*result is not NULL, might indicate memleak");

	if(((size >> 48) || (alignment >> 48)) && allocationBuffer->buffer.ptr)
		return Error_outOfBounds(
			size >> 48 ? 2 : 1, size >> 48 ? size : alignment, (U64)1 << 48,
			"AllocationBuffer_allocateBlock()::size or alignment is out of bounds (should be max 48-bit)"
		);

	U64 len = Buffer_length(allocationBuffer->buffer);

	if(size > len || alignment > len) {
		*result = NULL;
		return Error_outOfBounds(
			size > len ? 1 : 2, size > len ? size : alignment, len,
			"AllocationBuffer_allocateBlock()::size or alignment is bigger than allocationBuffer->buffer"
		);
	}

	//No allocations? We start at the front, it's always aligned

	if (ListAllocationBufferBlock_empty(allocationBuffer->allocations)) {

		AllocationBufferBlock v = (AllocationBufferBlock) { .end = size, .alignment = alignment };
		Error err = ListAllocationBufferBlock_pushBack(&allocationBuffer->allocations, v, alloc);

		if(err.genericError)
			return err;

		*result = allocationBuffer->buffer.ptr + v.start;
		return Error_none();
	}

	//Grab area behind last allocation to see if there's still space

	AllocationBufferBlock last = *ListAllocationBufferBlock_last(allocationBuffer->allocations);
	U64 lastAlign = AllocationBufferBlock_alignTo(last.end, alignment);

	if (lastAlign + size <= len) {

		AllocationBufferBlock v = (AllocationBufferBlock) {
			.start = last.end, .end = lastAlign + size, .alignment = alignment
		};

		Error err = ListAllocationBufferBlock_pushBack(&allocationBuffer->allocations, v, alloc);

		if(err.genericError)
			return err;

		*result = allocationBuffer->buffer.ptr + lastAlign;
		return Error_none();
	}

	//Grab area before first allocation to see if there's still space

	AllocationBufferBlock first = *allocationBuffer->allocations.ptr;

	if (size <= AllocationBufferBlock_getStart(first)) {

		AllocationBufferBlock v = (AllocationBufferBlock) {
			.start = AllocationBufferBlock_alignToBackwards(AllocationBufferBlock_getStart(first) - size, alignment),
			.end = AllocationBufferBlock_getStart(first),
			.alignment = alignment
		};

		Error err = ListAllocationBufferBlock_pushFront(&allocationBuffer->allocations, v, alloc);

		if(err.genericError)
			return err;

		*result = allocationBuffer->buffer.ptr + v.start;
		return Error_none();
	}

	//Try to find an empty spot in between.
	//This technically makes it not a ring buffer, but it mostly functions like one

	for (U64 i = 0; i < allocationBuffer->allocations.length; ++i) {

		AllocationBufferBlock *b = &allocationBuffer->allocations.ptrNonConst[i], v = *b;

		if(!AllocationBufferBlock_isFree(v))
			continue;

		if (size <= AllocationBufferBlock_size(v)) {

			//See if the buffer can hold this aligned as well

			U64 aligned = AllocationBufferBlock_alignTo(AllocationBufferBlock_getStart(v), alignment);

			if(aligned + size > v.end)
				continue;

			//We only split if >33% is left over.
			//Otherwise we scoop up the entire block.
			//This is to avoid tiny areas left over, causing the ring buffer portion to become slower.

			if (size * 4 / 3 >= AllocationBufferBlock_size(v)) {
				b->start &= ~((U64)1 << 63);
				b->alignment = alignment;
				*result = allocationBuffer->buffer.ptr + aligned;
				return Error_none();
			}

			//Splitting the buffer, ideally if we're near the back of the buffer
			//we want to put the empty buffer at the back too.
			//This will make it easier for blocks at the end to merge.

			if (AllocationBufferBlock_getCenter(v) >= (len / 2)) {

				if(aligned + size != v.end) {

					AllocationBufferBlock empty = (AllocationBufferBlock) {
						.start = aligned + size,
						.end = v.end,
						.alignment = 1
					};

					Error err = ListAllocationBufferBlock_insert(&allocationBuffer->allocations, i + 1, empty, alloc);

					if(err.genericError)
						return err;
				}

				v.start &= ~((U64)1 << 63);		//Occupied
				v.end = aligned + size;
				v.alignment = alignment;

				allocationBuffer->allocations.ptrNonConst[i] = v;
				*result = allocationBuffer->buffer.ptr + aligned;
				return Error_none();
			}

			aligned = AllocationBufferBlock_alignToBackwards(v.end - size, alignment);

			if(aligned < AllocationBufferBlock_getStart(v))
				continue;

			if(aligned != AllocationBufferBlock_getStart(v)) {

				//Try to split near the front

				AllocationBufferBlock empty = (AllocationBufferBlock) {
					.start = AllocationBufferBlock_getStart(v),
					.end = aligned,
					.alignment = 1
				};

				Error err = ListAllocationBufferBlock_insert(&allocationBuffer->allocations, i, empty, alloc);

				if(err.genericError)
					return err;
			}

			Bool spaceLeft = aligned != AllocationBufferBlock_getStart(v);

			v.start = aligned;
			v.end = aligned + size;

			allocationBuffer->allocations.ptrNonConst[i + spaceLeft] = v;
			*result = allocationBuffer->buffer.ptr + aligned;
			return Error_none();
		}
	}

	*result = NULL;					//Write null so out of memory can be detected
	return Error_outOfMemory(0, "AllocationBuffer_allocateBlock() out of memory");
}

Bool AllocationBuffer_freeBlock(AllocationBuffer *allocationBuffer, const U8 *ptr) {

	if(!allocationBuffer)
		return true;

	if(
		ptr < allocationBuffer->buffer.ptr ||
		ptr >= allocationBuffer->buffer.ptr + Buffer_length(allocationBuffer->buffer) ||
		!allocationBuffer->allocations.length
	)
		return false;

	//Middle block somewhere possibly, we have to find it first

	for (U64 i = 0, j = allocationBuffer->allocations.length; i < j; ++i) {

		AllocationBufferBlock *p = &allocationBuffer->allocations.ptrNonConst[i];

		if (!AllocationBufferBlock_isSame(*p, allocationBuffer->buffer.ptr, ptr))
			continue;

		p->start |= (U64)1 << 63;		//Free up
		U64 self = i;

		//Merge freed right blocks until they're gone

		AllocationBufferBlock *tmp;

		while (
			self + 1 < allocationBuffer->allocations.length &&
			AllocationBufferBlock_isFree(
				*(tmp = &allocationBuffer->allocations.ptrNonConst[self + 1])
			)
		) {
			p->end = tmp->end;
			ListAllocationBufferBlock_popLocation(&allocationBuffer->allocations, self + 1, NULL);
		}

		//Merge freed left blocks until they're gone

		while (
			self &&
			AllocationBufferBlock_isFree(
				*(tmp = &allocationBuffer->allocations.ptrNonConst[self - 1])
			)
		) {
			tmp->end = p->end;
			ListAllocationBufferBlock_popLocation(&allocationBuffer->allocations, self, NULL);
			--self;
		}

		//Check if it's the first block, so it can be popped

		if (
			allocationBuffer->allocations.length &&
			AllocationBufferBlock_isFree(*allocationBuffer->allocations.ptr)
		) {
			ListAllocationBufferBlock_popFront(&allocationBuffer->allocations, NULL);
			return true;
		}

		//Check if it's the last block, so it can be popped

		if (
			allocationBuffer->allocations.length &&
			AllocationBufferBlock_isFree(*ListAllocationBufferBlock_last(allocationBuffer->allocations))
		) {
			ListAllocationBufferBlock_popBack(&allocationBuffer->allocations, NULL);
			return true;
		}

		return true;
	}

	return false;
}

Bool AllocationBuffer_freeAll(AllocationBuffer *allocationBuffer) {
	return !ListAllocationBufferBlock_clear(&allocationBuffer->allocations).genericError;
}
