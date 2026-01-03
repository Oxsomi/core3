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

#include "types/container/list_impl.h"
#include "types/container/allocation_buffer.h"
#include "types/base/allocator.h"
#include "types/math/math.h"
#include "types/base/constants.h"

TListImpl(AllocationBufferBlock);

Bool AllocationBuffer_create(const AllocationBufferCreate *create, Bool isVirtual, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = NULL;

	if(!create || !create->allocationBuffer || !create->size)
		retError(clean, Error_nullPointer(
			0, "AllocationBuffer_create()::create, create->size or create->allocationBuffer is NULL"
		));

	if(create->allocationBuffer->allocations.ptr)
		retError(clean, Error_invalidOperation(
			0, "AllocationBuffer_create()::allocationBuffer isn't NULL, might indicate memleak"
		));

	if(create->size >> 62)
		retError(clean, Error_invalidParameter(
			0, 0, "AllocationBuffer_create()::size is out of bounds (should be max 62-bit)"
		));

	if (!isVirtual) {
		gotoIfError3(clean, Buffer_createEmptyBytes(create->size, create->alloc, &create->allocationBuffer->buffer, e_rr));
	}

	else create->allocationBuffer->buffer = (Buffer) { .lengthAndRefBits = ((U64)3 << 62) | create->size };

	create->allocationBuffer->nonLinearAlignment = create->nonLinearAlignment;
	alloc = create->alloc;

	gotoIfError3(clean, ListAllocationBufferBlock_reserve(&create->allocationBuffer->allocations, 16, alloc, e_rr));

clean:

	if (alloc && !s_uccess) {
		Buffer_free(&create->allocationBuffer->buffer, alloc);		//Ignores if !ptr
		*create->allocationBuffer = (AllocationBuffer){ 0 };
	}

	return s_uccess;
}

Bool AllocationBuffer_createRefFromRegion(const AllocationBufferCreate *create, const Buffer origin, U64 offset, Error *e_rr) {

	Bool s_uccess = true;
	Bool alloc = false;

	if(!create || !create->allocationBuffer || !create->size)
		retError(clean, Error_nullPointer(
			!size ? 2 : 3, "AllocationBuffer_createRefFromRegion()::size or allocationBuffer is NULL"
		));

	if(create->allocationBuffer->allocations.ptr)
		retError(clean, Error_invalidOperation(
			0, "AllocationBuffer_createRefFromRegion()::allocationBuffer isn't NULL, might indicate memleak"
		));

	gotoIfError3(clean, Buffer_createSubset(origin, offset, create->size, false, &create->allocationBuffer->buffer, e_rr));

	create->allocationBuffer->nonLinearAlignment = create->nonLinearAlignment;
	alloc = true;

	gotoIfError3(clean, ListAllocationBufferBlock_reserve(&create->allocationBuffer->allocations, 16, alloc, e_rr));

clean:

	if (alloc && !s_uccess)
		*create->allocationBuffer = (AllocationBuffer){ 0 };

	return s_uccess;
}

void AllocationBuffer_free(AllocationBuffer *allocationBuffer, const Allocator *alloc) {

	if (!allocationBuffer)
		return;

	Buffer_free(&allocationBuffer->buffer, alloc);		//Ignores if !ptr
	ListAllocationBufferBlock_free(&allocationBuffer->allocations, alloc);
	*allocationBuffer = (AllocationBuffer) { 0 };
}

static inline U64 AllocationBufferBlock_getStart(AllocationBufferBlock block) {
	return block.startAndNonLinearAndFree << 2 >> 2;
}

static inline U64 AllocationBufferBlock_size(AllocationBufferBlock block) {
	return block.end - AllocationBufferBlock_getStart(block);
}

static inline U64 AllocationBufferBlock_isFree(AllocationBufferBlock block) {
	return block.startAndNonLinearAndFree >> 63;
}

static inline Bool AllocationBufferBlock_isNonLinear(AllocationBufferBlock block) {
	return (block.startAndNonLinearAndFree >> 62) & 1;
}

static inline U64 AllocationBufferBlock_getCenter(AllocationBufferBlock block) {
	return (AllocationBufferBlock_getStart(block) + block.end) >> 1;
}

static inline U64 AllocationBufferBlock_alignTo(U64 a, U64 alignment) {
	return alignment ? (a + alignment - 1) / alignment * alignment : a;
}

static inline U64 AllocationBufferBlock_alignToBackwards(U64 a, U64 alignment) {
	return alignment ? a / alignment * alignment : a;
}

static inline U64 AllocationBufferBlock_getAligned(AllocationBufferBlock block) {
	return AllocationBufferBlock_alignTo(AllocationBufferBlock_getStart(block), block.alignment);
}

static inline Bool AllocationBufferBlock_isSame(AllocationBufferBlock block, const U8 *start, const U8 *ptr) {
	const U64 blockStart = AllocationBufferBlock_getStart(block);
	const U64 aligned = AllocationBufferBlock_getAligned(block);
	return ptr == start + blockStart || ptr == start + aligned;
}

Bool AllocationBuffer_allocateAndFillBlock(const AllocationBufferAllocate *allocate, const Buffer data, U8 **result, Error *e_rr) {

	Bool s_uccess = true;
	const U8 *defaultPtr = (U8*)1, *ptr = defaultPtr;

	if(!allocate || !allocate->allocationBuffer || !allocate->allocationBuffer->buffer.ptr || !result)
		retError(clean, Error_nullPointer(
			!allocate->allocationBuffer ? 0 : (!allocate->allocationBuffer->buffer.ptr ? 0 : 4),
			"AllocationBuffer_allocateAndFillBlock()::allocate or allocate->allocationBuffer is NULL"
		));
	
	gotoIfError3(clean, AllocationBuffer_allocateBlock(allocate, Buffer_length(data), &ptr, e_rr));

	Buffer_memcpy(Buffer_createRef((U8*)ptr, Buffer_length(data)), data);
	*result = (U8*)ptr;

clean:

	if (!s_uccess && ptr != defaultPtr)		//Touch pointer so it can be checked if blocks are all gone or not.
		*result = NULL;

	return s_uccess;
}

Bool AllocationBuffer_allocateBlock(const AllocationBufferAllocate *allocate, U64 size, const U8 **result, Error *e_rr) {

	Bool s_uccess = true;

	if (!allocate || !allocate->allocationBuffer || !size || !allocate->alignment || !result)
		retError(clean, Error_nullPointer(
			!result ? 2 : (!size ? 1 : 0),
			"AllocationBuffer_allocateBlock()::allocate, allocate->allocationBuffer, result or size or alignment is 0/NULL"
		));

	AllocationBuffer *allocationBuffer = allocate->allocationBuffer;
	const U64 alignment = allocate->alignment;
	const Bool isNonLinearResource = allocate->isNonLinearResource;
	const Allocator *alloc = allocate->alloc;

	if(*result && *result != (const U8*)1)
		retError(clean, Error_invalidParameter(
			4, 0, "AllocationBuffer_allocateBlock()::*result is not NULL, might indicate memleak"
		));

	if(((size >> 48) || (alignment >> 48)) && allocationBuffer->buffer.ptr)
		retError(clean, Error_outOfBounds(
			size >> 48 ? 2 : 1, size >> 48 ? size : alignment, (U64)1 << 48,
			"AllocationBuffer_allocateBlock()::size or alignment is out of bounds (should be max 48-bit)"
		));

	const U64 len = Buffer_length(allocationBuffer->buffer);

	if(size > len || alignment > len) {
		*result = NULL;
		retError(clean, Error_outOfBounds(
			size > len ? 1 : 2, size > len ? size : alignment, len,
			"AllocationBuffer_allocateBlock()::size or alignment is bigger than allocationBuffer->buffer"
		));
	}

	//No allocations? We start at the front, it's always aligned

	if (ListAllocationBufferBlock_empty(allocationBuffer->allocations)) {

		const AllocationBufferBlock v = (AllocationBufferBlock) { 
			.startAndNonLinearAndFree = (U64)isNonLinearResource << 62,
			.end = size,
			.alignment = alignment
		};

		gotoIfError3(clean, ListAllocationBufferBlock_pushBack(&allocationBuffer->allocations, v, allocate->alloc, e_rr));

		*result = allocationBuffer->buffer.ptr;
		goto clean;
	}

	//Grab area behind last allocation to see if there's still space

	const AllocationBufferBlock last = *ListAllocationBufferBlock_last(allocationBuffer->allocations);

	U64 nonLinearAlignment = U64_max(allocationBuffer->nonLinearAlignment, alignment);
	Bool mismatchAlignment = AllocationBufferBlock_isNonLinear(last) != isNonLinearResource;
	U64 nextAlignment = mismatchAlignment ? nonLinearAlignment : alignment;

	const U64 lastAlign = AllocationBufferBlock_alignTo(last.end, nextAlignment);

	if (lastAlign + size <= len) {

		const AllocationBufferBlock v = (AllocationBufferBlock) {
			.startAndNonLinearAndFree = last.end | ((U64) isNonLinearResource << 62),
			.end = lastAlign + size,
			.alignment = nextAlignment
		};

		gotoIfError3(clean, ListAllocationBufferBlock_pushBack(&allocationBuffer->allocations, v, alloc, e_rr));

		*result = allocationBuffer->buffer.ptr + lastAlign;
		goto clean;
	}

	//Grab area before first allocation to see if there's still space

	const AllocationBufferBlock first = *allocationBuffer->allocations.ptr;
	U64 firstOff = AllocationBufferBlock_getStart(first);

	mismatchAlignment = AllocationBufferBlock_isNonLinear(first) != isNonLinearResource;

	if (size <= firstOff && (!mismatchAlignment || !(firstOff & (allocationBuffer->nonLinearAlignment - 1)))) {

		const AllocationBufferBlock v = (AllocationBufferBlock) {

			.startAndNonLinearAndFree =
				AllocationBufferBlock_alignToBackwards(firstOff - size, alignment) | ((U64)isNonLinearResource << 62),

			.end = firstOff,
			.alignment = alignment
		};

		gotoIfError3(clean, ListAllocationBufferBlock_pushFront(&allocationBuffer->allocations, v, alloc, e_rr));

		*result = allocationBuffer->buffer.ptr + AllocationBufferBlock_getStart(v);
		goto clean;
	}

	//Try to find an empty spot in between.
	//This technically makes it not a ring buffer, but it mostly functions like one

	for (U64 i = 0, j = allocationBuffer->allocations.length; i < j; ++i) {

		AllocationBufferBlock *b = &allocationBuffer->allocations.ptrNonConst[i], v = *b;

		if(!AllocationBufferBlock_isFree(v))
			continue;

		U64 vstart = AllocationBufferBlock_getStart(v);

		if (size <= AllocationBufferBlock_size(v)) {

			//See if the buffer can hold this aligned as well

			mismatchAlignment = AllocationBufferBlock_isNonLinear(v) != isNonLinearResource;
			nextAlignment = mismatchAlignment ? nonLinearAlignment : alignment;
			U64 aligned = AllocationBufferBlock_alignTo(vstart, nextAlignment);

			if(aligned + size > v.end)
				continue;

			//Also make sure that if our next block mismatches type but isn't aligned to allocationBuffer->nonLinearAlignment
			// that we skip checking this spot

			if (i + 1 < j) {

				const AllocationBufferBlock *next = &allocationBuffer->allocations.ptrNonConst[i + 1];
				
				if(
					AllocationBufferBlock_isNonLinear(*next) != isNonLinearResource &&
					(AllocationBufferBlock_getStart(*next) & (allocationBuffer->nonLinearAlignment - 1))
				)
					continue;
			}

			//We only split if >33% is left over.
			//Otherwise, we scoop up the entire block.
			//This is to avoid tiny areas left over, causing the ring buffer portion to become slower.

			if (size * 4 / 3 >= AllocationBufferBlock_size(v)) {
				b->startAndNonLinearAndFree &= ~((U64)3 << 62);
				b->startAndNonLinearAndFree |= (U64) isNonLinearResource << 62;
				b->alignment = nextAlignment;
				*result = allocationBuffer->buffer.ptr + aligned;
				goto clean;
			}

			//Splitting the buffer, ideally if we're near the back of the buffer
			//we want to put the empty buffer at the back too.
			//This will make it easier for blocks at the end to merge.

			if (AllocationBufferBlock_getCenter(v) >= (len / 2)) {

				if(aligned + size != v.end) {

					const AllocationBufferBlock empty = (AllocationBufferBlock) {
						.startAndNonLinearAndFree = (aligned + size) | ((U64)1 << 63),
						.end = v.end,
						.alignment = 1
					};

					gotoIfError3(clean, ListAllocationBufferBlock_insert(&allocationBuffer->allocations, i + 1, empty, alloc, e_rr));
				}

				//Occupied

				v.startAndNonLinearAndFree &= ~((U64)3 << 62);
				v.startAndNonLinearAndFree |= (U64) isNonLinearResource << 62;
				v.end = aligned + size;
				v.alignment = alignment;

				allocationBuffer->allocations.ptrNonConst[i] = v;
				*result = allocationBuffer->buffer.ptr + aligned;
				goto clean;
			}

			aligned = AllocationBufferBlock_alignToBackwards(v.end - size, nextAlignment);

			if(aligned < AllocationBufferBlock_getStart(v))
				continue;

			if(aligned != AllocationBufferBlock_getStart(v)) {

				//Try to split near the front

				const AllocationBufferBlock empty = (AllocationBufferBlock) {
					.startAndNonLinearAndFree = ((U64)1 << 63) | AllocationBufferBlock_getStart(v),
					.end = aligned,
					.alignment = 1
				};

				gotoIfError3(clean, ListAllocationBufferBlock_insert(&allocationBuffer->allocations, i, empty, alloc, e_rr));
			}

			const Bool spaceLeft = aligned != AllocationBufferBlock_getStart(v);

			v.startAndNonLinearAndFree = aligned | ((U64) isNonLinearResource << 62);
			v.end = aligned + size;

			allocationBuffer->allocations.ptrNonConst[i + spaceLeft] = v;
			*result = allocationBuffer->buffer.ptr + aligned;
			goto clean;
		}
	}

	*result = NULL;					//Write null so out of memory can be detected
	retError(clean, Error_outOfMemory(0, "AllocationBuffer_allocateBlock() out of memory"));

clean:
	return s_uccess;
}

void AllocationBuffer_freeBlock(AllocationBuffer *allocationBuffer, const U8 *ptr) {

	if(!allocationBuffer)
		return;

	if(
		ptr < allocationBuffer->buffer.ptr ||
		ptr >= allocationBuffer->buffer.ptr + Buffer_length(allocationBuffer->buffer) ||
		!allocationBuffer->allocations.length
	)
		return;

	//Middle block somewhere possibly, we have to find it first

	for (U64 i = 0, j = allocationBuffer->allocations.length; i < j; ++i) {

		AllocationBufferBlock *p = &allocationBuffer->allocations.ptrNonConst[i];

		if (!AllocationBufferBlock_isSame(*p, allocationBuffer->buffer.ptr, ptr))
			continue;

		p->startAndNonLinearAndFree |= (U64)1 << 63;		//Free up
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
			return;
		}

		//Check if it's the last block, so it can be popped

		if (
			allocationBuffer->allocations.length &&
			AllocationBufferBlock_isFree(*ListAllocationBufferBlock_last(allocationBuffer->allocations))
		) {
			ListAllocationBufferBlock_popBack(&allocationBuffer->allocations, NULL);
			return;
		}

		return;
	}
}

void AllocationBuffer_freeAll(AllocationBuffer *allocationBuffer) {
	ListAllocationBufferBlock_clear(&allocationBuffer->allocations);
}
