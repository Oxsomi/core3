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

#include "types/base/string_read_helper.h"
#include "types/container/u128.h"
#include "types/container/string.h"
#include "all.h"

static const U64 mulParams[][2] = {
	{ 0x0123456789ABCDEF, 0xFEDCBA9876543210 },
	{ 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF },
	{ 0x2ABA5DA1FDF6DB0F, 0xE1147CF49D662B78 },
	{ 0x04D7A36BB020207F, 0xAC03EB4D2257AED9 },
	{ 0xC9DB73597154808E, 0xCE841590CDB048A4 }
};

static const C8 *stringified[] = {
	"0xFEDCBA98765432100123456789ABCDEF",
	("0b1111111111111111111111111111111111111111111111111111111111111111"
	   "1111111111111111111111111111111111111111111111111111111111111111"),
	"0o3410507636447263053360252722732077575555417",
	"0n2i0_jD8bUksGJNeskm821$",
	"274506787720133886812119851071477940366"
};

void Test_u128Cmp(Test *t) {

	Test_setModule(t, "U128_cmp");

	const U128 compares[] = {
		U128_createU64x2(0x0000000000000000, 0x0000000000000000),
		U128_createU64x2(0x0000000000000001, 0x0000000000000000),
		U128_createU64x2(0x000000007FFFFFFF, 0x0000000000000000),
		U128_createU64x2(0x0000000080000000, 0x0000000000000000),
		U128_createU64x2(0x0000000080000001, 0x0000000000000000),
		U128_createU64x2(0x00000000FFFFFFFF, 0x0000000000000000),
		U128_createU64x2(0x00000001FFFFFFFF, 0x0000000000000000),
		U128_createU64x2(0x00000001FFFFFFFF, 0x0000000000000001),
		U128_createU64x2(0x00000001FFFFFFFF, 0x00000000FFFFFFFF),
		U128_createU64x2(0x00000001FFFFFFFF, 0x00000001FFFFFFFF),
		U128_createU64x2(0x00000001FFFFFFFF, 0xFFFFFFFFFFFFFFFF)
	};

	for (U64 i = 1; i < sizeof(compares) / sizeof(compares[0]); ++i) {
		Test_assert(t, "U128_cmp Lt", U128_cmp(compares[i - 1], compares[i]) == ECompareResult_Lt);
		Test_assert(t, "U128_cmp Gt", U128_cmp(compares[i], compares[i - 1]) == ECompareResult_Gt);
	}
}

void Test_u128String(Test *t) {

	Test_setModule(t, "U128 string");

	CharString tmp = CharString_createNull();

	//createFromString

	for (U64 i = 0; i < sizeof(stringified) / sizeof(stringified[0]); ++i) {
		const U128 got      = U128_createFromString(CharString_createRefCStrConst(stringified[i]), t->alloc, &t->err);
		const U128 expected = U128_create((const U8*) &mulParams[i][0]);
		Test_assert(t, "createFromString",       !U128_neq(got, expected));
	}

	//toString

	for (U64 i = 0; i < sizeof(stringified) / sizeof(stringified[0]) && i < EIntEncoding_Count; ++i) {

		const CharString str        = CharString_createRefCStrConst(stringified[i]);
		const U128 val              = U128_create((const U8*) &mulParams[i][0]);
		const BigIntStringify spec  = { .leadingZeros = false, .alloc = t->alloc, .result = &tmp };

		Test_assert(t, "U128_toString",    U128_toString(&spec, (EIntEncoding)i, val, &t->err));
		Test_assert(t, "U128_toString eq", CharString_equalsStringSensitive(&str, &tmp));
		CharString_free(&tmp, t->alloc);
	}
}

void Test_u128BitScan(Test *t) {

	Test_setModule(t, "U128_bitScan");

	const U128 allOnes = U128_createU64x2(U64_MAX, U64_MAX);

	for (U8 n = 1; n <= 127; ++n) {
		const U128 shifted  = U128_rsh(allOnes, n);
		const U16  got      = U128_bitScan(shifted);
		const U16  expected = (U16)(127 - n);
		Test_assert(t, "U128_bitScan", got == expected);
	}
}

void Test_u128(Test *t) {
	Test_u128Cmp(t);
	Test_u128String(t);
	Test_u128BitScan(t);
}
