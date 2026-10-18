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

//graphics/test/interface/test_graphics_pipeline_cache.cpp

#include "test_graphics_shared.hpp"

namespace oxc { namespace c {
	#include "graphics/generic/instance.h"
	#include "types/container/buffer.h"
}}

// -- Pipeline cache --------------------------------------------------------------

//Each kind of pipeline (compute, graphics, raytracing) is built twice from the same description.
//The first is counted in the memory stats once, as known code or as an estimate; the second counts the same, and on
// Vulkan it is the cache hit path, so it must find the size the first recorded under the same key rather than add
// another entry. Freeing both takes them out again.
//The device's pipeline cache round trips through get and set, and a blob that isn't one is ignored rather than refused.

namespace {

	using namespace oxc;

	struct PipelineCounters {

		c::I64 known, estimate, knownBytes, knownIR;
		c::U64 sizes;

		static PipelineCounters of(c::GraphicsDevice *device) {
			return {
				c::AtomicI64_load(&device->pipelineCount), c::AtomicI64_load(&device->pipelineEstimateCount),
				c::AtomicI64_load(&device->pipelineBytes), c::AtomicI64_load(&device->pipelineKnownIRBytes),
				device->pipelineSizes.length
			};
		}
	};

	template<typename Create>
	void TestPipelineCache_twice(c::Test *t, c::GraphicsDevice *device, const c::C8 *module, Create create) {

		c::Test_setModule(t, module);

		const PipelineCounters before = PipelineCounters::of(device);

		{
			gfx::Pipeline first, second;

			if(!c::Test_assert(t, "firstCreate", create(first)))
				return;

			const PipelineCounters once = PipelineCounters::of(device);

			c::Test_assert(t, "countedOnce", (once.known - before.known) + (once.estimate - before.estimate) == 1);
			c::Test_assert(t, "irWithKnown", (once.knownIR - before.knownIR != 0) == (once.known != before.known));

			//At most one new entry: none when an earlier test already built the same pipeline

			c::Test_assert(t, "oneSizeEntry", once.sizes - before.sizes <= 2);

			if(!c::Test_assert(t, "secondCreate", create(second)))
				return;

			const PipelineCounters twice = PipelineCounters::of(device);

			c::Test_assert(t, "sameCategory", twice.known - once.known == once.known - before.known);
			c::Test_assert(t, "sameBytes", twice.knownBytes - once.knownBytes == once.knownBytes - before.knownBytes);
			c::Test_assert(t, "keyStable", twice.sizes == once.sizes);
		}

		const PipelineCounters after = PipelineCounters::of(device);

		c::Test_assert(t, "freedKnown", after.known == before.known);
		c::Test_assert(t, "freedEstimate", after.estimate == before.estimate);
		c::Test_assert(t, "freedBytes", after.knownBytes == before.knownBytes);
		c::Test_assert(t, "freedIR", after.knownIR == before.knownIR);
	}

	//A layout detected from the given entries, and the pipeline layout over it

	bool TestPipelineCache_layout(
		c::Test *t, gfx::Device &dev, const c::SHFile &file, const c::U32 *entries, c::U64 entryCount,
		gfx::DescriptorLayout &layout, gfx::PipelineLayout &pipelineLayout
	) {

		c::Error *e_rr = &t->err;
		gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

		if(!c::Test_assert(t, "detectLayout", dev.detectLayoutFromEntries(
			file, entries, entryCount, layoutInfo.list, c::EDescriptorLayoutFlags_None,
			(c::EDetectDescriptorLayoutFlags) 0, e_rr
		)))
			return false;

		if(!c::Test_assert(t, "layoutCreate", dev.createDescriptorLayout(
			layoutInfo.list, "Pipeline cache layout", layout, e_rr
		)))
			return false;

		c::PipelineLayoutInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.bindings = layout.handle();

		return c::Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
			pipelineLayoutInfo, "Pipeline cache pipeline layout", pipelineLayout, e_rr
		));
	}
}

extern "C" void Test_graphicsPipelineCache(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;
	using namespace oxc::gfxtest;

	c::Error *e_rr = &t->err;

	c::Test_setModule(t, "PipelineCache");

	Device dev = Device::share(deviceRef);
	c::GraphicsDevice *device = RefPtr_data(deviceRef, c::GraphicsDevice);
	const c::Allocator *alloc = c::GraphicsDeviceRef_getAlloc(deviceRef);
	const c::EGraphicsFeatures features = (c::EGraphicsFeatures) dev.info().capabilities.features;

	OwnedSHFile computeFile(dev.alloc()), vertexFile(dev.alloc()), pixelFile(dev.alloc()), raysFile(dev.alloc());

	if (
		!loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_pushconst.oiSH", computeFile.list) ||
		!loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_draw_vs.oiSH", vertexFile.list) ||
		!loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_draw_ps.oiSH", pixelFile.list) ||
		!loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_rays.oiSH", raysFile.list)
	) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping pipeline cache tests");
		return;
	}

	//Compute

	{
		const c::U32 entryId = entry(t, dev, computeFile.list, "main");

		OwnedLayoutInfo layoutInfo(dev.alloc());
		c::DescriptorBinding pushConstants{};
		DescriptorLayout layout;
		PipelineLayout pipelineLayout;

		const bool ready =
			entryId != c::U32_MAX &&
			c::Test_assert(t, "detectComputeLayout", dev.detectLayout(
				computeFile.list, entryId, layoutInfo.list, nullptr, &pushConstants, {}, nullptr,
				c::EDescriptorLayoutFlags_None, c::EDetectDescriptorLayoutFlags_AssumePushConstants, e_rr
			)) &&
			c::Test_assert(t, "computeLayoutCreate", dev.createDescriptorLayout(
				layoutInfo.list, "Pipeline cache compute layout", layout, e_rr
			));

		c::PipelineLayoutInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.bindings = layout.handle();
		pipelineLayoutInfo.pushConstants = pushConstants;

		if(ready && c::Test_assert(t, "computePipelineLayoutCreate", dev.createPipelineLayout(
			pipelineLayoutInfo, "Pipeline cache compute pipeline layout", pipelineLayout, e_rr
		)))
			TestPipelineCache_twice(t, device, "PipelineCache/compute", [&](Pipeline &result) {
				return dev.createComputePipeline(
					computeFile.list, "main", "Pipeline cache compute", result, {}, &pipelineLayout, e_rr
				);
			});
	}

	//Graphics

	if (!(features & c::EGraphicsFeatures_DirectRendering))
		c::Test_print(t, "Device lacks direct rendering, skipping the graphics pipeline cache test");

	else {

		const c::SHFile files[2] = { vertexFile.list, pixelFile.list };
		const c::U32 pixelId = entry(t, dev, files[1], "main");

		DescriptorLayout layout;
		PipelineLayout pipelineLayout;

		const c::PipelineGraphicsInfo info = {
			.attachmentFormatsExt = { c::ETextureFormatId_RGBA8 },
			.attachmentCountExt = 1
		};

		if(pixelId != c::U32_MAX && TestPipelineCache_layout(t, dev, files[1], &pixelId, 1, layout, pipelineLayout))
			TestPipelineCache_twice(t, device, "PipelineCache/graphics", [&](Pipeline &result) {
				return dev.createGraphicsPipeline(
					info, files, 2, { { "main", 0 }, { "main", 1 } }, "Pipeline cache graphics", result,
					{}, &pipelineLayout, e_rr
				);
			});
	}

	//Raytracing: on D3D12 a state object is always an estimate, since nothing gives its compiled form.
	//D3D12's GPU based validation breaks state objects (see TestShaders_rtDedicatedDevice); with nothing cached or
	// measured there, the test is only skipped rather than moved to a device of its own.

	const c::GraphicsInstance *instance = RefPtr_data(device->instance, c::GraphicsInstance);

	const c::Bool d3d12Gbv =
		instance->api == c::EGraphicsApi_Direct3D12 && (instance->flags & c::EGraphicsInstanceFlags_IsDebug) &&
		!(instance->flags & c::EGraphicsInstanceFlags_DisableGPUBV);

	if (!(features & c::EGraphicsFeatures_RayPipeline))
		c::Test_print(t, "Device lacks raytracing pipelines, skipping the raytracing pipeline cache test");

	else if (d3d12Gbv)
		c::Test_print(t, "D3D12 GPU based validation breaks state objects, skipping the raytracing pipeline cache test");

	else {

		const c::U32 entryIds[3] = {
			entry(t, dev, raysFile.list, "mainRaygen"),
			entry(t, dev, raysFile.list, "mainMiss"),
			entry(t, dev, raysFile.list, "mainClosestHit")
		};

		DescriptorLayout layout;
		PipelineLayout pipelineLayout;

		if(
			entryIds[0] != c::U32_MAX && entryIds[1] != c::U32_MAX && entryIds[2] != c::U32_MAX &&
			TestPipelineCache_layout(t, dev, raysFile.list, entryIds, 3, layout, pipelineLayout)
		)
			TestPipelineCache_twice(t, device, "PipelineCache/raytracing", [&](Pipeline &result) {
				return dev.createRaytracingPipeline(
					raysFile.list, { "mainRaygen" }, "mainMiss", { "mainClosestHit" }, "Pipeline cache rays", result,
					{}, 1, c::EPipelineRaytracingFlags_Default, &pipelineLayout, e_rr
				);
			});
	}

	//The blob: the header, the recorded sizes, then the driver's data

	c::Test_setModule(t, "PipelineCache/blob");

	c::Buffer blob = c::Buffer_createNull();

	if(c::Test_assert(t, "getCache", c::GraphicsDeviceRef_getPipelineCache(deviceRef, alloc, &blob, e_rr))) {

		const c::Bool hasHeader = c::Buffer_length(blob) >= 24 && *(const c::U32*) blob.ptr == 0x4350696F;
		c::Test_assert(t, "cacheHeader", hasHeader);

		if(hasHeader && device->pipelineSizes.length)
			c::Test_assert(t, "cacheHasSizes", *(const c::U32*) (blob.ptr + 8) == device->pipelineSizes.length / 2);

		c::Test_assert(t, "setCache", c::GraphicsDeviceRef_setPipelineCache(deviceRef, blob, e_rr));
	}

	c::Buffer_free(&blob, alloc);

	//Not a pipeline cache: ignored, not an error

	const c::U8 garbage[32] = { 1, 2, 3 };

	c::Test_assert(t, "garbageIgnored", c::GraphicsDeviceRef_setPipelineCache(
		deviceRef, c::Buffer_createRefConst(garbage, sizeof(garbage)), e_rr
	));
}
