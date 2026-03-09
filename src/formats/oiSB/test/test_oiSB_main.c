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

#include "test_oiSB_shared.h"
#include "types/container/test/basic_alloc.h"

int main() {

	const Allocator alloc = BasicAllocator_instance;
	Test t = (Test){ 0 };
	t.alloc = &alloc;

	Test_SBFileRoundTripSimpleF32(&t);
	Test_SBFileRoundTripMultipleVars(&t);
	Test_SBFileRoundTripStruct(&t);
	Test_SBFileRoundTripTightlyPacked(&t);
	Test_SBFileSubFileRoundTrip(&t);
	Test_SBFileCreateCopy(&t);
	Test_SBFileAllowSameNameInDifferentScopes(&t);

	Test_SBFileRoundTripInlineArray(&t);
	Test_SBFileInlineArrayBoundary32767(&t);
	Test_SBFileRoundTripArrayDim32768(&t);
	Test_SBFileRoundTripNDArray(&t);
	Test_SBFileArrayDeduplication(&t);

	Test_SBFileAddDuplicateVarName(&t);
	Test_SBFileAddVarOutOfBounds(&t);
	Test_SBFileAddVarInvalidParentId(&t);
	Test_SBFileAddVarParentIsPrimitive(&t);
	Test_SBFileAddVarStructIdOutOfBounds(&t);
	Test_SBFileAddStructVarMisalignedOffset(&t);
	Test_SBFileAddStructZeroStride(&t);
	Test_SBFileNameLengthBoundary(&t);
	Test_SBFileAddVarEmptyArrayList(&t);
	Test_SBFileReadInvalidStream(&t);
	Test_SBFileWriteEmpty(&t);
	Test_SBFileSizeConsistency(&t);

	Test_ESBTypeValuesF16(&t);
	Test_ESBTypeValuesI16(&t);
	Test_ESBTypeValuesU16(&t);
	Test_ESBTypeValuesF32(&t);
	Test_ESBTypeValuesI32(&t);
	Test_ESBTypeValuesU32(&t);
	Test_ESBTypeValuesF64(&t);
	Test_ESBTypeValuesI64(&t);
	Test_ESBTypeValuesU64(&t);
	Test_ESBTypeValuesF32Matrices(&t);
	Test_ESBTypeValuesMatrixOtherStrides(&t);

	Test_ESBTypeNameScalars(&t);
	Test_ESBTypeNameVec2(&t);
	Test_ESBTypeNameVec3(&t);
	Test_ESBTypeNameVec4(&t);
	Test_ESBTypeNameMatrixX2(&t);
	Test_ESBTypeNameMatrixX3(&t);
	Test_ESBTypeNameMatrixX4(&t);
	Test_ESBTypeNameMatrixOtherStrides(&t);
	Test_ESBTypeNameInvalidSlots(&t);
	Test_ESBTypeNameUniqueness(&t);

	Test_SBFileCombineFlags(&t);
	Test_SBFileCombineBufferSizeMismatch(&t);
	Test_SBFileCombineMissingVariable(&t);
	Test_SBFileCombineTypeMismatch(&t);
	Test_SBFileCombineUnflatten(&t);
	Test_SBFileCombineFlatSizeMismatch(&t);
	Test_SBFileHashConsistency(&t);

	BasicAllocator_checkLeakedMem(&t);
	return Test_end(&t);
}
