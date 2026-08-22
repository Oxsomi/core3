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

//graphics/test/interface/test_graphics_shaders_helpers.cpp
//
//The TestShaders_ helpers the whole graphics suite is built on: loading an oiSH, resolving an entry,
//building a pipeline, submitting and waiting, and pulling a resource back for the CPU to check.
//These are declared in test_graphics_shared.h and used well beyond this directory's own modules.
//Split out of test_graphics_shaders.c, which had grown past 2300 lines.

namespace oxc { namespace c {
	#include "types/base/string_base.h"
	#include "types/container/buffer.h"
	#include "types/container/memory_stream.h"
	#include "types/container/texture_format.h"
	#include "types/test/test.h"
	#include "formats/oiSH/sh_file.h"
	#include "formats/oiSH/sh_registers.h"
	#include "platforms/file.h"
	#include "platforms/logx.h"
	#include "platforms/platform.h"
	#include "graphics/generic/bindless_descriptor.h"
	#include "graphics/generic/blas.h"
	#include "graphics/generic/command_list.h"
	#include "graphics/generic/commands.h"
	#include "graphics/generic/depth_stencil.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_buffer.h"
	#include "graphics/generic/device_info.h"
	#include "graphics/generic/instance.h"
	#include "graphics/generic/opacity_micromap.h"
	#include "graphics/generic/descriptor_layout.h"
	#include "graphics/generic/pipeline.h"
	#include "graphics/generic/pipeline_layout.h"
	#include "graphics/generic/render_texture.h"
	#include "graphics/generic/texture.h"
	#include "graphics/generic/tlas.h"
	#include "test_graphics_shared.h"
} }

//Same namespace the C headers landed in, so the definitions here match the declarations in
//test_graphics_shared.h and the macros in those headers still expand to names that resolve.

namespace oxc { namespace c {

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

//A pipeline layout that keeps the runtime's own bindless set and per frame globals, and adds whatever push
//constants the entry declares.
//
//A shader that reads a push constant needs a layout that declares it. Composing from the device's own
//layouts rather than building a fresh one is what keeps the bindless handles and the globals reachable
//from the same shader.
//
//On DXIL a push constant reflects as the implicit $Globals cbuffer, so it cannot be found by name;
//AssumePushConstants is unambiguous here because detect skips the reserved space the globals live in.

Bool TestShaders_pushConstantLayout(
	Test *t, GraphicsDeviceRef *deviceRef, const SHFile *file, U32 entryId, PipelineLayoutRef **layout
) {

	const Allocator *alloc = Platform_instance->alloc;
	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	DescriptorLayoutInfo layoutInfo {};
	DescriptorBinding pushConstants {};
	Bool success = false;

	if(!Test_assert(t, "detectPushLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, file, entryId, EDescriptorLayoutFlags_None,
		EDetectDescriptorLayoutFlags_AssumePushConstants,
		NULL, NULL, &pushConstants, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "detectedPushConstants", pushConstants.count != 0))
		goto clean;

	//Detect reports the stage of the entry it was given, but a graphics pipeline reads the block from its
	//pixel stage and a raytracing one from raygen, miss and hit alike. A range whose visibility misses a
	//stage that reads it is rejected at pipeline creation, so every stage the tests use is named here.

	pushConstants.visibility =
		((U32)1 << ESHPipelineStage_Vertex) | ((U32)1 << ESHPipelineStage_Pixel) |
		((U32)1 << ESHPipelineStage_Compute) |
		((U32)1 << ESHPipelineStage_RaygenExt) | ((U32)1 << ESHPipelineStage_CallableExt) |
		((U32)1 << ESHPipelineStage_MissExt) | ((U32)1 << ESHPipelineStage_ClosestHitExt) |
		((U32)1 << ESHPipelineStage_AnyHitExt) | ((U32)1 << ESHPipelineStage_IntersectionExt);

	{
		const PipelineLayoutInfo pipelineLayoutInfo = {
			.bindings = device->defaultDescLayout,
			.pushDescriptors = device->defaultCBufferLayout,
			.pushConstants = pushConstants
		};

		const CharString name = CharString_createRefCStrConst("Shader test push constant layout");

		success = Test_assert(t, "createPushLayout", GraphicsDeviceRef_createPipelineLayout(
			deviceRef, &pipelineLayoutInfo, &name, layout, &t->err
		));
	}

clean:
	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	return success;
}

Bool TestShaders_computePipeline(
	Test *t, GraphicsDeviceRef *deviceRef, const SHFile *file, PipelineRef **pipeline
) {
	return TestShaders_computePipelinePush(t, deviceRef, file, pipeline, NULL);
}

//As above, but the shader reads push constants, so it needs a layout that declares them.
//layoutOut is optional and hands back the layout the caller has to release; passing NULL builds the pipeline
//on the device's default layout, which is what a shader without push constants wants.

Bool TestShaders_computePipelinePush(
	Test *t, GraphicsDeviceRef *deviceRef, const SHFile *file, PipelineRef **pipeline,
	PipelineLayoutRef **layoutOut
) {

	const U32 id = TestShaders_entry(t, deviceRef, file, "main");

	if(id == U32_MAX)
		return false;

	PipelineLayoutRef *layout = NULL;

	if(layoutOut) {

		if(!TestShaders_pushConstantLayout(t, deviceRef, file, id, &layout))
			return false;

		*layoutOut = layout;
	}

	const CharString entryName = CharString_createRefCStrConst("main");
	const CharString name = CharString_createRefCStrConst("Shader test compute pipeline");

	return Test_assert(t, "createComputePipeline", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, file, &name, id, &entryName, EPipelineFlags_None, layout, pipeline, &t->err
	));
}

Bool TestShaders_submitAndWait(Test *t, GraphicsDeviceRef *deviceRef, CommandListRef *commandList) {

	ListCommandListRef lists {};
	ListCommandListRef_createRefConst(&commandList, 1, &lists, NULL);

	const Bool ok = Test_assert(t, "submit", GraphicsDeviceRef_submitCommands(
		deviceRef, &lists, NULL, 0, 0, &t->err
	));

	return Test_assert(t, "wait", GraphicsDeviceRef_wait(deviceRef, &t->err)) && ok;
}

//D3D12's GPU based validation instruments raytracing libs into invalid bytecode
// ("Internal declaration 'GBV_Debug_Resource' is unused"), hardware then hangs executing it and even
// WARP's surviving run leaves those bytecode errors in the counters, failing the validation clean check.
//So on D3D12 with it enabled, raytracing modules trace on their own device with only GPU based validation
// off, which keeps the coverage and still hard checks that instance's counters at the end.

Bool TestShaders_rtDedicatedDevice(
	Test *t,
	GraphicsDeviceRef **deviceRef,
	GraphicsInstanceRef **ownInstanceRef,
	GraphicsDeviceRef **ownDeviceRef,
	RefPtrType *instanceType
) {

	const Allocator *alloc = Platform_instance->alloc;
	const GraphicsDevice *device = GraphicsDeviceRef_ptr(*deviceRef);
	const GraphicsInstance *suiteInstance = GraphicsInstanceRef_ptr(device->instance);

	const Bool gpuValidationOn =
		(suiteInstance->flags & EGraphicsInstanceFlags_IsDebug) &&
		!(suiteInstance->flags & EGraphicsInstanceFlags_DisableGPUBV);

	if (suiteInstance->api != EGraphicsApi_Direct3D12 || !gpuValidationOn)
		return true;

	Test_print(t, "D3D12 GPU based validation breaks raytracing state objects, tracing on a dedicated device");

	GraphicsApplicationInfo appInfo = {
		.name = CharString_createRefCStrConst("OxC3 ray trace test"),
		.version = 1
	};

	*instanceType = GraphicsInstance_makeType(suiteInstance->api, alloc);
	ListGraphicsDeviceInfo deviceInfos {};

	if(!Test_assert(t, "createOwnInstance", GraphicsInstance_create(
		&appInfo, suiteInstance->api, EGraphicsInstanceFlags_DisableGPUBV, alloc, instanceType, ownInstanceRef,
		&t->err
	)))
		return false;

	//The adapter has to be the same one the suite handed us, matched by name

	GraphicsInstance *ownInstance = GraphicsInstanceRef_ptr(*ownInstanceRef);
	Bool created = false;

	if (Test_assert(t, "ownDeviceInfos", GraphicsInstance_getDeviceInfos(ownInstance, &deviceInfos, &t->err))) {

		for(U64 i = 0; i < deviceInfos.length; ++i)
			if (Buffer_eq(
				Buffer_createRefConst(deviceInfos.ptr[i].name, sizeof(deviceInfos.ptr[i].name)),
				Buffer_createRefConst(device->info.name, sizeof(device->info.name))
			)) {
				created = Test_assert(t, "createOwnDevice", GraphicsDeviceRef_create(
					*ownInstanceRef, &deviceInfos.ptr[i], EGraphicsDeviceFlags_None,
					EGraphicsBufferingMode_Default, NULL, ownDeviceRef, &t->err
				));
				break;
			}
	}

	ListGraphicsDeviceInfo_free(&deviceInfos, alloc);

	if (!created) {
		RefPtr_dec(ownInstanceRef);
		return false;
	}

	*deviceRef = *ownDeviceRef;
	return true;
}

//The dedicated device's run has to be validation clean too, its counters just live on the own instance

void TestShaders_rtDedicatedDeviceEnd(Test *t, GraphicsInstanceRef **ownInstanceRef, GraphicsDeviceRef **ownDeviceRef) {

	if (*ownDeviceRef) {

		GraphicsDeviceRef_wait(*ownDeviceRef, NULL);
		RefPtr_dec(ownDeviceRef);

		GraphicsInstance *ownInstance = GraphicsInstanceRef_ptr(*ownInstanceRef);
		Test_assert(t, "ownValidationErrors", !GraphicsInstance_getValidationErrors(ownInstance));
		Test_assert(t, "ownValidationWarnings", !GraphicsInstance_getValidationWarnings(ownInstance));
	}

	RefPtr_dec(ownInstanceRef);
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

	ok &= TestShaders_submitAndWait(t, deviceRef, emptyList);
	return Test_assert(t, "bufferPullCompleted", pulled == 1) && ok;
}

//Texture pulls hand over an owned buffer; the callback copies out the 8x8 payload the checks below compare

static void TestShaders_pixelPull(RefPtr *resource, Buffer *data, void *context) {

	(void) resource;

	TestShaderPixels *result = (TestShaderPixels*) context;

	++result->count;
	result->len = data ? Buffer_length(*data) : 0;

	for(U64 i = 0; i < 64 && (i + 1) * 4 <= result->len; ++i)
		result->pixels[i] = ((const U32*)data->ptr)[i];
}

Bool TestShaders_pullPixels(
	Test *t, GraphicsDeviceRef *deviceRef, CommandListRef *emptyList, RefPtr *target, TestShaderPixels *pixels
) {

	Bool ok = Test_assert(t, "queuePixelPull", TextureRef_pullRegion(
		target, 0, 0, 0, 0, 0, 0, 0, TestShaders_pixelPull, pixels, &t->err
	));

	ok &= TestShaders_submitAndWait(t, deviceRef, emptyList);
	ok &= Test_assert(t, "pixelPullCompleted", pixels->count == 1);
	return Test_assert(t, "pixelPullLen", pixels->len == 64 * 4) && ok;
}

Bool TestShaders_checkPixels(
	Test *t, GraphicsDeviceRef *deviceRef, CommandListRef *emptyList, RefPtr *target, U32 expected
) {

	TestShaderPixels pixels {};

	if(!TestShaders_pullPixels(t, deviceRef, emptyList, target, &pixels))
		return false;

	U32 matching = 0;

	for(U64 i = 0; i < 64; ++i)
		matching += pixels.pixels[i] == expected;

	return Test_assert(t, "pixelsMatch", matching == 64);
}
} }
