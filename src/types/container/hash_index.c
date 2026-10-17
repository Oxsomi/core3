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

//types/container/hash_index.c

#include "types/container/hash_index.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

//Read once per table, when its slot array is built, and the only global anything here touches.

static U64 HashIndex_processSeed = 0x9E3779B97F4A7C15;

void HashIndex_initSeed(U64 seed) {
	HashIndex_processSeed = seed;
}

U32 HashIndex_finalize(U64 hash) {
	hash ^= hash >> 32;
	hash *= HashIndex_finalizeMultiplier;
	hash ^= hash >> 32;
	return (U32) hash;
}

U64 HashIndex_displacementCeiling(U64 capacity) {

	U64 bits = 0;

	while(bits < 63 && ((U64)1 << bits) < capacity)
		++bits;

	return (bits << 1) + 16;
}

static const void *HashIndex_keyOf(const HashIndexInfo *info, const void *entries, U64 index) {
	return info->getKey ?
		info->getKey(entries, index, info->context) :
		(const void*)((const U8*) entries + index * info->stride + info->keyOffset);
}

static U32 HashIndex_hashKey(const HashIndexInfo *info, U64 seed, const void *key) {
	return HashIndex_finalize(info->hash(key, seed, info->context));
}

static Bool HashIndex_entryMatches(const HashIndexInfo *info, const void *entries, U64 entry, const void *key) {
	return info->compare(HashIndex_keyOf(info, entries, entry), key, info->context) == ECompareResult_Eq;
}

//Robin Hood keeps homes non decreasing along a run, so a slot LESS displaced than the probe has reached is a
// proof of absence: the key would have taken that slot.

static U64 HashIndex_displacement(U64 pos, U32 hash, U64 mask) {
	return (pos - ((U64) hash & mask)) & mask;
}

//An occupant sitting closer to its own home than what is being carried hands the slot over and is carried on.
//Returns the greatest displacement anything ended up at, so the ceiling is compared against once rather than
// at every step. The load cap is what terminates this: an empty slot always exists.

static U64 HashIndex_place(HashIndex *index, U64 pos, U32 hash, U32 slotIndex) {

	const U64 mask = index->capacity - 1;
	U64 maxDisplacement = 0;

	for (;;) {

		const U64 displacement = HashIndex_displacement(pos, hash, mask);
		const HashSlot slot = index->slots[pos];

		if(maxDisplacement < displacement)
			maxDisplacement = displacement;

		if (!slot.index) {
			index->slots[pos].hash = hash;
			index->slots[pos].index = slotIndex;
			return maxDisplacement;
		}

		if (HashIndex_displacement(pos, slot.hash, mask) < displacement) {
			index->slots[pos].hash = hash;
			index->slots[pos].index = slotIndex;
			hash = slot.hash;
			slotIndex = slot.index;
		}

		pos = (pos + 1) & mask;
	}
}

//False means the index and the list it was laid beside disagree, which is a caller bug rather than a miss.

static Bool HashIndex_findSlot(const HashIndex *index, U32 hash, U64 entry, U64 *res) {

	const U64 mask = index->capacity - 1;
	U64 pos = (U64) hash & mask;

	for (U64 displacement = 0; ; ++displacement, pos = (pos + 1) & mask) {

		const HashSlot slot = index->slots[pos];

		if(!slot.index || HashIndex_displacement(pos, slot.hash, mask) < displacement)
			return false;

		if (slot.index - 1 == entry) {
			*res = pos;
			return true;
		}
	}
}

//Backward shift, so no tombstone is left behind and every run stays contiguous.

static void HashIndex_removeAt(HashIndex *index, U64 pos) {

	const U64 mask = index->capacity - 1;

	for (;;) {

		const U64 next = (pos + 1) & mask;
		const HashSlot slot = index->slots[next];

		if(!slot.index || !HashIndex_displacement(next, slot.hash, mask))
			break;

		index->slots[pos] = slot;
		pos = next;
	}

	index->slots[pos].hash = 0;
	index->slots[pos].index = 0;
}

//A key set solved for one slot survives every doubling, and the ceiling grows with the capacity, so growing
// on a breach would keep looking like it worked and hand an attacker unbounded memory.
//Bounding it against the entry count instead caps the table at 8 slots per entry, so a flood costs two
// doublings and a rejected input rather than a stall.

static Bool HashIndex_mayGrowOnBreach(U64 count, U64 capacity) {
	return count * 4 >= capacity;
}

//Smallest power of two that holds count within the load cap, never below the minimum capacity.

static U64 HashIndex_capacityFor(U64 count) {

	U64 capacity = HashIndex_minCapacity;

	while(
		capacity < HashIndex_maxCapacity &&
		count * HashIndex_maxLoadDenominator > capacity * HashIndex_maxLoadNumerator
	)
		capacity <<= 1;

	return capacity;
}

//Moves the index onto a new slot array, leaving it untouched when that fails.
//entries is only read when there are no slots to read instead, which is leaving small mode.

static Bool HashIndex_resize(
	HashIndex *index,
	const HashIndexInfo *info,
	const void *entries,
	U64 capacity,
	const Allocator *allocator,
	Error *e_rr
) {

	Bool s_uccess = true;
	Buffer slots = Buffer_createNull();
	HashIndex next = (HashIndex) { 0 };

	for (;;) {

		if(capacity > HashIndex_maxCapacity)
			retError(clean, Error_outOfBounds(
				3, capacity, HashIndex_maxCapacity, "HashIndex_resize()::capacity out of bounds"
			));

		gotoIfError3(clean, Buffer_createEmptyBytes(capacity * sizeof(HashSlot), allocator, &slots, e_rr));

		next = (HashIndex) { 0 };
		next.slots = (HashSlot*) slots.ptrNonConst;
		next.capacity = capacity;
		next.count = index->count;
		next.seed = index->capacity ? index->seed : HashIndex_processSeed;

		U64 maxDisplacement = 0;

		//Growth re-reads no key: the slot holds the whole hash. Only leaving small mode has to hash.

		if (index->capacity)
			for (U64 i = 0; i < index->capacity; ++i) {

				const HashSlot slot = index->slots[i];

				if(!slot.index)
					continue;

				const U64 displacement = HashIndex_place(
					&next, (U64) slot.hash & (capacity - 1), slot.hash, slot.index
				);

				if(maxDisplacement < displacement)
					maxDisplacement = displacement;
			}

		else for (U64 i = 0; i < index->count; ++i) {

			const U32 hash = HashIndex_hashKey(info, next.seed, HashIndex_keyOf(info, entries, i));
			const U64 displacement = HashIndex_place(&next, (U64) hash & (capacity - 1), hash, (U32)(i + 1));

			if(maxDisplacement < displacement)
				maxDisplacement = displacement;
		}

		if(maxDisplacement <= HashIndex_displacementCeiling(capacity))
			break;

		Buffer_free(&slots, allocator);
		next = (HashIndex) { 0 };

		if(!HashIndex_mayGrowOnBreach(index->count, capacity))
			retError(clean, Error_invalidState(0, "HashIndex_resize() key set exceeds the displacement ceiling"));

		capacity <<= 1;
	}

	HashIndex_free(index, allocator);
	*index = next;
	slots = Buffer_createNull();

clean:

	Buffer_free(&slots, allocator);
	return s_uccess;
}

U64 HashIndex_find(const HashIndex *index, const HashIndexInfo *info, const void *entries, const void *key) {

	if(!index || !info || !info->hash || !info->compare || !key)
		return U64_MAX;

	if (!index->capacity) {

		for(U64 i = 0; i < index->count; ++i)
			if(HashIndex_entryMatches(info, entries, i, key))
				return i;

		return U64_MAX;
	}

	const U64 mask = index->capacity - 1;
	const U32 hash = HashIndex_hashKey(info, index->seed, key);

	U64 pos = (U64) hash & mask;

	for (U64 displacement = 0; ; ++displacement, pos = (pos + 1) & mask) {

		const HashSlot slot = index->slots[pos];

		if(!slot.index || HashIndex_displacement(pos, slot.hash, mask) < displacement)
			return U64_MAX;

		//The stored hash rejects a wrong entry without touching the caller's data; the compare is what decides.

		if(slot.hash == hash && HashIndex_entryMatches(info, entries, slot.index - 1, key))
			return slot.index - 1;
	}
}

Bool HashIndex_reserve(
	HashIndex *index,
	const HashIndexInfo *info,
	const void *entries,
	U64 count,
	const Allocator *allocator,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!index || !info || !info->hash || !info->compare)
		retError(clean, Error_nullPointer(!index ? 0 : 1, "HashIndex_reserve()::index and info are required"));

	if(count > HashIndex_maxEntries)
		retError(clean, Error_outOfBounds(3, count, HashIndex_maxEntries, "HashIndex_reserve()::count out of bounds"));

	//Small mode holds no slots at all, so there is nothing to make room for.

	if(!index->capacity && count <= info->smallLimit)
		goto clean;

	if(index->capacity && count * HashIndex_maxLoadDenominator <= index->capacity * HashIndex_maxLoadNumerator)
		goto clean;

	gotoIfError3(clean, HashIndex_resize(index, info, entries, HashIndex_capacityFor(count), allocator, e_rr));

clean:
	return s_uccess;
}

Bool HashIndex_insert(
	HashIndex *index,
	const HashIndexInfo *info,
	const void *entries,
	const void *key,
	U64 entryIndex,
	U64 *existing,
	const Allocator *allocator,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(existing)
		*existing = U64_MAX;

	if(!index || !info || !info->hash || !info->compare || !key)
		retError(clean, Error_nullPointer(
			!index ? 0 : (!info ? 1 : 3), "HashIndex_insert()::index, info and key are required"
		));

	//Anything but the entry the list is about to push means the two have drifted apart, which is worth
	// catching here rather than at the lookup that silently returns the wrong entry later.

	if(entryIndex != index->count)
		retError(clean, Error_invalidParameter(4, 0, "HashIndex_insert()::entryIndex has to be the current count"));

	if(index->count >= HashIndex_maxEntries)
		retError(clean, Error_outOfBounds(4, index->count, HashIndex_maxEntries, "HashIndex_insert() index is full"));

	//Small mode: a linear scan, with no slots until it outgrows the limit.

	if (!index->capacity) {

		for(U64 i = 0; i < index->count; ++i)
			if (HashIndex_entryMatches(info, entries, i, key)) {

				if(existing)
					*existing = i;

				goto clean;
			}

		if (index->count + 1 <= info->smallLimit) {
			++index->count;
			goto clean;
		}

		gotoIfError3(clean, HashIndex_resize(
			index, info, entries, HashIndex_capacityFor(index->count + 1), allocator, e_rr
		));
	}

	//Checked before anything walks, which is what guarantees the loops below terminate.

	else if((index->count + 1) * HashIndex_maxLoadDenominator > index->capacity * HashIndex_maxLoadNumerator)
		gotoIfError3(clean, HashIndex_resize(index, info, entries, index->capacity << 1, allocator, e_rr));

	{
		const U64 mask = index->capacity - 1;
		const U32 hash = HashIndex_hashKey(info, index->seed, key);

		U64 pos = (U64) hash & mask;

		for (U64 displacement = 0; ; ++displacement, pos = (pos + 1) & mask) {

			const HashSlot slot = index->slots[pos];

			//Both prove absence, and are exactly where the new entry claims the slot.

			if(!slot.index || HashIndex_displacement(pos, slot.hash, mask) < displacement)
				break;

			if (slot.hash == hash && HashIndex_entryMatches(info, entries, slot.index - 1, key)) {

				if(existing)
					*existing = slot.index - 1;

				goto clean;
			}
		}

		const U64 maxDisplacement = HashIndex_place(index, pos, hash, (U32)(entryIndex + 1));
		++index->count;

		//A run past the ceiling is the flooding case rather than a tuning miss, so it grows even though the
		// load is fine. Undoing keeps this all or nothing once that growth is refused.

		if (maxDisplacement > HashIndex_displacementCeiling(index->capacity)) {

			Bool grown = false;

			if(HashIndex_mayGrowOnBreach(index->count, index->capacity))
				grown = HashIndex_resize(index, info, entries, index->capacity << 1, allocator, e_rr);

			else if(e_rr)
				*e_rr = Error_invalidState(0, "HashIndex_insert() key set exceeds the displacement ceiling");

			if (!grown) {

				U64 slot = 0;

				if(HashIndex_findSlot(index, hash, entryIndex, &slot))
					HashIndex_removeAt(index, slot);

				--index->count;
				s_uccess = false;
				goto clean;
			}
		}
	}

clean:
	return s_uccess;
}

Bool HashIndex_eraseSwap(
	HashIndex *index,
	const HashIndexInfo *info,
	const void *entries,
	U64 entryIndex,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!index || !info || !info->hash || !info->compare)
		retError(clean, Error_nullPointer(!index ? 0 : 1, "HashIndex_eraseSwap()::index and info are required"));

	if(entryIndex >= index->count)
		retError(clean, Error_outOfBounds(3, entryIndex, index->count, "HashIndex_eraseSwap()::entryIndex out of bounds"));

	if (index->capacity) {

		const U64 last = index->count - 1;

		U64 hole = 0, tail = 0;

		//Both are located before either is touched, so a disagreement is reported with nothing half done.

		const U32 hash = HashIndex_hashKey(info, index->seed, HashIndex_keyOf(info, entries, entryIndex));

		if(!HashIndex_findSlot(index, hash, entryIndex, &hole))
			retError(clean, Error_invalidState(0, "HashIndex_eraseSwap() entry has no slot, index and list disagree"));

		if (entryIndex != last) {

			const U32 tailHash = HashIndex_hashKey(info, index->seed, HashIndex_keyOf(info, entries, last));

			if(!HashIndex_findSlot(index, tailHash, last, &tail))
				retError(clean, Error_invalidState(
					1, "HashIndex_eraseSwap() last entry has no slot, index and list disagree"
				));

			//Repointing moves no slot, so the removal below still finds its own where it was located.

			index->slots[tail].index = (U32)(entryIndex + 1);
		}

		HashIndex_removeAt(index, hole);
	}

	--index->count;

clean:
	return s_uccess;
}

Bool HashIndex_clear(HashIndex *index, Error *e_rr) {

	Bool s_uccess = true;

	if(!index)
		retError(clean, Error_nullPointer(0, "HashIndex_clear()::index is required"));

	if(index->capacity)
		gotoIfError3(clean, Buffer_unsetAllBits(
			Buffer_createRef(index->slots, index->capacity * sizeof(HashSlot)), e_rr
		));

	index->count = 0;

clean:
	return s_uccess;
}

void HashIndex_free(HashIndex *index, const Allocator *allocator) {

	if(!index)
		return;

	if (index->capacity) {
		Buffer slots = Buffer_createManagedPtr(index->slots, index->capacity * sizeof(HashSlot));
		Buffer_free(&slots, allocator);
	}

	*index = (HashIndex) { 0 };
}
