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

#include "types/container/list.h"
#include "types/base/error.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/container/string.h"

GenericList ListVoid_toList(ListVoid v, U64 stride) {
	return (GenericList) {
		{ .ptr = v.ptr },
		.length = v.length,
		.stride = stride,
		.capacityAndRefInfo = v.capacityAndRefInfo
	};
}

Error ListVoid_fromList(GenericList list, U64 stride, ListVoid *result) {

	if(!result)
		return Error_nullPointer(2, "ListVoid_fromList()::result is required");

	if(result->ptr)
		return Error_invalidParameter(2, 0, "ListVoid_fromList()::result wasn't empty, could indicate memleak");

	if(list.stride != stride)
		return Error_invalidParameter(1, 0, "ListVoid_fromList()::result didn't have the right stride");

	*result = (ListVoid) { { .ptr = list.ptr }, .length = list.length, .capacityAndRefInfo = list.capacityAndRefInfo };
	return Error_none();
}

Bool GenericList_isConstRef(GenericList l) { return l.capacityAndRefInfo == U64_MAX; }
Bool GenericList_isRef(GenericList l) { return !l.capacityAndRefInfo || GenericList_isConstRef(l); }

Bool GenericList_empty(GenericList l) { return !l.length; }
Bool GenericList_any(GenericList l) { return l.length; }
U64  GenericList_bytes(GenericList l) { return l.length * l.stride; }
U64  GenericList_allocatedBytes(GenericList l) { return GenericList_isRef(l) ? 0 : l.capacityAndRefInfo * l.stride; }

U64 GenericList_capacity(GenericList l) { return GenericList_isRef(l) ? 0 : l.capacityAndRefInfo; }

Buffer GenericList_buffer(GenericList l) {
	return GenericList_isConstRef(l) ? Buffer_createNull() : Buffer_createRef(l.ptrNonConst, GenericList_bytes(l));
}

Buffer GenericList_allocatedBuffer(GenericList l) {
	return GenericList_isRef(l) ? Buffer_createNull() : Buffer_createRef(l.ptrNonConst, GenericList_allocatedBytes(l));
}

Buffer GenericList_bufferConst(GenericList l) { return Buffer_createRefConst(l.ptr, GenericList_bytes(l)); }

Buffer GenericList_allocatedBufferConst(GenericList l) {
	return Buffer_createRefConst(GenericList_isRef(l) ? NULL : l.ptr, GenericList_allocatedBytes(l));
}

void *GenericList_begin(GenericList list) { return GenericList_isConstRef(list) ? NULL : list.ptrNonConst; }

void *GenericList_end(GenericList list) {
	return GenericList_isConstRef(list) ? NULL : (U8*)list.ptrNonConst + GenericList_bytes(list);
}

void *GenericList_last(GenericList list) {
	return GenericList_isConstRef(list) || !list.length ? NULL : (U8*)list.ptrNonConst + GenericList_bytes(list) - list.stride;
}

const void *GenericList_beginConst(GenericList list) { return list.ptr; }
const void *GenericList_endConst(GenericList list) { return (const U8*)list.ptr + GenericList_bytes(list); }

const void *GenericList_lastConst(GenericList list) {
	return !list.length ? NULL : (const U8*) list.ptr + GenericList_bytes(list) - list.stride;
}

const void *GenericList_ptrConst(GenericList list, U64 elementOffset) {
	return elementOffset < list.length ? (const U8*)list.ptr + elementOffset * list.stride : NULL;
}

void *GenericList_ptr(GenericList list, U64 elementOffset) {
	return !GenericList_isConstRef(list) && elementOffset < list.length ?
		(U8*)list.ptrNonConst + elementOffset * list.stride : NULL;
}

Buffer GenericList_atConst(GenericList list, U64 offset) {
	return offset < list.length ?
		Buffer_createRefConst((const U8*)list.ptr + offset * list.stride, list.stride) : Buffer_createNull();
}

Buffer GenericList_at(GenericList list, U64 offset) {
	return offset < list.length && !GenericList_isConstRef(list) ?
		Buffer_createRef((U8*)list.ptrNonConst + offset * list.stride, list.stride) : Buffer_createNull();
}

Bool GenericList_eq(GenericList a, GenericList b) {
	return Buffer_eq(GenericList_bufferConst(a), GenericList_bufferConst(b));
}

Bool GenericList_neq(GenericList a, GenericList b) {
	return Buffer_neq(GenericList_bufferConst(a), GenericList_bufferConst(b));
}

GenericList GenericList_createEmpty(U64 stride) { return (GenericList) { .stride = stride }; }

Error GenericList_createReverse(GenericList list, Allocator allocator, GenericList *result) {
	return GenericList_createSubsetReverse(list, 0, list.length, allocator, result);
}

Bool GenericList_contains(GenericList list, Buffer buf, U64 offset, EqualsFunction eq) {
	return GenericList_findFirst(list, buf, offset, eq) != U64_MAX;
}

Error GenericList_create(U64 length, U64 stride, Allocator allocator, GenericList *result) {

	if(!result)
		return Error_nullPointer(3, "GenericList_create()::result is required");

	if (result->ptr)
		return Error_invalidOperation(0, "GenericList_create()::result wasn't empty, which might indicate memleak");

	if(!length && stride) {		//Keep the list empty
		*result = (GenericList) { .stride = stride };
		return Error_none();
	}

	if(!length || !stride)
		return Error_invalidParameter(!length ? 0 : 1, 0, "GenericList_create()::length and stride are required");

	if(length * stride / stride != length)
		return Error_overflow(0, length * stride, U64_MAX, "GenericList_create() overflow");

	Buffer buf = Buffer_createNull();
	const Error err = Buffer_createEmptyBytes(length * stride, allocator, &buf);

	if(err.genericError)
		return err;

	*result = (GenericList) {
		.ptr = buf.ptr,
		.length = length,
		.stride = stride,
		.capacityAndRefInfo = length
	};

	return Error_none();
}

Error GenericList_createRepeated(
	U64 length,
	U64 stride,
	Buffer data,
	Allocator allocator,
	GenericList *result
) {

	if(!result)
		return Error_nullPointer(4, "GenericList_createRepeated()::result is required");

	if (result->ptr)
		return Error_invalidOperation(0, "GenericList_createRepeated()::result wasn't empty, might indicate memleak");

	if(!length || !stride)
		return Error_invalidParameter(!length ? 0 : 1, 0, "GenericList_createRepeated()::length and stride are required");

	if(length * stride / stride != length)
		return Error_overflow(0, length * stride, U64_MAX, "GenericList_createRepeated() overflow");

	Buffer buf = Buffer_createNull();
	Error err = Buffer_createUninitializedBytes(length * stride, allocator, &buf);

	if(err.genericError)
		return err;

	*result = (GenericList) {
		.ptr = buf.ptr,
		.length = length,
		.stride = stride,
		.capacityAndRefInfo = length
	};

	for(U64 i = 0; i < length; ++i)
		if((err = GenericList_set(*result, i, data)).genericError) {
			Buffer_free(&buf, allocator);
			*result = (GenericList) { 0 };
			return err;
		}

	return Error_none();
}

Error GenericList_createCopy(GenericList list, Allocator allocator, GenericList *result) {

	if(!result)
		return Error_nullPointer(2, "GenericList_createCopy()::result is required");

	if (result->ptr)
		return Error_invalidOperation(0, "GenericList_createCopy()::result wasn't empty, might indicate memleak");

	if(GenericList_empty(list)) {
		*result = GenericList_createEmpty(list.stride);
		return Error_none();
	}

	const Error err = GenericList_create(list.length, list.stride, allocator, result);

	if(err.genericError)
		return err;

	Buffer_memcpy(GenericList_buffer(*result), GenericList_bufferConst(list));
	return Error_none();
}

Error GenericList_createCopySubset(GenericList list, U64 off, U64 len, Allocator allocator, GenericList *result) {

	GenericList tmp = (GenericList) { 0 };
	const Error err = GenericList_createSubset(list, off, len, &tmp);

	if(err.genericError)
		return err;

	return GenericList_createCopy(tmp, allocator, result);
}

Error GenericList_createSubset(GenericList list, U64 index, U64 length, GenericList *result) {

	if(!result || !length)
		return Error_nullPointer(3, "GenericList_createSubset()::result and length are required");

	if (result->ptr)
		return Error_invalidOperation(0, "GenericList_createSubset()::result wasn't empty, might indicate memleak");

	if(index + length < index)
		return Error_overflow(1, index + length, U64_MAX, "GenericList_createSubset() overflow");

	if(index + length > list.length)
		return Error_outOfBounds(1, index + length, list.length, "GenericList_createSubset()::index + length out of bounds");

	if(GenericList_isConstRef(list))
		return GenericList_createRefConst((const U8*)list.ptr + index * list.stride, length, list.stride, result);

	return GenericList_createRef((U8*)list.ptrNonConst + index * list.stride, length, list.stride, result);
}

Error GenericList_createSubsetReverse(
	GenericList list,
	U64 index,
	U64 length,
	Allocator allocator,
	GenericList *result
) {

	if(!result || !length)
		return Error_nullPointer(4, "GenericList_createSubsetReverse()::result and length are required");

	if(result->ptr)
		return Error_invalidOperation(0, "GenericList_createSubsetReverse()::result wasn't empty, might indicate memleak");

	if(index + length < length)
		return Error_overflow(1, index + length, U64_MAX, "GenericList_createSubsetReverse() overflow");

	if(index + length > list.length)
		return Error_outOfBounds(
			1, index + length, list.length, "GenericList_createSubsetReverse()::index + length out of bounds"
		);

	Error err = GenericList_create(length, list.stride, allocator, result);

	if(err.genericError)
		return err;

	for(U64 last = index + length - 1, i = 0; last != index - 1; --last, ++i) {

		Buffer buf = Buffer_createNull();

		if((err = GenericList_get(list, last, &buf)).genericError) {
			GenericList_free(result, allocator);
			return err;
		}

		if((err = GenericList_set(*result, i, buf)).genericError) {
			GenericList_free(result, allocator);
			return err;
		}
	}

	return Error_none();
}

Error GenericList_createRef(void *ptr, U64 length, U64 stride, GenericList *result) {

	if(!ptr || !result)
		return Error_nullPointer(!ptr ? 0 : 3, "GenericList_createRef()::ptr and result are required");

	if (result->ptr)
		return Error_invalidOperation(0, "GenericList_createRef()::result wasn't empty, might indicate memleak");

	if(!length || !stride)
		return Error_invalidParameter(!length ? 1 : 2, 0, "GenericList_createRef()::length and stride are required");

	if(length * stride / stride != length)
		return Error_overflow(1, length * stride, U64_MAX, "GenericList_createRef() overflow");

	*result = (GenericList) {
		.ptrNonConst = ptr,
		.length = length,
		.stride = stride
	};

	return Error_none();
}

Error GenericList_createRefConst(const void *ptr, U64 length, U64 stride, GenericList *result) {

	if(!ptr || !result)
		return Error_nullPointer(!ptr ? 0 : 3, "GenericList_createConstRef()::ptr and result are required");

	if (result->ptr)
		return Error_invalidOperation(0, "GenericList_createConstRef()::result wasn't empty, might indicate memleak");

	if(!length || !stride)
		return Error_invalidParameter(!length ? 1 : 2, 0, "GenericList_createConstRef()::length and stride are required");

	if(length * stride / stride != length)
		return Error_overflow(1, length * stride, U64_MAX, "GenericList_createConstRef() overflow");

	*result = (GenericList) {
		.ptr = ptr,
		.length = length,
		.stride = stride,
		.capacityAndRefInfo = U64_MAX
	};

	return Error_none();
}

GenericList GenericList_createRefFromList(GenericList list) {
	list.capacityAndRefInfo = U64_MAX;
	return list;
}

Error GenericList_set(GenericList list, U64 index, Buffer buf) {

	if(GenericList_isConstRef(list))
		return Error_constData(0, 0, "GenericList_set()::list is const");

	if(index >= list.length)
		return Error_outOfBounds(1, index, list.length, "GenericList_set()::index is out of bounds");

	const U64 bufLen = Buffer_length(buf);

	if(bufLen && bufLen != list.stride)
		return Error_invalidOperation(0, "GenericList_set()::buf.length incompatible with list");

	if(bufLen)
		Buffer_memcpy(Buffer_createRef((U8*)list.ptrNonConst + index * list.stride, list.stride), buf);

	else Buffer_unsetAllBits(Buffer_createRef((U8*)list.ptrNonConst + index * list.stride, list.stride));

	return Error_none();
}

Error GenericList_get(GenericList list, U64 index, Buffer *result) {

	if(!result)
		return Error_nullPointer(2, "GenericList_get()::result is required");

	if(index >= list.length)
		return Error_outOfBounds(1, index, list.length, "GenericList_get()::index out of bounds");

	if (GenericList_isConstRef(list)) {
		*result = Buffer_createRefConst((const U8*)list.ptr + index * list.stride, list.stride);
		return Error_none();
	}

	*result = Buffer_createRef((U8*)list.ptrNonConst + index * list.stride, list.stride);
	return Error_none();
}

Error GenericList_find(GenericList list, Buffer buf, EqualsFunction eq, Allocator allocator, ListU64 *result) {

	if(Buffer_length(buf) != list.stride)
		return Error_invalidParameter(1, 0, "GenericList_find()::buf.length incompatible with list");

	if(!result)
		return Error_nullPointer(3, "GenericList_find()::result is required");

	if(result->ptr)
		return Error_invalidParameter(3, 0, "GenericList_find()::result isn't empty, might indicate memleak");

	Error err = ListU64_reserve(result, list.length / 100 + 16, allocator);

	if(err.genericError)
		return err;

	for(U64 i = 0; i < list.length; ++i) {

		Bool b = !eq ? Buffer_eq(GenericList_atConst(list, i), buf) : eq(GenericList_ptrConst(list, i), buf.ptr);

		if(b && (err = ListU64_pushBack(result, i, allocator)).genericError) {
			ListU64_free(result, allocator);
			return err;
		}
	}

	return Error_none();
}

U64 GenericList_findFirst(GenericList list, Buffer buf, U64 index, EqualsFunction eq) {

	if(Buffer_length(buf) != list.stride)
		return U64_MAX;

	for(U64 i = index; i < list.length; ++i)
		if(!eq ? Buffer_eq(GenericList_atConst(list, i), buf) : eq(GenericList_ptrConst(list, i), buf.ptr))
			return i;

	return U64_MAX;
}

U64 GenericList_findLast(GenericList list, Buffer buf, U64 index, EqualsFunction eq) {

	if(Buffer_length(buf) != list.stride)
		return U64_MAX;

	for(U64 i = list.length - 1; i != U64_MAX && i >= index; --i)
		if(!eq ? Buffer_eq(GenericList_atConst(list, i), buf) : eq(GenericList_ptrConst(list, i), buf.ptr))
			return i;

	return U64_MAX;
}

U64 GenericList_count(GenericList list, Buffer buf, EqualsFunction eq) {

	if(Buffer_length(buf) != list.stride)
		return U64_MAX;

	U64 count = 0;

	for(U64 i = 0; i < list.length; ++i)
		if(!eq ? Buffer_eq(GenericList_atConst(list, i), buf) : eq(GenericList_ptrConst(list, i), buf.ptr))
			++count;

	return count;
}

Error GenericList_copy(GenericList src, U64 srcOffset, GenericList dst, U64 dstOffset, U64 count) {

	if(!src.length && !dst.length)
		return Error_none();

	GenericList srcOffsetted = (GenericList) { 0 };
	Error err = GenericList_createSubset(src, srcOffset, count, &srcOffsetted);

	if(err.genericError)
		return err;

	if(GenericList_isConstRef(dst))
		return Error_constData(2, 0, "GenericList_copy()::dst should be writable");

	GenericList dstOffsetted = (GenericList) { 0 };
	if((err = GenericList_createSubset(dst, dstOffset, count, &dstOffsetted)).genericError)
		return err;

	Buffer_memcpy(GenericList_buffer(dstOffsetted), GenericList_bufferConst(srcOffsetted));
	return Error_none();
}

Error GenericList_shrinkToFit(GenericList *list, Allocator allocator) {

	if(!list || !list->ptr)
		return Error_nullPointer(0, "GenericList_shrinkToFit()::list is required");

	if(GenericList_isRef(*list))
		return Error_constData(0, 0, "GenericList_shrinkToFit() is only allowed on managed memory");

	if(list->capacityAndRefInfo == list->length)
		return Error_none();

	GenericList copy = GenericList_createEmpty(list->stride);
	const Error err = GenericList_createCopy(*list, allocator, &copy);

	if(err.genericError)
		return err;

	GenericList_free(list, allocator);
	*list = copy;
	return err;
}

Error GenericList_eraseAllIndices(GenericList *list, ListU64 indices) {

	if(!list)
		return Error_nullPointer(0, "GenericList_eraseAllIndices()::list is required");

	if(GenericList_isRef(*list))
		return Error_constData(0, 0, "GenericList_eraseAllIndices()::list is only allowed on managed memory");

	if(!indices.length || !list->length)
		return Error_none();

	if(!GenericList_sortU64(ListU64_toList(indices)))
		return Error_invalidParameter(1, 0, "GenericList_eraseAllIndices()::indices sort failed");

	//Ensure none of them reference out of bounds or are duplicate

	U64 last = U64_MAX;

	for(const U64 *ptr = indices.ptr, *end = ListU64_endConst(indices); ptr < end; ++ptr) {

		if(last == *ptr)
			return Error_alreadyDefined(0, "GenericList_eraseAllIndices()::indices had a duplicate");

		if(*ptr >= list->length)
			return Error_outOfBounds(
				0, ptr - indices.ptr, list->length, "GenericList_eraseAllIndices()::indices[i] out of bounds"
			);

		last = *ptr;
	}

	//Since we're sorted from small to big,
	//we can just easily fetch where our next block of memory should go and move it backwards

	U64 curr = 0;

	for (const U64 *ptr = indices.ptr, *end = ListU64_endConst(indices); ptr < end; ++ptr) {

		if(!curr)
			curr = *ptr;

		const U64 me = *ptr + 1;
		const U64 neighbor = ptr + 1 != end ? *(ptr + 1) : list->length;

		if(me == neighbor)
			continue;

		Buffer_memmove(
			Buffer_createRef((U8*)list->ptrNonConst + curr * list->stride, (neighbor - me) * list->stride),
			Buffer_createRef((U8*)list->ptrNonConst + me * list->stride, (neighbor - me) * list->stride)
		);

		curr += neighbor - me;
	}

	list->length = curr;
	return Error_none();
}

//Easy insertion sort
//Uses about 8KB of cache on stack to hopefully sort quite quickly.
//Does mean that this can only be used for lists < 8KB.

Bool GenericList_insertionSort8K(GenericList list, CompareFunction func) {

	//for U8[8192] -> U64[1024]. Fits neatly into cache.
	//For bigger objects, qsort should probably be used.

	U8 sorted[8192];

	const U8 *tarr = GenericList_ptrConst(list, 0);
	U8 *tsorted = sorted;

	//Init first element
	Buffer_memcpy(Buffer_createRef(tsorted, list.stride), Buffer_createRefConst(tarr, list.stride));

	for(U64 j = 1; j < list.length; ++j) {

		const U8 *t = tarr + j * list.stride;
		U64 k = 0;

		//Check everything that came before us for order

		for (; k < j; ++k) {

			if (func(t, tsorted + k * list.stride) != ECompareResult_Lt)		//!isLess
				continue;

			//Move to the right

			for (U64 l = j; l != k; --l)
				Buffer_memmove(
					Buffer_createRef(tsorted + l * list.stride, list.stride),
					Buffer_createRef(tsorted + (l - 1) * list.stride, list.stride)
				);

			break;
		}

		//Now that everything's moved, we can insert ourselves

		Buffer_memcpy(
			Buffer_createRef(tsorted + k * list.stride, list.stride),
			Buffer_createRefConst(t, list.stride)
		);
	}

	Buffer_memcpy(GenericList_buffer(list), Buffer_createRefConst(sorted, sizeof(sorted)));
	return true;
}

#define TGenericList_tsort(T)																		\
ECompareResult sort##T(const T *a, const T *b) {													\
	return *a < *b ? ECompareResult_Lt : (*a > *b ? ECompareResult_Gt : ECompareResult_Eq);			\
}

#define TGenericList_sorts(f) f(U64); f(I64); f(U32); f(I32); f(U16); f(I16); f(U8); f(I8); f(F32); f(F64);

TGenericList_sorts(TGenericList_tsort);

//https://stackoverflow.com/questions/33884057/quick-sort-stackoverflow-error-for-large-arrays
//qsort U64 should be taking about ~76ms / 1M elem and ~13ms / 1M (sorted).
//qsort U8 should be taking about half the time for unsorted because of cache speedups.
//Expect F32 sorting to be at least 2x to 5x slower (and F64 even slower).
//(Profiled on a 3900x)

U64 GenericList_qpartition(GenericList list, U64 begin, U64 last, CompareFunction f) {

	U8 tmp[1024 * 2];		//We only support 1024 stride lists. We don't want to allocate

	Buffer_memcpy(
		Buffer_createRef(tmp, 1024),
		Buffer_createRefConst((const U8*)list.ptr + (begin + last) / 2 * list.stride, list.stride)
	);

	U64 i = begin - 1, j = last + 1;

	while (true) {

		while(++i < last && f((const U8*)list.ptr + i * list.stride, tmp) == ECompareResult_Lt);
		while(--j > begin && f((const U8*)list.ptr + j * list.stride, tmp) == ECompareResult_Gt);

		if(i < j) {

			Buffer_memcpy(
				Buffer_createRef(tmp + 1024, 1024),
				Buffer_createRefConst((const U8*)list.ptr + i * list.stride, list.stride)
			);

			Buffer_memcpy(
				Buffer_createRef((U8*)list.ptr + i * list.stride, list.stride),
				Buffer_createRefConst((const U8*)list.ptr + j * list.stride, list.stride)
			);

			Buffer_memcpy(
				Buffer_createRef((U8*)list.ptrNonConst + j * list.stride, list.stride),
				Buffer_createRefConst(tmp + 1024, 1024)
			);
		}

		else return j;
	}
}

Bool GenericList_qsortRecurse(GenericList list, U64 begin, U64 end, CompareFunction f) {

	if(begin >> 63 || end >> 63)
		return false;

	while(begin < end && end != U64_MAX) {

		const U64 pivot = GenericList_qpartition(list, begin, end, f);

		if (pivot == U64_MAX)		//Does return a modified array, but it's not fully sorted
			return false;

		if ((I64)pivot - begin <= (I64)end - (pivot + 1)) {
			GenericList_qsortRecurse(list, begin, pivot, f);
			begin = pivot + 1;
			continue;
		}

		GenericList_qsortRecurse(list, pivot + 1, end, f);
		end = pivot;
	}

	return true;
}

Bool GenericList_qsort(GenericList list, CompareFunction f) {
	return GenericList_qsortRecurse(list, 0, list.length - 1, f);
}

Bool GenericList_sortCustom(GenericList list, CompareFunction f) {

	if(list.length <= 1)
		return true;

	if(list.stride >= 1024)			//Current limitation, because we don't allocate.
		return false;

	if(GenericList_isConstRef(list))
		return false;

	if(GenericList_bytes(list) <= 8192)
		return GenericList_insertionSort8K(list, f);

	return GenericList_qsort(list, f);
}

#define TGenericList_sort(T) Bool GenericList_sort##T(GenericList l) { 	\
	return GenericList_sortCustom(l, (CompareFunction) sort##T); 		\
}

TGenericList_sorts(TGenericList_sort);

ECompareResult GenericList_compareString(const CharString *a, const CharString *b) {
	return CharString_compareSensitive(*a, *b);
}

ECompareResult GenericList_compareStringInsensitive(const CharString *a, const CharString *b) {
	return CharString_compareInsensitive(*a, *b);
}

Bool GenericList_sortString(GenericList list, EStringCase stringCase) {

	if(list.stride != sizeof(CharString))			//Current limitation, because we don't allocate.
		return false;

	return
		stringCase == EStringCase_Insensitive ?
		GenericList_sortCustom(list, (CompareFunction) GenericList_compareStringInsensitive) :
		GenericList_sortCustom(list, (CompareFunction) GenericList_compareString);
}

Bool GenericList_sortStringSensitive(GenericList list) { return GenericList_sortString(list, EStringCase_Sensitive); }
Bool GenericList_sortStringInsensitive(GenericList list) { return GenericList_sortString(list, EStringCase_Insensitive); }

Error GenericList_eraseFirst(GenericList *list, Buffer buf, U64 offset, EqualsFunction eq) {

	if(!list)
		return Error_nullPointer(0, "GenericList_eraseFirst()::list is required");

	const U64 ind = GenericList_findFirst(*list, buf, offset, eq);
	return ind == U64_MAX ? Error_none() : GenericList_erase(list, ind);
}

Error GenericList_eraseLast(GenericList *list, Buffer buf, U64 offset, EqualsFunction eq) {

	if(!list)
		return Error_nullPointer(0, "GenericList_eraseLast()::list is required");

	const U64 ind = GenericList_findLast(*list, buf, offset, eq);
	return ind == U64_MAX ? Error_none() : GenericList_erase(list, ind);
}

Error GenericList_eraseAll(GenericList *list, Buffer buf, Allocator allocator, EqualsFunction eq) {

	if(!list)
		return Error_nullPointer(0, "GenericList_eraseAll()::list is required");

	ListU64 indices = (ListU64) { 0 };
	Error err = GenericList_find(*list, buf, eq, allocator, &indices);

	if(err.genericError)
		return err;

	err = GenericList_eraseAllIndices(list, indices);
	ListU64_free(&indices, allocator);
	return err;
}

Error GenericList_erase(GenericList *list, U64 index) {

	if(!list)
		return Error_nullPointer(0, "GenericList_erase()::list is required");

	if(GenericList_isRef(*list))
		return Error_invalidParameter(0, 0, "GenericList_erase()::list needs to be managed memory");

	if(index >= list->length)
		return Error_outOfBounds(1, index, list->length, "GenericList_erase()::index out of bounds");

	if(index + 1 != list->length)
		Buffer_memmove(
			Buffer_createRef((U8*)list->ptrNonConst + index * list->stride, (list->length - 1 - index) * list->stride),
			Buffer_createRef((U8*)list->ptrNonConst + (index + 1) * list->stride, (list->length - 1 - index) * list->stride)
		);

	--list->length;
	return Error_none();
}

Error GenericList_resizeInternal(GenericList *list, U64 size, Allocator allocator, Bool doClear) {

	if(!list)
		return Error_nullPointer(0, "GenericList_resizeInternal()::list is required");

	if(GenericList_isRef(*list) && list->length)
		return Error_constData(0, 0, "GenericList_resizeInternal()::list has to be managed memory");

	if (size <= list->capacityAndRefInfo) {

		if(doClear)
			Buffer_unsetAllBits(
				Buffer_createRef((U8*)list->ptrNonConst + list->stride * list->length, (size - list->length) * list->stride)
			);

		list->length = size;
		return Error_none();
	}

	if(size * 3 / 3 != size)
		return Error_overflow(1, size * 3, U64_MAX, "GenericList_resizeInternal() overflow");

	const Error err = GenericList_reserve(list, size * 3 / 2, allocator);

	if(err.genericError)
		return err;

	if(doClear)
		Buffer_unsetAllBits(
			Buffer_createRef((U8*)list->ptrNonConst + list->stride * list->length, (size * 3 / 2 - list->length) * list->stride)
		);

	list->length = size;
	return Error_none();
}

Error GenericList_insert(GenericList *list, U64 index, Buffer buf, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0, "GenericList_insert()::list out of bounds");

	if(GenericList_isRef(*list) && list->length)
		return Error_invalidParameter(0, 0, "GenericList_insert()::list must be managed memory");

	if(list->stride != Buffer_length(buf))
		return Error_invalidOperation(0, "GenericList_insert()::stride and buf.length are incompatible");

	if(!(list->length + 1))
		return Error_overflow(0, 0, U64_MAX, "GenericList_insert() overflow");

	if (index == list->length) {		//Push back

		const Error err = GenericList_resizeInternal(list, list->length + 1, allocator, false);

		if(err.genericError)
			return err;

		Buffer_memcpy(
			Buffer_createRef((U8*)list->ptrNonConst + index * list->stride, list->stride),
			buf
		);

		return Error_none();
	}

	if(index >= list->length)
		return Error_outOfBounds(1, index, list->length, "GenericList_insert()::index out of bounds");

	const U64 prevSize = list->length;
	const Error err = GenericList_resizeInternal(list, list->length + 1, allocator, false);

	if(err.genericError)
		return err;

	//Copy our own data to the right

	if(prevSize)
		Buffer_memmove(
			Buffer_createRef((U8*)list->ptrNonConst + (index + 1) * list->stride, (prevSize - index) * list->stride),
			Buffer_createRef((U8*)list->ptrNonConst + index * list->stride, (prevSize - index) * list->stride)
		);

	//Copy the other data

	Buffer_memcpy(
		Buffer_createRef((U8*)list->ptrNonConst + index * list->stride, list->stride),
		buf
	);

	return Error_none();
}

Error GenericList_pushAll(GenericList *list, GenericList other, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0, "GenericList_pushAll()::list is required");

	if(GenericList_isRef(*list) && list->length)
		return Error_invalidParameter(0, 0, "GenericList_pushAll()::list must be managed memory");

	if(list->stride != other.stride)
		return Error_invalidOperation(0, "GenericList_pushAll()::list.stride and other.stride are incompatible");

	if(!other.length)
		return Error_none();

	if(list->length + other.length < list->length)
		return Error_overflow(0, list->length + other.length, U64_MAX, "GenericList_pushAll() overflow");

	const U64 oldSize = GenericList_bytes(*list);
	const Error err = GenericList_resizeInternal(list, list->length + other.length, allocator, false);

	if(err.genericError)
		return err;

	Buffer_memcpy(
		Buffer_createRef((U8*)list->ptrNonConst + oldSize, GenericList_bytes(other)),
		Buffer_createRefConst(other.ptr, GenericList_bytes(other))
	);

	return Error_none();
}

Error GenericList_swap(GenericList l, U64 i, U64 j) {

	if(i >= l.length || j >= l.length)
		return Error_outOfBounds(
			i >= l.length ? 1 : 2, i >= l.length ? i : j, l.length,
			"GenericList_swap()::i or j is out of bounds"
		);

	if(GenericList_isConstRef(l))
		return Error_constData(0, 0, "GenericList_swap()::list is const");

	U8 *iptr = GenericList_ptr(l, i);
	U8 *jptr = GenericList_ptr(l, j);

	//Based on the stride we can do it more optimally.
	//If it picks the unoptimized version (1 byte at a time) it will still run fine on Release mode.
	//However for debug that'd be a slowdown, so if swapping a lot on Debug, avoid misaligned GenericList.

	switch (l.stride & 7) {

		case 0:

			for (U64 off = 0; off < l.stride; off += 8) {
				const U64 t = *(const U64*)(iptr + off);
				*(U64*)(iptr + off) = *(const U64*)(jptr + off);
				*(U64*)(jptr + off) = t;
			}

			break;

		case 4:

			for (U64 off = 0; off < l.stride; off += 4) {
				const U32 t = *(const U32*)(iptr + off);
				*(U32*)(iptr + off) = *(const U32*)(jptr + off);
				*(U32*)(jptr + off) = t;
			}

			break;

		case 2:
		case 6:

			for (U64 off = 0; off < l.stride; off += 2) {
				const U16 t = *(const U16*)(iptr + off);
				*(U16*)(iptr + off) = *(const U16*)(jptr + off);
				*(U16*)(jptr + off) = t;
			}

			break;

		default:

			for (U64 off = 0; off < l.stride; ++off) {
				const U8 t = iptr[off];
				iptr[off] = jptr[off];
				jptr[off] = t;
			}

			break;
	}

	return Error_none();
}

Bool GenericList_reverse(GenericList l) {

	if(l.length <= 1)
		return true;

	if(GenericList_isConstRef(l))
		return false;

	if (l.length == 2) {
		GenericList_swap(l, 0, 1);
		return true;
	}

	for (U64 i = 0; i < l.length / 2; ++i) {
		U64 j = l.length - 1 - i;
		GenericList_swap(l, i, j);
	}

	return true;
}

Error GenericList_insertAll(GenericList *list, GenericList other, U64 offset, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0, "GenericList_insertAll()::list is required");

	if(list->length && GenericList_isRef(*list))
		return Error_invalidParameter(0, 0, "GenericList_insertAll()::list must be managed memory");

	if(!other.length)
		return Error_none();

	if(list->stride != other.stride)
		return Error_invalidOperation(0, "GenericList_insertAll()::list->stride and other.stride are incompatible");

	if(list->length + other.length < list->length)
		return Error_overflow(0, list->length + other.length, U64_MAX, "GenericList_insertAll() overflow");

	if(offset > list->length)
		return Error_outOfBounds(2, offset, list->length, "GenericList_insertAll()::offset out of bounds");

	const U64 prevSize = list->length;
	const Error err = GenericList_resizeInternal(list, list->length + other.length, allocator, false);

	if(err.genericError)
		return err;

	//Copy our own data to the right

	if(prevSize)
		Buffer_memmove(
			Buffer_createRef((U8*)list->ptrNonConst + (offset + other.length) * list->stride, (prevSize - offset) * list->stride),
			Buffer_createRef((U8*)list->ptrNonConst + offset * list->stride, (prevSize - offset) * list->stride)
		);

	//Copy the other data

	Buffer_memcpy(
		Buffer_createRef((U8*)list->ptrNonConst + offset * list->stride, other.length * list->stride),
		GenericList_bufferConst(other)
	);

	return Error_none();
}

Error GenericList_reserve(GenericList *list, U64 capacity, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0, "GenericList_reserve()::list is required");

	if(GenericList_isRef(*list) && list->length)
		return Error_invalidOperation(0, "GenericList_reserve()::list must be managed memory");

	if(!list->stride)
		return Error_invalidOperation(0, "GenericList_reserve()::stride is required");

	if(capacity * list->stride / list->stride != capacity)
		return Error_overflow(1, capacity * list->stride, U64_MAX, "GenericList_reserve() overflow");

	if(capacity <= GenericList_capacity(*list))
		return Error_none();

	Buffer buffer = Buffer_createNull();
	const Error err = Buffer_createUninitializedBytes(capacity * list->stride, allocator, &buffer);

	if(err.genericError)
		return err;

	Buffer_memcpy(buffer, GenericList_bufferConst(*list));

	Buffer curr = Buffer_createManagedPtr((U8*)list->ptrNonConst, GenericList_allocatedBytes(*list));

	if(Buffer_length(curr))
		Buffer_free(&curr, allocator);

	list->ptr = buffer.ptr;
	list->capacityAndRefInfo = capacity;
	return err;
}

Error GenericList_resize(GenericList *list, U64 size, Allocator allocator) {
	return GenericList_resizeInternal(list, size, allocator, true);
}

Error GenericList_pushBack(GenericList *list, Buffer buf, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0, "GenericList_pushBack()::list is required");

	if(GenericList_isRef(*list) && list->ptr)
		return Error_constData(0, 0, "GenericList_pushBack()::list needs to be managed memory");

	const Error err = GenericList_resizeInternal(list, list->length + 1, allocator, false);

	if(err.genericError)
		return err;

	return GenericList_set(*list, list->length - 1, buf);
}

Error GenericList_pushFront(GenericList *list, Buffer buf, Allocator allocator) {

	if(!list || !list->length)
		return GenericList_pushBack(list, buf, allocator);

	return GenericList_insert(list, 0, buf, allocator);
}

Error GenericList_popLocation(GenericList *list, U64 index, Buffer buf) {

	if(!list)
		return Error_nullPointer(0, "GenericList_popLocation()::list is required");

	if(index >= list->length)
		return Error_outOfBounds(1, index, list->length, "GenericList_popLocation()::index out of bounds");

	if(GenericList_isRef(*list))
		return Error_constData(0, 0, "GenericList_popLocation()::list needs to be managed memory");

	Buffer result = Buffer_createNull();
	const Error err = GenericList_get(*list, index, &result);

	if(err.genericError)
		return err;

	if(Buffer_length(buf)) {

		if(Buffer_length(buf) != Buffer_length(result))
			return Error_invalidOperation(0, "GenericList_popLocation()::buf.length and list->stride are incompatible");

		Buffer_memcpy(buf, result);
	}

	return GenericList_erase(list, index);
}

Error GenericList_popBack(GenericList *list, Buffer output) {

	if(!list)
		return Error_nullPointer(0, "GenericList_popBack()::list is required");

	if(!list->length)
		return Error_outOfBounds(0, 0, 0, "GenericList_popBack()::list.length needs to be at least 1");

	if(Buffer_length(output) && Buffer_length(output) != list->stride)
		return Error_invalidOperation(0, "GenericList_popBack()::output.length must be equal to list->stride");

	if(GenericList_isRef(*list))
		return Error_constData(0, 0, "GenericList_popBack()::list must be managed memory");

	if(Buffer_length(output))
		Buffer_memcpy(output, Buffer_createRefConst((const U8*)list->ptr + (list->length - 1) * list->stride, list->stride));

	--list->length;
	return Error_none();
}

Error GenericList_popFront(GenericList *list, Buffer output) {
	return GenericList_popLocation(list, 0, output);
}

Error GenericList_clear(GenericList *list) {

	if(!list)
		return Error_nullPointer(0, "GenericList_clear()::list is required");

	list->length = 0;
	return Error_none();
}

Bool GenericList_free(GenericList *result, Allocator allocator) {

	if(!result)
		return true;

	Bool err = true;

	if (!GenericList_isRef(*result) && result->capacityAndRefInfo) {
		Buffer buf = Buffer_createManagedPtr(result->ptrNonConst, GenericList_allocatedBytes(*result));
		err = Buffer_free(&buf, allocator);
	}

	*result = GenericList_createEmpty(result->stride);
	return err;
}
