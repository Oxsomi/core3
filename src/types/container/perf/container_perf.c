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

//types/container/perf/container_perf.c

#include "types/base/time.h"
#include "types/base/mathi.h"
#include "types/container/perf/container_perf.h"
#include "types/container/buffer_encrypt.h"
#include "types/container/buffer.h"
#include "types/container/hash_index.h"
#include "types/container/list_basic_types.h"
#include "types/container/string.h"
#include "types/container/log.h"
#include "types/math/rand.h"

#include <stdio.h>

static const U32 sizes[] = { 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536 };
static const U8  cryptoState[] = { 0, 1, 3 };   // Test non 256-bit, 512-bit, etc.
static const I64 blockSizeHints[] = { -1, -2, -4, -8, -16, 0 };

Bool Perf_aesThroughput(const Allocator *alloc, const CharString *outputCsv, Bool logToConsole, Error *e_rr) {

	Bool s_uccess = true;

	const AESEncryptionKey key = { 0 };

	Buffer     full   = Buffer_createNull();
	CharString csv    = CharString_createNull();
	CharString tmpStr = CharString_createNull();

	gotoIfError3(clean, Buffer_createUninitializedBytes(1 * GIBI, alloc, &full, e_rr));
	Buffer_csprng(full);

	gotoIfError3(clean, CharString_format(
		alloc, &csv, e_rr,
		"%s,%s,%s,%s,%s,%s,%s\n",
		"Type", "Crypto state", "Batch size", "Total size", "Count", "Seconds", "GiB/s"
	));

	for (U64 m = 0; m < 2; ++m)
		for (U64 l = 0; l < sizeof(cryptoState) / sizeof(cryptoState[0]); ++l) {

			AESEncryptionContext ctx = { 0 };
			U8 blockSizeMax = 0, use256Or512 = 0;

			//Probe support for cpu support
			gotoIfError3(clean, Buffer_aesExpertCreate(
				I32x4_zero(), EBufferEncryptionType_AES256GCM,
				key, -16, 0, cryptoState[l],
				&blockSizeMax, &use256Or512, &ctx, e_rr
			));

			if (use256Or512 != cryptoState[l])   //Unsupported on this hardware
				continue;

			for (U64 k = 0; k < sizeof(blockSizeHints) / sizeof(blockSizeHints[0]); ++k) {

				Bool printedHeader = false;

				for (U64 i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {

					const U64 siz   = sizes[i];
					const U64 elems = Buffer_length(full) / siz;

					I64 blockSizeHint = blockSizeHints[k];
					if (!blockSizeHint)
						blockSizeHint = (I64)siz;

					gotoIfError3(clean, Buffer_aesExpertCreate(
						I32x4_zero(), EBufferEncryptionType_AES256GCM,
						key, blockSizeHint, 0, cryptoState[l],
						&blockSizeMax, &use256Or512, &ctx, e_rr
					));

					if (blockSizeHints[k] && blockSizeMax < (U8)(U8)-blockSizeHints[k])
						break;   // This block size is not supported

					if (logToConsole && !printedHeader) {
						Log_debugLn(
							alloc,
							"Starting profile for AES256GCM blockSize = %"PRIu64" (crypto: %"PRIx8", %s)",
							(U64)-blockSizeHints[k],
							cryptoState[l],
							m ? "Instant encrypt" : "Streaming"
						);
						printedHeader = true;
					}

					U64 count = 0;
					Ns  curr  = Time_now();

					while (true) {

						for (U64 j = 0; j < 256; ++j) {

							if (m)
								gotoIfError3(clean, Buffer_aesExpertCreate(
									I32x4_zero(), EBufferEncryptionType_AES256GCM,
									key, blockSizeHint, siz, cryptoState[l],
									&blockSizeMax, &use256Or512, &ctx, e_rr
								));

							Buffer dat = Buffer_createRef(
								full.ptrNonConst + ((count + j) % elems) * siz, siz
							);
							Buffer_aesExpertEncUpdate(&ctx, dat, 0, blockSizeMax, use256Or512);

							if (m)
								Buffer_aesExpertFinalize(&ctx, 0, siz, I32x4_zero());
						}

						count += 256;

						if (Time_elapsed(curr) >= (DNs)(SECOND * 3))
							break;
					}

					if (!m)
						Buffer_aesExpertFinalize(&ctx, 0, count * siz, I32x4_zero());

					{
						DNs diff = Time_elapsed(curr);

						if (logToConsole)
							Log_debugLn(
								alloc,
								"AES-256-GCM ops for 3s on %"PRIu64" byte blocks: %"PRIu64" ops in %fs (%f GiB/s)",
								siz, count,
								(F64)diff / SECOND,
								count * siz / ((F64)diff / SECOND) / GIBI
							);

						gotoIfError3(clean, CharString_format(
							alloc, &tmpStr, e_rr,
							"%s%s,%"PRIx8",%"PRIu8",%"PRIu64",%"PRIu64",%f,%f\n",
							csv.ptr ? csv.ptr : "",
							!m ? "Streaming" : "Instant",
							cryptoState[l],
							blockSizeMax,
							siz, count,
							(F64)diff / SECOND,
							count * siz / ((F64)diff / SECOND) / GIBI
						));

						CharString_free(&csv, alloc);
						csv    = tmpStr;
						tmpStr = CharString_createNull();
					}
				}
			}
		}

	if (outputCsv) {

		FILE *f = fopen(outputCsv->ptr, "wb");

		if (!f)
			retError(clean, Error_invalidState(0, "Perf_aesThroughput(): fopen failed"));

		fwrite(csv.ptr, 1, CharString_length(csv), f);
		fclose(f);
	}

clean:
	CharString_free(&csv,    alloc);
	CharString_free(&tmpStr, alloc);
	Buffer_free(&full, alloc);
	return s_uccess;
}

//Sizes a consumer actually sees, from a push descriptor table up to a corner dedup over a big mesh.
//The last is 3/4 of 2^24 slots, where the displacement ceiling is checked against reality.

static const U64 hashIndexCounts[] = {
	16, 64, 256, 1024, 4096, 16384, 65536, 262144, 1048576, 4194304, 12582912
};

//Random_sample advances the LCG and reports [0, 1>; the word behind it is what a key wants.
//Its period is 2^32 and every count above stays well inside it, so the keys are distinct.

static U32 Perf_hashIndexRandom(U32 *seed) {
	Random_sample(seed);
	return *seed;
}

static U64 Perf_hashIndexMaxDisplacement(const HashIndex *index) {

	U64 maxDisplacement = 0;

	for (U64 i = 0; i < index->capacity; ++i) {

		if(!index->slots[i].index)
			continue;

		const U64 home = index->slots[i].hash & (index->capacity - 1);
		const U64 displacement = (i - home) & (index->capacity - 1);

		if(maxDisplacement < displacement)
			maxDisplacement = displacement;
	}

	return maxDisplacement;
}

//HashIndex against the linear scan a consumer would otherwise write, plus the number the design rests on:
// the greatest displacement Robin Hood leaves behind, since the ceiling that bounds a flood is only worth
// having while honest keys stay far below it at every size.

Bool Perf_hashIndex(const Allocator *alloc, const CharString *outputCsv, Bool logToConsole, Error *e_rr) {

	Bool s_uccess = true;

	CharString csv    = CharString_createNull();
	CharString tmpStr = CharString_createNull();

	ListU64 keys  = (ListU64) { 0 };
	ListU64 list  = (ListU64) { 0 };
	HashIndex index = (HashIndex) { 0 };

	HashIndexInfo info = (HashIndexInfo) { 0 };
	info.hash = U64_hash;
	info.compare = U64_compare;
	info.stride = sizeof(U64);
	info.smallLimit = 32;

	gotoIfError3(clean, CharString_createCopy(
		CharString_createRefCStrConst("Structure,Count,Capacity,Operation,Seconds,OpsPerSecond,MaxDisplacement\n"),
		alloc, &csv, e_rr
	));

	for (U64 s = 0; s < sizeof(hashIndexCounts) / sizeof(hashIndexCounts[0]); ++s) {

		const U64 count = hashIndexCounts[s];
		U32 seed = Random_seed((U32) count, 0x0C3A5417);

		ListU64_free(&keys, alloc);
		gotoIfError3(clean, ListU64_resize(&keys, count * 2, alloc, e_rr));

		//The first half is what goes in, the second half is what is looked up and never found.

		for(U64 i = 0; i < count * 2; ++i)
			keys.ptrNonConst[i] = ((U64) Perf_hashIndexRandom(&seed) << 32) | Perf_hashIndexRandom(&seed);

		//Both reserves hoisted, as every hot path this exists for does.

		HashIndex_free(&index, alloc);
		ListU64_free(&list, alloc);

		gotoIfError3(clean, HashIndex_reserve(&index, &info, list.ptr, count, alloc, e_rr));
		gotoIfError3(clean, ListU64_reserve(&list, count, alloc, e_rr));

		Ns start = Time_now();

		for (U64 i = 0; i < count; ++i) {

			U64 existing = 0;
			gotoIfError3(clean, HashIndex_insert(&index, &info, list.ptr, &keys.ptr[i], list.length, &existing, alloc, e_rr));

			if(existing == U64_MAX)
				gotoIfError3(clean, ListU64_pushBack(&list, keys.ptr[i], alloc, e_rr));
		}

		DNs build = Time_elapsed(start);

		//Misses are what a filling dedup table is mostly made of, and what the early exit is there for.

		U64 found = 0;
		start = Time_now();

		for(U64 i = 0; i < count; ++i)
			found += HashIndex_find(&index, &info, list.ptr, &keys.ptr[i]) != U64_MAX;

		DNs hit = Time_elapsed(start);

		U64 missed = 0;
		start = Time_now();

		for(U64 i = 0; i < count; ++i)
			missed += HashIndex_find(&index, &info, list.ptr, &keys.ptr[count + i]) == U64_MAX;

		DNs miss = Time_elapsed(start);

		if(found != count || missed != count)
			retError(clean, Error_invalidState(0, "Perf_hashIndex() lookup disagreed with what was inserted"));

		const U64 capacity = index.capacity;
		const U64 maxDisplacement = Perf_hashIndexMaxDisplacement(&index);
		const U64 ceiling = HashIndex_displacementCeiling(capacity);

		//Twice the observed maximum is the margin the design claims, so failing here is a real result.

		if(capacity && maxDisplacement * 2 >= ceiling)
			retError(clean, Error_invalidState(
				1, "Perf_hashIndex() random keys came within half the displacement ceiling"
			));

		//From the front, so every erase is the swap-and-repoint case rather than a pop.

		start = Time_now();

		for (U64 i = 0; i < count; ++i) {
			gotoIfError3(clean, HashIndex_eraseSwap(&index, &info, list.ptr, 0, e_rr));
			list.ptrNonConst[0] = list.ptr[list.length - 1];
			gotoIfError3(clean, ListU64_popBack(&list, NULL, e_rr));
		}

		DNs erase = Time_elapsed(start);

		//The linear scan, over the same misses. Quadratic, so it stops where it stops being honest to run.

		const Bool scanned = count <= 16384;
		DNs scan = 0;

		if (scanned) {

			U64 hits = 0;
			start = Time_now();

			for(U64 i = 0; i < count; ++i)
				for(U64 j = 0; j < count; ++j)
					if (keys.ptr[j] == keys.ptr[count + i]) {
						++hits;
						break;
					}

			scan = Time_elapsed(start);

			//Checked rather than discarded: a result nothing reads is a loop the compiler deletes.

			if(hits)
				retError(clean, Error_invalidState(3, "Perf_hashIndex() the absent keys were not absent"));
		}

		if (logToConsole)
			Log_debugLn(
				alloc,
				"HashIndex %"PRIu64" keys (capacity %"PRIu64", load %.2f, max displacement %"PRIu64" of %"PRIu64
				"): build %fs, hit %fs, miss %fs, erase %fs",
				count, capacity, capacity ? (F64) count / capacity : 0.0, maxDisplacement, ceiling,
				(F64) build / SECOND, (F64) hit / SECOND, (F64) miss / SECOND, (F64) erase / SECOND
			);

		static const C8 *names[] = { "build", "find hit", "find miss", "eraseSwap", "linear scan miss" };
		const DNs timings[] = { build, hit, miss, erase, scan };

		for (U64 o = 0; o < sizeof(timings) / sizeof(timings[0]); ++o) {

			if(o == 4 && !scanned)
				continue;

			gotoIfError3(clean, CharString_format(
				alloc, &tmpStr, e_rr,
				"%s,%"PRIu64",%"PRIu64",%s,%f,%f,%"PRIu64"\n",
				o == 4 ? "ListU64 scan" : "HashIndex",
				count, capacity, names[o],
				(F64) timings[o] / SECOND,
				count / ((F64) timings[o] / SECOND),
				maxDisplacement
			));

			gotoIfError3(clean, CharString_appendString(&csv, &tmpStr, alloc, e_rr));
			CharString_free(&tmpStr, alloc);
		}
	}

	if (outputCsv) {

		FILE *f = fopen(outputCsv->ptr, "wb");

		if (!f)
			retError(clean, Error_invalidState(2, "Perf_hashIndex(): fopen failed"));

		fwrite(csv.ptr, 1, CharString_length(csv), f);
		fclose(f);
	}

clean:
	HashIndex_free(&index, alloc);
	ListU64_free(&list, alloc);
	ListU64_free(&keys, alloc);
	CharString_free(&csv,    alloc);
	CharString_free(&tmpStr, alloc);
	return s_uccess;
}
