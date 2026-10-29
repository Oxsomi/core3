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

//graphics/test/interface/test_graphics_wave_size.cpp
//
//A compute entry's [WaveSize], dispatched: where the device runs a size the entry names, the entry is found and its
// waves run at that size, which the shader writes back; where it runs none, the entry is refused.
//SPIR-V carries no wave size, so on Vulkan only the pipeline's request (VK_EXT_subgroup_size_control) makes the size
// the one the entry names.
//An entry whose size isn't the device's default subgroup size tells a request from none.

#include "test_graphics_shared.hpp"
#include "types/container/log.hpp"

namespace oxc { namespace c {
	#include "graphics/generic/device_buffer.h"
	#include "formats/oiSH/sh_file.h"
}}

typedef struct TestWaveSizeEntry {
	const oxc::c::C8 *path;
	const oxc::c::C8 *entry;
} TestWaveSizeEntry;

static const TestWaveSizeEntry testWaveSizeEntries[] = {
	{ "//OxC3_gtest/test_shaders/test_wave_size.oiSH",       "mainW16" },
	{ "//OxC3_gtest/test_shaders/test_wave_size.oiSH",       "mainW32" },
	{ "//OxC3_gtest/test_shaders/test_wave_size.oiSH",       "mainW64" },
	{ "//OxC3_gtest/test_shaders/test_wave_size_range.oiSH", "main" }
};

//Runs one entry and returns the lane counts its waves ran at, ORed, or 0 where it couldn't run.

static oxc::c::U32 Test_runWaveSize(
	oxc::c::Test *t, oxc::gfx::Device &dev, const oxc::gfx::CommandList &emptyList, const oxc::c::SHFile &file,
	const oxc::c::C8 *entry
) {

	using namespace oxc;
	using namespace oxc::gfx;
	using namespace oxc::gfxtest;

	c::Error *e_rr = &t->err;

	const c::U32 entryId = dev.getFirstShaderEntry(file, entry, c::ESHExtension_None, c::ESHExtension_None);

	DeviceBuffer output;
	PipelineLayout layout;
	Pipeline pipeline;
	CommandList commandList;

	if(!(
		c::Test_assert(t, "waveSizeOutput", dev.createBuffer(
			c::EDeviceBufferUsage_None,
			(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWriteBindless | c::EGraphicsResourceFlag_CPUBacked),
			"Wave size output", 16, output, nullptr, e_rr
		)) &&
		pushConstantLayout(t, dev, file, entryId, layout) &&
		c::Test_assert(t, "waveSizePipeline", dev.createComputePipeline(
			file, entry, "Wave size pipeline", pipeline, {}, &layout, e_rr
		)) &&
		c::Test_assert(t, "waveSizeList", dev.createCommandList(2 * c::KIBI, 64, 16, commandList, true, e_rr)) &&
		c::Test_assert(t, "waveSizeBegin", commandList.begin(true, e_rr))
	))
		return 0;

	const c::Transition outputWrite = {
		.resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
	};

	const c::U32 pushData[4] = { output.writeHandle(), 0, 0, 0 };

	{
		CommandScope scope = commandList.scope({ outputWrite }, 1, {}, e_rr);

		if (
			!(
			c::Test_assert(t, "waveSizeScope", (c::Bool) scope) &&
			c::Test_assert(t, "waveSizeBind", scope.setComputePipeline(pipeline, e_rr)) &&
			c::Test_assert(t, "waveSizePush", scope.setPushConstants(pushData, e_rr)) &&
			c::Test_assert(t, "waveSizeDispatch", scope.dispatch1D(1, e_rr)) &&
			c::Test_assert(t, "waveSizeScopeEnd", scope.end(e_rr))
		))
			return 0;
	}

	if (
		!(
		c::Test_assert(t, "waveSizeEnd", commandList.end(e_rr)) &&
		submitAndWait(t, dev, commandList) &&
		pullBuffer(t, dev, emptyList, output) &&
		c::Test_assert(t, "waveSizeReadback", c::Buffer_length(output.data()->cpuData) >= 4)
	))
		return 0;

	return *(const c::U32*) output.data()->cpuData.ptr;
}

extern "C" void Test_graphicsWaveSize(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;
	using namespace oxc::gfxtest;

	c::Error *e_rr = &t->err;

	c::Test_setModule(t, "GraphicsDevice/waveSize");

	Device dev = Device::share(deviceRef);
	const c::GraphicsDeviceCapabilities caps = dev.info().capabilities;

	if(!(caps.flags.features & c::EGraphicsFeatures_SubgroupOperations)) {
		c::Test_print(t, "Device has no subgroup operations, skipping");
		return;
	}

	if(!dev.hasBindlessTable()) {
		c::Test_print(t, "Device has no bindless descriptor table, skipping");
		return;
	}

	//What a device claims has to be a range [WaveSize] can name: powers of two from 4 to 128, the low end first.

	const c::Bool rangeValid =
		caps.waveSizeMin >= 4 && caps.waveSizeMax <= 128 && caps.waveSizeMin <= caps.waveSizeMax &&
		!(caps.waveSizeMin & (caps.waveSizeMin - 1)) && !(caps.waveSizeMax & (caps.waveSizeMax - 1));

	if(!c::Test_assert(t, "waveSizeRange", rangeValid))
		return;

	CommandList emptyList;

	if (
		!(
		c::Test_assert(t, "waveSizeEmptyList", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)) &&
		c::Test_assert(t, "waveSizeEmptyBegin", emptyList.begin(true, e_rr)) &&
		c::Test_assert(t, "waveSizeEmptyEnd", emptyList.end(e_rr))
	))
		return;

	const c::Bool isD3D12 = dev.api() == c::EGraphicsApi_Direct3D12;
	c::U32 ran = 0, refused = 0;

	for(c::U64 i = 0; i < sizeof(testWaveSizeEntries) / sizeof(testWaveSizeEntries[0]); ++i) {

		const TestWaveSizeEntry *test = &testWaveSizeEntries[i];
		OwnedSHFile file(dev.alloc());

		if(!loadFile(t, test->path, file.list)) {
			c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping");
			return;
		}

		//The oiSH every backend shares keeps the size DXIL reflected, which is what Vulkan asks for.

		c::U16 waveSize = 0;
		const c::CharString name = c::CharString_createRefCStrConst(test->entry);

		for(c::U64 j = 0; j < file.list.entries.length; ++j)
			if(c::CharString_equalsStringSensitive(&file.list.entries.ptr[j].name, &name))
				waveSize = file.list.entries.ptr[j].waveSize;

		if(!c::Test_assert(t, "waveSizeInEntry", waveSize != 0))
			continue;

		//D3D12 also needs the shader model each form takes.

		const c::Bool ranged = c::SHEntry_waveSizeLanes(waveSize, c::ESHWaveSize_Min) != 0;

		const c::U32 model = ranged ? c::EDxGraphicsFeatures_WaveSizeMinMax : c::EDxGraphicsFeatures_WaveSize;
		const c::U8 picked = c::GraphicsDeviceCapabilities_pickWaveSize(&caps, waveSize, caps.waveSizeMin);
		const c::Bool runs = picked && (!isD3D12 || (caps.flags.featuresExt & model));

		const c::U32 entryId = dev.getFirstShaderEntry(file.list, test->entry, c::ESHExtension_None, c::ESHExtension_None);

		if(!runs) {
			c::Test_assert(t, "waveSizeRefused", entryId == c::U32_MAX);
			++refused;
			continue;
		}

		if(!c::Test_assert(t, "waveSizeFound", entryId != c::U32_MAX))
			continue;

		const c::U32 lanes = Test_runWaveSize(t, dev, emptyList, file.list, test->entry);

		//A required size is exact. A ranged one is the preferred size where the device runs it, on Vulkan, which
		// picks it; D3D12's driver picks, so there the waves only have to share one size from the range.

		const c::U8 low = c::U8_max(c::SHEntry_waveSizeLanes(waveSize, c::ESHWaveSize_Min), caps.waveSizeMin);
		const c::U8 high = c::U8_min(c::SHEntry_waveSizeLanes(waveSize, c::ESHWaveSize_Max), caps.waveSizeMax);
		const c::U8 preferred = c::SHEntry_waveSizeLanes(waveSize, c::ESHWaveSize_Preferred);

		const c::Bool exact = !ranged || (!isD3D12 && preferred >= low && preferred <= high);
		const c::Bool oneSize = lanes && !(lanes & (lanes - 1)) && lanes >= low && lanes <= high;

		if(!c::Test_assert(t, "waveSizeLanes", exact ? lanes == picked : oneSize))
			Log::debugLn(
				*dev.alloc(), "-- waveSize: %s ran at lanes 0x%08X, expected %" PRIu8 " (device %" PRIu8 " to %" PRIu8 ")",
				test->entry, lanes, picked, caps.waveSizeMin, caps.waveSizeMax
			);

		++ran;
	}

	Log::debugLn(
		*dev.alloc(), "-- waveSize: %" PRIu32 " entries ran at their size and %" PRIu32 " were refused (device %" PRIu8
		" to %" PRIu8 " lanes)", ran, refused, caps.waveSizeMin, caps.waveSizeMax
	);
}
