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

//graphics/test/interface/test_graphics_bindful_resources.c
//
//Every descriptor KIND reachable through a bindful table, each one executed rather than merely bound:
//samplers, constant buffers, RW textures, arrays, multiple spaces, a register shared by two stages,
//comparison sampling, structured buffers, append counters and float atomics.
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

// -- 58. Structured buffers: element addressing and stride ----------------------

//Every other bindful buffer in the suite is byte addressed, so the structured path is untouched: the table
// validates the descriptor's range against the stride reflection reported, and the shader indexes by
// element rather than by byte. Each of the four fields is transformed differently, so a stride the backend
// got wrong shows up as a specific field landing on a neighbour's value.

typedef struct TestBindfulElem {
	U32 a, b, c, d;
} TestBindfulElem;

void Test_graphicsBindfulStructured(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/structured");

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *input = NULL;
	DeviceBufferRef *output = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_structured.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping structured buffer tests");
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

	//The stride is what makes this a structured buffer rather than a byte address one, so it is asserted
	// rather than assumed from the results

	Bool strideMatches = false;

	for(U64 i = 0; i < layoutInfo.bindings.length; ++i)
		if(layoutInfo.bindings.ptr[i].structedBufferStride == sizeof(TestBindfulElem))
			strideMatches = true;

	Test_assert(t, "reflectedStride", strideMatches);

	CharString name = CharString_createRefCStrConst("Structured layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 2, .maxDescriptorTables = 1 };
	name = CharString_createRefCStrConst("Structured heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Structured table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	TestBindfulElem elems[64];

	for(U32 i = 0; i < 64; ++i)
		elems[i] = (TestBindfulElem) { .a = i, .b = i * 7, .c = i * 3, .d = i * 11 };

	Buffer inputRef = Buffer_createRefConst(elems, sizeof(elems));
	name = CharString_createRefCStrConst("Structured input");

	if(!Test_assert(t, "inputCreate", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL,
		&name, &inputRef, &input, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Structured output");

	if(!Test_assert(t, "outputCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, sizeof(elems), &output, &t->err
	)))
		goto clean;

	const Descriptor inputDesc = Descriptor_buffer(input, 0, 0, NULL, 0);
	const Descriptor outputDesc = Descriptor_buffer(output, 0, 0, NULL, 0);

	const CharString inputName = CharString_createRefCStrConst("input");
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setInput", DescriptorTableRef_setDescriptorByName(table, &inputName, 0, false, &inputDesc, &t->err));
	Test_assert(t, "setOutput", DescriptorTableRef_setDescriptorByName(table, &outputName, 0, false, &outputDesc, &t->err));

	//A range that isn't a whole number of elements has to be refused, which is the stride rule itself

	const Descriptor misalignedDesc = Descriptor_buffer(input, 0, sizeof(elems) - 4, NULL, 0);

	Test_assert(t, "misalignedRangeRefused", !DescriptorTableRef_setDescriptorByName(
		table, &inputName, 0, true, &misalignedDesc, NULL
	));

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Structured pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Structured pipeline");

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
		(Transition) { .resource = input, .stage = EPipelineStage_Compute },
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

			const TestBindfulElem *values = (const TestBindfulElem*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			Bool allMatch = true;

			for(U32 i = 0; i < 64; ++i)
				allMatch &=
					values[i].a == i * 2 &&
					values[i].b == i * 7 + 100 &&
					values[i].c == ((i * 3) ^ 0xFFu) &&
					values[i].d == i * 11 + i;

			Test_assert(t, "structuredResults", allMatch);
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
	RefPtr_dec(&input);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);
}

// -- 59. Append buffer with a counter resource ----------------------------------

//A counter is a second resource hanging off one descriptor, and the table only accepts one on an
// Append/Consume register, which nothing has ever exercised. Vulkan's descriptor write refuses counters
// outright today, so this executes the append on D3D12 and pins that documented refusal on Vulkan, which
// keeps the boundary visible rather than leaving the backend difference implicit.

void Test_graphicsBindfulAppendCounter(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/appendCounter");

	const Allocator *alloc = Platform_instance->alloc;
	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	const Bool isVulkan = GraphicsInstanceRef_ptr(device->instance)->api == EGraphicsApi_Vulkan;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *appended = NULL;
	DeviceBufferRef *counter = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_append.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping append counter tests");
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

	CharString name = CharString_createRefCStrConst("Append counter layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 2, .maxDescriptorTables = 1 };
	name = CharString_createRefCStrConst("Append counter heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Append counter table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Append counter data");

	if(!Test_assert(t, "appendedCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &appended, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Append counter counter");

	if(!Test_assert(t, "counterCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 4, &counter, &t->err
	)))
		goto clean;

	const CharString appendedName = CharString_createRefCStrConst("appended");
	const Descriptor appendDesc = Descriptor_buffer(appended, 0, 0, counter, 0);

	if (isVulkan) {

		//Vulkan's descriptor write reports the counter as unimplemented rather than silently ignoring it,
		// which is what keeps a shader from reading a counter that was never bound

		Test_assert(t, "counterRefusedOnVulkan", !DescriptorTableRef_setDescriptorByName(
			table, &appendedName, 0, false, &appendDesc, NULL
		));

		Test_print(t, "Vulkan has no append/consume counter support yet, refusal pinned instead of executing");
		goto clean;
	}

	if(!Test_assert(t, "setAppended", DescriptorTableRef_setDescriptorByName(
		table, &appendedName, 0, false, &appendDesc, &t->err
	)))
		goto clean;

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Append counter pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Append counter pipeline");

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
		(Transition) { .resource = appended, .stage = EPipelineStage_Compute, .isWrite = true },
		(Transition) { .resource = counter, .stage = EPipelineStage_Compute, .isWrite = true }
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
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, counter)) {

			//One in every four threads appended, so the counter is exactly a quarter of the 64 threads

			const U32 counterValue = *(const U32*) DeviceBufferRef_ptr(counter)->cpuData.ptr;

			Test_assert(t, "counterValue", counterValue == 16);
		}

clean:

	if(table)
		DescriptorTableRef_unsetDescriptors(table, 0, 0, 1, NULL);

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&counter);
	RefPtr_dec(&appended);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);
}

// -- 62. Float atomics, reachable at last through a bindful layout --------------

//AtomicF32 was compile-tested only, because oxc::AtomicAddF32 takes its target by reference and so needs a
// typed float lvalue, while the bindless resource set exposes nothing but RWByteAddressBuffer. A bindful
// layout can declare RWStructuredBuffer<float> at a classic register, which is exactly the missing piece.
//The extension has no DXIL intrinsic (ESHExtension_NoDxilCompile), so only a SPIRV backend has an
// entrypoint to run; the module skips wherever that binary doesn't exist rather than failing.

static void TestBindful_atomicFloatWithWidth(Test *t, GraphicsDeviceRef *deviceRef, Bool isDouble) {

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *output = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };

	const C8 *path =
		isDouble ?
		"//OxC3_gtest/test_shaders/test_bindful_atomic_f64.oiSH" :
		"//OxC3_gtest/test_shaders/test_bindful_atomic_f32.oiSH";

	if (!TestShaders_loadFile(t, path, &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping float atomic tests");
		return;
	}

	const U32 entryId = TestShaders_entry(t, deviceRef, &file, "main");

	if (entryId == U32_MAX) {
		Test_print(t, "Float atomics have no DXIL intrinsic, so this backend has no entrypoint to run");
		goto clean;
	}

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &file, entryId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	CharString name = CharString_createRefCStrConst("Float atomic layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 1, .maxDescriptorTables = 1 };
	name = CharString_createRefCStrConst("Float atomic heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Float atomic table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	//The buffer starts zeroed, so the accumulated total is exactly the thread count

	F64 initial[64] = { 0 };
	const U64 elemSize = isDouble ? sizeof(F64) : sizeof(F32);
	Buffer initialRef = Buffer_createRefConst(initial, elemSize * 64);
	name = CharString_createRefCStrConst("Float atomic output");

	if(!Test_assert(t, "outputCreate", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, &initialRef, &output, &t->err
	)))
		goto clean;

	const Descriptor outputDesc = Descriptor_buffer(output, 0, 0, NULL, 0);
	const CharString bufName = CharString_createRefCStrConst("buf");

	Test_assert(t, "setOutput", DescriptorTableRef_setDescriptorByName(table, &bufName, 0, false, &outputDesc, &t->err));

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Float atomic pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Float atomic pipeline");

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
	Test_assert(t, "bindTable", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
	Test_assert(t, "dispatch", CommandListRef_dispatch1D(commandList, 1, &t->err));
	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			//64 threads each add one, and a lost update lands short: the sum is exact at either width

			const void *ptr = DeviceBufferRef_ptr(output)->cpuData.ptr;
			const F64 total = isDouble ? *(const F64*) ptr : (F64) *(const F32*) ptr;

			Test_assert(t, "atomicFloatTotal", total == 64.0);
		}

clean:

	if(table)
		DescriptorTableRef_unsetDescriptors(table, 0, 0, 1, NULL);

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&output);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);
}

//AtomicF32 and AtomicF64 are separate device capabilities and separate SPIRV capabilities, so each is run
//only where it is claimed; neither has a DXIL intrinsic, so a DXIL backend simply has no entrypoint.

void Test_graphicsBindfulAtomicFloat(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/atomicFloat");

	const GraphicsDeviceCapabilities caps = GraphicsDeviceRef_ptr(deviceRef)->info.capabilities;

	if(caps.dataTypes & EGraphicsDataTypes_AtomicF32)
		TestBindful_atomicFloatWithWidth(t, deviceRef, false);

	else Test_print(t, "Device lacks 32 bit float atomics, skipping that width");

	if(caps.dataTypes & EGraphicsDataTypes_AtomicF64)
		TestBindful_atomicFloatWithWidth(t, deviceRef, true);

	else Test_print(t, "Device lacks 64 bit float atomics, skipping that width");
}
