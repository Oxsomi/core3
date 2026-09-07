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

//formats/oiSR/test/test_oiSR_json.c

#include "test_oiSR_shared.h"
#include "formats/oiSR/sr_file.h"
#include "formats/json/json_writer.h"
#include "types/container/string.h"
#include "types/base/string_read_helper.h"

static Bool jsonHas(const CharString *doc, const C8 *needle) {
	const CharString n = CharString_createRefCStrConst(needle);
	return CharString_findFirstStringSensitive(doc, &n, 0, 0) != U64_MAX;
}

//The sample tree, pinned by the fragments a reader keys on: the header, one node of each kind it holds, the type
// with both spellings and its array shape, and the empty builtin summary

void Test_SRFileWriteJson(Test *t) {

	Test_setModule(t, "SRFile: JSON view");

	SRFile sr = (SRFile) { 0 };
	CharString whole = CharString_createNull();
	CharString wrapped = CharString_createNull();

	if(!Test_assert(t, "build", Test_SRBuildSample(t, &sr)))
		goto clean;

	JsonWriter w = JsonWriter_create(&whole, false, t->alloc);

	Test_assert(t, "writes a complete document", SRFile_writeJson(&sr, &w, t->alloc, &t->err) && JsonWriter_isComplete(&w));

	Test_assert(t, "the hash travels as hex text", jsonHas(&whole, "\"hash\":\""));

	Test_assert(t, "the header", jsonHas(
		&whole,
		"\"header\":{\"version\":\"1.1\",\"flags\":{\"hasSymbols\":true},"
		"\"features\":[\"Basics\",\"Functions\",\"Namespaces\",\"UserTypes\",\"Scopes\",\"SymbolInfo\"],"
		"\"counts\":{\"nodes\":6,\"annotations\":1,\"registers\":0,\"enumValues\":0,\"types\":1,\"arrayDims\":2,"
		"\"interfaces\":0}}"
	));

	Test_assert(t, "the namespace root", jsonHas(
		&whole,
		"\"nodes\":[{\"id\":0,\"kind\":\"Namespace\",\"name\":\"MyNS\",\"parent\":-1,\"children\":[1],"
		"\"loc\":{\"file\":\"test.hlsl\",\"line\":1,\"col\":1,\"len\":7,\"lines\":1},\"annotations\":[],\"type\":null,"
		"\"implements\":[],\"arrays\":[],\"semantic\":null,\"direction\":null,\"register\":null,\"enumValue\":null,"
		"\"entry\":null,\"returns\":false,\"builtin\":false}"
	));

	Test_assert(t, "the typed variable", jsonHas(
		&whole, "\"id\":2,\"kind\":\"Variable\",\"name\":\"pos\",\"parent\":1,\"children\":[]"
	));

	Test_assert(t, "its type names both spellings", jsonHas(
		&whole, "\"type\":{\"name\":\"float3\",\"display\":\"vec3\",\"cls\":\""
	));

	Test_assert(t, "and its declaring node and array shape", jsonHas(
		&whole, "\"rows\":1,\"cols\":3,\"def\":1,\"base\":-1},\"implements\":[],\"arrays\":[2,3]"
	));

	Test_assert(t, "the function keeps its annotation's brackets", jsonHas(
		&whole,
		"\"kind\":\"Function\",\"name\":\"main\",\"parent\":-1,\"children\":[5],"
		"\"loc\":{\"file\":\"test.hlsl\",\"line\":5,\"col\":1,\"len\":7,\"lines\":1},\"annotations\":[\"[shader]\"]"
	));

	Test_assert(t, "the parameter", jsonHas(
		&whole,
		"\"kind\":\"Parameter\",\"name\":\"uv\",\"parent\":4,\"children\":[],"
		"\"loc\":{\"file\":\"test.hlsl\",\"line\":6,\"col\":1,\"len\":7,\"lines\":1},\"annotations\":[],\"type\":null,"
		"\"implements\":[],\"arrays\":[],\"semantic\":\"TEXCOORD0\",\"direction\":\"in\""
	));

	Test_assert(t, "nothing comes from a builtin include", jsonHas(&whole, "\"builtinCollapsed\":[]}"));

	//The members form lands inside an object the embedder opened, which is how a frame puts a name before them

	JsonWriter wm = JsonWriter_create(&wrapped, false, t->alloc);

	Test_assert(t, "the members write into an open object",
		JsonWriter_beginObject(&wm, &t->err) &&
		JsonWriter_keyCstr(&wm, "name", "x", &t->err) &&
		SRFile_writeJsonMembers(&sr, &wm, t->alloc, &t->err) &&
		JsonWriter_endObject(&wm, &t->err) && JsonWriter_isComplete(&wm)
	);

	Test_assert(t, "after the embedder's own keys", CharString_startsWithCStringSensitive(
		&wrapped, "{\"name\":\"x\",\"compilerVersion\":{", 0
	));

	Test_assert(t, "a missing file is refused", !SRFile_writeJson(NULL, &w, t->alloc, NULL));

clean:
	CharString_free(&whole, t->alloc);
	CharString_free(&wrapped, t->alloc);
	SRFile_free(&sr, t->alloc);
}
