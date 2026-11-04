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
#include "formats/oiDL/dl_entry.h"
#include "types/container/list_basic_types.h"
#include "types/base/string_base.h"
#include "types/container/memory_stream.h"
#include "types/container/stream.h"
#include "types/container/buffer.h"
#include "types/container/test/failing_stream.h"

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

//SBFile_combine with structs: names hold the structs first, so every variable sits past them in the name table.
//What a structured buffer of a struct reflects as, on SPIR-V and DXIL alike: two structs, a variable of one, its members.

void Test_SBFileCombineStructs(Test *t) {

	Test_setModule(t, "SBFile combine: layouts with structs");

	SBFile files[2] = { 0 };
	SBFile combined = { 0 };

	for (U32 f = 0; f < 2; ++f) {

		const ESBVarFlag used = f ? ESBVarFlag_IsUsedVarDXIL : ESBVarFlag_IsUsedVarSPIRV;
		SBFile *file = &files[f];

		CharString rec = CharString_createRefCStrConst("Rec");
		CharString ray = CharString_createRefCStrConst("Ray");
		CharString core = CharString_createRefCStrConst("core");
		CharString x = CharString_createRefCStrConst("x");
		CharString y = CharString_createRefCStrConst("y");

		Test_assert(t, "create", SBFile_create(ESBSettingsFlags_None, 32, t->alloc, file, &t->err));
		Test_assert(t, "addStructRec", SBFile_addStruct(file, &rec, (SBStruct) { .stride = 32 }, t->alloc, &t->err));
		Test_assert(t, "addStructRay", SBFile_addStruct(file, &ray, (SBStruct) { .stride = 16 }, t->alloc, &t->err));

		Test_assert(t, "addCore", SBFile_addVariableAsStruct(
			file, &core, 0, U16_MAX, 0, used, NULL, t->alloc, &t->err
		));

		Test_assert(t, "addX", SBFile_addVariableAsType(file, &x, 0, 0, ESBType_F32, used, NULL, t->alloc, &t->err));
		Test_assert(t, "addY", SBFile_addVariableAsType(file, &y, 4, 0, ESBType_F32, used, NULL, t->alloc, &t->err));
	}

	Test_assert(t, "combine", SBFile_combine(&files[0], &files[1], t->alloc, &combined, &t->err));
	Test_assert(t, "combinedVars", combined.vars.length == 3);
	Test_assert(t, "combinedStructs", combined.structs.length == 2);

	for (U64 i = 0; i < combined.vars.length; ++i) {
		const ESBVarFlag flags = combined.vars.ptr[i].flags;
		Test_assert(t, "bothFlags", (flags & ESBVarFlag_IsUsedVarSPIRV) && (flags & ESBVarFlag_IsUsedVarDXIL));
	}

	for (U32 f = 0; f < 2; ++f)
		SBFile_free(&files[f], t->alloc);

	SBFile_free(&combined, t->alloc);
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

		if (
			!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &a, &t->err) ||
			!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &b, &t->err)
		) {
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
			U16 idx = arrayField & (U16) I16_MAX;

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

static void Test_SBFileInterleavedHash(Test *t) {

	Test_setModule(t, "SBFile hash: interleaved reflection and serialized table order agree");

	SBFile sb = { 0 }, readBack = { 0 }, combined = { 0 };
	StreamRef *stream = NULL;
	const RefPtrType type = MemoryStream_makeType(t->alloc);
	U64 writeOff = 0, readOff = 0;
	CharString sn = CharString_createRefCStrConst("S");
	CharString sv = CharString_createRefCStrConst("s");
	CharString x = CharString_createRefCStrConst("x");
	CharString tn = CharString_createRefCStrConst("T");
	CharString tv = CharString_createRefCStrConst("t");
	CharString y = CharString_createRefCStrConst("y");
	U32 dimensions[2] = { 2, 2 };
	ListU32 array = { 0 };

	Test_assert(t, "create", SBFile_create(ESBSettingsFlags_None, 128, t->alloc, &sb, &t->err));
	Test_assert(t, "S", SBFile_addStruct(&sb, &sn, (SBStruct) { 64 }, t->alloc, &t->err));
	Test_assert(t, "s", SBFile_addVariableAsStruct(&sb, &sv, 0, U16_MAX, 0, 0, NULL, t->alloc, &t->err));
	Test_assert(t, "dimensions", ListU32_createRefConst(dimensions, 2, &array, &t->err));
	Test_assert(t, "x", SBFile_addVariableAsType(&sb, &x, 0, 0, ESBType_F32, 0, &array, t->alloc, &t->err));
	Test_assert(t, "T after variables", SBFile_addStruct(&sb, &tn, (SBStruct) { 64 }, t->alloc, &t->err));
	Test_assert(t, "t", SBFile_addVariableAsStruct(&sb, &tv, 64, U16_MAX, 1, 0, NULL, t->alloc, &t->err));
	Test_assert(t, "dimensions reused", ListU32_createRefConst(dimensions, 2, &array, &t->err));
	Test_assert(t, "y", SBFile_addVariableAsType(&sb, &y, 64, 2, ESBType_F32, 0, &array, t->alloc, &t->err));

	Test_assert(t, "stream", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &stream, &t->err));
	Test_assert(t, "write", SBFile_write(&sb, t->alloc, stream, &writeOff, &t->err));
	Test_assert(t, "read", SBFile_read(stream, &readOff, false, t->alloc, &readBack, &t->err));
	Test_assert(t, "hash survives table ordering", sb.hash == readBack.hash);
	Test_assert(t, "equal after round trip", SBFile_equals(&sb, &readBack));
	Test_assert(t, "combine interleaved with itself", SBFile_combine(&sb, &sb, t->alloc, &combined, &t->err));
	Test_assert(t, "combine keeps canonical hash", SBFile_equals(&sb, &combined));

	ListU32_free(&array, t->alloc);
	SBFile_free(&sb, t->alloc);
	SBFile_free(&readBack, t->alloc);
	SBFile_free(&combined, t->alloc);
	RefPtr_dec(&stream);
}

//Hash changes when variables are added, and two identical builds produce the same hash.
void Test_SBFileHashConsistency(Test *t) {

	Test_SBFileInterleavedHash(t);

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

//Offsets, sizes and children: nothing may wrap a U32 or sit outside what holds it

static Bool sbAddType(
	SBFile *sb, const C8 *name, U32 offset, U16 parent, ESBType type, ListU32 *arrays, const Allocator *alloc
) {
	CharString str = CharString_createRefCStrConst(name);
	return SBFile_addVariableAsType(sb, &str, offset, parent, type, ESBVarFlag_None, arrays, alloc, NULL);
}

void Test_SBFileOffsetOverflow(Test *t) {

	Test_setModule(t, "SBFile add: offset + size wrapping a U32 is rejected");

	SBFile sb = { 0 };

	Test_assert(t, "create", SBFile_create(ESBSettingsFlags_None, 32, t->alloc, &sb, &t->err));
	Test_assert(t, "F32 at 0xFFFFFFFC", !sbAddType(&sb, "a", 0xFFFFFFFC, U16_MAX, ESBType_F32, NULL, t->alloc));
	Test_assert(t, "F32 at 0xFFFFFFFC is not kept", sb.vars.length == 0);
	SBFile_free(&sb, t->alloc);

	//A buffer of 4 GiB - 1: an F32 ending exactly at 2^32 doesn't fit, but wraps to 0 in U32

	sb = (SBFile) { 0 };
	Test_assert(t, "create big", SBFile_create(ESBSettingsFlags_None, U32_MAX, t->alloc, &sb, &t->err));
	Test_assert(t, "last F32", !sbAddType(&sb, "a", 0xFFFFFFFC, U16_MAX, ESBType_F32, NULL, t->alloc));
	Test_assert(t, "F32 before it", sbAddType(&sb, "b", 0xFFFFFFF8, U16_MAX, ESBType_F32, NULL, t->alloc));

	//The array's bytes fit a U32 but offset + bytes doesn't

	U32 dim = 0x3FFFFFFF;
	ListU32 arr = { 0 };

	Test_assert(t, "ref array", ListU32_createRefConst(&dim, 1, &arr, &t->err));
	Test_assert(t, "huge array at 16", !sbAddType(&sb, "c", 16, U16_MAX, ESBType_F32, &arr, t->alloc));
	SBFile_free(&sb, t->alloc);

	//A file can carry the offset: tamper a valid one

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	StreamRef *s = NULL;
	SBFile src = { 0 }, result = { 0 };
	U64 off = 0;

	Test_assert(t, "create src", SBFile_create(ESBSettingsFlags_None, 32, t->alloc, &src, &t->err));
	Test_assert(t, "add src", sbAddType(&src, "a", 0, U16_MAX, ESBType_F32, NULL, t->alloc));
	Test_assert(t, "stream", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &s, &t->err));
	Test_assert(t, "write", SBFile_write(&src, t->alloc, s, &off, &t->err));

	if (s) {

		MemoryStream *ms = RefPtr_data(s, MemoryStream);

		//[magic 4][header 12][structs 0][SBVar: structId 2, array 2, offset 4]
		const U32 bad = 0xFFFFFFFC;

		Buffer_memcpy(Buffer_createRef(ms->data.ptrNonConst + 20, 4), Buffer_createRefConst(&bad, 4));

		U64 ro = 0;

		Test_assert(t, "read of wrapped offset fails", !SBFile_read(s, &ro, false, t->alloc, &result, NULL));
	}

	SBFile_free(&result, t->alloc);
	SBFile_free(&src, t->alloc);
	RefPtr_dec(&s);
}

void Test_SBFileChildInsideParent(Test *t) {

	Test_setModule(t, "SBFile add: a child has to fit in its parent struct");

	SBFile sb = { 0 };
	const Allocator *alloc = t->alloc;
	CharString sn = CharString_createRefCStrConst("S");
	CharString vn = CharString_createRefCStrConst("s");

	//Offsets are from the start of the buffer: the struct sits at 16 and spans [16, 40)

	Test_assert(t, "create", SBFile_create(ESBSettingsFlags_None, 48, t->alloc, &sb, &t->err));
	Test_assert(t, "struct", SBFile_addStruct(&sb, &sn, (SBStruct) { .stride = 24 }, t->alloc, &t->err));
	Test_assert(t, "var", SBFile_addVariableAsStruct(&sb, &vn, 16, U16_MAX, 0, ESBVarFlag_None, NULL, t->alloc, &t->err));

	Test_assert(t, "far outside", !sbAddType(&sb, "a", 1024, 0, ESBType_F32, NULL, alloc));
	Test_assert(t, "before the parent", !sbAddType(&sb, "b", 0, 0, ESBType_F32, NULL, alloc));
	Test_assert(t, "right after the parent", !sbAddType(&sb, "c", 40, 0, ESBType_F32, NULL, alloc));
	Test_assert(t, "straddling the end", !sbAddType(&sb, "d", 32, 0, ESBType_F32x4, NULL, alloc));
	Test_assert(t, "first member", sbAddType(&sb, "e", 16, 0, ESBType_F32, NULL, alloc));
	Test_assert(t, "last member", sbAddType(&sb, "f", 36, 0, ESBType_F32, NULL, alloc));
	Test_assert(t, "a vector inside", sbAddType(&sb, "g", 16, 0, ESBType_F32x4, NULL, alloc));

	//A struct member too

	CharString inner = CharString_createRefCStrConst("Inner");
	CharString innerVar = CharString_createRefCStrConst("inner");

	Test_assert(t, "inner struct", SBFile_addStruct(&sb, &inner, (SBStruct) { .stride = 32 }, t->alloc, &t->err));
	Test_assert(t, "bigger than parent", !SBFile_addVariableAsStruct(
		&sb, &innerVar, 16, 0, 1, ESBVarFlag_None, NULL, alloc, NULL
	));

	SBFile_free(&sb, t->alloc);
}

static void Test_SBFileNestedStructArray(Test *t) {

	Test_setModule(t, "SBFile nested cbuffer struct array omits final element padding");

	SBFile sb = { 0 };
	CharString sn = CharString_createRefCStrConst("S");
	CharString pn = CharString_createRefCStrConst("P");
	CharString pv = CharString_createRefCStrConst("p");
	CharString sv = CharString_createRefCStrConst("s");
	U32 count = 2;
	ListU32 array = { 0 };

	Test_assert(t, "Create nested buffer", SBFile_create(ESBSettingsFlags_None, 32, t->alloc, &sb, &t->err));
	Test_assert(t, "Array element stride", SBFile_addStruct(&sb, &sn, (SBStruct) { 16 }, t->alloc, &t->err));
	Test_assert(t, "Parent occupied size", SBFile_addStruct(&sb, &pn, (SBStruct) { 20 }, t->alloc, &t->err));
	Test_assert(t, "Parent variable", SBFile_addVariableAsStruct(&sb, &pv, 0, U16_MAX, 1, 0, NULL, t->alloc, &t->err));
	Test_assert(t, "Two elements", ListU32_createRefConst(&count, 1, &array, &t->err));

	Bool added = SBFile_addVariableAsStruct(&sb, &sv, 0, 0, 0, 0, &array, t->alloc, &t->err);

	Test_assert(t, "Nested struct array fits without final padding", added);

	if (added) {

		Test_assert(t, "Element member fits all instances", sbAddType(&sb, "a", 0, 1, ESBType_F32, NULL, t->alloc));
		Test_assert(
			t, "Larger member exceeds parent in last instance",
			!sbAddType(&sb, "tooLarge", 0, 1, ESBType_F32x4, NULL, t->alloc)
		);

		Test_assert(t, "Float follows occupied parent bytes", sbAddType(&sb, "x", 20, U16_MAX, ESBType_F32, NULL, t->alloc));

		CharString innerName = CharString_createRefCStrConst("Inner");
		CharString innerVar = CharString_createRefCStrConst("inner");

		Test_assert(t, "Padded nested structure", SBFile_addStruct(&sb, &innerName, (SBStruct) { 16 }, t->alloc, &t->err));
		Test_assert(t, "Nested structure inside array", SBFile_addVariableAsStruct(
			&sb, &innerVar, 0, 1, 2, 0, NULL, t->alloc, &t->err
		));

		Test_assert(t, "Nested primitive fits last array instance", sbAddType(&sb, "a", 0, 4, ESBType_F32, NULL, t->alloc));
		Test_assert(
			t, "Nested primitive cannot occupy array padding",
			!sbAddType(&sb, "tooLarge", 0, 4, ESBType_F32x4, NULL, t->alloc)
		);

		SBFile readBack = { 0 };
		StreamRef *stream = NULL;
		const RefPtrType type = MemoryStream_makeType(t->alloc);
		U64 offset = 0, readOff = 0;

		Test_assert(t, "Nested stream", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &stream, &t->err));
		Test_assert(t, "Write nested array", SBFile_write(&sb, t->alloc, stream, &offset, &t->err));
		Test_assert(t, "Read nested array", SBFile_read(stream, &readOff, false, t->alloc, &readBack, &t->err));
		Test_assert(t, "Nested layout survives round trip", SBFile_equals(&sb, &readBack));
		SBFile_free(&readBack, t->alloc);
		RefPtr_dec(&stream);
	}

	ListU32_free(&array, t->alloc);
	SBFile_free(&sb, t->alloc);
}

void Test_SBFileSelfAlignment(Test *t) {

	Test_SBFileNestedStructArray(t);

	Test_setModule(t, "SBFile add: types are self aligned, CBuffer types don't span 16 bytes");

	const Allocator *alloc = t->alloc;
	SBFile sb = { 0 };

	Test_assert(t, "create packed", SBFile_create(ESBSettingsFlags_IsTightlyPacked, 64, t->alloc, &sb, &t->err));
	Test_assert(t, "packed F32 at 1", !sbAddType(&sb, "a", 1, U16_MAX, ESBType_F32, NULL, alloc));
	Test_assert(t, "packed F16 at 1", !sbAddType(&sb, "b", 1, U16_MAX, ESBType_F16, NULL, alloc));
	Test_assert(t, "packed U64 at 4", !sbAddType(&sb, "c", 4, U16_MAX, ESBType_U64, NULL, alloc));
	Test_assert(t, "packed U64x2 at 12", !sbAddType(&sb, "d", 12, U16_MAX, ESBType_U64x2, NULL, alloc));
	Test_assert(t, "packed I16 at 2", sbAddType(&sb, "e", 2, U16_MAX, ESBType_I16, NULL, alloc));
	Test_assert(t, "packed F32x4 at 4", sbAddType(&sb, "f", 4, U16_MAX, ESBType_F32x4, NULL, alloc));
	Test_assert(t, "packed U64 at 24", sbAddType(&sb, "g", 24, U16_MAX, ESBType_U64, NULL, alloc));
	SBFile_free(&sb, t->alloc);

	sb = (SBFile) { 0 };
	Test_assert(t, "create cbuffer", SBFile_create(ESBSettingsFlags_None, 64, t->alloc, &sb, &t->err));
	Test_assert(t, "cbuffer F32 at 2", !sbAddType(&sb, "a", 2, U16_MAX, ESBType_F32, NULL, alloc));
	Test_assert(t, "cbuffer F32x4 at 4", !sbAddType(&sb, "b", 4, U16_MAX, ESBType_F32x4, NULL, alloc));
	Test_assert(t, "cbuffer F32x3 at 8", !sbAddType(&sb, "c", 8, U16_MAX, ESBType_F32x3, NULL, alloc));

	U32 dim = 3;
	ListU32 arr = { 0 };

	Test_assert(t, "ref array", ListU32_createRefConst(&dim, 1, &arr, &t->err));
	Test_assert(t, "cbuffer array starts unaligned", !sbAddType(&sb, "d", 4, U16_MAX, ESBType_F32, &arr, alloc));
	Test_assert(t, "cbuffer array starts aligned", sbAddType(&sb, "e", 16, U16_MAX, ESBType_F32, &arr, alloc));
	Test_assert(t, "cbuffer F32x3 at 0", sbAddType(&sb, "f", 0, U16_MAX, ESBType_F32x3, NULL, alloc));
	Test_assert(t, "cbuffer F32 next to it", sbAddType(&sb, "g", 12, U16_MAX, ESBType_F32, NULL, alloc));
	SBFile_free(&sb, t->alloc);
}

//An allocator that refuses everything, to fail a push in the middle of an add

static Bool sbRefuseAlloc(void *ctx, U64 length, Buffer *output, Error *e_rr) {

	(void) ctx;
	(void) length;
	(void) output;

	if (e_rr) *e_rr = Error_outOfMemory(0, "sbRefuseAlloc() refuses every allocation");
	return false;
}

static void sbRefuseFree(void *ctx, Buffer buf) { (void) ctx; (void) buf; }

void Test_SBFileHashUnchangedOnFailure(Test *t) {

	Test_setModule(t, "SBFile add: a failed add leaves the hash alone");

	//NoReserve leaves the lists empty, so the first push has to allocate and fails

	SBFile sb = { 0 };
	Allocator refuse = (Allocator) { .alloc = sbRefuseAlloc, .free = sbRefuseFree };

	Test_assert(t, "create", SBFile_create(ESBSettingsFlags_CreateNoReserve, 16, t->alloc, &sb, &t->err));

	U64 hash = sb.hash;
	CharString sn = CharString_createRefCStrConst("S");

	Test_assert(t, "struct add fails", !SBFile_addStruct(&sb, &sn, (SBStruct) { .stride = 16 }, &refuse, NULL));
	Test_assert(t, "struct hash is unchanged", sb.hash == hash);
	Test_assert(t, "struct isn't kept", sb.structs.length == 0);

	Test_assert(t, "var add fails", !sbAddType(&sb, "a", 0, U16_MAX, ESBType_F32, NULL, &refuse));
	Test_assert(t, "var hash is unchanged", sb.hash == hash);
	Test_assert(t, "var isn't kept", sb.vars.length == 0);

	SBFile_free(&sb, t->alloc);
}

//Combine: the hash is of the combined content, and a buffer read from a file combines with a fresh one

static Bool sbBuildHashFile(Test *t, SBFile *sb, ESBSettingsFlags flags, ESBVarFlag used) {

	CharString sn = CharString_createRefCStrConst("S");
	CharString core = CharString_createRefCStrConst("core");
	CharString x = CharString_createRefCStrConst("x");
	CharString m = CharString_createRefCStrConst("m");

	U32 dims[2] = { 2, 3 };
	ListU32 arr = { 0 };

	return
		SBFile_create(flags, 128, t->alloc, sb, &t->err) &&
		SBFile_addStruct(sb, &sn, (SBStruct) { .stride = 32 }, t->alloc, &t->err) &&
		SBFile_addVariableAsStruct(sb, &core, 0, U16_MAX, 0, used, NULL, t->alloc, &t->err) &&
		SBFile_addVariableAsType(sb, &x, 0, 0, ESBType_F32, used, NULL, t->alloc, &t->err) &&
		ListU32_createRefConst(dims, 2, &arr, &t->err) &&
		SBFile_addVariableAsType(sb, &m, 32, U16_MAX, ESBType_F32, used, &arr, t->alloc, &t->err);
}

void Test_SBFileCombineHash(Test *t) {

	Test_setModule(t, "SBFile combine: hash of the combined content");

	SBFile a = { 0 }, b = { 0 }, both = { 0 }, combined = { 0 }, same = { 0 };

	Test_assert(t, "a", sbBuildHashFile(t, &a, ESBSettingsFlags_None, ESBVarFlag_IsUsedVarSPIRV));
	Test_assert(t, "b", sbBuildHashFile(t, &b, ESBSettingsFlags_None, ESBVarFlag_IsUsedVarDXIL));
	Test_assert(t, "both", sbBuildHashFile(
		t, &both, ESBSettingsFlags_None, (ESBVarFlag) (ESBVarFlag_IsUsedVarSPIRV | ESBVarFlag_IsUsedVarDXIL)
	));

	Test_assert(t, "combine(a, a)", SBFile_combine(&a, &a, t->alloc, &same, &t->err));
	Test_assert(t, "combine(a, a) hash", same.hash == a.hash);
	Test_assert(t, "combine(a, a) equals a", SBFile_equals(&same, &a));

	Test_assert(t, "combine(a, b)", SBFile_combine(&a, &b, t->alloc, &combined, &t->err));
	Test_assert(t, "hash differs from the inputs", combined.hash != a.hash && combined.hash != b.hash);
	Test_assert(t, "hash is of the merged content", combined.hash == both.hash);
	Test_assert(t, "combined equals the merged build", SBFile_equals(&combined, &both));

	SBFile_free(&a, t->alloc);
	SBFile_free(&b, t->alloc);
	SBFile_free(&both, t->alloc);
	SBFile_free(&same, t->alloc);
	SBFile_free(&combined, t->alloc);
}

void Test_SBFileCombineReadBack(Test *t) {

	Test_setModule(t, "SBFile combine: a read back buffer combines with a fresh one");

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	StreamRef *s = NULL;
	SBFile fresh = { 0 }, written = { 0 }, readBack = { 0 }, combined = { 0 };
	U64 off = 0, ro = 0;

	Test_assert(t, "fresh", sbBuildHashFile(t, &fresh, ESBSettingsFlags_None, ESBVarFlag_IsUsedVarSPIRV));
	Test_assert(t, "written", sbBuildHashFile(t, &written, ESBSettingsFlags_HideMagicNumber, ESBVarFlag_IsUsedVarDXIL));
	Test_assert(t, "stream", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &s, &t->err));
	Test_assert(t, "write", SBFile_write(&written, t->alloc, s, &off, &t->err));
	Test_assert(t, "read", SBFile_read(s, &ro, true, t->alloc, &readBack, &t->err));
	Test_assert(t, "read back hides the magic number", readBack.flags & ESBSettingsFlags_HideMagicNumber);

	Test_assert(t, "combine", SBFile_combine(&fresh, &readBack, t->alloc, &combined, &t->err));
	SBFile_free(&combined, t->alloc);
	combined = (SBFile) { 0 };
	Test_assert(t, "combine reversed", SBFile_combine(&readBack, &fresh, t->alloc, &combined, &t->err));

	SBFile_free(&combined, t->alloc);
	SBFile_free(&fresh, t->alloc);
	SBFile_free(&written, t->alloc);
	SBFile_free(&readBack, t->alloc);
	RefPtr_dec(&s);
}

//A write that fails anywhere has to fail the writer

void Test_SBFileWriteFailures(Test *t) {

	Test_setModule(t, "SBFile write: every failing write is reported");

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	SBFile sb = { 0 };

	Test_assert(t, "build", sbBuildHashFile(t, &sb, ESBSettingsFlags_None, ESBVarFlag_IsUsedVarSPIRV));

	StreamRef *s = NULL;
	U64 off = 0;

	Test_assert(t, "stream", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &s, &t->err));
	FailingStream_arm(s, 0);
	Test_assert(t, "counting write", SBFile_write(&sb, t->alloc, s, &off, &t->err));

	U64 writes = FailingStream_writes();

	RefPtr_dec(&s);
	Test_assert(t, "the writer writes", writes > 0);

	for (U64 i = 1; i <= writes; ++i) {

		off = 0;
		Test_assert(t, "stream again", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &s, &t->err));
		FailingStream_arm(s, i);

		Bool ok = SBFile_write(&sb, t->alloc, s, &off, NULL);

		Test_assert(t, "the write was reached", FailingStream_failed());
		Test_assert(t, "a failed write fails the writer", !ok);
		Test_assert(t, "a failed write leaves the offset", !off);
		RefPtr_dec(&s);
	}

	SBFile_free(&sb, t->alloc);
}

//Counts come from the file: nothing big may be requested for a file that can't hold them

typedef struct SBPeakAlloc {
	Allocator inner;
	U64 peak;
} SBPeakAlloc;

static Bool sbPeakAlloc(void *ctx, U64 length, Buffer *output, Error *e_rr) {

	SBPeakAlloc *peak = (SBPeakAlloc*) ctx;

	if (length > peak->peak)
		peak->peak = length;

	//The stream cursor takes a 128 KiB cache, the file's counts would take more
	if (length > 192 * KIBI) {
		if (e_rr) *e_rr = Error_outOfMemory(0, "sbPeakAlloc() capped");
		return false;
	}

	return peak->inner.alloc(peak->inner.ptr, length, output, e_rr);
}

static void sbPeakFree(void *ctx, Buffer buf) {
	SBPeakAlloc *peak = (SBPeakAlloc*) ctx;
	peak->inner.free(peak->inner.ptr, buf);
}

void Test_SBFileReadHugeCounts(Test *t) {

	Test_setModule(t, "SBFile read: counts the stream can't hold are rejected before allocating");

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	for (U32 i = 0; i < 2; ++i) {

		//i = 0 declares 65535 structs and vars; i = 1 declares 255 arrays of 255 dimensions the file has no room for

		U8 file[320] = { 0 };

		SBHeader header = (SBHeader) {
			.version = ESBVersion_V1_2,
			.arrays = i ? 255 : 0,
			.structs = i ? 0 : 65535,
			.vars = i ? 1 : 65535,
			.bufferSize = 16
		};

		U32 magic = SBHeader_MAGIC;

		Buffer_memcpy(Buffer_createRef(file, 4), Buffer_createRefConst(&magic, 4));
		Buffer_memcpy(Buffer_createRef(file + 4, sizeof(header)), Buffer_createRefConst(&header, sizeof(header)));

		U64 used = 16;

		if (i) {

			used += 12;        //The var

			for (U32 j = 0; j < 255; ++j)        //255 arrays of 255 dimensions: 260 KB of U32s
				file[used + j] = 255;

			used += 255;
		}

		StreamRef *s = NULL;
		U64 off = 0;
		StreamCursor cursor = (StreamCursor) { 0 };

		Test_assert(t, "stream", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &s, &t->err));
		Test_assert(t, "cursor", s && StreamCursor_create(s, 0, true, t->alloc, &cursor, &t->err));
		Test_assert(t, "append", s && StreamCursor_append(&cursor, &off, file, used, t->alloc, &t->err));
		Test_assert(t, "flush", s && StreamCursor_flush(&cursor, t->alloc, &t->err));
		StreamCursor_close(&cursor, t->alloc);

		SBPeakAlloc peak = (SBPeakAlloc) { .inner = *t->alloc };
		Allocator capped = (Allocator) { .ptr = &peak, .alloc = sbPeakAlloc, .free = sbPeakFree };

		SBFile result = { 0 };
		U64 ro = 0;

		Test_assert(t, "read fails", s && !SBFile_read(s, &ro, false, &capped, &result, NULL));
		Test_assert(t, "nothing big was requested", peak.peak <= 192 * KIBI);

		SBFile_free(&result, &capped);
		RefPtr_dec(&s);
	}
}

//A failed add leaves the caller's arrays list as it was, also when the add would have reused an entry or inlined it

static Bool sbOwnedList(Test *t, const U32 *values, U64 count, ListU32 *result) {

	ListU32 ref = { 0 };
	return ListU32_createRefConst(values, count, &ref, &t->err) && ListU32_createCopy(ref, t->alloc, result, &t->err);
}

static void sbArraysKeptOnFailure(Test *t, Bool asStruct, const U32 *dims, U64 count) {

	SBFile sb = { 0 };
	Allocator refuse = (Allocator) { .alloc = sbRefuseAlloc, .free = sbRefuseFree };

	CharString sn = CharString_createRefCStrConst("S");
	CharString first = CharString_createRefCStrConst("first");
	CharString second = CharString_createRefCStrConst("second");

	Test_assert(t, "create", SBFile_create(ESBSettingsFlags_None, 256, t->alloc, &sb, &t->err));
	Test_assert(t, "struct", SBFile_addStruct(&sb, &sn, (SBStruct) { .stride = 16 }, t->alloc, &t->err));

	//More than one dimension is stored in the file, so a second identical list is a reuse of that entry

	if (count > 1) {

		ListU32 ref = { 0 };

		Test_assert(t, "ref", ListU32_createRefConst(dims, count, &ref, &t->err));
		Test_assert(t, "first var", asStruct ?
			SBFile_addVariableAsStruct(&sb, &first, 0, U16_MAX, 0, ESBVarFlag_None, &ref, t->alloc, &t->err) :
			SBFile_addVariableAsType(&sb, &first, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, &ref, t->alloc, &t->err)
		);
	}

	//The variable list is created with room for 4, so with 4 in it the next push needs memory the refusing allocator won't give

	for (U32 i = (U32) sb.vars.length; i < 4; ++i) {
		const C8 fillerName[2] = { (C8) ('a' + i), '\0' };
		Test_assert(t, "filler", sbAddType(&sb, fillerName, 192 + i * 4, U16_MAX, ESBType_F32, NULL, t->alloc));
	}

	ListU32 arrays = { 0 };
	Test_assert(t, "owned list", sbOwnedList(t, dims, count, &arrays));

	const U32 *ptr = arrays.ptr;
	const U64 vars = sb.vars.length;
	const U64 hash = sb.hash;

	Test_assert(t, "add fails", asStruct ?
		!SBFile_addVariableAsStruct(&sb, &second, 128, U16_MAX, 0, ESBVarFlag_None, &arrays, &refuse, NULL) :
		!SBFile_addVariableAsType(&sb, &second, 128, U16_MAX, ESBType_F32, ESBVarFlag_None, &arrays, &refuse, NULL)
	);

	Test_assert(t, "file unchanged", sb.vars.length == vars && sb.hash == hash);
	Test_assert(t, "arrays still owned", arrays.ptr == ptr && arrays.length == count);

	Bool same = arrays.ptr && arrays.length == count;

	for (U64 i = 0; same && i < count; ++i)
		same = arrays.ptr[i] == dims[i];

	Test_assert(t, "arrays intact", same);

	//The same list is accepted by a call that succeeds, and is then the file's

	Test_assert(t, "add works", asStruct ?
		SBFile_addVariableAsStruct(&sb, &second, 128, U16_MAX, 0, ESBVarFlag_None, &arrays, t->alloc, &t->err) :
		SBFile_addVariableAsType(&sb, &second, 128, U16_MAX, ESBType_F32, ESBVarFlag_None, &arrays, t->alloc, &t->err)
	);

	Test_assert(t, "arrays emptied on success", !arrays.ptr && !arrays.length);

	ListU32_free(&arrays, t->alloc);
	SBFile_free(&sb, t->alloc);
}

void Test_SBFileArraysKeptOnFailure(Test *t) {

	Test_setModule(t, "SBFile add: a failed add leaves *arrays as it was");

	const U32 inlineDim[1] = { 5 };
	const U32 nd[2] = { 2, 3 };

	sbArraysKeptOnFailure(t, false, inlineDim, 1);
	sbArraysKeptOnFailure(t, false, nd, 2);
	sbArraysKeptOnFailure(t, true, inlineDim, 1);
	sbArraysKeptOnFailure(t, true, nd, 2);
}

//A freed file is empty again, and so is the output of a failed read

void Test_SBFileFreeZeroes(Test *t) {

	Test_setModule(t, "SBFile free: the struct is zeroed and can be created into again");

	SBFile sb = { 0 };

	Test_assert(t, "create", SBFile_create(ESBSettingsFlags_IsTightlyPacked, 32, t->alloc, &sb, &t->err));
	Test_assert(t, "add", sbAddType(&sb, "a", 0, U16_MAX, ESBType_F32, NULL, t->alloc));
	SBFile_free(&sb, t->alloc);

	Test_assert(t, "bufferSize zeroed", sb.bufferSize == 0);
	Test_assert(t, "flags zeroed", sb.flags == 0);
	Test_assert(t, "hash zeroed", sb.hash == 0);
	Test_assert(t, "not filled", !SBFile_isFilled(&sb));
	Test_assert(t, "create again", SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &sb, &t->err));
	SBFile_free(&sb, t->alloc);

	//A read that fails after it allocated is followed by a good one into the same struct

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	StreamRef *s = NULL;
	SBFile src = { 0 }, result = { 0 };
	U64 off = 0;

	Test_assert(t, "create src", SBFile_create(ESBSettingsFlags_None, 32, t->alloc, &src, &t->err));
	Test_assert(t, "add src", sbAddType(&src, "a", 0, U16_MAX, ESBType_F32, NULL, t->alloc));
	Test_assert(t, "stream", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &s, &t->err));
	Test_assert(t, "write", SBFile_write(&src, t->alloc, s, &off, &t->err));

	if (s) {

		MemoryStream *ms = RefPtr_data(s, MemoryStream);

		//[magic 4][header 12][structs 0][SBVar: structId 2, array 2, offset 4]
		const U32 bad = 0xFFFFFFFC;
		U32 good = 0;

		Buffer_memcpy(Buffer_createRef(&good, 4), Buffer_createRefConst(ms->data.ptr + 20, 4));
		Buffer_memcpy(Buffer_createRef(ms->data.ptrNonConst + 20, 4), Buffer_createRefConst(&bad, 4));

		U64 ro = 0;

		Test_assert(t, "bad read fails", !SBFile_read(s, &ro, false, t->alloc, &result, NULL));
		Test_assert(t, "failed read leaves it empty", !SBFile_isFilled(&result));

		Buffer_memcpy(Buffer_createRef(ms->data.ptrNonConst + 20, 4), Buffer_createRefConst(&good, 4));

		ro = 0;
		Test_assert(t, "good read into the same struct", SBFile_read(s, &ro, false, t->alloc, &result, &t->err));
		Test_assert(t, "read equals src", SBFile_equals(&result, &src));
	}

	SBFile_free(&result, t->alloc);
	SBFile_free(&src, t->alloc);
	RefPtr_dec(&s);
}

//Create refuses anything that would leak, whichever member is set

void Test_SBFileCreateRefusesFilled(Test *t) {

	Test_setModule(t, "SBFile create: refuses an output that holds any member");

	const Allocator *alloc = t->alloc;
	SBFile sb = { 0 };

	//Names only, a struct only and an array only

	DLSettings settings = (DLSettings) { .dataType = EDLDataType_String, .flags = EDLSettingsFlags_HideMagicNumber };

	Test_assert(t, "names", DLFile_create(&settings, KIBI, alloc, &sb.names, &t->err));
	Test_assert(t, "names only is refused", !SBFile_create(ESBSettingsFlags_None, 16, alloc, &sb, NULL));
	DLFile_free(&sb.names, alloc);

	Test_assert(t, "struct", ListSBStruct_pushBack(&sb.structs, (SBStruct) { .stride = 16 }, alloc, &t->err));
	Test_assert(t, "struct only is refused", !SBFile_create(ESBSettingsFlags_None, 16, alloc, &sb, NULL));
	ListSBStruct_free(&sb.structs, alloc);

	const U32 nd[2] = { 2, 3 };
	ListU32 dims = { 0 };

	Test_assert(t, "dims", sbOwnedList(t, nd, 2, &dims));
	Test_assert(t, "array", ListListU32_pushBack(&sb.arrays, dims, alloc, &t->err));
	Test_assert(t, "array only is refused", !SBFile_create(ESBSettingsFlags_None, 16, alloc, &sb, NULL));
	ListListU32_freeUnderlying(&sb.arrays, alloc);

	//The other creators refuse the same outputs

	Test_assert(t, "empty is accepted", SBFile_create(ESBSettingsFlags_None, 16, alloc, &sb, &t->err));
	Test_assert(t, "add", sbAddType(&sb, "a", 0, U16_MAX, ESBType_F32, NULL, alloc));
	Test_assert(t, "filled is refused", !SBFile_create(ESBSettingsFlags_None, 16, alloc, &sb, NULL));

	SBFile other = { 0 };

	Test_assert(t, "copy", SBFile_createCopy(&sb, alloc, &other, &t->err));
	Test_assert(t, "copy into filled is refused", !SBFile_createCopy(&sb, alloc, &other, NULL));
	Test_assert(t, "combine into filled is refused", !SBFile_combine(&sb, &other, alloc, &other, NULL));

	SBFile_free(&other, alloc);

	//Names alone count for createCopy and combine too

	Test_assert(t, "names for copy", DLFile_create(&settings, KIBI, alloc, &other.names, &t->err));
	Test_assert(t, "copy into names only is refused", !SBFile_createCopy(&sb, alloc, &other, NULL));
	Test_assert(t, "combine into names only is refused", !SBFile_combine(&sb, &sb, alloc, &other, NULL));
	DLFile_free(&other.names, alloc);

	SBFile_free(&sb, alloc);
}

//The Invalid mask leaves every defined flag alone

void Test_SBFileSettingsFlags(Test *t) {

	Test_setModule(t, "SBFile create: settings flags");

	const ESBSettingsFlags defined = (ESBSettingsFlags) (
		ESBSettingsFlags_HideMagicNumber | ESBSettingsFlags_IsTightlyPacked | ESBSettingsFlags_CreateNoReserve
	);

	Test_assert(t, "Invalid excludes every defined flag", !(ESBSettingsFlags_Invalid & defined));

	SBFile sb = { 0 };

	Test_assert(t, "unknown flag is refused", !SBFile_create((ESBSettingsFlags) (1 << 3), 16, t->alloc, &sb, NULL));
	Test_assert(t, "defined flags are accepted", SBFile_create(defined, 16, t->alloc, &sb, &t->err));
	SBFile_free(&sb, t->alloc);
}

//A struct name can be as long as a variable name and the file format's limit

void Test_SBFileStructNameLength(Test *t) {

	Test_setModule(t, "SBFile addStruct: 128 byte name accepted, 129 refused");

	SBFile sb = { 0 };
	C8 name[130];

	for (U32 i = 0; i < 129; ++i)
		name[i] = 'a';

	name[129] = '\0';

	CharString n129 = CharString_createRefCStrConst(name);
	CharString n128 = CharString_createRefSizedConst(name, 128, false);

	Test_assert(t, "create", SBFile_create(ESBSettingsFlags_None, 32, t->alloc, &sb, &t->err));
	Test_assert(t, "129 refused", !SBFile_addStruct(&sb, &n129, (SBStruct) { .stride = 16 }, t->alloc, NULL));
	Test_assert(t, "refused leaves no struct", sb.structs.length == 0);
	Test_assert(t, "128 accepted", SBFile_addStruct(&sb, &n128, (SBStruct) { .stride = 16 }, t->alloc, &t->err));
	SBFile_free(&sb, t->alloc);
}

//A file that refers to names, structs or arrays that aren't there prints instead of reading out of bounds

void Test_SBFilePrintMalformed(Test *t) {

	Test_setModule(t, "SBFile print: a malformed file doesn't read out of bounds");

	SBFile sb = { 0 };

	Test_assert(t, "create", SBFile_create(ESBSettingsFlags_None, 64, t->alloc, &sb, &t->err));
	Test_assert(t, "add a", sbAddType(&sb, "a", 0, U16_MAX, ESBType_F32, NULL, t->alloc));
	Test_assert(t, "add b", sbAddType(&sb, "b", 16, U16_MAX, ESBType_F32, NULL, t->alloc));

	if (sb.vars.length == 2) {

		SBFile_print(&sb, 0, U16_MAX, true, t->alloc);

		SBVar *vars = sb.vars.ptrNonConst;

		//The lists have no allocation, so reading past their length faults instead of reading stale memory

		ListSBStruct_free(&sb.structs, t->alloc);
		ListListU32_freeUnderlying(&sb.arrays, t->alloc);

		//The struct id points past the structs and the names

		vars[0].structId = 7;
		SBFile_print(&sb, 0, U16_MAX, true, t->alloc);
		vars[0].structId = U16_MAX;

		//The array id points past the arrays

		vars[0].arrayDimOrArrayId = 0x8000 | 9;
		SBFile_print(&sb, 0, U16_MAX, true, t->alloc);
		vars[0].arrayDimOrArrayId = 0;

		//A parent that's the variable itself would recurse forever

		vars[1].parentId = 1;
		SBFile_print(&sb, 0, 1, true, t->alloc);
		vars[1].parentId = U16_MAX;

		//There are fewer names than variables

		DLFile_remove(&sb.names, sb.names.entryStrings.length - 1, t->alloc, NULL);
		SBFile_print(&sb, 0, U16_MAX, true, t->alloc);
	}

	SBFile_free(&sb, t->alloc);
}
