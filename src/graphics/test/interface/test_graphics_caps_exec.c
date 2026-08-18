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

//graphics/test/interface/test_graphics_caps_exec.c
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
//  RayMicromapOpacity - the engine cannot BUILD or attach an opacity micromap: detection and device
//   enablement are fully wired on both backends, but BLAS has no OMM member, no create entry point takes
//   one, and neither backend ever sets the OMM geometry type (vk_blas.c never chains the triangles pNext,
//   dx_blas.c never uses OMM_TRIANGLES).
//   Until an attach path exists there is no OMM in any scene to observe.
//   When it lands, the honest test is BINARY: an OMM marking a region fully transparent makes a ray MISS
//   where it would otherwise hit, which is exact, unlike ray T which is not bit exact across vendors.
//   Smallest useful step: a special index buffer variant of createBLASExt
//   (no micromap object, no OC1 encoding) is enough for that test.
//  RayMotionBlur - no HLSL surface for motion traces.
//  Raytracing - an umbrella bit with no ESHExtension; RayQuery and RayPipeline are the concrete bits.
//  CoopVec / CoopMat / CoopFP8 / CoopVecTraining - need buffer types the bindless layout lacks; compile
//   coverage lives in the shader compiler's feature corpus.

#include "types/test/test.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/base/error.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/device_info.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/bindless_descriptor.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/commands.h"
#include "graphics/generic/pipeline.h"
#include "test_graphics_shared.h"

//Each capability under test: the oiSH to load, the device bit that gates it, and the extension the
//entrypoint was compiled with.
//Keeping the device bit and the shader extension side by side is the point of the table: they are two
// independent statements about the same capability, and this module exists to check they agree.

//One checked word of the readback.
//Checking by offset rather than as a contiguous block matters because a shader is free to leave words it
// never writes untouched, and asserting on those would be asserting on whatever the allocation happened to
// hold.

typedef struct TestCapabilityWord {
	U32 offset;                             //Byte offset into the readback
	U32 value;                              //Expected U32 there
} TestCapabilityWord;

#define TEST_CAPS_MAX_WORDS 4

typedef struct TestCapabilityShader {

	const C8 *path;
	const C8 *name;

	EGraphicsFeatures feature;              //Bit in capabilities.features, or 0 if gated on dataTypes
	EGraphicsDataTypes dataType;            //Bit in capabilities.dataTypes, or 0 if gated on features

	ESHExtension require;                   //Extension the entrypoint declares

	TestCapabilityWord expected[TEST_CAPS_MAX_WORDS];
	U8 expectedCount;                       //How many of the words above are checked

	Bool spirvOnly;                         //No DXIL variant exists, so D3D12 has nothing to run
	Bool needsTlas;                         //Wants the shared TLAS handle in app data slot 1

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
		0, EGraphicsDataTypes_AtomicI64, ESHExtension_AtomicI64,
		{ { 0, TEST_CAPS_SUM_64 }, { 8, 0xFFFFFFFEu }, { 12, 0x00000001u } }, 3,
		false, false
	},

	//Plain 64 bit integer maths, gated only on I64 so it still runs on a device that has the type without the
	// atomic, which the entry above skips entirely.
	{
		"//OxC3_gtest/test_shaders/test_caps_i64.oiSH", "i64",
		0, EGraphicsDataTypes_I64, ESHExtension_I64,
		{ { 0, TEST_CAPS_SUM_64 } }, 1,
		false, false
	},

	//Doubles: 2^24 + k round trips only at 64 bit width, so an F32 computation lands on the sum of the even k.
	//Gated on I64 as well as F64, matching the entrypoint's two extensions: reading the double back as an
	// integer is what checks it, and DXC's SPIR-V backend lowers that conversion through a 64 bit integer, so
	// the module genuinely uses Int64 and has to declare it.
	{
		"//OxC3_gtest/test_shaders/test_caps_f64.oiSH", "f64",
		0, EGraphicsDataTypes_F64 | EGraphicsDataTypes_I64, ESHExtension_F64,
		{ { 0, TEST_CAPS_SUM_64 } }, 1,
		false, false
	},

	//Halves: three predicates bracket the significand from both sides without depending on a rounding mode.
	{
		"//OxC3_gtest/test_shaders/test_caps_f16.oiSH", "f16",
		0, EGraphicsDataTypes_F16, ESHExtension_16BitTypes,
		{ { 0, TEST_CAPS_SUM_64 }, { 4, 64 }, { 8, 64 } }, 3,
		false, false
	},

	//Derivatives in compute, which need the stage to form 2x2 quads out of the thread group.
	//A 2x2 group keeps DXC on the quad derivative path rather than the linear one.
	//Both map to this same extension, so either would be accepted, but the quad path is the one worth exercising.
	{
		"//OxC3_gtest/test_shaders/test_caps_computederiv.oiSH", "computeDeriv",
		EGraphicsFeatures_ComputeDeriv, 0,
		ESHExtension_ComputeDeriv,
		{ { 0, 4 } }, 1,
		false, false
	},

	//Shuffle across lane ^ 1, checked per lane rather than summed, since XOR 1 is a permutation and a sum
	// would be identical whether the shuffle moved anything or not.
	//128 threads because WaveReadLaneAt may only name an active lane and a 64 thread group half fills a 128
	// wide wave.
	{
		"//OxC3_gtest/test_shaders/test_caps_subgroup_shuffle.oiSH", "subgroupShuffle",
		EGraphicsFeatures_SubgroupShuffle | EGraphicsFeatures_SubgroupOperations, 0,
		ESHExtension_SubgroupShuffle,
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
		EGraphicsFeatures_SubgroupArithmetic | EGraphicsFeatures_SubgroupOperations, 0,
		ESHExtension_SubgroupArithmetic,
		{ { 0, TEST_CAPS_SUM_64 } }, 1,
		false, false
	},

	//Ballot bit count per wave, natively detectable on both backends unlike the two entries above.
	//Every lane is active, so the counts sum to the thread count rather than the wave count.
	{
		"//OxC3_gtest/test_shaders/test_caps_subgroup_ops.oiSH", "subgroupOperations",
		EGraphicsFeatures_SubgroupOperations, 0,
		ESHExtension_SubgroupOperations,
		{ { 0, 64 } }, 1,
		false, false
	},

	//Inline raytracing against a two instance TLAS: hit instance 0, hit instance 1, then miss both.
	//The middle word is the regression guard for vk_tlas.c writing every instance's acceleration structure
	// reference into instance 0's slot, which left instance 1 unreachable.
	{
		"//OxC3_gtest/test_shaders/test_caps_rayquery.oiSH", "rayQuery",
		EGraphicsFeatures_RayQuery, 0,
		ESHExtension_RayQuery,
		{ { 0, 1 }, { 4, 1 }, { 8, 0 } }, 3,
		false, true
	},

	//Triangle vertex position fetch: both threads must read back the exact object space vertices the BLAS was built from.
	//Bit exact comparison is legitimate for once, since this is stored data rather than a computed quantity.
	//Thread 1 traces the TRANSLATED instance and still has to see the untranslated positions, which is what
	// separates object space from world space.
	{
		"//OxC3_gtest/test_shaders/test_caps_raytriposition.oiSH", "rayTriPosition",
		EGraphicsFeatures_RayQuery | EGraphicsFeatures_RayTriPosition, 0,
		ESHExtension_RayTriPosition,
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
	Test *t,
	GraphicsDeviceRef *deviceRef,
	Bool forceNoDataAccess,        //Deliberately unflagged BLAS, for the negative RayTriPosition guard test
	DeviceBufferRef **positionsOut,
	BLASRef **blasOut,
	TLASRef **tlasOut
) {

	DeviceBufferRef *positions = NULL;
	BLASRef *blas = NULL;
	TLASRef *tlas = NULL;
	CommandListRef *buildList = NULL;

	const F32 triangle[12] = {
		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1
	};

	Buffer triData = Buffer_createRefConst(triangle, sizeof(triangle));
	CharString name = CharString_createRefCStrConst("Capability trace positions");

	if(!Test_assert(t, "capPositions", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &triData, &positions, &t->err
	)))
		goto clean;

	const DeviceData positionData = (DeviceData) { .buffer = positions };
	name = CharString_createRefCStrConst("Capability trace BLAS");

	//Position fetch is a build time opt in: without AllowDataAccess the vertex data simply isn't kept in the
	// acceleration structure, so the flag rides along whenever the device claims the capability.

	const GraphicsDeviceCapabilities blasCaps = GraphicsDeviceRef_ptr(deviceRef)->info.capabilities;

	const ERTASBuildFlags blasFlags =
		(blasCaps.features & EGraphicsFeatures_RayTriPosition) &&
		!(blasCaps.experimentalFeatures & EGraphicsFeatures_RayTriPosition) &&
		!forceNoDataAccess
		? ERTASBuildFlags_AllowDataAccessExt : ERTASBuildFlags_None;

	const BLASCreateInfo blasInfo = BLASCreateInfo_unindexed(
		blasFlags, EBLASFlag_None, ETextureFormatId_RGBA32f, 0, 16, positionData
	);

	if(!Test_assert(t, "capBlas", GraphicsDeviceRef_createBLASExt(deviceRef, &blasInfo, &name, &blas, &t->err)))
		goto clean;

	//Two instances of the one BLAS: the second translated +2 along X, so a ray aimed there can only hit if
	// that instance got its own acceleration structure reference.
	//Every other test in the suite uses a single instance, which is why a backend writing all references into
	// instance 0's slot went unnoticed.

	TLASInstanceStatic instance[2] = {
		(TLASInstanceStatic) {
			.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
			.data = (TLASInstanceData) {
				.instanceId24_mask8 = 0xFFu << 24,
				.sbtOffset24_flags8 = (U32) ETLASInstanceFlag_Default << 24,
				.blasCpu = blas
			}
		},
		(TLASInstanceStatic) {
			.transform = { { 1, 0, 0, 2 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
			.data = (TLASInstanceData) {
				.instanceId24_mask8 = 0xFFu << 24,
				.sbtOffset24_flags8 = (U32) ETLASInstanceFlag_Default << 24,
				.blasCpu = blas
			}
		}
	};

	ListTLASInstanceStatic instances = (ListTLASInstanceStatic) { 0 };
	ListTLASInstanceStatic_createRefConst(instance, 2, &instances, NULL);

	name = CharString_createRefCStrConst("Capability trace TLAS");

	if(!Test_assert(t, "capTlas", GraphicsDeviceRef_createTLASExt(
		deviceRef, ERTASBuildFlags_DefaultTLAS, NULL, &instances, false, NULL, &name, &tlas, &t->err
	)))
		goto clean;

	//Creating them only declares them.
	//The build has to run on the device before anything can trace against it.

	//Each build gets its own scope, and the TLAS scope is numbered after the BLAS one, because the TLAS reads
	// what the BLAS build produced and the backend derives that dependency from the scope ids.

	if(!(
		Test_assert(t, "capAsList", GraphicsDeviceRef_createCommandList(
			deviceRef, 2 * KIBI, 64, 16, true, &buildList, &t->err
		)) &&
		Test_assert(t, "capAsBegin", CommandListRef_begin(buildList, true, U64_MAX, &t->err)) &&
		Test_assert(t, "capAsBlasScope", CommandListRef_startScope(buildList, NULL, 1, NULL, &t->err)) &&
		Test_assert(t, "capAsBlas", CommandListRef_updateBLASExt(buildList, blas, &t->err)) &&
		Test_assert(t, "capAsBlasScopeEnd", CommandListRef_endScope(buildList, &t->err)) &&
		Test_assert(t, "capAsTlasScope", CommandListRef_startScope(buildList, NULL, 2, NULL, &t->err)) &&
		Test_assert(t, "capAsTlas", CommandListRef_updateTLASExt(buildList, tlas, &t->err)) &&
		Test_assert(t, "capAsTlasScopeEnd", CommandListRef_endScope(buildList, &t->err)) &&
		Test_assert(t, "capAsEnd", CommandListRef_end(buildList, &t->err)) &&
		TestShaders_submitAndWait(t, deviceRef, buildList, NULL, 0)
	))
		goto clean;

	if(!Test_assert(t, "capTlasHandle", TLASRef_ptr(tlas)->handle != BindlessDescriptor_None))
		goto clean;

	*positionsOut = positions;
	*blasOut = blas;
	*tlasOut = tlas;

	RefPtr_dec(&buildList);
	return;

clean:

	RefPtr_dec(&buildList);
	RefPtr_dec(&tlas);
	RefPtr_dec(&blas);
	RefPtr_dec(&positions);
}

static Bool Test_runCapabilityShader(
	Test *t,
	GraphicsDeviceRef *deviceRef,
	CommandListRef *emptyList,
	const TestCapabilityShader *cap,
	TLASRef *tlas,
	Bool *ranOut
) {

	const Allocator *alloc = Platform_instance->alloc;
	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	*ranOut = false;

	//Whether the device claims this capability at all.
	//Every bit has to be present rather than any of them, since a shader declaring two extensions needs both;
	// gating on either would send a device missing one of them into a lookup that can only fail.

	const Bool hasFeatures =
		!cap->feature || (device->info.capabilities.features & cap->feature) == cap->feature;

	const Bool hasDataTypes =
		!cap->dataType || (device->info.capabilities.dataTypes & cap->dataType) == cap->dataType;

	const Bool supported = (cap->feature || cap->dataType) && hasFeatures && hasDataTypes;

	if(!supported)
		return true;

	//A capability the backend itself marks experimental is claimed but not held to: the claim rides on a
	// preview stack (D3D12 proxies RayTriPosition off preview SM6.10, for instance), and WARP's preview
	// pipeline genuinely fastfails under GBV when asked to run it.
	//Skipping is honest here, since the skip is logged and the bit leaves experimentalFeatures the moment the
	// real capability query is wired, at which point this line stops matching and the entry runs.

	if (device->info.capabilities.experimentalFeatures & cap->feature) {
		Log_debugLnx("-- capabilityExec: %s claimed but experimental on this backend, skipped", cap->name);
		return true;
	}

	//An entrypoint with no variant for this backend can only fail the lookup, and checkShaderFeatures logs a
	// device error on the way out.
	//That error is harmless here, but it reads like a real failure in the test output,
	// so decide up front rather than probing and letting the engine complain.

	const GraphicsInstance *instance = GraphicsInstanceRef_ptr(device->instance);
	const Bool isSpirvDevice = instance->api != EGraphicsApi_Direct3D12;

	if(cap->spirvOnly && !isSpirvDevice)
		return true;

	SHFile file = (SHFile) { 0 };

	if(!TestShaders_loadFile(t, cap->path, &file))
		return true;

	Bool ok = true;
	DeviceBufferRef *output = NULL;
	PipelineRef *pipeline = NULL;
	CommandListRef *commandList = NULL;

	const CharString outputName = CharString_createRefCStrConst("Capability output");
	const CharString entryName = CharString_createRefCStrConst("main");

	//The entrypoint is only present when compiled with the extension, so this doubles as the check that the
	// shader really was built for the capability rather than silently falling back.

	const U32 entryId = GraphicsDeviceRef_getFirstShaderEntry(
		deviceRef, &file, &entryName, NULL, NULL, ESHExtension_None, cap->require
	);

	if (entryId == U32_MAX) {

		//No entrypoint for this backend's binary type.
		//That is legitimate rather than a failure:
		// an entrypoint restricted with [[oxc::binary]] only exists for the backend it named,
		// and the device consuming the other one has nothing to run.

		Log_debugLnx("-- capabilityExec: %s claimed but no entrypoint for this backend, skipped", cap->name);

		SHFile_free(&file, alloc);
		return true;
	}

	ok &= Test_assert(t, "capOutput", GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_None,
		//ShaderWriteBindless rather than ShaderWrite: the shaders index the buffer through its bindless write handle,
		// and only the bindless flag allocates one.
		//Plain ShaderWrite leaves writeHandle at 0,
		// which makes every shader write land on descriptor 0 and the readback come back untouched.

		EGraphicsResourceFlag_ShaderWriteBindless | EGraphicsResourceFlag_CPUBacked,
		NULL, &outputName, 16, &output, &t->err
	));

	if(ok) {

		const CharString pipelineName = CharString_createRefCStrConst("Capability pipeline");

		ok &= Test_assert(t, "capPipeline", GraphicsDeviceRef_createPipelineCompute(
			deviceRef, &file, &pipelineName, entryId, &entryName, EPipelineFlags_None, NULL, &pipeline, &t->err
		));
	}

	if (ok) {

		ok &= Test_assert(t, "capList", GraphicsDeviceRef_createCommandList(
			deviceRef, 2 * KIBI, 64, 16, true, &commandList, &t->err
		));

		ok = ok && Test_assert(t, "capBegin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		//The dispatch has to sit in a scope that declares the write, or the backend has no barrier to insert
		// and refuses the bind.

		//A tracing entry reads the TLAS as well as writing the output, and the scope has to say so or the
		// backend has no barrier to put the acceleration structure into a readable state.

		const Transition transitions[2] = {
			(Transition) { .resource = output, .stage = EPipelineStage_Compute, .isWrite = true },
			(Transition) { .resource = tlas,   .stage = EPipelineStage_Compute }
		};

		ListTransition outputTransition = (ListTransition) { 0 };
		ListTransition_createRefConst(transitions, cap->needsTlas ? 2 : 1, &outputTransition, NULL);

		if (ok) {
			ok &= Test_assert(t, "capScope", CommandListRef_startScope(commandList, &outputTransition, 1, NULL, &t->err));
			ok &= Test_assert(t, "capBind", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
			ok &= Test_assert(t, "capDispatch", CommandListRef_dispatch1D(commandList, 1, &t->err));
			ok &= Test_assert(t, "capScopeEnd", CommandListRef_endScope(commandList, &t->err));
			ok &= Test_assert(t, "capEnd", CommandListRef_end(commandList, &t->err));
		}
	}

	//App data carries the output buffer's bindless write handle, matching what the shaders read at slot 0.

	if (ok) {

		//Slot 1 carries the shared TLAS for the entries that trace against it, matching what the shaders read.

		const U32 appData[2] = {
			DeviceBufferRef_ptr(output)->writeHandle,
			tlas ? TLASRef_ptr(tlas)->handle : 0
		};

		ok &= TestShaders_submitAndWait(t, deviceRef, commandList, appData, sizeof(appData));
		ok &= TestShaders_pullBuffer(t, deviceRef, emptyList, output);
	}

	if (ok) {

		const DeviceBuffer *outputPtr = DeviceBufferRef_ptr(output);

		for (U8 w = 0; w < cap->expectedCount; ++w) {

			const TestCapabilityWord word = cap->expected[w];

			//A word past the readback is a broken table entry rather than a broken device, so it fails here
			// instead of silently comparing against a zero that was never written.

			if(!Test_assert(t, "capResultInRange", Buffer_length(outputPtr->cpuData) >= (U64)word.offset + 4)) {
				ok = false;
				break;
			}

			const U32 got = *(const U32*)(outputPtr->cpuData.ptr + word.offset);

			ok &= Test_assert(t, "capResult", got == word.value);

			if(got != word.value)
				Log_debugLnx(
					"-- capabilityExec: %s at +%"PRIu32" expected 0x%08X, got 0x%08X",
					cap->name, word.offset, word.value, got
				);
		}

		*ranOut = ok;
	}

	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&output);
	SHFile_free(&file, alloc);

	return ok;
}

//The negative side of the RayTriPosition record time validation: a pipeline that declares position fetch,
// dispatched in a scope whose only transitioned TLAS provably contains a BLAS built without
// ERTASBuildFlags_AllowDataAccessExt, has to be refused when the dispatch is recorded.
//The positive side needs no extra test, since the rayTriPosition entry above dispatches through the same
// check against the properly flagged TLAS.

static void Test_capabilityRayTriPositionGuard(Test *t, GraphicsDeviceRef *deviceRef) {

	const GraphicsDeviceCapabilities caps = GraphicsDeviceRef_ptr(deviceRef)->info.capabilities;

	//Same gates as the positive entry: claimed, not experimental, and RayQuery for the inline trace.

	const EGraphicsFeatures needed = EGraphicsFeatures_RayQuery | EGraphicsFeatures_RayTriPosition;

	if((caps.features & needed) != needed || (caps.experimentalFeatures & EGraphicsFeatures_RayTriPosition))
		return;

	const Allocator *alloc = Platform_instance->alloc;

	DeviceBufferRef *positions = NULL;
	BLASRef *blas = NULL;
	TLASRef *tlas = NULL;
	DeviceBufferRef *output = NULL;
	PipelineRef *pipeline = NULL;
	CommandListRef *commandList = NULL;
	SHFile file = (SHFile) { 0 };

	Test_buildCapabilityTlas(t, deviceRef, true, &positions, &blas, &tlas);

	if(!Test_assert(t, "rtpGuardTlas", tlas != NULL))
		goto clean;

	//The cached bits are what the record time check reads, so they are pinned down here as well.

	Test_assert(t, "rtpGuardKnown", TLASRef_ptr(tlas)->blasDataAccessKnown);
	Test_assert(t, "rtpGuardAllOff", !TLASRef_ptr(tlas)->blasDataAccessAll);

	if(!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_caps_raytriposition.oiSH", &file))
		goto clean;

	const CharString entryName = CharString_createRefCStrConst("main");

	const U32 entryId = GraphicsDeviceRef_getFirstShaderEntry(
		deviceRef, &file, &entryName, NULL, NULL, ESHExtension_None, ESHExtension_RayTriPosition
	);

	if(!Test_assert(t, "rtpGuardEntry", entryId != U32_MAX))
		goto clean;

	const CharString pipelineName = CharString_createRefCStrConst("RTP guard pipeline");
	const CharString outputName = CharString_createRefCStrConst("RTP guard output");

	if(!(
		Test_assert(t, "rtpGuardPipeline", GraphicsDeviceRef_createPipelineCompute(
			deviceRef, &file, &pipelineName, entryId, &entryName, EPipelineFlags_None, NULL, &pipeline, &t->err
		)) &&
		Test_assert(t, "rtpGuardOutput", GraphicsDeviceRef_createBuffer(
			deviceRef, EDeviceBufferUsage_None,
			EGraphicsResourceFlag_ShaderWriteBindless | EGraphicsResourceFlag_CPUBacked,
			NULL, &outputName, 16, &output, &t->err
		)) &&
		Test_assert(t, "rtpGuardList", GraphicsDeviceRef_createCommandList(
			deviceRef, 2 * KIBI, 64, 16, true, &commandList, &t->err
		)) &&
		Test_assert(t, "rtpGuardBegin", CommandListRef_begin(commandList, true, U64_MAX, &t->err))
	))
		goto clean;

	const Transition transitions[2] = {
		(Transition) { .resource = output, .stage = EPipelineStage_Compute, .isWrite = true },
		(Transition) { .resource = tlas,   .stage = EPipelineStage_Compute }
	};

	ListTransition scopeTransitions = (ListTransition) { 0 };
	ListTransition_createRefConst(transitions, 2, &scopeTransitions, NULL);

	if(!(
		Test_assert(t, "rtpGuardScope", CommandListRef_startScope(commandList, &scopeTransitions, 1, NULL, &t->err)) &&
		Test_assert(t, "rtpGuardBind", CommandListRef_setComputePipeline(commandList, pipeline, &t->err))
	))
		goto clean;

	//The dispatch itself is what has to fail, since the only TLAS this scope can trace provably lacks the flag.

	Test_assert(t, "rtpGuardRejects", !CommandListRef_dispatch1D(commandList, 1, NULL));

clean:

	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&output);
	RefPtr_dec(&tlas);
	RefPtr_dec(&blas);
	RefPtr_dec(&positions);
	SHFile_free(&file, alloc);
}

void Test_graphicsCapabilityExecution(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "GraphicsDevice/capabilityExec");

	CommandListRef *emptyList = NULL;

	if(!(
		Test_assert(t, "createEmptyList", GraphicsDeviceRef_createCommandList(
			deviceRef, 2 * KIBI, 64, 16, true, &emptyList, &t->err
		)) &&
		Test_assert(t, "beginEmptyList", CommandListRef_begin(emptyList, true, U64_MAX, &t->err)) &&
		Test_assert(t, "endEmptyList", CommandListRef_end(emptyList, &t->err))
	)) {
		RefPtr_dec(&emptyList);
		return;
	}

	//One TLAS shared by every entry that traces, built here rather than per entry so the acceleration
	// structure build isn't repeated for each capability that happens to want one.
	//It only exists where the device claims RayQuery.
	//Entries that need it are skipped otherwise.

	DeviceBufferRef *positions = NULL;
	BLASRef *blas = NULL;
	TLASRef *tlas = NULL;

	if(GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.features & EGraphicsFeatures_RayQuery)
		Test_buildCapabilityTlas(t, deviceRef, false, &positions, &blas, &tlas);

	U32 verified = 0, skipped = 0;

	for (U64 i = 0; i < sizeof(testCapabilityShaders) / sizeof(testCapabilityShaders[0]); ++i) {

		Bool ran = false;

		//An entry that traces has nothing to trace against without the TLAS, so it doesn't run at all rather
		// than dispatching with a handle that points nowhere.

		if(testCapabilityShaders[i].needsTlas && !tlas) {
			++skipped;
			continue;
		}

		Test_runCapabilityShader(t, deviceRef, emptyList, &testCapabilityShaders[i], tlas, &ran);

		if(ran)
			++verified;

		else ++skipped;
	}

	//Says which capabilities were actually executed, so a green run isn't read as "all of them".

	Log_debugLnx("-- capabilityExec: %"PRIu32" capabilities executed and verified, %"PRIu32" not run", verified, skipped);

	Test_capabilityRayTriPositionGuard(t, deviceRef);

	//The TLAS outlives every dispatch that traced against it, so it's released last.

	GraphicsDeviceRef_wait(deviceRef, NULL);

	RefPtr_dec(&tlas);
	RefPtr_dec(&blas);
	RefPtr_dec(&positions);
	RefPtr_dec(&emptyList);
}
