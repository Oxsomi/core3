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

//graphics/test/interface/test_graphics_bindful_rays.cpp
//
//Bindful ray tracing: a TLAS reached through a table instead of a handle, opacity micromaps, and inline
//ray tracing issued from a graphics stage.
//Split out of test_graphics_bindful.c, which had grown to 24 modules in one file.

namespace oxc { namespace c {
	#include "types/base/string_base.h"
	#include "types/container/list_basic_types.h"
	#include "types/test/test.h"
	#include "formats/oiSH/sh_file.h"
	#include "formats/oiSH/sh_registers.h"
	#include "platforms/platform.h"
	#include "graphics/generic/blas.h"
	#include "graphics/generic/command_list.h"
	#include "graphics/generic/commands.h"
	#include "graphics/generic/depth_stencil.h"
	#include "graphics/generic/descriptor_heap.h"
	#include "graphics/generic/descriptor_layout.h"
	#include "graphics/generic/descriptor_table.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_buffer.h"
	#include "graphics/generic/device_info.h"
	#include "graphics/generic/device_texture.h"
	#include "graphics/generic/graphics_types.h"
	#include "graphics/generic/instance.h"
	#include "graphics/generic/pipeline.h"
	#include "graphics/generic/pipeline_layout.h"
	#include "graphics/generic/render_texture.h"
	#include "graphics/generic/sampler.h"
	#include "graphics/generic/tlas.h"
	#include "test_graphics_shared.h"
} }

//Same namespace the C headers landed in, so the definitions here match the declarations in
//test_graphics_shared.h and the macros in those headers still expand to names that resolve.

namespace oxc { namespace c {

// -- 46. Bindful ray tracing: the TLAS through a table instead of a handle -------

//Ray tracing itself never needed bindless; this proves it by tracing the same 4 ray scene as the bindless
// rays module with the TLAS and output buffer coming from classic registers.
//The TLAS is created with disallowBindlessDescriptor, so this works on a device without bindless at all.

void Test_graphicsBindfulRays(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/rays");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!(device->info.capabilities.features & EGraphicsFeatures_RayPipeline)) {
		Test_print(t, "Device lacks raytracing pipelines, skipping bindful ray trace tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *positions = NULL;
	DeviceBufferRef *output = NULL;
	BLASRef *blas = NULL;
	TLASRef *tlas = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file {};
	DescriptorLayoutInfo layoutInfo {};
	ListU32 entrypoints {};

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_rays.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful ray trace tests");
		return;
	}

	GraphicsInstanceRef *ownInstanceRef = NULL;
	GraphicsDeviceRef *ownDeviceRef = NULL;
	RefPtrType instanceType {};

	if (!TestShaders_rtDedicatedDevice(t, &deviceRef, &ownInstanceRef, &ownDeviceRef, &instanceType)) {
		SHFile_free(&file, alloc);
		return;
	}

	//The same one triangle scene the bindless rays module uses

	const F32 triangle[12] = {
		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1
	};

	Buffer triData = Buffer_createRefConst(triangle, sizeof(triangle));
	CharString name = CharString_createRefCStrConst("Bindful rays positions");

	//CPUBacked because the BLAS refit at the end rewrites these positions in place and marks them dirty

	if(!Test_assert(t, "createPositions", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_CPUBacked, NULL,
		&name, &triData, &positions, &t->err
	)))
		goto clean;

	const BLASCreateInfo blasInfo = BLASCreateInfo_unindexed(
		ERTASBuildFlags_AllowUpdate, EBLASFlag_None, ETextureFormatId_RGBA32f, 0, 16,
		{ .buffer = positions }
	);

	name = CharString_createRefCStrConst("Bindful rays BLAS");

	if(!Test_assert(t, "createBlas", GraphicsDeviceRef_createBLASExt(deviceRef, &blasInfo, &name, &blas, &t->err)))
		goto clean;

	//Scoped so the goto above jumps around these rather than into them.
	{
	const TLASInstance instance = {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
		.data = {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (U32) ETLASInstanceFlag_Default << 24,
			.blasCpu = blas
		}
	};

	ListTLASInstance instances {};
	ListTLASInstance_createRefConst(&instance, 1, &instances, NULL);

	name = CharString_createRefCStrConst("Bindful rays TLAS");

	if(!Test_assert(t, "createTlas", GraphicsDeviceRef_createTLASExt(
		deviceRef, (ERTASBuildFlags) (ERTASBuildFlags_DefaultTLAS | ERTASBuildFlags_AllowUpdate),
		&instances, true, NULL,
		&name, &tlas, &t->err
	)))
		goto clean;

	const U32 raygenId = TestShaders_entry(t, deviceRef, &file, "mainRaygen");
	const U32 missId = TestShaders_entry(t, deviceRef, &file, "mainMiss");
	const U32 hitId = TestShaders_entry(t, deviceRef, &file, "mainClosestHit");

	if(raygenId == U32_MAX || missId == U32_MAX || hitId == U32_MAX)
		goto clean;

	//The layout comes from all three stages' reflection at once

	const U32 entryIds[3] = { raygenId, missId, hitId };

	Test_assert(t, "entrypointsRef", ListU32_createRefConst(entryIds, 3, &entrypoints, &t->err));

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntries(
		deviceRef, &file, &entrypoints, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful rays layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = { .maxAccelerationStructures = 1,
		.maxBuffersRW = 1, .maxDescriptorTables = 1
	};

	name = CharString_createRefCStrConst("Bindful rays heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful rays table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful rays output");

	if(!Test_assert(t, "outputCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		(EGraphicsResourceFlag) (EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked),
		NULL, &name, 4 * sizeof(U32), &output, &t->err
	)))
		goto clean;

	const Descriptor tlasDesc = Descriptor_tlas(tlas);
	const Descriptor outputDesc = Descriptor_buffer(output, 0, 0, NULL, 0);

	const CharString sceneName = CharString_createRefCStrConst("scene");
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setTlas", DescriptorTableRef_setDescriptorByName(table, &sceneName, 0, false, &tlasDesc, &t->err));
	Test_assert(t, "setOutput", DescriptorTableRef_setDescriptorByName(table, &outputName, 0, false, &outputDesc, &t->err));

	PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout };
	name = CharString_createRefCStrConst("Bindful rays pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
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

	name = CharString_createRefCStrConst("Bindful rays pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineRaytracingExt(
		deviceRef, &stageList, &fileList, &groupList, &info, &name, EPipelineFlags_None,
		pipelineLayout, &pipeline, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "listCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, 4 * KIBI, 64, 16, true, &commandList, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "emptyListCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, KIBI, 16, 8, true, &emptyList, &t->err
	)))
		goto clean;

	Test_assert(t, "beginEmpty", CommandListRef_begin(emptyList, true, U64_MAX, &t->err));
	Test_assert(t, "endEmpty", CommandListRef_end(emptyList, &t->err));

	const Transition traceTransitions[2] = {
		{ .resource = output, .stage = EPipelineStage_RaygenExt, .isWrite = true },
		{ .resource = tlas, .stage = EPipelineStage_RaygenExt }
	};

	ListTransition traceTransitionList {};
	ListTransition_createRefConst(traceTransitions, 2, &traceTransitionList, NULL);

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

	Test_assert(t, "scopeBlas", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
	Test_assert(t, "updateBlas", CommandListRef_updateBLASExt(commandList, blas, &t->err));
	Test_assert(t, "scopeBlasEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "scopeTlas", CommandListRef_startScope(commandList, NULL, 2, NULL, &t->err));
	Test_assert(t, "updateTlas", CommandListRef_updateTLASExt(commandList, tlas, &t->err));
	Test_assert(t, "scopeTlasEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "scopeTrace", CommandListRef_startScope(commandList, &traceTransitionList, 3, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTable", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setRaytracingPipeline(commandList, pipeline, &t->err));
	Test_assert(t, "trace", CommandListRef_dispatch1DRaysExt(commandList, 0, 4, &t->err));
	Test_assert(t, "scopeTraceEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			Test_assert(t, "bindfulRayResults", values[0] == 1 && values[1] == 1 && !values[2] && !values[3]);
		}

	//Refit in place: the same TLAS object gets new instance data and is rebuilt rather than recreated, so
	// the descriptor written into the table earlier has to keep addressing it. Moving the instance far along
	// Z takes it out of every ray's path, so all four rays must miss without the table being touched again.

	const TLASInstance movedInstance = {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 1000 } },
		.data = {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (U32) ETLASInstanceFlag_Default << 24,
			.blasCpu = blas
		}
	};

	ListTLASInstance movedInstances {};
	ListTLASInstance_createRefConst(&movedInstance, 1, &movedInstances, NULL);

	if (Test_assert(t, "setInstancesMoved", TLASRef_setInstancesExt(tlas, &movedInstances, &t->err))) {

		Test_assert(t, "beginRefit", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		Test_assert(t, "scopeRefit", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
		Test_assert(t, "updateTlasRefit", CommandListRef_updateTLASExt(commandList, tlas, &t->err));
		Test_assert(t, "scopeRefitEnd", CommandListRef_endScope(commandList, &t->err));

		Test_assert(t, "scopeTraceRefit", CommandListRef_startScope(
			commandList, &traceTransitionList, 2, NULL, &t->err
		));

		Test_assert(t, "bindHeapRefit", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
		Test_assert(t, "bindTableRefit", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
		Test_assert(t, "bindPipelineRefit", CommandListRef_setRaytracingPipeline(commandList, pipeline, &t->err));
		Test_assert(t, "traceRefit", CommandListRef_dispatch1DRaysExt(commandList, 0, 4, &t->err));
		Test_assert(t, "scopeTraceRefitEnd", CommandListRef_endScope(commandList, &t->err));

		Test_assert(t, "endRefit", CommandListRef_end(commandList, &t->err));

		if (TestShaders_submitAndWait(t, deviceRef, commandList))
			if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

				const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

				Test_assert(t, "refitMissesAll", !values[0] && !values[1] && !values[2] && !values[3]);
			}

		//Refit back to where it started: the second refit reads the state the first one left behind, so a
		// refit that quietly rebuilt from the original instances instead would land on the wrong result here

		if (Test_assert(t, "setInstancesBack", TLASRef_setInstancesExt(tlas, &instances, &t->err))) {

			Test_assert(t, "beginRefitBack", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

			Test_assert(t, "scopeRefitBack", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
			Test_assert(t, "updateTlasRefitBack", CommandListRef_updateTLASExt(commandList, tlas, &t->err));
			Test_assert(t, "scopeRefitBackEnd", CommandListRef_endScope(commandList, &t->err));

			Test_assert(t, "scopeTraceBack", CommandListRef_startScope(
				commandList, &traceTransitionList, 2, NULL, &t->err
			));

			Test_assert(t, "bindHeapBack", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
			Test_assert(t, "bindTableBack", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
			Test_assert(t, "bindPipelineBack", CommandListRef_setRaytracingPipeline(commandList, pipeline, &t->err));
			Test_assert(t, "traceBack", CommandListRef_dispatch1DRaysExt(commandList, 0, 4, &t->err));
			Test_assert(t, "scopeTraceBackEnd", CommandListRef_endScope(commandList, &t->err));

			Test_assert(t, "endRefitBack", CommandListRef_end(commandList, &t->err));

			if (TestShaders_submitAndWait(t, deviceRef, commandList))
				if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

					const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

					Test_assert(t, "refitBackHitsAgain", values[0] == 1 && values[1] == 1 && !values[2] && !values[3]);
				}
		}

		//Refitting the geometry itself rather than the instance: the triangle's own vertices move out of
		// reach, so the BLAS has to rebuild from the rewritten positions and the TLAS after it. Both rays
		// missing proves the new vertex data really reached the bottom level structure.

		F32 *movedPositions = (F32*) DeviceBufferRef_ptr(positions)->cpuData.ptrNonConst;

		for(U64 i = 0; i < 3; ++i)
			movedPositions[i * 4 + 2] = 1000;                //Z of every vertex, the stride is 4 floats

		if (Test_assert(t, "markPositionsDirty", DeviceBufferRef_markDirty(positions, 0, sizeof(triangle), &t->err))) {

			Test_assert(t, "beginBlasRefit", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

			Test_assert(t, "scopeBlasRefit", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
			Test_assert(t, "updateBlasRefit", CommandListRef_updateBLASExt(commandList, blas, &t->err));
			Test_assert(t, "scopeBlasRefitEnd", CommandListRef_endScope(commandList, &t->err));

			Test_assert(t, "scopeTlasAfterBlas", CommandListRef_startScope(commandList, NULL, 2, NULL, &t->err));
			Test_assert(t, "updateTlasAfterBlas", CommandListRef_updateTLASExt(commandList, tlas, &t->err));
			Test_assert(t, "scopeTlasAfterBlasEnd", CommandListRef_endScope(commandList, &t->err));

			Test_assert(t, "scopeTraceBlasRefit", CommandListRef_startScope(
				commandList, &traceTransitionList, 3, NULL, &t->err
			));

			Test_assert(t, "bindHeapBlasRefit", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
			Test_assert(t, "bindTableBlasRefit", CommandListRef_bindDescriptorTable(commandList, table, &t->err));

			Test_assert(t, "bindPipelineBlasRefit", CommandListRef_setRaytracingPipeline(
				commandList, pipeline, &t->err
			));

			Test_assert(t, "traceBlasRefit", CommandListRef_dispatch1DRaysExt(commandList, 0, 4, &t->err));
			Test_assert(t, "scopeTraceBlasRefitEnd", CommandListRef_endScope(commandList, &t->err));

			Test_assert(t, "endBlasRefit", CommandListRef_end(commandList, &t->err));

			if (TestShaders_submitAndWait(t, deviceRef, commandList))
				if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

					const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

					Test_assert(t, "blasRefitMissesAll", !values[0] && !values[1] && !values[2] && !values[3]);
				}
		}
	}

	}

clean:

	if(table) {
		DescriptorTableRef_unsetDescriptors(table, 0, 0, 1, NULL);
		DescriptorTableRef_unsetDescriptors(table, 1, 0, 1, NULL);
	}

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&output);
	RefPtr_dec(&tlas);
	RefPtr_dec(&blas);
	RefPtr_dec(&positions);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);

	TestShaders_rtDedicatedDeviceEnd(t, &ownInstanceRef, &ownDeviceRef);
}

// -- 60. Opacity micromaps through a bindful table ------------------------------

//Opacity micromaps sit below the binding model entirely, but their only execution coverage runs through
// bindless handles, so a device without bindless never traces one. The special index form is enough to
// prove the path end to end: one triangle marked fully opaque must be hit, the same triangle marked fully
// transparent must be missed, and the difference can only come from the micromap indices being consumed.
//The pipeline has to opt in, since both APIs ignore micromaps otherwise and would report hits either way.

static void TestBindful_ommWithFormat(
	Test *t,
	GraphicsDeviceRef *deviceRef,
	const SHFile *file,
	DeviceBufferRef *positions,
	DescriptorHeapRef *heap,
	DescriptorTableRef *table,
	PipelineLayoutRef *pipelineLayout,
	DeviceBufferRef *output,
	CommandListRef *emptyList,
	ETextureFormatId ommIndexFormat
) {

	DeviceBufferRef *indices = NULL;
	DeviceBufferRef *ommOpaque = NULL;
	DeviceBufferRef *ommTransparent = NULL;
	BLASRef *blasOpaque = NULL;
	BLASRef *blasTransparent = NULL;
	TLASRef *tlasOpaque = NULL;
	TLASRef *tlasTransparent = NULL;
	PipelineRef *pipeline = NULL;
	CommandListRef *commandList = NULL;

	const U16 triangleIndices[3] = { 0, 1, 2 };
	Buffer indexData = Buffer_createRefConst(triangleIndices, sizeof(triangleIndices));
	CharString name = CharString_createRefCStrConst("Bindful OMM indices");

	if(!Test_assert(t, "ommCreateIndices", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &indexData, &indices, &t->err
	)))
		goto clean;

	//One triangle, so each micromap index buffer is exactly one element wide

	//Scoped so the goto above jumps around these rather than into them.
	{
	const U8 ommStride = ommIndexFormat == ETextureFormatId_R32u ? 4 : (ommIndexFormat == ETextureFormatId_R16u ? 2 : 1);

	const U32 opaqueIndex = EOMMSpecialIndex_pack(EOMMSpecialIndex_FullyOpaque, ommIndexFormat);
	const U32 transparentIndex = EOMMSpecialIndex_pack(EOMMSpecialIndex_FullyTransparent, ommIndexFormat);

	Buffer opaqueData = Buffer_createRefConst(&opaqueIndex, ommStride);
	Buffer transparentData = Buffer_createRefConst(&transparentIndex, ommStride);

	name = CharString_createRefCStrConst("Bindful OMM indices, fully opaque");

	if(!Test_assert(t, "ommCreateOpaque", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &opaqueData, &ommOpaque, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful OMM indices, fully transparent");

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

	name = CharString_createRefCStrConst("Bindful OMM BLAS, fully opaque");

	if(!Test_assert(t, "ommCreateBlasOpaque", GraphicsDeviceRef_createBLASExt(
		deviceRef, &opaqueInfo, &name, &blasOpaque, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful OMM BLAS, fully transparent");

	if(!Test_assert(t, "ommCreateBlasTransparent", GraphicsDeviceRef_createBLASExt(
		deviceRef, &transparentInfo, &name, &blasTransparent, &t->err
	)))
		goto clean;

	//DisableCulling so a back facing hit still counts, exactly as the bindless micromap test does

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

	name = CharString_createRefCStrConst("Bindful OMM TLAS, fully opaque");

	if(!Test_assert(t, "ommCreateTlasOpaque", GraphicsDeviceRef_createTLASExt(
		deviceRef, ERTASBuildFlags_DefaultTLAS, &ommInstances, true, NULL, &name, &tlasOpaque, &t->err
	)))
		goto clean;

	ommInstance.data.blasCpu = blasTransparent;
	name = CharString_createRefCStrConst("Bindful OMM TLAS, fully transparent");

	if(!Test_assert(t, "ommCreateTlasTransparent", GraphicsDeviceRef_createTLASExt(
		deviceRef, ERTASBuildFlags_DefaultTLAS, &ommInstances, true, NULL, &name, &tlasTransparent, &t->err
	)))
		goto clean;

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

	ListSHFile fileList {};
	ListSHFile_createRefConst(file, 1, &fileList, NULL);

	PipelineRaytracingGroup group = {
		.closestHit = 2, .anyHit = U32_MAX, .intersection = U32_MAX
	};

	ListPipelineRaytracingGroup groupList {};
	ListPipelineRaytracingGroup_createRefConst(&group, 1, &groupList, NULL);

	//Without the opt in both APIs ignore the micromap and the transparent triangle would report hits

	const PipelineRaytracingInfo info = {
		.flags = EPipelineRaytracingFlags_Default | EPipelineRaytracingFlags_AllowOpacityMicromapExt,
		.maxRecursionDepth = 1
	};

	name = CharString_createRefCStrConst("Bindful OMM pipeline");

	if(!Test_assert(t, "ommCreatePipeline", GraphicsDeviceRef_createPipelineRaytracingExt(
		deviceRef, &ommStageList, &fileList, &groupList, &info, &name, EPipelineFlags_None,
		pipelineLayout, &pipeline, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "ommCreateList", GraphicsDeviceRef_createCommandList(
		deviceRef, 4 * KIBI, 64, 16, true, &commandList, &t->err
	)))
		goto clean;

	//Fully opaque first: the same four rays the plain trace uses, so two of them have to hit

	for (U8 pass = 0; pass < 2; ++pass) {

		TLASRef *tlas = pass ? tlasTransparent : tlasOpaque;
		const Descriptor tlasDesc = Descriptor_tlas(tlas);
		const CharString sceneName = CharString_createRefCStrConst("scene");

		Test_assert(t, "ommSetTlas", DescriptorTableRef_setDescriptorByName(
			table, &sceneName, 0, true, &tlasDesc, &t->err
		));

		const Transition traceTransitions[2] = {
			{ .resource = output, .stage = EPipelineStage_RaygenExt, .isWrite = true },
			{ .resource = tlas, .stage = EPipelineStage_RaygenExt }
		};

		ListTransition traceTransitionList {};
		ListTransition_createRefConst(traceTransitions, 2, &traceTransitionList, NULL);

		Test_assert(t, "ommBegin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		Test_assert(t, "ommScopeBlas", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
		Test_assert(t, "ommUpdateBlas", CommandListRef_updateBLASExt(
			commandList, pass ? blasTransparent : blasOpaque, &t->err
		));
		Test_assert(t, "ommScopeBlasEnd", CommandListRef_endScope(commandList, &t->err));

		Test_assert(t, "ommScopeTlas", CommandListRef_startScope(commandList, NULL, 2, NULL, &t->err));
		Test_assert(t, "ommUpdateTlas", CommandListRef_updateTLASExt(commandList, tlas, &t->err));
		Test_assert(t, "ommScopeTlasEnd", CommandListRef_endScope(commandList, &t->err));

		Test_assert(t, "ommScopeTrace", CommandListRef_startScope(commandList, &traceTransitionList, 3, NULL, &t->err));
		Test_assert(t, "ommBindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
		Test_assert(t, "ommBindTable", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
		Test_assert(t, "ommBindPipeline", CommandListRef_setRaytracingPipeline(commandList, pipeline, &t->err));
		Test_assert(t, "ommTrace", CommandListRef_dispatch1DRaysExt(commandList, 0, 4, &t->err));
		Test_assert(t, "ommScopeTraceEnd", CommandListRef_endScope(commandList, &t->err));

		Test_assert(t, "ommEnd", CommandListRef_end(commandList, &t->err));

		if (TestShaders_submitAndWait(t, deviceRef, commandList))
			if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

				const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

				if(pass)
					Test_assert(t, "ommResultsTransparent", !values[0] && !values[1] && !values[2] && !values[3]);

				else Test_assert(t, "ommResultsOpaque", values[0] == 1 && values[1] == 1 && !values[2] && !values[3]);
			}
	}

	}

clean:

	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&tlasTransparent);
	RefPtr_dec(&tlasOpaque);
	RefPtr_dec(&blasTransparent);
	RefPtr_dec(&blasOpaque);
	RefPtr_dec(&ommTransparent);
	RefPtr_dec(&ommOpaque);
	RefPtr_dec(&indices);
}

void Test_graphicsBindfulOmm(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/omm");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	const GraphicsDeviceCapabilities caps = device->info.capabilities;

	if (!(caps.features & EGraphicsFeatures_RayPipeline)) {
		Test_print(t, "Device lacks raytracing pipelines, skipping bindful micromap tests");
		return;
	}

	if (!(caps.features & EGraphicsFeatures_RayMicromapOpacity)) {
		Test_print(t, "Device lacks opacity micromaps, skipping bindful micromap tests");
		return;
	}

	if (caps.experimentalFeatures & EGraphicsFeatures_RayMicromapOpacity) {
		Test_print(t, "Opacity micromaps claimed but experimental on this backend, skipping bindful micromap tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	DeviceBufferRef *positions = NULL;
	DeviceBufferRef *output = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file {};
	DescriptorLayoutInfo layoutInfo {};
	ListU32 entrypoints {};

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_rays.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful micromap tests");
		return;
	}

	GraphicsInstanceRef *ownInstanceRef = NULL;
	GraphicsDeviceRef *ownDeviceRef = NULL;
	RefPtrType instanceType {};

	if (!TestShaders_rtDedicatedDevice(t, &deviceRef, &ownInstanceRef, &ownDeviceRef, &instanceType)) {
		SHFile_free(&file, alloc);
		return;
	}

	const F32 triangle[12] = {
		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1
	};

	Buffer triData = Buffer_createRefConst(triangle, sizeof(triangle));
	CharString name = CharString_createRefCStrConst("Bindful OMM positions");

	if(!Test_assert(t, "createPositions", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &triData, &positions, &t->err
	)))
		goto clean;

	//Scoped so the goto above jumps around these rather than into them.
	{
	const U32 raygenId = TestShaders_entry(t, deviceRef, &file, "mainRaygen");
	const U32 missId = TestShaders_entry(t, deviceRef, &file, "mainMiss");
	const U32 hitId = TestShaders_entry(t, deviceRef, &file, "mainClosestHit");

	if(raygenId == U32_MAX || missId == U32_MAX || hitId == U32_MAX)
		goto clean;

	const U32 entryIds[3] = { raygenId, missId, hitId };

	Test_assert(t, "entrypointsRef", ListU32_createRefConst(entryIds, 3, &entrypoints, &t->err));

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntries(
		deviceRef, &file, &entrypoints, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful OMM layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = { .maxAccelerationStructures = 1,
		.maxBuffersRW = 1, .maxDescriptorTables = 1
	};

	name = CharString_createRefCStrConst("Bindful OMM heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful OMM table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful OMM output");

	if(!Test_assert(t, "outputCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		(EGraphicsResourceFlag) (EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked),
		NULL, &name, 4 * sizeof(U32), &output, &t->err
	)))
		goto clean;

	const Descriptor outputDesc = Descriptor_buffer(output, 0, 0, NULL, 0);
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setOutput", DescriptorTableRef_setDescriptorByName(table, &outputName, 0, false, &outputDesc, &t->err));

	PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout };
	name = CharString_createRefCStrConst("Bindful OMM pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "emptyListCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, KIBI, 16, 8, true, &emptyList, &t->err
	)))
		goto clean;

	Test_assert(t, "beginEmpty", CommandListRef_begin(emptyList, true, U64_MAX, &t->err));
	Test_assert(t, "endEmpty", CommandListRef_end(emptyList, &t->err));

	TestBindful_ommWithFormat(
		t, deviceRef, &file, positions, heap, table, pipelineLayout, output, emptyList, ETextureFormatId_R16u
	);

	//R8u indices need their own capability, since Vulkan's EXT extension forbids them and only the KHR
	// promotion or D3D12 accepts them

	if (caps.features2 & EGraphicsFeatures2_RayMicromapOpacityU8) {

		Test_print(t, "Repeating the bindful micromap pair with R8u indices");

		TestBindful_ommWithFormat(
			t, deviceRef, &file, positions, heap, table, pipelineLayout, output, emptyList, ETextureFormatId_R8u
		);
	}

	}

clean:

	if(table) {
		DescriptorTableRef_unsetDescriptors(table, 0, 0, 1, NULL);
		DescriptorTableRef_unsetDescriptors(table, 1, 0, 1, NULL);
	}

	RefPtr_dec(&emptyList);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&output);
	RefPtr_dec(&positions);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);

	TestShaders_rtDedicatedDeviceEnd(t, &ownInstanceRef, &ownDeviceRef);
}

// -- 61. Inline raytracing from a graphics stage --------------------------------

//Every other RayQuery test traces from compute, which hid a real backend bug: D3D12 accepts only a short
// list of sync scopes alongside the acceleration structure access bits, and the per stage graphics scopes
// are not on it, so a TLAS transitioned for a pixel shader produced a barrier the debug layer rejects.
// Compute maps to a legal scope, so no compute test could ever reach it.
//Tracing the same scene from a pixel shader is what puts a graphics stage on a TLAS transition.

void Test_graphicsBindfulRayQueryGraphics(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/rayQueryGraphics");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!(device->info.capabilities.features & EGraphicsFeatures_RayQuery)) {
		Test_print(t, "Device lacks ray query, skipping graphics stage ray query tests");
		return;
	}

	if (!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering)) {
		Test_print(t, "Device lacks direct rendering, skipping graphics stage ray query tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *positions = NULL;
	BLASRef *blas = NULL;
	TLASRef *tlas = NULL;
	RenderTextureRef *target = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile files[2] = { 0 };

	DescriptorLayoutInfo layoutInfo {};
	ListU32 entrypoints {};

	if (
		!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_draw_vs.oiSH", &files[0]) ||
		!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_rayquery_ps.oiSH", &files[1])
	) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping graphics ray query tests");
		SHFile_free(&files[0], alloc);
		return;
	}

	ListSHFile fileList {};
	ListSHFile_createRefConst(files, 2, &fileList, NULL);

	const U32 vertexId = TestShaders_entry(t, deviceRef, &files[0], "main");
	const U32 pixelId = TestShaders_entry(t, deviceRef, &files[1], "main");

	if(vertexId == U32_MAX || pixelId == U32_MAX) {
		Test_print(t, "No ray query entrypoint for this backend, skipping graphics ray query tests");
		goto clean;
	}

	//The same one triangle scene the other raytracing modules use

	//Scoped so the goto above jumps around these rather than into them.
	{
	const F32 triangle[12] = {
		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1
	};

	Buffer triData = Buffer_createRefConst(triangle, sizeof(triangle));
	CharString name = CharString_createRefCStrConst("Ray query graphics positions");

	if(!Test_assert(t, "createPositions", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &triData, &positions, &t->err
	)))
		goto clean;

	const BLASCreateInfo blasInfo = BLASCreateInfo_unindexed(
		ERTASBuildFlags_None, EBLASFlag_None, ETextureFormatId_RGBA32f, 0, 16,
		{ .buffer = positions }
	);

	name = CharString_createRefCStrConst("Ray query graphics BLAS");

	if(!Test_assert(t, "createBlas", GraphicsDeviceRef_createBLASExt(deviceRef, &blasInfo, &name, &blas, &t->err)))
		goto clean;

	const TLASInstance instance = {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
		.data = {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (U32) ETLASInstanceFlag_Default << 24,
			.blasCpu = blas
		}
	};

	ListTLASInstance instances {};
	ListTLASInstance_createRefConst(&instance, 1, &instances, NULL);

	name = CharString_createRefCStrConst("Ray query graphics TLAS");

	if(!Test_assert(t, "createTlas", GraphicsDeviceRef_createTLASExt(
		deviceRef, ERTASBuildFlags_DefaultTLAS, &instances, true, NULL, &name, &tlas, &t->err
	)))
		goto clean;

	//Only the pixel shader owns a register, so the layout comes from that entry

	const U32 entryIds[1] = { pixelId };

	Test_assert(t, "entrypointsRef", ListU32_createRefConst(entryIds, 1, &entrypoints, &t->err));

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntries(
		deviceRef, &files[1], &entrypoints, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Ray query graphics layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = { .maxAccelerationStructures = 1, .maxDescriptorTables = 1 };
	name = CharString_createRefCStrConst("Ray query graphics heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Ray query graphics table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	const Descriptor tlasDesc = Descriptor_tlas(tlas);
	const CharString sceneName = CharString_createRefCStrConst("scene");

	Test_assert(t, "setTlas", DescriptorTableRef_setDescriptorByName(table, &sceneName, 0, false, &tlasDesc, &t->err));

	name = CharString_createRefCStrConst("Ray query graphics target");

	if(!Test_assert(t, "targetCreate", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &name, &target, &t->err
	)))
		goto clean;

	PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout };
	name = CharString_createRefCStrConst("Ray query graphics pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	PipelineStage stages[2] = {
		{ .binaryId = vertexId, .shFileId = 0 },
		{ .binaryId = pixelId, .shFileId = 1 }
	};

	ListPipelineStage stageList {};
	ListPipelineStage_createRefConst(stages, 2, &stageList, NULL);

	const PipelineGraphicsInfo pipelineInfo = {
		.attachmentFormatsExt = { ETextureFormatId_RGBA8 },
		.attachmentCountExt = 1
	};

	name = CharString_createRefCStrConst("Ray query graphics pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineGraphics(
		deviceRef, &fileList, &stageList, &pipelineInfo, &name, EPipelineFlags_None,
		pipelineLayout, &pipeline, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "listCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, 4 * KIBI, 64, 16, true, &commandList, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "emptyListCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, KIBI, 16, 8, true, &emptyList, &t->err
	)))
		goto clean;

	Test_assert(t, "beginEmpty", CommandListRef_begin(emptyList, true, U64_MAX, &t->err));
	Test_assert(t, "endEmpty", CommandListRef_end(emptyList, &t->err));

	//The TLAS is declared at the PIXEL stage, which is the whole point of the module: that is what puts a
	// graphics sync scope on an acceleration structure barrier

	const Transition transition = { .resource = tlas, .stage = EPipelineStage_Pixel };

	ListTransition transitionList {};
	ListTransition_createRefConst(&transition, 1, &transitionList, NULL);

	const AttachmentInfo attachment = { .image = target, .load = ELoadAttachmentType_Clear };
	ListAttachmentInfo colors {};
	ListAttachmentInfo_createRefConst(&attachment, 1, &colors, NULL);

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

	Test_assert(t, "scopeBlas", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
	Test_assert(t, "updateBlas", CommandListRef_updateBLASExt(commandList, blas, &t->err));
	Test_assert(t, "scopeBlasEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "scopeTlas", CommandListRef_startScope(commandList, NULL, 2, NULL, &t->err));
	Test_assert(t, "updateTlas", CommandListRef_updateTLASExt(commandList, tlas, &t->err));
	Test_assert(t, "scopeTlasEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 3, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTable", CommandListRef_bindDescriptorTable(commandList, table, &t->err));

	Test_assert(t, "renderStart", CommandListRef_startRenderExt(
		commandList, I32x2_zero, I32x2_create2(8, 8), &colors, NULL, &t->err
	));

	Test_assert(t, "viewportScissor", CommandListRef_setViewportAndScissor(
		commandList, I32x2_zero, I32x2_zero, &t->err
	));

	Test_assert(t, "bindPipeline", CommandListRef_setGraphicsPipeline(commandList, pipeline, &t->err));
	Test_assert(t, "draw", CommandListRef_drawUnindexed(commandList, 3, 1, &t->err));
	Test_assert(t, "renderEnd", CommandListRef_endRenderExt(commandList, &t->err));
	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList))
		TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF3366FFu);

	}

clean:

	if(table)
		DescriptorTableRef_unsetDescriptors(table, 0, 0, 1, NULL);

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&target);
	RefPtr_dec(&tlas);
	RefPtr_dec(&blas);
	RefPtr_dec(&positions);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&files[0], alloc);
	SHFile_free(&files[1], alloc);
}
} }
