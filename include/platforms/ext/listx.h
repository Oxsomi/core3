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
#include "types/base/error.h"
#include "types/base/algorithm.h"
#include "platforms/platform.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Include this file before including list.h so the x functions can be found

typedef struct GenericList GenericList;
typedef struct ListU64 ListU64;
typedef struct Buffer Buffer;

static inline Bool GenericList_createx(U64 length, U64 stride, GenericList *result, Error *e_rr) {
	return GenericList_create(length, stride, Platform_instance->alloc, result, e_rr);
}

static inline Bool GenericList_createRepeatedx(U64 length, U64 stride, const Buffer *data, GenericList *result, Error *e_rr) {
	return GenericList_createRepeated(length, stride, data, Platform_instance->alloc, result, e_rr);
}

static inline Bool GenericList_createCopyx(const GenericList list, GenericList *result, Error *e_rr) {
	return GenericList_createCopy(list, Platform_instance->alloc, result, e_rr);
}

static inline Bool GenericList_createCopySubsetx(
	const GenericList list,
	U64 offset,
	U64 len,
	GenericList *result,
	Error *e_rr
) {
	return GenericList_createCopySubset(list, offset, len, Platform_instance->alloc, result, e_rr);
}

static inline Bool GenericList_createSubsetReversex(
	const GenericList list,
	U64 index,
	U64 length,
	GenericList *result,
	Error *e_rr
) {
	return GenericList_createSubsetReverse(list, index, length, Platform_instance->alloc, result, e_rr);
}

static inline Bool GenericList_createReversex(const GenericList list, GenericList *result, Error *e_rr) {
	return GenericList_createSubsetReversex(list, 0, list.length, result, e_rr);
}

static inline Bool GenericList_findx(
	const GenericList list,
	const Buffer *buf,
	EqualsFunction eq,
	ListU64 *result,
	Error *e_rr
) {
	return GenericList_find(list, buf, eq, Platform_instance->alloc, result, e_rr);
}

static inline Bool GenericList_eraseAllx(GenericList *list, const Buffer *buf, EqualsFunction eq, Error *e_rr) {
	return GenericList_eraseAll(list, buf, Platform_instance->alloc, eq, e_rr);
}

static inline Bool GenericList_insertx(GenericList *list, U64 index, const Buffer *buf, Error *e_rr) {
	return GenericList_insert(list, index, buf, Platform_instance->alloc, e_rr);
}

static inline Bool GenericList_pushAllx(GenericList *list, const GenericList other, Error *e_rr) {
	return GenericList_pushAll(list, other, Platform_instance->alloc, e_rr);
}

static inline Bool GenericList_insertAllx(GenericList *list, const GenericList other, U64 offset, Error *e_rr) {
	return GenericList_insertAll(list, other, offset, Platform_instance->alloc, e_rr);
}

static inline Bool GenericList_reservex(GenericList *list, U64 capacity, Error *e_rr) {
	return GenericList_reserve(list, capacity, Platform_instance->alloc, e_rr);
}

static inline Bool GenericList_resizex(GenericList *list, U64 size, Error *e_rr) {
	return GenericList_resize(list, size, Platform_instance->alloc, e_rr);
}

static inline Bool GenericList_shrinkToFitx(GenericList *list, Error *e_rr) {
	return GenericList_shrinkToFit(list, Platform_instance->alloc, e_rr);
}

static inline Bool GenericList_pushBackx(GenericList *l, const Buffer *buf, Error *e_rr) {
	return GenericList_pushBack(l, buf, Platform_instance->alloc, e_rr);
}

static inline Bool GenericList_pushFrontx(GenericList *l, const Buffer *buf, Error *e_rr) {
	return GenericList_pushFront(l, buf, Platform_instance->alloc, e_rr);
}

static inline void GenericList_freex(GenericList *result) { GenericList_free(result, Platform_instance->alloc); }

#define TListX(Name)																			\
Bool Name##_createx(U64 length, Name *result, Error *e_rr);										\
Bool Name##_createRepeatedx(U64 length, Name##_Type t, Name *result, Error *e_rr);				\
Bool Name##_createCopyx(Name l, Name *result, Error *e_rr);										\
Bool Name##_createCopySubsetx(Name l, U64 off, U64 len, Name *result, Error *e_rr);				\
Bool Name##_createSubsetReversex(Name l, U64 index, U64 length, Name *result, Error *e_rr);		\
Bool Name##_createReversex(Name l, Name *result, Error *e_rr);									\
																								\
Bool Name##_findx(Name l, Name##_Type t, EqualsFunction eq, ListU64 *result, Error *e_rr);		\
																								\
Bool Name##_eraseAllx(Name *l, Name##_Type t, EqualsFunction eq, Error *e_rr);					\
Bool Name##_insertx(Name *l, U64 index, Name##_Type t, Error *e_rr);							\
Bool Name##_pushAllx(Name *l, Name other, Error *e_rr);											\
Bool Name##_insertAllx(Name *l, Name other, U64 offset, Error *e_rr);							\
																								\
Bool Name##_reservex(Name *l, U64 n, Error *e_rr);												\
Bool Name##_resizex(Name *l, U64 n, Error *e_rr);												\
Bool Name##_shrinkToFitx(Name *l, Error *e_rr);													\
																								\
Bool Name##_pushBackx(Name *l, Name##_Type t, Error *e_rr);										\
Bool Name##_pushFrontx(Name *l, Name##_Type t, Error *e_rr);									\
																								\
void Name##_freex(Name *l);

static inline void ListBuffer_freeUnderlyingx(ListBuffer *list) {
	ListBuffer_freeUnderlying(list, Platform_instance->alloc);
}

static inline void ListListU8_freeUnderlyingx(ListListU8 *list) {
	ListListU8_freeUnderlying(list, Platform_instance->alloc);
}

static inline void ListListU16_freeUnderlyingx(ListListU16 *list) {
	ListListU16_freeUnderlying(list, Platform_instance->alloc);
}

static inline void ListListU32_freeUnderlyingx(ListListU32 *list) {
	ListListU32_freeUnderlying(list, Platform_instance->alloc);
}

static inline void ListListU64_freeUnderlyingx(ListListU64 *list) {
	ListListU64_freeUnderlying(list, Platform_instance->alloc);
}

static inline Bool ListListU8_createCopyUnderlyingx(const ListListU8 *src, ListListU8 *dst, Error *e_rr) {
	return ListListU8_createCopyUnderlying(src, Platform_instance->alloc, dst, e_rr);
}

static inline Bool ListListU16_createCopyUnderlyingx(const ListListU16 *src, ListListU16 *dst, Error *e_rr) {
	return ListListU16_createCopyUnderlying(src, Platform_instance->alloc, dst, e_rr);
}

static inline Bool ListListU32_createCopyUnderlyingx(const ListListU32 *src, ListListU32 *dst, Error *e_rr) {
	return ListListU32_createCopyUnderlying(src, Platform_instance->alloc, dst, e_rr);
}

static inline Bool ListListU64_createCopyUnderlyingx(const ListListU64 *src, ListListU64 *dst, Error *e_rr) {
	return ListListU64_createCopyUnderlying(src, Platform_instance->alloc, dst, e_rr);
}

#ifdef __cplusplus
	}
#endif
