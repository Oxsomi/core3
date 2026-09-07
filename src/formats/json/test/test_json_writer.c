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

//formats/json/test/test_json_writer.c

#include "test_json_shared.h"
#include "formats/json/json_writer.h"
#include "types/container/string.h"
#include "types/base/string_read_helper.h"

//The bytes are pinned exactly: a document is consumed by parsers and by the recording the web frontend
//commits, so a stray space or a moved comma is a change, not a style choice.

static Bool jsonEquals(const CharString *out, const C8 *expected) {
	const CharString ref = CharString_createRefCStrConst(expected);
	return CharString_equalsStringSensitive(out, &ref);
}

void Test_jsonWriter(Test *t) {

	Test_setModule(t, "JsonWriter");

	CharString out = CharString_createNull();

	//Compact: every value kind, escapes, nesting, and empty containers

	{
		JsonWriter w = JsonWriter_create(&out, false, t->alloc);

		//Half through key then value, half through the key-and-value helpers: both spell the same bytes

		const Bool ok =
			JsonWriter_beginObject(&w, &t->err) &&
			JsonWriter_key(&w, "name", &t->err) && JsonWriter_cstr(&w, "a\"b\\c\n\x01", &t->err) &&
			JsonWriter_keyU64(&w, "n", 42, &t->err) &&
			JsonWriter_keyI64(&w, "neg", -7, &t->err) &&
			JsonWriter_keyF64(&w, "f", 1.5, &t->err) &&
			JsonWriter_keyBool(&w, "t", true, &t->err) &&
			JsonWriter_keyNull(&w, "none", &t->err) &&
			JsonWriter_keyCstr(&w, "blank", NULL, &t->err) &&
			JsonWriter_keyArray(&w, "arr", &t->err) &&
				JsonWriter_u64(&w, 1, &t->err) &&
				JsonWriter_str(&w, CharString_createRefCStrConst("x"), &t->err) &&
				JsonWriter_beginObject(&w, &t->err) && JsonWriter_endObject(&w, &t->err) &&
				JsonWriter_raw(&w, "0x10", &t->err) &&
			JsonWriter_endArray(&w, &t->err) &&
			JsonWriter_keyArray(&w, "empty", &t->err) && JsonWriter_endArray(&w, &t->err) &&
			JsonWriter_endObject(&w, &t->err);

		Test_assert(t, "a compact document builds and completes", ok && JsonWriter_isComplete(&w));

		Test_assert(t, "its bytes are exact", jsonEquals(
			&out,
			"{\"name\":\"a\\\"b\\\\c\\n\\u0001\",\"n\":42,\"neg\":-7,\"f\":1.5,\"t\":true,\"none\":null,\"blank\":\"\","
			"\"arr\":[1,\"x\",{},0x10],\"empty\":[]}"
		));
	}

	//Pretty: a tab per level, a space after the colon, empty containers on their own line, closers at the
	//parent's indent

	CharString_free(&out, t->alloc);

	{
		JsonWriter w = JsonWriter_create(&out, true, t->alloc);

		const Bool ok =
			JsonWriter_beginObject(&w, &t->err) &&
			JsonWriter_key(&w, "a", &t->err) && JsonWriter_u64(&w, 1, &t->err) &&
			JsonWriter_key(&w, "b", &t->err) && JsonWriter_beginArray(&w, &t->err) &&
				JsonWriter_u64(&w, 1, &t->err) && JsonWriter_u64(&w, 2, &t->err) &&
			JsonWriter_endArray(&w, &t->err) &&
			JsonWriter_keyObject(&w, "c", &t->err) && JsonWriter_endObject(&w, &t->err) &&
			JsonWriter_endObject(&w, &t->err);

		Test_assert(t, "a pretty document builds", ok && JsonWriter_isComplete(&w));
		Test_assert(t, "its layout is exact", jsonEquals(
			&out, "{\n\t\"a\": 1,\n\t\"b\": [\n\t\t1,\n\t\t2\n\t],\n\t\"c\": {}\n}"
		));
	}

	//Misuse is refused rather than emitted, since a document that is not JSON helps nobody downstream

	CharString_free(&out, t->alloc);

	{
		JsonWriter w = JsonWriter_create(&out, false, t->alloc);

		Test_assert(t, "a value without a key inside an object is refused",
			JsonWriter_beginObject(&w, &t->err) && !JsonWriter_u64(&w, 1, NULL));

		Test_assert(t, "closing it as an array is refused", !JsonWriter_endArray(&w, NULL));
		Test_assert(t, "a key whose value is still due refuses a second key",
			JsonWriter_key(&w, "k", &t->err) && !JsonWriter_key(&w, "again", NULL));
		Test_assert(t, "and refuses to close", !JsonWriter_endObject(&w, NULL));
		Test_assert(t, "the value closes it", JsonWriter_null(&w, &t->err) && JsonWriter_endObject(&w, &t->err));
		Test_assert(t, "closing with nothing open is refused", !JsonWriter_endObject(&w, NULL));
	}

	CharString_free(&out, t->alloc);

	{
		JsonWriter w = JsonWriter_create(&out, false, t->alloc);

		Test_assert(t, "a key inside an array is refused",
			JsonWriter_beginArray(&w, &t->err) && !JsonWriter_key(&w, "k", NULL));

		Bool opened = true;

		for (U64 i = 1; i < 64 && opened; ++i)
			opened = JsonWriter_beginArray(&w, &t->err);

		Test_assert(t, "sixty four levels open", opened);
		Test_assert(t, "the sixty fifth is refused", !JsonWriter_beginArray(&w, NULL));
	}

	CharString_free(&out, t->alloc);
	Test_setModule(t, NULL);
}
