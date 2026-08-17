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

//types/container/list_impl.h

#pragma once
#include "types/container/list.h"
#include "types/container/buffer.h"
#include "types/base/error.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;

//Helpers for creating the "template" functions of a TList

#ifndef TListXBaseImpl
	#define TListXBaseImpl(Name)
#endif

#define TListWrapCtor(Name, ...) {                                                                                            \
																															\
	GenericList list = (GenericList) { 0 };                                                                                 \
	Bool s_uccess = true;                                                                                                    \
	Bool isAlloc = false;                                                                                                    \
	__VA_ARGS__;                                                                                                            \
	isAlloc = alloc;                                                                                                        \
																															\
	gotoIfError3(clean, ListVoid_fromList(list, sizeof(Name##_Type), (ListVoid*)result, e_rr));                                \
																															\
clean:                                                                                                                        \
	if(!s_uccess && isAlloc)                                                                                                \
		GenericList_free(&list, alloc);                                                                                        \
																															\
	return s_uccess;                                                                                                        \
}

#define TListNamedBaseImpl(Name)                                                                                             \
																															\
Bool Name##_swap(Name l, U64 i, U64 j, Error *e_rr) { return GenericList_swap(Name##_toList(l), i, j, e_rr); }                \
Bool Name##_reverse(Name l) { return GenericList_reverse(Name##_toList(l)); }                                                \
Bool Name##_sortCustom(Name l, CompareFunction func, void *context) {                                                         \
	return GenericList_sortCustom(Name##_toList(l), func, context);                                                           \
}                                                                                                                             \
																															\
Bool Name##_createSubset(const Name l, U64 index, U64 length, Name *result, Error *e_rr) {                                    \
	const Allocator *alloc = (const Allocator*)NULL;                                                                        \
	TListWrapCtor(Name, gotoIfError3(clean, GenericList_createSubset(Name##_toList(l), index, length, &list, e_rr)));        \
}                                                                                                                            \
																															\
Bool Name##_create(U64 length, const Allocator *alloc, Name *result, Error *e_rr) {                                            \
	TListWrapCtor(Name, gotoIfError3(clean, GenericList_create(length, sizeof(Name##_Type), alloc, &list, e_rr)));            \
}                                                                                                                            \
																															\
Bool Name##_createCopy(const Name l, const Allocator *alloc, Name *result, Error *e_rr) {                                    \
	TListWrapCtor(Name, gotoIfError3(clean, GenericList_createCopy(Name##_toList(l), alloc, &list, e_rr)));                    \
}                                                                                                                            \
																															\
Bool Name##_createCopySubset(const Name l, U64 off, U64 len, const Allocator *alloc, Name *result, Error *e_rr) {            \
	TListWrapCtor(Name, gotoIfError3(clean, GenericList_createCopySubset(Name##_toList(l), off, len, alloc, &list, e_rr)));    \
}                                                                                                                            \
																															\
Bool Name##_createRepeated(U64 len, Name##_Type data, const Allocator *alloc, Name *result, Error *e_rr) {                    \
	Buffer buf = Buffer_createRefConst((const U8*)&data, sizeof(Name##_Type));                                                \
	TListWrapCtor(Name, gotoIfError3(                                                                                        \
		clean, GenericList_createRepeated(len, sizeof(Name##_Type), &buf, alloc, &list, e_rr)                                \
	));                                                                                                                        \
}                                                                                                                            \
																															\
Bool Name##_createSubsetReverse(const Name l, U64 id, U64 len, const Allocator *alloc, Name *result, Error *e_rr) {            \
	TListWrapCtor(Name, gotoIfError3(                                                                                        \
		clean, GenericList_createSubsetReverse(Name##_toList(l), id, len, alloc, &list, e_rr)                                \
	));                                                                                                                        \
}                                                                                                                            \
																															\
Bool Name##_createReverse(const Name l, const Allocator *alloc, Name *result, Error *e_rr) {                                \
	TListWrapCtor(Name, gotoIfError3(clean, GenericList_createReverse(Name##_toList(l), alloc, &list, e_rr)));                \
}                                                                                                                            \
																															\
Bool Name##_createRef(Name##_Type *ptr, U64 length, Name *result, Error *e_rr) {                                            \
	const Allocator *alloc = (const Allocator*)NULL;                                                                        \
	TListWrapCtor(Name, gotoIfError3(clean, GenericList_createRef(ptr, length, sizeof(Name##_Type), &list, e_rr)));            \
}                                                                                                                            \
																															\
Bool Name##_createRefConst(const Name##_Type *ptr, U64 length, Name *result, Error *e_rr) {                                    \
	const Allocator *alloc = (const Allocator*)NULL;                                                                        \
	TListWrapCtor(Name, gotoIfError3(clean, GenericList_createRefConst(ptr, length, sizeof(Name##_Type), &list, e_rr)));    \
}                                                                                                                            \
																															\
Bool Name##_set(Name l, U64 index, Name##_Type t, Error *e_rr) {                                                            \
	Buffer buf = Buffer_createRefConst((const U8*)&t, sizeof(Name##_Type));                                                    \
	return GenericList_set(Name##_toList(l), index, &buf, e_rr);                                                            \
}                                                                                                                            \
																															\
Bool Name##_get(const Name l, U64 index, Name##_Type *t, Error *e_rr) {                                                        \
																															\
	Bool s_uccess = true;                                                                                                    \
																															\
	if(!t)                                                                                                                    \
		retError(clean, Error_nullPointer(2, #Name "_get()::t is required"));                                                \
																															\
	Buffer buf = Buffer_createNull();                                                                                        \
	gotoIfError3(clean, GenericList_get(Name##_toList(l), index, &buf, e_rr));                                                \
																															\
	*t = *(const Name##_Type*) buf.ptr;                                                                                        \
clean:                                                                                                                        \
	return s_uccess;                                                                                                        \
}                                                                                                                            \
																															\
Bool Name##_contains(const Name l, Name##_Type t, U64 offset, EqualsFunction eq) {                                            \
	Buffer buf = Buffer_createRefConst((const U8*)&t, sizeof(Name##_Type));                                                    \
	return GenericList_contains(Name##_toList(l), &buf, offset, eq);                                                        \
}                                                                                                                            \
																															\
U64 Name##_count(const Name l, Name##_Type t, EqualsFunction eq) {                                                            \
	Buffer buf = Buffer_createRefConst((const U8*)&t, sizeof(Name##_Type));                                                    \
	return GenericList_count(Name##_toList(l), &buf, eq);                                                                    \
}                                                                                                                            \
																															\
U64 Name##_findFirst(const Name l, Name##_Type t, U64 index, EqualsFunction eq) {                                            \
	Buffer buf = Buffer_createRefConst((const U8*)&t, sizeof(Name##_Type));                                                    \
	return GenericList_findFirst(Name##_toList(l), &buf, index, eq);                                                        \
}                                                                                                                            \
																															\
U64 Name##_findLast(const Name l, Name##_Type t, U64 index, EqualsFunction eq) {                                            \
	Buffer buf = Buffer_createRefConst((const U8*)&t, sizeof(Name##_Type));                                                    \
	return GenericList_findLast(Name##_toList(l), &buf, index, eq);                                                            \
}                                                                                                                            \
																															\
Bool Name##_copy(const Name src, U64 srcOffset, Name dst, U64 dstOffset, U64 count, Error *e_rr) {                            \
	return GenericList_copy(Name##_toList(src), srcOffset, Name##_toList(dst), dstOffset, count, e_rr);                        \
}                                                                                                                            \
																															\
Bool Name##_find(const Name l, Name##_Type t, EqualsFunction eq, const Allocator *allocator, ListU64 *result, Error *e_rr) {\
	Buffer buf = Buffer_createRefConst((const U8*)&t, sizeof(Name##_Type));                                                    \
	return GenericList_find(Name##_toList(l), &buf, eq, allocator, result, e_rr);                                            \
}                                                                                                                            \
																															\
Bool Name##_popBack(Name *l, Name##_Type *output, Error *e_rr) {                                                            \
	Buffer buf = Buffer_createNull();                                                                                        \
	if(output) buf = Buffer_createRef(output, sizeof(Name##_Type));                                                            \
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_popBack(&list, !output ? NULL : &buf, e_rr)));                    \
}                                                                                                                            \
																															\
Bool Name##_popFront(Name *l, Name##_Type *output, Error *e_rr) {                                                            \
	Buffer buf = Buffer_createNull();                                                                                        \
	if(output) buf = Buffer_createRef(output, sizeof(Name##_Type));                                                            \
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_popFront(&list, !output ? NULL : &buf, e_rr)));                \
}                                                                                                                            \
																															\
Bool Name##_popLocation(Name *l, U64 index, Name##_Type *output, Error *e_rr) {                                                \
	Buffer buf = Buffer_createNull();                                                                                        \
	if(output) buf = Buffer_createRef(output, sizeof(Name##_Type));                                                            \
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_popLocation(&list, index, !output ? NULL : &buf, e_rr)));        \
}                                                                                                                            \
																															\
Bool Name##_pushBack(Name *l, Name##_Type t, const Allocator *allocator, Error *e_rr) {                                        \
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));                                                            \
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_pushBack(&list, &buf, allocator, e_rr)));                        \
}                                                                                                                            \
																															\
Bool Name##_pushFront(Name *l, Name##_Type t, const Allocator *allocator, Error *e_rr) {                                    \
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));                                                            \
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_pushFront(&list, &buf, allocator, e_rr)));                        \
}                                                                                                                            \
																															\
Bool Name##_eraseAllIndices(Name *l, const ListU64 *indices, Error *e_rr) {                                                    \
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_eraseAllIndices(&list, indices, e_rr)));                        \
}                                                                                                                            \
																															\
Bool Name##_reserve(Name *l, U64 capacity, const Allocator *allocator, Error *e_rr) {                                        \
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_reserve(&list, capacity, allocator, e_rr)));                    \
}                                                                                                                            \
																															\
Bool Name##_resize(Name *l, U64 size, const Allocator *allocator, Error *e_rr) {                                            \
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_resize(&list, size, allocator, e_rr)));                        \
}                                                                                                                            \
																															\
Bool Name##_shrinkToFit(Name *l, const Allocator *allocator, Error *e_rr) {                                                    \
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_shrinkToFit(&list, allocator, e_rr)));                            \
}                                                                                                                            \
																															\
Bool Name##_eraseFirst(Name *l, Name##_Type t, U64 offset, EqualsFunction eq, Error *e_rr) {                                \
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));                                                            \
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_eraseFirst(&list, &buf, offset, eq, e_rr)));                    \
}                                                                                                                            \
																															\
Bool Name##_eraseLast(Name *l, Name##_Type t, U64 offset, EqualsFunction eq, Error *e_rr) {                                    \
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));                                                            \
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_eraseLast(&list, &buf, offset, eq, e_rr)));                    \
}                                                                                                                            \
																															\
Bool Name##_eraseAll(Name *l, Name##_Type t, const Allocator *allocator, EqualsFunction eq, Error *e_rr) {                    \
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));                                                            \
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_eraseAll(&list, &buf, allocator, eq, e_rr)));                    \
}                                                                                                                            \
																															\
Bool Name##_erase(Name *l, U64 index, Error *e_rr) {                                                                        \
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_erase(&list, index, e_rr)));                                    \
}                                                                                                                            \
																															\
Bool Name##_insert(Name *l, U64 index, Name##_Type t, const Allocator *allocator, Error *e_rr) {                            \
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));                                                            \
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_insert(&list, index, &buf, allocator, e_rr)));                    \
}                                                                                                                            \
																															\
Bool Name##_pushAll(Name *l, const Name other, const Allocator *allocator, Error *e_rr) {                                    \
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_pushAll(&list, Name##_toList(other), allocator, e_rr)));        \
}                                                                                                                            \
																															\
Bool Name##_insertAll(Name *l, const Name oth, U64 off, const Allocator *allocator, Error *e_rr) {                            \
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_insertAll(&list, Name##_toList(oth), off, allocator, e_rr)));    \
}                                                                                                                            \
																															\
Bool Name##_clear(Name *l, Error *e_rr) {                                                                                    \
	Bool s_uccess = true;                                                                                                    \
	if(!l) retError(clean, Error_nullPointer(0, #Name "_clear::l is required"));                                            \
	l->length = 0;                                                                                                            \
clean:                                                                                                                        \
	return s_uccess;                                                                                                        \
}                                                                                                                            \
																															\
void Name##_free(Name *l, const Allocator *allocator) {                                                                        \
																															\
	if(!l)                                                                                                                    \
		return;                                                                                                                \
																															\
	GenericList temp = Name##_toList(*l);                                                                                    \
	GenericList_free(&temp, allocator);                                                                                        \
	*l = (Name) { 0 };                                                                                                        \
}                                                                                                                            \
																															\
TListXBaseImpl(Name)

#define TListNamedImpl(Name) TListNamedBaseImpl(Name)
#define TListImpl(T) TListNamedImpl(List##T)

#define TListSortImpl(T) TListNamedBaseImpl(List##T); Bool List##T##_sort(List##T l) {                                    \
	GenericList gll = List##T##_toList(l);                                                                                \
	return GenericList_sort##T(gll);                                                                                    \
}

#ifdef __cplusplus
	}
#endif
