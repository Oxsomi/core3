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

//graphics/test/interface/test_graphics_capabilities.c
//
//Coverage group C: the capability bits themselves.
//
//  37. GraphicsDevice/capabilities - the invariants between bits, and that gated APIs agree with them
//
//There are ~57 capability bits across features, features2, dataTypes and featuresExt, and a module per bit
//would mostly restate the backend that set it.
//What isn't covered anywhere else is whether the reported set is internally consistent, and whether the API
// actually agrees with what was reported.
//A backend that reports a sub-feature without its parent, or refuses a feature it claims, is a real bug that
// no single-feature test would catch, because each of those tests reads the same bit it is validating.

#include "types/test/test.h"
#include "types/container/string.h"
#include "types/base/error.h"
#include "types/base/constants.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_info.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/render_texture.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/pipeline_structs.h"
#include "test_graphics_shared.h"

//Names are only for the coverage log, so a run says which bits this adapter actually exercised rather than
// leaving "it passed" ambiguous between "tested" and "skipped".

typedef struct TestCapabilityBit {
	U32 bit;
	const C8 *name;
} TestCapabilityBit;

static const TestCapabilityBit testFeatureBits[] = {
	{ EGraphicsFeatures_DirectRendering,        "DirectRendering" },
	{ EGraphicsFeatures_VariableRateShading,    "VariableRateShading" },
	{ EGraphicsFeatures_MultiDrawIndirectCount, "MultiDrawIndirectCount" },
	{ EGraphicsFeatures_MeshShader,             "MeshShader" },
	{ EGraphicsFeatures_GeometryShader,         "GeometryShader" },
	{ EGraphicsFeatures_SubgroupArithmetic,     "SubgroupArithmetic" },
	{ EGraphicsFeatures_SubgroupShuffle,        "SubgroupShuffle" },
	{ EGraphicsFeatures_Multiview,              "Multiview" },
	{ EGraphicsFeatures_Raytracing,             "Raytracing" },
	{ EGraphicsFeatures_RayPipeline,            "RayPipeline" },
	{ EGraphicsFeatures_RayQuery,               "RayQuery" },
	{ EGraphicsFeatures_RayMicromapOpacity,     "RayMicromapOpacity" },
	{ EGraphicsFeatures_RayReorder,             "RayReorder" },
	{ EGraphicsFeatures_RayValidation,          "RayValidation" },
	{ EGraphicsFeatures_RayTriPosition,         "RayTriPosition" },
	{ EGraphicsFeatures_LUID,                   "LUID" },
	{ EGraphicsFeatures_Wireframe,              "Wireframe" },
	{ EGraphicsFeatures_LogicOp,                "LogicOp" },
	{ EGraphicsFeatures_DualSrcBlend,           "DualSrcBlend" },
	{ EGraphicsFeatures_SwapchainCompute,       "SwapchainCompute" },
	{ EGraphicsFeatures_ComputeDeriv,           "ComputeDeriv" },
	{ EGraphicsFeatures_MeshTaskTexDeriv,       "MeshTaskTexDeriv" },
	{ EGraphicsFeatures_WriteMSTexture,         "WriteMSTexture" },
	{ EGraphicsFeatures_Bindless,               "Bindless" },
	{ EGraphicsFeatures_SubgroupOperations,     "SubgroupOperations" },
	{ EGraphicsFeatures_CoopVec,                "CoopVec" },
	{ EGraphicsFeatures_CoopMat,                "CoopMat" },
	{ EGraphicsFeatures_CoopFP8,                "CoopFP8" },
	{ EGraphicsFeatures_CoopVecTraining,        "CoopVecTraining" }
};

static const TestCapabilityBit testDataTypeBits[] = {
	{ EGraphicsDataTypes_F64,        "F64" },
	{ EGraphicsDataTypes_I64,        "I64" },
	{ EGraphicsDataTypes_F16,        "F16" },
	{ EGraphicsDataTypes_I16,        "I16" },
	{ EGraphicsDataTypes_AtomicI64,  "AtomicI64" },
	{ EGraphicsDataTypes_AtomicF32,  "AtomicF32" },
	{ EGraphicsDataTypes_AtomicF64,  "AtomicF64" },
	{ EGraphicsDataTypes_ASTC,       "ASTC" },
	{ EGraphicsDataTypes_BCn,        "BCn" },
	{ EGraphicsDataTypes_MSAA2x,     "MSAA2x" },
	{ EGraphicsDataTypes_MSAA8x,     "MSAA8x" },
	{ EGraphicsDataTypes_RGB32f,     "RGB32f" },
	{ EGraphicsDataTypes_RGB32i,     "RGB32i" },
	{ EGraphicsDataTypes_RGB32u,     "RGB32u" },
	{ EGraphicsDataTypes_D24S8,      "D24S8" },
	{ EGraphicsDataTypes_S8,         "S8" },
	{ EGraphicsDataTypes_D32S8,      "D32S8" }
};

void Test_graphicsCapabilities(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "GraphicsDevice/capabilities");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	const GraphicsDeviceCapabilities caps = device->info.capabilities;

	const EGraphicsFeatures f = caps.features;
	const EGraphicsFeatures2 f2 = caps.features2;
	const EGraphicsDataTypes d = caps.dataTypes;

	// -- Invariants a reported capability set has to satisfy -----------------------------------------

	//Raytracing is the umbrella bit: the backends set it exactly when a ray pipeline or ray query is present,
	// so it can be neither claimed alone nor omitted when one of those is there.

	const Bool anyRay = (f & (EGraphicsFeatures_RayPipeline | EGraphicsFeatures_RayQuery)) != 0;

	Test_assert(t, "raytracingMatchesRayApis", !!(f & EGraphicsFeatures_Raytracing) == anyRay);

	//Every ray sub-feature is derived inside the raytracing branch, so none can appear without it.

	const EGraphicsFeatures raySubFeatures =
		EGraphicsFeatures_RayMicromapOpacity |
		EGraphicsFeatures_RayReorder | EGraphicsFeatures_RayValidation | EGraphicsFeatures_RayTriPosition;

	if(f & raySubFeatures)
		Test_assert(t, "raySubFeatureImpliesRaytracing", (f & EGraphicsFeatures_Raytracing) != 0);

	//RayReorderActual only says the device really reorders.
	//The API bit has to be there too.

	if(f2 & EGraphicsFeatures2_RayReorderActual)
		Test_assert(t, "reorderActualImpliesReorder", (f & EGraphicsFeatures_RayReorder) != 0);

	//Same shape for opacity micromaps, except this one is derived rather than reported, so the check also
	// covers the derivation running on a device that never claimed the base feature.

	if(f2 & EGraphicsFeatures2_RayMicromapOpacityActual)
		Test_assert(t, "ommActualImpliesOmm", (f & EGraphicsFeatures_RayMicromapOpacity) != 0);

	//The RT specific features2 bits are equally meaningless without raytracing itself.

	const EGraphicsFeatures2 rayFeatures2 =
		EGraphicsFeatures2_RayClusterAS | EGraphicsFeatures2_RayPartitionedTLAS |
		EGraphicsFeatures2_RayIndirectASBuild | EGraphicsFeatures2_RayReorderActual |
		EGraphicsFeatures2_RayMicromapOpacityActual;

	if(f2 & rayFeatures2)
		Test_assert(t, "rayFeatures2ImpliesRaytracing", (f & EGraphicsFeatures_Raytracing) != 0);

	//One of the two block compression families is mandatory, per the enum's own documentation.

	Test_assert(t, "hasBlockCompression", (d & (EGraphicsDataTypes_ASTC | EGraphicsDataTypes_BCn)) != 0);

	//Shader models are cumulative, so each bit has to imply the one below it.
	//The D3D12 probe used to satisfy this vacuously by reporting every model the RUNTIME knew.
	//Now that it reads the device's real ceiling, the chain is what catches a probe regressing to that.
	//featuresExt means something else on Vulkan, so only the D3D12 backend is held to it.

	if (GraphicsInstanceRef_ptr(device->instance)->api == EGraphicsApi_Direct3D12) {

		const U32 fx = caps.featuresExt;

		if(fx & EDxGraphicsFeatures_SM6_10)
			Test_assert(t, "sm610ImpliesSm69", (fx & EDxGraphicsFeatures_SM6_9) != 0);

		if(fx & EDxGraphicsFeatures_SM6_9)
			Test_assert(t, "sm69ImpliesSm68", (fx & EDxGraphicsFeatures_SM6_8) != 0);

		if(fx & EDxGraphicsFeatures_SM6_8)
			Test_assert(t, "sm68ImpliesSm67", (fx & EDxGraphicsFeatures_SM6_7) != 0);

		if(fx & EDxGraphicsFeatures_SM6_7)
			Test_assert(t, "sm67ImpliesSm66", (fx & EDxGraphicsFeatures_SM6_6) != 0);
	}

	//Cooperative FP8 and training are refinements of the cooperative types, not standalone features.

	if(d && (f & EGraphicsFeatures_CoopVecTraining))
		Test_assert(t, "coopTrainingImpliesCoopVec", (f & EGraphicsFeatures_CoopVec) != 0);

	if(f & EGraphicsFeatures_CoopFP8)
		Test_assert(t, "coopFP8ImpliesCoopType", (f & (EGraphicsFeatures_CoopVec | EGraphicsFeatures_CoopMat)) != 0);

	//Subgroup arithmetic and shuffle are specific subgroup operations, so the umbrella bit has to be set.

	if(f & (EGraphicsFeatures_SubgroupArithmetic | EGraphicsFeatures_SubgroupShuffle))
		Test_assert(t, "subgroupSubsetImpliesOperations", (f & EGraphicsFeatures_SubgroupOperations) != 0);

	//Mesh/task derivatives require mesh shaders to exist at all.

	if(f & EGraphicsFeatures_MeshTaskTexDeriv)
		Test_assert(t, "meshDerivImpliesMeshShader", (f & EGraphicsFeatures_MeshShader) != 0);

	//experimentalFeatures is documented as a subset of features, so a bit can't be experimental without also
	// being reported as present.

	Test_assert(t, "experimentalIsSubset", (caps.experimentalFeatures & ~f) == 0);
	Test_assert(t, "experimental2IsSubset", (caps.experimentalFeatures2 & ~f2) == 0);

	//Memory limits have to be usable numbers, since allocation paths divide and compare against them.
	//maxBufferSize and maxAllocationSize are independent limits, the largest single buffer resource against the
	// largest single memory allocation, so neither bounds the other and only the floor is a real invariant.
	//Device selection refuses anything below 256 MiB on either, so reaching this point guarantees both.

	Test_assert(t, "hasMemory", caps.sharedMemory || caps.dedicatedMemory);
	Test_assert(t, "maxBufferSizeFloor", caps.maxBufferSize >= 256 * MIBI);
	Test_assert(t, "maxAllocationSizeFloor", caps.maxAllocationSize >= 256 * MIBI);

	// -- The API has to agree with what was reported -------------------------------------------------

	//Each of these reads a capability bit and then exercises the thing it gates, in whichever direction the
	// bit points.
	//A mismatch here means the reported set and the implementation disagree, which is exactly what a per
	// feature test that only runs when its own bit is set can never see.

	const CharString name = CharString_createRefCStrConst("Capability probe");

	//MSAA: a render texture at a sample count the device claims must be creatable, and one it doesn't claim
	// must be refused.

	{
		RenderTextureRef *rt = NULL;

		const Bool has2x = (d & EGraphicsDataTypes_MSAA2x) != 0;

		const Bool created2x = GraphicsDeviceRef_createRenderTexture(
			deviceRef, ETextureType_2D, 4, 4, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
			EMSAASamples_x2Ext, NULL, &name, &rt, NULL
		);

		Test_assert(t, "msaa2xMatchesCapability", created2x == has2x);
		RefPtr_dec(&rt);

		const Bool has8x = (d & EGraphicsDataTypes_MSAA8x) != 0;

		const Bool created8x = GraphicsDeviceRef_createRenderTexture(
			deviceRef, ETextureType_2D, 4, 4, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
			EMSAASamples_x8Ext, NULL, &name, &rt, NULL
		);

		Test_assert(t, "msaa8xMatchesCapability", created8x == has8x);
		RefPtr_dec(&rt);
	}

	//MSAA off is not a capability, it is the baseline, so it must always work.

	{
		RenderTextureRef *rt = NULL;

		Test_assert(t, "msaaOffAlwaysWorks", GraphicsDeviceRef_createRenderTexture(
			deviceRef, ETextureType_2D, 4, 4, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
			EMSAASamples_Off, NULL, &name, &rt, &t->err
		));

		RefPtr_dec(&rt);
	}

	//Bindless: a layout that asks for bindless arrays is only legal when the device reports Bindless.

	{
		const Bool hasBindless = (f & EGraphicsFeatures_Bindless) != 0;

		DescriptorLayoutRef *layout = NULL;
		ListDescriptorBinding bindings = (ListDescriptorBinding) { 0 };

		DescriptorLayoutInfo info = (DescriptorLayoutInfo) {
			.flags = EDescriptorLayoutFlags_AllowBindlessOnArrays,
			.bindings = bindings
		};

		const Bool created = GraphicsDeviceRef_createDescriptorLayout(deviceRef, &info, &name, &layout, NULL);

		//An empty binding list may be refused for reasons unrelated to bindless, so this only asserts the one
		// direction that is unambiguous: without the capability it must not succeed.

		if(!hasBindless)
			Test_assert(t, "bindlessRefusedWithoutCapability", !created);

		RefPtr_dec(&layout);
		ListDescriptorBinding_free(&bindings, Platform_instance->alloc);
	}

	// -- Coverage report -----------------------------------------------------------------------------

	//Which bits this adapter actually has, so a green run is readable as coverage rather than as a claim that
	// everything was exercised.

	U32 featureCount = 0, dataTypeCount = 0;

	for(U64 i = 0; i < sizeof(testFeatureBits) / sizeof(testFeatureBits[0]); ++i)
		featureCount += (f & testFeatureBits[i].bit) != 0;

	for(U64 i = 0; i < sizeof(testDataTypeBits) / sizeof(testDataTypeBits[0]); ++i)
		dataTypeCount += (d & testDataTypeBits[i].bit) != 0;

	Log_debugLnx(
		"-- capabilities: %"PRIu32"/%"PRIu64" features, %"PRIu32"/%"PRIu64" data types, features2 0x%"PRIx32,
		featureCount, (U64)(sizeof(testFeatureBits) / sizeof(testFeatureBits[0])),
		dataTypeCount, (U64)(sizeof(testDataTypeBits) / sizeof(testDataTypeBits[0])),
		(U32) f2
	);

	//A device reporting nothing at all would make every conditional assert above vacuous, so make that loud.

	Test_assert(t, "reportsAnyFeature", featureCount > 0);
	Test_assert(t, "reportsAnyDataType", dataTypeCount > 0);
}
