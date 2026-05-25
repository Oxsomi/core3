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

//formats/oiSB/test/test_oiSB_names.c

#include "test_oiSB_shared.h"
#include "formats/oiSB/sb_file.h"
#include "types/base/string.h"
#include "types/base/string_read_helper.h"

//ESBType_create(stride, prim, vec, mat) = (mat << 6) | (stride << 4) | (prim << 2) | vec
//
//Stride: X8=0, X16=1, X32=2, X64=3
//Prim:   Invalid=0, Float=1, Int=2, UInt=3
//Vec:    N1=0, N2=1, N3=2, N4=3
//Matrix: N1=0, N2=1, N3=2, N4=3
//
//Valid names exist where stride != X8 and prim != Invalid.
//Matrix-N1 scalars/vectors:    base[+x2/x3/x4]          e.g. F32, F32x2, F32x3, F32x4
//Matrix-N2/3/4 scalar column:  basex1x<mat>              e.g. F32x1x2, F32x1x3, F32x1x4
//Matrix-N2/3/4 vector columns: base<vec>x<mat>           e.g. F32x2x2, F32x4x3, F32x4x4

static inline Bool cmpstr(const C8 *a, const C8 *b) {
	const CharString str = CharString_createRefCStrConst(a);
	return CharString_equalsCStringSensitive(&str, b);
}

//One-scalar types across all six stride/prim combinations.
void Test_ESBTypeNameScalars(Test *t) {

	Test_setModule(t, "ESBType name: scalars (vec=N1, mat=N1)");

	Test_assert(t, "F16", cmpstr(ESBType_name(ESBType_F16), "F16"));
	Test_assert(t, "I16", cmpstr(ESBType_name(ESBType_I16), "I16"));
	Test_assert(t, "U16", cmpstr(ESBType_name(ESBType_U16), "U16"));
	Test_assert(t, "F32", cmpstr(ESBType_name(ESBType_F32), "F32"));
	Test_assert(t, "I32", cmpstr(ESBType_name(ESBType_I32), "I32"));
	Test_assert(t, "U32", cmpstr(ESBType_name(ESBType_U32), "U32"));
	Test_assert(t, "F64", cmpstr(ESBType_name(ESBType_F64), "F64"));
	Test_assert(t, "I64", cmpstr(ESBType_name(ESBType_I64), "I64"));
	Test_assert(t, "U64", cmpstr(ESBType_name(ESBType_U64), "U64"));
}

//Two-component vector types.
void Test_ESBTypeNameVec2(Test *t) {

	Test_setModule(t, "ESBType name: x2 vectors");

	Test_assert(t, "F16x2", cmpstr(ESBType_name(ESBType_F16x2), "F16x2"));
	Test_assert(t, "I16x2", cmpstr(ESBType_name(ESBType_I16x2), "I16x2"));
	Test_assert(t, "U16x2", cmpstr(ESBType_name(ESBType_U16x2), "U16x2"));
	Test_assert(t, "F32x2", cmpstr(ESBType_name(ESBType_F32x2), "F32x2"));
	Test_assert(t, "I32x2", cmpstr(ESBType_name(ESBType_I32x2), "I32x2"));
	Test_assert(t, "U32x2", cmpstr(ESBType_name(ESBType_U32x2), "U32x2"));
	Test_assert(t, "F64x2", cmpstr(ESBType_name(ESBType_F64x2), "F64x2"));
	Test_assert(t, "I64x2", cmpstr(ESBType_name(ESBType_I64x2), "I64x2"));
	Test_assert(t, "U64x2", cmpstr(ESBType_name(ESBType_U64x2), "U64x2"));
}

//Three-component vector types.
void Test_ESBTypeNameVec3(Test *t) {

	Test_setModule(t, "ESBType name: x3 vectors");

	Test_assert(t, "F16x3", cmpstr(ESBType_name(ESBType_F16x3), "F16x3"));
	Test_assert(t, "I16x3", cmpstr(ESBType_name(ESBType_I16x3), "I16x3"));
	Test_assert(t, "U16x3", cmpstr(ESBType_name(ESBType_U16x3), "U16x3"));
	Test_assert(t, "F32x3", cmpstr(ESBType_name(ESBType_F32x3), "F32x3"));
	Test_assert(t, "I32x3", cmpstr(ESBType_name(ESBType_I32x3), "I32x3"));
	Test_assert(t, "U32x3", cmpstr(ESBType_name(ESBType_U32x3), "U32x3"));
	Test_assert(t, "F64x3", cmpstr(ESBType_name(ESBType_F64x3), "F64x3"));
	Test_assert(t, "I64x3", cmpstr(ESBType_name(ESBType_I64x3), "I64x3"));
	Test_assert(t, "U64x3", cmpstr(ESBType_name(ESBType_U64x3), "U64x3"));
}

//Four-component vector types.
void Test_ESBTypeNameVec4(Test *t) {

	Test_setModule(t, "ESBType name: x4 vectors");

	Test_assert(t, "F16x4", cmpstr(ESBType_name(ESBType_F16x4), "F16x4"));
	Test_assert(t, "I16x4", cmpstr(ESBType_name(ESBType_I16x4), "I16x4"));
	Test_assert(t, "U16x4", cmpstr(ESBType_name(ESBType_U16x4), "U16x4"));
	Test_assert(t, "F32x4", cmpstr(ESBType_name(ESBType_F32x4), "F32x4"));
	Test_assert(t, "I32x4", cmpstr(ESBType_name(ESBType_I32x4), "I32x4"));
	Test_assert(t, "U32x4", cmpstr(ESBType_name(ESBType_U32x4), "U32x4"));
	Test_assert(t, "F64x4", cmpstr(ESBType_name(ESBType_F64x4), "F64x4"));
	Test_assert(t, "I64x4", cmpstr(ESBType_name(ESBType_I64x4), "I64x4"));
	Test_assert(t, "U64x4", cmpstr(ESBType_name(ESBType_U64x4), "U64x4"));
}

//Matrix types: x2 column count (all row/prim combinations for F32).
//Also spot-checks I32 and U32 matrices to guard against prim offset bugs.
void Test_ESBTypeNameMatrixX2(Test *t) {

	Test_setModule(t, "ESBType name: x1x2 matrices");

	//F32 x1x2
	Test_assert(t, "F32x1x2", cmpstr(ESBType_name(ESBType_F32x1x2), "F32x1x2"));
	Test_assert(t, "F32x2x2", cmpstr(ESBType_name(ESBType_F32x2x2), "F32x2x2"));
	Test_assert(t, "F32x3x2", cmpstr(ESBType_name(ESBType_F32x3x2), "F32x3x2"));
	Test_assert(t, "F32x4x2", cmpstr(ESBType_name(ESBType_F32x4x2), "F32x4x2"));

	//I32 x1x2, guard against stride or prim offset bug in the macro table
	Test_assert(t, "I32x1x2", cmpstr(ESBType_name(ESBType_I32x1x2), "I32x1x2"));
	Test_assert(t, "I32x4x2", cmpstr(ESBType_name(ESBType_I32x4x2), "I32x4x2"));

	//U32 x1x2
	Test_assert(t, "U32x1x2", cmpstr(ESBType_name(ESBType_U32x1x2), "U32x1x2"));
	Test_assert(t, "U32x4x2", cmpstr(ESBType_name(ESBType_U32x4x2), "U32x4x2"));
}

//Matrix types: x3 column count.
void Test_ESBTypeNameMatrixX3(Test *t) {

	Test_setModule(t, "ESBType name: x?x3 matrices");

	Test_assert(t, "F32x1x3", cmpstr(ESBType_name(ESBType_F32x1x3), "F32x1x3"));
	Test_assert(t, "F32x2x3", cmpstr(ESBType_name(ESBType_F32x2x3), "F32x2x3"));
	Test_assert(t, "F32x3x3", cmpstr(ESBType_name(ESBType_F32x3x3), "F32x3x3"));
	Test_assert(t, "F32x4x3", cmpstr(ESBType_name(ESBType_F32x4x3), "F32x4x3"));
	Test_assert(t, "I32x1x3", cmpstr(ESBType_name(ESBType_I32x1x3), "I32x1x3"));
	Test_assert(t, "U32x4x3", cmpstr(ESBType_name(ESBType_U32x4x3), "U32x4x3"));
}

//Matrix types: x4 column count, including the F32x4x4 corner.
void Test_ESBTypeNameMatrixX4(Test *t) {

	Test_setModule(t, "ESBType name: x?x4 matrices");

	Test_assert(t, "F32x1x4", cmpstr(ESBType_name(ESBType_F32x1x4), "F32x1x4"));
	Test_assert(t, "F32x2x4", cmpstr(ESBType_name(ESBType_F32x2x4), "F32x2x4"));
	Test_assert(t, "F32x3x4", cmpstr(ESBType_name(ESBType_F32x3x4), "F32x3x4"));
	Test_assert(t, "F32x4x4", cmpstr(ESBType_name(ESBType_F32x4x4), "F32x4x4"));
	Test_assert(t, "I32x1x4", cmpstr(ESBType_name(ESBType_I32x1x4), "I32x1x4"));
	Test_assert(t, "U32x4x4", cmpstr(ESBType_name(ESBType_U32x4x4), "U32x4x4"));
}

//F16 and F64 matrices: ensures the stride dimension is not confused with F32.
void Test_ESBTypeNameMatrixOtherStrides(Test *t) {

	Test_setModule(t, "ESBType name: F16 and F64 matrices");

	Test_assert(t, "F16x4x4", cmpstr(ESBType_name(ESBType_F16x4x4), "F16x4x4"));
	Test_assert(t, "F16x1x2", cmpstr(ESBType_name(ESBType_F16x1x2), "F16x1x2"));
	Test_assert(t, "F64x4x4", cmpstr(ESBType_name(ESBType_F64x4x4), "F64x4x4"));
	Test_assert(t, "F64x1x2", cmpstr(ESBType_name(ESBType_F64x1x2), "F64x1x2"));
	Test_assert(t, "I64x4x4", cmpstr(ESBType_name(ESBType_I64x4x4), "I64x4x4"));
	Test_assert(t, "U64x4x4", cmpstr(ESBType_name(ESBType_U64x4x4), "U64x4x4"));
}

//Invalid slots (stride=X8, or prim=Invalid) must return an empty string, not NULL
//and not a name belonging to a valid type.
void Test_ESBTypeNameInvalidSlots(Test *t) {

	Test_setModule(t, "ESBType name: invalid slots return empty string");

	//stride=X8, prim=Float, vec=N1, mat=N1  -> value = (0<<4)|(1<<2)|0 = 4
	ESBType strideX8 = ESBType_create(ESBStride_X8, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N1);
	const C8 *nameX8 = ESBType_name(strideX8);
	Test_assert(t, "stride X8 returns non-NULL", nameX8 != NULL);
	Test_assert(t, "stride X8 returns empty", nameX8[0] == '\0');

	//prim=Invalid, stride=X32, vec=N1, mat=N1  -> value = (2<<4)|(0<<2)|0 = 32
	ESBType primInvalid = ESBType_create(ESBStride_X32, ESBPrimitive_Invalid, ESBVector_N1, ESBMatrix_N1);
	const C8 *namePrimInv = ESBType_name(primInvalid);
	Test_assert(t, "prim Invalid returns non-NULL", namePrimInv != NULL);
	Test_assert(t, "prim Invalid returns empty", namePrimInv[0] == '\0');

	//stride=X8, prim=Invalid (double-invalid corner)  -> value = 0
	ESBType doubleInvalid = ESBType_create(ESBStride_X8, ESBPrimitive_Invalid, ESBVector_N1, ESBMatrix_N1);
	const C8 *nameDouble = ESBType_name(doubleInvalid);
	Test_assert(t, "double-invalid returns non-NULL", nameDouble != NULL);
	Test_assert(t, "double-invalid returns empty", nameDouble[0] == '\0');

	//stride=X8 with a matrix component, still invalid
	ESBType strideX8Mat = ESBType_create(ESBStride_X8, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N4);
	const C8 *nameX8Mat = ESBType_name(strideX8Mat);
	Test_assert(t, "stride X8 mat N4 returns non-NULL", nameX8Mat != NULL);
	Test_assert(t, "stride X8 mat N4 returns empty", nameX8Mat[0] == '\0');
}

//ESBType_name must return a distinct pointer for every valid type and the
//returned string must not be shared with a different valid type's name.
//This catches accidental aliasing in the name table initialisation.
void Test_ESBTypeNameUniqueness(Test *t) {

	Test_setModule(t, "ESBType name: all valid names are unique strings");

	//Collect all non-empty names and check for duplicates by string value.
	//There are 256 possible type byte values (8-bit field).
	//144 of them are valid; the rest must be empty.
	const C8 *seen[256] = { 0 };
	U32 validCount = 0;
	U32 emptyCount = 0;
	Bool collision = false;

	for (U32 i = 0; i < 256; ++i) {
		const C8 *name = ESBType_name((ESBType)i);

		Test_assert(t, "name not NULL", name != NULL);

		if (!name || name[0] == '\0') {
			++emptyCount;
			continue;
		}

		++validCount;

		//Check this string hasn't appeared before
		for (U32 j = 0; j < i; ++j) {
			if (seen[j] && cmpstr(seen[j], name)) {
				collision = true;
				break;
			}
		}

		seen[i] = name;
	}

	Test_assert(t, "144 valid names", validCount == 144);
	Test_assert(t, "112 empty slots", emptyCount == 112);
	Test_assert(t, "no name collision", !collision);
}
