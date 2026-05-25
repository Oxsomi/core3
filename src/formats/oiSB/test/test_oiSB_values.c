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

//formats/oiSB/test/test_oiSB_values.c

#include "test_oiSB_shared.h"
#include "formats/oiSB/sb_file.h"

//Helper macro: assert all four fields of a type identifier in one line.
#define assertType(t, id, stride, prim, vec, mat)													\
	Test_assert(t, #id " stride", ESBType_getStride(id)    == (stride));							\
	Test_assert(t, #id " prim",   ESBType_getPrimitive(id) == (prim));								\
	Test_assert(t, #id " vec",    ESBType_getVector(id)    == (vec));								\
	Test_assert(t, #id " mat",    ESBType_getMatrix(id)    == (mat))

//F16 scalars and vectors.
void Test_ESBTypeValuesF16(Test *t) {

	Test_setModule(t, "ESBType values: F16");

	assertType(t, ESBType_F16, ESBStride_X16, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N1);
	assertType(t, ESBType_F16x2, ESBStride_X16, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N1);
	assertType(t, ESBType_F16x3, ESBStride_X16, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N1);
	assertType(t, ESBType_F16x4, ESBStride_X16, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N1);
}

//I16 scalars and vectors.
void Test_ESBTypeValuesI16(Test *t) {

	Test_setModule(t, "ESBType values: I16");

	assertType(t, ESBType_I16, ESBStride_X16, ESBPrimitive_Int, ESBVector_N1, ESBMatrix_N1);
	assertType(t, ESBType_I16x2, ESBStride_X16, ESBPrimitive_Int, ESBVector_N2, ESBMatrix_N1);
	assertType(t, ESBType_I16x3, ESBStride_X16, ESBPrimitive_Int, ESBVector_N3, ESBMatrix_N1);
	assertType(t, ESBType_I16x4, ESBStride_X16, ESBPrimitive_Int, ESBVector_N4, ESBMatrix_N1);
}

//U16 scalars and vectors.
void Test_ESBTypeValuesU16(Test *t) {

	Test_setModule(t, "ESBType values: U16");

	assertType(t, ESBType_U16, ESBStride_X16, ESBPrimitive_UInt, ESBVector_N1, ESBMatrix_N1);
	assertType(t, ESBType_U16x2, ESBStride_X16, ESBPrimitive_UInt, ESBVector_N2, ESBMatrix_N1);
	assertType(t, ESBType_U16x3, ESBStride_X16, ESBPrimitive_UInt, ESBVector_N3, ESBMatrix_N1);
	assertType(t, ESBType_U16x4, ESBStride_X16, ESBPrimitive_UInt, ESBVector_N4, ESBMatrix_N1);
}

//F32 scalars and vectors.
void Test_ESBTypeValuesF32(Test *t) {

	Test_setModule(t, "ESBType values: F32");

	assertType(t, ESBType_F32, ESBStride_X32, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N1);
	assertType(t, ESBType_F32x2, ESBStride_X32, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N1);
	assertType(t, ESBType_F32x3, ESBStride_X32, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N1);
	assertType(t, ESBType_F32x4, ESBStride_X32, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N1);
}

//I32 scalars and vectors.
void Test_ESBTypeValuesI32(Test *t) {

	Test_setModule(t, "ESBType values: I32");

	assertType(t, ESBType_I32, ESBStride_X32, ESBPrimitive_Int, ESBVector_N1, ESBMatrix_N1);
	assertType(t, ESBType_I32x2, ESBStride_X32, ESBPrimitive_Int, ESBVector_N2, ESBMatrix_N1);
	assertType(t, ESBType_I32x3, ESBStride_X32, ESBPrimitive_Int, ESBVector_N3, ESBMatrix_N1);
	assertType(t, ESBType_I32x4, ESBStride_X32, ESBPrimitive_Int, ESBVector_N4, ESBMatrix_N1);
}

//U32 scalars and vectors.
void Test_ESBTypeValuesU32(Test *t) {

	Test_setModule(t, "ESBType values: U32");

	assertType(t, ESBType_U32, ESBStride_X32, ESBPrimitive_UInt, ESBVector_N1, ESBMatrix_N1);
	assertType(t, ESBType_U32x2, ESBStride_X32, ESBPrimitive_UInt, ESBVector_N2, ESBMatrix_N1);
	assertType(t, ESBType_U32x3, ESBStride_X32, ESBPrimitive_UInt, ESBVector_N3, ESBMatrix_N1);
	assertType(t, ESBType_U32x4, ESBStride_X32, ESBPrimitive_UInt, ESBVector_N4, ESBMatrix_N1);
}

//F64 scalars and vectors.
void Test_ESBTypeValuesF64(Test *t) {

	Test_setModule(t, "ESBType values: F64");

	assertType(t, ESBType_F64, ESBStride_X64, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N1);
	assertType(t, ESBType_F64x2, ESBStride_X64, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N1);
	assertType(t, ESBType_F64x3, ESBStride_X64, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N1);
	assertType(t, ESBType_F64x4, ESBStride_X64, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N1);
}

//I64 scalars and vectors.
void Test_ESBTypeValuesI64(Test *t) {

	Test_setModule(t, "ESBType values: I64");

	assertType(t, ESBType_I64, ESBStride_X64, ESBPrimitive_Int, ESBVector_N1, ESBMatrix_N1);
	assertType(t, ESBType_I64x2, ESBStride_X64, ESBPrimitive_Int, ESBVector_N2, ESBMatrix_N1);
	assertType(t, ESBType_I64x3, ESBStride_X64, ESBPrimitive_Int, ESBVector_N3, ESBMatrix_N1);
	assertType(t, ESBType_I64x4, ESBStride_X64, ESBPrimitive_Int, ESBVector_N4, ESBMatrix_N1);
}

//U64 scalars and vectors.
void Test_ESBTypeValuesU64(Test *t) {

	Test_setModule(t, "ESBType values: U64");

	assertType(t, ESBType_U64, ESBStride_X64, ESBPrimitive_UInt, ESBVector_N1, ESBMatrix_N1);
	assertType(t, ESBType_U64x2, ESBStride_X64, ESBPrimitive_UInt, ESBVector_N2, ESBMatrix_N1);
	assertType(t, ESBType_U64x3, ESBStride_X64, ESBPrimitive_UInt, ESBVector_N3, ESBMatrix_N1);
	assertType(t, ESBType_U64x4, ESBStride_X64, ESBPrimitive_UInt, ESBVector_N4, ESBMatrix_N1);
}

//F32 matrix types: all four column counts, covering all row counts.
//These are the most commonly used matrix types and the most likely to be
//mis-encoded since the mat field sits in the high two bits.
void Test_ESBTypeValuesF32Matrices(Test *t) {

	Test_setModule(t, "ESBType values: F32 matrices");

	assertType(t, ESBType_F32x1x2, ESBStride_X32, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N2);
	assertType(t, ESBType_F32x2x2, ESBStride_X32, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N2);
	assertType(t, ESBType_F32x3x2, ESBStride_X32, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N2);
	assertType(t, ESBType_F32x4x2, ESBStride_X32, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N2);

	assertType(t, ESBType_F32x1x3, ESBStride_X32, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N3);
	assertType(t, ESBType_F32x2x3, ESBStride_X32, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N3);
	assertType(t, ESBType_F32x3x3, ESBStride_X32, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N3);
	assertType(t, ESBType_F32x4x3, ESBStride_X32, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N3);

	assertType(t, ESBType_F32x1x4, ESBStride_X32, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N4);
	assertType(t, ESBType_F32x2x4, ESBStride_X32, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N4);
	assertType(t, ESBType_F32x3x4, ESBStride_X32, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N4);
	assertType(t, ESBType_F32x4x4, ESBStride_X32, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N4);
}

//Spot-check one matrix type per non-F32 stride to guard against the mat field
//corrupting the stride bits or vice versa.
void Test_ESBTypeValuesMatrixOtherStrides(Test *t) {

	Test_setModule(t, "ESBType values: matrices at F16/I32/U32/F64 strides");

	assertType(t, ESBType_F16x4x4, ESBStride_X16, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N4);
	assertType(t, ESBType_F16x1x2, ESBStride_X16, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N2);
	assertType(t, ESBType_I32x4x4, ESBStride_X32, ESBPrimitive_Int, ESBVector_N4, ESBMatrix_N4);
	assertType(t, ESBType_U32x4x4, ESBStride_X32, ESBPrimitive_UInt, ESBVector_N4, ESBMatrix_N4);
	assertType(t, ESBType_F64x4x4, ESBStride_X64, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N4);
	assertType(t, ESBType_I64x4x4, ESBStride_X64, ESBPrimitive_Int, ESBVector_N4, ESBMatrix_N4);
	assertType(t, ESBType_U64x4x4, ESBStride_X64, ESBPrimitive_UInt, ESBVector_N4, ESBMatrix_N4);
}
