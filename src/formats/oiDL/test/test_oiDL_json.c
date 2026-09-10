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

//formats/oiDL/test/test_oiDL_json.c

#include "test_oiDL_shared.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"
#include "formats/json/json_writer.h"
#include "types/container/memory_stream.h"
#include "types/container/ref_ptr.h"
#include "types/container/string.h"
#include "types/base/string_read_helper.h"

//The bytes are pinned: a document is read by a script or diffed against another, so a moved key is a
//contract change rather than a style choice.

static Bool dlJsonEquals(const CharString *out, const C8 *expected) {
	const CharString ref = CharString_createRefCStrConst(expected);
	return CharString_equalsStringSensitive(out, &ref);
}

void Test_DLWriteJson(Test *t) {

	Test_setModule(t, "DLFile: JSON view");

	DLFile file = (DLFile) { 0 };
	CharString out = CharString_createNull();

	//The stream's type has to outlive every reference to it, and the file still holds one until
	//clean: a RefPtr keeps the type by pointer and RefPtr_dec reads it back to free the object.
	const RefPtrType memType = MemoryStream_makeType(t->alloc);

	const DLSettings settings = { .dataType = EDLDataType_String };

	if(!Test_assert(t, "create", DLFile_create(&settings, 0, t->alloc, &file, &t->err)))
		goto clean;

	CharString first = CharString_createRefCStrConst("hello");
	CharString second = CharString_createRefCStrConst("a \"quoted\" one");

	Test_assert(t, "add first", DLFile_addEntryString(&file, &first, t->alloc, &t->err));
	Test_assert(t, "add second", DLFile_addEntryString(&file, &second, t->alloc, &t->err));

	//Without contents an entry is its shape alone, which is what a listing wants

	{
		JsonWriter w = JsonWriter_create(&out, false, t->alloc);

		Test_assert(t, "writes a complete document",
			DLFile_writeJson(&file, false, &w, &t->err) && JsonWriter_isComplete(&w));

		Test_assert(t, "its bytes are exact", dlJsonEquals(
			&out,
			"{\"header\":{\"type\":\"String\",\"encryption\":\"None\",\"compression\":\"None\","
			"\"hidesMagicNumber\":false,\"entries\":2},\"entries\":["
			"{\"id\":0,\"size\":5,\"loaded\":true,\"contents\":null},"
			"{\"id\":1,\"size\":14,\"loaded\":true,\"contents\":null}]}"
		));
	}

	//With contents a held text entry carries itself, escaped like any other string

	CharString_free(&out, t->alloc);

	{
		JsonWriter w = JsonWriter_create(&out, false, t->alloc);

		Test_assert(t, "writes with contents",
			DLFile_writeJson(&file, true, &w, &t->err) && JsonWriter_isComplete(&w));

		Test_assert(t, "the text travels escaped", dlJsonEquals(
			&out,
			"{\"header\":{\"type\":\"String\",\"encryption\":\"None\",\"compression\":\"None\","
			"\"hidesMagicNumber\":false,\"entries\":2},\"entries\":["
			"{\"id\":0,\"size\":5,\"loaded\":true,\"contents\":\"hello\"},"
			"{\"id\":1,\"size\":14,\"loaded\":true,\"contents\":\"a \\\"quoted\\\" one\"}]}"
		));
	}

	//An entry the reader left as a stream never carries its text, however loudly contents are asked for

	CharString_free(&out, t->alloc);

	{
		StreamRef *ms = NULL;

		if (Test_assert(t, "stream", MemoryStream_create(4, EMemoryStreamFlags_None, &memType, &ms, &t->err))) {

			Test_assert(t, "add a stream backed entry", DLFile_addEntryStream(&file, &ms, 0, 4, t->alloc, &t->err));
			Test_assert(t, "which is not held", !DLFile_isFullyLoaded(&file, 2));

			JsonWriter w = JsonWriter_create(&out, false, t->alloc);

			Test_assert(t, "writes with contents asked for",
				DLFile_writeJson(&file, true, &w, &t->err) && JsonWriter_isComplete(&w));

			Test_assert(t, "the stream backed entry says so and withholds its text", dlJsonEquals(
				&out,
				"{\"header\":{\"type\":\"String\",\"encryption\":\"None\",\"compression\":\"None\","
				"\"hidesMagicNumber\":false,\"entries\":3},\"entries\":["
				"{\"id\":0,\"size\":5,\"loaded\":true,\"contents\":\"hello\"},"
				"{\"id\":1,\"size\":14,\"loaded\":true,\"contents\":\"a \\\"quoted\\\" one\"},"
				"{\"id\":2,\"size\":4,\"loaded\":false,\"contents\":null}]}"
			));

			RefPtr_dec(&ms);
		}
	}

	Test_assert(t, "a missing file is refused", !DLFile_writeJson(NULL, false, NULL, NULL));

clean:
	CharString_free(&out, t->alloc);
	DLFile_free(&file, t->alloc);
}

//A data file's entries are binary, so they report their size and never their bytes, contents or not

void Test_DLWriteJsonData(Test *t) {

	Test_setModule(t, "DLFile: JSON view of a data file");

	DLFile file = (DLFile) { 0 };
	CharString out = CharString_createNull();
	Buffer entry = Buffer_createNull();

	const DLSettings settings = { .dataType = EDLDataType_Data };

	if(!Test_assert(t, "create", DLFile_create(&settings, 0, t->alloc, &file, &t->err)))
		goto clean;

	static const U8 bytes[4] = { 1, 2, 3, 4 };

	if(!Test_assert(t, "copy", Buffer_createCopy(Buffer_createRefConst(bytes, sizeof(bytes)), t->alloc, &entry, &t->err)))
		goto clean;

	Test_assert(t, "add", DLFile_addEntry(&file, &entry, t->alloc, &t->err));

	JsonWriter w = JsonWriter_create(&out, false, t->alloc);

	Test_assert(t, "writes without contents",
		DLFile_writeJson(&file, false, &w, &t->err) && JsonWriter_isComplete(&w));

	Test_assert(t, "which leaves the bytes out", dlJsonEquals(
		&out,
		"{\"header\":{\"type\":\"Data\",\"encryption\":\"None\",\"compression\":\"None\","
		"\"hidesMagicNumber\":false,"
		"\"entries\":1},\"entries\":[{\"id\":0,\"size\":4,\"loaded\":true,\"contents\":null}]}"
	));

	//Asked for, a held entry's bytes travel as hex, the way file data prints binary

	CharString_free(&out, t->alloc);

	{
		JsonWriter verbose = JsonWriter_create(&out, false, t->alloc);

		Test_assert(t, "writes with contents asked for",
			DLFile_writeJson(&file, true, &verbose, &t->err) && JsonWriter_isComplete(&verbose));

		Test_assert(t, "as hex", dlJsonEquals(
			&out,
			"{\"header\":{\"type\":\"Data\",\"encryption\":\"None\",\"compression\":\"None\","
		"\"hidesMagicNumber\":false,"
			"\"entries\":1},\"entries\":[{\"id\":0,\"size\":4,\"loaded\":true,\"contents\":\"01020304\"}]}"
		));
	}

clean:
	Buffer_free(&entry, t->alloc);
	CharString_free(&out, t->alloc);
	DLFile_free(&file, t->alloc);
}
