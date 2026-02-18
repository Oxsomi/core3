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

#include "types/base/platform_types.h"
#include "types/base/string_read.h"
#include "types/base/string_read_helper.h"
#include "types/base/mathf.h"
#include "shared.h"

void Test_stringRead(Test *test) {

	Test_setModule(test, "String read");

	CharString str = CharString_createRefCStrConst("Hello, World!");
	CharString empty = CharString_createNull();
	CharString s = CharString_createNull();

	//startsWith / endsWith (char, sensitive)

	Test_assert(test, "CharString_startsWithSensitive", CharString_startsWithSensitive(str, 'H', 0));
	Test_assert(test, "CharString_startsWithSensitive", !CharString_startsWithSensitive(str, 'h', 0));
	Test_assert(test, "CharString_startsWithSensitive", CharString_startsWithSensitive(str, 'e', 1));
	Test_assert(test, "CharString_startsWithSensitive", !CharString_startsWithSensitive(empty, 'H', 0));

	Test_assert(test, "CharString_endsWithSensitive", CharString_endsWithSensitive(str, '!', 0));
	Test_assert(test, "CharString_endsWithSensitive", !CharString_endsWithSensitive(str, '?', 0));
	Test_assert(test, "CharString_endsWithSensitive", CharString_endsWithSensitive(str, 'd', 1));
	Test_assert(test, "CharString_endsWithSensitive", !CharString_endsWithSensitive(empty, '!', 0));

	//startsWith / endsWith (char, insensitive)

	Test_assert(test, "CharString_startsWithInsensitive", CharString_startsWithInsensitive(str, 'h', 0));
	Test_assert(test, "CharString_startsWithInsensitive", CharString_startsWithInsensitive(str, 'H', 0));
	Test_assert(test, "CharString_startsWithInsensitive", !CharString_startsWithInsensitive(str, 'x', 0));

	Test_assert(test, "CharString_endsWithInsensitive", CharString_endsWithInsensitive(str, '!', 0));
	Test_assert(test, "CharString_endsWithInsensitive", !CharString_endsWithInsensitive(str, 'x', 0));

	//startsWith / endsWith (string, sensitive)

	CharString hello = CharString_createRefCStrConst("Hello");
	CharString world = CharString_createRefCStrConst("World!");
	CharString helloL = CharString_createRefCStrConst("hello");

	Test_assert(test, "CharString_startsWithStringSensitive", CharString_startsWithStringSensitive(&str, &hello, 0));
	Test_assert(test, "CharString_startsWithStringSensitive", !CharString_startsWithStringSensitive(&str, &helloL, 0));
	Test_assert(test, "CharString_startsWithStringSensitive", CharString_startsWithStringSensitive(&str, &world, 7));

	Test_assert(test, "CharString_endsWithStringSensitive", CharString_endsWithStringSensitive(&str, &world, 0));
	Test_assert(test, "CharString_endsWithStringSensitive", !CharString_endsWithStringSensitive(&str, &hello, 0));

	//startsWith / endsWith (string, insensitive)

	Test_assert(test, "CharString_startsWithStringInsensitive", CharString_startsWithStringInsensitive(&str, &helloL, 0));
	Test_assert(test, "CharString_startsWithStringInsensitive", CharString_startsWithStringInsensitive(&str, &hello, 0));

	//countAll (char, sensitive)

	CharString abcabc = CharString_createRefCStrConst("abcabcabc");

	Test_assert(test, "CharString_countAllSensitive", CharString_countAllSensitive(&abcabc, 'a', 0) == 3);
	Test_assert(test, "CharString_countAllSensitive", CharString_countAllSensitive(&abcabc, 'b', 0) == 3);
	Test_assert(test, "CharString_countAllSensitive", CharString_countAllSensitive(&abcabc, 'z', 0) == 0);
	Test_assert(test, "CharString_countAllSensitive", CharString_countAllSensitive(&abcabc, 'a', 3) == 2);
	Test_assert(test, "CharString_countAllSensitive", CharString_countAllSensitive(&abcabc, 'A', 0) == 0);

	//countAll (char, insensitive)

	CharString mixedCase = CharString_createRefCStrConst("aAbBaA");

	Test_assert(test, "CharString_countAllInsensitive", CharString_countAllInsensitive(&mixedCase, 'a', 0) == 4);
	Test_assert(test, "CharString_countAllInsensitive", CharString_countAllInsensitive(&mixedCase, 'B', 0) == 2);

	//countAll (string, sensitive)

	CharString abc3 = CharString_createRefCStrConst("abcabcabc");
	CharString needle = CharString_createRefCStrConst("abc");
	CharString needleU = CharString_createRefCStrConst("ABC");

	Test_assert(test, "CharString_countAllStringSensitive", CharString_countAllStringSensitive(&abc3, &needle, 0) == 3);
	Test_assert(test, "CharString_countAllStringSensitive", CharString_countAllStringSensitive(&abc3, &needleU, 0) == 0);
	Test_assert(test, "CharString_countAllStringInsensitive", CharString_countAllStringInsensitive(&abc3, &needleU, 0) == 3);

	//findFirst / findLast (char, sensitive)

	str = CharString_createRefCStrConst("abcdefabcdef");

	Test_assert(test, "CharString_findFirstSensitive", CharString_findFirstSensitive(&str, 'a', 0, 0) == 0);
	Test_assert(test, "CharString_findFirstSensitive", CharString_findFirstSensitive(&str, 'd', 0, 0) == 3);
	Test_assert(test, "CharString_findFirstSensitive", CharString_findFirstSensitive(&str, 'a', 1, 0) == 6);
	Test_assert(test, "CharString_findFirstSensitive", CharString_findFirstSensitive(&str, 'z', 0, 0) == U64_MAX);
	Test_assert(test, "CharString_findFirstSensitive", CharString_findFirstSensitive(&str, 'A', 0, 0) == U64_MAX);

	Test_assert(test, "CharString_findLastSensitive", CharString_findLastSensitive(&str, 'a', 0, 0) == 6);
	Test_assert(test, "CharString_findLastSensitive", CharString_findLastSensitive(&str, 'f', 0, 0) == 11);
	Test_assert(test, "CharString_findLastSensitive", CharString_findLastSensitive(&str, 'z', 0, 0) == U64_MAX);

	//findFirst / findLast (char, insensitive)

	Test_assert(test, "CharString_findFirstInsensitive", CharString_findFirstInsensitive(&str, 'A', 0, 0) == 0);
	Test_assert(test, "CharString_findLastInsensitive", CharString_findLastInsensitive(&str, 'F', 0, 0) == 11);

	//findFirst / findLast with length limit

	Test_assert(test, "CharString_findFirstSensitive", CharString_findFirstSensitive(&str, 'f', 0, 5) == U64_MAX);
	Test_assert(test, "CharString_findFirstSensitive", CharString_findFirstSensitive(&str, 'f', 0, 6) == 5);

	//findFirst / findLast (string, sensitive)

	CharString abc = CharString_createRefCStrConst("abc");
	CharString ABC = CharString_createRefCStrConst("ABC");

	Test_assert(test, "CharString_findFirstStringSensitive", CharString_findFirstStringSensitive(&str, &abc, 0, 0) == 0);
	Test_assert(test, "CharString_findFirstStringSensitive", CharString_findFirstStringSensitive(&str, &abc, 1, 0) == 6);
	Test_assert(test, "CharString_findFirstStringSensitive", CharString_findFirstStringSensitive(&str, &ABC, 0, 0) == U64_MAX);
	Test_assert(test, "CharString_findFirstStringInsensitive", CharString_findFirstStringInsensitive(&str, &ABC, 0, 0) == 0);

	Test_assert(test, "CharString_findLastStringSensitive", CharString_findLastStringSensitive(&str, &abc, 0, 0) == 6);
	Test_assert(test, "CharString_findLastStringInsensitive", CharString_findLastStringInsensitive(&str, &ABC, 0, 0) == 6);

	//contains (char)

	Test_assert(test, "CharString_containsSensitive", CharString_containsSensitive(&str, 'a', 0, 0));
	Test_assert(test, "CharString_containsSensitive", !CharString_containsSensitive(&str, 'z', 0, 0));
	Test_assert(test, "CharString_containsInsensitive", CharString_containsInsensitive(&str, 'A', 0, 0));

	//contains (string)

	Test_assert(test, "CharString_containsStringSensitive", CharString_containsStringSensitive(&str, &abc, 0, 0));
	Test_assert(test, "CharString_containsStringSensitive", !CharString_containsStringSensitive(&str, &ABC, 0, 0));
	Test_assert(test, "CharString_containsStringInsensitive", CharString_containsStringInsensitive(&str, &ABC, 0, 0));

	Test_assert(test, "CharString_containsStringSensitive", CharString_containsStringSensitive(&str, &abc, 1, 0));
	Test_assert(test, "CharString_containsStringSensitive", !CharString_containsStringSensitive(&str, &abc, 7, 0));

	Test_assert(test, "CharString_containsStringSensitive", CharString_containsStringSensitive(&str, &abc, 0, 6));
	Test_assert(test, "CharString_containsStringSensitive", !CharString_containsStringSensitive(&str, &abc, 0, 2));

	CharString defNeedle = CharString_createRefCStrConst("def");
	Test_assert(test, "CharString_containsStringSensitive", CharString_containsStringSensitive(&str, &defNeedle, 3, 6));
	Test_assert(test, "CharString_containsStringSensitive", !CharString_containsStringSensitive(&str, &abc, 3, 3));

	CharString DEF = CharString_createRefCStrConst("DEF");
	Test_assert(test, "CharString_containsStringInsensitive", CharString_containsStringInsensitive(&str, &DEF, 3, 6));
	Test_assert(test, "CharString_containsStringInsensitive", !CharString_containsStringInsensitive(&str, &ABC, 3, 3));

	//equals (char)

	CharString singleA = CharString_createRefCStrConst("a");

	Test_assert(test, "CharString_equalsSensitive", CharString_equalsSensitive(singleA, 'a'));
	Test_assert(test, "CharString_equalsSensitive", !CharString_equalsSensitive(singleA, 'A'));
	Test_assert(test, "CharString_equalsInsensitive", CharString_equalsInsensitive(singleA, 'A'));
	Test_assert(test, "CharString_equalsSensitive", !CharString_equalsSensitive(str, 'H'));

	//equals (string)

	str = CharString_createRefCStrConst("Hello, World!");
	CharString hello2 = CharString_createRefCStrConst("Hello, World!");
	CharString hello3 = CharString_createRefCStrConst("hello, world!");
	CharString hello4 = CharString_createRefCStrConst("Hello, World");

	Test_assert(test, "CharString_equalsStringSensitive", CharString_equalsStringSensitive(&str, &hello2));
	Test_assert(test, "CharString_equalsStringSensitive", !CharString_equalsStringSensitive(&str, &hello3));
	Test_assert(test, "CharString_equalsStringSensitive", !CharString_equalsStringSensitive(&str, &hello4));
	Test_assert(test, "CharString_equalsStringInsensitive", CharString_equalsStringInsensitive(&str, &hello3));

	//compare

	CharString aStr = CharString_createRefCStrConst("abc");
	CharString bStr = CharString_createRefCStrConst("abd");
	CharString aStr2 = CharString_createRefCStrConst("abc");

	Test_assert(test, "CharString_compareSensitive", CharString_compareSensitive(&aStr, &bStr) == ECompareResult_Lt);
	Test_assert(test, "CharString_compareSensitive", CharString_compareSensitive(&bStr, &aStr) == ECompareResult_Gt);
	Test_assert(test, "CharString_compareSensitive", CharString_compareSensitive(&aStr, &aStr2) == ECompareResult_Eq);

	CharString upperA = CharString_createRefCStrConst("ABC");
	Test_assert(test, "CharString_compareInsensitive", CharString_compareInsensitive(&aStr, &upperA) == ECompareResult_Eq);

	//parse functions

	U64 u64;
	I64 i64;
	F64 f64;
	F32 f32;

	//parseDec

	Test_assert(test, "CharString_parseDec", CharString_parseDec(CharString_createRefCStrConst("12345"), &u64));
	Test_assert(test, "CharString_parseDec", u64 == 12345);

	Test_assert(test, "CharString_parseDec", CharString_parseDec(CharString_createRefCStrConst("0"), &u64));
	Test_assert(test, "CharString_parseDec", u64 == 0);

	Test_assert(test, "CharString_parseDec", !CharString_parseDec(CharString_createRefCStrConst("123abc"), &u64));
	Test_assert(test, "CharString_parseDec", !CharString_parseDec(empty, &u64));

	//parseDecSigned

	Test_assert(test, "CharString_parseDecSigned", CharString_parseDecSigned(CharString_createRefCStrConst("99"), &i64));
	Test_assert(test, "CharString_parseDecSigned", i64 == 99);

	Test_assert(test, "CharString_parseDecSigned", CharString_parseDecSigned(CharString_createRefCStrConst("-9"), &i64));
	Test_assert(test, "CharString_parseDecSigned", i64 == -9);

	Test_assert(test, "CharString_parseDecSigned", CharString_parseDecSigned(CharString_createRefCStrConst("0"), &i64));
	Test_assert(test, "CharString_parseDecSigned", i64 == 0);

	Test_assert(test, "CharString_parseDecSigned", !CharString_parseDecSigned(CharString_createRefCStrConst("--1"), &i64));

	//parseHex

	Test_assert(test, "CharString_parseHex", CharString_parseHex(CharString_createRefCStrConst("0xFF"), &u64));
	Test_assert(test, "CharString_parseHex", u64 == 255);

	Test_assert(test, "CharString_parseHex", CharString_parseHex(CharString_createRefCStrConst("FF"), &u64));
	Test_assert(test, "CharString_parseHex", u64 == 255);

	Test_assert(test, "CharString_parseHex", CharString_parseHex(CharString_createRefCStrConst("DEADBEEF"), &u64));
	Test_assert(test, "CharString_parseHex", u64 == 0xDEADBEEF);

	Test_assert(test, "CharString_parseHex", !CharString_parseHex(CharString_createRefCStrConst("0xGG"), &u64));

	//parseBin

	Test_assert(test, "CharString_parseBin", CharString_parseBin(CharString_createRefCStrConst("0b1010"), &u64));
	Test_assert(test, "CharString_parseHex", u64 == 10);

	Test_assert(test, "CharString_parseBin (no prefix)", CharString_parseBin(CharString_createRefCStrConst("1111"), &u64));
	Test_assert(test, "CharString_parseHex", u64 == 15);

	Test_assert(test, "CharString_parseBin (invalid)", !CharString_parseBin(CharString_createRefCStrConst("0b2"), &u64));

	//parseOct

	Test_assert(test, "CharString_parseOct", CharString_parseOct(CharString_createRefCStrConst("0o17"), &u64));
	Test_assert(test, "CharString_parseOct", u64 == 15);

	Test_assert(test, "CharString_parseOct", CharString_parseOct(CharString_createRefCStrConst("17"), &u64));
	Test_assert(test, "CharString_parseOct", u64 == 15);

	Test_assert(test, "CharString_parseOct", !CharString_parseOct(CharString_createRefCStrConst("0o8"), &u64));

	//parseNyto

	Test_assert(test, "CharString_parseNyto", CharString_parseNyto(CharString_createRefCStrConst("0nAB"), &u64));
	Test_assert(test, "CharString_parseNyto", u64 == ((10 << 6) | 11));

	Test_assert(test, "CharString_parseNyto", !CharString_parseNyto(CharString_createRefCStrConst("0n!"), &u64));

	//parseU64 (auto)

	Test_assert(test, "CharString_parseU64", CharString_parseU64(CharString_createRefCStrConst("42"), &u64));
	Test_assert(test, "CharString_parseU64", u64 == 42);

	Test_assert(test, "CharString_parseU64", CharString_parseU64(CharString_createRefCStrConst("0xFF"), &u64));
	Test_assert(test, "CharString_parseU64", u64 == 255);

	Test_assert(test, "CharString_parseU64", CharString_parseU64(CharString_createRefCStrConst("0b101"), &u64));
	Test_assert(test, "CharString_parseU64", u64 == 5);

	Test_assert(test, "CharString_parseU64", CharString_parseU64(CharString_createRefCStrConst("0o10"), &u64));
	Test_assert(test, "CharString_parseU64", u64 == 8);

	//parseDouble / parseFloat

	Test_assert(test, "CharString_parseDouble", CharString_parseDouble(CharString_createRefCStrConst("3.14"), &f64));
	Test_assert(test, "CharString_parseDouble", F64_approxEq(f64, 3.14, 0.001));

	Test_assert(test, "CharString_parseDouble", CharString_parseDouble(CharString_createRefCStrConst("-1.5"), &f64));
	Test_assert(test, "CharString_parseDouble", F64_approxEq(f64, -1.5, 0.001));

	Test_assert(test, "CharString_parseDouble", CharString_parseDouble(CharString_createRefCStrConst("1e3"), &f64));
	Test_assert(test, "CharString_parseDouble", F64_approxEq(f64, 1000, 0.001));

	Test_assert(test, "CharString_parseDouble", !CharString_parseDouble(CharString_createRefCStrConst("abc"), &f64));

	Test_assert(test, "CharString_parseFloat", CharString_parseFloat(CharString_createRefCStrConst("1.5"), &f32));
	Test_assert(test, "CharString_parseDouble", F32_approxEq(f32, 1.5f, 0.001f));

	Test_assert(test, "CharString_parseFloat", CharString_parseFloat(CharString_createRefCStrConst("-2.0"), &f32));
	Test_assert(test, "CharString_parseDouble", F32_approxEq(f32, -2, 0.001f));

	Test_assert(test, "CharString_parseFloat", !CharString_parseFloat(CharString_createRefCStrConst("xyz"), &f32));

	//isNyto / isHex / isBin / isOct / isDec / isAlphaNumeric / isSignedNumber / isUnsignedNumber / isFloat

	Test_assert(test, "CharString_isDec", CharString_isDec(CharString_createRefCStrConst("12345")));
	Test_assert(test, "CharString_isDec", !CharString_isDec(CharString_createRefCStrConst("123a")));
	Test_assert(test, "CharString_isDec", !CharString_isDec(empty));

	Test_assert(test, "CharString_isHex", CharString_isHex(CharString_createRefCStrConst("0xDEAD")));
	Test_assert(test, "CharString_isHex", CharString_isHex(CharString_createRefCStrConst("DEAD")));
	Test_assert(test, "CharString_isHex", !CharString_isHex(CharString_createRefCStrConst("0xGG")));

	Test_assert(test, "CharString_isBin", CharString_isBin(CharString_createRefCStrConst("0b1010")));
	Test_assert(test, "CharString_isBin", !CharString_isBin(CharString_createRefCStrConst("0b2")));

	Test_assert(test, "CharString_isOct", CharString_isOct(CharString_createRefCStrConst("0o77")));
	Test_assert(test, "CharString_isOct", !CharString_isOct(CharString_createRefCStrConst("0o8")));

	Test_assert(test, "CharString_isNyto", CharString_isNyto(CharString_createRefCStrConst("0nAZ09_$")));
	Test_assert(test, "CharString_isNyto", !CharString_isNyto(CharString_createRefCStrConst("0n!")));

	Test_assert(test, "CharString_isAlphaNumeric", CharString_isAlphaNumeric(CharString_createRefCStrConst("abc123")));
	Test_assert(test, "CharString_isAlphaNumeric", !CharString_isAlphaNumeric(CharString_createRefCStrConst("abc!")));

	Test_assert(test, "CharString_isSignedNumber", CharString_isSignedNumber(CharString_createRefCStrConst("123")));
	Test_assert(test, "CharString_isSignedNumber", CharString_isSignedNumber(CharString_createRefCStrConst("-123")));
	Test_assert(test, "CharString_isSignedNumber", !CharString_isSignedNumber(CharString_createRefCStrConst("12a")));

	Test_assert(test, "CharString_isUnsignedNumber", CharString_isUnsignedNumber(CharString_createRefCStrConst("123")));
	Test_assert(test, "CharString_isUnsignedNumber", !CharString_isUnsignedNumber(CharString_createRefCStrConst("-123")));

	Test_assert(test, "CharString_isFloat", CharString_isFloat(CharString_createRefCStrConst("3")));
	Test_assert(test, "CharString_isFloat", CharString_isFloat(CharString_createRefCStrConst("3.14")));
	Test_assert(test, "CharString_isFloat", CharString_isFloat(CharString_createRefCStrConst("1e10")));
	Test_assert(test, "CharString_isFloat", CharString_isFloat(CharString_createRefCStrConst("-1.5e-3")));
	Test_assert(test, "CharString_isFloat", !CharString_isFloat(CharString_createRefCStrConst("abc")));

	//cut

	CharString cutSrc = CharString_createRefCStrConst("Hello, World!");

	Test_assert(test, "CharString_cut", CharString_cut(&cutSrc, 7, 5, &s));
	CharString world2 = CharString_createRefCStrConst("World");
	Test_assert(test, "CharString_cut", CharString_equalsStringSensitive(&s, &world2));
	Test_assert(test, "CharString_cut", CharString_isRef(s));

	Test_assert(test, "CharString_cutEnd", CharString_cutEnd(&cutSrc, 5, &s));
	CharString helloC = CharString_createRefCStrConst("Hello");
	Test_assert(test, "CharString_cutEnd", CharString_equalsStringSensitive(&s, &helloC));

	Test_assert(test, "CharString_cutBegin", CharString_cutBegin(&cutSrc, 7, &s));
	CharString world3 = CharString_createRefCStrConst("World!");
	Test_assert(test, "CharString_cutBegin", CharString_equalsStringSensitive(&s, &world3));

	Test_assert(test, "CharString_cut", !CharString_cut(&cutSrc, 7, 100, &s));
	Test_assert(test, "CharString_cutBegin", !CharString_cutBegin(&cutSrc, 100, &s));

	//cutAfter / cutBefore (char)

	CharString slashStr = CharString_createRefCStrConst("a/b/c");

	Test_assert(test, "CharString_cutAfterFirstSensitive", CharString_cutAfterFirstSensitive(&slashStr, '/', &s));
	CharString aStr3 = CharString_createRefCStrConst("a");
	Test_assert(test, "CharString_cutAfterFirstSensitive", CharString_equalsStringSensitive(&s, &aStr3));

	Test_assert(test, "CharString_cutAfterLastSensitive", CharString_cutAfterLastSensitive(&slashStr, '/', &s));
	CharString abStr = CharString_createRefCStrConst("a/b");
	Test_assert(test, "CharString_cutAfterLastSensitive", CharString_equalsStringSensitive(&s, &abStr));

	Test_assert(test, "CharString_cutBeforeFirstSensitive", CharString_cutBeforeFirstSensitive(&slashStr, '/', &s));
	CharString bcStr = CharString_createRefCStrConst("b/c");
	Test_assert(test, "CharString_cutBeforeFirstSensitive", CharString_equalsStringSensitive(&s, &bcStr));

	Test_assert(test, "CharString_cutBeforeLastSensitive", CharString_cutBeforeLastSensitive(&slashStr, '/', &s));
	CharString cStr = CharString_createRefCStrConst("c");
	Test_assert(test, "CharString_cutBeforeLastSensitive", CharString_equalsStringSensitive(&s, &cStr));

	CharString mixStr = CharString_createRefCStrConst("aXbXc");

	Test_assert(test, "CharString_cutAfterFirstInsensitive", CharString_cutAfterFirstInsensitive(&mixStr, 'x', &s));
	Test_assert(test, "CharString_cutAfterFirstInsensitive", CharString_equalsStringSensitive(&s, &aStr3));

	//cutAfter / cutBefore (string)

	CharString dSls = CharString_createRefCStrConst("foo::bar::baz");
	CharString sep = CharString_createRefCStrConst("::");

	Test_assert(test, "CharString_cutAfterFirstStringSensitive", CharString_cutAfterFirstStringSensitive(&dSls, &sep, &s));
	CharString fooStr = CharString_createRefCStrConst("foo");
	Test_assert(test, "CharString_cutAfterFirstStringSensitive", CharString_equalsStringSensitive(&s, &fooStr));

	Test_assert(test, "CharString_cutAfterLastStringSensitive", CharString_cutAfterLastStringSensitive(&dSls, &sep, &s));
	CharString fooBarStr = CharString_createRefCStrConst("foo::bar");
	Test_assert(test, "CharString_cutAfterLastStringSensitive", CharString_equalsStringSensitive(&s, &fooBarStr));

	Test_assert(test, "CharString_cutBeforeFirstStringSensitive", CharString_cutBeforeFirstStringSensitive(&dSls, &sep, &s));
	CharString barBazStr = CharString_createRefCStrConst("bar::baz");
	Test_assert(test, "CharString_cutBeforeFirstStringSensitive", CharString_equalsStringSensitive(&s, &barBazStr));

	Test_assert(test, "CharString_cutBeforeLastStringSensitive", CharString_cutBeforeLastStringSensitive(&dSls, &sep, &s));
	CharString bazStr = CharString_createRefCStrConst("baz");
	Test_assert(test, "CharString_cutBeforeLastStringSensitive", CharString_equalsStringSensitive(&s, &bazStr));

	Test_assert(test, "CharString_cutAfterFirstStringInsensitive", CharString_cutAfterFirstStringInsensitive(&dSls, &sep, &s));
	Test_assert(test, "CharString_cutAfterFirstStringInsensitive", CharString_equalsStringSensitive(&s, &fooStr));

	Test_assert(test, "CharString_cutBeforeLastStringInsensitive", CharString_cutBeforeLastStringInsensitive(&dSls, &sep, &s));
	Test_assert(test, "CharString_cutBeforeLastStringInsensitive", CharString_equalsStringSensitive(&s, &bazStr));

	//cut missing char (should fail)

	Test_assert(test, "CharString_cutAfterFirstSensitive (missing)", !CharString_cutAfterFirstSensitive(&slashStr, 'z', &s));
	Test_assert(test, "CharString_cutBeforeLastSensitive (missing)", !CharString_cutBeforeLastSensitive(&slashStr, 'z', &s));

	//isValidFileName

	static const C8 *validFileNames[] = {
		"file.txt",
		"myfile",
		"my_file_name",
		"file123.txt",
		"My.File.txt",
		"CONtrol",
		"NULlptr.txt",
		"PRNtscreen.pdf",
		"AUXiliary",
		"COM",
		"LPT",
		"COMBO",
		"LPTx",
		"COM1file",
		"LPT1port"
	};

	static const C8 *invalidFileNames[] = {
		"",
		" file.txt",
		"file.txt ",
		"file.",
		".",
		"..",
		"my/file.txt",
		"my\\file.txt",
		"my:file.txt",
		"my*file.txt",
		"my?file.txt",
		"my\"file.txt",
		"my<file.txt",
		"my>file.txt",
		"my|file.txt",
		"CON",
		"con",
		"AUX",
		"NUL",
		"PRN",
		"CON.txt",
		"nul.txt",
		"AUX.log",
		"PRN.pdf",
		"COM1",
		"COM9",
		"LPT1",
		"LPT1.txt",
		"com1",
		"lpt2"
	};

	for (U64 i = 0; i < sizeof(validFileNames) / sizeof(validFileNames[0]); ++i) {
		s = CharString_createRefCStrConst(validFileNames[i]);
		Test_assert(test, validFileNames[i], CharString_isValidFileName(s));
	}

	for (U64 i = 0; i < sizeof(invalidFileNames) / sizeof(invalidFileNames[0]); ++i) {
		s = CharString_createRefCStrConst(invalidFileNames[i]);
		Test_assert(test, invalidFileNames[i][0] ? invalidFileNames[i] : "(empty)", !CharString_isValidFileName(s));
	}

	//isValidFilePath

	static const C8 *validFilePaths[] = {
		"file.txt",
		"folder/file.txt",
		"a/b/c/d.txt",
		"folder\\file.txt",
		"a/b\\c",
		"folder/",
		"folder\\",
		"./file.txt",
		"../file.txt",
		"a/../b",
		"//virtual/file.txt"
	};

	static const C8 *invalidFilePaths[] = {
		"",
		"folder//file.txt",
		"folder/\\file.txt",
		"folder/CON.txt",
		"a/NUL/b",
		"folder./file.txt",
		"folder/LPT1.txt"
	};

	for (U64 i = 0; i < sizeof(validFilePaths) / sizeof(validFilePaths[0]); ++i) {
		s = CharString_createRefCStrConst(validFilePaths[i]);
		Test_assert(test, validFilePaths[i], CharString_isValidFilePath(s));
	}

	for (U64 i = 0; i < sizeof(invalidFilePaths) / sizeof(invalidFilePaths[0]); ++i) {
		s = CharString_createRefCStrConst(invalidFilePaths[i]);
		Test_assert(test, invalidFilePaths[i][0] ? invalidFilePaths[i] : "(empty)", !CharString_isValidFilePath(s));
	}

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS

		static const C8 *validFilePathsWin[] = {
			"C:/folder/file.txt",
			"C:\\folder\\file.txt"
		};

		static const C8 *invalidFilePathsWin[] = {
			"C:folder/file.txt",
			"0:/folder"
		};

		for (U64 i = 0; i < sizeof(validFilePathsWin) / sizeof(validFilePathsWin[0]); ++i) {
			s = CharString_createRefCStrConst(validFilePathsWin[i]);
			Test_assert(test, validFilePathsWin[i], CharString_isValidFilePath(s));
		}

		for (U64 i = 0; i < sizeof(invalidFilePathsWin) / sizeof(invalidFilePathsWin[0]); ++i) {
			s = CharString_createRefCStrConst(invalidFilePathsWin[i]);
			Test_assert(test, invalidFilePathsWin[i], !CharString_isValidFilePath(s));
		}

	#else

		static const C8 *validFilePathsUnix[] = {
			"/C/folder/file.txt"
		};

		static const C8 *invalidFilePathsUnix[] = {
			"C:/folder/file.txt"
		};

		for (U64 i = 0; i < sizeof(validFilePathsUnix) / sizeof(validFilePathsUnix[0]); ++i) {
			s = CharString_createRefCStrConst(validFilePathsUnix[i]);
			Test_assert(test, validFilePathsUnix[i], CharString_isValidFilePath(s));
		}

		for (U64 i = 0; i < sizeof(invalidFilePathsUnix) / sizeof(invalidFilePathsUnix[0]); ++i) {
			s = CharString_createRefCStrConst(invalidFilePathsUnix[i]);
			Test_assert(test, invalidFilePathsUnix[i], !CharString_isValidFilePath(s));
		}

	#endif
}
