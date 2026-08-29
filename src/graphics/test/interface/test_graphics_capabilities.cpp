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

//graphics/test/interface/test_graphics_capabilities.cpp
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
//
//Written against the C++ layer (graphics/graphics.hpp) rather than the C API, so every handle releases itself
// and there is no RefPtr_dec ladder left to keep in step with the locals.
//Three things stay on the C entry points, each called out where it happens: the owning instance, which is a
// raw device field the wrapper does not expose, the MSAA sample count, which both wrapper texture factories
// pin to Off, and the log, whose x macros cannot be reached through a namespace.

#include "graphics/graphics.hpp"

//Log::debugLn is the C++ front for Log_debugLnx; the x macros name ELogOptions_NewLine unqualified and so
//cannot be reached through the c namespace.

#include "types/container/log.hpp"

namespace oxc { namespace c {
	#include "test_graphics_shared.h"
	#include "platforms/platform.h"
}}

namespace {

	//graphics.hpp already has the guard this was: OwnedList frees its list on every exit path, error
	//returns included.

	using OwnedLayoutInfo = oxc::gfx::OwnedList<oxc::c::DescriptorLayoutInfo, oxc::c::DescriptorLayoutInfo_free>;

	//Names are only for the coverage log, so a run says which bits this adapter actually exercised rather than
	// leaving "it passed" ambiguous between "tested" and "skipped".

	struct TestCapabilityBit {
		oxc::c::U32 bit;
		const oxc::c::C8 *name;
	};

	const TestCapabilityBit testFeatureBits[] = {
		{ oxc::c::EGraphicsFeatures_DirectRendering,        "DirectRendering" },
		{ oxc::c::EGraphicsFeatures_VariableRateShading,    "VariableRateShading" },
		{ oxc::c::EGraphicsFeatures_MultiDrawIndirectCount, "MultiDrawIndirectCount" },
		{ oxc::c::EGraphicsFeatures_MeshShader,             "MeshShader" },
		{ oxc::c::EGraphicsFeatures_GeometryShader,         "GeometryShader" },
		{ oxc::c::EGraphicsFeatures_SubgroupArithmetic,     "SubgroupArithmetic" },
		{ oxc::c::EGraphicsFeatures_SubgroupShuffle,        "SubgroupShuffle" },
		{ oxc::c::EGraphicsFeatures_Multiview,              "Multiview" },
		{ oxc::c::EGraphicsFeatures_Raytracing,             "Raytracing" },
		{ oxc::c::EGraphicsFeatures_RayPipeline,            "RayPipeline" },
		{ oxc::c::EGraphicsFeatures_RayQuery,               "RayQuery" },
		{ oxc::c::EGraphicsFeatures_RayMicromapOpacity,     "RayMicromapOpacity" },
		{ oxc::c::EGraphicsFeatures_RayReorder,             "RayReorder" },
		{ oxc::c::EGraphicsFeatures_RayValidation,          "RayValidation" },
		{ oxc::c::EGraphicsFeatures_RayTriPosition,         "RayTriPosition" },
		{ oxc::c::EGraphicsFeatures_LUID,                   "LUID" },
		{ oxc::c::EGraphicsFeatures_Wireframe,              "Wireframe" },
		{ oxc::c::EGraphicsFeatures_LogicOp,                "LogicOp" },
		{ oxc::c::EGraphicsFeatures_DualSrcBlend,           "DualSrcBlend" },
		{ oxc::c::EGraphicsFeatures_SwapchainCompute,       "SwapchainCompute" },
		{ oxc::c::EGraphicsFeatures_ComputeDeriv,           "ComputeDeriv" },
		{ oxc::c::EGraphicsFeatures_MeshTaskTexDeriv,       "MeshTaskTexDeriv" },
		{ oxc::c::EGraphicsFeatures_WriteMSTexture,         "WriteMSTexture" },
		{ oxc::c::EGraphicsFeatures_Bindless,               "Bindless" },
		{ oxc::c::EGraphicsFeatures_SubgroupOperations,     "SubgroupOperations" },
		{ oxc::c::EGraphicsFeatures_CoopVec,                "CoopVec" },
		{ oxc::c::EGraphicsFeatures_CoopMat,                "CoopMat" },
		{ oxc::c::EGraphicsFeatures_CoopFP8,                "CoopFP8" },
		{ oxc::c::EGraphicsFeatures_CoopVecTraining,        "CoopVecTraining" }
	};

	const TestCapabilityBit testDataTypeBits[] = {
		{ oxc::c::EGraphicsDataTypes_F64,        "F64" },
		{ oxc::c::EGraphicsDataTypes_I64,        "I64" },
		{ oxc::c::EGraphicsDataTypes_F16,        "F16" },
		{ oxc::c::EGraphicsDataTypes_I16,        "I16" },
		{ oxc::c::EGraphicsDataTypes_AtomicI64,  "AtomicI64" },
		{ oxc::c::EGraphicsDataTypes_AtomicF32,  "AtomicF32" },
		{ oxc::c::EGraphicsDataTypes_AtomicF64,  "AtomicF64" },
		{ oxc::c::EGraphicsDataTypes_ASTC,       "ASTC" },
		{ oxc::c::EGraphicsDataTypes_BCn,        "BCn" },
		{ oxc::c::EGraphicsDataTypes_MSAA2x,     "MSAA2x" },
		{ oxc::c::EGraphicsDataTypes_MSAA8x,     "MSAA8x" },
		{ oxc::c::EGraphicsDataTypes_RGB32f,     "RGB32f" },
		{ oxc::c::EGraphicsDataTypes_RGB32i,     "RGB32i" },
		{ oxc::c::EGraphicsDataTypes_RGB32u,     "RGB32u" },
		{ oxc::c::EGraphicsDataTypes_D24S8,      "D24S8" },
		{ oxc::c::EGraphicsDataTypes_S8,         "S8" },
		{ oxc::c::EGraphicsDataTypes_D32S8,      "D32S8" }
	};

	//DescriptorLayoutInfo is a plain C struct with no wrapper, so it gets a local guard rather than a manual
	//free on every exit path.

}

extern "C" void Test_graphicsCapabilities(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Error *e_rr = &t->err;
	const c::Allocator *alloc = c::Platform_instance->alloc;

	Test_setModule(t, "GraphicsDevice/capabilities");

	//The harness owns this ref, so it is borrowed rather than adopted.

	Device dev = Device::share(deviceRef);

	const c::GraphicsDeviceCapabilities caps = dev.info().capabilities;

	const c::EGraphicsFeatures f = caps.features;
	const c::EGraphicsFeatures2 f2 = caps.features2;
	const c::EGraphicsDataTypes d = caps.dataTypes;

	// -- Invariants a reported capability set has to satisfy -----------------------------------------

	//Raytracing is the umbrella bit: the backends set it exactly when a ray pipeline or ray query is present,
	// so it can be neither claimed alone nor omitted when one of those is there.

	const c::Bool anyRay = (f & (c::EGraphicsFeatures_RayPipeline | c::EGraphicsFeatures_RayQuery)) != 0;

	Test_assert(t, "raytracingMatchesRayApis", !!(f & c::EGraphicsFeatures_Raytracing) == anyRay);

	//Every ray sub-feature is derived inside the raytracing branch, so none can appear without it.

	const c::EGraphicsFeatures raySubFeatures = (c::EGraphicsFeatures)(
		c::EGraphicsFeatures_RayMicromapOpacity |
		c::EGraphicsFeatures_RayReorder | c::EGraphicsFeatures_RayValidation | c::EGraphicsFeatures_RayTriPosition
	);

	if(f & raySubFeatures)
		Test_assert(t, "raySubFeatureImpliesRaytracing", (f & c::EGraphicsFeatures_Raytracing) != 0);

	//RayReorderActual only says the device really reorders.
	//The API bit has to be there too.

	if(f2 & c::EGraphicsFeatures2_RayReorderActual)
		Test_assert(t, "reorderActualImpliesReorder", (f & c::EGraphicsFeatures_RayReorder) != 0);

	//Same shape for opacity micromaps, except this one is derived rather than reported, so the check also
	//covers the derivation running on a device that never claimed the base feature.

	if(f2 & c::EGraphicsFeatures2_RayMicromapOpacityActual)
		Test_assert(t, "ommActualImpliesOmm", (f & c::EGraphicsFeatures_RayMicromapOpacity) != 0);

	//8-bit OMM indices only mean anything on a device that takes micromaps at all

	if(f2 & c::EGraphicsFeatures2_RayMicromapOpacityU8)
		Test_assert(t, "ommU8ImpliesOmm", (f & c::EGraphicsFeatures_RayMicromapOpacity) != 0);

	//The RT specific features2 bits are equally meaningless without raytracing itself.

	const c::EGraphicsFeatures2 rayFeatures2 = (c::EGraphicsFeatures2)(
		c::EGraphicsFeatures2_RayClusterAS | c::EGraphicsFeatures2_RayPartitionedTLAS |
		c::EGraphicsFeatures2_RayIndirectASBuild | c::EGraphicsFeatures2_RayReorderActual |
		c::EGraphicsFeatures2_RayMicromapOpacityActual | c::EGraphicsFeatures2_RayMicromapOpacityU8
	);

	if(f2 & rayFeatures2)
		Test_assert(t, "rayFeatures2ImpliesRaytracing", (f & c::EGraphicsFeatures_Raytracing) != 0);

	//One of the two block compression families is mandatory, per the enum's own documentation.

	Test_assert(t, "hasBlockCompression", (d & (c::EGraphicsDataTypes_ASTC | c::EGraphicsDataTypes_BCn)) != 0);

	//Shader models are cumulative, so each bit has to imply the one below it.
	//The D3D12 probe used to satisfy this vacuously by reporting every model the RUNTIME knew.
	//Now that it reads the device's real ceiling, the chain is what catches a probe regressing to that.
	//featuresExt means something else on Vulkan, so only the D3D12 backend is held to it.
	//Which api built this device is only reachable through the device's own instance field, which the wrapper
	// does not expose, and both Ref_ptr helpers are macros naming their result type unqualified, so this pair
	// stays on the C API with the two types pulled into scope for the expansion.

	using c::GraphicsDevice;
	using c::GraphicsInstance;

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (GraphicsInstanceRef_ptr(device->instance)->api == c::EGraphicsApi_Direct3D12) {

		const c::U32 fx = caps.featuresExt;

		if(fx & c::EDxGraphicsFeatures_SM6_10)
			Test_assert(t, "sm610ImpliesSm69", (fx & c::EDxGraphicsFeatures_SM6_9) != 0);

		if(fx & c::EDxGraphicsFeatures_SM6_9)
			Test_assert(t, "sm69ImpliesSm68", (fx & c::EDxGraphicsFeatures_SM6_8) != 0);

		if(fx & c::EDxGraphicsFeatures_SM6_8)
			Test_assert(t, "sm68ImpliesSm67", (fx & c::EDxGraphicsFeatures_SM6_7) != 0);

		if(fx & c::EDxGraphicsFeatures_SM6_7)
			Test_assert(t, "sm67ImpliesSm66", (fx & c::EDxGraphicsFeatures_SM6_6) != 0);
	}

	//Cooperative FP8 and training are refinements of the cooperative types, not standalone features.

	if(d && (f & c::EGraphicsFeatures_CoopVecTraining))
		Test_assert(t, "coopTrainingImpliesCoopVec", (f & c::EGraphicsFeatures_CoopVec) != 0);

	if(f & c::EGraphicsFeatures_CoopFP8)
		Test_assert(t, "coopFP8ImpliesCoopType", (f & (c::EGraphicsFeatures_CoopVec | c::EGraphicsFeatures_CoopMat)) != 0);

	//Subgroup arithmetic and shuffle are specific subgroup operations, so the umbrella bit has to be set.

	if(f & (c::EGraphicsFeatures_SubgroupArithmetic | c::EGraphicsFeatures_SubgroupShuffle))
		Test_assert(t, "subgroupSubsetImpliesOperations", (f & c::EGraphicsFeatures_SubgroupOperations) != 0);

	//Mesh/task derivatives require mesh shaders to exist at all.

	if(f & c::EGraphicsFeatures_MeshTaskTexDeriv)
		Test_assert(t, "meshDerivImpliesMeshShader", (f & c::EGraphicsFeatures_MeshShader) != 0);

	//experimentalFeatures is documented as a subset of features, so a bit can't be experimental without also
	//being reported as present.

	Test_assert(t, "experimentalIsSubset", (caps.experimentalFeatures & ~f) == 0);
	Test_assert(t, "experimental2IsSubset", (caps.experimentalFeatures2 & ~f2) == 0);

	//Memory limits have to be usable numbers, since allocation paths divide and compare against them.
	//maxBufferSize and maxAllocationSize are independent limits, the largest single buffer resource against the
	// largest single memory allocation, so neither bounds the other and only the floor is a real invariant.
	//Device selection refuses anything below 256 MiB on either, so reaching this point guarantees both.

	Test_assert(t, "hasMemory", caps.sharedMemory || caps.dedicatedMemory);
	Test_assert(t, "maxBufferSizeFloor", caps.maxBufferSize >= 256 * c::MIBI);
	Test_assert(t, "maxAllocationSizeFloor", caps.maxAllocationSize >= 256 * c::MIBI);

	// -- The API has to agree with what was reported -------------------------------------------------

	//Each of these reads a capability bit and then exercises the thing it gates, in whichever direction the
	// bit points.
	//A mismatch here means the reported set and the implementation disagree, which is exactly what a per
	// feature test that only runs when its own bit is set can never see.

	const c::CharString probeName = c::CharString_createRefCStrConst("Capability probe");

	//MSAA: a render texture at a sample count the device claims must be creatable, and one it doesn't claim
	// must be refused.
	//Device::createRenderTexture pins EMSAASamples_Off, so the sample count under test can only be spelled
	// through the C entry point; what comes back is adopted so it still frees itself.

	{
		c::RenderTextureRef *rt = nullptr;

		const c::Bool has2x = (d & c::EGraphicsDataTypes_MSAA2x) != 0;

		const c::Bool created2x = c::GraphicsDeviceRef_createRenderTexture(
			deviceRef, c::ETextureType_2D, 4, 4, 1, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None,
			c::EMSAASamples_x2Ext, nullptr, &probeName, &rt, nullptr
		);

		Test_assert(t, "msaa2xMatchesCapability", created2x == has2x);

		RenderTexture owned2x(RefPtr<c::RenderTexture>::adopt(rt));
		rt = nullptr;

		const c::Bool has8x = (d & c::EGraphicsDataTypes_MSAA8x) != 0;

		const c::Bool created8x = c::GraphicsDeviceRef_createRenderTexture(
			deviceRef, c::ETextureType_2D, 4, 4, 1, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None,
			c::EMSAASamples_x8Ext, nullptr, &probeName, &rt, nullptr
		);

		Test_assert(t, "msaa8xMatchesCapability", created8x == has8x);

		RenderTexture owned8x(RefPtr<c::RenderTexture>::adopt(rt));
	}

	//MSAA off is not a capability, it is the baseline, so it must always work.

	{
		RenderTexture rt;

		Test_assert(t, "msaaOffAlwaysWorks", dev.createRenderTexture(
			4, 4, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Capability probe", rt, c::EMSAASamples_Off,
			nullptr, e_rr
		));
	}

	//Bindless: a layout that asks for bindless arrays is only legal when the device reports Bindless.

	{
		const c::Bool hasBindless = (f & c::EGraphicsFeatures_Bindless) != 0;

		//Declared so the layout unwinds before the info it was built from, matching the old teardown order.

		OwnedLayoutInfo info(alloc);
		DescriptorLayout layout;

		info.list.flags = c::EDescriptorLayoutFlags_AllowBindlessOnArrays;

		const c::Bool created = dev.createDescriptorLayout(info.list, "Capability probe", layout, nullptr);

		//An empty binding list may be refused for reasons unrelated to bindless, so this only asserts the one
		// direction that is unambiguous: without the capability it must not succeed.

		if(!hasBindless)
			Test_assert(t, "bindlessRefusedWithoutCapability", !created);
	}

	// -- Coverage report -----------------------------------------------------------------------------

	//Which bits this adapter actually has, so a green run is readable as coverage rather than as a claim that
	// everything was exercised.

	c::U32 featureCount = 0, dataTypeCount = 0;

	for(c::U64 i = 0; i < sizeof(testFeatureBits) / sizeof(testFeatureBits[0]); ++i)
		featureCount += (f & testFeatureBits[i].bit) != 0;

	for(c::U64 i = 0; i < sizeof(testDataTypeBits) / sizeof(testDataTypeBits[0]); ++i)
		dataTypeCount += (d & testDataTypeBits[i].bit) != 0;

	//Log_debugLnx is a macro that names ELogOptions_NewLine unqualified, so it cannot be spelled through the c
	// namespace; oxc::Log is the C++ front for the same call and takes the allocator the x suffix implies.
	//The format string also needs spaces around the PRI macros, which C++11 would otherwise read as
	// user-defined literal suffixes on the string before them.

	Log::debugLn(
		*alloc,
		"-- capabilities: %" PRIu32 "/%" PRIu64 " features, %" PRIu32 "/%" PRIu64 " data types, features2 0x%" PRIx32,
		featureCount, (c::U64)(sizeof(testFeatureBits) / sizeof(testFeatureBits[0])),
		dataTypeCount, (c::U64)(sizeof(testDataTypeBits) / sizeof(testDataTypeBits[0])),
		(c::U32) f2
	);

	//A device reporting nothing at all would make every conditional assert above vacuous, so make that loud.

	Test_assert(t, "reportsAnyFeature", featureCount > 0);
	Test_assert(t, "reportsAnyDataType", dataTypeCount > 0);
}
