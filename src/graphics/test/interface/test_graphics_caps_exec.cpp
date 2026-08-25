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

//graphics/test/interface/test_graphics_caps_exec.cpp
//
//Coverage group C, execution half.
//
//  38. GraphicsDevice/capabilityExec - dispatch a shader per capability and verify what it computed
//
//Module 37 checks that the reported capability set is self consistent.
//It cannot tell whether a capability the device claims actually works,
// because every assertion it could make reads the same bit it is validating.
//This module closes that: each shader below is gated with [[oxc::extension]] so the entrypoint only exists
// for a backend that declared it, and the result is read back and compared, so a capability that is claimed
// but miscompiled fails here rather than passing quietly.
//
//WHAT IS NOT COVERED, AND WHY
//
//A compute dispatch writing one bindless buffer is what this module does, so bits needing another vehicle
// live in their vehicle's module instead: Wireframe, LogicOp and MSAA 2x/4x/8x execute in Shaders/draw,
// stencil bearing depth formats clear and pull in GraphicsDevice/execute, optional texture formats round
// trip in GraphicsDevice/formatRoundTrip, and the SER trace twin lives in Shaders/rays (dormant until a
// backend's claim leaves experimentalFeatures, see below).
//
//Claimed but EXPERIMENTAL, so execution is skipped with a log rather than attempted:
//  RayTriPosition on D3D12 (proxied off preview SM6.10; the entry runs on Vulkan).
//  RayReorder everywhere: on Vulkan the NV device extension can't accept the EXT SPIR-V the shader stack
//   deliberately emits, and on D3D12 the SM6.9 dx::HitObject needs is preview only.
//
//Not shader visible at all:
//  LUID (asserted host side in the interface module), RayValidation, SwapchainCompute (a swapchain creation
//  flag), RayIndirectASBuild (a host side build command), RayClusterAS and RayPartitionedTLAS (detected
//  only, no engine API).
//
//Blocked by the engine rather than by any test, and worth fixing:
//  AtomicF32 / AtomicF64 - the helpers take their target by [[vk::ext_reference]] and need a float lvalue,
//   which the bindless layout has none of; see the note at the end of the table.
//  WriteMSTexture - needs RWTexture2DMS, which the bindless layout doesn't carry.
//  DescriptorHeap - the extension and the compile path exist, but there is no runtime heap path yet.
//  Multiview - no viewMask is ever set on the render or the pipeline, so every render is single view.
//  DualSrcBlend - the second source must be Location 0 Index 1 on SPIR-V, and neither the reflection
//   (which keys outputs on location alone) nor oiSH can represent that yet; plain MRT works and is covered.
//  RayMicromapOpacity - stage 1 (special indices only) is now ATTACHABLE: BLASCreateInfo carries
//   ommIndexFormat/ommIndexBuffer, BLASCreateInfo_indexedWithOmmIndicesExt builds one, vk_blas.c chains
//   VkAccelerationStructureTrianglesOpacityMicromapEXT with a null micromap handle and dx_blas.c switches the
//   geometry to OMM_TRIANGLES with a null micromap array.
//   Execution is still NOT covered here: that needs the allow OMM opt-in on the pipeline and
//   RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS on the shader side, and the ray must not use FORCE_OPAQUE or the
//   micromap is bypassed and the test would pass for the wrong reason.
//   The honest test remains BINARY: a fully transparent special index makes a ray MISS where it would
//   otherwise hit, which is exact, unlike ray T which is not bit exact across vendors.
//  Raytracing - an umbrella bit with no ESHExtension; RayQuery and RayPipeline are the concrete bits.
//  CoopVec / CoopMat / CoopFP8 / CoopVecTraining - need buffer types the bindless layout lacks; compile
//   coverage lives in the shader compiler's feature corpus.

//The shared helpers in terms of the handle types. Both C++ headers come BEFORE the block below: a
//standard header included after the C headers landed in oxc::c finds its guard already tripped and
//leaves its symbols in that namespace.

#include "test_graphics_shared.hpp"

//Log::debugLn is the C++ front for Log_debugLnx; the x macros name ELogOptions_NewLine unqualified and
//so cannot be reached through the c namespace.

#include "types/container/log.hpp"

namespace oxc { namespace c {
	#include "types/base/error.h"
	#include "types/container/buffer.h"
	#include "types/container/string.h"
	#include "types/test/test.h"
	#include "formats/oiSH/sh_registers.h"
	#include "platforms/logx.h"
	#include "platforms/platform.h"
	#include "graphics/generic/bindless_descriptor.h"
	#include "graphics/generic/blas.h"
	#include "graphics/generic/command_list.h"
	#include "graphics/generic/commands.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_buffer.h"
	#include "graphics/generic/device_info.h"
	#include "graphics/generic/instance.h"
	#include "graphics/generic/pipeline.h"
	#include "graphics/generic/tlas.h"
	#include "test_graphics_shared.h"
} }

using namespace oxc;

//Same namespace the C headers landed in, so the definitions here match the declarations in
//test_graphics_shared.h and the macros in those headers still expand to names that resolve.

//Each capability under test: the oiSH to load, the device bit that gates it, and the extension the
//entrypoint was compiled with.
//Keeping the device bit and the shader extension side by side is the point of the table: they are two
// independent statements about the same capability, and this module exists to check they agree.

//One checked word of the readback.
//Checking by offset rather than as a contiguous block matters because a shader is free to leave words it
// never writes untouched, and asserting on those would be asserting on whatever the allocation happened to
// hold.

typedef struct TestCapabilityWord {
	c::U32 offset;                             //Byte offset into the readback
	c::U32 value;                              //Expected U32 there
} TestCapabilityWord;

#define TEST_CAPS_MAX_WORDS 4

typedef struct TestCapabilityShader {

	const c::C8 *path;
	const c::C8 *name;

	c::EGraphicsFeatures feature;              //Bit in capabilities.features, or 0 if gated on dataTypes
	c::EGraphicsDataTypes dataType;            //Bit in capabilities.dataTypes, or 0 if gated on features

	c::ESHExtension require;                   //Extension the entrypoint declares

	TestCapabilityWord expected[TEST_CAPS_MAX_WORDS];
	c::U8 expectedCount;                       //How many of the words above are checked

	c::Bool spirvOnly;                         //No DXIL variant exists, so D3D12 has nothing to run
	c::Bool needsTlas;                         //Wants the shared TLAS handle in push constant slot 1

} TestCapabilityShader;

//1 + 2 + ... + 64, the total every thread contributing exactly once must produce.

#define TEST_CAPS_SUM_64 2080

static const TestCapabilityShader testCapabilityShaders[] = {

	//64 threads atomically increment a counter.
	//A lost update lands short of the sum.
	//The 64 bit store at offset 8 is checked as its two halves: 0x1FFFFFFFE is deliberately past the 32 bit
	// boundary, so a truncating store leaves the high word at 0 and is caught rather than merely suspected.
	//
	//What this does NOT cover is the 64 bit atomic itself, despite the name of the bit it is gated on.
	//The counter is incremented with a 32 bit InterlockedAdd,
	// so a device whose 64 bit atomic unit is absent or emulated passes this unchanged.
	//That is a DXC gap rather than a choice: its SPIR-V backend answers InterlockedAdd64 with "intrinsic
	// 'InterlockedAdd64' method unimplemented", so the 64 bit atomic can only be spelled for DXIL today.
	//See build/reports/dxc_issue_3_interlockedadd64_spirv.md.
	//It stays gated on AtomicI64 because the entrypoint declares that extension, so the lookup needs it.
	{
		"//OxC3_gtest/test_shaders/test_caps_atomics.oiSH", "atomicI64",
		(c::EGraphicsFeatures) (0), (c::EGraphicsDataTypes) (c::EGraphicsDataTypes_AtomicI64), c::ESHExtension_AtomicI64,
		{ { 0, TEST_CAPS_SUM_64 }, { 8, 0xFFFFFFFEu }, { 12, 0x00000001u } }, 3,
		false, false
	},

	//Plain 64 bit integer maths, gated only on I64 so it still runs on a device that has the type without the
	// atomic, which the entry above skips entirely.
	{
		"//OxC3_gtest/test_shaders/test_caps_i64.oiSH", "i64",
		(c::EGraphicsFeatures) (0), (c::EGraphicsDataTypes) (c::EGraphicsDataTypes_I64), c::ESHExtension_I64,
		{ { 0, TEST_CAPS_SUM_64 } }, 1,
		false, false
	},

	//Doubles: 2^24 + k round trips only at 64 bit width, so an F32 computation lands on the sum of the even k.
	//Gated on I64 as well as F64, matching the entrypoint's two extensions: reading the double back as an
	// integer is what checks it, and DXC's SPIR-V backend lowers that conversion through a 64 bit integer, so
	// the module genuinely uses Int64 and has to declare it.
	{
		"//OxC3_gtest/test_shaders/test_caps_f64.oiSH", "f64",
		(c::EGraphicsFeatures) (0),
		(c::EGraphicsDataTypes) (c::EGraphicsDataTypes_F64 | c::EGraphicsDataTypes_I64), c::ESHExtension_F64,
		{ { 0, TEST_CAPS_SUM_64 } }, 1,
		false, false
	},

	//Halves: three predicates bracket the significand from both sides without depending on a rounding mode.
	{
		"//OxC3_gtest/test_shaders/test_caps_f16.oiSH", "f16",
		(c::EGraphicsFeatures) (0), (c::EGraphicsDataTypes) (c::EGraphicsDataTypes_F16), c::ESHExtension_16BitTypes,
		{ { 0, TEST_CAPS_SUM_64 }, { 4, 64 }, { 8, 64 } }, 3,
		false, false
	},

	//Derivatives in compute, which need the stage to form 2x2 quads out of the thread group.
	//A 2x2 group keeps DXC on the quad derivative path rather than the linear one.
	//Both map to this same extension, so either would be accepted, but the quad path is the one worth exercising.
	{
		"//OxC3_gtest/test_shaders/test_caps_computederiv.oiSH", "computeDeriv",
		(c::EGraphicsFeatures) (c::EGraphicsFeatures_ComputeDeriv), (c::EGraphicsDataTypes) (0),
		c::ESHExtension_ComputeDeriv,
		{ { 0, 4 } }, 1,
		false, false
	},

	//Shuffle across lane ^ 1, checked per lane rather than summed, since XOR 1 is a permutation and a sum
	// would be identical whether the shuffle moved anything or not.
	//128 threads because WaveReadLaneAt may only name an active lane and a 64 thread group half fills a 128
	// wide wave.
	{
		"//OxC3_gtest/test_shaders/test_caps_subgroup_shuffle.oiSH", "subgroupShuffle",
		(c::EGraphicsFeatures) (c::EGraphicsFeatures_SubgroupShuffle | c::EGraphicsFeatures_SubgroupOperations),
		(c::EGraphicsDataTypes) (0),
		c::ESHExtension_SubgroupShuffle,
		{ { 0, 128 } }, 1,
		false, false
	},

	//Wave wide reduction, accumulated once per wave.
	//A reduction that didn't cross the wave lands wrong.
	//Both feature bits are required, matching the shader's two extensions: WaveActiveSum needs
	// SubgroupArithmetic and WaveIsFirstLane needs SubgroupOperations.
	//Runs on both backends: WaveActiveSum is plain HLSL, so DXIL compiles it fine.
	//DXIL reflection just can't DETECT the extension (absent from ESHExtension_DxilNative), so on DXIL it is
	// annotation-driven rather than demotable, exactly like the shuffle entry above.
	{
		"//OxC3_gtest/test_shaders/test_caps_subgroup.oiSH", "subgroupArithmetic",
		(c::EGraphicsFeatures) (c::EGraphicsFeatures_SubgroupArithmetic | c::EGraphicsFeatures_SubgroupOperations),
		(c::EGraphicsDataTypes) (0),
		c::ESHExtension_SubgroupArithmetic,
		{ { 0, TEST_CAPS_SUM_64 } }, 1,
		false, false
	},

	//Ballot bit count per wave, natively detectable on both backends unlike the two entries above.
	//Every lane is active, so the counts sum to the thread count rather than the wave count.
	{
		"//OxC3_gtest/test_shaders/test_caps_subgroup_ops.oiSH", "subgroupOperations",
		(c::EGraphicsFeatures) (c::EGraphicsFeatures_SubgroupOperations), (c::EGraphicsDataTypes) (0),
		c::ESHExtension_SubgroupOperations,
		{ { 0, 64 } }, 1,
		false, false
	},

	//Inline raytracing against a two instance TLAS: hit instance 0, hit instance 1, then miss both.
	//The middle word is the regression guard for vk_tlas.c writing every instance's acceleration structure
	// reference into instance 0's slot, which left instance 1 unreachable.
	{
		"//OxC3_gtest/test_shaders/test_caps_rayquery.oiSH", "rayQuery",
		(c::EGraphicsFeatures) (c::EGraphicsFeatures_RayQuery), (c::EGraphicsDataTypes) (0),
		c::ESHExtension_RayQuery,
		{ { 0, 1 }, { 4, 1 }, { 8, 0 } }, 3,
		false, true
	},

	//Triangle vertex position fetch: both threads must read back the exact object space vertices the BLAS was built from.
	//Bit exact comparison is legitimate for once, since this is stored data rather than a computed quantity.
	//Thread 1 traces the TRANSLATED instance and still has to see the untranslated positions, which is what
	// separates object space from world space.
	{
		"//OxC3_gtest/test_shaders/test_caps_raytriposition.oiSH", "rayTriPosition",
		(c::EGraphicsFeatures) (c::EGraphicsFeatures_RayQuery | c::EGraphicsFeatures_RayTriPosition), (c::EGraphicsDataTypes) (0),
		c::ESHExtension_RayTriPosition,
		{ { 0, 1 }, { 4, 1 } }, 2,
		false, true
	}

	//AtomicF32 and AtomicF64 have no entry here on purpose, and it is a gap in the engine rather than in the
	// test.
	//oxc::AtomicAddF32 takes its target by [[vk::ext_reference]], so it needs a float lvalue, and the
	// bindless layout in resources.hlsli only exposes RWByteAddressBuffer, which yields none.
	//Targeting groupshared instead does compile, but that is the Workgroup storage class, and the device only
	// enables shaderBufferFloat32AtomicAdd, so the validation layers reject the module at create time.
	//The compile path is already covered by shader_compiler/test/features/atomic_f32.hlsl, which uses the
	// RWStructuredBuffer<float> form the helper documents.
	//Only execution is unreachable, and it stays that way until the bindless layout carries a typed float buffer.
};

//Runs one capability shader and compares the first U32 it wrote.
//Returns whether the shader ran at all, so the caller can tell "verified" apart from "device lacks it".

//Builds the single triangle the tracing entries aim at, and the acceleration structures over it.
//The geometry matches test_graphics_shaders.c's ray pipeline test on purpose, so an inline trace and a
// pipeline trace are aiming at the same thing and a disagreement between them means something.
//Everything is left NULL on failure, which the caller reads as "no tracing this run".

static void Test_buildCapabilityTlas(
	c::Test *t,
	gfx::Device &dev,
	c::Bool forceNoDataAccess,        //Deliberately unflagged BLAS, for the negative RayTriPosition guard test
	gfx::DeviceBuffer &positions,
	gfx::Blas &blas,
	gfx::Tlas &tlas
) {

	c::Error *e_rr = &t->err;

	const c::F32 triangle[12] = {
		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1
	};

	c::Buffer triData = c::Buffer_createRefConst(triangle, sizeof(triangle));

	if(!Test_assert(t, "capPositions", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
		"Capability trace positions", &triData, positions, nullptr, e_rr
	)))
		return;

	const c::DeviceData positionData = { .buffer = positions.handle() };

	//Position fetch is a build time opt in: without AllowDataAccess the vertex data simply isn't kept in the
	// acceleration structure, so the flag rides along whenever the device claims the capability.

	const c::GraphicsDeviceCapabilities blasCaps = dev.info().capabilities;

	const c::ERTASBuildFlags blasFlags =
		(blasCaps.features & c::EGraphicsFeatures_RayTriPosition) &&
		!(blasCaps.experimentalFeatures & c::EGraphicsFeatures_RayTriPosition) &&
		!forceNoDataAccess
		? c::ERTASBuildFlags_AllowDataAccessExt : c::ERTASBuildFlags_None;

	const c::BLASCreateInfo blasInfo = c::BLASCreateInfo_unindexed(
		blasFlags, c::EBLASFlag_None, c::ETextureFormatId_RGBA32f, 0, 16, positionData
	);

	if(!Test_assert(t, "capBlas", dev.createBlas(blasInfo, "Capability trace BLAS", blas, e_rr)))
		return;

	//Two instances of the one BLAS: the second translated +2 along X, so a ray aimed there can only hit if
	// that instance got its own acceleration structure reference.
	//Every other test in the suite uses a single instance, which is why a backend writing all references into
	// instance 0's slot went unnoticed.

	const c::TLASInstance instance[2] = {
		{
			.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
			.data = {
				.instanceId24_mask8 = 0xFFu << 24,
				.sbtOffset24_flags8 = (c::U32) c::ETLASInstanceFlag_Default << 24,
				.blasCpu = blas.handle()
			}
		},
		{
			.transform = { { 1, 0, 0, 2 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
			.data = {
				.instanceId24_mask8 = 0xFFu << 24,
				.sbtOffset24_flags8 = (c::U32) c::ETLASInstanceFlag_Default << 24,
				.blasCpu = blas.handle()
			}
		}
	};

	if(!Test_assert(t, "capTlas", dev.createTlas(
		c::ERTASBuildFlags_DefaultTLAS, instance, 2, "Capability trace TLAS", tlas, false, e_rr
	)))
		return;

	//Creating them only declares them.
	//The build has to run on the device before anything can trace against it.

	//Each build gets its own scope, and the TLAS scope is numbered after the BLAS one, because the TLAS reads
	// what the BLAS build produced and the backend derives that dependency from the scope ids.

	gfx::CommandList buildList;

	if(!(
		Test_assert(t, "capAsList", dev.createCommandList(2 * c::KIBI, 64, 16, buildList, true, e_rr)) &&
		Test_assert(t, "capAsBegin", buildList.begin(true, e_rr))
	)) {
		tlas.release();
		return;
	}

	{
		gfx::CommandScope scope = buildList.scope({}, 1, {}, e_rr);
		Test_assert(t, "capAsBlasScope", (c::Bool) scope);
		Test_assert(t, "capAsBlas", scope.updateBlas(blas, e_rr));
		Test_assert(t, "capAsBlasScopeEnd", scope.end(e_rr));
	}

	{
		gfx::CommandScope scope = buildList.scope({}, 2, {}, e_rr);
		Test_assert(t, "capAsTlasScope", (c::Bool) scope);
		Test_assert(t, "capAsTlas", scope.updateTlas(tlas, e_rr));
		Test_assert(t, "capAsTlasScopeEnd", scope.end(e_rr));
	}

	if(!(
		Test_assert(t, "capAsEnd", buildList.end(e_rr)) &&
		gfxtest::submitAndWait(t, dev, buildList) &&
		Test_assert(t, "capTlasHandle", tlas.bindlessHandle() != c::BindlessDescriptor_None)
	))
		tlas.release();
}

static c::Bool Test_runCapabilityShader(
	c::Test *t,
	gfx::Device &dev,
	const gfx::CommandList &emptyList,
	const TestCapabilityShader *cap,
	const gfx::Tlas &tlas,
	c::Bool *ranOut
) {

	c::Error *e_rr = &t->err;
	const c::GraphicsDeviceCapabilities caps = dev.info().capabilities;

	*ranOut = false;

	//Whether the device claims this capability at all.
	//Every bit has to be present rather than any of them, since a shader declaring two extensions needs both;
	// gating on either would send a device missing one of them into a lookup that can only fail.

	const c::Bool hasFeatures =
		!cap->feature || (caps.features & cap->feature) == cap->feature;

	const c::Bool hasDataTypes =
		!cap->dataType || (caps.dataTypes & cap->dataType) == cap->dataType;

	const c::Bool supported = (cap->feature || cap->dataType) && hasFeatures && hasDataTypes;

	if(!supported)
		return true;

	//A capability the backend itself marks experimental is claimed but not held to: the claim rides on a
	// preview stack (D3D12 proxies RayTriPosition off preview SM6.10, for instance), and WARP's preview
	// pipeline genuinely fastfails under GBV when asked to run it.
	//Skipping is honest here, since the skip is logged and the bit leaves experimentalFeatures the moment the
	// real capability query is wired, at which point this line stops matching and the entry runs.

	if (caps.experimentalFeatures & cap->feature) {
		Log::debugLn(
			*dev.alloc(), "-- capabilityExec: %s claimed but experimental on this backend, skipped", cap->name
		);
		return true;
	}

	//An entrypoint with no variant for this backend can only fail the lookup, and checkShaderFeatures logs a
	// device error on the way out.
	//That error is harmless here, but it reads like a real failure in the test output,
	// so decide up front rather than probing and letting the engine complain.

	const c::Bool isSpirvDevice = dev.api() != c::EGraphicsApi_Direct3D12;

	if(cap->spirvOnly && !isSpirvDevice)
		return true;

	gfxtest::OwnedSHFile file(dev.alloc());

	if(!gfxtest::loadFile(t, cap->path, file.list))
		return true;

	c::Bool ok = true;
	gfx::DeviceBuffer output;
	gfx::Pipeline pipeline;
	gfx::PipelineLayout pipelineLayout;
	gfx::CommandList commandList;

	//The entrypoint is only present when compiled with the extension, so this doubles as the check that the
	// shader really was built for the capability rather than silently falling back.

	const c::U32 entryId = dev.getFirstShaderEntry(file.list, "main", c::ESHExtension_None, cap->require);

	if (entryId == c::U32_MAX) {

		//No entrypoint for this backend's binary type.
		//That is legitimate rather than a failure:
		// an entrypoint restricted with [[oxc::binary]] only exists for the backend it named,
		// and the device consuming the other one has nothing to run.

		Log::debugLn(
			*dev.alloc(), "-- capabilityExec: %s claimed but no entrypoint for this backend, skipped", cap->name
		);

		return true;
	}

	ok &= Test_assert(t, "capOutput", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		//ShaderWriteBindless rather than ShaderWrite: the shaders index the buffer through its bindless write handle,
		// and only the bindless flag allocates one.
		//Plain ShaderWrite leaves writeHandle at 0,
		// which makes every shader write land on descriptor 0 and the readback come back untouched.

		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWriteBindless | c::EGraphicsResourceFlag_CPUBacked),
		"Capability output", 16, output, nullptr, e_rr
	));

	if(ok) {

		ok = ok && gfxtest::pushConstantLayout(t, dev, file.list, entryId, pipelineLayout);

		ok = ok && Test_assert(t, "capPipeline", dev.createComputePipeline(
			file.list, "main", "Capability pipeline", pipeline, {}, &pipelineLayout, e_rr
		));
	}

	if (ok) {

		ok &= Test_assert(t, "capList", dev.createCommandList(2 * c::KIBI, 64, 16, commandList, true, e_rr));
		ok = ok && Test_assert(t, "capBegin", commandList.begin(true, e_rr));

		//The dispatch has to sit in a scope that declares the write, or the backend has no barrier to insert
		// and refuses the bind.

		//A tracing entry reads the TLAS as well as writing the output, and the scope has to say so or the
		// backend has no barrier to put the acceleration structure into a readable state.

		const c::Transition outputWrite = {
			.resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
		};

		const c::Transition tlasRead = {
			.resource = tlas.handle(), .stage = c::EPipelineStage_Compute
		};

		//The push block: the output handle at slot 0, and the shared TLAS at slot 1
		//for the entries that trace against it, matching what the shaders read.

		const c::U32 pushData[4] = {
			output.writeHandle(),
			tlas.valid() ? tlas.bindlessHandle() : 0,
			0, 0
		};

		if (ok) {

			gfx::CommandScope scope =
				cap->needsTlas ?
				commandList.scope({ outputWrite, tlasRead }, 1, {}, e_rr) :
				commandList.scope({ outputWrite }, 1, {}, e_rr);

			ok &= Test_assert(t, "capScope", (c::Bool) scope);
			ok &= Test_assert(t, "capBind", scope.setComputePipeline(pipeline, e_rr));
			ok &= Test_assert(t, "capPush", scope.setPushConstants(pushData, e_rr));
			ok &= Test_assert(t, "capDispatch", scope.dispatch1D(1, e_rr));
			ok &= Test_assert(t, "capScopeEnd", scope.end(e_rr));
			ok &= Test_assert(t, "capEnd", commandList.end(e_rr));
		}
	}

	if (ok) {

		ok &= gfxtest::submitAndWait(t, dev, commandList);
		ok &= gfxtest::pullBuffer(t, dev, emptyList, output);
	}

	if (ok) {

		const c::DeviceBuffer *outputPtr = output.data();

		for (c::U8 w = 0; w < cap->expectedCount; ++w) {

			const TestCapabilityWord word = cap->expected[w];

			//A word past the readback is a broken table entry rather than a broken device, so it fails here
			// instead of silently comparing against a zero that was never written.

			if(!Test_assert(t, "capResultInRange", c::Buffer_length(outputPtr->cpuData) >= (c::U64)word.offset + 4)) {
				ok = false;
				break;
			}

			const c::U32 got = *(const c::U32*)(outputPtr->cpuData.ptr + word.offset);

			ok &= Test_assert(t, "capResult", got == word.value);

			if(got != word.value)
				Log::debugLn(
					*dev.alloc(),
					"-- capabilityExec: %s at +%" PRIu32 " expected 0x%08X, got 0x%08X",
					cap->name, word.offset, word.value, got
				);
		}

		*ranOut = ok;
	}

	return ok;
}

//The negative side of the RayTriPosition record time validation: a pipeline that declares position fetch,
// dispatched in a scope whose only transitioned TLAS provably contains a BLAS built without
// ERTASBuildFlags_AllowDataAccessExt, has to be refused when the dispatch is recorded.
//The positive side needs no extra test, since the rayTriPosition entry above dispatches through the same
// check against the properly flagged TLAS.

static void Test_capabilityRayTriPositionGuard(c::Test *t, gfx::Device &dev) {

	c::Error *e_rr = &t->err;
	const c::GraphicsDeviceCapabilities caps = dev.info().capabilities;

	//Same gates as the positive entry: claimed, not experimental, and RayQuery for the inline trace.

	const c::EGraphicsFeatures needed =
		(c::EGraphicsFeatures) (c::EGraphicsFeatures_RayQuery | c::EGraphicsFeatures_RayTriPosition);

	if((caps.features & needed) != needed || (caps.experimentalFeatures & c::EGraphicsFeatures_RayTriPosition))
		return;

	gfx::DeviceBuffer positions, output;
	gfx::Blas blas;
	gfx::Tlas tlas;

	Test_buildCapabilityTlas(t, dev, true, positions, blas, tlas);

	if(!Test_assert(t, "rtpGuardTlas", tlas.valid()))
		return;

	//The cached bits are what the record time check reads, so they are pinned down here as well.

	Test_assert(t, "rtpGuardKnown", c::TLAS_hasFlag(tlas.data(), c::ETLASFlag_BlasDataAccessKnown));
	Test_assert(t, "rtpGuardAllOff", !c::TLAS_hasFlag(tlas.data(), c::ETLASFlag_BlasDataAccessAll));

	gfxtest::OwnedSHFile file(dev.alloc());

	if(!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_caps_raytriposition.oiSH", file.list))
		return;

	const c::U32 entryId = dev.getFirstShaderEntry(
		file.list, "main", c::ESHExtension_None, c::ESHExtension_RayTriPosition
	);

	if(!Test_assert(t, "rtpGuardEntry", entryId != c::U32_MAX))
		return;

	//The shader reads its output handle from a push constant, and a range the layout doesn't declare is
	//rejected at pipeline creation, so the layout is detected here like everywhere else.

	gfx::PipelineLayout pipelineLayout;

	if(!gfxtest::pushConstantLayout(t, dev, file.list, entryId, pipelineLayout))
		return;

	gfx::Pipeline pipeline;
	gfx::CommandList commandList;

	if(!(
		Test_assert(t, "rtpGuardPipeline", dev.createComputePipeline(
			file.list, "main", "RTP guard pipeline", pipeline, {}, &pipelineLayout, e_rr
		)) &&
		Test_assert(t, "rtpGuardOutput", dev.createBuffer(
			c::EDeviceBufferUsage_None,
			(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWriteBindless | c::EGraphicsResourceFlag_CPUBacked),
			"RTP guard output", 16, output, nullptr, e_rr
		)) &&
		Test_assert(t, "rtpGuardList", dev.createCommandList(2 * c::KIBI, 64, 16, commandList, true, e_rr)) &&
		Test_assert(t, "rtpGuardBegin", commandList.begin(true, e_rr))
	))
		return;

	const c::Transition transitions[2] = {
		{ .resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true },
		{ .resource = tlas.handle(),   .stage = c::EPipelineStage_Compute }
	};

	gfx::CommandScope scope = commandList.scope({ transitions[0], transitions[1] }, 1, {}, e_rr);

	if(!(
		Test_assert(t, "rtpGuardScope", (c::Bool) scope) &&
		Test_assert(t, "rtpGuardBind", scope.setComputePipeline(pipeline, e_rr))
	))
		return;

	//The dispatch itself is what has to fail, since the only TLAS this scope can trace provably lacks the flag.

	Test_assert(t, "rtpGuardRejects", !scope.dispatch1D(1, nullptr));
}

//38. A shader per capability, dispatched and its results verified.

extern "C" void Test_graphicsCapabilityExecution(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Error *e_rr = &t->err;

	c::Test_setModule(t, "GraphicsDevice/capabilityExec");

	gfx::Device dev = gfx::Device::share(deviceRef);
	gfx::CommandList emptyList;

	if(!(
		Test_assert(t, "createEmptyList", dev.createCommandList(2 * c::KIBI, 64, 16, emptyList, true, e_rr)) &&
		Test_assert(t, "beginEmptyList", emptyList.begin(true, e_rr)) &&
		Test_assert(t, "endEmptyList", emptyList.end(e_rr))
	))
		return;

	//One TLAS shared by every entry that traces, built here rather than per entry so the acceleration
	// structure build isn't repeated for each capability that happens to want one.
	//It only exists where the device claims RayQuery.
	//Entries that need it are skipped otherwise.

	gfx::DeviceBuffer positions;
	gfx::Blas blas;
	gfx::Tlas tlas;

	if(dev.info().capabilities.features & c::EGraphicsFeatures_RayQuery)
		Test_buildCapabilityTlas(t, dev, false, positions, blas, tlas);

	c::U32 verified = 0, skipped = 0;

	for (c::U64 i = 0; i < sizeof(testCapabilityShaders) / sizeof(testCapabilityShaders[0]); ++i) {

		c::Bool ran = false;

		//An entry that traces has nothing to trace against without the TLAS, so it doesn't run at all rather
		// than dispatching with a handle that points nowhere.

		if(testCapabilityShaders[i].needsTlas && !tlas.valid()) {
			++skipped;
			continue;
		}

		Test_runCapabilityShader(t, dev, emptyList, &testCapabilityShaders[i], tlas, &ran);

		if(ran)
			++verified;

		else ++skipped;
	}

	//Says which capabilities were actually executed, so a green run isn't read as "all of them".

	Log::debugLn(
		*dev.alloc(),
		"-- capabilityExec: %" PRIu32 " capabilities executed and verified, %" PRIu32 " not run",
		verified, skipped
	);

	Test_capabilityRayTriPositionGuard(t, dev);

	//Everything above is still in flight until the device is idle, and the handles here release in reverse
	// declaration order once this returns, so the wait has to come first.

	(void) dev.wait(nullptr);
}
