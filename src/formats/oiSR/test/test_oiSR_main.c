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

//formats/oiSR/test/test_oiSR_main.c

#include "test_oiSR_shared.h"
#include "formats/oiSR/sr_file.h"
#include "types/test/test.h"
#include "types/container/memory_stream.h"
#include "types/container/test/basic_alloc.h"
#include "types/container/string.h"
#include "types/base/string_base.h"
#include "types/base/string_read_helper.h"

//Build a small but representative reflection tree:
//  Namespace "MyNS" { Struct "Light" { Variable "pos"; Variable "color"; } }
//  Function "main" [shader] ( Parameter "uv" : TEXCOORD0 )
static Bool buildSample(Test *t, SRFile *sr) {

	if(!SRFile_create(
		ESRSettingsFlags_HasSymbols, ESRFeature_All | ESRFeature_SymbolInfo, t->alloc, sr, &t->err
	))
		return false;

	U32 nsName = 0, lightName = 0, posName = 0, colName = 0, mainName = 0, uvName = 0, sem = 0, anno = 0, file = 0;
	U32 f3Name = 0, v3Name = 0;

	CharString sNs    = CharString_createRefCStrConst("MyNS");
	CharString sLight = CharString_createRefCStrConst("Light");
	CharString sPos   = CharString_createRefCStrConst("pos");
	CharString sCol   = CharString_createRefCStrConst("color");
	CharString sMain  = CharString_createRefCStrConst("main");
	CharString sUv    = CharString_createRefCStrConst("uv");
	CharString sSem   = CharString_createRefCStrConst("TEXCOORD0");
	CharString sAnno  = CharString_createRefCStrConst("shader");
	CharString sFile  = CharString_createRefCStrConst("test.hlsl");
	CharString sF3    = CharString_createRefCStrConst("float3");
	CharString sV3    = CharString_createRefCStrConst("vec3");

	Bool ok = true;
	ok &= SRFile_addString(sr, &sNs,    t->alloc, &nsName,    &t->err);
	ok &= SRFile_addString(sr, &sLight, t->alloc, &lightName, &t->err);
	ok &= SRFile_addString(sr, &sPos,   t->alloc, &posName,   &t->err);
	ok &= SRFile_addString(sr, &sCol,   t->alloc, &colName,   &t->err);
	ok &= SRFile_addString(sr, &sMain,  t->alloc, &mainName,  &t->err);
	ok &= SRFile_addString(sr, &sUv,    t->alloc, &uvName,    &t->err);
	ok &= SRFile_addString(sr, &sSem,   t->alloc, &sem,       &t->err);
	ok &= SRFile_addString(sr, &sAnno,  t->alloc, &anno,      &t->err);
	ok &= SRFile_addString(sr, &sFile,  t->alloc, &file,      &t->err);
	ok &= SRFile_addString(sr, &sF3,    t->alloc, &f3Name,    &t->err);
	ok &= SRFile_addString(sr, &sV3,    t->alloc, &v3Name,    &t->err);

	if(!ok)
		return false;

	//Annotation table: [shader] on the function

	SRAnnotation shaderAnno = (SRAnnotation) { .nameId = anno, .isBuiltin = true };

	if(!ListSRAnnotation_pushBack(&sr->annotations, shaderAnno, t->alloc, &t->err))
		return false;

	//Nodes (node index order matters: parents come before children)

	SRNode nodes[6] = {
		//0: namespace MyNS
		{ .nameId = nsName,    .semanticId = U32_MAX, .localId = U32_MAX, .parent = U32_MAX, .fwdBckDeclareNode = U32_MAX,
		  .childCount = 1, .annotationStart = U32_MAX, .annotationCount = 0, .type = ESRNodeType_Namespace },
		//1: struct Light (parent 0)
		{ .nameId = lightName, .semanticId = U32_MAX, .localId = U32_MAX, .parent = 0, .fwdBckDeclareNode = U32_MAX,
		  .childCount = 2, .annotationStart = U32_MAX, .annotationCount = 0, .type = ESRNodeType_Struct },
		//2: variable pos (parent 1)
		{ .nameId = posName,   .semanticId = U32_MAX, .localId = U32_MAX, .parent = 1, .fwdBckDeclareNode = U32_MAX,
		  .childCount = 0, .annotationStart = U32_MAX, .annotationCount = 0, .type = ESRNodeType_Variable },
		//3: variable color (parent 1)
		{ .nameId = colName,   .semanticId = U32_MAX, .localId = U32_MAX, .parent = 1, .fwdBckDeclareNode = U32_MAX,
		  .childCount = 0, .annotationStart = U32_MAX, .annotationCount = 0, .type = ESRNodeType_Variable },
		//4: function main [shader] (root)
		{ .nameId = mainName,  .semanticId = U32_MAX, .localId = U32_MAX, .parent = U32_MAX, .fwdBckDeclareNode = U32_MAX,
		  .childCount = 1, .annotationStart = 0, .annotationCount = 1, .type = ESRNodeType_Function },
		//5: parameter uv : TEXCOORD0 (parent 4)
		{ .nameId = uvName,    .semanticId = sem,     .localId = U32_MAX, .parent = 4, .fwdBckDeclareNode = U32_MAX,
		  .childCount = 0, .annotationStart = U32_MAX, .annotationCount = 0, .type = ESRNodeType_Parameter }
	};

	for(U32 i = 0; i < 6; ++i) {

		if(!ListSRNode_pushBack(&sr->nodes, nodes[i], t->alloc, &t->err))
			return false;

		//One symbol per node (parallel array), all in test.hlsl on line i+1

		SRSymbol sym = (SRSymbol) {
			.fileNameId = file, .line = i + 1, .lineCount = 1, .columnStart = 1, .columnEnd = 8
		};

		if(!ListSRSymbol_pushBack(&sr->symbols, sym, t->alloc, &t->err))
			return false;
	}

	//Two array-dimension lengths [2][3] shared by the type record below, so the arrayDims pool round-trips.

	if(!ListU32_pushBack(&sr->arrayDims, 2, t->alloc, &t->err) || !ListU32_pushBack(&sr->arrayDims, 3, t->alloc, &t->err))
		return false;

	//A type record for the pos variable (node 2): underlying float3 written as the alias vec3 (so both name paths
	//round-trip), shaped as a [2][3] array (elements = flattened 6, dims from the pool) and pointed at the Light struct
	//(node 1) as its go-to-definition target, so the type-graph + array fields are all exercised through write/read.

	SRType posType = (SRType) {
		.nodeId = 2, .typeClass = ESRTypeClass_Vector, .rows = 1, .cols = 3,
		.arrayDimCount = 2, .elements = 6,
		.typeNameId = f3Name, .displayNameId = v3Name,
		.defNodeId = 1, .baseNodeId = U32_MAX, .arrayDimStart = 0
	};

	if(!ListSRType_pushBack(&sr->types, posType, t->alloc, &t->err))
		return false;

	return SRFile_finalize(sr, t->alloc, &t->err);
}

//Write then read back an SRFile. Caller owns *archiveSr (RefPtr_dec) and *result (SRFile_free).
static Bool srRoundTrip(Test *t, const SRFile *src, Bool subFile, StreamRef **archiveSr, SRFile *result, const RefPtrType *type) {

	StreamRef *sr = NULL;

	if(!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &sr, &t->err))
		return false;

	U64 off = 0;

	if(!SRFile_write(src, t->alloc, sr, &off, &t->err)) {
		RefPtr_dec(&sr);
		return false;
	}

	U64 readOff = 0;

	if(!SRFile_read(sr, &readOff, subFile, t->alloc, result, &t->err)) {
		RefPtr_dec(&sr);
		return false;
	}

	*archiveSr = sr;
	return true;
}

static void assertSampleContent(Test *t, const SRFile *r) {

	Test_assert(t, "node count",       r->nodes.length == 6);
	Test_assert(t, "symbol count",     r->symbols.length == 6);
	Test_assert(t, "annotation count", r->annotations.length == 1);
	Test_assert(t, "has symbols flag", (r->flags & ESRSettingsFlags_HasSymbols) != 0);
	Test_assert(t, "symbol feature",   (r->features & ESRFeature_SymbolInfo) != 0);

	if(r->nodes.length != 6)
		return;

	Test_assert(t, "node0 is namespace", r->nodes.ptr[0].type == ESRNodeType_Namespace);
	Test_assert(t, "node1 is struct",    r->nodes.ptr[1].type == ESRNodeType_Struct);
	Test_assert(t, "node1 parent is 0",  r->nodes.ptr[1].parent == 0);
	Test_assert(t, "node2 parent is 1",  r->nodes.ptr[2].parent == 1);
	Test_assert(t, "node3 parent is 1",  r->nodes.ptr[3].parent == 1);
	Test_assert(t, "node4 is function",  r->nodes.ptr[4].type == ESRNodeType_Function);
	Test_assert(t, "node4 is root",      r->nodes.ptr[4].parent == U32_MAX);
	Test_assert(t, "node4 has annot",    r->nodes.ptr[4].annotationCount == 1);
	Test_assert(t, "node5 is parameter", r->nodes.ptr[5].type == ESRNodeType_Parameter);
	Test_assert(t, "node5 has semantic", r->nodes.ptr[5].semanticId != U32_MAX);

	CharString light = CharString_createRefCStrConst("Light");
	Test_assert(t, "struct name resolves",
		r->nodes.ptr[1].nameId < r->names.entryStrings.length &&
		CharString_equalsStringSensitive(&r->names.entryStrings.ptr[r->nodes.ptr[1].nameId], &light)
	);

	CharString sem = CharString_createRefCStrConst("TEXCOORD0");
	Test_assert(t, "semantic resolves",
		CharString_equalsStringSensitive(&r->names.entryStrings.ptr[r->nodes.ptr[5].semanticId], &sem)
	);

	if(r->symbols.length == 6)
		Test_assert(t, "node4 symbol line", r->symbols.ptr[4].line == 5);

	//Type tier: the pos variable (node 2) round-tripped as float3 (vector 1x3)

	Test_assert(t, "type count", r->types.length == 1);

	if(r->types.length == 1) {

		Test_assert(t, "type keys node 2", r->types.ptr[0].nodeId == 2);
		Test_assert(t, "type is vector 1x3",
			r->types.ptr[0].typeClass == ESRTypeClass_Vector && r->types.ptr[0].rows == 1 && r->types.ptr[0].cols == 3);

		CharString f3 = CharString_createRefCStrConst("float3");
		Test_assert(t, "underlying type name resolves to float3",
			r->types.ptr[0].typeNameId < r->names.entryStrings.length &&
			CharString_equalsStringSensitive(&r->names.entryStrings.ptr[r->types.ptr[0].typeNameId], &f3));

		CharString v3 = CharString_createRefCStrConst("vec3");
		Test_assert(t, "display type name resolves to vec3",
			r->types.ptr[0].displayNameId < r->names.entryStrings.length &&
			CharString_equalsStringSensitive(&r->names.entryStrings.ptr[r->types.ptr[0].displayNameId], &v3));

		//Go-to-definition points at the Light struct (node 1)

		Test_assert(t, "type def node is the struct", r->types.ptr[0].defNodeId == 1 &&
			r->nodes.ptr[r->types.ptr[0].defNodeId].type == ESRNodeType_Struct);

		//Multi-dimensional array [2][3] round-tripped through the shared pool

		Test_assert(t, "type has 2 array dims", r->types.ptr[0].arrayDimCount == 2 && r->types.ptr[0].arrayDimStart == 0);
		Test_assert(t, "array dims are [2][3]",
			r->arrayDims.length == 2 && r->arrayDims.ptr[0] == 2 && r->arrayDims.ptr[1] == 3);
	}
}

static void Test_SRFileRoundTrip(Test *t) {

	Test_setModule(t, "SRFile round-trip: full file");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	SRFile sr = { 0 }, result = { 0 };
	StreamRef *archiveSr = NULL;

	if(!buildSample(t, &sr)) {
		Test_assert(t, "build sample", false);
		goto clean;
	}

	if(!srRoundTrip(t, &sr, false, &archiveSr, &result, &type)) {
		Test_assert(t, "round-trip", false);
		goto clean;
	}

	assertSampleContent(t, &result);
	Test_assert(t, "hash preserved", result.hash == sr.hash);

clean:
	SRFile_free(&sr, t->alloc);
	SRFile_free(&result, t->alloc);
	RefPtr_dec(&archiveSr);
}

static void Test_SRFileSubFileRoundTrip(Test *t) {

	Test_setModule(t, "SRFile round-trip: sub-file (no magic)");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	SRFile sr = { 0 }, result = { 0 };
	StreamRef *stream = NULL;

	if(!buildSample(t, &sr)) {
		Test_assert(t, "build sample", false);
		goto clean;
	}

	//Force the sub-file (HideMagicNumber) representation by writing with the flag set

	sr.flags |= ESRSettingsFlags_HideMagicNumber;

	if(!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &stream, &t->err)) {
		Test_assert(t, "create stream", false);
		goto clean;
	}

	U64 off = 0;
	Test_assert(t, "write sub-file", SRFile_write(&sr, t->alloc, stream, &off, &t->err));

	U64 readOff = 0;
	Test_assert(t, "read sub-file", SRFile_read(stream, &readOff, true, t->alloc, &result, &t->err));

	Test_assert(t, "sub-file hide magic", (result.flags & ESRSettingsFlags_HideMagicNumber) != 0);
	assertSampleContent(t, &result);
	Test_assert(t, "hash preserved", result.hash == sr.hash);

clean:
	SRFile_free(&sr, t->alloc);
	SRFile_free(&result, t->alloc);
	RefPtr_dec(&stream);
}

static void Test_SRFileCreateCopy(Test *t) {

	Test_setModule(t, "SRFile createCopy: independent deep copy");

	SRFile sr = { 0 }, copy = { 0 };

	if(!buildSample(t, &sr)) {
		Test_assert(t, "build sample", false);
		goto clean;
	}

	Test_assert(t, "createCopy", SRFile_createCopy(&sr, t->alloc, &copy, &t->err));
	Test_assert(t, "copy nodes",  copy.nodes.length == sr.nodes.length);
	Test_assert(t, "copy hash",   copy.hash == sr.hash);
	assertSampleContent(t, &copy);

clean:
	SRFile_free(&sr, t->alloc);
	SRFile_free(&copy, t->alloc);
}

static void Test_SRFileSizeConsistency(Test *t) {

	Test_setModule(t, "SRFile size consistency (length-only == streamed)");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	SRFile sr = { 0 };
	MemoryStreamRef *ms = NULL;

	if(!buildSample(t, &sr)) {
		Test_assert(t, "build sample", false);
		goto clean;
	}

	U64 sizeOnly = 0;
	Test_assert(t, "length-only write", SRFile_write(&sr, t->alloc, NULL, &sizeOnly, &t->err));

	if(!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &ms, &t->err)) {
		Test_assert(t, "create stream", false);
		goto clean;
	}

	U64 streamSize = 0;
	Test_assert(t, "stream write", SRFile_write(&sr, t->alloc, ms, &streamSize, &t->err));
	Test_assert(t, "sizes match", sizeOnly == streamSize);

clean:
	SRFile_free(&sr, t->alloc);
	RefPtr_dec(&ms);
}

static void Test_SRFileAddStringDedup(Test *t) {

	Test_setModule(t, "SRFile addString: dedup + empty handling");

	SRFile sr = { 0 };

	if(!SRFile_create(ESRSettingsFlags_None, ESRFeature_All, t->alloc, &sr, &t->err)) {
		Test_assert(t, "create", false);
		goto clean;
	}

	CharString a = CharString_createRefCStrConst("foo");
	CharString b = CharString_createRefCStrConst("foo");
	CharString empty = CharString_createNull();

	U32 idA = 1, idB = 2, idEmpty = 3;
	Test_assert(t, "add foo",       SRFile_addString(&sr, &a, t->alloc, &idA, &t->err));
	Test_assert(t, "add foo again", SRFile_addString(&sr, &b, t->alloc, &idB, &t->err));
	Test_assert(t, "add empty",     SRFile_addString(&sr, &empty, t->alloc, &idEmpty, &t->err));

	Test_assert(t, "dedup same id",  idA == idB);
	Test_assert(t, "empty is U32_MAX", idEmpty == U32_MAX);
	Test_assert(t, "one string only", sr.names.entryStrings.length == 1);

clean:
	SRFile_free(&sr, t->alloc);
}

int main() {

	const Allocator alloc = BasicAllocator_instance;
	Test t = (Test) { 0 };
	t.alloc = &alloc;

	Test_SRFileRoundTrip(&t);
	Test_SRFileSubFileRoundTrip(&t);
	Test_SRFileCreateCopy(&t);
	Test_SRFileSizeConsistency(&t);
	Test_SRFileAddStringDedup(&t);

	Test_SRReadHeaderTamper(&t);
	Test_SRReadNodeFieldBounds(&t);
	Test_SRReadSymbolAndAnnotationBounds(&t);
	Test_SRReadFwdDeclare(&t);
	Test_SRReadStreamShape(&t);
	Test_SRFileStructuralRoundTrips(&t);
	Test_SRFileHashAndName(&t);
	Test_SRFileCreateAndWriteGuards(&t);

	BasicAllocator_checkLeakedMem(&t);
	return Test_end(&t);
}
