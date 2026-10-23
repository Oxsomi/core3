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
#include "types/container/log.hpp"

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

	//The (key, bytes) pair of key among count pairs, or null

	const c::U64 *TestPipelineCache_find(const c::U64 *pairs, c::U64 count, c::U64 key) {

		for(c::U64 i = 0; i < count; ++i)
			if(pairs[i * 2] == key)
				return pairs + i * 2;

		return nullptr;
	}
}

//A device of its own on the same adapter, so its cache is the one set and nothing else has built pipelines on it

namespace {

	bool TestPipelineCache_ownDevice(
		oxc::c::Test *t, const oxc::c::GraphicsDevice *suite, oxc::c::RefPtrType *instanceType,
		oxc::c::GraphicsInstanceRef **instanceRef, oxc::c::GraphicsDeviceRef **deviceRef
	) {

		using namespace oxc;

		const c::Allocator *alloc = c::Platform_instance->alloc;
		const c::GraphicsInstance *suiteInstance = RefPtr_data(suite->instance, c::GraphicsInstance);

		const c::GraphicsApplicationInfo appInfo = { c::CharString_createRefCStrConst("OxC3 pipeline cache test"), 1, 0 };
		*instanceType = c::GraphicsInstance_makeType(suiteInstance->api, alloc);

		const c::EGraphicsInstanceFlags flags = (c::EGraphicsInstanceFlags) (
			suiteInstance->flags | c::EGraphicsInstanceFlags_DisableGPUBV
		);

		if(!c::Test_assert(t, "ownInstance", c::GraphicsInstance_create(
			&appInfo, suiteInstance->api, flags, alloc, instanceType, instanceRef, &t->err
		)))
			return false;

		c::ListGraphicsDeviceInfo infos{};
		bool created = false;

		if(c::Test_assert(t, "ownInfos", c::GraphicsInstance_getDeviceInfos(
			RefPtr_data(*instanceRef, c::GraphicsInstance), &infos, NULL, &t->err
		))) {

			for(c::U64 i = 0; !created && i < infos.length; ++i)
				if(c::Buffer_eq(
					c::Buffer_createRefConst(infos.ptr[i].name, sizeof(infos.ptr[i].name)),
					c::Buffer_createRefConst(suite->info.name, sizeof(suite->info.name))
				))
					created = c::Test_assert(t, "ownDevice", c::GraphicsDeviceRef_create(
						*instanceRef, &infos.ptr[i], c::EGraphicsDeviceFlags_None, c::EGraphicsBufferingMode_Default,
						NULL, NULL, deviceRef, &t->err
					));
		}

		c::ListGraphicsDeviceInfo_free(&infos, alloc);

		if(!created)
			c::RefPtr_dec(instanceRef);

		return created;
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
	const c::EGraphicsFeatures features = (c::EGraphicsFeatures) dev.info().capabilities.flags.features;

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

	//A cache set on a device that has only built its own pipelines is taken.
	//Those are built at create, before an application can hand the device a cache, so they cannot be what refuses it.
	//Taking it records the sizes the blob holds, and where the driver records sizes the blob holds more than the fresh
	// device's own.
	//A driver whose cache doesn't grow per pipeline (llvmpipe) records none, so the blob is also set with one more pair,
	// which the device only records if it took the blob.

	c::Test_setModule(t, "PipelineCache/internalPipelines");

	c::RefPtrType ownType{};
	c::GraphicsInstanceRef *ownInstance = nullptr;
	c::GraphicsDeviceRef *ownDeviceRef = nullptr;
	c::Buffer synthetic = c::Buffer_createNull();

	if(c::Test_assert(t, "getCacheForOwn", c::GraphicsDeviceRef_getPipelineCache(deviceRef, alloc, &blob, e_rr))) {

		//The header is magic, version, flags, api, padding, then the size count at 8 and the driver's bytes at 16

		const c::U32 sizeCount = c::Buffer_length(blob) >= 24 ? *(const c::U32*) (blob.ptr + 8) : 0;
		const c::U64 driverBytes = c::Buffer_length(blob) >= 24 ? *(const c::U64*) (blob.ptr + 16) : 0;

		if(!driverBytes)
			c::Test_print(t, "The driver keeps no pipeline cache here, skipping");

		else if(TestPipelineCache_ownDevice(t, device, &ownType, &ownInstance, &ownDeviceRef)) {

			c::GraphicsDevice *own = RefPtr_data(ownDeviceRef, c::GraphicsDevice);
			const c::U64 ownSizes = own->pipelineSizes.length / 2;

			Log::debugLn(
				*alloc, "-- internalPipelines: the blob has %" PRIu32 " sizes, the fresh device %" PRIu64, sizeCount, ownSizes
			);

			if(sizeCount > ownSizes) {
				c::Test_assert(t, "setOwnCache", c::GraphicsDeviceRef_setPipelineCache(ownDeviceRef, blob, e_rr));
				c::Test_assert(t, "ownCacheTaken", own->pipelineSizes.length / 2 >= sizeCount);
			}

			//A key neither the blob nor the device holds, looked for since a key is a hash and can be any 64 bit value

			const c::U64 *pairs = (const c::U64*) (blob.ptr + 24);
			c::U64 key = c::U64_MAX;

			while(
				TestPipelineCache_find(pairs, sizeCount, key) ||
				TestPipelineCache_find(own->pipelineSizes.ptr, own->pipelineSizes.length / 2, key)
			)
				--key;

			const c::U64 pair[2] = { key, 4096 };
			const c::U64 head = 24 + sizeCount * sizeof(pair);

			//The blob with that pair after its own: the size count plus one, the pairs, the new pair, then the driver's bytes

			if(
				c::Test_assert(t, "blobLength", c::Buffer_length(blob) == head + driverBytes) &&
				c::Test_assert(t, "syntheticBlob", c::Buffer_createUninitializedBytes(
					head + sizeof(pair) + driverBytes, alloc, &synthetic, e_rr
				))
			) {

				c::U8 *out = synthetic.ptrNonConst;
				const c::Buffer driverData = c::Buffer_createRefConst(blob.ptr + head, driverBytes);

				c::Buffer_memcpy(c::Buffer_createRef(out, head), c::Buffer_createRefConst(blob.ptr, head));
				c::Buffer_memcpy(c::Buffer_createRef(out + head, sizeof(pair)), c::Buffer_createRefConst(pair, sizeof(pair)));
				c::Buffer_memcpy(c::Buffer_createRef(out + head + sizeof(pair), driverBytes), driverData);
				*(c::U32*) (out + 8) = sizeCount + 1;

				c::Test_assert(t, "setSyntheticCache", c::GraphicsDeviceRef_setPipelineCache(ownDeviceRef, synthetic, e_rr));

				const c::U64 *recorded = own->pipelineSizes.ptr;
				const c::U64 recordedCount = own->pipelineSizes.length / 2;
				const c::U64 *taken = TestPipelineCache_find(recorded, recordedCount, key);

				c::Test_assert(t, "syntheticTaken", taken && taken[1] == pair[1]);

				c::Bool everyPair = true;

				for(c::U64 i = 0; i < sizeCount; ++i) {
					taken = TestPipelineCache_find(recorded, recordedCount, pairs[i * 2]);
					everyPair &= taken && taken[1] == pairs[i * 2 + 1];
				}

				c::Test_assert(t, "blobPairsTaken", everyPair);
			}
		}
	}

	c::Buffer_free(&synthetic, alloc);
	c::Buffer_free(&blob, alloc);

	if(ownDeviceRef)
		c::RefPtr_dec(&ownDeviceRef);

	if(ownInstance)
		c::RefPtr_dec(&ownInstance);
}
