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

//The shared helpers in terms of the handle types. Both C++ headers come BEFORE the block below: a
//standard header included after the C headers landed in oxc::c finds its guard already tripped and
//leaves its symbols in that namespace.

#include "test_graphics_shared.hpp"

//Log::debugLn is the C++ front for Log_debugLnx; the x macros name ELogOptions_NewLine unqualified and
//so cannot be reached through the c namespace.

#include "types/container/log.hpp"

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

using namespace oxc;

//Same namespace the C headers landed in, so the definitions here match the declarations in
//test_graphics_shared.h and the macros in those headers still expand to names that resolve.

//The output handle and the acceleration structure, which is what the ray shaders read.
//Shared by the module, since every pipeline here declares the same block.
//The layout is a module static, so it outlives every scope that creates a pipeline against it and has to be
//released by hand at the end of the module instead: a handle released after its device is gone frees into
//nothing.

static gfx::PipelineLayout raysPushLayout;
static c::U32 raysPushData[4] = {};

static c::Bool TestShaders_raysLayout(c::Test *t, gfx::Device &dev, const c::SHFile &file, c::U32 raygenId) {
	return raysPushLayout || gfxtest::pushConstantLayout(t, dev, file, raygenId, raysPushLayout);
}

static c::Bool TestShaders_pushRays(c::Test *t, gfx::CommandScope &scope) {
	return Test_assert(t, "pushRays", scope.setPushConstants(raysPushData, &t->err));
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
	c::Test *t,
	gfx::Device &dev,
	const c::SHFile &file,
	const gfx::DeviceBuffer &positions,
	const gfx::DeviceBuffer &output,
	const gfx::CommandList &emptyList,
	c::ETextureFormatId ommIndexFormat
) {

	c::Error *e_rr = &t->err;

	const c::GraphicsDeviceCapabilities caps = dev.info().capabilities;

	if (!(caps.features & c::EGraphicsFeatures_RayMicromapOpacity)) {
		c::Test_print(t, "Device lacks opacity micromaps, skipping OMM trace test");
		return;
	}

	if (caps.experimentalFeatures & c::EGraphicsFeatures_RayMicromapOpacity) {
		c::Test_print(t, "Opacity micromaps claimed but experimental on this backend, skipping OMM trace test");
		return;
	}

	gfx::DeviceBuffer indices, ommOpaque, ommTransparent;
	gfx::Blas blasOpaque, blasTransparent;
	gfx::Tlas tlasOpaque, tlasTransparent;
	gfx::Pipeline pipeline;
	gfx::CommandList opaqueList, transparentList;

	//An OMM index is per triangle, which is why this geometry is indexed where the plain scene is not.

	const c::U16 triangleIndices[3] = { 0, 1, 2 };
	c::Buffer indexData = c::Buffer_createRefConst(triangleIndices, sizeof(triangleIndices));

	if(!Test_assert(t, "ommCreateIndices", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
		"OMM triangle indices", &indexData, indices, nullptr, e_rr
	)))
		return;

	//Packed into a U32 and sliced to the element width, which reads out the low bytes on the little endian
	// targets OxC3 runs on; one triangle, so the buffer is exactly one element.

	const c::U8 ommStride =
		ommIndexFormat == c::ETextureFormatId_R32u ? 4 : (ommIndexFormat == c::ETextureFormatId_R16u ? 2 : 1);

	const c::U32 opaqueIndex = c::EOMMSpecialIndex_pack(c::EOMMSpecialIndex_FullyOpaque, ommIndexFormat);
	const c::U32 transparentIndex = c::EOMMSpecialIndex_pack(c::EOMMSpecialIndex_FullyTransparent, ommIndexFormat);

	c::Buffer opaqueData = c::Buffer_createRefConst(&opaqueIndex, ommStride);
	c::Buffer transparentData = c::Buffer_createRefConst(&transparentIndex, ommStride);

	if(!Test_assert(t, "ommCreateOpaque", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
		"OMM indices, fully opaque", &opaqueData, ommOpaque, nullptr, e_rr
	)))
		return;

	if(!Test_assert(t, "ommCreateTransparent", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
		"OMM indices, fully transparent", &transparentData, ommTransparent, nullptr, e_rr
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

	if(!Test_assert(t, "ommCreateBlasOpaque", dev.createBlas(opaqueInfo, "OMM BLAS, fully opaque", blasOpaque, e_rr)))
		return;

	if(!Test_assert(t, "ommCreateBlasTransparent", dev.createBlas(
		transparentInfo, "OMM BLAS, fully transparent", blasTransparent, e_rr
	)))
		return;

	//ForceDisableAnyHit is deliberately absent where the rest of the module uses Default.
	//It is FORCE_OPAQUE on both APIs, and a forced opaque instance makes traversal ignore opacity micromaps
	// entirely, so the transparent half would report hits and pass for the wrong reason.

	c::TLASInstance ommInstance = {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
		.data = {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (c::U32) c::ETLASInstanceFlag_DisableCulling << 24,
			.blasCpu = blasOpaque.handle()
		}
	};

	if(!Test_assert(t, "ommCreateTlasOpaque", dev.createTlas(
		c::ERTASBuildFlags_DefaultTLAS, &ommInstance, 1, "OMM TLAS, fully opaque", tlasOpaque, false, e_rr
	)))
		return;

	ommInstance.data.blasCpu = blasTransparent.handle();

	if(!Test_assert(t, "ommCreateTlasTransparent", dev.createTlas(
		c::ERTASBuildFlags_DefaultTLAS, &ommInstance, 1, "OMM TLAS, fully transparent", tlasTransparent, false, e_rr
	)))
		return;

	//A pipeline of its own: both APIs ignore micromaps unless the pipeline opted in, so the plain one the
	// module already built would trace straight through the transparent triangle and report hits.

	const c::U32 raygenId = gfxtest::entry(t, dev, file, "mainRaygen");
	const c::U32 missId = gfxtest::entry(t, dev, file, "mainMiss");
	const c::U32 hitId = gfxtest::entry(t, dev, file, "mainClosestHit");

	if(raygenId == c::U32_MAX || missId == c::U32_MAX || hitId == c::U32_MAX)
		return;

	TestShaders_raysLayout(t, dev, file, raygenId);

	if(!Test_assert(t, "ommCreatePipeline", dev.createRaytracingPipeline(
		file, { "mainRaygen" }, "mainMiss", { "mainClosestHit" }, "OMM ray trace pipeline", pipeline, {}, 1,
		(c::EPipelineRaytracingFlags) (
			c::EPipelineRaytracingFlags_Default | c::EPipelineRaytracingFlags_AllowOpacityMicromapExt
		),
		&raysPushLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "ommCreateOpaqueList", dev.createCommandList(2 * c::KIBI, 32, 16, opaqueList, true, e_rr)))
		return;

	if(!Test_assert(t, "ommCreateTransparentList", dev.createCommandList(
		2 * c::KIBI, 32, 16, transparentList, true, e_rr
	)))
		return;

	//Opaque first, so a failure below can be read as "the OMM path broke tracing" rather than "the micromap
	// culled something", which are the two ways this can go wrong and want different fixes.

	const c::Transition opaqueTransitions[2] = {
		{ .resource = output.handle(), .stage = c::EPipelineStage_RaygenExt, .isWrite = true },
		{ .resource = tlasOpaque.handle(), .stage = c::EPipelineStage_RaygenExt }
	};

	//Set before the recording: a push constant is captured when the list is recorded, not when it is
	//submitted.

	raysPushData[0] = output.writeHandle();
	raysPushData[1] = tlasOpaque.bindlessHandle();

	Test_assert(t, "ommBeginOpaque", opaqueList.begin(true, e_rr));

	{
		gfx::CommandScope scope = opaqueList.scope({}, 1, {}, e_rr);
		Test_assert(t, "ommScopeBlasOpaque", (c::Bool) scope);
		Test_assert(t, "ommUpdateBlasOpaque", scope.updateBlas(blasOpaque, e_rr));
		Test_assert(t, "ommScopeBlasOpaqueEnd", scope.end(e_rr));
	}

	{
		gfx::CommandScope scope = opaqueList.scope({}, 2, {}, e_rr);
		Test_assert(t, "ommScopeTlasOpaque", (c::Bool) scope);
		Test_assert(t, "ommUpdateTlasOpaque", scope.updateTlas(tlasOpaque, e_rr));
		Test_assert(t, "ommScopeTlasOpaqueEnd", scope.end(e_rr));
	}

	{
		gfx::CommandScope scope = opaqueList.scopeSpan(opaqueTransitions, 2, 3, nullptr, 0, e_rr);
		Test_assert(t, "ommScopeTraceOpaque", (c::Bool) scope);

		Test_assert(t, "ommBindOpaque", scope.setRaytracingPipeline(pipeline, e_rr));
		TestShaders_pushRays(t, scope);
		Test_assert(t, "ommTraceOpaque", scope.dispatch1DRays(0, 4, e_rr));
		Test_assert(t, "ommScopeTraceOpaqueEnd", scope.end(e_rr));
	}

	Test_assert(t, "ommEndOpaque", opaqueList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, opaqueList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			Test_assert(t, "ommResultsOpaque", values[0] == 1 && values[1] == 1 && !values[2] && !values[3]);
		}

	const c::Transition transparentTransitions[2] = {
		{ .resource = output.handle(), .stage = c::EPipelineStage_RaygenExt, .isWrite = true },
		{ .resource = tlasTransparent.handle(), .stage = c::EPipelineStage_RaygenExt }
	};

	raysPushData[1] = tlasTransparent.bindlessHandle();

	Test_assert(t, "ommBeginTransparent", transparentList.begin(true, e_rr));

	{
		gfx::CommandScope scope = transparentList.scope({}, 1, {}, e_rr);
		Test_assert(t, "ommScopeBlasTransparent", (c::Bool) scope);
		Test_assert(t, "ommUpdateBlasTransparent", scope.updateBlas(blasTransparent, e_rr));
		Test_assert(t, "ommScopeBlasTransparentEnd", scope.end(e_rr));
	}

	{
		gfx::CommandScope scope = transparentList.scope({}, 2, {}, e_rr);
		Test_assert(t, "ommScopeTlasTransparent", (c::Bool) scope);
		Test_assert(t, "ommUpdateTlasTransparent", scope.updateTlas(tlasTransparent, e_rr));
		Test_assert(t, "ommScopeTlasTransparentEnd", scope.end(e_rr));
	}

	{
		gfx::CommandScope scope = transparentList.scopeSpan(transparentTransitions, 2, 3, nullptr, 0, e_rr);
		Test_assert(t, "ommScopeTraceTransparent", (c::Bool) scope);

		Test_assert(t, "ommBindTransparent", scope.setRaytracingPipeline(pipeline, e_rr));
		TestShaders_pushRays(t, scope);
		Test_assert(t, "ommTraceTransparent", scope.dispatch1DRays(0, 4, e_rr));
		Test_assert(t, "ommScopeTraceTransparentEnd", scope.end(e_rr));
	}

	Test_assert(t, "ommEndTransparent", transparentList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, transparentList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			Test_assert(t, "ommResultsTransparent", !values[0] && !values[1] && !values[2] && !values[3]);
		}
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
	c::Test *t,
	gfx::Device &dev,
	const c::SHFile &file,
	const gfx::DeviceBuffer &positions,
	const gfx::DeviceBuffer &output,
	const gfx::CommandList &emptyList
) {

	c::Error *e_rr = &t->err;

	const c::GraphicsDeviceCapabilities caps = dev.info().capabilities;

	if (!(caps.features & c::EGraphicsFeatures_RayMicromapOpacity)) {
		c::Test_print(t, "Device lacks opacity micromaps, skipping micromap array test");
		return;
	}

	if (caps.experimentalFeatures & c::EGraphicsFeatures_RayMicromapOpacity) {
		c::Test_print(t, "Opacity micromaps claimed but experimental on this backend, skipping micromap array test");
		return;
	}

	//The KHR path builds micromap arrays as acceleration structures, which isn't implemented until a driver
	// exists to test it against; special index OMM covers such a device above.

	if (
		dev.api() == c::EGraphicsApi_Vulkan &&
		(caps.featuresExt & c::EVkGraphicsFeatures_OpacityMicromapKHR)
	) {
		c::Test_print(t, "Micromap arrays aren't implemented on the Vulkan KHR path yet, skipping");
		return;
	}

	gfx::DeviceBuffer indices, inputBits, entries;
	gfx::OpacityMicromap micromap;
	gfx::Pipeline pipeline;

	gfx::DeviceBuffer ommIndex[5];
	gfx::Blas blas[5];
	gfx::Tlas tlas[5];
	gfx::CommandList lists[5];

	const c::U16 triangleIndices[3] = { 0, 1, 2 };
	c::Buffer indexData = c::Buffer_createRefConst(triangleIndices, sizeof(triangleIndices));

	if(!Test_assert(t, "ommArrayCreateIndices", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
		"OMM array triangle indices", &indexData, indices, nullptr, e_rr
	)))
		return;

	//One byte of opacity bits per entry, 4 bytes apart so every dataOffset stays 4 byte aligned.
	//2-state: bit set is opaque, cleared is transparent, and only the low 4 bits exist at level 1.

	c::U8 opacityBits[20] = { 0 };

	for(c::U8 k = 0; k < 4; ++k)
		opacityBits[k * 4] = 0xF & ~(1 << k);

	//opacityBits[16] stays 0: entry 4 is fully transparent

	c::Buffer bitsData = c::Buffer_createRefConst(opacityBits, sizeof(opacityBits));

	if(!Test_assert(t, "ommArrayCreateBits", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
		"OMM array opacity bits", &bitsData, inputBits, nullptr, e_rr
	)))
		return;

	c::OpacityMicromapEntry entryData[5];

	for(c::U8 k = 0; k < 5; ++k)
		entryData[k] = {
			.dataOffset = (c::U32) k * 4,
			.subdivisionLevel = 1,
			.format = c::EOpacityMicromapFormat_Opacity2State
		};

	c::Buffer entryRef = c::Buffer_createRefConst(entryData, sizeof(entryData));

	if(!Test_assert(t, "ommArrayCreateEntries", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
		"OMM array entries", &entryRef, entries, nullptr, e_rr
	)))
		return;

	const c::OpacityMicromapUsage usage = {
		.count = 5, .subdivisionLevel = 1, .format = c::EOpacityMicromapFormat_Opacity2State
	};

	//Named rather than compound literals: C++ has no address of a braced list, and these are passed by
	//pointer.

	const c::DeviceData inputBitsData = inputBits.region();
	const c::DeviceData entriesData = entries.region();

	c::OpacityMicromapCreateInfo micromapInfo = c::OpacityMicromapCreateInfo_uniform(
		c::ERTASBuildFlags_None,
		&inputBitsData,
		&entriesData,
		sizeof(c::OpacityMicromapEntry),
		&usage
	);

	if(!Test_assert(t, "ommArrayCreate", dev.createOpacityMicromap(micromapInfo, "OMM array micromap", micromap, e_rr)))
		return;

	//The same pipeline shape the special index test uses; micromaps are ignored without the opt in

	const c::U32 raygenId = gfxtest::entry(t, dev, file, "mainRaygen");
	const c::U32 missId = gfxtest::entry(t, dev, file, "mainMiss");
	const c::U32 hitId = gfxtest::entry(t, dev, file, "mainClosestHit");

	if(raygenId == c::U32_MAX || missId == c::U32_MAX || hitId == c::U32_MAX)
		return;

	TestShaders_raysLayout(t, dev, file, raygenId);

	if(!Test_assert(t, "ommArrayCreatePipeline", dev.createRaytracingPipeline(
		file, { "mainRaygen" }, "mainMiss", { "mainClosestHit" }, "OMM array pipeline", pipeline, {}, 1,
		(c::EPipelineRaytracingFlags) (
			c::EPipelineRaytracingFlags_Default | c::EPipelineRaytracingFlags_AllowOpacityMicromapExt
		),
		&raysPushLayout, e_rr
	)))
		return;

	//How rays 0 and 1 resolved per probe, packed as (ray0Hit << 1) | ray1Hit

	c::U8 outcomes[4] = { 0 };
	c::Bool traced = true;

	for (c::U8 k = 0; k < 5 && traced; ++k) {

		const c::U16 entryIndex = k;
		c::Buffer ommIndexData = c::Buffer_createRefConst(&entryIndex, sizeof(entryIndex));

		traced &= Test_assert(t, "ommArrayCreateIndexBuf", dev.createBufferData(
			c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None,
			"OMM array index buffer", &ommIndexData, ommIndex[k], nullptr, e_rr
		));

		if(!traced)
			break;

		const c::BLASCreateInfo blasInfo = c::BLASCreateInfo_indexedWithOmmExt(
			c::ERTASBuildFlags_None, c::EBLASFlag_None, c::ETextureFormatId_RGBA32f, 0, 16,
			positions.region(),
			c::ETextureFormatId_R16u, indices.region(),
			c::ETextureFormatId_R16u, ommIndex[k].region(),
			micromap.handle()
		);

		traced &= Test_assert(t, "ommArrayCreateBlas", dev.createBlas(blasInfo, "OMM array BLAS", blas[k], e_rr));

		if(!traced)
			break;

		//Nudged so ray 0 sits strictly inside the center sub triangle and ray 1 strictly inside the corner
		// one; without this ray 0 would land exactly on the shared edge and the outcome would be tie break
		// dependent.

		const c::TLASInstance ommInstance = {
			.transform = { { 1, 0, 0, -0.05f }, { 0, 1, 0, -0.05f }, { 0, 0, 1, 0 } },
			.data = {
				.instanceId24_mask8 = 0xFFu << 24,
				.sbtOffset24_flags8 = (c::U32) c::ETLASInstanceFlag_DisableCulling << 24,
				.blasCpu = blas[k].handle()
			}
		};

		traced &= Test_assert(t, "ommArrayCreateTlas", dev.createTlas(
			c::ERTASBuildFlags_DefaultTLAS, &ommInstance, 1, "OMM array TLAS", tlas[k], false, e_rr
		));

		traced &= Test_assert(t, "ommArrayCreateList", dev.createCommandList(2 * c::KIBI, 32, 16, lists[k], true, e_rr));

		if(!traced)
			break;

		const c::Transition traceTransitions[2] = {
			{ .resource = output.handle(), .stage = c::EPipelineStage_RaygenExt, .isWrite = true },
			{ .resource = tlas[k].handle(), .stage = c::EPipelineStage_RaygenExt }
		};

		//The micromap build only runs once; recording it again is a no-op after it completed

		raysPushData[0] = output.writeHandle();
		raysPushData[1] = tlas[k].bindlessHandle();

		Test_assert(t, "ommArrayBegin", lists[k].begin(true, e_rr));

		{
			gfx::CommandScope scope = lists[k].scope({}, 1, {}, e_rr);
			Test_assert(t, "ommArrayScopeOmm", (c::Bool) scope);
			Test_assert(t, "ommArrayUpdateOmm", scope.updateOmm(micromap, e_rr));
			Test_assert(t, "ommArrayScopeOmmEnd", scope.end(e_rr));
		}

		{
			gfx::CommandScope scope = lists[k].scope({}, 2, {}, e_rr);
			Test_assert(t, "ommArrayScopeBlas", (c::Bool) scope);
			Test_assert(t, "ommArrayUpdateBlas", scope.updateBlas(blas[k], e_rr));
			Test_assert(t, "ommArrayScopeBlasEnd", scope.end(e_rr));
		}

		{
			gfx::CommandScope scope = lists[k].scope({}, 3, {}, e_rr);
			Test_assert(t, "ommArrayScopeTlas", (c::Bool) scope);
			Test_assert(t, "ommArrayUpdateTlas", scope.updateTlas(tlas[k], e_rr));
			Test_assert(t, "ommArrayScopeTlasEnd", scope.end(e_rr));
		}

		{
			gfx::CommandScope scope = lists[k].scopeSpan(traceTransitions, 2, 4, nullptr, 0, e_rr);
			Test_assert(t, "ommArrayScopeTrace", (c::Bool) scope);

			Test_assert(t, "ommArrayBind", scope.setRaytracingPipeline(pipeline, e_rr));
			TestShaders_pushRays(t, scope);
			Test_assert(t, "ommArrayTrace", scope.dispatch1DRays(0, 4, e_rr));
			Test_assert(t, "ommArrayScopeTraceEnd", scope.end(e_rr));
		}

		Test_assert(t, "ommArrayEnd", lists[k].end(e_rr));

		traced &= gfxtest::submitAndWait(t, dev, lists[k]);
		traced = traced && gfxtest::pullBuffer(t, dev, emptyList, output);

		if (traced) {

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			//The geometric misses stay misses no matter what the micromap says

			Test_assert(t, "ommArrayOutsideMiss", !values[2] && !values[3]);

			if(k == 4)
				Test_assert(t, "ommArrayAllTransparent", !values[0] && !values[1]);

			else outcomes[k] = (c::U8)(((values[0] == 1) << 1) | (values[1] == 1));
		}
	}

	//The probe set proves per micro triangle addressing without assuming the space filling curve's order

	if (traced) {

		c::U8 missHit = 0, hitMiss = 0, hitHit = 0, missMiss = 0;

		for (c::U8 k = 0; k < 4; ++k)
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
			!!(c::AtomicI64_load(&c::deviceOf(dev.handle())->runtimeMessages) &
			(c::I64) c::EGraphicsDeviceMessage_OmmLikelyEmulated) ==
			!(caps.features2 & c::EGraphicsFeatures2_RayMicromapOpacityActual)
		);
	}
}

static void TestShaders_ommSpecialIndex(
	c::Test *t,
	gfx::Device &dev,
	const c::SHFile &file,
	const gfx::DeviceBuffer &positions,
	const gfx::DeviceBuffer &output,
	const gfx::CommandList &emptyList
) {

	TestShaders_ommSpecialIndexWithFormat(t, dev, file, positions, output, emptyList, c::ETextureFormatId_R16u);

	//The 8-bit pair is the same scene through a 1 byte element, where the special index truncates to 0xFF.

	const c::GraphicsDeviceCapabilities caps = dev.info().capabilities;

	if (caps.features2 & c::EGraphicsFeatures2_RayMicromapOpacityU8) {
		c::Test_print(t, "Repeating the OMM special index pair with R8u indices");
		TestShaders_ommSpecialIndexWithFormat(t, dev, file, positions, output, emptyList, c::ETextureFormatId_R8u);
	}

	else c::Test_print(t, "Device lacks 8-bit OMM indices, R8u trace pair skipped");
}

//The whole ray pipeline vehicle (scene, SBT, dispatch and readback) shared by the plain and the SER
//variant, which differ only in which oiSH drives it: both trace the same 4 rays at the same triangle and
//have to land on the same (1, 1, 0, 0).

//dev crosses BY VALUE because RtDedicatedDevice rebinds it: the swap must not outlive this call.

static void TestShaders_raysWithFile(
	c::Test *t, gfx::Device dev, const c::C8 *moduleName, const c::C8 *path, c::Bool testApiExtras
) {

	c::Test_setModule(t, moduleName);

	c::Error *e_rr = &t->err;

	if (!(dev.info().capabilities.features & c::EGraphicsFeatures_RayPipeline)) {
		c::Test_print(t, "Device lacks raytracing pipelines, skipping ray trace tests");
		return;
	}

	if (!dev.hasBindlessTable()) {
		c::Test_print(t, "Device has no bindless descriptor table, skipping ray trace tests");
		return;
	}

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, path, file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping ray trace tests");
		return;
	}

	gfxtest::RtDedicatedDevice dedicated(t, dev);

	if(!dedicated)
		return;

	//The layout is a module static shared by every pipeline here, so it outlives the scope that created it.
	//Declared AFTER the dedicated device so it destroys FIRST: the layout is made on whichever device is
	//current, and releasing it after that device is gone frees into nothing.

	struct LayoutGuard {
		~LayoutGuard() { raysPushLayout.release(); }
	} layoutGuard;

	gfx::DeviceBuffer positions, output;
	gfx::Blas blas;
	gfx::Tlas tlas;
	gfx::Pipeline pipeline;
	gfx::CommandList commandList, emptyList, refitList, blasRefitList;

	const c::F32 triangle[12] = {
		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1
	};

	c::Buffer triData = c::Buffer_createRefConst(triangle, sizeof(triangle));

	//CPUBacked because the BLAS refit below rewrites these positions in place and marks them dirty, which is
	// what a refit reads: the BLAS keeps pointing at this same buffer.

	if(!Test_assert(t, "createPositions", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_CPUBacked,
		"Ray trace positions", &triData, positions, nullptr, e_rr
	)))
		return;

	//AllowUpdate on the parent because the refit below updates FROM this one, and both APIs require the source
	// of an update to have been built with it.
	//Our own validation only checks the refit's flags, so leaving it off here fails in the driver instead.

	const c::BLASCreateInfo blasInfo = c::BLASCreateInfo_unindexed(
		c::ERTASBuildFlags_AllowUpdate, c::EBLASFlag_None, c::ETextureFormatId_RGBA32f, 0, 16, positions.region()
	);

	if(!Test_assert(t, "createBlas", dev.createBlas(blasInfo, "Ray trace BLAS", blas, e_rr)))
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
		&instance, 1, "Ray trace TLAS", tlas, false, e_rr
	)))
		return;

	//The whole point of this TLAS is being reachable from the raygen shader

	Test_assert(t, "tlasHandle", tlas.bindlessHandle() != c::BindlessDescriptor_None);

	Test_assert(t, "createOutput", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWriteBindless | c::EGraphicsResourceFlag_CPUBacked),
		"Ray trace output", 4 * sizeof(c::U32), output, nullptr, e_rr
	));

	//Raygen, miss and closest hit in that stage order; the one triangle group points at stage 2

	const c::U32 raygenId = gfxtest::entry(t, dev, file.list, "mainRaygen");
	const c::U32 missId = gfxtest::entry(t, dev, file.list, "mainMiss");
	const c::U32 hitId = gfxtest::entry(t, dev, file.list, "mainClosestHit");

	if(!output || raygenId == c::U32_MAX || missId == c::U32_MAX || hitId == c::U32_MAX)
		return;

	TestShaders_raysLayout(t, dev, file.list, raygenId);

	if(!Test_assert(t, "createPipeline", dev.createRaytracingPipeline(
		file.list, { "mainRaygen" }, "mainMiss", { "mainClosestHit" }, "Ray trace pipeline", pipeline, {}, 1,
		c::EPipelineRaytracingFlags_Default, &raysPushLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "createList", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "createEmptyList", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmptyList", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmptyList", emptyList.end(e_rr));

	//Build the scene and trace in one submit, split into scopes for the same dependency reason as the AS module

	raysPushData[0] = output.writeHandle();
	raysPushData[1] = tlas.bindlessHandle();

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

	const c::Transition traceTransitions[2] = {
		{ .resource = output.handle(), .stage = c::EPipelineStage_RaygenExt, .isWrite = true },
		{ .resource = tlas.handle(), .stage = c::EPipelineStage_RaygenExt }
	};

	{
		gfx::CommandScope scope = commandList.scopeSpan(traceTransitions, 2, 3, nullptr, 0, e_rr);
		Test_assert(t, "scopeTrace", (c::Bool) scope);
		Test_assert(t, "bindPipeline", scope.setRaytracingPipeline(pipeline, e_rr));
		TestShaders_pushRays(t, scope);
		Test_assert(t, "trace", scope.dispatch1DRays(0, 4, e_rr));
		Test_assert(t, "scopeTraceEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if(!gfxtest::submitAndWait(t, dev, commandList))
		return;

	if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

		const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

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

		const c::BindlessDescriptor handleBeforeRefit = tlas.bindlessHandle();

		c::TLASInstance moved = instance;
		moved.transform[2][3] = 1000;                //Translate Z; the transform is row major 3x4

		c::Bool madeRefit = Test_assert(t, "setInstancesMoved", tlas.setInstances(&moved, 1, e_rr));

		madeRefit &= Test_assert(t, "createRefitList", dev.createCommandList(c::KIBI, 16, 8, refitList, true, e_rr));

		if (madeRefit) {

			//The scene the shader reads is reached through the same handle as before, so the push block that
			// drove the first trace drives this one unchanged.

			Test_assert(t, "refitKeepsHandle", tlas.bindlessHandle() == handleBeforeRefit);

			Test_assert(t, "beginRefit", refitList.begin(true, e_rr));

			{
				gfx::CommandScope scope = refitList.scope({}, 4, {}, e_rr);
				Test_assert(t, "scopeRefit", (c::Bool) scope);
				Test_assert(t, "updateTlasRefit", scope.updateTlas(tlas, e_rr));
				Test_assert(t, "scopeRefitEnd", scope.end(e_rr));
			}

			{
				gfx::CommandScope scope = refitList.scopeSpan(traceTransitions, 2, 5, nullptr, 0, e_rr);
				Test_assert(t, "scopeTraceRefit", (c::Bool) scope);
				Test_assert(t, "bindPipelineRefit", scope.setRaytracingPipeline(pipeline, e_rr));
				TestShaders_pushRays(t, scope);
				Test_assert(t, "traceRefit", scope.dispatch1DRays(0, 4, e_rr));
				Test_assert(t, "scopeTraceRefitEnd", scope.end(e_rr));
			}

			Test_assert(t, "endRefit", refitList.end(e_rr));

			if (gfxtest::submitAndWait(t, dev, refitList))
				if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

					const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;
					Test_assert(t, "rayResultsRefit", !values[0] && !values[1] && !values[2] && !values[3]);
				}

			//Refit straight back to where it started.
			//This is the case the old copy based design could not express without a third acceleration
			// structure that pinned the second one, which pinned the first: chaining refits grew memory for as
			// long as the chain was alive.
			//In place it is the same object every time, so this costs nothing at all, and landing back on the
			// original result proves a second refit reads the state the first one left rather than the state
			// the AS was originally built from.

			if (Test_assert(t, "setInstancesBack", tlas.setInstances(&instance, 1, e_rr))) {

				Test_assert(t, "beginRefitBack", refitList.begin(true, e_rr));

				{
					gfx::CommandScope scope = refitList.scope({}, 4, {}, e_rr);
					Test_assert(t, "scopeRefitBack", (c::Bool) scope);
					Test_assert(t, "updateTlasRefitBack", scope.updateTlas(tlas, e_rr));
					Test_assert(t, "scopeRefitBackEnd", scope.end(e_rr));
				}

				{
					gfx::CommandScope scope = refitList.scopeSpan(traceTransitions, 2, 5, nullptr, 0, e_rr);
					Test_assert(t, "scopeTraceRefitBack", (c::Bool) scope);
					Test_assert(t, "bindPipelineRefitBack", scope.setRaytracingPipeline(pipeline, e_rr));
					TestShaders_pushRays(t, scope);
					Test_assert(t, "traceRefitBack", scope.dispatch1DRays(0, 4, e_rr));
					Test_assert(t, "scopeTraceRefitBackEnd", scope.end(e_rr));
				}

				Test_assert(t, "endRefitBack", refitList.end(e_rr));

				if (gfxtest::submitAndWait(t, dev, refitList))
					if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

						const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

						Test_assert(
							t, "rayResultsRefitBack",
							values[0] == 1 && values[1] == 1 && !values[2] && !values[3]
						);
					}
			}
		}

		//Opacity micromaps run here rather than at the end, because the BLAS refit below rewrites the position
		// buffer these BLASes are built over and would leave them tracing a triangle that moved away.

		TestShaders_ommSpecialIndex(t, dev, file.list, positions, output, emptyList);
		TestShaders_ommMicromapArray(t, dev, file.list, positions, output, emptyList);

		//The same idea one level down, so the BLAS update path gets the same treatment.
		//Here the triangle itself moves rather than the instance, by rewriting the position buffer the BLAS
		// already reads; the TLAS is refitted straight after because an instance caches the bounds of the BLAS
		// it points at, so a BLAS that changed leaves every TLAS over it stale.

		c::Bool madeBlasRefit = Test_assert(t, "createBlasRefitList", dev.createCommandList(
			2 * c::KIBI, 32, 16, blasRefitList, true, e_rr
		));

		if (madeBlasRefit) {

			c::F32 *positionData2 = (c::F32*) positions.data()->cpuData.ptrNonConst;

			for(c::U64 i = 0; i < 3; ++i)
				positionData2[i * 4 + 2] = 1000;                //Z of every vertex, the stride is 4 floats

			madeBlasRefit = Test_assert(t, "markPositionsDirty", positions.markDirty(0, sizeof(triangle), e_rr));
		}

		if (madeBlasRefit) {

			//The OMM helpers above share this block and leave it pointing at a TLAS of their own that is
			//already destroyed, so it is restored before anything records against it again.

			raysPushData[0] = output.writeHandle();
			raysPushData[1] = tlas.bindlessHandle();

			Test_assert(t, "beginBlasRefit", blasRefitList.begin(true, e_rr));

			{
				gfx::CommandScope scope = blasRefitList.scope({}, 6, {}, e_rr);
				Test_assert(t, "scopeBlasRefit", (c::Bool) scope);
				Test_assert(t, "updateBlasRefit", scope.updateBlas(blas, e_rr));
				Test_assert(t, "scopeBlasRefitEnd", scope.end(e_rr));
			}

			{
				gfx::CommandScope scope = blasRefitList.scope({}, 7, {}, e_rr);
				Test_assert(t, "scopeTlasAfterBlas", (c::Bool) scope);
				Test_assert(t, "updateTlasAfterBlas", scope.updateTlas(tlas, e_rr));
				Test_assert(t, "scopeTlasAfterBlasEnd", scope.end(e_rr));
			}

			{
				gfx::CommandScope scope = blasRefitList.scopeSpan(traceTransitions, 2, 8, nullptr, 0, e_rr);
				Test_assert(t, "scopeTraceBlasRefit", (c::Bool) scope);
				Test_assert(t, "bindPipelineBlasRefit", scope.setRaytracingPipeline(pipeline, e_rr));
				TestShaders_pushRays(t, scope);
				Test_assert(t, "traceBlasRefit", scope.dispatch1DRays(0, 4, e_rr));
				Test_assert(t, "scopeTraceBlasRefitEnd", scope.end(e_rr));
			}

			Test_assert(t, "endBlasRefit", blasRefitList.end(e_rr));

			if (gfxtest::submitAndWait(t, dev, blasRefitList))
				if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

					const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;
					Test_assert(t, "rayResultsBlasRefit", !values[0] && !values[1] && !values[2] && !values[3]);
				}
		}
	}
}

extern "C" void Test_graphicsShaderRays(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	gfx::Device dev = gfx::Device::share(deviceRef);

	TestShaders_raysWithFile(t, dev, "Shaders/rays", "//OxC3_gtest/test_shaders/test_rays.oiSH", true);

	//The SER variant records the hit as a HitObject, hints the scheduler with MaybeReorderThread,
	// then invokes the recorded shader explicitly.
	//Reordering itself is unobservable by design,
	// so what is checked is that the split path lands on exactly the same payloads as the plain TraceRay above.
	//Running with the reorder hint in place is also the only "doesn't break" coverage RayReorderActual can
	// get: a device that claims to actually reorder still has to produce identical results.
	//An experimental claim is skipped: on Vulkan the NV device extension can't accept the EXT SPIR-V the
	// shader stack emits, and on D3D12 the SM6.9 the shaders need is preview only.

	const c::GraphicsDeviceCapabilities caps = dev.info().capabilities;

	if(!(caps.features & c::EGraphicsFeatures_RayReorder))
		return;

	if (caps.experimentalFeatures & c::EGraphicsFeatures_RayReorder) {
		c::Test_print(t, "RayReorder claimed but experimental on this backend, skipping SER trace test");
		return;
	}

	TestShaders_raysWithFile(
		t, dev, "Shaders/raysSer", "//OxC3_gtest/test_shaders/test_rays_ser.oiSH", false
	);
}
