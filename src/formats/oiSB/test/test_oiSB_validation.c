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

//formats/oiSB/test/test_oiSB_validation.c

#include "test_oiSB_shared.h"
#include "formats/oiSB/sb_file.h"
#include "types/container/list_basic_types.h"
#include "types/base/string_base.h"

//SBFile_combine merges two files with the same layout but different use flags.
void Test_SBFileCombineFlags(Test *t) {

	Test_setModule(t, "SBFile combine: merge SPIRV + DXIL use flags");

	{
		SBFile a        = { 0 };
		SBFile b        = { 0 };
		SBFile combined = { 0 };

		if (
			!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &a, &t->err) ||
			!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &b, &t->err)
		) {
			Test_assert(t, "create a and b", false);
			goto doneCombine;
		}

		CharString nameA = CharString_createRefCStrConst("val");
		CharString nameB = CharString_createRefCStrConst("val");
		Test_assert(t, "add to a",
			SBFile_addVariableAsType(&a, &nameA, 0, U16_MAX, ESBType_F32, ESBVarFlag_IsUsedVarSPIRV, NULL, t->alloc, &t->err)
		);
		Test_assert(t, "add to b",
			SBFile_addVariableAsType(&b, &nameB, 0, U16_MAX, ESBType_F32, ESBVarFlag_IsUsedVarDXIL, NULL, t->alloc, &t->err)
		);

		Test_assert(t, "combine", SBFile_combine(&a, &b, t->alloc, &combined, &t->err));

		Test_assert(t, "combined vars count", combined.vars.length == 1);

		if (combined.vars.length == 1) {
			ESBVarFlag flags = combined.vars.ptr[0].flags;
			Test_assert(t, "SPIRV flag set", flags & ESBVarFlag_IsUsedVarSPIRV);
			Test_assert(t, "DXIL flag set",  flags & ESBVarFlag_IsUsedVarDXIL);
		}

	doneCombine:
		SBFile_free(&a,        t->alloc);
		SBFile_free(&b,        t->alloc);
		SBFile_free(&combined, t->alloc);
	}
}

//SBFile_combine rejects files with mismatched buffer sizes.
void Test_SBFileCombineBufferSizeMismatch(Test *t) {

	Test_setModule(t, "SBFile combine: reject mismatched bufferSize");

	{
		SBFile a        = { 0 };
		SBFile b        = { 0 };
		SBFile combined = { 0 };

		if (
			!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &a, &t->err) ||
			!SBFile_create(ESBSettingsFlags_None, 32, t->alloc, &b, &t->err)
		) {
			Test_assert(t, "create a and b", false);
			goto doneMismatch;
		}

		CharString nameA = CharString_createRefCStrConst("x");
		CharString nameB = CharString_createRefCStrConst("x");
		SBFile_addVariableAsType(&a, &nameA, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, NULL);
		SBFile_addVariableAsType(&b, &nameB, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, NULL);

		Test_assert(t, "bufferSize mismatch fails", !SBFile_combine(&a, &b, t->alloc, &combined, NULL));

	doneMismatch:
		SBFile_free(&a,        t->alloc);
		SBFile_free(&b,        t->alloc);
		SBFile_free(&combined, t->alloc);
	}
}

//SBFile_combine rejects a variable that exists in b but not in a.
void Test_SBFileCombineMissingVariable(Test *t) {

	Test_setModule(t, "SBFile combine: variable in b not found in a is rejected");

	{
		SBFile a        = { 0 };
		SBFile b        = { 0 };
		SBFile combined = { 0 };

		if (!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &a, &t->err) ||
			!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &b, &t->err)) {
			Test_assert(t, "create a and b", false);
			goto doneMissingVar;
		}

		CharString nameA = CharString_createRefCStrConst("x");
		CharString nameB = CharString_createRefCStrConst("y");    //Different name, won't be found in a
		SBFile_addVariableAsType(&a, &nameA, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, NULL);
		SBFile_addVariableAsType(&b, &nameB, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, NULL);

		Test_assert(t, "missing var fails",
			!SBFile_combine(&a, &b, t->alloc, &combined, NULL)
		);

	doneMissingVar:
		SBFile_free(&a,        t->alloc);
		SBFile_free(&b,        t->alloc);
		SBFile_free(&combined, t->alloc);
	}
}

//SBFile_combine rejects a variable present in both files but with a mismatching type.
void Test_SBFileCombineTypeMismatch(Test *t) {

	Test_setModule(t, "SBFile combine: variable type mismatch is rejected");

	{
		SBFile a        = { 0 };
		SBFile b        = { 0 };
		SBFile combined = { 0 };

		if (
			!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &a, &t->err) ||
			!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &b, &t->err)
		) {
			Test_assert(t, "create a and b", false);
			goto doneTypeMismatch;
		}

		CharString nameA = CharString_createRefCStrConst("v");
		CharString nameB = CharString_createRefCStrConst("v");
		SBFile_addVariableAsType(&a, &nameA, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, NULL);
		SBFile_addVariableAsType(&b, &nameB, 0, U16_MAX, ESBType_I32, ESBVarFlag_None, NULL, t->alloc, NULL);

		Test_assert(t, "type mismatch fails", !SBFile_combine(&a, &b, t->alloc, &combined, NULL));

	doneTypeMismatch:
		SBFile_free(&a,        t->alloc);
		SBFile_free(&b,        t->alloc);
		SBFile_free(&combined, t->alloc);
	}
}

//SBFile_combine with a flat [9] in a and ND [3][3] in b: unflatten path.
//The combined result should adopt the ND shape.
void Test_SBFileCombineUnflatten(Test *t) {

	Test_setModule(t, "SBFile combine: unflatten [9] + [3][3] -> [3][3]");

	{
		SBFile a        = { 0 };
		SBFile b        = { 0 };
		SBFile combined = { 0 };

		//F32[9] tightly packed = 36 bytes
		if (
			!SBFile_create(ESBSettingsFlags_IsTightlyPacked, 36, t->alloc, &a, &t->err) ||
			!SBFile_create(ESBSettingsFlags_IsTightlyPacked, 36, t->alloc, &b, &t->err)
		) {
			Test_assert(t, "create a and b", false);
			goto doneUnflatten;
		}

		//a: flat [9] as DXIL might emit it
		U32 dimA = 9;
		ListU32 arrA = { 0 };
		Test_assert(t, "ref flat [9]", ListU32_createRefConst(&dimA, 1, &arrA, &t->err));
		CharString nameA = CharString_createRefCStrConst("mat");
		Test_assert(t, "add flat [9]",
			SBFile_addVariableAsType(&a, &nameA, 0, U16_MAX, ESBType_F32, ESBVarFlag_IsUsedVarDXIL, &arrA, t->alloc, &t->err)
		);

		//b: ND [3][3] as SPIRV might emit it
		U32 dimsB[2] = { 3, 3 };
		ListU32 arrB = { 0 };
		Test_assert(t, "ref ND [3][3]", ListU32_createRefConst(dimsB, 2, &arrB, &t->err));
		CharString nameB = CharString_createRefCStrConst("mat");
		Test_assert(t, "add ND [3][3]",
			SBFile_addVariableAsType(&b, &nameB, 0, U16_MAX, ESBType_F32, ESBVarFlag_IsUsedVarSPIRV, &arrB, t->alloc, &t->err)
		);

		Test_assert(t, "unflatten combine", SBFile_combine(&a, &b, t->alloc, &combined, &t->err));

		Test_assert(t, "combined vars == 1", combined.vars.length == 1);

		//After unflatten, the var should point to a [3][3] array table entry
		if (combined.vars.length == 1 && combined.arrays.length >= 1) {
			U16 arrayField = combined.vars.ptr[0].arrayDimOrArrayId;
			Test_assert(t, "unflatten bit15 set", arrayField >> 15);
			U16 idx = arrayField & (U16)I16_MAX;
			if (idx < combined.arrays.length) {
				Test_assert(t, "unflatten dims == 2", combined.arrays.ptr[idx].length == 2);
				if (combined.arrays.ptr[idx].length == 2) {
					Test_assert(t, "unflatten dim[0] == 3", combined.arrays.ptr[idx].ptr[0] == 3);
					Test_assert(t, "unflatten dim[1] == 3", combined.arrays.ptr[idx].ptr[1] == 3);
				}
			}
		}

		//Both use flags should be set after merging
		if (combined.vars.length == 1) {
			ESBVarFlag flags = combined.vars.ptr[0].flags;
			Test_assert(t, "unflatten SPIRV set", flags & ESBVarFlag_IsUsedVarSPIRV);
			Test_assert(t, "unflatten DXIL set",  flags & ESBVarFlag_IsUsedVarDXIL);
		}

	doneUnflatten:
		SBFile_free(&a,        t->alloc);
		SBFile_free(&b,        t->alloc);
		SBFile_free(&combined, t->alloc);
	}
}

//SBFile_combine rejects two flat arrays whose total element counts differ.
void Test_SBFileCombineFlatSizeMismatch(Test *t) {

	Test_setModule(t, "SBFile combine: flat array element count mismatch is rejected");

	{
		SBFile a        = { 0 };
		SBFile b        = { 0 };
		SBFile combined = { 0 };

		if (
			!SBFile_create(ESBSettingsFlags_IsTightlyPacked, 36, t->alloc, &a, &t->err) ||
			!SBFile_create(ESBSettingsFlags_IsTightlyPacked, 40, t->alloc, &b, &t->err)
		) {
			Test_assert(t, "create a and b", false);
			goto doneFlatMismatch;
		}

		U32 dimA = 9, dimB = 10;
		ListU32 arrA = { 0 }, arrB = { 0 };
		ListU32_createRefConst(&dimA, 1, &arrA, NULL);
		ListU32_createRefConst(&dimB, 1, &arrB, NULL);

		CharString nameA = CharString_createRefCStrConst("v");
		CharString nameB = CharString_createRefCStrConst("v");
		SBFile_addVariableAsType(&a, &nameA, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, &arrA, t->alloc, NULL);
		SBFile_addVariableAsType(&b, &nameB, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, &arrB, t->alloc, NULL);

		Test_assert(t, "flat size mismatch fails",
			!SBFile_combine(&a, &b, t->alloc, &combined, NULL)
		);

	doneFlatMismatch:
		SBFile_free(&a,        t->alloc);
		SBFile_free(&b,        t->alloc);
		SBFile_free(&combined, t->alloc);
	}
}

//Hash changes when variables are added, and two identical builds produce the same hash.
void Test_SBFileHashConsistency(Test *t) {

	Test_setModule(t, "SBFile hash: consistent across two identical builds");

	{
		SBFile a = { 0 };
		SBFile b = { 0 };

		Bool ok =
			SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &a, &t->err) &&
			SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &b, &t->err);

		if (!ok) {
			Test_assert(t, "create a and b", false);
			goto doneHash;
		}

		U64 hashEmpty = a.hash;

		CharString nameA = CharString_createRefCStrConst("x");
		CharString nameB = CharString_createRefCStrConst("x");
		SBFile_addVariableAsType(&a, &nameA, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, NULL);
		SBFile_addVariableAsType(&b, &nameB, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, NULL);

		Test_assert(t, "hash changes after add",     a.hash != hashEmpty);
		Test_assert(t, "identical builds same hash", a.hash == b.hash);

		//A different name must produce a different hash
		SBFile c = { 0 };
		if (SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &c, &t->err)) {
			CharString nameC = CharString_createRefCStrConst("y");
			SBFile_addVariableAsType(&c, &nameC, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, NULL);
			Test_assert(t, "different name different hash", a.hash != c.hash);
			SBFile_free(&c, t->alloc);
		}

	doneHash:
		SBFile_free(&a, t->alloc);
		SBFile_free(&b, t->alloc);
	}
}
