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

//shader_compiler/compiler_spv_capability.cpp

#include "shader_compiler/compiler.h"

#include "SPIRV-Reflect/spirv_reflect.h"
#include "compiler_spv_internal.hpp"

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

		//SPV_EXT_shader_invocation_reorder (ShaderInvocationReorderEXT) is what DXC emits for dx::HitObject SER.
		case SpvCapabilityShaderInvocationReorderEXT:
			ext = ESHExtension_RayReorder;
			break;

		//SM6.10 ray triangle vertex position fetch (SPV_KHR_ray_tracing_position_fetch):
		// the ray-pipeline (RayTracingPositionFetchKHR) and RayQuery (RayQueryPositionFetchKHR) capabilities share one
		// extension.
		//SV_Barycentrics / GetAttributeAtVertex (SPV_KHR_fragment_shader_barycentric)
		case SpvCapabilityFragmentBarycentricKHR:
			ext = ESHExtension_Barycentrics;
			break;

		case SpvCapabilityRayTracingPositionFetchKHR:
		case SpvCapabilityRayQueryPositionFetchKHR:
			ext = ESHExtension_RayTriPosition;
			break;

		//SM6.10 cooperative vectors (per-thread matrix*vector) lower to SPV_NV_cooperative_vector (NV-only).
		case SpvCapabilityCooperativeVectorNV:
			ext = ESHExtension_CoopVec;
			break;

		//Training capability (outer-product / reduce-sum accumulate) is a distinct device tier (Tier 1.1), gated separately.
		case SpvCapabilityCooperativeVectorTrainingNV:
			ext = ESHExtension_CoopVecTraining;
			break;

		//SM6.10 cooperative matrix (subgroup matrix*matrix / GEMM) lowers to the cross-vendor SPV_KHR_cooperative_matrix.
		case SpvCapabilityCooperativeMatrixKHR:
			ext = ESHExtension_CoopMat;
			break;

		//Full bindless (SM6.6 dynamic resources) lowers to SPV_EXT_descriptor_heap.
		case SpvCapabilityDescriptorHeapEXT:
			ext = ESHExtension_DescriptorHeap;
			break;

		//Cooperative matrix (and, per spec, cooperative vector) require the Vulkan memory model.
		//DXC upgrades OpMemoryModel to Vulkan and declares these.
		//They enable no oiSH extension of their own, but must be allowed.
		case SpvCapabilityVulkanMemoryModel:
		case SpvCapabilityVulkanMemoryModelDeviceScope:
			break;

		//FP8 (e4m3/e5m2) in cooperative matrix (SPV_EXT_float8): the reflection-detectable half of the CoopFP8 tier
		//(cooperative-vector FP8 is only an interpretation, so it's annotation-driven - see ESHExtension_CoopFP8).
		case SpvCapabilityFloat8CooperativeMatrixEXT:
			ext = ESHExtension_CoopFP8;
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

		//Both compute-derivative group modes are the same OxC3 feature (ComputeDeriv).
		//DXC emits the Quads variant for even 2D thread groups (ddx/ddy in compute); the Linear variant for other layouts.
		case SpvCapabilityComputeDerivativeGroupLinearNV:
		case SpvCapabilityComputeDerivativeGroupQuadsNV:
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

		case SpvCapabilityStorageImageReadWithoutFormat:
		case SpvCapabilityStorageImageWriteWithoutFormat:

		//Capabilities introduced by the SPIRV-Headers update that OxC3 doesn't expose as oiSH extensions
		// (ML / tensors / cooperative matrix / bfloat / small floats / vendor FPGA & subgroup ops).
		//Allowed but map to no extension.
		case SpvCapabilityTensorsARM:
		case SpvCapabilityStorageTensorArrayDynamicIndexingARM:
		case SpvCapabilityStorageTensorArrayNonUniformIndexingARM:
		case SpvCapabilityGraphARM:
		case SpvCapabilityCooperativeMatrixLayoutsARM:
		case SpvCapabilityFloat8EXT:
		case SpvCapabilityFloat6EXT:
		case SpvCapabilityFloat4EXT:
		case SpvCapabilityFloat8UnsignedE8M0EXT:
		case SpvCapabilityMXInt8EXT:
		case SpvCapabilityBitcastExtractEXT:
		case SpvCapabilityUntypedPointersKHR:
		case SpvCapabilityTileShadingQCOM:
		case SpvCapabilityCooperativeMatrixConversionQCOM:
		case SpvCapabilityTextureBlockMatch2QCOM:
		case SpvCapabilityMultipleWaitQueuesQCOM:
		case SpvCapabilityImageGatherLinearQCOM:
		case SpvCapabilityImageGatherExtendedModesQCOM:
		case SpvCapabilityShaderEnqueueAMDX:
		case SpvCapabilityQuadControlKHR:
		case SpvCapabilityInt4TypeINTEL:
		case SpvCapabilityInt4CooperativeMatrixINTEL:
		case SpvCapabilityBFloat16TypeKHR:
		case SpvCapabilityBFloat16DotProductKHR:
		case SpvCapabilityBFloat16CooperativeMatrixKHR:
		case SpvCapabilityAbortKHR:
		case SpvCapabilityConstantDataKHR:
		case SpvCapabilityPoisonFreezeKHR:
		case SpvCapabilityWeakLinkageAMD:
		case SpvCapabilityDisplacementMicromapNV:
		case SpvCapabilityAtomicFloat16VectorNV:
		case SpvCapabilityRayTracingDisplacementMicromapNV:
		case SpvCapabilityRawAccessChainsNV:
		case SpvCapabilityRayTracingSpheresGeometryNV:
		case SpvCapabilityRayTracingLinearSweptSpheresGeometryNV:
		case SpvCapabilityPushConstantBanksNV:
		case SpvCapabilityLongVectorEXT:
		case SpvCapabilityShader64BitIndexingEXT:
		case SpvCapabilityCooperativeMatrixReductionsNV:
		case SpvCapabilityCooperativeMatrixConversionsNV:
		case SpvCapabilityCooperativeMatrixPerElementOperationsNV:
		case SpvCapabilityCooperativeMatrixTensorAddressingNV:
		case SpvCapabilityCooperativeMatrixBlockLoadsNV:
		case SpvCapabilityRayTracingClusterAccelerationStructureNV:
		case SpvCapabilityTensorAddressingNV:
		case SpvCapabilityCooperativeMatrixDecodeVectorNV:
		case SpvCapabilityReplicatedCompositesEXT:
		case SpvCapabilityFloatControls2:
		case SpvCapabilityFMAKHR:
		case SpvCapabilityRayTracingOpacityMicromapExecutionModeKHR:
		case SpvCapabilityLongCompositesINTEL:
		case SpvCapabilityArithmeticFenceEXT:
		case SpvCapabilityFPGAClusterAttributesV2ALTERA:
		case SpvCapabilityTaskSequenceALTERA:
		case SpvCapabilityFPMaxErrorINTEL:
		case SpvCapabilityGlobalVariableHostAccessINTEL:
		case SpvCapabilityGlobalVariableFPGADecorationsALTERA:
		case SpvCapabilitySubgroupBufferPrefetchINTEL:
		case SpvCapabilitySubgroup2DBlockIOINTEL:
		case SpvCapabilitySubgroup2DBlockTransformINTEL:
		case SpvCapabilitySubgroup2DBlockTransposeINTEL:
		case SpvCapabilitySubgroupMatrixMultiplyAccumulateINTEL:
		case SpvCapabilityTernaryBitwiseFunctionINTEL:
		case SpvCapabilityUntypedVariableLengthArrayINTEL:
		case SpvCapabilitySpecConditionalINTEL:
		case SpvCapabilityFunctionVariantsINTEL:
		case SpvCapabilityPredicatedIOINTEL:
		case SpvCapabilityRoundedDivideSqrtINTEL:
		case SpvCapabilityTensorFloat32RoundingINTEL:
		case SpvCapabilityMaskedGatherScatterINTEL:
		case SpvCapabilityCacheControlsINTEL:
		case SpvCapabilityRegisterLimitsINTEL:
		case SpvCapabilityBindlessImagesINTEL:
		case SpvCapabilityDotProductFloat16AccFloat32VALVE:
		case SpvCapabilityDotProductFloat16AccFloat16VALVE:
		case SpvCapabilityDotProductBFloat16AccVALVE:
		case SpvCapabilityDotProductFloat8AccFloat32VALVE:
		case SpvCapabilityIntrinsicSAMSUNG:
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

		case SpvCapabilityOptNoneINTEL:

		case SpvCapabilityDebugInfoModuleINTEL:
		case SpvCapabilityBFloat16ConversionINTEL:
		case SpvCapabilitySplitBarrierINTEL:
		case SpvCapabilityFPGAKernelAttributesv2INTEL:
		case SpvCapabilityFPGALatencyControlINTEL:
		case SpvCapabilityFPGAArgumentInterfacesINTEL:

		//Possible in the future? TODO:

		case SpvCapabilityShaderViewportIndexLayerEXT:
		case SpvCapabilityDemoteToHelperInvocation:
		case SpvCapabilityExpectAssumeKHR:
		case SpvCapabilityBitInstructions:

		case SpvCapabilityMultiViewport:
		case SpvCapabilityShaderLayer:
		case SpvCapabilityShaderViewportIndex:

		case SpvCapabilityFragmentShaderSampleInterlockEXT:
		case SpvCapabilityFragmentShaderShadingRateInterlockEXT:
		case SpvCapabilityFragmentShaderPixelInterlockEXT:

		case SpvCapabilityRayTraversalPrimitiveCullingKHR:
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

		//NV SER is unsupported; OxC3 maps the EXT form (SPV_EXT_shader_invocation_reorder) instead.
		case SpvCapabilityShaderInvocationReorderNV:

		//Motion blur was removed 2026-08-18: DXR has no equivalent and RTXMG dropped it, so a shader asking
		// for it can no longer be represented in oiSH.
		case SpvCapabilityRayTracingMotionBlurNV:

			retError(clean, Error_invalidState(
				2, "spvMapCapabilityToESHExtension() SPIRV contained capability that isn't supported in oiSH"
			));

		case SpvCapabilityMax:
			retError(clean, Error_invalidState(
				2, "spvMapCapabilityToESHExtension() SPIRV contained invalid capability that isn't supported in SPIRV-Headers"
			));
	}

	//Handled separately to ensure there's no default case in the switch,
	// so that new capabilities are reported when SPIRV-Header update on some compilers.

	if(capability > SpvCapabilityMax)
		retError(clean, Error_invalidState(
			2, "spvMapCapabilityToESHExtension() SPIRV contained invalid capability that isn't supported in SPIRV-Headers"
		));

	*extension = ext;

clean:
	return s_uccess;
}
