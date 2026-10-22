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

//SBFile_combine merges a layout that holds a struct, where a variable's name id is offset past the
//struct names rather than being its own index.
//A layout with no struct never separates the two, so this is the case that tells them apart.
void Test_SBFileCombineStruct(Test *t) {

	Test_setModule(t, "SBFile combine: layout holding a struct");

	{
		SBFile a        = { 0 };
		SBFile b        = { 0 };
		SBFile combined = { 0 };

		if (
			!SBFile_create(ESBSettingsFlags_None, 32, t->alloc, &a, &t->err) ||
			!SBFile_create(ESBSettingsFlags_None, 32, t->alloc, &b, &t->err)
		) {
			Test_assert(t, "create a and b", false);
			goto doneCombineStruct;
		}

		//The same layout twice, each seen as used by one backend: MyStruct { F32x4 pos; F32x4 col; }

		SBStruct myStruct = { .stride = 32 };
		SBFile *files[2] = { &a, &b };
		ESBVarFlag used[2] = { ESBVarFlag_IsUsedVarSPIRV, ESBVarFlag_IsUsedVarDXIL };
		Bool built = true;

		for (U8 i = 0; i < 2; ++i) {

			CharString structName = CharString_createRefCStrConst("MyStruct");
			CharString rootName = CharString_createRefCStrConst("myVar");
			CharString posName = CharString_createRefCStrConst("pos");
			CharString colName = CharString_createRefCStrConst("col");

			built = built &&
				SBFile_addStruct(files[i], &structName, myStruct, t->alloc, &t->err) &&
				SBFile_addVariableAsStruct(files[i], &rootName, 0, U16_MAX, 0, used[i], NULL, t->alloc, &t->err) &&
				SBFile_addVariableAsType(files[i], &posName, 0, 0, ESBType_F32x4, used[i], NULL, t->alloc, &t->err) &&
				SBFile_addVariableAsType(files[i], &colName, 16, 0, ESBType_F32x4, used[i], NULL, t->alloc, &t->err);
		}

		Test_assert(t, "build both layouts", built);
		Test_assert(t, "combine", SBFile_combine(&a, &b, t->alloc, &combined, &t->err));

		Test_assert(t, "combined struct count", combined.structs.length == 1);
		Test_assert(t, "combined vars count", combined.vars.length == 3);

		//Every variable is matched by name, so every one of them carries both backends' use flags.

		Bool merged = combined.vars.length == 3;

		for(U64 i = 0; merged && i < combined.vars.length; ++i)
			merged =
				(combined.vars.ptr[i].flags & ESBVarFlag_IsUsedVarSPIRV) &&
				(combined.vars.ptr[i].flags & ESBVarFlag_IsUsedVarDXIL);

		Test_assert(t, "use flags merged on every variable", merged);

	doneCombineStruct:
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

//The shared layout claim: validation refuses misaligned layouts on flagged files, leaves unflagged
// files free, and SBFile_combine grants the flag when the merged layout proves the rule.

void Test_SBFileSharedLayout(Test *t) {

	Test_setModule(t, "SBFile shared layout");

	//Packing rules bind a file only through the claim, so each case builds its layout with and without it.

	const ESBSettingsFlags packedShared = ESBSettingsFlags_IsTightlyPacked | ESBSettingsFlags_HasSharedLayout;

	//uint pad; uint4 test; with test at 4, in all four claim and packing combinations.
	//A cbuffer keeps DX's 16-byte rows only under the claim, so there test straddles a row and is refused
	// while adding; unclaimed it is recorded (the scalar SPIR-V layout puts it there) and the probe reports
	// the same straddle, which is what keeps combine from promoting a file that wouldn't read back.
	//Tightly packed, 4 is the structured buffer layout itself and is accepted either way.

	static const C8 *rowLabels[2][2] = {
		{ "an unclaimed cbuffer records a U32x4 straddling a row", "an unclaimed structured buffer keeps a U32x4 at 4" },
		{ "a shared cbuffer refuses a U32x4 straddling a row", "a shared structured buffer keeps a U32x4 at 4" }
	};

	for (U8 shared = 0; shared < 2; ++shared)
		for (U8 packed = 0; packed < 2; ++packed) {

			SBFile rows = { 0 };

			const ESBSettingsFlags rowFlags = (ESBSettingsFlags) (
				(shared ? ESBSettingsFlags_HasSharedLayout : ESBSettingsFlags_None) |
				(packed ? ESBSettingsFlags_IsTightlyPacked : ESBSettingsFlags_None)
			);

			if (!SBFile_create(rowFlags, 32, t->alloc, &rows, &t->err)) {
				Test_assert(t, "create row layout", false);
				continue;
			}

			CharString padName = CharString_createRefCStrConst("pad");
			CharString testName = CharString_createRefCStrConst("test");

			const Bool padOk = SBFile_addVariableAsType(
				&rows, &padName, 0, U16_MAX, ESBType_U32, ESBVarFlag_None, NULL, t->alloc, &t->err
			);

			//The refusal is the expected result for a shared cbuffer, so e_rr stays NULL

			const Bool testOk = SBFile_addVariableAsType(
				&rows, &testName, 4, U16_MAX, ESBType_U32x4, ESBVarFlag_None, NULL, t->alloc, NULL
			);

			const Bool valid = SBFile_validateSharedLayout(&rows, NULL);

			Bool expected = padOk && testOk && valid;

			if (shared && !packed)
				expected = padOk && !testOk;

			else if (!shared && !packed)
				expected = padOk && testOk && !valid;

			Test_assert(t, rowLabels[shared][packed], expected);
			SBFile_free(&rows, t->alloc);
		}

	//A U64 at offset 4 breaks the scalar alignment rule; at 8 it keeps it

	{
		SBFile bad = { 0 };
		SBFile good = { 0 };

		//bad is built without the claim, where a U64 at 4 is recorded as declared, and only then claims a
		// shared layout it can't keep, the way a stale or hand edited file would arrive.

		Bool made =
			SBFile_create(ESBSettingsFlags_IsTightlyPacked, 16, t->alloc, &bad, &t->err) &&
			SBFile_create(packedShared, 16, t->alloc, &good, &t->err);

		Test_assert(t, "create layouts", made);

		if (made) {

			CharString a0 = CharString_createRefCStrConst("a");
			CharString c0 = CharString_createRefCStrConst("c");
			CharString a1 = CharString_createRefCStrConst("a");
			CharString c1 = CharString_createRefCStrConst("c");
			CharString c2 = CharString_createRefCStrConst("c2");

			Bool built =
				SBFile_addVariableAsType(&bad, &a0, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err) &&
				SBFile_addVariableAsType(&bad, &c0, 4, U16_MAX, ESBType_U64, ESBVarFlag_None, NULL, t->alloc, &t->err) &&
				SBFile_addVariableAsType(&good, &a1, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err) &&
				SBFile_addVariableAsType(&good, &c1, 8, U16_MAX, ESBType_U64, ESBVarFlag_None, NULL, t->alloc, &t->err);

			bad.flags = (ESBSettingsFlags) (bad.flags | ESBSettingsFlags_HasSharedLayout);

			Test_assert(t, "build layouts", built);
			Test_assert(t, "misaligned U64 fails validation", !SBFile_validateSharedLayout(&bad, NULL));
			Test_assert(t, "aligned U64 passes validation", SBFile_validateSharedLayout(&good, NULL));

			//Refusals are the expected results below, so e_rr stays NULL on each negative call

			U64 off = 0;
			Test_assert(t, "flagged write refuses the broken claim", !SBFile_write(&bad, t->alloc, NULL, &off, NULL));

			off = 0;
			Test_assert(t, "flagged write keeps the valid claim", SBFile_write(&good, t->alloc, NULL, &off, &t->err));

			Test_assert(
				t, "a claimed file refuses a misaligned U64 while adding",
				!SBFile_addVariableAsType(&good, &c2, 4, U16_MAX, ESBType_U64, ESBVarFlag_None, NULL, t->alloc, NULL)
			);
		}

		SBFile_free(&bad, t->alloc);
		SBFile_free(&good, t->alloc);
	}

	//Without the flag the same broken layout writes freely; only the probe reports it

	{
		SBFile any = { 0 };

		if (SBFile_create(ESBSettingsFlags_IsTightlyPacked, 16, t->alloc, &any, &t->err)) {

			CharString a0 = CharString_createRefCStrConst("a");
			CharString c0 = CharString_createRefCStrConst("c");

			Bool built =
				SBFile_addVariableAsType(&any, &a0, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err) &&
				SBFile_addVariableAsType(&any, &c0, 4, U16_MAX, ESBType_U64, ESBVarFlag_None, NULL, t->alloc, &t->err);

			U64 off = 0;
			Test_assert(t, "unflagged probe still reports", built && !SBFile_validateSharedLayout(&any, NULL));
			Test_assert(t, "unflagged write stays free", built && SBFile_write(&any, t->alloc, NULL, &off, &t->err));
		}

		else Test_assert(t, "create unflagged file", false);

		SBFile_free(&any, t->alloc);
	}

	//A struct whose stride isn't aligned to its widest member breaks the claim too

	{
		SBFile strided = { 0 };

		if (SBFile_create(packedShared, 12, t->alloc, &strided, &t->err)) {

			CharString structName = CharString_createRefCStrConst("S");
			CharString rootName = CharString_createRefCStrConst("s");
			CharString memberName = CharString_createRefCStrConst("c");

			Bool built =
				SBFile_addStruct(&strided, &structName, (SBStruct) { .stride = 12 }, t->alloc, &t->err) &&
				SBFile_addVariableAsStruct(&strided, &rootName, 0, U16_MAX, 0, ESBVarFlag_None, NULL, t->alloc, &t->err) &&
				SBFile_addVariableAsType(&strided, &memberName, 0, 0, ESBType_U64, ESBVarFlag_None, NULL, t->alloc, &t->err);

			Test_assert(
				t, "12 byte stride with a U64 member fails validation",
				built && !SBFile_validateSharedLayout(&strided, NULL)
			);
		}

		else Test_assert(t, "create strided file", false);

		SBFile_free(&strided, t->alloc);
	}

	//The merge grants the claim it just proved, and drops it for a layout it can't

	{
		SBFile a = { 0 };
		SBFile b = { 0 };
		SBFile combined = { 0 };

		Bool made =
			SBFile_create(ESBSettingsFlags_IsTightlyPacked, 16, t->alloc, &a, &t->err) &&
			SBFile_create(packedShared, 16, t->alloc, &b, &t->err);

		if (made) {

			CharString nameA = CharString_createRefCStrConst("c");
			CharString nameB = CharString_createRefCStrConst("c");

			Bool built =
				SBFile_addVariableAsType(
					&a, &nameA, 8, U16_MAX, ESBType_U64, ESBVarFlag_IsUsedVarSPIRV, NULL, t->alloc, &t->err
				) &&
				SBFile_addVariableAsType(
					&b, &nameB, 8, U16_MAX, ESBType_U64, ESBVarFlag_IsUsedVarDXIL, NULL, t->alloc, &t->err
				);

			Test_assert(
				t, "flag mismatch between inputs still merges",
				built && SBFile_combine(&a, &b, t->alloc, &combined, &t->err)
			);
			Test_assert(t, "the proven merge carries the claim", (combined.flags & ESBSettingsFlags_HasSharedLayout) != 0);
		}

		else Test_assert(t, "create promotion pair", false);

		SBFile_free(&a, t->alloc);
		SBFile_free(&b, t->alloc);
		SBFile_free(&combined, t->alloc);
	}

	{
		SBFile a = { 0 };
		SBFile b = { 0 };
		SBFile combined = { 0 };

		Bool made =
			SBFile_create(ESBSettingsFlags_IsTightlyPacked, 16, t->alloc, &a, &t->err) &&
			SBFile_create(ESBSettingsFlags_IsTightlyPacked, 16, t->alloc, &b, &t->err);

		if (made) {

			CharString nameA = CharString_createRefCStrConst("c");
			CharString nameB = CharString_createRefCStrConst("c");

			Bool built =
				SBFile_addVariableAsType(
					&a, &nameA, 4, U16_MAX, ESBType_U64, ESBVarFlag_IsUsedVarSPIRV, NULL, t->alloc, &t->err
				) &&
				SBFile_addVariableAsType(
					&b, &nameB, 4, U16_MAX, ESBType_U64, ESBVarFlag_IsUsedVarDXIL, NULL, t->alloc, &t->err
				);

			Test_assert(
				t, "agreeing broken layouts still merge",
				built && SBFile_combine(&a, &b, t->alloc, &combined, &t->err)
			);
			Test_assert(t, "the unprovable merge stays unclaimed", !(combined.flags & ESBSettingsFlags_HasSharedLayout));
		}

		else Test_assert(t, "create unpromoted pair", false);

		SBFile_free(&a, t->alloc);
		SBFile_free(&b, t->alloc);
		SBFile_free(&combined, t->alloc);
	}
}
