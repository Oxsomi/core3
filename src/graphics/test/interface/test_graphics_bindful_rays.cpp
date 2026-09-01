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

//The shared helpers in terms of the handle types. Both C++ headers come BEFORE the block below: a
//standard header included after the C headers landed in oxc::c finds its guard already tripped and
//leaves its symbols in that namespace.

#include "test_graphics_shared.hpp"

//Log::debugLn is the C++ front for Log_debugLnx; the x macros name ELogOptions_NewLine unqualified and
//so cannot be reached through the c namespace.

#include "types/container/log.hpp"

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

using namespace oxc;

//Same namespace the C headers landed in, so the definitions here match the declarations in
//test_graphics_shared.h and the macros in those headers still expand to names that resolve.

// -- 46. Bindful ray tracing: the TLAS through a table instead of a handle -------

//Ray tracing itself never needed bindless; this proves it by tracing the same 4 ray scene as the bindless
// rays module with the TLAS and output buffer coming from classic registers.
//The TLAS is created with disallowBindlessDescriptor, so this works on a device without bindless at all.

extern "C" void Test_graphicsBindfulRays(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/rays");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	if (!(dev.info().capabilities.features & c::EGraphicsFeatures_RayPipeline)) {
		c::Test_print(t, "Device lacks raytracing pipelines, skipping bindful ray trace tests");
		return;
	}

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_rays.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful ray trace tests");
		return;
	}

	gfxtest::RtDedicatedDevice dedicated(t, dev);

	if(!dedicated)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceBuffer positions, output;
	gfx::Blas blas;
	gfx::Tlas tlas;
	gfx::CommandList commandList, emptyList;

	//The table holds no reference of its own, so its descriptors go back before the resources they name do.

	gfxtest::TableGuard tableGuard{ { &table } };

	//The same one triangle scene the bindless rays module uses

	const c::F32 triangle[12] = {
		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1
	};

	c::Buffer triData = c::Buffer_createRefConst(triangle, sizeof(triangle));

	//CPUBacked because the BLAS refit at the end rewrites these positions in place and marks them dirty

	if(!Test_assert(t, "createPositions", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_CPUBacked,
		"Bindful rays positions", &triData, positions, nullptr, e_rr
	)))
		return;

	const c::BLASCreateInfo blasInfo = c::BLASCreateInfo_unindexed(
		c::ERTASBuildFlags_AllowUpdate, c::EBLASFlag_None, c::ETextureFormatId_RGBA32f, 0, 16, positions.region()
	);

	if(!Test_assert(t, "createBlas", dev.createBlas(blasInfo, "Bindful rays BLAS", blas, e_rr)))
		return;

	const c::TLASInstance instance = {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
		.data = {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (c::U32) c::ETLASInstanceFlag_Default << 24,
			.blasCpu = blas.handle()
		}
	};

	if(!Test_assert(t, "createTlas", dev.createTlas(
		(c::ERTASBuildFlags) (c::ERTASBuildFlags_DefaultTLAS | c::ERTASBuildFlags_AllowUpdate),
		&instance, 1, "Bindful rays TLAS", tlas, true, e_rr
	)))
		return;

	const c::U32 raygenId = gfxtest::entry(t, dev, file.list, "mainRaygen");
	const c::U32 missId = gfxtest::entry(t, dev, file.list, "mainMiss");
	const c::U32 hitId = gfxtest::entry(t, dev, file.list, "mainClosestHit");

	if(raygenId == c::U32_MAX || missId == c::U32_MAX || hitId == c::U32_MAX)
		return;

	//The layout comes from all three stages' reflection at once

	const c::U32 entryIds[3] = { raygenId, missId, hitId };

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayoutFromEntries(
		file.list, entryIds, 3, layoutInfo.list, c::EDescriptorLayoutFlags_None,
		(c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(
		layoutInfo.list, "Bindful rays layout", layout, e_rr
	)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxAccelerationStructures = 1,
		.maxBuffersRW = 1, .maxDescriptorTables = 1
	};

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Bindful rays heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Bindful rays table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	if(!Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Bindful rays output", 4 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	const c::Descriptor tlasDesc = c::Descriptor_tlas(tlas.handle());
	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setTlas", table.setByName("scene", tlasDesc, 0, false, e_rr));
	Test_assert(t, "setOutput", table.setByName("output", outputDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Bindful rays pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCreate", dev.createRaytracingPipeline(
		file.list, { "mainRaygen" }, "mainMiss", { "mainClosestHit" }, "Bindful rays pipeline", pipeline, {}, 1,
		c::EPipelineRaytracingFlags_Default, &pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	const c::Transition traceTransitions[2] = {
		{ .resource = output.handle(), .stage = c::EPipelineStage_RaygenExt, .isWrite = true },
		{ .resource = tlas.handle(), .stage = c::EPipelineStage_RaygenExt }
	};

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "scopeBlas", (c::Bool) scope);
		Test_assert(t, "updateBlas", scope.updateBlas(blas, e_rr));
		Test_assert(t, "scopeBlasEnd", scope.end(e_rr));
	}

	{
		gfx::CommandScope scope = commandList.scope({}, 2, {}, e_rr);
		Test_assert(t, "scopeTlas", (c::Bool) scope);
		Test_assert(t, "updateTlas", scope.updateTlas(tlas, e_rr));
		Test_assert(t, "scopeTlasEnd", scope.end(e_rr));
	}

	{
		gfx::CommandScope scope = commandList.scopeSpan(traceTransitions, 2, 3, nullptr, 0, e_rr);
		Test_assert(t, "scopeTrace", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		Test_assert(t, "bindPipeline", scope.setRaytracingPipeline(pipeline, e_rr));
		Test_assert(t, "trace", scope.dispatch1DRays(0, 4, e_rr));
		Test_assert(t, "scopeTraceEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			Test_assert(t, "bindfulRayResults", values[0] == 1 && values[1] == 1 && !values[2] && !values[3]);
		}

	//Refit in place: the same TLAS object gets new instance data and is rebuilt rather than recreated, so
	// the descriptor written into the table earlier has to keep addressing it. Moving the instance far along
	// Z takes it out of every ray's path, so all four rays must miss without the table being touched again.

	const c::TLASInstance movedInstance = {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 1000 } },
		.data = {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (c::U32) c::ETLASInstanceFlag_Default << 24,
			.blasCpu = blas.handle()
		}
	};

	if (Test_assert(t, "setInstancesMoved", tlas.setInstances(&movedInstance, 1, e_rr))) {

		Test_assert(t, "beginRefit", commandList.begin(true, e_rr));

		{
			gfx::CommandScope scope = commandList.scope({}, 1, {}, e_rr);
			Test_assert(t, "scopeRefit", (c::Bool) scope);
			Test_assert(t, "updateTlasRefit", scope.updateTlas(tlas, e_rr));
			Test_assert(t, "scopeRefitEnd", scope.end(e_rr));
		}

		{
			gfx::CommandScope scope = commandList.scopeSpan(traceTransitions, 2, 2, nullptr, 0, e_rr);
			Test_assert(t, "scopeTraceRefit", (c::Bool) scope);
			Test_assert(t, "bindHeapRefit", scope.bindDescriptorHeap(heap, e_rr));
			Test_assert(t, "bindTableRefit", scope.bindDescriptorTable(table, e_rr));
			Test_assert(t, "bindPipelineRefit", scope.setRaytracingPipeline(pipeline, e_rr));
			Test_assert(t, "traceRefit", scope.dispatch1DRays(0, 4, e_rr));
			Test_assert(t, "scopeTraceRefitEnd", scope.end(e_rr));
		}

		Test_assert(t, "endRefit", commandList.end(e_rr));

		if (gfxtest::submitAndWait(t, dev, commandList))
			if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

				const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

				Test_assert(t, "refitMissesAll", !values[0] && !values[1] && !values[2] && !values[3]);
			}

		//Refit back to where it started: the second refit reads the state the first one left behind, so a
		// refit that quietly rebuilt from the original instances instead would land on the wrong result here

		if (Test_assert(t, "setInstancesBack", tlas.setInstances(&instance, 1, e_rr))) {

			Test_assert(t, "beginRefitBack", commandList.begin(true, e_rr));

			{
				gfx::CommandScope scope = commandList.scope({}, 1, {}, e_rr);
				Test_assert(t, "scopeRefitBack", (c::Bool) scope);
				Test_assert(t, "updateTlasRefitBack", scope.updateTlas(tlas, e_rr));
				Test_assert(t, "scopeRefitBackEnd", scope.end(e_rr));
			}

			{
				gfx::CommandScope scope = commandList.scopeSpan(traceTransitions, 2, 2, nullptr, 0, e_rr);
				Test_assert(t, "scopeTraceBack", (c::Bool) scope);
				Test_assert(t, "bindHeapBack", scope.bindDescriptorHeap(heap, e_rr));
				Test_assert(t, "bindTableBack", scope.bindDescriptorTable(table, e_rr));
				Test_assert(t, "bindPipelineBack", scope.setRaytracingPipeline(pipeline, e_rr));
				Test_assert(t, "traceBack", scope.dispatch1DRays(0, 4, e_rr));
				Test_assert(t, "scopeTraceBackEnd", scope.end(e_rr));
			}

			Test_assert(t, "endRefitBack", commandList.end(e_rr));

			if (gfxtest::submitAndWait(t, dev, commandList))
				if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

					const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

					Test_assert(t, "refitBackHitsAgain", values[0] == 1 && values[1] == 1 && !values[2] && !values[3]);
				}
		}

		//Refitting the geometry itself rather than the instance: the triangle's own vertices move out of
		// reach, so the BLAS has to rebuild from the rewritten positions and the TLAS after it. Both rays
		// missing proves the new vertex data really reached the bottom level structure.

		c::F32 *movedPositions = (c::F32*) positions.data()->cpuData.ptrNonConst;

		for(c::U64 i = 0; i < 3; ++i)
			movedPositions[i * 4 + 2] = 1000;                //Z of every vertex, the stride is 4 floats

		if (Test_assert(t, "markPositionsDirty", positions.markDirty(0, sizeof(triangle), e_rr))) {

			Test_assert(t, "beginBlasRefit", commandList.begin(true, e_rr));

			{
				gfx::CommandScope scope = commandList.scope({}, 1, {}, e_rr);
				Test_assert(t, "scopeBlasRefit", (c::Bool) scope);
				Test_assert(t, "updateBlasRefit", scope.updateBlas(blas, e_rr));
				Test_assert(t, "scopeBlasRefitEnd", scope.end(e_rr));
			}

			{
				gfx::CommandScope scope = commandList.scope({}, 2, {}, e_rr);
				Test_assert(t, "scopeTlasAfterBlas", (c::Bool) scope);
				Test_assert(t, "updateTlasAfterBlas", scope.updateTlas(tlas, e_rr));
				Test_assert(t, "scopeTlasAfterBlasEnd", scope.end(e_rr));
			}

			{
				gfx::CommandScope scope = commandList.scopeSpan(traceTransitions, 2, 3, nullptr, 0, e_rr);
				Test_assert(t, "scopeTraceBlasRefit", (c::Bool) scope);
				Test_assert(t, "bindHeapBlasRefit", scope.bindDescriptorHeap(heap, e_rr));
				Test_assert(t, "bindTableBlasRefit", scope.bindDescriptorTable(table, e_rr));
				Test_assert(t, "bindPipelineBlasRefit", scope.setRaytracingPipeline(pipeline, e_rr));
				Test_assert(t, "traceBlasRefit", scope.dispatch1DRays(0, 4, e_rr));
				Test_assert(t, "scopeTraceBlasRefitEnd", scope.end(e_rr));
			}

			Test_assert(t, "endBlasRefit", commandList.end(e_rr));

			if (gfxtest::submitAndWait(t, dev, commandList))
				if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

					const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

					Test_assert(t, "blasRefitMissesAll", !values[0] && !values[1] && !values[2] && !values[3]);
				}
		}
	}
}

// -- 60. Opacity micromaps through a bindful table ------------------------------

//Opacity micromaps sit below the binding model entirely, but their only execution coverage runs through
// bindless handles, so a device without bindless never traces one. The special index form is enough to
// prove the path end to end: one triangle marked fully opaque must be hit, the same triangle marked fully
// transparent must be missed, and the difference can only come from the micromap indices being consumed.
//The pipeline has to opt in, since both APIs ignore micromaps otherwise and would report hits either way.

static void TestBindful_ommWithFormat(
	c::Test *t,
	gfx::Device &dev,
	const c::SHFile &file,
	const gfx::DeviceBuffer &positions,
	const gfx::DescriptorHeap &heap,
	gfx::DescriptorTable &table,
	const gfx::PipelineLayout &pipelineLayout,
	const gfx::DeviceBuffer &output,
	const gfx::CommandList &emptyList,
	c::ETextureFormatId ommIndexFormat
) {

	c::Error *e_rr = &t->err;

	gfx::DeviceBuffer indices, ommOpaque, ommTransparent;
	gfx::Blas blasOpaque, blasTransparent;
	gfx::Tlas tlasOpaque, tlasTransparent;
	gfx::Pipeline pipeline;
	gfx::CommandList commandList;

	const c::U16 triangleIndices[3] = { 0, 1, 2 };
	c::Buffer indexData = c::Buffer_createRefConst(triangleIndices, sizeof(triangleIndices));

	if(!Test_assert(t, "ommCreateIndices", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
		"Bindful OMM indices", &indexData, indices, nullptr, e_rr
	)))
		return;

	//One triangle, so each micromap index buffer is exactly one element wide

	const c::U8 ommStride = ommIndexFormat == c::ETextureFormatId_R32u ? 4 : (ommIndexFormat == c::ETextureFormatId_R16u ? 2 : 1);

	const c::U32 opaqueIndex = c::EOMMSpecialIndex_pack(c::EOMMSpecialIndex_FullyOpaque, ommIndexFormat);
	const c::U32 transparentIndex = c::EOMMSpecialIndex_pack(c::EOMMSpecialIndex_FullyTransparent, ommIndexFormat);

	c::Buffer opaqueData = c::Buffer_createRefConst(&opaqueIndex, ommStride);
	c::Buffer transparentData = c::Buffer_createRefConst(&transparentIndex, ommStride);

	if(!Test_assert(t, "ommCreateOpaque", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
		"Bindful OMM indices, fully opaque", &opaqueData, ommOpaque, nullptr, e_rr
	)))
		return;

	if(!Test_assert(t, "ommCreateTransparent", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
		"Bindful OMM indices, fully transparent", &transparentData, ommTransparent, nullptr, e_rr
	)))
		return;

	const c::DeviceData positionData = positions.region();
	const c::DeviceData indexBufferData = indices.region();

	const c::BLASCreateInfo opaqueInfo = c::BLASCreateInfo_indexedWithOmmIndicesExt(
		c::ERTASBuildFlags_None, c::EBLASFlag_None, c::ETextureFormatId_RGBA32f, 0, 16, positionData,
		c::ETextureFormatId_R16u, indexBufferData,
		ommIndexFormat, ommOpaque.region()
	);

	const c::BLASCreateInfo transparentInfo = c::BLASCreateInfo_indexedWithOmmIndicesExt(
		c::ERTASBuildFlags_None, c::EBLASFlag_None, c::ETextureFormatId_RGBA32f, 0, 16, positionData,
		c::ETextureFormatId_R16u, indexBufferData,
		ommIndexFormat, ommTransparent.region()
	);

	if(!Test_assert(t, "ommCreateBlasOpaque", dev.createBlas(
		opaqueInfo, "Bindful OMM BLAS, fully opaque", blasOpaque, e_rr
	)))
		return;

	if(!Test_assert(t, "ommCreateBlasTransparent", dev.createBlas(
		transparentInfo, "Bindful OMM BLAS, fully transparent", blasTransparent, e_rr
	)))
		return;

	//DisableCulling so a back facing hit still counts, exactly as the bindless micromap test does

	c::TLASInstance ommInstance = {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
		.data = {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (c::U32) c::ETLASInstanceFlag_DisableCulling << 24,
			.blasCpu = blasOpaque.handle()
		}
	};

	if(!Test_assert(t, "ommCreateTlasOpaque", dev.createTlas(
		c::ERTASBuildFlags_DefaultTLAS, &ommInstance, 1, "Bindful OMM TLAS, fully opaque", tlasOpaque, true, e_rr
	)))
		return;

	ommInstance.data.blasCpu = blasTransparent.handle();

	if(!Test_assert(t, "ommCreateTlasTransparent", dev.createTlas(
		c::ERTASBuildFlags_DefaultTLAS, &ommInstance, 1, "Bindful OMM TLAS, fully transparent",
		tlasTransparent, true, e_rr
	)))
		return;

	const c::U32 raygenId = gfxtest::entry(t, dev, file, "mainRaygen");
	const c::U32 missId = gfxtest::entry(t, dev, file, "mainMiss");
	const c::U32 hitId = gfxtest::entry(t, dev, file, "mainClosestHit");

	if(raygenId == c::U32_MAX || missId == c::U32_MAX || hitId == c::U32_MAX)
		return;

	//Without the opt in both APIs ignore the micromap and the transparent triangle would report hits

	if(!Test_assert(t, "ommCreatePipeline", dev.createRaytracingPipeline(
		file, { "mainRaygen" }, "mainMiss", { "mainClosestHit" }, "Bindful OMM pipeline", pipeline, {}, 1,
		(c::EPipelineRaytracingFlags) (
			c::EPipelineRaytracingFlags_Default | c::EPipelineRaytracingFlags_AllowOpacityMicromapExt
		),
		&pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "ommCreateList", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	//Fully opaque first: the same four rays the plain trace uses, so two of them have to hit

	for (c::U8 pass = 0; pass < 2; ++pass) {

		const gfx::Tlas &tlas = pass ? tlasTransparent : tlasOpaque;
		const c::Descriptor tlasDesc = c::Descriptor_tlas(tlas.handle());

		Test_assert(t, "ommSetTlas", table.setByName("scene", tlasDesc, 0, true, e_rr));

		const c::Transition traceTransitions[2] = {
			{ .resource = output.handle(), .stage = c::EPipelineStage_RaygenExt, .isWrite = true },
			{ .resource = tlas.handle(), .stage = c::EPipelineStage_RaygenExt }
		};

		Test_assert(t, "ommBegin", commandList.begin(true, e_rr));

		{
			gfx::CommandScope scope = commandList.scope({}, 1, {}, e_rr);
			Test_assert(t, "ommScopeBlas", (c::Bool) scope);
			Test_assert(t, "ommUpdateBlas", scope.updateBlas(pass ? blasTransparent : blasOpaque, e_rr));
			Test_assert(t, "ommScopeBlasEnd", scope.end(e_rr));
		}

		{
			gfx::CommandScope scope = commandList.scope({}, 2, {}, e_rr);
			Test_assert(t, "ommScopeTlas", (c::Bool) scope);
			Test_assert(t, "ommUpdateTlas", scope.updateTlas(tlas, e_rr));
			Test_assert(t, "ommScopeTlasEnd", scope.end(e_rr));
		}

		{
			gfx::CommandScope scope = commandList.scopeSpan(traceTransitions, 2, 3, nullptr, 0, e_rr);
			Test_assert(t, "ommScopeTrace", (c::Bool) scope);
			Test_assert(t, "ommBindHeap", scope.bindDescriptorHeap(heap, e_rr));
			Test_assert(t, "ommBindTable", scope.bindDescriptorTable(table, e_rr));
			Test_assert(t, "ommBindPipeline", scope.setRaytracingPipeline(pipeline, e_rr));
			Test_assert(t, "ommTrace", scope.dispatch1DRays(0, 4, e_rr));
			Test_assert(t, "ommScopeTraceEnd", scope.end(e_rr));
		}

		Test_assert(t, "ommEnd", commandList.end(e_rr));

		if (gfxtest::submitAndWait(t, dev, commandList))
			if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

				const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

				if(pass)
					Test_assert(t, "ommResultsTransparent", !values[0] && !values[1] && !values[2] && !values[3]);

				else Test_assert(t, "ommResultsOpaque", values[0] == 1 && values[1] == 1 && !values[2] && !values[3]);
			}
	}
}

extern "C" void Test_graphicsBindfulOmm(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/omm");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	const c::GraphicsDeviceCapabilities caps = dev.info().capabilities;

	if (!(caps.features & c::EGraphicsFeatures_RayPipeline)) {
		c::Test_print(t, "Device lacks raytracing pipelines, skipping bindful micromap tests");
		return;
	}

	if (!(caps.features & c::EGraphicsFeatures_RayMicromapOpacity)) {
		c::Test_print(t, "Device lacks opacity micromaps, skipping bindful micromap tests");
		return;
	}

	if (caps.experimentalFeatures & c::EGraphicsFeatures_RayMicromapOpacity) {
		c::Test_print(t, "Opacity micromaps claimed but experimental on this backend, skipping bindful micromap tests");
		return;
	}

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_rays.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful micromap tests");
		return;
	}

	gfxtest::RtDedicatedDevice dedicated(t, dev);

	if(!dedicated)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::DeviceBuffer positions, output;
	gfx::CommandList emptyList;

	gfxtest::TableGuard tableGuard{ { &table } };

	const c::F32 triangle[12] = {
		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1
	};

	c::Buffer triData = c::Buffer_createRefConst(triangle, sizeof(triangle));

	if(!Test_assert(t, "createPositions", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
		"Bindful OMM positions", &triData, positions, nullptr, e_rr
	)))
		return;

	const c::U32 raygenId = gfxtest::entry(t, dev, file.list, "mainRaygen");
	const c::U32 missId = gfxtest::entry(t, dev, file.list, "mainMiss");
	const c::U32 hitId = gfxtest::entry(t, dev, file.list, "mainClosestHit");

	if(raygenId == c::U32_MAX || missId == c::U32_MAX || hitId == c::U32_MAX)
		return;

	const c::U32 entryIds[3] = { raygenId, missId, hitId };

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayoutFromEntries(
		file.list, entryIds, 3, layoutInfo.list, c::EDescriptorLayoutFlags_None,
		(c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(layoutInfo.list, "Bindful OMM layout", layout, e_rr)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxAccelerationStructures = 1,
		.maxBuffersRW = 1, .maxDescriptorTables = 1
	};

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Bindful OMM heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(layout, "Bindful OMM table", table, c::EDescriptorTableFlags_None, e_rr)))
		return;

	if(!Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Bindful OMM output", 4 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setOutput", table.setByName("output", outputDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Bindful OMM pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	TestBindful_ommWithFormat(
		t, dev, file.list, positions, heap, table, pipelineLayout, output, emptyList, c::ETextureFormatId_R16u
	);

	//R8u indices need their own capability, since Vulkan's EXT extension forbids them and only the KHR
	// promotion or D3D12 accepts them

	if (caps.features2 & c::EGraphicsFeatures2_RayMicromapOpacityU8) {

		c::Test_print(t, "Repeating the bindful micromap pair with R8u indices");

		TestBindful_ommWithFormat(
			t, dev, file.list, positions, heap, table, pipelineLayout, output, emptyList, c::ETextureFormatId_R8u
		);
	}
}

// -- 61. Inline raytracing from a graphics stage --------------------------------

//Every other RayQuery test traces from compute, which hid a real backend bug: D3D12 accepts only a short
// list of sync scopes alongside the acceleration structure access bits, and the per stage graphics scopes
// are not on it, so a TLAS transitioned for a pixel shader produced a barrier the debug layer rejects.
// Compute maps to a legal scope, so no compute test could ever reach it.
//Tracing the same scene from a pixel shader is what puts a graphics stage on a TLAS transition.

extern "C" void Test_graphicsBindfulRayQueryGraphics(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/rayQueryGraphics");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	if (!(dev.info().capabilities.features & c::EGraphicsFeatures_RayQuery)) {
		c::Test_print(t, "Device lacks ray query, skipping graphics stage ray query tests");
		return;
	}

	if (!(dev.info().capabilities.features & c::EGraphicsFeatures_DirectRendering)) {
		c::Test_print(t, "Device lacks direct rendering, skipping graphics stage ray query tests");
		return;
	}

	gfxtest::OwnedSHFile vertexFile(dev.alloc()), pixelFile(dev.alloc());

	if (
		!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_draw_vs.oiSH", vertexFile.list) ||
		!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_rayquery_ps.oiSH", pixelFile.list)
	) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping graphics ray query tests");
		return;
	}

	const c::SHFile files[2] = { vertexFile.list, pixelFile.list };

	const c::U32 vertexId = gfxtest::entry(t, dev, files[0], "main");
	const c::U32 pixelId = gfxtest::entry(t, dev, files[1], "main");

	if(vertexId == c::U32_MAX || pixelId == c::U32_MAX) {
		c::Test_print(t, "No ray query entrypoint for this backend, skipping graphics ray query tests");
		return;
	}

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceBuffer positions;
	gfx::Blas blas;
	gfx::Tlas tlas;
	gfx::RenderTexture target;
	gfx::CommandList commandList, emptyList;

	gfxtest::TableGuard tableGuard{ { &table } };

	//The same one triangle scene the other raytracing modules use

	const c::F32 triangle[12] = {
		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1
	};

	c::Buffer triData = c::Buffer_createRefConst(triangle, sizeof(triangle));

	if(!Test_assert(t, "createPositions", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
		"Ray query graphics positions", &triData, positions, nullptr, e_rr
	)))
		return;

	const c::BLASCreateInfo blasInfo = c::BLASCreateInfo_unindexed(
		c::ERTASBuildFlags_None, c::EBLASFlag_None, c::ETextureFormatId_RGBA32f, 0, 16, positions.region()
	);

	if(!Test_assert(t, "createBlas", dev.createBlas(blasInfo, "Ray query graphics BLAS", blas, e_rr)))
		return;

	const c::TLASInstance instance = {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
		.data = {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (c::U32) c::ETLASInstanceFlag_Default << 24,
			.blasCpu = blas.handle()
		}
	};

	if(!Test_assert(t, "createTlas", dev.createTlas(
		c::ERTASBuildFlags_DefaultTLAS, &instance, 1, "Ray query graphics TLAS", tlas, true, e_rr
	)))
		return;

	//Only the pixel shader owns a register, so the layout comes from that entry

	const c::U32 entryIds[1] = { pixelId };

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayoutFromEntries(
		files[1], entryIds, 1, layoutInfo.list, c::EDescriptorLayoutFlags_None,
		(c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(
		layoutInfo.list, "Ray query graphics layout", layout, e_rr
	)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxAccelerationStructures = 1, .maxDescriptorTables = 1 };

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Ray query graphics heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Ray query graphics table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	const c::Descriptor tlasDesc = c::Descriptor_tlas(tlas.handle());

	Test_assert(t, "setTlas", table.setByName("scene", tlasDesc, 0, false, e_rr));

	if(!Test_assert(t, "targetCreate", dev.createRenderTexture(
		8, 8, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Ray query graphics target", target,
		c::EMSAASamples_Off, nullptr, e_rr
	)))
		return;

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Ray query graphics pipeline layout", pipelineLayout, e_rr
	)))
		return;

	const c::PipelineGraphicsInfo pipelineInfo = {
		.attachmentFormatsExt = { c::ETextureFormatId_RGBA8 },
		.attachmentCountExt = 1
	};

	if(!Test_assert(t, "pipelineCreate", dev.createGraphicsPipeline(
		pipelineInfo, files, 2, { { "main", 0 }, { "main", 1 } }, "Ray query graphics pipeline", pipeline,
		{}, &pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	//The TLAS is declared at the PIXEL stage, which is the whole point of the module: that is what puts a
	// graphics sync scope on an acceleration structure barrier

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "scopeBlas", (c::Bool) scope);
		Test_assert(t, "updateBlas", scope.updateBlas(blas, e_rr));
		Test_assert(t, "scopeBlasEnd", scope.end(e_rr));
	}

	{
		gfx::CommandScope scope = commandList.scope({}, 2, {}, e_rr);
		Test_assert(t, "scopeTlas", (c::Bool) scope);
		Test_assert(t, "updateTlas", scope.updateTlas(tlas, e_rr));
		Test_assert(t, "scopeTlasEnd", scope.end(e_rr));
	}

	{
		gfx::CommandScope scope = commandList.scope(
			{ { .resource = tlas.handle(), .stage = c::EPipelineStage_Pixel } }, 3, {}, e_rr
		);

		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));

		{
			gfx::CommandRender render = scope.render(
				c::I32x2_zero, c::I32x2_create2(8, 8),
				{ { .image = target.handle(), .load = c::ELoadAttachmentType_Clear } }, nullptr, e_rr
			);

			Test_assert(t, "renderStart", (c::Bool) render);
			Test_assert(t, "viewportScissor", render.setViewportAndScissor(c::I32x2_zero, c::I32x2_zero, e_rr));
			Test_assert(t, "bindPipeline", render.setGraphicsPipeline(pipeline, e_rr));
			Test_assert(t, "draw", render.drawUnindexed(3, 1, e_rr));
			Test_assert(t, "renderEnd", render.end(e_rr));
		}

		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList))
		(void) gfxtest::checkPixels(t, dev, emptyList, target.handle(), 0xFF3366FFu);
}
