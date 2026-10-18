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

//types/container/hash_set_impl.h

#pragma once
#include "types/container/hash_set.h"
#include "types/base/error.h"
#include "types/base/mathi.h"

//info is an expression for the const HashIndexInfo * of the key inside one entry, whose stride is sizeof(entry).

#define THashSetNamedImpl(ListName, Name, info)                                                                             \
																															\
U64 Name##_find(const Name *set, const void *key) {                                                                         \
	return set ? HashIndex_find(&set->index, (info), set->list.ptr, key) : U64_MAX;                                         \
}                                                                                                                           \
																															\
Bool Name##_reserve(Name *set, U64 count, const Allocator *allocator, Error *e_rr) {                                        \
																															\
	Bool s_uccess = true;                                                                                                   \
	U64 capacity = 0, grown = 0;                                                                                            \
																															\
	if(!set)                                                                                                                \
		retError(clean, Error_nullPointer(0, #Name "_reserve()::set is required"));                                         \
																															\
	gotoIfError3(clean, HashIndex_reserve(&set->index, (info), set->list.ptr, count, allocator, e_rr));                     \
	capacity = GenericList_capacity(ListName##_toList(set->list));                                                          \
	grown = capacity * 3 / 2;                                                                                               \
																															\
	if(count > capacity)                                                                                                    \
		gotoIfError3(clean, ListName##_reserve(&set->list, count > grown ? count : grown, allocator, e_rr));                \
																															\
clean:                                                                                                                      \
	return s_uccess;                                                                                                        \
}                                                                                                                           \
																															\
Bool Name##_insert(Name *set, ListName##_Type entry, U64 *existing, const Allocator *allocator, Error *e_rr) {              \
																															\
	Bool s_uccess = true;                                                                                                   \
	U64 found = U64_MAX;                                                                                                    \
																															\
	const HashIndexInfo *keyInfo = (info);                                                                                  \
																															\
	const void *key = keyInfo->getKey ?                                                                                     \
		keyInfo->getKey(&entry, 0, keyInfo->context) :                                                                      \
		(const void*)((const U8*) &entry + keyInfo->keyOffset);                                                             \
																															\
	if(existing)                                                                                                            \
		*existing = U64_MAX;                                                                                                \
																															\
	if(!set)                                                                                                                \
		retError(clean, Error_nullPointer(0, #Name "_insert()::set is required"));                                          \
																															\
	found = HashIndex_find(&set->index, keyInfo, set->list.ptr, key);                                                       \
																															\
	if(found != U64_MAX) {                                                                                                  \
		if(existing)                                                                                                        \
			*existing = found;                                                                                              \
		goto clean;                                                                                                         \
	}                                                                                                                       \
																															\
	gotoIfError3(clean, HashIndex_reserve(                                                                                  \
		&set->index, keyInfo, set->list.ptr, set->list.length + 1, allocator, e_rr                                          \
	));                                                                                                                     \
																															\
	gotoIfError3(clean, ListName##_reserveForPush(&set->list, allocator, e_rr));                                            \
																															\
	gotoIfError3(clean, HashIndex_insert(                                                                                   \
		&set->index, keyInfo, set->list.ptr, key, set->list.length, &found, allocator, e_rr                                 \
	));                                                                                                                     \
																															\
	if(existing)                                                                                                            \
		*existing = found;                                                                                                  \
																															\
	if(found == U64_MAX)                                                                                                    \
		gotoIfError3(clean, ListName##_pushBack(&set->list, entry, allocator, e_rr));                                       \
																															\
clean:                                                                                                                      \
	return s_uccess;                                                                                                        \
}                                                                                                                           \
																															\
Bool Name##_eraseSwap(Name *set, U64 entryIndex, ListName##_Type *erased, Error *e_rr) {                                    \
																															\
	Bool s_uccess = true;                                                                                                   \
																															\
	if(!set)                                                                                                                \
		retError(clean, Error_nullPointer(0, #Name "_eraseSwap()::set is required"));                                       \
																															\
	gotoIfError3(clean, HashIndex_eraseSwap(&set->index, (info), set->list.ptr, entryIndex, e_rr));                         \
																															\
	if(erased)                                                                                                              \
		*erased = set->list.ptr[entryIndex];                                                                                \
																															\
	set->list.ptrNonConst[entryIndex] = set->list.ptr[set->list.length - 1];                                                \
	--set->list.length;                                                                                                     \
																															\
clean:                                                                                                                      \
	return s_uccess;                                                                                                        \
}                                                                                                                           \
																															\
Bool Name##_clear(Name *set, Error *e_rr) {                                                                                 \
																															\
	Bool s_uccess = true;                                                                                                   \
																															\
	if(!set)                                                                                                                \
		retError(clean, Error_nullPointer(0, #Name "_clear()::set is required"));                                           \
																															\
	gotoIfError3(clean, HashIndex_clear(&set->index, e_rr));                                                                \
	gotoIfError3(clean, ListName##_clear(&set->list, e_rr));                                                                \
																															\
clean:                                                                                                                      \
	return s_uccess;                                                                                                        \
}                                                                                                                           \
																															\
void Name##_free(Name *set, const Allocator *allocator) {                                                                   \
																															\
	if(!set)                                                                                                                \
		return;                                                                                                             \
																															\
	HashIndex_free(&set->index, allocator);                                                                                 \
	ListName##_free(&set->list, allocator);                                                                                 \
}

#define THashSetImpl(T, info) THashSetNamedImpl(List##T, HashSet##T, info)

//The Key variants build the info for a key type with KeyT##_hash and KeyT##_compare (U32, U64) at
// HASHINDEX_SMALL_LIMIT_WORD, and check the key is sizeof(KeyT) bytes.
//Key: the entry is the key, like a list of pointers or ids. KeyField: the key is entry.field.

#define THashSetNamedImplKeyAt(ListName, Name, KeyT, keyOffsetValue)                                                        \
																															\
static const HashIndexInfo Name##_indexInfo = {                                                                             \
	.hash = KeyT##_hash,                                                                                                    \
	.compare = KeyT##_compare,                                                                                              \
	.stride = sizeof(ListName##_Type),                                                                                      \
	.keyOffset = (keyOffsetValue),                                                                                          \
	.smallLimit = HASHINDEX_SMALL_LIMIT_WORD                                                                                \
};                                                                                                                          \
																															\
THashSetNamedImpl(ListName, Name, &Name##_indexInfo)

#define THashSetNamedImplKey(ListName, Name, KeyT)                                                                          \
																															\
static_assert(sizeof(KeyT) == sizeof(ListName##_Type), #Name " entry has to be sizeof(" #KeyT ") bytes");                   \
																															\
THashSetNamedImplKeyAt(ListName, Name, KeyT, 0)

#define THashSetNamedImplKeyField(ListName, Name, KeyT, field)                                                              \
																															\
static_assert(sizeof(((ListName##_Type*) 0)->field) == sizeof(KeyT), #field " has to be sizeof(" #KeyT ") bytes");          \
																															\
THashSetNamedImplKeyAt(ListName, Name, KeyT, offsetof(ListName##_Type, field))

#define THashSetImplKey(T, KeyT) THashSetNamedImplKey(List##T, HashSet##T, KeyT)
#define THashSetImplKeyField(T, KeyT, field) THashSetNamedImplKeyField(List##T, HashSet##T, KeyT, field)
