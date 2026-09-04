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
//Split out of test_graphics_shaders.c, which had grown past 2300 lines.
//
//These are written against the C++ layer and declared in test_graphics_shared.hpp; the C forms in
//test_graphics_shared.h forward to them, which is what lets a module that hasn't been converted yet keep
//calling the same helper. Loading and the pull callbacks stay on the C API on purpose: files, streams and
//oiSH parsing are not graphics/graphics.hpp's domain, and a callback crossing the C boundary keeps a
//builtin-only signature.

#include "test_graphics_shared.hpp"

//Log::debugLn is the C++ front for Log_debugLnx, whose x macros name ELogOptions_NewLine unqualified and
//so cannot be reached through the c namespace.

#include "types/container/log.hpp"

namespace oxc { namespace c {
	#include "types/base/string_base.h"
	#include "types/container/buffer.h"
	#include "types/container/memory_stream.h"
	#include "types/container/texture_format.h"
	#include "formats/oiSH/sh_file.h"
	#include "formats/oiSH/sh_registers.h"
	#include "platforms/file.h"
	#include "platforms/logx.h"
	#include "graphics/generic/device_info.h"
	#include "graphics/generic/texture.h"
}}

namespace oxc { namespace gfxtest {

	using namespace gfx;

	//The gtest section only holds compiled oiSH files when the build had the shader compiler, so absence
	//means skip. Loading the section twice is harmless, which keeps every module self contained.

	c::Bool loadFile(c::Test *t, const c::C8 *pathStr, c::SHFile &file) noexcept {

		const c::Allocator *alloc = c::Platform_instance->alloc;

		const c::CharString section = c::CharString_createRefCStrConst("//OxC3_gtest");
		const c::CharString path = c::CharString_createRefCStrConst(pathStr);
		const c::RefPtrType memStreamType = c::MemoryStream_makeType(alloc);
		const c::RefPtrType fileHandleType = c::FileHandle_makeType(alloc);

		c::File_loadVirtual(&section, &memStreamType, nullptr, nullptr, alloc, nullptr);

		if(!c::File_hasFile(&path, alloc))
			return false;

		c::Buffer data = c::Buffer_createNull();
		c::MemoryStreamRef *stream = nullptr;
		c::Bool loaded = false;

		if(!c::Test_assert(t, "readShaderFile", c::File_read(
			&path, c::U64_MAX, 0, 0, &fileHandleType, &data, &t->err
		)))
			return false;

		c::U64 streamOffset = 0;

		if(c::Test_assert(t, "createShaderStream", c::MemoryStream_createFromBufferRegion(
			c::Buffer_createRefFromBuffer(data, true), 0, c::Buffer_length(data), c::EMemoryStreamFlags_None,
			&memStreamType, &stream, &t->err
		)) && stream)
			loaded = c::Test_assert(t, "parseShaderFile", c::SHFile_read(
				(c::StreamRef*) stream, &streamOffset, false, alloc, &file, &t->err
			));

		c::RefPtr_dec(&stream);
		c::Buffer_free(&data, alloc);

		return loaded;
	}

	//Every test shader file holds a single stage; only the raytracing file carries more than one named entry

	c::U32 entry(c::Test *t, Device &dev, const c::SHFile &file, const c::C8 *name) noexcept {

		const c::U32 id = dev.getFirstShaderEntry(file, name);

		c::Test_assert(t, "entryFound", id != c::U32_MAX);
		return id;
	}

	//A pipeline layout that keeps the runtime's own bindless set and per frame globals, and adds whatever
	//push constants the entry declares.
	//
	//Composing from the device's own layouts rather than building a fresh one is what keeps the bindless
	//handles and the globals reachable from the same shader.
	//
	//On DXIL a push constant reflects as the implicit $Globals cbuffer, so it cannot be found by name;
	//AssumePushConstants is unambiguous here because detect skips the reserved space the globals live in.

	c::Bool pushConstantLayout(
		c::Test *t, Device &dev, const c::SHFile &file, c::U32 entryId, PipelineLayout &layout
	) noexcept {

		c::Error *e_rr = &t->err;

		OwnedLayoutInfo layoutInfo(dev.alloc());
		c::DescriptorBinding pushConstants{};

		if(!c::Test_assert(t, "detectPushLayout", dev.detectLayout(
			file, entryId, layoutInfo.list, nullptr, &pushConstants, {}, nullptr,
			c::EDescriptorLayoutFlags_None, c::EDetectDescriptorLayoutFlags_AssumePushConstants, e_rr
		)))
			return false;

		if(!c::Test_assert(t, "detectedPushConstants", pushConstants.count != 0))
			return false;

		//Detect reports the stage of the entry it was given, but a graphics pipeline reads the block from
		//its pixel stage and a raytracing one from raygen, miss and hit alike. A range whose visibility
		//misses a stage that reads it is rejected at pipeline creation, so every stage the tests use is
		//named here.

		pushConstants.visibility =
			((c::U32)1 << c::EGfxPipelineStage_Vertex) | ((c::U32)1 << c::EGfxPipelineStage_Pixel) |
			((c::U32)1 << c::EGfxPipelineStage_Compute) |
			((c::U32)1 << c::EGfxPipelineStage_RaygenExt) | ((c::U32)1 << c::EGfxPipelineStage_CallableExt) |
			((c::U32)1 << c::EGfxPipelineStage_MissExt) | ((c::U32)1 << c::EGfxPipelineStage_ClosestHitExt) |
			((c::U32)1 << c::EGfxPipelineStage_AnyHitExt) | ((c::U32)1 << c::EGfxPipelineStage_IntersectionExt);

		c::PipelineLayoutInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.bindings = dev.defaultDescLayout();
		pipelineLayoutInfo.pushDescriptors = dev.defaultCBufferLayout();
		pipelineLayoutInfo.pushConstants = pushConstants;

		return c::Test_assert(t, "createPushLayout", dev.createPipelineLayout(
			pipelineLayoutInfo, "Shader test push constant layout", layout, e_rr
		));
	}

	c::Bool computePipeline(c::Test *t, Device &dev, const c::SHFile &file, Pipeline &pipeline) noexcept {

		if(entry(t, dev, file, "main") == c::U32_MAX)
			return false;

		return c::Test_assert(t, "createComputePipeline", dev.createComputePipeline(
			file, "main", "Shader test compute pipeline", pipeline, {}, nullptr, &t->err
		));
	}

	//As above, but the shader reads push constants, so it needs a layout that declares them.
	//The layout has to outlive the pipeline, so it is handed back rather than released here.

	c::Bool computePipelinePush(
		c::Test *t, Device &dev, const c::SHFile &file, Pipeline &pipeline, PipelineLayout &layout
	) noexcept {

		const c::U32 id = entry(t, dev, file, "main");

		if(id == c::U32_MAX)
			return false;

		if(!pushConstantLayout(t, dev, file, id, layout))
			return false;

		return c::Test_assert(t, "createComputePipeline", dev.createComputePipeline(
			file, "main", "Shader test compute pipeline", pipeline, {}, &layout, &t->err
		));
	}

	c::Bool submitAndWait(c::Test *t, Device &dev, const CommandList &commandList) noexcept {

		const c::Bool ok = c::Test_assert(t, "submit", dev.submit({ &commandList }, {}, 0, 0, &t->err));
		return c::Test_assert(t, "wait", dev.wait(&t->err)) && ok;
	}

	namespace {

		void countPull(void *resource, void *context) {
			(void) resource;
			++*(c::U32*)context;
		}

		//Texture pulls hand over an owned buffer; the callback copies out the 8x8 payload the checks compare.

		void pixelPull(void *resource, void *dataPtr, void *context) {

			(void) resource;

			const c::Buffer *data = (const c::Buffer*) dataPtr;
			c::TestShaderPixels *result = (c::TestShaderPixels*) context;

			++result->count;
			result->len = data ? c::Buffer_length(*data) : 0;

			for(c::U64 i = 0; i < 64 && (i + 1) * 4 <= result->len; ++i)
				result->pixels[i] = ((const c::U32*)data->ptr)[i];
		}
	}

	//Scribbling the CPU copy first makes sure only a real GPU to CPU pull can produce the expected values.

	c::Bool pullBuffer(c::Test *t, Device &dev, const CommandList &emptyList, const DeviceBuffer &buffer) noexcept {

		c::DeviceBuffer *bufferPtr = buffer.data();

		for(c::U64 i = 0; i < c::Buffer_length(bufferPtr->cpuData); ++i)
			bufferPtr->cpuData.ptrNonConst[i] = 0xCC;

		c::U32 pulled = 0;

		c::Bool ok = c::Test_assert(t, "queueBufferPull", c::DeviceBufferRef_pullRegion(
			(c::DeviceBufferRef*) buffer.handle(), 0, 0, countPull, &pulled, &t->err
		));

		ok &= submitAndWait(t, dev, emptyList);
		return c::Test_assert(t, "bufferPullCompleted", pulled == 1) && ok;
	}

	c::Bool pullPixels(
		c::Test *t, Device &dev, const CommandList &emptyList, c::RefPtr *target, c::TestShaderPixels &pixels
	) noexcept {

		c::Bool ok = c::Test_assert(t, "queuePixelPull", c::TextureRef_pullRegion(
			target, 0, 0, 0, 0, 0, 0, 0, pixelPull, &pixels, &t->err
		));

		ok &= submitAndWait(t, dev, emptyList);
		ok &= c::Test_assert(t, "pixelPullCompleted", pixels.count == 1);
		return c::Test_assert(t, "pixelPullLen", pixels.len == 64 * 4) && ok;
	}

	c::Bool checkPixels(
		c::Test *t, Device &dev, const CommandList &emptyList, c::RefPtr *target, c::U32 expected
	) noexcept {

		c::TestShaderPixels pixels{};

		if(!pullPixels(t, dev, emptyList, target, pixels))
			return false;

		c::U32 matching = 0;
		c::U64 firstBad = 64;

		for(c::U64 i = 0; i < 64; ++i) {

			if(pixels.pixels[i] == expected)
				++matching;

			else if(firstBad == 64)
				firstBad = i;
		}

		//Every draw in a module checks its target through this one helper, so a failure otherwise says only
		//"pixelsMatch" with no way to tell WHICH draw produced it or how wrong it was. That is the difference
		//between an intermittent CI failure being diagnosable and being a shrug, so the mismatch says what it
		//actually got and where.

		if(matching != 64)
			Log::debugLn(
				*dev.alloc(),
				"-- pixelsMatch: %" PRIu32 " of 64 texels matched, expected %08X, first mismatch at %"
				PRIu64 " was %08X",
				matching, expected, firstBad, pixels.pixels[firstBad]
			);

		return c::Test_assert(t, "pixelsMatch", matching == 64);
	}

	//The swap itself is the C helper below, which the unconverted modules still call directly; this only
	//makes it scoped and rebinds the caller's Device to whatever came back.

	RtDedicatedDevice::RtDedicatedDevice(c::Test *test, Device &dev) noexcept : t(test) {

		c::GraphicsDeviceRef *raw = (c::GraphicsDeviceRef*) dev.handle();

		ok = c::TestShaders_rtDedicatedDevice(t, &raw, &ownInstance, &ownDevice, &instanceType);

		if(ok && ownDevice) {
			dev = Device::share(ownDevice);
			bound = &dev;
		}
	}

	RtDedicatedDevice::~RtDedicatedDevice() noexcept {

		//The caller's reference goes back FIRST, so the dec inside the end call is the one that destroys the
		//device and its teardown validation output is counted before the asserts read the counters.
		//Every call site declares its resource handles after this object, so they are already released by
		//now and nothing reaches for the device after this point.

		if(bound)
			bound->release();

		c::TestShaders_rtDedicatedDeviceEnd(t, &ownInstance, &ownDevice);
	}
}}

//The C declarations in test_graphics_shared.h, forwarding to the implementations above.
//They exist for the modules that still record against the C API; each one goes away with its last caller.

namespace oxc { namespace c {

	Bool TestShaders_loadFile(Test *t, const C8 *pathStr, SHFile *file) {
		return gfxtest::loadFile(t, pathStr, *file);
	}

	U32 TestShaders_entry(Test *t, GraphicsDeviceRef *deviceRef, const SHFile *file, const C8 *name) {
		gfx::Device dev = gfx::Device::share(deviceRef);
		return gfxtest::entry(t, dev, *file, name);
	}

	//The C caller owns what it is handed, so the reference is taken before the handle releases its own.

	Bool TestShaders_pushConstantLayout(
		Test *t, GraphicsDeviceRef *deviceRef, const SHFile *file, U32 entryId, PipelineLayoutRef **layout
	) {

		gfx::Device dev = gfx::Device::share(deviceRef);
		gfx::PipelineLayout out;

		if(!gfxtest::pushConstantLayout(t, dev, *file, entryId, out))
			return false;

		*layout = (PipelineLayoutRef*) out.handle();
		RefPtr_inc(*layout);
		return true;
	}

	Bool TestShaders_computePipeline(
		Test *t, GraphicsDeviceRef *deviceRef, const SHFile *file, PipelineRef **pipeline
	) {
		return TestShaders_computePipelinePush(t, deviceRef, file, pipeline, NULL);
	}

	Bool TestShaders_computePipelinePush(
		Test *t, GraphicsDeviceRef *deviceRef, const SHFile *file, PipelineRef **pipeline,
		PipelineLayoutRef **layoutOut
	) {

		gfx::Device dev = gfx::Device::share(deviceRef);
		gfx::Pipeline outPipeline;
		gfx::PipelineLayout outLayout;

		const Bool ok =
			layoutOut ?
			gfxtest::computePipelinePush(t, dev, *file, outPipeline, outLayout) :
			gfxtest::computePipeline(t, dev, *file, outPipeline);

		if(!ok)
			return false;

		if (layoutOut) {
			*layoutOut = (PipelineLayoutRef*) outLayout.handle();
			RefPtr_inc(*layoutOut);
		}

		*pipeline = (PipelineRef*) outPipeline.handle();
		RefPtr_inc(*pipeline);
		return true;
	}

	Bool TestShaders_submitAndWait(Test *t, GraphicsDeviceRef *deviceRef, CommandListRef *commandList) {

		gfx::Device dev = gfx::Device::share(deviceRef);
		gfx::CommandList list{ ::oxc::RefPtr<CommandList>::share(commandList) };

		return gfxtest::submitAndWait(t, dev, list);
	}

	Bool TestShaders_pullBuffer(
		Test *t, GraphicsDeviceRef *deviceRef, CommandListRef *emptyList, DeviceBufferRef *buffer
	) {

		gfx::Device dev = gfx::Device::share(deviceRef);
		gfx::CommandList list{ ::oxc::RefPtr<CommandList>::share(emptyList) };
		gfx::DeviceBuffer buf{ ::oxc::RefPtr<DeviceBuffer>::share(buffer) };

		return gfxtest::pullBuffer(t, dev, list, buf);
	}

	Bool TestShaders_pullPixels(
		Test *t, GraphicsDeviceRef *deviceRef, CommandListRef *emptyList, RefPtr *target, TestShaderPixels *pixels
	) {

		gfx::Device dev = gfx::Device::share(deviceRef);
		gfx::CommandList list{ ::oxc::RefPtr<CommandList>::share(emptyList) };

		return gfxtest::pullPixels(t, dev, list, target, *pixels);
	}

	Bool TestShaders_checkPixels(
		Test *t, GraphicsDeviceRef *deviceRef, CommandListRef *emptyList, RefPtr *target, U32 expected
	) {

		gfx::Device dev = gfx::Device::share(deviceRef);
		gfx::CommandList list{ ::oxc::RefPtr<CommandList>::share(emptyList) };

		return gfxtest::checkPixels(t, dev, list, target, expected);
	}

	//Instance and device creation is what this is about, so it stays on the C API: there is nothing for the
	//handle layer to own until the device exists, and the caller is handed raw refs either way.

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
						EGraphicsBufferingMode_Default, NULL, NULL, ownDeviceRef, &t->err
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
}}
