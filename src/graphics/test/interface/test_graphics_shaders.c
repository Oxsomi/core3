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

//graphics/test/interface/test_graphics_shaders.c

//Shader execution modules (31, 32, 33): pipelines built from the //OxC3_gtest test shaders really dispatch,
// draw and trace, and every result is read back and byte compared.
//Everything before these only proved recording and replay survive; these prove shaders compute the right values.
//The modules gate on the device's default bindless table, since the shaders reach their resources through it.

#include "graphics/generic/instance.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_info.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/render_texture.h"
#include "graphics/generic/depth_stencil.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/bindless_descriptor.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/commands.h"
#include "platforms/platform.h"
#include "platforms/file.h"
#include "formats/oiSH/sh_file.h"
#include "formats/oiSP/sp_file.h"
#include "graphics/generic/pipeline_serialize.h"
#include "types/test/test.h"
#include "types/container/memory_stream.h"
#include "types/container/texture_format.h"
#include "types/container/buffer.h"
#include "types/base/string_base.h"
#include "test_graphics_shared.h"

//One app data layout shared by every test shader, so a single submit can feed mixed pipelines.
//handles[0] = output buffer, handles[1] = base value or TLAS, handles[2] = indirect argument buffer.
//color is what the pixel shaders return, read as F32x4 at U32 offset 4.

typedef struct TestShaderAppData {
	U32 handles[4];
	F32 color[4];
} TestShaderAppData;

//The gtest section only holds compiled oiSH files when the build had the shader compiler, so absence means skip.
//Loading the section twice is harmless, which keeps every module self contained.

static Bool TestShaders_loadFile(Test *t, const C8 *pathStr, SHFile *file) {

	const Allocator *alloc = Platform_instance->alloc;

	const CharString section = CharString_createRefCStrConst("//OxC3_gtest");
	const CharString path = CharString_createRefCStrConst(pathStr);
	const RefPtrType memStreamType = MemoryStream_makeType(alloc);
	const RefPtrType fileHandleType = FileHandle_makeType(alloc);

	File_loadVirtual(&section, &memStreamType, NULL, NULL, alloc, NULL);

	if(!File_hasFile(&path, alloc))
		return false;

	Buffer data = Buffer_createNull();
	MemoryStreamRef *stream = NULL;
	Bool loaded = false;

	if(!Test_assert(t, "readShaderFile", File_read(&path, U64_MAX, 0, 0, &fileHandleType, &data, &t->err)))
		return false;

	U64 streamOffset = 0;

	if(Test_assert(t, "createShaderStream", MemoryStream_createFromBufferRegion(
		Buffer_createRefFromBuffer(data, true), 0, Buffer_length(data), EMemoryStreamFlags_None,
		&memStreamType, &stream, &t->err
	)) && stream)
		loaded = Test_assert(t, "parseShaderFile", SHFile_read(
			(StreamRef*) stream, &streamOffset, false, alloc, file, &t->err
		));

	RefPtr_dec(&stream);
	Buffer_free(&data, alloc);

	return loaded;
}

//Every test shader file holds a single stage; only the raytracing file carries more than one named entry

static U32 TestShaders_entry(Test *t, GraphicsDeviceRef *deviceRef, const SHFile *file, const C8 *name) {

	const CharString entryName = CharString_createRefCStrConst(name);

	const U32 id = GraphicsDeviceRef_getFirstShaderEntry(
		deviceRef, file, &entryName, NULL, NULL, ESHExtension_None, ESHExtension_None
	);

	Test_assert(t, "entryFound", id != U32_MAX);
	return id;
}

static Bool TestShaders_computePipeline(
	Test *t, GraphicsDeviceRef *deviceRef, const SHFile *file, PipelineRef **pipeline
) {

	const U32 id = TestShaders_entry(t, deviceRef, file, "main");

	if(id == U32_MAX)
		return false;

	const CharString entryName = CharString_createRefCStrConst("main");
	const CharString name = CharString_createRefCStrConst("Shader test compute pipeline");

	return Test_assert(t, "createComputePipeline", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, file, &name, id, &entryName, EPipelineFlags_None, NULL, pipeline, &t->err
	));
}

//The vertex and pixel shader are separate single entry files, picked out of one shared file list by slot

static Bool TestShaders_graphicsPipeline(
	Test *t, GraphicsDeviceRef *deviceRef, const ListSHFile *files, U16 vertexFile, U16 pixelFile,
	const PipelineGraphicsInfo *info, PipelineRef **pipeline
) {

	const U32 vertexId = TestShaders_entry(t, deviceRef, &files->ptr[vertexFile], "main");
	const U32 pixelId = TestShaders_entry(t, deviceRef, &files->ptr[pixelFile], "main");

	if(vertexId == U32_MAX || pixelId == U32_MAX)
		return false;

	PipelineStage stages[2] = {
		(PipelineStage) { .binaryId = vertexId, .shFileId = vertexFile },
		(PipelineStage) { .binaryId = pixelId, .shFileId = pixelFile }
	};

	ListPipelineStage stageList = (ListPipelineStage) { 0 };
	ListPipelineStage_createRefConst(stages, 2, &stageList, NULL);

	const CharString name = CharString_createRefCStrConst("Shader test graphics pipeline");

	return Test_assert(t, "createGraphicsPipeline", GraphicsDeviceRef_createPipelineGraphics(
		deviceRef, files, &stageList, info, &name, EPipelineFlags_None, NULL, pipeline, &t->err
	));
}

static Bool TestShaders_submitAndWait(
	Test *t, GraphicsDeviceRef *deviceRef, CommandListRef *commandList, const void *appData, U64 appDataLen
) {

	ListCommandListRef lists = (ListCommandListRef) { 0 };
	ListCommandListRef_createRefConst(&commandList, 1, &lists, NULL);

	const Buffer appDataBuf = appData ? Buffer_createRefConst(appData, appDataLen) : Buffer_createNull();

	const Bool ok = Test_assert(t, "submit", GraphicsDeviceRef_submitCommands(
		deviceRef, &lists, NULL, appData ? &appDataBuf : NULL, 0, 0, &t->err
	));

	return Test_assert(t, "wait", GraphicsDeviceRef_wait(deviceRef, &t->err)) && ok;
}

static void TestShaders_countPull(RefPtr *resource, void *context) {
	(void) resource;
	++*(U32*)context;
}

//Scribbling the CPU copy first makes sure only a real GPU to CPU pull can produce the expected values

static Bool TestShaders_pullBuffer(Test *t, GraphicsDeviceRef *deviceRef, CommandListRef *emptyList, DeviceBufferRef *buffer) {

	DeviceBuffer *bufferPtr = DeviceBufferRef_ptr(buffer);

	for(U64 i = 0; i < Buffer_length(bufferPtr->cpuData); ++i)
		bufferPtr->cpuData.ptrNonConst[i] = 0xCC;

	U32 pulled = 0;

	Bool ok = Test_assert(t, "queueBufferPull", DeviceBufferRef_pullRegion(
		buffer, 0, 0, TestShaders_countPull, &pulled, &t->err
	));

	ok &= TestShaders_submitAndWait(t, deviceRef, emptyList, NULL, 0);
	return Test_assert(t, "bufferPullCompleted", pulled == 1) && ok;
}

//Texture pulls hand over an owned buffer; the callback copies out the 8x8 payload the checks below compare

typedef struct TestShaderPixels {
	U32 count;
	U32 padding;
	U64 len;
	U32 pixels[64];
} TestShaderPixels;

static void TestShaders_pixelPull(RefPtr *resource, Buffer *data, void *context) {

	(void) resource;

	TestShaderPixels *result = (TestShaderPixels*) context;

	++result->count;
	result->len = data ? Buffer_length(*data) : 0;

	for(U64 i = 0; i < 64 && (i + 1) * 4 <= result->len; ++i)
		result->pixels[i] = ((const U32*)data->ptr)[i];
}

static Bool TestShaders_pullPixels(
	Test *t, GraphicsDeviceRef *deviceRef, CommandListRef *emptyList, RefPtr *target, TestShaderPixels *pixels
) {

	Bool ok = Test_assert(t, "queuePixelPull", TextureRef_pullRegion(
		target, 0, 0, 0, 0, 0, 0, TestShaders_pixelPull, pixels, &t->err
	));

	ok &= TestShaders_submitAndWait(t, deviceRef, emptyList, NULL, 0);
	ok &= Test_assert(t, "pixelPullCompleted", pixels->count == 1);
	return Test_assert(t, "pixelPullLen", pixels->len == 64 * 4) && ok;
}

static Bool TestShaders_checkPixels(
	Test *t, GraphicsDeviceRef *deviceRef, CommandListRef *emptyList, RefPtr *target, U32 expected
) {

	TestShaderPixels pixels = (TestShaderPixels) { 0 };

	if(!TestShaders_pullPixels(t, deviceRef, emptyList, target, &pixels))
		return false;

	U32 matching = 0;

	for(U64 i = 0; i < 64; ++i)
		matching += pixels.pixels[i] == expected;

	return Test_assert(t, "pixelsMatch", matching == 64);
}

//Opens a scope, starts a cleared render into the 8x8 target and binds the pipeline with full viewport and scissor.
//The caller already began the command list, so a compute scope can precede the render scope when a phase needs one.

static Bool TestShaders_openDraw(
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

static Bool TestShaders_closeDraw(Test *t, CommandListRef *commandList) {
	Bool ok = Test_assert(t, "renderEnd", CommandListRef_endRenderExt(commandList, &t->err));
	ok &= Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	return Test_assert(t, "end", CommandListRef_end(commandList, &t->err)) && ok;
}

// -- 31. Compute execution -------------------------------------------------------

//A 64 thread dispatch writes base + thread id per slot, so the readback proves every thread really ran.
//The same shader then runs through dispatchIndirect twice: once from CPU written arguments at an aligned
// offset and once from arguments another dispatch wrote on the GPU, which is the full indirect pipeline.

void Test_graphicsShaderCompute(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Shaders/compute");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!device->defaultDescriptorTable) {
		Test_print(t, "Device has no bindless descriptor table, skipping compute execution tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	SHFile writeFile = (SHFile) { 0 };
	SHFile argsFile = (SHFile) { 0 };

	if (
		!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_write.oiSH", &writeFile) ||
		!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_write_args.oiSH", &argsFile)
	) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping compute execution tests");
		SHFile_free(&writeFile, alloc);
		return;
	}

	DeviceBufferRef *output = NULL;
	DeviceBufferRef *cpuArgs = NULL;
	DeviceBufferRef *gpuArgs = NULL;
	PipelineRef *pipelineWrite = NULL;
	PipelineRef *pipelineArgs = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	//128 slots, so the GPU written two group dispatch at the end has room for both groups

	CharString name = CharString_createRefCStrConst("Shader compute output");

	Test_assert(t, "createOutput", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWriteBindless | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 128 * sizeof(U32), &output, &t->err
	));

	if(!output || !TestShaders_computePipeline(t, deviceRef, &writeFile, &pipelineWrite))
		goto clean;

	if(!Test_assert(t, "createList", GraphicsDeviceRef_createCommandList(
		deviceRef, 4 * KIBI, 64, 16, true, &commandList, &t->err
	)))
		goto clean;

	//An empty list completes pulls without re-running work, since replaying a dispatch with NULL app data
	// would make the shader index descriptor 0 instead of the real output buffer

	if(!Test_assert(t, "createEmptyList", GraphicsDeviceRef_createCommandList(
		deviceRef, KIBI, 16, 8, true, &emptyList, &t->err
	)))
		goto clean;

	Test_assert(t, "beginEmptyList", CommandListRef_begin(emptyList, true, U64_MAX, &t->err));
	Test_assert(t, "endEmptyList", CommandListRef_end(emptyList, &t->err));

	const Transition outputWrite = (Transition) {
		.resource = output, .stage = EPipelineStage_Compute, .isWrite = true
	};

	ListTransition outputTransition = (ListTransition) { 0 };
	ListTransition_createRefConst(&outputWrite, 1, &outputTransition, NULL);

	DeviceBuffer *outputPtr = DeviceBufferRef_ptr(output);
	const U32 *values = (const U32*) outputPtr->cpuData.ptr;

	//Direct dispatch

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "scope", CommandListRef_startScope(commandList, &outputTransition, 1, NULL, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipelineWrite, &t->err));
	Test_assert(t, "dispatch", CommandListRef_dispatch1D(commandList, 1, &t->err));
	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	TestShaderAppData appData = (TestShaderAppData) {
		.handles = { outputPtr->writeHandle, 0xC0DE0000u }
	};

	if(!TestShaders_submitAndWait(t, deviceRef, commandList, &appData, sizeof(appData)))
		goto clean;

	if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

		U32 matching = 0;

		for(U32 i = 0; i < 64; ++i)
			matching += values[i] == 0xC0DE0000u + i;

		Test_assert(t, "directDispatchValues", matching == 64);
	}

	//Indirect dispatch from CPU written arguments; the nonzero offset checks aligned addressing into the buffer

	const U32 cpuArgsData[8] = { 0, 0, 0, 0, 1, 1, 1, 0 };
	Buffer cpuArgsRef = Buffer_createRefConst(cpuArgsData, sizeof(cpuArgsData));

	name = CharString_createRefCStrConst("Shader compute indirect args");

	Test_assert(t, "createCpuArgs", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_Indirect, EGraphicsResourceFlag_None, NULL,
		&name, &cpuArgsRef, &cpuArgs, &t->err
	));

	if (cpuArgs) {

		Test_assert(t, "beginIndirect", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
		Test_assert(t, "scopeIndirect", CommandListRef_startScope(commandList, &outputTransition, 1, NULL, &t->err));
		Test_assert(t, "bindIndirect", CommandListRef_setComputePipeline(commandList, pipelineWrite, &t->err));
		Test_assert(t, "dispatchIndirect", CommandListRef_dispatchIndirect(commandList, cpuArgs, 16, &t->err));
		Test_assert(t, "scopeIndirectEnd", CommandListRef_endScope(commandList, &t->err));
		Test_assert(t, "endIndirect", CommandListRef_end(commandList, &t->err));

		appData.handles[1] = 0xD15C0000u;

		if(
			TestShaders_submitAndWait(t, deviceRef, commandList, &appData, sizeof(appData)) &&
			TestShaders_pullBuffer(t, deviceRef, emptyList, output)
		) {

			U32 matching = 0;

			for(U32 i = 0; i < 64; ++i)
				matching += values[i] == 0xD15C0000u + i;

			Test_assert(t, "cpuIndirectValues", matching == 64);
		}
	}

	//Indirect dispatch from GPU written arguments: one dispatch writes { 2, 1, 1 }, the next consumes it,
	// so 128 threads have to land and the argument buffer is proven writable and consumable in one submit

	name = CharString_createRefCStrConst("Shader compute GPU args");

	Test_assert(t, "createGpuArgs", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_Indirect,
		EGraphicsResourceFlag_ShaderWriteBindless,
		NULL, &name, 32, &gpuArgs, &t->err
	));

	if (gpuArgs && TestShaders_computePipeline(t, deviceRef, &argsFile, &pipelineArgs)) {

		const Transition argsWrite = (Transition) {
			.resource = gpuArgs, .stage = EPipelineStage_Compute, .isWrite = true
		};

		ListTransition argsTransition = (ListTransition) { 0 };
		ListTransition_createRefConst(&argsWrite, 1, &argsTransition, NULL);

		Test_assert(t, "beginGpu", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		Test_assert(t, "scopeArgs", CommandListRef_startScope(commandList, &argsTransition, 1, NULL, &t->err));
		Test_assert(t, "bindArgs", CommandListRef_setComputePipeline(commandList, pipelineArgs, &t->err));
		Test_assert(t, "dispatchArgs", CommandListRef_dispatch1D(commandList, 1, &t->err));
		Test_assert(t, "scopeArgsEnd", CommandListRef_endScope(commandList, &t->err));

		Test_assert(t, "scopeGpu", CommandListRef_startScope(commandList, &outputTransition, 2, NULL, &t->err));
		Test_assert(t, "bindGpu", CommandListRef_setComputePipeline(commandList, pipelineWrite, &t->err));
		Test_assert(t, "dispatchGpu", CommandListRef_dispatchIndirect(commandList, gpuArgs, 16, &t->err));
		Test_assert(t, "scopeGpuEnd", CommandListRef_endScope(commandList, &t->err));

		Test_assert(t, "endGpu", CommandListRef_end(commandList, &t->err));

		appData.handles[1] = 0xA53F0000u;
		appData.handles[2] = DeviceBufferRef_ptr(gpuArgs)->writeHandle;

		if(
			TestShaders_submitAndWait(t, deviceRef, commandList, &appData, sizeof(appData)) &&
			TestShaders_pullBuffer(t, deviceRef, emptyList, output)
		) {

			U32 matching = 0;

			for(U32 i = 0; i < 128; ++i)
				matching += values[i] == 0xA53F0000u + i;

			Test_assert(t, "gpuIndirectValues", matching == 128);
		}
	}

clean:

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipelineArgs);
	RefPtr_dec(&pipelineWrite);
	RefPtr_dec(&gpuArgs);
	RefPtr_dec(&cpuArgs);
	RefPtr_dec(&output);

	SHFile_free(&argsFile, alloc);
	SHFile_free(&writeFile, alloc);
}

// -- 32. Draw execution ----------------------------------------------------------

//Every draw renders into an 8x8 target and the resolved pixels are byte compared, colors picked to be exact
// in 8 bit UNORM so rounding can't blur a pass into a flake.
//Covered here: a fullscreen triangle, scissor clipping, additive blending, an indexed and instanced draw
// through real vertex and index buffers, depth test accept and reject, indirect draws from CPU and GPU
// written arguments and an MSAA 4x draw that resolves into the readback target.

void Test_graphicsShaderDraw(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Shaders/draw");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!device->defaultDescriptorTable) {
		Test_print(t, "Device has no bindless descriptor table, skipping draw execution tests");
		return;
	}

	if (!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering)) {
		Test_print(t, "Device lacks direct rendering, skipping draw execution tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	//One shared file list all graphics pipelines pick their stages from by slot

	static const C8 *drawShaderPaths[] = {
		"//OxC3_gtest/test_shaders/test_draw_vs.oiSH",
		"//OxC3_gtest/test_shaders/test_draw_ps.oiSH",
		"//OxC3_gtest/test_shaders/test_depth_vs.oiSH",
		"//OxC3_gtest/test_shaders/test_depth_ps.oiSH",
		"//OxC3_gtest/test_shaders/test_vertex_vs.oiSH",
		"//OxC3_gtest/test_shaders/test_write_args.oiSH"
	};

	SHFile files[6] = { 0 };
	Bool loadedAll = true;

	for(U64 i = 0; i < 6; ++i)
		loadedAll &= TestShaders_loadFile(t, drawShaderPaths[i], &files[i]);

	if (!loadedAll) {

		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping draw execution tests");

		for(U64 i = 0; i < 6; ++i)
			SHFile_free(&files[i], alloc);

		return;
	}

	ListSHFile fileList = (ListSHFile) { 0 };
	ListSHFile_createRefConst(files, 6, &fileList, NULL);

	RenderTextureRef *target = NULL;
	RenderTextureRef *msaaTarget = NULL;
	DepthStencilRef *depth = NULL;
	DeviceBufferRef *vertexBuffer = NULL;
	DeviceBufferRef *indexBuffer = NULL;
	DeviceBufferRef *cpuArgs = NULL;
	DeviceBufferRef *gpuArgs = NULL;
	PipelineRef *flatPipeline = NULL;
	PipelineRef *blendPipeline = NULL;
	PipelineRef *vertexPipeline = NULL;
	PipelineRef *depthPipeline = NULL;
	PipelineRef *msaaPipeline = NULL;
	PipelineRef *argsPipeline = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	CharString name = CharString_createRefCStrConst("Shader draw target");

	Test_assert(t, "createTarget", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &name, &target, &t->err
	));

	const PipelineGraphicsInfo flatInfo = (PipelineGraphicsInfo) {
		.attachmentCountExt = 1,
		.attachmentFormatsExt = { ETextureFormatId_RGBA8 }
	};

	if(!target || !TestShaders_graphicsPipeline(t, deviceRef, &fileList, 0, 1, &flatInfo, &flatPipeline))
		goto clean;

	if(!Test_assert(t, "createList", GraphicsDeviceRef_createCommandList(
		deviceRef, 8 * KIBI, 128, 32, true, &commandList, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "createEmptyList", GraphicsDeviceRef_createCommandList(
		deviceRef, KIBI, 16, 8, true, &emptyList, &t->err
	)))
		goto clean;

	Test_assert(t, "beginEmptyList", CommandListRef_begin(emptyList, true, U64_MAX, &t->err));
	Test_assert(t, "endEmptyList", CommandListRef_end(emptyList, &t->err));

	//Fullscreen triangle: every pixel has to hold the app data color exactly

	Test_assert(t, "beginFlat", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

	TestShaderAppData appData = (TestShaderAppData) { .color = { 1, 102.f / 255, 51.f / 255, 1 } };

	if (TestShaders_openDraw(t, commandList, 1, target, flatPipeline)) {

		Test_assert(t, "draw", CommandListRef_drawUnindexed(commandList, 3, 1, &t->err));

		if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
			t, deviceRef, commandList, &appData, sizeof(appData)
		))
			TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF3366FFu);
	}

	//Scissor: the draw only lands on the left half, the right half keeps the clear

	Test_assert(t, "beginScissor", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

	appData.color[0] = 0; appData.color[1] = 204.f / 255; appData.color[2] = 0;

	if (TestShaders_openDraw(t, commandList, 1, target, flatPipeline)) {

		Test_assert(t, "scissorHalf", CommandListRef_setScissor(
			commandList, I32x2_zero, I32x2_create2(4, 8), &t->err
		));

		Test_assert(t, "drawScissor", CommandListRef_drawUnindexed(commandList, 3, 1, &t->err));

		if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
			t, deviceRef, commandList, &appData, sizeof(appData)
		)) {

			TestShaderPixels pixels = (TestShaderPixels) { 0 };

			if (TestShaders_pullPixels(t, deviceRef, emptyList, target, &pixels)) {

				U32 matching = 0;

				for(U64 i = 0; i < 64; ++i)
					matching += pixels.pixels[i] == ((i & 7) < 4 ? 0xFF00CC00u : 0u);

				Test_assert(t, "scissorPixels", matching == 64);
			}
		}
	}

	//Additive blend: two fullscreen instances of the same color have to sum to exactly twice the bytes

	PipelineGraphicsInfo blendInfo = flatInfo;
	blendInfo.blendState = (BlendState) {
		.enable = true,
		.renderTargetMask = 1,
		.writeMask = { EWriteMask_All },
		.attachments = { (BlendStateAttachment) {
			.srcBlend = EBlend_One, .dstBlend = EBlend_One,
			.srcBlendAlpha = EBlend_One, .dstBlendAlpha = EBlend_One,
			.blendOp = EBlendOp_Add, .blendOpAlpha = EBlendOp_Add
		} }
	};

	if (TestShaders_graphicsPipeline(t, deviceRef, &fileList, 0, 1, &blendInfo, &blendPipeline)) {

		Test_assert(t, "beginBlend", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		appData.color[0] = 51.f / 255; appData.color[1] = 102.f / 255; appData.color[2] = 0; appData.color[3] = 51.f / 255;

		if (TestShaders_openDraw(t, commandList, 1, target, blendPipeline)) {

			Test_assert(t, "drawBlend", CommandListRef_drawUnindexed(commandList, 3, 2, &t->err));

			if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
				t, deviceRef, commandList, &appData, sizeof(appData)
			))
				TestShaders_checkPixels(t, deviceRef, emptyList, target, 0x6600CC66u);
		}
	}

	//Indexed and instanced draw through real buffers: instance 0 covers the left half, instance 1 the right,
	// so full coverage proves the index buffer, the vertex fetch and both instances all worked.
	//This is also the only path that replays setPrimitiveBuffers, which used to record 8 bytes of pointer
	// instead of the command payload.

	const F32 quad[8] = { -1, -1, 1, -1, -1, 1, 1, 1 };
	const U16 quadIndices[6] = { 0, 1, 2, 2, 1, 3 };

	Buffer dataRef = Buffer_createRefConst(quad, sizeof(quad));
	name = CharString_createRefCStrConst("Shader draw vertices");

	Test_assert(t, "createVertexBuffer", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_Vertex, EGraphicsResourceFlag_None, NULL,
		&name, &dataRef, &vertexBuffer, &t->err
	));

	dataRef = Buffer_createRefConst(quadIndices, sizeof(quadIndices));
	name = CharString_createRefCStrConst("Shader draw indices");

	Test_assert(t, "createIndexBuffer", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_Index, EGraphicsResourceFlag_None, NULL,
		&name, &dataRef, &indexBuffer, &t->err
	));

	PipelineGraphicsInfo vertexInfo = flatInfo;
	vertexInfo.vertexLayout.bufferStrides12_isInstance1[0] = sizeof(F32) * 2;
	vertexInfo.vertexLayout.attributes[0] = (VertexAttribute) { .format = ETextureFormatId_RG32f };

	if (
		vertexBuffer && indexBuffer &&
		TestShaders_graphicsPipeline(t, deviceRef, &fileList, 4, 1, &vertexInfo, &vertexPipeline)
	) {

		Test_assert(t, "beginVertex", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		appData.color[0] = 204.f / 255; appData.color[1] = 0; appData.color[2] = 204.f / 255; appData.color[3] = 1;

		if (TestShaders_openDraw(t, commandList, 1, target, vertexPipeline)) {

			SetPrimitiveBuffersCmd primitives = (SetPrimitiveBuffersCmd) { 0 };
			primitives.vertexBuffers[0] = vertexBuffer;
			primitives.indexBuffer = indexBuffer;
			primitives.isIndex32Bit = false;

			Test_assert(t, "setPrimitiveBuffers", CommandListRef_setPrimitiveBuffers(commandList, &primitives, &t->err));
			Test_assert(t, "drawIndexed", CommandListRef_drawIndexed(commandList, 6, 2, &t->err));

			if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
				t, deviceRef, commandList, &appData, sizeof(appData)
			))
				TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFFCC00CCu);
		}
	}

	//Depth story in one draw: a far triangle writes, a nearer one passes, the farthest one after it must be
	// rejected, so the survivor's color and its exact depth prove both accept and reject paths

	name = CharString_createRefCStrConst("Shader draw depth");

	Test_assert(t, "createDepth", GraphicsDeviceRef_createDepthStencil(
		deviceRef, 8, 8, EDepthStencilFormat_D32, false, EMSAASamples_Off, NULL, &name, &depth, &t->err
	));

	//Reverse Z is an app convention (fold 1 - z into the projection); the viewport is a plain 0..1 range
	// on every backend, so the test shader outputs reversed z directly: near stores the higher value,
	// the far clear is 0 and the compare is Greater

	PipelineGraphicsInfo depthInfo = flatInfo;
	depthInfo.depthStencil = (DepthStencilState) { .flags = EDepthStencilFlags_DepthWrite, .depthCompare = ECompareOp_Gt };
	depthInfo.depthFormatExt = EDepthStencilFormat_D32;

	if (
		depth &&
		TestShaders_graphicsPipeline(t, deviceRef, &fileList, 2, 3, &depthInfo, &depthPipeline)
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

		if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0)) {

			//The middle triangle's green and its 0.7 depth, straight from the shader with no viewport
			// remap in between; a small tolerance stays anyway for the wider GPU test rig

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

	//Indirect draw from CPU written arguments

	const U32 drawArgs[4] = { 3, 1, 0, 0 };
	dataRef = Buffer_createRefConst(drawArgs, sizeof(drawArgs));
	name = CharString_createRefCStrConst("Shader draw indirect args");

	Test_assert(t, "createDrawArgs", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_Indirect, EGraphicsResourceFlag_None, NULL,
		&name, &dataRef, &cpuArgs, &t->err
	));

	if (cpuArgs) {

		Test_assert(t, "beginIndirect", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		appData.color[0] = 1; appData.color[1] = 1; appData.color[2] = 0; appData.color[3] = 1;

		if (TestShaders_openDraw(t, commandList, 1, target, flatPipeline)) {

			Test_assert(t, "drawIndirect", CommandListRef_drawIndirect(commandList, cpuArgs, 0, 1, false, &t->err));

			if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
				t, deviceRef, commandList, &appData, sizeof(appData)
			))
				TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF00FFFFu);
		}
	}

	//Indirect draw from GPU written arguments: a compute scope writes { 3 vertices, 2 instances } and the
	// render scope consumes it in the same submit

	name = CharString_createRefCStrConst("Shader draw GPU args");

	Test_assert(t, "createGpuDrawArgs", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_Indirect,
		EGraphicsResourceFlag_ShaderWriteBindless,
		NULL, &name, 32, &gpuArgs, &t->err
	));

	if (gpuArgs && TestShaders_computePipeline(t, deviceRef, &files[5], &argsPipeline)) {

		const Transition argsWrite = (Transition) {
			.resource = gpuArgs, .stage = EPipelineStage_Compute, .isWrite = true
		};

		ListTransition argsTransition = (ListTransition) { 0 };
		ListTransition_createRefConst(&argsWrite, 1, &argsTransition, NULL);

		Test_assert(t, "beginGpuDraw", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		Test_assert(t, "scopeGpuArgs", CommandListRef_startScope(commandList, &argsTransition, 1, NULL, &t->err));
		Test_assert(t, "bindGpuArgs", CommandListRef_setComputePipeline(commandList, argsPipeline, &t->err));
		Test_assert(t, "dispatchGpuArgs", CommandListRef_dispatch1D(commandList, 1, &t->err));
		Test_assert(t, "scopeGpuArgsEnd", CommandListRef_endScope(commandList, &t->err));

		appData.handles[2] = DeviceBufferRef_ptr(gpuArgs)->writeHandle;
		appData.color[0] = 51.f / 255; appData.color[1] = 51.f / 255; appData.color[2] = 1; appData.color[3] = 1;

		if (TestShaders_openDraw(t, commandList, 2, target, flatPipeline)) {

			Test_assert(t, "drawGpuIndirect", CommandListRef_drawIndirect(commandList, gpuArgs, 0, 1, false, &t->err));

			if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
				t, deviceRef, commandList, &appData, sizeof(appData)
			))
				TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFFFF3333u);
		}
	}

	//MSAA 4x: a fully covered pixel resolves to exactly the flat color, and the result lands in the
	// readback target through the resolve attachment rather than a copy

	name = CharString_createRefCStrConst("Shader draw MSAA target");

	Test_assert(t, "createMsaaTarget", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_x4, NULL, &name, &msaaTarget, &t->err
	));

	PipelineGraphicsInfo msaaInfo = flatInfo;
	msaaInfo.msaa = EMSAASamples_x4;

	if (
		msaaTarget &&
		TestShaders_graphicsPipeline(t, deviceRef, &fileList, 0, 1, &msaaInfo, &msaaPipeline)
	) {

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

		appData.color[0] = 102.f / 255; appData.color[1] = 1; appData.color[2] = 51.f / 255; appData.color[3] = 1;

		if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
			t, deviceRef, commandList, &appData, sizeof(appData)
		))
			TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF33FF66u);
	}

clean:

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&argsPipeline);
	RefPtr_dec(&msaaPipeline);
	RefPtr_dec(&depthPipeline);
	RefPtr_dec(&vertexPipeline);
	RefPtr_dec(&blendPipeline);
	RefPtr_dec(&flatPipeline);
	RefPtr_dec(&gpuArgs);
	RefPtr_dec(&cpuArgs);
	RefPtr_dec(&indexBuffer);
	RefPtr_dec(&vertexBuffer);
	RefPtr_dec(&depth);
	RefPtr_dec(&msaaTarget);
	RefPtr_dec(&target);

	for(U64 i = 0; i < 6; ++i)
		SHFile_free(&files[i], alloc);
}

// -- 33. Named entrypoint graphics pipeline --------------------------------------

//A graphics pipeline whose vertex and pixel shaders do NOT name their entrypoint "main".
//The compiler renames a non lib module's sole entrypoint to "main" in the SPIR-V while reflection keeps the
// original name ("mainVS"/"mainPS"), so pipeline creation has to look up "main" and not the reflected name.
//Only creation is exercised, since that is exactly where the wrong name is rejected by validation.

void Test_graphicsShaderNamedEntry(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Shaders/namedEntry");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!device->defaultDescriptorTable) {
		Test_print(t, "Device has no bindless descriptor table, skipping named entrypoint test");
		return;
	}

	if (!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering)) {
		Test_print(t, "Device lacks direct rendering, skipping named entrypoint test");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	SHFile files[2] = { 0 };

	const Bool loadedAll =
		TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_named_vs.oiSH", &files[0]) &
		TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_named_ps.oiSH", &files[1]);

	if (!loadedAll) {

		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping named entrypoint test");

		for(U64 i = 0; i < 2; ++i)
			SHFile_free(&files[i], alloc);

		return;
	}

	ListSHFile fileList = (ListSHFile) { 0 };
	ListSHFile_createRefConst(files, 2, &fileList, NULL);

	PipelineRef *pipeline = NULL;

	const U32 vertexId = TestShaders_entry(t, deviceRef, &files[0], "mainVS");
	const U32 pixelId = TestShaders_entry(t, deviceRef, &files[1], "mainPS");

	if (vertexId != U32_MAX && pixelId != U32_MAX) {

		PipelineStage stages[2] = {
			(PipelineStage) { .binaryId = vertexId, .shFileId = 0 },
			(PipelineStage) { .binaryId = pixelId, .shFileId = 1 }
		};

		ListPipelineStage stageList = (ListPipelineStage) { 0 };
		ListPipelineStage_createRefConst(stages, 2, &stageList, NULL);

		const PipelineGraphicsInfo info = (PipelineGraphicsInfo) {
			.attachmentCountExt = 1,
			.attachmentFormatsExt = { ETextureFormatId_RGBA8 }
		};

		const CharString name = CharString_createRefCStrConst("Named entrypoint graphics pipeline");

		//A named entrypoint is looked up under the name the SPIR-V module actually kept, which the compile path
		// may have renamed to "main", rather than under the HLSL name the oiSH records.

		Test_assert(t, "createNamedEntryPipeline", GraphicsDeviceRef_createPipelineGraphics(
			deviceRef, &fileList, &stageList, &info, &name, EPipelineFlags_None, NULL, &pipeline, &t->err
		));
	}

	RefPtr_dec(&pipeline);

	for(U64 i = 0; i < 2; ++i)
		SHFile_free(&files[i], alloc);
}

// -- 34. Ray trace execution -----------------------------------------------------

//The same one triangle scene the AS module builds, but with the bindless descriptor enabled so a real
// raytracing pipeline can fetch the TLAS and trace against it.
//Two rays start above the triangle's interior and two outside, so the readback distinguishes the closest
// hit and miss shaders actually running from any default.

//A live pipeline dumps to an oiSP that's exact, lowers back to the state it was created with, and rebuilds.

void Test_graphicsShaderPipelineSerialize(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Shaders/pipeline serialize (oiSP)");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!device->defaultDescriptorTable) {
		Test_print(t, "Device has no bindless descriptor table, skipping pipeline serialize tests");
		return;
	}

	if (!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering)) {
		Test_print(t, "Device lacks direct rendering, skipping pipeline serialize tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	SHFile files[2] = { 0 };
	ListSHFile fileList = (ListSHFile) { 0 };
	ListCharString names = (ListCharString) { 0 };
	PipelineRef *pipeline = NULL;
	PipelineRef *rebuilt = NULL;
	SPFile sp = (SPFile) { 0 };
	U32 pipelineId = U32_MAX;

	const CharString nameArr[2] = {
		CharString_createRefCStrConst("test_draw_vs.oiSH"), CharString_createRefCStrConst("test_draw_ps.oiSH")
	};

	if(
		!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_draw_vs.oiSH", &files[0]) ||
		!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_draw_ps.oiSH", &files[1])
	)
		goto clean;

	ListSHFile_createRefConst(files, 2, &fileList, NULL);
	ListCharString_createRefConst(nameArr, 2, &names, NULL);

	//State with enough non default values that a lossy round trip would show

	PipelineGraphicsInfo info = (PipelineGraphicsInfo) {
		.attachmentCountExt = 1,
		.attachmentFormatsExt = { ETextureFormatId_RGBA8 },
		.rasterizer = (Rasterizer) { .cullMode = ECullMode_None },
		.blendState = (BlendState) {
			.enable = true, .renderTargetMask = 1, .writeMask = { EWriteMask_All },
			.attachments = { (BlendStateAttachment) {
				.srcBlend = EBlend_One, .dstBlend = EBlend_One, .srcBlendAlpha = EBlend_One, .dstBlendAlpha = EBlend_One,
				.blendOp = EBlendOp_Add, .blendOpAlpha = EBlendOp_Add
			} }
		}
	};

	if(!TestShaders_graphicsPipeline(t, deviceRef, &fileList, 0, 1, &info, &pipeline))
		goto clean;

	if(!Test_assert(t, "createSP", SPFile_create(ESPSettingsFlags_None, alloc, &sp, &t->err)))
		goto clean;

	if(!Test_assert(t, "dump", Pipeline_toSPFile(
		pipeline, &fileList, &names, CharString_createRefCStrConst("draw"), alloc, &sp, &pipelineId, &t->err
	)))
		goto clean;

	//Dumped state is exact (nothing assumed) and keeps the one declared target rather than growing to eight

	Test_assert(t, "dumpExact", SPFile_isExact(&sp, pipelineId));

	const SPGraphicsState *state = SPFile_graphicsState(&sp, pipelineId);

	if (Test_assert(t, "dumpHasGraphicsState", state != NULL)) {
		Test_assert(t, "dumpTargetCount", state->renderTargetCount == 1);
		Test_assert(t, "dumpTargetFormat", state->renderTargetFormats[0] == (U8) ETextureFormatId_RGBA8);
		Test_assert(t, "dumpBlend", state->blend.enable && state->blend.attachments[0].srcBlend == EBlend_One);
	}

	//Lowering gives back what the pipeline was created with

	PipelineGraphicsInfo back = (PipelineGraphicsInfo) { 0 };

	if (Test_assert(t, "lower", SPFile_toGraphicsInfo(&sp, pipelineId, &back, &t->err))) {
		Test_assert(t, "lowerTargets", back.attachmentCountExt == 1 && back.attachmentFormatsExt[0] == ETextureFormatId_RGBA8);
		Test_assert(t, "lowerRasterizer", back.rasterizer.cullMode == ECullMode_None);
		Test_assert(t, "lowerBlend", back.blendState.enable && back.blendState.renderTargetMask == 1);
		Test_assert(
			t, "lowerAttachment",
			back.blendState.attachments[0].dstBlendAlpha == EBlend_One && back.blendState.attachments[0].blendOp == EBlendOp_Add
		);
		Test_assert(t, "lowerWriteMask", back.blendState.writeMask[0] == EWriteMask_All);
	}

	//And a stored pipeline rebuilds against the same oiSH files, resolved by name

	Test_assert(t, "rebuild", GraphicsDeviceRef_createPipelineFromSPFile(
		deviceRef, &sp, pipelineId, &fileList, &names, NULL, alloc, &rebuilt, &t->err
	));

	Test_assert(t, "rebuilt", rebuilt != NULL);

clean:
	RefPtr_dec(&rebuilt);
	RefPtr_dec(&pipeline);
	SPFile_free(&sp, alloc);
	ListCharString_free(&names, alloc);
	ListSHFile_free(&fileList, alloc);
	SHFile_free(&files[0], alloc);
	SHFile_free(&files[1], alloc);
}

void Test_graphicsShaderRays(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Shaders/rays");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!(device->info.capabilities.features & EGraphicsFeatures_RayPipeline)) {
		Test_print(t, "Device lacks raytracing pipelines, skipping ray trace tests");
		return;
	}

	if (!device->defaultDescriptorTable) {
		Test_print(t, "Device has no bindless descriptor table, skipping ray trace tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	SHFile file = (SHFile) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_rays.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping ray trace tests");
		return;
	}

	//D3D12's GPU based validation instruments raytracing libs into invalid bytecode
	// ("Internal declaration 'GBV_Debug_Resource' is unused"), hardware then hangs executing it and even
	// WARP's surviving run leaves those bytecode errors in the counters, failing the validation clean check.
	//So on D3D12 with it enabled the module traces on its own device with only GPU based validation off,
	// which keeps the coverage and still hard checks that instance's counters at the end.

	const GraphicsInstance *suiteInstance = GraphicsInstanceRef_ptr(device->instance);

	const Bool gpuValidationOn =
		(suiteInstance->flags & EGraphicsInstanceFlags_IsDebug) &&
		!(suiteInstance->flags & EGraphicsInstanceFlags_DisableGPUBV);

	GraphicsInstanceRef *ownInstanceRef = NULL;
	GraphicsDeviceRef *ownDeviceRef = NULL;

	if (suiteInstance->api == EGraphicsApi_Direct3D12 && gpuValidationOn) {

		Test_print(t, "D3D12 GPU based validation breaks raytracing state objects, tracing on a dedicated device");

		GraphicsApplicationInfo appInfo = (GraphicsApplicationInfo) {
			.name = CharString_createRefCStrConst("OxC3 ray trace test"),
			.version = 1
		};

		RefPtrType instanceType = GraphicsInstance_makeType(suiteInstance->api, alloc);
		ListGraphicsDeviceInfo deviceInfos = (ListGraphicsDeviceInfo) { 0 };

		if(!Test_assert(t, "createOwnInstance", GraphicsInstance_create(
			&appInfo, suiteInstance->api, EGraphicsInstanceFlags_DisableGPUBV, alloc, &instanceType, &ownInstanceRef, &t->err
		))) {
			SHFile_free(&file, alloc);
			return;
		}

		//The adapter has to be the same one the suite handed us, matched by name

		GraphicsInstance *ownInstance = GraphicsInstanceRef_ptr(ownInstanceRef);
		Bool created = false;

		if (Test_assert(t, "ownDeviceInfos", GraphicsInstance_getDeviceInfos(ownInstance, &deviceInfos, &t->err))) {

			for(U64 i = 0; i < deviceInfos.length; ++i)
				if (Buffer_eq(
					Buffer_createRefConst(deviceInfos.ptr[i].name, sizeof(deviceInfos.ptr[i].name)),
					Buffer_createRefConst(device->info.name, sizeof(device->info.name))
				)) {
					created = Test_assert(t, "createOwnDevice", GraphicsDeviceRef_create(
						ownInstanceRef, &deviceInfos.ptr[i], EGraphicsDeviceFlags_None,
						EGraphicsBufferingMode_Default, NULL, &ownDeviceRef, &t->err
					));
					break;
				}
		}

		ListGraphicsDeviceInfo_free(&deviceInfos, alloc);

		if (!created) {
			RefPtr_dec(&ownInstanceRef);
			SHFile_free(&file, alloc);
			return;
		}

		deviceRef = ownDeviceRef;
		device = GraphicsDeviceRef_ptr(deviceRef);
	}

	DeviceBufferRef *positions = NULL;
	DeviceBufferRef *output = NULL;
	BLASRef *blas = NULL;
	TLASRef *tlas = NULL;
	PipelineRef *pipeline = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	const F32 triangle[12] = {
		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1
	};

	Buffer triData = Buffer_createRefConst(triangle, sizeof(triangle));
	CharString name = CharString_createRefCStrConst("Ray trace positions");

	if(!Test_assert(t, "createPositions", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &triData, &positions, &t->err
	)))
		goto clean;

	const DeviceData positionData = (DeviceData) { .buffer = positions };
	name = CharString_createRefCStrConst("Ray trace BLAS");

	if(!Test_assert(t, "createBlas", GraphicsDeviceRef_createBLASExt(
		deviceRef, ERTASBuildFlags_None, EBLASFlag_None, ETextureFormatId_RGBA32f, 0,
		ETextureFormatId_Undefined, 16, positionData, (DeviceData) { 0 }, NULL, &name, &blas, &t->err
	)))
		goto clean;

	TLASInstanceStatic instance = (TLASInstanceStatic) {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
		.data = (TLASInstanceData) {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (U32) ETLASInstanceFlag_Default << 24,
			.blasCpu = blas
		}
	};

	ListTLASInstanceStatic instances = (ListTLASInstanceStatic) { 0 };
	ListTLASInstanceStatic_createRefConst(&instance, 1, &instances, NULL);

	name = CharString_createRefCStrConst("Ray trace TLAS");

	if(!Test_assert(t, "createTlas", GraphicsDeviceRef_createTLASExt(
		deviceRef, ERTASBuildFlags_DefaultTLAS, NULL, &instances, false, NULL, &name, &tlas, &t->err
	)))
		goto clean;

	//The whole point of this TLAS is being reachable from the raygen shader

	Test_assert(t, "tlasHandle", TLASRef_ptr(tlas)->handle != BindlessDescriptor_None);

	name = CharString_createRefCStrConst("Ray trace output");

	Test_assert(t, "createOutput", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWriteBindless | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 4 * sizeof(U32), &output, &t->err
	));

	//Raygen, miss and closest hit in that stage order; the one triangle group points at stage 2

	const U32 raygenId = TestShaders_entry(t, deviceRef, &file, "mainRaygen");
	const U32 missId = TestShaders_entry(t, deviceRef, &file, "mainMiss");
	const U32 hitId = TestShaders_entry(t, deviceRef, &file, "mainClosestHit");

	if(!output || raygenId == U32_MAX || missId == U32_MAX || hitId == U32_MAX)
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

	name = CharString_createRefCStrConst("Ray trace pipeline");

	if(!Test_assert(t, "createPipeline", GraphicsDeviceRef_createPipelineRaytracingExt(
		deviceRef, &stageList, &fileList, &groupList, &info, &name, EPipelineFlags_None, NULL, &pipeline, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "createList", GraphicsDeviceRef_createCommandList(
		deviceRef, 4 * KIBI, 64, 16, true, &commandList, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "createEmptyList", GraphicsDeviceRef_createCommandList(
		deviceRef, KIBI, 16, 8, true, &emptyList, &t->err
	)))
		goto clean;

	Test_assert(t, "beginEmptyList", CommandListRef_begin(emptyList, true, U64_MAX, &t->err));
	Test_assert(t, "endEmptyList", CommandListRef_end(emptyList, &t->err));

	//Build the scene and trace in one submit, split into scopes for the same dependency reason as the AS module

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

	Test_assert(t, "scopeBlas", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
	Test_assert(t, "updateBlas", CommandListRef_updateBLASExt(commandList, blas, &t->err));
	Test_assert(t, "scopeBlasEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "scopeTlas", CommandListRef_startScope(commandList, NULL, 2, NULL, &t->err));
	Test_assert(t, "updateTlas", CommandListRef_updateTLASExt(commandList, tlas, &t->err));
	Test_assert(t, "scopeTlasEnd", CommandListRef_endScope(commandList, &t->err));

	const Transition traceTransitions[2] = {
		(Transition) { .resource = output, .stage = EPipelineStage_RaygenExt, .isWrite = true },
		(Transition) { .resource = tlas, .stage = EPipelineStage_RaygenExt }
	};

	ListTransition traceTransitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(traceTransitions, 2, &traceTransitionList, NULL);

	Test_assert(t, "scopeTrace", CommandListRef_startScope(commandList, &traceTransitionList, 3, NULL, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setRaytracingPipeline(commandList, pipeline, &t->err));
	Test_assert(t, "trace", CommandListRef_dispatch1DRaysExt(commandList, 0, 4, &t->err));
	Test_assert(t, "scopeTraceEnd", CommandListRef_endScope(commandList, &t->err));

	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	TestShaderAppData appData = (TestShaderAppData) {
		.handles = { DeviceBufferRef_ptr(output)->writeHandle, TLASRef_ptr(tlas)->handle }
	};

	if(!TestShaders_submitAndWait(t, deviceRef, commandList, &appData, sizeof(appData)))
		goto clean;

	if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

		const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

		Test_assert(t, "rayResults", values[0] == 1 && values[1] == 1 && !values[2] && !values[3]);
	}

clean:

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&tlas);
	RefPtr_dec(&blas);
	RefPtr_dec(&output);
	RefPtr_dec(&positions);

	SHFile_free(&file, alloc);

	//The dedicated device's run has to be validation clean too, its counters just live on the own instance

	if (ownDeviceRef) {

		GraphicsDeviceRef_wait(ownDeviceRef, NULL);
		RefPtr_dec(&ownDeviceRef);

		GraphicsInstance *ownInstance = GraphicsInstanceRef_ptr(ownInstanceRef);
		Test_assert(t, "ownValidationErrors", !GraphicsInstance_getValidationErrors(ownInstance));
		Test_assert(t, "ownValidationWarnings", !GraphicsInstance_getValidationWarnings(ownInstance));
	}

	RefPtr_dec(&ownInstanceRef);
}
