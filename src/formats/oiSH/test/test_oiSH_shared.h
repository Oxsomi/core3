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

//formats/oiSH/test/test_oiSH_shared.h

#pragma once
#include "formats/oiSH/sh_file.h"
#include "types/test/test.h"

void Test_SHFileCreateFree(Test *t);
void Test_SHFileCreateAlreadyAllocated(Test *t);
void Test_SHFileCreateInvalidFlags(Test *t);

void Test_SHEntryStageName(Test *t);
void Test_SHPipelineStagePrefixes(Test *t);

void Test_SHExtensionSetSanity(Test *t);
void Test_SHVendorNames(Test *t);
void Test_SHBinaryTypeNames(Test *t);
void Test_SHBindingsDummy(Test *t);

void Test_SHEntryRuntimeGetCombinations(Test *t);
void Test_SHBinaryIdentifierEquals(Test *t);

void Test_SHFileAddIncNullGuards(Test *t);
void Test_SHFileAddIncEmptyPathRejected(Test *t);
void Test_SHFileAddIncZeroCrcRejected(Test *t);
void Test_SHFileAddIncDuplicateSameCrc(Test *t);
void Test_SHFileAddIncDuplicateDifferentCrc(Test *t);
void Test_SHFileAddIncSortedOrder(Test *t);
void Test_SHFileAddIncMultipleDistinct(Test *t);

void Test_SHFileAddEntryBinaryIdOutOfBounds(Test *t);
void Test_SHFileAddEntrySemanticNamesLessThanUniqueInputs(Test *t);
void Test_SHFileAddEntryUniqueInputSemanticsLimit(Test *t);
void Test_SHFileAddEntryIOOnlyGraphics(Test *t);
void Test_SHFileAddEntryNonRTPayloadRejected(Test *t);
void Test_SHFileAddEntryRaygenNoPayload(Test *t);
void Test_SHFileAddEntryIntersectionOnWrongStage(Test *t);
void Test_SHFileAddEntryHitStageSizes(Test *t);
void Test_SHFileAddEntryCallablePayload(Test *t);
void Test_SHFileAddEntryMissMustHavePayload(Test *t);
void Test_SHFileAddEntryWaveSizeWorkgraph(Test *t);
void Test_SHFileAddEntryWaveSizeInvalidNibbles(Test *t);
void Test_SHFileAddEntryWaveSizeOnNonCompute(Test *t);
void Test_SHFileAddEntryTaskNeedsGroup(Test *t);
void Test_SHFileAddEntryMeshNeedsGroup(Test *t);
void Test_SHFileAddEntryGraphicsCannotHaveGroup(Test *t);
void Test_SHFileAddEntryComputeInvalidGroup(Test *t);
void Test_SHFileAddEntryInvalidStage(Test *t);
void Test_SHFileAddEntryEmptyName(Test *t);
void Test_SHFileAddEntryNullGuards(Test *t);
void Test_SHFileEntryAndBinaryLinked(Test *t);
void Test_SHFileCombineBinaryIdRemapping(Test *t);
void Test_SHFileCombineRegistersMerged(Test *t);

void Test_SHFileAddBinNullGuards(Test *t);
void Test_SHFileAddBinNoBinaryData(Test *t);
void Test_SHFileAddBinZeroVendorMask(Test *t);
void Test_SHFileAddBinZeroVendorMask(Test *t);
void Test_SHFileAddBinVendorMaskNormalized(Test *t);
void Test_SHFileAddBinVendorMaskOutOfBounds(Test *t);
void Test_SHFileAddBinInvalidExtensions(Test *t);
void Test_SHFileAddBinInvalidStageType(Test *t);
void Test_SHFileAddBinShaderVersionOutOfRange(Test *t);
void Test_SHFileAddBinShaderVersionAtBoundaries(Test *t);
void Test_SHFileAddBinSPIRVMisaligned(Test *t);
void Test_SHFileAddBinEntrypointAnnotationMutuallyExclusive(Test *t);
void Test_SHFileAddBinOddDefinesRejected(Test *t);
void Test_SHFileAddBinDuplicateDefineNameRejected(Test *t);
void Test_SHFileAddBinValidDefines(Test *t);
void Test_SHFileAddBinUniformEmptyName(Test *t);
void Test_SHFileAddBinUniformDigitFirstChar(Test *t);
void Test_SHFileAddBinUniformUnderscoreStart(Test *t);
void Test_SHFileAddBinUniformInvalidBodyChar(Test *t);
void Test_SHFileAddBinUniformDuplicateName(Test *t);
void Test_SHFileAddBinValid(Test *t);
void Test_SHFileAddBinDuplicateIdentifierRejected(Test *t);
void Test_SHFileAddBinMultipleDistinct(Test *t);
void Test_SHFileAddBinExtensionFlagsStored(Test *t);

void Test_SHFileRegisterAddSampler(Test *t);
void Test_SHFileRegisterAddSamplerComparisonState(Test *t);
void Test_SHFileRegisterDuplicateNameRejected(Test *t);
void Test_SHFileRegisterDuplicateSPIRVBindingRejected(Test *t);
void Test_SHFileRegisterAddTexture(Test *t);
void Test_SHFileRegisterAddRWTexture(Test *t);
void Test_SHFileRegisterArray(Test *t);
void Test_SHFileRegisterHashDedup(Test *t);
void Test_SHFileRegisterSubpassInput(Test *t);
void Test_SHFileRegisterStoredInBinary(Test *t);

void Test_SHFileCombineNullGuards(Test *t);
void Test_SHFileCombineFlagsMismatch(Test *t);
void Test_SHFileCombineCompilerVersionMismatch(Test *t);
void Test_SHFileCombineSourceHashMismatch(Test *t);
void Test_SHFileCombineEntryGroupMismatch(Test *t);
void Test_SHFileCombineMergesSPIRVAndDXIL(Test *t);
void Test_SHFileCombineEquivalentToSequential(Test *t);
void Test_SHFileCombineIncludesMatchSequential(Test *t);
void Test_SHFileCombineConflictingBinaryContents(Test *t);

void Test_SHFileRoundTripBasic(Test *t);
void Test_SHFileRoundTripInclude(Test *t);
void Test_SHFileRoundTripBinaryExtensions(Test *t);
void Test_SHFileRoundTripComputeGroupSize(Test *t);
void Test_SHFileRoundTripRTStages(Test *t);
void Test_SHFileRoundTripMultipleBinaries(Test *t);
void Test_SHFileRoundTripRegisterSurvives(Test *t);
void Test_SHFileRoundTripDefines(Test *t);
void Test_SHFileRoundTripArrayRegisters(Test *t);
void Test_SHFileRoundTripSemanticNames(Test *t);
void Test_SHFileRoundTripShaderBufferInRegister(Test *t);

void Test_SHFileRegisterAddConstantBuffer(Test *t);
void Test_SHFileRegisterAddByteAddressBuffer(Test *t);
void Test_SHFileRegisterAddStructuredBuffer(Test *t);
void Test_SHFileRegisterAddAccelerationStructure(Test *t);
void Test_SHFileRegisterBufferWriteFlagRejections(Test *t);
void Test_SHFileRegisterConstantBufferSizeLimit(Test *t);

static inline Bool Test_SHFileCreate(Test *t, SHFile *sh) {
	return SHFile_create(ESHSettingsFlags_None, OXC3_VERSION, 0xCAFE, t->alloc, sh, &t->err);
}

static const U8 kDummySPIRV[4] = { 0x03, 0x02, 0x23, 0x07 };

static inline SHBindings makeDualBinding(U32 set, U32 spirvBinding, U32 dxilBinding) {
	SHBindings b = SHBindings_dummy();
	b.arr[ESHBinaryType_SPIRV].space = set;
	b.arr[ESHBinaryType_SPIRV].binding = spirvBinding;
	b.arr[ESHBinaryType_DXIL].space = set;
	b.arr[ESHBinaryType_DXIL].binding = dxilBinding;
	return b;
}

//Build a minimal valid SHBinaryInfo.
//Pass entrypoint = NULL + hasShaderAnnotation = true for a lib binary.
static inline SHBinaryInfo makeBinaryInfo(
	ESHPipelineStage stage,
	const C8 *entrypoint,
	Bool hasShaderAnnotation
) {
	SHBinaryInfo info = (SHBinaryInfo) {
		.identifier = (SHBinaryIdentifier) {
			.entrypoint    = entrypoint
				? CharString_createRefCStrConst(entrypoint)
				: CharString_createNull(),
			.extensions    = ESHExtension_None,
			.shaderVersion = OISH_SHADER_MODEL_MIN,
			.stageType     = stage
		},
		.vendorMask          = (U16)((1u << ESHVendor_Count) - 1),
		.hasShaderAnnotation = hasShaderAnnotation
	};

	info.binaries[ESHBinaryType_SPIRV] = Buffer_createRefConst(kDummySPIRV, sizeof(kDummySPIRV));
	return info;
}

static inline Bool addComputeEntry(
	SHFile *sh,
	const C8 *name,
	U16 gx,
	U16 gy,
	U16 gz,
	Test *t,
	Bool ignoreErr,
	const U16 *id
) {
	SHEntry e = (SHEntry) { 0 };
	e.name = CharString_createRefCStrConst(name);
	e.stage = ESHPipelineStage_Compute;
	e.groupX = gx;
	e.groupY = gy;
	e.groupZ = gz;
	e.binaryIds = (ListU16) { .ptr = id, .length = id ? 1 : 0, .capacityAndRefInfo = U64_MAX };
	return SHFile_addEntrypoint(sh, &e, t->alloc, ignoreErr ? NULL : &t->err);
}
