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

//formats/oiSB/test/test_oiSB_arrays.c

#include "test_oiSB_shared.h"
#include "formats/oiSB/sb_file.h"
#include "types/container/memory_stream.h"
#include "types/container/list_basic_types.h"
#include "types/base/string.h"

extern Bool sbRoundTrip(Test *t, const SBFile *src, StreamRef **archiveSr, SBFile *result, const RefPtrType *type);

//1D array variable (inline dimension <= 32767). Verifies arrayDimOrArrayId encoding.
void Test_SBFileRoundTripInlineArray(Test *t) {

	Test_setModule(t, "SBFile arrays: inline 1D array (dim=4)");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		SBFile sb     = { 0 };
		SBFile result = { 0 };
		StreamRef *archiveSr = NULL;

		//F32x4[4] at offset 0, each element takes 16 bytes -> 64 bytes total
		if (!SBFile_create(ESBSettingsFlags_None, 64, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile inline arr", false);
			goto doneInlineArr;
		}

		U32 dim = 4;
		ListU32 arr = { 0 };
		Test_assert(t, "create arr ref", ListU32_createRefConst(&dim, 1, &arr, &t->err));

		CharString name = CharString_createRefCStrConst("myArr");
		Test_assert(t, "add array var",
			SBFile_addVariableAsType(&sb, &name, 0, U16_MAX, ESBType_F32x4, ESBVarFlag_None, &arr, t->alloc, &t->err)
		);

		Test_assert(t, "inline arr round-trip", sbRoundTrip(t, &sb, &archiveSr, &result, &type));

		Test_assert(t, "vars count", result.vars.length == 1);

		if (result.vars.length == 1) {
			//Inline: bit15 must NOT be set, value == dim
			U16 arrayField = result.vars.ptr[0].arrayDimOrArrayId;
			Test_assert(t, "inline bit15 clear", !(arrayField >> 15));
			Test_assert(t, "inline dim == 4",    (arrayField & (U16)I16_MAX) == 4);
			Test_assert(t, "var type",           result.vars.ptr[0].type == ESBType_F32x4);
		}

	doneInlineArr:
		SBFile_free(&sb, t->alloc);
		SBFile_free(&result, t->alloc);
		RefPtr_dec(&archiveSr);
	}
}

//Array dimension of exactly 32767 must still be stored inline (highest value below the threshold).
void Test_SBFileInlineArrayBoundary32767(Test *t) {

	Test_setModule(t, "SBFile arrays: dim==32767 stays inline (boundary)");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		SBFile sb     = { 0 };
		SBFile result = { 0 };
		StreamRef *archiveSr = NULL;

		//F32[32767] tightly packed = 32767 * 4 = 131068 bytes
		if (!SBFile_create(ESBSettingsFlags_IsTightlyPacked, 131068, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile dim32767", false);
			goto done32767;
		}

		U32 dim = 32767;
		ListU32 arr = { 0 };
		Test_assert(t, "create dim32767 ref", ListU32_createRefConst(&dim, 1, &arr, &t->err));

		CharString name = CharString_createRefCStrConst("nearBig");
		Test_assert(t, "add dim32767 var",
			SBFile_addVariableAsType(&sb, &name, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, &arr, t->alloc, &t->err)
		);

		//Must be inline: no array table entry should be allocated
		Test_assert(t, "no array table entry", sb.arrays.length == 0);

		if (sb.vars.length == 1) {
			U16 arrayField = sb.vars.ptr[0].arrayDimOrArrayId;
			Test_assert(t, "bit15 clear for 32767", !(arrayField >> 15));
			Test_assert(t, "inline dim == 32767",    arrayField == 32767);
		}

		Test_assert(t, "dim32767 round-trip", sbRoundTrip(t, &sb, &archiveSr, &result, &type));

		if (result.vars.length == 1) {
			U16 arrayField = result.vars.ptr[0].arrayDimOrArrayId;
			Test_assert(t, "rt bit15 clear",  !(arrayField >> 15));
			Test_assert(t, "rt dim == 32767", (arrayField & (U16)I16_MAX) == 32767);
		}

	done32767:
		SBFile_free(&sb, t->alloc);
		SBFile_free(&result, t->alloc);
		RefPtr_dec(&archiveSr);
	}
}

//Array dimension of exactly 32768 (0x8000) cannot fit in the 15-bit inline field.
//Must be stored in the full array table with bit15 set, and survive round-trip.
void Test_SBFileRoundTripArrayDim32768(Test *t) {

	Test_setModule(t, "SBFile arrays: dim==32768 forces full array table (boundary)");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		SBFile sb     = { 0 };
		SBFile result = { 0 };
		StreamRef *archiveSr = NULL;

		//F32[32768] tightly packed = 32768 * 4 = 131072 bytes
		if (!SBFile_create(ESBSettingsFlags_IsTightlyPacked, 131072, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile dim32768", false);
			goto done32768;
		}

		U32 dim = 32768;
		ListU32 arr = { 0 };
		Test_assert(t, "create dim32768 ref", ListU32_createRefConst(&dim, 1, &arr, &t->err));

		CharString name = CharString_createRefCStrConst("bigArr");
		Test_assert(t, "add dim32768 var",
			SBFile_addVariableAsType(&sb, &name, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, &arr, t->alloc, &t->err)
		);

		//Inline threshold is <= 32767; 32768 must go through the full array table
		Test_assert(t, "array table allocated", sb.arrays.length == 1);

		if (sb.vars.length == 1) {
			U16 arrayField = sb.vars.ptr[0].arrayDimOrArrayId;
			Test_assert(t, "bit15 set for 32768", arrayField >> 15);
			Test_assert(t, "arrayIdx is 0",       (arrayField & (U16)I16_MAX) == 0);
		}

		Test_assert(t, "dim32768 round-trip", sbRoundTrip(t, &sb, &archiveSr, &result, &type));

		Test_assert(t, "rt vars count",   result.vars.length   == 1);
		Test_assert(t, "rt arrays count", result.arrays.length == 1);

		if (result.vars.length == 1 && result.arrays.length >= 1) {
			U16 arrayField = result.vars.ptr[0].arrayDimOrArrayId;
			Test_assert(t, "rt bit15 set", arrayField >> 15);
			U16 arrayIdx = arrayField & (U16)I16_MAX;
			Test_assert(t, "rt arrayIdx ok", arrayIdx < result.arrays.length);
			if (arrayIdx < result.arrays.length) {
				Test_assert(t, "rt array length 1", result.arrays.ptr[arrayIdx].length == 1);
				if (result.arrays.ptr[arrayIdx].length == 1)
					Test_assert(t, "rt dim == 32768", result.arrays.ptr[arrayIdx].ptr[0] == 32768);
			}
		}

	done32768:
		SBFile_free(&sb, t->alloc);
		SBFile_free(&result, t->alloc);
		RefPtr_dec(&archiveSr);
	}
}

//Multi-dimensional array (2D: [3][3]). Verifies full array table path (bit15 set).
void Test_SBFileRoundTripNDArray(Test *t) {

	Test_setModule(t, "SBFile arrays: multi-dimensional [3][3]");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		SBFile sb     = { 0 };
		SBFile result = { 0 };
		StreamRef *archiveSr = NULL;

		//F32[3][3] = 9 elements, each 16 bytes (cbuffer) = 144 bytes
		if (!SBFile_create(ESBSettingsFlags_None, 144, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile ND arr", false);
			goto doneND;
		}

		U32 dims[2] = { 3, 3 };
		ListU32 arr = { 0 };
		Test_assert(t, "create ND arr ref", ListU32_createRefConst(dims, 2, &arr, &t->err));

		CharString name = CharString_createRefCStrConst("matrix33");
		Test_assert(t, "add ND array var",
			SBFile_addVariableAsType(&sb, &name, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, &arr, t->alloc, &t->err)
		);

		Test_assert(t, "ND arr round-trip", sbRoundTrip(t, &sb, &archiveSr, &result, &type));

		Test_assert(t, "vars count",   result.vars.length   == 1);
		Test_assert(t, "arrays count", result.arrays.length == 1);

		if (result.vars.length == 1) {
			U16 arrayField = result.vars.ptr[0].arrayDimOrArrayId;
			Test_assert(t, "ND bit15 set", arrayField >> 15);

			U16 arrayIdx = arrayField & (U16)I16_MAX;
			Test_assert(t, "arrayIdx in bounds", arrayIdx < result.arrays.length);

			if (arrayIdx < result.arrays.length) {
				Test_assert(t, "ND dims count", result.arrays.ptr[arrayIdx].length == 2);
				if (result.arrays.ptr[arrayIdx].length == 2) {
					Test_assert(t, "ND dim[0]", result.arrays.ptr[arrayIdx].ptr[0] == 3);
					Test_assert(t, "ND dim[1]", result.arrays.ptr[arrayIdx].ptr[1] == 3);
				}
			}
		}

	doneND:
		SBFile_free(&sb, t->alloc);
		SBFile_free(&result, t->alloc);
		RefPtr_dec(&archiveSr);
	}
}

//Two variables with the same ND array shape must share a single array table entry.
void Test_SBFileArrayDeduplication(Test *t) {

	Test_setModule(t, "SBFile arrays: identical ND arrays are deduplicated");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		SBFile sb     = { 0 };
		SBFile result = { 0 };
		StreamRef *archiveSr = NULL;

		//Two F32[32768] vars at different offsets, tightly packed.
		//Each has a 1-element array with dim=32768 -> both go through the array table.
		//They should share one entry, not duplicate it.
		if (!SBFile_create(ESBSettingsFlags_IsTightlyPacked, 262144, t->alloc, &sb, &t->err)) {
			Test_assert(t, "create SBFile dedup", false);
			goto doneDedup;
		}

		U32 dim = 32768;
		ListU32 arrA = { 0 }, arrB = { 0 };
		Test_assert(t, "ref A", ListU32_createRefConst(&dim, 1, &arrA, &t->err));
		Test_assert(t, "ref B", ListU32_createRefConst(&dim, 1, &arrB, &t->err));

		CharString nameA = CharString_createRefCStrConst("arrA");
		CharString nameB = CharString_createRefCStrConst("arrB");
		Test_assert(t, "add arrA",
			SBFile_addVariableAsType(&sb, &nameA, 0,      U16_MAX, ESBType_F32, ESBVarFlag_None, &arrA, t->alloc, &t->err)
		);
		Test_assert(t, "add arrB",
			SBFile_addVariableAsType(&sb, &nameB, 131072, U16_MAX, ESBType_F32, ESBVarFlag_None, &arrB, t->alloc, &t->err)
		);

		//Both reference the same dims, so only one array table entry should exist
		Test_assert(t, "arrays deduped to 1", sb.arrays.length == 1);

		if (sb.vars.length == 2) {
			U16 idxA = sb.vars.ptr[0].arrayDimOrArrayId & (U16)I16_MAX;
			U16 idxB = sb.vars.ptr[1].arrayDimOrArrayId & (U16)I16_MAX;
			Test_assert(t, "both point to same entry", idxA == idxB);
		}

		Test_assert(t, "dedup round-trip",     sbRoundTrip(t, &sb, &archiveSr, &result, &type));
		Test_assert(t, "rt arrays count == 1", result.arrays.length == 1);

	doneDedup:
		SBFile_free(&sb, t->alloc);
		SBFile_free(&result, t->alloc);
		RefPtr_dec(&archiveSr);
	}
}
