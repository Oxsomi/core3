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

//types/container/test/test_types_container_hash_set.c
//
//Covers THashSet over a whole entry key, a key inside a struct and a key behind a callback: get or add, find,
// small mode, eraseSwap handing the entry back, clear, geometric reserve, an allocator that fails and a flood.

#include "test_types_container_shared.h"
#include "test_types_container_flood.h"
#include "types/container/hash_set_impl.h"
#include "types/container/list_impl.h"
#include "types/container/list_basic_types.h"
#include "types/base/allocator.h"
#include "types/base/mathi.h"

#include <stddef.h>

typedef struct KeyedEntry {
	U64 payload;
	U32 id;
	U32 value;
} KeyedEntry;

TList(KeyedEntry);
TListImpl(KeyedEntry);

THashSet(U64);
THashSet(KeyedEntry);

THashSetNamed(ListKeyedEntry, HashSetKeyedGet);
THashSetNamed(ListU64, HashSetFlood);

static const HashIndexInfo HashSetTest_u64Info = {
	.hash = U64_hash,
	.compare = U64_compare,
	.stride = sizeof(U64),
	.smallLimit = 4
};

static const HashIndexInfo HashSetTest_entryInfo = {
	.hash = U32_hash,
	.compare = U32_compare,
	.stride = sizeof(KeyedEntry),
	.keyOffset = offsetof(KeyedEntry, id),
	.smallLimit = 4
};

//The key is read through a callback, and keyOffset stays 0 where the payload is, so a read that skips the callback
// hashes the wrong field.

static const void *HashSetTest_getId(const void *entries, U64 index, void *context) {
	(void) context;
	return &((const KeyedEntry*) entries)[index].id;
}

static const HashIndexInfo HashSetTest_getKeyInfo = {
	.hash = U32_hash,
	.compare = U32_compare,
	.getKey = HashSetTest_getId,
	.smallLimit = 4
};

static const HashIndexInfo HashSetTest_floodInfo = {
	.hash = HashFlood_identityHash,
	.compare = U64_compare,
	.stride = sizeof(U64),
	.smallLimit = 4
};

THashSetImpl(U64, &HashSetTest_u64Info);
THashSetImpl(KeyedEntry, &HashSetTest_entryInfo);
THashSetNamedImpl(ListKeyedEntry, HashSetKeyedGet, &HashSetTest_getKeyInfo);
THashSetNamedImpl(ListU64, HashSetFlood, &HashSetTest_floodInfo);

//Statically zeroed, so it only works if a zeroed set needs no init call.

static HashSetU64 HashSetTest_global;

static KeyedEntry HashSetTest_entry(U32 id, U32 value) {
	KeyedEntry entry = (KeyedEntry) { 0 };
	entry.payload = (U64) id * 1000003 + value;
	entry.id = id;
	entry.value = value;
	return entry;
}

//Every entry is found at the index it sits at, and both halves agree on how many there are.

static Bool HashSetTest_u64Consistent(const HashSetU64 *set) {

	if(set->index.count != set->list.length)
		return false;

	for (U64 i = 0; i < set->list.length; ++i)
		if(HashSetU64_find(set, &set->list.ptr[i]) != i)
			return false;

	return true;
}

static Bool HashSetTest_entryConsistent(const HashSetKeyedEntry *set) {

	if(set->index.count != set->list.length)
		return false;

	for (U64 i = 0; i < set->list.length; ++i)
		if(HashSetKeyedEntry_find(set, &set->list.ptr[i].id) != i)
			return false;

	return true;
}

static Bool HashSetTest_getKeyConsistent(const HashSetKeyedGet *set) {

	if(set->index.count != set->list.length)
		return false;

	for (U64 i = 0; i < set->list.length; ++i)
		if(HashSetKeyedGet_find(set, &set->list.ptr[i].id) != i)
			return false;

	return true;
}

static Bool HashSetTest_floodConsistent(const HashSetFlood *set) {

	if(set->index.count != set->list.length)
		return false;

	for (U64 i = 0; i < set->list.length; ++i)
		if(HashSetFlood_find(set, &set->list.ptr[i]) != i)
			return false;

	return true;
}

static Bool HashSetTest_sameEntry(KeyedEntry a, KeyedEntry b) {
	return a.payload == b.payload && a.id == b.id && a.value == b.value;
}

void Test_hashSet(Test *t) {

	const Allocator *alloc = t->alloc;
	Error *e_rr = &t->err;

	Test_setModule(t, "HashSet");

	//========================= zeroed set =========================

	{
		HashSetU64 set = (HashSetU64) { 0 };
		const U64 key = 42;

		Test_assert(t, "zeroed set is empty", !set.list.length && !set.index.count && HashIndex_isSmall(set.index));
		Test_assert(t, "zeroed set finds nothing", HashSetU64_find(&set, &key) == U64_MAX);
		Test_assert(t, "zeroed set clears", HashSetU64_clear(&set, e_rr));

		HashSetU64_free(&set, alloc);
		Test_assert(t, "freeing a zeroed set is a no-op", !set.list.ptr && !set.index.slots);
	}

	{
		const U64 key = 7;
		U64 existing = 0;

		Test_assert(t, "a static set takes an insert with no init call", (
			HashSetU64_insert(&HashSetTest_global, key, &existing, alloc, e_rr) &&
			existing == U64_MAX &&
			HashSetU64_find(&HashSetTest_global, &key) == 0
		));

		HashSetU64_free(&HashSetTest_global, alloc);
		Test_assert(t, "free zeroes both halves", (
			!HashSetTest_global.list.ptr &&
			!HashSetTest_global.list.length &&
			!HashSetTest_global.index.slots &&
			!HashSetTest_global.index.count
		));
	}

	//========================= get or add and find, in small mode =========================

	{
		HashSetU64 set = (HashSetU64) { 0 };
		U64 existing = 0;
		Bool ok = true;

		for(U64 i = 0; i < 3; ++i)
			ok &= HashSetU64_insert(&set, i * 7, &existing, alloc, e_rr) && existing == U64_MAX;

		Test_assert(t, "3 distinct keys added in small mode", (
			ok && set.list.length == 3 && HashIndex_isSmall(set.index) && HashSetTest_u64Consistent(&set)
		));

		//Get or add reports the existing entry and changes nothing.

		Test_assert(t, "a duplicate reports the existing entry", (
			HashSetU64_insert(&set, 2 * 7, &existing, alloc, e_rr) &&
			existing == 2 &&
			set.list.length == 3 &&
			set.index.count == 3 &&
			HashIndex_isSmall(set.index)
		));

		Test_assert(t, "existing is optional", (
			HashSetU64_insert(&set, 1 * 7, NULL, alloc, e_rr) && set.list.length == 3 && HashIndex_isSmall(set.index)
		));

		const U64 present = 1 * 7, absent = 1;

		Test_assert(t, "find by key", HashSetU64_find(&set, &present) == 1 && HashIndex_isSmall(set.index));
		Test_assert(t, "an absent key is not found", (
			HashSetU64_find(&set, &absent) == U64_MAX && HashIndex_isSmall(set.index)
		));

		Test_assert(t, "the set is consistent after duplicates", (
			HashSetTest_u64Consistent(&set) && HashIndex_isSmall(set.index)
		));

		HashSetU64_free(&set, alloc);
	}

	//========================= a duplicate at the small limit =========================

	{
		HashSetU64 set = (HashSetU64) { 0 };
		U64 existing = 0;
		Bool ok = true;

		for(U64 i = 0; i < 4; ++i)
			ok &= HashSetU64_insert(&set, i * 7, NULL, alloc, e_rr);

		const U64 capacity = set.list.capacityAndRefInfo;
		const U64 *ptr = set.list.ptr;

		Test_assert(t, "setup: exactly at the small limit", ok && set.list.length == 4 && HashIndex_isSmall(set.index));
		Test_assert(t, "a duplicate at the limit reports the entry", (
			HashSetU64_insert(&set, 3 * 7, &existing, alloc, e_rr) && existing == 3
		));

		Test_assert(t, "and neither leaves small mode nor grows the list", (
			HashIndex_isSmall(set.index) &&
			set.list.length == 4 &&
			set.list.capacityAndRefInfo == capacity &&
			set.list.ptr == ptr &&
			HashSetTest_u64Consistent(&set)
		));

		HashSetU64_free(&set, alloc);
	}

	//========================= leaving small mode =========================

	{
		HashSetU64 set = (HashSetU64) { 0 };
		Bool ok = true;

		for(U64 i = 0; i < 4; ++i)
			ok &= HashSetU64_insert(&set, i, NULL, alloc, e_rr);

		Test_assert(t, "still small at the limit", ok && HashIndex_isSmall(set.index));

		ok &= HashSetU64_insert(&set, 4, NULL, alloc, e_rr);

		Test_assert(t, "one past the limit builds the slot array", ok && !HashIndex_isSmall(set.index));
		Test_assert(t, "every earlier key survived the transition", HashSetTest_u64Consistent(&set));

		for(U64 i = 5; i < 200; ++i)
			ok &= HashSetU64_insert(&set, i, NULL, alloc, e_rr);

		Test_assert(t, "growth kept every key", ok && set.list.length == 200 && HashSetTest_u64Consistent(&set));

		HashSetU64_free(&set, alloc);
	}

	//========================= a key inside a struct =========================

	{
		HashSetKeyedEntry set = (HashSetKeyedEntry) { 0 };
		U64 existing = 0;
		Bool ok = true;

		for(U32 i = 0; i < 10; ++i)
			ok &= (
				HashSetKeyedEntry_insert(&set, HashSetTest_entry(i * 3, i), &existing, alloc, e_rr) &&
				existing == U64_MAX
			);

		Test_assert(t, "10 entries keyed on id", ok && set.list.length == 10 && HashSetTest_entryConsistent(&set));

		//The offered entry is dropped on a hit, so the stored one keeps its value.

		Test_assert(t, "a duplicate id reports the stored entry and keeps it", (
			HashSetKeyedEntry_insert(&set, HashSetTest_entry(4 * 3, 999), &existing, alloc, e_rr) &&
			existing == 4 &&
			set.list.length == 10 &&
			set.list.ptr[4].value == 4
		));

		const U32 id = 7 * 3;
		const U64 found = HashSetKeyedEntry_find(&set, &id);

		Test_assert(t, "find takes the key, not an entry", found == 7 && set.list.ptr[found].value == 7);

		//An entry updates in place as long as its key stays.

		set.list.ptrNonConst[found].value = 70;

		Test_assert(t, "an in place update keeps the entry findable", (
			HashSetKeyedEntry_find(&set, &id) == 7 && set.list.ptr[7].value == 70
		));

		HashSetKeyedEntry_free(&set, alloc);
	}

	//========================= eraseSwap =========================

	{
		HashSetKeyedEntry set = (HashSetKeyedEntry) { 0 };
		KeyedEntry removed = (KeyedEntry) { 0 };
		Bool ok = true;

		for(U32 i = 0; i < 10; ++i)
			ok &= HashSetKeyedEntry_insert(&set, HashSetTest_entry(i * 3, i), NULL, alloc, e_rr);

		const KeyedEntry second = set.list.ptr[2];
		const KeyedEntry last = set.list.ptr[9];

		Test_assert(t, "eraseSwap", ok && HashSetKeyedEntry_eraseSwap(&set, 2, &removed, e_rr));
		Test_assert(t, "the removed entry is handed back whole", HashSetTest_sameEntry(removed, second));
		Test_assert(t, "the last entry took the hole whole", (
			set.list.length == 9 && HashSetTest_sameEntry(set.list.ptr[2], last)
		));

		const U32 erasedId = 2 * 3;
		Test_assert(t, "the erased key is gone", HashSetKeyedEntry_find(&set, &erasedId) == U64_MAX);
		Test_assert(t, "the moved entry is found where it landed", HashSetKeyedEntry_find(&set, &last.id) == 2);
		Test_assert(t, "both halves agree after the swap", HashSetTest_entryConsistent(&set));

		const KeyedEntry tail = set.list.ptr[8];

		Test_assert(t, "erasing the last entry", (
			HashSetKeyedEntry_eraseSwap(&set, 8, &removed, e_rr) && set.list.length == 8
		));

		Test_assert(t, "the last entry is handed back and gone", (
			HashSetTest_sameEntry(removed, tail) &&
			HashSetKeyedEntry_find(&set, &tail.id) == U64_MAX &&
			HashSetTest_entryConsistent(&set)
		));

		removed = HashSetTest_entry(777, 777);

		Test_assert(t, "an index past the end is refused", (
			!HashSetKeyedEntry_eraseSwap(&set, 8, &removed, NULL) && set.list.length == 8 && set.index.count == 8
		));

		Test_assert(t, "and leaves erased untouched", HashSetTest_sameEntry(removed, HashSetTest_entry(777, 777)));

		HashSetKeyedEntry_free(&set, alloc);
	}

	//========================= eraseSwap in small mode =========================

	{
		HashSetKeyedEntry set = (HashSetKeyedEntry) { 0 };
		KeyedEntry removed = (KeyedEntry) { 0 };
		Bool ok = true;

		for(U32 i = 0; i < 4; ++i)
			ok &= HashSetKeyedEntry_insert(&set, HashSetTest_entry(i * 3, i), NULL, alloc, e_rr);

		const KeyedEntry e0 = set.list.ptr[0], e1 = set.list.ptr[1], e2 = set.list.ptr[2], e3 = set.list.ptr[3];

		Test_assert(t, "setup: 4 entries in small mode", ok && HashIndex_isSmall(set.index));

		//First, which pulls the last entry into the front.

		Test_assert(t, "small mode erase of the first entry", (
			HashSetKeyedEntry_eraseSwap(&set, 0, &removed, e_rr) &&
			HashSetTest_sameEntry(removed, e0) &&
			set.list.length == 3 &&
			HashSetTest_sameEntry(set.list.ptr[0], e3) &&
			HashIndex_isSmall(set.index) &&
			HashSetTest_entryConsistent(&set)
		));

		//Middle, with the list now e3, e1, e2.

		Test_assert(t, "small mode erase of a middle entry", (
			HashSetKeyedEntry_eraseSwap(&set, 1, &removed, e_rr) &&
			HashSetTest_sameEntry(removed, e1) &&
			set.list.length == 2 &&
			HashSetTest_sameEntry(set.list.ptr[1], e2) &&
			HashIndex_isSmall(set.index) &&
			HashSetTest_entryConsistent(&set)
		));

		//Last, with the list now e3, e2.

		Test_assert(t, "small mode erase of the last entry", (
			HashSetKeyedEntry_eraseSwap(&set, 1, &removed, e_rr) &&
			HashSetTest_sameEntry(removed, e2) &&
			set.list.length == 1 &&
			HashSetTest_sameEntry(set.list.ptr[0], e3) &&
			HashIndex_isSmall(set.index) &&
			HashSetTest_entryConsistent(&set)
		));

		Test_assert(t, "erased is optional", (
			HashSetKeyedEntry_eraseSwap(&set, 0, NULL, e_rr) && !set.list.length && !set.index.count
		));

		removed = HashSetTest_entry(555, 555);

		Test_assert(t, "an empty small set refuses an erase", !HashSetKeyedEntry_eraseSwap(&set, 0, &removed, NULL));
		Test_assert(t, "and leaves erased untouched", HashSetTest_sameEntry(removed, HashSetTest_entry(555, 555)));

		HashSetKeyedEntry_free(&set, alloc);
	}

	//========================= a key behind a callback =========================

	{
		HashSetKeyedGet set = (HashSetKeyedGet) { 0 };
		KeyedEntry removed = (KeyedEntry) { 0 };
		U64 existing = 0;
		Bool ok = true;

		for(U32 i = 0; i < 3; ++i)
			ok &= (
				HashSetKeyedGet_insert(&set, HashSetTest_entry(i * 3, i), &existing, alloc, e_rr) &&
				existing == U64_MAX
			);

		Test_assert(t, "3 entries keyed through the callback, in small mode", (
			ok && set.list.length == 3 && HashIndex_isSmall(set.index) && HashSetTest_getKeyConsistent(&set)
		));

		for(U32 i = 3; i < 10; ++i)
			ok &= (
				HashSetKeyedGet_insert(&set, HashSetTest_entry(i * 3, i), &existing, alloc, e_rr) &&
				existing == U64_MAX
			);

		Test_assert(t, "10 entries keyed through the callback", (
			ok && set.list.length == 10 && !HashIndex_isSmall(set.index) && HashSetTest_getKeyConsistent(&set)
		));

		Test_assert(t, "a duplicate id reports the stored entry and keeps it", (
			HashSetKeyedGet_insert(&set, HashSetTest_entry(4 * 3, 999), &existing, alloc, e_rr) &&
			existing == 4 &&
			set.list.length == 10 &&
			set.list.ptr[4].value == 4
		));

		const U32 id = 7 * 3, absent = 1;
		Test_assert(t, "find takes the key", HashSetKeyedGet_find(&set, &id) == 7);
		Test_assert(t, "an absent id is not found", HashSetKeyedGet_find(&set, &absent) == U64_MAX);

		const KeyedEntry last = set.list.ptr[9], third = set.list.ptr[3];

		Test_assert(t, "eraseSwap through the callback", (
			HashSetKeyedGet_eraseSwap(&set, 3, &removed, e_rr) &&
			HashSetTest_sameEntry(removed, third) &&
			HashSetTest_sameEntry(set.list.ptr[3], last) &&
			set.list.length == 9 &&
			HashSetKeyedGet_find(&set, &third.id) == U64_MAX &&
			HashSetKeyedGet_find(&set, &last.id) == 3 &&
			HashSetTest_getKeyConsistent(&set)
		));

		HashSetKeyedGet_free(&set, alloc);
	}

	//========================= erasing in scattered order =========================

	{
		HashSetU64 set = (HashSetU64) { 0 };
		Bool ok = true;

		for(U64 i = 0; i < 300; ++i)
			ok &= HashSetU64_insert(&set, i * 0x9E3779B1, NULL, alloc, e_rr);

		for(U64 i = 0; i < 200 && ok; ++i)
			ok &= HashSetU64_eraseSwap(&set, (i * 7919) % set.list.length, NULL, e_rr);

		Test_assert(t, "200 scattered erases left 100", ok && set.list.length == 100);
		Test_assert(t, "the set is consistent after scattered erases", HashSetTest_u64Consistent(&set));

		for(U64 i = 0; i < 100 && ok; ++i)
			ok &= HashSetU64_eraseSwap(&set, 0, NULL, e_rr);

		Test_assert(t, "erased everything", ok && !set.list.length && !set.index.count);

		const U64 key = 5;
		Test_assert(t, "the emptied set takes inserts again", (
			HashSetU64_insert(&set, key, NULL, alloc, e_rr) && HashSetU64_find(&set, &key) == 0
		));

		HashSetU64_free(&set, alloc);
	}

	//========================= clear =========================

	{
		HashSetU64 set = (HashSetU64) { 0 };
		Bool ok = true;

		for(U64 i = 0; i < 100; ++i)
			ok &= HashSetU64_insert(&set, i, NULL, alloc, e_rr);

		const U64 indexCapacity = set.index.capacity;
		const U64 listCapacity = set.list.capacityAndRefInfo;

		Test_assert(t, "clear empties both halves", (
			ok && HashSetU64_clear(&set, e_rr) && !set.list.length && !set.index.count
		));

		Test_assert(t, "clear keeps the capacity of both", (
			indexCapacity && set.index.capacity == indexCapacity && set.list.capacityAndRefInfo == listCapacity
		));

		const U64 gone = 5;
		Test_assert(t, "a cleared set finds nothing", HashSetU64_find(&set, &gone) == U64_MAX);
		Test_assert(t, "refill after clear", (
			HashSetU64_insert(&set, gone, NULL, alloc, e_rr) && set.list.length == 1 && HashSetTest_u64Consistent(&set)
		));

		HashSetU64_free(&set, alloc);
	}

	//========================= reserve makes the hot path allocation free =========================

	{
		HashSetU64 set = (HashSetU64) { 0 };
		Bool ok = HashSetU64_reserve(&set, 300, alloc, e_rr);

		const U64 indexCapacity = set.index.capacity;
		const U64 listCapacity = set.list.capacityAndRefInfo;

		Test_assert(t, "reserve sized both halves", ok && indexCapacity == 512 && listCapacity == 300);

		for(U64 i = 0; i < 300; ++i)
			ok &= HashSetU64_insert(&set, i * 0x9E3779B1, NULL, alloc, e_rr);

		Test_assert(t, "300 inserts grew neither half", (
			ok && set.index.capacity == indexCapacity && set.list.capacityAndRefInfo == listCapacity
		));

		Test_assert(t, "the reserved set is consistent", HashSetTest_u64Consistent(&set));

		HashSetU64_free(&set, alloc);
	}

	//========================= out of memory =========================

	{
		const Allocator failing = (Allocator) { 0 };
		const U64 key = 9;
		U64 existing = 0;

		//Neither half has room, so the list is the one that refuses.

		HashSetU64 empty = (HashSetU64) { 0 };

		Test_assert(t, "insert reports a failing allocator", !HashSetU64_insert(&empty, key, &existing, &failing, NULL));
		Test_assert(t, "existing is U64_MAX on failure", existing == U64_MAX);
		Test_assert(t, "a failed insert changed nothing", (
			!empty.list.ptr && !empty.list.length && !empty.index.count && HashSetU64_find(&empty, &key) == U64_MAX
		));

		//The list has room but the index has to leave small mode, so the index is the one that refuses.

		HashSetU64 full = (HashSetU64) { 0 };
		Bool ok = ListU64_reserve(&full.list, 16, alloc, e_rr);

		for(U64 i = 0; i < 4; ++i)
			ok &= HashSetU64_insert(&full, i, NULL, alloc, e_rr);

		Test_assert(t, "setup: a full small index beside a roomy list", ok && HashIndex_isSmall(full.index));
		Test_assert(t, "the index refusing fails the insert", !HashSetU64_insert(&full, key, &existing, &failing, NULL));
		Test_assert(t, "and leaves both halves as they were", (
			full.list.length == 4 &&
			full.index.count == 4 &&
			HashIndex_isSmall(full.index) &&
			HashSetU64_find(&full, &key) == U64_MAX &&
			HashSetTest_u64Consistent(&full)
		));

		//The index has room but the list is empty, so the list refuses and the index must not have taken the key.

		HashSetU64 roomy = (HashSetU64) { 0 };

		ok = HashIndex_reserve(&roomy.index, &HashSetTest_u64Info, roomy.list.ptr, 100, alloc, e_rr);

		Test_assert(t, "setup: a roomy index beside an empty list", ok && !HashIndex_isSmall(roomy.index));
		Test_assert(t, "the list refusing fails the insert", !HashSetU64_insert(&roomy, key, &existing, &failing, NULL));
		Test_assert(t, "and the index did not take the key", (
			!roomy.list.length && !roomy.index.count && HashSetU64_find(&roomy, &key) == U64_MAX
		));

		//Nothing was left half done, so the same sets take the insert once the allocator works.

		Test_assert(t, "the sets work after a failed insert", (
			HashSetU64_insert(&empty, key, &existing, alloc, e_rr) &&
			HashSetU64_insert(&full, key, NULL, alloc, e_rr) &&
			HashSetU64_insert(&roomy, key, NULL, alloc, e_rr) &&
			existing == U64_MAX &&
			HashSetTest_u64Consistent(&empty) &&
			HashSetTest_u64Consistent(&full) &&
			HashSetTest_u64Consistent(&roomy)
		));

		HashSetU64_free(&empty, alloc);
		HashSetU64_free(&full, alloc);
		HashSetU64_free(&roomy, alloc);
	}

	//========================= a duplicate never needs the allocator =========================

	{
		const Allocator failing = (Allocator) { 0 };
		U64 existing = 0;

		//Small mode with the list exactly full, so a reserve for one more would have to grow both.

		HashSetU64 small = (HashSetU64) { 0 };
		Bool ok = ListU64_reserve(&small.list, 4, alloc, e_rr);

		for(U64 i = 0; i < 4; ++i)
			ok &= HashSetU64_insert(&small, i * 7, NULL, alloc, e_rr);

		const U64 smallCapacity = small.list.capacityAndRefInfo;
		const U64 *smallPtr = small.list.ptr;

		Test_assert(t, "setup: a small set whose list is exactly full", (
			ok && HashIndex_isSmall(small.index) && small.list.length == 4 && smallCapacity == 4
		));

		Test_assert(t, "a duplicate succeeds with a failing allocator in small mode", (
			HashSetU64_insert(&small, 2 * 7, &existing, &failing, NULL) && existing == 2
		));

		Test_assert(t, "and changed nothing", (
			HashIndex_isSmall(small.index) &&
			small.list.length == 4 &&
			small.index.count == 4 &&
			small.list.capacityAndRefInfo == smallCapacity &&
			small.list.ptr == smallPtr &&
			HashSetTest_u64Consistent(&small)
		));

		//Slot array mode with the list exactly full.

		HashSetU64 big = (HashSetU64) { 0 };
		ok = HashSetU64_reserve(&big, 96, alloc, e_rr);

		for(U64 i = 0; i < 96; ++i)
			ok &= HashSetU64_insert(&big, i * 0x9E3779B1, NULL, alloc, e_rr);

		const U64 bigCapacity = big.list.capacityAndRefInfo;
		const U64 indexCapacity = big.index.capacity;
		const U64 *bigPtr = big.list.ptr;
		const HashSlot *slots = big.index.slots;

		Test_assert(t, "setup: a slot array set whose list is exactly full", (
			ok && !HashIndex_isSmall(big.index) && big.list.length == bigCapacity
		));

		existing = 0;

		Test_assert(t, "a duplicate succeeds with a failing allocator past small mode", (
			HashSetU64_insert(&big, (U64) 40 * 0x9E3779B1, &existing, &failing, NULL) && existing == 40
		));

		Test_assert(t, "and changed nothing there either", (
			big.list.length == 96 &&
			big.index.count == 96 &&
			big.list.capacityAndRefInfo == bigCapacity &&
			big.index.capacity == indexCapacity &&
			big.list.ptr == bigPtr &&
			big.index.slots == slots &&
			HashSetTest_u64Consistent(&big)
		));

		HashSetU64_free(&small, alloc);
		HashSetU64_free(&big, alloc);
	}

	//========================= a missing set =========================

	{
		const U64 key = 1;
		U64 existing = 0;
		KeyedEntry removed = HashSetTest_entry(5, 5);

		Test_assert(t, "find on NULL is a miss", HashSetU64_find(NULL, &key) == U64_MAX);
		Test_assert(t, "insert on NULL fails", !HashSetU64_insert(NULL, key, &existing, alloc, NULL));
		Test_assert(t, "and reports U64_MAX", existing == U64_MAX);
		Test_assert(t, "reserve on NULL fails", !HashSetU64_reserve(NULL, 8, alloc, NULL));
		Test_assert(t, "eraseSwap on NULL fails", !HashSetKeyedEntry_eraseSwap(NULL, 0, &removed, NULL));
		Test_assert(t, "and leaves erased untouched", HashSetTest_sameEntry(removed, HashSetTest_entry(5, 5)));
		Test_assert(t, "clear on NULL fails", !HashSetU64_clear(NULL, NULL));

		HashSetU64_free(NULL, alloc);
	}

	//========================= reserve and a failing allocator =========================

	{
		const Allocator failing = (Allocator) { 0 };
		HashSetU64 set = (HashSetU64) { 0 };
		Bool ok = true;

		for(U64 i = 0; i < 10; ++i)
			ok &= HashSetU64_insert(&set, i * 0x9E3779B1, NULL, alloc, e_rr);

		const U64 *ptr = set.list.ptr;
		const HashSlot *slots = set.index.slots;
		const U64 listCapacity = set.list.capacityAndRefInfo;
		const U64 indexCapacity = set.index.capacity;

		Test_assert(t, "setup: 10 keys past small mode", ok && !HashIndex_isSmall(set.index));
		Test_assert(t, "reserve fails with a failing allocator", !HashSetU64_reserve(&set, 5000, &failing, NULL));
		Test_assert(t, "and leaves both halves and every key as they were", (
			set.list.length == 10 &&
			set.index.count == 10 &&
			set.list.ptr == ptr &&
			set.index.slots == slots &&
			set.list.capacityAndRefInfo == listCapacity &&
			set.index.capacity == indexCapacity &&
			HashSetTest_u64Consistent(&set)
		));

		HashSetU64_free(&set, alloc);

		//The index already has room, so only the list refuses.

		HashSetU64 roomy = (HashSetU64) { 0 };
		ok = HashIndex_reserve(&roomy.index, &HashSetTest_u64Info, roomy.list.ptr, 100, alloc, e_rr);

		Test_assert(t, "setup: a roomy index beside an empty list", ok && !HashIndex_isSmall(roomy.index));
		Test_assert(t, "reserve fails when only the list refuses", !HashSetU64_reserve(&roomy, 50, &failing, NULL));
		Test_assert(t, "and the set is still empty", !roomy.list.ptr && !roomy.list.length && !roomy.index.count);

		HashSetU64_free(&roomy, alloc);

		//Up to the small limit neither half needs a slot array.

		HashSetU64 fresh = (HashSetU64) { 0 };

		Test_assert(t, "reserve up to the small limit", HashSetU64_reserve(&fresh, 4, alloc, e_rr));
		Test_assert(t, "keeps the index small", HashIndex_isSmall(fresh.index) && fresh.list.capacityAndRefInfo == 4);

		HashSetU64_free(&fresh, alloc);
	}

	//========================= reserving one more grows geometrically =========================

	{
		HashSetU64 set = (HashSetU64) { 0 };
		Bool ok = true;
		U64 changes = 0;

		const U64 *ptr = set.list.ptr;
		U64 capacity = set.list.capacityAndRefInfo;

		for(U64 i = 0; i < 1000; ++i) {

			ok &= HashSetU64_reserve(&set, set.list.length + 1, alloc, e_rr);
			ok &= HashSetU64_insert(&set, i * 0x9E3779B1, NULL, alloc, e_rr);

			if(set.list.ptr != ptr || set.list.capacityAndRefInfo != capacity)
				++changes;

			ptr = set.list.ptr;
			capacity = set.list.capacityAndRefInfo;
		}

		Test_assert(t, "1000 reserve and insert rounds", ok && set.list.length == 1000 && HashSetTest_u64Consistent(&set));
		Test_assert(t, "reserving one more does not reallocate the list every time", changes < 40);

		HashSetU64_free(&set, alloc);
	}

	//========================= a flood is refused and the set stays whole =========================

	{
		HashSetFlood set = (HashSetFlood) { 0 };
		U64 added = 0, refusedKey = 0, existing = 0;
		Bool refused = false;

		//Distinct keys that all finalize to one 32 bit hash, so no capacity can separate them.

		for (U64 i = 0; i < 1024 && !refused; ++i) {

			const U64 key = HashFlood_unfinalize((i << 32) | 0xABCD1234);
			existing = 0;

			if(!HashSetFlood_insert(&set, key, &existing, alloc, NULL)) {
				refused = true;
				refusedKey = key;
				break;
			}

			++added;
		}

		Test_assert(t, "the flood is refused through the set", refused && added > 1);
		Test_assert(t, "a refused insert reports U64_MAX", existing == U64_MAX);
		Test_assert(t, "both halves still agree", set.list.length == set.index.count && set.list.length == added);
		Test_assert(t, "the refused key is not findable", HashSetFlood_find(&set, &refusedKey) == U64_MAX);

		Bool inList = false;

		for (U64 i = 0; i < set.list.length; ++i)
			inList |= set.list.ptr[i] == refusedKey;

		Test_assert(t, "and not in the list", !inList);
		Test_assert(t, "what did get in is still findable", HashSetTest_floodConsistent(&set));

		//A key that finalizes to the far side of the table from the colliding run.

		const U64 other = HashFlood_unfinalize(((U64) 5000 << 32) | (0xABCD1234 + set.index.capacity / 2));

		Test_assert(t, "a key that does not collide still inserts", (
			HashSetFlood_insert(&set, other, &existing, alloc, e_rr) &&
			existing == U64_MAX &&
			HashSetFlood_find(&set, &other) == added &&
			HashSetTest_floodConsistent(&set)
		));

		HashSetFlood_free(&set, alloc);
	}
}
