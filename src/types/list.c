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

#include "types/list_impl.h"

TListNamedBaseImpl(ListU64);

Bool ListU64_sort(ListU64 l) {
	return GenericList_sortU64(ListU64_toList(l));
}

TListSortImpl(U8);	TListSortImpl(U16); TListSortImpl(U32);
TListSortImpl(I8);	TListSortImpl(I16); TListSortImpl(I32); TListSortImpl(I64);
TListSortImpl(F32); TListSortImpl(F64);

TListImpl(Buffer);

TListImpl(ListU8);
TListImpl(ListU16);
TListImpl(ListU32);
TListImpl(ListU64);

void ListListU8_freeUnderlying(ListListU8 *list, Allocator alloc) {

	if(!list)
		return;

	for(U16 i = 0; i < list->length; ++i)
		ListU8_free(&list->ptrNonConst[i], alloc);

	ListListU8_free(list, alloc);
}

void ListListU16_freeUnderlying(ListListU16 *list, Allocator alloc) {

	if(!list)
		return;

	for(U16 i = 0; i < list->length; ++i)
		ListU16_free(&list->ptrNonConst[i], alloc);

	ListListU16_free(list, alloc);
}

void ListListU32_freeUnderlying(ListListU32 *list, Allocator alloc) {

	if(!list)
		return;

	for(U16 i = 0; i < list->length; ++i)
		ListU32_free(&list->ptrNonConst[i], alloc);

	ListListU32_free(list, alloc);
}

void ListListU64_freeUnderlying(ListListU64 *list, Allocator alloc) {

	if(!list)
		return;

	for(U16 i = 0; i < list->length; ++i)
		ListU64_free(&list->ptrNonConst[i], alloc);

	ListListU64_free(list, alloc);
}
