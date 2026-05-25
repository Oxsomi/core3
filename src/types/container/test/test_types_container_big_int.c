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

//types/container/test/test_types_container_big_int.c

#include "test_types_container_shared.h"
#include "types/base/string_read_helper.h"
#include "types/container/string.h"
#include "types/container/big_int.h"
#include "types/container/u128.h"

static const U64 mulParams[][2] = {
	{ 0x0123456789ABCDEF, 0xFEDCBA9876543210 },
	{ 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF },
	{ 0x2ABA5DA1FDF6DB0F, 0xE1147CF49D662B78 },
	{ 0x04D7A36BB020207F, 0xAC03EB4D2257AED9 },
	{ 0xC9DB73597154808E, 0xCE841590CDB048A4 }
};

static const U64 mulResult[][2] = {
	{ 0x2236D88FE5618CF0, 0x0121FA00AD77D742 },
	{ 0x0000000000000001, 0xFFFFFFFFFFFFFFFE },
	{ 0xE9F1BC96FD7C3408, 0x259137B5CA1EDC29 },
	{ 0xAFD5326D0A7ADDA7, 0x0340F4C6AD1EF492 },
	{ 0x3191981675EA4AF8, 0xA2D6BCFAA1676325 }
};

static const U64 addResult[][2] = {
	{ 0x235A1DF76F0D5ADF, 0xFFFEB49923CC0952 },
	{ 0x0000000000000000, 0xFFFFFFFFFFFFFFFE },
	{ 0x14AC1A38FB730F17, 0x06A5B4AA678507A2 },
	{ 0xB4ACD5D8BA9AFE26, 0xAF44E013CF76A36B },
	{ 0xFB6D0B6FE73ECB86, 0x715AD28B6F17ABC9 }
};

static const C8 *stringified[] = {
	"0xFEDCBA98765432100123456789ABCDEF",
	("0b1111111111111111111111111111111111111111111111111111111111111111"
	   "1111111111111111111111111111111111111111111111111111111111111111"),
	"0o3410507636447263053360252722732077575555417",
	"0n2i0_jD8bUksGJNeskm821$",
	"274506787720133886812119851071477940366"
};

void Test_bigIntCmp(Test *t) {

	Test_setModule(t, "BigInt_cmp");

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
		BigInt aBig = { 0 }, bBig = { 0 };

		if (!BigInt_createRefConst((const U64*)&compares[i - 1], 2, &aBig, &t->err)) {
			Test_assert(t, "BigInt_createRefConst", false);
			continue;
		}

		if (!BigInt_createRefConst((const U64*)&compares[i], 2, &bBig, &t->err)) {
			Test_assert(t, "BigInt_createRefConst", false);
			continue;
		}

		Test_assert(t, "BigInt_cmp", BigInt_cmp(aBig, bBig) == ECompareResult_Lt);
		Test_assert(t, "BigInt_cmp", BigInt_cmp(bBig, aBig) == ECompareResult_Gt);
	}
}

void Test_bigIntString(Test *t) {

	Test_setModule(t, "BigInt string");

	CharString tmp = CharString_createNull();

	//createFromString

	for (U64 i = 0; i < sizeof(stringified) / sizeof(stringified[0]); ++i) {
		BigInt aBig = { 0 }, bBig = { 0 };

		const CharString text    = CharString_createRefCStrConst(stringified[i]);
		const BigIntCreate create = { .text = &text, .bitCount = 128, .alloc = t->alloc, .big = &aBig };

		if (!BigInt_createFromString(&create, &t->err)) {
			Test_assert(t, "BigInt_createFromString ref", false);
			continue;
		}

		if (!BigInt_createRefConst(&mulParams[i][0], 2, &bBig, &t->err)) {
			BigInt_free(&aBig, t->alloc);
			Test_assert(t, "BigInt_createFromString ref", false);
			continue;
		}

		Test_assert(t, "BigInt_createFromString", !BigInt_neq(aBig, bBig));
		BigInt_free(&aBig, t->alloc);
	}

	//toString

	for (U64 i = 0; i < sizeof(stringified) / sizeof(stringified[0]) && i < EIntEncoding_Count; ++i) {
		BigInt bBig = { 0 };

		if (!BigInt_createRefConst(&mulParams[i][0], 2, &bBig, &t->err)) {
			Test_assert(t, "BigInt_toString createRef", false);
			continue;
		}

		BigIntStringify spec = { .leadingZeros = false, .alloc = t->alloc, .result = &tmp };

		if (!BigInt_toString(&spec, (EIntEncoding)i, bBig, &t->err)) {
			Test_assert(t, "BigInt_toString", false);
			continue;
		}

		const CharString ref = CharString_createRefCStrConst(stringified[i]);
		Test_assert(t, "BigInt_toString", CharString_equalsStringSensitive(&ref, &tmp));
		CharString_free(&tmp, t->alloc);
	}
}

void Test_bigIntMul(Test *t) {

	Test_setModule(t, "BigInt_mul");

	for (U64 i = 0; i < sizeof(mulParams) / sizeof(mulParams[0]); ++i) {
		BigInt aBig = { 0 }, bBig = { 0 }, cBig = { 0 };
		U64 temp[4] = { mulParams[i][0], 0, mulParams[i][1], 0 };

		if (
			!BigInt_createRef(&temp[0], 2, &aBig, &t->err)         ||
			!BigInt_createRefConst(&temp[2], 2, &bBig, &t->err)    ||
			!BigInt_createRefConst(&mulResult[i][0], 2, &cBig, &t->err)
		) {
			Test_assert(t, "BigInt_mul createRef", false);
			continue;
		}

		Test_assert(t, "BigInt_mul",    BigInt_mul(&aBig, bBig, t->alloc, NULL));
		Test_assert(t, "BigInt_mul eq", !BigInt_neq(aBig, cBig));
	}
}

void Test_bigIntAdd(Test *t) {

	Test_setModule(t, "BigInt_add");

	for (U64 i = 0; i < sizeof(mulParams) / sizeof(mulParams[0]); ++i) {

		BigInt aBig = { 0 }, bBig = { 0 }, cBig = { 0 };
		U64 temp[2] = { mulParams[i][0], mulParams[i][1] };

		if (
			!BigInt_createRef(&temp[0], 2, &aBig, &t->err)                 ||
			!BigInt_createRefConst(&mulResult[i][0], 2, &bBig, &t->err)    ||
			!BigInt_createRefConst(&addResult[i][0], 2, &cBig, &t->err)
		) {
			Test_assert(t, "BigInt_add createRef", false);
			continue;
		}

		Test_assert(t, "BigInt_add", BigInt_add(&aBig, bBig));
		Test_assert(t, "BigInt_add", !BigInt_neq(aBig, cBig));
	}
}

void Test_bigIntSub(Test *t) {

	Test_setModule(t, "BigInt_sub");

	for (U64 i = 0; i < sizeof(mulParams) / sizeof(mulParams[0]); ++i) {
		BigInt aBig = { 0 }, bBig = { 0 }, cBig = { 0 };
		U64 temp[2] = { addResult[i][0], addResult[i][1] };

		if (
			!BigInt_createRef(&temp[0], 2, &cBig, &t->err)                 ||
			!BigInt_createRefConst(&mulParams[i][0], 2, &aBig, &t->err)    ||
			!BigInt_createRefConst(&mulResult[i][0], 2, &bBig, &t->err)
		) {
			Test_assert(t, "BigInt_sub createRef", false);
			continue;
		}

		// c - b == a
		Test_assert(t, "BigInt_sub", BigInt_sub(&cBig, bBig));
		Test_assert(t, "BigInt_sub", !BigInt_neq(aBig, cBig));

		// Restore c to addResult, then c - a == b
		temp[0] = addResult[i][0];
		temp[1] = addResult[i][1];

		Test_assert(t, "BigInt_sub", BigInt_sub(&cBig, aBig));
		Test_assert(t, "BigInt_sub", !BigInt_neq(bBig, cBig));
	}
}

void Test_bigIntLsh(Test *t) {

	Test_setModule(t, "BigInt_lsh");

	const U64 base[2] = { mulParams[1][0], mulParams[1][1] };

	for (U16 n = 1; n <= 127; ++n) {
		BigInt aBig = { 0 };
		U64 temp[2] = { base[0], base[1] };

		if (!BigInt_createRef(&temp[0], 2, &aBig, &t->err)) {
			Test_assert(t, "BigInt_lsh createRef", false);
			continue;
		}

		// Compute expected via U64 arithmetic
		U64 expLo, expHi;

		if (n < 64) {
			expLo = base[0] << n;
			expHi = (base[1] << n) | (base[0] >> (64 - n));
		}
		else {
			expLo = 0;
			expHi = base[0] << (n - 64);
		}

		BigInt expBig = { 0 };
		U64 expTemp[2] = { expLo, expHi };

		if (!BigInt_createRefConst(&expTemp[0], 2, &expBig, &t->err)) {
			Test_assert(t, "BigInt_lsh createRef", false);
			continue;
		}

		Test_assert(t, "BigInt_lsh",    BigInt_lsh(&aBig, n));
		Test_assert(t, "BigInt_lsh eq", !BigInt_neq(aBig, expBig));
	}
}

void Test_bigIntRsh(Test *t) {

	Test_setModule(t, "BigInt_rsh");

	const U64 base[2] = { mulParams[1][0], mulParams[1][1] };

	for (U16 n = 1; n <= 127; ++n) {
		BigInt aBig = { 0 };
		U64 temp[2] = { base[0], base[1] };

		if (!BigInt_createRef(&temp[0], 2, &aBig, &t->err)) {
			Test_assert(t, "BigInt_rsh createRef", false);
			continue;
		}

		// Compute expected via U64 arithmetic
		U64 expLo, expHi;

		if (n < 64) {
			expHi = base[1] >> n;
			expLo = (base[0] >> n) | (base[1] << (64 - n));
		}
		else {
			expHi = 0;
			expLo = base[1] >> (n - 64);
		}

		BigInt expBig = { 0 };
		U64 expTemp[2] = { expLo, expHi };

		if (!BigInt_createRefConst(&expTemp[0], 2, &expBig, &t->err)) {
			Test_assert(t, "BigInt_rsh createRef", false);
			continue;
		}

		Test_assert(t, "BigInt_rsh",    BigInt_rsh(&aBig, n));
		Test_assert(t, "BigInt_rsh eq", !BigInt_neq(aBig, expBig));
	}
}

void Test_bigIntBitScan(Test *t) {

	Test_setModule(t, "BigInt_bitScan");

	const U64 base[2] = { mulParams[1][0], mulParams[1][1] };

	for (U16 n = 1; n <= 127; ++n) {
		U64 expLo, expHi;

		if (n < 64) {
			expHi = base[1] >> n;
			expLo = (base[0] >> n) | (base[1] << (64 - n));
		}
		else {
			expHi = 0;
			expLo = base[1] >> (n - 64);
		}

		U64 expTemp[2] = { expLo, expHi };
		BigInt aBig    = { 0 };

		if (!BigInt_createRefConst(&expTemp[0], 2, &aBig, &t->err)) {
			Test_assert(t, "BigInt_bitScan createRef", false);
			continue;
		}

		const U16 got      = BigInt_bitScan(aBig);
		const U16 expected = (U16)(127 - n);
		Test_assert(t, "BigInt_bitScan", got == expected);
	}
}

void Test_bigInt(Test *t) {
	Test_bigIntCmp(t);
	Test_bigIntString(t);
	Test_bigIntMul(t);
	Test_bigIntAdd(t);
	Test_bigIntSub(t);
	Test_bigIntLsh(t);
	Test_bigIntRsh(t);
	Test_bigIntBitScan(t);
}
