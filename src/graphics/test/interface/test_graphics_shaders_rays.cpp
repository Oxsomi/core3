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

//graphics/test/interface/test_graphics_shaders_rays.cpp
//
//Ray trace execution, including the acceleration structures and shader binding tables it has to build
//first, which is most of the file.
//Split out of test_graphics_shaders.c, which had grown past 2300 lines.

namespace oxc { namespace c {
	#include "types/base/string_base.h"
	#include "types/container/buffer.h"
	#include "types/container/memory_stream.h"
	#include "types/container/texture_format.h"
	#include "types/test/test.h"
	#include "formats/oiSH/sh_file.h"
	#include "formats/oiSH/sh_registers.h"
	#include "platforms/file.h"
	#include "platforms/logx.h"
	#include "platforms/platform.h"
	#include "graphics/generic/bindless_descriptor.h"
	#include "graphics/generic/blas.h"
	#include "graphics/generic/command_list.h"
	#include "graphics/generic/commands.h"
	#include "graphics/generic/depth_stencil.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_buffer.h"
	#include "graphics/generic/device_info.h"
	#include "graphics/generic/instance.h"
	#include "graphics/generic/opacity_micromap.h"
	#include "graphics/generic/pipeline.h"
	#include "graphics/generic/render_texture.h"
	#include "graphics/generic/texture.h"
	#include "graphics/generic/tlas.h"
	#include "test_graphics_shared.h"
} }

//Same namespace the C headers landed in, so the definitions here match the declarations in
//test_graphics_shared.h and the macros in those headers still expand to names that resolve.

namespace oxc { namespace c {

//The output handle and the acceleration structure, which is what the ray shaders read.
//Shared by the module, since every pipeline here declares the same block.

static PipelineLayoutRef *raysPushLayout = NULL;
static U32 raysPushData[4] = {};

static Bool TestShaders_raysLayout(Test *t, GraphicsDeviceRef *deviceRef, const SHFile *file, U32 raygenId) {
	return raysPushLayout || TestShaders_pushConstantLayout(t, deviceRef, file, raygenId, &raysPushLayout);
}

static Bool TestShaders_pushRays(Test *t, CommandListRef *commandList) {
	const Buffer ref = Buffer_createRefConst(raysPushData, sizeof(raysPushData));
	return Test_assert(t, "pushRays", CommandListRef_setPushConstants(commandList, ref, &t->err));
}

// -- 33. Ray trace execution -----------------------------------------------------

//The same one triangle scene the AS module builds, but with the bindless descriptor enabled so a real
// raytracing pipeline can fetch the TLAS and trace against it.
//Two rays start above the triangle's interior and two outside, so the readback distinguishes the closest
// hit and miss shaders actually running from any default.

//Special index opacity micromaps, traced for real rather than only validated at create.
//Two BLASes over the same triangle and the same index buffer, differing in nothing but the per triangle
// special index they carry: FullyOpaque has to trace exactly like the plain scene, FullyTransparent has to
// make every one of the 4 rays miss.
//Both halves are needed. "Everything missed" on its own is also what a BLAS that quietly failed to build
// looks like, so the opaque half is what proves the geometry survived the OMM path at all, and only the pair
// together says the micromap was consulted.
//Narrow formats rather than R32u on purpose: the special indices are signed constants matched against an
// unsigned element, so the truncated widths (0xFFFF for R16u, 0xFF for R8u) are where a driver would
// disagree with our packing.
//The wrapper below runs R16u everywhere and repeats the pair with R8u where RayMicromapOpacityU8 is set,
// which on Vulkan doubles as the only execution coverage the KHR extension path can get.

static void TestShaders_ommSpecialIndexWithFormat(
	Test *t,
	GraphicsDeviceRef *deviceRef,
	const SHFile *file,
	DeviceBufferRef *positions,
	DeviceBufferRef *output,
	CommandListRef *emptyList,
	ETextureFormatId ommIndexFormat
) {

	const GraphicsDeviceCapabilities caps = GraphicsDeviceRef_ptr(deviceRef)->info.capabilities;

	if (!(caps.features & EGraphicsFeatures_RayMicromapOpacity)) {
		Test_print(t, "Device lacks opacity micromaps, skipping OMM trace test");
		return;
	}

	if (caps.experimentalFeatures & EGraphicsFeatures_RayMicromapOpacity) {
		Test_print(t, "Opacity micromaps claimed but experimental on this backend, skipping OMM trace test");
		return;
	}

	DeviceBufferRef *indices = NULL;
	DeviceBufferRef *ommOpaque = NULL;
	DeviceBufferRef *ommTransparent = NULL;
	BLASRef *blasOpaque = NULL;
	BLASRef *blasTransparent = NULL;
	TLASRef *tlasOpaque = NULL;
	TLASRef *tlasTransparent = NULL;
	PipelineRef *pipeline = NULL;
	CommandListRef *opaqueList = NULL;
	CommandListRef *transparentList = NULL;

	//An OMM index is per triangle, which is why this geometry is indexed where the plain scene is not.

	const U16 triangleIndices[3] = { 0, 1, 2 };
	Buffer indexData = Buffer_createRefConst(triangleIndices, sizeof(triangleIndices));

	CharString name = CharString_createRefCStrConst("OMM triangle indices");

	if(!Test_assert(t, "ommCreateIndices", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &indexData, &indices, &t->err
	)))
		goto clean;

	//Packed into a U32 and sliced to the element width, which reads out the low bytes on the little endian
	// targets OxC3 runs on; one triangle, so the buffer is exactly one element.

	//Scoped so the goto above jumps around these rather than into them.
	{
	const U8 ommStride = ommIndexFormat == ETextureFormatId_R32u ? 4 : (ommIndexFormat == ETextureFormatId_R16u ? 2 : 1);

	const U32 opaqueIndex = EOMMSpecialIndex_pack(EOMMSpecialIndex_FullyOpaque, ommIndexFormat);
	const U32 transparentIndex = EOMMSpecialIndex_pack(EOMMSpecialIndex_FullyTransparent, ommIndexFormat);

	Buffer opaqueData = Buffer_createRefConst(&opaqueIndex, ommStride);
	Buffer transparentData = Buffer_createRefConst(&transparentIndex, ommStride);

	name = CharString_createRefCStrConst("OMM indices, fully opaque");

	if(!Test_assert(t, "ommCreateOpaque", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &opaqueData, &ommOpaque, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("OMM indices, fully transparent");

	if(!Test_assert(t, "ommCreateTransparent", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &transparentData, &ommTransparent, &t->err
	)))
		goto clean;

	const DeviceData positionData = { .buffer = positions };
	const DeviceData indexBufferData = { .buffer = indices };

	const BLASCreateInfo opaqueInfo = BLASCreateInfo_indexedWithOmmIndicesExt(
		ERTASBuildFlags_None, EBLASFlag_None, ETextureFormatId_RGBA32f, 0, 16, positionData,
		ETextureFormatId_R16u, indexBufferData,
		ommIndexFormat, { .buffer = ommOpaque }
	);

	const BLASCreateInfo transparentInfo = BLASCreateInfo_indexedWithOmmIndicesExt(
		ERTASBuildFlags_None, EBLASFlag_None, ETextureFormatId_RGBA32f, 0, 16, positionData,
		ETextureFormatId_R16u, indexBufferData,
		ommIndexFormat, { .buffer = ommTransparent }
	);

	name = CharString_createRefCStrConst("OMM BLAS, fully opaque");

	if(!Test_assert(t, "ommCreateBlasOpaque", GraphicsDeviceRef_createBLASExt(
		deviceRef, &opaqueInfo, &name, &blasOpaque, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("OMM BLAS, fully transparent");

	if(!Test_assert(t, "ommCreateBlasTransparent", GraphicsDeviceRef_createBLASExt(
		deviceRef, &transparentInfo, &name, &blasTransparent, &t->err
	)))
		goto clean;

	//ForceDisableAnyHit is deliberately absent where the rest of the module uses Default.
	//It is FORCE_OPAQUE on both APIs, and a forced opaque instance makes traversal ignore opacity micromaps
	// entirely, so the transparent half would report hits and pass for the wrong reason.

	TLASInstance ommInstance = {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
		.data = {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (U32) ETLASInstanceFlag_DisableCulling << 24,
			.blasCpu = blasOpaque
		}
	};

	ListTLASInstance ommInstances {};
	ListTLASInstance_createRefConst(&ommInstance, 1, &ommInstances, NULL);

	name = CharString_createRefCStrConst("OMM TLAS, fully opaque");

	if(!Test_assert(t, "ommCreateTlasOpaque", GraphicsDeviceRef_createTLASExt(
		deviceRef, ERTASBuildFlags_DefaultTLAS, &ommInstances, false, NULL, &name, &tlasOpaque, &t->err
	)))
		goto clean;

	ommInstance.data.blasCpu = blasTransparent;
	name = CharString_createRefCStrConst("OMM TLAS, fully transparent");

	if(!Test_assert(t, "ommCreateTlasTransparent", GraphicsDeviceRef_createTLASExt(
		deviceRef, ERTASBuildFlags_DefaultTLAS, &ommInstances, false, NULL, &name, &tlasTransparent, &t->err
	)))
		goto clean;

	//A pipeline of its own: both APIs ignore micromaps unless the pipeline opted in, so the plain one the
	// module already built would trace straight through the transparent triangle and report hits.

	const U32 raygenId = TestShaders_entry(t, deviceRef, file, "mainRaygen");
	const U32 missId = TestShaders_entry(t, deviceRef, file, "mainMiss");
	const U32 hitId = TestShaders_entry(t, deviceRef, file, "mainClosestHit");

	if(raygenId == U32_MAX || missId == U32_MAX || hitId == U32_MAX)
		goto clean;

	PipelineStage ommStages[3] = {
		{ .binaryId = raygenId },
		{ .binaryId = missId },
		{ .binaryId = hitId }
	};

	ListPipelineStage ommStageList {};
	ListPipelineStage_createRefConst(ommStages, 3, &ommStageList, NULL);

	ListSHFile ommFileList {};
	ListSHFile_createRefConst(file, 1, &ommFileList, NULL);

	PipelineRaytracingGroup ommGroup = {
		.closestHit = 2, .anyHit = U32_MAX, .intersection = U32_MAX
	};

	ListPipelineRaytracingGroup ommGroupList {};
	ListPipelineRaytracingGroup_createRefConst(&ommGroup, 1, &ommGroupList, NULL);

	const PipelineRaytracingInfo ommPipelineInfo = {
		.flags = EPipelineRaytracingFlags_Default | EPipelineRaytracingFlags_AllowOpacityMicromapExt,
		.maxRecursionDepth = 1
	};

	name = CharString_createRefCStrConst("OMM ray trace pipeline");

	TestShaders_raysLayout(t, deviceRef, file, raygenId);

	if(!Test_assert(t, "ommCreatePipeline", GraphicsDeviceRef_createPipelineRaytracingExt(
		deviceRef, &ommStageList, &ommFileList, &ommGroupList, &ommPipelineInfo, &name,
		EPipelineFlags_None, raysPushLayout, &pipeline, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "ommCreateOpaqueList", GraphicsDeviceRef_createCommandList(
		deviceRef, 2 * KIBI, 32, 16, true, &opaqueList, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "ommCreateTransparentList", GraphicsDeviceRef_createCommandList(
		deviceRef, 2 * KIBI, 32, 16, true, &transparentList, &t->err
	)))
		goto clean;

	//Opaque first, so a failure below can be read as "the OMM path broke tracing" rather than "the micromap
	// culled something", which are the two ways this can go wrong and want different fixes.

	const Transition opaqueTransitions[2] = {
		{ .resource = output, .stage = EPipelineStage_RaygenExt, .isWrite = true },
		{ .resource = tlasOpaque, .stage = EPipelineStage_RaygenExt }
	};

	ListTransition opaqueTransitionList {};
	ListTransition_createRefConst(opaqueTransitions, 2, &opaqueTransitionList, NULL);

	//Set before the recording: a push constant is captured when the list is recorded, not when it is
	//submitted.

	raysPushData[0] = DeviceBufferRef_ptr(output)->writeHandle;
	raysPushData[1] = TLASRef_ptr(tlasOpaque)->handle;

	Test_assert(t, "ommBeginOpaque", CommandListRef_begin(opaqueList, true, U64_MAX, &t->err));

	Test_assert(t, "ommScopeBlasOpaque", CommandListRef_startScope(opaqueList, NULL, 1, NULL, &t->err));
	Test_assert(t, "ommUpdateBlasOpaque", CommandListRef_updateBLASExt(opaqueList, blasOpaque, &t->err));
	Test_assert(t, "ommScopeBlasOpaqueEnd", CommandListRef_endScope(opaqueList, &t->err));

	Test_assert(t, "ommScopeTlasOpaque", CommandListRef_startScope(opaqueList, NULL, 2, NULL, &t->err));
	Test_assert(t, "ommUpdateTlasOpaque", CommandListRef_updateTLASExt(opaqueList, tlasOpaque, &t->err));
	Test_assert(t, "ommScopeTlasOpaqueEnd", CommandListRef_endScope(opaqueList, &t->err));

	Test_assert(t, "ommScopeTraceOpaque", CommandListRef_startScope(
		opaqueList, &opaqueTransitionList, 3, NULL, &t->err
	));

	Test_assert(t, "ommBindOpaque", CommandListRef_setRaytracingPipeline(opaqueList, pipeline, &t->err));
		TestShaders_pushRays(t, opaqueList);
	Test_assert(t, "ommTraceOpaque", CommandListRef_dispatch1DRaysExt(opaqueList, 0, 4, &t->err));
	Test_assert(t, "ommScopeTraceOpaqueEnd", CommandListRef_endScope(opaqueList, &t->err));

	Test_assert(t, "ommEndOpaque", CommandListRef_end(opaqueList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, opaqueList))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			Test_assert(t, "ommResultsOpaque", values[0] == 1 && values[1] == 1 && !values[2] && !values[3]);
		}

	const Transition transparentTransitions[2] = {
		{ .resource = output, .stage = EPipelineStage_RaygenExt, .isWrite = true },
		{ .resource = tlasTransparent, .stage = EPipelineStage_RaygenExt }
	};

	ListTransition transparentTransitionList {};
	ListTransition_createRefConst(transparentTransitions, 2, &transparentTransitionList, NULL);

	raysPushData[1] = TLASRef_ptr(tlasTransparent)->handle;

	Test_assert(t, "ommBeginTransparent", CommandListRef_begin(transparentList, true, U64_MAX, &t->err));

	Test_assert(t, "ommScopeBlasTransparent", CommandListRef_startScope(transparentList, NULL, 1, NULL, &t->err));

	Test_assert(t, "ommUpdateBlasTransparent", CommandListRef_updateBLASExt(
		transparentList, blasTransparent, &t->err
	));

	Test_assert(t, "ommScopeBlasTransparentEnd", CommandListRef_endScope(transparentList, &t->err));

	Test_assert(t, "ommScopeTlasTransparent", CommandListRef_startScope(transparentList, NULL, 2, NULL, &t->err));

	Test_assert(t, "ommUpdateTlasTransparent", CommandListRef_updateTLASExt(
		transparentList, tlasTransparent, &t->err
	));

	Test_assert(t, "ommScopeTlasTransparentEnd", CommandListRef_endScope(transparentList, &t->err));

	Test_assert(t, "ommScopeTraceTransparent", CommandListRef_startScope(
		transparentList, &transparentTransitionList, 3, NULL, &t->err
	));

	Test_assert(t, "ommBindTransparent", CommandListRef_setRaytracingPipeline(transparentList, pipeline, &t->err));
		TestShaders_pushRays(t, transparentList);
	Test_assert(t, "ommTraceTransparent", CommandListRef_dispatch1DRaysExt(transparentList, 0, 4, &t->err));
	Test_assert(t, "ommScopeTraceTransparentEnd", CommandListRef_endScope(transparentList, &t->err));

	Test_assert(t, "ommEndTransparent", CommandListRef_end(transparentList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, transparentList))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			Test_assert(t, "ommResultsTransparent", !values[0] && !values[1] && !values[2] && !values[3]);
		}

	}

clean:

	RefPtr_dec(&transparentList);
	RefPtr_dec(&opaqueList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&tlasTransparent);
	RefPtr_dec(&tlasOpaque);
	RefPtr_dec(&blasTransparent);
	RefPtr_dec(&blasOpaque);
	RefPtr_dec(&ommTransparent);
	RefPtr_dec(&ommOpaque);
	RefPtr_dec(&indices);
}

//A real micromap ARRAY, built on the GPU and linked into a BLAS, rather than the special index form above.
//One micromap holds five 2-state subdivision level 1 entries: entry k of 0..3 is opaque except for micro
// triangle k, entry 4 is fully transparent.
//The instance is nudged by (-0.05, -0.05) so ray 0 lands strictly inside the CENTER sub triangle and ray 1
// strictly inside the corner one at the barycentric origin, comfortably away from every sub triangle edge.
//The assertions are deliberately mapping agnostic: the spec's space filling curve decides which bit is which
// sub triangle, so instead of assuming that order, the four single bit probes must produce exactly one
// (miss, hit), exactly one (hit, miss) and two (hit, hit) for rays 0 and 1.
//That is only satisfiable if each probe culled a DIFFERENT sub triangle, which is per micro triangle
// addressing proven without a single assumption about the curve; entry 4 proving all miss pins the decode.

static void TestShaders_ommMicromapArray(
	Test *t,
	GraphicsDeviceRef *deviceRef,
	const SHFile *file,
	DeviceBufferRef *positions,
	DeviceBufferRef *output,
	CommandListRef *emptyList
) {

	const GraphicsDeviceCapabilities caps = GraphicsDeviceRef_ptr(deviceRef)->info.capabilities;

	if (!(caps.features & EGraphicsFeatures_RayMicromapOpacity)) {
		Test_print(t, "Device lacks opacity micromaps, skipping micromap array test");
		return;
	}

	if (caps.experimentalFeatures & EGraphicsFeatures_RayMicromapOpacity) {
		Test_print(t, "Opacity micromaps claimed but experimental on this backend, skipping micromap array test");
		return;
	}

	//The KHR path builds micromap arrays as acceleration structures, which isn't implemented until a driver
	// exists to test it against; special index OMM covers such a device above.

	const GraphicsInstance *instance = GraphicsInstanceRef_ptr(GraphicsDeviceRef_ptr(deviceRef)->instance);

	if (
		instance->api == EGraphicsApi_Vulkan &&
		(caps.featuresExt & EVkGraphicsFeatures_OpacityMicromapKHR)
	) {
		Test_print(t, "Micromap arrays aren't implemented on the Vulkan KHR path yet, skipping");
		return;
	}

	DeviceBufferRef *indices = NULL;
	DeviceBufferRef *inputBits = NULL;
	DeviceBufferRef *entries = NULL;
	OpacityMicromapRef *micromap = NULL;
	PipelineRef *pipeline = NULL;

	DeviceBufferRef *ommIndex[5] = { 0 };
	BLASRef *blas[5] = { 0 };
	TLASRef *tlas[5] = { 0 };
	CommandListRef *lists[5] = { 0 };

	const U16 triangleIndices[3] = { 0, 1, 2 };
	Buffer indexData = Buffer_createRefConst(triangleIndices, sizeof(triangleIndices));

	CharString name = CharString_createRefCStrConst("OMM array triangle indices");

	if(!Test_assert(t, "ommArrayCreateIndices", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &indexData, &indices, &t->err
	)))
		goto clean;

	//One byte of opacity bits per entry, 4 bytes apart so every dataOffset stays 4 byte aligned.
	//2-state: bit set is opaque, cleared is transparent, and only the low 4 bits exist at level 1.

	//Scoped so the goto above jumps around these rather than into them.
	{
	U8 opacityBits[20] = { 0 };

	for(U8 k = 0; k < 4; ++k)
		opacityBits[k * 4] = 0xF & ~(1 << k);

	//opacityBits[16] stays 0: entry 4 is fully transparent

	Buffer bitsData = Buffer_createRefConst(opacityBits, sizeof(opacityBits));
	name = CharString_createRefCStrConst("OMM array opacity bits");

	if(!Test_assert(t, "ommArrayCreateBits", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &bitsData, &inputBits, &t->err
	)))
		goto clean;

	OpacityMicromapEntry entryData[5];

	for(U8 k = 0; k < 5; ++k)
		entryData[k] = {
			.dataOffset = (U32) k * 4,
			.subdivisionLevel = 1,
			.format = EOpacityMicromapFormat_Opacity2State
		};

	Buffer entryRef = Buffer_createRefConst(entryData, sizeof(entryData));
	name = CharString_createRefCStrConst("OMM array entries");

	if(!Test_assert(t, "ommArrayCreateEntries", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &entryRef, &entries, &t->err
	)))
		goto clean;

	const OpacityMicromapUsage usage = {
		.count = 5, .subdivisionLevel = 1, .format = EOpacityMicromapFormat_Opacity2State
	};

	//Named rather than compound literals: C++ has no address of a braced list, and these are passed by
	//pointer.

	const DeviceData inputBitsData = { .buffer = inputBits };
	const DeviceData entriesData = { .buffer = entries };

	OpacityMicromapCreateInfo micromapInfo = OpacityMicromapCreateInfo_uniform(
		ERTASBuildFlags_None,
		&inputBitsData,
		&entriesData,
		sizeof(OpacityMicromapEntry),
		&usage
	);

	name = CharString_createRefCStrConst("OMM array micromap");

	if(!Test_assert(t, "ommArrayCreate", GraphicsDeviceRef_createOpacityMicromapExt(
		deviceRef, &micromapInfo, &name, &micromap, &t->err
	)))
		goto clean;

	//The same pipeline shape the special index test uses; micromaps are ignored without the opt in

	const U32 raygenId = TestShaders_entry(t, deviceRef, file, "mainRaygen");
	const U32 missId = TestShaders_entry(t, deviceRef, file, "mainMiss");
	const U32 hitId = TestShaders_entry(t, deviceRef, file, "mainClosestHit");

	if(raygenId == U32_MAX || missId == U32_MAX || hitId == U32_MAX)
		goto clean;

	PipelineStage stages[3] = {
		{ .binaryId = raygenId },
		{ .binaryId = missId },
		{ .binaryId = hitId }
	};

	ListPipelineStage stageList {};
	ListPipelineStage_createRefConst(stages, 3, &stageList, NULL);

	ListSHFile fileList {};
	ListSHFile_createRefConst(file, 1, &fileList, NULL);

	PipelineRaytracingGroup group = {
		.closestHit = 2, .anyHit = U32_MAX, .intersection = U32_MAX
	};

	ListPipelineRaytracingGroup groupList {};
	ListPipelineRaytracingGroup_createRefConst(&group, 1, &groupList, NULL);

	const PipelineRaytracingInfo pipelineInfo = {
		.flags = EPipelineRaytracingFlags_Default | EPipelineRaytracingFlags_AllowOpacityMicromapExt,
		.maxRecursionDepth = 1
	};

	name = CharString_createRefCStrConst("OMM array pipeline");

	TestShaders_raysLayout(t, deviceRef, file, raygenId);

	if(!Test_assert(t, "ommArrayCreatePipeline", GraphicsDeviceRef_createPipelineRaytracingExt(
		deviceRef, &stageList, &fileList, &groupList, &pipelineInfo, &name,
		EPipelineFlags_None, raysPushLayout, &pipeline, &t->err
	)))
		goto clean;

	//How rays 0 and 1 resolved per probe, packed as (ray0Hit << 1) | ray1Hit

	U8 outcomes[4] = { 0 };
	Bool traced = true;

	for (U8 k = 0; k < 5 && traced; ++k) {

		const U16 entryIndex = k;
		Buffer ommIndexData = Buffer_createRefConst(&entryIndex, sizeof(entryIndex));

		name = CharString_createRefCStrConst("OMM array index buffer");

		traced &= Test_assert(t, "ommArrayCreateIndexBuf", GraphicsDeviceRef_createBufferData(
			deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
			&name, &ommIndexData, &ommIndex[k], &t->err
		));

		if(!traced)
			break;

		const BLASCreateInfo blasInfo = BLASCreateInfo_indexedWithOmmExt(
			ERTASBuildFlags_None, EBLASFlag_None, ETextureFormatId_RGBA32f, 0, 16,
			{ .buffer = positions },
			ETextureFormatId_R16u, { .buffer = indices },
			ETextureFormatId_R16u, { .buffer = ommIndex[k] },
			micromap
		);

		name = CharString_createRefCStrConst("OMM array BLAS");

		traced &= Test_assert(t, "ommArrayCreateBlas", GraphicsDeviceRef_createBLASExt(
			deviceRef, &blasInfo, &name, &blas[k], &t->err
		));

		if(!traced)
			break;

		//Nudged so ray 0 sits strictly inside the center sub triangle and ray 1 strictly inside the corner
		// one; without this ray 0 would land exactly on the shared edge and the outcome would be tie break
		// dependent.

		const TLASInstance ommInstance = {
			.transform = { { 1, 0, 0, -0.05f }, { 0, 1, 0, -0.05f }, { 0, 0, 1, 0 } },
			.data = {
				.instanceId24_mask8 = 0xFFu << 24,
				.sbtOffset24_flags8 = (U32) ETLASInstanceFlag_DisableCulling << 24,
				.blasCpu = blas[k]
			}
		};

		ListTLASInstance ommInstances {};
		ListTLASInstance_createRefConst(&ommInstance, 1, &ommInstances, NULL);

		name = CharString_createRefCStrConst("OMM array TLAS");

		traced &= Test_assert(t, "ommArrayCreateTlas", GraphicsDeviceRef_createTLASExt(
			deviceRef, ERTASBuildFlags_DefaultTLAS, &ommInstances, false, NULL, &name, &tlas[k], &t->err
		));

		traced &= Test_assert(t, "ommArrayCreateList", GraphicsDeviceRef_createCommandList(
			deviceRef, 2 * KIBI, 32, 16, true, &lists[k], &t->err
		));

		if(!traced)
			break;

		const Transition traceTransitions[2] = {
			{ .resource = output, .stage = EPipelineStage_RaygenExt, .isWrite = true },
			{ .resource = tlas[k], .stage = EPipelineStage_RaygenExt }
		};

		ListTransition traceTransitionList {};
		ListTransition_createRefConst(traceTransitions, 2, &traceTransitionList, NULL);

		//The micromap build only runs once; recording it again is a no-op after it completed

		raysPushData[0] = DeviceBufferRef_ptr(output)->writeHandle;
		raysPushData[1] = TLASRef_ptr(tlas[k])->handle;

		Test_assert(t, "ommArrayBegin", CommandListRef_begin(lists[k], true, U64_MAX, &t->err));

		Test_assert(t, "ommArrayScopeOmm", CommandListRef_startScope(lists[k], NULL, 1, NULL, &t->err));
		Test_assert(t, "ommArrayUpdateOmm", CommandListRef_updateOmmExt(lists[k], micromap, &t->err));
		Test_assert(t, "ommArrayScopeOmmEnd", CommandListRef_endScope(lists[k], &t->err));

		Test_assert(t, "ommArrayScopeBlas", CommandListRef_startScope(lists[k], NULL, 2, NULL, &t->err));
		Test_assert(t, "ommArrayUpdateBlas", CommandListRef_updateBLASExt(lists[k], blas[k], &t->err));
		Test_assert(t, "ommArrayScopeBlasEnd", CommandListRef_endScope(lists[k], &t->err));

		Test_assert(t, "ommArrayScopeTlas", CommandListRef_startScope(lists[k], NULL, 3, NULL, &t->err));
		Test_assert(t, "ommArrayUpdateTlas", CommandListRef_updateTLASExt(lists[k], tlas[k], &t->err));
		Test_assert(t, "ommArrayScopeTlasEnd", CommandListRef_endScope(lists[k], &t->err));

		Test_assert(t, "ommArrayScopeTrace", CommandListRef_startScope(
			lists[k], &traceTransitionList, 4, NULL, &t->err
		));

		Test_assert(t, "ommArrayBind", CommandListRef_setRaytracingPipeline(lists[k], pipeline, &t->err));
		TestShaders_pushRays(t, lists[k]);
		Test_assert(t, "ommArrayTrace", CommandListRef_dispatch1DRaysExt(lists[k], 0, 4, &t->err));
		Test_assert(t, "ommArrayScopeTraceEnd", CommandListRef_endScope(lists[k], &t->err));

		Test_assert(t, "ommArrayEnd", CommandListRef_end(lists[k], &t->err));

		traced &= TestShaders_submitAndWait(t, deviceRef, lists[k]);
		traced = traced && TestShaders_pullBuffer(t, deviceRef, emptyList, output);

		if (traced) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			//The geometric misses stay misses no matter what the micromap says

			Test_assert(t, "ommArrayOutsideMiss", !values[2] && !values[3]);

			if(k == 4)
				Test_assert(t, "ommArrayAllTransparent", !values[0] && !values[1]);

			else outcomes[k] = (U8)(((values[0] == 1) << 1) | (values[1] == 1));
		}
	}

	//The probe set proves per micro triangle addressing without assuming the space filling curve's order

	if (traced) {

		U8 missHit = 0, hitMiss = 0, hitHit = 0, missMiss = 0;

		for (U8 k = 0; k < 4; ++k)
			switch (outcomes[k]) {
				case 1:     ++missHit;    break;        //ray 0 culled: this bit is the center sub triangle
				case 2:     ++hitMiss;    break;        //ray 1 culled: this bit is the corner sub triangle
				case 3:     ++hitHit;     break;        //a sub triangle neither ray visits
				default:    ++missMiss;   break;        //one bit culled both rays: not per micro triangle
			}

		Test_assert(t, "ommArrayCenterProbe", missHit == 1);
		Test_assert(t, "ommArrayCornerProbe", hitMiss == 1);
		Test_assert(t, "ommArrayUntouchedProbes", hitHit == 2);
		Test_assert(t, "ommArrayNoDoubleCull", !missMiss);

		//The likely emulated hint fires once per device rather than per BLAS: five linked micromaps, one
		// bit. Non NV vendors (WARP included) claim RayMicromapOpacityActual, so the bit only appears
		// where that is unset.

		Test_assert(
			t, "ommArrayHintOnce",
			!!(AtomicI64_load(&GraphicsDeviceRef_ptr(deviceRef)->runtimeMessages) &
			(I64) EGraphicsDeviceMessage_OmmLikelyEmulated) ==
			!(caps.features2 & EGraphicsFeatures2_RayMicromapOpacityActual)
		);
	}

	}

clean:

	for (U8 k = 0; k < 5; ++k) {
		RefPtr_dec(&lists[k]);
		RefPtr_dec(&tlas[k]);
		RefPtr_dec(&blas[k]);
		RefPtr_dec(&ommIndex[k]);
	}

	RefPtr_dec(&pipeline);
	RefPtr_dec(&micromap);
	RefPtr_dec(&entries);
	RefPtr_dec(&inputBits);
	RefPtr_dec(&indices);
}

static void TestShaders_ommSpecialIndex(
	Test *t,
	GraphicsDeviceRef *deviceRef,
	const SHFile *file,
	DeviceBufferRef *positions,
	DeviceBufferRef *output,
	CommandListRef *emptyList
) {

	TestShaders_ommSpecialIndexWithFormat(t, deviceRef, file, positions, output, emptyList, ETextureFormatId_R16u);

	//The 8-bit pair is the same scene through a 1 byte element, where the special index truncates to 0xFF.

	const GraphicsDeviceCapabilities caps = GraphicsDeviceRef_ptr(deviceRef)->info.capabilities;

	if (caps.features2 & EGraphicsFeatures2_RayMicromapOpacityU8) {
		Test_print(t, "Repeating the OMM special index pair with R8u indices");
		TestShaders_ommSpecialIndexWithFormat(t, deviceRef, file, positions, output, emptyList, ETextureFormatId_R8u);
	}

	else Test_print(t, "Device lacks 8-bit OMM indices, R8u trace pair skipped");
}

//The whole ray pipeline vehicle (scene, SBT, dispatch and readback) shared by the plain and the SER
//variant, which differ only in which oiSH drives it: both trace the same 4 rays at the same triangle and
//have to land on the same (1, 1, 0, 0).

static void TestShaders_raysWithFile(
	Test *t, GraphicsDeviceRef *deviceRef, const C8 *moduleName, const C8 *path, Bool testApiExtras
) {

	Test_setModule(t, moduleName);

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!(device->info.capabilities.features & EGraphicsFeatures_RayPipeline)) {
		Test_print(t, "Device lacks raytracing pipelines, skipping ray trace tests");
		return;
	}

	if (!device->defaultDescriptorTable) {
		Test_print(t, "Device has no bindless descriptor table, skipping ray trace tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	SHFile file {};

	if (!TestShaders_loadFile(t, path, &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping ray trace tests");
		return;
	}

	GraphicsInstanceRef *ownInstanceRef = NULL;
	GraphicsDeviceRef *ownDeviceRef = NULL;
	RefPtrType instanceType {};

	if (!TestShaders_rtDedicatedDevice(t, &deviceRef, &ownInstanceRef, &ownDeviceRef, &instanceType)) {
		SHFile_free(&file, alloc);
		return;
	}

	device = GraphicsDeviceRef_ptr(deviceRef);

	DeviceBufferRef *positions = NULL;
	DeviceBufferRef *output = NULL;
	BLASRef *blas = NULL;
	TLASRef *tlas = NULL;
	PipelineRef *pipeline = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;
	CommandListRef *refitList = NULL;
	CommandListRef *blasRefitList = NULL;

	const F32 triangle[12] = {
		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1
	};

	Buffer triData = Buffer_createRefConst(triangle, sizeof(triangle));
	CharString name = CharString_createRefCStrConst("Ray trace positions");

	//CPUBacked because the BLAS refit below rewrites these positions in place and marks them dirty, which is
	// what a refit reads: the BLAS keeps pointing at this same buffer.

	if(!Test_assert(t, "createPositions", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_CPUBacked, NULL,
		&name, &triData, &positions, &t->err
	)))
		goto clean;

	//Scoped so the goto above jumps around these rather than into them.
	{
	const DeviceData positionData = { .buffer = positions };
	name = CharString_createRefCStrConst("Ray trace BLAS");

	//AllowUpdate on the parent because the refit below updates FROM this one, and both APIs require the source
	// of an update to have been built with it.
	//Our own validation only checks the refit's flags, so leaving it off here fails in the driver instead.

	const BLASCreateInfo blasInfo = BLASCreateInfo_unindexed(
		ERTASBuildFlags_AllowUpdate, EBLASFlag_None, ETextureFormatId_RGBA32f, 0, 16, positionData
	);

	if(!Test_assert(t, "createBlas", GraphicsDeviceRef_createBLASExt(deviceRef, &blasInfo, &name, &blas, &t->err)))
		goto clean;

	TLASInstance instance = {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
		.data = {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (U32) ETLASInstanceFlag_Default << 24,
			.blasCpu = blas
		}
	};

	ListTLASInstance instances {};
	ListTLASInstance_createRefConst(&instance, 1, &instances, NULL);

	name = CharString_createRefCStrConst("Ray trace TLAS");

	if(!Test_assert(t, "createTlas", GraphicsDeviceRef_createTLASExt(
		deviceRef, (ERTASBuildFlags) (ERTASBuildFlags_DefaultTLAS | ERTASBuildFlags_AllowUpdate),
		&instances, false, NULL,
		&name, &tlas, &t->err
	)))
		goto clean;

	//The whole point of this TLAS is being reachable from the raygen shader

	Test_assert(t, "tlasHandle", TLASRef_ptr(tlas)->handle != BindlessDescriptor_None);

	name = CharString_createRefCStrConst("Ray trace output");

	Test_assert(t, "createOutput", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		(EGraphicsResourceFlag) (EGraphicsResourceFlag_ShaderWriteBindless | EGraphicsResourceFlag_CPUBacked),
		NULL, &name, 4 * sizeof(U32), &output, &t->err
	));

	//Raygen, miss and closest hit in that stage order; the one triangle group points at stage 2

	const U32 raygenId = TestShaders_entry(t, deviceRef, &file, "mainRaygen");
	const U32 missId = TestShaders_entry(t, deviceRef, &file, "mainMiss");
	const U32 hitId = TestShaders_entry(t, deviceRef, &file, "mainClosestHit");

	if(!output || raygenId == U32_MAX || missId == U32_MAX || hitId == U32_MAX)
		goto clean;

	PipelineStage stages[3] = {
		{ .binaryId = raygenId },
		{ .binaryId = missId },
		{ .binaryId = hitId }
	};

	ListPipelineStage stageList {};
	ListPipelineStage_createRefConst(stages, 3, &stageList, NULL);

	ListSHFile fileList {};
	ListSHFile_createRefConst(&file, 1, &fileList, NULL);

	PipelineRaytracingGroup group = {
		.closestHit = 2, .anyHit = U32_MAX, .intersection = U32_MAX
	};

	ListPipelineRaytracingGroup groupList {};
	ListPipelineRaytracingGroup_createRefConst(&group, 1, &groupList, NULL);

	const PipelineRaytracingInfo info = {
		.flags = EPipelineRaytracingFlags_Default,
		.maxRecursionDepth = 1
	};

	name = CharString_createRefCStrConst("Ray trace pipeline");

	TestShaders_raysLayout(t, deviceRef, &file, raygenId);

	if(!Test_assert(t, "createPipeline", GraphicsDeviceRef_createPipelineRaytracingExt(
		deviceRef, &stageList, &fileList, &groupList, &info, &name, EPipelineFlags_None, raysPushLayout, &pipeline, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "createList", GraphicsDeviceRef_createCommandList(
		deviceRef, 4 * KIBI, 64, 16, true, &commandList, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "createEmptyList", GraphicsDeviceRef_createCommandList(
		deviceRef, KIBI, 16, 8, true, &emptyList, &t->err
	)))
		goto clean;

	Test_assert(t, "beginEmptyList", CommandListRef_begin(emptyList, true, U64_MAX, &t->err));
	Test_assert(t, "endEmptyList", CommandListRef_end(emptyList, &t->err));

	//Build the scene and trace in one submit, split into scopes for the same dependency reason as the AS module

	raysPushData[0] = DeviceBufferRef_ptr(output)->writeHandle;
	raysPushData[1] = TLASRef_ptr(tlas)->handle;

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

	Test_assert(t, "scopeBlas", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
	Test_assert(t, "updateBlas", CommandListRef_updateBLASExt(commandList, blas, &t->err));
	Test_assert(t, "scopeBlasEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "scopeTlas", CommandListRef_startScope(commandList, NULL, 2, NULL, &t->err));
	Test_assert(t, "updateTlas", CommandListRef_updateTLASExt(commandList, tlas, &t->err));
	Test_assert(t, "scopeTlasEnd", CommandListRef_endScope(commandList, &t->err));

	const Transition traceTransitions[2] = {
		{ .resource = output, .stage = EPipelineStage_RaygenExt, .isWrite = true },
		{ .resource = tlas, .stage = EPipelineStage_RaygenExt }
	};

	ListTransition traceTransitionList {};
	ListTransition_createRefConst(traceTransitions, 2, &traceTransitionList, NULL);

	Test_assert(t, "scopeTrace", CommandListRef_startScope(commandList, &traceTransitionList, 3, NULL, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setRaytracingPipeline(commandList, pipeline, &t->err));
		TestShaders_pushRays(t, commandList);
	Test_assert(t, "trace", CommandListRef_dispatch1DRaysExt(commandList, 0, 4, &t->err));
	Test_assert(t, "scopeTraceEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if(!TestShaders_submitAndWait(t, deviceRef, commandList))
		goto clean;

	if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

		const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

		Test_assert(t, "rayResults", values[0] == 1 && values[1] == 1 && !values[2] && !values[3]);
	}

	//Refit the TLAS by moving its one instance far along Z, so the same 4 rays can no longer reach the triangle
	// and every one has to miss.
	//That is what separates a refit that ran from one that silently did nothing: a no-op refit still leaves a
	// valid AS describing the ORIGINAL scene, so it would keep reporting the two hits above.
	//A refit is in place, so this is the SAME TLAS throughout: no second object, no reallocation, and the
	// bindless handle the shader was already given keeps working, which the assert below pins down.
	//Only one of the two ray variants runs this and the blocks after it: refit and opacity micromaps are API
	// level rather than SER specific, so there is no reason to pay for them twice.

	if (testApiExtras) {

		const BindlessDescriptor handleBeforeRefit = TLASRef_ptr(tlas)->handle;

		TLASInstance moved = instance;
		moved.transform[2][3] = 1000;                //Translate Z; the transform is row major 3x4

		ListTLASInstance movedInstances {};
		ListTLASInstance_createRefConst(&moved, 1, &movedInstances, NULL);

		Bool madeRefit = Test_assert(t, "setInstancesMoved", TLASRef_setInstancesExt(tlas, &movedInstances, &t->err));

		madeRefit &= Test_assert(t, "createRefitList", GraphicsDeviceRef_createCommandList(
			deviceRef, KIBI, 16, 8, true, &refitList, &t->err
		));

		if (madeRefit) {

			//The scene the shader reads is reached through the same handle as before, so the push block that
			// drove the first trace drives this one unchanged.

			Test_assert(t, "refitKeepsHandle", TLASRef_ptr(tlas)->handle == handleBeforeRefit);

			Test_assert(t, "beginRefit", CommandListRef_begin(refitList, true, U64_MAX, &t->err));

			Test_assert(t, "scopeRefit", CommandListRef_startScope(refitList, NULL, 4, NULL, &t->err));
			Test_assert(t, "updateTlasRefit", CommandListRef_updateTLASExt(refitList, tlas, &t->err));
			Test_assert(t, "scopeRefitEnd", CommandListRef_endScope(refitList, &t->err));

			Test_assert(t, "scopeTraceRefit", CommandListRef_startScope(
				refitList, &traceTransitionList, 5, NULL, &t->err
			));

			Test_assert(t, "bindPipelineRefit", CommandListRef_setRaytracingPipeline(refitList, pipeline, &t->err));
		TestShaders_pushRays(t, refitList);
			Test_assert(t, "traceRefit", CommandListRef_dispatch1DRaysExt(refitList, 0, 4, &t->err));
			Test_assert(t, "scopeTraceRefitEnd", CommandListRef_endScope(refitList, &t->err));

			Test_assert(t, "endRefit", CommandListRef_end(refitList, &t->err));

			if (TestShaders_submitAndWait(t, deviceRef, refitList))
				if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

					const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;
					Test_assert(t, "rayResultsRefit", !values[0] && !values[1] && !values[2] && !values[3]);
				}

			//Refit straight back to where it started.
			//This is the case the old copy based design could not express without a third acceleration
			// structure that pinned the second one, which pinned the first: chaining refits grew memory for as
			// long as the chain was alive.
			//In place it is the same object every time, so this costs nothing at all, and landing back on the
			// original result proves a second refit reads the state the first one left rather than the state
			// the AS was originally built from.

			ListTLASInstance backInstances {};
			ListTLASInstance_createRefConst(&instance, 1, &backInstances, NULL);

			if (Test_assert(t, "setInstancesBack", TLASRef_setInstancesExt(tlas, &backInstances, &t->err))) {

				Test_assert(t, "beginRefitBack", CommandListRef_begin(refitList, true, U64_MAX, &t->err));

				Test_assert(t, "scopeRefitBack", CommandListRef_startScope(refitList, NULL, 4, NULL, &t->err));
				Test_assert(t, "updateTlasRefitBack", CommandListRef_updateTLASExt(refitList, tlas, &t->err));
				Test_assert(t, "scopeRefitBackEnd", CommandListRef_endScope(refitList, &t->err));

				Test_assert(t, "scopeTraceRefitBack", CommandListRef_startScope(
					refitList, &traceTransitionList, 5, NULL, &t->err
				));

				Test_assert(t, "bindPipelineRefitBack", CommandListRef_setRaytracingPipeline(
					refitList, pipeline, &t->err
				));
				TestShaders_pushRays(t, refitList);

				Test_assert(t, "traceRefitBack", CommandListRef_dispatch1DRaysExt(refitList, 0, 4, &t->err));
				Test_assert(t, "scopeTraceRefitBackEnd", CommandListRef_endScope(refitList, &t->err));
				Test_assert(t, "endRefitBack", CommandListRef_end(refitList, &t->err));

				if (TestShaders_submitAndWait(t, deviceRef, refitList))
					if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

						const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

						Test_assert(
							t, "rayResultsRefitBack",
							values[0] == 1 && values[1] == 1 && !values[2] && !values[3]
						);
					}
			}
		}

		//Opacity micromaps run here rather than at the end, because the BLAS refit below rewrites the position
		// buffer these BLASes are built over and would leave them tracing a triangle that moved away.

		TestShaders_ommSpecialIndex(t, deviceRef, &file, positions, output, emptyList);
		TestShaders_ommMicromapArray(t, deviceRef, &file, positions, output, emptyList);

		//The same idea one level down, so the BLAS update path gets the same treatment.
		//Here the triangle itself moves rather than the instance, by rewriting the position buffer the BLAS
		// already reads; the TLAS is refitted straight after because an instance caches the bounds of the BLAS
		// it points at, so a BLAS that changed leaves every TLAS over it stale.

		Bool madeBlasRefit = Test_assert(t, "createBlasRefitList", GraphicsDeviceRef_createCommandList(
			deviceRef, 2 * KIBI, 32, 16, true, &blasRefitList, &t->err
		));

		if (madeBlasRefit) {

			F32 *positionData2 = (F32*) DeviceBufferRef_ptr(positions)->cpuData.ptrNonConst;

			for(U64 i = 0; i < 3; ++i)
				positionData2[i * 4 + 2] = 1000;                //Z of every vertex, the stride is 4 floats

			madeBlasRefit = Test_assert(t, "markPositionsDirty", DeviceBufferRef_markDirty(
				positions, 0, sizeof(triangle), &t->err
			));
		}

		if (madeBlasRefit) {

			//The OMM helpers above share this block and leave it pointing at a TLAS of their own that is
			//already destroyed, so it is restored before anything records against it again.

			raysPushData[0] = DeviceBufferRef_ptr(output)->writeHandle;
			raysPushData[1] = TLASRef_ptr(tlas)->handle;

			Test_assert(t, "beginBlasRefit", CommandListRef_begin(blasRefitList, true, U64_MAX, &t->err));

			Test_assert(t, "scopeBlasRefit", CommandListRef_startScope(blasRefitList, NULL, 6, NULL, &t->err));
			Test_assert(t, "updateBlasRefit", CommandListRef_updateBLASExt(blasRefitList, blas, &t->err));
			Test_assert(t, "scopeBlasRefitEnd", CommandListRef_endScope(blasRefitList, &t->err));

			Test_assert(t, "scopeTlasAfterBlas", CommandListRef_startScope(blasRefitList, NULL, 7, NULL, &t->err));
			Test_assert(t, "updateTlasAfterBlas", CommandListRef_updateTLASExt(blasRefitList, tlas, &t->err));
			Test_assert(t, "scopeTlasAfterBlasEnd", CommandListRef_endScope(blasRefitList, &t->err));

			Test_assert(t, "scopeTraceBlasRefit", CommandListRef_startScope(
				blasRefitList, &traceTransitionList, 8, NULL, &t->err
			));

			Test_assert(t, "bindPipelineBlasRefit", CommandListRef_setRaytracingPipeline(
				blasRefitList, pipeline, &t->err
			));
			TestShaders_pushRays(t, blasRefitList);

			Test_assert(t, "traceBlasRefit", CommandListRef_dispatch1DRaysExt(blasRefitList, 0, 4, &t->err));
			Test_assert(t, "scopeTraceBlasRefitEnd", CommandListRef_endScope(blasRefitList, &t->err));

			Test_assert(t, "endBlasRefit", CommandListRef_end(blasRefitList, &t->err));

			if (TestShaders_submitAndWait(t, deviceRef, blasRefitList))
				if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

					const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;
					Test_assert(t, "rayResultsBlasRefit", !values[0] && !values[1] && !values[2] && !values[3]);
				}
		}

	}

	}

clean:

	RefPtr_dec(&blasRefitList);
	RefPtr_dec(&refitList);
	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&tlas);
	RefPtr_dec(&blas);
	RefPtr_dec(&output);
	RefPtr_dec(&positions);

	//A module static so every pipeline here shares one layout, which means it outlives the scope that
	//created it and has to be released at the end of the module instead.

	RefPtr_dec(&raysPushLayout);

	SHFile_free(&file, alloc);

	TestShaders_rtDedicatedDeviceEnd(t, &ownInstanceRef, &ownDeviceRef);
}

void Test_graphicsShaderRays(Test *t, GraphicsDeviceRef *deviceRef) {

	TestShaders_raysWithFile(t, deviceRef, "Shaders/rays", "//OxC3_gtest/test_shaders/test_rays.oiSH", true);

	//The SER variant records the hit as a HitObject, hints the scheduler with MaybeReorderThread,
	// then invokes the recorded shader explicitly.
	//Reordering itself is unobservable by design,
	// so what is checked is that the split path lands on exactly the same payloads as the plain TraceRay above.
	//Running with the reorder hint in place is also the only "doesn't break" coverage RayReorderActual can
	// get: a device that claims to actually reorder still has to produce identical results.
	//An experimental claim is skipped: on Vulkan the NV device extension can't accept the EXT SPIR-V the
	// shader stack emits, and on D3D12 the SM6.9 the shaders need is preview only.

	const GraphicsDeviceCapabilities caps = GraphicsDeviceRef_ptr(deviceRef)->info.capabilities;

	if(!(caps.features & EGraphicsFeatures_RayReorder))
		return;

	if (caps.experimentalFeatures & EGraphicsFeatures_RayReorder) {
		Test_print(t, "RayReorder claimed but experimental on this backend, skipping SER trace test");
		return;
	}

	TestShaders_raysWithFile(
		t, deviceRef, "Shaders/raysSer", "//OxC3_gtest/test_shaders/test_rays_ser.oiSH", false
	);
}
} }
