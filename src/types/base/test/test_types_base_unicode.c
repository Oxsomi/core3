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

//types/base/test/test_types_base_unicode.c

#include "test_types_base_shared.h"
#include "types/base/unicode.h"
#include "types/base/string_read_helper.h"
#include "types/base/string_mut_helper.h"

//UTF-8 is spelled out as bytes; a hex escape ends at its second digit only because a string literal break follows.
//U+212A Kelvin sign folds to k, so it's the 3 byte code point that matches a 1 byte one; U+017F long s folds to s.

#define KELVIN "\xE2\x84\xAA"
#define LONG_S "\xC5\xBF"

static Bool Test_compareIs(const C8 *a, const C8 *b, EStringCase stringCase, ECompareResult expected) {
	const CharString strA = CharString_createRefCStrConst(a), strB = CharString_createRefCStrConst(b);
	return CharString_compare(&strA, &strB, stringCase) == expected;
}

static Bool Test_equalsIs(const C8 *a, const C8 *b, EStringCase stringCase) {
	const CharString strA = CharString_createRefCStrConst(a), strB = CharString_createRefCStrConst(b);
	return CharString_equalsString(&strA, &strB, stringCase);
}

static Bool Test_startsWith(const C8 *a, const C8 *b, EStringCase stringCase) {
	const CharString strA = CharString_createRefCStrConst(a), strB = CharString_createRefCStrConst(b);
	const CharStringSensOff at = { &strA, stringCase, 0 };
	return CharString_startsWithString(&at, &strB);
}

static Bool Test_endsWith(const C8 *a, const C8 *b, EStringCase stringCase, U64 off) {
	const CharString strA = CharString_createRefCStrConst(a), strB = CharString_createRefCStrConst(b);
	const CharStringSensOff at = { &strA, stringCase, off };
	return CharString_endsWithString(&at, &strB);
}

static U64 Test_count(const C8 *a, const C8 *b, EStringCase stringCase) {
	const CharString strA = CharString_createRefCStrConst(a), strB = CharString_createRefCStrConst(b);
	const CharStringSensOff at = { &strA, stringCase, 0 };
	return CharString_countAllString(&at, &strB);
}

//A string over a stack buffer that the erase functions accept as owned; nothing frees it

static CharString Test_owned(LongString buf, const C8 *str) {

	const CharString ref = CharString_createRefCStrConst(str);
	const U64 len = CharString_length(ref);

	Buffer_memcpy(Buffer_createRef(buf, sizeof(LongString)), CharString_bufferConst(ref));
	buf[len] = '\0';

	return (CharString) {
		.ptr = buf,
		.lenAndNullTerminated = len | ((U64) 1 << 63),
		.capacityAndRefInfo = sizeof(LongString) - 1
	};
}

static void Test_unicodeUTF8(Test *test) {

	Test_setModule(test, "Unicode UTF-8");

	//Every scalar value round trips at the length both lengthUTF8 and its lead byte give, and one byte less is cut off

	Bool roundTrips = true;

	for (UnicodeCodePoint c = 0; c <= 0x10FFFF && roundTrips; ++c) {

		if (c >= 0xD800 && c <= 0xDFFF)
			continue;

		C8 bytes[4] = { 0 };
		UnicodeCodePoint decoded = 0;

		const U8 len = UnicodeCodePoint_toUTF8(c, bytes, sizeof(bytes));
		const Bool decodes = len && UnicodeCodePoint_fromUTF8(bytes, len, &decoded) == len && decoded == c;
		const Bool cutOff = len && !UnicodeCodePoint_fromUTF8(bytes, len - 1, &decoded);

		roundTrips =
			decodes && cutOff && len == UnicodeCodePoint_lengthUTF8(c) && len == UnicodeCodePoint_leadLengthUTF8(bytes[0]);
	}

	Test_assert(test, "every scalar round trips", roundTrips);

	C8 out[4] = { 0 };
	UnicodeCodePoint c = 0;

	Test_assert(test, "surrogates aren't written", !UnicodeCodePoint_toUTF8(0xD800, out, 4));
	Test_assert(test, "past U+10FFFF isn't written", !UnicodeCodePoint_toUTF8(0x110000, out, 4));
	Test_assert(test, "what doesn't fit isn't written", !UnicodeCodePoint_toUTF8(0x20AC, out, 2) && !out[0]);
	Test_assert(test, "no output", !UnicodeCodePoint_toUTF8('a', NULL, 4));
	Test_assert(test, "no input", !UnicodeCodePoint_fromUTF8(NULL, 4, &c) && !UnicodeCodePoint_fromUTF8("a", 0, &c));
	Test_assert(test, "control characters decode", UnicodeCodePoint_fromUTF8("\x01", 1, &c) == 1 && c == 1);

	static const C8 *invalid[] = {
		"\x80",                    //A continuation
		"\xC0\x80",                //Overlong NUL
		"\xC1\xBF",                //Overlong two byte
		"\xE0\x9F\xBF",            //Overlong three byte
		"\xED\xA0\x80",            //Surrogate
		"\xF0\x8F\xBF\xBF",        //Overlong four byte
		"\xF4\x90\x80\x80",        //Past U+10FFFF
		"\xF5\x80\x80\x80",
		"\xFF",
		"\xE2\x84\x41",            //Not a continuation
		"\xC3"                     //Cut off
	};

	for (U64 i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
		const CharString str = CharString_createRefCStrConst(invalid[i]);
		Test_assert(test, "invalid UTF-8", !UnicodeCodePoint_fromUTF8(str.ptr, CharString_length(str), &c));
	}

	//readFolded is what the insensitive functions walk with: one folded code point at a time, or U32_MAX at the end

	U64 i = 0;

	Test_assert(test, "readFolded without i", UnicodeCodePoint_readFolded("a", NULL, 1) == U32_MAX);
	Test_assert(test, "readFolded without ptr", UnicodeCodePoint_readFolded(NULL, &i, 5) == U32_MAX && i == 5);

	i = 3;
	Test_assert(test, "readFolded at the end", UnicodeCodePoint_readFolded("abc", &i, 3) == U32_MAX && i == 3);

	i = 0;
	Test_assert(test, "readFolded multi byte", UnicodeCodePoint_readFolded(KELVIN "x", &i, 4) == 'k' && i == 3);

	i = 0;
	Test_assert(test, "readFolded invalid", UnicodeCodePoint_readFolded("\xFF", &i, 1) == 0x110000 + 0xFF && i == 1);

	i = 0;
	Test_assert(test, "readFolded cut off by end", UnicodeCodePoint_readFolded(KELVIN, &i, 2) == 0x110000 + 0xE2 && i == 1);
}

static void Test_unicodeCase(Test *test) {

	Test_setModule(test, "Unicode case");

	//Dotless i joins i (as NTFS's uppercase does) while dotted capital I stays apart

	static const UnicodeCodePoint folds[][2] = {
		{ 'A', 'a' }, { 'a', 'a' }, { '[', '[' }, { 0xC9, 0xE9 }, { 0x3A3, 0x3C3 }, { 0x3C2, 0x3C3 },
		{ 0x212A, 'k' }, { 0x17F, 's' }, { 0x1E9E, 0xDF }, { 0x131, 'i' }, { 0x130, 0x130 }, { 0x10400, 0x10428 },
		{ 0x100, 0x101 }, { 0x101, 0x101 }, { 0x12E, 0x12F }, { 0xAB70, 0x13A0 }, { 0x10FFFF, 0x10FFFF }
	};

	static const UnicodeCodePoint lowers[][2] = {
		{ 'A', 'a' }, { 'a', 'a' }, { 0xC9, 0xE9 }, { 0x130, 'i' }, { 0x212A, 'k' }, { 0x23A, 0x2C65 },
		{ 0x1E9E, 0xDF }, { 0x1C5, 0x1C6 }, { 0x10400, 0x10428 }, { 0x17F, 0x17F }, { 0x13A0, 0xAB70 }
	};

	static const UnicodeCodePoint uppers[][2] = {
		{ 'a', 'A' }, { 'A', 'A' }, { 0xE9, 0xC9 }, { 0x131, 'I' }, { 0x17F, 'S' }, { 0x250, 0x2C6F },
		{ 0xDF, 0xDF }, { 0x1C5, 0x1C4 }, { 0x10428, 0x10400 }, { 0x3C2, 0x3A3 }, { 0x10FFFF, 0x10FFFF }
	};

	for (U64 i = 0; i < sizeof(folds) / sizeof(folds[0]); ++i)
		Test_assert(test, "fold", UnicodeCodePoint_caseFold(folds[i][0]) == folds[i][1]);

	for (U64 i = 0; i < sizeof(lowers) / sizeof(lowers[0]); ++i)
		Test_assert(test, "lower", UnicodeCodePoint_toLower(lowers[i][0]) == lowers[i][1]);

	for (U64 i = 0; i < sizeof(uppers) / sizeof(uppers[0]); ++i)
		Test_assert(test, "upper", UnicodeCodePoint_toUpper(uppers[i][0]) == uppers[i][1]);

	//A folded code point folds to itself, or equal names wouldn't be transitive

	Bool idempotent = true;

	for (UnicodeCodePoint c = 0; c <= 0x10FFFF && idempotent; ++c)
		if (c < 0xD800 || c > 0xDFFF)
			idempotent = UnicodeCodePoint_caseFold(UnicodeCodePoint_caseFold(c)) == UnicodeCodePoint_caseFold(c);

	Test_assert(test, "fold is idempotent", idempotent);
}

static void Test_unicodeStrings(Test *test) {

	Test_setModule(test, "Unicode case insensitive strings");

	//Compare: folded code points, invalid bytes after all of them

	Test_assert(test, "ascii", Test_compareIs("Readme.TXT", "README.txt", EStringCase_Insensitive, ECompareResult_Eq));

	Test_assert(test, "latin", Test_compareIs(
		"\xC3\x89" "cole", "\xC3\xA9" "COLE", EStringCase_Insensitive, ECompareResult_Eq
	));

	Test_assert(test, "sigma", Test_compareIs("\xCE\xA3", "\xCF\x82", EStringCase_Insensitive, ECompareResult_Eq));
	Test_assert(test, "kelvin", Test_compareIs(KELVIN, "K", EStringCase_Insensitive, ECompareResult_Eq));
	Test_assert(test, "dotless i", Test_compareIs("\xC4\xB1", "I", EStringCase_Insensitive, ECompareResult_Eq));
	Test_assert(test, "dotted I", Test_compareIs("\xC4\xB0", "i", EStringCase_Insensitive, ECompareResult_Gt));

	Test_assert(test, "deseret", Test_compareIs(
		"\xF0\x90\x90\x80", "\xF0\x90\x90\xA8", EStringCase_Insensitive, ECompareResult_Eq
	));

	Test_assert(test, "order folds first", Test_compareIs("a", "B", EStringCase_Insensitive, ECompareResult_Lt));
	Test_assert(test, "prefix", Test_compareIs("abc", "ABCD", EStringCase_Insensitive, ECompareResult_Lt));
	Test_assert(test, "empty", Test_compareIs("", "", EStringCase_Insensitive, ECompareResult_Eq));

	Test_assert(test, "invalid after code points", Test_compareIs(
		"\xFF", "\xF0\x90\x90\x80", EStringCase_Insensitive, ECompareResult_Gt
	));

	Test_assert(test, "invalid equals itself", Test_compareIs("a\xFF", "A\xFF", EStringCase_Insensitive, ECompareResult_Eq));

	//Common bytes are passed unfolded, but a code point the first difference cuts through is folded whole

	Test_assert(test, "difference in a continuation", Test_compareIs(
		"x\xC3\x89", "x\xC3\xA9", EStringCase_Insensitive, ECompareResult_Eq
	));

	Test_assert(test, "common bytes a string ends in", Test_compareIs(
		"x\xE2\x84", "x" KELVIN, EStringCase_Insensitive, ECompareResult_Gt
	));

	//InsensitiveAscii is the byte path: only A-Z fold

	Test_assert(test, "ascii only ascii", Test_compareIs("ABC", "abc", EStringCase_InsensitiveAscii, ECompareResult_Eq));
	Test_assert(test, "ascii keeps kelvin", Test_compareIs(KELVIN, "K", EStringCase_InsensitiveAscii, ECompareResult_Gt));
	Test_assert(test, "sensitive", Test_compareIs("a", "B", EStringCase_Sensitive, ECompareResult_Gt));

	//Equals, whatever the byte lengths

	Test_assert(test, "equals kelvin", Test_equalsIs(KELVIN "elvin", "kELVIN", EStringCase_Insensitive));
	Test_assert(test, "equals long s", Test_equalsIs(LONG_S "cript", "SCRIPT", EStringCase_Insensitive));
	Test_assert(test, "ascii equals no long s", !Test_equalsIs(LONG_S "cript", "script", EStringCase_InsensitiveAscii));

	const CharString eolA = CharString_createRefCStrConst("a\r\n" KELVIN), eolB = CharString_createRefCStrConst("A\nk");
	Test_assert(test, "equals ignoring eol", CharString_equalsStringIgnoreEolInsensitive(&eolA, &eolB));

	//Starts and ends with: the match can be longer or shorter than the needle

	Test_assert(test, "starts with shorter match", Test_startsWith(KELVIN "elvin", "kel", EStringCase_Insensitive));
	Test_assert(test, "starts with longer needle", Test_startsWith("kelvin", KELVIN, EStringCase_Insensitive));
	Test_assert(test, "starts with fewer bytes", Test_startsWith("k", KELVIN, EStringCase_Insensitive));
	Test_assert(test, "ascii starts with", !Test_startsWith(KELVIN "elvin", "kel", EStringCase_InsensitiveAscii));

	Test_assert(test, "ends with", Test_endsWith("273 " KELVIN, "k", EStringCase_Insensitive, 0));
	Test_assert(test, "ends with longer needle", Test_endsWith("273 k", KELVIN, EStringCase_Insensitive, 0));
	Test_assert(test, "ends with offset", Test_endsWith("a" KELVIN "b", "K", EStringCase_Insensitive, 1));
	Test_assert(test, "ends with invalid", Test_endsWith("abc\xFF", "\xFF", EStringCase_Insensitive, 0));
	Test_assert(test, "ends with mismatch", !Test_endsWith("abc", "k", EStringCase_Insensitive, 0));

	//Find and count give the match's own length

	const CharString hay = CharString_createRefCStrConst("x" KELVIN "y"), k = CharString_createRefCStrConst("k");
	const CharStringSensOffLen whole = { &hay, EStringCase_Insensitive, 0, 0 };
	U64 matchLen = 0;

	Test_assert(test, "find first", CharString_findFirstStringMatch(&whole, &k, &matchLen) == 1 && matchLen == 3);

	const CharString kks = CharString_createRefCStrConst("kK" KELVIN);
	const CharStringSensOffLen kksWhole = { &kks, EStringCase_Insensitive, 0, 0 };

	Test_assert(test, "find last", CharString_findLastStringMatch(&kksWhole, &k, &matchLen) == 2 && matchLen == 3);

	const CharStringSensOffLen kksAscii = { &kks, EStringCase_InsensitiveAscii, 0, 0 };

	Test_assert(test, "ascii find last", CharString_findLastStringMatch(&kksAscii, &k, &matchLen) == 1 && matchLen == 1);

	const CharString abk = CharString_createRefCStrConst("abk"), kelvin = CharString_createRefCStrConst(KELVIN);
	const CharStringSensOffLen abkWhole = { &abk, EStringCase_Insensitive, 0, 0 };

	Test_assert(test, "needle past ascii, match in it", CharString_findFirstStringMatch(&abkWhole, &kelvin, &matchLen) == 2);
	Test_assert(test, "needle past ascii, match length", matchLen == 1);

	const CharString greek = CharString_createRefCStrConst("\xCE\xB1\xCE\xB2\xCE\x93\xCE\xB4");        //alpha beta Gamma delta
	const CharString gamma = CharString_createRefCStrConst("\xCE\xB3\xCE\xB4");                     //gamma delta
	const CharStringSensOffLen greekWhole = { &greek, EStringCase_Insensitive, 0, 0 };

	Test_assert(test, "find past ascii", CharString_findFirstStringMatch(&greekWhole, &gamma, &matchLen) == 4);
	Test_assert(test, "count", Test_count("k" KELVIN "K", "K", EStringCase_Insensitive) == 3);
	Test_assert(test, "ascii count", Test_count("k" KELVIN "K", "K", EStringCase_InsensitiveAscii) == 2);
	Test_assert(test, "sensitive count", Test_count("k" KELVIN "K", "K", EStringCase_Sensitive) == 1);

	//Whatever consumes a match takes its length

	LongString buf;
	CharString s = Test_owned(buf, "a" KELVIN "bkc");
	const CharString upperK = CharString_createRefCStrConst("K");

	Test_assert(test, "erase all", CharString_eraseAllStringInsensitive(&s, &upperK, 0, 0));
	Test_assert(test, "erase all result", CharString_equalsCStringSensitive(&s, "abc"));

	s = Test_owned(buf, "a" KELVIN "bkc");

	Test_assert(test, "erase first", CharString_eraseFirstStringInsensitive(&s, &upperK, 0, 0));
	Test_assert(test, "erase first result", CharString_equalsCStringSensitive(&s, "abkc"));

	const CharString cut = CharString_createRefCStrConst("a" KELVIN "b");
	CharString after = CharString_createNull();

	Test_assert(test, "cut before", CharString_cutBeforeFirstStringInsensitive(&cut, &upperK, &after));
	Test_assert(test, "cut before result", CharString_equalsCStringSensitive(&after, "b"));
}

static void Test_unicodeAsciiWrappers(Test *test) {

	Test_setModule(test, "Ascii case insensitive wrappers");

	//The Ascii twins fold A-Z only; the plain ones fold Unicode, so U+212A and U+017F match k and s there only

	const CharString upper = CharString_createRefCStrConst("ABC"), lower = CharString_createRefCStrConst("abc");
	const CharString kelvin = CharString_createRefCStrConst(KELVIN), longS = CharString_createRefCStrConst(LONG_S);
	const CharString k = CharString_createRefCStrConst("k"), s = CharString_createRefCStrConst("s");

	Test_assert(test, "equals ascii case", CharString_equalsStringInsensitiveAscii(&upper, &lower));
	Test_assert(test, "equals ascii kelvin", !CharString_equalsStringInsensitiveAscii(&kelvin, &k));
	Test_assert(test, "equals ascii long s", !CharString_equalsStringInsensitiveAscii(&longS, &s));
	Test_assert(test, "equals kelvin", CharString_equalsStringInsensitive(&kelvin, &k));
	Test_assert(test, "equals long s", CharString_equalsStringInsensitive(&longS, &s));

	Test_assert(test, "equals cstring ascii case", CharString_equalsCStringInsensitiveAscii(&upper, "abc"));
	Test_assert(test, "equals cstring ascii kelvin", !CharString_equalsCStringInsensitiveAscii(&kelvin, "k"));
	Test_assert(test, "equals cstring kelvin", CharString_equalsCStringInsensitive(&kelvin, "k"));

	Test_assert(test, "compare ascii case", CharString_compareInsensitiveAscii(&upper, &lower) == ECompareResult_Eq);
	Test_assert(test, "compare ascii kelvin", CharString_compareInsensitiveAscii(&kelvin, &k) == ECompareResult_Gt);
	Test_assert(test, "compare kelvin", CharString_compareInsensitive(&kelvin, &k) == ECompareResult_Eq);

	const CharString kelvinWord = CharString_createRefCStrConst(KELVIN "elvin"), prefix = CharString_createRefCStrConst("kel");
	const CharString upperWord = CharString_createRefCStrConst("KELVIN");

	Test_assert(test, "starts with ascii case", CharString_startsWithStringInsensitiveAscii(&upperWord, &prefix, 0));
	Test_assert(test, "starts with ascii kelvin", !CharString_startsWithStringInsensitiveAscii(&kelvinWord, &prefix, 0));
	Test_assert(test, "starts with kelvin", CharString_startsWithStringInsensitive(&kelvinWord, &prefix, 0));
	Test_assert(test, "starts with cstring ascii case", CharString_startsWithCStringInsensitiveAscii(&upperWord, "kel", 0));
	Test_assert(test, "starts with cstring ascii kelvin", !CharString_startsWithCStringInsensitiveAscii(&kelvinWord, "kel", 0));

	const CharString suffix = CharString_createRefCStrConst("lvin"), kelvinEnd = CharString_createRefCStrConst("273 " KELVIN);

	Test_assert(test, "ends with ascii case", CharString_endsWithStringInsensitiveAscii(&upperWord, &suffix, 0));
	Test_assert(test, "ends with ascii kelvin", !CharString_endsWithStringInsensitiveAscii(&kelvinEnd, &k, 0));
	Test_assert(test, "ends with kelvin", CharString_endsWithStringInsensitive(&kelvinEnd, &k, 0));
	Test_assert(test, "ends with char ascii case", CharString_endsWithInsensitiveAscii(upperWord, 'n', 0));

	const CharString hay = CharString_createRefCStrConst("x" KELVIN "y"), hayUpper = CharString_createRefCStrConst("xKy");

	Test_assert(test, "contains ascii case", CharString_containsStringInsensitiveAscii(&hayUpper, &k, 0, 0));
	Test_assert(test, "contains ascii kelvin", !CharString_containsStringInsensitiveAscii(&hay, &k, 0, 0));
	Test_assert(test, "contains kelvin", CharString_containsStringInsensitive(&hay, &k, 0, 0));
	Test_assert(test, "find first ascii case", CharString_findFirstStringInsensitiveAscii(&hayUpper, &k, 0, 0) == 1);
	Test_assert(test, "find first ascii kelvin", CharString_findFirstStringInsensitiveAscii(&hay, &k, 0, 0) == U64_MAX);
	Test_assert(test, "find first kelvin", CharString_findFirstStringInsensitive(&hay, &k, 0, 0) == 1);
	Test_assert(test, "count ascii", CharString_countAllStringInsensitiveAscii(&hay, &k, 0) == 0);
	Test_assert(test, "count", CharString_countAllStringInsensitive(&hay, &k, 0) == 1);

	LongString buf;
	CharString mut = Test_owned(buf, "a" KELVIN "bKc");
	const CharString upperK = CharString_createRefCStrConst("K");

	Test_assert(test, "erase all ascii", CharString_eraseAllStringInsensitiveAscii(&mut, &upperK, 0, 0));
	Test_assert(test, "erase all ascii result", CharString_equalsCStringSensitive(&mut, "a" KELVIN "bc"));

	mut = Test_owned(buf, "a" KELVIN "bKc");

	Test_assert(test, "erase all", CharString_eraseAllStringInsensitive(&mut, &upperK, 0, 0));
	Test_assert(test, "erase all result", CharString_equalsCStringSensitive(&mut, "abc"));

	mut = Test_owned(buf, "kKk");

	Test_assert(test, "replace all ascii", CharString_replaceAllInsensitiveAscii(&mut, 'K', 'z', 0, 0));
	Test_assert(test, "replace all ascii result", CharString_equalsCStringSensitive(&mut, "zzz"));
}

void Test_unicode(Test *test) {
	Test_unicodeUTF8(test);
	Test_unicodeCase(test);
	Test_unicodeStrings(test);
	Test_unicodeAsciiWrappers(test);
}
