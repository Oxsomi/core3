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

//types/container/test/test_types_container_string.c

#include "test_types_container_shared.h"
#include "types/base/string_read_helper.h"
#include "types/base/allocator.h"
#include "types/base/type_id.h"
#include "types/container/string.h"

#include <locale.h>

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

		((U64) C8_nyto('N') << 54) | ((U64) C8_nyto('i') << 48) | ((U64) C8_nyto('c') << 42) | ((U64) C8_nyto('e') << 36) |
		((U64) C8_nyto('N') << 30) | ((U64) C8_nyto('u') << 24) | ((U64) C8_nyto('m') << 18) | ((U64) C8_nyto('b') << 12) |
		((U64) C8_nyto('e') <<  6) | C8_nyto('r'),

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

	for (U32 i = 0; i < (U32) (sizeof(resultsStr) / sizeof(resultsStr[0])); ++i)
		Test_assert(t, "parse round-trip", tmpRes[i] == resultsU64[i]);
}

//Formatting a float must not follow LC_NUMERIC: under every locale the text has to be the C locale's, byte for byte,
// which is "%.17g" and so reads back as the same double.
//A German locale is tried under both spellings, Windows and POSIX; a machine with neither still checks the C locale.

static Bool Test_stringDecimalIs(const C8 *out, const C8 *expected) {
	const CharString a = CharString_createRefCStrConst(out);
	return CharString_equalsCStringSensitive(&a, expected);
}

static void Test_stringDecimal(Test *t) {

	Test_setModule(t, "CharString decimal");

	C8 out[CharString_decimalLen];

	Test_assert(t, "precision 0 refused", !CharString_formatDecimal(1, 0, out));
	Test_assert(t, "precision 18 refused", !CharString_formatDecimal(1, 18, out));

	static const U64 specialBits[] = { 0xFFF8000000000000, 0x7FF0000000000000, 0xFFF0000000000000 };
	static const C8 *specialText[] = { "nan", "inf", "-inf" };

	for(U64 i = 0; i < 3; ++i) {
		F64 v = 0;
		Buffer_memcpy(Buffer_createRef(&v, sizeof(v)), Buffer_createRefConst(&specialBits[i], sizeof(v)));
		Test_assert(t, specialText[i], CharString_formatDecimal(v, 17, out) && Test_stringDecimalIs(out, specialText[i]));
	}

	static const F64 values[] = {
		1.5, -2.25, 0.1, 0.33333333333333331, 123456789.125, 5e-324, 1.7976931348623157e308, -0.0, 0
	};
	static const C8 *locales[] = { "de-DE", "de_DE.UTF-8", "de_DE" };
	C8 expected[sizeof(values) / sizeof(values[0])][CharString_decimalLen];

	setlocale(LC_NUMERIC, "C");

	for(U64 i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
		Test_assert(t, "C locale", CharString_formatDecimal(values[i], 17, expected[i]));

	Test_assert(t, "1.5 at 9", CharString_formatDecimal(1.5, 9, out) && Test_stringDecimalIs(out, "1.5"));

	Bool commaLocale = false;

	for(U64 l = 0; l < sizeof(locales) / sizeof(locales[0]); ++l) {

		if(!setlocale(LC_NUMERIC, locales[l]))
			continue;

		commaLocale = true;

		for(U64 i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {

			Test_assert(
				t, "same text as the C locale",
				CharString_formatDecimal(values[i], 17, out) && Test_stringDecimalIs(out, expected[i])
			);

			F64 parsed = 0;

			Test_assert(t, "decimal parse ignores locale", CharString_parseDouble(
				CharString_createRefCStrConst(expected[i]), &parsed
			) && parsed == values[i]);
		}
	}

	setlocale(LC_NUMERIC, "C");

	if(!commaLocale)
		Test_print(t, "CharString_formatDecimal locale test ran in the C locale only (no decimal comma locale installed)");
}

typedef struct StringFailAllocator {
	const Allocator *alloc;
	U64 calls, live;
} StringFailAllocator;

static Bool StringFail_alloc(void *allocator, U64 length, Buffer *output, Error *e_rr) {

	Bool s_uccess = true;
	StringFailAllocator *state = (StringFailAllocator*) allocator;

	if(++state->calls == 2)
		retError(clean, Error_outOfMemory(0, "StringFail_alloc() injected suffix allocation failure"));

	gotoIfError3(clean, state->alloc->alloc(state->alloc->ptr, length, output, e_rr));
	++state->live;

clean:
	return s_uccess;
}

static void StringFail_free(void *allocator, Buffer buffer) {

	StringFailAllocator *state = (StringFailAllocator*) allocator;

	state->alloc->free(state->alloc->ptr, buffer);
	--state->live;
}

static void Test_stringHLSLFailure(Test *t) {

	Test_setModule(t, "CharString HLSL allocation failure");

	StringFailAllocator state = { .alloc = t->alloc };
	const Allocator alloc = { .ptr = &state, .alloc = StringFail_alloc, .free = StringFail_free };
	CharString result = CharString_createNull();
	Error err = Error_none();

	Test_assert(t, "HLSL vector suffix allocation fails", !CharString_createFromETypeIdHLSL(
		(TypeId) ETypeId_F32x4, EHLSLStringifyFlags_None, &alloc, &result, &err
	));

	Test_assert(t, "HLSL suffix failure reports allocation error", err.genericError == EGenericError_OutOfMemory);
	Test_assert(t, "HLSL failure follows successful prefix allocation", state.calls == 2);
	Test_assert(t, "HLSL failure releases prefix allocation", !state.live);
	Test_assert(t, "HLSL failure clears output", !result.ptr && !CharString_length(result));

	CharString_free(&result, &alloc);

	Test_assert(t, "HLSL failed output cleanup is safe", !state.live);
}

void Test_string(Test *t) {
	Test_stringNumbers(t);
	Test_stringDecimal(t);
	Test_stringHLSLFailure(t);
}
