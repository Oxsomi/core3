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

//types/container/test/test_types_container_flood.h
//
//Keys that all finalize to one hash, shared by the HashIndex and HashSet tests.

#pragma once
#include "types/container/hash_index.h"

//The finalizer is a bijection, so a flood is solved for rather than searched for.

static inline U64 HashFlood_modInverse(U64 v) {

	U64 x = v;        //Correct to 3 bits for odd v, and Newton doubles that every step

	for(U8 i = 0; i < 5; ++i)
		x *= 2 - v * x;

	return x;
}

static inline U64 HashFlood_unfinalize(U64 out) {
	const U64 b = out ^ (out >> 32);
	const U64 a = b * HashFlood_modInverse(HashIndex_finalizeMultiplier);
	return a ^ (a >> 32);
}

//The key is the hash, so only the finalizer is inverted here; with the seed known, inverting fnv1a is as cheap.

static inline U64 HashFlood_identityHash(const void *key, U64 seed, void *context) {
	(void) seed; (void) context;
	return *(const U64*) key;
}
