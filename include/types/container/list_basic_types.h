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

#ifdef __cplusplus
	extern "C" {
#endif

TListNamedBase(ListU64);
Bool ListU64_sort(ListU64 l);

TListSort(U8); TListSort(U16); TListSort(U32);
TListSort(I8); TListSort(I16); TListSort(I32); TListSort(I64);
TListSort(F32); TListSort(F64);

TList(ListU8);
TList(ListU16);
TList(ListU32);
TList(ListU64);

void ListListU8_freeUnderlying (ListListU8  *list, const Allocator *alloc);
void ListListU16_freeUnderlying(ListListU16 *list, const Allocator *alloc);
void ListListU32_freeUnderlying(ListListU32 *list, const Allocator *alloc);
void ListListU64_freeUnderlying(ListListU64 *list, const Allocator *alloc);

Bool ListListU8_createCopyUnderlying (const ListListU8  *src, const Allocator *alloc, ListListU8  *dst, Error *e_rr);
Bool ListListU16_createCopyUnderlying(const ListListU16 *src, const Allocator *alloc, ListListU16 *dst, Error *e_rr);
Bool ListListU32_createCopyUnderlying(const ListListU32 *src, const Allocator *alloc, ListListU32 *dst, Error *e_rr);
Bool ListListU64_createCopyUnderlying(const ListListU64 *src, const Allocator *alloc, ListListU64 *dst, Error *e_rr);

TList(Buffer);

void ListBuffer_freeUnderlying(ListBuffer *list, const Allocator *alloc);

#ifdef __cplusplus
	}
#endif
