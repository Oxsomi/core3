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

//types/container/test/test_types_container_crc32c.c

#include "test_types_container_shared.h"
#include "types/base/string_base.h"
#include "types/container/buffer.h"

void Test_crc32c(Test *test) {

	Test_setModule(test, "CRC32C");

	//v is stored in little-endian byte order and cast directly to U32

	typedef struct { const C8 *str; const C8 v[4]; } CRC32CCase;

	static const CRC32CCase cases[] = {
		{ "",                                                                                 { 0x00, 0x00, 0x00, 0x00 } },
		{ "a",                                                                                { 0x30, 0x43, 0xD0, 0xC1 } },
		{ "abc",                                                                              { 0xB7, 0x3F, 0x4B, 0x36 } },
		{ "message digest",                                                                   { 0xD0, 0x79, 0xBD, 0x02 } },
		{ "abcdefghijklmnopqrstuvwxyz",                                                       { 0x25, 0xEF, 0xE6, 0x9E } },
		{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",                   { 0x7D, 0xD5, 0x45, 0xA2 } },
		{ "12345678901234567890123456789012345678901234567890123456789012345678901234567890", { 0x81, 0x67, 0x7A, 0x47 } },
		{ "123456789",                                                                        { 0x83, 0x92, 0x06, 0xE3 } }
	};

	for (U64 i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
		const Buffer buf = Buffer_createRefConst(cases[i].str, CharString_calcStrLen(cases[i].str, U64_MAX));
		const U32 want = *(const U32 *)cases[i].v;
		const U32 got = Buffer_crc32c(buf);
		Test_assert(test, cases[i].str, got == want);
	}

	//Every misaligned base has to produce the same CRC as the identical bytes hashed from an aligned base.
	//The hardware path peels up to seven bytes to reach 8 byte alignment, and the peel used to read its U32
	// straight off an odd address, which sanitizers reject as a misaligned load (the CLI hashes argv-backed
	// string refs, so arbitrary alignment is a real input, not a contrived one).
	//Sweeping every length also covers a buffer that ends before alignment is ever reached, the plain U64
	// loop and each tail width.

	Test_setModule(test, "CRC32C misaligned");

	C8 pattern[72];

	for (U64 i = 0; i < sizeof(pattern); ++i)
		pattern[i] = (C8)(i * 37 + 11);

	U64 alignedStorage[9];
	C8 *alignedC8 = (C8*) alignedStorage;

	static const C8 *offsetNames[8] = {
		"offset 0", "offset 1", "offset 2", "offset 3", "offset 4", "offset 5", "offset 6", "offset 7"
	};

	for (U64 off = 0; off < 8; ++off) {

		Bool allMatch = true;

		for (U64 len = 0; len + off <= sizeof(pattern); ++len) {

			Buffer_memcpy(Buffer_createRef(alignedC8, len), Buffer_createRefConst(pattern + off, len));

			const U32 misaligned = Buffer_crc32c(Buffer_createRefConst(pattern + off, len));
			const U32 aligned = Buffer_crc32c(Buffer_createRefConst(alignedC8, len));

			allMatch &= misaligned == aligned;
		}

		Test_assert(test, offsetNames[off], allMatch);
	}
}
