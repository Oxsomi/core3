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

//formats/oiSB/test/test_oiSB_roundtrip.c

#include "test_oiSB_shared.h"
#include "formats/oiSB/sb_file.h"
#include "types/container/memory_stream.h"
#include "types/base/string_base.h"
#include "types/base/string_read_helper.h"

//Helper: write then read back an SBFile, returns false on any failure.
//Caller owns *archiveSr (RefPtr_dec) and *result (SBFile_free).
Bool sbRoundTrip(
	Test *t,
	const SBFile *src,
	StreamRef **archiveSr,
	SBFile *result,
	const RefPtrType *type
) {
	StreamRef *sr = NULL;

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &sr, &t->err))
		return false;

	U64 off = 0;

	if (!SBFile_write(src, t->alloc, sr, &off, &t->err)) {
		RefPtr_dec(&sr);
		return false;
	}

	U64 readOff = 0;

	if (!SBFile_read(sr, &readOff, false, t->alloc, result, &t->err)) {
		RefPtr_dec(&sr);
		return false;
	}

	*archiveSr = sr;
	return true;
}

//Single F32 variable at root scope. Verifies basic write/read round-trip.
void Test_SBFileRoundTripSimpleF32(Test *t) {

	Test_setModule(t, "SBFile round-trip: single F32 variable");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		SBFile sb     = { 0 };
		SBFile result = { 0 };
		StreamRef *archiveSr = NULL;

		if (!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile", false);
			goto doneF32;
		}

		CharString name = CharString_createRefCStrConst("myFloat");
		Test_assert(t, "add F32 var",
			SBFile_addVariableAsType(&sb, &name, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err)
		);

		Test_assert(t, "F32 round-trip", sbRoundTrip(t, &sb, &archiveSr, &result, &type));

		Test_assert(t, "F32 vars count",    result.vars.length    == 1);
		Test_assert(t, "F32 structs count", result.structs.length == 0);
		Test_assert(t, "F32 bufferSize",    result.bufferSize     == 16);
		Test_assert(t, "F32 flags",         result.flags          == ESBSettingsFlags_None);

		if (result.vars.length == 1) {
			Test_assert(t, "F32 var offset",   result.vars.ptr[0].offset   == 0);
			Test_assert(t, "F32 var type",     result.vars.ptr[0].type     == ESBType_F32);
			Test_assert(t, "F32 var parentId", result.vars.ptr[0].parentId == U16_MAX);
			Test_assert(t, "F32 var structId", result.vars.ptr[0].structId == U16_MAX);
			Test_assert(t, "F32 var name",     CharString_equalsStringSensitive(&result.names.entryStrings.ptr[0], &name));
		}

	doneF32:
		SBFile_free(&sb, t->alloc);
		SBFile_free(&result, t->alloc);
		RefPtr_dec(&archiveSr);
	}
}

//Multiple scalar variables at root scope. Verifies ordering is preserved.
void Test_SBFileRoundTripMultipleVars(Test *t) {

	Test_setModule(t, "SBFile round-trip: multiple root variables");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		SBFile sb     = { 0 };
		SBFile result = { 0 };
		StreamRef *archiveSr = NULL;

		//Layout: F32 @ 0, I32 @ 4, U32x4 @ 16  (standard cbuffer packing)
		if (!SBFile_create(ESBSettingsFlags_None, 32, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile multi", false);
			goto doneMulti;
		}

		CharString nameA = CharString_createRefCStrConst("a");
		CharString nameB = CharString_createRefCStrConst("b");
		CharString nameC = CharString_createRefCStrConst("c");

		Test_assert(t, "add F32",   SBFile_addVariableAsType(
			&sb, &nameA, 0,  U16_MAX, ESBType_F32,   ESBVarFlag_None, NULL, t->alloc, &t->err
		));

		Test_assert(t, "add I32",   SBFile_addVariableAsType(
			&sb, &nameB, 4,  U16_MAX, ESBType_I32,   ESBVarFlag_None, NULL, t->alloc, &t->err
		));

		Test_assert(t, "add U32x4", SBFile_addVariableAsType(
			&sb, &nameC, 16, U16_MAX, ESBType_U32x4, ESBVarFlag_None, NULL, t->alloc, &t->err
		));

		Test_assert(t, "multi round-trip", sbRoundTrip(t, &sb, &archiveSr, &result, &type));

		Test_assert(t, "multi vars count", result.vars.length == 3);

		if (result.vars.length == 3) {

			Test_assert(t, "var0 type",   result.vars.ptr[0].type   == ESBType_F32);
			Test_assert(t, "var0 offset", result.vars.ptr[0].offset == 0);
			Test_assert(t, "var0 name", CharString_equalsStringSensitive(&result.names.entryStrings.ptr[0], &nameA));

			Test_assert(t, "var1 type",   result.vars.ptr[1].type   == ESBType_I32);
			Test_assert(t, "var1 offset", result.vars.ptr[1].offset == 4);
			Test_assert(t, "var1 name", CharString_equalsStringSensitive(&result.names.entryStrings.ptr[1], &nameB));

			Test_assert(t, "var2 type",   result.vars.ptr[2].type   == ESBType_U32x4);
			Test_assert(t, "var2 offset", result.vars.ptr[2].offset == 16);
			Test_assert(t, "var2 name", CharString_equalsStringSensitive(&result.names.entryStrings.ptr[2], &nameC));
		}

	doneMulti:
		SBFile_free(&sb, t->alloc);
		SBFile_free(&result, t->alloc);
		RefPtr_dec(&archiveSr);
	}
}

//Struct with member variables. Verifies struct registration and variable parentId linkage.
void Test_SBFileRoundTripStruct(Test *t) {

	Test_setModule(t, "SBFile round-trip: struct with members");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		SBFile sb     = { 0 };
		SBFile result = { 0 };
		StreamRef *archiveSr = NULL;

		//MyStruct { F32x4 pos; F32x4 col; } at offset 0, buffer = 32 bytes
		if (!SBFile_create(ESBSettingsFlags_None, 32, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile struct", false);
			goto doneStruct;
		}

		SBStruct myStruct = { .stride = 32 };
		CharString sName = CharString_createRefCStrConst("MyStruct");
		Test_assert(t, "add struct", SBFile_addStruct(&sb, &sName, myStruct, t->alloc, &t->err));

		//Add the root variable referencing the struct (structId = 0)
		CharString vName = CharString_createRefCStrConst("myVar");
		Test_assert(t, "add struct var",
			SBFile_addVariableAsStruct(&sb, &vName, 0, U16_MAX, 0, ESBVarFlag_None, NULL, t->alloc, &t->err)
		);

		//Add members (parentId = 0, which is the index of myVar in vars)
		CharString posName = CharString_createRefCStrConst("pos");
		CharString colName = CharString_createRefCStrConst("col");

		Test_assert(t, "add pos", SBFile_addVariableAsType(
			&sb, &posName, 0,  0, ESBType_F32x4, ESBVarFlag_None, NULL, t->alloc, &t->err
		));

		Test_assert(t, "add col", SBFile_addVariableAsType(
			&sb, &colName, 16, 0, ESBType_F32x4, ESBVarFlag_None, NULL, t->alloc, &t->err
		));

		Test_assert(t, "struct round-trip", sbRoundTrip(t, &sb, &archiveSr, &result, &type));

		Test_assert(t, "struct count", result.structs.length == 1);
		Test_assert(t, "vars count",   result.vars.length    == 3);
		Test_assert(t, "bufferSize",   result.bufferSize     == 32);

		const CharString *strs = &result.names.entryStrings.ptr[0];

		if (result.structs.length >= 1) {
			Test_assert(t, "struct stride", result.structs.ptr[0].stride == 32);
			Test_assert(t, "root var name", CharString_equalsStringSensitive(strs, &sName));
		}

		if (result.vars.length >= 3) {

			strs += result.structs.length;

			Test_assert(t, "root var structId", result.vars.ptr[0].structId == 0);
			Test_assert(t, "root var parentId", result.vars.ptr[0].parentId == U16_MAX);
			Test_assert(t, "root var offset",   result.vars.ptr[0].offset   == 0);
			Test_assert(t, "root var name",     CharString_equalsStringSensitive(strs, &vName));

			Test_assert(t, "pos parentId",      result.vars.ptr[1].parentId == 0);
			Test_assert(t, "pos type",          result.vars.ptr[1].type     == ESBType_F32x4);
			Test_assert(t, "pos name",          CharString_equalsStringSensitive(strs + 1, &posName));

			Test_assert(t, "col parentId",      result.vars.ptr[2].parentId == 0);
			Test_assert(t, "col type",          result.vars.ptr[2].type     == ESBType_F32x4);
			Test_assert(t, "col offset",        result.vars.ptr[2].offset   == 16);
			Test_assert(t, "pos name",          CharString_equalsStringSensitive(strs + 2, &colName));
		}

	doneStruct:
		SBFile_free(&sb, t->alloc);
		SBFile_free(&result, t->alloc);
		RefPtr_dec(&archiveSr);
	}
}

//Tightly packed layout flag is preserved through round-trip.
void Test_SBFileRoundTripTightlyPacked(Test *t) {

	Test_setModule(t, "SBFile round-trip: tightly packed flag");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		SBFile sb     = { 0 };
		SBFile result = { 0 };
		StreamRef *archiveSr = NULL;

		//Tightly packed: F32x3 = 12 bytes, no padding
		if (!SBFile_create(ESBSettingsFlags_IsTightlyPacked, 12, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create tightly packed SBFile", false);
			goto donePacked;
		}

		CharString name = CharString_createRefCStrConst("rgb");
		Test_assert(t, "add F32x3",
			SBFile_addVariableAsType(&sb, &name, 0, U16_MAX, ESBType_F32x3, ESBVarFlag_None, NULL, t->alloc, &t->err)
		);

		Test_assert(t, "tightly packed round-trip", sbRoundTrip(t, &sb, &archiveSr, &result, &type));

		Test_assert(t, "tightly packed flag set", result.flags & ESBSettingsFlags_IsTightlyPacked);
		Test_assert(t, "bufferSize",              result.bufferSize == 12);
		Test_assert(t, "vars count",              result.vars.length == 1);

		if (result.vars.length >= 1)
			Test_assert(t, "root var name", CharString_equalsStringSensitive(&result.names.entryStrings.ptr[0], &name));

	donePacked:
		SBFile_free(&sb, t->alloc);
		SBFile_free(&result, t->alloc);
		RefPtr_dec(&archiveSr);
	}
}

//isSubFile = true skips the magic number during read, matching a HideMagicNumber write.
void Test_SBFileSubFileRoundTrip(Test *t) {

	Test_setModule(t, "SBFile round-trip: sub-file (no magic)");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		SBFile sb     = { 0 };
		SBFile result = { 0 };
		StreamRef *sr = NULL;

		if (!SBFile_create(ESBSettingsFlags_HideMagicNumber, 16, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create sub-file SBFile", false);
			goto doneSubFile;
		}

		CharString name = CharString_createRefCStrConst("v");
		Test_assert(t, "add var",
			SBFile_addVariableAsType(&sb, &name, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err)
		);

		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &sr, &t->err)) {
			Test_assert(t, "create stream sub-file", false);
			goto doneSubFile;
		}

		U64 off = 0;
		Test_assert(t, "write sub-file", SBFile_write(&sb, t->alloc, sr, &off, &t->err));

		U64 readOff = 0;
		Test_assert(t, "read sub-file", SBFile_read(sr, &readOff, true, t->alloc, &result, &t->err));

		Test_assert(t, "sub-file vars count", result.vars.length == 1);
		Test_assert(t, "sub-file hide magic", result.flags & ESBSettingsFlags_HideMagicNumber);

		if (result.vars.length >= 1)
			Test_assert(t, "root var name", CharString_equalsStringSensitive(&result.names.entryStrings.ptr[0], &name));

	doneSubFile:
		SBFile_free(&sb, t->alloc);
		SBFile_free(&result, t->alloc);
		RefPtr_dec(&sr);
	}
}

//SBFile_createCopy produces an independent copy with identical content.
void Test_SBFileCreateCopy(Test *t) {

	Test_setModule(t, "SBFile createCopy: independent deep copy");

	{
		SBFile src  = { 0 };
		SBFile copy = { 0 };

		if (!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &src, &t->err)) {
			Test_assert(t, "create src", false);
			goto doneCopy;
		}

		CharString name = CharString_createRefCStrConst("x");
		Test_assert(t, "add var to src",
			SBFile_addVariableAsType(&src, &name, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err)
		);

		Test_assert(t, "createCopy", SBFile_createCopy(&src, t->alloc, &copy, &t->err));

		Test_assert(t, "copy vars count",    copy.vars.length    == src.vars.length);
		Test_assert(t, "copy structs count", copy.structs.length == src.structs.length);
		Test_assert(t, "copy bufferSize",    copy.bufferSize     == src.bufferSize);
		Test_assert(t, "copy hash matches",  copy.hash           == src.hash);

		if (copy.vars.length == 1 && src.vars.length == 1) {
			Test_assert(t, "copy var type",   copy.vars.ptr[0].type   == src.vars.ptr[0].type);
			Test_assert(t, "copy var offset", copy.vars.ptr[0].offset == src.vars.ptr[0].offset);
			Test_assert(t, "copy var name", CharString_equalsStringSensitive(&copy.names.entryStrings.ptr[0], &name));
		}

	doneCopy:
		SBFile_free(&src,  t->alloc);
		SBFile_free(&copy, t->alloc);
	}
}

//Duplicate names are allowed in different parent scopes (sibling structs).
void Test_SBFileAllowSameNameInDifferentScopes(Test *t) {

	Test_setModule(t, "SBFile add variable: same name allowed in different parent scopes");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		SBFile sb     = { 0 };
		SBFile result = { 0 };
		StreamRef *archiveSr = NULL;

		//Two structs each with a member called "x"
		//Layout: [StructA @ 0][StructB @ 32], buffer = 64 bytes
		if (!SBFile_create(ESBSettingsFlags_None, 64, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile scopes", false);
			goto doneScopes;
		}

		SBStruct sa  = { .stride = 32 };
		SBStruct sb2 = { .stride = 32 };
		CharString sNameA = CharString_createRefCStrConst("SA");
		CharString sNameB = CharString_createRefCStrConst("SB");
		Test_assert(t, "add struct SA", SBFile_addStruct(&sb, &sNameA, sa,  t->alloc, &t->err));
		Test_assert(t, "add struct SB", SBFile_addStruct(&sb, &sNameB, sb2, t->alloc, &t->err));

		CharString vNameA = CharString_createRefCStrConst("varA");
		CharString vNameB = CharString_createRefCStrConst("varB");
		Test_assert(t, "add varA root",
			SBFile_addVariableAsStruct(&sb, &vNameA, 0,  U16_MAX, 0, ESBVarFlag_None, NULL, t->alloc, &t->err)
		);
		Test_assert(t, "add varB root",
			SBFile_addVariableAsStruct(&sb, &vNameB, 32, U16_MAX, 1, ESBVarFlag_None, NULL, t->alloc, &t->err)
		);

		//Both children named "x" but under different parents (vars index 0 and 1)
		CharString xA = CharString_createRefCStrConst("x");
		CharString xB = CharString_createRefCStrConst("x");
		Test_assert(t, "child x under varA",
			SBFile_addVariableAsType(&sb, &xA, 0, 0, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err)
		);
		Test_assert(t, "child x under varB",
			SBFile_addVariableAsType(&sb, &xB, 0, 1, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err)
		);

		Test_assert(t, "scopes round-trip", sbRoundTrip(t, &sb, &archiveSr, &result, &type));
		Test_assert(t, "scopes vars count", result.vars.length == 4);

	doneScopes:
		SBFile_free(&sb, t->alloc);
		SBFile_free(&result, t->alloc);
		RefPtr_dec(&archiveSr);
	}
}

static Bool SBFile_testSizeConsistency(
	const SBFile *sbFile,
	const RefPtrType *memoryStreamType,
	Test *t
) {
	MemoryStreamRef *ms = NULL;
	Bool             ok = false;

	U64 sizeOnly = 0;

	if (!SBFile_write(sbFile, t->alloc, NULL, &sizeOnly, &t->err))
		goto clean;

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, memoryStreamType, &ms, &t->err))
		goto clean;

	U64 streamSize = 0;

	if (!SBFile_write(sbFile, t->alloc, ms, &streamSize, &t->err))
		goto clean;

	ok = sizeOnly == streamSize;

clean:
	RefPtr_dec(&ms);
	return ok;
}

void Test_SBFileSizeConsistency(Test *t) {

	Test_setModule(t, "SBFile_sizeConsistency");

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{                                    //Single F32 variable
		SBFile sb = { 0 };

		if (!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &sb, &t->err)) {
			Test_assert(t, "SizeConsistency F32: setup", false);
			goto skipF32;
		}

		CharString name = CharString_createRefCStrConst("myFloat");
		SBFile_addVariableAsType(&sb, &name, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err);
		Test_assert(t, "SizeConsistency F32", SBFile_testSizeConsistency(&sb, &type, t));

	skipF32:
		SBFile_free(&sb, t->alloc);
	}

	{                                    //Multiple root variables
		SBFile sb = { 0 };

		if (!SBFile_create(ESBSettingsFlags_None, 32, t->alloc, &sb, &t->err)) {
			Test_assert(t, "SizeConsistency multi: setup", false);
			goto skipMulti;
		}

		CharString nameA = CharString_createRefCStrConst("a");
		CharString nameB = CharString_createRefCStrConst("b");
		CharString nameC = CharString_createRefCStrConst("c");
		SBFile_addVariableAsType(&sb, &nameA, 0,  U16_MAX, ESBType_F32,   ESBVarFlag_None, NULL, t->alloc, &t->err);
		SBFile_addVariableAsType(&sb, &nameB, 4,  U16_MAX, ESBType_I32,   ESBVarFlag_None, NULL, t->alloc, &t->err);
		SBFile_addVariableAsType(&sb, &nameC, 16, U16_MAX, ESBType_U32x4, ESBVarFlag_None, NULL, t->alloc, &t->err);
		Test_assert(t, "SizeConsistency multi", SBFile_testSizeConsistency(&sb, &type, t));

	skipMulti:
		SBFile_free(&sb, t->alloc);
	}

	{                                    //Struct with members
		SBFile sb = { 0 };

		if (!SBFile_create(ESBSettingsFlags_None, 32, t->alloc, &sb, &t->err)) {
			Test_assert(t, "SizeConsistency struct: setup", false);
			goto skipStruct;
		}

		SBStruct myStruct = { .stride = 32 };
		CharString sName  = CharString_createRefCStrConst("MyStruct");
		CharString vName  = CharString_createRefCStrConst("myVar");
		CharString posName = CharString_createRefCStrConst("pos");
		CharString colName = CharString_createRefCStrConst("col");
		SBFile_addStruct(&sb, &sName, myStruct, t->alloc, &t->err);
		SBFile_addVariableAsStruct(&sb, &vName, 0, U16_MAX, 0, ESBVarFlag_None, NULL, t->alloc, &t->err);
		SBFile_addVariableAsType(&sb, &posName, 0,  0, ESBType_F32x4, ESBVarFlag_None, NULL, t->alloc, &t->err);
		SBFile_addVariableAsType(&sb, &colName, 16, 0, ESBType_F32x4, ESBVarFlag_None, NULL, t->alloc, &t->err);
		Test_assert(t, "SizeConsistency struct", SBFile_testSizeConsistency(&sb, &type, t));

	skipStruct:
		SBFile_free(&sb, t->alloc);
	}

	{                                    //Tightly packed flag
		SBFile sb = { 0 };

		if (!SBFile_create(ESBSettingsFlags_IsTightlyPacked, 12, t->alloc, &sb, &t->err)) {
			Test_assert(t, "SizeConsistency tightlyPacked: setup", false);
			goto skipPacked;
		}

		CharString name = CharString_createRefCStrConst("rgb");
		SBFile_addVariableAsType(&sb, &name, 0, U16_MAX, ESBType_F32x3, ESBVarFlag_None, NULL, t->alloc, &t->err);
		Test_assert(t, "SizeConsistency tightlyPacked", SBFile_testSizeConsistency(&sb, &type, t));

	skipPacked:
		SBFile_free(&sb, t->alloc);
	}

	{                                    //HideMagicNumber variant
		SBFile sb = { 0 };

		if (!SBFile_create(ESBSettingsFlags_HideMagicNumber, 16, t->alloc, &sb, &t->err)) {
			Test_assert(t, "SizeConsistency hideMagic: setup", false);
			goto skipHide;
		}

		CharString name = CharString_createRefCStrConst("v");
		SBFile_addVariableAsType(&sb, &name, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err);
		Test_assert(t, "SizeConsistency hideMagic", SBFile_testSizeConsistency(&sb, &type, t));

	skipHide:
		SBFile_free(&sb, t->alloc);
	}
}
