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
	(
		"0b1111111111111111111111111111111111111111111111111111111111111111"
		"1111111111111111111111111111111111111111111111111111111111111111"
	),
	"0o3410507636447263053360252722732077575555417",
	"0n2i0_jD8bUksGJNeskm821$",
	"274506787720133886812119851071477940366"
};

void Test_bigIntCmp(Test *t) {

	Test_setModule(t, "BigInt_cmp");

	//Ascending values as BigInt limbs, least significant first, which is the order BigInt itself uses.
	//These were U128s handed to BigInt_createRefConst through a (const U64*) cast, but on every non-Windows
	// target U128 is __uint128_t, and reading that object through a U64 lvalue is an aliasing violation.
	//At -O0 nothing acted on it; a Release build read stale limbs instead, so every comparison that had to
	// fall through to the low limb returned the wrong answer.
	//The limbs are the actual subject here, so there is nothing to convert in the first place.

	static const U64 compares[][2] = {
		{ 0x0000000000000000, 0x0000000000000000 },
		{ 0x0000000000000001, 0x0000000000000000 },
		{ 0x000000007FFFFFFF, 0x0000000000000000 },
		{ 0x0000000080000000, 0x0000000000000000 },
		{ 0x0000000080000001, 0x0000000000000000 },
		{ 0x00000000FFFFFFFF, 0x0000000000000000 },
		{ 0x00000001FFFFFFFF, 0x0000000000000000 },
		{ 0x00000001FFFFFFFF, 0x0000000000000001 },
		{ 0x00000001FFFFFFFF, 0x00000000FFFFFFFF },
		{ 0x00000001FFFFFFFF, 0x00000001FFFFFFFF },
		{ 0x00000001FFFFFFFF, 0xFFFFFFFFFFFFFFFF }
	};

	for (U64 i = 1; i < sizeof(compares) / sizeof(compares[0]); ++i) {
		BigInt aBig = { 0 }, bBig = { 0 };

		if (!BigInt_createRefConst(compares[i - 1], 2, &aBig, &t->err)) {
			Test_assert(t, "BigInt_createRefConst", false);
			continue;
		}

		if (!BigInt_createRefConst(compares[i], 2, &bBig, &t->err)) {
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
		const U16 expected = (U16) (127 - n);
		Test_assert(t, "BigInt_bitScan", got == expected);
	}

	//The lowest set bit, with every bit above it set too so it isn't found by accident.

	for (U16 p = 0; p < 128; ++p) {

		U64 limbs[2] = { 0, 0 };

		for (U16 b = p; b < 128; ++b)
			limbs[b >> 6] |= (U64) 1 << (b & 63);

		BigInt aBig = { 0 };

		if (!BigInt_createRefConst(&limbs[0], 2, &aBig, &t->err)) {
			Test_assert(t, "BigInt_bitScanReverse createRef", false);
			continue;
		}

		Test_assert(t, "BigInt_bitScanReverse", BigInt_bitScanReverse(aBig) == p);
	}

	const U64 zero[2] = { 0, 0 };
	BigInt zeroBig = { 0 };

	if (Test_assert(t, "BigInt zero createRef", BigInt_createRefConst(&zero[0], 2, &zeroBig, &t->err))) {
		Test_assert(t, "BigInt_bitScan zero", BigInt_bitScan(zeroBig) == U16_MAX);
		Test_assert(t, "BigInt_bitScanReverse zero", BigInt_bitScanReverse(zeroBig) == U16_MAX);
	}
}

static void Test_bigIntBoundaries(Test *t) {

	Test_setModule(t, "BigInt boundaries");

	U64 limbs[] = { U64_MAX, U64_MAX, 0 }, one = 1;
	BigInt value = { 0 }, increment = { 0 }, copy = { 0 };

	if (!Test_assert(
		t, "create arithmetic refs",
		BigInt_createRef(limbs, 3, &value, &t->err) && BigInt_createRef(&one, 1, &increment, &t->err)
	))
		return;

	Test_assert(
		t, "carry propagates across extra limbs",
		BigInt_add(&value, increment) && !limbs[0] && !limbs[1] && limbs[2] == 1
	);

	limbs[0] = limbs[1] = 0;
	limbs[2] = 1;

	Test_assert(
		t, "borrow propagates across extra limbs",
		BigInt_sub(&value, increment) && limbs[0] == U64_MAX && limbs[1] == U64_MAX && !limbs[2]
	);

	if (Test_assert(t, "copy succeeds", BigInt_createCopy(&value, t->alloc, &copy, &t->err))) {
		Test_assert(t, "copy preserves source", limbs[0] == U64_MAX && limbs[1] == U64_MAX && !limbs[2]);
		Test_assert(t, "copy preserves value", copy.data[0] == U64_MAX && copy.data[1] == U64_MAX);
	}

	BigInt_free(&copy, t->alloc);

	const C8 *decimals[] = { "10", "255", "1000000", "18446744073709551615", "18446744073709551616" };
	const U64 expected[] = { 10, 255, 1000000, U64_MAX };

	for(U64 i = 0; i < sizeof(decimals) / sizeof(decimals[0]); ++i) {

		const CharString text = CharString_createRefCStrConst(decimals[i]);
		BigInt parsed = { 0 };
		const BigIntCreate create = { .text = &text, .bitCount = i < 3 ? 0 : 64, .alloc = t->alloc, .big = &parsed };
		const Bool ok = BigInt_createFromDec(&create, NULL);

		Test_assert(t, "decimal width and overflow", ok == (i < 4));

		if(ok && i < 4)
			Test_assert(t, "decimal value", parsed.data[0] == expected[i]);

		BigInt_free(&parsed, t->alloc);
	}

	for(U64 bit = 0; bit < 192; ++bit) {

		limbs[0] = limbs[1] = limbs[2] = 0;
		limbs[bit >> 6] = (U64) 1 << (bit & 63);

		for(EIntEncoding encoding = EIntEncoding_Oct; encoding <= EIntEncoding_Nyto; ++encoding) {

			CharString text = CharString_createNull();
			BigInt parsed = { 0 };
			const BigIntStringify stringify = { .alloc = t->alloc, .result = &text };
			const BigIntCreate create = { .text = &text, .bitCount = 192, .alloc = t->alloc, .big = &parsed };

			if (Test_assert(t, "format bit across limbs", BigInt_toString(&stringify, encoding, value, &t->err))) {

				if (Test_assert(t, "parse formatted bit", BigInt_createFromBase2Type(&create, encoding, &t->err)))
					Test_assert(t, "bit round trip", !BigInt_neq(value, parsed));
			}

			BigInt_free(&parsed, t->alloc);
			CharString_free(&text, t->alloc);
		}
	}

	limbs[0] = limbs[1] = limbs[2] = 0;

	CharString zeroText = CharString_createNull();
	const CharString expectedZero = CharString_createRefCStrConst("0x0");
	const BigIntStringify stringify = { .alloc = t->alloc, .result = &zeroText };

	Test_assert(
		t, "zero formats as one digit",
		BigInt_toString(&stringify, EIntEncoding_Hex, value, &t->err) &&
		CharString_equalsStringSensitive(&zeroText, &expectedZero)
	);

	CharString_free(&zeroText, t->alloc);
	limbs[0] = 0xFFF;

	Test_assert(t, "format reused string first value", BigInt_toString(&stringify, EIntEncoding_Hex, value, &t->err));

	limbs[0] = 0x100;

	const CharString reusedExpected = CharString_createRefCStrConst("0x100");

	Test_assert(
		t, "format reused string clears previous digits",
		BigInt_toString(&stringify, EIntEncoding_Hex, value, &t->err) &&
		CharString_equalsStringSensitive(&zeroText, &reusedExpected)
	);

	CharString_free(&zeroText, t->alloc);

	const CharString max128 = CharString_createRefCStrConst("340282366920938463463374607431768211455");
	const CharString overflow128 = CharString_createRefCStrConst("340282366920938463463374607431768211456");
	U128 parsed128 = U128_createFromDec(max128, t->alloc, &t->err);

	Test_assert(t, "U128 maximum decimal", U128_lo(parsed128) == U64_MAX && U128_hi(parsed128) == U64_MAX);

	Error overflowError = Error_none();

	parsed128 = U128_createFromDec(overflow128, t->alloc, &overflowError);

	Test_assert(t, "U128 decimal overflow", overflowError.genericError && !U128_lo(parsed128) && !U128_hi(parsed128));
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
	Test_bigIntBoundaries(t);
}
