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

#include "types/base/time.h"
#include "types/container/perf/container_perf.h"
#include "types/container/buffer_encrypt.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/container/log.h"

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
