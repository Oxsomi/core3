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

//types/base/test/test_types_base_string.c

#include "test_types_base_shared.h"
#include "types/base/string_base.h"

void Test_string(Test *test) {

	Test_setModule(test, "String");

	const C8 *cstr = "Hello, World!";
	ShortString shortStr = "Short";
	LongString longStrBuf = "This is a long string buffer";

	//Simple creation and properties

	CharString str = CharString_createRefCStrConst(cstr);
	
	Test_assert(test, "CharString_createRefCStrConst", CharString_length(str) == 13);
	Test_assert(test, "CharString_isRef", CharString_isRef(str));
	Test_assert(test, "CharString_isConstRef", CharString_isConstRef(str));
	Test_assert(test, "CharString_isEmpty", !CharString_isEmpty(str));
	Test_assert(test, "CharString_isNullTerminated", CharString_isNullTerminated(str));
	Test_assert(test, "CharString_capacity", CharString_capacity(str) == 0);
	Test_assert(test, "CharString_bytes", CharString_bytes(str) == 13);

	//Auto detection

	CharString autoStr = CharString_createRefAuto((C8*)cstr, 100);
	
	Test_assert(test, "CharString_createRefAuto", CharString_length(autoStr) == 13);
	Test_assert(test, "CharString_createRefAuto (ref)", CharString_isRef(autoStr));
	Test_assert(test, "CharString_createRefAuto (not const)", !CharString_isConstRef(autoStr));
	Test_assert(test, "CharString_createRefAuto (null term)", CharString_isNullTerminated(autoStr));

	//Sized creation

	CharString sizedStr = CharString_createRefSizedConst(cstr, 5, false);
	
	Test_assert(test, "CharString_createRefSizedConst", CharString_length(sizedStr) == 5);
	Test_assert(test, "CharString_createRefSizedConst (ref)", CharString_isRef(sizedStr));
	Test_assert(test, "CharString_createRefSizedConst (const)", CharString_isConstRef(sizedStr));
	Test_assert(test, "CharString_createRefSizedConst (not null term)", !CharString_isNullTerminated(sizedStr));

	//With null terminator

	CharString sizedStrNull = CharString_createRefSizedConst(cstr, 13, true);
	
	Test_assert(test, "CharString_createRefSizedConst (null)", CharString_length(sizedStrNull) == 13);
	Test_assert(test, "CharString_createRefSizedConst (null term)", CharString_isNullTerminated(sizedStrNull));

	//ShortString and LongString

	CharString shortStrRef = CharString_createRefShortString(shortStr);
	
	Test_assert(test, "CharString_createRefShortString", CharString_length(shortStrRef) == 5);
	Test_assert(test, "CharString_createRefShortString (ref)", CharString_isRef(shortStrRef));
	Test_assert(test, "CharString_createRefShortString (not const)", !CharString_isConstRef(shortStrRef));

	CharString shortStrRefConst = CharString_createRefShortStringConst(shortStr);
	
	Test_assert(test, "CharString_createRefShortStringConst", CharString_length(shortStrRefConst) == 5);
	Test_assert(test, "CharString_createRefShortStringConst (const)", CharString_isConstRef(shortStrRefConst));

	CharString longStrRefConst = CharString_createRefLongStringConst(longStrBuf);
	
	Test_assert(test, "CharString_createRefLongStringConst", CharString_length(longStrRefConst) == 28);
	Test_assert(test, "CharString_createRefLongStringConst (const)", CharString_isConstRef(longStrRefConst));

	//Null string

	CharString nullStr = CharString_createNull();
	
	Test_assert(test, "CharString_createNull", CharString_isEmpty(nullStr));
	Test_assert(test, "CharString_createNull (length)", CharString_length(nullStr) == 0);
	Test_assert(test, "CharString_createNull (ref)", CharString_isRef(nullStr));

	//Iteration

	Test_assert(test, "CharString_beginConst", CharString_beginConst(str) == cstr);
	Test_assert(test, "CharString_endConst", CharString_endConst(str) == cstr + 13);

	Test_assert(test, "CharString_begin (const ref)", !CharString_begin(str));
	Test_assert(test, "CharString_end (const ref)", !CharString_end(str));

	//Mut iteration
	
	Test_assert(test, "CharString_begin (mutable)", CharString_begin(autoStr) == (C8*)cstr);
	Test_assert(test, "CharString_end (mutable)", CharString_end(autoStr) == (C8*)cstr + 13);

	//Char at

	const C8 *chPtr = CharString_charAtConst(str, 0);
	Test_assert(test, "CharString_charAtConst", chPtr && *chPtr == 'H');
	
	chPtr = CharString_charAtConst(str, 7);
	Test_assert(test, "CharString_charAtConst", chPtr && *chPtr == 'W');
	
	chPtr = CharString_charAtConst(str, 12);
	Test_assert(test, "CharString_charAtConst", chPtr && *chPtr == '!');
	
	chPtr = CharString_charAtConst(str, 13);
	Test_assert(test, "CharString_charAtConst (out of bounds)", !chPtr);

	//Char at (mut)

	C8 *chPtrMut = CharString_charAt(str, 0);
	Test_assert(test, "CharString_charAt (const ref)", !chPtrMut);

	chPtrMut = CharString_charAt(autoStr, 0);
	Test_assert(test, "CharString_charAt (mutable)", chPtrMut && *chPtrMut == 'H');

	//getAt

	Test_assert(test, "CharString_getAt", CharString_getAt(str, 0) == 'H');
	Test_assert(test, "CharString_getAt", CharString_getAt(str, 7) == 'W');
	Test_assert(test, "CharString_getAt (out of bounds)", CharString_getAt(str, 100) == C8_MAX);

	//setAt (fail on const)

	Test_assert(test, "CharString_setAt (const ref)", !CharString_setAt(str, 0, 'X'));

	//setAt (should work on mutable ref)

	C8 mut[20] = "Hello";
	CharString mutableStr = CharString_createRefAuto(mut, 20);
	
	Test_assert(test, "CharString_setAt (mutable)", CharString_setAt(mutableStr, 0, 'J'));
	Test_assert(test, "CharString_setAt (value)", mut[0] == 'J');
	Test_assert(test, "CharString_setAt (rest)", mut[1] == 'e');

	Test_assert(test, "CharString_setAt (null)", !CharString_setAt(mutableStr, 0, '\0'));

	Test_assert(test, "CharString_setAt (out of bounds)", !CharString_setAt(mutableStr, 100, 'X'));

	//ASCII validation

	CharString asciiStr = CharString_createRefCStrConst("Hello123");
	Test_assert(test, "CharString_isValidAscii", CharString_isValidAscii(asciiStr));

	C8 nonAsciiBuf[10] = "Hi\xFF\xFE";
	CharString nonAsciiStr = CharString_createRefSized(nonAsciiBuf, 4, false);
	Test_assert(test, "CharString_isValidAscii (non-ascii)", !CharString_isValidAscii(nonAsciiStr));

	//Buffer

	Buffer buf = CharString_bufferConst(str);
	Test_assert(test, "CharString_bufferConst", Buffer_length(buf) == 13);
	Test_assert(test, "CharString_bufferConst (ptr)", buf.ptr == (const U8*)cstr);
	Test_assert(test, "CharString_bufferConst (const)", Buffer_isConstRef(buf));

	//Buffer (mut)
	buf = CharString_buffer(str);
	Test_assert(test, "CharString_buffer (const ref)", !buf.ptr);

	buf = CharString_buffer(autoStr);
	Test_assert(test, "CharString_buffer (mutable)", Buffer_length(buf) == 13);
	Test_assert(test, "CharString_buffer (ptr)", buf.ptrNonConst == (U8*)cstr);

	//Allocated buffer

	buf = CharString_allocatedBuffer(str);
	Test_assert(test, "CharString_allocatedBuffer (ref)", !buf.ptr);

	buf = CharString_allocatedBufferConst(str);
	Test_assert(test, "CharString_allocatedBufferConst (ref)", !buf.ptr);

	//calcStrLen

	U64 len = CharString_calcStrLen("Test", 100);
	Test_assert(test, "CharString_calcStrLen", len == 4);

	len = CharString_calcStrLen("Test", 2);
	Test_assert(test, "CharString_calcStrLen (limited)", len == 2);

	len = CharString_calcStrLen(NULL, 100);
	Test_assert(test, "CharString_calcStrLen (NULL)", !len);

	const C8 *noNullTerm = "NoNull";
	len = CharString_calcStrLen(noNullTerm, 6);
	Test_assert(test, "CharString_calcStrLen (no null)", len == 6);

	//newLine

	CharString newLine = CharString_newLine();
	Test_assert(test, "CharString_newLine", CharString_length(newLine) == 1);
	Test_assert(test, "CharString_newLine (value)", CharString_getAt(newLine, 0) == '\n');

	//createRefStr

	CharString original = CharString_createRefCStrConst("Original");
	CharString refFromStr = CharString_createRefStrConst(original);
	
	Test_assert(test, "CharString_createRefStrConst", CharString_length(refFromStr) == 8);
	Test_assert(test, "CharString_createRefStrConst (ptr)", refFromStr.ptr == original.ptr);
	Test_assert(test, "CharString_createRefStrConst (const)", CharString_isConstRef(refFromStr));

	//Clear

	Test_assert(test, "CharString_clear (const ref)", !CharString_clear(&str));

	//Test null terminated flag

	C8 ownedBuf[64] = "Test";
	CharString ownedStr = (CharString) {
		.ptrNonConst = ownedBuf,
		.lenAndNullTerminated = 4 | ((U64)1 << 63),        //4 chars, null terminated
		.capacityAndRefInfo = 64                        //Pretend we own 64 bytes
	};
	
	Test_assert(test, "Owned string setup", CharString_length(ownedStr) == 4);
	Test_assert(test, "Owned string setup (null term)", CharString_isNullTerminated(ownedStr));
	Test_assert(test, "Owned string setup (not ref)", !CharString_isRef(ownedStr));
	Test_assert(test, "Owned string setup (not const)", !CharString_isConstRef(ownedStr));
	Test_assert(test, "Owned string setup (capacity)", CharString_capacity(ownedStr) == 64);

	//Clear owned string

	Test_assert(test, "CharString_clear (owned)", CharString_clear(&ownedStr));
	Test_assert(test, "CharString_clear (length)", !CharString_length(ownedStr));
	Test_assert(test, "CharString_clear (null term preserved)", CharString_isNullTerminated(ownedStr));
	Test_assert(test, "CharString_clear (null char)", ownedStr.ptrNonConst[0] == '\0');
	Test_assert(test, "CharString_clear (capacity)", CharString_capacity(ownedStr) == 64);
}
