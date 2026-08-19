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
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *src = NULL;
	DeviceBufferRef *dstA = NULL;
	DeviceBufferRef *dstB = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };

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

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 4, .maxDescriptorTables = 2 };
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
	RefPtr_dec(&tableB);
	RefPtr_dec(&tableA);
	RefPtr_dec(&otherHeap);
	RefPtr_dec(&heap);
	RefPtr_dec(&layout);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
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

// -- 45. Bindful ray tracing: the TLAS through a table instead of a handle -------

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

	if(!Test_assert(t, "createPositions", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &triData, &positions, &t->err
	)))
		goto clean;

	const BLASCreateInfo blasInfo = BLASCreateInfo_unindexed(
		ERTASBuildFlags_None, EBLASFlag_None, ETextureFormatId_RGBA32f, 0, 16,
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
		deviceRef, ERTASBuildFlags_DefaultTLAS, &instances, true, NULL, &name, &tlas, &t->err
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
