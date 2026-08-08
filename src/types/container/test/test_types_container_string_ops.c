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

//types/container/test/test_types_container_string_ops.c
//
//CharString beyond number conversion: refs vs owned memory, mutation, search, cutting, splitting and replacing.
//
//The bias here is towards boundaries rather than happy paths:
// inserting at 0 and at the end, erasing the last character,
// replacing with a longer and a shorter needle,
// and mutating a ref (which must be refused, not silently corrupt someone else's memory).

#include "test_types_container_shared.h"
#include "types/base/string_read_helper.h"
#include "types/base/string_mut_helper.h"
#include "types/container/string.h"
#include "types/container/string_helper.h"
#include "types/container/list_impl.h"

//Shorthand: a const ref to a literal, which is what most of these compare against
#define S(x) CharString_createRefCStrConst(x)

static Bool Test_strEq(CharString a, const C8 *b) {
	const CharString bs = S(b);
	return CharString_equalsStringSensitive(&a, &bs);
}

//========================= refs vs owned =========================

static void Test_stringRefs(Test *t) {

	Test_setModule(t, "CharString refs");

	const CharString constRef = S("hello");

	Test_assert(t, "const ref is a ref", CharString_isRef(constRef) && CharString_isConstRef(constRef));
	Test_assert(t, "const ref has no capacity", !CharString_capacity(constRef));
	Test_assert(t, "const ref length", CharString_length(constRef) == 5);
	Test_assert(t, "literal ref is null terminated", CharString_isNullTerminated(constRef));

	//A sized ref over the middle of a buffer isn't null terminated, and must not pretend to be
	const C8 raw[] = "abcdef";
	const CharString mid = CharString_createRefSizedConst(raw + 1, 3, false);
	Test_assert(t, "sized ref length", CharString_length(mid) == 3 && Test_strEq(mid, "bcd"));
	Test_assert(t, "sized ref isn't null terminated", !CharString_isNullTerminated(mid));

	//Mutating a const ref has to be refused; silently writing would corrupt the literal
	CharString mutable_ = constRef;
	Test_assert(t, "append to const ref rejected", !CharString_append(&mutable_, '!', t->alloc, NULL));
	Test_assert(t, "insert into const ref rejected", !CharString_insert(&mutable_, '!', 0, t->alloc, NULL));
	Test_assert(t, "resize const ref rejected", !CharString_resize(&mutable_, 16, ' ', t->alloc, NULL));
	Test_assert(t, "const ref unchanged after rejections", Test_strEq(mutable_, "hello"));

	//clear only applies to owned memory
	Test_assert(t, "clear on ref rejected", !CharString_clear(&mutable_));

	//Freeing a ref is a no-op rather than a crash, and leaves it empty
	CharString freeMe = S("hello");
	CharString_free(&freeMe, t->alloc);
	Test_assert(t, "freeing a ref is safe", CharString_isEmpty(freeMe));

	//An owned string reports capacity and isn't a ref
	CharString owned = CharString_createNull();

	if (CharString_createCopy(constRef, t->alloc, &owned, &t->err)) {
		Test_assert(t, "owned isn't a ref", !CharString_isRef(owned));
		Test_assert(t, "owned has capacity", CharString_capacity(owned) >= CharString_length(owned));
		Test_assert(t, "copy is null terminated", CharString_isNullTerminated(owned));
		Test_assert(t, "copy matches source", Test_strEq(owned, "hello"));
		Test_assert(t, "copy doesn't alias the source", owned.ptr != constRef.ptr);
		CharString_free(&owned, t->alloc);
		Test_assert(t, "free empties the string", CharString_isEmpty(owned) && !owned.ptr);
	}
	else Test_assert(t, "createCopy", false);

	//Empty is a legal input everywhere
	const CharString empty = CharString_createNull();
	Test_assert(t, "null string is empty", CharString_isEmpty(empty) && !CharString_length(empty));
	Test_assert(t, "empty equals empty", CharString_equalsStringSensitive(&empty, &empty));
}

//========================= mutation =========================

static void Test_stringMutate(Test *t) {

	Test_setModule(t, "CharString mutate");

	CharString s = CharString_createNull();

	//create fills with a repeated character
	if (CharString_create('x', 4, t->alloc, &s, &t->err)) {
		Test_assert(t, "create repeats", Test_strEq(s, "xxxx"));
		CharString_free(&s, t->alloc);
	}
	else Test_assert(t, "create", false);

	Test_assert(t, "create empty", CharString_create('x', 0, t->alloc, &s, &t->err) && CharString_isEmpty(s));
	CharString_free(&s, t->alloc);

	//append / appendString
	Test_assert(t, "seed", CharString_createCopy(S("ab"), t->alloc, &s, &t->err));
	Test_assert(t, "append char", CharString_append(&s, 'c', t->alloc, &t->err) && Test_strEq(s, "abc"));

	const CharString de = S("de");
	Test_assert(t, "appendString", CharString_appendString(&s, &de, t->alloc, &t->err) && Test_strEq(s, "abcde"));
	Test_assert(t, "append stays null terminated", CharString_isNullTerminated(s));

	//insert at the front, the middle and the end; the middle case is the one that used to overrun
	Test_assert(t, "insert at front", CharString_insert(&s, '0', 0, t->alloc, &t->err) && Test_strEq(s, "0abcde"));
	Test_assert(t, "insert in middle", CharString_insert(&s, '_', 3, t->alloc, &t->err) && Test_strEq(s, "0ab_cde"));

	//just before the last character, which the old `i != strl - 1` guard skipped entirely
	Test_assert(t, "insert before last", CharString_insert(&s, '+', 6, t->alloc, &t->err) && Test_strEq(s, "0ab_cd+e"));
	Test_assert(t, "insert at end", CharString_insert(&s, '!', CharString_length(s), t->alloc, &t->err));
	Test_assert(t, "insert at end result", Test_strEq(s, "0ab_cd+e!"));
	Test_assert(t, "insert past end rejected", !CharString_insert(&s, '?', 999, t->alloc, NULL));
	CharString_free(&s, t->alloc);

	//insertString at every position
	Test_assert(t, "seed", CharString_createCopy(S("hello"), t->alloc, &s, &t->err));
	const CharString mid = S("__");
	Test_assert(t, "insertString middle", CharString_insertString(&s, &mid, 2, t->alloc, &t->err));
	Test_assert(t, "insertString middle result", Test_strEq(s, "he__llo"));
	Test_assert(t, "prependString", CharString_prependString(&s, &mid, t->alloc, &t->err) && Test_strEq(s, "__he__llo"));
	Test_assert(t, "prepend char", CharString_prepend(&s, '>', t->alloc, &t->err) && Test_strEq(s, ">__he__llo"));
	CharString_free(&s, t->alloc);

	//resize both ways, reserve
	Test_assert(t, "seed", CharString_createCopy(S("abc"), t->alloc, &s, &t->err));
	Test_assert(t, "resize grows with fill", CharString_resize(&s, 6, '.', t->alloc, &t->err) && Test_strEq(s, "abc..."));
	Test_assert(t, "resize shrinks", CharString_resize(&s, 2, '.', t->alloc, &t->err) && Test_strEq(s, "ab"));
	Test_assert(t, "resize to 0", CharString_resize(&s, 0, '.', t->alloc, &t->err) && CharString_isEmpty(s));

	Test_assert(t, "reserve", CharString_reserve(&s, 128, t->alloc, &t->err) && CharString_capacity(s) >= 128);
	Test_assert(t, "reserve keeps length", CharString_isEmpty(s));
	CharString_free(&s, t->alloc);

	//pop / erase
	Test_assert(t, "seed", CharString_createCopy(S("0123456789"), t->alloc, &s, &t->err));
	Test_assert(t, "popEnd", CharString_popEnd(&s, &t->err) && Test_strEq(s, "012345678"));
	Test_assert(t, "popFront", CharString_popFront(&s, &t->err) && Test_strEq(s, "12345678"));
	Test_assert(t, "popEndCount", CharString_popEndCount(&s, 3, &t->err) && Test_strEq(s, "12345"));
	Test_assert(t, "popFrontCount", CharString_popFrontCount(&s, 2, &t->err) && Test_strEq(s, "345"));
	Test_assert(t, "eraseAt", CharString_eraseAt(&s, 1, &t->err) && Test_strEq(s, "35"));
	Test_assert(t, "eraseAt last", CharString_eraseAt(&s, 1, &t->err) && Test_strEq(s, "3"));
	Test_assert(t, "eraseAt out of bounds rejected", !CharString_eraseAt(&s, 99, NULL));
	CharString_free(&s, t->alloc);

	//eraseAll / eraseFirst / eraseLast
	Test_assert(t, "seed", CharString_createCopy(S("a-b-c-d"), t->alloc, &s, &t->err));
	Test_assert(t, "eraseFirst", CharString_eraseFirstSensitive(&s, '-', 0, 0) && Test_strEq(s, "ab-c-d"));
	Test_assert(t, "eraseLast", CharString_eraseLastSensitive(&s, '-', 0, 0) && Test_strEq(s, "ab-cd"));
	Test_assert(t, "eraseAll", CharString_eraseAllSensitive(&s, '-', 0, 0) && Test_strEq(s, "abcd"));
	CharString_free(&s, t->alloc);

	//transform + trim
	Test_assert(t, "seed", CharString_createCopy(S("MiXeD"), t->alloc, &s, &t->err));
	Test_assert(t, "toLower", CharString_toLower(&s) && Test_strEq(s, "mixed"));
	Test_assert(t, "toUpper", CharString_toUpper(&s) && Test_strEq(s, "MIXED"));
	CharString_free(&s, t->alloc);

	Test_assert(t, "trim both ends", Test_strEq(CharString_trim(S("  pad  ")), "pad"));
	Test_assert(t, "trim no-op", Test_strEq(CharString_trim(S("pad")), "pad"));
	Test_assert(t, "trim all whitespace", CharString_isEmpty(CharString_trim(S("    "))));

	//clear keeps the allocation but empties the string
	Test_assert(t, "seed", CharString_createCopy(S("abc"), t->alloc, &s, &t->err));
	Test_assert(t, "clear", CharString_clear(&s) && CharString_isEmpty(s));
	Test_assert(t, "clear kept capacity", CharString_capacity(s) > 0);
	CharString_free(&s, t->alloc);
}

//========================= replace =========================

static void Test_stringReplace(Test *t) {

	Test_setModule(t, "CharString replace");

	CharString s = CharString_createNull();

	Test_assert(t, "seed", CharString_createCopy(S("a.b.c"), t->alloc, &s, &t->err));
	Test_assert(t, "replaceAll char", CharString_replaceAllSensitive(&s, '.', '/', 0, 0) && Test_strEq(s, "a/b/c"));
	Test_assert(t, "replaceFirst char", CharString_replaceFirstSensitive(&s, '/', '-', 0, 0) && Test_strEq(s, "a-b/c"));
	Test_assert(t, "replaceLast char", CharString_replaceLastSensitive(&s, '/', '+', 0, 0) && Test_strEq(s, "a-b+c"));
	CharString_free(&s, t->alloc);

	//String replacement has to handle a replacement that's longer and shorter than the needle,
	//since both resize the buffer in opposite directions.
	{
		Test_assert(t, "seed", CharString_createCopy(S("one two one"), t->alloc, &s, &t->err));

		const CharString needle = S("one"), longer = S("XXXXX");
		CharStringReplace2 r = { .s = &s, .search = &needle, .replace = &longer, .allocator = t->alloc };
		Test_assert(t, "replaceAllString longer", CharString_replaceAllStringSensitive(&r, &t->err));
		Test_assert(t, "replaceAllString longer result", Test_strEq(s, "XXXXX two XXXXX"));
		CharString_free(&s, t->alloc);
	}

	{
		Test_assert(t, "seed", CharString_createCopy(S("aaa bbb aaa"), t->alloc, &s, &t->err));

		const CharString needle = S("aaa"), shorter = S("z");
		CharStringReplace2 r = { .s = &s, .search = &needle, .replace = &shorter, .allocator = t->alloc };
		Test_assert(t, "replaceAllString shorter", CharString_replaceAllStringSensitive(&r, &t->err));
		Test_assert(t, "replaceAllString shorter result", Test_strEq(s, "z bbb z"));
		CharString_free(&s, t->alloc);
	}

	{
		Test_assert(t, "seed", CharString_createCopy(S("one two one"), t->alloc, &s, &t->err));

		const CharString needle = S("one"), rep = S("1");
		CharStringReplace2 r = { .s = &s, .search = &needle, .replace = &rep, .allocator = t->alloc };
		Test_assert(t, "replaceFirstString", CharString_replaceFirstStringSensitive(&r, &t->err));
		Test_assert(t, "replaceFirstString result", Test_strEq(s, "1 two one"));
		CharString_free(&s, t->alloc);
	}

	//A needle that isn't there must leave the string alone rather than mangle it
	{
		Test_assert(t, "seed", CharString_createCopy(S("abc"), t->alloc, &s, &t->err));

		const CharString needle = S("zzz"), rep = S("!");
		CharStringReplace2 r = { .s = &s, .search = &needle, .replace = &rep, .allocator = t->alloc };
		Test_assert(t, "replace missing needle", CharString_replaceAllStringSensitive(&r, &t->err));
		Test_assert(t, "replace missing needle unchanged", Test_strEq(s, "abc"));
		CharString_free(&s, t->alloc);
	}
}

//========================= search =========================

static void Test_stringSearch(Test *t) {

	Test_setModule(t, "CharString search");

	const CharString s = S("Hello World Hello");
	const CharString hello = S("Hello"), hi = S("hello"), missing = S("zzz");

	Test_assert(t, "findFirst char", CharString_findFirstSensitive(&s, 'o', 0, 0) == 4);
	Test_assert(t, "findLast char", CharString_findLastSensitive(&s, 'o', 0, 0) == 16);
	Test_assert(t, "findFirst missing", CharString_findFirstSensitive(&s, 'q', 0, 0) == U64_MAX);

	Test_assert(t, "findFirstString", CharString_findFirstStringSensitive(&s, &hello, 0, 0) == 0);
	Test_assert(t, "findLastString", CharString_findLastStringSensitive(&s, &hello, 0, 0) == 12);
	Test_assert(t, "findFirstString case matters", CharString_findFirstStringSensitive(&s, &hi, 0, 0) == U64_MAX);
	Test_assert(t, "findFirstString insensitive", CharString_findFirstStringInsensitive(&s, &hi, 0, 0) == 0);
	Test_assert(t, "findString missing", CharString_findFirstStringSensitive(&s, &missing, 0, 0) == U64_MAX);

	Test_assert(t, "contains char", CharString_containsSensitive(&s, 'W', 0, 0));
	Test_assert(t, "contains string", CharString_containsStringSensitive(&s, &hello, 0, 0));
	Test_assert(t, "doesn't contain", !CharString_containsStringSensitive(&s, &missing, 0, 0));

	Test_assert(t, "countAll char", CharString_countAllSensitive(&s, 'l', 0) == 5);
	Test_assert(t, "countAllString", CharString_countAllStringSensitive(&s, &hello, 0) == 2);
	Test_assert(t, "countAllString insensitive", CharString_countAllStringInsensitive(&s, &hi, 0) == 2);

	Test_assert(t, "startsWith char", CharString_startsWithSensitive(s, 'H', 0));
	Test_assert(t, "endsWith char", CharString_endsWithSensitive(s, 'o', 0));
	Test_assert(t, "startsWithString", CharString_startsWithStringSensitive(&s, &hello, 0));
	Test_assert(t, "endsWithString", CharString_endsWithStringSensitive(&s, &hello, 0));
	Test_assert(t, "startsWithString case matters", !CharString_startsWithStringSensitive(&s, &hi, 0));
	Test_assert(t, "startsWithString insensitive", CharString_startsWithStringInsensitive(&s, &hi, 0));

	//An empty haystack must answer without reading anything
	const CharString empty = CharString_createNull();
	Test_assert(t, "find in empty", CharString_findFirstSensitive(&empty, 'a', 0, 0) == U64_MAX);
	Test_assert(t, "empty doesn't contain", !CharString_containsSensitive(&empty, 'a', 0, 0));
	Test_assert(t, "empty countAll", !CharString_countAllSensitive(&empty, 'a', 0));

	//findAll collects every occurrence
	ListU64 finds = (ListU64) { 0 };
	const CharStringFind find = { .s = &s, .alloc = t->alloc, .result = &finds };

	if (CharString_findAllSensitive(&find, 'l', &t->err)) {
		Test_assert(t, "findAll count", finds.length == 5);
		Test_assert(t, "findAll first", finds.length && finds.ptr[0] == 2);
		Test_assert(t, "findAll last", finds.length && finds.ptr[finds.length - 1] == 15);
		ListU64_free(&finds, t->alloc);
	}
	else Test_assert(t, "findAll", false);

	//Comparison
	const CharString a = S("abc"), b = S("abd"), aUpper = S("ABC");
	Test_assert(t, "equals", CharString_equalsStringSensitive(&a, &a));
	Test_assert(t, "not equals", !CharString_equalsStringSensitive(&a, &b));
	Test_assert(t, "equals insensitive", CharString_equalsStringInsensitive(&a, &aUpper));
	Test_assert(t, "not equals sensitive", !CharString_equalsStringSensitive(&a, &aUpper));
	Test_assert(t, "compare lt", CharString_compareSensitive(&a, &b) == ECompareResult_Lt);
	Test_assert(t, "compare gt", CharString_compareSensitive(&b, &a) == ECompareResult_Gt);
	Test_assert(t, "compare eq", CharString_compareSensitive(&a, &a) == ECompareResult_Eq);

	//Hash is content based, so a ref and a copy of the same text must agree
	CharString copy = CharString_createNull();

	if (CharString_createCopy(a, t->alloc, &copy, &t->err)) {
		Test_assert(t, "hash is content based", CharString_hash(a) == CharString_hash(copy));
		Test_assert(t, "hash differs for different content", CharString_hash(a) != CharString_hash(b));
		CharString_free(&copy, t->alloc);
	}
	else Test_assert(t, "hash copy", false);
}

//========================= cut =========================

static void Test_stringCut(Test *t) {

	Test_setModule(t, "CharString cut");

	const CharString s = S("a/b/c");
	CharString out = CharString_createNull();

	Test_assert(t, "cut", CharString_cut(&s, 2, 3, &out) && Test_strEq(out, "b/c"));
	Test_assert(t, "cut to end", CharString_cut(&s, 2, 0, &out) && Test_strEq(out, "b/c"));
	Test_assert(t, "cutEnd", CharString_cutEnd(&s, 3, &out) && Test_strEq(out, "a/b"));
	Test_assert(t, "cutBegin", CharString_cutBegin(&s, 2, &out) && Test_strEq(out, "b/c"));
	Test_assert(t, "cut out of bounds rejected", !CharString_cut(&s, 99, 1, &out));

	//A cut is a ref into the original, so it must not own anything
	Test_assert(t, "cut result is a ref", CharString_cut(&s, 0, 1, &out) && CharString_isRef(out));

	//Read these as "cut away everything after the separator", not "give me the part after it" - the
	//naming trips people up, so it's pinned here. cutAfter keeps the head, cutBefore keeps the tail.

	Test_assert(t, "cutAfterFirst keeps the head", CharString_cutAfterFirstSensitive(&s, '/', &out) && Test_strEq(out, "a"));
	Test_assert(t, "cutAfterLast keeps the head", CharString_cutAfterLastSensitive(&s, '/', &out) && Test_strEq(out, "a/b"));
	Test_assert(t, "cutBeforeFirst keeps the tail", CharString_cutBeforeFirstSensitive(&s, '/', &out) && Test_strEq(out, "b/c"));
	Test_assert(t, "cutBeforeLast keeps the tail", CharString_cutBeforeLastSensitive(&s, '/', &out) && Test_strEq(out, "c"));

	//A separator that isn't there
	Test_assert(t, "cutAfter missing separator", !CharString_cutAfterFirstSensitive(&s, '?', &out));

	//getFilePath / getBasePath format the path first, so they take a mutable string
	CharString path = CharString_createNull();

	if (CharString_createCopy(S("some/dir/file.txt"), t->alloc, &path, &t->err)) {
		Test_assert(t, "getFilePath", Test_strEq(CharString_getFilePath(&path), "file.txt"));
		Test_assert(t, "getBasePath", Test_strEq(CharString_getBasePath(&path), "some/dir"));
		CharString_free(&path, t->alloc);
	}
	else Test_assert(t, "path copy", false);
}

//========================= split =========================

static void Test_stringSplit(Test *t) {

	Test_setModule(t, "CharString split");

	ListCharString parts = (ListCharString) { 0 };
	const CharString s = S("a,b,,c");
	const CharStringSplit split = { .s = &s, .allocator = t->alloc, .result = &parts };

	if (CharString_splitSensitive(&split, ',', &t->err)) {
		Test_assert(t, "split count keeps empties", parts.length == 4);
		Test_assert(t, "split[0]", parts.length > 0 && Test_strEq(parts.ptr[0], "a"));
		Test_assert(t, "split[2] is empty", parts.length > 2 && CharString_isEmpty(parts.ptr[2]));
		Test_assert(t, "split[3]", parts.length > 3 && Test_strEq(parts.ptr[3], "c"));

		//The parts are refs into the source, so freeing the list must not free their memory
		Test_assert(t, "split parts are refs", parts.length > 0 && CharString_isRef(parts.ptr[0]));
		ListCharString_free(&parts, t->alloc);
	}
	else Test_assert(t, "split", false);

	//No separator at all yields the whole string
	const CharString single = S("abc");
	const CharStringSplit split2 = { .s = &single, .allocator = t->alloc, .result = &parts };

	if (CharString_splitSensitive(&split2, ',', &t->err)) {
		Test_assert(t, "split without separator", parts.length == 1 && Test_strEq(parts.ptr[0], "abc"));
		ListCharString_free(&parts, t->alloc);
	}
	else Test_assert(t, "split single", false);

	//splitString on a multi character separator
	const CharString s3 = S("a::b::c");
	const CharString sep = S("::");
	const CharStringSplit split3 = { .s = &s3, .allocator = t->alloc, .result = &parts };

	if (CharString_splitStringSensitive(&split3, &sep, &t->err)) {
		Test_assert(t, "splitString count", parts.length == 3);
		Test_assert(t, "splitString[1]", parts.length > 1 && Test_strEq(parts.ptr[1], "b"));
		ListCharString_free(&parts, t->alloc);
	}
	else Test_assert(t, "splitString", false);

	//splitLine has to cope with both line ending conventions
	const CharString lines = S("one\ntwo\r\nthree");

	if (CharString_splitLine(lines, t->alloc, &parts, &t->err)) {
		Test_assert(t, "splitLine count", parts.length == 3);
		Test_assert(t, "splitLine[0]", parts.length > 0 && Test_strEq(parts.ptr[0], "one"));
		Test_assert(t, "splitLine strips CR", parts.length > 1 && Test_strEq(parts.ptr[1], "two"));
		Test_assert(t, "splitLine[2]", parts.length > 2 && Test_strEq(parts.ptr[2], "three"));
		ListCharString_free(&parts, t->alloc);
	}
	else Test_assert(t, "splitLine", false);
}

//========================= ListCharString =========================

static void Test_stringList(Test *t) {

	Test_setModule(t, "ListCharString");

	ListCharString list = (ListCharString) { 0 };
	Bool ok = true;

	ok &= ListCharString_pushBack(&list, S("banana"), t->alloc, &t->err);
	ok &= ListCharString_pushBack(&list, S("Apple"), t->alloc, &t->err);
	ok &= ListCharString_pushBack(&list, S("cherry"), t->alloc, &t->err);
	Test_assert(t, "pushBack refs", ok && list.length == 3);

	//A deep copy has to duplicate the memory, so freeing the original leaves the copy intact
	ListCharString copy = (ListCharString) { 0 };

	if (ListCharString_createCopyUnderlying(&list, t->alloc, &copy, &t->err)) {

		Test_assert(t, "copyUnderlying count", copy.length == 3);
		Test_assert(t, "copyUnderlying owns its memory", !CharString_isRef(copy.ptr[0]));
		Test_assert(t, "copyUnderlying doesn't alias", copy.ptr[0].ptr != list.ptr[0].ptr);
		Test_assert(t, "copyUnderlying content", Test_strEq(copy.ptr[0], "banana"));

		ListCharString_freeUnderlying(&copy, t->alloc);
		Test_assert(t, "freeUnderlying empties", !copy.length && !copy.ptr);
	}
	else Test_assert(t, "copyUnderlying", false);

	//Sorting is by content; sensitive puts uppercase first, insensitive doesn't
	Test_assert(t, "sortSensitive", ListCharString_sortSensitive(list));
	Test_assert(t, "sortSensitive puts uppercase first", Test_strEq(list.ptr[0], "Apple"));

	Test_assert(t, "sortInsensitive", ListCharString_sortInsensitive(list));
	Test_assert(t, "sortInsensitive order", (
		Test_strEq(list.ptr[0], "Apple") && Test_strEq(list.ptr[1], "banana") && Test_strEq(list.ptr[2], "cherry")
	));

	ListCharString_free(&list, t->alloc);
	Test_assert(t, "free of a ref list", !list.length);
}

//========================= validation and parsing =========================

static void Test_stringValidate(Test *t) {

	Test_setModule(t, "CharString validate");

	Test_assert(t, "isHex", CharString_isHex(S("0x1F")) && !CharString_isHex(S("0xZZ")));
	Test_assert(t, "isDec", CharString_isDec(S("1234")) && !CharString_isDec(S("12a4")));
	Test_assert(t, "isOct", CharString_isOct(S("0o707")) && !CharString_isOct(S("0o909")));
	Test_assert(t, "isBin", CharString_isBin(S("0b1010")) && !CharString_isBin(S("0b1210")));
	Test_assert(t, "isAlphaNumeric", CharString_isAlphaNumeric(S("abc123")) && !CharString_isAlphaNumeric(S("abc-123")));

	Test_assert(t, "isUnsignedNumber", CharString_isUnsignedNumber(S("123")));
	Test_assert(t, "isSignedNumber", CharString_isSignedNumber(S("-123")));
	Test_assert(t, "isFloat", CharString_isFloat(S("1.5")) && CharString_isFloat(S("-1.5e3")));
	Test_assert(t, "isFloat rejects text", !CharString_isFloat(S("abc")));

	Test_assert(t, "isValidFileName", CharString_isValidFileName(S("file.txt")));
	Test_assert(t, "isValidFileName rejects slash", !CharString_isValidFileName(S("dir/file.txt")));
	Test_assert(t, "isValidFilePath", CharString_isValidFilePath(S("dir/file.txt")));

	Test_assert(t, "isValidAscii", CharString_isValidAscii(S("plain ascii")));

	//The parses the existing number test doesn't cover
	U64 u = 0;
	Test_assert(t, "parseU64", CharString_parseU64(S("0x10"), &u) && u == 16);
	Test_assert(t, "parseU64 decimal", CharString_parseU64(S("42"), &u) && u == 42);

	I64 i = 0;
	Test_assert(t, "parseDecSigned negative", CharString_parseDecSigned(S("-42"), &i) && i == -42);
	Test_assert(t, "parseDecSigned positive", CharString_parseDecSigned(S("42"), &i) && i == 42);

	F32 f = 0;
	Test_assert(t, "parseFloat", CharString_parseFloat(S("1.5"), &f) && f == 1.5f);
	Test_assert(t, "parseFloat negative", CharString_parseFloat(S("-0.25"), &f) && f == -0.25f);
	Test_assert(t, "parseFloat rejects text", !CharString_parseFloat(S("abc"), &f));

	F64 d = 0;
	Test_assert(t, "parseDouble", CharString_parseDouble(S("2.5"), &d) && d == 2.5);

	//Codepoint counting; U64_MAX signals invalid UTF-8
	Test_assert(t, "unicodeCodepoints ascii", CharString_unicodeCodepoints(S("abc")) == 3);
}

void Test_stringOps(Test *t) {
	Test_stringRefs(t);
	Test_stringMutate(t);
	Test_stringReplace(t);
	Test_stringSearch(t);
	Test_stringCut(t);
	Test_stringSplit(t);
	Test_stringList(t);
	Test_stringValidate(t);
}
