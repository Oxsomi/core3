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

#include "types/list.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/allocator.h"
#include "types/string.h"

Bool List_isConstRef(List l) { return l.capacityAndRefInfo == U64_MAX; }
Bool List_isRef(List l) { return !l.capacityAndRefInfo || List_isConstRef(l); }

Bool List_empty(List l) { return !l.length; }
Bool List_any(List l) { return l.length; }
U64  List_bytes(List l) { return l.length * l.stride; }
U64  List_allocatedBytes(List l) { return List_isRef(l) ? 0 : l.capacityAndRefInfo * l.stride; }

U64 List_capacity(List l) { return List_isRef(l) ? 0 : l.capacityAndRefInfo; }

Buffer List_buffer(List l) { 
	return List_isConstRef(l) ? Buffer_createNull() : Buffer_createRef((U8*)l.ptr, List_bytes(l)); 
}

Buffer List_allocatedBuffer(List l) { 
	return List_isRef(l) ? Buffer_createNull() : 
		Buffer_createRef((U8*)l.ptr, List_allocatedBytes(l)); 
}

Buffer List_bufferConst(List l) { return Buffer_createConstRef(l.ptr, List_bytes(l)); }

Buffer List_allocatedBufferConst(List l) { 
	return Buffer_createConstRef(List_isRef(l) ? NULL : l.ptr, List_allocatedBytes(l)); 
}

U8 *List_begin(List list) { return List_isConstRef(list) ? NULL : (U8*)list.ptr; }
U8 *List_end(List list) { return List_isConstRef(list) ? NULL : (U8*)list.ptr + List_bytes(list); }

U8 *List_last(List list) { 
	return List_isConstRef(list) || !list.length ? NULL : (U8*)list.ptr + List_bytes(list) - list.stride; 
}

const U8 *List_beginConst(List list) { return list.ptr; }
const U8 *List_endConst(List list) { return list.ptr + List_bytes(list); }
const U8 *List_lastConst(List list) { return !list.length ? NULL : list.ptr + List_bytes(list) - list.stride; }

const U8 *List_ptrConst(List list, U64 elementOffset) {
	return elementOffset < list.length ? list.ptr + elementOffset * list.stride : NULL;
}

U8 *List_ptr(List list, U64 elementOffset) {
	return !List_isConstRef(list) && elementOffset < list.length ? (U8*)list.ptr + elementOffset * list.stride : NULL;
}

Buffer List_atConst(List list, U64 offset) {
	return offset < list.length ? 
		Buffer_createConstRef(list.ptr + offset * list.stride, list.stride) : Buffer_createNull();
}

Buffer List_at(List list, U64 offset) {
	return offset < list.length && !List_isConstRef(list) ? 
		Buffer_createRef((U8*)list.ptr + offset * list.stride, list.stride) : Buffer_createNull();
}

Error List_eq(List a, List b, Bool *result) {
	return Buffer_eq(List_bufferConst(a), List_bufferConst(b), result);
}

Error List_neq(List a, List b, Bool *result) {
	return Buffer_neq(List_bufferConst(a), List_bufferConst(b), result);
}

List List_createEmpty(U64 stride) { return (List) { .stride = stride }; }

Error List_createReverse(List list, Allocator allocator, List *result) { 
	return List_createSubsetReverse(list, 0, list.length, allocator, result); 
}

Bool List_contains(List list, Buffer buf, U64 offset) { 
	return List_findFirst(list, buf, offset) != U64_MAX; 
}

Error List_create(U64 length, U64 stride, Allocator allocator, List *result) {

	if(!result)
		return Error_nullPointer(3);

	if (result->ptr)
		return Error_invalidOperation(0);

	if(!length || !stride)
		return Error_invalidParameter(!length ? 0 : 1, 0);

	if(length * stride / stride != length)
		return Error_overflow(0, length * stride, U64_MAX);

	Buffer buf = Buffer_createNull();
	Error err = Buffer_createEmptyBytes(length * stride, allocator, &buf);

	if(err.genericError)
		return err;

	*result = (List) { 
		.ptr = buf.ptr, 
		.length = length, 
		.stride = stride,
		.capacityAndRefInfo = length
	};

	return Error_none();
}

Error List_createFromBuffer(Buffer buf, U64 stride, List *result) {

	if(!result)
		return Error_nullPointer(3);

	if (result->ptr)
		return Error_invalidOperation(0);

	if(!buf.ptr || !stride)
		return Error_invalidParameter(!buf.ptr ? 0 : 1, 0);

	if(Buffer_length(buf) % stride)
		return Error_invalidParameter(0, 1);

	*result = (List) { 
		.ptr = buf.ptr, 
		.length = Buffer_length(buf) / stride, 
		.stride = stride,
		.capacityAndRefInfo = Buffer_isConstRef(buf) ? U64_MAX : 0
	};

	return Error_none();
}

Error List_createRepeated(
	U64 length,
	U64 stride,
	Buffer data,
	Allocator allocator,
	List *result
) {

	if(!result)
		return Error_nullPointer(4);

	if (result->ptr)
		return Error_invalidOperation(0);

	if(!length || !stride)
		return Error_invalidParameter(!length ? 0 : 1, 0);

	if(length * stride / stride != length)
		return Error_overflow(0, length * stride, U64_MAX);

	Buffer buf = Buffer_createNull();
	Error err = Buffer_createUninitializedBytes(length * stride, allocator, &buf);

	if(err.genericError)
		return err;

	*result = (List) { 
		.ptr = buf.ptr, 
		.length = length, 
		.stride = stride,
		.capacityAndRefInfo = length
	};

	for(U64 i = 0; i < length; ++i)
		if((err = List_set(*result, i, data)).genericError) {
			Buffer_free(&buf, allocator);
			*result = (List) { 0 };
			return err;
		}

	return Error_none();
}

Error List_createCopy(List list, Allocator allocator, List *result) {

	if(!result)
		return Error_nullPointer(2);

	if (result->ptr)
		return Error_invalidOperation(0);

	if(List_empty(list)) {
		*result = List_createEmpty(list.stride);
		return Error_none();
	}

	Error err = List_create(list.length, list.stride, allocator, result);

	if(err.genericError)
		return err;

	Buffer_copy(List_buffer(*result), List_bufferConst(list));
	return Error_none();
}

Error List_createSubset(List list, U64 index, U64 length, List *result) {

	if(!result)
		return Error_nullPointer(3);

	if (result->ptr)
		return Error_invalidOperation(0);

	if(!length)
		return Error_invalidParameter(2, 0);

	if(index + length < index)
		return Error_overflow(1, index + length, U64_MAX);

	if(index + length > list.length)
		return Error_outOfBounds(1, index + length, list.length);

	if(List_isConstRef(list))
		return List_createConstRef(list.ptr + index * list.stride, length, list.stride, result);

	return List_createRef((U8*)list.ptr + index * list.stride, length, list.stride, result);
}

Error List_createSubsetReverse(
	List list,
	U64 index,
	U64 length,
	Allocator allocator,
	List *result
) {
	
	if(!result)
		return Error_nullPointer(4);
	
	if(result->ptr)
		return Error_nullPointer(4);

	if(!length)
		return Error_invalidParameter(2, 0);

	if(index + length < length)
		return Error_overflow(1, index + length, U64_MAX);

	if(index + length > list.length)
		return Error_outOfBounds(1, index + length, list.length);

	Error err = List_create(length, list.stride, allocator, result);

	if(err.genericError)
		return err;

	for(U64 last = index + length - 1, i = 0; last != index - 1; --last, ++i) {

		Buffer buf = Buffer_createNull();

		if((err = List_getConst(list, last, &buf)).genericError) {
			List_free(result, allocator);
			return err;
		}

		if((err = List_set(*result, i, buf)).genericError) {
			List_free(result, allocator);
			return err;
		}
	}

	return Error_none();
}

Error List_createRef(U8 *ptr, U64 length, U64 stride, List *result) {

	if(!ptr || !result)
		return Error_nullPointer(!ptr ? 0 : 3);

	if (result->ptr)
		return Error_invalidOperation(0);

	if(!length || !stride)
		return Error_invalidParameter(!length ? 1 : 2, 0);

	if(length * stride / stride != length)
		return Error_overflow(1, length * stride, U64_MAX);

	*result = (List) { 
		.ptr = ptr, 
		.length = length, 
		.stride = stride 
	};

	return Error_none();
}

Error List_createConstRef(const U8 *ptr, U64 length, U64 stride, List *result) {

	if(!ptr || !result)
		return Error_nullPointer(!ptr ? 0 : 3);

	if (result->ptr)
		return Error_invalidOperation(0);

	if(!length || !stride)
		return Error_invalidParameter(!length ? 1 : 2, 0);

	if(length * stride / stride != length)
		return Error_overflow(1, length * stride, U64_MAX);

	*result = (List) { 
		.ptr = (U8*) ptr, 
		.length = length, 
		.stride = stride, 
		.capacityAndRefInfo = U64_MAX 
	};

	return Error_none();
}

Error List_set(List list, U64 index, Buffer buf) {

	if(List_isConstRef(list))
		return Error_constData(0, 0);

	if(index >= list.length)
		return Error_outOfBounds(1, index, list.length);

	U64 bufLen = Buffer_length(buf);

	if(bufLen && bufLen != list.stride)
		return Error_invalidOperation(0);

	if(bufLen)
		Buffer_copy(Buffer_createRef((U8*)list.ptr + index * list.stride, list.stride), buf);

	else Buffer_unsetAllBits(Buffer_createRef((U8*)list.ptr + index * list.stride, list.stride));

	return Error_none();
}

Error List_get(List list, U64 index, Buffer *result) {

	if(!result)
		return Error_nullPointer(2);

	if(index >= list.length)
		return Error_outOfBounds(1, index, list.length);

	if(List_isConstRef(list))
		return Error_constData(0, 0);

	*result = Buffer_createRef((U8*)list.ptr + index * list.stride, list.stride);
	return Error_none();
}

Error List_getConst(List list, U64 index, Buffer *result) {

	if(!result)
		return Error_nullPointer(2);

	if(index >= list.length)
		return Error_outOfBounds(1, index, list.length);

	*result = Buffer_createConstRef(list.ptr + index * list.stride, list.stride);
	return Error_none();
}

Error List_find(List list, Buffer buf, Allocator allocator, List *result) {

	if(Buffer_length(buf) != list.stride)
		return Error_invalidParameter(1, 0);

	if(!result)
		return Error_nullPointer(3);

	if(result->ptr)
		return Error_invalidParameter(3, 0);

	*result = List_createEmpty(sizeof(U64));
	Error err = List_reserve(result, list.length / 100 + 16, allocator);

	if(err.genericError)
		return err;

	for(U64 i = 0; i < list.length; ++i) {

		Bool b = false;
		err = Buffer_eq(List_atConst(list, i), buf, &b);

		if(err.genericError) {
			List_free(result, allocator);
			return err;
		}

		if(b && (err = List_pushBack(result, Buffer_createConstRef(&i, sizeof(i)), allocator)).genericError) {
			List_free(result, allocator);
			return err;
		}
	}

	return Error_none();
}

U64 List_findFirst(List list, Buffer buf, U64 index) {

	if(Buffer_length(buf) != list.stride)
		return U64_MAX;

	Error err;

	for(U64 i = index; i < list.length; ++i) {

		Bool b = false;
		err = Buffer_eq(List_atConst(list, i), buf, &b);

		if(err.genericError)
			continue;

		if(b)
			return i;
	}

	return U64_MAX;
}

U64 List_findLast(List list, Buffer buf, U64 index) {

	if(Buffer_length(buf) != list.stride)
		return U64_MAX;

	Error err;

	for(U64 i = list.length - 1; i != U64_MAX && i >= index; --i) {

		Bool b = false;
		err = Buffer_eq(List_atConst(list, i), buf, &b);

		if(err.genericError)
			continue;

		if(b)
			return i;
	}

	return U64_MAX;
}

U64 List_count(List list, Buffer buf) {

	if(Buffer_length(buf) != list.stride)
		return U64_MAX;

	U64 count = 0;
	Error err;

	for(U64 i = 0; i < list.length; ++i) {

		Bool b = false;
		err = Buffer_eq(List_atConst(list, i), buf, &b);

		if(err.genericError)
			continue;

		if(b)
			++count;
	}

	return count;
}

Error List_copy(List src, U64 srcOffset, List dst, U64 dstOffset, U64 count) {

	Error err = List_createSubset(src, srcOffset, count, &src);

	if(err.genericError)
		return err;

	if(List_isConstRef(dst))
		return Error_constData(2, 0);

	if((err = List_createSubset(dst, dstOffset, count, &dst)).genericError)
		return err;

	Buffer_copy(List_buffer(dst), List_bufferConst(src));
	return Error_none();
}

Error List_shrinkToFit(List *list, Allocator allocator) {

	if(!list || !list->ptr)
		return Error_nullPointer(0);

	if(List_isConstRef(*list))
		return Error_constData(0, 0);

	if(list->capacityAndRefInfo == list->length)
		return Error_none();

	List copy = List_createEmpty(list->stride);
	Error err = List_createCopy(*list, allocator, &copy);

	if(err.genericError)
		return err;

	List_free(list, allocator);
	*list = copy;
	return err;
}

Error List_eraseAllIndices(List *list, List indices) {

	if(!list)
		return Error_nullPointer(0);

	if(List_isConstRef(*list))
		return Error_constData(0, 0);

	if(!indices.length || !list->length)
		return Error_none();

	if(indices.stride != sizeof(U64))
		return Error_invalidOperation(0);

	if(!List_sortU64(indices))
		return Error_invalidParameter(1, 0);

	//Ensure none of them reference out of bounds or are duplicate

	U64 last = U64_MAX;

	for(U64 *ptr = (U64*)indices.ptr, *end = (U64*)List_end(indices); ptr < end; ++ptr) {

		if(last == *ptr)
			return Error_alreadyDefined(0);

		if(*ptr >= list->length)
			return Error_outOfBounds(0, ptr - (U64*)indices.ptr, list->length);

		last = *ptr;
	}

	//Since we're sorted from small to big, 
	//we can just easily fetch where our next block of memory should go and move it backwards

	U64 curr = 0;

	for (U64 *ptr = (U64*)indices.ptr, *end = (U64*)List_end(indices); ptr < end; ++ptr) {

		if(!curr)
			curr = *ptr;

		U64 me = *ptr + 1;
		U64 neighbor = ptr + 1 != end ? *(ptr + 1) : list->length;

		if(me == neighbor)
			continue;

		Buffer_revCopy(
			Buffer_createRef((U8*)list->ptr + curr * list->stride, (neighbor - me) * list->stride),
			Buffer_createConstRef(list->ptr + me * list->stride, (neighbor - me) * list->stride)
		);

		curr += neighbor - me;
	}

	list->length = curr;
	return Error_none();
}

//Easy insertion sort
//Uses about 8KB of cache on stack to hopefully sort quite quickly.
//Does mean that this can only be used for lists < 8KB.

inline Bool List_insertionSort8K(List list, CompareFunction func) {

	//for U8[8192] -> U64[1024]. Fits neatly into cache.
	//For bigger objects, qsort should probably be used.

	U8 sorted[8192];

	const U8 *tarr = List_ptrConst(list, 0);
	U8 *tsorted = sorted;

	//Init first element
	Buffer_copy(Buffer_createRef(tsorted, list.stride), Buffer_createConstRef(tarr, list.stride));

	for(U64 j = 1; j < list.length; ++j) {

		const U8 *t = tarr + j * list.stride;
		U64 k = 0;

		//Check everything that came before us for order

		for (; k < j; ++k) {

			if (func(t, tsorted + k * list.stride) != ECompareResult_Lt)		//!isLess
				continue;

			//Move to the right

			for (U64 l = j; l != k; --l)
				Buffer_copy(
					Buffer_createRef(tsorted + l * list.stride, list.stride),
					Buffer_createConstRef(tsorted + (l - 1) * list.stride, list.stride)
				);

			break;
		}

		//Now that everything's moved, we can insert ourselves

		Buffer_copy(
			Buffer_createRef(tsorted + k * list.stride, list.stride), 
			Buffer_createConstRef(t, list.stride)
		);
	}

	Buffer_copy(List_buffer(list), Buffer_createConstRef(sorted, sizeof(sorted)));
	return true;
}

#define TList_tsort(T)																		\
ECompareResult sort##T(const T *a, const T *b) {											\
	return *a < *b ? ECompareResult_Lt : (*a > *b ? ECompareResult_Gt : ECompareResult_Eq); \
}

#define TList_sorts(f) f(U64); f(I64); f(U32); f(I32); f(U16); f(I16); f(U8); f(I8); f(F32); f(F64);

TList_sorts(TList_tsort);

//https://stackoverflow.com/questions/33884057/quick-sort-stackoverflow-error-for-large-arrays
//qsort U64 should be taking about ~76ms / 1M elem and ~13ms / 1M (sorted).
//qsort U8 should be taking about half the time for unsorted because of cache speedups.
//Expect F32 sorting to be at least 2x to 5x slower (and F64 even slower).
//(Profiled on a 3900x)

inline U64 List_qpartition(List list, U64 begin, U64 last, CompareFunction f) {

	U8 tmp[1024 * 2];		//We only support 1024 stride lists. We don't want to allocate

	Buffer_copy(
		Buffer_createRef(tmp, 1024),
		Buffer_createConstRef(
			list.ptr + (begin + last) / 2 * list.stride,
			list.stride
		)
	);

	U64 i = begin - 1, j = last + 1;

	while (true) {

		while(++i < last && f(list.ptr + i * list.stride, tmp) == ECompareResult_Lt);
		while(--j > begin && f(list.ptr + j * list.stride, tmp) == ECompareResult_Gt);

		if(i < j) {

			Buffer_copy(
				Buffer_createRef(tmp + 1024, 1024),
				Buffer_createConstRef(
					list.ptr + i * list.stride,
					list.stride
				)
			);

			Buffer_copy(
				Buffer_createRef(
					(U8*)list.ptr + i * list.stride,
					list.stride
				),
				Buffer_createConstRef(
					list.ptr + j * list.stride,
					list.stride
				)

			);

			Buffer_copy(
				Buffer_createRef(
					(U8*)list.ptr + j * list.stride,
					list.stride
				),
				Buffer_createConstRef(tmp + 1024, 1024)
			);
		}

		else return j;
	}
}

inline Bool List_qsortRecurse(List list, U64 begin, U64 end, CompareFunction f) {

	if(begin >> 63 || end >> 63)
		return false;

	while(begin < end && end != U64_MAX) {

		U64 pivot = List_qpartition(list, begin, end, f);

		if (pivot == U64_MAX)		//Does return a modified array, but it's not fully sorted
			return false;

		if ((I64)pivot - begin <= (I64)end - (pivot + 1)) {
			List_qsortRecurse(list, begin, pivot, f);
			begin = pivot + 1;
			continue;
		}

		List_qsortRecurse(list, pivot + 1, end, f);
		end = pivot;
	}

	return true;
}

inline Bool List_qsort(List list, CompareFunction f) { return List_qsortRecurse(list, 0, list.length - 1, f); }

Bool List_sortCustom(List list, CompareFunction f) {

	if(list.length <= 1)
		return true;

	if(list.stride >= 1024)			//Current limitation, because we don't allocate.
		return false;

	if(List_isConstRef(list))
		return false;

	if(List_bytes(list) <= 8192)
		return List_insertionSort8K(list, f);

	return List_qsort(list, f);
}

#define TList_sort(T) Bool List_sort##T(List l) { return List_sortCustom(l, sort##T); }
TList_sorts(TList_sort);

ECompareResult List_compareString(const CharString *a, const CharString *b) {
	return CharString_compare(*a, *b, EStringCase_Sensitive);
}

ECompareResult List_compareStringInsensitive(const CharString *a, const CharString *b) {
	return CharString_compare(*a, *b, EStringCase_Insensitive);
}

Bool List_sortString(List list, EStringCase stringCase) {

	if(list.stride != sizeof(CharString))			//Current limitation, because we don't allocate.
		return false;

	return 
		stringCase == EStringCase_Insensitive ? List_sortCustom(list, (CompareFunction) List_compareStringInsensitive) :
		List_sortCustom(list, (CompareFunction) List_compareString);
}

Error List_eraseFirst(List *list, Buffer buf, U64 offset) {

	if(!list)
		return Error_nullPointer(0);

	U64 ind = List_findLast(*list, buf, offset);
	return ind == U64_MAX ? Error_none() : List_erase(list, ind);
}

Error List_eraseLast(List *list, Buffer buf, U64 offset) {

	if(!list)
		return Error_nullPointer(0);

	U64 ind = List_findLast(*list, buf, offset);
	return ind == U64_MAX ? Error_none() : List_erase(list, ind);
}

Error List_eraseAll(List *list, Buffer buf, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0);
	
	List indices = (List) { 0 };
	Error err = List_find(*list, buf, allocator, &indices);

	if(err.genericError)
		return err;

	err = List_eraseAllIndices(list, indices);
	List_free(&indices, allocator);
	return err;
}

Error List_erase(List *list, U64 index) {

	if(!list)
		return Error_nullPointer(0);

	if(List_isRef(*list))
		return Error_invalidParameter(0, 0);

	if(index >= list->length)
		return Error_outOfBounds(1, index, list->length);

	Buffer_revCopy(
		Buffer_createRef((U8*)list->ptr + index * list->stride, (list->length - 1) * list->stride),
		Buffer_createConstRef(list->ptr + (index + 1) * list->stride, (list->length - 1) * list->stride)
	);

	--list->length;
	return Error_none();
}

Error List_insert(List *list, U64 index, Buffer buf, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0);

	if(List_isRef(*list))
		return Error_invalidParameter(0, 0);

	if(list->stride != Buffer_length(buf))
		return Error_invalidOperation(0);

	if(list->length + 1 == 0)
		return Error_overflow(0, 0, U64_MAX);

	if (index == list->length) {		//Push back

		Error err = List_resize(list, list->length + 1, allocator);

		if(err.genericError)
			return err;

		Buffer_copy(
			Buffer_createRef((U8*)list->ptr + index * list->stride, list->stride), 
			buf
		);

		return Error_none();
	}

	if(index >= list->length)
		return Error_outOfBounds(1, index, list->length);

	U64 prevSize = list->length;
	Error err = List_resize(list, list->length + 1, allocator);

	if(err.genericError)
		return err;

	//Copy our own data to the right

	if(prevSize)
		Buffer_revCopy(
			Buffer_createRef((U8*)list->ptr + (index + 1) * list->stride, (prevSize - index) * list->stride), 
			Buffer_createConstRef((U8*)list->ptr + index * list->stride, (prevSize - index) * list->stride)
		);

	//Copy the other data

	Buffer_copy(
		Buffer_createRef((U8*)list->ptr + index * list->stride, list->stride), 
		buf
	);

	return Error_none();
}

Error List_pushAll(List *list, List other, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0);

	if(List_isRef(*list) && list->length)
		return Error_invalidParameter(0, 0);

	if(list->stride != other.stride)
		return Error_invalidOperation(0);

	if(!other.length)
		return Error_none();

	if(list->length + other.length < list->length)
		return Error_overflow(0, list->length + other.length, U64_MAX);

	U64 oldSize = List_bytes(*list);
	Error err = List_resize(list, list->length + other.length, allocator);

	if(err.genericError)
		return err;

	Buffer_copy(
		Buffer_createRef((U8*)list->ptr + oldSize, List_bytes(other)),
		Buffer_createConstRef(other.ptr, List_bytes(other))
	);

	return Error_none();
}

Error List_swap(List l, U64 i, U64 j) {

	if(i >= l.length || j >= l.length)
		return Error_outOfBounds(i >= l.length ? 1 : 2, i >= l.length ? i : j, l.length);

	if(List_isConstRef(l))
		return Error_constData(0, 0);

	U8 *iptr = List_ptr(l, i);
	U8 *jptr = List_ptr(l, j);

	U64 off = 0;

	for (; off + 7 < l.stride; off += 8) {
		U64 t = *(const U64*)(iptr + off);
		*(U64*)(iptr + off) = *(const U64*)(jptr + off);
		*(U64*)(jptr + off) = t;
	}

	for (; off < l.stride; ++off) {
		U8 t = iptr[off];
		iptr[off] = jptr[off];
		jptr[off] = t;
	}

	return Error_none();
}

Bool List_reverse(List l) {

	if(l.length <= 1)
		return true;

	if(List_isConstRef(l))
		return false;

	if (l.length == 2) {
		List_swap(l, 0, 1);
		return true;
	}

	for (U64 i = 0; i < l.length / 2; ++i) {
		U64 j = l.length - 1 - i;
		List_swap(l, i, j);
	}

	return true;
}

Error List_insertAll(List *list, List other, U64 offset, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0);

	if(List_isRef(*list))
		return Error_invalidParameter(0, 0);

	if(!other.length)
		return Error_none();

	if(list->stride != other.stride)
		return Error_invalidOperation(0);

	if(list->length + other.length < list->length)
		return Error_overflow(0, list->length + other.length, U64_MAX);

	if(offset >= list->length)
		return Error_outOfBounds(2, offset, list->length);

	U64 prevSize = list->length;
	Error err = List_resize(list, list->length + other.length, allocator);

	if(err.genericError)
		return err;

	//Copy our own data to the right

	if(prevSize)
		Buffer_revCopy(
			Buffer_createRef((U8*)list->ptr + (offset + other.length) * list->stride, (prevSize - offset) * list->stride), 
			Buffer_createConstRef(list->ptr + offset * list->stride, (prevSize - offset) * list->stride)
		);

	//Copy the other data

	Buffer_copy(
		Buffer_createRef((U8*)list->ptr + offset * list->stride, other.length * list->stride), 
		List_bufferConst(other)
	);

	return Error_none();
}

Error List_reserve(List *list, U64 capacity, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0);

	if(List_isRef(*list) && list->length)
		return Error_invalidOperation(0);

	if(capacity * list->stride / list->stride != capacity)
		return Error_overflow(1, capacity * list->stride, U64_MAX);

	if(capacity <= list->capacityAndRefInfo)
		return Error_none();

	Buffer buffer = Buffer_createNull();
	Error err = Buffer_createUninitializedBytes(capacity * list->stride, allocator, &buffer);

	if(err.genericError)
		return err;

	Buffer_copy(buffer, List_bufferConst(*list));

	Buffer curr = Buffer_createManagedPtr((U8*)list->ptr, List_allocatedBytes(*list));

	if(Buffer_length(curr))
		Buffer_free(&curr, allocator);

	list->ptr = buffer.ptr;
	list->capacityAndRefInfo = capacity;
	return err;
}

Error List_resize(List *list, U64 size, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0);

	if(List_isRef(*list) && list->length)
		return Error_constData(0, 0);

	if (size <= list->capacityAndRefInfo) {

		Buffer_unsetAllBits(
			Buffer_createRef((U8*)list->ptr + list->stride * list->length, (size - list->length) * list->stride)
		);

		list->length = size;
		return Error_none();
	}

	if(size * 3 / 3 != size)
		return Error_overflow(1, size * 3, U64_MAX);

	Error err = List_reserve(list, size * 3 / 2, allocator);

	if(err.genericError)
		return err;

	Buffer_unsetAllBits(
		Buffer_createRef((U8*)list->ptr + list->stride * list->length, (size * 3 / 2 - list->length) * list->stride)
	);

	list->length = size;
	return Error_none();
}

Error List_pushBack(List *list, Buffer buf, Allocator allocator) {

	if(!list)
		return Error_nullPointer(0);

	if(List_isConstRef(*list))
		return Error_constData(0, 0);

	Error err = List_resize(list, list->length + 1, allocator);

	if(err.genericError)
		return err;
	
	return List_set(*list, list->length - 1, buf);
}

Error List_pushFront(List *list, Buffer buf, Allocator allocator) {

	if(!list || !list->length)
		return List_pushBack(list, buf, allocator);

	return List_insert(list, 0, buf, allocator);
}

Error List_popLocation(List *list, U64 index, Buffer buf) {

	if(!list)
		return Error_nullPointer(0);

	if(index >= list->length)
		return Error_outOfBounds(1, index, list->length);

	if(List_isConstRef(*list))
		return Error_constData(0, 0);

	Buffer result = Buffer_createNull();
	Error err = List_getConst(*list, index, &result);

	if(err.genericError)
		return err;

	if(Buffer_length(buf)) {

		if(Buffer_length(buf) != Buffer_length(result))
			return Error_invalidOperation(0);

		Buffer_copy(buf, result);
	}

	return List_erase(list, index);
}

Error List_popBack(List *list, Buffer output) {

	if(!list)
		return Error_nullPointer(0);

	if(!list->length)
		return Error_outOfBounds(0, 0, 0);

	if(Buffer_length(output) && Buffer_length(output) != list->stride)
		return Error_invalidOperation(0);

	if(List_isConstRef(*list))
		return Error_constData(0, 0);

	if(Buffer_length(output))
		Buffer_copy(output, Buffer_createConstRef(list->ptr + (list->length - 1) * list->stride, list->stride));

	--list->length;
	return Error_none();
}

Error List_popFront(List *list, Buffer output) {
	return List_popLocation(list, 0, output);
}

Error List_clear(List *list) {

	if(!list)
		return Error_nullPointer(0);

	list->length = 0;
	return Error_none();
}

Bool List_free(List *result, Allocator allocator) {

	if(!result)
		return true;

	Bool err = true;

	if (!List_isRef(*result) && result->length) {
		Buffer buf = Buffer_createManagedPtr((U8*)result->ptr, List_allocatedBytes(*result));
		err = Buffer_free(&buf, allocator);
	}

	*result = List_createEmpty(result->stride);
	return err;
}
