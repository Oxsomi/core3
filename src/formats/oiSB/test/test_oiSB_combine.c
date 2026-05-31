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

//formats/oiSB/test/test_oiSB_combine.c

#include "test_oiSB_shared.h"
#include "formats/oiSB/sb_file.h"
#include "types/container/memory_stream.h"
#include "types/base/string.h"

//Adding a duplicate variable name in the same parent scope must fail.
void Test_SBFileAddDuplicateVarName(Test *t) {

	Test_setModule(t, "SBFile validation: reject duplicate name in same scope");

	{
		SBFile sb = { 0 };

		if (!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile dup", false);
			goto doneDup;
		}

		CharString nameA = CharString_createRefCStrConst("myVar");
		CharString nameB = CharString_createRefCStrConst("myVar");

		Test_assert(t, "first add succeeds",
			SBFile_addVariableAsType(&sb, &nameA, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, NULL)
		);
		Test_assert(t, "duplicate add fails",
			!SBFile_addVariableAsType(&sb, &nameB, 4, U16_MAX, ESBType_I32, ESBVarFlag_None, NULL, t->alloc, NULL)
		);

		//Ensure the file wasn't corrupted, still has exactly one var
		Test_assert(t, "vars count still 1", sb.vars.length == 1);

	doneDup:
		SBFile_free(&sb, t->alloc);
	}
}

//Variable at an offset that would place it out of the buffer must be rejected.
void Test_SBFileAddVarOutOfBounds(Test *t) {

	Test_setModule(t, "SBFile validation: offset + size exceeds bufferSize");

	{
		SBFile sb = { 0 };

		if (!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile oob", false);
			goto doneOOB;
		}

		//F32x4 = 16 bytes; offset 4 + 16 = 20 > bufferSize 16
		CharString name = CharString_createRefCStrConst("oob");
		Test_assert(t, "out of bounds var fails",
			!SBFile_addVariableAsType(&sb, &name, 4, U16_MAX, ESBType_F32x4, ESBVarFlag_None, NULL, t->alloc, NULL)
		);
		Test_assert(t, "vars count still 0", sb.vars.length == 0);

	doneOOB:
		SBFile_free(&sb, t->alloc);
	}
}

//parentId pointing to a variable index that doesn't exist yet must be rejected.
void Test_SBFileAddVarInvalidParentId(Test *t) {

	Test_setModule(t, "SBFile validation: invalid parentId is rejected");

	{
		SBFile sb = { 0 };

		if (!SBFile_create(ESBSettingsFlags_None, 64, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile bad parent", false);
			goto doneBadParent;
		}

		//No vars exist yet, so parentId=0 is out of bounds
		CharString name = CharString_createRefCStrConst("child");
		Test_assert(t, "bad parentId fails",
			!SBFile_addVariableAsType(&sb, &name, 0, 0, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, NULL)
		);

	doneBadParent:
		SBFile_free(&sb, t->alloc);
	}
}

//parentId pointing to a primitive (non-struct) variable must be rejected.
void Test_SBFileAddVarParentIsPrimitive(Test *t) {

	Test_setModule(t, "SBFile validation: parent is primitive type, not struct");

	{
		SBFile sb = { 0 };

		if (!SBFile_create(ESBSettingsFlags_None, 32, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile prim parent", false);
			goto donePrimParent;
		}

		//Add a primitive var at index 0
		CharString parentName = CharString_createRefCStrConst("prim");
		Test_assert(t, "add prim var",
			SBFile_addVariableAsType(&sb, &parentName, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err)
		);

		//Try to nest a child under it, must fail because parent is a type, not a struct var
		CharString childName = CharString_createRefCStrConst("child");
		Test_assert(t, "child of primitive fails",
			!SBFile_addVariableAsType(&sb, &childName, 0, 0, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, NULL)
		);

	donePrimParent:
		SBFile_free(&sb, t->alloc);
	}
}

//structId out of bounds for addVariableAsStruct must be rejected.
void Test_SBFileAddVarStructIdOutOfBounds(Test *t) {

	Test_setModule(t, "SBFile validation: structId out of bounds");

	{
		SBFile sb = { 0 };

		if (!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile bad structId", false);
			goto doneBadStruct;
		}

		//No structs registered, structId=0 is out of bounds
		CharString name = CharString_createRefCStrConst("v");
		Test_assert(t, "bad structId fails",
			!SBFile_addVariableAsStruct(&sb, &name, 0, U16_MAX, 0, ESBVarFlag_None, NULL, t->alloc, NULL)
		);

	doneBadStruct:
		SBFile_free(&sb, t->alloc);
	}
}

//Non-16-byte-aligned offset for a struct variable in standard (non-tightly-packed) mode must fail.
void Test_SBFileAddStructVarMisalignedOffset(Test *t) {

	Test_setModule(t, "SBFile validation: misaligned struct var offset in standard mode");

	{
		SBFile sb = { 0 };

		if (!SBFile_create(ESBSettingsFlags_None, 64, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile misalign", false);
			goto doneMisalign;
		}

		SBStruct s = { .stride = 16 };
		CharString sName = CharString_createRefCStrConst("S");
		Test_assert(t, "add struct", SBFile_addStruct(&sb, &sName, s, t->alloc, &t->err));

		//offset=4 is not 16-byte aligned; must fail in standard (cbuffer) mode
		CharString vName = CharString_createRefCStrConst("bad");
		Test_assert(t, "misaligned struct var fails",
			!SBFile_addVariableAsStruct(&sb, &vName, 4, U16_MAX, 0, ESBVarFlag_None, NULL, t->alloc, NULL)
		);

	doneMisalign:
		SBFile_free(&sb, t->alloc);
	}
}

//SBFile_addStruct rejects zero stride.
void Test_SBFileAddStructZeroStride(Test *t) {

	Test_setModule(t, "SBFile validation: zero stride struct is rejected");

	{
		SBFile sb = { 0 };

		if (!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile zero stride", false);
			goto doneZeroStride;
		}

		SBStruct zeroStride = { .stride = 0 };
		CharString name = CharString_createRefCStrConst("Bad");
		Test_assert(t, "zero stride struct fails",
			!SBFile_addStruct(&sb, &name, zeroStride, t->alloc, NULL)
		);

	doneZeroStride:
		SBFile_free(&sb, t->alloc);
	}
}

//Name exactly 128 bytes long must be accepted; 129 bytes must be rejected.
void Test_SBFileNameLengthBoundary(Test *t) {

	Test_setModule(t, "SBFile validation: 128-byte name accepted, 129-byte rejected");

	{
		SBFile sb = { 0 };

		if (!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile name boundary", false);
			goto doneNameBoundary;
		}

		//Build names of exactly 128 and 129 chars
		C8 buf128[129];
		C8 buf129[130];
		for (U32 i = 0; i < 128; ++i) buf128[i] = 'a';
		for (U32 i = 0; i < 129; ++i) buf129[i] = 'b';
		buf128[128] = '\0';
		buf129[129] = '\0';

		CharString name128 = CharString_createRefCStrConst(buf128);
		CharString name129 = CharString_createRefCStrConst(buf129);

		Test_assert(t, "128-byte name accepted",
			SBFile_addVariableAsType(&sb, &name128, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, NULL)
		);
		Test_assert(t, "129-byte name rejected",
			!SBFile_addVariableAsType(&sb, &name129, 4, U16_MAX, ESBType_I32, ESBVarFlag_None, NULL, t->alloc, NULL)
		);

	doneNameBoundary:
		SBFile_free(&sb, t->alloc);
	}
}

//A variable with arrays->length == 0 passed explicitly must be rejected (caller should pass NULL instead).
void Test_SBFileAddVarEmptyArrayList(Test *t) {

	Test_setModule(t, "SBFile validation: explicit empty arrays list is rejected");

	{
		SBFile sb = { 0 };

		if (!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile empty arr", false);
			goto doneEmptyArr;
		}

		ListU32 emptyArr = { 0 };    //length == 0
		CharString name = CharString_createRefCStrConst("v");

		Test_assert(t, "empty arrays list fails",
			!SBFile_addVariableAsType(&sb, &name, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, &emptyArr, t->alloc, NULL)
		);

	doneEmptyArr:
		SBFile_free(&sb, t->alloc);
	}
}

//Reading from a zeroed/invalid stream must fail cleanly.
void Test_SBFileReadInvalidStream(Test *t) {

	Test_setModule(t, "SBFile validation: read invalid stream fails cleanly");

	{
		const RefPtrType type = MemoryStream_makeType(t->alloc);
		StreamRef *sr = NULL;
		SBFile result = { 0 };

		if (!MemoryStream_create(sizeof(SBHeader) * 2, EMemoryStreamFlags_IsWritable, &type, &sr, &t->err)) {
			Test_assert(t, "create zeroed stream", false);
			goto doneInvalid;
		}

		U64 off = 0;
		Test_assert(t, "read invalid stream fails", !SBFile_read(sr, &off, false, t->alloc, &result, NULL));
		Test_assert(t, "result empty on fail",      !result.vars.ptr);

	doneInvalid:
		RefPtr_dec(&sr);
		SBFile_free(&result, t->alloc);
	}
}

//Writing a NULL/empty SBFile must fail.
void Test_SBFileWriteEmpty(Test *t) {

	Test_setModule(t, "SBFile validation: write empty SBFile fails");

	{
		const RefPtrType type = MemoryStream_makeType(t->alloc);
		StreamRef *sr = NULL;

		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &sr, &t->err)) {
			Test_assert(t, "create write stream", false);
			goto doneWriteEmpty;
		}

		SBFile empty = { 0 };
		U64 off = 0;
		Test_assert(t, "write empty SBFile fails", !SBFile_write(&empty, t->alloc, sr, &off, NULL));

	doneWriteEmpty:
		RefPtr_dec(&sr);
	}
}