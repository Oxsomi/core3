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

//graphics/test/interface/test_graphics_bindful_draw.c
//
//Bindful draws: a graphics pipeline sourcing its colour through a table, indirect dispatch from CPU and
//GPU written arguments, and the fixed function suite on a layout with no bindings at all.
//Split out of test_graphics_bindful.c, which had grown to 24 modules in one file.

#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/device_info.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/sampler.h"
#include "graphics/generic/render_texture.h"
#include "graphics/generic/depth_stencil.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/commands.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/tlas.h"
#include "platforms/platform.h"
#include "formats/oiSH/sh_file.h"
#include "graphics/generic/graphics_types.h"
#include "types/test/test.h"
#include "types/base/string_base.h"
#include "types/container/list_basic_types.h"
#include "test_graphics_shared.h"

//The shaders test file keeps its own copies of these, but those pass a NULL pipeline layout, which means
//the device's default (bindless) layout; everything here brings its own layout instead.

static Bool TestBindful_graphicsPipeline(
	Test *t,
	GraphicsDeviceRef *deviceRef,
	const ListSHFile *files,
	U16 vertexFile,
	U16 pixelFile,
	const C8 *pixelEntry,
	const PipelineGraphicsInfo *info,
	PipelineLayoutRef *layout,
	PipelineRef **pipeline
) {

	const U32 vertexId = TestShaders_entry(t, deviceRef, &files->ptr[vertexFile], "main");
	const U32 pixelId = TestShaders_entry(t, deviceRef, &files->ptr[pixelFile], pixelEntry);

	if(vertexId == U32_MAX || pixelId == U32_MAX)
		return false;

	PipelineStage stages[2] = {
		(PipelineStage) { .binaryId = vertexId, .shFileId = vertexFile },
		(PipelineStage) { .binaryId = pixelId, .shFileId = pixelFile }
	};

	ListPipelineStage stageList = (ListPipelineStage) { 0 };
	ListPipelineStage_createRefConst(stages, 2, &stageList, NULL);

	const CharString name = CharString_createRefCStrConst("Bindful test graphics pipeline");

	return Test_assert(t, "createGraphicsPipeline", GraphicsDeviceRef_createPipelineGraphics(
		deviceRef, files, &stageList, info, &name, EPipelineFlags_None, layout, pipeline, &t->err
	));
}

//Opens a scope, starts a cleared render into the 8x8 target and binds the pipeline with full viewport and scissor

static Bool TestBindful_openDraw(
	Test *t, CommandListRef *commandList, U32 scopeId, RefPtr *target, PipelineRef *pipeline
) {

	Bool ok = Test_assert(t, "scope", CommandListRef_startScope(commandList, NULL, scopeId, NULL, &t->err));

	const AttachmentInfo color = (AttachmentInfo) { .image = target, .load = ELoadAttachmentType_Clear };
	ListAttachmentInfo colors = (ListAttachmentInfo) { 0 };
	ListAttachmentInfo_createRefConst(&color, 1, &colors, NULL);

	ok &= Test_assert(t, "renderStart", CommandListRef_startRenderExt(
		commandList, I32x2_zero, I32x2_create2(8, 8), &colors, NULL, &t->err
	));

	ok &= Test_assert(t, "viewportScissor", CommandListRef_setViewportAndScissor(
		commandList, I32x2_zero, I32x2_zero, &t->err
	));

	return Test_assert(t, "bindPipeline", CommandListRef_setGraphicsPipeline(commandList, pipeline, &t->err)) && ok;
}

static Bool TestBindful_closeDraw(Test *t, CommandListRef *commandList) {
	Bool ok = Test_assert(t, "renderEnd", CommandListRef_endRenderExt(commandList, &t->err));
	ok &= Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	return Test_assert(t, "end", CommandListRef_end(commandList, &t->err)) && ok;
}

// -- 44. Bindful draw: a graphics pipeline sourcing its color through a table ----

//The draw work op validates the same custom layout state the dispatches do, but through the graphics bind
// point (its own root signature slot on D3D12, its own descriptor set bind on Vulkan), which nothing else
// covers. A fullscreen triangle reads its color from a classic register and every pixel has to match it.

void Test_graphicsBindfulDraw(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/draw");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering)) {
		Test_print(t, "Device lacks direct rendering, skipping bindful draw tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *color = NULL;
	RenderTextureRef *target = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile files[2] = { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };
	ListU32 entrypoints = (ListU32) { 0 };

	if (
		!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_draw_vs.oiSH", &files[0]) ||
		!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_draw_ps.oiSH", &files[1])
	) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful draw tests");
		SHFile_free(&files[0], alloc);
		return;
	}

	ListSHFile fileList = (ListSHFile) { 0 };
	ListSHFile_createRefConst(files, 2, &fileList, NULL);

	const U32 vertexId = TestShaders_entry(t, deviceRef, &files[0], "main");
	const U32 pixelId = TestShaders_entry(t, deviceRef, &files[1], "main");

	if(vertexId == U32_MAX || pixelId == U32_MAX)
		goto clean;

	//Only the pixel shader owns a register, but the layout comes from both stages on principle

	const U32 entryIds[1] = { pixelId };

	Test_assert(t, "entrypointsRef", ListU32_createRefConst(entryIds, 1, &entrypoints, &t->err));

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntries(
		deviceRef, &files[1], &entrypoints, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	CharString name = CharString_createRefCStrConst("Bindful draw layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 1, .maxDescriptorTables = 1 };
	name = CharString_createRefCStrConst("Bindful draw heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful draw table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	//The color the pixel shader reads; exact in 8 bit UNORM so the byte compare can't flake

	const F32 colorData[4] = { 1, 102.f / 255, 51.f / 255, 1 };
	Buffer colorRef = Buffer_createRefConst(colorData, sizeof(colorData));
	name = CharString_createRefCStrConst("Bindful draw color");

	if(!Test_assert(t, "colorCreate", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL,
		&name, &colorRef, &color, &t->err
	)))
		goto clean;

	const Descriptor colorDesc = Descriptor_buffer(color, 0, 0, NULL, 0);
	const CharString colorName = CharString_createRefCStrConst("color");

	Test_assert(t, "setColor", DescriptorTableRef_setDescriptorByName(table, &colorName, 0, false, &colorDesc, &t->err));

	name = CharString_createRefCStrConst("Bindful draw target");

	if(!Test_assert(t, "targetCreate", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &name, &target, &t->err
	)))
		goto clean;

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Bindful draw pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	PipelineStage stages[2] = {
		(PipelineStage) { .binaryId = vertexId, .shFileId = 0 },
		(PipelineStage) { .binaryId = pixelId, .shFileId = 1 }
	};

	ListPipelineStage stageList = (ListPipelineStage) { 0 };
	ListPipelineStage_createRefConst(stages, 2, &stageList, NULL);

	const PipelineGraphicsInfo pipelineInfo = (PipelineGraphicsInfo) {
		.attachmentCountExt = 1,
		.attachmentFormatsExt = { ETextureFormatId_RGBA8 }
	};

	name = CharString_createRefCStrConst("Bindful draw pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineGraphics(
		deviceRef, &fileList, &stageList, &pipelineInfo, &name, EPipelineFlags_None,
		pipelineLayout, &pipeline, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "listCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, 2 * KIBI, 32, 16, true, &commandList, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "emptyListCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, KIBI, 16, 8, true, &emptyList, &t->err
	)))
		goto clean;

	Test_assert(t, "beginEmpty", CommandListRef_begin(emptyList, true, U64_MAX, &t->err));
	Test_assert(t, "endEmpty", CommandListRef_end(emptyList, &t->err));

	const Transition transition = (Transition) {
		.resource = color, .stage = EPipelineStage_Pixel
	};

	ListTransition transitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(&transition, 1, &transitionList, NULL);

	const AttachmentInfo attachment = (AttachmentInfo) { .image = target, .load = ELoadAttachmentType_Clear };
	ListAttachmentInfo colors = (ListAttachmentInfo) { 0 };
	ListAttachmentInfo_createRefConst(&attachment, 1, &colors, NULL);

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 1, NULL, &t->err));
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

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
		TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF3366FFu);

clean:

	if(table)
		DescriptorTableRef_unsetDescriptors(table, 0, 0, 1, NULL);

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&target);
	RefPtr_dec(&color);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&files[0], alloc);
	SHFile_free(&files[1], alloc);
}

// -- 51. Bindful indirect dispatch: CPU written and GPU written arguments --------

//Indirect execution was proven only through bindless, so a device without it had none. Two cases here:
// arguments uploaded by the CPU and read at a nonzero offset, and arguments a dispatch writes on the GPU
// that the next scope consumes in the same submit.
//The consumer is the same write shader module 41 uses, so the values (i * 3 + 7) prove which threads ran:
// one group means 64 slots, the GPU written { 2, 1, 1 } means 128.

void Test_graphicsBindfulIndirect(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/indirect");

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layoutWrite = NULL;
	DescriptorLayoutRef *layoutArgs = NULL;
	DescriptorTableRef *tableCpu = NULL;
	DescriptorTableRef *tableGpu = NULL;
	DescriptorTableRef *tableArgs = NULL;
	PipelineLayoutRef *pipelineLayoutWrite = NULL;
	PipelineLayoutRef *pipelineLayoutArgs = NULL;
	PipelineRef *pipelineWrite = NULL;
	PipelineRef *pipelineArgs = NULL;
	DeviceBufferRef *outputCpu = NULL;
	DeviceBufferRef *outputGpu = NULL;
	DeviceBufferRef *cpuArgs = NULL;
	DeviceBufferRef *gpuArgs = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile writeFile = (SHFile) { 0 };
	SHFile argsFile = (SHFile) { 0 };
	DescriptorLayoutInfo layoutWriteInfo = (DescriptorLayoutInfo) { 0 };
	DescriptorLayoutInfo layoutArgsInfo = (DescriptorLayoutInfo) { 0 };

	if (
		!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_write.oiSH", &writeFile) ||
		!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_write_args.oiSH", &argsFile)
	) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful indirect tests");
		SHFile_free(&writeFile, alloc);
		return;
	}

	const U32 writeId = TestShaders_entry(t, deviceRef, &writeFile, "main");
	const U32 argsId = TestShaders_entry(t, deviceRef, &argsFile, "main");

	if(writeId == U32_MAX || argsId == U32_MAX)
		goto clean;

	if(!Test_assert(t, "detectLayoutWrite", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &writeFile, writeId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutWriteInfo, NULL, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "detectLayoutArgs", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &argsFile, argsId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutArgsInfo, NULL, &t->err
	)))
		goto clean;

	CharString name = CharString_createRefCStrConst("Bindful indirect write layout");

	if(!Test_assert(t, "layoutWriteCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutWriteInfo, &name, &layoutWrite, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful indirect args layout");

	if(!Test_assert(t, "layoutArgsCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutArgsInfo, &name, &layoutArgs, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 3, .maxDescriptorTables = 3 };
	name = CharString_createRefCStrConst("Bindful indirect heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful indirect CPU args table");

	if(!Test_assert(t, "tableCpuCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layoutWrite, EDescriptorTableFlags_None, &name, &tableCpu, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful indirect GPU args table");

	if(!Test_assert(t, "tableGpuCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layoutWrite, EDescriptorTableFlags_None, &name, &tableGpu, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful indirect args writer table");

	if(!Test_assert(t, "tableArgsCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layoutArgs, EDescriptorTableFlags_None, &name, &tableArgs, &t->err
	)))
		goto clean;

	//Separate outputs per case, so a case that silently did nothing can't pass on the other's values

	name = CharString_createRefCStrConst("Bindful indirect CPU output");

	if(!Test_assert(t, "outputCpuCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &outputCpu, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful indirect GPU output");

	if(!Test_assert(t, "outputGpuCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 128 * sizeof(U32), &outputGpu, &t->err
	)))
		goto clean;

	//The dispatch arguments sit at byte 16, so the read also proves offset addressing into the buffer

	const U32 cpuArgsData[8] = { 0, 0, 0, 0, 1, 1, 1, 0 };
	Buffer cpuArgsRef = Buffer_createRefConst(cpuArgsData, sizeof(cpuArgsData));
	name = CharString_createRefCStrConst("Bindful indirect CPU args");

	if(!Test_assert(t, "cpuArgsCreate", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_Indirect, EGraphicsResourceFlag_None, NULL,
		&name, &cpuArgsRef, &cpuArgs, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful indirect GPU args");

	if(!Test_assert(t, "gpuArgsCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_Indirect, EGraphicsResourceFlag_ShaderWrite,
		NULL, &name, 32, &gpuArgs, &t->err
	)))
		goto clean;

	const Descriptor outputCpuDesc = Descriptor_buffer(outputCpu, 0, 0, NULL, 0);
	const Descriptor outputGpuDesc = Descriptor_buffer(outputGpu, 0, 0, NULL, 0);
	const Descriptor gpuArgsDesc = Descriptor_buffer(gpuArgs, 0, 0, NULL, 0);

	const CharString outputName = CharString_createRefCStrConst("output");
	const CharString argsName = CharString_createRefCStrConst("args");

	Test_assert(t, "setOutputCpu", DescriptorTableRef_setDescriptorByName(
		tableCpu, &outputName, 0, false, &outputCpuDesc, &t->err
	));

	Test_assert(t, "setOutputGpu", DescriptorTableRef_setDescriptorByName(
		tableGpu, &outputName, 0, false, &outputGpuDesc, &t->err
	));

	Test_assert(t, "setGpuArgs", DescriptorTableRef_setDescriptorByName(
		tableArgs, &argsName, 0, false, &gpuArgsDesc, &t->err
	));

	PipelineLayoutInfo pipelineLayoutWriteInfo = (PipelineLayoutInfo) { .bindings = layoutWrite };
	name = CharString_createRefCStrConst("Bindful indirect write pipeline layout");

	if(!Test_assert(t, "pipelineLayoutWriteCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutWriteInfo, &name, &pipelineLayoutWrite, &t->err
	)))
		goto clean;

	PipelineLayoutInfo pipelineLayoutArgsInfo = (PipelineLayoutInfo) { .bindings = layoutArgs };
	name = CharString_createRefCStrConst("Bindful indirect args pipeline layout");

	if(!Test_assert(t, "pipelineLayoutArgsCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutArgsInfo, &name, &pipelineLayoutArgs, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful indirect write pipeline");

	if(!Test_assert(t, "pipelineWriteCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &writeFile, &name, writeId, NULL, EPipelineFlags_None, pipelineLayoutWrite, &pipelineWrite, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful indirect args pipeline");

	if(!Test_assert(t, "pipelineArgsCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &argsFile, &name, argsId, NULL, EPipelineFlags_None, pipelineLayoutArgs, &pipelineArgs, &t->err
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

	//The indirect buffer itself needs no transition; only a buffer a shader writes does

	const Transition outputCpuWrite = (Transition) {
		.resource = outputCpu, .stage = EPipelineStage_Compute, .isWrite = true
	};

	ListTransition outputCpuTransition = (ListTransition) { 0 };
	ListTransition_createRefConst(&outputCpuWrite, 1, &outputCpuTransition, NULL);

	const Transition argsWrite = (Transition) {
		.resource = gpuArgs, .stage = EPipelineStage_Compute, .isWrite = true
	};

	ListTransition argsTransition = (ListTransition) { 0 };
	ListTransition_createRefConst(&argsWrite, 1, &argsTransition, NULL);

	const Transition outputGpuWrite = (Transition) {
		.resource = outputGpu, .stage = EPipelineStage_Compute, .isWrite = true
	};

	ListTransition outputGpuTransition = (ListTransition) { 0 };
	ListTransition_createRefConst(&outputGpuWrite, 1, &outputGpuTransition, NULL);

	//CPU written arguments: one group, so exactly the first 64 slots come back written

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "scopeCpu", CommandListRef_startScope(commandList, &outputCpuTransition, 1, NULL, &t->err));
	Test_assert(t, "bindHeapCpu", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTableCpu", CommandListRef_bindDescriptorTable(commandList, tableCpu, &t->err));
	Test_assert(t, "bindPipelineCpu", CommandListRef_setComputePipeline(commandList, pipelineWrite, &t->err));
	Test_assert(t, "dispatchIndirectCpu", CommandListRef_dispatchIndirect(commandList, cpuArgs, 16, &t->err));
	Test_assert(t, "scopeCpuEnd", CommandListRef_endScope(commandList, &t->err));

	//GPU written arguments: the writer dispatch and its consumer live in one submit, so the arguments never
	// travel through the CPU

	Test_assert(t, "scopeArgs", CommandListRef_startScope(commandList, &argsTransition, 2, NULL, &t->err));
	Test_assert(t, "bindHeapArgs", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTableArgs", CommandListRef_bindDescriptorTable(commandList, tableArgs, &t->err));
	Test_assert(t, "bindPipelineArgs", CommandListRef_setComputePipeline(commandList, pipelineArgs, &t->err));
	Test_assert(t, "dispatchArgs", CommandListRef_dispatch1D(commandList, 1, &t->err));
	Test_assert(t, "scopeArgsEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "scopeGpu", CommandListRef_startScope(commandList, &outputGpuTransition, 3, NULL, &t->err));
	Test_assert(t, "bindHeapGpu", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTableGpu", CommandListRef_bindDescriptorTable(commandList, tableGpu, &t->err));
	Test_assert(t, "bindPipelineGpu", CommandListRef_setComputePipeline(commandList, pipelineWrite, &t->err));
	Test_assert(t, "dispatchIndirectGpu", CommandListRef_dispatchIndirect(commandList, gpuArgs, 16, &t->err));
	Test_assert(t, "scopeGpuEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0)) {

		Bool okCpu = TestShaders_pullBuffer(t, deviceRef, emptyList, outputCpu);
		Bool okGpu = TestShaders_pullBuffer(t, deviceRef, emptyList, outputGpu);

		if (okCpu && okGpu) {

			const U32 *cpuValues = (const U32*) DeviceBufferRef_ptr(outputCpu)->cpuData.ptr;
			const U32 *gpuValues = (const U32*) DeviceBufferRef_ptr(outputGpu)->cpuData.ptr;

			Bool cpuMatch = true;

			for(U32 i = 0; i < 64; ++i)
				cpuMatch &= cpuValues[i] == i * 3 + 7;

			Test_assert(t, "cpuIndirectValues", cpuMatch);

			Bool gpuMatch = true;

			for(U32 i = 0; i < 128; ++i)
				gpuMatch &= gpuValues[i] == i * 3 + 7;

			Test_assert(t, "gpuIndirectValues", gpuMatch);
		}
	}

clean:

	if(tableCpu)
		DescriptorTableRef_unsetDescriptors(tableCpu, 0, 0, 1, NULL);

	if(tableGpu)
		DescriptorTableRef_unsetDescriptors(tableGpu, 0, 0, 1, NULL);

	if(tableArgs)
		DescriptorTableRef_unsetDescriptors(tableArgs, 0, 0, 1, NULL);

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipelineArgs);
	RefPtr_dec(&pipelineWrite);
	RefPtr_dec(&pipelineLayoutArgs);
	RefPtr_dec(&pipelineLayoutWrite);
	RefPtr_dec(&gpuArgs);
	RefPtr_dec(&cpuArgs);
	RefPtr_dec(&outputGpu);
	RefPtr_dec(&outputCpu);
	RefPtr_dec(&tableArgs);
	RefPtr_dec(&tableGpu);
	RefPtr_dec(&tableCpu);
	RefPtr_dec(&layoutArgs);
	RefPtr_dec(&layoutWrite);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutWriteInfo, alloc);
	DescriptorLayoutInfo_free(&layoutArgsInfo, alloc);
	SHFile_free(&writeFile, alloc);
	SHFile_free(&argsFile, alloc);
}

// -- 52. Fixed function draws on a layout with no bindings ----------------------

//Every draw capability the suite proves through the bindless module is really fixed function: depth test,
// multiple render targets, vertex and index fetch, scissor, indirect arguments and MSAA resolve touch no
// descriptor at all. Their shaders declare zero registers, so one pipeline layout with NO bindings serves
// all of them and no heap or table is ever bound, which is also a layout shape nothing else exercises.

void Test_graphicsBindfulDrawFixed(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/drawFixed");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering)) {
		Test_print(t, "Device lacks direct rendering, skipping fixed function draw tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	//Slot order in the file list below, so a pipeline names its stages by index

	static const C8 *shaderPaths[] = {
		"//OxC3_gtest/test_shaders/test_bindful_draw_vs.oiSH",        //0: fullscreen triangle
		"//OxC3_gtest/test_shaders/test_bindful_flat_ps.oiSH",        //1: constant color, no registers
		"//OxC3_gtest/test_shaders/test_depth_vs.oiSH",               //2: three triangles at fixed depths
		"//OxC3_gtest/test_shaders/test_depth_ps.oiSH",               //3: interpolated color
		"//OxC3_gtest/test_shaders/test_vertex_vs.oiSH",              //4: vertex buffer + instancing
		"//OxC3_gtest/test_shaders/test_draw_mrt_ps.oiSH"             //5: two targets, constant colors
	};

	SHFile files[6] = { 0 };
	Bool loadedAll = true;

	for(U64 i = 0; i < 6; ++i)
		loadedAll &= TestShaders_loadFile(t, shaderPaths[i], &files[i]);

	if (!loadedAll) {

		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping fixed function draw tests");

		for(U64 i = 0; i < 6; ++i)
			SHFile_free(&files[i], alloc);

		return;
	}

	ListSHFile fileList = (ListSHFile) { 0 };
	ListSHFile_createRefConst(files, 6, &fileList, NULL);

	PipelineLayoutRef *pipelineLayout = NULL;
	RenderTextureRef *target = NULL;
	RenderTextureRef *mrtTarget = NULL;
	RenderTextureRef *msaaTarget = NULL;
	DepthStencilRef *depth = NULL;
	DepthStencilRef *msaaDepth = NULL;
	DepthStencilRef *resolvedDepth = NULL;
	RenderTextureRef *msaaDepthColor = NULL;
	PipelineRef *msaaDepthPipeline = NULL;
	DeviceBufferRef *vertexBuffer = NULL;
	DeviceBufferRef *indexBuffer = NULL;
	DeviceBufferRef *drawArgs = NULL;
	PipelineRef *flatPipeline = NULL;
	PipelineRef *depthPipeline = NULL;
	PipelineRef *vertexPipeline = NULL;
	PipelineRef *mrtPipeline = NULL;
	PipelineRef *msaaPipeline = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	//A custom pipeline layout that declares nothing: the work ops accept it without a heap or a table,
	// which is what makes every draw below independent of the binding model

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { 0 };
	CharString name = CharString_createRefCStrConst("Fixed function pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Fixed function target");

	if(!Test_assert(t, "targetCreate", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &name, &target, &t->err
	)))
		goto clean;

	const PipelineGraphicsInfo flatInfo = (PipelineGraphicsInfo) {
		.attachmentCountExt = 1,
		.attachmentFormatsExt = { ETextureFormatId_RGBA8 }
	};

	if(!TestBindful_graphicsPipeline(t, deviceRef, &fileList, 0, 1, "main", &flatInfo, pipelineLayout, &flatPipeline))
		goto clean;

	if(!Test_assert(t, "listCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, 8 * KIBI, 128, 32, true, &commandList, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "emptyListCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, KIBI, 16, 8, true, &emptyList, &t->err
	)))
		goto clean;

	Test_assert(t, "beginEmpty", CommandListRef_begin(emptyList, true, U64_MAX, &t->err));
	Test_assert(t, "endEmpty", CommandListRef_end(emptyList, &t->err));

	//A plain fullscreen triangle first, so the later cases have a known good baseline to differ from

	Test_assert(t, "beginFlat", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

	if (TestBindful_openDraw(t, commandList, 1, target, flatPipeline)) {

		Test_assert(t, "drawFlat", CommandListRef_drawUnindexed(commandList, 3, 1, &t->err));

		if(TestBindful_closeDraw(t, commandList) && TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
			TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF3366FFu);
	}

	//Scissor: only the left half is drawn, the right half keeps the clear

	Test_assert(t, "beginScissor", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

	if (TestBindful_openDraw(t, commandList, 1, target, flatPipeline)) {

		Test_assert(t, "scissorHalf", CommandListRef_setScissor(
			commandList, I32x2_zero, I32x2_create2(4, 8), &t->err
		));

		Test_assert(t, "drawScissor", CommandListRef_drawUnindexed(commandList, 3, 1, &t->err));

		if(TestBindful_closeDraw(t, commandList) && TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0)) {

			TestShaderPixels pixels = (TestShaderPixels) { 0 };

			if (TestShaders_pullPixels(t, deviceRef, emptyList, target, &pixels)) {

				U32 matching = 0;

				for(U64 i = 0; i < 64; ++i)
					matching += pixels.pixels[i] == ((i & 7) < 4 ? 0xFF3366FFu : 0u);

				Test_assert(t, "scissorPixels", matching == 64);
			}
		}
	}

	//Depth story in one draw: a far triangle writes, a nearer one passes, the farthest after it is rejected,
	// so the survivor's color and its exact depth prove both the accept and the reject path

	name = CharString_createRefCStrConst("Fixed function depth");

	Test_assert(t, "depthCreate", GraphicsDeviceRef_createDepthStencil(
		deviceRef, 8, 8, EDepthStencilFormat_D32, false, EMSAASamples_Off, NULL, &name, &depth, &t->err
	));

	PipelineGraphicsInfo depthInfo = flatInfo;
	depthInfo.depthStencil = (DepthStencilState) { .flags = EDepthStencilFlags_DepthWrite, .depthCompare = ECompareOp_Gt };
	depthInfo.depthFormatExt = EDepthStencilFormat_D32;

	if (
		depth &&
		TestBindful_graphicsPipeline(t, deviceRef, &fileList, 2, 3, "main", &depthInfo, pipelineLayout, &depthPipeline)
	) {

		Test_assert(t, "beginDepth", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
		Test_assert(t, "scopeDepth", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));

		const AttachmentInfo color = (AttachmentInfo) { .image = target, .load = ELoadAttachmentType_Clear };
		ListAttachmentInfo colors = (ListAttachmentInfo) { 0 };
		ListAttachmentInfo_createRefConst(&color, 1, &colors, NULL);

		const DepthStencilAttachmentInfo depthAttach = (DepthStencilAttachmentInfo) {
			.image = depth,
			.depthLoad = ELoadAttachmentType_Clear,
			.clearDepth = 0
		};

		Test_assert(t, "renderStartDepth", CommandListRef_startRenderExt(
			commandList, I32x2_zero, I32x2_create2(8, 8), &colors, &depthAttach, &t->err
		));

		Test_assert(t, "viewportScissorDepth", CommandListRef_setViewportAndScissor(
			commandList, I32x2_zero, I32x2_zero, &t->err
		));

		Test_assert(t, "bindDepth", CommandListRef_setGraphicsPipeline(commandList, depthPipeline, &t->err));
		Test_assert(t, "drawDepth", CommandListRef_drawUnindexed(commandList, 9, 1, &t->err));

		if(TestBindful_closeDraw(t, commandList) && TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0)) {

			TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF00FF00u);

			TestShaderPixels depthPixels = (TestShaderPixels) { 0 };

			if (TestShaders_pullPixels(t, deviceRef, emptyList, depth, &depthPixels)) {

				U32 matching = 0;

				for(U64 i = 0; i < 64; ++i) {

					F32 depthValue = 0;
					Buffer_memcpy(
						Buffer_createRef(&depthValue, sizeof(depthValue)),
						Buffer_createRefConst(&depthPixels.pixels[i], sizeof(U32))
					);

					const F32 delta = depthValue - 0.7f;
					matching += delta > -1e-6f && delta < 1e-6f;
				}

				Test_assert(t, "depthValues", matching == 64);
			}
		}
	}

	//Indexed and instanced draw through real buffers: instance 0 covers the left half, instance 1 the right,
	// so full coverage proves the index buffer, the vertex fetch and both instances all worked

	const F32 quad[8] = { -1, -1, 1, -1, -1, 1, 1, 1 };
	const U16 quadIndices[6] = { 0, 1, 2, 2, 1, 3 };

	Buffer dataRef = Buffer_createRefConst(quad, sizeof(quad));
	name = CharString_createRefCStrConst("Fixed function vertices");

	Test_assert(t, "vertexBufferCreate", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_Vertex, EGraphicsResourceFlag_None, NULL,
		&name, &dataRef, &vertexBuffer, &t->err
	));

	dataRef = Buffer_createRefConst(quadIndices, sizeof(quadIndices));
	name = CharString_createRefCStrConst("Fixed function indices");

	Test_assert(t, "indexBufferCreate", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_Index, EGraphicsResourceFlag_None, NULL,
		&name, &dataRef, &indexBuffer, &t->err
	));

	PipelineGraphicsInfo vertexInfo = flatInfo;
	vertexInfo.vertexLayout.bufferStrides12_isInstance1[0] = sizeof(F32) * 2;
	vertexInfo.vertexLayout.attributes[0] = (VertexAttribute) { .format = ETextureFormatId_RG32f };

	if (
		vertexBuffer && indexBuffer &&
		TestBindful_graphicsPipeline(t, deviceRef, &fileList, 4, 1, "main", &vertexInfo, pipelineLayout, &vertexPipeline)
	) {

		Test_assert(t, "beginVertex", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		if (TestBindful_openDraw(t, commandList, 1, target, vertexPipeline)) {

			SetPrimitiveBuffersCmd primitives = (SetPrimitiveBuffersCmd) { 0 };
			primitives.vertexBuffers[0] = vertexBuffer;
			primitives.indexBuffer = indexBuffer;
			primitives.isIndex32Bit = false;

			Test_assert(t, "setPrimitiveBuffers", CommandListRef_setPrimitiveBuffers(commandList, &primitives, &t->err));
			Test_assert(t, "drawIndexed", CommandListRef_drawIndexed(commandList, 6, 2, &t->err));

			if(TestBindful_closeDraw(t, commandList) && TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
				TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF3366FFu);
		}
	}

	//Indirect draw from CPU written arguments: 3 vertices, 1 instance, so it lands the same fullscreen pixels

	const U32 drawArgsData[4] = { 3, 1, 0, 0 };
	dataRef = Buffer_createRefConst(drawArgsData, sizeof(drawArgsData));
	name = CharString_createRefCStrConst("Fixed function draw args");

	Test_assert(t, "drawArgsCreate", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_Indirect, EGraphicsResourceFlag_None, NULL,
		&name, &dataRef, &drawArgs, &t->err
	));

	if (drawArgs) {

		Test_assert(t, "beginIndirect", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		if (TestBindful_openDraw(t, commandList, 1, target, flatPipeline)) {

			Test_assert(t, "drawIndirect", CommandListRef_drawIndirect(
				commandList, drawArgs, 0, 1, false, &t->err
			));

			if(TestBindful_closeDraw(t, commandList) && TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
				TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF3366FFu);
		}
	}

	//Multiple render targets: each target gets its own constant, so a swapped or shared attachment shows up

	name = CharString_createRefCStrConst("Fixed function MRT target");

	Test_assert(t, "mrtTargetCreate", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &name, &mrtTarget, &t->err
	));

	PipelineGraphicsInfo mrtInfo = (PipelineGraphicsInfo) {
		.attachmentCountExt = 2,
		.attachmentFormatsExt = { ETextureFormatId_RGBA8, ETextureFormatId_RGBA8 }
	};

	if (
		mrtTarget &&
		TestBindful_graphicsPipeline(t, deviceRef, &fileList, 0, 5, "mainMrt", &mrtInfo, pipelineLayout, &mrtPipeline)
	) {

		Test_assert(t, "beginMrt", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
		Test_assert(t, "scopeMrt", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));

		const AttachmentInfo mrtColors[2] = {
			(AttachmentInfo) { .image = target, .load = ELoadAttachmentType_Clear },
			(AttachmentInfo) { .image = mrtTarget, .load = ELoadAttachmentType_Clear }
		};

		ListAttachmentInfo colors = (ListAttachmentInfo) { 0 };
		ListAttachmentInfo_createRefConst(mrtColors, 2, &colors, NULL);

		Test_assert(t, "renderStartMrt", CommandListRef_startRenderExt(
			commandList, I32x2_zero, I32x2_create2(8, 8), &colors, NULL, &t->err
		));

		Test_assert(t, "viewportScissorMrt", CommandListRef_setViewportAndScissor(
			commandList, I32x2_zero, I32x2_zero, &t->err
		));

		Test_assert(t, "bindMrt", CommandListRef_setGraphicsPipeline(commandList, mrtPipeline, &t->err));
		Test_assert(t, "drawMrt", CommandListRef_drawUnindexed(commandList, 3, 1, &t->err));

		if(TestBindful_closeDraw(t, commandList) && TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0)) {
			TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF3366FFu);
			TestShaders_checkPixels(t, deviceRef, emptyList, mrtTarget, 0xFF00CC00u);
		}
	}

	//MSAA: the multisampled target resolves into the readback target, so every resolved pixel is the constant

	const EMSAASamples msaaCounts[3] = { EMSAASamples_x4, EMSAASamples_x2Ext, EMSAASamples_x8Ext };
	const EGraphicsDataTypes msaaTypes[3] = {
		(EGraphicsDataTypes) 0, EGraphicsDataTypes_MSAA2x, EGraphicsDataTypes_MSAA8x
	};

	for (U64 m = 0; m < 3; ++m) {

		if(msaaTypes[m] && !(device->info.capabilities.dataTypes & msaaTypes[m]))
			continue;

		name = CharString_createRefCStrConst("Fixed function MSAA target");

		if(!Test_assert(t, "msaaTargetCreate", GraphicsDeviceRef_createRenderTexture(
			deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
			msaaCounts[m], NULL, &name, &msaaTarget, &t->err
		)))
			break;

		PipelineGraphicsInfo msaaInfo = flatInfo;
		msaaInfo.msaa = msaaCounts[m];

		if(!TestBindful_graphicsPipeline(
			t, deviceRef, &fileList, 0, 1, "main", &msaaInfo, pipelineLayout, &msaaPipeline
		))
			break;

		Test_assert(t, "beginMsaa", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
		Test_assert(t, "scopeMsaa", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));

		const AttachmentInfo msaaColor = (AttachmentInfo) {
			.image = msaaTarget,
			.load = ELoadAttachmentType_Clear,
			.resolveMode = EMSAAResolveMode_Average,
			.resolveImage = target
		};

		ListAttachmentInfo colors = (ListAttachmentInfo) { 0 };
		ListAttachmentInfo_createRefConst(&msaaColor, 1, &colors, NULL);

		Test_assert(t, "renderStartMsaa", CommandListRef_startRenderExt(
			commandList, I32x2_zero, I32x2_create2(8, 8), &colors, NULL, &t->err
		));

		Test_assert(t, "viewportScissorMsaa", CommandListRef_setViewportAndScissor(
			commandList, I32x2_zero, I32x2_zero, &t->err
		));

		Test_assert(t, "bindMsaa", CommandListRef_setGraphicsPipeline(commandList, msaaPipeline, &t->err));
		Test_assert(t, "drawMsaa", CommandListRef_drawUnindexed(commandList, 3, 1, &t->err));

		if(TestBindful_closeDraw(t, commandList) && TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
			TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF3366FFu);

		RefPtr_dec(&msaaPipeline);
		RefPtr_dec(&msaaTarget);
	}

	//A multisampled DEPTH attachment resolving into a single sample one, which nothing exercised before and
	// which turned out to be broken on both backends: Vulkan wanted a colour attachment scope for the resolve
	// even on depth, and D3D12 was handing ResolveSubresource a DSV format it refuses outright.
	//Min rather than average, since averaging depth isn't universally supported.

	const Bool canResolveDepth = (device->info.capabilities.dataTypes & EGraphicsDataTypes_MSAA2x) != 0;

	if (!canResolveDepth)
		Test_print(t, "Device lacks 2x MSAA, skipping the depth resolve leg");

	if (canResolveDepth) {

		name = CharString_createRefCStrConst("Fixed function MSAA depth");

		Bool madeDepthResolve = Test_assert(t, "msaaDepthCreate", GraphicsDeviceRef_createDepthStencil(
			deviceRef, 8, 8, EDepthStencilFormat_D32, false, EMSAASamples_x2Ext, NULL, &name, &msaaDepth, &t->err
		));

		name = CharString_createRefCStrConst("Fixed function resolved depth");

		madeDepthResolve &= Test_assert(t, "resolvedDepthCreate", GraphicsDeviceRef_createDepthStencil(
			deviceRef, 8, 8, EDepthStencilFormat_D32, true, EMSAASamples_Off, NULL, &name, &resolvedDepth, &t->err
		));

		name = CharString_createRefCStrConst("Fixed function MSAA depth colour");

		madeDepthResolve &= Test_assert(t, "msaaDepthColorCreate", GraphicsDeviceRef_createRenderTexture(
			deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
			EMSAASamples_x2Ext, NULL, &name, &msaaDepthColor, &t->err
		));

		PipelineGraphicsInfo msaaDepthInfo = flatInfo;
		msaaDepthInfo.depthStencil = (DepthStencilState) {
			.flags = EDepthStencilFlags_DepthWrite, .depthCompare = ECompareOp_Gt
		};

		msaaDepthInfo.depthFormatExt = EDepthStencilFormat_D32;
		msaaDepthInfo.msaa = EMSAASamples_x2Ext;

		madeDepthResolve = madeDepthResolve && TestBindful_graphicsPipeline(
			t, deviceRef, &fileList, 2, 3, "main", &msaaDepthInfo, pipelineLayout, &msaaDepthPipeline
		);

		if (madeDepthResolve) {

			Test_assert(t, "beginDepthResolve", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
			Test_assert(t, "scopeDepthResolve", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));

			const AttachmentInfo depthResolveColor = (AttachmentInfo) {
				.image = msaaDepthColor,
				.load = ELoadAttachmentType_Clear,
				.resolveMode = EMSAAResolveMode_Average,
				.resolveImage = target
			};

			ListAttachmentInfo depthResolveColors = (ListAttachmentInfo) { 0 };
			ListAttachmentInfo_createRefConst(&depthResolveColor, 1, &depthResolveColors, NULL);

			const DepthStencilAttachmentInfo depthResolveAttach = (DepthStencilAttachmentInfo) {
				.image = msaaDepth,
				.depthLoad = ELoadAttachmentType_Clear,
				.clearDepth = 0,
				.resolveImage = resolvedDepth,
				.depthStencilResolve = EMSAAResolveMode_Min
			};

			Test_assert(t, "renderStartDepthResolve", CommandListRef_startRenderExt(
				commandList, I32x2_zero, I32x2_create2(8, 8), &depthResolveColors, &depthResolveAttach, &t->err
			));

			Test_assert(t, "viewportScissorDepthResolve", CommandListRef_setViewportAndScissor(
				commandList, I32x2_zero, I32x2_zero, &t->err
			));

			Test_assert(t, "bindDepthResolve", CommandListRef_setGraphicsPipeline(commandList, msaaDepthPipeline, &t->err));
			Test_assert(t, "drawDepthResolve", CommandListRef_drawUnindexed(commandList, 9, 1, &t->err));

			if(TestBindful_closeDraw(t, commandList) && TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0)) {

				//The surviving triangle's depth, resolved out of the multisampled buffer

				TestShaderPixels depthPixels = (TestShaderPixels) { 0 };

				if (TestShaders_pullPixels(t, deviceRef, emptyList, resolvedDepth, &depthPixels)) {

					U32 matching = 0;

					for(U64 i = 0; i < 64; ++i) {

						F32 depthValue = 0;
						Buffer_memcpy(
							Buffer_createRef(&depthValue, sizeof(depthValue)),
							Buffer_createRefConst(&depthPixels.pixels[i], sizeof(U32))
						);

						const F32 delta = depthValue - 0.7f;
						matching += delta > -1e-6f && delta < 1e-6f;
					}

					Test_assert(t, "depthResolveValues", matching == 64);
				}
			}
		}
	}

clean:

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&msaaPipeline);
	RefPtr_dec(&mrtPipeline);
	RefPtr_dec(&vertexPipeline);
	RefPtr_dec(&depthPipeline);
	RefPtr_dec(&flatPipeline);
	RefPtr_dec(&drawArgs);
	RefPtr_dec(&indexBuffer);
	RefPtr_dec(&vertexBuffer);
	RefPtr_dec(&depth);
	RefPtr_dec(&msaaDepthPipeline);
	RefPtr_dec(&msaaDepthColor);
	RefPtr_dec(&resolvedDepth);
	RefPtr_dec(&msaaDepth);
	RefPtr_dec(&msaaTarget);
	RefPtr_dec(&mrtTarget);
	RefPtr_dec(&target);
	RefPtr_dec(&pipelineLayout);

	for(U64 i = 0; i < 6; ++i)
		SHFile_free(&files[i], alloc);
}
