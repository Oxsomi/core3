/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/ext/listx_impl.h"
#include "types/base/allocator.h"
#include "types/base/c8.h"
#include "types/container/buffer.h"
#include "platforms/log.h"
#include "shader_compiler/compiler.h"
#include "optimizer.hpp"
#include "SPIRV-Reflect/spirv_reflect.h"

Bool spvTypeToESBType(SpvReflectTypeDescription *desc, ESBType *type, Error *e_rr) {

	Bool s_uccess = true;
	SpvReflectNumericTraits numeric = desc->traits.numeric;

	ESBPrimitive prim = ESBPrimitive_Invalid;
	ESBStride stride = ESBStride_X8;
	ESBVector vector = ESBVector_N1;
	ESBMatrix matrix = ESBMatrix_N1;

	if(!desc || !type)
		retError(clean, Error_nullPointer(!desc ? 0 : 1, "spvTypeToESBType()::desc and type are required"))

	switch (desc->type_flags) {

		case SPV_REFLECT_TYPE_FLAG_BOOL:
		case SPV_REFLECT_TYPE_FLAG_BOOL | SPV_REFLECT_TYPE_FLAG_VECTOR:
		case SPV_REFLECT_TYPE_FLAG_BOOL | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX:
		case SPV_REFLECT_TYPE_FLAG_BOOL | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_BOOL | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_BOOL | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX | SPV_REFLECT_TYPE_FLAG_ARRAY:

			if(numeric.scalar.signedness || numeric.scalar.width != 32)
				retError(clean, Error_unsupportedOperation(
					0, "spvTypeToESBType()::desc has an unrecognized type (signed bool or size != 32)"
				))

			prim = ESBPrimitive_UInt;
			break;

		case SPV_REFLECT_TYPE_FLAG_INT:
		case SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_VECTOR:
		case SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX:
		case SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX | SPV_REFLECT_TYPE_FLAG_ARRAY:
			prim = numeric.scalar.signedness ? ESBPrimitive_Int : ESBPrimitive_UInt;
			break;

		case SPV_REFLECT_TYPE_FLAG_FLOAT:
		case SPV_REFLECT_TYPE_FLAG_FLOAT | SPV_REFLECT_TYPE_FLAG_VECTOR:
		case SPV_REFLECT_TYPE_FLAG_FLOAT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX:
		case SPV_REFLECT_TYPE_FLAG_FLOAT | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_FLOAT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_ARRAY:
		case SPV_REFLECT_TYPE_FLAG_FLOAT | SPV_REFLECT_TYPE_FLAG_VECTOR | SPV_REFLECT_TYPE_FLAG_MATRIX | SPV_REFLECT_TYPE_FLAG_ARRAY:

			prim = ESBPrimitive_Float;

			if(numeric.scalar.signedness || (
				numeric.scalar.width != 16 &&
				numeric.scalar.width != 32 &&
				numeric.scalar.width != 64
			))
				retError(clean, Error_unsupportedOperation(
					0, "spvTypeToESBType()::desc has an unrecognized type (signed floatXX or size != [16, 32, 64])"
				))

			break;

		default:
			retError(clean, Error_unsupportedOperation(0, "spvTypeToESBType()::desc has an unrecognized type"))
	}

	switch(numeric.scalar.width) {

		case  8:	stride = ESBStride_X8;		break;
		case 16:	stride = ESBStride_X16;		break;
		case 32:	stride = ESBStride_X32;		break;
		case 64:	stride = ESBStride_X64;		break;

		default:
			retError(clean, Error_unsupportedOperation(
				0, "spvTypeToESBType()::desc has an unrecognized type (8, 16, 32, 64)"
			))
	}

	switch(numeric.matrix.column_count ? numeric.matrix.column_count : numeric.vector.component_count) {

		case 0:
		case 1:		vector = ESBVector_N1;		break;

		case 2:		vector = ESBVector_N2;		break;
		case 3:		vector = ESBVector_N3;		break;
		case 4:		vector = ESBVector_N4;		break;

		default:
			retError(clean, Error_unsupportedOperation(
				0, "spvTypeToESBType()::desc has an unrecognized type (vecN)"
			))
	}

	switch(numeric.matrix.row_count) {

		case 0:
		case 1:		matrix = ESBMatrix_N1;		break;

		case 2:		matrix = ESBMatrix_N2;		break;
		case 3:		matrix = ESBMatrix_N3;		break;
		case 4:		matrix = ESBMatrix_N4;		break;

		default:
			retError(clean, Error_unsupportedOperation(
				0, "spvTypeToESBType()::desc has an unrecognized type (matWxH)"
			))
	}

	if(numeric.matrix.stride && numeric.matrix.stride != 0x10)
		retError(clean, Error_unsupportedOperation(0, "spvTypeToESBType()::desc has matrix with stride != 16"))

	*type = (ESBType) ESBType_create(stride, prim, vector, matrix);

clean:
	return s_uccess;
}

Bool spvMapCapabilityToESHExtension(SpvCapability capability, ESHExtension *extension, Error *e_rr) {

	Bool s_uccess = true;
	ESHExtension ext = (ESHExtension)(1 << ESHExtension_Count);

	switch (capability) {

		//Shader types

		case SpvCapabilityTessellation:
		case SpvCapabilityGeometry:

		case SpvCapabilityRayTracingKHR:
		case SpvCapabilityMeshShadingEXT:
			break;

		//RT:

		case SpvCapabilityRayTracingOpacityMicromapEXT:
			ext = ESHExtension_RayMicromapOpacity;
			break;

		case SpvCapabilityRayQueryKHR:
			ext = ESHExtension_RayQuery;
			break;

		case SpvCapabilityRayTracingMotionBlurNV:
			ext = ESHExtension_RayMotionBlur;
			break;

		case SpvCapabilityShaderInvocationReorderNV:
			ext = ESHExtension_RayReorder;
			break;

		case SpvCapabilityAtomicFloat32AddEXT:
		case SpvCapabilityAtomicFloat32MinMaxEXT:
			ext = ESHExtension_AtomicF32;
			break;

		case SpvCapabilityAtomicFloat64AddEXT:
		case SpvCapabilityAtomicFloat64MinMaxEXT:
			ext = ESHExtension_AtomicF64;
			break;

		case SpvCapabilityGroupNonUniformArithmetic:
			ext = ESHExtension_SubgroupArithmetic;
			break;

		case SpvCapabilityGroupNonUniformShuffle:
			ext = ESHExtension_SubgroupShuffle;
			break;

		case SpvCapabilityGroupNonUniform:
		case SpvCapabilityGroupNonUniformVote:
		case SpvCapabilityGroupNonUniformBallot:
		case SpvCapabilitySubgroupVoteKHR:
		case SpvCapabilitySubgroupBallotKHR:
			ext = ESHExtension_SubgroupOperations;
			break;

		case SpvCapabilityMultiView:
			ext = ESHExtension_Multiview;
			break;

		//Types

		case SpvCapabilityStorageBuffer16BitAccess:
		case SpvCapabilityStorageUniform16:
		case SpvCapabilityStoragePushConstant16:
		case SpvCapabilityStorageInputOutput16:

		case SpvCapabilityInt16:
		case SpvCapabilityFloat16:
			ext = ESHExtension_16BitTypes;
			break;

		case SpvCapabilityFloat64:
			ext = ESHExtension_F64;
			break;

		case SpvCapabilityInt64:
			ext = ESHExtension_I64;
			break;

		case SpvCapabilityInt64Atomics:
			ext = (ESHExtension)(ESHExtension_I64 | ESHExtension_AtomicI64);
			break;

		case SpvCapabilityComputeDerivativeGroupLinearNV:
			ext = ESHExtension_ComputeDeriv;
			break;

		case SpvCapabilityImageMSArray:
		case SpvCapabilityStorageImageMultisample:
			ext = ESHExtension_WriteMSTexture;
			break;

		//No-op, not important, always supported

		case SpvCapabilityShader:
		case SpvCapabilityMatrix:
		case SpvCapabilityAtomicStorage:

		case SpvCapabilityRuntimeDescriptorArray:
		case SpvCapabilityShaderNonUniform:
		case SpvCapabilityUniformTexelBufferArrayDynamicIndexing:
		case SpvCapabilityStorageTexelBufferArrayDynamicIndexing:
		case SpvCapabilityUniformBufferArrayNonUniformIndexing:
		case SpvCapabilitySampledImageArrayNonUniformIndexing:
		case SpvCapabilityStorageBufferArrayNonUniformIndexing:
		case SpvCapabilityStorageImageArrayNonUniformIndexing:
		case SpvCapabilityUniformTexelBufferArrayNonUniformIndexing:
		case SpvCapabilityStorageTexelBufferArrayNonUniformIndexing:

		case SpvCapabilityStorageImageExtendedFormats:
		case SpvCapabilityImageQuery:
		case SpvCapabilityDerivativeControl:

		case SpvCapabilityInputAttachment:
		case SpvCapabilityMinLod:
		case SpvCapabilityUniformBufferArrayDynamicIndexing:
		case SpvCapabilitySampledImageArrayDynamicIndexing:
		case SpvCapabilityStorageBufferArrayDynamicIndexing:
		case SpvCapabilityStorageImageArrayDynamicIndexing:

		case SpvCapabilitySampledCubeArray:
		case SpvCapabilitySampled1D:
		case SpvCapabilityImage1D:
		case SpvCapabilityImageCubeArray:
			break;

		//Unsupported

		//Provisional

		case SpvCapabilityRayQueryProvisionalKHR:
		case SpvCapabilityRayTracingProvisionalKHR:

		//AMD

		case SpvCapabilityGroups:

		case SpvCapabilityFloat16ImageAMD:
		case SpvCapabilityImageGatherBiasLodAMD:
		case SpvCapabilityFragmentMaskAMD:
		case SpvCapabilityImageReadWriteLodAMD:

		//QCOM

		case SpvCapabilityTextureSampleWeightedQCOM:
		case SpvCapabilityTextureBoxFilterQCOM:
		case SpvCapabilityTextureBlockMatchQCOM:

		//NV

		case SpvCapabilitySampleMaskOverrideCoverageNV:
		case SpvCapabilityGeometryShaderPassthroughNV:
		case SpvCapabilityShaderViewportMaskNV:
		case SpvCapabilityShaderStereoViewNV:
		case SpvCapabilityPerViewAttributesNV:
		case SpvCapabilityMeshShadingNV:
		case SpvCapabilityImageFootprintNV:
		case SpvCapabilityComputeDerivativeGroupQuadsNV:
		case SpvCapabilityGroupNonUniformPartitionedNV:
		case SpvCapabilityRayTracingNV:
		case SpvCapabilityCooperativeMatrixNV:
		case SpvCapabilityShaderSMBuiltinsNV:
		case SpvCapabilityBindlessTextureNV:

		//Intel

		case SpvCapabilitySubgroupShuffleINTEL:
		case SpvCapabilitySubgroupBufferBlockIOINTEL:
		case SpvCapabilitySubgroupImageBlockIOINTEL:
		case SpvCapabilitySubgroupImageMediaBlockIOINTEL:
		case SpvCapabilityRoundToInfinityINTEL:
		case SpvCapabilityFloatingPointModeINTEL:
		case SpvCapabilityIntegerFunctions2INTEL:
		case SpvCapabilityFunctionPointersINTEL:
		case SpvCapabilityIndirectReferencesINTEL:
		case SpvCapabilityAsmINTEL:

		case SpvCapabilityVectorComputeINTEL:
		case SpvCapabilityVectorAnyINTEL:

		case SpvCapabilitySubgroupAvcMotionEstimationINTEL:
		case SpvCapabilitySubgroupAvcMotionEstimationIntraINTEL:
		case SpvCapabilitySubgroupAvcMotionEstimationChromaINTEL:
		case SpvCapabilityVariableLengthArrayINTEL:
		case SpvCapabilityFunctionFloatControlINTEL:
		case SpvCapabilityFPGAMemoryAttributesINTEL:
		case SpvCapabilityFPFastMathModeINTEL:
		case SpvCapabilityArbitraryPrecisionIntegersINTEL:
		case SpvCapabilityArbitraryPrecisionFloatingPointINTEL:
		case SpvCapabilityUnstructuredLoopControlsINTEL:
		case SpvCapabilityFPGALoopControlsINTEL:
		case SpvCapabilityKernelAttributesINTEL:
		case SpvCapabilityFPGAKernelAttributesINTEL:
		case SpvCapabilityFPGAMemoryAccessesINTEL:
		case SpvCapabilityFPGAClusterAttributesINTEL:
		case SpvCapabilityLoopFuseINTEL:
		case SpvCapabilityFPGADSPControlINTEL:
		case SpvCapabilityMemoryAccessAliasingINTEL:
		case SpvCapabilityFPGAInvocationPipeliningAttributesINTEL:
		case SpvCapabilityFPGABufferLocationINTEL:
		case SpvCapabilityArbitraryPrecisionFixedPointINTEL:
		case SpvCapabilityUSMStorageClassesINTEL:
		case SpvCapabilityRuntimeAlignedAttributeINTEL:
		case SpvCapabilityIOPipesINTEL:
		case SpvCapabilityBlockingPipesINTEL:
		case SpvCapabilityFPGARegINTEL:

		case SpvCapabilityLongConstantCompositeINTEL:
		case SpvCapabilityOptNoneINTEL:

		case SpvCapabilityDebugInfoModuleINTEL:
		case SpvCapabilityBFloat16ConversionINTEL:
		case SpvCapabilitySplitBarrierINTEL:
		case SpvCapabilityFPGAKernelAttributesv2INTEL:
		case SpvCapabilityFPGALatencyControlINTEL:
		case SpvCapabilityFPGAArgumentInterfacesINTEL:

		//Possible in the future? TODO:

		case SpvCapabilityShaderViewportIndexLayerEXT:
		case SpvCapabilityFragmentBarycentricKHR:
		case SpvCapabilityDemoteToHelperInvocation:
		case SpvCapabilityExpectAssumeKHR:
		case SpvCapabilityCooperativeMatrixKHR:
		case SpvCapabilityBitInstructions:

		case SpvCapabilityMultiViewport:
		case SpvCapabilityShaderLayer:
		case SpvCapabilityShaderViewportIndex:

		case SpvCapabilityFragmentShaderSampleInterlockEXT:
		case SpvCapabilityFragmentShaderShadingRateInterlockEXT:
		case SpvCapabilityFragmentShaderPixelInterlockEXT:

		case SpvCapabilityRayTraversalPrimitiveCullingKHR:
		case SpvCapabilityRayTracingPositionFetchKHR:
		case SpvCapabilityRayQueryPositionFetchKHR:
		case SpvCapabilityRayCullMaskKHR:

		case SpvCapabilityAtomicFloat16AddEXT:
		case SpvCapabilityAtomicFloat16MinMaxEXT:

		case SpvCapabilityInputAttachmentArrayDynamicIndexing:
		case SpvCapabilityInputAttachmentArrayNonUniformIndexing:

		case SpvCapabilityGroupNonUniformShuffleRelative:
		case SpvCapabilityGroupNonUniformClustered:
		case SpvCapabilityGroupNonUniformQuad:
		case SpvCapabilityGroupNonUniformRotateKHR:
		case SpvCapabilityGroupUniformArithmeticKHR:

		case SpvCapabilityDotProductInputAll:
		case SpvCapabilityDotProductInput4x8Bit:
		case SpvCapabilityDotProductInput4x8BitPacked:
		case SpvCapabilityDotProduct:

		case SpvCapabilityVulkanMemoryModel:
		case SpvCapabilityVulkanMemoryModelDeviceScope:

		case SpvCapabilityShaderClockKHR:
		case SpvCapabilityFragmentFullyCoveredEXT:
		case SpvCapabilityFragmentDensityEXT:
		case SpvCapabilityPhysicalStorageBufferAddresses:

		case SpvCapabilityDenormPreserve:
		case SpvCapabilityDenormFlushToZero:
		case SpvCapabilitySignedZeroInfNanPreserve:
		case SpvCapabilityRoundingModeRTE:
		case SpvCapabilityRoundingModeRTZ:
		case SpvCapabilityStencilExportEXT:

		case SpvCapabilityInt64ImageEXT:

		case SpvCapabilityStorageBuffer8BitAccess:
		case SpvCapabilityUniformAndStorageBuffer8BitAccess:
		case SpvCapabilityStoragePushConstant8:

		case SpvCapabilityDeviceGroup:
		case SpvCapabilityVariablePointersStorageBuffer:
		case SpvCapabilityVariablePointers:
		case SpvCapabilitySampleMaskPostDepthCoverage:

		case SpvCapabilityWorkgroupMemoryExplicitLayoutKHR:
		case SpvCapabilityWorkgroupMemoryExplicitLayout8BitAccessKHR:
		case SpvCapabilityWorkgroupMemoryExplicitLayout16BitAccessKHR:

		case SpvCapabilityDrawParameters:
		case SpvCapabilityUniformDecoration:

		case SpvCapabilityCoreBuiltinsARM:

		case SpvCapabilityInterpolationFunction:
		case SpvCapabilityTransformFeedback:
		case SpvCapabilitySampledBuffer:
		case SpvCapabilityImageBuffer:

		case SpvCapabilityTileImageColorReadAccessEXT:
		case SpvCapabilityTileImageDepthReadAccessEXT:
		case SpvCapabilityTileImageStencilReadAccessEXT:

		case SpvCapabilityFragmentShadingRateKHR:

		case SpvCapabilityGeometryStreams:
		case SpvCapabilityStorageImageReadWithoutFormat:
		case SpvCapabilityStorageImageWriteWithoutFormat:

		case SpvCapabilityImageRect:
		case SpvCapabilitySampledRect:
		case SpvCapabilityGenericPointer:
		case SpvCapabilityInt8:
		case SpvCapabilitySparseResidency:
		case SpvCapabilitySampleRateShading:

		case SpvCapabilityImageGatherExtended:
		case SpvCapabilityClipDistance:
		case SpvCapabilityCullDistance:

		case SpvCapabilityAtomicStorageOps:

		case SpvCapabilityTessellationPointSize:
		case SpvCapabilityGeometryPointSize:

		//Unsupported, we don't support kernels, only shaders

		case SpvCapabilityAddresses:
		case SpvCapabilityLinkage:

		case SpvCapabilityKernel:
		case SpvCapabilityFloat16Buffer:
		case SpvCapabilityVector16:
		case SpvCapabilityImageBasic:
		case SpvCapabilityImageReadWrite:
		case SpvCapabilityImageMipmap:
		case SpvCapabilityPipes:
		case SpvCapabilityDeviceEnqueue:
		case SpvCapabilityLiteralSampler:
		case SpvCapabilitySubgroupDispatch:
		case SpvCapabilityNamedBarrier:
		case SpvCapabilityPipeStorage:

			retError(clean, Error_invalidState(
				2, "spvMapCapabilityToESHExtension() SPIRV contained capability that isn't supported in oiSH"
			))

		case SpvCapabilityMax:
			retError(clean, Error_invalidState(
				2, "spvMapCapabilityToESHExtension() SPIRV contained invalid capability that isn't supported in SPIRV-Headers"
			))
	}

	//Handled separately to ensure there's no default case in the switch,
	//so that new capabilities are reported when SPIRV-Header update on some compilers.

	if(capability > SpvCapabilityMax)
		retError(clean, Error_invalidState(
			2, "spvMapCapabilityToESHExtension() SPIRV contained invalid capability that isn't supported in SPIRV-Headers"
		))

	*extension = ext;

clean:
	return s_uccess;
}

Bool SpvReflectFormatToESBType(SpvReflectFormat format, ESBType *type, Error *e_rr) {

	Bool s_uccess = true;

	switch (format) {

		case SPV_REFLECT_FORMAT_R16_UINT:				*type = ESBType_U16;		break;
		case SPV_REFLECT_FORMAT_R16_SINT:				*type = ESBType_I16;		break;
		case SPV_REFLECT_FORMAT_R16_SFLOAT:				*type = ESBType_F16;		break;

		case SPV_REFLECT_FORMAT_R16G16_UINT:			*type = ESBType_U16x2;		break;
		case SPV_REFLECT_FORMAT_R16G16_SINT:			*type = ESBType_I16x2;		break;
		case SPV_REFLECT_FORMAT_R16G16_SFLOAT:			*type = ESBType_F16x2;		break;

		case SPV_REFLECT_FORMAT_R16G16B16_UINT:			*type = ESBType_U16x3;		break;
		case SPV_REFLECT_FORMAT_R16G16B16_SINT:			*type = ESBType_I16x3;		break;
		case SPV_REFLECT_FORMAT_R16G16B16_SFLOAT:		*type = ESBType_F16x3;		break;

		case SPV_REFLECT_FORMAT_R16G16B16A16_UINT:		*type = ESBType_U16x4;		break;
		case SPV_REFLECT_FORMAT_R16G16B16A16_SINT:		*type = ESBType_I16x4;		break;
		case SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT:	*type = ESBType_F16x4;		break;

		case SPV_REFLECT_FORMAT_R32_UINT:				*type = ESBType_U32;		break;
		case SPV_REFLECT_FORMAT_R32_SINT:				*type = ESBType_I32;		break;
		case SPV_REFLECT_FORMAT_R32_SFLOAT:				*type = ESBType_F32;		break;

		case SPV_REFLECT_FORMAT_R32G32_UINT:			*type = ESBType_U32x2;		break;
		case SPV_REFLECT_FORMAT_R32G32_SINT:			*type = ESBType_I32x2;		break;
		case SPV_REFLECT_FORMAT_R32G32_SFLOAT:			*type = ESBType_F32x2;		break;

		case SPV_REFLECT_FORMAT_R32G32B32_UINT:			*type = ESBType_U32x3;		break;
		case SPV_REFLECT_FORMAT_R32G32B32_SINT:			*type = ESBType_I32x3;		break;
		case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:		*type = ESBType_F32x3;		break;

		case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:		*type = ESBType_U32x4;		break;
		case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:		*type = ESBType_I32x4;		break;
		case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:	*type = ESBType_F32x4;		break;

		case SPV_REFLECT_FORMAT_R64_UINT:				*type = ESBType_U64;		break;
		case SPV_REFLECT_FORMAT_R64_SINT:				*type = ESBType_I64;		break;
		case SPV_REFLECT_FORMAT_R64_SFLOAT:				*type = ESBType_F64;		break;

		case SPV_REFLECT_FORMAT_R64G64_UINT:			*type = ESBType_U64x2;		break;
		case SPV_REFLECT_FORMAT_R64G64_SINT:			*type = ESBType_I64x2;		break;
		case SPV_REFLECT_FORMAT_R64G64_SFLOAT:			*type = ESBType_F64x2;		break;

		case SPV_REFLECT_FORMAT_R64G64B64_UINT:			*type = ESBType_U64x3;		break;
		case SPV_REFLECT_FORMAT_R64G64B64_SINT:			*type = ESBType_I64x3;		break;
		case SPV_REFLECT_FORMAT_R64G64B64_SFLOAT:		*type = ESBType_F64x3;		break;

		case SPV_REFLECT_FORMAT_R64G64B64A64_UINT:		*type = ESBType_U64x4;		break;
		case SPV_REFLECT_FORMAT_R64G64B64A64_SINT:		*type = ESBType_I64x4;		break;
		case SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT:	*type = ESBType_F64x4;		break;

		default:
			retError(clean, Error_invalidState(
				0, "SpvReflectFormatToESBType() couldn't map SPV_REFLECT_FORMAT to ESBType"
			))
	}

clean:
	return s_uccess;
}

Bool SpvCalculateStructLength(const SpvReflectTypeDescription *typeDesc, U64 *result, Error *e_rr) {

	U64 len = 0;
	Bool s_uccess = true;

	for (U64 i = 0; i < typeDesc->member_count; ++i) {

		SpvReflectTypeDescription typeDesck = typeDesc->members[i];

		SpvReflectTypeFlags disallowed =
			SPV_REFLECT_TYPE_FLAG_EXTERNAL_MASK | SPV_REFLECT_TYPE_FLAG_REF | SPV_REFLECT_TYPE_FLAG_VOID;

		if(typeDesck.type_flags & disallowed)
			retError(clean, Error_invalidState(
				0, "SpvCalculateStructLength() can't be called on a struct that contains external data or a ref or void"
			))

		U64 currLen = 0;

		//Array specifies stride, so that's easy

		if (typeDesck.type_flags & SPV_REFLECT_TYPE_FLAG_ARRAY) {

			U64 arrayLen = typeDesck.traits.array.stride;

			for (U64 j = 0; j < typeDesck.traits.array.dims_count; ++j) {

				U64 prevArrayLen = arrayLen;
				arrayLen *= typeDesck.traits.array.dims[j];

				if(arrayLen < prevArrayLen)
					retError(clean, Error_overflow(
						0, arrayLen, prevArrayLen, "SpvCalculateStructLength() arrayLen overflow"
					))
			}

			currLen = arrayLen;
		}

		//Struct causes recursion

		else if(typeDesck.type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT)
			gotoIfError3(clean, SpvCalculateStructLength(typeDesck.struct_type_description, &currLen, e_rr))

		//Otherwise, we can easily calculate it via SpvReflectNumericTraits

		else {

			const SpvReflectNumericTraits *numeric = &typeDesck.traits.numeric;

			currLen = numeric->scalar.width >> 3;

			if(typeDesck.type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX)
				currLen =
					!(typeDesck.decoration_flags & SPV_REFLECT_DECORATION_ROW_MAJOR) ?
					numeric->matrix.stride * numeric->matrix.column_count :
					numeric->matrix.stride * numeric->matrix.row_count;

			else if(typeDesck.type_flags & SPV_REFLECT_TYPE_FLAG_VECTOR)
				currLen *= numeric->vector.component_count;
		}

		U64 prevLen = len;
		len += currLen;

		if(len < prevLen)
			retError(clean, Error_overflow(
				0, len, prevLen, "SpvCalculateStructLength() len overflow"
			))
	}

clean:

	if(len)
		*result = len;

	return s_uccess;
}

Bool Compiler_convertMemberSPIRV(
	SBFile *sbFile,
	const SpvReflectBlockVariable *var,
	U16 parent,
	U32 offset,
	Bool isPacked,
	Allocator alloc,
	Error *e_rr
) {
	Bool s_uccess = true;

	ESBType shType = (ESBType) 0;
	U64 perElementStride = 0;
	U16 structId = U16_MAX;
	U64 expectedSize = perElementStride;
	CharString str = CharString_createRefCStrConst(var->name);
	ListU32 arrays = ListU32{};

	if(var->array.dims_count > SPV_REFLECT_MAX_ARRAY_DIMS)
		retError(clean, Error_invalidState(
			0, "Compiler_convertMemberSPIRV() array dimensions out of bounds"
		))

	if(var->array.dims_count && !var->array.stride)
		retError(clean, Error_invalidState(0, "Compiler_convertMemberSPIRV() array stride unset"))

	for(U64 m = 0; m < var->array.dims_count; ++m)
		if(!var->array.dims[m] || var->array.spec_constant_op_ids[m] != U32_MAX)
			retError(clean, Error_invalidState(
				0, "Compiler_convertMemberSPIRV() invalid array data (0 or has spec constant op)"
			))

	if(var->flags && var->flags != SPV_REFLECT_VARIABLE_FLAGS_UNUSED)
		retError(clean, Error_invalidState(
			0, "Compiler_convertMemberSPIRV() unsupported value in cbuffer member"
		))

	if(!(var->type_description->type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT)) {
		gotoIfError3(clean, spvTypeToESBType(var->type_description, &shType, e_rr))
		perElementStride = var->array.dims_count ? var->array.stride : ESBType_getSize(shType, isPacked);
	}

	else {

		perElementStride = var->array.dims_count ? var->array.stride : var->size;

		U32 stride = var->array.dims_count ? var->array.stride : var->size;

		CharString structName = CharString_createRefCStrConst(var->type_description->type_name);

		U64 j = 0;

		for (; j < sbFile->structs.length; ++j) {

			SBStruct strct = sbFile->structs.ptr[j];
			CharString structNamej = sbFile->structNames.ptr[j];

			if(
				CharString_equalsStringSensitive(structName, structNamej) && strct.stride == stride
			)
				break;
		}

		//Insert type

		if (j == sbFile->structs.length)
			gotoIfError3(clean, SBFile_addStruct(
				sbFile, &structName, SBStruct{ .stride = stride }, alloc, e_rr
			))

		structId = (U16) j;
	}

	expectedSize = perElementStride;

	for(U64 m = 0; m < var->array.dims_count; ++m)
		expectedSize *= var->array.dims[m];

	if(var->size > expectedSize)
		retError(clean, Error_invalidState(0, "Compiler_convertMemberSPIRV() var had mismatching size"))

	if(var->array.dims_count)
		gotoIfError2(clean, ListU32_createRefConst(var->array.dims, var->array.dims_count, &arrays))

	if(shType != (ESBType) 0)
		gotoIfError3(clean, SBFile_addVariableAsType(
			sbFile,
			&str,
			offset + var->offset, parent, shType,
			var->flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED ? ESBVarFlag_None : ESBVarFlag_IsUsedVarSPIRV,
			arrays.length ? &arrays : NULL,
			alloc, e_rr
		))

	else {

		U16 newParent = (U16) sbFile->vars.length;

		gotoIfError3(clean, SBFile_addVariableAsStruct(
			sbFile,
			&str,
			offset + var->offset, parent, structId,
			var->flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED ? ESBVarFlag_None : ESBVarFlag_IsUsedVarSPIRV,
			arrays.length ? &arrays : NULL,
			alloc, e_rr
		))

		if(!var->member_count || !var->members)
			retError(clean, Error_invalidState(0, "Compiler_convertMemberSPIRV() missing member_count or members"))

		for (U64 j = 0; j < var->member_count; ++j)
			gotoIfError3(clean, Compiler_convertMemberSPIRV(
				sbFile, &var->members[j], newParent, offset + var->offset, isPacked, alloc, e_rr
			))
	}

clean:
	return s_uccess;
}

Bool Compiler_convertShaderBufferSPIRV(
	SpvReflectDescriptorBinding *binding,
	Bool isPacked,
	Allocator alloc,
	SBFile *sbFile,
	Error *e_rr
) {
	Bool s_uccess = true;

	//StructuredBuffer; the inner element represents the whole buffer

	ESBSettingsFlags packedFlags = isPacked ? ESBSettingsFlags_IsTightlyPacked : (ESBSettingsFlags) 0;

	if (!binding->block.padded_size) {

		if(binding->block.member_count != 1 || !binding->block.members)
			retError(clean, Error_invalidState(
				0, "Compiler_convertShaderBufferSPIRV()::binding is missing member count or members"
			))

		SpvReflectBlockVariable *innerStruct = binding->block.members;

		if(!innerStruct->member_count || !innerStruct->members || !innerStruct->padded_size) {

			ESBType type{};
			gotoIfError3(clean, spvTypeToESBType(innerStruct->type_description, &type, e_rr))

			if(type && (innerStruct->members || innerStruct->member_count || innerStruct->padded_size))
				retError(clean, Error_invalidState(
					0, "Compiler_convertShaderBufferSPIRV() inner struct is assumed to be a type, but has invalid members"
				))

			U32 paddedSize = ESBType_getSize(type, isPacked);

			if(!isPacked)
				paddedSize = (paddedSize + 15) &~ 15;

			gotoIfError3(clean, SBFile_create(packedFlags, paddedSize, alloc, sbFile, e_rr))
			CharString elementName = CharString_createRefCStrConst("$Element");
			ListU32 arrays{};

			if(innerStruct->array.dims_count)
				gotoIfError2(clean, ListU32_createRefConst(innerStruct->array.dims, innerStruct->array.dims_count, &arrays))

			gotoIfError3(clean, SBFile_addVariableAsType(
				sbFile,
				&elementName,
				0, U16_MAX, type,
				binding->block.flags != SPV_REFLECT_VARIABLE_FLAGS_UNUSED ? ESBVarFlag_None : ESBVarFlag_IsUsedVarSPIRV,
				arrays.length ? &arrays : NULL,
				alloc, e_rr
			))

			goto clean;
		}

		const C8 *structNameC = innerStruct->type_description->type_name;
		CharString structName = CharString_createRefCStrConst(structNameC);

		gotoIfError3(clean, SBFile_create(packedFlags, innerStruct->padded_size, alloc, sbFile, e_rr))
		gotoIfError3(clean, SBFile_addStruct(sbFile, &structName, SBStruct{ .stride = innerStruct->padded_size }, alloc, e_rr))

		CharString element = CharString_createRefCStrConst("$Element");

		gotoIfError3(clean, SBFile_addVariableAsStruct(
			sbFile,
			&element,
			0, U16_MAX, 0,
			innerStruct->flags & SPV_REFLECT_VARIABLE_FLAGS_UNUSED ? ESBVarFlag_None : ESBVarFlag_IsUsedVarSPIRV,
			NULL,
			alloc,
			e_rr
		))

		for (U64 l = 0; l < innerStruct->member_count; ++l) {
			SpvReflectBlockVariable var = innerStruct->members[l];
			gotoIfError3(clean, Compiler_convertMemberSPIRV(sbFile, &var, 0, 0, isPacked, alloc, e_rr))
		}

		goto clean;
	}

	//CBuffer or storage buffer (without dynamic entries)

	gotoIfError3(clean, SBFile_create(ESBSettingsFlags_None, binding->block.padded_size, alloc, sbFile, e_rr))

	for (U64 l = 0; l < binding->block.member_count; ++l) {
		SpvReflectBlockVariable var = binding->block.members[l];
		gotoIfError3(clean, Compiler_convertMemberSPIRV(sbFile, &var, U16_MAX, 0, isPacked, alloc, e_rr))
	}

clean:
	if(!s_uccess && sbFile)
		SBFile_free(sbFile, alloc);

	return s_uccess;
}

Bool Compiler_convertRegisterSPIRV(
	ListSHRegisterRuntime *registers,
	SpvReflectDescriptorBinding *binding,
	U32 expectedSet,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	Bool isUnused = !binding->accessed;
	CharString name = CharString_createRefCStrConst(binding->name);

	ListU32 arrays{};
	SBFile sbFile{};

	U64 flatLen = 1;

	ESHBufferType bufferType = ESHBufferType_Count;
	Bool shouldBeBufferWrite = false;

	//Copy image desc to U64[3] for easier compares (copy is for alignment)

	static_assert(
		sizeof(binding->image) == sizeof(U64) * 3,
		"Compiler_convertRegisterSPIRV() does compares in U64[3] of binding->image, but size changed"
	);

	U64 imagePtrU64[3];
	Buffer_memcpy(
		Buffer_createRef(imagePtrU64, sizeof(imagePtrU64)),
		Buffer_createRefConst(&binding->image, sizeof(binding->image))
	);

	//Copy numeric desc to U64[3] for easier compares (copy is for alignment)

	static_assert(
		sizeof(binding->block.numeric) == sizeof(U64) * 3,
		"Compiler_convertShaderBufferSPIRV() does compares in U64[3] of binding->block.numeric, but size changed"
		);

	U64 numericTraitsU64[3];		//Fixes alignment
	Buffer_memcpy(
		Buffer_createRef(numericTraitsU64, sizeof(numericTraitsU64)),
		Buffer_createRefConst(&binding->block.numeric, sizeof(binding->block.numeric))
	);

	//Variable info

	const void *blockPtr = &binding->block.name;
	const U64 *blockPtrU64 = (const U64*) blockPtr;

	constexpr U8 blockSize = 40;

	static_assert(
		!(offsetof(SpvReflectBlockVariable, name) & 7) &&
		offsetof(SpvReflectBlockVariable, member_count) - offsetof(SpvReflectBlockVariable, name) == sizeof(U64) * blockSize,
		"Compiler_convertRegisterSPIRV() expected SpvReflectBlockVariable to be made of 43 U64s + 2x U32 and be aligned"
	);

	SHBindings bindings;

	for(U8 i = 0; i < ESHBinaryType_Count; ++i)
		bindings.arr[i] = SHBinding{ .space = U32_MAX, .binding = U32_MAX };

	bindings.arr[ESHBinaryType_SPIRV] = SHBinding{ .space = binding->set, .binding = binding->binding };

	if(expectedSet != binding->set)
		retError(clean, Error_invalidState(
			1, "Compiler_convertRegisterSPIRV() binding->set != parent->set"
		))

	if(binding->binding == U32_MAX && binding->set == U32_MAX)
		retError(clean, Error_invalidState(
			1, "Compiler_convertRegisterSPIRV() binding = U32_MAX, set = U32_MAX is reserved"
		))

	if(binding->descriptor_type != SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT && binding->input_attachment_index)
		retError(clean, Error_invalidState(
			1, "Compiler_convertRegisterSPIRV() input attachment index is invalid on non input attachment"
		))

	if(binding->byte_address_buffer_offset_count || binding->byte_address_buffer_offsets || binding->user_type)
		retError(clean, Error_invalidState(
			1, "Compiler_convertRegisterSPIRV() unsupported BAB offsets/count and user_type"
		))

	if(binding->array.dims_count) {

		gotoIfError2(clean, ListU32_createRefConst(binding->array.dims, binding->array.dims_count, &arrays))

		for(U64 i = 0; i < binding->array.dims_count; ++i) {

			flatLen *= binding->array.dims[i];

			if(!flatLen || flatLen >> 32)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() invalid flat length (out of bounds or 0)"
				))
		}

		if(flatLen != binding->count)
			retError(clean, Error_invalidState(
				1, "Compiler_convertRegisterSPIRV() register flat length mismatches binding count"
			))
	}

	switch (binding->descriptor_type) {

		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {

			SpvReflectDecorationFlags sbufferFlags =
				SPV_REFLECT_DECORATION_ROW_MAJOR | SPV_REFLECT_DECORATION_COLUMN_MAJOR | SPV_REFLECT_DECORATION_NON_WRITABLE;

			if(
				!binding->block.member_count ||
				(binding->block.decoration_flags && binding->block.decoration_flags != SPV_REFLECT_DECORATION_NON_WRITABLE) ||
				(binding->block.flags && binding->block.flags != SPV_REFLECT_VARIABLE_FLAGS_UNUSED) ||
				numericTraitsU64[0] ||
				numericTraitsU64[1] ||
				numericTraitsU64[2] ||
				imagePtrU64[0] ||
				imagePtrU64[1] ||
				imagePtrU64[2] ||
				!binding->block.members ||
				(binding->decoration_flags &~ sbufferFlags)
			)
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterSPIRV() invalid constant buffer data"
				))

			CharString typeName = CharString_createRefCStrConst(binding->type_description->type_name);
			Bool isAtomic = binding->uav_counter_id != U32_MAX || binding->uav_counter_binding;

			bufferType = ESHBufferType_StorageBuffer;

			if(binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
				bufferType = ESHBufferType_ConstantBuffer;

			else if(CharString_startsWithStringSensitive(typeName, CharString_createRefCStrConst("type."), 0)) {

				typeName.ptr += 5;
				typeName.lenAndNullTerminated -= 5;

				Bool shouldBeWrite = false;

				if (CharString_startsWithStringSensitive(typeName, CharString_createRefCStrConst("RW"), 0)) {
					typeName.ptr += 2;
					typeName.lenAndNullTerminated -= 2;
					shouldBeWrite = true;
				}

				if (CharString_equalsStringSensitive(typeName, CharString_createRefCStrConst("ByteAddressBuffer")))
					bufferType = ESHBufferType_ByteAddressBuffer;

				else {

					CharString appendBuffer = CharString_createRefCStrConst("AppendStructuredBuffer.");
					CharString consumeBuffer = CharString_createRefCStrConst("ConsumeStructuredBuffer.");
					CharString structuredBuffer = CharString_createRefCStrConst("StructuredBuffer.");

					if (
						CharString_startsWithStringSensitive(typeName, appendBuffer, 0) ||
						CharString_startsWithStringSensitive(typeName, consumeBuffer, 0)
					) {

						if(shouldBeWrite)
							retError(clean, Error_invalidState(
								0, "Compiler_convertRegisterSPIRV() invalid RW prefix for append/consume buffer"
							))

						bufferType = ESHBufferType_StructuredBufferAtomic;
						shouldBeWrite = true;
					}

					//TODO: Remember counter binding for SPIRV.

					else if(CharString_equalsStringSensitive(typeName, CharString_createRefCStrConst("ACSBuffer.counter")))
						goto clean;

					else if(CharString_startsWithStringSensitive(typeName, structuredBuffer, 0))
						bufferType = ESHBufferType_StructuredBuffer;

					else retError(clean, Error_invalidState(
						0, "Compiler_convertRegisterSPIRV() invalid RW prefix for append/consume buffer"
					))
				}

				shouldBeBufferWrite = shouldBeWrite;
			}

			if(bufferType == ESHBufferType_StorageBuffer && isAtomic)
				bufferType = ESHBufferType_StorageBufferAtomic;

			if(
				bufferType != ESHBufferType_StorageBufferAtomic &&
				bufferType != ESHBufferType_StructuredBufferAtomic &&
				isAtomic
			)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (has atomic, but invalid type)"
				))

			if(bufferType != ESHBufferType_ByteAddressBuffer && bufferType != ESHBufferType_AccelerationStructure) {

				//TODO: Storage buffer can have "static" part that's constant sized

				if (
					(
						bufferType == ESHBufferType_StructuredBuffer ||
						bufferType == ESHBufferType_StructuredBufferAtomic ||
						bufferType == ESHBufferType_StorageBuffer ||
						bufferType == ESHBufferType_StorageBufferAtomic
					) != (!binding->block.size || !binding->block.padded_size)
				)
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() buffer requires size and/or padded size to be set/unset"
					))

				gotoIfError3(clean, Compiler_convertShaderBufferSPIRV(
					binding,
					binding->descriptor_type != SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					alloc,
					&sbFile,
					e_rr
				))
			}

			break;
		}

		default:
			break;
	}

	if (bufferType == ESHBufferType_Count) {

		for(U8 i = 0; i < blockSize; ++i)
			if(blockPtrU64[i])
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() invalid register had buffer decorations but wasn't one"
				))

		if(
			binding->uav_counter_binding ||
			binding->uav_counter_id != U32_MAX ||
			binding->block.spirv_id ||
			binding->block.members ||
			binding->block.type_description ||
			binding->block.word_offset.offset
		)
			retError(clean, Error_invalidState(
				1, "Compiler_convertRegisterSPIRV() invalid register had buffer decorations but wasn't one"
			))
	}

	switch (binding->descriptor_type) {

		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:

			if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_CBV)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (not cbv)"
				))

			if(arrays.ptr)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() arrays aren't allowed on uniform buffers"
				))

			if(binding->decoration_flags)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() unexpected decoration flags on resource"
				))

			if(binding->uav_counter_id != U32_MAX || binding->uav_counter_binding)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() uav_counter_id or uav_counter_binding can't be set on cbv"
				))

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				ESHBufferType_ConstantBuffer,
				false,
				(U8)((!isUnused) << ESHBinaryType_SPIRV),
				&name,
				NULL,
				&sbFile,
				bindings,
				alloc,
				e_rr
			))

			goto clean;

		case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: {

			if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_SAMPLER)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (not sampler)"
				))

			if(imagePtrU64[0] || imagePtrU64[1] || imagePtrU64[2])
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() invalid sampler register"
				))

			gotoIfError3(clean, ListSHRegisterRuntime_addSampler(
				registers,
				(U8)((!isUnused) << ESHBinaryType_SPIRV),
				false,
				&name,
				arrays.length ? &arrays : NULL,
				bindings,
				alloc,
				e_rr
			))

			goto clean;
		}

		case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			Log_errorLn(alloc, "Unsupported combined image sampler");		//TODO:
			retError(clean, Error_invalidState(1, "Compiler_convertRegisterSPIRV() combined image samplers not supported yet"))
			break;

		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {

			if(binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {

				if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_SRV)
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() sampled image didn't have SRV resource flag"
					))

				if(binding->image.image_format || binding->image.sampled != 1)
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() unexpected image data on sampled image"
					))
			}
			else {

				if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_UAV)
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() storage image didn't have UAV resource flag"
					))

				if(!binding->image.image_format || binding->image.sampled != 2)
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() unexpected image data on storage image"
					))
			}

			if(binding->decoration_flags)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() unexpected decoration flags on image"
				))

			if(binding->image.ms && (binding->image.dim != SpvDim2D || binding->image.depth != 2))
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() unexpected multi sample image"
				))

			ESHTextureType type = ESHTextureType_Texture2D;
			Bool isArray = binding->image.arrayed;
			U8 reqDepth = 0;

			if(binding->image.ms) {
				type = ESHTextureType_Texture2DMS;
				reqDepth = 2;
			}

			else switch(binding->image.dim) {

				case SpvDim1D:				reqDepth = 1;	type = ESHTextureType_Texture1D;	break;
				case SpvDim2D:				reqDepth = 2;	type = ESHTextureType_Texture2D;	break;
				case SpvDim3D:				reqDepth = 3;	type = ESHTextureType_Texture3D;	break;
				case SpvDimCube:			reqDepth = 3;	type = ESHTextureType_TextureCube;	break;

				case SpvDimRect:
				case SpvDimBuffer:
				case SpvDimSubpassData:
				default:
					retError(clean, Error_invalidState(
						1, "Compiler_convertRegisterSPIRV() unsupported image type"
					))
			}

			if(binding->image.depth != reqDepth)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() Unexpected image depth"
				))

			if(binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
				gotoIfError3(clean, ListSHRegisterRuntime_addTexture(
					registers,
					type,
					isArray,
					false,
					(U8)((!isUnused) << ESHBinaryType_SPIRV),
					ESHTexturePrimitive_Count,
					&name,
					arrays.length ? &arrays : NULL,
					bindings,
					alloc,
					e_rr
				))

			else {

				ETextureFormatId formatId = ETextureFormatId_Undefined;

				switch (binding->image.image_format) {
					case SpvImageFormatRgba32f:		formatId = ETextureFormatId_RGBA32f;	break;
					case SpvImageFormatRgba16f:		formatId = ETextureFormatId_RGBA16f;	break;
					case SpvImageFormatR32f:		formatId = ETextureFormatId_R32f;		break;
					case SpvImageFormatRgba8:		formatId = ETextureFormatId_RGBA8;		break;
					case SpvImageFormatRgba8Snorm:	formatId = ETextureFormatId_RGBA8s;		break;
					case SpvImageFormatRg32f:		formatId = ETextureFormatId_RG32f;		break;
					case SpvImageFormatRg16f:		formatId = ETextureFormatId_RG16f;		break;
					case SpvImageFormatR16f:		formatId = ETextureFormatId_R16f;		break;
					case SpvImageFormatRgba16:		formatId = ETextureFormatId_RGBA16;		break;
					case SpvImageFormatRgb10A2:		formatId = ETextureFormatId_BGR10A2;	break;
					case SpvImageFormatRg16:		formatId = ETextureFormatId_RG16;		break;
					case SpvImageFormatRg8:			formatId = ETextureFormatId_RG8;		break;
					case SpvImageFormatR16:			formatId = ETextureFormatId_R16;		break;
					case SpvImageFormatR8:			formatId = ETextureFormatId_R8;			break;
					case SpvImageFormatRgba16Snorm:	formatId = ETextureFormatId_RGBA16s;	break;
					case SpvImageFormatRg16Snorm:	formatId = ETextureFormatId_RG16s;		break;
					case SpvImageFormatRg8Snorm:	formatId = ETextureFormatId_RG8s;		break;
					case SpvImageFormatR16Snorm:	formatId = ETextureFormatId_R16s;		break;
					case SpvImageFormatR8Snorm:		formatId = ETextureFormatId_R8s;		break;
					case SpvImageFormatRgba32i:		formatId = ETextureFormatId_RGBA32i;	break;
					case SpvImageFormatRgba16i:		formatId = ETextureFormatId_RGBA16i;	break;
					case SpvImageFormatRgba8i:		formatId = ETextureFormatId_RGBA8i;		break;
					case SpvImageFormatR32i:		formatId = ETextureFormatId_R32i;		break;
					case SpvImageFormatRg32i:		formatId = ETextureFormatId_RG32i;		break;
					case SpvImageFormatRg16i:		formatId = ETextureFormatId_RG16i;		break;
					case SpvImageFormatRg8i:		formatId = ETextureFormatId_RG8i;		break;
					case SpvImageFormatR16i:		formatId = ETextureFormatId_R16i;		break;
					case SpvImageFormatR8i:			formatId = ETextureFormatId_R8i;		break;
					case SpvImageFormatRgba32ui:	formatId = ETextureFormatId_RGBA32u;	break;
					case SpvImageFormatRgba16ui:	formatId = ETextureFormatId_RGBA16u;	break;
					case SpvImageFormatRgba8ui:		formatId = ETextureFormatId_RGBA8u;		break;
					case SpvImageFormatR32ui:		formatId = ETextureFormatId_R32u;		break;
					case SpvImageFormatRg32ui:		formatId = ETextureFormatId_RG32u;		break;
					case SpvImageFormatRg16ui:		formatId = ETextureFormatId_RG16u;		break;
					case SpvImageFormatRg8ui:		formatId = ETextureFormatId_RG8u;		break;
					case SpvImageFormatR16ui:		formatId = ETextureFormatId_R16u;		break;
					case SpvImageFormatR8ui:		formatId = ETextureFormatId_R8u;		break;

					default:
					case SpvImageFormatRgb10a2ui:
					case SpvImageFormatR64ui:
					case SpvImageFormatR64i:
					case SpvImageFormatR11fG11fB10f:
						retError(clean, Error_invalidState(
							1, "Compiler_convertRegisterSPIRV() unsupported image format: rg11fb10f, r64i, r64ui, rgb10a2ui"
						))
				}

				gotoIfError3(clean, ListSHRegisterRuntime_addRWTexture(
					registers,
					type,
					isArray,
					(U8)((!isUnused) << ESHBinaryType_SPIRV),
					ESHTexturePrimitive_Count,
					formatId,
					&name,
					arrays.ptr ? &arrays : NULL,
					bindings,
					alloc,
					e_rr
				))
			}

			break;
		}

		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: {

			if(
				binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_SRV &&
				binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_UAV
			)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (not uav/srv)"
				))

			Bool isWrite = !(binding->block.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE);

			if((binding->resource_type == SPV_REFLECT_RESOURCE_FLAG_UAV) != isWrite)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (expected uav or srv but got the opposite)"
				))

			if(shouldBeBufferWrite != isWrite)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (type had RW, but buffer didn't)"
				))

			Bool hasSBFile =
				bufferType != ESHBufferType_ByteAddressBuffer &&
				bufferType != ESHBufferType_AccelerationStructure;

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				bufferType,
				isWrite,
				(U8)((!isUnused) << ESHBinaryType_SPIRV),
				&name,
				arrays.length ? &arrays : NULL,
				hasSBFile ? &sbFile : NULL,
				bindings,
				alloc,
				e_rr
			))

			goto clean;
		}

		case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: {

			if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_SRV)
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() mismatching resource_type (not SRV)"
				))

			if(imagePtrU64[0] || imagePtrU64[1] || imagePtrU64[2])
				retError(clean, Error_invalidState(
					1, "Compiler_convertRegisterSPIRV() invalid RTAS register"
				))

			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				ESHBufferType_AccelerationStructure,
				false,
				(U8)((!isUnused) << ESHBinaryType_SPIRV),
				&name,
				arrays.length ? &arrays : NULL,
				NULL,
				bindings,
				alloc,
				e_rr
			))

			break;
		}

		case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:

			if(binding->resource_type != SPV_REFLECT_RESOURCE_FLAG_SRV)
				retError(clean, Error_invalidState(
					2, "Compiler_convertRegisterSPIRV() mismatching resource_type (not SRV)"
				))

			if(imagePtrU64[0] || imagePtrU64[1] || imagePtrU64[2])
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterSPIRV() invalid input attachment register"
				))

			if(binding->input_attachment_index >> 16)
				retError(clean, Error_invalidState(
					0, "Compiler_convertRegisterSPIRV() input attachment register out of bounds"
				))

			gotoIfError3(clean, ListSHRegisterRuntime_addSubpassInput(
				registers,
				(U8)((!isUnused) << ESHBinaryType_SPIRV),
				&name,
				bindings,
				(U16) binding->input_attachment_index,
				alloc,
				e_rr
			))

			break;

		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
		default:
			retError(clean, Error_invalidState(
				1,
				"Compiler_convertRegisterSPIRV() unsupported descriptor type "
				"(uniform/storage buffer dynamic or storage/uniform texel buffer)"
			))
	}

clean:
	SBFile_free(&sbFile, alloc);
	return s_uccess;
}

Bool Compiler_processSPIRV(
	Buffer *result,
	ListSHRegisterRuntime *registers,
	CompilerSettings settings,
	SHBinaryIdentifier toCompile,
	SpinLock *lock,
	ListSHEntryRuntime entries,
	ESHExtension *demotions,
	Allocator alloc,
	Error *e_rr
) {

	//Ensure we have a valid SPIRV file

	const void *resultPtr = result->ptr;
	U64 binLen = Buffer_length(*result);

	Bool s_uccess = true;
	SpvReflectResult res = SPV_REFLECT_RESULT_ERROR_NULL_POINTER;
	ESHExtension exts = ESHExtension_None;
	SpvReflectShaderModule spvMod{};
	spvtools::Optimizer optimizer{ SPV_ENV_UNIVERSAL_1_3 };

	ListCharString strings{};
	U8 inputSemanticCount = 0;

	if(
		binLen < 0x8 ||
		(binLen & 3) ||
		Buffer_readU32(*result, 0, NULL) != 0x07230203
	)
		retError(clean, Error_invalidState(2, "Compiler_processSPIRV() SPIRV returned is invalid"))

	if(!demotions || !result || !registers)
		retError(clean, Error_nullPointer(0, "Compiler_processSPIRV() demotions, result and registers are required"))

	//Reflect binary information, since our own parser doesn't have the info yet.

	res = spvReflectCreateShaderModule2(SPV_REFLECT_MODULE_FLAG_NO_COPY, binLen, resultPtr, &spvMod);

	if(res != SPV_REFLECT_RESULT_SUCCESS)
		retError(clean, Error_invalidState(2, "Compiler_processSPIRV() SPIRV returned couldn't be reflected"))

	//Validate capabilities.
	//This makes sure that we only output a binary that's supported by oiSH and no unknown extensions are used.

	for (U64 i = 0; i < spvMod.capability_count; ++i) {

		ESHExtension ext = (ESHExtension)(1 << ESHExtension_Count);
		gotoIfError3(clean, spvMapCapabilityToESHExtension(spvMod.capabilities[i].value, &ext, e_rr))

		//Check if extension was known to oiSH

		if(!(ext >> ESHExtension_Count))
			exts = (ESHExtension) (exts | ext);
	}

	if((toCompile.extensions & exts) != exts)
		retError(clean, Error_invalidState(
			2, "Compiler_processSPIRV() SPIRV contained capability that wasn't enabled by oiSH file (use annotations)"
		))

	//Extensions that can be generated by spvMapCapabilityToESHExtension.
	//This is used to see if demotion is possible

	*demotions = (ESHExtension)((~exts) & ESHExtension_SpirvNative);

	//Check entrypoints

	for(U64 i = 0; i < spvMod.entry_point_count; ++i) {

		SpvReflectEntryPoint entrypoint = spvMod.entry_points[i];
		Bool searchPayload = false;
		Bool searchIntersection = false;

		U8 payloadSize = 0, intersectSize = 0;

		U32 localSize[3] = { 0 };

		ESHPipelineStage stage = ESHPipelineStage_Count;

		switch (entrypoint.spirv_execution_model) {

			case SpvExecutionModelIntersectionKHR:
				searchIntersection = true;
				searchPayload = true;
				stage = ESHPipelineStage_IntersectionExt;
				break;

			case SpvExecutionModelAnyHitKHR:
				searchPayload = true;
				stage = ESHPipelineStage_AnyHitExt;
				break;

			case SpvExecutionModelClosestHitKHR:
				searchPayload = true;
				stage = ESHPipelineStage_ClosestHitExt;
				break;

			case SpvExecutionModelMissKHR:
				searchPayload = true;
				stage = ESHPipelineStage_MissExt;
				break;

			case SpvExecutionModelCallableKHR:
				searchPayload = true;
				stage = ESHPipelineStage_CallableExt;
				break;

			case SpvExecutionModelMeshEXT:
			case SpvExecutionModelTaskEXT:
			case SpvExecutionModelGLCompute: {

				switch (entrypoint.spirv_execution_model) {
					case SpvExecutionModelMeshEXT:	stage = ESHPipelineStage_MeshExt;	break;
					case SpvExecutionModelTaskEXT:	stage = ESHPipelineStage_TaskExt;	break;
					default:						stage = ESHPipelineStage_Compute;	break;

				}

				localSize[0] = entrypoint.local_size.x;
				localSize[1] = entrypoint.local_size.y;
				localSize[2] = entrypoint.local_size.z;
				gotoIfError3(clean, Compiler_validateGroupSize(localSize, e_rr))
				break;
			}

			case SpvExecutionModelRayGenerationKHR:			stage = ESHPipelineStage_RaygenExt;		break;
			case SpvExecutionModelVertex:					stage = ESHPipelineStage_Vertex;		break;
			case SpvExecutionModelFragment:					stage = ESHPipelineStage_Pixel;			break;
			case SpvExecutionModelGeometry:					stage = ESHPipelineStage_GeometryExt;	break;
			case SpvExecutionModelTessellationControl:		stage = ESHPipelineStage_Hull;			break;
			case SpvExecutionModelTessellationEvaluation:	stage = ESHPipelineStage_Domain;		break;

			default:
				retError(clean, Error_invalidState(
					2, "Compiler_processSPIRV() SPIRV contained unsupported execution model"
				))
		}

		if (searchPayload || searchIntersection)
			for (U64 j = 0; j < entrypoint.interface_variable_count; ++j) {

				SpvReflectInterfaceVariable var = entrypoint.interface_variables[j];

				Bool isPayload = var.storage_class == SpvStorageClassIncomingRayPayloadKHR;
				Bool isIntersection = var.storage_class == SpvStorageClassHitAttributeKHR;

				if(!isPayload && !isIntersection)
					continue;

				//Get struct size

				if(
					!var.type_description ||
					var.type_description->op != SpvOpTypeStruct ||
					var.type_description->type_flags != (SPV_REFLECT_TYPE_FLAG_STRUCT | SPV_REFLECT_TYPE_FLAG_EXTERNAL_BLOCK)
				)
					retError(clean, Error_invalidState(
						0, "Compiler_processSPIRV() struct payload or intersection isn't a struct"
					))

				U64 structSize = 0;
				gotoIfError3(clean, SpvCalculateStructLength(var.type_description, &structSize, e_rr))

				//Validate payload/intersect size

				if (isPayload) {

					if(structSize > 128)
						retError(clean, Error_outOfBounds(
							0, structSize, 128, "Compiler_processSPIRV() payload out of bounds"
						))

					payloadSize = (U8) structSize;
					continue;
				}

				if(structSize > 32)
					retError(clean, Error_outOfBounds(
						0, structSize, 32, "Compiler_processSPIRV() intersection attribute out of bounds"
					))

				intersectSize = (U8) structSize;
			}

		if(searchPayload && !payloadSize)
			retError(clean, Error_invalidState(0, "Compiler_processSPIRV() payload wasn't found in SPIRV"))

		if(searchIntersection && !intersectSize)
			retError(clean, Error_invalidState(0, "Compiler_processSPIRV() intersection attribute wasn't found in SPIRV"))

		if(stage == ESHPipelineStage_Count)
			retError(clean, Error_invalidState(
				0, "Compiler_processSPIRV() SPIRV entrypoint couldn't be mapped to ESHPipelineStage"
			))

		Bool isGfx =
			!(stage >= ESHPipelineStage_RtStartExt && stage <= ESHPipelineStage_RtEndExt) &&
			stage != ESHPipelineStage_WorkgraphExt &&
			stage != ESHPipelineStage_Compute;

		//Reflect inputs & outputs

		ESBType inputs[16] = {};
		ESBType outputs[16] = {};
		U8 inputSemantics[16] = {};
		U8 outputSemantics[16] = {};

		if (isGfx) {

			for (U64 j = 0; j < (U64)entrypoint.input_variable_count + entrypoint.output_variable_count; ++j) {

				Bool isOutput = j >= entrypoint.input_variable_count;

				const SpvReflectInterfaceVariable *input =
					isOutput ? entrypoint.output_variables[j - entrypoint.input_variable_count] :
					entrypoint.input_variables[j];

				if(input->built_in != (SpvBuiltIn)-1)		//We don't care about builtins
					continue;

				ESBType *inputType = isOutput ? outputs : inputs;
				U8 *inputSemantic = isOutput ? outputSemantics : inputSemantics;

				if(input->location >= 16)
					retError(clean, Error_invalidState(
						0, "Compiler_processSPIRV() input/output location out of bounds (allowed up to 16)"
					))

				if(inputType[input->location])
					retError(clean, Error_invalidState(
						0, "Compiler_processSPIRV() input/output location is already defined"
					))

				gotoIfError3(clean, SpvReflectFormatToESBType(input->format, &inputType[input->location], e_rr))

				//Grab and parse semantic

				if(!input->name || !input->name[0])
					continue;

				CharString semantic = CharString_createRefCStrConst(input->name);
				CharString inVar = CharString_createRefCStrConst("in.var.");
				CharString outVar = CharString_createRefCStrConst("out.var.");

				Bool isInVar = CharString_startsWithStringSensitive(semantic, inVar, 0);
				Bool isOutVar = CharString_startsWithStringSensitive(semantic, outVar, 0);

				if(!isInVar && !isOutVar)
					continue;

				U64 offset = isInVar ? CharString_length(inVar) : CharString_length(outVar);
				semantic.ptr += offset;
				semantic.lenAndNullTerminated -= offset;

				U64 semanticValue = 0;

				U64 semanticl = CharString_length(semantic);
				U64 firstSemanticId = semanticl;

				while(firstSemanticId && C8_isDec(CharString_getAt(semantic, firstSemanticId - 1))) {
					--firstSemanticId;
				}

				if(!firstSemanticId)
					retError(clean, Error_invalidState(
						0, "Compiler_processSPIRV() bitfield accidentally detected as semantic"
					))

				CharString semanticValueStr = CharString_createRefSizedConst(
					semantic.ptr + firstSemanticId, semanticl - firstSemanticId, true
				);

				CharString realSemanticName = CharString_createRefSizedConst(
					semantic.ptr, firstSemanticId, firstSemanticId == semanticl
				);

				if (firstSemanticId != semanticl && !CharString_parseDec(semanticValueStr, &semanticValue))
					retError(clean, Error_invalidState(
						0, "Compiler_processSPIRV() couldn't parse semantic value"
					))

				U64 semanticName = 0;

				if(
					!CharString_equalsStringInsensitive(realSemanticName, CharString_createRefCStrConst("SV_TARGET")) &&
					!CharString_equalsStringInsensitive(realSemanticName, CharString_createRefCStrConst("TEXCOORD"))
				) {
					U64 start = isOutput ? inputSemanticCount : 0;
					U64 end = isOutput ? strings.length : inputSemanticCount;
					U64 k = start;

					for(; k < end; ++k)
						if(CharString_equalsStringInsensitive(strings.ptr[k], realSemanticName))
							break;

					if(k == end)
						gotoIfError2(clean, ListCharString_pushBack(&strings, realSemanticName, alloc))

					if(!isOutput && k == end)
						++inputSemanticCount;

					semanticName = (k - start) + 1;
				}

				if(semanticName >= 16)
					retError(clean, Error_invalidState(
						1, "Compiler_processSPIRV() unique semantic name out of bounds"
					))

				if(semanticValue >= 15)
					retError(clean, Error_invalidState(
						1, "Compiler_processSPIRV() unique semantic id out of bounds"
					))

				semanticValue |= (U8)(semanticName << 4);
				inputSemantic[input->location] = !semanticName ? 0 : (U8) semanticValue;
			}
		}

		//Grab constant buffers

		for (U64 j = 0; j < entrypoint.descriptor_set_count; ++j) {

			SpvReflectDescriptorSet descriptorSet = entrypoint.descriptor_sets[j];

			for (U64 k = 0; k < descriptorSet.binding_count; ++k) {

				SpvReflectDescriptorBinding *binding = descriptorSet.bindings[k];

				if(!binding)
					continue;

				gotoIfError3(clean, Compiler_convertRegisterSPIRV(registers, binding, descriptorSet.set, alloc, e_rr))
			}
		}

		gotoIfError3(clean, Compiler_finalizeEntrypoint(
			localSize, payloadSize, intersectSize, 0,
			inputs, outputs,
			inputSemanticCount, &strings, inputSemantics, outputSemantics,
			CharString_length(toCompile.entrypoint) ? toCompile.entrypoint :
			CharString_createRefCStrConst(entrypoint.name),
			lock, entries,
			alloc, e_rr
		))
	}

	//Strip debug and optimize

	if(!settings.debug) {

		optimizer.SetMessageConsumer(
			[alloc](spv_message_level_t level, const C8 *source, const spv_position_t &position, const C8 *msg) -> void {

				const C8 *format = "%s:L#%zu:%zu (index: %zu): %s";

				switch(level) {

					case SPV_MSG_FATAL:
					case SPV_MSG_INTERNAL_ERROR:
					case SPV_MSG_ERROR:
						Log_errorLn(alloc, format, source, position.line, position.column, position.index, msg);
						break;

					case SPV_MSG_WARNING:
						Log_warnLn(alloc, format, source, position.line, position.column, position.index, msg);
						break;

					default:
						Log_debugLn(alloc, format, source, position.line, position.column, position.index, msg);
						break;
				}
			}
		);

		optimizer.RegisterPassesFromFlags({ "-O", "--legalize-hlsl" });
		optimizer.RegisterPass(spvtools::CreateStripDebugInfoPass()).RegisterPass(spvtools::CreateStripReflectInfoPass());

		std::vector<U32> tmp;
		std::vector<U32> copied;

		if ((U64)resultPtr & 3) {		//Fix alignment
			copied.resize(binLen >> 2);
			Buffer_memcpy(Buffer_createRef(copied.data(), binLen), Buffer_createRefConst(resultPtr, binLen));
			resultPtr = copied.data();
		}

		if(!optimizer.Run((const U32*)resultPtr, binLen >> 2, &tmp))
			retError(clean, Error_invalidState(0, "Compiler_processSPIRV() stripping spirv failed"))

		Buffer_free(result, alloc);
		gotoIfError2(clean, Buffer_createCopy(Buffer_createRefConst(tmp.data(), (U64)tmp.size() << 2), alloc, result))
	}

clean:

	ListCharString_freeUnderlying(&strings, alloc);

	spvReflectDestroyShaderModule(&spvMod);
	return s_uccess;
}

Bool Compiler_disassembleSPIRV(Buffer buf, Allocator alloc, CharString *result, Error *e_rr) {
	
	Bool s_uccess = true;
	U64 binLen = Buffer_length(buf);
	const void *resultPtr = buf.ptr;

	spvtools::SpirvTools tool{ SPV_ENV_UNIVERSAL_1_3 };
	std::string str;

	spv_binary_to_text_options_t opts = (spv_binary_to_text_options_t) (
		SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES |
		SPV_BINARY_TO_TEXT_OPTION_NESTED_INDENT |
		SPV_BINARY_TO_TEXT_OPTION_REORDER_BLOCKS |
		SPV_BINARY_TO_TEXT_OPTION_COMMENT |
		SPV_BINARY_TO_TEXT_OPTION_SHOW_BYTE_OFFSET |
		SPV_BINARY_TO_TEXT_OPTION_INDENT
	);

	std::vector<U32> copied;

	if(
		binLen < 0x8 ||
		(binLen & 3) ||
		Buffer_readU32(buf, 0, NULL) != 0x07230203
	)
		retError(clean, Error_invalidState(0, "Compiler_createDisassembly() SPIRV is invalid"))

	if ((U64)resultPtr & 3) {		//Fix alignment
		copied.resize(binLen >> 2);
		Buffer_memcpy(Buffer_createRef(copied.data(), binLen), Buffer_createRefConst(resultPtr, binLen));
		resultPtr = copied.data();
	}

	if(!tool.Disassemble((const U32*)resultPtr, binLen >> 2, &str, opts))
		retError(clean, Error_invalidOperation(0, "Compiler_createDisassembly() SPIRV couldn't be disassembled"))

	gotoIfError2(clean, CharString_createCopy(
		CharString_createRefSizedConst(str.c_str(), str.size(), true), alloc, result
	))

clean:
	return s_uccess;
}
