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

#pragma once
#include "types/container/list.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Helpers for creating the "template" functions of a TList

#ifndef TListXBaseImpl
	#define TListXBaseImpl(Name)
#endif

#define TListWrapCtor(Name, ...) {																						\
																														\
	GenericList list = (GenericList) { 0 }; 																			\
	__VA_ARGS__;																										\
																														\
	if (err.genericError || (err = ListVoid_fromList(list, sizeof(Name##_Type), (ListVoid*)result)).genericError)		\
		GenericList_free(&list, alloc);																					\
																														\
	return err;																											\
}

#define TListNamedBaseImpl(Name) 																						\
																														\
GenericList Name##_toList(Name t) {																						\
	return ListVoid_toList(																								\
		(ListVoid) { 																									\
			.ptr = (const void*) t.ptr,																					\
			.length = t.length,																							\
			.capacityAndRefInfo = t.capacityAndRefInfo																	\
		},																												\
		sizeof(Name##_Type)																								\
	);																													\
}																														\
																														\
Error Name##_fromList(GenericList list, Name *result) { 																\
	return ListVoid_fromList(list, sizeof(Name##_Type), (ListVoid*)result); 											\
}																														\
																														\
Bool Name##_isConstRef(Name l) { return GenericList_isConstRef(Name##_toList(l)); }										\
Bool Name##_isRef(Name l) { return GenericList_isRef(Name##_toList(l)); }												\
Bool Name##_empty(Name l) { return GenericList_empty(Name##_toList(l)); }												\
Bool Name##_any(Name l) { return GenericList_any(Name##_toList(l)); }													\
U64  Name##_bytes(Name l) { return GenericList_bytes(Name##_toList(l)); }												\
U64  Name##_allocatedBytes(Name l) { return GenericList_allocatedBytes(Name##_toList(l)); }								\
U64  Name##_stride() { return sizeof(Name##_Type); }																	\
																														\
Buffer Name##_buffer(Name l) { return GenericList_buffer(Name##_toList(l)); }											\
Buffer Name##_bufferConst(Name l) { return GenericList_bufferConst(Name##_toList(l)); }									\
Buffer Name##_allocatedBuffer(Name l) { return GenericList_allocatedBuffer(Name##_toList(l)); }							\
Buffer Name##_allocatedBufferConst(Name l) { return GenericList_allocatedBufferConst(Name##_toList(l)); }				\
																														\
Name##_Type *Name##_begin(Name l) { return (Name##_Type*) GenericList_begin(Name##_toList(l)); }						\
Name##_Type *Name##_end(Name l) { return (Name##_Type*) GenericList_end(Name##_toList(l)); }							\
Name##_Type *Name##_last(Name l) { return (Name##_Type*) GenericList_last(Name##_toList(l)); }							\
Name##_Type *Name##_ptr(Name l, U64 i) { return (Name##_Type*) GenericList_ptr(Name##_toList(l), i); }					\
Name##_Type Name##_at(Name l, U64 i) { return *(Name##_Type*) GenericList_ptr(Name##_toList(l), i); }					\
																														\
const Name##_Type *Name##_beginConst(Name l) { return (Name##_Type*) GenericList_beginConst(Name##_toList(l)); }		\
const Name##_Type *Name##_endConst(Name l) { return (Name##_Type*) GenericList_endConst(Name##_toList(l)); }			\
const Name##_Type *Name##_lastConst(Name l) { return (Name##_Type*) GenericList_lastConst(Name##_toList(l)); }			\
const Name##_Type *Name##_ptrConst(Name l, U64 i) { return (Name##_Type*) GenericList_ptrConst(Name##_toList(l), i); }	\
Name##_Type Name##_atConst(Name l, U64 i) { return *(Name##_Type*) GenericList_ptrConst(Name##_toList(l), i); }			\
																														\
Error Name##_swap(Name l, U64 i, U64 j) { return GenericList_swap(Name##_toList(l), i, j); }							\
Bool Name##_reverse(Name l) { return GenericList_reverse(Name##_toList(l)); }											\
Bool Name##_sortCustom(Name l, CompareFunction func) { return GenericList_sortCustom(Name##_toList(l), func); }			\
																														\
Bool Name##_eq(Name a, Name b) { return GenericList_eq(Name##_toList(a), Name##_toList(b)); }							\
Bool Name##_neq(Name a, Name b) { return !Name##_eq(a, b); }															\
																														\
Error Name##_createFromBuffer(Buffer buf, Name *result) {																\
	Allocator alloc = (Allocator) { 0 };																				\
	TListWrapCtor(Name, Error err = GenericList_createFromBuffer(buf, sizeof(Name##_Type), &list));						\
}																														\
																														\
Error Name##_createSubset(Name l, U64 index, U64 length, Name *result) {												\
	Allocator alloc = (Allocator) { 0 };																				\
	TListWrapCtor(Name, Error err = GenericList_createSubset(Name##_toList(l), index, length, &list));					\
}																														\
																														\
Error Name##_create(U64 length, Allocator alloc, Name *result) {														\
	TListWrapCtor(Name, Error err = GenericList_create(length, sizeof(Name##_Type), alloc, &list));						\
}																														\
																														\
Error Name##_createCopy(Name l, Allocator alloc, Name *result) {														\
	TListWrapCtor(Name, Error err = GenericList_createCopy(Name##_toList(l), alloc, &list));							\
}																														\
																														\
Error Name##_createCopySubset(Name l, U64 off, U64 len, Allocator alloc, Name *result) {								\
	TListWrapCtor(Name, Error err = GenericList_createCopySubset(Name##_toList(l), off, len, alloc, &list));			\
}																														\
																														\
Error Name##_createRepeated(U64 length, Name##_Type data, Allocator alloc, Name *result) {								\
	Buffer buf = Buffer_createRefConst((const U8*)&data, sizeof(Name##_Type));											\
	TListWrapCtor(Name, Error err = GenericList_createRepeated(length, sizeof(Name##_Type), buf, alloc, &list));		\
}																														\
																														\
Error Name##_createSubsetReverse(Name l, U64 index, U64 length, Allocator alloc, Name *result) {						\
	TListWrapCtor(Name, Error err = GenericList_createSubsetReverse(Name##_toList(l), index, length, alloc, &list));	\
}																														\
																														\
Error Name##_createReverse(Name l, Allocator alloc, Name *result) {														\
	TListWrapCtor(Name, Error err = GenericList_createReverse(Name##_toList(l), alloc, &list));							\
}																														\
																														\
Error Name##_createRef(Name##_Type *ptr, U64 length, Name *result) {													\
	Allocator alloc = (Allocator) { 0 };																				\
	TListWrapCtor(Name, Error err = GenericList_createRef(ptr, length, sizeof(Name##_Type), &list));					\
}																														\
																														\
Error Name##_createRefConst(const Name##_Type *ptr, U64 length, Name *result) {											\
Allocator alloc = (Allocator) { 0 };																					\
	TListWrapCtor(Name, Error err = GenericList_createRefConst(ptr, length, sizeof(Name##_Type), &list));				\
}																														\
																														\
Name Name##_createRefFromList(Name t) {																					\
	t.capacityAndRefInfo = U64_MAX;																						\
	return t;																											\
}																														\
																														\
Error Name##_set(Name l, U64 index, Name##_Type t) {																	\
	Buffer buf = Buffer_createRefConst((const U8*)&t, sizeof(Name##_Type));												\
	return GenericList_set(Name##_toList(l), index, buf);																\
}																														\
																														\
Error Name##_get(Name l, U64 index, Name##_Type *t) {																	\
																														\
	if(!t)																												\
		return Error_nullPointer(2, #Name "_get()::t is required");														\
																														\
	Buffer buf = Buffer_createNull();																					\
	Error err = GenericList_get(Name##_toList(l), index, &buf);															\
																														\
	if(err.genericError)																								\
		return err;																										\
																														\
	*t = *(const Name##_Type*) buf.ptr;																					\
	return Error_none();																								\
}																														\
																														\
Bool Name##_contains(Name l, Name##_Type t, U64 offset, EqualsFunction eq) {											\
	Buffer buf = Buffer_createRefConst((const U8*)&t, sizeof(Name##_Type));												\
	return GenericList_contains(Name##_toList(l), buf, offset, eq);														\
}																														\
																														\
U64 Name##_count(Name l, Name##_Type t, EqualsFunction eq) {															\
	Buffer buf = Buffer_createRefConst((const U8*)&t, sizeof(Name##_Type));												\
	return GenericList_count(Name##_toList(l), buf, eq);																\
}																														\
																														\
U64 Name##_findFirst(Name l, Name##_Type t, U64 index, EqualsFunction eq) {												\
	Buffer buf = Buffer_createRefConst((const U8*)&t, sizeof(Name##_Type));												\
	return GenericList_findFirst(Name##_toList(l), buf, index, eq);														\
}																														\
																														\
U64 Name##_findLast(Name l, Name##_Type t, U64 index, EqualsFunction eq) {												\
	Buffer buf = Buffer_createRefConst((const U8*)&t, sizeof(Name##_Type));												\
	return GenericList_findLast(Name##_toList(l), buf, index, eq);														\
}																														\
																														\
Error Name##_copy(Name src, U64 srcOffset, Name dst, U64 dstOffset, U64 count) {										\
	return GenericList_copy(Name##_toList(src), srcOffset, Name##_toList(dst), dstOffset, count);						\
}																														\
																														\
Error Name##_find(Name l, Name##_Type t, EqualsFunction eq, Allocator allocator, ListU64 *result) {						\
	Buffer buf = Buffer_createRefConst((const U8*)&t, sizeof(Name##_Type));												\
	return GenericList_find(Name##_toList(l), buf, eq, allocator, result);												\
}																														\
																														\
Error Name##_popBack(Name *l, Name##_Type *output) {																	\
	Buffer buf = Buffer_createNull();																					\
	if(output) buf = Buffer_createRef(output, sizeof(Name##_Type));														\
	TListWrapModifying(Name, Error err = GenericList_popBack(&list, buf));												\
}																														\
																														\
Error Name##_popFront(Name *l, Name##_Type *output) {																	\
	Buffer buf = Buffer_createNull();																					\
	if(output) buf = Buffer_createRef(output, sizeof(Name##_Type));														\
	TListWrapModifying(Name, Error err = GenericList_popFront(&list, buf));												\
}																														\
																														\
Error Name##_popLocation(Name *l, U64 index, Name##_Type *output) {														\
	Buffer buf = Buffer_createNull();																					\
	if(output) buf = Buffer_createRef(output, sizeof(Name##_Type));														\
	TListWrapModifying(Name, Error err = GenericList_popLocation(&list, index, buf));									\
}																														\
																														\
Error Name##_pushBack(Name *l, Name##_Type t, Allocator allocator) {													\
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));														\
	TListWrapModifying(Name, Error err = GenericList_pushBack(&list, buf, allocator));									\
}																														\
																														\
Error Name##_pushFront(Name *l, Name##_Type t, Allocator allocator) {													\
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));														\
	TListWrapModifying(Name, Error err = GenericList_pushFront(&list, buf, allocator));									\
}																														\
																														\
Error Name##_eraseAllIndices(Name *l, ListU64 indices) {																\
	TListWrapModifying(Name, Error err = GenericList_eraseAllIndices(&list, indices));									\
}																														\
																														\
Error Name##_reserve(Name *l, U64 capacity, Allocator allocator) {														\
	TListWrapModifying(Name, Error err = GenericList_reserve(&list, capacity, allocator));								\
}																														\
																														\
Error Name##_resize(Name *l, U64 size, Allocator allocator) {															\
	TListWrapModifying(Name, Error err = GenericList_resize(&list, size, allocator));									\
}																														\
																														\
Error Name##_shrinkToFit(Name *l, Allocator allocator) {																\
	TListWrapModifying(Name, Error err = GenericList_shrinkToFit(&list, allocator));									\
}																														\
																														\
Error Name##_eraseFirst(Name *l, Name##_Type t, U64 offset, EqualsFunction eq) {										\
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));														\
	TListWrapModifying(Name, Error err = GenericList_eraseFirst(&list, buf, offset, eq));								\
}																														\
																														\
Error Name##_eraseLast(Name *l, Name##_Type t, U64 offset, EqualsFunction eq) {											\
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));														\
	TListWrapModifying(Name, Error err = GenericList_eraseLast(&list, buf, offset, eq));								\
}																														\
																														\
Error Name##_eraseAll(Name *l, Name##_Type t, Allocator allocator, EqualsFunction eq) {									\
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));														\
	TListWrapModifying(Name, Error err = GenericList_eraseAll(&list, buf, allocator, eq));								\
}																														\
																														\
Error Name##_erase(Name *l, U64 index) {																				\
	TListWrapModifying(Name, Error err = GenericList_erase(&list, index));												\
}																														\
																														\
Error Name##_insert(Name *l, U64 index, Name##_Type t, Allocator allocator) {											\
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));														\
	TListWrapModifying(Name, Error err = GenericList_insert(&list, index, buf, allocator));								\
}																														\
																														\
Error Name##_pushAll(Name *l, Name other, Allocator allocator) {														\
	TListWrapModifying(Name, Error err = GenericList_pushAll(&list, Name##_toList(other), allocator));					\
}																														\
																														\
Error Name##_insertAll(Name *l, Name other, U64 offset, Allocator allocator) {											\
	TListWrapModifying(Name, Error err = GenericList_insertAll(&list, Name##_toList(other), offset, allocator));		\
}																														\
																														\
Error Name##_clear(Name *l) {																							\
	if(!l) return Error_nullPointer(0, #Name "_clear::l is required");													\
	l->length = 0;																										\
	return Error_none();																								\
}																														\
																														\
Bool Name##_free(Name *l, Allocator allocator) {																		\
																														\
	if(!l)																												\
		return true;																									\
																														\
	GenericList temp = Name##_toList(*l);																				\
	Bool b = GenericList_free(&temp, allocator);																		\
	*l = (Name) { 0 };																									\
	return b;																											\
}																														\
																														\
TListXBaseImpl(Name)

#define TListNamedImpl(Name) TListNamedBaseImpl(Name)
#define TListImpl(T) TListNamedImpl(List##T)

#define TListSortImpl(T) TListNamedBaseImpl(List##T); Bool List##T##_sort(List##T l) {									\
	return GenericList_sort##T(List##T##_toList(l));																	\
}

#ifdef __cplusplus
	}
#endif
