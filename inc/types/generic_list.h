/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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

#pragma once
#include "types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct Error Error;

#define TListDefinition(T, Name) typedef T Name##_Type; typedef struct Name {											\
																														\
	union {																												\
		const Name##_Type *ptr;																							\
		Name##_Type *ptrNonConst;		/* check if !const first */														\
	};																													\
																														\
	U64 length;																											\
	U64 capacityAndRefInfo;																								\
																														\
} Name

TListDefinition(U64, ListU64);		//Used by _find and _eraseAllIndices
TListDefinition(void, ListVoid);

//A list for POD types
//Allows both const and non const, only access ptrNonConst if const is checked before
//GenericList is used by TList(T) to handle most logic

typedef struct GenericList {

	union {
		const void *ptr;
		void *ptrNonConst;		//Check if !const first
	};

	U64 stride;
	U64 length;
	U64 capacityAndRefInfo;		//If capacityAndRefInfo is 0 or U64_MAX, it indicates a ref

} GenericList;

Bool GenericList_isConstRef(GenericList l);
Bool GenericList_isRef(GenericList l);

Bool GenericList_empty(GenericList l);
Bool GenericList_any(GenericList l);
U64  GenericList_bytes(GenericList l);
U64  GenericList_allocatedBytes(GenericList l);

U64 GenericList_capacity(GenericList l);

Buffer GenericList_buffer(GenericList l);
Buffer GenericList_allocatedBuffer(GenericList l);

Buffer GenericList_bufferConst(GenericList l);
Buffer GenericList_allocatedBufferConst(GenericList l);

void *GenericList_begin(GenericList list);
void *GenericList_end(GenericList list);
void *GenericList_last(GenericList list);

const void *GenericList_beginConst(GenericList list);
const void *GenericList_endConst(GenericList list);
const void *GenericList_lastConst(GenericList list);

const void *GenericList_ptrConst(GenericList list, U64 elementOffset);
void *GenericList_ptr(GenericList list, U64 elementOffset);

Buffer GenericList_at(GenericList list, U64 offset);
Buffer GenericList_atConst(GenericList list, U64 offset);

Bool GenericList_eq(GenericList a, GenericList b);
Bool GenericList_neq(GenericList a, GenericList b);

GenericList GenericList_createEmpty(U64 stride);
Error GenericList_createFromBuffer(Buffer buf, U64 stride, GenericList *result);
Error GenericList_createSubset(GenericList list, U64 index, U64 length, GenericList *result);

Error GenericList_create(U64 length, U64 stride, Allocator allocator, GenericList *result);
Error GenericList_createCopy(GenericList list, Allocator allocator, GenericList *result);
Error GenericList_createCopySubset(GenericList list, U64 off, U64 len, Allocator allocator, GenericList *result);

Error GenericList_createRepeated(
	U64 length,
	U64 stride,
	Buffer data,
	Allocator allocator,
	GenericList *result
);

Error GenericList_createSubsetReverse(
	GenericList list,
	U64 index,
	U64 length,
	Allocator allocator,
	GenericList *result
);

Error GenericList_createReverse(GenericList list, Allocator allocator, GenericList *result);

Error GenericList_createRef(void *ptr, U64 length, U64 stride, GenericList *result);
Error GenericList_createRefConst(const void *ptr, U64 length, U64 stride, GenericList *result);

Error GenericList_set(GenericList list, U64 index, Buffer buf);
Error GenericList_get(GenericList list, U64 index, Buffer *result);

Error GenericList_copy(
	GenericList src,
	U64 srcOffset,
	GenericList dst,
	U64 dstOffset,
	U64 count
);

Error GenericList_swap(GenericList list, U64 i, U64 j);
Bool GenericList_reverse(GenericList list);

Error GenericList_find(GenericList list, Buffer buf, EqualsFunction eq, Allocator allocator, ListU64 *result);
Error GenericList_eraseAllIndices(GenericList *list, ListU64 indices);

U64 GenericList_findFirst(GenericList list, Buffer buf, U64 index, EqualsFunction eq);
U64 GenericList_findLast(GenericList list, Buffer buf, U64 index, EqualsFunction eq);
U64 GenericList_count(GenericList list, Buffer buf, EqualsFunction eq);

Bool GenericList_contains(GenericList list, Buffer buf, U64 offset, EqualsFunction eq);

Error GenericList_eraseFirst(GenericList *list, Buffer buf, U64 offset, EqualsFunction eq);
Error GenericList_eraseLast(GenericList *list, Buffer buf, U64 offset, EqualsFunction eq);
Error GenericList_eraseAll(GenericList *list, Buffer buf, Allocator allocator, EqualsFunction eq);
Error GenericList_erase(GenericList *list, U64 index);
Error GenericList_insert(GenericList *list, U64 index, Buffer buf, Allocator allocator);
Error GenericList_pushAll(GenericList *list, GenericList other, Allocator allocator);
Error GenericList_insertAll(GenericList *list, GenericList other, U64 offset, Allocator allocator);

Error GenericList_reserve(GenericList *list, U64 capacity, Allocator allocator);
Error GenericList_resize(GenericList *list, U64 size, Allocator allocator);

Error GenericList_shrinkToFit(GenericList *list, Allocator allocator);

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

Bool GenericList_sortString(GenericList list, EStringCase stringCase);
Bool GenericList_sortStringSensitive(GenericList list);
Bool GenericList_sortStringInsensitive(GenericList list);

Bool GenericList_sortCustom(GenericList list, CompareFunction func);

//Expects buf to be sized to stride (to allow copying to the stack)

Error GenericList_popBack(GenericList *list, Buffer output);
Error GenericList_popFront(GenericList *list, Buffer output);
Error GenericList_popLocation(GenericList *list, U64 index, Buffer buf);

Error GenericList_pushBack(GenericList *list, Buffer buf, Allocator allocator);
Error GenericList_pushFront(GenericList *list, Buffer buf, Allocator allocator);

Error GenericList_clear(GenericList *list);		//Doesn't remove data, only makes it unavailable

Bool GenericList_free(GenericList *result, Allocator allocator);

//TList template helper

#define TListWrapModifying(Name, ...) {																					\
																														\
	if(!l)																												\
		return Error_nullPointer(0, #Name " TListWrapNullCheck::l is required");										\
																														\
	GenericList list = Name##_toList(*l);																				\
	__VA_ARGS__;																										\
																														\
	if(err.genericError)																								\
		return err;																										\
																														\
	*l = (Name) { 0 };																									\
	Name##_fromList(list, l); /* can't error */																			\
	return Error_none();																								\
}

#ifdef __cplusplus
	}
#endif
