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

//types/container/list.h

#pragma once
#include "types/container/generic_list.h"

//Extended TList functions

#ifndef TListX
	#define TListX(Name)
#endif

#ifdef __cplusplus
	extern "C" {
#endif

//TList is a simple wrapper around GenericList to make it easier and safer to use.
//GenericList doesn't care about the type (only the stride), TList does care about type too.
//GenericList[] can be useful when handling lots of arrays of different types (though it adds a U64 for the stride each).
		
static inline GenericList ListVoid_toList(const ListVoid v, U64 stride) {
	GenericList list = { { 0 } };
	list.ptr = v.ptr;
	list.length = v.length;
	list.stride = stride;
	list.capacityAndRefInfo = v.capacityAndRefInfo;
	return list;
}

Bool ListVoid_fromList(const GenericList list, U64 stride, ListVoid *result, Error *e_rr);

#define TListNamedBase(Name)                                                                                                \
																															\
static inline GenericList Name##_toList(Name t) {                                                                           \
	ListVoid lvoid = { { 0 } };                                                                                             \
	lvoid.ptr = (const void*) t.ptr;                                                                                        \
	lvoid.length = t.length;                                                                                                \
	lvoid.capacityAndRefInfo = t.capacityAndRefInfo;                                                                        \
	return ListVoid_toList(lvoid, sizeof(Name##_Type));                                                                     \
}                                                                                                                           \
																															\
static inline Bool Name##_fromList(GenericList list, Name *result, Error *e_rr) {                                           \
	return ListVoid_fromList(list, sizeof(Name##_Type), (ListVoid*)result, e_rr);                                           \
}                                                                                                                           \
																															\
static inline Bool Name##_isConstRef(const Name l) { return GenericList_isConstRef(Name##_toList(l)); }                     \
static inline Bool Name##_isRef(const Name l) { return GenericList_isRef(Name##_toList(l)); }                               \
static inline Bool Name##_empty(const Name l) { return GenericList_empty(Name##_toList(l)); }                               \
static inline Bool Name##_any(const Name l) { return GenericList_any(Name##_toList(l)); }                                   \
static inline U64  Name##_bytes(const Name l) { return GenericList_bytes(Name##_toList(l)); }                               \
static inline U64  Name##_allocatedBytes(const Name l) { return GenericList_allocatedBytes(Name##_toList(l)); }             \
static inline U64  Name##_stride() { return sizeof(Name##_Type); }                                                          \
																															\
static inline Buffer Name##_buffer(const Name l) { return GenericList_buffer(Name##_toList(l)); }                           \
static inline Buffer Name##_bufferConst(const Name l) { return GenericList_bufferConst(Name##_toList(l)); }                 \
static inline Buffer Name##_allocatedBuffer(const Name l) { return GenericList_allocatedBuffer(Name##_toList(l)); }         \
static inline Buffer Name##_allocatedBufferConst(const Name l) {                                                            \
	return GenericList_allocatedBufferConst(Name##_toList(l));                                                              \
}                                                                                                                           \
																															\
static inline Name##_Type *Name##_begin(const Name l) { return (Name##_Type*) GenericList_begin(Name##_toList(l)); }        \
static inline Name##_Type *Name##_end(const Name l) { return (Name##_Type*) GenericList_end(Name##_toList(l)); }            \
static inline Name##_Type *Name##_last(const Name l) { return (Name##_Type*) GenericList_last(Name##_toList(l)); }          \
																															\
static inline Name##_Type *Name##_ptr(const Name l, U64 i) {                                                                \
	return (Name##_Type*) GenericList_ptr(Name##_toList(l), i);                                                             \
}                                                                                                                           \
																															\
static inline Name##_Type Name##_at(const Name l, U64 i) {                                                                  \
	return *(Name##_Type*) GenericList_ptr(Name##_toList(l), i);                                                            \
}                                                                                                                           \
																															\
static inline const Name##_Type *Name##_beginConst(const Name l) {                                                          \
	return (Name##_Type*) GenericList_beginConst(Name##_toList(l));                                                         \
}                                                                                                                           \
																															\
static inline const Name##_Type *Name##_endConst(const Name l) {                                                            \
	return (Name##_Type*) GenericList_endConst(Name##_toList(l));                                                           \
}                                                                                                                           \
																															\
static inline const Name##_Type *Name##_lastConst(const Name l) {                                                           \
	return (Name##_Type*) GenericList_lastConst(Name##_toList(l));                                                          \
}                                                                                                                           \
																															\
static inline const Name##_Type *Name##_ptrConst(const Name l, U64 i) {                                                     \
		return (Name##_Type*) GenericList_ptrConst(Name##_toList(l), i);                                                    \
}                                                                                                                           \
																															\
static inline Name##_Type Name##_atConst(const Name l, U64 i) {                                                             \
			return *(Name##_Type*) GenericList_ptrConst(Name##_toList(l), i);                                               \
}                                                                                                                           \
																															\
static inline Bool Name##_eq(const Name a, const Name b) {                                                                  \
	GenericList agl = Name##_toList(a);                                                                                     \
	GenericList bgl = Name##_toList(b);                                                                                     \
	return GenericList_eq(&agl, &bgl);                                                                                      \
}                                                                                                                           \
																															\
static inline Bool Name##_neq(const Name a, const Name b) { return !Name##_eq(a, b); }                                      \
																															\
Bool Name##_swap(Name l, U64 i, U64 j, Error *e_rr);                                                                        \
Bool Name##_reverse(Name l);                                                                                                \
Bool Name##_sortCustom(Name l, CompareFunction func);                                                                       \
																															\
Bool Name##_createSubset(const Name l, U64 index, U64 length, Name *result, Error *e_rr);                                   \
Bool Name##_create(U64 length, const Allocator *alloc, Name *result, Error *e_rr);                                          \
Bool Name##_createCopy(const Name l, const Allocator *alloc, Name *result, Error *e_rr);                                    \
Bool Name##_createCopySubset(const Name l, U64 off, U64 len, const Allocator *alloc, Name *result, Error *e_rr);            \
Bool Name##_createRepeated(U64 length, Name##_Type data, const Allocator *alloc, Name *result, Error *e_rr);                \
Bool Name##_createSubsetReverse(const Name l, U64 index, U64 length, const Allocator *alloc, Name *result, Error *e_rr);    \
Bool Name##_createReverse(const Name l, const Allocator *alloc, Name *result, Error *e_rr);                                 \
Bool Name##_createRef(Name##_Type *ptr, U64 length, Name *result, Error *e_rr);                                             \
Bool Name##_createRefConst(const Name##_Type *ptr, U64 length, Name *result, Error *e_rr);                                  \
																															\
static inline Name Name##_createRefFromList(Name t) {                                                                       \
	t.capacityAndRefInfo = U64_MAX;                                                                                         \
	return t;                                                                                                               \
}                                                                                                                           \
																															\
Bool Name##_set(Name l, U64 index, Name##_Type t, Error *e_rr);                                                             \
Bool Name##_get(const Name l, U64 index, Name##_Type *t, Error *e_rr);                                                      \
																															\
Bool Name##_contains(const Name l, Name##_Type t, U64 offset, EqualsFunction eq);                                           \
U64 Name##_count(const Name l, Name##_Type t, EqualsFunction eq);                                                           \
U64 Name##_findFirst(const Name l, Name##_Type t, U64 index, EqualsFunction eq);                                            \
U64 Name##_findLast(const Name l, Name##_Type t, U64 index, EqualsFunction eq);                                             \
																															\
Bool Name##_copy(const Name src, U64 srcOffset, Name dst, U64 dstOffset, U64 count, Error *e_rr);                           \
Bool Name##_find(const Name l, Name##_Type t, EqualsFunction eq, const Allocator *allocator, ListU64 *result, Error *e_rr); \
																															\
Bool Name##_popBack(Name *l, Name##_Type *output, Error *e_rr);                                                             \
Bool Name##_popFront(Name *l, Name##_Type *output, Error *e_rr);                                                            \
Bool Name##_popLocation(Name *l, U64 index, Name##_Type *output, Error *e_rr);                                              \
																															\
Bool Name##_pushBack(Name *l, Name##_Type t, const Allocator *allocator, Error *e_rr);                                      \
Bool Name##_pushFront(Name *l, Name##_Type t, const Allocator *allocator, Error *e_rr);                                     \
																															\
Bool Name##_eraseAllIndices(Name *l, const ListU64 *indices, Error *e_rr);                                                  \
Bool Name##_reserve(Name *l, U64 capacity, const Allocator *allocator, Error *e_rr);                                        \
Bool Name##_resize(Name *l, U64 size, const Allocator *allocator, Error *e_rr);                                             \
Bool Name##_shrinkToFit(Name *l, const Allocator *allocator, Error *e_rr);                                                  \
																															\
Bool Name##_eraseFirst(Name *l, Name##_Type t, U64 offset, EqualsFunction eq, Error *e_rr);                                 \
Bool Name##_eraseLast(Name *l, Name##_Type t, U64 offset, EqualsFunction eq, Error *e_rr);                                  \
Bool Name##_eraseAll(Name *l, Name##_Type t, const Allocator *allocator, EqualsFunction eq, Error *e_rr);                   \
Bool Name##_erase(Name *l, U64 index, Error *e_rr);                                                                         \
																															\
Bool Name##_insert(Name *l, U64 index, Name##_Type t, const Allocator *allocator, Error *e_rr);                             \
Bool Name##_pushAll(Name *l, const Name other, const Allocator *allocator, Error *e_rr);                                    \
Bool Name##_insertAll(Name *l, const Name other, U64 offset, const Allocator *allocator, Error *e_rr);                      \
																															\
Bool Name##_clear(Name *l, Error *e_rr);                                                                                    \
void Name##_free(Name *l, const Allocator *allocator);                                                                      \
																															\
TListX(Name)

#define TListNamed(T, Name) TListDefinition(T, Name); TListNamedBase(Name)
#define TList(T) TListNamed(T, List##T)
#define TListSort(T) TList(T);                        Bool List##T##_sort(List##T l)

#ifdef __cplusplus
	}
#endif
