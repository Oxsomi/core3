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

#include "test_types_container_shared.h"
#include "types/base/string_read_helper.h"
#include "types/container/string.h"

void Test_stringNumbers(Test *t) {

	Test_setModule(t, "CharString numbers");

	const CharString resultsStr[] = {
		CharString_createRefCStrConst("0x1234"),
		CharString_createRefCStrConst("0b10101"),
		CharString_createRefCStrConst("0o707"),
		CharString_createRefCStrConst("0nNiceNumber"),
		CharString_createRefCStrConst("69420")
	};

	const U64 resultsU64[] = {

		0x1234,
		0b10101,
		0707,

		((U64)C8_nyto('N') << 54) | ((U64)C8_nyto('i') << 48) | ((U64)C8_nyto('c') << 42) | ((U64)C8_nyto('e') << 36) |
		((U64)C8_nyto('N') << 30) | ((U64)C8_nyto('u') << 24) | ((U64)C8_nyto('m') << 18) | ((U64)C8_nyto('b') << 12) |
		((U64)C8_nyto('e') <<  6) | C8_nyto('r'),

		69420
	};

	CharString tmpStr = { 0 };

	//Number to string

	CharStringCreateNumber createNumber = (CharStringCreateNumber) {
		.v         = resultsU64[0],
		.leadingZeros = 0,
		.allocator = t->alloc,
		.result    = &tmpStr
	};

	if (CharString_createHex(&createNumber, &t->err)) {
		Test_assert(t, "createHex", CharString_equalsStringSensitive(resultsStr + 0, &tmpStr));
		CharString_free(&tmpStr, t->alloc);
	}
	else Test_assert(t, "createHex", false);

	createNumber.v = resultsU64[1];

	if (CharString_createBin(&createNumber, &t->err)) {
		Test_assert(t, "createBin", CharString_equalsStringSensitive(resultsStr + 1, &tmpStr));
		CharString_free(&tmpStr, t->alloc);
	}
	else Test_assert(t, "createBin", false);

	createNumber.v = resultsU64[2];

	if (CharString_createOct(&createNumber, &t->err)) {
		Test_assert(t, "createOct", CharString_equalsStringSensitive(resultsStr + 2, &tmpStr));
		CharString_free(&tmpStr, t->alloc);
	}
	else Test_assert(t, "createOct", false);

	createNumber.v = resultsU64[3];

	if (CharString_createNyto(&createNumber, &t->err)) {
		Test_assert(t, "createNyto", CharString_equalsStringSensitive(resultsStr + 3, &tmpStr));
		CharString_free(&tmpStr, t->alloc);
	}
	else Test_assert(t, "createNyto", false);

	createNumber.v = resultsU64[4];

	if (CharString_createDec(&createNumber, &t->err)) {
		Test_assert(t, "createDec", CharString_equalsStringSensitive(resultsStr + 4, &tmpStr));
		CharString_free(&tmpStr, t->alloc);
	}
	else Test_assert(t, "createDec", false);

	//String to number

	U64 tmpRes[5] = { 0 };

	Test_assert(t, "parseHex",  CharString_parseHex (resultsStr[0], tmpRes + 0));
	Test_assert(t, "parseBin",  CharString_parseBin (resultsStr[1], tmpRes + 1));
	Test_assert(t, "parseOct",  CharString_parseOct (resultsStr[2], tmpRes + 2));
	Test_assert(t, "parseNyto", CharString_parseNyto(resultsStr[3], tmpRes + 3));
	Test_assert(t, "parseDec",  CharString_parseDec (resultsStr[4], tmpRes + 4));

	for (U32 i = 0; i < (U32)(sizeof(resultsStr) / sizeof(resultsStr[0])); ++i)
		Test_assert(t, "parse round-trip", tmpRes[i] == resultsU64[i]);
}

void Test_string(Test *t) {
	Test_stringNumbers(t);
}
