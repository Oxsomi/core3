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

#include "types/math/u128.h"
#include "shared.h"

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

void Test_u128Mul(Test *test) {

	Test_setModule(test, "U128_mul64");

	for (U64 i = 0; i < sizeof(mulParams) / sizeof(mulParams[0]); ++i) {
		const U128 expected = U128_createU64x2(mulResult[i][0], mulResult[i][1]);
		const U128 got = U128_mul64(mulParams[i][0], mulParams[i][1]);
		Test_assert(test, "U128_mul64", !U128_neq(expected, got));
	}
}

void Test_u128Add(Test *test) {

	Test_setModule(test, "U128_add");

	for (U64 i = 0; i < sizeof(mulParams) / sizeof(mulParams[0]); ++i) {
		const U128 a = U128_createU64x2(mulParams[i][0], mulParams[i][1]);
		const U128 b = U128_createU64x2(mulResult[i][0], mulResult[i][1]);
		const U128 expected = U128_createU64x2(addResult[i][0], addResult[i][1]);
		Test_assert(test, "U128_add", !U128_neq(U128_add(a, b), expected));
	}
}

void Test_u128Sub(Test *test) {

	Test_setModule(test, "U128_sub");

	for (U64 i = 0; i < sizeof(mulParams) / sizeof(mulParams[0]); ++i) {

		const U128 a = U128_createU64x2(mulParams[i][0], mulParams[i][1]);
		const U128 b = U128_createU64x2(mulResult[i][0], mulResult[i][1]);
		const U128 c = U128_createU64x2(addResult[i][0], addResult[i][1]);

		//c - b == a  and  c - a == b

		Test_assert(test, "U128_sub (c - b)", !U128_neq(U128_sub(c, b), a));
		Test_assert(test, "U128_sub (c - a)", !U128_neq(U128_sub(c, a), b));
	}
}

void Test_u128Lsh(Test *test) {

	Test_setModule(test, "U128_lsh");

	const U128 allOnes = U128_createU64x2(U64_MAX, U64_MAX);


	for (U8 n = 1; n <= 127; ++n) {

		const U128 got = U128_lsh(allOnes, n);

		U64 lo, hi;

		if (n < 64) {
			lo = U64_MAX << n;
			hi = U64_MAX;
		}
		else {
			lo = 0;
			hi = U64_MAX << (n - 64);
		}

		const U128 expected = U128_createU64x2(lo, hi);
		Test_assert(test, "U128_lsh", !U128_neq(got, expected));
	}
}

void Test_u128Rsh(Test *test) {

	Test_setModule(test, "U128_rsh");

	const U128 allOnes = U128_createU64x2(U64_MAX, U64_MAX);

	for (U8 n = 1; n <= 127; ++n) {
		const U128 got = U128_rsh(allOnes, n);

		U64 lo, hi;

		if (n < 64) {
			lo = U64_MAX;
			hi = U64_MAX >> n;
		}
		else {
			lo = U64_MAX >> (n - 64);
			hi = 0;
		}

		const U128 expected = U128_createU64x2(lo, hi);
		Test_assert(test, "U128_rsh", !U128_neq(got, expected));
	}
}

void Test_u128(Test *test) {
	Test_u128Mul(test);
	Test_u128Add(test);
	Test_u128Sub(test);
	Test_u128Lsh(test);
	Test_u128Rsh(test);
}
