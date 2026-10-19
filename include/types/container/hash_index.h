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

//types/container/hash_index.h

#pragma once
#include "types/base/algorithm.h"
#include "types/container/list.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct Error Error;

//An open addressed index laid BESIDE a list the caller owns, owning slots and nothing else: no keys, no
// values, no entries. THashSet (hash_set.h) owns a list together with its index and is what callers use.
//Iteration stays the caller's list in the caller's order, so the seed below can never reorder anything
// observable, which is what lets it be on everywhere.
//A zeroed HashIndex is a valid empty index, which the tracked allocator needs: its set is a statically
// zeroed global already in use before anything could create one.

//The load cap is checked before a probe walks, so an empty slot always exists and no loop needs a limit.

static const U64 HashIndex_maxLoadNumerator = 3;
static const U64 HashIndex_maxLoadDenominator = 4;

static const U64 HashIndex_minCapacity = 64;

//A slot holds a 32 bit hash, which addresses at most 2^32 slots, and an entry index + 1 in a U32.
//At 3/4 load that is about 3.2 billion entries in 32 GiB of slots; a table past that is sharded by the caller.

static const U64 HashIndex_maxEntries = 0xFFFFFFFE;
static const U64 HashIndex_maxCapacity = (U64)1 << 32;

//Mixes fnv1a's weak low bits into the range a mask reads, and nothing else: it is invertible, so it is worth
// nothing against chosen keys.
//Applied to whatever a HashFunction returns, so a hash written by a caller never has to.

static const U64 HashIndex_finalizeMultiplier = 0xD6E8FEB86659FD93;

static inline U32 HashIndex_finalize(U64 hash) {
	hash ^= hash >> 32;
	hash *= HashIndex_finalizeMultiplier;
	hash ^= hash >> 32;
	return (U32) hash;
}

//2 * log2(capacity) + 16. A constant ceiling won't do: Robin Hood compresses the spread but the maximum
// still grows with the table, so one small enough to be a defence fires on honest keys.

U64 HashIndex_displacementCeiling(U64 capacity);

//The one global this reads. Platform_create seeds it from Buffer_csprng before the first table is built;
// until then it is a fixed constant, so a types only test is reproducible. A table captures it once.
//It is per module: a binary that links its own copy of types/container has its own, and has to be handed the
// host's value when it is loaded.
//Seeding is only worth having while all three hold, because a table that leaks its hashes looks defended
// without being so:
//    1. A stored hash lives only in the slot array, which is never written to a file.
//    2. No error string, log line, debug print or iteration order exposes a hash or the seed.
//       One observed (key, hash) pair recovers the seed algebraically.
//    3. CRC32C is never the flooding defence. It is affine over GF(2), so seeding it changes nothing.

void HashIndex_initSeed(U64 seed);

//The whole hash rather than a tag, because the fairness compare, erase and growth all need a slot's distance
// from its own home without having hashed its key. Growth then re-reads no key at all.

typedef struct HashSlot {
	U32 hash;
	U32 index;       //Entry index + 1, so 0 is empty and a zeroed array is already a valid empty index
} HashSlot;

TList(HashSlot);

typedef struct HashIndex {
	ListHashSlot slots;        //Empty in small mode, else a power of two long, so the mask is length - 1
	U64 count;                 //Entries indexed, which is the length of the caller's list
	U64 seed;
} HashIndex;

//Where an entry's key lives. NULL means entries + index * stride + keyOffset, the fast path for a list of
// pointers or handles; a callback covers the rest, like InputDevice.handles holding two strides in one Buffer.

typedef const void *(*HashKeyFunction)(const void *entries, U64 index, void *context);

//The small limit for a word sized key.

#define HASHINDEX_SMALL_LIMIT_WORD 8

//Describes the key type and belongs to its index for the table's whole life: changing the hash or the small
// limit under a built table invalidates every slot.

typedef struct HashIndexInfo {

	HashFunction hash;
	CompareFunction compare;        //ECompareResult_Eq is a match, the ordering is unused
	HashKeyFunction getKey;
	void *context;                  //Handed to hash, compare and getKey untouched

	U64 stride, keyOffset;          //Only read when getKey is NULL

	//Up to this many entries there is no slot array and lookup scans, which beats hashing at that size.
	//HASHINDEX_SMALL_LIMIT_WORD for a word, 4 for a string.

	U32 smallLimit, pad;

} HashIndexInfo;

//Everything a lookup runs is inline and HashIndex_find is forced, since a compiler declines it on cost alone, and a
// copy that isn't inlined keeps the hash, the compare and the key layout behind indirect calls.

static inline const void *HashIndex_keyOf(const HashIndexInfo *info, const void *entries, U64 index) {
	return info->getKey ?
		info->getKey(entries, index, info->context) :
		(const void*)((const U8*) entries + index * info->stride + info->keyOffset);
}

static inline U32 HashIndex_hashKey(const HashIndexInfo *info, U64 seed, const void *key) {
	return HashIndex_finalize(info->hash(key, seed, info->context));
}

static inline Bool HashIndex_entryMatches(const HashIndexInfo *info, const void *entries, U64 entry, const void *key) {
	return info->compare(HashIndex_keyOf(info, entries, entry), key, info->context) == ECompareResult_Eq;
}

//Robin Hood keeps homes non decreasing along a run, so a slot LESS displaced than the probe has reached is a
// proof of absence: the key would have taken that slot.

static inline U64 HashIndex_displacement(U64 pos, U32 hash, U64 mask) {
	return (pos - ((U64) hash & mask)) & mask;
}

//U64_MAX when absent. A hit is ALWAYS confirmed by comparing the key: there is deliberately no find-by-hash
// entry point, since hash equality is necessary and never sufficient.

__forceinline__ static U64 HashIndex_find(
	const HashIndex *index,
	const HashIndexInfo *info,
	const void *entries,
	const void *key
) {

	if(!index || !info || !info->hash || !info->compare || !key)
		return U64_MAX;

	if (!index->slots.length) {

		for(U64 i = 0; i < index->count; ++i)
			if(HashIndex_entryMatches(info, entries, i, key))
				return i;

		return U64_MAX;
	}

	const U64 mask = index->slots.length - 1;
	const U32 hash = HashIndex_hashKey(info, index->seed, key);

	U64 pos = (U64) hash & mask;

	for (U64 displacement = 0; ; ++displacement, pos = (pos + 1) & mask) {

		const HashSlot slot = index->slots.ptr[pos];

		if(!slot.index || HashIndex_displacement(pos, slot.hash, mask) < displacement)
			return U64_MAX;

		//The stored hash rejects a wrong entry without touching the caller's data; the compare is what decides.

		if(slot.hash == hash && HashIndex_entryMatches(info, entries, slot.index - 1, key))
			return slot.index - 1;
	}
}

//Makes room for count entries, so a hot path can be made allocation free.
//Takes the info and the entries because leaving small mode indexes what the list already holds.

Bool HashIndex_reserve(
	HashIndex *index,
	const HashIndexInfo *info,
	const void *entries,
	U64 count,
	const Allocator *allocator,
	Error *e_rr
);

//Get or add. A hit reports the entry in *existing and changes nothing; a miss claims entryIndex, which has to
// be the index the caller is about to push at, which is what stops an index and its list drifting apart.
//The key is passed rather than read out of entries, because on a miss the entry isn't in the list yet.
//
//Mutation is two phase, so a failure can't land halfway:
//
//    gotoIfError3(clean, HashIndex_reserve(&index, &info, list.ptr, list.length + 1, alloc, e_rr));
//    gotoIfError3(clean, ListX_reserveForPush(&list, alloc, e_rr));
//    gotoIfError3(clean, HashIndex_insert(&index, &info, list.ptr, &key, list.length, &existing, alloc, e_rr));
//
//    if(existing == U64_MAX)
//        gotoIfError3(clean, ListX_pushBack(&list, entry, alloc, e_rr));
//
//Hoist both reserves out of the loop wherever the count is known, which is every hot path here.
//Fails with Error_invalidState on a run past the displacement ceiling in a table already holding more than
// 4 slots per entry, which is the flooding case and the only thing that allocates after the reserves.

Bool HashIndex_insert(
	HashIndex *index,
	const HashIndexInfo *info,
	const void *entries,
	const void *key,
	U64 entryIndex,
	U64 *existing,        //Optional. U64_MAX when the key was added rather than found
	const Allocator *allocator,
	Error *e_rr
);

//Removes entry entryIndex and, when it isn't the last, repoints the last entry's slot at it. This reads both
// keys out of the list, so the caller moves entries[count - 1] into entries[entryIndex] and pops AFTER:
//
//    gotoIfError3(clean, HashIndex_eraseSwap(&index, &info, list.ptr, i, e_rr));
//    list.ptrNonConst[i] = list.ptr[list.length - 1];
//    gotoIfError3(clean, ListX_popBack(&list, NULL, e_rr));
//
//Iteration order changes, which is the trade: an ordered erase would memmove the tail on every one.
//There is none, because no site in the tree wants it. Never allocates.

Bool HashIndex_eraseSwap(
	HashIndex *index,
	const HashIndexInfo *info,
	const void *entries,
	U64 entryIndex,
	Error *e_rr
);

Bool HashIndex_clear(HashIndex *index, Error *e_rr);        //Keeps the capacity and the seed, never allocates
void HashIndex_free(HashIndex *index, const Allocator *allocator);

static inline Bool HashIndex_isSmall(const HashIndex index) { return !index.slots.length; }

#ifdef __cplusplus
	}
#endif
