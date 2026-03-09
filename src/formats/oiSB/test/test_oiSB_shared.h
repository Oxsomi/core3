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

#pragma once
#include "types/test/test.h"

void Test_SBFileRoundTripSimpleF32(Test *t);
void Test_SBFileRoundTripMultipleVars(Test *t);
void Test_SBFileRoundTripStruct(Test *t);
void Test_SBFileRoundTripTightlyPacked(Test *t);
void Test_SBFileSubFileRoundTrip(Test *t);
void Test_SBFileCreateCopy(Test *t);
void Test_SBFileAllowSameNameInDifferentScopes(Test *t);

void Test_SBFileRoundTripInlineArray(Test *t);
void Test_SBFileInlineArrayBoundary32767(Test *t);
void Test_SBFileRoundTripArrayDim32768(Test *t);
void Test_SBFileRoundTripNDArray(Test *t);
void Test_SBFileArrayDeduplication(Test *t);

void Test_SBFileAddDuplicateVarName(Test *t);
void Test_SBFileAddVarOutOfBounds(Test *t);
void Test_SBFileAddVarInvalidParentId(Test *t);
void Test_SBFileAddVarParentIsPrimitive(Test *t);
void Test_SBFileAddVarStructIdOutOfBounds(Test *t);
void Test_SBFileAddStructVarMisalignedOffset(Test *t);
void Test_SBFileAddStructZeroStride(Test *t);
void Test_SBFileNameLengthBoundary(Test *t);
void Test_SBFileAddVarEmptyArrayList(Test *t);
void Test_SBFileReadInvalidStream(Test *t);
void Test_SBFileWriteEmpty(Test *t);

void Test_SBFileSizeConsistency(Test *t);

void Test_ESBTypeValuesF16(Test *t);
void Test_ESBTypeValuesI16(Test *t);
void Test_ESBTypeValuesU16(Test *t);
void Test_ESBTypeValuesF32(Test *t);
void Test_ESBTypeValuesI32(Test *t);
void Test_ESBTypeValuesU32(Test *t);
void Test_ESBTypeValuesF64(Test *t);
void Test_ESBTypeValuesI64(Test *t);
void Test_ESBTypeValuesU64(Test *t);
void Test_ESBTypeValuesF32Matrices(Test *t);
void Test_ESBTypeValuesMatrixOtherStrides(Test *t);

void Test_ESBTypeNameScalars(Test *t);
void Test_ESBTypeNameVec2(Test *t);
void Test_ESBTypeNameVec3(Test *t);
void Test_ESBTypeNameVec4(Test *t);
void Test_ESBTypeNameMatrixX2(Test *t);
void Test_ESBTypeNameMatrixX3(Test *t);
void Test_ESBTypeNameMatrixX4(Test *t);
void Test_ESBTypeNameMatrixOtherStrides(Test *t);
void Test_ESBTypeNameInvalidSlots(Test *t);
void Test_ESBTypeNameUniqueness(Test *t);

void Test_SBFileCombineFlags(Test *t);
void Test_SBFileCombineBufferSizeMismatch(Test *t);
void Test_SBFileCombineMissingVariable(Test *t);
void Test_SBFileCombineTypeMismatch(Test *t);
void Test_SBFileCombineUnflatten(Test *t);
void Test_SBFileCombineFlatSizeMismatch(Test *t);
void Test_SBFileHashConsistency(Test *t);
