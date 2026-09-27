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

//graphics/test/interface/test_graphics_blas_geometry.cpp

#include "test_graphics_shared.hpp"

#include "types/container/log.hpp"

namespace oxc { namespace c {
	#include "types/base/string_base.h"
	#include "types/container/list_basic_types.h"
	#include "types/test/test.h"
	#include "platforms/platform.h"
	#include "graphics/generic/blas.h"
	#include "graphics/generic/command_list.h"
	#include "graphics/generic/commands.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_buffer.h"
	#include "graphics/generic/device_info.h"
	#include "graphics/generic/graphics_types.h"
	#include "graphics/generic/instance.h"
	#include "graphics/generic/tlas.h"
	#include "test_graphics_shared.h"
} }

using namespace oxc;

// -- 66. Several geometries in one BLAS, told apart by GeometryIndex() -----

//One structure per mesh rather than per material run is what makes a multi material mesh a single instance,
//and the only thing that keeps the runs apart in a shader is the geometry index.
//
//The three geometries here are SLICES of one position buffer, which is the shape a reader produces: the
//material runs sit back to back in the stream and each geometry points at its own range rather than at a
//buffer of its own.

namespace {

	//Three triangles, one per geometry, spread along x so a ray can only reach one of them.
	//Geometry g covers x in [g * 2, g * 2 + 1], which is what the shader aims at.

	const c::F32 GEOMETRY_TRIANGLES[36] = {

		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1,

		2, 0, 0, 1,
		3, 0, 0, 1,
		2, 1, 0, 1,

		4, 0, 0, 1,
		5, 0, 0, 1,
		4, 1, 0, 1
	};

	constexpr c::U32 GEOMETRY_COUNT = 3;
	constexpr c::U64 GEOMETRY_BYTES = 3 * 4 * sizeof(c::F32);        //Three vertices of RGBA32f
}

extern "C" void Test_graphicsBlasGeometry(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "BLAS/geometries");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	//Inline raytracing rather than a ray pipeline, since the index is readable through RayQuery and a device
	// may have one without the other.

	if (!(dev.info().capabilities.features & c::EGraphicsFeatures_RayQuery)) {
		c::Test_print(t, "Device lacks ray queries, skipping multi geometry BLAS tests");
		return;
	}

	//Rebinds dev on the backend that needs a private device for GPU based validation. Every handle below is
	// declared after it so all of them are released before its teardown runs.

	gfxtest::RtDedicatedDevice dedicated(t, dev);

	if (!dedicated)
		return;

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_blas_geometry.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping");
		return;
	}

	gfx::DeviceBuffer positions, output;
	gfx::Blas blas;
	gfx::Tlas tlas;
	gfx::Pipeline pipeline;
	gfx::PipelineLayout pipelineLayout;
	gfx::CommandList commandList, buildList, emptyList;

	c::Buffer triData = c::Buffer_createRefConst(GEOMETRY_TRIANGLES, sizeof(GEOMETRY_TRIANGLES));

	if (!Test_assert(t, "geometryPositions", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
		"Multi geometry positions", &triData, positions, nullptr, e_rr
	)))
		return;

	//Every geometry is a RANGE of the one buffer, in the order a shader reads back as GeometryIndex().

	c::BLASGeometry geometries[GEOMETRY_COUNT];

	for (c::U32 i = 0; i < GEOMETRY_COUNT; ++i)
		geometries[i] = c::BLASGeometry_unindexed(
			c::ETextureFormatId_RGBA32f, 0, 16, positions.region(i * GEOMETRY_BYTES, GEOMETRY_BYTES)
		);

	//Only the middle one is opaque, which is what the second half of the dispatch culls. The flag is per
	// geometry on both APIs, so a BLAS that still carried one set for all of them fails this.

	geometries[1].flags = c::EBLASGeometryFlag_DisableAnyHit;

	c::ListBLASGeometry geometryList = {};

	if (!Test_assert(t, "geometryList", c::ListBLASGeometry_createRefConst(
		geometries, GEOMETRY_COUNT, &geometryList, e_rr
	)))
		return;

	const c::BLASCreateInfo blasInfo = c::BLASCreateInfo_geometries(c::ERTASBuildFlags_DefaultBLAS, geometryList
	);

	if (!Test_assert(t, "geometryBlas", dev.createBlas(blasInfo, "Multi geometry BLAS", blas, e_rr)))
		return;

	const c::TLASInstance instance = {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
		.data = {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (c::U32) c::ETLASInstanceFlag_DisableCulling << 24,
			.blasCpu = blas.handle()
		}
	};

	//The bindless descriptor is allowed rather than disallowed: the shader reaches the structure through
	// tlasExtUniform, which is the bindless handle, not through a descriptor table.

	if (!Test_assert(t, "geometryTlas", dev.createTlas(
		c::ERTASBuildFlags_DefaultTLAS, &instance, 1, "Multi geometry TLAS", tlas, false, e_rr
	)))
		return;

	//ShaderWriteBindless rather than ShaderWrite: the shader indexes the buffer through its bindless write
	// handle, and only the bindless flag allocates one.

	if (!Test_assert(t, "geometryOutput", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWriteBindless | c::EGraphicsResourceFlag_CPUBacked),
		"Multi geometry output", 32, output, nullptr, e_rr
	)))
		return;

	const c::U32 entryId = dev.getFirstShaderEntry(file.list, "main", c::ESHExtension_None, c::ESHExtension_RayQuery);

	if (entryId == c::U32_MAX) {
		c::Test_print(t, "No ray query entrypoint for this backend, skipping multi geometry BLAS tests");
		return;
	}

	if (!(
		Test_assert(t, "geometryEmptyList", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)) &&

		//The readback submits this one, so it has to be a CLOSED list rather than only an allocated one

		Test_assert(t, "geometryEmptyBegin", emptyList.begin(true, e_rr)) &&
		Test_assert(t, "geometryEmptyEnd", emptyList.end(e_rr)) &&
		Test_assert(t, "geometryBuildList", dev.createCommandList(4 * c::KIBI, 64, 16, buildList, true, e_rr)) &&
		Test_assert(t, "geometryBuildBegin", buildList.begin(true, e_rr))
	))
		return;

	{
		gfx::CommandScope scope = buildList.scope({}, 1, {}, e_rr);
		Test_assert(t, "geometryBlasScope", (c::Bool) scope);
		Test_assert(t, "geometryBlasBuild", scope.updateBlas(blas, e_rr));
		Test_assert(t, "geometryBlasScopeEnd", scope.end(e_rr));
	}

	{
		gfx::CommandScope scope = buildList.scope({}, 2, {}, e_rr);
		Test_assert(t, "geometryTlasScope", (c::Bool) scope);
		Test_assert(t, "geometryTlasBuild", scope.updateTlas(tlas, e_rr));
		Test_assert(t, "geometryTlasScopeEnd", scope.end(e_rr));
	}

	//The structures are submitted before the dispatch is even recorded: a TLAS only takes its bindless slot
	// once its build has run, and a push constant filled in before that points the shader at nothing.

	if (!(
		Test_assert(t, "geometryBuildEnd", buildList.end(e_rr)) &&
		gfxtest::submitAndWait(t, dev, buildList) &&
		Test_assert(t, "geometryTlasHandle", tlas.bindlessHandle() != c::BindlessDescriptor_None)
	))
		return;

	if (!(
		gfxtest::pushConstantLayout(t, dev, file.list, entryId, pipelineLayout) &&
		Test_assert(t, "geometryPipeline", dev.createComputePipeline(
			file.list, "main", "Multi geometry pipeline", pipeline, {}, &pipelineLayout, e_rr
		)) &&
		Test_assert(t, "geometryTraceList", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)) &&
		Test_assert(t, "geometryBegin", commandList.begin(true, e_rr))
	))
		return;

	//The dispatch writes the output and reads the structure, and the scope has to say so or the backend has
	// no barrier to put either into the state the trace needs.

	const c::Transition outputWrite = {
		.resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
	};

	const c::Transition tlasRead = { .resource = tlas.handle(), .stage = c::EPipelineStage_Compute };

	const c::U32 pushData[4] = { output.writeHandle(), tlas.bindlessHandle(), 0, 0 };

	{
		gfx::CommandScope scope = commandList.scope({ outputWrite, tlasRead }, 1, {}, e_rr);
		Test_assert(t, "geometryTraceScope", (c::Bool) scope);
		Test_assert(t, "geometryBind", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "geometryPush", scope.setPushConstants(pushData, e_rr));
		Test_assert(t, "geometryDispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "geometryTraceScopeEnd", scope.end(e_rr));
	}

	if (!(
		Test_assert(t, "geometryEnd", commandList.end(e_rr)) &&
		gfxtest::submitAndWait(t, dev, commandList) &&
		gfxtest::pullBuffer(t, dev, emptyList, output)
	))
		return;

	//Rays 0..3 take the flags as built: each hit reports the geometry it landed in, and the ray past the last
	// one reports nothing at all.
	//Rays 4..7 repeat them culling opaque geometry, so only the middle one drops out.

	const c::U32 expected[8] = {
		0, 1, 2, 0xFFFFFFFFu,
		0, 0xFFFFFFFFu, 2, 0xFFFFFFFFu
	};

	const c::DeviceBuffer *outputPtr = output.data();

	if (!Test_assert(t, "geometryReadback", c::Buffer_length(outputPtr->cpuData) >= sizeof(expected)))
		return;

	for (c::U32 i = 0; i < 8; ++i) {

		const c::U32 got = *(const c::U32*)(outputPtr->cpuData.ptr + i * 4);

		Test_assert(t, "geometryIndex", got == expected[i]);

		if(got != expected[i])
			Log::debugLn(
				*dev.alloc(), "-- blasGeometry: ray %" PRIu32 " expected 0x%08X, got 0x%08X", i, expected[i], got
			);
	}
}
