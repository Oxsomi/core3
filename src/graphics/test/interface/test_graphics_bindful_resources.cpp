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

//graphics/test/interface/test_graphics_bindful_resources.cpp
//
//Every descriptor KIND reachable through a bindful table, each one executed rather than merely bound:
//samplers, constant buffers, RW textures, arrays, multiple spaces, a register shared by two stages,
//comparison sampling, structured buffers, append counters and float atomics.
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

// -- 43. Bindful sampling: a texture, a sampler and a buffer in one table --------

//The sampler range lands in the separate sampler root table on D3D12 (a second root parameter next to the
// resource one), which no other test exercises; on Vulkan it proves sampler descriptors in a plain set.
//An 8x8 nearest sampled texture at texel centers has to reproduce its texels exactly.

extern "C" void Test_graphicsBindfulSampler(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/sampler");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	const c::Allocator *alloc = dev.alloc();

	gfxtest::OwnedSHFile file(alloc);

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_sampler.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful sampler tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceTexture texture;
	gfx::Sampler sampler;
	gfx::DeviceBuffer output;
	gfx::CommandList commandList, emptyList;

	gfxtest::TableGuard tableGuard{ { &table } };

	gfxtest::OwnedLayoutInfo layoutInfo(alloc);

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(layoutInfo.list, "Bindful sampler layout", layout, e_rr)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxSamplers = 1,
		.maxTextures = 1, .maxBuffersRW = 1, .maxDescriptorTables = 1
	};

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Bindful sampler heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Bindful sampler table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	//An 8x8 RGBA8 texture whose red channel encodes the texel index times three.
	//createTexture takes ownership of the data, so the guard only has anything to free on the paths that
	//never get that far.

	gfx::OwnedList<c::Buffer> texData(alloc);

	if(!Test_assert(t, "texAlloc", c::Buffer_createUninitializedBytes(8 * 8 * 4, alloc, &texData.list, e_rr)))
		return;

	for(c::U64 i = 0; i < 64; ++i) {
		texData.list.ptrNonConst[i * 4] = (c::U8)(i * 3);
		texData.list.ptrNonConst[i * 4 + 1] = 0;
		texData.list.ptrNonConst[i * 4 + 2] = 0;
		texData.list.ptrNonConst[i * 4 + 3] = 0xFF;
	}

	if(!Test_assert(t, "textureCreate", dev.createTexture(
		c::ETextureType_2D, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_ShaderRead, 8, 8, 1,
		"Bindful sampler texture", &texData.list, texture, nullptr, e_rr
	)))
		return;

	//createTexture took ownership of the data; clearing the guard's buffer keeps it from freeing it twice

	texData.list = c::Buffer_createNull();

	const c::SamplerInfo samplerInfo = { .filter = c::ESamplerFilterMode_Nearest };

	if(!Test_assert(t, "samplerCreate", dev.createSampler(
		samplerInfo, "Bindful sampler sampler", sampler, nullptr, true, e_rr
	)))
		return;

	if(!Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Bindful sampler output", 64 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	const c::Descriptor texDesc = c::Descriptor_texture(texture.handle(), 0, 0, 0, 0, 0, 0);
	const c::Descriptor sampDesc = c::Descriptor_sampler(sampler.handle());
	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setTex", table.setByName("tex", texDesc, 0, false, e_rr));
	Test_assert(t, "setSamp", table.setByName("samp", sampDesc, 0, false, e_rr));
	Test_assert(t, "setOutput", table.setByName("output", outputDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Bindful sampler pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Bindful sampler pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(2 * c::KIBI, 32, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scope(
			{
				{ .resource = texture.handle(), .stage = c::EPipelineStage_Compute },
				{ .resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
			},
			1, {}, e_rr
		);

		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatch", scope.dispatch2D(1, 1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == i * 3;

			Test_assert(t, "bindfulSampleResults", allMatch);
		}
}

// -- 47. Bindful constant buffer: the CBV descriptor path, executed --------------

//The only executor of the constant buffer descriptor write (D3D12's CreateConstantBufferView call site is
// dead code without it). The buffer is exactly 256 bytes with EDeviceBufferUsage_Uniform, since a CBV's
// length must be 256-byte aligned and equal the size reflection reported.

extern "C" void Test_graphicsBindfulCbuffer(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/cbuffer");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_cbuffer.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful CBV tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceBuffer consts, output;
	gfx::CommandList commandList, emptyList;

	gfxtest::TableGuard tableGuard{ { &table } };

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(layoutInfo.list, "Bindful CBV layout", layout, e_rr)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxBuffersRW = 1,
		.maxConstantBuffers = 1, .maxDescriptorTables = 1
	};

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Bindful CBV heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Bindful CBV table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	//scaleBias.x = 3, scaleBias.y = 7: the shader writes i * 3 + 7, same values module 41 proves

	c::U32 constsData[64] = { 3, 7, 0, 0 };
	c::Buffer constsRef = c::Buffer_createRefConst(constsData, sizeof(constsData));

	if(!Test_assert(t, "constsCreate", dev.createBufferData(
		c::EDeviceBufferUsage_Uniform, c::EGraphicsResourceFlag_None,
		"Bindful CBV constants", &constsRef, consts, nullptr, e_rr
	)))
		return;

	if(!Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Bindful CBV output", 64 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	const c::Descriptor constsDesc = c::Descriptor_buffer(consts.handle(), 0, 0, NULL, 0);
	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setConsts", table.setByName("consts", constsDesc, 0, false, e_rr));
	Test_assert(t, "setOutput", table.setByName("output", outputDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Bindful CBV pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Bindful CBV pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(2 * c::KIBI, 32, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scope(
			{
				{ .resource = consts.handle(), .stage = c::EPipelineStage_Compute },
				{ .resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
			},
			1, {}, e_rr
		);

		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == i * 3 + 7;

			Test_assert(t, "cbufferResults", allMatch);
		}
}

// -- 48. Bindful RW texture: the storage image descriptor path, executed ---------

//The only executor of a UAV texture in a table. The target is a RenderTexture because DeviceTexture
// creation refuses ShaderWrite; k / 255 stores are exact in 8 bit UNORM, so the pull byte compares.

extern "C" void Test_graphicsBindfulRwTexture(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/rwTexture");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_rwtex.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful RW texture tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::RenderTexture target;
	gfx::CommandList commandList, emptyList;

	gfxtest::TableGuard tableGuard{ { &table } };

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(
		layoutInfo.list, "Bindful RW texture layout", layout, e_rr
	)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxTexturesRW = 1, .maxDescriptorTables = 1 };

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Bindful RW texture heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Bindful RW texture table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	if(!Test_assert(t, "targetCreate", dev.createRenderTexture(
		8, 8, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_ShaderWrite, "Bindful RW texture target",
		target, c::EMSAASamples_Off, nullptr, e_rr
	)))
		return;

	const c::Descriptor targetDesc = c::Descriptor_texture(target.handle(), 0, 0, 0, 0, 0, 0);

	Test_assert(t, "setTarget", table.setByName("outTex", targetDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Bindful RW texture pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Bindful RW texture pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(2 * c::KIBI, 32, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	//No initializing clear on purpose: the first use is the UAV write, which relies on the backend flagging
	// the first transition as a discard (D3D12's NOT_ZEROED rule wants a discard, clear or copy first)

	{
		gfx::CommandScope scope = commandList.scope(
			{ { .resource = target.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true } }, 1, {}, e_rr
		);

		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatch", scope.dispatch2D(1, 1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList)) {

		//Each pixel holds R = i * 3, A = 255, so the packed value is 0xFF000000 | (i * 3)

		c::TestShaderPixels pixels {};

		if (gfxtest::pullPixels(t, dev, emptyList, target.handle(), pixels)) {

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &= pixels.pixels[i] == (0xFF000000u | (i * 3));

			Test_assert(t, "rwTextureResults", allMatch);
		}
	}
}

// -- 49. Bindful descriptor array: one binding, four elements, each addressed ----

//A register array is a single binding with count 4; each element gets its own buffer, filled so every slot
// contributes a distinguishable term. Non bindless sets have no partially-bound semantics, so every element
// the shader statically reads is populated before submit.

extern "C" void Test_graphicsBindfulArray(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/array");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_array.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful array tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceBuffer inputs[4], output;
	gfx::CommandList commandList, emptyList;

	gfxtest::TableGuard tableGuard{ { &table } };

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(layoutInfo.list, "Bindful array layout", layout, e_rr)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxBuffersRW = 5, .maxDescriptorTables = 1 };

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Bindful array heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Bindful array table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	//Element j holds i + j * 1000 at slot i, so the sum 4i + 6000 breaks if any slot is misaddressed

	for(c::U32 j = 0; j < 4; ++j) {

		c::U32 data[64];

		for(c::U32 i = 0; i < 64; ++i)
			data[i] = i + j * 1000;

		c::Buffer dataRef = c::Buffer_createRefConst(data, sizeof(data));

		if(!Test_assert(t, "inputCreate", dev.createBufferData(
			c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
			"Bindful array input", &dataRef, inputs[j], nullptr, e_rr
		)))
			return;

		const c::Descriptor desc = c::Descriptor_buffer(inputs[j].handle(), 0, 0, NULL, 0);

		Test_assert(t, "setInput", table.setByName("inputs", desc, j, false, e_rr));
	}

	if(!Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Bindful array output", 64 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setOutput", table.setByName("output", outputDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Bindful array pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Bindful array pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	const c::Transition transitions[5] = {
		{ .resource = inputs[0].handle(), .stage = c::EPipelineStage_Compute },
		{ .resource = inputs[1].handle(), .stage = c::EPipelineStage_Compute },
		{ .resource = inputs[2].handle(), .stage = c::EPipelineStage_Compute },
		{ .resource = inputs[3].handle(), .stage = c::EPipelineStage_Compute },
		{ .resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
	};

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scopeSpan(transitions, 5, 1, nullptr, 0, e_rr);
		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == i * 4 + 6000;

			Test_assert(t, "arrayResults", allMatch);
		}
}

// -- 50. Bindful multi space: space0 and space1 in one layout --------------------

//space1 maps to a second descriptor set on Vulkan and a distinct RegisterSpace range on D3D12; no other
// layout in the suite leaves space0. The copy result proves both spaces really reached the shader.

extern "C" void Test_graphicsBindfulSpaces(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/spaces");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_spaces.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful spaces tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceBuffer src, output;
	gfx::CommandList commandList, emptyList;

	gfxtest::TableGuard tableGuard{ { &table } };

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(layoutInfo.list, "Bindful spaces layout", layout, e_rr)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxBuffersRW = 2, .maxDescriptorTables = 1 };

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Bindful spaces heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Bindful spaces table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	c::U32 srcData[64];

	for(c::U32 i = 0; i < 64; ++i)
		srcData[i] = i;

	c::Buffer srcRef = c::Buffer_createRefConst(srcData, sizeof(srcData));

	if(!Test_assert(t, "srcCreate", dev.createBufferData(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
		"Bindful spaces src", &srcRef, src, nullptr, e_rr
	)))
		return;

	if(!Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Bindful spaces output", 64 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	const c::Descriptor srcDesc = c::Descriptor_buffer(src.handle(), 0, 0, NULL, 0);
	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setSrc", table.setByName("input", srcDesc, 0, false, e_rr));
	Test_assert(t, "setOutput", table.setByName("output", outputDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Bindful spaces pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Bindful spaces pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(2 * c::KIBI, 32, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scope(
			{
				{ .resource = src.handle(), .stage = c::EPipelineStage_Compute },
				{ .resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
			},
			1, {}, e_rr
		);

		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == i * 5 + 3;

			Test_assert(t, "spacesResults", allMatch);
		}
}

// -- 54. One register visible to two stages -------------------------------------

//A binding both the vertex and the pixel stage read is the only source of a multi stage visibility mask;
// detection unions the two, D3D12 turns that into SHADER_VISIBILITY_ALL and Vulkan into multi bit stage
// flags, none of which any other test reaches (every other layout is compute only or pixel only).
//The vertex stage scales the triangle by a value it reads, so a register that never became visible there
// collapses the triangle and no pixel survives to be compared.

extern "C" void Test_graphicsBindfulSharedRegister(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/sharedRegister");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	if (!(dev.info().capabilities.features & c::EGraphicsFeatures_DirectRendering)) {
		c::Test_print(t, "Device lacks direct rendering, skipping shared register tests");
		return;
	}

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_shared_reg.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping shared register tests");
		return;
	}

	const c::U32 vertexId = gfxtest::entry(t, dev, file.list, "mainVertex");
	const c::U32 pixelId = gfxtest::entry(t, dev, file.list, "mainPixel");

	if(vertexId == c::U32_MAX || pixelId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceBuffer shared;
	gfx::RenderTexture target;
	gfx::CommandList commandList, emptyList;

	gfxtest::TableGuard tableGuard{ { &table } };

	//Detecting from both entries at once is what unions the visibility of the register they share

	const c::U32 entryIds[2] = { vertexId, pixelId };

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayoutFromEntries(
		file.list, entryIds, 2, layoutInfo.list, c::EDescriptorLayoutFlags_None,
		(c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	//The union is the point, so it is asserted rather than assumed from the pixels alone

	c::Bool sharedVisibility = false;

	for(c::U64 i = 0; i < layoutInfo.list.bindings.length; ++i) {

		const c::U32 visibility = layoutInfo.list.bindings.ptr[i].visibility;

		if(
			(visibility & (1 << c::ESHPipelineStage_Vertex)) &&
			(visibility & (1 << c::ESHPipelineStage_Pixel))
		)
			sharedVisibility = true;
	}

	Test_assert(t, "visibilityUnion", sharedVisibility);

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(layoutInfo.list, "Shared register layout", layout, e_rr)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxBuffersRW = 1, .maxDescriptorTables = 1 };

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Shared register heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Shared register table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	//Slot 0 is the vertex stage's scale (1), bytes 16.. are the color the pixel stage returns

	const c::F32 color[4] = { 1, 102.f / 255, 51.f / 255, 1 };
	c::U32 sharedData[8] = { 1, 0, 0, 0 };
	c::Buffer_memcpy(
		c::Buffer_createRef(&sharedData[4], sizeof(c::F32) * 4),
		c::Buffer_createRefConst(color, sizeof(color))
	);

	c::Buffer sharedRef = c::Buffer_createRefConst(sharedData, sizeof(sharedData));

	if(!Test_assert(t, "sharedCreate", dev.createBufferData(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
		"Shared register buffer", &sharedRef, shared, nullptr, e_rr
	)))
		return;

	const c::Descriptor sharedDesc = c::Descriptor_buffer(shared.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setShared", table.setByName("sharedParams", sharedDesc, 0, false, e_rr));

	if(!Test_assert(t, "targetCreate", dev.createRenderTexture(
		8, 8, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Shared register target", target,
		c::EMSAASamples_Off, nullptr, e_rr
	)))
		return;

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Shared register pipeline layout", pipelineLayout, e_rr
	)))
		return;

	const c::PipelineGraphicsInfo pipelineInfo = {
		.attachmentFormatsExt = { c::ETextureFormatId_RGBA8 },
		.attachmentCountExt = 1
	};

	if(!Test_assert(t, "pipelineCreate", dev.createGraphicsPipeline(
		pipelineInfo, file.list, { "mainVertex", "mainPixel" }, "Shared register pipeline", pipeline,
		{}, &pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(2 * c::KIBI, 32, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		//The vertex stage reads it too, so the transition names the earliest stage that touches it

		gfx::CommandScope scope = commandList.scope(
			{ { .resource = shared.handle(), .stage = c::EPipelineStage_Vertex } }, 1, {}, e_rr
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

// -- 57. Comparison sampler against a depth texture -----------------------------

//A SamplerComparisonState is a register type of its own, and writing a descriptor into it is type checked
// against the sampler's own comparison flag, so a plain sampler and a comparison sampler are not
// interchangeable. Only the plain one has ever been written, so the comparison branch never executed, and
// neither did a depth texture read through a table.
//The depth buffer is cleared to a known value by a render pass that draws nothing, then every thread
// compares its own reference against it: with a Less comparison the answer flips at exactly half the
// threads, which no filtering or precision detail can move.

extern "C" void Test_graphicsBindfulSamplerCmp(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/samplerCmp");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	if (!(dev.info().capabilities.features & c::EGraphicsFeatures_DirectRendering)) {
		c::Test_print(t, "Device lacks direct rendering, skipping comparison sampler tests");
		return;
	}

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_sampler_cmp.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping comparison sampler tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DepthStencil depth;
	gfx::RenderTexture colorTarget;
	gfx::Sampler sampler;
	gfx::DeviceBuffer output;
	gfx::CommandList commandList, emptyList;

	gfxtest::TableGuard tableGuard{ { &table } };

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(
		layoutInfo.list, "Comparison sampler layout", layout, e_rr
	)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxSamplers = 1,
		.maxTextures = 1, .maxBuffersRW = 1, .maxDescriptorTables = 1
	};

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Comparison sampler heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Comparison sampler table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	//allowShaderRead is what makes the depth buffer bindable as a texture at all

	if(!Test_assert(t, "depthCreate", dev.createDepthStencil(
		8, 8, c::EDepthStencilFormat_D32, true, "Comparison sampler depth", depth, c::EMSAASamples_Off, e_rr
	)))
		return;

	if(!Test_assert(t, "colorCreate", dev.createRenderTexture(
		8, 8, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Comparison sampler color", colorTarget,
		c::EMSAASamples_Off, nullptr, e_rr
	)))
		return;

	//Comparison samplers are a distinct descriptor: enableComparison is what the table's write checks the
	// register type against

	const c::SamplerInfo samplerInfo = {
		.filter = c::ESamplerFilterMode_Nearest,
		.comparisonFunction = c::ECompareOp_Lt
	,
		.enableComparison = true};

	if(!Test_assert(t, "samplerCreate", dev.createSampler(
		samplerInfo, "Comparison sampler sampler", sampler, nullptr, true, e_rr
	)))
		return;

	if(!Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Comparison sampler output", 64 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	const c::Descriptor depthDesc = c::Descriptor_texture(depth.handle(), 0, 0, 0, 0, 0, 0);
	const c::Descriptor samplerDesc = c::Descriptor_sampler(sampler.handle());
	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setDepth", table.setByName("depthTex", depthDesc, 0, false, e_rr));
	Test_assert(t, "setSampler", table.setByName("cmpSamp", samplerDesc, 0, false, e_rr));
	Test_assert(t, "setOutput", table.setByName("output", outputDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Comparison sampler pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Comparison sampler pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	//A render pass that draws nothing, purely so the depth clear fills the buffer with a known value

	{
		const c::DepthStencilAttachmentInfo depthAttach = {
			.image = depth.handle(),
			.depthLoad = c::ELoadAttachmentType_Clear,
			.clearDepth = 0.5f
		};

		gfx::CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "scopeClear", (c::Bool) scope);

		{
			gfx::CommandRender render = scope.render(
				c::I32x2_zero, c::I32x2_create2(8, 8),
				{ { .image = colorTarget.handle(), .load = c::ELoadAttachmentType_Clear } }, &depthAttach, e_rr
			);

			Test_assert(t, "renderStart", (c::Bool) render);
			Test_assert(t, "renderEnd", render.end(e_rr));
		}

		Test_assert(t, "scopeClearEnd", scope.end(e_rr));
	}

	{
		gfx::CommandScope scope = commandList.scope(
			{
				{ .resource = depth.handle(), .stage = c::EPipelineStage_Compute },
				{ .resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
			},
			2, {}, e_rr
		);

		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			//reference i / 64 against a depth of 0.5 with a Less comparison: the first 32 threads pass

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == (i < 32 ? 1u : 0u);

			Test_assert(t, "comparisonResults", allMatch);
		}
}

// -- 58. Structured buffers: element addressing and stride ----------------------

//Every other bindful buffer in the suite is byte addressed, so the structured path is untouched: the table
// validates the descriptor's range against the stride reflection reported, and the shader indexes by
// element rather than by byte. Each of the four fields is transformed differently, so a stride the backend
// got wrong shows up as a specific field landing on a neighbour's value.

typedef struct TestBindfulElem {
	c::U32 a, b, c, d;
} TestBindfulElem;

extern "C" void Test_graphicsBindfulStructured(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/structured");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_structured.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping structured buffer tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceBuffer input, output;
	gfx::CommandList commandList, emptyList;

	gfxtest::TableGuard tableGuard{ { &table } };

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	//The stride is what makes this a structured buffer rather than a byte address one, so it is asserted
	// rather than assumed from the results

	c::Bool strideMatches = false;

	for(c::U64 i = 0; i < layoutInfo.list.bindings.length; ++i)
		if(layoutInfo.list.bindings.ptr[i].structedBufferStride == sizeof(TestBindfulElem))
			strideMatches = true;

	Test_assert(t, "reflectedStride", strideMatches);

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(layoutInfo.list, "Structured layout", layout, e_rr)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxBuffersRW = 2, .maxDescriptorTables = 1 };

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Structured heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Structured table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	TestBindfulElem elems[64];

	for(c::U32 i = 0; i < 64; ++i)
		elems[i] = { .a = i, .b = i * 7, .c = i * 3, .d = i * 11 };

	c::Buffer inputRef = c::Buffer_createRefConst(elems, sizeof(elems));

	if(!Test_assert(t, "inputCreate", dev.createBufferData(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
		"Structured input", &inputRef, input, nullptr, e_rr
	)))
		return;

	if(!Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Structured output", sizeof(elems), output, nullptr, e_rr
	)))
		return;

	const c::Descriptor inputDesc = c::Descriptor_buffer(input.handle(), 0, 0, NULL, 0);
	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setInput", table.setByName("input", inputDesc, 0, false, e_rr));
	Test_assert(t, "setOutput", table.setByName("output", outputDesc, 0, false, e_rr));

	//A range that isn't a whole number of elements has to be refused, which is the stride rule itself

	const c::Descriptor misalignedDesc = c::Descriptor_buffer(input.handle(), 0, sizeof(elems) - 4, NULL, 0);

	Test_assert(t, "misalignedRangeRefused", !table.setByName("input", misalignedDesc, 0, true, nullptr));

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Structured pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Structured pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(2 * c::KIBI, 32, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scope(
			{
				{ .resource = input.handle(), .stage = c::EPipelineStage_Compute },
				{ .resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
			},
			1, {}, e_rr
		);

		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			const TestBindfulElem *values = (const TestBindfulElem*) output.data()->cpuData.ptr;

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &=
					values[i].a == i * 2 &&
					values[i].b == i * 7 + 100 &&
					values[i].c == ((i * 3) ^ 0xFFu) &&
					values[i].d == i * 11 + i;

			Test_assert(t, "structuredResults", allMatch);
		}
}

// -- 59. Append buffer with a counter resource ----------------------------------

//A counter is a second resource hanging off one descriptor, and the table only accepts one on an
// Append/Consume register, which nothing has ever exercised. Vulkan's descriptor write refuses counters
// outright today, so this executes the append on D3D12 and pins that documented refusal on Vulkan, which
// keeps the boundary visible rather than leaving the backend difference implicit.

extern "C" void Test_graphicsBindfulAppendCounter(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/appendCounter");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	const c::Bool isVulkan = dev.api() == c::EGraphicsApi_Vulkan;

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_append.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping append counter tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceBuffer appended, counter;
	gfx::CommandList commandList, emptyList;

	gfxtest::TableGuard tableGuard{ { &table } };

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(layoutInfo.list, "Append counter layout", layout, e_rr)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxBuffersRW = 2, .maxDescriptorTables = 1 };

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Append counter heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Append counter table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	const c::EGraphicsResourceFlag writeBacked =
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked);

	if(!Test_assert(t, "appendedCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, writeBacked, "Append counter data", 64 * sizeof(c::U32), appended, nullptr, e_rr
	)))
		return;

	if(!Test_assert(t, "counterCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, writeBacked, "Append counter counter", 4, counter, nullptr, e_rr
	)))
		return;

	const c::Descriptor appendDesc = c::Descriptor_buffer(appended.handle(), 0, 0, counter.handle(), 0);

	if (isVulkan) {

		//Vulkan's descriptor write reports the counter as unimplemented rather than silently ignoring it,
		// which is what keeps a shader from reading a counter that was never bound

		Test_assert(t, "counterRefusedOnVulkan", !table.setByName("appended", appendDesc, 0, false, nullptr));

		c::Test_print(t, "Vulkan has no append/consume counter support yet, refusal pinned instead of executing");
		return;
	}

	if(!Test_assert(t, "setAppended", table.setByName("appended", appendDesc, 0, false, e_rr)))
		return;

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Append counter pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Append counter pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(2 * c::KIBI, 32, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scope(
			{
				{ .resource = appended.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true },
				{ .resource = counter.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
			},
			1, {}, e_rr
		);

		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, counter)) {

			//One in every four threads appended, so the counter is exactly a quarter of the 64 threads

			const c::U32 counterValue = *(const c::U32*) counter.data()->cpuData.ptr;

			Test_assert(t, "counterValue", counterValue == 16);
		}
}

// -- 62. Float atomics, reachable at last through a bindful layout --------------

//AtomicF32 was compile-tested only, because oxc::AtomicAddF32 takes its target by reference and so needs a
// typed float lvalue, while the bindless resource set exposes nothing but RWByteAddressBuffer. A bindful
// layout can declare RWStructuredBuffer<float> at a classic register, which is exactly the missing piece.
//The extension has no DXIL intrinsic (ESHExtension_NoDxilCompile), so only a SPIRV backend has an
// entrypoint to run; the module skips wherever that binary doesn't exist rather than failing.

static void TestBindful_atomicFloatWithWidth(c::Test *t, gfx::Device &dev, c::Bool isDouble) {

	c::Error *e_rr = &t->err;

	const c::C8 *path =
		isDouble ?
		"//OxC3_gtest/test_shaders/test_bindful_atomic_f64.oiSH" :
		"//OxC3_gtest/test_shaders/test_bindful_atomic_f32.oiSH";

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, path, file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping float atomic tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if (entryId == c::U32_MAX) {
		c::Test_print(t, "Float atomics have no DXIL intrinsic, so this backend has no entrypoint to run");
		return;
	}

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceBuffer output;
	gfx::CommandList commandList, emptyList;

	gfxtest::TableGuard tableGuard{ { &table } };

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(layoutInfo.list, "Float atomic layout", layout, e_rr)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxBuffersRW = 1, .maxDescriptorTables = 1 };

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Float atomic heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Float atomic table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	//The buffer starts zeroed, so the accumulated total is exactly the thread count

	c::F64 initial[64] = { 0 };
	const c::U64 elemSize = isDouble ? sizeof(c::F64) : sizeof(c::F32);
	c::Buffer initialRef = c::Buffer_createRefConst(initial, elemSize * 64);

	if(!Test_assert(t, "outputCreate", dev.createBufferData(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Float atomic output", &initialRef, output, nullptr, e_rr
	)))
		return;

	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setOutput", table.setByName("buf", outputDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Float atomic pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Float atomic pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(2 * c::KIBI, 32, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scope(
			{ { .resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true } }, 1, {}, e_rr
		);

		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			//64 threads each add one, and a lost update lands short: the sum is exact at either width

			const void *ptr = output.data()->cpuData.ptr;
			const c::F64 total = isDouble ? *(const c::F64*) ptr : (c::F64) *(const c::F32*) ptr;

			Test_assert(t, "atomicFloatTotal", total == 64.0);
		}
}

//AtomicF32 and AtomicF64 are separate device capabilities and separate SPIRV capabilities, so each is run
//only where it is claimed; neither has a DXIL intrinsic, so a DXIL backend simply has no entrypoint.

extern "C" void Test_graphicsBindfulAtomicFloat(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/atomicFloat");

	gfx::Device dev = gfx::Device::share(deviceRef);

	const c::GraphicsDeviceCapabilities caps = dev.info().capabilities;

	if(caps.dataTypes & c::EGraphicsDataTypes_AtomicF32)
		TestBindful_atomicFloatWithWidth(t, dev, false);

	else c::Test_print(t, "Device lacks 32 bit float atomics, skipping that width");

	if(caps.dataTypes & c::EGraphicsDataTypes_AtomicF64)
		TestBindful_atomicFloatWithWidth(t, dev, true);

	else c::Test_print(t, "Device lacks 64 bit float atomics, skipping that width");
}

//The same shader as Bindful/sampler, but its sampler is IMMUTABLE.
//A baked sampler is described in the layout itself rather than bound: D3D12 puts it in the root signature as
// a static sampler, costing none of the 64 DWORD budget and no descriptor range, and Vulkan puts it in the
// set layout as pImmutableSamplers.
//Nothing writes it into the table, so a correct result is only possible if the layout carried it.

extern "C" void Test_graphicsBindfulStaticSampler(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;

	c::Test_setModule(t, "Bindful/staticSampler");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	const c::Allocator *alloc = dev.alloc();

	gfxtest::OwnedSHFile file(alloc);

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_sampler.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping static sampler tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceTexture texture;
	gfx::Sampler sampler;
	gfx::DeviceBuffer output;
	gfx::CommandList commandList, emptyList;

	gfxtest::TableGuard tableGuard{ { &table } };

	//Created before the layout, because the layout is what bakes it.
	//Bindless is disallowed on purpose: a baked sampler needs no bindless slot.

	const c::SamplerInfo samplerInfo = { .filter = c::ESamplerFilterMode_Nearest };

	if(!Test_assert(t, "samplerCreate", dev.createSampler(
		samplerInfo, "Static sampler", sampler, nullptr, true, e_rr
	)))
		return;

	gfxtest::OwnedLayoutInfo layoutInfo(alloc);

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	//The id is 1 based so a binding naming no sampler can leave the union zeroed.

	c::U32 immutableId = 0;

	if(!Test_assert(t, "addImmutableSampler", c::DescriptorLayoutInfo_addImmutableSampler(
		&layoutInfo.list, sampler.handle(), &immutableId, alloc, e_rr
	)))
		return;

	Test_assert(t, "immutableIdIsOneBased", immutableId == 1);

	//Reflection named the registers, so the sampler is found by the name the shader gave it.

	c::Bool marked = false;

	for (c::U64 i = 0; i < layoutInfo.list.bindingNames.length; ++i) {

		const c::CharString wanted = c::CharString_createRefCStrConst("samp");

		if(!c::CharString_equalsStringSensitive(&layoutInfo.list.bindingNames.ptr[i], &wanted))
			continue;

		layoutInfo.list.bindings.ptrNonConst[i].immutableSamplerId = immutableId;
		marked = true;
		break;
	}

	if(!Test_assert(t, "foundSamplerBinding", marked))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(layoutInfo.list, "Static sampler layout", layout, e_rr)))
		return;

	//maxSamplers is still declared: Vulkan allocates a descriptor for an immutable sampler binding even
	// though nothing ever writes it, while D3D12 needs none at all.

	c::DescriptorHeapInfo heapInfo = { .maxSamplers = 1,
		.maxTextures = 1, .maxBuffersRW = 1, .maxDescriptorTables = 1
	};

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Static sampler heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Static sampler table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	gfx::OwnedList<c::Buffer> texData(alloc);

	if(!Test_assert(t, "texAlloc", c::Buffer_createUninitializedBytes(8 * 8 * 4, alloc, &texData.list, e_rr)))
		return;

	for(c::U64 i = 0; i < 64; ++i) {
		texData.list.ptrNonConst[i * 4] = (c::U8)(i * 3);
		texData.list.ptrNonConst[i * 4 + 1] = 0;
		texData.list.ptrNonConst[i * 4 + 2] = 0;
		texData.list.ptrNonConst[i * 4 + 3] = 0xFF;
	}

	if(!Test_assert(t, "textureCreate", dev.createTexture(
		c::ETextureType_2D, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_ShaderRead, 8, 8, 1,
		"Static sampler texture", &texData.list, texture, nullptr, e_rr
	)))
		return;

	texData.list = c::Buffer_createNull();

	if(!Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Static sampler output", 64 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	const c::Descriptor texDesc = c::Descriptor_texture(texture.handle(), 0, 0, 0, 0, 0, 0);
	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, NULL, 0);

	//"samp" is deliberately NOT set: if the baked sampler did not reach the shader, the sample below reads
	// through a descriptor nothing ever wrote.

	Test_assert(t, "setTex", table.setByName("tex", texDesc, 0, false, e_rr));
	Test_assert(t, "setOutput", table.setByName("output", outputDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Static sampler pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Static sampler pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	if(
		!Test_assert(t, "listCreate", dev.createCommandList(2 * c::KIBI, 32, 16, commandList, true, e_rr)) ||
		!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr))
	)
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scope(
			{
				{ .resource = texture.handle(), .stage = c::EPipelineStage_Compute },
				{ .resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
			},
			1, {}, e_rr
		);

		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatch", scope.dispatch2D(1, 1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			c::Bool match = true;

			for(c::U32 i = 0; i < 64; ++i)
				match &= values[i] == (c::U32)(i * 3);

			Test_assert(t, "staticSamplerResults", match);
		}
}
