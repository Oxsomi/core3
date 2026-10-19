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

//types/container/hash_set.h

#pragma once
#include "types/container/hash_index.h"
#include "types/container/list.h"

//A TList that owns a HashIndex over its entries, so the protocol in hash_index.h is written once.
//THashSetNamedImpl (hash_set_impl.h) goes in one .c, beside the list's TListNamedImpl.
//A zeroed set is a valid empty set.
//.list is public for iteration and in-place updates; never change a key in place, add or remove only through the set.
//_find takes a pointer to a key, not to an entry.
//_insert is get or add and all or nothing; a hit never allocates, and existing (optional) gets its index or U64_MAX.
//_reserve covers both halves and grows the list geometrically, like a push would.
//_eraseSwap moves the last entry into the hole and hands the removed one back in erased (optional), so whatever it
// owns is released after its key has left the index.
//_free frees both halves, not what the entries own.

#define THashSetNamed(ListName, Name)                                                                                       \
																															\
typedef struct Name {                                                                                                       \
	ListName list;                                                                                                          \
	HashIndex index;                                                                                                        \
} Name;                                                                                                                     \
																															\
U64 Name##_find(const Name *set, const void *key);                                                                          \
Bool Name##_reserve(Name *set, U64 count, const Allocator *allocator, Error *e_rr);                                         \
Bool Name##_insert(Name *set, ListName##_Type entry, U64 *existing, const Allocator *allocator, Error *e_rr);               \
Bool Name##_eraseSwap(Name *set, U64 entryIndex, ListName##_Type *erased, Error *e_rr);                                     \
Bool Name##_clear(Name *set, Error *e_rr);                                                                                  \
void Name##_free(Name *set, const Allocator *allocator)

#define THashSet(T) THashSetNamed(List##T, HashSet##T)
