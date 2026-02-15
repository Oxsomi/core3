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

#pragma once
#include "listx.h"
#include "types/container/generic_list.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

//Helpers for creating the "template" functions of a TList

#define TListWrapCtorx(Name, ...) {																							\
																															\
	Bool s_uccess = true;																									\
	Bool alloc = false;																										\
	GenericList list = (GenericList) { 0 }; 																				\
	__VA_ARGS__;																											\
	alloc = true;																											\
																															\
	gotoIfError3(clean, ListVoid_fromList(list, sizeof(Name##_Type), (ListVoid*)result, e_rr));								\
																															\
clean:																														\
	if(!s_uccess && alloc)																									\
		GenericList_freex(&list);																							\
																															\
	return s_uccess;																										\
}

//Extended TList

#define TListXBaseImpl(Name)																								\
																															\
Bool Name##_createx(U64 length, Name *result, Error *e_rr) {																\
	TListWrapCtorx(Name, gotoIfError3(clean, GenericList_createx(length, sizeof(Name##_Type), &list, e_rr)));				\
}																															\
																															\
Bool Name##_createRepeatedx(U64 length, Name##_Type t, Name *result, Error *e_rr) {											\
	Buffer buf = Buffer_createRefConst((const U8*)&t, sizeof(Name##_Type));													\
	TListWrapCtorx(Name, gotoIfError3(clean, GenericList_createRepeatedx(length, sizeof(Name##_Type), &buf, &list, e_rr)));	\
}																															\
																															\
Bool Name##_createCopyx(Name l, Name *result, Error *e_rr) {																\
	TListWrapCtorx(Name, gotoIfError3(clean, GenericList_createCopyx(Name##_toList(l), &list, e_rr)));						\
}																															\
																															\
Bool Name##_createCopySubsetx(Name l, U64 off, U64 len, Name *result, Error *e_rr) {										\
	TListWrapCtorx(Name, gotoIfError3(clean, GenericList_createCopySubsetx(Name##_toList(l), off, len, &list, e_rr)));		\
}																															\
																															\
Bool Name##_createSubsetReversex(Name l, U64 index, U64 length, Name *result, Error *e_rr) {								\
	TListWrapCtorx(Name, gotoIfError3(clean, GenericList_createSubsetReversex(												\
		Name##_toList(l), index, length, &list, e_rr																		\
	)));																													\
}																															\
																															\
Bool Name##_createReversex(Name l, Name *result, Error *e_rr) {																\
	TListWrapCtorx(Name, gotoIfError3(clean, GenericList_createReversex(Name##_toList(l), &list, e_rr)));					\
}																															\
																															\
Bool Name##_findx(Name l, Name##_Type t, EqualsFunction eq, ListU64 *result, Error *e_rr) {									\
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));															\
	return GenericList_findx(Name##_toList(l), &buf, eq, result, e_rr);														\
}																															\
																															\
Bool Name##_eraseAllx(Name *l, Name##_Type t, EqualsFunction eq, Error *e_rr) {												\
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));															\
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_eraseAllx(&list, &buf, eq, e_rr)));							\
}																															\
																															\
Bool Name##_insertx(Name *l, U64 index, Name##_Type t, Error *e_rr) {														\
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));															\
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_insertx(&list, index, &buf, e_rr)));							\
}																															\
																															\
Bool Name##_pushAllx(Name *l, Name other, Error *e_rr) {																	\
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_pushAllx(&list, Name##_toList(other), e_rr)));					\
}																															\
																															\
Bool Name##_insertAllx(Name *l, Name other, U64 offset, Error *e_rr) {														\
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_insertAllx(&list, Name##_toList(other), offset, e_rr)));		\
}																															\
																															\
Bool Name##_reservex(Name *l, U64 n, Error *e_rr) {																			\
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_reservex(&list, n, e_rr)));									\
}																															\
																															\
Bool Name##_resizex(Name *l, U64 n, Error *e_rr) {																			\
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_resizex(&list, n, e_rr)));										\
}																															\
																															\
Bool Name##_shrinkToFitx(Name *l, Error *e_rr) {																			\
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_shrinkToFitx(&list, e_rr)));									\
}																															\
																															\
Bool Name##_pushBackx(Name *l, Name##_Type t, Error *e_rr) {																\
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));															\
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_pushBackx(&list, &buf, e_rr)));								\
}																															\
																															\
Bool Name##_pushFrontx(Name *l, Name##_Type t, Error *e_rr) {																\
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));															\
	TListWrapModifying(Name, gotoIfError3(clean, GenericList_pushFrontx(&list, buf, e_rr)));								\
}																															\
																															\
void Name##_freex(Name *l) {																								\
																															\
	if(!l)																													\
		return;																												\
																															\
	GenericList temp = Name##_toList(*l);																					\
	GenericList_freex(&temp);																								\
	*l = { 0 };																												\
}

#define TListXImpl(T) TListXBaseImpl(List##T);

#include "types/container/list_impl.h"
