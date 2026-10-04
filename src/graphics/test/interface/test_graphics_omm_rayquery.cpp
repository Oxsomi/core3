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

//graphics/test/interface/test_graphics_omm_rayquery.cpp

#include "test_graphics_shared.hpp"

#include "types/container/log.hpp"

namespace oxc { namespace c {
	#include "types/base/string_base.h"
	#include "types/container/buffer.h"
	#include "types/container/texture_format.h"
	#include "types/test/test.h"
	#include "platforms/platform.h"
	#include "graphics/generic/blas.h"
	#include "graphics/generic/command_list.h"
	#include "graphics/generic/commands.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_buffer.h"
	#include "graphics/generic/device_info.h"
	#include "graphics/generic/instance.h"
	#include "graphics/generic/opacity_micromap.h"
	#include "graphics/generic/tlas.h"
	#include "test_graphics_shared.h"
} }

using namespace oxc;

// -- 68. Opacity micromaps through an inline ray query ------------------------

//The micromap scenes the ray pipeline tests trace (TestShaders_omm in test_graphics_shaders_rays.cpp),
// traced with RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS> from a compute entry
// and from the raygen of a ray pipeline.
//On Vulkan a ray query reaching micromap geometry is only valid from an entry point declaring the
// OpacityMicromapIdKHR execution mode, which OXC_ENABLE_OPACITY_MICROMAP() in the entry point's body declares.
//Without it some drivers ignore the micromap, so the transparent probes below report hits; that result,
// not the validation layer, is what catches an entry point missing the mode.
//The ray pipeline is created without AllowOpacityMicromapExt: that flag covers TraceRay, not a ray query,
// so on Vulkan the execution mode is all that makes the raygen's query see the micromaps.
//
//Every probe is an instance of its own in ONE TLAS: per index width, the FullyOpaque and FullyTransparent
// special index BLASes, then five BLASes linking entries 0..4 of one shared micromap array.
//Instance k is the one triangle translated 2k along x, so the shader's four rays per instance reach no other.
//The instances are nudged by (-0.05, -0.05) for the reason the micromap array test gives: ray 0 then lands
// strictly inside the center sub triangle and ray 1 inside the corner one.

namespace {

	constexpr c::U32 OMM_FORMATS = 3;
	constexpr c::U32 OMM_ENTRIES = 5;
	constexpr c::U32 OMM_PROBES = 2 + OMM_ENTRIES;                  //Two special indices, then every entry
	constexpr c::U32 OMM_INSTANCES = OMM_FORMATS * OMM_PROBES;

	const c::ETextureFormatId OMM_INDEX_FORMATS[OMM_FORMATS] = {
		c::ETextureFormatId_R8u, c::ETextureFormatId_R16u, c::ETextureFormatId_R32u
	};

	const c::C8 *OMM_FORMAT_NAMES[OMM_FORMATS] = { "R8u", "R16u", "R32u" };

	//Four results per instance as test_omm_rayquery.hlsl writes them, checked the same way for every variant

	void checkOmmProbes(c::Test *t, gfx::Device &dev, const c::U32 *values, const c::C8 *variant) {

		for (c::U32 f = 0; f < OMM_FORMATS; ++f) {

			const c::U32 *probe = values + f * OMM_PROBES * 4;

			c::Bool ok = true;

			//The geometric misses stay misses whatever the micromap says, on every probe

			for (c::U32 p = 0; p < OMM_PROBES; ++p)
				ok &= Test_assert(t, "ommRqOutsideMiss", !probe[p * 4 + 2] && !probe[p * 4 + 3]);

			//FullyOpaque traces like the plain scene and FullyTransparent misses entirely; only the pair says the
			// micromap was consulted rather than the BLAS having quietly failed to build.

			ok &= Test_assert(t, "ommRqSpecialOpaque", probe[0] == 1 && probe[1] == 1);
			ok &= Test_assert(t, "ommRqSpecialTransparent", !probe[4] && !probe[5]);

			//The single bit probes, read mapping agnostically as the ray pipeline test does: each has to cull a
			// different sub triangle, giving exactly one (miss, hit), one (hit, miss) and two (hit, hit).

			c::U8 missHit = 0, hitMiss = 0, hitHit = 0, other = 0;

			for (c::U32 k = 0; k < 4; ++k) {

				const c::U32 *rays = probe + (2 + k) * 4;

				if(!rays[0] && rays[1] == 1)          ++missHit;          //ray 0 culled: the center sub triangle
				else if(rays[0] == 1 && !rays[1])     ++hitMiss;          //ray 1 culled: the corner sub triangle
				else if(rays[0] == 1 && rays[1] == 1) ++hitHit;           //a sub triangle neither ray visits
				else                                  ++other;            //both culled, or a hit on the wrong instance
			}

			ok &= Test_assert(t, "ommRqCenterProbe", missHit == 1);
			ok &= Test_assert(t, "ommRqCornerProbe", hitMiss == 1);
			ok &= Test_assert(t, "ommRqUntouchedProbes", hitHit == 2);
			ok &= Test_assert(t, "ommRqNoDoubleCull", !other);

			//Entry 4 is fully transparent

			ok &= Test_assert(t, "ommRqAllTransparent", !probe[(2 + 4) * 4] && !probe[(2 + 4) * 4 + 1]);

			if(ok)
				continue;

			for (c::U32 p = 0; p < OMM_PROBES; ++p)
				Log::debugLn(
					*dev.alloc(),
					"-- ommRayQuery %s %s probe %" PRIu32 ": %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32,
					variant, OMM_FORMAT_NAMES[f], p, probe[p * 4], probe[p * 4 + 1], probe[p * 4 + 2], probe[p * 4 + 3]
				);
		}
	}
}

extern "C" void Test_graphicsOmmRayQuery(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "BLAS/ommRayQuery");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	const c::GraphicsDeviceCapabilities caps = dev.info().capabilities;

	if (!(caps.features & c::EGraphicsFeatures_RayQuery)) {
		c::Test_print(t, "Device lacks ray queries, skipping OMM ray query tests");
		return;
	}

	if (!(caps.features & c::EGraphicsFeatures_RayMicromapOpacity)) {
		c::Test_print(
			t, "Device lacks opacity micromaps (Vulkan needs VK_KHR_opacity_micromap), skipping OMM ray query tests"
		);

		return;
	}

	if (caps.experimentalFeatures & c::EGraphicsFeatures_RayMicromapOpacity) {
		c::Test_print(t, "Opacity micromaps claimed but experimental on this backend, skipping OMM ray query tests");
		return;
	}

	//Rebinds dev on the backend that needs a private device for GPU based validation. Every handle below is
	// declared after it so all of them are released before its teardown runs.

	gfxtest::RtDedicatedDevice dedicated(t, dev);

	if (!dedicated)
		return;

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_omm_rayquery.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping OMM ray query tests");
		return;
	}

	gfx::DeviceBuffer positions, indices, inputBits, entries, output, raygenOutput;
	gfx::OpacityMicromap micromap;
	gfx::DeviceBuffer ommIndex[OMM_INSTANCES];
	gfx::Blas blas[OMM_INSTANCES];
	gfx::Tlas tlas;
	gfx::Pipeline pipeline, raygenPipeline;
	gfx::PipelineLayout pipelineLayout, raygenLayout;
	gfx::CommandList commandList, buildList, emptyList, raygenList;

	const c::F32 triangle[12] = {
		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1
	};

	c::Buffer triData = c::Buffer_createRefConst(triangle, sizeof(triangle));

	//An OMM index is per triangle, which is why the geometry is indexed

	const c::U16 triangleIndices[3] = { 0, 1, 2 };
	c::Buffer indexData = c::Buffer_createRefConst(triangleIndices, sizeof(triangleIndices));

	if (!(
		Test_assert(t, "ommRqPositions", dev.createBufferData(
			c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
			"OMM ray query positions", &triData, positions, nullptr, e_rr
		)) &&
		Test_assert(t, "ommRqIndices", dev.createBufferData(
			c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
			"OMM ray query triangle indices", &indexData, indices, nullptr, e_rr
		))
	))
		return;

	//The micromap array of the ray pipeline test: five 2-state subdivision level 1 entries, entry k of 0..3
	// opaque except for micro triangle k, entry 4 fully transparent.
	//One byte of opacity bits per entry, 4 bytes apart so every dataOffset stays 4 byte aligned.

	c::U8 opacityBits[OMM_ENTRIES * 4] = { 0 };

	for(c::U8 k = 0; k < 4; ++k)
		opacityBits[k * 4] = 0xF & ~(1 << k);

	c::Buffer bitsData = c::Buffer_createRefConst(opacityBits, sizeof(opacityBits));

	c::OpacityMicromapEntry entryData[OMM_ENTRIES];

	for(c::U8 k = 0; k < OMM_ENTRIES; ++k)
		entryData[k] = {
			.dataOffset = (c::U32) k * 4,
			.subdivisionLevel = 1,
			.format = c::EOpacityMicromapFormat_Opacity2State
		};

	c::Buffer entryRef = c::Buffer_createRefConst(entryData, sizeof(entryData));

	if (!(
		Test_assert(t, "ommRqBits", dev.createBufferData(
			c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
			"OMM ray query opacity bits", &bitsData, inputBits, nullptr, e_rr
		)) &&
		Test_assert(t, "ommRqEntries", dev.createBufferData(
			c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
			"OMM ray query entries", &entryRef, entries, nullptr, e_rr
		))
	))
		return;

	const c::OpacityMicromapUsage usage = {
		.count = OMM_ENTRIES, .subdivisionLevel = 1, .format = c::EOpacityMicromapFormat_Opacity2State
	};

	const c::DeviceData inputBitsData = inputBits.region();
	const c::DeviceData entriesData = entries.region();

	const c::OpacityMicromapCreateInfo micromapInfo = c::OpacityMicromapCreateInfo_uniform(
		c::ERTASBuildFlags_None,
		&inputBitsData,
		&entriesData,
		sizeof(c::OpacityMicromapEntry),
		&usage
	);

	if (!Test_assert(t, "ommRqMicromap", dev.createOpacityMicromap(
		micromapInfo, "OMM ray query micromap", micromap, e_rr
	)))
		return;

	//One BLAS per probe. A probe's OMM index buffer is one element, packed into a U32 and sliced to the element
	// width, which reads out the low bytes on the little endian targets OxC3 runs on.

	c::TLASInstance instances[OMM_INSTANCES];

	for (c::U32 f = 0; f < OMM_FORMATS; ++f) {

		const c::ETextureFormatId format = OMM_INDEX_FORMATS[f];
		const c::U8 stride = format == c::ETextureFormatId_R32u ? 4 : (format == c::ETextureFormatId_R16u ? 2 : 1);

		for (c::U32 p = 0; p < OMM_PROBES; ++p) {

			const c::U32 n = f * OMM_PROBES + p;

			const c::U32 value =
				p == 0 ? c::EOMMSpecialIndex_pack(c::EOMMSpecialIndex_FullyOpaque, format) :
				p == 1 ? c::EOMMSpecialIndex_pack(c::EOMMSpecialIndex_FullyTransparent, format) :
				p - 2;

			c::Buffer valueData = c::Buffer_createRefConst(&value, stride);

			if (!Test_assert(t, "ommRqIndexBuffer", dev.createBufferData(
				c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
				"OMM ray query index buffer", &valueData, ommIndex[n], nullptr, e_rr
			)))
				return;

			//Special indices attach no micromap object, entries index into the shared one

			const c::BLASGeometry geometry = p < 2 ?
				c::BLASGeometry_indexedWithOmmIndicesExt(
					c::ETextureFormatId_RGBA32f, 0, 16, positions.region(),
					c::ETextureFormatId_R16u, indices.region(),
					format, ommIndex[n].region()
				) :
				c::BLASGeometry_indexedWithOmmExt(
					c::ETextureFormatId_RGBA32f, 0, 16, positions.region(),
					c::ETextureFormatId_R16u, indices.region(),
					format, ommIndex[n].region(),
					micromap.handle()
				);

			const c::BLASCreateInfo blasInfo = c::BLASCreateInfo_single(c::ERTASBuildFlags_None, &geometry);

			if (!Test_assert(t, "ommRqBlas", dev.createBlas(blasInfo, "OMM ray query BLAS", blas[n], e_rr)))
				return;

			//ForceDisableAnyHit is deliberately absent: it is FORCE_OPAQUE on both APIs and makes traversal
			// ignore the micromap, so ETLASInstanceFlag_Default can't be used here.

			instances[n] = {
				.transform = { { 1, 0, 0, (c::F32) n * 2 - 0.05f }, { 0, 1, 0, -0.05f }, { 0, 0, 1, 0 } },
				.data = {
					.instanceId24_mask8 = 0xFFu << 24,
					.sbtOffset24_flags8 = (c::U32) c::ETLASInstanceFlag_DisableCulling << 24,
					.blasCpu = blas[n].handle()
				}
			};
		}
	}

	if (!Test_assert(t, "ommRqTlas", dev.createTlas(
		c::ERTASBuildFlags_DefaultTLAS, instances, OMM_INSTANCES, "OMM ray query TLAS", tlas, false, e_rr
	)))
		return;

	if (!Test_assert(t, "ommRqOutput", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWriteBindless | c::EGraphicsResourceFlag_CPUBacked),
		"OMM ray query output", OMM_INSTANCES * 4 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	//Both entries live in test_omm_rayquery.hlsl, each in the combinations of its own kind

	const c::ESHExtension ommExtensions = (c::ESHExtension) (
		c::ESHExtension_RayQuery | c::ESHExtension_RayMicromapOpacity
	);

	const c::U32 entryId = dev.getFirstShaderEntry(file.list, "main", c::ESHExtension_None, ommExtensions);

	//The device has proven RayQuery and OMM by now, so a missing entry is a compiler regression, not a skip

	if (!Test_assert(t, "ommRqComputeEntry", entryId != c::U32_MAX))
		return;

	const c::Bool hasRayPipeline = !!(caps.features & c::EGraphicsFeatures_RayPipeline);
	c::U32 raygenId = c::U32_MAX;

	if (hasRayPipeline) {

		raygenId = dev.getFirstShaderEntry(file.list, "mainRaygen", c::ESHExtension_None, ommExtensions);

		if (!Test_assert(t, "ommRqRaygenEntry", raygenId != c::U32_MAX))
			return;
	}

	else c::Test_print(t, "Device lacks ray pipelines, skipping the OMM ray query raygen variant");

	if (!(
		Test_assert(t, "ommRqEmptyList", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)) &&
		Test_assert(t, "ommRqEmptyBegin", emptyList.begin(true, e_rr)) &&
		Test_assert(t, "ommRqEmptyEnd", emptyList.end(e_rr)) &&
		Test_assert(t, "ommRqBuildList", dev.createCommandList(16 * c::KIBI, 128, 64, buildList, true, e_rr)) &&
		Test_assert(t, "ommRqBuildBegin", buildList.begin(true, e_rr))
	))
		return;

	//The micromap has to be built before any BLAS that links it, and every BLAS before the TLAS over them

	{
		gfx::CommandScope scope = buildList.scope({}, 1, {}, e_rr);
		Test_assert(t, "ommRqOmmScope", (c::Bool) scope);
		Test_assert(t, "ommRqOmmBuild", scope.updateOmm(micromap, e_rr));
		Test_assert(t, "ommRqOmmScopeEnd", scope.end(e_rr));
	}

	{
		gfx::CommandScope scope = buildList.scope({}, 2, {}, e_rr);
		Test_assert(t, "ommRqBlasScope", (c::Bool) scope);

		for (c::U32 n = 0; n < OMM_INSTANCES; ++n)
			Test_assert(t, "ommRqBlasBuild", scope.updateBlas(blas[n], e_rr));

		Test_assert(t, "ommRqBlasScopeEnd", scope.end(e_rr));
	}

	{
		gfx::CommandScope scope = buildList.scope({}, 3, {}, e_rr);
		Test_assert(t, "ommRqTlasScope", (c::Bool) scope);
		Test_assert(t, "ommRqTlasBuild", scope.updateTlas(tlas, e_rr));
		Test_assert(t, "ommRqTlasScopeEnd", scope.end(e_rr));
	}

	//Submitted before the dispatch is recorded: a TLAS only takes its bindless slot once its build has run

	if (!(
		Test_assert(t, "ommRqBuildEnd", buildList.end(e_rr)) &&
		gfxtest::submitAndWait(t, dev, buildList) &&
		Test_assert(t, "ommRqTlasHandle", tlas.bindlessHandle() != c::BindlessDescriptor_None)
	))
		return;

	if (!(
		gfxtest::pushConstantLayout(t, dev, file.list, entryId, pipelineLayout) &&
		Test_assert(t, "ommRqPipeline", dev.createComputePipeline(
			file.list, "main", "OMM ray query pipeline", pipeline, {}, &pipelineLayout, e_rr
		)) &&
		Test_assert(t, "ommRqTraceList", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)) &&
		Test_assert(t, "ommRqBegin", commandList.begin(true, e_rr))
	))
		return;

	const c::Transition outputWrite = {
		.resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
	};

	const c::Transition tlasRead = { .resource = tlas.handle(), .stage = c::EPipelineStage_Compute };

	const c::U32 pushData[4] = { output.writeHandle(), tlas.bindlessHandle(), OMM_INSTANCES, 0 };

	{
		gfx::CommandScope scope = commandList.scope({ outputWrite, tlasRead }, 1, {}, e_rr);
		Test_assert(t, "ommRqTraceScope", (c::Bool) scope);
		Test_assert(t, "ommRqBind", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "ommRqPush", scope.setPushConstants(pushData, e_rr));
		Test_assert(t, "ommRqDispatch", scope.dispatch1D((OMM_INSTANCES * 4 + 63) / 64, e_rr));
		Test_assert(t, "ommRqTraceScopeEnd", scope.end(e_rr));
	}

	if (!(
		Test_assert(t, "ommRqEnd", commandList.end(e_rr)) &&
		gfxtest::submitAndWait(t, dev, commandList) &&
		gfxtest::pullBuffer(t, dev, emptyList, output)
	))
		return;

	const c::DeviceBuffer *outputPtr = output.data();

	if (!Test_assert(
		t, "ommRqReadback", c::Buffer_length(outputPtr->cpuData) >= OMM_INSTANCES * 4 * sizeof(c::U32)
	))
		return;

	checkOmmProbes(t, dev, (const c::U32*) outputPtr->cpuData.ptr, "compute");

	if(!hasRayPipeline)
		return;

	//The raygen writes a buffer of its own, so nothing the compute dispatch left behind can pass for its results

	if (!(
		Test_assert(t, "ommRqRaygenOutput", dev.createBuffer(
			c::EDeviceBufferUsage_None,
			(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWriteBindless | c::EGraphicsResourceFlag_CPUBacked),
			"OMM ray query raygen output", OMM_INSTANCES * 4 * sizeof(c::U32), raygenOutput, nullptr, e_rr
		)) &&
		gfxtest::pushConstantLayout(t, dev, file.list, raygenId, raygenLayout) &&
		Test_assert(t, "ommRqRaygenPipeline", dev.createRaytracingPipeline(
			file.list, { "mainRaygen" }, "mainMiss", { "mainClosestHit" }, "OMM ray query raygen pipeline",
			raygenPipeline, {}, 1, c::EPipelineRaytracingFlags_Default, &raygenLayout, e_rr
		)) &&
		Test_assert(t, "ommRqRaygenList", dev.createCommandList(4 * c::KIBI, 64, 16, raygenList, true, e_rr)) &&
		Test_assert(t, "ommRqRaygenBegin", raygenList.begin(true, e_rr))
	))
		return;

	const c::Transition raygenTransitions[2] = {
		{ .resource = raygenOutput.handle(), .stage = c::EPipelineStage_RaygenExt, .isWrite = true },
		{ .resource = tlas.handle(), .stage = c::EPipelineStage_RaygenExt }
	};

	const c::U32 raygenPush[4] = { raygenOutput.writeHandle(), tlas.bindlessHandle(), OMM_INSTANCES, 0 };

	{
		gfx::CommandScope scope = raygenList.scopeSpan(raygenTransitions, 2, 1, nullptr, 0, e_rr);
		Test_assert(t, "ommRqRaygenScope", (c::Bool) scope);
		Test_assert(t, "ommRqRaygenBind", scope.setRaytracingPipeline(raygenPipeline, e_rr));
		Test_assert(t, "ommRqRaygenPush", scope.setPushConstants(raygenPush, e_rr));
		Test_assert(t, "ommRqRaygenDispatch", scope.dispatch1DRays(0, OMM_INSTANCES * 4, e_rr));
		Test_assert(t, "ommRqRaygenScopeEnd", scope.end(e_rr));
	}

	if (!(
		Test_assert(t, "ommRqRaygenEnd", raygenList.end(e_rr)) &&
		gfxtest::submitAndWait(t, dev, raygenList) &&
		gfxtest::pullBuffer(t, dev, emptyList, raygenOutput)
	))
		return;

	const c::DeviceBuffer *raygenPtr = raygenOutput.data();

	if (!Test_assert(
		t, "ommRqRaygenReadback", c::Buffer_length(raygenPtr->cpuData) >= OMM_INSTANCES * 4 * sizeof(c::U32)
	))
		return;

	checkOmmProbes(t, dev, (const c::U32*) raygenPtr->cpuData.ptr, "raygen");
}
