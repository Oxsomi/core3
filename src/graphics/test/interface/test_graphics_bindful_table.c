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

//graphics/test/interface/test_graphics_bindful_table.c
//
//Bindful descriptor table mechanics: binding one at record time, switching between them, updating a
//live table, recycling its slots, and where the push descriptor boundary sits.
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

	//A device without VK_KHR_push_descriptor refuses a caller owned push descriptor layout outright, which is
	// the documented gap rather than a failure of this module.
	//Android emulators are the usual case, since gfxstream drops the extension from the guest.

	if(!GraphicsDeviceRef_createDescriptorLayout(deviceRef, &pushInfo, &name, &pushLayout, NULL)) {
		Test_print(t, "Device has no push descriptor support, skipping push boundary tests");
		goto clean;
	}

	Test_assert(t, "pushLayoutCreate", pushLayout);

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
