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

//graphics/test/interface/test_graphics_interface.cpp

//Graphics interface (non-functional) unit tests.
//
//What is covered (headless-safe, always runs):
//  1. GraphicsInterface - create, api support query, EGraphicsApi_resolve
//  2. Instance type     - GraphicsInstance_makeType invariants
//  3. Instance          - create / free, double-dec safety, parameter validation
//  4. Object type table - RefPtrType invariants for all child object kinds
//  5. Texture formats   - format helper sanity (types/device_info)
//  6. Bindless packing  - BindlessDescriptor pack3/unpack3 and handle accessors (pure, no device needed)
// 15. Null device       - every resource creator rejects a NULL device instead of faulting on it
// 18. Descriptor pack   - buffer region + counter offset bit packing, texture/tlas/sampler union
// 19. TextureRange      - extent helpers
// 24. Default layout    - the bindless layout OxC3 shaders compile against (DXIL chain law, SPIRV sets, rt gating)
//
//What is covered when an adapter is present (skipped with a message otherwise;
//CI Linux may provide lavapipe, but a GPU is never guaranteed):
//  7. Device enumeration - getDeviceInfos / getPreferredDevice (+ validation)
//  8. Device             - create / wait / free
//  9. DeviceBuffer       - create, CPU-backed create, markDirty validation
// 10. Swapchain          - rejects a NULL window; a virtual one presents to memory (own module)
// 11. DescriptorLayout   - explicit binding set, layout/table invariants, parameter validation
// 12. Bindless           - allocate / free / reuse a descriptor in the device's default table
// 13. DeviceBuffer       - ExposeBindlessRead/Write only take a descriptor when asked
// 16. Submit             - begin/end state machine, empty frame submit (the only path that binds descriptors)
// 25. Sampler/data       - sampler field validation, createBufferData move/copy split, texture markDirty
// 26. PipelineLayout     - push constant limits, push descriptor flag routing
// 27. Shader reflection  - oiSH entry lookup, layout detection, checkShaderFeatures, compute pipeline creation
// 28. Device memory      - budget queries, staging buffer resize, handleNextFrame lock contract
// 29. GPU execution      - clear + cross scope copy + upload actually replayed on the device, twice
// 30. Acceleration structures - BLAS/TLAS creation, instance plumbing, real builds via submit (rt devices only)
// 31. Compute execution  - dispatch + CPU/GPU written indirect dispatch, results read back and compared
// 32. Draw execution     - triangle, scissor, blend, indexed/instanced, depth, indirect and MSAA resolve draws
// 33. Ray trace execution - a raytracing pipeline traces hit and miss rays against a real TLAS
//
//This file only keeps the interface/instance/device orchestration (1-4, 7-10, 15) and the entry point;
// the modules live in their own files so each area has room to grow:
//  test_graphics_format.c       - 5, 6, 17, 18, 19, 24 (pure, headless)
//  test_graphics_command_list.c - 14, 20, 21, 22 (recording, validation, render passes)
//  test_graphics_descriptors.c  - 11, 12, 13
//  test_graphics_resources.c    - 23, 25, 26, 27
//  test_graphics_execute.c      - 16, 28, 29, 30 (submission, readback, acceleration structures)
//  test_graphics_shaders.c      - 31, 32, 33 (shader execution against the //OxC3_gtest test shaders)
//  test_graphics_formats_frames.c - 34, 35, 36 (per format round trips, shape gates, frame ring)
//  test_graphics_capabilities.c - 37 (capability bit invariants and feature gated API agreement)
//  test_graphics_caps_exec.c    - 38 (a shader dispatched per capability, results verified)
//
//Numbering follows the order the modules were added, not the order they run in.
//
//Run in CI, no display, no human interaction required.

//The shared helpers in terms of the handle types, plus the shims for the macros a module cannot reach
//through a namespace qualifier.
//Both C++ headers come BEFORE the block below: a standard header included after the C headers landed in
//oxc::c finds its guard already tripped and leaves its symbols in that namespace.

#include "test_graphics_shared.hpp"

//Log::debugLn is the C++ front for Log_debugLnx; the x macros name ELogOptions_NewLine unqualified and so
//cannot be reached through the c namespace.

#include "types/container/log.hpp"

namespace oxc { namespace c {
	#include "types/base/error.h"
	#include "types/base/string_base.h"
	#include "types/container/memory_stream.h"
	#include "types/container/texture_format.h"
	#include "types/test/test.h"
	#include "formats/oiSH/sh_registers.h"
	#include "platforms/file.h"
	#include "platforms/logx.h"
	#include "platforms/platform.h"
	#include "platforms/window.h"
	#include "graphics/generic/blas.h"
	#include "graphics/generic/command_list.h"
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
	#include "graphics/generic/opacity_micromap.h"
	#include "graphics/generic/pipeline.h"
	#include "graphics/generic/pipeline_layout.h"
	#include "graphics/generic/render_texture.h"
	#include "graphics/generic/sampler.h"
	#include "graphics/generic/swapchain.h"
	#include "graphics/generic/tlas.h"
	#include "test_graphics_shared.h"
} }

//One file scope using rather than one per function: this module has a dozen static helpers besides
//its entry points, and they all reach c:: the same way.

using namespace oxc;

// -- 1. GraphicsInterface ------------------------------------------------------

static void Test_graphicsInterface(c::Test *t) {

	c::Test_setModule(t, "GraphicsInterface");

	Test_assert(t, "create", c::GraphicsInterface_create(&t->err));
	Test_assert(t, "createTwice", c::GraphicsInterface_create(&t->err));        //Guards against multiple inits

	//Resolving the default api must give a real api that's supported

	c::EGraphicsApi def = c::EGraphicsApi_resolve(c::EGraphicsApi_Count);
	Test_assert(t, "resolveDefault", def < c::EGraphicsApi_Count);
	Test_assert(t, "defaultSupported", c::GraphicsInterface_supportsApi(def));
	Test_assert(t, "resolveIdentity", c::EGraphicsApi_resolve(def) == def);
}

// -- 2. GraphicsInstance_makeType ----------------------------------------------

static void Test_graphicsInstanceType(c::Test *t) {

	c::Test_setModule(t, "GraphicsInstance/Type");

	const c::Allocator *alloc = c::Platform_instance->alloc;
	c::RefPtrType type = c::GraphicsInstance_makeType(c::EGraphicsApi_Count, alloc);

	Test_assert(t, "typeId", type.typeId == (c::TypeId) c::EGraphicsTypeId_GraphicsInstance);
	Test_assert(t, "length", c::RefPtrType_length(&type) >= sizeof(c::GraphicsInstance));
	Test_assert(t, "alloc", type.alloc == alloc);
	Test_assert(t, "freeFunc", type.free);
}

// -- 3/4. Instance create / free + object type table ---------------------------

static void Test_checkObjectType(
	c::Test *t, const c::C8 *name, const c::RefPtrType *type, c::TypeId typeId, c::U64 minLen, const c::Allocator *alloc
) {
	Test_assert(t, name, type->typeId == typeId && c::RefPtrType_length(type) >= minLen && type->alloc == alloc && type->free);
}

static void Test_graphicsInstance(c::Test *t) {

	c::Test_setModule(t, "GraphicsInstance");

	const c::Allocator *alloc = c::Platform_instance->alloc;

	c::GraphicsApplicationInfo appInfo = {
		.name = c::CharString_createRefCStrConst("OxC3 graphics interface test"),
		.version = 1
	};

	//TODO: Foreach all supported apis

	//Parameter validation: missing instance output

	c::GraphicsInstanceRef *instRef = NULL;

	c::Error err = c::Error_none();
	c::RefPtrType type = c::GraphicsInstance_makeType(c::EGraphicsApi_Count, alloc);

	Test_assert(t, "createNullApp", !c::GraphicsInstance_create(
		NULL, c::EGraphicsApi_Count, c::EGraphicsInstanceFlags_None, alloc, &type, &instRef, &err
	));
	Test_assert(t, "createNullInst", !c::GraphicsInstance_create(
		&appInfo, c::EGraphicsApi_Count, c::EGraphicsInstanceFlags_None, alloc, &type, NULL, &err
	));
	Test_assert(t, "createNullType", !c::GraphicsInstance_create(
		&appInfo, c::EGraphicsApi_Count, c::EGraphicsInstanceFlags_None, alloc, NULL, NULL, &err
	));

	//Real create.
	//Instance creation legitimately fails on machines without a compatible driver/ICD;
	// that's not a test failure, the device tests are simply skipped (like the adapter check below).

	if (!c::GraphicsInstance_create(
		&appInfo, c::EGraphicsApi_Count, c::EGraphicsInstanceFlags_None, alloc, &type, &instRef, &t->err
	)) {
		c::Test_print(t, "No compatible graphics driver, skipping instance tests");
		t->err = c::Error_none();
		return;
	}

	c::GraphicsInstance *inst = c::instanceOf(instRef);

	Test_assert(t, "instNotNull", inst != NULL);
	Test_assert(t, "apiResolved", inst && inst->api == c::EGraphicsApi_resolve(c::EGraphicsApi_Count));
	Test_assert(t, "allocStored", inst && inst->alloc == alloc);
	Test_assert(t, "refTypeId", instRef->refPtrType->typeId == (c::TypeId) c::EGraphicsTypeId_GraphicsInstance);

	//Object type table invariants: correct typeIds, length can hold the base struct, same alloc, free func set.
	//These types live in the instance so they're guaranteed to outlive the objects created with them.

	if (inst) {

		const c::GraphicsObjectTypes *types = &inst->types;

		Test_checkObjectType(
			t, "device", &types->device, (c::TypeId)c::EGraphicsTypeId_GraphicsDevice, sizeof(c::GraphicsDevice), alloc
		);
		Test_checkObjectType(t, "buffer", &types->buffer, (c::TypeId)c::EGraphicsTypeId_DeviceBuffer, sizeof(c::DeviceBuffer), alloc);

		Test_checkObjectType(
			t, "deviceTexture", &types->deviceTexture,
			(c::TypeId)c::EGraphicsTypeId_DeviceTexture, sizeof(c::DeviceTexture), alloc
		);

		Test_checkObjectType(
			t, "renderTexture", &types->renderTexture,
			(c::TypeId)c::EGraphicsTypeId_RenderTexture, sizeof(c::RenderTexture), alloc
		);

		Test_checkObjectType(
			t, "depthStencil", &types->depthStencil,
			(c::TypeId)c::EGraphicsTypeId_DepthStencil, sizeof(c::DepthStencil), alloc
		);

		Test_checkObjectType(t, "swapchain", &types->swapchain, (c::TypeId)c::EGraphicsTypeId_Swapchain, sizeof(c::Swapchain), alloc);

		//The pipeline kinds share a typeId but each length has to fit its own info block (Pipeline_infoOffset),
		// which is exactly the invariant whose violation used to overrun the heap on graphics pipeline creation

		Test_checkObjectType(
			t, "pipelineCompute", &types->pipelineCompute, (c::TypeId)c::EGraphicsTypeId_Pipeline, sizeof(c::Pipeline), alloc
		);

		Test_checkObjectType(
			t, "pipelineGraphics", &types->pipelineGraphics,
			(c::TypeId)c::EGraphicsTypeId_Pipeline, sizeof(c::Pipeline) + sizeof(c::PipelineGraphicsInfo), alloc
		);

		Test_checkObjectType(
			t, "pipelineRaytracing", &types->pipelineRaytracing,
			(c::TypeId)c::EGraphicsTypeId_Pipeline, sizeof(c::Pipeline) + sizeof(c::PipelineRaytracingInfo), alloc
		);

		Test_checkObjectType(t, "sampler", &types->sampler, (c::TypeId)c::EGraphicsTypeId_Sampler, sizeof(c::Sampler), alloc);
		Test_checkObjectType(t, "blas", &types->blas, (c::TypeId)c::EGraphicsTypeId_BLASExt, sizeof(c::BLAS), alloc);
		Test_checkObjectType(t, "tlas", &types->tlas, (c::TypeId)c::EGraphicsTypeId_TLASExt, sizeof(c::TLAS), alloc);

		Test_checkObjectType(
			t, "opacityMicromap", &types->opacityMicromap,
			(c::TypeId)c::EGraphicsTypeId_OpacityMicromapExt, sizeof(c::OpacityMicromap), alloc
		);

		Test_checkObjectType(
			t, "descriptorLayout", &types->descriptorLayout,
			(c::TypeId)c::EGraphicsTypeId_DescriptorLayout, sizeof(c::DescriptorLayout), alloc
		);

		Test_checkObjectType(
			t, "descriptorTable", &types->descriptorTable,
			(c::TypeId)c::EGraphicsTypeId_DescriptorTable, sizeof(c::DescriptorTable), alloc
		);

		Test_checkObjectType(
			t, "descriptorHeap", &types->descriptorHeap,
			(c::TypeId)c::EGraphicsTypeId_DescriptorHeap, sizeof(c::DescriptorHeap), alloc
		);

		Test_checkObjectType(
			t, "pipelineLayout", &types->pipelineLayout,
			(c::TypeId)c::EGraphicsTypeId_PipelineLayout, sizeof(c::PipelineLayout), alloc
		);

		Test_checkObjectType(
			t, "commandList", &types->commandList,
			(c::TypeId)c::EGraphicsTypeId_CommandList, sizeof(c::CommandList), alloc
		);
	}

	//Opacity micromap special indices are pure value math, so they are checked here rather than on a device.
	//The truncation is the part worth pinning: both APIs define these as negative signed values but read the
	// buffer as unsigned, so the same special index is a different stored value per element width, and getting
	// it wrong silently selects a DIFFERENT special index rather than failing.

	Test_assert(
		t, "ommPackR16u",
		c::EOMMSpecialIndex_pack(c::EOMMSpecialIndex_FullyTransparent, c::ETextureFormatId_R16u) == 0xFFFF
	);

	Test_assert(
		t, "ommPackR32u",
		c::EOMMSpecialIndex_pack(c::EOMMSpecialIndex_FullyTransparent, c::ETextureFormatId_R32u) == 0xFFFFFFFF
	);

	Test_assert(
		t, "ommPackUnknownOpaque",
		c::EOMMSpecialIndex_pack(c::EOMMSpecialIndex_FullyUnknownOpaque, c::ETextureFormatId_R32u) == 0xFFFFFFFC
	);

	Test_assert(
		t, "ommPackNarrowDiffers",
		c::EOMMSpecialIndex_pack(c::EOMMSpecialIndex_FullyOpaque, c::ETextureFormatId_R16u) == 0xFFFE
	);

	//A format that carries no OMM has no element to write, so it packs to nothing rather than to a real value.

	Test_assert(
		t, "ommPackUndefined",
		!c::EOMMSpecialIndex_pack(c::EOMMSpecialIndex_FullyTransparent, c::ETextureFormatId_Undefined)
	);

	Test_assert(t, "ommIndexMaxR16u", c::EOMMIndex_max(c::ETextureFormatId_R16u) == 0xFFFB);
	Test_assert(t, "ommIndexMaxR32u", c::EOMMIndex_max(c::ETextureFormatId_R32u) == 0xFFFFFFFB);

	Test_assert(t, "ommIndexSpecialTop", c::EOMMIndex_isSpecial(0xFFFF, c::ETextureFormatId_R16u));
	Test_assert(t, "ommIndexNotSpecial", !c::EOMMIndex_isSpecial(0xFFFB, c::ETextureFormatId_R16u));

	//0xFFFF is a real index at 32 bit width and a special one at 16, so the check has to be format driven.

	Test_assert(t, "ommIndexWidthMatters", !c::EOMMIndex_isSpecial(0xFFFF, c::ETextureFormatId_R32u));

	//Free through the generic ref counter; double-dec must be safe (pointer is NULLed)

	c::RefPtr_dec(&instRef);
	Test_assert(t, "freeNulled", instRef == NULL);

	c::RefPtr_dec(&instRef);
	Test_assert(t, "doubleDecSafe", instRef == NULL);
}

// -- 7-10. Device, DeviceBuffer and Swapchain ------------------------------------

//One adapter's full run: create the device, run every device scoped module, tear down again

static void Test_graphicsDeviceSingle(c::Test *t, c::GraphicsInstanceRef *instRef, const c::GraphicsDeviceInfo *info) {

	c::Test_setModule(t, "GraphicsDevice");

	c::GraphicsInstance *inst = c::instanceOf(instRef);
	const c::Allocator *alloc = inst->alloc;
	c::Error *e_rr = &t->err;

	Log::debugLn(*alloc, "-- GraphicsDevice: testing %s", info->name);

	//8. Device create / wait

	//Device::create takes an Instance, which is deliberately neither copyable nor borrowable because it owns
	// the RefPtrType the C side keeps pointing at, and this driver was handed a raw instance ref it does not
	// own. Module 8 is about GraphicsDeviceRef_create itself, so the C entry point is what gets called and
	// the handle adopts the result: share() takes a reference, the raw dec drops the one create() returned,
	// and the device is owned by `dev` from there on.

	c::GraphicsDeviceRef *created = NULL;

	if(!Test_assert(t, "deviceCreate", c::GraphicsDeviceRef_create(
		instRef, info, c::EGraphicsDeviceFlags_None, c::EGraphicsBufferingMode_Default, NULL, &created, &t->err
	)))
		return;

	gfx::Device dev = gfx::Device::share(created);
	c::RefPtr_dec(&created);

	c::GraphicsDeviceRef *deviceRef = (c::GraphicsDeviceRef*) dev.handle();

	Test_assert(t, "deviceTypeId", deviceRef->refPtrType->typeId == (c::TypeId) c::EGraphicsTypeId_GraphicsDevice);
	Test_assert(t, "deviceAlloc", dev.alloc() == alloc);
	Test_assert(t, "deviceTypes", c::GraphicsDeviceRef_getTypes(deviceRef) == &inst->types);
	Test_assert(t, "deviceWait", dev.wait(e_rr));

	//9. DeviceBuffer

	gfx::DeviceBuffer buffer, cpuBuffer;

	Test_assert(t, "bufferCreate", dev.createBuffer(
		c::EDeviceBufferUsage_Vertex, c::EGraphicsResourceFlag_None,
		"Test vertex buffer", 256, buffer, nullptr, e_rr
	));

	Test_assert(t, "bufferTypeId", buffer.valid() &&
		buffer.handle()->refPtrType->typeId == (c::TypeId) c::EGraphicsTypeId_DeviceBuffer);

	Test_assert(t, "bufferSize", buffer.valid() && buffer.data()->resource.size == 256);

	//markDirty requires a CPU-backed buffer; the vertex buffer above isn't.
	//A null resource is not something a handle can hold, so that one stays on the C entry point.

	Test_assert(t, "markDirtyNotBacked", !buffer.markDirty(0, 0, nullptr));
	Test_assert(t, "markDirtyNull", !c::DeviceBufferRef_markDirty(NULL, 0, 0, NULL));

	Test_assert(t, "cpuBufferCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_CPUBacked,
		"Test cpu buffer", 128, cpuBuffer, nullptr, e_rr
	));

	if(cpuBuffer) {
		Test_assert(t, "markDirty", cpuBuffer.markDirty(0, 64, e_rr));
		Test_assert(t, "markDirtyOOB", !cpuBuffer.markDirty(256, 1, nullptr));
	}

	//10. Swapchain needs a window; NULL must be rejected.
	//A VIRTUAL one is accepted and covered by its own module, since a swapchain over it owns its images and presents
	// to memory.

	gfx::Swapchain swapchain;

	Test_assert(t, "swapchainNullWindow", !dev.createSwapchain(nullptr, false, swapchain, {}, nullptr));

	//11-14. Everything that needs a live device but no submission.
	//Each of these owns and frees the objects it creates, so they can run in any order.

	c::Test_graphicsDescriptorTable(t, deviceRef);
	c::Test_graphicsBindlessDescriptor(t, deviceRef);
	c::Test_graphicsBufferBindless(t, deviceRef);
	c::Test_graphicsCommandList(t, deviceRef);
	c::Test_graphicsVirtualSwapchain(t, deviceRef);
	c::Test_graphicsPhysicalSwapchain(t, deviceRef);
	c::Test_graphicsCommandRecording(t, deviceRef);
	c::Test_graphicsCommandValidation(t, deviceRef);
	c::Test_graphicsRenderPass(t, deviceRef);
	c::Test_graphicsTextureRef(t, deviceRef);
	c::Test_graphicsSamplerAndData(t, deviceRef);
	c::Test_graphicsPipelineLayout(t, deviceRef);
	c::Test_graphicsShaderReflection(t, deviceRef);
	c::Test_graphicsSubmit(t, deviceRef);

	//40. Runs after submit because half of it needs a resource the device is still holding.

	c::Test_graphicsDescriptorAlloc(t, deviceRef);

	//41. The bindful path: a pipeline with its own layout and a table bound at record time.

	c::Test_graphicsBindful(t, deviceRef);
	c::Test_graphicsBindfulAdvanced(t, deviceRef);
	c::Test_graphicsBindfulSampler(t, deviceRef);
	c::Test_graphicsBindfulDraw(t, deviceRef);
	c::Test_graphicsBindfulLayoutSwitch(t, deviceRef);
	c::Test_graphicsBindfulCbuffer(t, deviceRef);
	c::Test_graphicsBindfulRwTexture(t, deviceRef);
	c::Test_graphicsBindfulArray(t, deviceRef);
	c::Test_graphicsBindfulSpaces(t, deviceRef);
	c::Test_graphicsBindfulIndirect(t, deviceRef);
	c::Test_graphicsBindfulDrawFixed(t, deviceRef);
	c::Test_graphicsBindfulTableUpdate(t, deviceRef);
	c::Test_graphicsBindfulSharedRegister(t, deviceRef);
	c::Test_graphicsBindfulHeapRecycle(t, deviceRef);
	c::Test_graphicsBindfulPushDescriptorBoundary(t, deviceRef);
	c::Test_graphicsBindfulSamplerCmp(t, deviceRef);
	c::Test_graphicsBindfulStructured(t, deviceRef);
	c::Test_graphicsBindfulAppendCounter(t, deviceRef);
	c::Test_graphicsBindlessInterleave(t, deviceRef);
	c::Test_graphicsBindlessEverywhere(t, deviceRef);
	c::Test_graphicsFrameGlobals(t, deviceRef);
	c::Test_graphicsBindfulRays(t, deviceRef);
	c::Test_graphicsBlasCompaction(t, deviceRef);
	c::Test_graphicsBindfulOmm(t, deviceRef);
	c::Test_graphicsBindfulRayQueryGraphics(t, deviceRef);
	c::Test_graphicsBindfulAtomicFloat(t, deviceRef);
	c::Test_graphicsBindfulPushConstants(t, deviceRef);
	c::Test_graphicsBindfulPushDescriptors(t, deviceRef);
	c::Test_graphicsBindfulReservedSpace(t, deviceRef);
	c::Test_graphicsGpuExecute(t, deviceRef);
	c::Test_graphicsAccelerationStructures(t, deviceRef);

	//31-33. Shader execution: real dispatches, draws and traces with verified results

	c::Test_graphicsShaderCompute(t, deviceRef);
	c::Test_graphicsTimestamps(t, deviceRef);
	c::Test_graphicsPredication(t, deviceRef);
	c::Test_graphicsShaderDraw(t, deviceRef);
	c::Test_graphicsShaderRays(t, deviceRef);

	//34-36. Resource round trips and the frame ring, rather than what a shader computes

	c::Test_graphicsFormatRoundTrip(t, deviceRef);
	c::Test_graphicsTextureShapes(t, deviceRef);
	c::Test_graphicsFramesInFlight(t, deviceRef);

	//37-38. The capability bits themselves: their invariants, and that gated APIs and shaders agree with them

	c::Test_graphicsCapabilities(t, deviceRef);
	c::Test_graphicsCapabilityExecution(t, deviceRef);

	c::Test_graphicsDeviceMemory(t, deviceRef);

	//39. Config variants run after this adapter's device is gone so the extra devices don't share its
	// memory, so the handles are released here rather than left to the end of the scope.

	buffer.release();
	cpuBuffer.release();

	(void) dev.wait(nullptr);
	dev.release();

	c::Test_graphicsConfigVariants(t, instRef, info);
}

static void Test_graphicsDeviceForApi(c::Test *t, c::EGraphicsApi api) {

	c::Test_setModule(t, "GraphicsDevice");
	c::Test_print(t, c::EGraphicsApi_name[api]);        //Marks which graphics API this device-test run targets

	const c::Allocator *alloc = c::Platform_instance->alloc;

	c::GraphicsApplicationInfo appInfo = {
		.name = c::CharString_createRefCStrConst("OxC3 graphics interface test"),
		.version = 1
	};

	c::RefPtrType type = c::GraphicsInstance_makeType(api, alloc);
	c::GraphicsInstanceRef *instRef = NULL;
	c::ListGraphicsDeviceInfo infos {};

	if (!c::GraphicsInstance_create(&appInfo, api, c::EGraphicsInstanceFlags_None, alloc, &type, &instRef, &t->err)) {
		c::Test_print(t, "No compatible graphics driver, skipping device tests");
		t->err = c::Error_none();
		return;
	}

	c::GraphicsInstance *inst = c::instanceOf(instRef);

	//getDeviceInfos validation

	Test_assert(t, "getDeviceInfosNullResult", !c::GraphicsInstance_getDeviceInfos(inst, NULL, NULL));
	Test_assert(t, "getDeviceInfosNullInst", !c::GraphicsInstance_getDeviceInfos(NULL, &infos, NULL));

	//getPreferredDevice validation

	c::GraphicsDeviceInfo preferred {};

	Test_assert(t, "getPreferredNullInfo", !c::GraphicsInstance_getPreferredDevice(
		inst, NULL, c::GraphicsInstance_vendorMaskAll, c::GraphicsInstance_deviceTypeAll, NULL, NULL
	));

	//A GPU (or software rasterizer like lavapipe) is never guaranteed here, so a machine without one has to stay green.
	//getDeviceInfos tells the two empty results apart through the error it returns.
	//EGenericError_NotFound means the api enumerated no adapter at all, which is nothing this test can hold against it.
	//Any other error means adapters were enumerated but every one of them failed OxC3's requirements.
	//That is a real result to fail on, and the device selection log right above names each requirement that went unmet.

	if (!c::GraphicsInstance_getDeviceInfos(inst, &infos, &t->err) || !infos.length) {

		if (t->err.genericError == c::EGenericError_NotFound) {
			c::Test_print(t, "No graphics adapter present, skipping device tests");
			t->err = c::Error_none();
			goto clean;
		}

		Test_assert(t, "deviceEnumeration", false);
		goto clean;
	}

	//Device creation loads the prebuilt shaders from the //OxC3_graphics section,
	// which are only packaged when the build has the shader compiler enabled.
	//Scoped so the two gotos ABOVE don't jump across these declarations into their lifetime, which C++
	// refuses outright: nothing below the block needs them, and the goto inside it only leaves scopes.

	{
		const c::CharString graphicsSection = c::CharString_createRefCStrConst("//OxC3_graphics");
		const c::CharString prebuiltShader = c::CharString_createRefCStrConst("//OxC3_graphics/shaders/image_copy.oiSH");
		const c::RefPtrType memStreamType = c::MemoryStream_makeType(alloc);

		if (
			!c::File_loadVirtual(&graphicsSection, &memStreamType, NULL, NULL, alloc, NULL) ||
			!c::File_hasFile(&prebuiltShader, alloc)
		) {
			c::Test_print(t, "Prebuilt shaders unavailable (built without shader compiler), skipping device tests");
			goto clean;
		}
	}

	Test_assert(t, "getPreferredDevice", c::GraphicsInstance_getPreferredDevice(
		inst, NULL, c::GraphicsInstance_vendorMaskAll, c::GraphicsInstance_deviceTypeAll, &preferred, &t->err
	));

	//LUID is host side identity rather than anything a shader can observe, so this is the only place it can be
	// checked at all.
	//Identity is what it promises, so identity is what gets asserted: two distinct adapters must not share a
	// LUID, and must not share a UUID either.
	//Deliberately NOT asserted: that a LUID is non zero (nothing promises that), that it is stable across
	// runs, or that it matches the same GPU seen through the other API, since one instance only sees one API.
	//The documented uuid[0] == luid fallback is also left alone: there is no bit saying whether UUIDs are
	// supported, so the condition can't be told apart from its own consequence.

	for (c::U64 i = 0; i < infos.length; ++i) {

		const c::GraphicsDeviceInfo *a = &infos.ptr[i];

		for (c::U64 j = i + 1; j < infos.length; ++j) {

			const c::GraphicsDeviceInfo *b = &infos.ptr[j];

			if((a->capabilities.features & b->capabilities.features) & c::EGraphicsFeatures_LUID)
				Test_assert(t, "luidUnique", a->luid != b->luid);

			Test_assert(t, "uuidUnique", a->uuid[0] != b->uuid[0] || a->uuid[1] != b->uuid[1]);
		}
	}

	//8-14. Every enumerated adapter gets the full battery, so integrated and software devices are exercised
	// on machines that have them rather than only the preferred device

	for(c::U64 i = 0; i < infos.length; ++i)
		Test_graphicsDeviceSingle(t, instRef, &infos.ptr[i]);

clean:

	c::ListGraphicsDeviceInfo_free(&infos, alloc);

	//After every device fully shut down, the whole api run has to be validation clean;
	// any error or warning the debug layers reported is a real defect, which is what hard fails CI

	c::Test_setModule(t, "GraphicsDevice/validation");
	Test_assert(t, "validationErrors", !c::GraphicsInstance_getValidationErrors(inst));
	Test_assert(t, "validationWarnings", !c::GraphicsInstance_getValidationWarnings(inst));

	c::RefPtr_dec(&instRef);
}

//Run the full device test suite for every graphics api the build actually supports, so under dynamic
//linking we exercise e.g. both Direct3D12 and Vulkan (instead of only the platform's default api).

static void Test_graphicsDevice(c::Test *t) {

	c::Test_setModule(t, "GraphicsDevice");

	c::Bool any = false;

	//Counted in the underlying type: C++ has no ++ for an enum, and 0 is not one either.

	for (c::U32 apiRaw = 0; apiRaw < (c::U32) c::EGraphicsApi_Count; ++apiRaw) {

		const c::EGraphicsApi api = (c::EGraphicsApi) apiRaw;

		if(!c::GraphicsInterface_supportsApi(api))
			continue;

		any = true;
		Test_graphicsDeviceForApi(t, api);
	}

	if(!any)
		c::Test_print(t, "No supported graphics api, skipping device tests");
}

// -- 15. Null device rejection ---------------------------------------------------

//Every creator reaches its RefPtrType through GraphicsDeviceRef_getTypes, which returns NULL for a NULL device.
//A member offset is then applied to that NULL, yielding a small non NULL pointer that RefPtr_create accepts
// and faults on, so a creator missing its device check segfaults instead of returning an error.
//Only the first member of GraphicsObjectTypes sits at offset 0, so every other creator is exposed.
//None of these need a live device, which is why this runs even where no adapter is present.

static void Test_graphicsNullDevice(c::Test *t) {

	c::Test_setModule(t, "GraphicsDevice/nullDevice");

	const c::CharString name = c::CharString_createRefCStrConst("Null device rejection");

	c::DeviceBufferRef *buffer = NULL;
	c::DeviceTextureRef *texture = NULL;
	c::RenderTextureRef *renderTexture = NULL;
	c::DepthStencilRef *depthStencil = NULL;
	c::SwapchainRef *swapchain = NULL;
	c::CommandListRef *commandList = NULL;

	c::Buffer empty = c::Buffer_createNull();

	Test_assert(t, "buffer", !c::GraphicsDeviceRef_createBuffer(
		NULL, c::EDeviceBufferUsage_Vertex, c::EGraphicsResourceFlag_None, NULL, &name, 256, &buffer, NULL
	));

	Test_assert(t, "texture", !c::GraphicsDeviceRef_createTexture(
		NULL, c::ETextureType_2D, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None,
		1, 1, 0, NULL, &name, &empty, &texture, NULL
	));

	Test_assert(t, "renderTexture", !c::GraphicsDeviceRef_createRenderTexture(
		NULL, c::ETextureType_2D, 1, 1, 1, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None,
		c::EMSAASamples_Off, NULL, &name, &renderTexture, NULL
	));

	Test_assert(t, "depthStencil", !c::GraphicsDeviceRef_createDepthStencil(
		NULL, 1, 1, c::EDepthStencilFormat_D32, false, c::EMSAASamples_Off, NULL, &name, &depthStencil, NULL
	));

	Test_assert(t, "swapchain", !c::GraphicsDeviceRef_createSwapchain(
		NULL, { 0 }, false, NULL, &swapchain, NULL
	));

	Test_assert(t, "commandList", !c::GraphicsDeviceRef_createCommandList(
		NULL, 2 * c::KIBI, 128, 64, true, &commandList, NULL
	));

	//A rejection must not leave a half built object behind, since the caller has no way to free one.

	Test_assert(t, "nothingAllocated",
		!buffer && !texture && !renderTexture && !depthStencil && !swapchain && !commandList
	);
}

// -- entry point ---------------------------------------------------------------

//Outside the namespace: this expands to the platform's entry point, and main has to be ::main.

using namespace oxc;
using namespace oxc::c;

OXC3_TEST_ENTRY(graphics_interface) {

	Error err = Error_none();
	if (!Platform_create(Platform_argc, Platform_argv, Platform_getData(), NULL, true, &err)) {
		Test_printPlatformCreateFail(&err);
		Platform_return(1);
	}

	Test t = { .alloc = Platform_instance->alloc };

	U64 allocsBefore = Platform_getActiveAllocations(0);

	Test_graphicsInterface(&t);
	Test_graphicsInstanceType(&t);
	Test_graphicsInstance(&t);
	Test_graphicsFormats(&t);
	Test_bindlessDescriptorPacking(&t);
	Test_descriptorPacking(&t);
	Test_textureRange(&t);
	Test_graphicsDefaultBindlessLayout(&t);
	Test_graphicsNullDevice(&t);
	Test_graphicsDevice(&t);

	//We might have instantiated a list with some capacity, make sure we get rid of it so the counter doesn't false positive.

	for(U64 i = 0; i < Platform_instance->archives.length; ++i)
		CAFile_free(&Platform_instance->archives.ptrNonConst[i], Platform_instance->alloc);

	ListCAFile_free(&Platform_instance->archives, Platform_instance->alloc);

	U64 allocsAfter = Platform_getActiveAllocations(0);

	Test_setModule(&t, NULL);
	Test_assert(&t, "NoLeaks", allocsAfter <= allocsBefore);

	int result = Test_end(&t);
	Platform_cleanup();
	Platform_return(result);
}
