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
#include "types/container/list_basic_types.h"
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

//What belongs to each node, answered without searching for it. The sample tree pins the shapes; the wide tree
//pins the grouping, which is where an off by one would put a node's children under its neighbour.

void Test_SRNodeIndex(Test *t) {

	Test_setModule(t, "SRFile: the node index");

	SRFile sr = (SRFile) { 0 };
	SRNodeIndex index = (SRNodeIndex) { 0 };

	if(!Test_assert(t, "build", Test_SRBuildSample(t, &sr)))
		goto clean;

	if(!Test_assert(t, "index", SRFile_createNodeIndex(&sr, t->alloc, &index, &t->err)))
		goto clean;

	Test_assert(t, "it covers every node",
		index.typeAt.length == sr.nodes.length && index.childOffset.length == sr.nodes.length + 1
	);

	//Six nodes, two of them roots, so exactly four child slots: an out of range parent never counts

	Test_assert(t, "the children list is exactly the child total",
		index.children.length == sr.nodes.length - 2 &&
		index.children.length == index.childOffset.ptr[sr.nodes.length]
	);

	Test_assert(t, "the interface list is empty rather than zero filled", !index.interfaces.length);

	//Namespace MyNS holds Light, which holds pos and color; main holds uv

	Test_assert(t, "the namespace has one child",
		index.childOffset.ptr[1] - index.childOffset.ptr[0] == 1 &&
		index.children.ptr[index.childOffset.ptr[0]] == 1
	);

	Test_assert(t, "the struct has both of its members, in order",
		index.childOffset.ptr[2] - index.childOffset.ptr[1] == 2 &&
		index.children.ptr[index.childOffset.ptr[1]] == 2 &&
		index.children.ptr[index.childOffset.ptr[1] + 1] == 3
	);

	Test_assert(t, "a leaf has none", index.childOffset.ptr[3] == index.childOffset.ptr[2]);

	Test_assert(t, "the function has its parameter",
		index.childOffset.ptr[5] - index.childOffset.ptr[4] == 1 &&
		index.children.ptr[index.childOffset.ptr[4]] == 5
	);

	//The one type record names node 2, and nothing else has a satellite

	Test_assert(t, "the typed node finds its type", index.typeAt.ptr[2] == 0);
	Test_assert(t, "an untyped node has none", index.typeAt.ptr[0] == U32_MAX && index.typeAt.ptr[5] == U32_MAX);

	Test_assert(t, "no node has a register or an enum value",
		index.registerAt.ptr[2] == U32_MAX && index.enumValueAt.ptr[2] == U32_MAX);

	Test_assert(t, "and none implements an interface", index.interfaceOffset.ptr[sr.nodes.length] == 0);

	Test_assert(t, "indexing an already built index is refused", !SRFile_createNodeIndex(&sr, t->alloc, &index, NULL));

	Test_assert(t, "and leaves the index it refused untouched",
		index.typeAt.length == sr.nodes.length && index.typeAt.ptr[2] == 0 &&
		index.children.ptr && index.children.ptr[index.childOffset.ptr[0]] == 1
	);

	//Two records naming one node is what a scan resolves by stopping at the first, so the index does the same

	SRNodeIndex duplicated = (SRNodeIndex) { 0 };
	SRType second = sr.types.ptr[0];
	second.rows = 9;

	if (Test_assert(t, "a second record for the same node", ListSRType_pushBack(&sr.types, second, t->alloc, &t->err))) {

		Test_assert(t, "reindex", SRFile_createNodeIndex(&sr, t->alloc, &duplicated, &t->err));
		Test_assert(t, "the first record still wins", duplicated.typeAt.ptr[2] == 0);
		SRNodeIndex_free(&duplicated, t->alloc);
	}

clean:
	SRNodeIndex_free(&index, t->alloc);
	SRFile_free(&sr, t->alloc);
}

//A parent with many children, which is the case the grouping has to get right

void Test_SRNodeIndexWide(Test *t) {

	Test_setModule(t, "SRFile: the node index over a wide tree");

	SRFile sr = (SRFile) { 0 };
	SRNodeIndex index = (SRNodeIndex) { 0 };
	const U32 count = 256;

	if(!Test_assert(t, "create", SRFile_create(ESRSettingsFlags_None, ESRFeature_All, t->alloc, &sr, &t->err)))
		goto clean;

	//One root, then every other node hanging off it, so a wrong offset shows up as a missing or shared child

	for (U32 i = 0; i < count; ++i) {

		const SRNode node = (SRNode) {
			.nameId = U32_MAX, .semanticId = U32_MAX, .localId = U32_MAX,
			.parent = i ? 0 : U32_MAX, .fwdBckDeclareNode = U32_MAX,
			.childCount = i ? 0 : count - 1, .annotationStart = U32_MAX,
			.type = i ? ESRNodeType_Variable : ESRNodeType_Namespace
		};

		if(!Test_assert(t, "node", ListSRNode_pushBack(&sr.nodes, node, t->alloc, &t->err)))
			goto clean;
	}

	if(!Test_assert(t, "index", SRFile_createNodeIndex(&sr, t->alloc, &index, &t->err)))
		goto clean;

	Test_assert(t, "the root owns every other node", index.childOffset.ptr[1] - index.childOffset.ptr[0] == count - 1);
	Test_assert(t, "and the list is exactly that long", index.children.length == count - 1);

	Bool ordered = true;

	for (U32 i = 0; i < count - 1 && ordered; ++i)
		ordered = index.children.ptr[i] == i + 1;

	Test_assert(t, "in ascending order", ordered);

	Bool leaves = true;

	for (U32 i = 1; i < count && leaves; ++i)
		leaves = index.childOffset.ptr[i] == index.childOffset.ptr[i + 1];

	Test_assert(t, "and none of them owns anything", leaves);

clean:
	SRNodeIndex_free(&index, t->alloc);
	SRFile_free(&sr, t->alloc);
}
