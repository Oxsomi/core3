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

//graphics/test/interface/test_graphics_bindful_draw.cpp
//
//Bindful draws: a graphics pipeline sourcing its color through a table, indirect dispatch from CPU and
//GPU written arguments, and the fixed function suite on a layout with no bindings at all.
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

//The shaders test file keeps its own copies of these, but those pass a NULL pipeline layout, which means
//the device's default (bindless) layout; everything here brings its own layout instead.

static c::Bool TestBindful_graphicsPipeline(
	c::Test *t,
	gfx::Device &dev,
	const c::SHFile *files,
	c::U64 fileCount,
	c::U16 vertexFile,
	c::U16 pixelFile,
	const c::C8 *vertexEntry,
	const c::C8 *pixelEntry,
	const c::PipelineGraphicsInfo &info,
	const gfx::PipelineLayout &layout,
	gfx::Pipeline &pipeline
) {

	const c::U32 vertexId = gfxtest::entry(t, dev, files[vertexFile], vertexEntry);
	const c::U32 pixelId = gfxtest::entry(t, dev, files[pixelFile], pixelEntry);

	if(vertexId == c::U32_MAX || pixelId == c::U32_MAX)
		return false;

	return Test_assert(t, "createGraphicsPipeline", dev.createGraphicsPipeline(
		info, files, fileCount, { { vertexEntry, vertexFile }, { pixelEntry, pixelFile } },
		"Bindful test graphics pipeline", pipeline, {}, &layout, &t->err
	));
}

//Opens a scope, starts a cleared render into the 8x8 target and binds the pipeline with full viewport and scissor

static gfxtest::DrawPass TestBindful_openDraw(
	c::Test *t, gfx::CommandList &commandList, c::U32 scopeId, c::RefPtr *target, const gfx::Pipeline &pipeline
) {

	c::Error *e_rr = &t->err;

	gfx::CommandScope scope = commandList.scope({}, scopeId, {}, e_rr);
	c::Bool ok = Test_assert(t, "scope", (c::Bool) scope);

	const c::AttachmentInfo color = { .image = target, .load = c::ELoadAttachmentType_Clear };

	gfx::CommandRender render = scope.render(c::I32x2_zero, c::I32x2_create2(8, 8), { color }, nullptr, e_rr);

	ok &= Test_assert(t, "renderStart", (c::Bool) render);
	ok &= Test_assert(t, "viewportScissor", render.setViewportAndScissor(c::I32x2_zero, c::I32x2_zero, e_rr));

	const c::Bool bound = Test_assert(t, "bindPipeline", render.setGraphicsPipeline(pipeline, e_rr));

	return gfxtest::DrawPass{
		static_cast<gfx::CommandScope&&>(scope), static_cast<gfx::CommandRender&&>(render), bound && ok
	};
}

//Taken as the two halves rather than as a DrawPass, because the cases that need their own attachments open
//the scope and the render themselves and still close them the same way.

static c::Bool TestBindful_closeDraw(
	c::Test *t, gfx::CommandScope &scope, gfx::CommandRender &render, gfx::CommandList &commandList
) {
	c::Bool ok = Test_assert(t, "renderEnd", render.end(&t->err));
	ok &= Test_assert(t, "scopeEnd", scope.end(&t->err));
	return Test_assert(t, "end", commandList.end(&t->err)) && ok;
}

static c::Bool TestBindful_closeDraw(c::Test *t, gfxtest::DrawPass &pass, gfx::CommandList &commandList) {
	return TestBindful_closeDraw(t, pass.scope, pass.render, commandList);
}

// -- 44. Bindful draw: a graphics pipeline sourcing its color through a table ----

//The draw work op validates the same custom layout state the dispatches do, but through the graphics bind
// point (its own root signature slot on D3D12, its own descriptor set bind on Vulkan), which nothing else
// covers. A fullscreen triangle reads its color from a classic register and every pixel has to match it.

extern "C" void Test_graphicsBindfulDraw(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/draw");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	if (!(dev.info().capabilities.features & c::EGraphicsFeatures_DirectRendering)) {
		c::Test_print(t, "Device lacks direct rendering, skipping bindful draw tests");
		return;
	}

	gfxtest::OwnedSHFile vertexFile(dev.alloc()), pixelFile(dev.alloc());

	if (
		!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_draw_vs.oiSH", vertexFile.list) ||
		!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_draw_ps.oiSH", pixelFile.list)
	) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful draw tests");
		return;
	}

	const c::SHFile files[2] = { vertexFile.list, pixelFile.list };

	const c::U32 vertexId = gfxtest::entry(t, dev, files[0], "main");
	const c::U32 pixelId = gfxtest::entry(t, dev, files[1], "main");

	if(vertexId == c::U32_MAX || pixelId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceBuffer color;
	gfx::RenderTexture target;
	gfx::CommandList commandList, emptyList;

	//The table holds no reference of its own, so its descriptor goes back before the buffer it names does.

	gfxtest::TableGuard tableGuard{ { &table } };

	//Only the pixel shader owns a register, but the layout comes from both stages on principle

	const c::U32 entryIds[1] = { pixelId };

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayoutFromEntries(
		files[1], entryIds, 1, layoutInfo.list, c::EDescriptorLayoutFlags_None,
		(c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(layoutInfo.list, "Bindful draw layout", layout, e_rr)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxBuffersRW = 1, .maxDescriptorTables = 1 };

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Bindful draw heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Bindful draw table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	//The color the pixel shader reads; exact in 8 bit UNORM so the byte compare can't flake

	const c::F32 colorData[4] = { 1, 102.f / 255, 51.f / 255, 1 };
	c::Buffer colorRef = c::Buffer_createRefConst(colorData, sizeof(colorData));

	if(!Test_assert(t, "colorCreate", dev.createBufferData(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
		"Bindful draw color", &colorRef, color, nullptr, e_rr
	)))
		return;

	const c::Descriptor colorDesc = c::Descriptor_buffer(color.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setColor", table.setByName("color", colorDesc, 0, false, e_rr));

	if(!Test_assert(t, "targetCreate", dev.createRenderTexture(
		8, 8, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Bindful draw target", target,
		c::EMSAASamples_Off, nullptr, e_rr
	)))
		return;

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Bindful draw pipeline layout", pipelineLayout, e_rr
	)))
		return;

	const c::PipelineGraphicsInfo pipelineInfo = {
		.attachmentFormatsExt = { c::ETextureFormatId_RGBA8 },
		.attachmentCountExt = 1
	};

	if(!Test_assert(t, "pipelineCreate", dev.createGraphicsPipeline(
		pipelineInfo, files, 2, { { "main", 0 }, { "main", 1 } }, "Bindful draw pipeline", pipeline,
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
		gfx::CommandScope scope = commandList.scope(
			{ { .resource = color.handle(), .stage = c::EPipelineStage_Pixel } }, 1, {}, e_rr
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

// -- 51. Bindful indirect dispatch: CPU written and GPU written arguments --------

//Indirect execution was proven only through bindless, so a device without it had none. Two cases here:
// arguments uploaded by the CPU and read at a nonzero offset, and arguments a dispatch writes on the GPU
// that the next scope consumes in the same submit.
//The consumer is the same write shader module 41 uses, so the values (i * 3 + 7) prove which threads ran:
// one group means 64 slots, the GPU written { 2, 1, 1 } means 128.

extern "C" void Test_graphicsBindfulIndirect(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/indirect");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	gfxtest::OwnedSHFile writeFile(dev.alloc()), argsFile(dev.alloc());

	if (
		!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_write.oiSH", writeFile.list) ||
		!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_write_args.oiSH", argsFile.list)
	) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful indirect tests");
		return;
	}

	const c::U32 writeId = gfxtest::entry(t, dev, writeFile.list, "main");
	const c::U32 argsId = gfxtest::entry(t, dev, argsFile.list, "main");

	if(writeId == c::U32_MAX || argsId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layoutWrite, layoutArgs;
	gfx::DescriptorTable tableCpu, tableGpu, tableArgs;
	gfx::PipelineLayout pipelineLayoutWrite, pipelineLayoutArgs;
	gfx::Pipeline pipelineWrite, pipelineArgs;
	gfx::DeviceBuffer outputCpu, outputGpu, cpuArgs, gpuArgs;
	gfx::CommandList commandList, emptyList;

	gfxtest::TableGuard tableGuard{ { &tableCpu, &tableGpu, &tableArgs } };

	gfxtest::OwnedLayoutInfo layoutWriteInfo(dev.alloc()), layoutArgsInfo(dev.alloc());

	if(!Test_assert(t, "detectLayoutWrite", dev.detectLayout(
		writeFile.list, writeId, layoutWriteInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "detectLayoutArgs", dev.detectLayout(
		argsFile.list, argsId, layoutArgsInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutWriteCreate", dev.createDescriptorLayout(
		layoutWriteInfo.list, "Bindful indirect write layout", layoutWrite, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutArgsCreate", dev.createDescriptorLayout(
		layoutArgsInfo.list, "Bindful indirect args layout", layoutArgs, e_rr
	)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxBuffersRW = 3, .maxDescriptorTables = 3 };

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Bindful indirect heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCpuCreate", heap.createTable(
		layoutWrite, "Bindful indirect CPU args table", tableCpu, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	if(!Test_assert(t, "tableGpuCreate", heap.createTable(
		layoutWrite, "Bindful indirect GPU args table", tableGpu, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	if(!Test_assert(t, "tableArgsCreate", heap.createTable(
		layoutArgs, "Bindful indirect args writer table", tableArgs, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	//Separate outputs per case, so a case that silently did nothing can't pass on the other's values

	const c::EGraphicsResourceFlag writeBacked =
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked);

	if(!Test_assert(t, "outputCpuCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, writeBacked, "Bindful indirect CPU output", 64 * sizeof(c::U32),
		outputCpu, nullptr, e_rr
	)))
		return;

	if(!Test_assert(t, "outputGpuCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, writeBacked, "Bindful indirect GPU output", 128 * sizeof(c::U32),
		outputGpu, nullptr, e_rr
	)))
		return;

	//The dispatch arguments sit at byte 16, so the read also proves offset addressing into the buffer

	const c::U32 cpuArgsData[8] = { 0, 0, 0, 0, 1, 1, 1, 0 };
	c::Buffer cpuArgsRef = c::Buffer_createRefConst(cpuArgsData, sizeof(cpuArgsData));

	if(!Test_assert(t, "cpuArgsCreate", dev.createBufferData(
		c::EDeviceBufferUsage_Indirect, c::EGraphicsResourceFlag_None,
		"Bindful indirect CPU args", &cpuArgsRef, cpuArgs, nullptr, e_rr
	)))
		return;

	if(!Test_assert(t, "gpuArgsCreate", dev.createBuffer(
		c::EDeviceBufferUsage_Indirect, c::EGraphicsResourceFlag_ShaderWrite,
		"Bindful indirect GPU args", 32, gpuArgs, nullptr, e_rr
	)))
		return;

	const c::Descriptor outputCpuDesc = c::Descriptor_buffer(outputCpu.handle(), 0, 0, NULL, 0);
	const c::Descriptor outputGpuDesc = c::Descriptor_buffer(outputGpu.handle(), 0, 0, NULL, 0);
	const c::Descriptor gpuArgsDesc = c::Descriptor_buffer(gpuArgs.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setOutputCpu", tableCpu.setByName("output", outputCpuDesc, 0, false, e_rr));
	Test_assert(t, "setOutputGpu", tableGpu.setByName("output", outputGpuDesc, 0, false, e_rr));
	Test_assert(t, "setGpuArgs", tableArgs.setByName("args", gpuArgsDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutWriteInfo = { .bindings = layoutWrite.handle() };

	if(!Test_assert(t, "pipelineLayoutWriteCreate", dev.createPipelineLayout(
		pipelineLayoutWriteInfo, "Bindful indirect write pipeline layout", pipelineLayoutWrite, e_rr
	)))
		return;

	c::PipelineLayoutInfo pipelineLayoutArgsInfo = { .bindings = layoutArgs.handle() };

	if(!Test_assert(t, "pipelineLayoutArgsCreate", dev.createPipelineLayout(
		pipelineLayoutArgsInfo, "Bindful indirect args pipeline layout", pipelineLayoutArgs, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineWriteCreate", dev.createComputePipeline(
		writeFile.list, "main", "Bindful indirect write pipeline", pipelineWrite, {}, &pipelineLayoutWrite, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineArgsCreate", dev.createComputePipeline(
		argsFile.list, "main", "Bindful indirect args pipeline", pipelineArgs, {}, &pipelineLayoutArgs, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	//The indirect buffer itself needs no transition; only a buffer a shader writes does

	const c::Transition outputCpuWrite = {
		.resource = outputCpu.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
	};

	const c::Transition argsWrite = {
		.resource = gpuArgs.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
	};

	const c::Transition outputGpuWrite = {
		.resource = outputGpu.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
	};

	//CPU written arguments: one group, so exactly the first 64 slots come back written

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scope({ outputCpuWrite }, 1, {}, e_rr);
		Test_assert(t, "scopeCpu", (c::Bool) scope);
		Test_assert(t, "bindHeapCpu", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTableCpu", scope.bindDescriptorTable(tableCpu, e_rr));
		Test_assert(t, "bindPipelineCpu", scope.setComputePipeline(pipelineWrite, e_rr));
		Test_assert(t, "dispatchIndirectCpu", scope.dispatchIndirect(cpuArgs, 16, e_rr));
		Test_assert(t, "scopeCpuEnd", scope.end(e_rr));
	}

	//GPU written arguments: the writer dispatch and its consumer live in one submit, so the arguments never
	// travel through the CPU

	{
		gfx::CommandScope scope = commandList.scope({ argsWrite }, 2, {}, e_rr);
		Test_assert(t, "scopeArgs", (c::Bool) scope);
		Test_assert(t, "bindHeapArgs", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTableArgs", scope.bindDescriptorTable(tableArgs, e_rr));
		Test_assert(t, "bindPipelineArgs", scope.setComputePipeline(pipelineArgs, e_rr));
		Test_assert(t, "dispatchArgs", scope.dispatch1D(1, e_rr));
		Test_assert(t, "scopeArgsEnd", scope.end(e_rr));
	}

	{
		gfx::CommandScope scope = commandList.scope({ outputGpuWrite }, 3, {}, e_rr);
		Test_assert(t, "scopeGpu", (c::Bool) scope);
		Test_assert(t, "bindHeapGpu", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTableGpu", scope.bindDescriptorTable(tableGpu, e_rr));
		Test_assert(t, "bindPipelineGpu", scope.setComputePipeline(pipelineWrite, e_rr));
		Test_assert(t, "dispatchIndirectGpu", scope.dispatchIndirect(gpuArgs, 16, e_rr));
		Test_assert(t, "scopeGpuEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList)) {

		c::Bool okCpu = gfxtest::pullBuffer(t, dev, emptyList, outputCpu);
		c::Bool okGpu = gfxtest::pullBuffer(t, dev, emptyList, outputGpu);

		if (okCpu && okGpu) {

			const c::U32 *cpuValues = (const c::U32*) outputCpu.data()->cpuData.ptr;
			const c::U32 *gpuValues = (const c::U32*) outputGpu.data()->cpuData.ptr;

			c::Bool cpuMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				cpuMatch &= cpuValues[i] == i * 3 + 7;

			Test_assert(t, "cpuIndirectValues", cpuMatch);

			c::Bool gpuMatch = true;

			for(c::U32 i = 0; i < 128; ++i)
				gpuMatch &= gpuValues[i] == i * 3 + 7;

			Test_assert(t, "gpuIndirectValues", gpuMatch);
		}
	}
}

// -- 52. Fixed function draws on a layout with no bindings ----------------------

//Every draw capability the suite proves through the bindless module is really fixed function: depth test,
// multiple render targets, vertex and index fetch, scissor, indirect arguments and MSAA resolve touch no
// descriptor at all. Their shaders declare zero registers, so one pipeline layout with NO bindings serves
// all of them and no heap or table is ever bound, which is also a layout shape nothing else exercises.

extern "C" void Test_graphicsBindfulDrawFixed(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/drawFixed");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	if (!(dev.info().capabilities.features & c::EGraphicsFeatures_DirectRendering)) {
		c::Test_print(t, "Device lacks direct rendering, skipping fixed function draw tests");
		return;
	}

	//Slot order in the file list below, so a pipeline names its stages by index

	static const c::C8 *shaderPaths[] = {
		"//OxC3_gtest/test_shaders/test_bindful_draw_vs.oiSH",        //0: fullscreen triangle
		"//OxC3_gtest/test_shaders/test_bindful_flat_ps.oiSH",        //1: constant color, no registers
		"//OxC3_gtest/test_shaders/test_depth_vs.oiSH",               //2: three triangles at fixed depths
		"//OxC3_gtest/test_shaders/test_depth_ps.oiSH",               //3: interpolated color
		"//OxC3_gtest/test_shaders/test_vertex_vs.oiSH",              //4: vertex buffer + instancing
		"//OxC3_gtest/test_shaders/test_draw_mrt_ps.oiSH",            //5: two targets, constant colors
		"//OxC3_gtest/test_shaders/test_depth_partial_vs.oiSH"        //6: partial coverage, for the resolve leg
	};

	//One guard covers the set rather than a per file OwnedSHFile that each carry the same allocator.

	c::SHFile files[7] = {};

	struct FileGuard {

		c::SHFile (&files)[7];
		const c::Allocator *alloc;

		~FileGuard() {

			for(c::U64 i = 0; i < 7; ++i)
				c::SHFile_free(&files[i], alloc);
		}
	} fileGuard{ files, dev.alloc() };

	c::Bool loadedAll = true;

	for(c::U64 i = 0; i < 7; ++i)
		loadedAll &= gfxtest::loadFile(t, shaderPaths[i], files[i]);

	if (!loadedAll) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping fixed function draw tests");
		return;
	}

	gfx::PipelineLayout pipelineLayout;
	gfx::RenderTexture target, mrtTarget;
	gfx::DepthStencil depth, msaaDepth, resolvedDepth;
	gfx::RenderTexture msaaDepthColor;
	gfx::Pipeline msaaDepthPipeline;
	gfx::DeviceBuffer vertexBuffer, indexBuffer, drawArgs;
	gfx::Pipeline flatPipeline, depthPipeline, vertexPipeline, mrtPipeline;
	gfx::CommandList commandList, emptyList;

	//A custom pipeline layout that declares nothing: the work ops accept it without a heap or a table,
	// which is what makes every draw below independent of the binding model

	c::PipelineLayoutInfo pipelineLayoutInfo {};

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Fixed function pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "targetCreate", dev.createRenderTexture(
		8, 8, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Fixed function target", target,
		c::EMSAASamples_Off, nullptr, e_rr
	)))
		return;

	const c::PipelineGraphicsInfo flatInfo = {
		.attachmentFormatsExt = { c::ETextureFormatId_RGBA8 },
		.attachmentCountExt = 1
	};

	if(!TestBindful_graphicsPipeline(t, dev, files, 7, 0, 1, "main", "main", flatInfo, pipelineLayout, flatPipeline))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(8 * c::KIBI, 128, 32, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	//A plain fullscreen triangle first, so the later cases have a known good baseline to differ from

	Test_assert(t, "beginFlat", commandList.begin(true, e_rr));

	{
		gfxtest::DrawPass pass = TestBindful_openDraw(t, commandList, 1, target.handle(), flatPipeline);

		if (pass) {

			Test_assert(t, "drawFlat", pass.render.drawUnindexed(3, 1, e_rr));

			if(TestBindful_closeDraw(t, pass, commandList) && gfxtest::submitAndWait(t, dev, commandList))
				(void) gfxtest::checkPixels(t, dev, emptyList, target.handle(), 0xFF3366FFu);
		}
	}

	//Scissor: only the left half is drawn, the right half keeps the clear

	Test_assert(t, "beginScissor", commandList.begin(true, e_rr));

	{
		gfxtest::DrawPass pass = TestBindful_openDraw(t, commandList, 1, target.handle(), flatPipeline);

		if (pass) {

			Test_assert(t, "scissorHalf", pass.render.setScissor(c::I32x2_zero, c::I32x2_create2(4, 8), e_rr));
			Test_assert(t, "drawScissor", pass.render.drawUnindexed(3, 1, e_rr));

			if(TestBindful_closeDraw(t, pass, commandList) && gfxtest::submitAndWait(t, dev, commandList)) {

				c::TestShaderPixels pixels {};

				if (gfxtest::pullPixels(t, dev, emptyList, target.handle(), pixels)) {

					c::U32 matching = 0;

					for(c::U64 i = 0; i < 64; ++i)
						matching += pixels.pixels[i] == ((i & 7) < 4 ? 0xFF3366FFu : 0u);

					Test_assert(t, "scissorPixels", matching == 64);
				}
			}
		}
	}

	//Depth story in one draw: a far triangle writes, a nearer one passes, the farthest after it is rejected,
	// so the survivor's color and its exact depth prove both the accept and the reject path

	Test_assert(t, "depthCreate", dev.createDepthStencil(
		8, 8, c::EDepthStencilFormat_D32, false, "Fixed function depth", depth, c::EMSAASamples_Off, e_rr
	));

	c::PipelineGraphicsInfo depthInfo = flatInfo;
	depthInfo.depthStencil = { .flags = c::EDepthStencilFlags_DepthWrite, .depthCompare = c::ECompareOp_Gt };
	depthInfo.depthFormatExt = c::EDepthStencilFormat_D32;

	if (
		depth &&
		TestBindful_graphicsPipeline(t, dev, files, 7, 2, 3, "main", "main", depthInfo, pipelineLayout, depthPipeline)
	) {

		Test_assert(t, "beginDepth", commandList.begin(true, e_rr));

		const c::DepthStencilAttachmentInfo depthAttach = {
			.image = depth.handle(),
			.depthLoad = c::ELoadAttachmentType_Clear,
			.clearDepth = 0
		};

		gfx::CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "scopeDepth", (c::Bool) scope);

		gfx::CommandRender render = scope.render(
			c::I32x2_zero, c::I32x2_create2(8, 8),
			{ { .image = target.handle(), .load = c::ELoadAttachmentType_Clear } }, &depthAttach, e_rr
		);

		Test_assert(t, "renderStartDepth", (c::Bool) render);
		Test_assert(t, "viewportScissorDepth", render.setViewportAndScissor(c::I32x2_zero, c::I32x2_zero, e_rr));
		Test_assert(t, "bindDepth", render.setGraphicsPipeline(depthPipeline, e_rr));
		Test_assert(t, "drawDepth", render.drawUnindexed(9, 1, e_rr));

		if(TestBindful_closeDraw(t, scope, render, commandList) && gfxtest::submitAndWait(t, dev, commandList)) {

			(void) gfxtest::checkPixels(t, dev, emptyList, target.handle(), 0xFF00FF00u);

			c::TestShaderPixels depthPixels {};

			if (gfxtest::pullPixels(t, dev, emptyList, depth.handle(), depthPixels)) {

				c::U32 matching = 0;

				for(c::U64 i = 0; i < 64; ++i) {

					c::F32 depthValue = 0;
					c::Buffer_memcpy(
						c::Buffer_createRef(&depthValue, sizeof(depthValue)),
						c::Buffer_createRefConst(&depthPixels.pixels[i], sizeof(c::U32))
					);

					const c::F32 delta = depthValue - 0.7f;
					matching += delta > -1e-6f && delta < 1e-6f;
				}

				Test_assert(t, "depthValues", matching == 64);
			}
		}
	}

	//Indexed and instanced draw through real buffers: instance 0 covers the left half, instance 1 the right,
	// so full coverage proves the index buffer, the vertex fetch and both instances all worked

	const c::F32 quad[8] = { -1, -1, 1, -1, -1, 1, 1, 1 };
	const c::U16 quadIndices[6] = { 0, 1, 2, 2, 1, 3 };

	c::Buffer dataRef = c::Buffer_createRefConst(quad, sizeof(quad));

	Test_assert(t, "vertexBufferCreate", dev.createBufferData(
		c::EDeviceBufferUsage_Vertex, c::EGraphicsResourceFlag_None,
		"Fixed function vertices", &dataRef, vertexBuffer, nullptr, e_rr
	));

	dataRef = c::Buffer_createRefConst(quadIndices, sizeof(quadIndices));

	Test_assert(t, "indexBufferCreate", dev.createBufferData(
		c::EDeviceBufferUsage_Index, c::EGraphicsResourceFlag_None,
		"Fixed function indices", &dataRef, indexBuffer, nullptr, e_rr
	));

	c::PipelineGraphicsInfo vertexInfo = flatInfo;
	vertexInfo.vertexLayout.bufferStrides12_isInstance1[0] = sizeof(c::F32) * 2;
	vertexInfo.vertexLayout.attributes[0] = { .format = c::ETextureFormatId_RG32f };

	if (
		vertexBuffer && indexBuffer &&
		TestBindful_graphicsPipeline(t, dev, files, 7, 4, 1, "main", "main", vertexInfo, pipelineLayout, vertexPipeline)
	) {

		Test_assert(t, "beginVertex", commandList.begin(true, e_rr));

		gfxtest::DrawPass pass = TestBindful_openDraw(t, commandList, 1, target.handle(), vertexPipeline);

		if (pass) {

			c::SetPrimitiveBuffersCmd primitives {};
			primitives.vertexBuffers[0] = vertexBuffer.handle();
			primitives.indexBuffer = indexBuffer.handle();
			primitives.isIndex32Bit = false;

			Test_assert(t, "setPrimitiveBuffers", pass.render.setPrimitiveBuffers(primitives, e_rr));
			Test_assert(t, "drawIndexed", pass.render.drawIndexed(6, 2, e_rr));

			if(TestBindful_closeDraw(t, pass, commandList) && gfxtest::submitAndWait(t, dev, commandList))
				(void) gfxtest::checkPixels(t, dev, emptyList, target.handle(), 0xFF3366FFu);
		}
	}

	//Indirect draw from CPU written arguments: 3 vertices, 1 instance, so it lands the same fullscreen pixels

	const c::U32 drawArgsData[4] = { 3, 1, 0, 0 };
	dataRef = c::Buffer_createRefConst(drawArgsData, sizeof(drawArgsData));

	Test_assert(t, "drawArgsCreate", dev.createBufferData(
		c::EDeviceBufferUsage_Indirect, c::EGraphicsResourceFlag_None,
		"Fixed function draw args", &dataRef, drawArgs, nullptr, e_rr
	));

	if (drawArgs) {

		Test_assert(t, "beginIndirect", commandList.begin(true, e_rr));

		gfxtest::DrawPass pass = TestBindful_openDraw(t, commandList, 1, target.handle(), flatPipeline);

		if (pass) {

			Test_assert(t, "drawIndirect", pass.render.drawIndirect(drawArgs, 0, 1, false, e_rr));

			if(TestBindful_closeDraw(t, pass, commandList) && gfxtest::submitAndWait(t, dev, commandList))
				(void) gfxtest::checkPixels(t, dev, emptyList, target.handle(), 0xFF3366FFu);
		}
	}

	//Multiple render targets: each target gets its own constant, so a swapped or shared attachment shows up

	Test_assert(t, "mrtTargetCreate", dev.createRenderTexture(
		8, 8, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Fixed function MRT target", mrtTarget,
		c::EMSAASamples_Off, nullptr, e_rr
	));

	c::PipelineGraphicsInfo mrtInfo = {
		.attachmentFormatsExt = { c::ETextureFormatId_RGBA8, c::ETextureFormatId_RGBA8 },
		.attachmentCountExt = 2
	};

	if (
		mrtTarget &&
		TestBindful_graphicsPipeline(t, dev, files, 7, 0, 5, "main", "mainMrt", mrtInfo, pipelineLayout, mrtPipeline)
	) {

		Test_assert(t, "beginMrt", commandList.begin(true, e_rr));

		gfx::CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "scopeMrt", (c::Bool) scope);

		gfx::CommandRender render = scope.render(
			c::I32x2_zero, c::I32x2_create2(8, 8),
			{
				{ .image = target.handle(), .load = c::ELoadAttachmentType_Clear },
				{ .image = mrtTarget.handle(), .load = c::ELoadAttachmentType_Clear }
			},
			nullptr, e_rr
		);

		Test_assert(t, "renderStartMrt", (c::Bool) render);
		Test_assert(t, "viewportScissorMrt", render.setViewportAndScissor(c::I32x2_zero, c::I32x2_zero, e_rr));
		Test_assert(t, "bindMrt", render.setGraphicsPipeline(mrtPipeline, e_rr));
		Test_assert(t, "drawMrt", render.drawUnindexed(3, 1, e_rr));

		if(TestBindful_closeDraw(t, scope, render, commandList) && gfxtest::submitAndWait(t, dev, commandList)) {
			(void) gfxtest::checkPixels(t, dev, emptyList, target.handle(), 0xFF3366FFu);
			(void) gfxtest::checkPixels(t, dev, emptyList, mrtTarget.handle(), 0xFF00CC00u);
		}
	}

	//MSAA: the multisampled target resolves into the readback target, so every resolved pixel is the constant

	const c::EMSAASamples msaaCounts[3] = { c::EMSAASamples_x4, c::EMSAASamples_x2Ext, c::EMSAASamples_x8Ext };
	const c::EGraphicsDataTypes msaaTypes[3] = {
		(c::EGraphicsDataTypes) 0, c::EGraphicsDataTypes_MSAA2x, c::EGraphicsDataTypes_MSAA8x
	};

	for (c::U64 m = 0; m < 3; ++m) {

		if(msaaTypes[m] && !(dev.info().capabilities.dataTypes & msaaTypes[m]))
			continue;

		//Declared per iteration, so each count gets its own target and pipeline and the previous pair is
		//released on the way out rather than by hand.

		gfx::RenderTexture msaaTarget;
		gfx::Pipeline msaaPipeline;

		if(!Test_assert(t, "msaaTargetCreate", dev.createRenderTexture(
			8, 8, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Fixed function MSAA target",
			msaaTarget, msaaCounts[m], nullptr, e_rr
		)))
			break;

		c::PipelineGraphicsInfo msaaInfo = flatInfo;
		msaaInfo.msaa = (c::U8) msaaCounts[m];

		if(!TestBindful_graphicsPipeline(t, dev, files, 7, 0, 1, "main", "main", msaaInfo, pipelineLayout, msaaPipeline))
			break;

		Test_assert(t, "beginMsaa", commandList.begin(true, e_rr));

		gfx::CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "scopeMsaa", (c::Bool) scope);

		const c::AttachmentInfo msaaColor = {
			.image = msaaTarget.handle(),
			.load = c::ELoadAttachmentType_Clear,
			.resolveMode = c::EMSAAResolveMode_Average,
			.resolveImage = target.handle()
		};

		gfx::CommandRender render = scope.render(
			c::I32x2_zero, c::I32x2_create2(8, 8), { msaaColor }, nullptr, e_rr
		);

		Test_assert(t, "renderStartMsaa", (c::Bool) render);
		Test_assert(t, "viewportScissorMsaa", render.setViewportAndScissor(c::I32x2_zero, c::I32x2_zero, e_rr));
		Test_assert(t, "bindMsaa", render.setGraphicsPipeline(msaaPipeline, e_rr));
		Test_assert(t, "drawMsaa", render.drawUnindexed(3, 1, e_rr));

		if(TestBindful_closeDraw(t, scope, render, commandList) && gfxtest::submitAndWait(t, dev, commandList))
			(void) gfxtest::checkPixels(t, dev, emptyList, target.handle(), 0xFF3366FFu);
	}

	//A multisampled DEPTH attachment resolving into a single sample one, which nothing exercised before and
	// which turned out to be broken on both backends: Vulkan wanted a color attachment scope for the resolve
	// even on depth, and D3D12 was handing ResolveSubresource a DSV format it refuses outright.
	//
	//The geometry covers only PART of the pixels it touches (mainPartial's diagonal), which is what makes the
	// resolve MODE observable: a fullscreen triangle leaves every sample of a pixel holding the same depth, so
	// min, max and average all agree and a backend that ignores the mode passes for the wrong reason. That is
	// exactly what hid D3D12 calling ResolveSubresource, which has no mode parameter and always averages.
	//
	//The same scene is resolved three times, once per mode, and the assertions are about how the three
	// RELATE rather than about which pixels the rasterizer decided to split: a pixel is fully covered
	// (0.7 everywhere), fully uncovered (the 0 clear) or split, and only a split one can make min and max
	// disagree. Position agnostic on purpose, since sample locations and the NDC y direction are not ours
	// to pin down here.

	const c::Bool canResolveDepth = (dev.info().capabilities.dataTypes & c::EGraphicsDataTypes_MSAA2x) != 0;

	if (!canResolveDepth)
		c::Test_print(t, "Device lacks 2x MSAA, skipping the depth resolve leg");

	if (canResolveDepth) {

		c::Bool madeDepthResolve = Test_assert(t, "msaaDepthCreate", dev.createDepthStencil(
			8, 8, c::EDepthStencilFormat_D32, false, "Fixed function MSAA depth", msaaDepth,
			c::EMSAASamples_x2Ext, e_rr
		));

		madeDepthResolve &= Test_assert(t, "resolvedDepthCreate", dev.createDepthStencil(
			8, 8, c::EDepthStencilFormat_D32, true, "Fixed function resolved depth", resolvedDepth,
			c::EMSAASamples_Off, e_rr
		));

		madeDepthResolve &= Test_assert(t, "msaaDepthColorCreate", dev.createRenderTexture(
			8, 8, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Fixed function MSAA depth color",
			msaaDepthColor, c::EMSAASamples_x2Ext, nullptr, e_rr
		));

		c::PipelineGraphicsInfo msaaDepthInfo = flatInfo;
		msaaDepthInfo.depthStencil = {
			.flags = c::EDepthStencilFlags_DepthWrite, .depthCompare = c::ECompareOp_Gt
		};

		msaaDepthInfo.depthFormatExt = c::EDepthStencilFormat_D32;
		msaaDepthInfo.msaa = c::EMSAASamples_x2Ext;

		madeDepthResolve = madeDepthResolve && TestBindful_graphicsPipeline(
			t, dev, files, 7, 6, 3, "main", "main", msaaDepthInfo, pipelineLayout, msaaDepthPipeline
		);

		//Min and Max only. Average is what startRenderExt now refuses for a depth plane, since it is the zero
		// value of the enum and so the one a caller lands on by forgetting the field, and averaging depth is
		// not something an implementation has to support.

		const c::EMSAAResolveMode resolveModes[2] = { c::EMSAAResolveMode_Min, c::EMSAAResolveMode_Max };

		c::F32 resolved[2][64] = {};
		c::Bool pulledAll = madeDepthResolve;

		for (c::U64 m = 0; m < 2 && pulledAll; ++m) {

			Test_assert(t, "beginDepthResolve", commandList.begin(true, e_rr));

			const c::AttachmentInfo depthResolveColor = {
				.image = msaaDepthColor.handle(),
				.load = c::ELoadAttachmentType_Clear,
				.resolveMode = c::EMSAAResolveMode_Average,
				.resolveImage = target.handle()
			};

			const c::DepthStencilAttachmentInfo depthResolveAttach = {
				.image = msaaDepth.handle(),
				.resolveImage = resolvedDepth.handle(),
				.depthLoad = c::ELoadAttachmentType_Clear,
				//U8 in the C struct, which documents the enum it carries (see PipelineGraphicsInfo::msaa)

				.depthStencilResolve = (c::U8) resolveModes[m],
				.clearDepth = 0
			};

			gfx::CommandScope scope = commandList.scope({}, 1, {}, e_rr);
			Test_assert(t, "scopeDepthResolve", (c::Bool) scope);

			gfx::CommandRender render = scope.render(
				c::I32x2_zero, c::I32x2_create2(8, 8), { depthResolveColor }, &depthResolveAttach, e_rr
			);

			Test_assert(t, "renderStartDepthResolve", (c::Bool) render);

			Test_assert(t, "viewportScissorDepthResolve", render.setViewportAndScissor(
				c::I32x2_zero, c::I32x2_zero, e_rr
			));

			Test_assert(t, "bindDepthResolve", render.setGraphicsPipeline(msaaDepthPipeline, e_rr));
			Test_assert(t, "drawDepthResolve", render.drawUnindexed(6, 1, e_rr));

			pulledAll =
				TestBindful_closeDraw(t, scope, render, commandList) &&
				gfxtest::submitAndWait(t, dev, commandList);

			if (!pulledAll)
				break;

			c::TestShaderPixels depthPixels {};
			pulledAll = gfxtest::pullPixels(t, dev, emptyList, resolvedDepth.handle(), depthPixels);

			if (!pulledAll)
				break;

			for(c::U64 i = 0; i < 64; ++i)
				c::Buffer_memcpy(
					c::Buffer_createRef(&resolved[m][i], sizeof(c::F32)),
					c::Buffer_createRefConst(&depthPixels.pixels[i], sizeof(c::U32))
				);
		}

		if (pulledAll) {

			//D32 holds both 0 and 0.7f exactly as they went in, so nothing here needs real tolerance; eps is
			// only there so the comparisons read as float ones.

			const c::F32 eps = 1e-6f;
			const c::F32 drawn = 0.7f;

			c::U32 ordered = 0, sampleValued = 0, covered = 0, uncovered = 0, split = 0, splitCorrect = 0;

			for (c::U64 i = 0; i < 64; ++i) {

				const c::F32 minV = resolved[0][i], maxV = resolved[1][i];

				ordered += minV <= maxV + eps;

				//Both modes hand back one of the samples, never a blend of them, which is what an averaging
				// backend would produce for a split pixel (0.35) and is caught right here.

				const c::Bool minIsSample = minV < eps || (minV > drawn - eps && minV < drawn + eps);
				const c::Bool maxIsSample = maxV < eps || (maxV > drawn - eps && maxV < drawn + eps);

				sampleValued += minIsSample && maxIsSample;

				if (maxV > minV + eps) {

					++split;

					//A split pixel holds one cleared sample and one the quad wrote, so min has to be the
					// clear and max the drawn depth. Swapped modes land here with the two the other way up.

					splitCorrect += minV < eps && maxV > drawn - eps && maxV < drawn + eps;
				}

				else if(minV > drawn - eps)
					++covered;

				else if(maxV < eps)
					++uncovered;
			}

			Test_assert(t, "depthResolveOrdered", ordered == 64);
			Test_assert(t, "depthResolveSampleValued", sampleValued == 64);

			//The whole point: a backend that ignores the mode resolves both passes the same way, so nothing
			// is split and this is the assert that catches it.

			Test_assert(t, "depthResolveModesDiffer", split > 0);
			Test_assert(t, "depthResolveSplitPixels", splitCorrect == split);

			//The edge has to leave pixels on both sides of it, or the split ones above prove nothing about
			// the geometry being partially covered rather than the resolve being noise.

			Test_assert(t, "depthResolveCovered", covered > 0);
			Test_assert(t, "depthResolveUncovered", uncovered > 0);
		}
	}
}
