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

#include "test_types_base_shared.h"
#include "types/base/string_mut.h"
#include "types/base/string_mut_helper.h"
#include "types/base/string_read_helper.h"

static inline void fillStr(LongString lstr, const C8 *cstr, CharString *str) {

	CharString newStr = CharString_createRefCStrConst(cstr);

	Buffer_memcpy(Buffer_createRef(lstr, sizeof(LongString)), CharString_bufferConst(newStr));

	if(CharString_length(newStr) < sizeof(LongString))
		lstr[CharString_length(newStr)] = '\0';

	CharString str2 = {
		.ptr = lstr,
		.lenAndNullTerminated = CharString_length(newStr),
		.capacityAndRefInfo = sizeof(LongString) - 1            //Pretend to be owned, doesn't matter, no free used.
	};

	*str = str2;
}

void Test_stringMut(Test *test) {

	Test_setModule(test, "String mut");
	
	LongString buf1;
	CharString str;
	fillStr(buf1, "Hello, World!", &str);

	//Erase middle

	Test_assert(test, "CharString_eraseAtCount", CharString_eraseAtCount(&str, 5, 2, &test->err));
	Test_assert(test, "CharString_eraseAtCount (length)", CharString_length(str) == 11);

	Test_assert(test, "CharString_eraseAtCount (value)", CharString_equalsCStringSensitive(&str, "HelloWorld!"));

	//Erase at start

	Test_assert(test, "CharString_eraseAtCount (front)", CharString_eraseAtCount(&str, 0, 2, &test->err));
	Test_assert(test, "CharString_eraseAtCount (front length)", CharString_length(str) == 9);

	Test_assert(test, "CharString_eraseAtCount (front value)", CharString_equalsCStringSensitive(&str, "lloWorld!"));

	//Erase at end
	
	Test_assert(test, "CharString_eraseAtCount (end)", CharString_eraseAtCount(&str, 7, 2, &test->err));
	Test_assert(test, "CharString_eraseAtCount (end length)", CharString_length(str) == 7);

	Test_assert(test, "CharString_eraseAtCount (end value)", CharString_equalsCStringSensitive(&str, "lloWorl"));

	//Test popFront/popEnd/popFrontCount/popEndCount
	
	Test_assert(test, "CharString_popFront", CharString_popFront(&str, &test->err));

	Test_assert(test, "CharString_popFront (value)", CharString_equalsCStringSensitive(&str, "loWorl"));

	Test_assert(test, "CharString_popEnd", CharString_popEnd(&str, &test->err));

	Test_assert(test, "CharString_popEnd (value)", CharString_equalsCStringSensitive(&str, "loWor"));

	Test_assert(test, "CharString_popFrontCount", CharString_popFrontCount(&str, 2, &test->err));

	Test_assert(test, "CharString_popFrontCount (value)", CharString_equalsCStringSensitive(&str, "Wor"));

	Test_assert(test, "CharString_popEndCount", CharString_popEndCount(&str, 2, &test->err));

	Test_assert(test, "CharString_popEndCount (value)", CharString_equalsCStringSensitive(&str, "W"));

	//Test eraseAll (character)

	fillStr(buf1, "Hello, Hello, Hello!", &str);
	Test_assert(test, "CharString_eraseAllSensitive", CharString_eraseAllSensitive(&str, 'l', 0, 0));

	Test_assert(test, "CharString_eraseAllSensitive (value)", CharString_equalsCStringSensitive(&str, "Heo, Heo, Heo!"));

	//Test eraseAll case insensitive

	fillStr(buf1, "Hello, HeLLo, HELLO!", &str);
	
	Test_assert(test, "CharString_eraseAllInsensitive", CharString_eraseAllInsensitive(&str, 'L', 0, 0));
	Test_assert(test, "CharString_eraseAllInsensitive (value)", CharString_equalsCStringSensitive(&str, "Heo, Heo, HEO!"));

	//Test eraseFirst/eraseLast

	fillStr(buf1, "abcabc", &str);
	Test_assert(test, "CharString_eraseFirstSensitive", CharString_eraseFirstSensitive(&str, 'b', 0, 0));

	Test_assert(test, "CharString_eraseFirstSensitive (value)", CharString_equalsCStringSensitive(&str, "acabc"));

	fillStr(buf1, "abcabc", &str);
	Test_assert(test, "CharString_eraseLastSensitive", CharString_eraseLastSensitive(&str, 'b', 0, 0));

	Test_assert(test, "CharString_eraseLastSensitive (value)", CharString_equalsCStringSensitive(&str, "abcac"));

	//Test eraseAllString

	fillStr(buf1, "Hello World, Hello World!", &str);
	CharString hello = CharString_createRefCStrConst("Hello");
	
	Test_assert(test, "CharString_eraseAllStringSensitive", CharString_eraseAllStringSensitive(&str, &hello, 0, 0));

	Test_assert(test, "CharString_eraseAllStringSensitive (value)", CharString_equalsCStringSensitive(&str, " World,  World!"));

	//Test eraseFirstString/eraseLastString

	fillStr(buf1, "test test test", &str);
	CharString testStr = CharString_createRefCStrConst("test");
	
	Test_assert(test, "CharString_eraseFirstStringSensitive", CharString_eraseFirstStringSensitive(&str, &testStr, 0, 0));

	Test_assert(test, "CharString_eraseFirstStringSensitive (value)", CharString_equalsCStringSensitive(&str, " test test"));

	Test_assert(test, "CharString_eraseLastStringSensitive", CharString_eraseLastStringSensitive(&str, &testStr, 0, 0));

	Test_assert(test, "CharString_eraseLastStringSensitive (value)", CharString_equalsCStringSensitive(&str, " test "));

	//Test erase with offset and length

	fillStr(buf1, "Hello World Hello World", &str);
	Test_assert(test, "CharString_eraseAllSensitive (range)", CharString_eraseAllSensitive(&str, 'o', 6, 11));

	Test_assert(test, "CharString_eraseAllSensitive (range value)", CharString_equalsCStringSensitive(&str, "Hello Wrld Hell World"));

	//Test replaceAll

	fillStr(buf1, "aAa bbb ccc", &str);
	Test_assert(test, "CharString_replaceAllSensitive", CharString_replaceAllSensitive(&str, 'a', 'x', 0, 0));

	Test_assert(test, "CharString_replaceAllSensitive (value)", CharString_equalsCStringSensitive(&str, "xAx bbb ccc"));

	//Test replaceAll case insensitive

	fillStr(buf1, "AaA BbB CcC", &str);
	Test_assert(test, "CharString_replaceAllInsensitive", CharString_replaceAllInsensitive(&str, 'A', 'x', 0, 0));

	Test_assert(test, "CharString_replaceAllInsensitive (value)", CharString_equalsCStringSensitive(&str, "xxx BbB CcC"));

	//Test replaceFirst/replaceLast

	fillStr(buf1, "abcabcabc", &str);
	Test_assert(test, "CharString_replaceFirstSensitive", CharString_replaceFirstSensitive(&str, 'b', 'x', 0, 0));

	Test_assert(test, "CharString_replaceFirstSensitive (value)", CharString_equalsCStringSensitive(&str, "axcabcabc"));

	Test_assert(test, "CharString_replaceLastSensitive", CharString_replaceLastSensitive(&str, 'b', 'x', 0, 0));

	Test_assert(test, "CharString_replaceLastSensitive (value)", CharString_equalsCStringSensitive(&str, "axcabcaxc"));

	//Test replace with range

	fillStr(buf1, "aaa bbb aaa", &str);
	Test_assert(test, "CharString_replaceAllSensitive (range)", CharString_replaceAllSensitive(&str, 'a', 'x', 4, 7));

	Test_assert(test, "CharString_replaceAllSensitive (range value)", CharString_equalsCStringSensitive(&str, "aaa bbb xxx"));

	//Test trim

	fillStr(buf1, "  \t Hello World \n\r  ", &str);
	CharString trimmed = CharString_trim(str);

	Test_assert(test, "CharString_trim", CharString_equalsCStringSensitive(&trimmed, "Hello World"));
	Test_assert(test, "CharString_trim (is ref)", CharString_isRef(trimmed));

	//Test trim with no whitespace

	trimmed = CharString_trim(trimmed);
	Test_assert(test, "CharString_trim (no ws)", CharString_equalsCStringSensitive(&trimmed, "Hello World"));

	//Test trim with only whitespace

	fillStr(buf1, "   \t\n\r  ", &str);
	trimmed = CharString_trim(str);
	
	Test_assert(test, "CharString_trim (only ws)", CharString_isEmpty(trimmed));

	//Test transform toLower

	fillStr(buf1, "HeLLo WoRLd!", &str);
	Test_assert(test, "CharString_toLower", CharString_toLower(&str));

	Test_assert(test, "CharString_toLower (value)", CharString_equalsCStringSensitive(&str, "hello world!"));

	//Test transform toUpper

	Test_assert(test, "CharString_toUpper", CharString_toUpper(&str));

	Test_assert(test, "CharString_toUpper (value)", CharString_equalsCStringSensitive(&str, "HELLO WORLD!"));

	//Test formatPath

	fillStr(buf1, "C:\\Users\\Test\\file.txt", &str);
	Test_assert(test, "CharString_formatPath", CharString_formatPath(&str));

	Test_assert(test, "CharString_formatPath (value)", CharString_equalsCStringSensitive(&str, "C:/Users/Test/file.txt"));

	//Test erase on const ref (should fail)

	CharString constRef = CharString_createRefCStrConst("Hello");
	Test_assert(test, "CharString_eraseAtCount (const ref)", !CharString_eraseAtCount(&constRef, 0, 1, NULL));

	//Test replace on const ref (should fail)

	Test_assert(test, "CharString_replaceAllSensitive (const ref)", !CharString_replaceAllSensitive(&constRef, 'H', 'J', 0, 0));

	//Test transform on const ref (should fail)

	Test_assert(test, "CharString_toLower (const ref)", !CharString_toLower(&constRef));

	//Test bounds checking

	fillStr(buf1, "Test", &str);
	Test_assert(test, "CharString_eraseAtCount (bounds)", !CharString_eraseAtCount(&str, 2, 10, NULL));
	Test_assert(test, "CharString_popEndCount (bounds)", !CharString_popEndCount(&str, 10, NULL));
}
