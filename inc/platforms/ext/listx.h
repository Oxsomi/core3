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
#include "types/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Include this file before including list.h so the x functions can be found

typedef struct GenericList GenericList;
typedef struct ListU64 ListU64;

Error GenericList_createx(U64 length, U64 stride, GenericList *result);
Error GenericList_createRepeatedx(U64 length, U64 stride, Buffer data, GenericList *result);
Error GenericList_createCopyx(GenericList list, GenericList *result);
Error GenericList_createCopySubsetx(GenericList list, U64 offset, U64 len, GenericList *result);

Error GenericList_createSubsetReversex(GenericList list, U64 index, U64 length, GenericList *result);
Error GenericList_createReversex(GenericList list, GenericList *result);

Error GenericList_findx(GenericList list, Buffer buf, EqualsFunction eq, ListU64 *result);

Error GenericList_eraseAllx(GenericList *list, Buffer buf, EqualsFunction eq);
Error GenericList_insertx(GenericList *list, U64 index, Buffer buf);
Error GenericList_pushAllx(GenericList *list, GenericList other);
Error GenericList_insertAllx(GenericList *list, GenericList other, U64 offset);

Error GenericList_reservex(GenericList *list, U64 capacity);
Error GenericList_resizex(GenericList *list, U64 size);
Error GenericList_shrinkToFitx(GenericList *list);

Error GenericList_pushBackx(GenericList *list, Buffer buf);
Error GenericList_pushFrontx(GenericList *list, Buffer buf);

Bool GenericList_freex(GenericList *result);

#define TListX(Name)																	\
Error Name##_createx(U64 length, Name *result);											\
Error Name##_createRepeatedx(U64 length, Name##_Type t, Name *result);					\
Error Name##_createCopyx(Name l, Name *result);											\
Error Name##_createCopySubsetx(Name l, U64 off, U64 len, Name *result);					\
Error Name##_createSubsetReversex(Name l, U64 index, U64 length, Name *result);			\
Error Name##_createReversex(Name l, Name *result);										\
																						\
Error Name##_findx(Name l, Name##_Type t, EqualsFunction eq, ListU64 *result);			\
																						\
Error Name##_eraseAllx(Name *l, Name##_Type t, EqualsFunction eq);						\
Error Name##_insertx(Name *l, U64 index, Name##_Type t);								\
Error Name##_pushAllx(Name *l, Name other);												\
Error Name##_insertAllx(Name *l, Name other, U64 offset);								\
																						\
Error Name##_reservex(Name *l, U64 n);													\
Error Name##_resizex(Name *l, U64 n);													\
Error Name##_shrinkToFitx(Name *l);														\
																						\
Error Name##_pushBackx(Name *l, Name##_Type t);											\
Error Name##_pushFrontx(Name *l, Name##_Type t);										\
																						\
Bool Name##_freex(Name *l);

#ifdef __cplusplus
	}
#endif
