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

//formats/oiCA/test/test_oiCA_json.c

#include "test_oiCA_shared.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_edit.h"
#include "formats/oiCA/ca_lookup.h"
#include "formats/oiCA/ca_props.h"
#include "formats/json/json_writer.h"
#include "types/container/memory_stream.h"
#include "types/container/ref_ptr.h"
#include "types/container/string.h"
#include "types/base/string_read_helper.h"

extern const CASettings kCASettings;

static Bool caJsonHas(const CharString *doc, const C8 *needle) {
	const CharString n = CharString_createRefCStrConst(needle);
	return CharString_findFirstStringSensitive(doc, &n, 0, 0) != U64_MAX;
}

//The table is what an archive is asked about, so the shape of a folder, a file with data and a file without
//is pinned, along with the counts the header reports.

void Test_CAWriteJson(Test *t) {

	Test_setModule(t, "CAFile: JSON view");

	CAFile ca = (CAFile) { 0 };
	CharString out = CharString_createNull();
	Buffer data = Buffer_createNull();

	if(!Test_assert(t, "create", CAFile_create(&kCASettings, 0, 0, t->alloc, &ca, &t->err)))
		goto clean;

	CharString folderName = CharString_createRefCStrConst("shaders");
	const CAHandle folder = CAFile_addFolder(&ca, CAHandle_Root, &folderName, t->alloc, &t->err);

	if(!Test_assert(t, "add folder", folder != CAHandle_Invalid))
		goto clean;

	CharString fileName = CharString_createRefCStrConst("a.hlsl");
	const CAHandle file = CAFile_addFile(&ca, folder, &fileName, 0, t->alloc, &t->err);

	if(!Test_assert(t, "add file", file != CAHandle_Invalid))
		goto clean;

	static const U8 bytes[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

	if(!Test_assert(t, "copy", Buffer_createCopy(Buffer_createRefConst(bytes, sizeof(bytes)), t->alloc, &data, &t->err)))
		goto clean;

	Test_assert(t, "set data", CAFile_setData(&ca, file, t->alloc, &data, &t->err));

	JsonWriter w = JsonWriter_create(&out, false, t->alloc);

	Test_assert(t, "writes a complete document",
		CAFile_writeJson(&ca, false, &w, t->alloc, &t->err) && JsonWriter_isComplete(&w));

	Test_assert(t, "the header counts what is in it", caJsonHas(
		&out,
		"{\"header\":{\"encryption\":\"None\",\"compression\":\"None\",\"hasDate\":false,\"hasFullDate\":false,"
		"\"counts\":{\"files\":1,\"folders\":1}}"
	));

	Test_assert(t, "a folder has no size, contents or time", caJsonHas(
		&out,
		"{\"path\":\"shaders\",\"type\":\"folder\",\"size\":null,\"loaded\":null,\"timestamp\":null,"
		"\"contents\":null}"
	));

	Test_assert(t, "a file reports its size and that its data is held", caJsonHas(
		&out, "{\"path\":\"shaders/a.hlsl\",\"type\":\"file\",\"size\":8,\"loaded\":true"
	));

	Test_assert(t, "no bytes travel unless they are asked for", !caJsonHas(&out, "\"contents\":\"01"));

	//Asked for, a held file's bytes travel as hex

	CharString_free(&out, t->alloc);

	{
		JsonWriter verbose = JsonWriter_create(&out, false, t->alloc);

		Test_assert(t, "writes with contents asked for",
			CAFile_writeJson(&ca, true, &verbose, t->alloc, &t->err) && JsonWriter_isComplete(&verbose));

		Test_assert(t, "as hex", caJsonHas(&out, "\"contents\":\"0102030405060708\""));
	}

	//A file the reader left as a stream reports itself as not held, which is what the rule is for

	{
		const RefPtrType memType = MemoryStream_makeType(t->alloc);
		StreamRef *sr = NULL;

		if (Test_assert(t, "stream", MemoryStream_create(8, EMemoryStreamFlags_None, &memType, &sr, &t->err))) {

			Test_assert(t, "set a stream", CAFile_setDataStream(&ca, file, t->alloc, &sr, 0, 8, &t->err));
			Test_assert(t, "which is not held", !CAFile_isLoaded(&ca, file));

			CharString_free(&out, t->alloc);
			JsonWriter streamed = JsonWriter_create(&out, false, t->alloc);

			Test_assert(t, "writes again",
				CAFile_writeJson(&ca, true, &streamed, t->alloc, &t->err) && JsonWriter_isComplete(&streamed));

			Test_assert(t, "and says the data is not held", caJsonHas(
				&out, "{\"path\":\"shaders/a.hlsl\",\"type\":\"file\",\"size\":8,\"loaded\":false"
			));

			Test_assert(t, "withholding its bytes however loudly they are asked for",
				!caJsonHas(&out, "\"contents\":\"01"));

			RefPtr_dec(&sr);
		}
	}

	Test_assert(t, "a missing archive is refused", !CAFile_writeJson(NULL, false, &w, t->alloc, NULL));

clean:
	Buffer_free(&data, t->alloc);
	CharString_free(&out, t->alloc);
	CAFile_free(&ca, t->alloc);
}
