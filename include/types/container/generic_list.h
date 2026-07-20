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

//types/container/generic_list.h

#pragma once
#include "types/container/generic_list_predeclare.h"
#include "types/base/buffer_base.h"
#include "types/base/c8.h"
#include "types/base/algorithm.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct Error Error;

void ListU64_free(ListU64 *indices, const Allocator *allocator);

//A list for POD types (non pod types can be used by oxc::List)
//Allows both const and non const, only access ptrNonConst if const is checked before
//GenericList is used by TList(T) to handle most logic

typedef struct GenericList {

	union {
		const void *ptr;
		void *ptrNonConst;        //Check if !const first
	};

	U64 stride;
	U64 length;
	U64 capacityAndRefInfo;        //If capacityAndRefInfo is 0 or U64_MAX, it indicates a ref

} GenericList;

static inline Bool GenericList_isConstRef(const GenericList l) { return l.capacityAndRefInfo == U64_MAX; }
static inline Bool GenericList_isRef(const GenericList l) { return !l.capacityAndRefInfo || GenericList_isConstRef(l); }

static inline Bool GenericList_empty(const GenericList l) { return !l.length; }
static inline Bool GenericList_any(const GenericList l) { return l.length; }
static inline U64  GenericList_bytes(const GenericList l) { return l.length * l.stride; }
static inline U64  GenericList_allocatedBytes(const GenericList l) {
	return GenericList_isRef(l) ? 0 : l.capacityAndRefInfo * l.stride;
}

static inline U64 GenericList_capacity(const GenericList l) { return GenericList_isRef(l) ? 0 : l.capacityAndRefInfo; }

static inline Buffer GenericList_buffer(const GenericList l) {
	return GenericList_isConstRef(l) ? Buffer_createNull() : Buffer_createRef(l.ptrNonConst, GenericList_bytes(l));
}

static inline Buffer GenericList_allocatedBuffer(const GenericList l) {
	return GenericList_isRef(l) ? Buffer_createNull() : Buffer_createRef(l.ptrNonConst, GenericList_allocatedBytes(l));
}

static inline Buffer GenericList_bufferConst(const GenericList l) {
	return Buffer_createRefConst(l.ptr, GenericList_bytes(l));
}

static inline Buffer GenericList_allocatedBufferConst(const GenericList l) {
	return Buffer_createRefConst(GenericList_isRef(l) ? NULL : l.ptr, GenericList_allocatedBytes(l));
}

static inline void *GenericList_begin(const GenericList list) {
	return GenericList_isConstRef(list) ? NULL : list.ptrNonConst;
}

static inline void *GenericList_end(const GenericList list) {
	return GenericList_isConstRef(list) ? NULL : (U8*)list.ptrNonConst + GenericList_bytes(list);
}

static inline void *GenericList_last(const GenericList list) {
	return GenericList_isConstRef(list) || !list.length ? NULL :
		(U8*)list.ptrNonConst + GenericList_bytes(list) - list.stride;
}

static inline const void *GenericList_beginConst(const GenericList list) { return list.ptr; }

static inline const void *GenericList_endConst(const GenericList list) {
	return (const U8*)list.ptr + GenericList_bytes(list);
}

static inline const void *GenericList_lastConst(const GenericList list) {
	return !list.length ? NULL : (const U8*)list.ptr + GenericList_bytes(list) - list.stride;
}

static inline const void *GenericList_ptrConst(const GenericList list, U64 elementOffset) {
	return elementOffset < list.length ? (const U8*)list.ptr + elementOffset * list.stride : NULL;
}

static inline void *GenericList_ptr(const GenericList list, U64 elementOffset) {
	return !GenericList_isConstRef(list) && elementOffset < list.length ?
		(U8*)list.ptrNonConst + elementOffset * list.stride : NULL;
}

static inline Buffer GenericList_atConst(const GenericList list, U64 offset) {
	return offset < list.length ?
		Buffer_createRefConst((const U8*)list.ptr + offset * list.stride, list.stride) : Buffer_createNull();
}

static inline Buffer GenericList_at(const GenericList list, U64 offset) {
	return offset < list.length && !GenericList_isConstRef(list) ?
		Buffer_createRef((U8*)list.ptrNonConst + offset * list.stride, list.stride) : Buffer_createNull();
}

static inline Bool GenericList_eq(const GenericList *a, const GenericList *b) {
	return a && b && Buffer_eq(GenericList_bufferConst(*a), GenericList_bufferConst(*b));
}

static inline Bool GenericList_neq(const GenericList *a, const GenericList *b) {
	return a && b && Buffer_neq(GenericList_bufferConst(*a), GenericList_bufferConst(*b));
}

static inline GenericList GenericList_createEmpty(U64 stride) {
	GenericList empty = { 0 };
	empty.stride = stride;
	return empty;
}

Bool GenericList_create(U64 length, U64 stride, const Allocator *allocator, GenericList *result, Error *e_rr);

Bool GenericList_createRepeated(
	U64 length,
	U64 stride,
	const Buffer *data,
	const Allocator *allocator,
	GenericList *result,
	Error *e_rr
);

Bool GenericList_createCopy(const GenericList list, const Allocator *allocator, GenericList *result, Error *e_rr);
Bool GenericList_createSubset(const GenericList list, U64 index, U64 length, GenericList *result, Error *e_rr);

static inline Bool GenericList_createCopySubset(
	const GenericList list,
	U64 off,
	U64 len,
	const Allocator *allocator,
	GenericList *result,
	Error *e_rr
) {
	Bool s_uccess = true;
	GenericList tmp = { 0 };
	gotoIfError3(clean, GenericList_createSubset(list, off, len, &tmp, e_rr));
	gotoIfError3(clean, GenericList_createCopy(tmp, allocator, result, e_rr));

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
);

static inline Bool GenericList_createReverse(
	const GenericList list,
	const Allocator *allocator,
	GenericList *result,
	Error *e_rr
) {
	return GenericList_createSubsetReverse(list, 0, list.length, allocator, result, e_rr);
}

//Ensure that ptr is properly aligned
Bool GenericList_createRef(void *ptr, U64 length, U64 stride, GenericList *result, Error *e_rr);

//Ensure that ptr is properly aligned
Bool GenericList_createRefConst(const void *ptr, U64 length, U64 stride, GenericList *result, Error *e_rr);

static inline GenericList GenericList_createRefFromList(GenericList list) {
	list.capacityAndRefInfo = U64_MAX;
	return list;
}

//GenericList_set with NULL as buf will clear contents of that index
Bool GenericList_set(GenericList list, U64 index, const Buffer *buf, Error *e_rr);
Bool GenericList_get(const GenericList list, U64 index, Buffer *result, Error *e_rr);

Bool GenericList_find(
	const GenericList list,
	const Buffer *buf,
	EqualsFunction eq,
	const Allocator *allocator,
	ListU64 *result,
	Error *e_rr
);

Bool GenericList_copy(
	const GenericList src,
	U64 srcOffset,
	GenericList dst,
	U64 dstOffset,
	U64 count,
	Error *e_rr
);

//Limitation of a max stride of 1024, like GenericList_sortCustom, used to put a temp on the stack.
Bool GenericList_swap(GenericList list, U64 i, U64 j, Error *e_rr);

static inline Bool GenericList_reverse(GenericList l) {

	if (GenericList_isConstRef(l))
		return false;

	const U64 len = l.length;

	if (len <= 1)
		return true;

	if (len == 2) {
		GenericList_swap(l, 0, 1, NULL);
		return true;
	}

	for (U64 i = 0; i < len / 2; ++i) {
		U64 j = len - 1 - i;
		GenericList_swap(l, i, j, NULL);
	}

	return true;
}

Bool GenericList_eraseAllIndices(GenericList *list, const ListU64 *indices, Error *e_rr);

static inline U64 GenericList_findFirst(const GenericList list, const Buffer *buf, U64 index, EqualsFunction eq) {

	if (!buf || Buffer_length(*buf) != list.stride)
		return U64_MAX;

	for (U64 i = index; i < list.length; ++i)
		if (!eq ? Buffer_eq(GenericList_atConst(list, i), *buf) : eq(GenericList_ptrConst(list, i), buf->ptr))
			return i;

	return U64_MAX;
}

static inline U64 GenericList_findLast(const GenericList list, const Buffer *buf, U64 index, EqualsFunction eq) {

	if (!buf || Buffer_length(*buf) != list.stride)
		return U64_MAX;

	for (U64 i = list.length - 1; i != U64_MAX && i >= index; --i)
		if (!eq ? Buffer_eq(GenericList_atConst(list, i), *buf) : eq(GenericList_ptrConst(list, i), buf->ptr))
			return i;

	return U64_MAX;
}

static inline U64 GenericList_count(const GenericList list, const Buffer *buf, EqualsFunction eq) {

	if (!buf || Buffer_length(*buf) != list.stride)
		return U64_MAX;

	U64 count = 0;

	for (U64 i = 0; i < list.length; ++i)
		if (!eq ? Buffer_eq(GenericList_atConst(list, i), *buf) : eq(GenericList_ptrConst(list, i), buf->ptr))
			++count;

	return count;
}

static inline Bool GenericList_contains(const GenericList list, const Buffer *buf, U64 offset, EqualsFunction eq) {
	return GenericList_findFirst(list, buf, offset, eq) != U64_MAX;
}

Bool GenericList_erase(GenericList *list, U64 index, Error *e_rr);

static inline Bool GenericList_eraseFirstLast(
	GenericList *list,
	const Buffer *buf,
	U64 offset,
	EqualsFunction eq,
	Bool isFirst,
	Error *e_rr
) {

	Bool s_uccess = true;

	if (!list)
		retError(clean, Error_nullPointer(0, "GenericList_eraseFirstLast()::list is required"));

	//Scoped so the goto above doesn't cross initialization (required when consumed as C++)

	{
		const U64 ind = isFirst ? GenericList_findFirst(*list, buf, offset, eq) : GenericList_findLast(*list, buf, offset, eq);

		if (ind == U64_MAX)
			goto clean;

		gotoIfError3(clean, GenericList_erase(list, ind, e_rr));
	}

clean:
	return s_uccess;
}

static inline Bool GenericList_eraseLast(GenericList *list, const Buffer *buf, U64 offset, EqualsFunction eq, Error *e_rr) {
	return GenericList_eraseFirstLast(list, buf, offset, eq, false, e_rr);
}

static inline Bool GenericList_eraseFirst(GenericList *list, const Buffer *buf, U64 offset, EqualsFunction eq, Error *e_rr) {
	return GenericList_eraseFirstLast(list, buf, offset, eq, true, e_rr);
}

static inline Bool GenericList_eraseAll(
	GenericList *list,
	const Buffer *buf,
	const Allocator *allocator,
	EqualsFunction eq,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListU64 indices = { 0 };

	if (!list)
		retError(clean, Error_nullPointer(0, "GenericList_eraseAll()::list is required"));

	//TODO: Just loop and erase as they're encountered

	gotoIfError3(clean, GenericList_find(*list, buf, eq, allocator, &indices, e_rr));
	gotoIfError3(clean, GenericList_eraseAllIndices(list, &indices, e_rr));

clean:
	if(indices.ptr)
		ListU64_free(&indices, allocator);

	return s_uccess;
}

Bool GenericList_insert(GenericList *list, U64 index, const Buffer *buf, const Allocator *allocator, Error *e_rr);
Bool GenericList_pushAll(GenericList *list, const GenericList other, const Allocator *allocator, Error *e_rr);
Bool GenericList_insertAll(GenericList *list, const GenericList other, U64 offset, const Allocator *allocator, Error *e_rr);

Bool GenericList_reserve(GenericList *list, U64 capacity, const Allocator *allocator, Error *e_rr);
Bool GenericList_resize(GenericList *list, U64 size, const Allocator *allocator, Error *e_rr);

Bool GenericList_shrinkToFit(GenericList *list, const Allocator *allocator, Error *e_rr);

Bool GenericList_sortU64(GenericList list);
Bool GenericList_sortU32(GenericList list);
Bool GenericList_sortU16(GenericList list);
Bool GenericList_sortU8(GenericList list);

Bool GenericList_sortI64(GenericList list);
Bool GenericList_sortI32(GenericList list);
Bool GenericList_sortI16(GenericList list);
Bool GenericList_sortI8(GenericList list);

Bool GenericList_sortF32(GenericList list);
Bool GenericList_sortF64(GenericList list);

//Only allowed when the stride is <= 1024 (it needs to copy to a temp buffer)
Bool GenericList_sortCustom(GenericList list, CompareFunction func);

Bool GenericList_sortString(GenericList list, EStringCase stringCase);

static inline Bool GenericList_sortStringSensitive(GenericList list) {
	return GenericList_sortString(list, EStringCase_Sensitive);
}

static inline Bool GenericList_sortStringInsensitive(GenericList list) {
	return GenericList_sortString(list, EStringCase_Insensitive);
}

//Expects buf to be sized to stride (to allow copying to the stack)
Bool GenericList_popBack(GenericList *list, Buffer *output, Error *e_rr);

//Expects buf to be sized to stride (to allow copying to the stack)
Bool GenericList_popLocation(GenericList *list, U64 index, Buffer *buf, Error *e_rr);

Bool GenericList_pushBack(GenericList *list, const Buffer *buf, const Allocator *allocator, Error *e_rr);

static inline Bool GenericList_pushFront(GenericList *list, const Buffer *buf, const Allocator *allocator, Error *e_rr) {

	if (!list || !list->length)
		return GenericList_pushBack(list, buf, allocator, e_rr);

	return GenericList_insert(list, 0, buf, allocator, e_rr);
}

static inline Bool GenericList_popFront(GenericList *list, Buffer *output, Error *e_rr) {
	return GenericList_popLocation(list, 0, output, e_rr);
}

static inline Bool GenericList_clear(GenericList *list, Error *e_rr) {

	if (!list) {
		if(e_rr) *e_rr = Error_nullPointer(0, "GenericList_clear()::list is required");
		return false;
	}

	list->length = 0;
	return true;
}

void GenericList_free(GenericList *result, const Allocator *allocator);

//TList template helper

#define TListWrapModifying(Name, ...) {                                                                                    \
																														\
	Bool s_uccess = true;                                                                                                \
																														\
	if(!l)                                                                                                                \
		retError(clean, Error_nullPointer(0, #Name " TListWrapNullCheck::l is required"));                                \
																														\
	GenericList list = Name##_toList(*l);                                                                                \
	__VA_ARGS__;                                                                                                        \
																														\
	*l = (Name) { 0 };                                                                                                    \
	gotoIfError3(clean, Name##_fromList(list, l, e_rr));                                                                \
clean:                                                                                                                    \
	return s_uccess;                                                                                                    \
}

#ifdef __cplusplus
	}
#endif
