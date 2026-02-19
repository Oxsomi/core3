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

#include "types/base/string.h"
#include "types/container/buffer.h"
#include "shared.h"

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
}
