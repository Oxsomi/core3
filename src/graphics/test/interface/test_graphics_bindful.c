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

//graphics/test/interface/test_graphics_bindful.c

//The bindful path (module 41): pipelines with their own layout, tables and heaps bound at record time.
//Deliberately free of any bindless requirement, so it must pass on devices without bindless at all;
// the config module also runs it on a DisableBindless device to prove exactly that.

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

// -- 41. Bindful: descriptor table bound at record time ---------------------------

//Everything else in the suite reaches resources through the device's default bindless table; this is the
// first path where a pipeline brings its OWN layout and the heap and table are bound with commands.
//The layout is auto detected from the shader's reflection and the descriptor set by register name, so the
// test never restates what the shader already declares.
//The shader has a single classic register (u0), so the whole flow works on a device without bindless.

void Test_graphicsBindful(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful");

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *buffer = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_write.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful tests");
		return;
	}

	const U32 entryId = TestShaders_entry(t, deviceRef, &file, "main");

	if(entryId == U32_MAX)
		goto clean;

	//The reflection knows the registers, so the layout comes from the shader rather than being restated

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &file, entryId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	CharString name = CharString_createRefCStrConst("Bindful layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 1, .maxDescriptorTables = 1 };
	name = CharString_createRefCStrConst("Bindful heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	//64 threads each write id * 3 + 7 as a U32, so the pull can prove the dispatch really ran bindful

	name = CharString_createRefCStrConst("Bindful output buffer");

	if(!Test_assert(t, "bufferCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &buffer, &t->err
	)))
		goto clean;

	const Descriptor desc = Descriptor_buffer(buffer, 0, 0, NULL, 0);
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setDescriptor", DescriptorTableRef_setDescriptorByName(table, &outputName, 0, false, &desc, &t->err));

	//The pipeline brings its own layout rather than the device's default bindless one

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Bindful pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &file, &name, entryId, NULL, EPipelineFlags_None, pipelineLayout, &pipeline, &t->err
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
		.resource = buffer, .stage = EPipelineStage_Compute, .isWrite = true
	};

	ListTransition transitions = (ListTransition) { 0 };
	ListTransition_createRefConst(&transition, 1, &transitions, NULL);

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

	//The work op is the validator: without a heap or table the dispatch has to be refused.
	//A refused work op invalidates its scope, which endScope hides wholesale; that is by design, so the real
	// dispatch lives in a scope of its own and rebinds its state (scope end resets pipeline and table binds).

	Test_assert(t, "scopeNeg", CommandListRef_startScope(commandList, &transitions, 1, NULL, &t->err));
	Test_assert(t, "bindPipelineNeg", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
	Test_assert(t, "dispatchWithoutHeap", !CommandListRef_dispatch1D(commandList, 1, NULL));
	Test_assert(t, "bindHeapNeg", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "dispatchWithoutTable", !CommandListRef_dispatch1D(commandList, 1, NULL));

	//The recorders themselves refuse NULL and refs of the wrong type before any state changes

	Test_assert(t, "bindHeapNull", !CommandListRef_bindDescriptorHeap(commandList, NULL, NULL));
	Test_assert(t, "bindHeapWrongType", !CommandListRef_bindDescriptorHeap(commandList, buffer, NULL));
	Test_assert(t, "bindTableNull", !CommandListRef_bindDescriptorTable(commandList, NULL, NULL));
	Test_assert(t, "bindTableWrongType", !CommandListRef_bindDescriptorTable(commandList, heap, NULL));
	Test_assert(t, "scopeNegEnd", CommandListRef_endScope(commandList, &t->err));

	//The real work; scope state (pipeline, heap and table binds) reset at endScope, so everything binds again

	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitions, 2, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTable", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
	Test_assert(t, "dispatch", CommandListRef_dispatch1D(commandList, 1, &t->err));
	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, buffer)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(buffer)->cpuData.ptr;

			Bool allMatch = true;

			for(U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == i * 3 + 7;

			Test_assert(t, "bindfulResults", allMatch);
		}

clean:

	if(table)
		DescriptorTableRef_unsetDescriptors(table, 0, 0, 1, NULL);

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&buffer);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);
}

// -- 42. Bindful, advanced: multi binding tables, table switching, wrong heap ----

//The copy shader reads t0 and writes u1, so one table carries an SRV and a UAV range together; two tables
// over the same layout then prove switching tables between dispatches, and a table from another heap proves
// the wrong heap negative. Runs everywhere, bindless included but never required.

void Test_graphicsBindfulAdvanced(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/advanced");

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorHeapRef *otherHeap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *tableA = NULL;
	DescriptorTableRef *tableB = NULL;
	DescriptorTableRef *tableOther = NULL;
	DescriptorLayoutRef *layoutTwin = NULL;
	DescriptorTableRef *tableTwin = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *src = NULL;
	DeviceBufferRef *dstA = NULL;
	DeviceBufferRef *dstB = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };
	DescriptorLayoutInfo layoutTwinInfo = (DescriptorLayoutInfo) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_copy.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping advanced bindful tests");
		return;
	}

	const U32 entryId = TestShaders_entry(t, deviceRef, &file, "main");

	if(entryId == U32_MAX)
		goto clean;

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &file, entryId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	CharString name = CharString_createRefCStrConst("Bindful advanced layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	//Room for both real tables; the other heap exists only to prove its table can't bind under this one

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 6, .maxDescriptorTables = 3 };
	name = CharString_createRefCStrConst("Bindful advanced heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	DescriptorHeapInfo otherHeapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 2, .maxDescriptorTables = 1 };
	name = CharString_createRefCStrConst("Bindful advanced other heap");

	if(!Test_assert(t, "otherHeapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &otherHeapInfo, &name, &otherHeap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful advanced table A");

	if(!Test_assert(t, "tableACreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &tableA, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful advanced table B");

	if(!Test_assert(t, "tableBCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &tableB, &t->err
	)))
		goto clean;

	//A second detect yields a structurally IDENTICAL layout; the work ops must still refuse a table made
	// from it, because layout compatibility is exact object identity, not structural equality

	if(!Test_assert(t, "detectTwinLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &file, entryId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutTwinInfo, NULL, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful advanced twin layout");

	if(!Test_assert(t, "layoutTwinCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutTwinInfo, &name, &layoutTwin, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful advanced twin table");

	if(!Test_assert(t, "tableTwinCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layoutTwin, EDescriptorTableFlags_None, &name, &tableTwin, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful advanced other table");

	if(!Test_assert(t, "tableOtherCreate", DescriptorHeapRef_createDescriptorTable(
		otherHeap, layout, EDescriptorTableFlags_None, &name, &tableOther, &t->err
	)))
		goto clean;

	//src holds 0..63, the shader writes i * 2 + 1 through whichever table is bound

	U32 srcData[64];

	for(U32 i = 0; i < 64; ++i)
		srcData[i] = i;

	Buffer srcRef = Buffer_createRefConst(srcData, sizeof(srcData));
	name = CharString_createRefCStrConst("Bindful advanced src");

	if(!Test_assert(t, "srcCreate", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL,
		&name, &srcRef, &src, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful advanced dst A");

	if(!Test_assert(t, "dstACreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &dstA, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful advanced dst B");

	if(!Test_assert(t, "dstBCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &dstB, &t->err
	)))
		goto clean;

	const Descriptor srcDesc = Descriptor_buffer(src, 0, 0, NULL, 0);
	const Descriptor dstADesc = Descriptor_buffer(dstA, 0, 0, NULL, 0);
	const Descriptor dstBDesc = Descriptor_buffer(dstB, 0, 0, NULL, 0);

	const CharString inputName = CharString_createRefCStrConst("input");
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setSrcA", DescriptorTableRef_setDescriptorByName(tableA, &inputName, 0, false, &srcDesc, &t->err));
	Test_assert(t, "setDstA", DescriptorTableRef_setDescriptorByName(tableA, &outputName, 0, false, &dstADesc, &t->err));
	Test_assert(t, "setSrcB", DescriptorTableRef_setDescriptorByName(tableB, &inputName, 0, false, &srcDesc, &t->err));
	Test_assert(t, "setDstB", DescriptorTableRef_setDescriptorByName(tableB, &outputName, 0, false, &dstBDesc, &t->err));

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Bindful advanced pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful advanced pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &file, &name, entryId, NULL, EPipelineFlags_None, pipelineLayout, &pipeline, &t->err
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

	const Transition transitions[3] = {
		(Transition) { .resource = src,  .stage = EPipelineStage_Compute },
		(Transition) { .resource = dstA, .stage = EPipelineStage_Compute, .isWrite = true },
		(Transition) { .resource = dstB, .stage = EPipelineStage_Compute, .isWrite = true }
	};

	ListTransition transitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(transitions, 3, &transitionList, NULL);

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

	//A table from another heap must be refused at the work op; its own scope so the refusal can't hide the
	// real work recorded below

	Test_assert(t, "scopeNeg", CommandListRef_startScope(commandList, &transitionList, 1, NULL, &t->err));
	Test_assert(t, "bindHeapNeg", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindOtherTable", CommandListRef_bindDescriptorTable(commandList, tableOther, &t->err));
	Test_assert(t, "bindPipelineNeg", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
	Test_assert(t, "dispatchWrongHeap", !CommandListRef_dispatch1D(commandList, 1, NULL));
	Test_assert(t, "bindTwinTable", CommandListRef_bindDescriptorTable(commandList, tableTwin, &t->err));
	Test_assert(t, "dispatchTwinLayout", !CommandListRef_dispatch1D(commandList, 1, NULL));
	Test_assert(t, "scopeNegEnd", CommandListRef_endScope(commandList, &t->err));

	//Two dispatches through two tables, switching between them without touching the heap or pipeline

	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 2, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));

	Test_assert(t, "bindTableA", CommandListRef_bindDescriptorTable(commandList, tableA, &t->err));
	Test_assert(t, "dispatchA", CommandListRef_dispatch1D(commandList, 1, &t->err));

	Test_assert(t, "bindTableB", CommandListRef_bindDescriptorTable(commandList, tableB, &t->err));
	Test_assert(t, "dispatchB", CommandListRef_dispatch1D(commandList, 1, &t->err));

	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0)) {

		Bool okA = TestShaders_pullBuffer(t, deviceRef, emptyList, dstA);
		Bool okB = TestShaders_pullBuffer(t, deviceRef, emptyList, dstB);

		if (okA && okB) {

			const U32 *a = (const U32*) DeviceBufferRef_ptr(dstA)->cpuData.ptr;
			const U32 *b = (const U32*) DeviceBufferRef_ptr(dstB)->cpuData.ptr;

			Bool allMatch = true;

			for(U32 i = 0; i < 64; ++i)
				allMatch &= a[i] == i * 2 + 1 && b[i] == i * 2 + 1;

			Test_assert(t, "bindfulSwitchResults", allMatch);
		}
	}

clean:

	if(tableA) {
		DescriptorTableRef_unsetDescriptors(tableA, 0, 0, 1, NULL);
		DescriptorTableRef_unsetDescriptors(tableA, 1, 0, 1, NULL);
	}

	if(tableB) {
		DescriptorTableRef_unsetDescriptors(tableB, 0, 0, 1, NULL);
		DescriptorTableRef_unsetDescriptors(tableB, 1, 0, 1, NULL);
	}

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&dstB);
	RefPtr_dec(&dstA);
	RefPtr_dec(&src);
	RefPtr_dec(&tableOther);
	RefPtr_dec(&tableTwin);
	RefPtr_dec(&layoutTwin);
	RefPtr_dec(&tableB);
	RefPtr_dec(&tableA);
	RefPtr_dec(&otherHeap);
	RefPtr_dec(&heap);
	RefPtr_dec(&layout);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	DescriptorLayoutInfo_free(&layoutTwinInfo, alloc);
	SHFile_free(&file, alloc);
}

// -- 43. Bindful sampling: a texture, a sampler and a buffer in one table --------

//The sampler range lands in the separate sampler root table on D3D12 (a second root parameter next to the
// resource one), which no other test exercises; on Vulkan it proves sampler descriptors in a plain set.
//An 8x8 nearest sampled texture at texel centers has to reproduce its texels exactly.

void Test_graphicsBindfulSampler(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/sampler");

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceTextureRef *texture = NULL;
	SamplerRef *sampler = NULL;
	DeviceBufferRef *output = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };
	Buffer texData = Buffer_createNull();

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_sampler.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful sampler tests");
		return;
	}

	const U32 entryId = TestShaders_entry(t, deviceRef, &file, "main");

	if(entryId == U32_MAX)
		goto clean;

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &file, entryId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	CharString name = CharString_createRefCStrConst("Bindful sampler layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) {
		.maxTextures = 1, .maxSamplers = 1, .maxBuffersRW = 1, .maxDescriptorTables = 1
	};

	name = CharString_createRefCStrConst("Bindful sampler heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful sampler table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	//An 8x8 RGBA8 texture whose red channel encodes the texel index times three

	if(!Test_assert(t, "texAlloc", Buffer_createUninitializedBytes(8 * 8 * 4, alloc, &texData, &t->err)))
		goto clean;

	for(U64 i = 0; i < 64; ++i) {
		texData.ptrNonConst[i * 4] = (U8)(i * 3);
		texData.ptrNonConst[i * 4 + 1] = 0;
		texData.ptrNonConst[i * 4 + 2] = 0;
		texData.ptrNonConst[i * 4 + 3] = 0xFF;
	}

	name = CharString_createRefCStrConst("Bindful sampler texture");

	if(!Test_assert(t, "textureCreate", GraphicsDeviceRef_createTexture(
		deviceRef, ETextureType_2D, ETextureFormatId_RGBA8, EGraphicsResourceFlag_ShaderRead,
		8, 8, 1, NULL, &name, &texData, &texture, &t->err
	)))
		goto clean;

	//createTexture took ownership of the data; clearing the local keeps clean from double freeing it

	texData = Buffer_createNull();

	const SamplerInfo samplerInfo = (SamplerInfo) { .filter = ESamplerFilterMode_Nearest };
	name = CharString_createRefCStrConst("Bindful sampler sampler");

	if(!Test_assert(t, "samplerCreate", GraphicsDeviceRef_createSampler(
		deviceRef, samplerInfo, true, NULL, &name, &sampler, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful sampler output");

	if(!Test_assert(t, "outputCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &output, &t->err
	)))
		goto clean;

	const Descriptor texDesc = Descriptor_texture(texture, 0, 0, 0, 0, 0, 0);
	const Descriptor sampDesc = Descriptor_sampler(sampler);
	const Descriptor outputDesc = Descriptor_buffer(output, 0, 0, NULL, 0);

	const CharString texName = CharString_createRefCStrConst("tex");
	const CharString sampName = CharString_createRefCStrConst("samp");
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setTex", DescriptorTableRef_setDescriptorByName(table, &texName, 0, false, &texDesc, &t->err));
	Test_assert(t, "setSamp", DescriptorTableRef_setDescriptorByName(table, &sampName, 0, false, &sampDesc, &t->err));
	Test_assert(t, "setOutput", DescriptorTableRef_setDescriptorByName(table, &outputName, 0, false, &outputDesc, &t->err));

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Bindful sampler pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful sampler pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &file, &name, entryId, NULL, EPipelineFlags_None, pipelineLayout, &pipeline, &t->err
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

	const Transition transitions[2] = {
		(Transition) { .resource = texture, .stage = EPipelineStage_Compute },
		(Transition) { .resource = output, .stage = EPipelineStage_Compute, .isWrite = true }
	};

	ListTransition transitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(transitions, 2, &transitionList, NULL);

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 1, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTable", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
	Test_assert(t, "dispatch", CommandListRef_dispatch2D(commandList, 1, 1, &t->err));
	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			Bool allMatch = true;

			for(U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == i * 3;

			Test_assert(t, "bindfulSampleResults", allMatch);
		}

clean:

	if(table) {
		DescriptorTableRef_unsetDescriptors(table, 0, 0, 1, NULL);
		DescriptorTableRef_unsetDescriptors(table, 1, 0, 1, NULL);
		DescriptorTableRef_unsetDescriptors(table, 2, 0, 1, NULL);
	}

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&output);
	RefPtr_dec(&sampler);
	RefPtr_dec(&texture);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	Buffer_free(&texData, alloc);
	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);
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

	SHFile files[2] = { { 0 } };
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

// -- 45. Bindful layout switch: two custom layouts in one scope ------------------

//Module 42 switches tables under ONE pipeline layout and the interleave module crosses custom to default;
// neither ever switches between two DIFFERENT custom layouts. On D3D12 that changes the root signature,
// which drops all root arguments, so the second dispatch only works if the lazy emission re-establishes
// everything. Two pipelines with structurally different layouts dispatch back to back; both outputs checked.

void Test_graphicsBindfulLayoutSwitch(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/layoutSwitch");

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layoutW = NULL;
	DescriptorLayoutRef *layoutC = NULL;
	DescriptorTableRef *tableW = NULL;
	DescriptorTableRef *tableC = NULL;
	PipelineLayoutRef *pipelineLayoutW = NULL;
	PipelineLayoutRef *pipelineLayoutC = NULL;
	PipelineRef *pipelineW = NULL;
	PipelineRef *pipelineC = NULL;
	DeviceBufferRef *bufW = NULL;
	DeviceBufferRef *src = NULL;
	DeviceBufferRef *bufC = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile writeFile = (SHFile) { 0 };
	SHFile copyFile = (SHFile) { 0 };
	DescriptorLayoutInfo layoutWInfo = (DescriptorLayoutInfo) { 0 };
	DescriptorLayoutInfo layoutCInfo = (DescriptorLayoutInfo) { 0 };

	if (
		!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_write.oiSH", &writeFile) ||
		!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_copy.oiSH", &copyFile)
	) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping layout switch tests");
		SHFile_free(&writeFile, alloc);
		return;
	}

	const U32 writeId = TestShaders_entry(t, deviceRef, &writeFile, "main");
	const U32 copyId = TestShaders_entry(t, deviceRef, &copyFile, "main");

	if(writeId == U32_MAX || copyId == U32_MAX)
		goto clean;

	if(!Test_assert(t, "detectLayoutW", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &writeFile, writeId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutWInfo, NULL, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "detectLayoutC", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &copyFile, copyId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutCInfo, NULL, &t->err
	)))
		goto clean;

	CharString name = CharString_createRefCStrConst("Layout switch write layout");

	if(!Test_assert(t, "layoutWCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutWInfo, &name, &layoutW, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Layout switch copy layout");

	if(!Test_assert(t, "layoutCCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutCInfo, &name, &layoutC, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 3, .maxDescriptorTables = 2 };
	name = CharString_createRefCStrConst("Layout switch heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Layout switch write table");

	if(!Test_assert(t, "tableWCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layoutW, EDescriptorTableFlags_None, &name, &tableW, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Layout switch copy table");

	if(!Test_assert(t, "tableCCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layoutC, EDescriptorTableFlags_None, &name, &tableC, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Layout switch write output");

	if(!Test_assert(t, "bufWCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &bufW, &t->err
	)))
		goto clean;

	U32 srcData[64];

	for(U32 i = 0; i < 64; ++i)
		srcData[i] = i;

	Buffer srcRef = Buffer_createRefConst(srcData, sizeof(srcData));
	name = CharString_createRefCStrConst("Layout switch src");

	if(!Test_assert(t, "srcCreate", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL,
		&name, &srcRef, &src, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Layout switch copy output");

	if(!Test_assert(t, "bufCCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &bufC, &t->err
	)))
		goto clean;

	const Descriptor bufWDesc = Descriptor_buffer(bufW, 0, 0, NULL, 0);
	const Descriptor srcDesc = Descriptor_buffer(src, 0, 0, NULL, 0);
	const Descriptor bufCDesc = Descriptor_buffer(bufC, 0, 0, NULL, 0);

	const CharString inputName = CharString_createRefCStrConst("input");
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setBufW", DescriptorTableRef_setDescriptorByName(tableW, &outputName, 0, false, &bufWDesc, &t->err));
	Test_assert(t, "setSrc", DescriptorTableRef_setDescriptorByName(tableC, &inputName, 0, false, &srcDesc, &t->err));
	Test_assert(t, "setBufC", DescriptorTableRef_setDescriptorByName(tableC, &outputName, 0, false, &bufCDesc, &t->err));

	PipelineLayoutInfo pipelineLayoutWInfo = (PipelineLayoutInfo) { .bindings = layoutW };
	name = CharString_createRefCStrConst("Layout switch write pipeline layout");

	if(!Test_assert(t, "pipelineLayoutWCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutWInfo, &name, &pipelineLayoutW, &t->err
	)))
		goto clean;

	PipelineLayoutInfo pipelineLayoutCInfo = (PipelineLayoutInfo) { .bindings = layoutC };
	name = CharString_createRefCStrConst("Layout switch copy pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutCInfo, &name, &pipelineLayoutC, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Layout switch write pipeline");

	if(!Test_assert(t, "pipelineWCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &writeFile, &name, writeId, NULL, EPipelineFlags_None, pipelineLayoutW, &pipelineW, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Layout switch copy pipeline");

	if(!Test_assert(t, "pipelineCCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &copyFile, &name, copyId, NULL, EPipelineFlags_None, pipelineLayoutC, &pipelineC, &t->err
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

	const Transition transitions[3] = {
		(Transition) { .resource = bufW, .stage = EPipelineStage_Compute, .isWrite = true },
		(Transition) { .resource = src,  .stage = EPipelineStage_Compute },
		(Transition) { .resource = bufC, .stage = EPipelineStage_Compute, .isWrite = true }
	};

	ListTransition transitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(transitions, 3, &transitionList, NULL);

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 1, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));

	Test_assert(t, "bindTableW", CommandListRef_bindDescriptorTable(commandList, tableW, &t->err));
	Test_assert(t, "bindPipelineW", CommandListRef_setComputePipeline(commandList, pipelineW, &t->err));
	Test_assert(t, "dispatchW", CommandListRef_dispatch1D(commandList, 1, &t->err));

	Test_assert(t, "bindTableC", CommandListRef_bindDescriptorTable(commandList, tableC, &t->err));
	Test_assert(t, "bindPipelineC", CommandListRef_setComputePipeline(commandList, pipelineC, &t->err));
	Test_assert(t, "dispatchC", CommandListRef_dispatch1D(commandList, 1, &t->err));

	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0)) {

		Bool okW = TestShaders_pullBuffer(t, deviceRef, emptyList, bufW);
		Bool okC = TestShaders_pullBuffer(t, deviceRef, emptyList, bufC);

		if (okW && okC) {

			const U32 *w = (const U32*) DeviceBufferRef_ptr(bufW)->cpuData.ptr;
			const U32 *c = (const U32*) DeviceBufferRef_ptr(bufC)->cpuData.ptr;

			Bool allMatch = true;

			for(U32 i = 0; i < 64; ++i)
				allMatch &= w[i] == i * 3 + 7 && c[i] == i * 2 + 1;

			Test_assert(t, "layoutSwitchResults", allMatch);
		}
	}

clean:

	if(tableW)
		DescriptorTableRef_unsetDescriptors(tableW, 0, 0, 1, NULL);

	if(tableC) {
		DescriptorTableRef_unsetDescriptors(tableC, 0, 0, 1, NULL);
		DescriptorTableRef_unsetDescriptors(tableC, 1, 0, 1, NULL);
	}

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipelineC);
	RefPtr_dec(&pipelineW);
	RefPtr_dec(&pipelineLayoutC);
	RefPtr_dec(&pipelineLayoutW);
	RefPtr_dec(&bufC);
	RefPtr_dec(&src);
	RefPtr_dec(&bufW);
	RefPtr_dec(&tableC);
	RefPtr_dec(&tableW);
	RefPtr_dec(&layoutC);
	RefPtr_dec(&layoutW);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutWInfo, alloc);
	DescriptorLayoutInfo_free(&layoutCInfo, alloc);
	SHFile_free(&writeFile, alloc);
	SHFile_free(&copyFile, alloc);
}

// -- 46. Bindful ray tracing: the TLAS through a table instead of a handle -------

//Ray tracing itself never needed bindless; this proves it by tracing the same 4 ray scene as the bindless
// rays module with the TLAS and output buffer coming from classic registers.
//The TLAS is created with disallowBindlessDescriptor, so this works on a device without bindless at all.

void Test_graphicsBindfulRays(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/rays");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!(device->info.capabilities.features & EGraphicsFeatures_RayPipeline)) {
		Test_print(t, "Device lacks raytracing pipelines, skipping bindful ray trace tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *positions = NULL;
	DeviceBufferRef *output = NULL;
	BLASRef *blas = NULL;
	TLASRef *tlas = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };
	ListU32 entrypoints = (ListU32) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_rays.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful ray trace tests");
		return;
	}

	GraphicsInstanceRef *ownInstanceRef = NULL;
	GraphicsDeviceRef *ownDeviceRef = NULL;
	RefPtrType instanceType = (RefPtrType) { 0 };

	if (!TestShaders_rtDedicatedDevice(t, &deviceRef, &ownInstanceRef, &ownDeviceRef, &instanceType)) {
		SHFile_free(&file, alloc);
		return;
	}

	//The same one triangle scene the bindless rays module uses

	const F32 triangle[12] = {
		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1
	};

	Buffer triData = Buffer_createRefConst(triangle, sizeof(triangle));
	CharString name = CharString_createRefCStrConst("Bindful rays positions");

	//CPUBacked because the BLAS refit at the end rewrites these positions in place and marks them dirty

	if(!Test_assert(t, "createPositions", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_CPUBacked, NULL,
		&name, &triData, &positions, &t->err
	)))
		goto clean;

	const BLASCreateInfo blasInfo = BLASCreateInfo_unindexed(
		ERTASBuildFlags_AllowUpdate, EBLASFlag_None, ETextureFormatId_RGBA32f, 0, 16,
		(DeviceData) { .buffer = positions }
	);

	name = CharString_createRefCStrConst("Bindful rays BLAS");

	if(!Test_assert(t, "createBlas", GraphicsDeviceRef_createBLASExt(deviceRef, &blasInfo, &name, &blas, &t->err)))
		goto clean;

	const TLASInstance instance = (TLASInstance) {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
		.data = (TLASInstanceData) {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (U32) ETLASInstanceFlag_Default << 24,
			.blasCpu = blas
		}
	};

	ListTLASInstance instances = (ListTLASInstance) { 0 };
	ListTLASInstance_createRefConst(&instance, 1, &instances, NULL);

	name = CharString_createRefCStrConst("Bindful rays TLAS");

	if(!Test_assert(t, "createTlas", GraphicsDeviceRef_createTLASExt(
		deviceRef, ERTASBuildFlags_DefaultTLAS | ERTASBuildFlags_AllowUpdate, &instances, true, NULL,
		&name, &tlas, &t->err
	)))
		goto clean;

	const U32 raygenId = TestShaders_entry(t, deviceRef, &file, "mainRaygen");
	const U32 missId = TestShaders_entry(t, deviceRef, &file, "mainMiss");
	const U32 hitId = TestShaders_entry(t, deviceRef, &file, "mainClosestHit");

	if(raygenId == U32_MAX || missId == U32_MAX || hitId == U32_MAX)
		goto clean;

	//The layout comes from all three stages' reflection at once

	const U32 entryIds[3] = { raygenId, missId, hitId };

	Test_assert(t, "entrypointsRef", ListU32_createRefConst(entryIds, 3, &entrypoints, &t->err));

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntries(
		deviceRef, &file, &entrypoints, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful rays layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) {
		.maxBuffersRW = 1, .maxAccelerationStructures = 1, .maxDescriptorTables = 1
	};

	name = CharString_createRefCStrConst("Bindful rays heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful rays table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful rays output");

	if(!Test_assert(t, "outputCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 4 * sizeof(U32), &output, &t->err
	)))
		goto clean;

	const Descriptor tlasDesc = Descriptor_tlas(tlas);
	const Descriptor outputDesc = Descriptor_buffer(output, 0, 0, NULL, 0);

	const CharString sceneName = CharString_createRefCStrConst("scene");
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setTlas", DescriptorTableRef_setDescriptorByName(table, &sceneName, 0, false, &tlasDesc, &t->err));
	Test_assert(t, "setOutput", DescriptorTableRef_setDescriptorByName(table, &outputName, 0, false, &outputDesc, &t->err));

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Bindful rays pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	PipelineStage stages[3] = {
		(PipelineStage) { .binaryId = raygenId },
		(PipelineStage) { .binaryId = missId },
		(PipelineStage) { .binaryId = hitId }
	};

	ListPipelineStage stageList = (ListPipelineStage) { 0 };
	ListPipelineStage_createRefConst(stages, 3, &stageList, NULL);

	ListSHFile fileList = (ListSHFile) { 0 };
	ListSHFile_createRefConst(&file, 1, &fileList, NULL);

	PipelineRaytracingGroup group = (PipelineRaytracingGroup) {
		.closestHit = 2, .anyHit = U32_MAX, .intersection = U32_MAX
	};

	ListPipelineRaytracingGroup groupList = (ListPipelineRaytracingGroup) { 0 };
	ListPipelineRaytracingGroup_createRefConst(&group, 1, &groupList, NULL);

	const PipelineRaytracingInfo info = (PipelineRaytracingInfo) {
		.flags = EPipelineRaytracingFlags_Default,
		.maxRecursionDepth = 1
	};

	name = CharString_createRefCStrConst("Bindful rays pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineRaytracingExt(
		deviceRef, &stageList, &fileList, &groupList, &info, &name, EPipelineFlags_None,
		pipelineLayout, &pipeline, &t->err
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

	const Transition traceTransitions[2] = {
		(Transition) { .resource = output, .stage = EPipelineStage_RaygenExt, .isWrite = true },
		(Transition) { .resource = tlas, .stage = EPipelineStage_RaygenExt }
	};

	ListTransition traceTransitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(traceTransitions, 2, &traceTransitionList, NULL);

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

	Test_assert(t, "scopeBlas", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
	Test_assert(t, "updateBlas", CommandListRef_updateBLASExt(commandList, blas, &t->err));
	Test_assert(t, "scopeBlasEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "scopeTlas", CommandListRef_startScope(commandList, NULL, 2, NULL, &t->err));
	Test_assert(t, "updateTlas", CommandListRef_updateTLASExt(commandList, tlas, &t->err));
	Test_assert(t, "scopeTlasEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "scopeTrace", CommandListRef_startScope(commandList, &traceTransitionList, 3, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTable", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setRaytracingPipeline(commandList, pipeline, &t->err));
	Test_assert(t, "trace", CommandListRef_dispatch1DRaysExt(commandList, 0, 4, &t->err));
	Test_assert(t, "scopeTraceEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			Test_assert(t, "bindfulRayResults", values[0] == 1 && values[1] == 1 && !values[2] && !values[3]);
		}

	//Refit in place: the same TLAS object gets new instance data and is rebuilt rather than recreated, so
	// the descriptor written into the table earlier has to keep addressing it. Moving the instance far along
	// Z takes it out of every ray's path, so all four rays must miss without the table being touched again.

	const TLASInstance movedInstance = (TLASInstance) {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 1000 } },
		.data = (TLASInstanceData) {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (U32) ETLASInstanceFlag_Default << 24,
			.blasCpu = blas
		}
	};

	ListTLASInstance movedInstances = (ListTLASInstance) { 0 };
	ListTLASInstance_createRefConst(&movedInstance, 1, &movedInstances, NULL);

	if (Test_assert(t, "setInstancesMoved", TLASRef_setInstancesExt(tlas, &movedInstances, &t->err))) {

		Test_assert(t, "beginRefit", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		Test_assert(t, "scopeRefit", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
		Test_assert(t, "updateTlasRefit", CommandListRef_updateTLASExt(commandList, tlas, &t->err));
		Test_assert(t, "scopeRefitEnd", CommandListRef_endScope(commandList, &t->err));

		Test_assert(t, "scopeTraceRefit", CommandListRef_startScope(
			commandList, &traceTransitionList, 2, NULL, &t->err
		));

		Test_assert(t, "bindHeapRefit", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
		Test_assert(t, "bindTableRefit", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
		Test_assert(t, "bindPipelineRefit", CommandListRef_setRaytracingPipeline(commandList, pipeline, &t->err));
		Test_assert(t, "traceRefit", CommandListRef_dispatch1DRaysExt(commandList, 0, 4, &t->err));
		Test_assert(t, "scopeTraceRefitEnd", CommandListRef_endScope(commandList, &t->err));

		Test_assert(t, "endRefit", CommandListRef_end(commandList, &t->err));

		if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
			if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

				const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

				Test_assert(t, "refitMissesAll", !values[0] && !values[1] && !values[2] && !values[3]);
			}

		//Refit back to where it started: the second refit reads the state the first one left behind, so a
		// refit that quietly rebuilt from the original instances instead would land on the wrong result here

		if (Test_assert(t, "setInstancesBack", TLASRef_setInstancesExt(tlas, &instances, &t->err))) {

			Test_assert(t, "beginRefitBack", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

			Test_assert(t, "scopeRefitBack", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
			Test_assert(t, "updateTlasRefitBack", CommandListRef_updateTLASExt(commandList, tlas, &t->err));
			Test_assert(t, "scopeRefitBackEnd", CommandListRef_endScope(commandList, &t->err));

			Test_assert(t, "scopeTraceBack", CommandListRef_startScope(
				commandList, &traceTransitionList, 2, NULL, &t->err
			));

			Test_assert(t, "bindHeapBack", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
			Test_assert(t, "bindTableBack", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
			Test_assert(t, "bindPipelineBack", CommandListRef_setRaytracingPipeline(commandList, pipeline, &t->err));
			Test_assert(t, "traceBack", CommandListRef_dispatch1DRaysExt(commandList, 0, 4, &t->err));
			Test_assert(t, "scopeTraceBackEnd", CommandListRef_endScope(commandList, &t->err));

			Test_assert(t, "endRefitBack", CommandListRef_end(commandList, &t->err));

			if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
				if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

					const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

					Test_assert(t, "refitBackHitsAgain", values[0] == 1 && values[1] == 1 && !values[2] && !values[3]);
				}
		}

		//Refitting the geometry itself rather than the instance: the triangle's own vertices move out of
		// reach, so the BLAS has to rebuild from the rewritten positions and the TLAS after it. Both rays
		// missing proves the new vertex data really reached the bottom level structure.

		F32 *movedPositions = (F32*) DeviceBufferRef_ptr(positions)->cpuData.ptrNonConst;

		for(U64 i = 0; i < 3; ++i)
			movedPositions[i * 4 + 2] = 1000;                //Z of every vertex, the stride is 4 floats

		if (Test_assert(t, "markPositionsDirty", DeviceBufferRef_markDirty(positions, 0, sizeof(triangle), &t->err))) {

			Test_assert(t, "beginBlasRefit", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

			Test_assert(t, "scopeBlasRefit", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
			Test_assert(t, "updateBlasRefit", CommandListRef_updateBLASExt(commandList, blas, &t->err));
			Test_assert(t, "scopeBlasRefitEnd", CommandListRef_endScope(commandList, &t->err));

			Test_assert(t, "scopeTlasAfterBlas", CommandListRef_startScope(commandList, NULL, 2, NULL, &t->err));
			Test_assert(t, "updateTlasAfterBlas", CommandListRef_updateTLASExt(commandList, tlas, &t->err));
			Test_assert(t, "scopeTlasAfterBlasEnd", CommandListRef_endScope(commandList, &t->err));

			Test_assert(t, "scopeTraceBlasRefit", CommandListRef_startScope(
				commandList, &traceTransitionList, 3, NULL, &t->err
			));

			Test_assert(t, "bindHeapBlasRefit", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
			Test_assert(t, "bindTableBlasRefit", CommandListRef_bindDescriptorTable(commandList, table, &t->err));

			Test_assert(t, "bindPipelineBlasRefit", CommandListRef_setRaytracingPipeline(
				commandList, pipeline, &t->err
			));

			Test_assert(t, "traceBlasRefit", CommandListRef_dispatch1DRaysExt(commandList, 0, 4, &t->err));
			Test_assert(t, "scopeTraceBlasRefitEnd", CommandListRef_endScope(commandList, &t->err));

			Test_assert(t, "endBlasRefit", CommandListRef_end(commandList, &t->err));

			if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
				if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

					const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

					Test_assert(t, "blasRefitMissesAll", !values[0] && !values[1] && !values[2] && !values[3]);
				}
		}
	}

clean:

	if(table) {
		DescriptorTableRef_unsetDescriptors(table, 0, 0, 1, NULL);
		DescriptorTableRef_unsetDescriptors(table, 1, 0, 1, NULL);
	}

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&output);
	RefPtr_dec(&tlas);
	RefPtr_dec(&blas);
	RefPtr_dec(&positions);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);

	TestShaders_rtDedicatedDeviceEnd(t, &ownInstanceRef, &ownDeviceRef);
}

// -- 47. Bindful constant buffer: the CBV descriptor path, executed --------------

//The only executor of the constant buffer descriptor write (D3D12's CreateConstantBufferView call site is
// dead code without it). The buffer is exactly 256 bytes with EDeviceBufferUsage_Uniform, since a CBV's
// length must be 256-byte aligned and equal the size reflection reported.

void Test_graphicsBindfulCbuffer(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/cbuffer");

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *consts = NULL;
	DeviceBufferRef *output = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_cbuffer.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful CBV tests");
		return;
	}

	const U32 entryId = TestShaders_entry(t, deviceRef, &file, "main");

	if(entryId == U32_MAX)
		goto clean;

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &file, entryId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	CharString name = CharString_createRefCStrConst("Bindful CBV layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) {
		.maxConstantBuffers = 1, .maxBuffersRW = 1, .maxDescriptorTables = 1
	};

	name = CharString_createRefCStrConst("Bindful CBV heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful CBV table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	//scaleBias.x = 3, scaleBias.y = 7: the shader writes i * 3 + 7, same values module 41 proves

	U32 constsData[64] = { 3, 7, 0, 0 };
	Buffer constsRef = Buffer_createRefConst(constsData, sizeof(constsData));
	name = CharString_createRefCStrConst("Bindful CBV constants");

	if(!Test_assert(t, "constsCreate", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_Uniform, EGraphicsResourceFlag_None, NULL,
		&name, &constsRef, &consts, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful CBV output");

	if(!Test_assert(t, "outputCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &output, &t->err
	)))
		goto clean;

	const Descriptor constsDesc = Descriptor_buffer(consts, 0, 0, NULL, 0);
	const Descriptor outputDesc = Descriptor_buffer(output, 0, 0, NULL, 0);

	const CharString constsName = CharString_createRefCStrConst("consts");
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setConsts", DescriptorTableRef_setDescriptorByName(table, &constsName, 0, false, &constsDesc, &t->err));
	Test_assert(t, "setOutput", DescriptorTableRef_setDescriptorByName(table, &outputName, 0, false, &outputDesc, &t->err));

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Bindful CBV pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful CBV pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &file, &name, entryId, NULL, EPipelineFlags_None, pipelineLayout, &pipeline, &t->err
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

	const Transition transitions[2] = {
		(Transition) { .resource = consts, .stage = EPipelineStage_Compute },
		(Transition) { .resource = output, .stage = EPipelineStage_Compute, .isWrite = true }
	};

	ListTransition transitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(transitions, 2, &transitionList, NULL);

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 1, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTable", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
	Test_assert(t, "dispatch", CommandListRef_dispatch1D(commandList, 1, &t->err));
	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			Bool allMatch = true;

			for(U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == i * 3 + 7;

			Test_assert(t, "cbufferResults", allMatch);
		}

clean:

	if(table) {
		DescriptorTableRef_unsetDescriptors(table, 0, 0, 1, NULL);
		DescriptorTableRef_unsetDescriptors(table, 1, 0, 1, NULL);
	}

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&output);
	RefPtr_dec(&consts);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);
}

// -- 48. Bindful RW texture: the storage image descriptor path, executed ---------

//The only executor of a UAV texture in a table. The target is a RenderTexture because DeviceTexture
// creation refuses ShaderWrite; k / 255 stores are exact in 8 bit UNORM, so the pull byte compares.

void Test_graphicsBindfulRwTexture(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/rwTexture");

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	RenderTextureRef *target = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_rwtex.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful RW texture tests");
		return;
	}

	const U32 entryId = TestShaders_entry(t, deviceRef, &file, "main");

	if(entryId == U32_MAX)
		goto clean;

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &file, entryId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	CharString name = CharString_createRefCStrConst("Bindful RW texture layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxTexturesRW = 1, .maxDescriptorTables = 1 };
	name = CharString_createRefCStrConst("Bindful RW texture heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful RW texture table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful RW texture target");

	if(!Test_assert(t, "targetCreate", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_ShaderWrite,
		EMSAASamples_Off, NULL, &name, &target, &t->err
	)))
		goto clean;

	const Descriptor targetDesc = Descriptor_texture(target, 0, 0, 0, 0, 0, 0);
	const CharString targetName = CharString_createRefCStrConst("outTex");

	Test_assert(t, "setTarget", DescriptorTableRef_setDescriptorByName(table, &targetName, 0, false, &targetDesc, &t->err));

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Bindful RW texture pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful RW texture pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &file, &name, entryId, NULL, EPipelineFlags_None, pipelineLayout, &pipeline, &t->err
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
		.resource = target, .stage = EPipelineStage_Compute, .isWrite = true
	};

	ListTransition transitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(&transition, 1, &transitionList, NULL);

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

	//No initializing clear on purpose: the first use is the UAV write, which relies on the backend flagging
	// the first transition as a discard (D3D12's NOT_ZEROED rule wants a discard, clear or copy first)

	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 1, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTable", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
	Test_assert(t, "dispatch", CommandListRef_dispatch2D(commandList, 1, 1, &t->err));
	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0)) {

		//Each pixel holds R = i * 3, A = 255, so the packed value is 0xFF000000 | (i * 3)

		TestShaderPixels pixels = (TestShaderPixels) { 0 };

		if (TestShaders_pullPixels(t, deviceRef, emptyList, target, &pixels)) {

			Bool allMatch = true;

			for(U32 i = 0; i < 64; ++i)
				allMatch &= pixels.pixels[i] == (0xFF000000u | (i * 3));

			Test_assert(t, "rwTextureResults", allMatch);
		}
	}

clean:

	if(table)
		DescriptorTableRef_unsetDescriptors(table, 0, 0, 1, NULL);

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&target);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);
}

// -- 49. Bindful descriptor array: one binding, four elements, each addressed ----

//A register array is a single binding with count 4; each element gets its own buffer, filled so every slot
// contributes a distinguishable term. Non bindless sets have no partially-bound semantics, so every element
// the shader statically reads is populated before submit.

void Test_graphicsBindfulArray(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/array");

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *inputs[4] = { NULL };
	DeviceBufferRef *output = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_array.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful array tests");
		return;
	}

	const U32 entryId = TestShaders_entry(t, deviceRef, &file, "main");

	if(entryId == U32_MAX)
		goto clean;

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &file, entryId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	CharString name = CharString_createRefCStrConst("Bindful array layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 5, .maxDescriptorTables = 1 };
	name = CharString_createRefCStrConst("Bindful array heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful array table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	//Element j holds i + j * 1000 at slot i, so the sum 4i + 6000 breaks if any slot is misaddressed

	const CharString inputsName = CharString_createRefCStrConst("inputs");

	for(U32 j = 0; j < 4; ++j) {

		U32 data[64];

		for(U32 i = 0; i < 64; ++i)
			data[i] = i + j * 1000;

		Buffer dataRef = Buffer_createRefConst(data, sizeof(data));
		name = CharString_createRefCStrConst("Bindful array input");

		if(!Test_assert(t, "inputCreate", GraphicsDeviceRef_createBufferData(
			deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL,
			&name, &dataRef, &inputs[j], &t->err
		)))
			goto clean;

		const Descriptor desc = Descriptor_buffer(inputs[j], 0, 0, NULL, 0);

		Test_assert(t, "setInput", DescriptorTableRef_setDescriptorByName(table, &inputsName, j, false, &desc, &t->err));
	}

	name = CharString_createRefCStrConst("Bindful array output");

	if(!Test_assert(t, "outputCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &output, &t->err
	)))
		goto clean;

	const Descriptor outputDesc = Descriptor_buffer(output, 0, 0, NULL, 0);
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setOutput", DescriptorTableRef_setDescriptorByName(table, &outputName, 0, false, &outputDesc, &t->err));

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Bindful array pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful array pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &file, &name, entryId, NULL, EPipelineFlags_None, pipelineLayout, &pipeline, &t->err
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

	const Transition transitions[5] = {
		(Transition) { .resource = inputs[0], .stage = EPipelineStage_Compute },
		(Transition) { .resource = inputs[1], .stage = EPipelineStage_Compute },
		(Transition) { .resource = inputs[2], .stage = EPipelineStage_Compute },
		(Transition) { .resource = inputs[3], .stage = EPipelineStage_Compute },
		(Transition) { .resource = output, .stage = EPipelineStage_Compute, .isWrite = true }
	};

	ListTransition transitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(transitions, 5, &transitionList, NULL);

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 1, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTable", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
	Test_assert(t, "dispatch", CommandListRef_dispatch1D(commandList, 1, &t->err));
	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			Bool allMatch = true;

			for(U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == i * 4 + 6000;

			Test_assert(t, "arrayResults", allMatch);
		}

clean:

	if(table) {

		for(U32 j = 0; j < 4; ++j)
			DescriptorTableRef_unsetDescriptors(table, 0, j, 1, NULL);

		DescriptorTableRef_unsetDescriptors(table, 1, 0, 1, NULL);
	}

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&output);

	for(U32 j = 0; j < 4; ++j)
		RefPtr_dec(&inputs[j]);

	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);
}

// -- 50. Bindful multi space: space0 and space1 in one layout --------------------

//space1 maps to a second descriptor set on Vulkan and a distinct RegisterSpace range on D3D12; no other
// layout in the suite leaves space0. The copy result proves both spaces really reached the shader.

void Test_graphicsBindfulSpaces(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/spaces");

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *src = NULL;
	DeviceBufferRef *output = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_spaces.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful spaces tests");
		return;
	}

	const U32 entryId = TestShaders_entry(t, deviceRef, &file, "main");

	if(entryId == U32_MAX)
		goto clean;

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &file, entryId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	CharString name = CharString_createRefCStrConst("Bindful spaces layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 2, .maxDescriptorTables = 1 };
	name = CharString_createRefCStrConst("Bindful spaces heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful spaces table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	U32 srcData[64];

	for(U32 i = 0; i < 64; ++i)
		srcData[i] = i;

	Buffer srcRef = Buffer_createRefConst(srcData, sizeof(srcData));
	name = CharString_createRefCStrConst("Bindful spaces src");

	if(!Test_assert(t, "srcCreate", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL,
		&name, &srcRef, &src, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful spaces output");

	if(!Test_assert(t, "outputCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &output, &t->err
	)))
		goto clean;

	const Descriptor srcDesc = Descriptor_buffer(src, 0, 0, NULL, 0);
	const Descriptor outputDesc = Descriptor_buffer(output, 0, 0, NULL, 0);

	const CharString inputName = CharString_createRefCStrConst("input");
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setSrc", DescriptorTableRef_setDescriptorByName(table, &inputName, 0, false, &srcDesc, &t->err));
	Test_assert(t, "setOutput", DescriptorTableRef_setDescriptorByName(table, &outputName, 0, false, &outputDesc, &t->err));

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Bindful spaces pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful spaces pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &file, &name, entryId, NULL, EPipelineFlags_None, pipelineLayout, &pipeline, &t->err
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

	const Transition transitions[2] = {
		(Transition) { .resource = src, .stage = EPipelineStage_Compute },
		(Transition) { .resource = output, .stage = EPipelineStage_Compute, .isWrite = true }
	};

	ListTransition transitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(transitions, 2, &transitionList, NULL);

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 1, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTable", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
	Test_assert(t, "dispatch", CommandListRef_dispatch1D(commandList, 1, &t->err));
	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			Bool allMatch = true;

			for(U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == i * 5 + 3;

			Test_assert(t, "spacesResults", allMatch);
		}

clean:

	if(table) {
		DescriptorTableRef_unsetDescriptors(table, 0, 0, 1, NULL);
		DescriptorTableRef_unsetDescriptors(table, 1, 0, 1, NULL);
	}

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&output);
	RefPtr_dec(&src);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);
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

	SHFile files[6] = { { 0 } };
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
	RefPtr_dec(&msaaTarget);
	RefPtr_dec(&mrtTarget);
	RefPtr_dec(&target);
	RefPtr_dec(&pipelineLayout);

	for(U64 i = 0; i < 6; ++i)
		SHFile_free(&files[i], alloc);
}

// -- 53. Updating a table between two submits of the same table ------------------

//Descriptor writes land in the backend immediately under a spinlock, with no frame fence of their own, and
// a non bindless Vulkan set has no update-after-bind semantics: rebinding a descriptor a submitted frame
// still references would be a use after free of the descriptor.
//What is legal is updating once that frame retired, which is exactly what this records: submit, wait, point
// the same binding of the same table at another buffer, submit again. Both results are checked, so a stale
// descriptor surviving the update shows up as the first buffer's values coming back twice.

void Test_graphicsBindfulTableUpdate(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/tableUpdate");

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *srcA = NULL;
	DeviceBufferRef *srcB = NULL;
	DeviceBufferRef *output = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_copy.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping table update tests");
		return;
	}

	const U32 entryId = TestShaders_entry(t, deviceRef, &file, "main");

	if(entryId == U32_MAX)
		goto clean;

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &file, entryId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	CharString name = CharString_createRefCStrConst("Table update layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 3, .maxDescriptorTables = 1 };
	name = CharString_createRefCStrConst("Table update heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Table update table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	//Two sources whose results can't be confused: A gives i * 2 + 1, B gives i * 2 + 1001

	U32 dataA[64], dataB[64];

	for(U32 i = 0; i < 64; ++i) {
		dataA[i] = i;
		dataB[i] = i + 500;
	}

	Buffer refA = Buffer_createRefConst(dataA, sizeof(dataA));
	name = CharString_createRefCStrConst("Table update src A");

	if(!Test_assert(t, "srcACreate", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL,
		&name, &refA, &srcA, &t->err
	)))
		goto clean;

	Buffer refB = Buffer_createRefConst(dataB, sizeof(dataB));
	name = CharString_createRefCStrConst("Table update src B");

	if(!Test_assert(t, "srcBCreate", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL,
		&name, &refB, &srcB, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Table update output");

	if(!Test_assert(t, "outputCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &output, &t->err
	)))
		goto clean;

	const Descriptor srcADesc = Descriptor_buffer(srcA, 0, 0, NULL, 0);
	const Descriptor srcBDesc = Descriptor_buffer(srcB, 0, 0, NULL, 0);
	const Descriptor outputDesc = Descriptor_buffer(output, 0, 0, NULL, 0);

	const CharString inputName = CharString_createRefCStrConst("input");
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setSrcA", DescriptorTableRef_setDescriptorByName(table, &inputName, 0, false, &srcADesc, &t->err));
	Test_assert(t, "setOutput", DescriptorTableRef_setDescriptorByName(table, &outputName, 0, false, &outputDesc, &t->err));

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Table update pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Table update pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &file, &name, entryId, NULL, EPipelineFlags_None, pipelineLayout, &pipeline, &t->err
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

	//Both sources are declared up front: transitions are recorded against concrete resources, and the second
	// submit replays the same list after the table points at the other one

	const Transition transitions[3] = {
		(Transition) { .resource = srcA, .stage = EPipelineStage_Compute },
		(Transition) { .resource = srcB, .stage = EPipelineStage_Compute },
		(Transition) { .resource = output, .stage = EPipelineStage_Compute, .isWrite = true }
	};

	ListTransition transitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(transitions, 3, &transitionList, NULL);

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 1, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTable", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
	Test_assert(t, "dispatch", CommandListRef_dispatch1D(commandList, 1, &t->err));
	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	//First submit reads A; submitAndWait returns only once the frame retired, which is what makes the
	// descriptor update below legal

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			Bool allMatch = true;

			for(U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == i * 2 + 1;

			Test_assert(t, "firstSubmitValues", allMatch);
		}

	//Same table, same binding, another buffer

	Test_assert(t, "updateSrc", DescriptorTableRef_setDescriptorByName(table, &inputName, 0, true, &srcBDesc, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			Bool allMatch = true;

			for(U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == (i + 500) * 2 + 1;

			Test_assert(t, "secondSubmitValues", allMatch);
		}

clean:

	if(table) {
		DescriptorTableRef_unsetDescriptors(table, 0, 0, 1, NULL);
		DescriptorTableRef_unsetDescriptors(table, 1, 0, 1, NULL);
	}

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&output);
	RefPtr_dec(&srcB);
	RefPtr_dec(&srcA);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);
}

// -- 54. One register visible to two stages -------------------------------------

//A binding both the vertex and the pixel stage read is the only source of a multi stage visibility mask;
// detection unions the two, D3D12 turns that into SHADER_VISIBILITY_ALL and Vulkan into multi bit stage
// flags, none of which any other test reaches (every other layout is compute only or pixel only).
//The vertex stage scales the triangle by a value it reads, so a register that never became visible there
// collapses the triangle and no pixel survives to be compared.

void Test_graphicsBindfulSharedRegister(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/sharedRegister");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering)) {
		Test_print(t, "Device lacks direct rendering, skipping shared register tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *shared = NULL;
	RenderTextureRef *target = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };
	ListU32 entrypoints = (ListU32) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_shared_reg.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping shared register tests");
		return;
	}

	ListSHFile fileList = (ListSHFile) { 0 };
	ListSHFile_createRefConst(&file, 1, &fileList, NULL);

	const U32 vertexId = TestShaders_entry(t, deviceRef, &file, "mainVertex");
	const U32 pixelId = TestShaders_entry(t, deviceRef, &file, "mainPixel");

	if(vertexId == U32_MAX || pixelId == U32_MAX)
		goto clean;

	//Detecting from both entries at once is what unions the visibility of the register they share

	const U32 entryIds[2] = { vertexId, pixelId };

	Test_assert(t, "entrypointsRef", ListU32_createRefConst(entryIds, 2, &entrypoints, &t->err));

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntries(
		deviceRef, &file, &entrypoints, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	//The union is the point, so it is asserted rather than assumed from the pixels alone

	Bool sharedVisibility = false;

	for(U64 i = 0; i < layoutInfo.bindings.length; ++i) {

		const U32 visibility = layoutInfo.bindings.ptr[i].visibility;

		if(
			(visibility & (1 << ESHPipelineStage_Vertex)) &&
			(visibility & (1 << ESHPipelineStage_Pixel))
		)
			sharedVisibility = true;
	}

	Test_assert(t, "visibilityUnion", sharedVisibility);

	CharString name = CharString_createRefCStrConst("Shared register layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 1, .maxDescriptorTables = 1 };
	name = CharString_createRefCStrConst("Shared register heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Shared register table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	//Slot 0 is the vertex stage's scale (1), bytes 16.. are the color the pixel stage returns

	const F32 color[4] = { 1, 102.f / 255, 51.f / 255, 1 };
	U32 sharedData[8] = { 1, 0, 0, 0 };
	Buffer_memcpy(
		Buffer_createRef(&sharedData[4], sizeof(F32) * 4),
		Buffer_createRefConst(color, sizeof(color))
	);

	Buffer sharedRef = Buffer_createRefConst(sharedData, sizeof(sharedData));
	name = CharString_createRefCStrConst("Shared register buffer");

	if(!Test_assert(t, "sharedCreate", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL,
		&name, &sharedRef, &shared, &t->err
	)))
		goto clean;

	const Descriptor sharedDesc = Descriptor_buffer(shared, 0, 0, NULL, 0);
	const CharString sharedName = CharString_createRefCStrConst("sharedParams");

	Test_assert(t, "setShared", DescriptorTableRef_setDescriptorByName(table, &sharedName, 0, false, &sharedDesc, &t->err));

	name = CharString_createRefCStrConst("Shared register target");

	if(!Test_assert(t, "targetCreate", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &name, &target, &t->err
	)))
		goto clean;

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Shared register pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	PipelineStage stages[2] = {
		(PipelineStage) { .binaryId = vertexId, .shFileId = 0 },
		(PipelineStage) { .binaryId = pixelId, .shFileId = 0 }
	};

	ListPipelineStage stageList = (ListPipelineStage) { 0 };
	ListPipelineStage_createRefConst(stages, 2, &stageList, NULL);

	const PipelineGraphicsInfo pipelineInfo = (PipelineGraphicsInfo) {
		.attachmentCountExt = 1,
		.attachmentFormatsExt = { ETextureFormatId_RGBA8 }
	};

	name = CharString_createRefCStrConst("Shared register pipeline");

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

	//The vertex stage reads it too, so the transition names the earliest stage that touches it

	const Transition transition = (Transition) { .resource = shared, .stage = EPipelineStage_Vertex };

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
	RefPtr_dec(&shared);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);
}

// -- 55. Descriptor table slot recycling ----------------------------------------

//A heap's table budget is a real allocation, not a counter: freeing a table has to hand its slot back on
// both backends (D3D12 frees the heap region it sub allocated, Vulkan frees the set back to its pool).
//Refusing to allocate past the budget is already covered; what is not is that the slot works again
// afterwards, so this fills the budget, frees, reallocates and then actually dispatches through the
// recycled table rather than only creating it.

void Test_graphicsBindfulHeapRecycle(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/heapRecycle");

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *tableFirst = NULL;
	DescriptorTableRef *tableOverBudget = NULL;
	DescriptorTableRef *tableRecycled = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *output = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_write.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping heap recycle tests");
		return;
	}

	const U32 entryId = TestShaders_entry(t, deviceRef, &file, "main");

	if(entryId == U32_MAX)
		goto clean;

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &file, entryId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	CharString name = CharString_createRefCStrConst("Heap recycle layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	//Room for exactly one table, so the second allocation has to be refused and the third can only succeed
	// by reusing what the first one gave back

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 1, .maxDescriptorTables = 1 };
	name = CharString_createRefCStrConst("Heap recycle heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Heap recycle first table");

	if(!Test_assert(t, "tableFirstCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &tableFirst, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Heap recycle over budget table");

	Test_assert(t, "tableOverBudgetRefused", !DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &tableOverBudget, NULL
	));

	Test_assert(t, "tableOverBudgetNull", !tableOverBudget);

	//Freeing a table that never held a descriptor keeps the debug build's leaked descriptor warning quiet,
	// which would otherwise fire for slots still set at free time

	RefPtr_dec(&tableFirst);

	name = CharString_createRefCStrConst("Heap recycle recycled table");

	if(!Test_assert(t, "tableRecycledCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &tableRecycled, &t->err
	)))
		goto clean;

	//Creating it is not enough: the recycled slot has to survive a real dispatch through it

	name = CharString_createRefCStrConst("Heap recycle output");

	if(!Test_assert(t, "outputCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &output, &t->err
	)))
		goto clean;

	const Descriptor outputDesc = Descriptor_buffer(output, 0, 0, NULL, 0);
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setOutput", DescriptorTableRef_setDescriptorByName(
		tableRecycled, &outputName, 0, false, &outputDesc, &t->err
	));

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Heap recycle pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Heap recycle pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &file, &name, entryId, NULL, EPipelineFlags_None, pipelineLayout, &pipeline, &t->err
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
		.resource = output, .stage = EPipelineStage_Compute, .isWrite = true
	};

	ListTransition transitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(&transition, 1, &transitionList, NULL);

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 1, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTable", CommandListRef_bindDescriptorTable(commandList, tableRecycled, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
	Test_assert(t, "dispatch", CommandListRef_dispatch1D(commandList, 1, &t->err));
	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			Bool allMatch = true;

			for(U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == i * 3 + 7;

			Test_assert(t, "recycledTableResults", allMatch);
		}

clean:

	if(tableRecycled)
		DescriptorTableRef_unsetDescriptors(tableRecycled, 0, 0, 1, NULL);

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&output);
	RefPtr_dec(&tableRecycled);
	RefPtr_dec(&tableOverBudget);
	RefPtr_dec(&tableFirst);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);
}

// -- 56. The push descriptor boundary -------------------------------------------

//Push descriptors and push constants are phase 2: no command writes them yet, so a pipeline whose layout
// declares push descriptors would read whatever the backend happened to leave behind. The work ops refuse
// it instead, and this pins that refusal so the day it becomes supported is a deliberate change here
// rather than a silent one.
//The refusal is checked with a heap and a table already bound, since the push descriptor check runs before
// the unbound heap check and would otherwise pass for the wrong reason.

void Test_graphicsBindfulPushDescriptorBoundary(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/pushBoundary");

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorLayoutRef *pushLayout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *output = NULL;
	CommandListRef *commandList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };
	DescriptorLayoutInfo pushInfo = (DescriptorLayoutInfo) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_spaces.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping push boundary tests");
		return;
	}

	const U32 entryId = TestShaders_entry(t, deviceRef, &file, "main");

	if(entryId == U32_MAX)
		goto clean;

	//Naming one register makes it a push descriptor while the other stays a normal binding, which is the
	// mixed layout the work ops have to refuse. The assume-all flag would leave no normal bindings at all.
	//This shader keeps its two registers in different spaces on purpose: Vulkan refuses a pipeline layout
	// whose push descriptors and normal bindings land in the same set.

	const CharString pushName = CharString_createRefCStrConst("input");
	ListCharString pushNames = (ListCharString) { 0 };
	ListCharString_createRefConst(&pushName, 1, &pushNames, NULL);

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &file, entryId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		&pushNames, NULL, NULL, &layoutInfo, &pushInfo, &t->err
	)))
		goto clean;

	if (!pushInfo.bindings.length || !layoutInfo.bindings.length) {
		Test_print(t, "Shader didn't split into push and normal bindings, skipping push boundary tests");
		goto clean;
	}

	CharString name = CharString_createRefCStrConst("Push boundary layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Push boundary push layout");

	if(!Test_assert(t, "pushLayoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &pushInfo, &name, &pushLayout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 2, .maxDescriptorTables = 1 };
	name = CharString_createRefCStrConst("Push boundary heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Push boundary table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Push boundary output");

	if(!Test_assert(t, "outputCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &output, &t->err
	)))
		goto clean;

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) {
		.bindings = layout, .pushDescriptors = pushLayout
	};

	name = CharString_createRefCStrConst("Push boundary pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Push boundary pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &file, &name, entryId, NULL, EPipelineFlags_None, pipelineLayout, &pipeline, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "listCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, 2 * KIBI, 32, 16, true, &commandList, &t->err
	)))
		goto clean;

	const Transition transition = (Transition) {
		.resource = output, .stage = EPipelineStage_Compute, .isWrite = true
	};

	ListTransition transitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(&transition, 1, &transitionList, NULL);

	//Everything a normal dispatch needs is in place, so the only thing left to refuse it is the layout's
	// push descriptors. The scope stays unsubmitted: a refused work op invalidates it and end() hides it.

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 1, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTable", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
	Test_assert(t, "dispatchRefused", !CommandListRef_dispatch1D(commandList, 1, NULL));
	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

clean:

	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&output);
	RefPtr_dec(&table);
	RefPtr_dec(&pushLayout);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	DescriptorLayoutInfo_free(&pushInfo, alloc);
	SHFile_free(&file, alloc);
}

// -- 57. Comparison sampler against a depth texture -----------------------------

//A SamplerComparisonState is a register type of its own, and writing a descriptor into it is type checked
// against the sampler's own comparison flag, so a plain sampler and a comparison sampler are not
// interchangeable. Only the plain one has ever been written, so the comparison branch never executed, and
// neither did a depth texture read through a table.
//The depth buffer is cleared to a known value by a render pass that draws nothing, then every thread
// compares its own reference against it: with a Less comparison the answer flips at exactly half the
// threads, which no filtering or precision detail can move.

void Test_graphicsBindfulSamplerCmp(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/samplerCmp");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering)) {
		Test_print(t, "Device lacks direct rendering, skipping comparison sampler tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DepthStencilRef *depth = NULL;
	RenderTextureRef *colorTarget = NULL;
	SamplerRef *sampler = NULL;
	DeviceBufferRef *output = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_sampler_cmp.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping comparison sampler tests");
		return;
	}

	const U32 entryId = TestShaders_entry(t, deviceRef, &file, "main");

	if(entryId == U32_MAX)
		goto clean;

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &file, entryId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	CharString name = CharString_createRefCStrConst("Comparison sampler layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) {
		.maxTextures = 1, .maxSamplers = 1, .maxBuffersRW = 1, .maxDescriptorTables = 1
	};

	name = CharString_createRefCStrConst("Comparison sampler heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Comparison sampler table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	//allowShaderRead is what makes the depth buffer bindable as a texture at all

	name = CharString_createRefCStrConst("Comparison sampler depth");

	if(!Test_assert(t, "depthCreate", GraphicsDeviceRef_createDepthStencil(
		deviceRef, 8, 8, EDepthStencilFormat_D32, true, EMSAASamples_Off, NULL, &name, &depth, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Comparison sampler color");

	if(!Test_assert(t, "colorCreate", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &name, &colorTarget, &t->err
	)))
		goto clean;

	//Comparison samplers are a distinct descriptor: enableComparison is what the table's write checks the
	// register type against

	const SamplerInfo samplerInfo = (SamplerInfo) {
		.filter = ESamplerFilterMode_Nearest,
		.enableComparison = true,
		.comparisonFunction = ECompareOp_Lt
	};

	name = CharString_createRefCStrConst("Comparison sampler sampler");

	if(!Test_assert(t, "samplerCreate", GraphicsDeviceRef_createSampler(
		deviceRef, samplerInfo, true, NULL, &name, &sampler, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Comparison sampler output");

	if(!Test_assert(t, "outputCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &output, &t->err
	)))
		goto clean;

	const Descriptor depthDesc = Descriptor_texture(depth, 0, 0, 0, 0, 0, 0);
	const Descriptor samplerDesc = Descriptor_sampler(sampler);
	const Descriptor outputDesc = Descriptor_buffer(output, 0, 0, NULL, 0);

	const CharString depthName = CharString_createRefCStrConst("depthTex");
	const CharString samplerName = CharString_createRefCStrConst("cmpSamp");
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setDepth", DescriptorTableRef_setDescriptorByName(table, &depthName, 0, false, &depthDesc, &t->err));
	Test_assert(t, "setSampler", DescriptorTableRef_setDescriptorByName(
		table, &samplerName, 0, false, &samplerDesc, &t->err
	));
	Test_assert(t, "setOutput", DescriptorTableRef_setDescriptorByName(table, &outputName, 0, false, &outputDesc, &t->err));

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Comparison sampler pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Comparison sampler pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &file, &name, entryId, NULL, EPipelineFlags_None, pipelineLayout, &pipeline, &t->err
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

	const Transition transitions[2] = {
		(Transition) { .resource = depth, .stage = EPipelineStage_Compute },
		(Transition) { .resource = output, .stage = EPipelineStage_Compute, .isWrite = true }
	};

	ListTransition transitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(transitions, 2, &transitionList, NULL);

	//A render pass that draws nothing, purely so the depth clear fills the buffer with a known value

	const AttachmentInfo color = (AttachmentInfo) { .image = colorTarget, .load = ELoadAttachmentType_Clear };
	ListAttachmentInfo colors = (ListAttachmentInfo) { 0 };
	ListAttachmentInfo_createRefConst(&color, 1, &colors, NULL);

	const DepthStencilAttachmentInfo depthAttach = (DepthStencilAttachmentInfo) {
		.image = depth,
		.depthLoad = ELoadAttachmentType_Clear,
		.clearDepth = 0.5f
	};

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

	Test_assert(t, "scopeClear", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));

	Test_assert(t, "renderStart", CommandListRef_startRenderExt(
		commandList, I32x2_zero, I32x2_create2(8, 8), &colors, &depthAttach, &t->err
	));

	Test_assert(t, "renderEnd", CommandListRef_endRenderExt(commandList, &t->err));
	Test_assert(t, "scopeClearEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 2, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTable", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
	Test_assert(t, "dispatch", CommandListRef_dispatch1D(commandList, 1, &t->err));
	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			//reference i / 64 against a depth of 0.5 with a Less comparison: the first 32 threads pass

			Bool allMatch = true;

			for(U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == (i < 32 ? 1u : 0u);

			Test_assert(t, "comparisonResults", allMatch);
		}

clean:

	if(table) {
		DescriptorTableRef_unsetDescriptors(table, 0, 0, 1, NULL);
		DescriptorTableRef_unsetDescriptors(table, 1, 0, 1, NULL);
		DescriptorTableRef_unsetDescriptors(table, 2, 0, 1, NULL);
	}

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&output);
	RefPtr_dec(&sampler);
	RefPtr_dec(&colorTarget);
	RefPtr_dec(&depth);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);
}
