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

//types/container/test/test_types_container_hash_index.c
//
//Covers HashIndex: the small mode transition, get or add, eraseSwap with the entry that moved, the backward
// shift, the early miss exit, growth, a randomized displacement maximum and a constructed flood.

#include "test_types_container_shared.h"
#include "test_types_container_flood.h"
#include "types/container/hash_index.h"
#include "types/container/list_basic_types.h"
#include "types/container/string.h"
#include "types/math/rand.h"
#include "types/base/allocator.h"
#include "types/base/mathi.h"

//A U64 keyed set beside a ListU64, the shape every consumer in the tree has.

static HashIndexInfo HashIndexTest_u64Info(U32 smallLimit) {
	HashIndexInfo info = (HashIndexInfo) { 0 };
	info.hash = U64_hash;
	info.compare = U64_compare;
	info.stride = sizeof(U64);
	info.smallLimit = smallLimit;
	return info;
}

//Get or add in the two phase order the header documents.

static Bool HashIndexTest_add(
	HashIndex *index,
	const HashIndexInfo *info,
	ListU64 *list,
	U64 key,
	U64 *existing,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	gotoIfError3(clean, HashIndex_reserve(index, info, list->ptr, list->length + 1, alloc, e_rr));
	gotoIfError3(clean, ListU64_reserve(list, list->length + 1, alloc, e_rr));
	gotoIfError3(clean, HashIndex_insert(index, info, list->ptr, &key, list->length, existing, alloc, e_rr));

	if(*existing == U64_MAX)
		gotoIfError3(clean, ListU64_pushBack(list, key, alloc, e_rr));

clean:
	return s_uccess;
}

//eraseSwap in the order the header documents: the list may only move afterwards.

static Bool HashIndexTest_erase(
	HashIndex *index,
	const HashIndexInfo *info,
	ListU64 *list,
	U64 entry,
	Error *e_rr
) {

	Bool s_uccess = true;

	gotoIfError3(clean, HashIndex_eraseSwap(index, info, list->ptr, entry, e_rr));

	list->ptrNonConst[entry] = list->ptr[list->length - 1];

	gotoIfError3(clean, ListU64_popBack(list, NULL, e_rr));

clean:
	return s_uccess;
}

//Every key is findable at the entry it actually sits at, and both halves agree on how many there are.

static Bool HashIndexTest_consistent(const HashIndex *index, const HashIndexInfo *info, const ListU64 *list) {

	if(index->count != list->length)
		return false;

	for (U64 i = 0; i < list->length; ++i) {

		const U64 key = list->ptr[i];

		if(HashIndex_find(index, info, list->ptr, &key) != i)
			return false;
	}

	return true;
}

//Random_nextFloat advances the LCG and reports [0, 1>; the word behind it is what a key wants.
//Its period is 2^32, so every draw below is distinct, which is what lets the counts be asserted exactly.

static U32 HashIndexTest_random(U32 *seed) {
	Random_nextFloat(seed);
	return *seed;
}

void Test_hashIndex(Test *t) {

	const Allocator *alloc = t->alloc;
	Error *e_rr = &t->err;

	Test_setModule(t, "HashIndex");

	//Fixed so the flood below is reproducible. Platform_create hands this entropy instead.

	HashIndex_initSeed(0x0123456789ABCDEF);

	//========================= zeroed index =========================

	{
		HashIndex index = (HashIndex) { 0 };
		HashIndexInfo info = HashIndexTest_u64Info(32);
		const U64 key = 42;

		Test_assert(t, "zeroed index is small", HashIndex_isSmall(index));
		Test_assert(t, "zeroed index finds nothing", HashIndex_find(&index, &info, NULL, &key) == U64_MAX);
		Test_assert(t, "zeroed index clears", HashIndex_clear(&index, e_rr));

		HashIndex_free(&index, alloc);
		Test_assert(t, "freeing a zeroed index is a no-op", !index.slots.ptr && !index.slots.length);
	}

	//========================= small mode, get or add and erase =========================

	{
		HashIndex index = (HashIndex) { 0 };

		//16 rather than 8: HashIndex_reserve is told how many entries the list MAY hold, so at a limit of 8 the
		//reserve for a ninth leaves small mode even when that insert turns out to be a duplicate.

		HashIndexInfo info = HashIndexTest_u64Info(16);
		ListU64 list = (ListU64) { 0 };
		U64 existing = 0;
		Bool ok = true;

		for(U64 i = 0; i < 8; ++i)
			ok &= HashIndexTest_add(&index, &info, &list, i * 7, &existing, alloc, e_rr) && existing == U64_MAX;

		Test_assert(t, "small mode added 8 distinct keys", ok && list.length == 8);
		Test_assert(t, "small mode holds no slots", HashIndex_isSmall(index));
		Test_assert(t, "small mode is consistent", HashIndexTest_consistent(&index, &info, &list));

		//Get or add reports the existing entry and changes nothing.

		Test_assert(t, "duplicate returns the existing entry", (
			HashIndexTest_add(&index, &info, &list, 3 * 7, &existing, alloc, e_rr) &&
			existing == 3 &&
			list.length == 8 &&
			HashIndex_isSmall(index)
		));

		const U64 absent = 1;
		Test_assert(t, "absent key is not found", HashIndex_find(&index, &info, list.ptr, &absent) == U64_MAX);

		//Erasing entry 2 moves entry 7 into its place, the trade eraseSwap documents.

		const U64 moved = list.ptr[7];
		Test_assert(t, "small mode eraseSwap", HashIndexTest_erase(&index, &info, &list, 2, e_rr));
		Test_assert(t, "the last entry took the hole", list.length == 7 && list.ptr[2] == moved);
		Test_assert(t, "still small mode after erase", HashIndex_isSmall(index));
		Test_assert(t, "small mode is consistent after erase", HashIndexTest_consistent(&index, &info, &list));

		const U64 erased = 2 * 7;
		Test_assert(t, "erased key is gone", HashIndex_find(&index, &info, list.ptr, &erased) == U64_MAX);

		HashIndex_free(&index, alloc);
		ListU64_free(&list, alloc);
	}

	//========================= leaving small mode and growing =========================

	{
		HashIndex index = (HashIndex) { 0 };
		HashIndexInfo info = HashIndexTest_u64Info(8);
		ListU64 list = (ListU64) { 0 };
		U64 existing = 0;
		Bool ok = true;

		for(U64 i = 0; i < 8; ++i)
			ok &= HashIndexTest_add(&index, &info, &list, i, &existing, alloc, e_rr);

		Test_assert(t, "still small at the limit", ok && HashIndex_isSmall(index));

		ok &= HashIndexTest_add(&index, &info, &list, 8, &existing, alloc, e_rr);

		Test_assert(t, "one past the limit builds the slot array", ok && !HashIndex_isSmall(index));
		Test_assert(t, "the first slot array is the minimum capacity", index.slots.length == 64);
		Test_assert(t, "every earlier key survived the transition", HashIndexTest_consistent(&index, &info, &list));

		//Past 3/4 of 64 the table doubles, re-placing from its slots rather than from the keys.

		for(U64 i = 9; i < 60; ++i)
			ok &= HashIndexTest_add(&index, &info, &list, i, &existing, alloc, e_rr);

		Test_assert(t, "growth kept every key", ok && index.slots.length == 128 && list.length == 60);
		Test_assert(t, "consistent after growth", HashIndexTest_consistent(&index, &info, &list));
		Test_assert(t, "load stays at or below 3/4", index.count * 4 <= index.slots.length * 3);

		//clear keeps the slot array, so a table refilled every frame allocates once.

		const U64 capacity = index.slots.length;
		Test_assert(t, "clear", HashIndex_clear(&index, e_rr) && ListU64_clear(&list, e_rr));
		Test_assert(t, "clear keeps the capacity", index.slots.length == capacity && !index.count);

		const U64 gone = 11;
		Test_assert(t, "cleared table finds nothing", HashIndex_find(&index, &info, list.ptr, &gone) == U64_MAX);
		Test_assert(t, "refill after clear", HashIndexTest_add(&index, &info, &list, 11, &existing, alloc, e_rr));
		Test_assert(t, "refilled table is consistent", HashIndexTest_consistent(&index, &info, &list));

		HashIndex_free(&index, alloc);
		ListU64_free(&list, alloc);
	}

	//========================= reserve makes the hot path allocation free =========================

	{
		HashIndex index = (HashIndex) { 0 };
		HashIndexInfo info = HashIndexTest_u64Info(4);
		ListU64 list = (ListU64) { 0 };
		U64 existing = 0;
		Bool ok = true;

		ok &= HashIndex_reserve(&index, &info, list.ptr, 300, alloc, e_rr);
		ok &= ListU64_reserve(&list, 300, alloc, e_rr);

		const U64 capacity = index.slots.length;

		Test_assert(t, "reserve built a table for 300", ok && capacity == 512);

		for(U64 i = 0; i < 300; ++i)
			ok &= HashIndexTest_add(&index, &info, &list, i * 0x9E3779B1, &existing, alloc, e_rr);

		Test_assert(t, "300 inserts fit the reserved table", ok && index.slots.length == capacity);
		Test_assert(t, "reserved table is consistent", HashIndexTest_consistent(&index, &info, &list));

		HashIndex_free(&index, alloc);
		ListU64_free(&list, alloc);
	}

	//========================= randomized erase, which is what exercises the backward shift =========================

	{
		HashIndex index = (HashIndex) { 0 };
		HashIndexInfo info = HashIndexTest_u64Info(4);
		ListU64 list = (ListU64) { 0 };
		U64 existing = 0;
		U32 seed = 0x5EED1234;
		Bool ok = HashIndex_reserve(&index, &info, list.ptr, 2000, alloc, e_rr);

		//Up front, because ListX_reserve sets an absolute capacity.

		ok &= ListU64_reserve(&list, 2000, alloc, e_rr);

		for(U64 i = 0; i < 2000; ++i)
			ok &= HashIndexTest_add(&index, &info, &list, HashIndexTest_random(&seed), &existing, alloc, e_rr);

		Test_assert(t, "2000 random keys added", ok);
		Test_assert(t, "consistent before erasing", HashIndexTest_consistent(&index, &info, &list));

		//Erasing from the middle leaves a hole for the backward shift to close.

		for(U64 i = 0; i < 1500 && ok; ++i)
			ok &= HashIndexTest_erase(&index, &info, &list, HashIndexTest_random(&seed) % list.length, e_rr);

		Test_assert(t, "erased down to 500", ok && list.length == 500);
		Test_assert(t, "consistent after 1500 erases", HashIndexTest_consistent(&index, &info, &list));

		//Refilling reuses the slots the shift freed, so no tombstone is left to fill the table up.

		for(U64 i = 0; i < 1500 && ok; ++i)
			ok &= HashIndexTest_add(&index, &info, &list, HashIndexTest_random(&seed), &existing, alloc, e_rr);

		Test_assert(t, "refilled to 2000", ok && list.length == 2000);
		Test_assert(t, "consistent after refilling", HashIndexTest_consistent(&index, &info, &list));
		Test_assert(t, "load stays at or below 3/4 after churn", index.count * 4 <= index.slots.length * 3);

		for(U64 i = 0; i < 2000 && ok; ++i)
			ok &= HashIndexTest_erase(&index, &info, &list, 0, e_rr);

		Test_assert(t, "erased everything", ok && !index.count && !list.length);

		HashIndex_free(&index, alloc);
		ListU64_free(&list, alloc);
	}

	//========================= string keys, sensitive and folded =========================

	{
		HashIndex index = (HashIndex) { 0 };
		HashIndexInfo info = (HashIndexInfo) { 0 };
		info.hash = CharString_hashSeeded;
		info.compare = GenericList_compareString;
		info.stride = sizeof(CharString);
		info.smallLimit = 4;

		ListCharString list = (ListCharString) { 0 };
		Bool ok = true;

		static const C8 *names[] = { "alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta", "theta" };

		for (U64 i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {

			CharString str = CharString_createRefCStrConst(names[i]);
			U64 existing = 0;

			ok &= HashIndex_reserve(&index, &info, list.ptr, list.length + 1, alloc, e_rr);
			ok &= ListCharString_reserve(&list, list.length + 1, alloc, e_rr);
			ok &= HashIndex_insert(&index, &info, list.ptr, &str, list.length, &existing, alloc, e_rr);

			if(ok && existing == U64_MAX)
				ok &= ListCharString_pushBack(&list, str, alloc, e_rr);
		}

		Test_assert(t, "8 string keys added", ok && list.length == 8);

		CharString gamma = CharString_createRefCStrConst("gamma");
		CharString upper = CharString_createRefCStrConst("GAMMA");
		CharString absent = CharString_createRefCStrConst("iota");

		Test_assert(t, "string key found", HashIndex_find(&index, &info, list.ptr, &gamma) == 2);
		Test_assert(t, "case matters when the pair is sensitive", HashIndex_find(&index, &info, list.ptr, &upper) == U64_MAX);
		Test_assert(t, "absent string key", HashIndex_find(&index, &info, list.ptr, &absent) == U64_MAX);

		//CharString_hash is pinned to fnv1a over the length and then the bytes, computed here without going through it.

		CharString hashed[] = {
			CharString_createRefCStrConst(""),
			CharString_createRefCStrConst("gamma"),
			CharString_createRefCStrConst("a string longer than eight bytes"),
			CharString_createRefCStrConst("twelve bytes")
		};

		Bool pinned = true;

		for (U64 i = 0; i < sizeof(hashed) / sizeof(hashed[0]); ++i)
			pinned &= (
				Buffer_fnv1a64(
					CharString_bufferConst(hashed[i]),
					Buffer_fnv1a64Single(CharString_length(hashed[i]), Buffer_fnv1a64Offset)
				) == CharString_hash(hashed[i])
			);

		Test_assert(t, "CharString_hash keeps its fnv1a formula", pinned);

		//The pair has to fold in the hash too, or an equal key lands in a different run and is never compared.

		HashIndexInfo folded = info;
		folded.hash = CharString_hashSeededInsensitive;
		folded.compare = GenericList_compareStringInsensitive;

		HashIndex insensitive = (HashIndex) { 0 };
		ok = true;

		for (U64 i = 0; i < list.length; ++i) {
			U64 existing = 0;
			ok &= HashIndex_reserve(&insensitive, &folded, list.ptr, i + 1, alloc, e_rr);
			ok &= HashIndex_insert(&insensitive, &folded, list.ptr, &list.ptr[i], i, &existing, alloc, e_rr);
		}

		Test_assert(t, "folded index built", ok && insensitive.count == list.length);
		Test_assert(t, "folded index ignores case",
			HashIndex_find(&insensitive, &folded, list.ptr, &upper) == 2 &&
			HashIndex_find(&insensitive, &folded, list.ptr, &gamma) == 2
		);

		Test_assert(t, "folding a key hashes it the same either way",
			HashIndex_finalize(CharString_hashSeededInsensitive(&upper, insensitive.seed, NULL)) ==
			HashIndex_finalize(CharString_hashSeededInsensitive(&gamma, insensitive.seed, NULL))
		);

		HashIndex_free(&insensitive, alloc);
		HashIndex_free(&index, alloc);
		ListCharString_free(&list, alloc);
	}

	//========================= the displacement ceiling on honest keys =========================

	{
		HashIndex index = (HashIndex) { 0 };
		HashIndexInfo info = HashIndexTest_u64Info(4);
		ListU64 list = (ListU64) { 0 };
		U64 existing = 0;
		U32 seed = 0xC0FFEE11;
		Bool ok = HashIndex_reserve(&index, &info, list.ptr, 786000, alloc, e_rr);

		ok &= ListU64_reserve(&list, 786000, alloc, e_rr);

		//3/4 of 2^20 slots, where the ceiling must stay far out of reach or it would fire on honest input.

		for (U64 i = 0; i < 786000; ++i) {
			const U64 key = ((U64) HashIndexTest_random(&seed) << 32) | HashIndexTest_random(&seed);
			ok &= HashIndexTest_add(&index, &info, &list, key, &existing, alloc, e_rr);
		}

		Test_assert(t, "786000 random keys added", ok && index.slots.length == ((U64)1 << 20));

		U64 maxDisplacement = 0;

		for (U64 i = 0; i < index.slots.length; ++i) {

			if(!index.slots.ptr[i].index)
				continue;

			const U64 home = index.slots.ptr[i].hash & (index.slots.length - 1);
			const U64 displacement = (i - home) & (index.slots.length - 1);

			if(maxDisplacement < displacement)
				maxDisplacement = displacement;
		}

		Test_assert(t, "observed maximum stays under half the ceiling at 2^20",
			maxDisplacement * 2 < HashIndex_displacementCeiling(index.slots.length)
		);

		HashIndex_free(&index, alloc);
		ListU64_free(&list, alloc);
	}

	//========================= a constructed flood is refused, not survived =========================

	{
		HashIndex index = (HashIndex) { 0 };
		HashIndexInfo info = (HashIndexInfo) { 0 };
		info.hash = HashFlood_identityHash;
		info.compare = U64_compare;
		info.stride = sizeof(U64);
		info.smallLimit = 4;

		ListU64 list = (ListU64) { 0 };
		Bool refused = false;
		U64 added = 0;

		//Distinct keys that all finalize to the same 32 bit hash, so no capacity can separate them.
		//This is the worst an attacker who solves for the hash can do, and it has to cost them a rejected
		//input rather than a stall.

		for (U64 i = 0; i < 1024 && !refused; ++i) {

			const U64 key = HashFlood_unfinalize(((U64) i << 32) | 0xABCD1234);
			U64 existing = 0;

			if (!HashIndexTest_add(&index, &info, &list, key, &existing, alloc, NULL)) {
				refused = true;
				break;
			}

			++added;
		}

		Test_assert(t, "the flood is refused rather than absorbed", refused);
		Test_assert(t, "it is refused within two doublings of what it earned", index.slots.length <= 256);
		Test_assert(t, "the refused insert left the index matching the list", index.count == list.length);
		Test_assert(t, "what did get in is still findable", HashIndexTest_consistent(&index, &info, &list));

		//Without this the test could pass for the wrong reason, by never having built a collision at all.

		U64 occupied = 0, collided = 0;

		for (U64 i = 0; i < index.slots.length; ++i) {

			if(!index.slots.ptr[i].index)
				continue;

			++occupied;
			collided += index.slots.ptr[i].hash == 0xABCD1234;
		}

		Test_assert(t, "every key that got in shares one hash", added > 1 && occupied && occupied == collided);

		HashIndex_free(&index, alloc);
		ListU64_free(&list, alloc);
	}

	//========================= the ceiling formula =========================

	{
		Test_assert(t, "ceiling of the smallest table", HashIndex_displacementCeiling(HashIndex_minCapacity) == 28);
		Test_assert(t, "ceiling gains two per doubling", HashIndex_displacementCeiling(128) == 30);
		Test_assert(t, "ceiling at 2^20 slots", HashIndex_displacementCeiling((U64)1 << 20) == 56);
		Test_assert(t, "ceiling of the widest table", HashIndex_displacementCeiling(HashIndex_maxCapacity) == 80);
		Test_assert(t, "ceiling rounds a size that is no power of two up", HashIndex_displacementCeiling(65) == 30);
		Test_assert(t, "ceiling of zero", HashIndex_displacementCeiling(0) == 16);
		Test_assert(t, "ceiling of one", HashIndex_displacementCeiling(1) == 16);
		Test_assert(t, "ceiling stops at 63 bits", HashIndex_displacementCeiling(U64_MAX) == 142);
	}

	//========================= a hit at the load limit allocates nothing =========================

	{
		HashIndex index = (HashIndex) { 0 };
		HashIndexInfo info = HashIndexTest_u64Info(0);
		ListU64 list = (ListU64) { 0 };
		const Allocator failing = (Allocator) { 0 };
		U64 existing = 0;
		Bool ok = true;

		for(U64 i = 0; i < 48; ++i)
			ok &= HashIndexTest_add(&index, &info, &list, i * 11, &existing, alloc, e_rr) && existing == U64_MAX;

		const U64 capacity = index.slots.length;
		const U64 present = 5 * 11, absent = 48 * 11;

		Test_assert(t, "setup: a hashed index at exactly 3/4 load", ok && capacity == 64 && index.count == 48);

		Test_assert(t, "a hit at the load limit allocates nothing and cannot fail", (
			HashIndex_insert(&index, &info, list.ptr, &present, list.length, &existing, &failing, NULL) &&
			existing == 5 && index.slots.length == capacity && index.count == 48
		));

		Test_assert(t, "a miss at the load limit needs the allocator and changes nothing without it", (
			!HashIndex_insert(&index, &info, list.ptr, &absent, list.length, &existing, &failing, NULL) &&
			existing == U64_MAX && index.slots.length == capacity && index.count == 48 &&
			HashIndexTest_consistent(&index, &info, &list)
		));

		Test_assert(t, "and takes it once the allocator works", (
			HashIndexTest_add(&index, &info, &list, absent, &existing, alloc, e_rr) &&
			existing == U64_MAX && index.slots.length == capacity * 2 && HashIndexTest_consistent(&index, &info, &list)
		));

		HashIndex_free(&index, alloc);
		ListU64_free(&list, alloc);
	}

	//========================= out of memory =========================

	{
		HashIndex index = (HashIndex) { 0 };
		HashIndexInfo info = HashIndexTest_u64Info(0);
		const Allocator failing = (Allocator) { 0 };
		const U64 key = 1;
		U64 existing = 0;

		Test_assert(t, "insert reports a failing allocator",
			!HashIndex_insert(&index, &info, NULL, &key, 0, &existing, &failing, NULL)
		);

		Test_assert(t, "a failed insert changed nothing", !index.count && HashIndex_isSmall(index));

		HashIndex_free(&index, alloc);
	}

	//========================= guards =========================

	{
		HashIndex index = (HashIndex) { 0 };
		HashIndexInfo info = HashIndexTest_u64Info(32);
		ListU64 list = (ListU64) { 0 };
		U64 existing = 0;

		Test_assert(t, "erasing from an empty index is out of bounds",
			!HashIndex_eraseSwap(&index, &info, list.ptr, 0, NULL)
		);

		Test_assert(t, "first insert", HashIndexTest_add(&index, &info, &list, 5, &existing, alloc, e_rr));

		const U64 key = 6;
		Test_assert(t, "insert rejects an entry index that is not the count",
			!HashIndex_insert(&index, &info, list.ptr, &key, 7, &existing, alloc, NULL)
		);

		Test_assert(t, "a rejected insert changed nothing", index.count == 1 && list.length == 1);

		HashIndex_free(&index, alloc);
		ListU64_free(&list, alloc);
	}
}
