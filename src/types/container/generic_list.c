/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

//types/container/generic_list.c

#include "types/container/list.h"
#include "types/base/error.h"
#include "types/base/string_read.h"
#include "types/container/buffer.h"
#include "types/container/list_basic_types.h"
#include "types/base/allocator.h"
#include "types/container/string.h"
#include "types/base/constants.h"

Bool ListVoid_fromList(const GenericList list, U64 stride, ListVoid *result, Error *e_rr) {

	Bool s_uccess = true;

	if(!result)
		retError(clean, Error_nullPointer(2, "ListVoid_fromList()::list and result are required"));

	if (result->ptr)
		retError(clean, Error_invalidParameter(2, 0, "ListVoid_fromList()::result wasn't empty, could indicate memleak"));

	if(list.stride != stride)
		retError(clean, Error_invalidParameter(1, 0, "ListVoid_fromList()::result didn't have the right stride"));

	*result = (ListVoid) { { .ptr = list.ptr }, .length = list.length, .capacityAndRefInfo = list.capacityAndRefInfo };

clean:
	return s_uccess;
}

Bool GenericList_create(U64 length, U64 stride, const Allocator *allocator, GenericList *result, Error *e_rr) {

	Bool s_uccess = true;

	if(!result)
		retError(clean, Error_nullPointer(3, "GenericList_create()::result is required"));

	if (result->ptr)
		retError(clean, Error_invalidOperation(0, "GenericList_create()::result wasn't empty, which might indicate memleak"));

	if(!length && stride) {        //Keep the list empty
		*result = GenericList_createEmpty(stride);
		goto clean;
	}

	if(!length || !stride)
		retError(clean, Error_invalidParameter(!length ? 0 : 1, 0, "GenericList_create()::length and stride are required"));

	if(length * stride / stride != length)
		retError(clean, Error_overflow(0, length * stride, U64_MAX, "GenericList_create() overflow"));

	Buffer buf = Buffer_createNull();
	gotoIfError3(clean, Buffer_createEmptyBytes(length * stride, allocator, &buf, e_rr));

	*result = (GenericList) {
		.ptr = buf.ptr,
		.length = length,
		.stride = stride,
		.capacityAndRefInfo = length
	};

clean:
	return s_uccess;
}

Bool GenericList_createRepeated(
	U64 length,
	U64 stride,
	const Buffer *data,
	const Allocator *allocator,
	GenericList *result,
	Error *e_rr
) {

	Bool s_uccess = true;
	Buffer buf = Buffer_createNull();
	Bool alloc = false;

	if(!result)
		retError(clean, Error_nullPointer(4, "GenericList_createRepeated()::result is required"));

	if (result->ptr)
		retError(clean, Error_invalidOperation(
			0, "GenericList_createRepeated()::result wasn't empty, might indicate memleak"
		));

	if(!length || !stride)
		retError(clean, Error_invalidParameter(
			!length ? 0 : 1, 0, "GenericList_createRepeated()::length and stride are required"
		));

	if(length * stride / stride != length)
		retError(clean, Error_overflow(0, length * stride, U64_MAX, "GenericList_createRepeated() overflow"));

	gotoIfError3(clean, Buffer_createUninitializedBytes(length * stride, allocator, &buf, e_rr));
	alloc = true;

	*result = (GenericList) {
		.ptr = buf.ptr,
		.length = length,
		.stride = stride,
		.capacityAndRefInfo = length
	};

	for (U64 i = 0; i < length; ++i)
		gotoIfError3(clean, GenericList_set(*result, i, data, e_rr));

clean:

	if (!s_uccess && alloc) {
		Buffer_free(&buf, allocator);
		*result = (GenericList){ 0 };
	}

	return s_uccess;
}

Bool GenericList_createCopy(const GenericList list, const Allocator *allocator, GenericList *result, Error *e_rr) {

	Bool s_uccess = true;

	if(!result)
		retError(clean, Error_nullPointer(2, "GenericList_createCopy()::list and result are required"));

	if (result->ptr)
		retError(clean, Error_invalidOperation(0, "GenericList_createCopy()::result wasn't empty, might indicate memleak"));

	if(GenericList_empty(list)) {
		*result = GenericList_createEmpty(list.stride);
		goto clean;
	}

	gotoIfError3(clean, GenericList_create(list.length, list.stride, allocator, result, e_rr));
	Buffer_memcpy(GenericList_buffer(*result), GenericList_bufferConst(list));

clean:
	return s_uccess;
}

Bool GenericList_createSubset(GenericList list, U64 index, U64 length, GenericList *result, Error *e_rr) {

	Bool s_uccess = true;

	if(!result || !length)
		retError(clean, Error_nullPointer(!length ? 3 : 4, "GenericList_createSubset()::result and length are required"));

	if (result->ptr)
		retError(clean, Error_invalidOperation(0, "GenericList_createSubset()::result wasn't empty, might indicate memleak"));

	if(index + length < index)
		retError(clean, Error_overflow(1, index + length, U64_MAX, "GenericList_createSubset() overflow"));

	if(index + length > list.length)
		retError(clean, Error_outOfBounds(
			1, index + length, list.length, "GenericList_createSubset()::index + length out of bounds"
		));

	U64 stride = list.stride;

	if (GenericList_isConstRef(list)) {
		gotoIfError3(clean, GenericList_createRefConst((const U8*)list.ptr + index * stride, length, stride, result, e_rr));
	}

	else gotoIfError3(clean, GenericList_createRef((U8*)list.ptrNonConst + index * stride, length, stride, result, e_rr));

clean:
	return s_uccess;
}

Bool GenericList_createSubsetReverse(
	const GenericList list,
	U64 index,
	U64 length,
	const Allocator *allocator,
	GenericList *result,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool alloc = false;

	if(!result || !length)
		retError(clean, Error_nullPointer(
			!length ? 2 : 4, "GenericList_createSubsetReverse()::result and length are required"
		));

	if(result->ptr)
		retError(clean, Error_invalidOperation(
			0, "GenericList_createSubsetReverse()::result wasn't empty, might indicate memleak"
		));

	if(index + length < length)
		retError(clean, Error_overflow(1, index + length, U64_MAX, "GenericList_createSubsetReverse() overflow"));

	if(index + length > list.length)
		retError(clean, Error_outOfBounds(
			1, index + length, list.length, "GenericList_createSubsetReverse()::index + length out of bounds"
		));

	gotoIfError3(clean, GenericList_create(length, list.stride, allocator, result, e_rr));
	alloc = true;

	for(U64 last = index + length - 1, i = 0; last != index - 1; --last, ++i) {
		Buffer buf = Buffer_createNull();
		gotoIfError3(clean, GenericList_get(list, last, &buf, e_rr));
		gotoIfError3(clean, GenericList_set(*result, i, &buf, e_rr));
	}

clean:

	if(!s_uccess && alloc)
		GenericList_free(result, allocator);

	return s_uccess;
}

Bool GenericList_createRef(void *ptr, U64 length, U64 stride, GenericList *result, Error *e_rr) {

	Bool s_uccess = true;

	if(!ptr || !result)
		retError(clean, Error_nullPointer(!ptr ? 0 : 3, "GenericList_createRef()::ptr and result are required"));

	if (result->ptr)
		retError(clean, Error_invalidOperation(0, "GenericList_createRef()::result wasn't empty, might indicate memleak"));

	if(!length || !stride)
		retError(clean, Error_invalidParameter(!length ? 1 : 2, 0, "GenericList_createRef()::length and stride are required"));

	if(length * stride / stride != length)
		retError(clean, Error_overflow(1, length * stride, U64_MAX, "GenericList_createRef() overflow"));

	*result = (GenericList) {
		.ptrNonConst = ptr,
		.length = length,
		.stride = stride
	};

clean:
	return s_uccess;
}

Bool GenericList_createRefConst(const void *ptr, U64 length, U64 stride, GenericList *result, Error *e_rr) {

	Bool s_uccess = true;

	if(!ptr || !result)
		retError(clean, Error_nullPointer(!ptr ? 0 : 3, "GenericList_createConstRef()::ptr and result are required"));

	if (result->ptr)
		retError(clean, Error_invalidOperation(
			0, "GenericList_createConstRef()::result wasn't empty, might indicate memleak"
		));

	if(!length || !stride)
		retError(clean, Error_invalidParameter(
			!length ? 1 : 2, 0, "GenericList_createConstRef()::length and stride are required"
		));

	if(length * stride / stride != length)
		retError(clean, Error_overflow(1, length * stride, U64_MAX, "GenericList_createConstRef() overflow"));

	*result = (GenericList) {
		.ptr = ptr,
		.length = length,
		.stride = stride,
		.capacityAndRefInfo = U64_MAX
	};

clean:
	return s_uccess;
}

Bool GenericList_set(GenericList list, U64 index, const Buffer *buf, Error *e_rr) {

	Bool s_uccess = true;

	if(GenericList_isConstRef(list))
		retError(clean, Error_constData(0, 0, "GenericList_set()::list is const"));

	if(index >= list.length)
		retError(clean, Error_outOfBounds(1, index, list.length, "GenericList_set()::index is out of bounds"));

	U64 stride = list.stride;

	const U64 bufLen = buf ? Buffer_length(*buf) : 0;

	if(bufLen && bufLen != stride)
		retError(clean, Error_invalidOperation(0, "GenericList_set()::buf.length incompatible with list"));

	if(bufLen)
		Buffer_memcpy(Buffer_createRef((U8*)list.ptrNonConst + index * stride, stride), *buf);

	else Buffer_unsetAllBits(Buffer_createRef((U8*)list.ptrNonConst + index * stride, stride), e_rr);

clean:
	return s_uccess;
}

Bool GenericList_get(const GenericList list, U64 index, Buffer *result, Error *e_rr) {

	Bool s_uccess = true;

	if(!result)
		retError(clean, Error_nullPointer(2, "GenericList_get()::list and result are required"));

	U64 stride = list.stride;

	if(index >= list.length)
		retError(clean, Error_outOfBounds(1, index, list.length, "GenericList_get()::index out of bounds"));

	if (GenericList_isConstRef(list)) {
		*result = Buffer_createRefConst((const U8*)list.ptr + index * stride, stride);
		goto clean;
	}

	*result = Buffer_createRef((U8*)list.ptrNonConst + index * stride, stride);

clean:
	return s_uccess;
}

Bool GenericList_find(
	const GenericList list,
	const Buffer *buf,
	EqualsFunction eq,
	const Allocator *allocator,
	ListU64 *result,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool alloc = false;

	if (!result)
		retError(clean, Error_nullPointer(3, "GenericList_find()::list and result are required"));

	if(!buf || Buffer_length(*buf) != list.stride)
		retError(clean, Error_invalidParameter(1, 0, "GenericList_find()::buf.length incompatible with list"));

	if(result->ptr)
		retError(clean, Error_invalidParameter(3, 0, "GenericList_find()::result isn't empty, might indicate memleak"));

	const U64 length = list.length;

	gotoIfError3(clean, ListU64_reserve(result, length / 100 + 16, allocator, e_rr));
	alloc = true;

	for(U64 i = 0; i < length; ++i) {

		Bool b = !eq ? Buffer_eq(GenericList_atConst(list, i), *buf) : eq(GenericList_ptrConst(list, i), buf->ptr);

		if (b)
			gotoIfError3(clean, ListU64_pushBack(result, i, allocator, e_rr));
	}

clean:

	if(!s_uccess && alloc)
		ListU64_free(result, allocator);

	return s_uccess;
}

Bool GenericList_copy(
	const GenericList src,
	U64 srcOffset,
	GenericList dst,
	U64 dstOffset,
	U64 count,
	Error *e_rr
) {

	Bool s_uccess = true;

	if (!src.length && !dst.length)
		goto clean;

	GenericList srcOffsetted = (GenericList) { 0 };
	gotoIfError3(clean, GenericList_createSubset(src, srcOffset, count, &srcOffsetted, e_rr));

	if(GenericList_isConstRef(dst))
		retError(clean, Error_constData(2, 0, "GenericList_copy()::dst should be writable"));

	GenericList dstOffsetted = (GenericList) { 0 };
	gotoIfError3(clean, GenericList_createSubset(dst, dstOffset, count, &dstOffsetted, e_rr));

	Buffer_memcpy(GenericList_buffer(dstOffsetted), GenericList_bufferConst(srcOffsetted));

clean:
	return s_uccess;
}

Bool GenericList_swap(GenericList l, U64 i, U64 j, Error *e_rr) {

	Bool s_uccess = true;

	const U64 stride = l.stride;

	if(stride > 1024 || !stride)        //Current limitation, because we don't allocate.
		retError(clean, Error_invalidParameter(0, 0, "GenericList_swap()::l must be a stride of max 1024 (and >0)"));

	const U64 len = l.length;

	if(i >= len || j >= len)
		retError(clean, Error_outOfBounds(
			i >= len ? 1 : 2, i >= len ? i : j, len,
			"GenericList_swap()::i or j is out of bounds"
		));

	if(GenericList_isConstRef(l))
		retError(clean, Error_constData(0, 0, "GenericList_swap()::list is const"));

	U8 tmp[1024];
	Buffer tmpBuf = Buffer_createRef(tmp, stride);

	U8 *iptr = GenericList_ptr(l, i);
	Buffer iptrBuf = Buffer_createRef(iptr, stride);

	Buffer_memcpy(tmpBuf, iptrBuf);

	U8 *jptr = GenericList_ptr(l, j);
	Buffer jptrBuf = Buffer_createRef(jptr, stride);

	Buffer_memcpy(iptrBuf, jptrBuf);
	Buffer_memcpy(jptrBuf, tmpBuf);

clean:
	return s_uccess;
}

Bool GenericList_eraseAllIndices(GenericList *list, const ListU64 *indices, Error *e_rr) {

	Bool s_uccess = true;

	if(!list)
		retError(clean, Error_nullPointer(0, "GenericList_eraseAllIndices()::list is required"));

	if(GenericList_isRef(*list))
		retError(clean, Error_constData(0, 0, "GenericList_eraseAllIndices()::list is only allowed on managed memory"));

	if (!indices || !indices->length || !list->length)
		goto clean;

	GenericList indicesAsGenericList = ListU64_toList(*indices);

	if(!GenericList_sortU64(indicesAsGenericList))
		retError(clean, Error_invalidParameter(1, 0, "GenericList_eraseAllIndices()::indices sort failed"));

	//Ensure none of them reference out of bounds or are duplicate

	U64 last = U64_MAX;
	const U64 *indicesPtr = indices->ptr;
	const U64 *indicesEnd = ListU64_endConst(*indices);

	for(const U64 *ptr = indicesPtr, *end = indicesEnd; ptr < end; ++ptr) {

		if(last == *ptr)
			retError(clean, Error_alreadyDefined(0, "GenericList_eraseAllIndices()::indices had a duplicate"));

		if(*ptr >= list->length)
			retError(clean, Error_outOfBounds(
				0, ptr - indicesPtr, list->length, "GenericList_eraseAllIndices()::indices[i] out of bounds"
			));

		last = *ptr;
	}

	//Since we're sorted from small to big,
	//we can just easily fetch where our next block of memory should go and move it backwards

	U64 curr = 0;
	const U64 stride = list->stride;

	for (const U64 *ptr = indicesPtr, *end = indicesEnd; ptr < end; ++ptr) {

		if(!curr)
			curr = *ptr;

		const U64 me = *ptr + 1;
		const U64 neighbor = ptr + 1 != end ? *(ptr + 1) : list->length;

		if(me == neighbor)
			continue;

		Buffer_memmove(
			Buffer_createRef((U8*)list->ptrNonConst + curr * stride, (neighbor - me) * stride),
			Buffer_createRef((U8*)list->ptrNonConst + me * stride, (neighbor - me) * stride)
		);

		curr += neighbor - me;
	}

	list->length = curr;

clean:
	return s_uccess;
}

Bool GenericList_erase(GenericList *list, U64 index, Error *e_rr) {

	Bool s_uccess = true;

	if(!list)
		retError(clean, Error_nullPointer(0, "GenericList_erase()::list is required"));

	if(GenericList_isRef(*list))
		retError(clean, Error_invalidParameter(0, 0, "GenericList_erase()::list needs to be managed memory"));

	if(index >= list->length)
		retError(clean, Error_outOfBounds(1, index, list->length, "GenericList_erase()::index out of bounds"));

	if(index + 1 != list->length)
		Buffer_memmove(
			Buffer_createRef((U8*)list->ptrNonConst + index * list->stride, (list->length - 1 - index) * list->stride),
			Buffer_createRef((U8*)list->ptrNonConst + (index + 1) * list->stride, (list->length - 1 - index) * list->stride)
		);

	--list->length;

clean:
	return s_uccess;
}

Bool GenericList_resizeInternal(GenericList *list, U64 size, const Allocator *allocator, Bool doClear, Error *e_rr);

Bool GenericList_insert(GenericList *list, U64 index, const Buffer *buf, const Allocator *allocator, Error *e_rr) {

	Bool s_uccess = true;

	if(!list)
		retError(clean, Error_nullPointer(0, "GenericList_insert()::list out of bounds"));

	if(GenericList_isRef(*list) && list->length)
		retError(clean, Error_invalidParameter(0, 0, "GenericList_insert()::list must be managed memory"));

	if(!buf || list->stride != Buffer_length(*buf))
		retError(clean, Error_invalidOperation(0, "GenericList_insert()::stride and buf.length are incompatible"));

	if(!(list->length + 1))
		retError(clean, Error_overflow(0, 0, U64_MAX, "GenericList_insert() overflow"));

	if (index == list->length) {        //Push back

		gotoIfError3(clean, GenericList_resizeInternal(list, list->length + 1, allocator, false, e_rr));

		Buffer_memcpy(
			Buffer_createRef((U8*)list->ptrNonConst + index * list->stride, list->stride),
			*buf
		);

		goto clean;
	}

	if(index >= list->length)
		retError(clean, Error_outOfBounds(1, index, list->length, "GenericList_insert()::index out of bounds"));

	const U64 prevSize = list->length;
	gotoIfError3(clean, GenericList_resizeInternal(list, list->length + 1, allocator, false, e_rr));

	//Copy our own data to the right

	if(prevSize)
		Buffer_memmove(
			Buffer_createRef((U8*)list->ptrNonConst + (index + 1) * list->stride, (prevSize - index) * list->stride),
			Buffer_createRef((U8*)list->ptrNonConst + index * list->stride, (prevSize - index) * list->stride)
		);

	//Copy the other data

	Buffer_memcpy(
		Buffer_createRef((U8*)list->ptrNonConst + index * list->stride, list->stride),
		*buf
	);

clean:
	return s_uccess;
}

Bool GenericList_pushAll(GenericList *list, const GenericList other, const Allocator *allocator, Error *e_rr) {

	Bool s_uccess = true;

	if(!list)
		retError(clean, Error_nullPointer(0, "GenericList_pushAll()::list is required"));

	if(GenericList_isRef(*list) && list->length)
		retError(clean, Error_invalidParameter(0, 0, "GenericList_pushAll()::list must be managed memory"));

	if (!other.length)
		goto clean;

	if(list->stride != other.stride)
		retError(clean, Error_invalidOperation(0, "GenericList_pushAll()::list.stride and other.stride are incompatible"));

	if(list->length + other.length < list->length)
		retError(clean, Error_overflow(0, list->length + other.length, U64_MAX, "GenericList_pushAll() overflow"));

	const U64 oldSize = GenericList_bytes(*list);
	gotoIfError3(clean, GenericList_resizeInternal(list, list->length + other.length, allocator, false, e_rr));

	Buffer_memcpy(
		Buffer_createRef((U8*)list->ptrNonConst + oldSize, GenericList_bytes(other)),
		Buffer_createRefConst(other.ptr, GenericList_bytes(other))
	);

clean:
	return s_uccess;
}

Bool GenericList_insertAll(GenericList *list, const GenericList other, U64 offset, const Allocator *allocator, Error *e_rr) {

	Bool s_uccess = true;

	if(!list)
		retError(clean, Error_nullPointer(0, "GenericList_insertAll()::list is required"));

	if(list->length && GenericList_isRef(*list))
		retError(clean, Error_invalidParameter(0, 0, "GenericList_insertAll()::list must be managed memory"));

	if (!other.length)
		goto clean;

	if(list->stride != other.stride)
		retError(clean, Error_invalidOperation(0, "GenericList_insertAll()::list->stride and other.stride are incompatible"));

	if(list->length + other.length < list->length)
		retError(clean, Error_overflow(0, list->length + other.length, U64_MAX, "GenericList_insertAll() overflow"));

	if(offset > list->length)
		retError(clean, Error_outOfBounds(2, offset, list->length, "GenericList_insertAll()::offset out of bounds"));

	const U64 prevSize = list->length;
	gotoIfError3(clean, GenericList_resizeInternal(list, list->length + other.length, allocator, false, e_rr));

	//Copy our own data to the right

	if(prevSize)
		Buffer_memmove(
			Buffer_createRef(
				(U8*)list->ptrNonConst + (offset + other.length) * list->stride,
				(prevSize - offset) * list->stride
			),
			Buffer_createRef((U8*)list->ptrNonConst + offset * list->stride, (prevSize - offset) * list->stride)
		);

	//Copy the other data

	Buffer_memcpy(
		Buffer_createRef((U8*)list->ptrNonConst + offset * list->stride, other.length * list->stride),
		GenericList_bufferConst(other)
	);

clean:
	return s_uccess;
}

Bool GenericList_resizeInternal(GenericList *list, U64 size, const Allocator *allocator, Bool doClear, Error *e_rr) {

	Bool s_uccess = true;

	if(!list)
		retError(clean, Error_nullPointer(0, "GenericList_resizeInternal()::list is required"));

	if(GenericList_isRef(*list) && list->length)
		retError(clean, Error_constData(0, 0, "GenericList_resizeInternal()::list has to be managed memory"));

	if (list->length == size)
		goto clean;

	if (size <= list->capacityAndRefInfo) {

		if(doClear && size > list->length)
			gotoIfError3(clean, Buffer_unsetAllBits(
				Buffer_createRef((U8*)list->ptrNonConst + list->stride * list->length, (size - list->length) * list->stride),
				e_rr
			));

		list->length = size;
		goto clean;
	}

	if(size * 3 / 3 != size)
		retError(clean, Error_overflow(1, size * 3, U64_MAX, "GenericList_resizeInternal() overflow"));

	gotoIfError3(clean, GenericList_reserve(list, size * 3 / 2, allocator, e_rr));

	if(doClear)
		gotoIfError3(clean, Buffer_unsetAllBits(
			Buffer_createRef((U8*)list->ptrNonConst + list->stride * list->length, (size * 3 / 2 - list->length) * list->stride),
			e_rr
		));

	list->length = size;

clean:
	return s_uccess;
}

Bool GenericList_reserve(GenericList *list, U64 capacity, const Allocator *allocator, Error *e_rr) {

	Bool s_uccess = true;

	if(!list)
		retError(clean, Error_nullPointer(0, "GenericList_reserve()::list is required"));

	if(GenericList_isRef(*list) && list->length)
		retError(clean, Error_invalidOperation(0, "GenericList_reserve()::list must be managed memory"));

	if(!list->stride)
		retError(clean, Error_invalidOperation(0, "GenericList_reserve()::stride is required"));

	if(capacity * list->stride / list->stride != capacity)
		retError(clean, Error_overflow(1, capacity * list->stride, U64_MAX, "GenericList_reserve() overflow"));

	if (capacity <= GenericList_capacity(*list))
		goto clean;

	Buffer buffer = Buffer_createNull();
	gotoIfError3(clean, Buffer_createUninitializedBytes(capacity * list->stride, allocator, &buffer, e_rr));

	Buffer_memcpy(buffer, GenericList_bufferConst(*list));

	Buffer curr = Buffer_createManagedPtr((U8*)list->ptrNonConst, GenericList_allocatedBytes(*list));

	if(Buffer_length(curr))
		Buffer_free(&curr, allocator);

	list->ptr = buffer.ptr;
	list->capacityAndRefInfo = capacity;

clean:
	return s_uccess;
}

Bool GenericList_resize(GenericList *list, U64 size, const Allocator *allocator, Error *e_rr) {
	return GenericList_resizeInternal(list, size, allocator, true, e_rr);
}

Bool GenericList_shrinkToFit(GenericList *list, const Allocator *allocator, Error *e_rr) {

	Bool s_uccess = true;

	if(!list || !list->ptr)
		retError(clean, Error_nullPointer(0, "GenericList_shrinkToFit()::list is required"));

	if(GenericList_isRef(*list))
		retError(clean, Error_constData(0, 0, "GenericList_shrinkToFit() is only allowed on managed memory"));

	if (list->capacityAndRefInfo == list->length)
		goto clean;

	GenericList copy = GenericList_createEmpty(list->stride);
	gotoIfError3(clean, GenericList_createCopy(*list, allocator, &copy, e_rr));

	GenericList_free(list, allocator);
	*list = copy;

clean:
	return s_uccess;
}

//Easy insertion sort
//Uses about 8KB of cache on stack to hopefully sort quite quickly.
//Does mean that this can only be used for lists < 8KB.

static inline Bool GenericList_insertionSort8K(GenericList list, CompareFunction func) {

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

			if (func(t, tsorted + k * list.stride) != ECompareResult_Lt)        //!isLess
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

#define TGenericList_tsort(T)                                                                        \
ECompareResult sort##T(const T *a, const T *b) {                                                    \
	return *a < *b ? ECompareResult_Lt : (*a > *b ? ECompareResult_Gt : ECompareResult_Eq);            \
}

#define TGenericList_sorts(f) f(U64); f(I64); f(U32); f(I32); f(U16); f(I16); f(U8); f(I8); f(F32); f(F64);

TGenericList_sorts(TGenericList_tsort);

//https://stackoverflow.com/questions/33884057/quick-sort-stackoverflow-error-for-large-arrays
//qsort U64 should be taking about ~76ms / 1M elem and ~13ms / 1M (sorted).
//qsort U8 should be taking about half the time for unsorted because of cache speedups.
//Expect F32 sorting to be at least 2x to 5x slower (and F64 even slower).
//(Profiled on a 3900x)

static inline U64 GenericList_qpartition(GenericList list, U64 begin, U64 last, CompareFunction f) {

	U8 tmp[1024 * 2];        //We only support max of 1024 stride lists. We don't want to allocate

	Buffer_memcpy(
		Buffer_createRef(tmp, list.stride),
		Buffer_createRefConst((const U8*)list.ptr + (begin + last) / 2 * list.stride, list.stride)
	);

	U64 i = begin - 1, j = last + 1;

	while (true) {

		while(++i < last && f((const U8*)list.ptr + i * list.stride, tmp) == ECompareResult_Lt);
		while(--j > begin && f((const U8*)list.ptr + j * list.stride, tmp) == ECompareResult_Gt);

		if(i < j) {

			Buffer_memcpy(
				Buffer_createRef(tmp + list.stride, list.stride),
				Buffer_createRefConst((const U8*)list.ptr + i * list.stride, list.stride)
			);

			Buffer_memcpy(
				Buffer_createRef((U8*)list.ptr + i * list.stride, list.stride),
				Buffer_createRefConst((const U8*)list.ptr + j * list.stride, list.stride)
			);

			Buffer_memcpy(
				Buffer_createRef((U8*)list.ptrNonConst + j * list.stride, list.stride),
				Buffer_createRefConst(tmp + list.stride, list.stride)
			);
		}

		else return j;
	}
}

static Bool GenericList_qsortRecurse(GenericList list, U64 begin, U64 end, CompareFunction f) {

	if(begin >> 63 || end >> 63)
		return false;

	while(begin < end && end != U64_MAX) {

		const U64 pivot = GenericList_qpartition(list, begin, end, f);

		if (pivot == U64_MAX)        //Does return a modified array, but it's not fully sorted
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

static inline Bool GenericList_qsort(GenericList list, CompareFunction f) {
	return GenericList_qsortRecurse(list, 0, list.length - 1, f);
}

Bool GenericList_sortCustom(GenericList list, CompareFunction f) {

	if(list.length <= 1)
		return true;

	if(list.stride > 1024)            //Current limitation, because we don't allocate.
		return false;

	if(GenericList_isConstRef(list))
		return false;

	if(GenericList_bytes(list) <= 8192)
		return GenericList_insertionSort8K(list, f);

	return GenericList_qsort(list, f);
}

#define TGenericList_sort(T) Bool GenericList_sort##T(GenericList l) {    \
	return GenericList_sortCustom(l, (CompareFunction) sort##T);         \
}

TGenericList_sorts(TGenericList_sort);

static inline ECompareResult GenericList_compareString(const CharString *a, const CharString *b) {
	return CharString_compareSensitive(a, b);
}

static inline ECompareResult GenericList_compareStringInsensitive(const CharString *a, const CharString *b) {
	return CharString_compareInsensitive(a, b);
}

Bool GenericList_sortString(GenericList list, EStringCase stringCase) {

	if(list.stride != sizeof(CharString))        //We don't know the real type, but at least it's a check
		return false;

	return
		stringCase == EStringCase_Insensitive ?
		GenericList_sortCustom(list, (CompareFunction) GenericList_compareStringInsensitive) :
		GenericList_sortCustom(list, (CompareFunction) GenericList_compareString);
}

Bool GenericList_pushBack(GenericList *list, const Buffer *buf, const Allocator *allocator, Error *e_rr) {

	Bool s_uccess = true;

	if(!list)
		retError(clean, Error_nullPointer(0, "GenericList_pushBack()::list is required"));

	if(GenericList_isRef(*list) && list->ptr)
		retError(clean, Error_constData(0, 0, "GenericList_pushBack()::list needs to be managed memory"));

	gotoIfError3(clean, GenericList_resizeInternal(list, list->length + 1, allocator, false, e_rr));
	gotoIfError3(clean, GenericList_set(*list, list->length - 1, buf, e_rr));

clean:
	return s_uccess;
}

Bool GenericList_popLocation(GenericList *list, U64 index, Buffer *buf, Error *e_rr) {

	Bool s_uccess = true;

	if(!list)
		retError(clean, Error_nullPointer(0, "GenericList_popLocation()::list is required"));

	if(index >= list->length)
		retError(clean, Error_outOfBounds(1, index, list->length, "GenericList_popLocation()::index out of bounds"));

	if(GenericList_isRef(*list))
		retError(clean, Error_constData(0, 0, "GenericList_popLocation()::list needs to be managed memory"));

	Buffer result = Buffer_createNull();
	gotoIfError3(clean, GenericList_get(*list, index, &result, e_rr));

	if(buf) {

		if(Buffer_length(*buf) != Buffer_length(result))
			retError(clean, Error_invalidOperation(
				0, "GenericList_popLocation()::buf.length and list->stride are incompatible"
			));

		Buffer_memcpy(*buf, result);
	}

	gotoIfError3(clean, GenericList_erase(list, index, e_rr));

clean:
	return s_uccess;
}

Bool GenericList_popBack(GenericList *list, Buffer *output, Error *e_rr) {

	Bool s_uccess = true;

	if(!list)
		retError(clean, Error_nullPointer(0, "GenericList_popBack()::list is required"));

	if(!list->length)
		retError(clean, Error_outOfBounds(0, 0, 0, "GenericList_popBack()::list.length needs to be at least 1"));

	if(output && Buffer_length(*output) != list->stride)
		retError(clean, Error_invalidOperation(0, "GenericList_popBack()::output.length must be equal to list->stride"));

	if(GenericList_isRef(*list))
		retError(clean, Error_constData(0, 0, "GenericList_popBack()::list must be managed memory"));

	if(output)
		Buffer_memcpy(*output, Buffer_createRefConst((const U8*)list->ptr + (list->length - 1) * list->stride, list->stride));

	--list->length;

clean:
	return s_uccess;
}

void GenericList_free(GenericList *result, const Allocator *allocator) {

	if(!result)
		return;

	if (!GenericList_isRef(*result) && result->capacityAndRefInfo) {
		Buffer buf = Buffer_createManagedPtr(result->ptrNonConst, GenericList_allocatedBytes(*result));
		Buffer_free(&buf, allocator);
	}

	*result = GenericList_createEmpty(result->stride);
}
