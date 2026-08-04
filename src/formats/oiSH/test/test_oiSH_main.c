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

//formats/oiSH/test/test_oiSH_main.c

#include "types/test/test.h"
#include "test_oiSH_shared.h"
#include "types/container/test/basic_alloc.h"

OXC3_TEST_MAIN(formats_oiSH) {

	const Allocator alloc = BasicAllocator_instance;
	Test t = (Test){ 0 };
	t.alloc = &alloc;

	Test_SHFileCreateFree(&t);
	Test_SHFileCreateAlreadyAllocated(&t);
	Test_SHFileCreateInvalidFlags(&t);

	Test_SHEntryStageName(&t);
	Test_SHPipelineStagePrefixes(&t);

	Test_SHExtensionSetSanity(&t);
	Test_SHVendorNames(&t);
	Test_SHBinaryTypeNames(&t);
	Test_SHBindingsDummy(&t);

	Test_SHEntryRuntimeGetCombinations(&t);
	Test_SHBinaryIdentifierEquals(&t);

	Test_SHFileAddIncNullGuards(&t);
	Test_SHFileAddIncEmptyPathRejected(&t);
	Test_SHFileAddIncZeroCrcRejected(&t);
	Test_SHFileAddIncDuplicateSameCrc(&t);
	Test_SHFileAddIncDuplicateDifferentCrc(&t);
	Test_SHFileAddIncSortedOrder(&t);
	Test_SHFileAddIncMultipleDistinct(&t);

	Test_SHFileAddEntryNullGuards(&t);
	Test_SHFileAddEntryEmptyName(&t);
	Test_SHFileAddEntryInvalidStage(&t);
	Test_SHFileAddEntryComputeInvalidGroup(&t);
	Test_SHFileAddEntryNonRTPayloadRejected(&t);
	Test_SHFileAddEntryGraphicsCannotHaveGroup(&t);
	Test_SHFileAddEntryIntersectionOnWrongStage(&t);
	Test_SHFileAddEntryWaveSizeWorkgraph(&t);
	Test_SHFileAddEntryWaveSizeInvalidNibbles(&t);
	Test_SHFileAddEntryWaveSizeOnNonCompute(&t);
	Test_SHFileAddEntryHitStageSizes(&t);
	Test_SHFileAddEntryCallablePayload(&t);
	Test_SHFileAddEntryMissMustHavePayload(&t);
	Test_SHFileAddEntryTaskNeedsGroup(&t);
	Test_SHFileAddEntryRaygenNoPayload(&t);
	Test_SHFileAddEntryMeshNeedsGroup(&t);
	Test_SHFileAddEntryIOOnlyGraphics(&t);
	Test_SHFileAddEntryUniqueInputSemanticsLimit(&t);
	Test_SHFileAddEntrySemanticNamesLessThanUniqueInputs(&t);
	Test_SHFileAddEntryBinaryIdOutOfBounds(&t);
	Test_SHFileEntryAndBinaryLinked(&t);

	Test_SHFileAddBinNullGuards(&t);
	Test_SHFileAddBinNoBinaryData(&t);
	Test_SHFileAddBinZeroVendorMask(&t);
	Test_SHFileAddBinZeroVendorMask(&t);
	Test_SHFileAddBinVendorMaskNormalized(&t);
	Test_SHFileAddBinVendorMaskOutOfBounds(&t);
	Test_SHFileAddBinInvalidExtensions(&t);
	Test_SHFileAddBinInvalidStageType(&t);
	Test_SHFileAddBinShaderVersionOutOfRange(&t);
	Test_SHFileAddBinShaderVersionAtBoundaries(&t);
	Test_SHFileAddBinSPIRVMisaligned(&t);
	Test_SHFileAddBinEntrypointAnnotationMutuallyExclusive(&t);
	Test_SHFileAddBinOddDefinesRejected(&t);
	Test_SHFileAddBinDuplicateDefineNameRejected(&t);
	Test_SHFileAddBinValidDefines(&t);
	Test_SHFileAddBinUniformEmptyName(&t);
	Test_SHFileAddBinUniformDigitFirstChar(&t);
	Test_SHFileAddBinUniformUnderscoreStart(&t);
	Test_SHFileAddBinUniformInvalidBodyChar(&t);
	Test_SHFileAddBinUniformDuplicateName(&t);
	Test_SHFileAddBinValid(&t);
	Test_SHFileAddBinDuplicateIdentifierRejected(&t);
	Test_SHFileAddBinMultipleDistinct(&t);
	Test_SHFileAddBinExtensionFlagsStored(&t);

	Test_SHFileRegisterAddSampler(&t);
	Test_SHFileRegisterAddSamplerComparisonState(&t);
	Test_SHFileRegisterDuplicateNameRejected(&t);
	Test_SHFileRegisterDuplicateSPIRVBindingRejected(&t);
	Test_SHFileRegisterAddTexture(&t);
	Test_SHFileRegisterAddRWTexture(&t);
	Test_SHFileRegisterArray(&t);
	Test_SHFileRegisterHashDedup(&t);
	Test_SHFileRegisterSubpassInput(&t);
	Test_SHFileRegisterStoredInBinary(&t);
	
	Test_SHFileCombineNullGuards(&t);
	Test_SHFileCombineFlagsMismatch(&t);
	Test_SHFileCombineCompilerVersionMismatch(&t);
	Test_SHFileCombineSourceHashMismatch(&t);
	Test_SHFileCombineEntryGroupMismatch(&t);
	Test_SHFileCombineMergesSPIRVAndDXIL(&t);
	Test_SHFileCombineEquivalentToSequential(&t);
	Test_SHFileCombineIncludesMatchSequential(&t);
	Test_SHFileCombineConflictingBinaryContents(&t);
	Test_SHFileCombineBinaryIdRemapping(&t);
	Test_SHFileCombineRegistersMerged(&t);

	Test_SHFileRoundTripBasic(&t);
	Test_SHFileRoundTripInclude(&t);
	Test_SHFileRoundTripBinaryExtensions(&t);
	Test_SHFileRoundTripComputeGroupSize(&t);
	Test_SHFileRoundTripRTStages(&t);
	Test_SHFileRoundTripMultipleBinaries(&t);
	Test_SHFileRoundTripRegisterSurvives(&t);
	Test_SHFileRoundTripDefines(&t);
	Test_SHFileRoundTripArrayRegisters(&t);
	Test_SHFileRoundTripSemanticNames(&t);
	Test_SHFileRoundTripShaderBufferInRegister(&t);
	Test_SHFileRegisterAddConstantBuffer(&t);
	Test_SHFileRegisterAddByteAddressBuffer(&t);
	Test_SHFileRegisterAddStructuredBuffer(&t);
	Test_SHFileRegisterAddAccelerationStructure(&t);
	Test_SHFileRegisterBufferWriteFlagRejections(&t);
	Test_SHFileRegisterConstantBufferSizeLimit(&t);

	BasicAllocator_checkLeakedMem(&t);
	return Test_end(&t);
}
