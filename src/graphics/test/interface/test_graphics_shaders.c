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
#include "graphics/generic/opacity_micromap.h"
#include "graphics/generic/bindless_descriptor.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/commands.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "platforms/file.h"
#include "formats/oiSH/sh_file.h"
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
	U32 logicSrc0[4];        //U32 offsets 8..11: what logic op instance 0 writes
	U32 logicSrc1[4];        //U32 offsets 12..15: what instance 1 XORs on top
} TestShaderAppData;

//The gtest section only holds compiled oiSH files when the build had the shader compiler, so absence means skip.
//Loading the section twice is harmless, which keeps every module self contained.

Bool TestShaders_loadFile(Test *t, const C8 *pathStr, SHFile *file) {

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

U32 TestShaders_entry(Test *t, GraphicsDeviceRef *deviceRef, const SHFile *file, const C8 *name) {

	const CharString entryName = CharString_createRefCStrConst(name);

	const U32 id = GraphicsDeviceRef_getFirstShaderEntry(
		deviceRef, file, &entryName, NULL, NULL, ESHExtension_None, ESHExtension_None
	);

	Test_assert(t, "entryFound", id != U32_MAX);
	return id;
}

Bool TestShaders_computePipeline(
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

//The pixel entry is named because SHFile_combine matches entries by name, so two pixel shaders in one
//package can't both be "main" unless they agree on their signature - which a 1 target and a 2 target one
//don't.

static Bool TestShaders_graphicsPipelineNamed(
	Test *t, GraphicsDeviceRef *deviceRef, const ListSHFile *files, U16 vertexFile, U16 pixelFile,
	const C8 *pixelEntry, const PipelineGraphicsInfo *info, PipelineRef **pipeline
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

	const CharString name = CharString_createRefCStrConst("Shader test graphics pipeline");

	return Test_assert(t, "createGraphicsPipeline", GraphicsDeviceRef_createPipelineGraphics(
		deviceRef, files, &stageList, info, &name, EPipelineFlags_None, NULL, pipeline, &t->err
	));
}

static Bool TestShaders_graphicsPipeline(
	Test *t, GraphicsDeviceRef *deviceRef, const ListSHFile *files, U16 vertexFile, U16 pixelFile,
	const PipelineGraphicsInfo *info, PipelineRef **pipeline
) {
	return TestShaders_graphicsPipelineNamed(t, deviceRef, files, vertexFile, pixelFile, "main", info, pipeline);
}

Bool TestShaders_submitAndWait(
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

Bool TestShaders_pullBuffer(Test *t, GraphicsDeviceRef *deviceRef, CommandListRef *emptyList, DeviceBufferRef *buffer) {

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
		target, 0, 0, 0, 0, 0, 0, 0, TestShaders_pixelPull, pixels, &t->err
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
		"//OxC3_gtest/test_shaders/test_write_args.oiSH",
		"//OxC3_gtest/test_shaders/test_draw_mrt_ps.oiSH",
		"//OxC3_gtest/test_shaders/test_logicop_vs.oiSH",
		"//OxC3_gtest/test_shaders/test_logicop_ps.oiSH",
		"//OxC3_gtest/test_shaders/test_draw_dualsrc_ps.oiSH"
	};

	SHFile files[10] = { 0 };
	Bool loadedAll = true;

	for(U64 i = 0; i < 10; ++i)
		loadedAll &= TestShaders_loadFile(t, drawShaderPaths[i], &files[i]);

	if (!loadedAll) {

		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping draw execution tests");

		for(U64 i = 0; i < 10; ++i)
			SHFile_free(&files[i], alloc);

		return;
	}

	ListSHFile fileList = (ListSHFile) { 0 };
	ListSHFile_createRefConst(files, 10, &fileList, NULL);

	RenderTextureRef *target = NULL;
	RenderTextureRef *msaaTarget = NULL;
	DepthStencilRef *depth = NULL;
	DeviceBufferRef *vertexBuffer = NULL;
	DeviceBufferRef *indexBuffer = NULL;
	DeviceBufferRef *cpuArgs = NULL;
	DeviceBufferRef *gpuArgs = NULL;
	PipelineRef *flatPipeline = NULL;
	PipelineRef *blendPipeline = NULL;
	PipelineRef *wirePipeline = NULL;
	PipelineRef *mrtPipeline = NULL;
	RenderTextureRef *mrtTarget = NULL;
	PipelineRef *logicPipeline = NULL;
	RenderTextureRef *logicTarget = NULL;
	PipelineRef *dualPipeline = NULL;
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

	//Wireframe, when the adapter claims it.
	//The same fullscreen triangle that fills all 64 pixels above covers only its edges in wireframe, so the
	// interior keeps the clear.
	//Counting rather than comparing a fixed picture is deliberate: which pixels an edge touches is the
	// rasterizer's business and differs between implementations, but "some but not all" separates a wireframe
	// that took effect from one that was silently ignored, which would fill all 64 exactly as the flat draw did.

	if(GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.features & EGraphicsFeatures_Wireframe) {

		PipelineGraphicsInfo wireInfo = flatInfo;
		wireInfo.rasterizer.flags = (U16)(wireInfo.rasterizer.flags | ERasterizerFlags_IsWireframeExt);

		if (TestShaders_graphicsPipeline(t, deviceRef, &fileList, 0, 1, &wireInfo, &wirePipeline)) {

			Test_assert(t, "beginWire", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

			appData.color[0] = 1; appData.color[1] = 1; appData.color[2] = 1; appData.color[3] = 1;

			if (TestShaders_openDraw(t, commandList, 1, target, wirePipeline)) {

				Test_assert(t, "drawWire", CommandListRef_drawUnindexed(commandList, 3, 1, &t->err));

				if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
					t, deviceRef, commandList, &appData, sizeof(appData)
				)) {

					TestShaderPixels pixels = (TestShaderPixels) { 0 };

					if (TestShaders_pullPixels(t, deviceRef, emptyList, target, &pixels)) {

						U32 drawn = 0;

						for(U64 i = 0; i < 64; ++i)
							drawn += pixels.pixels[i] == 0xFFFFFFFFu;

						Test_assert(t, "wireDrewSomething", drawn > 0);
						Test_assert(t, "wireLeftInterior", drawn < 64);
					}
				}
			}
		}
	}

	//Multiple render targets: one draw writing two attachments, each getting its own constant.
	//This is the runtime half of the same thing the packaged MRT shader covers at compile time.
	//Nothing else in the suite binds more than one attachment,
	// which is how a backend divergence on the second target's semantic index stayed invisible
	// until packaging refused to merge the two.
	//Both targets are pulled and checked separately, so writing one output to both, or swapping them, fails.

	{
		name = CharString_createRefCStrConst("Shader draw MRT target 1");

		if(Test_assert(t, "createMrtTarget", GraphicsDeviceRef_createRenderTexture(
			deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
			EMSAASamples_Off, NULL, &name, &mrtTarget, &t->err
		))) {

			PipelineGraphicsInfo mrtInfo = flatInfo;
			mrtInfo.attachmentCountExt = 2;
			mrtInfo.attachmentFormatsExt[1] = ETextureFormatId_RGBA8;

			if (TestShaders_graphicsPipelineNamed(
				t, deviceRef, &fileList, 0, 6, "mainMrt", &mrtInfo, &mrtPipeline
			)) {

				Test_assert(t, "beginMrt", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
				Test_assert(t, "scopeMrt", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));

				const AttachmentInfo mrtColors[2] = {
					(AttachmentInfo) { .image = target,    .load = ELoadAttachmentType_Clear },
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

				if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
					t, deviceRef, commandList, &appData, sizeof(appData)
				)) {
					TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF3366FFu);
					TestShaders_checkPixels(t, deviceRef, emptyList, mrtTarget, 0xFF00CC00u);
				}
			}
		}
	}

	//Logic op, when the adapter claims it: one draw of two instances into a UINT target, the pipeline set to XOR.
	//Instance 0 XORs its value against the zero clear and instance 1 XORs on top,
	// so the readback holds src0 ^ src1 - a value no other op produces from these inputs.
	//Distinct failures land on distinct constants: a dropped logic op (plain overwrite) gives the last value
	// written, OR and AND give their own results, a dead draw leaves the clear.
	//The target is RGBA8u rather than the module's RGBA8, since a logic op is only defined on an integer
	// framebuffer and D3D12 refuses it on UNORM outright.
	//The gate is required: Vulkan reports logicOp false on most mobile GPUs and MoltenVK, and the pipeline
	// create would fail there rather than skip.

	if(!(GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.features & EGraphicsFeatures_LogicOp))
		Test_print(t, "Device doesn't support logicOp, skipping logic op test");

	else {

		name = CharString_createRefCStrConst("Shader draw logic op target");

		if(Test_assert(t, "createLogicTarget", GraphicsDeviceRef_createRenderTexture(
			deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8u, EGraphicsResourceFlag_None,
			EMSAASamples_Off, NULL, &name, &logicTarget, &t->err
		))) {

			PipelineGraphicsInfo logicInfo = (PipelineGraphicsInfo) {
				.attachmentCountExt = 1,
				.attachmentFormatsExt = { ETextureFormatId_RGBA8u },
				.blendState = (BlendState) {
					.enable = true,                       //Required: D3D12 drops LogicOpEnable otherwise
					.renderTargetMask = 0,                //Required: a logic op excludes blending
					.logicOpExt = ELogicOpExt_Xor,
					.writeMask = { EWriteMask_All }
				}
			};

			if (TestShaders_graphicsPipeline(t, deviceRef, &fileList, 7, 8, &logicInfo, &logicPipeline)) {

				Test_assert(t, "beginLogic", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

				appData.logicSrc0[0] = 0xF0; appData.logicSrc0[1] = 0x33;
				appData.logicSrc0[2] = 0x5A; appData.logicSrc0[3] = 0xFF;
				appData.logicSrc1[0] = 0x0F; appData.logicSrc1[1] = 0x11;
				appData.logicSrc1[2] = 0x3C; appData.logicSrc1[3] = 0x0F;

				if (TestShaders_openDraw(t, commandList, 1, logicTarget, logicPipeline)) {

					Test_assert(t, "drawLogic", CommandListRef_drawUnindexed(commandList, 3, 2, &t->err));

					//XOR of the two sources per channel: R 0xF0^0x0F, G 0x33^0x11, B 0x5A^0x3C, A 0xFF^0x0F.
					//R is the low byte of the pulled U32, matching every other expectation in this module.

					if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
						t, deviceRef, commandList, &appData, sizeof(appData)
					))
						TestShaders_checkPixels(t, deviceRef, emptyList, logicTarget, 0xF06622FFu);
				}
			}
		}
	}

	//Dual source blend, when the adapter claims it: the pixel shader emits two colors from one draw and the
	// blend multiplies the first by the second, so the attachment ends up holding the app data color scaled by
	// exactly a half - a result no single source factor produces from these inputs, so a backend that ignored
	// the second source lands on the unscaled color and fails.
	//The destination factor is Zero, so the cleared attachment contributes nothing.
	//This is also the runtime proof of the dual source reflection path: both outputs sit at LOCATION 0 on
	// SPIR-V, told apart by the Index decoration the DUAL_SRC macros apply.

	if(GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.features & EGraphicsFeatures_DualSrcBlend) {

		PipelineGraphicsInfo dualInfo = flatInfo;
		dualInfo.blendState = (BlendState) {
			.enable = true,
			.renderTargetMask = 1,
			.writeMask = { EWriteMask_All },
			.attachments = { (BlendStateAttachment) {
				.srcBlend = EBlend_Src1ColorExt, .dstBlend = EBlend_Zero,
				.srcBlendAlpha = EBlend_Src1AlphaExt, .dstBlendAlpha = EBlend_Zero,
				.blendOp = EBlendOp_Add, .blendOpAlpha = EBlendOp_Add
			} }
		};

		if (TestShaders_graphicsPipelineNamed(
			t, deviceRef, &fileList, 0, 9, "mainDualSrc", &dualInfo, &dualPipeline
		)) {

			Test_assert(t, "beginDual", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

			//All four channels at 0.8, so the halved result is 0.4, which lands on 102 in 8 bit unorm with no
			// rounding ambiguity either before or after the multiply.

			appData.color[0] = 204.f / 255; appData.color[1] = 204.f / 255;
			appData.color[2] = 204.f / 255; appData.color[3] = 204.f / 255;

			if (TestShaders_openDraw(t, commandList, 1, target, dualPipeline)) {

				Test_assert(t, "drawDual", CommandListRef_drawUnindexed(commandList, 3, 1, &t->err));

				if(TestShaders_closeDraw(t, commandList) && TestShaders_submitAndWait(
					t, deviceRef, commandList, &appData, sizeof(appData)
				))
					TestShaders_checkPixels(t, deviceRef, emptyList, target, 0x66666666u);
			}
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

	//MSAA: a fully covered pixel resolves to exactly the flat color whatever the sample count, and the result
	// lands in the readback target through the resolve attachment rather than a copy.
	//4x is required of every adapter; 2x and 8x are optional, so those are skipped unless the adapter claims
	// them, and running when it does is what turns the claim into something checked rather than reported.
	//Target and pipeline are rebuilt per count, since both bake the sample count in.

	static const struct {
		EMSAASamples samples;
		EGraphicsDataTypes dataType;            //0 when the count is required rather than optional
	} msaaCases[] = {
		{ EMSAASamples_x2Ext, EGraphicsDataTypes_MSAA2x },
		{ EMSAASamples_x4,    (EGraphicsDataTypes) 0    },
		{ EMSAASamples_x8Ext, EGraphicsDataTypes_MSAA8x }
	};

	const EGraphicsDataTypes msaaTypes = GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.dataTypes;
	U32 msaaRun = 0, msaaSkipped = 0;

	for (U64 m = 0; m < sizeof(msaaCases) / sizeof(msaaCases[0]); ++m) {

		if (msaaCases[m].dataType && !(msaaTypes & msaaCases[m].dataType)) {
			++msaaSkipped;
			continue;
		}

		name = CharString_createRefCStrConst("Shader draw MSAA target");

		if(!Test_assert(t, "createMsaaTarget", GraphicsDeviceRef_createRenderTexture(
			deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
			msaaCases[m].samples, NULL, &name, &msaaTarget, &t->err
		)))
			continue;

		PipelineGraphicsInfo msaaInfo = flatInfo;
		msaaInfo.msaa = msaaCases[m].samples;

		if (TestShaders_graphicsPipeline(t, deviceRef, &fileList, 0, 1, &msaaInfo, &msaaPipeline)) {

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
			)) {
				TestShaders_checkPixels(t, deviceRef, emptyList, target, 0xFF33FF66u);
				++msaaRun;
			}
		}

		//Both are rebuilt next iteration, and RefPtr_dec nulls them so the clean block below is still safe.

		RefPtr_dec(&msaaPipeline);
		RefPtr_dec(&msaaTarget);
	}

	Log_debugLnx(
		"-- draw: %"PRIu32" MSAA sample counts resolved, %"PRIu32" not claimed by this adapter",
		msaaRun, msaaSkipped
	);

clean:

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&argsPipeline);
	RefPtr_dec(&msaaPipeline);
	RefPtr_dec(&depthPipeline);
	RefPtr_dec(&vertexPipeline);
	RefPtr_dec(&dualPipeline);
	RefPtr_dec(&logicPipeline);
	RefPtr_dec(&logicTarget);
	RefPtr_dec(&mrtPipeline);
	RefPtr_dec(&mrtTarget);
	RefPtr_dec(&wirePipeline);
	RefPtr_dec(&blendPipeline);
	RefPtr_dec(&flatPipeline);
	RefPtr_dec(&gpuArgs);
	RefPtr_dec(&cpuArgs);
	RefPtr_dec(&indexBuffer);
	RefPtr_dec(&vertexBuffer);
	RefPtr_dec(&depth);
	RefPtr_dec(&msaaTarget);
	RefPtr_dec(&target);

	for(U64 i = 0; i < 10; ++i)
		SHFile_free(&files[i], alloc);
}

// -- 33. Ray trace execution -----------------------------------------------------

//The same one triangle scene the AS module builds, but with the bindless descriptor enabled so a real
// raytracing pipeline can fetch the TLAS and trace against it.
//Two rays start above the triangle's interior and two outside, so the readback distinguishes the closest
// hit and miss shaders actually running from any default.

//Special index opacity micromaps, traced for real rather than only validated at create.
//Two BLASes over the same triangle and the same index buffer, differing in nothing but the per triangle
// special index they carry: FullyOpaque has to trace exactly like the plain scene, FullyTransparent has to
// make every one of the 4 rays miss.
//Both halves are needed. "Everything missed" on its own is also what a BLAS that quietly failed to build
// looks like, so the opaque half is what proves the geometry survived the OMM path at all, and only the pair
// together says the micromap was consulted.
//Narrow formats rather than R32u on purpose: the special indices are signed constants matched against an
// unsigned element, so the truncated widths (0xFFFF for R16u, 0xFF for R8u) are where a driver would
// disagree with our packing.
//The wrapper below runs R16u everywhere and repeats the pair with R8u where RayMicromapOpacityU8 is set,
// which on Vulkan doubles as the only execution coverage the KHR extension path can get.

static void TestShaders_ommSpecialIndexWithFormat(
	Test *t,
	GraphicsDeviceRef *deviceRef,
	const SHFile *file,
	DeviceBufferRef *positions,
	DeviceBufferRef *output,
	CommandListRef *emptyList,
	ETextureFormatId ommIndexFormat
) {

	const GraphicsDeviceCapabilities caps = GraphicsDeviceRef_ptr(deviceRef)->info.capabilities;

	if (!(caps.features & EGraphicsFeatures_RayMicromapOpacity)) {
		Test_print(t, "Device lacks opacity micromaps, skipping OMM trace test");
		return;
	}

	if (caps.experimentalFeatures & EGraphicsFeatures_RayMicromapOpacity) {
		Test_print(t, "Opacity micromaps claimed but experimental on this backend, skipping OMM trace test");
		return;
	}

	DeviceBufferRef *indices = NULL;
	DeviceBufferRef *ommOpaque = NULL;
	DeviceBufferRef *ommTransparent = NULL;
	BLASRef *blasOpaque = NULL;
	BLASRef *blasTransparent = NULL;
	TLASRef *tlasOpaque = NULL;
	TLASRef *tlasTransparent = NULL;
	PipelineRef *pipeline = NULL;
	CommandListRef *opaqueList = NULL;
	CommandListRef *transparentList = NULL;

	//An OMM index is per triangle, which is why this geometry is indexed where the plain scene is not.

	const U16 triangleIndices[3] = { 0, 1, 2 };
	Buffer indexData = Buffer_createRefConst(triangleIndices, sizeof(triangleIndices));

	CharString name = CharString_createRefCStrConst("OMM triangle indices");

	if(!Test_assert(t, "ommCreateIndices", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &indexData, &indices, &t->err
	)))
		goto clean;

	//Packed into a U32 and sliced to the element width, which reads out the low bytes on the little endian
	// targets OxC3 runs on; one triangle, so the buffer is exactly one element.

	const U8 ommStride = ommIndexFormat == ETextureFormatId_R32u ? 4 : (ommIndexFormat == ETextureFormatId_R16u ? 2 : 1);

	const U32 opaqueIndex = EOMMSpecialIndex_pack(EOMMSpecialIndex_FullyOpaque, ommIndexFormat);
	const U32 transparentIndex = EOMMSpecialIndex_pack(EOMMSpecialIndex_FullyTransparent, ommIndexFormat);

	Buffer opaqueData = Buffer_createRefConst(&opaqueIndex, ommStride);
	Buffer transparentData = Buffer_createRefConst(&transparentIndex, ommStride);

	name = CharString_createRefCStrConst("OMM indices, fully opaque");

	if(!Test_assert(t, "ommCreateOpaque", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &opaqueData, &ommOpaque, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("OMM indices, fully transparent");

	if(!Test_assert(t, "ommCreateTransparent", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &transparentData, &ommTransparent, &t->err
	)))
		goto clean;

	const DeviceData positionData = (DeviceData) { .buffer = positions };
	const DeviceData indexBufferData = (DeviceData) { .buffer = indices };

	const BLASCreateInfo opaqueInfo = BLASCreateInfo_indexedWithOmmIndicesExt(
		ERTASBuildFlags_None, EBLASFlag_None, ETextureFormatId_RGBA32f, 0, 16, positionData,
		ETextureFormatId_R16u, indexBufferData,
		ommIndexFormat, (DeviceData) { .buffer = ommOpaque }
	);

	const BLASCreateInfo transparentInfo = BLASCreateInfo_indexedWithOmmIndicesExt(
		ERTASBuildFlags_None, EBLASFlag_None, ETextureFormatId_RGBA32f, 0, 16, positionData,
		ETextureFormatId_R16u, indexBufferData,
		ommIndexFormat, (DeviceData) { .buffer = ommTransparent }
	);

	name = CharString_createRefCStrConst("OMM BLAS, fully opaque");

	if(!Test_assert(t, "ommCreateBlasOpaque", GraphicsDeviceRef_createBLASExt(
		deviceRef, &opaqueInfo, &name, &blasOpaque, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("OMM BLAS, fully transparent");

	if(!Test_assert(t, "ommCreateBlasTransparent", GraphicsDeviceRef_createBLASExt(
		deviceRef, &transparentInfo, &name, &blasTransparent, &t->err
	)))
		goto clean;

	//ForceDisableAnyHit is deliberately absent where the rest of the module uses Default.
	//It is FORCE_OPAQUE on both APIs, and a forced opaque instance makes traversal ignore opacity micromaps
	// entirely, so the transparent half would report hits and pass for the wrong reason.

	TLASInstance ommInstance = (TLASInstance) {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
		.data = (TLASInstanceData) {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (U32) ETLASInstanceFlag_DisableCulling << 24,
			.blasCpu = blasOpaque
		}
	};

	ListTLASInstance ommInstances = (ListTLASInstance) { 0 };
	ListTLASInstance_createRefConst(&ommInstance, 1, &ommInstances, NULL);

	name = CharString_createRefCStrConst("OMM TLAS, fully opaque");

	if(!Test_assert(t, "ommCreateTlasOpaque", GraphicsDeviceRef_createTLASExt(
		deviceRef, ERTASBuildFlags_DefaultTLAS, &ommInstances, false, NULL, &name, &tlasOpaque, &t->err
	)))
		goto clean;

	ommInstance.data.blasCpu = blasTransparent;
	name = CharString_createRefCStrConst("OMM TLAS, fully transparent");

	if(!Test_assert(t, "ommCreateTlasTransparent", GraphicsDeviceRef_createTLASExt(
		deviceRef, ERTASBuildFlags_DefaultTLAS, &ommInstances, false, NULL, &name, &tlasTransparent, &t->err
	)))
		goto clean;

	//A pipeline of its own: both APIs ignore micromaps unless the pipeline opted in, so the plain one the
	// module already built would trace straight through the transparent triangle and report hits.

	const U32 raygenId = TestShaders_entry(t, deviceRef, file, "mainRaygen");
	const U32 missId = TestShaders_entry(t, deviceRef, file, "mainMiss");
	const U32 hitId = TestShaders_entry(t, deviceRef, file, "mainClosestHit");

	if(raygenId == U32_MAX || missId == U32_MAX || hitId == U32_MAX)
		goto clean;

	PipelineStage ommStages[3] = {
		(PipelineStage) { .binaryId = raygenId },
		(PipelineStage) { .binaryId = missId },
		(PipelineStage) { .binaryId = hitId }
	};

	ListPipelineStage ommStageList = (ListPipelineStage) { 0 };
	ListPipelineStage_createRefConst(ommStages, 3, &ommStageList, NULL);

	ListSHFile ommFileList = (ListSHFile) { 0 };
	ListSHFile_createRefConst(file, 1, &ommFileList, NULL);

	PipelineRaytracingGroup ommGroup = (PipelineRaytracingGroup) {
		.closestHit = 2, .anyHit = U32_MAX, .intersection = U32_MAX
	};

	ListPipelineRaytracingGroup ommGroupList = (ListPipelineRaytracingGroup) { 0 };
	ListPipelineRaytracingGroup_createRefConst(&ommGroup, 1, &ommGroupList, NULL);

	const PipelineRaytracingInfo ommPipelineInfo = (PipelineRaytracingInfo) {
		.flags = EPipelineRaytracingFlags_Default | EPipelineRaytracingFlags_AllowOpacityMicromapExt,
		.maxRecursionDepth = 1
	};

	name = CharString_createRefCStrConst("OMM ray trace pipeline");

	if(!Test_assert(t, "ommCreatePipeline", GraphicsDeviceRef_createPipelineRaytracingExt(
		deviceRef, &ommStageList, &ommFileList, &ommGroupList, &ommPipelineInfo, &name,
		EPipelineFlags_None, NULL, &pipeline, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "ommCreateOpaqueList", GraphicsDeviceRef_createCommandList(
		deviceRef, 2 * KIBI, 32, 16, true, &opaqueList, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "ommCreateTransparentList", GraphicsDeviceRef_createCommandList(
		deviceRef, 2 * KIBI, 32, 16, true, &transparentList, &t->err
	)))
		goto clean;

	//Opaque first, so a failure below can be read as "the OMM path broke tracing" rather than "the micromap
	// culled something", which are the two ways this can go wrong and want different fixes.

	const Transition opaqueTransitions[2] = {
		(Transition) { .resource = output, .stage = EPipelineStage_RaygenExt, .isWrite = true },
		(Transition) { .resource = tlasOpaque, .stage = EPipelineStage_RaygenExt }
	};

	ListTransition opaqueTransitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(opaqueTransitions, 2, &opaqueTransitionList, NULL);

	Test_assert(t, "ommBeginOpaque", CommandListRef_begin(opaqueList, true, U64_MAX, &t->err));

	Test_assert(t, "ommScopeBlasOpaque", CommandListRef_startScope(opaqueList, NULL, 1, NULL, &t->err));
	Test_assert(t, "ommUpdateBlasOpaque", CommandListRef_updateBLASExt(opaqueList, blasOpaque, &t->err));
	Test_assert(t, "ommScopeBlasOpaqueEnd", CommandListRef_endScope(opaqueList, &t->err));

	Test_assert(t, "ommScopeTlasOpaque", CommandListRef_startScope(opaqueList, NULL, 2, NULL, &t->err));
	Test_assert(t, "ommUpdateTlasOpaque", CommandListRef_updateTLASExt(opaqueList, tlasOpaque, &t->err));
	Test_assert(t, "ommScopeTlasOpaqueEnd", CommandListRef_endScope(opaqueList, &t->err));

	Test_assert(t, "ommScopeTraceOpaque", CommandListRef_startScope(
		opaqueList, &opaqueTransitionList, 3, NULL, &t->err
	));

	Test_assert(t, "ommBindOpaque", CommandListRef_setRaytracingPipeline(opaqueList, pipeline, &t->err));
	Test_assert(t, "ommTraceOpaque", CommandListRef_dispatch1DRaysExt(opaqueList, 0, 4, &t->err));
	Test_assert(t, "ommScopeTraceOpaqueEnd", CommandListRef_endScope(opaqueList, &t->err));

	Test_assert(t, "ommEndOpaque", CommandListRef_end(opaqueList, &t->err));

	const TestShaderAppData opaqueAppData = (TestShaderAppData) {
		.handles = { DeviceBufferRef_ptr(output)->writeHandle, TLASRef_ptr(tlasOpaque)->handle }
	};

	if (TestShaders_submitAndWait(t, deviceRef, opaqueList, &opaqueAppData, sizeof(opaqueAppData)))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			Test_assert(t, "ommResultsOpaque", values[0] == 1 && values[1] == 1 && !values[2] && !values[3]);
		}

	const Transition transparentTransitions[2] = {
		(Transition) { .resource = output, .stage = EPipelineStage_RaygenExt, .isWrite = true },
		(Transition) { .resource = tlasTransparent, .stage = EPipelineStage_RaygenExt }
	};

	ListTransition transparentTransitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(transparentTransitions, 2, &transparentTransitionList, NULL);

	Test_assert(t, "ommBeginTransparent", CommandListRef_begin(transparentList, true, U64_MAX, &t->err));

	Test_assert(t, "ommScopeBlasTransparent", CommandListRef_startScope(transparentList, NULL, 1, NULL, &t->err));

	Test_assert(t, "ommUpdateBlasTransparent", CommandListRef_updateBLASExt(
		transparentList, blasTransparent, &t->err
	));

	Test_assert(t, "ommScopeBlasTransparentEnd", CommandListRef_endScope(transparentList, &t->err));

	Test_assert(t, "ommScopeTlasTransparent", CommandListRef_startScope(transparentList, NULL, 2, NULL, &t->err));

	Test_assert(t, "ommUpdateTlasTransparent", CommandListRef_updateTLASExt(
		transparentList, tlasTransparent, &t->err
	));

	Test_assert(t, "ommScopeTlasTransparentEnd", CommandListRef_endScope(transparentList, &t->err));

	Test_assert(t, "ommScopeTraceTransparent", CommandListRef_startScope(
		transparentList, &transparentTransitionList, 3, NULL, &t->err
	));

	Test_assert(t, "ommBindTransparent", CommandListRef_setRaytracingPipeline(transparentList, pipeline, &t->err));
	Test_assert(t, "ommTraceTransparent", CommandListRef_dispatch1DRaysExt(transparentList, 0, 4, &t->err));
	Test_assert(t, "ommScopeTraceTransparentEnd", CommandListRef_endScope(transparentList, &t->err));

	Test_assert(t, "ommEndTransparent", CommandListRef_end(transparentList, &t->err));

	const TestShaderAppData transparentAppData = (TestShaderAppData) {
		.handles = { DeviceBufferRef_ptr(output)->writeHandle, TLASRef_ptr(tlasTransparent)->handle }
	};

	if (TestShaders_submitAndWait(t, deviceRef, transparentList, &transparentAppData, sizeof(transparentAppData)))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			Test_assert(t, "ommResultsTransparent", !values[0] && !values[1] && !values[2] && !values[3]);
		}

clean:

	RefPtr_dec(&transparentList);
	RefPtr_dec(&opaqueList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&tlasTransparent);
	RefPtr_dec(&tlasOpaque);
	RefPtr_dec(&blasTransparent);
	RefPtr_dec(&blasOpaque);
	RefPtr_dec(&ommTransparent);
	RefPtr_dec(&ommOpaque);
	RefPtr_dec(&indices);
}

//A real micromap ARRAY, built on the GPU and linked into a BLAS, rather than the special index form above.
//One micromap holds five 2-state subdivision level 1 entries: entry k of 0..3 is opaque except for micro
// triangle k, entry 4 is fully transparent.
//The instance is nudged by (-0.05, -0.05) so ray 0 lands strictly inside the CENTER sub triangle and ray 1
// strictly inside the corner one at the barycentric origin, comfortably away from every sub triangle edge.
//The assertions are deliberately mapping agnostic: the spec's space filling curve decides which bit is which
// sub triangle, so instead of assuming that order, the four single bit probes must produce exactly one
// (miss, hit), exactly one (hit, miss) and two (hit, hit) for rays 0 and 1.
//That is only satisfiable if each probe culled a DIFFERENT sub triangle, which is per micro triangle
// addressing proven without a single assumption about the curve; entry 4 proving all miss pins the decode.

static void TestShaders_ommMicromapArray(
	Test *t,
	GraphicsDeviceRef *deviceRef,
	const SHFile *file,
	DeviceBufferRef *positions,
	DeviceBufferRef *output,
	CommandListRef *emptyList
) {

	const GraphicsDeviceCapabilities caps = GraphicsDeviceRef_ptr(deviceRef)->info.capabilities;

	if (!(caps.features & EGraphicsFeatures_RayMicromapOpacity)) {
		Test_print(t, "Device lacks opacity micromaps, skipping micromap array test");
		return;
	}

	if (caps.experimentalFeatures & EGraphicsFeatures_RayMicromapOpacity) {
		Test_print(t, "Opacity micromaps claimed but experimental on this backend, skipping micromap array test");
		return;
	}

	//The KHR path builds micromap arrays as acceleration structures, which isn't implemented until a driver
	// exists to test it against; special index OMM covers such a device above.

	const GraphicsInstance *instance = GraphicsInstanceRef_ptr(GraphicsDeviceRef_ptr(deviceRef)->instance);

	if (
		instance->api == EGraphicsApi_Vulkan &&
		(caps.featuresExt & EVkGraphicsFeatures_OpacityMicromapKHR)
	) {
		Test_print(t, "Micromap arrays aren't implemented on the Vulkan KHR path yet, skipping");
		return;
	}

	DeviceBufferRef *indices = NULL;
	DeviceBufferRef *inputBits = NULL;
	DeviceBufferRef *entries = NULL;
	OpacityMicromapRef *micromap = NULL;
	PipelineRef *pipeline = NULL;

	DeviceBufferRef *ommIndex[5] = { 0 };
	BLASRef *blas[5] = { 0 };
	TLASRef *tlas[5] = { 0 };
	CommandListRef *lists[5] = { 0 };

	const U16 triangleIndices[3] = { 0, 1, 2 };
	Buffer indexData = Buffer_createRefConst(triangleIndices, sizeof(triangleIndices));

	CharString name = CharString_createRefCStrConst("OMM array triangle indices");

	if(!Test_assert(t, "ommArrayCreateIndices", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &indexData, &indices, &t->err
	)))
		goto clean;

	//One byte of opacity bits per entry, 4 bytes apart so every dataOffset stays 4 byte aligned.
	//2-state: bit set is opaque, cleared is transparent, and only the low 4 bits exist at level 1.

	U8 opacityBits[20] = { 0 };

	for(U8 k = 0; k < 4; ++k)
		opacityBits[k * 4] = 0xF & ~(1 << k);

	//opacityBits[16] stays 0: entry 4 is fully transparent

	Buffer bitsData = Buffer_createRefConst(opacityBits, sizeof(opacityBits));
	name = CharString_createRefCStrConst("OMM array opacity bits");

	if(!Test_assert(t, "ommArrayCreateBits", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &bitsData, &inputBits, &t->err
	)))
		goto clean;

	OpacityMicromapEntry entryData[5];

	for(U8 k = 0; k < 5; ++k)
		entryData[k] = (OpacityMicromapEntry) {
			.dataOffset = (U32) k * 4,
			.subdivisionLevel = 1,
			.format = EOpacityMicromapFormat_Opacity2State
		};

	Buffer entryRef = Buffer_createRefConst(entryData, sizeof(entryData));
	name = CharString_createRefCStrConst("OMM array entries");

	if(!Test_assert(t, "ommArrayCreateEntries", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&name, &entryRef, &entries, &t->err
	)))
		goto clean;

	const OpacityMicromapUsage usage = (OpacityMicromapUsage) {
		.count = 5, .subdivisionLevel = 1, .format = EOpacityMicromapFormat_Opacity2State
	};

	OpacityMicromapCreateInfo micromapInfo = OpacityMicromapCreateInfo_uniform(
		ERTASBuildFlags_None,
		&(DeviceData) { .buffer = inputBits },
		&(DeviceData) { .buffer = entries },
		sizeof(OpacityMicromapEntry),
		&usage
	);

	name = CharString_createRefCStrConst("OMM array micromap");

	if(!Test_assert(t, "ommArrayCreate", GraphicsDeviceRef_createOpacityMicromapExt(
		deviceRef, &micromapInfo, &name, &micromap, &t->err
	)))
		goto clean;

	//The same pipeline shape the special index test uses; micromaps are ignored without the opt in

	const U32 raygenId = TestShaders_entry(t, deviceRef, file, "mainRaygen");
	const U32 missId = TestShaders_entry(t, deviceRef, file, "mainMiss");
	const U32 hitId = TestShaders_entry(t, deviceRef, file, "mainClosestHit");

	if(raygenId == U32_MAX || missId == U32_MAX || hitId == U32_MAX)
		goto clean;

	PipelineStage stages[3] = {
		(PipelineStage) { .binaryId = raygenId },
		(PipelineStage) { .binaryId = missId },
		(PipelineStage) { .binaryId = hitId }
	};

	ListPipelineStage stageList = (ListPipelineStage) { 0 };
	ListPipelineStage_createRefConst(stages, 3, &stageList, NULL);

	ListSHFile fileList = (ListSHFile) { 0 };
	ListSHFile_createRefConst(file, 1, &fileList, NULL);

	PipelineRaytracingGroup group = (PipelineRaytracingGroup) {
		.closestHit = 2, .anyHit = U32_MAX, .intersection = U32_MAX
	};

	ListPipelineRaytracingGroup groupList = (ListPipelineRaytracingGroup) { 0 };
	ListPipelineRaytracingGroup_createRefConst(&group, 1, &groupList, NULL);

	const PipelineRaytracingInfo pipelineInfo = (PipelineRaytracingInfo) {
		.flags = EPipelineRaytracingFlags_Default | EPipelineRaytracingFlags_AllowOpacityMicromapExt,
		.maxRecursionDepth = 1
	};

	name = CharString_createRefCStrConst("OMM array pipeline");

	if(!Test_assert(t, "ommArrayCreatePipeline", GraphicsDeviceRef_createPipelineRaytracingExt(
		deviceRef, &stageList, &fileList, &groupList, &pipelineInfo, &name,
		EPipelineFlags_None, NULL, &pipeline, &t->err
	)))
		goto clean;

	//How rays 0 and 1 resolved per probe, packed as (ray0Hit << 1) | ray1Hit

	U8 outcomes[4] = { 0 };
	Bool traced = true;

	for (U8 k = 0; k < 5 && traced; ++k) {

		const U16 entryIndex = k;
		Buffer ommIndexData = Buffer_createRefConst(&entryIndex, sizeof(entryIndex));

		name = CharString_createRefCStrConst("OMM array index buffer");

		traced &= Test_assert(t, "ommArrayCreateIndexBuf", GraphicsDeviceRef_createBufferData(
			deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
			&name, &ommIndexData, &ommIndex[k], &t->err
		));

		if(!traced)
			break;

		const BLASCreateInfo blasInfo = BLASCreateInfo_indexedWithOmmExt(
			ERTASBuildFlags_None, EBLASFlag_None, ETextureFormatId_RGBA32f, 0, 16,
			(DeviceData) { .buffer = positions },
			ETextureFormatId_R16u, (DeviceData) { .buffer = indices },
			ETextureFormatId_R16u, (DeviceData) { .buffer = ommIndex[k] },
			micromap
		);

		name = CharString_createRefCStrConst("OMM array BLAS");

		traced &= Test_assert(t, "ommArrayCreateBlas", GraphicsDeviceRef_createBLASExt(
			deviceRef, &blasInfo, &name, &blas[k], &t->err
		));

		if(!traced)
			break;

		//Nudged so ray 0 sits strictly inside the center sub triangle and ray 1 strictly inside the corner
		// one; without this ray 0 would land exactly on the shared edge and the outcome would be tie break
		// dependent.

		const TLASInstance ommInstance = (TLASInstance) {
			.transform = { { 1, 0, 0, -0.05f }, { 0, 1, 0, -0.05f }, { 0, 0, 1, 0 } },
			.data = (TLASInstanceData) {
				.instanceId24_mask8 = 0xFFu << 24,
				.sbtOffset24_flags8 = (U32) ETLASInstanceFlag_DisableCulling << 24,
				.blasCpu = blas[k]
			}
		};

		ListTLASInstance ommInstances = (ListTLASInstance) { 0 };
		ListTLASInstance_createRefConst(&ommInstance, 1, &ommInstances, NULL);

		name = CharString_createRefCStrConst("OMM array TLAS");

		traced &= Test_assert(t, "ommArrayCreateTlas", GraphicsDeviceRef_createTLASExt(
			deviceRef, ERTASBuildFlags_DefaultTLAS, &ommInstances, false, NULL, &name, &tlas[k], &t->err
		));

		traced &= Test_assert(t, "ommArrayCreateList", GraphicsDeviceRef_createCommandList(
			deviceRef, 2 * KIBI, 32, 16, true, &lists[k], &t->err
		));

		if(!traced)
			break;

		const Transition traceTransitions[2] = {
			(Transition) { .resource = output, .stage = EPipelineStage_RaygenExt, .isWrite = true },
			(Transition) { .resource = tlas[k], .stage = EPipelineStage_RaygenExt }
		};

		ListTransition traceTransitionList = (ListTransition) { 0 };
		ListTransition_createRefConst(traceTransitions, 2, &traceTransitionList, NULL);

		//The micromap build only runs once; recording it again is a no-op after it completed

		Test_assert(t, "ommArrayBegin", CommandListRef_begin(lists[k], true, U64_MAX, &t->err));

		Test_assert(t, "ommArrayScopeOmm", CommandListRef_startScope(lists[k], NULL, 1, NULL, &t->err));
		Test_assert(t, "ommArrayUpdateOmm", CommandListRef_updateOmmExt(lists[k], micromap, &t->err));
		Test_assert(t, "ommArrayScopeOmmEnd", CommandListRef_endScope(lists[k], &t->err));

		Test_assert(t, "ommArrayScopeBlas", CommandListRef_startScope(lists[k], NULL, 2, NULL, &t->err));
		Test_assert(t, "ommArrayUpdateBlas", CommandListRef_updateBLASExt(lists[k], blas[k], &t->err));
		Test_assert(t, "ommArrayScopeBlasEnd", CommandListRef_endScope(lists[k], &t->err));

		Test_assert(t, "ommArrayScopeTlas", CommandListRef_startScope(lists[k], NULL, 3, NULL, &t->err));
		Test_assert(t, "ommArrayUpdateTlas", CommandListRef_updateTLASExt(lists[k], tlas[k], &t->err));
		Test_assert(t, "ommArrayScopeTlasEnd", CommandListRef_endScope(lists[k], &t->err));

		Test_assert(t, "ommArrayScopeTrace", CommandListRef_startScope(
			lists[k], &traceTransitionList, 4, NULL, &t->err
		));

		Test_assert(t, "ommArrayBind", CommandListRef_setRaytracingPipeline(lists[k], pipeline, &t->err));
		Test_assert(t, "ommArrayTrace", CommandListRef_dispatch1DRaysExt(lists[k], 0, 4, &t->err));
		Test_assert(t, "ommArrayScopeTraceEnd", CommandListRef_endScope(lists[k], &t->err));

		Test_assert(t, "ommArrayEnd", CommandListRef_end(lists[k], &t->err));

		const TestShaderAppData appData = (TestShaderAppData) {
			.handles = { DeviceBufferRef_ptr(output)->writeHandle, TLASRef_ptr(tlas[k])->handle }
		};

		traced &= TestShaders_submitAndWait(t, deviceRef, lists[k], &appData, sizeof(appData));
		traced = traced && TestShaders_pullBuffer(t, deviceRef, emptyList, output);

		if (traced) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			//The geometric misses stay misses no matter what the micromap says

			Test_assert(t, "ommArrayOutsideMiss", !values[2] && !values[3]);

			if(k == 4)
				Test_assert(t, "ommArrayAllTransparent", !values[0] && !values[1]);

			else outcomes[k] = (U8)(((values[0] == 1) << 1) | (values[1] == 1));
		}
	}

	//The probe set proves per micro triangle addressing without assuming the space filling curve's order

	if (traced) {

		U8 missHit = 0, hitMiss = 0, hitHit = 0, missMiss = 0;

		for (U8 k = 0; k < 4; ++k)
			switch (outcomes[k]) {
				case 1:     ++missHit;    break;        //ray 0 culled: this bit is the center sub triangle
				case 2:     ++hitMiss;    break;        //ray 1 culled: this bit is the corner sub triangle
				case 3:     ++hitHit;     break;        //a sub triangle neither ray visits
				default:    ++missMiss;   break;        //one bit culled both rays: not per micro triangle
			}

		Test_assert(t, "ommArrayCenterProbe", missHit == 1);
		Test_assert(t, "ommArrayCornerProbe", hitMiss == 1);
		Test_assert(t, "ommArrayUntouchedProbes", hitHit == 2);
		Test_assert(t, "ommArrayNoDoubleCull", !missMiss);

		//The likely emulated hint fires once per device rather than per BLAS: five linked micromaps, one
		// bit. Non NV vendors (WARP included) claim RayMicromapOpacityActual, so the bit only appears
		// where that is unset.

		Test_assert(
			t, "ommArrayHintOnce",
			!!(AtomicI64_load(&GraphicsDeviceRef_ptr(deviceRef)->runtimeMessages) &
			(I64) EGraphicsDeviceMessage_OmmLikelyEmulated) ==
			!(caps.features2 & EGraphicsFeatures2_RayMicromapOpacityActual)
		);
	}

clean:

	for (U8 k = 0; k < 5; ++k) {
		RefPtr_dec(&lists[k]);
		RefPtr_dec(&tlas[k]);
		RefPtr_dec(&blas[k]);
		RefPtr_dec(&ommIndex[k]);
	}

	RefPtr_dec(&pipeline);
	RefPtr_dec(&micromap);
	RefPtr_dec(&entries);
	RefPtr_dec(&inputBits);
	RefPtr_dec(&indices);
}

static void TestShaders_ommSpecialIndex(
	Test *t,
	GraphicsDeviceRef *deviceRef,
	const SHFile *file,
	DeviceBufferRef *positions,
	DeviceBufferRef *output,
	CommandListRef *emptyList
) {

	TestShaders_ommSpecialIndexWithFormat(t, deviceRef, file, positions, output, emptyList, ETextureFormatId_R16u);

	//The 8-bit pair is the same scene through a 1 byte element, where the special index truncates to 0xFF.

	const GraphicsDeviceCapabilities caps = GraphicsDeviceRef_ptr(deviceRef)->info.capabilities;

	if (caps.features2 & EGraphicsFeatures2_RayMicromapOpacityU8) {
		Test_print(t, "Repeating the OMM special index pair with R8u indices");
		TestShaders_ommSpecialIndexWithFormat(t, deviceRef, file, positions, output, emptyList, ETextureFormatId_R8u);
	}

	else Test_print(t, "Device lacks 8-bit OMM indices, R8u trace pair skipped");
}

//The whole ray pipeline vehicle (scene, SBT, dispatch and readback) shared by the plain and the SER
//variant, which differ only in which oiSH drives it: both trace the same 4 rays at the same triangle and
//have to land on the same (1, 1, 0, 0).

static void TestShaders_raysWithFile(
	Test *t, GraphicsDeviceRef *deviceRef, const C8 *moduleName, const C8 *path, Bool testApiExtras
) {

	Test_setModule(t, moduleName);

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

	if (!TestShaders_loadFile(t, path, &file)) {
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

	//Declared out here with the ref it belongs to, not in the branch that fills it in.
	//RefPtr_create keeps a POINTER to the type rather than a copy (see ref_ptr.h), so a type scoped to that
	// branch is already gone by the time the instance is released at the end of this function, which is a
	// stack use after scope that only ASan was ever going to notice.

	RefPtrType instanceType = (RefPtrType) { 0 };

	if (suiteInstance->api == EGraphicsApi_Direct3D12 && gpuValidationOn) {

		Test_print(t, "D3D12 GPU based validation breaks raytracing state objects, tracing on a dedicated device");

		GraphicsApplicationInfo appInfo = (GraphicsApplicationInfo) {
			.name = CharString_createRefCStrConst("OxC3 ray trace test"),
			.version = 1
		};

		instanceType = GraphicsInstance_makeType(suiteInstance->api, alloc);
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
	CommandListRef *refitList = NULL;
	CommandListRef *blasRefitList = NULL;

	const F32 triangle[12] = {
		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1
	};

	Buffer triData = Buffer_createRefConst(triangle, sizeof(triangle));
	CharString name = CharString_createRefCStrConst("Ray trace positions");

	//CPUBacked because the BLAS refit below rewrites these positions in place and marks them dirty, which is
	// what a refit reads: the BLAS keeps pointing at this same buffer.

	if(!Test_assert(t, "createPositions", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_CPUBacked, NULL,
		&name, &triData, &positions, &t->err
	)))
		goto clean;

	const DeviceData positionData = (DeviceData) { .buffer = positions };
	name = CharString_createRefCStrConst("Ray trace BLAS");

	//AllowUpdate on the parent because the refit below updates FROM this one, and both APIs require the source
	// of an update to have been built with it.
	//Our own validation only checks the refit's flags, so leaving it off here fails in the driver instead.

	const BLASCreateInfo blasInfo = BLASCreateInfo_unindexed(
		ERTASBuildFlags_AllowUpdate, EBLASFlag_None, ETextureFormatId_RGBA32f, 0, 16, positionData
	);

	if(!Test_assert(t, "createBlas", GraphicsDeviceRef_createBLASExt(deviceRef, &blasInfo, &name, &blas, &t->err)))
		goto clean;

	TLASInstance instance = (TLASInstance) {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
		.data = (TLASInstanceData) {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (U32) ETLASInstanceFlag_Default << 24,
			.blasCpu = blas
		}
	};

	ListTLASInstance instances = (ListTLASInstance) { 0 };
	ListTLASInstance_createRefConst(&instance, 1, &instances, NULL);

	name = CharString_createRefCStrConst("Ray trace TLAS");

	if(!Test_assert(t, "createTlas", GraphicsDeviceRef_createTLASExt(
		deviceRef, ERTASBuildFlags_DefaultTLAS | ERTASBuildFlags_AllowUpdate, &instances, false, NULL,
		&name, &tlas, &t->err
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

	//Refit the TLAS by moving its one instance far along Z, so the same 4 rays can no longer reach the triangle
	// and every one has to miss.
	//That is what separates a refit that ran from one that silently did nothing: a no-op refit still leaves a
	// valid AS describing the ORIGINAL scene, so it would keep reporting the two hits above.
	//A refit is in place, so this is the SAME TLAS throughout: no second object, no reallocation, and the
	// bindless handle the shader was already given keeps working, which the assert below pins down.
	//Only one of the two ray variants runs this and the blocks after it: refit and opacity micromaps are API
	// level rather than SER specific, so there is no reason to pay for them twice.

	if (testApiExtras) {

		const BindlessDescriptor handleBeforeRefit = TLASRef_ptr(tlas)->handle;

		TLASInstance moved = instance;
		moved.transform[2][3] = 1000;                //Translate Z; the transform is row major 3x4

		ListTLASInstance movedInstances = (ListTLASInstance) { 0 };
		ListTLASInstance_createRefConst(&moved, 1, &movedInstances, NULL);

		Bool madeRefit = Test_assert(t, "setInstancesMoved", TLASRef_setInstancesExt(tlas, &movedInstances, &t->err));

		madeRefit &= Test_assert(t, "createRefitList", GraphicsDeviceRef_createCommandList(
			deviceRef, KIBI, 16, 8, true, &refitList, &t->err
		));

		if (madeRefit) {

			//The scene the shader reads is reached through the same handle as before, so the app data that
			// drove the first trace drives this one unchanged.

			Test_assert(t, "refitKeepsHandle", TLASRef_ptr(tlas)->handle == handleBeforeRefit);

			Test_assert(t, "beginRefit", CommandListRef_begin(refitList, true, U64_MAX, &t->err));

			Test_assert(t, "scopeRefit", CommandListRef_startScope(refitList, NULL, 4, NULL, &t->err));
			Test_assert(t, "updateTlasRefit", CommandListRef_updateTLASExt(refitList, tlas, &t->err));
			Test_assert(t, "scopeRefitEnd", CommandListRef_endScope(refitList, &t->err));

			Test_assert(t, "scopeTraceRefit", CommandListRef_startScope(
				refitList, &traceTransitionList, 5, NULL, &t->err
			));

			Test_assert(t, "bindPipelineRefit", CommandListRef_setRaytracingPipeline(refitList, pipeline, &t->err));
			Test_assert(t, "traceRefit", CommandListRef_dispatch1DRaysExt(refitList, 0, 4, &t->err));
			Test_assert(t, "scopeTraceRefitEnd", CommandListRef_endScope(refitList, &t->err));

			Test_assert(t, "endRefit", CommandListRef_end(refitList, &t->err));

			if (TestShaders_submitAndWait(t, deviceRef, refitList, &appData, sizeof(appData)))
				if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

					const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;
					Test_assert(t, "rayResultsRefit", !values[0] && !values[1] && !values[2] && !values[3]);
				}

			//Refit straight back to where it started.
			//This is the case the old copy based design could not express without a third acceleration
			// structure that pinned the second one, which pinned the first: chaining refits grew memory for as
			// long as the chain was alive.
			//In place it is the same object every time, so this costs nothing at all, and landing back on the
			// original result proves a second refit reads the state the first one left rather than the state
			// the AS was originally built from.

			ListTLASInstance backInstances = (ListTLASInstance) { 0 };
			ListTLASInstance_createRefConst(&instance, 1, &backInstances, NULL);

			if (Test_assert(t, "setInstancesBack", TLASRef_setInstancesExt(tlas, &backInstances, &t->err))) {

				Test_assert(t, "beginRefitBack", CommandListRef_begin(refitList, true, U64_MAX, &t->err));

				Test_assert(t, "scopeRefitBack", CommandListRef_startScope(refitList, NULL, 4, NULL, &t->err));
				Test_assert(t, "updateTlasRefitBack", CommandListRef_updateTLASExt(refitList, tlas, &t->err));
				Test_assert(t, "scopeRefitBackEnd", CommandListRef_endScope(refitList, &t->err));

				Test_assert(t, "scopeTraceRefitBack", CommandListRef_startScope(
					refitList, &traceTransitionList, 5, NULL, &t->err
				));

				Test_assert(t, "bindPipelineRefitBack", CommandListRef_setRaytracingPipeline(
					refitList, pipeline, &t->err
				));

				Test_assert(t, "traceRefitBack", CommandListRef_dispatch1DRaysExt(refitList, 0, 4, &t->err));
				Test_assert(t, "scopeTraceRefitBackEnd", CommandListRef_endScope(refitList, &t->err));
				Test_assert(t, "endRefitBack", CommandListRef_end(refitList, &t->err));

				if (TestShaders_submitAndWait(t, deviceRef, refitList, &appData, sizeof(appData)))
					if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

						const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

						Test_assert(
							t, "rayResultsRefitBack",
							values[0] == 1 && values[1] == 1 && !values[2] && !values[3]
						);
					}
			}
		}

		//Opacity micromaps run here rather than at the end, because the BLAS refit below rewrites the position
		// buffer these BLASes are built over and would leave them tracing a triangle that moved away.

		TestShaders_ommSpecialIndex(t, deviceRef, &file, positions, output, emptyList);
		TestShaders_ommMicromapArray(t, deviceRef, &file, positions, output, emptyList);

		//The same idea one level down, so the BLAS update path gets the same treatment.
		//Here the triangle itself moves rather than the instance, by rewriting the position buffer the BLAS
		// already reads; the TLAS is refitted straight after because an instance caches the bounds of the BLAS
		// it points at, so a BLAS that changed leaves every TLAS over it stale.

		Bool madeBlasRefit = Test_assert(t, "createBlasRefitList", GraphicsDeviceRef_createCommandList(
			deviceRef, 2 * KIBI, 32, 16, true, &blasRefitList, &t->err
		));

		if (madeBlasRefit) {

			F32 *positionData2 = (F32*) DeviceBufferRef_ptr(positions)->cpuData.ptrNonConst;

			for(U64 i = 0; i < 3; ++i)
				positionData2[i * 4 + 2] = 1000;                //Z of every vertex, the stride is 4 floats

			madeBlasRefit = Test_assert(t, "markPositionsDirty", DeviceBufferRef_markDirty(
				positions, 0, sizeof(triangle), &t->err
			));
		}

		if (madeBlasRefit) {

			Test_assert(t, "beginBlasRefit", CommandListRef_begin(blasRefitList, true, U64_MAX, &t->err));

			Test_assert(t, "scopeBlasRefit", CommandListRef_startScope(blasRefitList, NULL, 6, NULL, &t->err));
			Test_assert(t, "updateBlasRefit", CommandListRef_updateBLASExt(blasRefitList, blas, &t->err));
			Test_assert(t, "scopeBlasRefitEnd", CommandListRef_endScope(blasRefitList, &t->err));

			Test_assert(t, "scopeTlasAfterBlas", CommandListRef_startScope(blasRefitList, NULL, 7, NULL, &t->err));
			Test_assert(t, "updateTlasAfterBlas", CommandListRef_updateTLASExt(blasRefitList, tlas, &t->err));
			Test_assert(t, "scopeTlasAfterBlasEnd", CommandListRef_endScope(blasRefitList, &t->err));

			Test_assert(t, "scopeTraceBlasRefit", CommandListRef_startScope(
				blasRefitList, &traceTransitionList, 8, NULL, &t->err
			));

			Test_assert(t, "bindPipelineBlasRefit", CommandListRef_setRaytracingPipeline(
				blasRefitList, pipeline, &t->err
			));

			Test_assert(t, "traceBlasRefit", CommandListRef_dispatch1DRaysExt(blasRefitList, 0, 4, &t->err));
			Test_assert(t, "scopeTraceBlasRefitEnd", CommandListRef_endScope(blasRefitList, &t->err));

			Test_assert(t, "endBlasRefit", CommandListRef_end(blasRefitList, &t->err));

			if (TestShaders_submitAndWait(t, deviceRef, blasRefitList, &appData, sizeof(appData)))
				if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

					const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;
					Test_assert(t, "rayResultsBlasRefit", !values[0] && !values[1] && !values[2] && !values[3]);
				}
		}

	}

clean:

	RefPtr_dec(&blasRefitList);
	RefPtr_dec(&refitList);
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

void Test_graphicsShaderRays(Test *t, GraphicsDeviceRef *deviceRef) {

	TestShaders_raysWithFile(t, deviceRef, "Shaders/rays", "//OxC3_gtest/test_shaders/test_rays.oiSH", true);

	//The SER variant records the hit as a HitObject, hints the scheduler with MaybeReorderThread,
	// then invokes the recorded shader explicitly.
	//Reordering itself is unobservable by design,
	// so what is checked is that the split path lands on exactly the same payloads as the plain TraceRay above.
	//Running with the reorder hint in place is also the only "doesn't break" coverage RayReorderActual can
	// get: a device that claims to actually reorder still has to produce identical results.
	//An experimental claim is skipped: on Vulkan the NV device extension can't accept the EXT SPIR-V the
	// shader stack emits, and on D3D12 the SM6.9 the shaders need is preview only.

	const GraphicsDeviceCapabilities caps = GraphicsDeviceRef_ptr(deviceRef)->info.capabilities;

	if(!(caps.features & EGraphicsFeatures_RayReorder))
		return;

	if (caps.experimentalFeatures & EGraphicsFeatures_RayReorder) {
		Test_print(t, "RayReorder claimed but experimental on this backend, skipping SER trace test");
		return;
	}

	TestShaders_raysWithFile(
		t, deviceRef, "Shaders/raysSer", "//OxC3_gtest/test_shaders/test_rays_ser.oiSH", false
	);
}
