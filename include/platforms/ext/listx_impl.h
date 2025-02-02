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
#include "listx.h"
#include "types/container/generic_list.h"
#include "types/container/list.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

//Helpers for creating the "template" functions of a TList

#define TListWrapCtorx(Name, ...) {																						\
																														\
	GenericList list = (GenericList) { 0 }; 																			\
	__VA_ARGS__;																										\
																														\
	if (err.genericError || (err = ListVoid_fromList(list, sizeof(Name##_Type), (ListVoid*)result)).genericError)		\
		GenericList_freex(&list);																						\
																														\
	return err;																											\
}

//Extended TList

#define TListXBaseImpl(Name)																							\
																														\
Error Name##_createx(U64 length, Name *result) {																		\
	TListWrapCtorx(Name, Error err = GenericList_createx(length, sizeof(Name##_Type), &list));							\
}																														\
																														\
Error Name##_createRepeatedx(U64 length, Name##_Type t, Name *result) {													\
	Buffer buf = Buffer_createRefConst((const U8*)&t, sizeof(Name##_Type));												\
	TListWrapCtorx(Name, Error err = GenericList_createRepeatedx(length, sizeof(Name##_Type), buf, &list));				\
}																														\
																														\
Error Name##_createCopyx(Name l, Name *result) {																		\
	TListWrapCtorx(Name, Error err = GenericList_createCopyx(Name##_toList(l), &list));									\
}																														\
																														\
Error Name##_createCopySubsetx(Name l, U64 off, U64 len, Name *result) {												\
	TListWrapCtorx(Name, Error err = GenericList_createCopySubsetx(Name##_toList(l), off, len, &list));					\
}																														\
																														\
Error Name##_createSubsetReversex(Name l, U64 index, U64 length, Name *result) {										\
	TListWrapCtorx(Name, Error err = GenericList_createSubsetReversex(Name##_toList(l), index, length, &list));			\
}																														\
																														\
Error Name##_createReversex(Name l, Name *result) {																		\
	TListWrapCtorx(Name, Error err = GenericList_createReversex(Name##_toList(l), &list));								\
}																														\
																														\
Error Name##_findx(Name l, Name##_Type t, EqualsFunction eq, ListU64 *result) {											\
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));														\
	return GenericList_findx(Name##_toList(l), buf, eq, result);														\
}																														\
																														\
Error Name##_eraseAllx(Name *l, Name##_Type t, EqualsFunction eq) {														\
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));														\
	TListWrapModifying(Name, Error err = GenericList_eraseAllx(&list, buf, eq));										\
}																														\
																														\
Error Name##_insertx(Name *l, U64 index, Name##_Type t) {																\
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));														\
	TListWrapModifying(Name, Error err = GenericList_insertx(&list, index, buf));										\
}																														\
																														\
Error Name##_pushAllx(Name *l, Name other) {																			\
	TListWrapModifying(Name, Error err = GenericList_pushAllx(&list, Name##_toList(other)));							\
}																														\
																														\
Error Name##_insertAllx(Name *l, Name other, U64 offset) {																\
	TListWrapModifying(Name, Error err = GenericList_insertAllx(&list, Name##_toList(other), offset));					\
}																														\
																														\
Error Name##_reservex(Name *l, U64 n) {																					\
	TListWrapModifying(Name, Error err = GenericList_reservex(&list, n));												\
}																														\
																														\
Error Name##_resizex(Name *l, U64 n) { TListWrapModifying(Name, Error err = GenericList_resizex(&list, n)); }			\
Error Name##_shrinkToFitx(Name *l) { TListWrapModifying(Name, Error err = GenericList_shrinkToFitx(&list)); }			\
																														\
Error Name##_pushBackx(Name *l, Name##_Type t) {																		\
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));														\
	TListWrapModifying(Name, Error err = GenericList_pushBackx(&list, buf));											\
}																														\
																														\
Error Name##_pushFrontx(Name *l, Name##_Type t) {																		\
	Buffer buf = Buffer_createRefConst(&t, sizeof(Name##_Type));														\
	TListWrapModifying(Name, Error err = GenericList_pushFrontx(&list, buf));											\
}																														\
																														\
Bool Name##_freex(Name *l) {																							\
																														\
	if(!l)																												\
		return true;																									\
																														\
	GenericList temp = Name##_toList(*l);																				\
	Bool b = GenericList_freex(&temp);																					\
	*l = (Name) { 0 };																									\
	return b;																											\
}

#define TListXImpl(T) TListXBaseImpl(List##T);

#include "types/container/list_impl.h"
