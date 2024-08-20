/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "generic_list.h"

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

GenericList ListVoid_toList(ListVoid ptr, U64 stride);
Error ListVoid_fromList(GenericList list, U64 stride, ListVoid *result);

#define TListNamedBase(Name) 																			\
																										\
GenericList Name##_toList(Name t);																		\
Error Name##_fromList(GenericList list, Name *result); 													\
																										\
Bool Name##_isConstRef(Name l);																			\
Bool Name##_isRef(Name l);																				\
Bool Name##_empty(Name l);																				\
Bool Name##_any(Name l);																				\
U64  Name##_bytes(Name l);																				\
U64  Name##_allocatedBytes(Name l);																		\
U64  Name##_stride();																					\
																										\
Buffer Name##_buffer(Name l);																			\
Buffer Name##_bufferConst(Name l);																		\
Buffer Name##_allocatedBuffer(Name l);																	\
Buffer Name##_allocatedBufferConst(Name l);																\
																										\
Name##_Type *Name##_begin(Name l);																		\
Name##_Type *Name##_end(Name l);																		\
Name##_Type *Name##_last(Name l);																		\
Name##_Type *Name##_ptr(Name l, U64 i);																	\
Name##_Type Name##_at(Name l, U64 i);																	\
																										\
const Name##_Type *Name##_beginConst(Name l);															\
const Name##_Type *Name##_endConst(Name l);																\
const Name##_Type *Name##_lastConst(Name l);															\
const Name##_Type *Name##_ptrConst(Name l, U64 i);														\
Name##_Type Name##_atConst(Name l, U64 i);																\
																										\
Error Name##_swap(Name l, U64 i, U64 j);																\
Bool Name##_reverse(Name l);																			\
Bool Name##_sortCustom(Name l, CompareFunction func);													\
																										\
Bool Name##_eq(Name a, Name b);																			\
Bool Name##_neq(Name a, Name b);																		\
																										\
Error Name##_createFromBuffer(Buffer buf, Name *result);												\
Error Name##_createSubset(Name l, U64 index, U64 length, Name *result);									\
Error Name##_create(U64 length, Allocator alloc, Name *result);											\
Error Name##_createCopy(Name l, Allocator alloc, Name *result);											\
Error Name##_createSubsetCopy(Name l, U64 off, U64 len, Allocator alloc, Name *result);					\
Error Name##_createRepeated(U64 length, Name##_Type data, Allocator alloc, Name *result);				\
Error Name##_createSubsetReverse(Name l, U64 index, U64 length, Allocator alloc, Name *result);			\
Error Name##_createReverse(Name l, Allocator alloc, Name *result);										\
Error Name##_createRef(Name##_Type *ptr, U64 length, Name *result);										\
Error Name##_createRefConst(const Name##_Type *ptr, U64 length, Name *result);							\
Name Name##_createRefFromList(Name t);																	\
																										\
Error Name##_set(Name l, U64 index, Name##_Type t);														\
Error Name##_get(Name l, U64 index, Name##_Type *t);													\
																										\
Bool Name##_contains(Name l, Name##_Type t, U64 offset, EqualsFunction eq);								\
U64 Name##_count(Name l, Name##_Type t, EqualsFunction eq);												\
U64 Name##_findFirst(Name l, Name##_Type t, U64 index, EqualsFunction eq);								\
U64 Name##_findLast(Name l, Name##_Type t, U64 index, EqualsFunction eq);								\
																										\
Error Name##_copy(Name src, U64 srcOffset, Name dst, U64 dstOffset, U64 count);							\
Error Name##_find(Name l, Name##_Type t, EqualsFunction eq, Allocator allocator, ListU64 *result);		\
																										\
Error Name##_popBack(Name *l, Name##_Type *output);														\
Error Name##_popFront(Name *l, Name##_Type *output);													\
Error Name##_popLocation(Name *l, U64 index, Name##_Type *output);										\
																										\
Error Name##_pushBack(Name *l, Name##_Type t, Allocator allocator);										\
Error Name##_pushFront(Name *l, Name##_Type t, Allocator allocator);									\
																										\
Error Name##_reserve(Name *l, U64 capacity, Allocator allocator);										\
Error Name##_resize(Name *l, U64 size, Allocator allocator);											\
Error Name##_shrinkToFit(Name *l, Allocator allocator);													\
																										\
Error Name##_eraseFirst(Name *l, Name##_Type t, U64 offset, EqualsFunction eq);							\
Error Name##_eraseLast(Name *l, Name##_Type t, U64 offset, EqualsFunction eq);							\
Error Name##_eraseAll(Name *l, Name##_Type t, Allocator allocator, EqualsFunction eq);					\
Error Name##_erase(Name *l, U64 index);																	\
Error Name##_eraseAllIndices(Name *l, ListU64 indices);													\
																										\
Error Name##_insert(Name *l, U64 index, Name##_Type t, Allocator allocator);							\
																										\
Error Name##_pushAll(Name *l, Name other, Allocator allocator);											\
Error Name##_insertAll(Name *l, Name other, U64 offset, Allocator allocator);							\
																										\
Error Name##_clear(Name *l);																			\
Bool Name##_free(Name *l, Allocator allocator);															\
																										\
TListX(Name)

#define TListNamed(T, Name) TListDefinition(T, Name); TListNamedBase(Name)
#define TList(T) TListNamed(T, List##T)
#define TListSort(T) TList(T);						Bool List##T##_sort(List##T l)

TListNamedBase(ListU64);
Bool ListU64_sort(ListU64 l);

TListSort(U8); TListSort(U16); TListSort(U32);
TListSort(I8); TListSort(I16); TListSort(I32); TListSort(I64);
TListSort(F32); TListSort(F64);

TList(ListU8);
TList(ListU16);
TList(ListU32);
TList(ListU64);

void ListListU8_freeUnderlying (ListListU8  *list, Allocator alloc);
void ListListU16_freeUnderlying(ListListU16 *list, Allocator alloc);
void ListListU32_freeUnderlying(ListListU32 *list, Allocator alloc);
void ListListU64_freeUnderlying(ListListU64 *list, Allocator alloc);

TList(Buffer);

#ifdef __cplusplus
	}
#endif
