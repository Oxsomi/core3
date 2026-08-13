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

//graphics/test/interface/test_graphics_interface.c

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
// 17. TLASTransformSRT  - interleaved VkSRTDataNV get/set round trip and pinned field layout
// 18. Descriptor pack   - buffer region + counter offset bit packing, texture/tlas/sampler union
// 19. TextureRange      - extent helpers
// 24. Default layout    - the bindless layout OxC3 shaders compile against (DXIL chain law, SPIRV sets, rt gating)
//
//What is covered when an adapter is present (skipped with a message otherwise;
//CI Linux may provide lavapipe, but a GPU is never guaranteed):
//  7. Device enumeration - getDeviceInfos / getPreferredDevice (+ validation)
//  8. Device             - create / wait / free
//  9. DeviceBuffer       - create, CPU-backed create, markDirty validation
// 10. Swapchain          - rejects NULL / non-physical window
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
//
//Numbering follows the order the modules were added, not the order they run in.
//
//Run in CI, no display, no human interaction required.

#include "graphics/generic/instance.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_info.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/render_texture.h"
#include "graphics/generic/depth_stencil.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/sampler.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/graphics_types.h"
#include "platforms/platform.h"
#include "platforms/file.h"
#include "platforms/logx.h"
#include "types/test/test.h"
#include "types/container/memory_stream.h"
#include "types/container/texture_format.h"
#include "types/base/string_base.h"
#include "types/base/error.h"
#include "test_graphics_shared.h"

// -- 1. GraphicsInterface ------------------------------------------------------

static void Test_graphicsInterface(Test *t) {

	Test_setModule(t, "GraphicsInterface");

	Test_assert(t, "create", GraphicsInterface_create(&t->err));
	Test_assert(t, "createTwice", GraphicsInterface_create(&t->err));        //Guards against multiple inits

	//Resolving the default api must give a real api that's supported

	EGraphicsApi def = EGraphicsApi_resolve(EGraphicsApi_Count);
	Test_assert(t, "resolveDefault", def < EGraphicsApi_Count);
	Test_assert(t, "defaultSupported", GraphicsInterface_supportsApi(def));
	Test_assert(t, "resolveIdentity", EGraphicsApi_resolve(def) == def);
}

// -- 2. GraphicsInstance_makeType ----------------------------------------------

static void Test_graphicsInstanceType(Test *t) {

	Test_setModule(t, "GraphicsInstance/Type");

	const Allocator *alloc = Platform_instance->alloc;
	RefPtrType type = GraphicsInstance_makeType(EGraphicsApi_Count, alloc);

	Test_assert(t, "typeId", type.typeId == (TypeId) EGraphicsTypeId_GraphicsInstance);
	Test_assert(t, "length", RefPtrType_length(&type) >= sizeof(GraphicsInstance));
	Test_assert(t, "alloc", type.alloc == alloc);
	Test_assert(t, "freeFunc", type.free);
}

// -- 3/4. Instance create / free + object type table ---------------------------

static void Test_checkObjectType(
	Test *t, const C8 *name, const RefPtrType *type, TypeId typeId, U64 minLen, const Allocator *alloc
) {
	Test_assert(t, name, type->typeId == typeId && RefPtrType_length(type) >= minLen && type->alloc == alloc && type->free);
}

static void Test_graphicsInstance(Test *t) {

	Test_setModule(t, "GraphicsInstance");

	const Allocator *alloc = Platform_instance->alloc;

	GraphicsApplicationInfo appInfo = (GraphicsApplicationInfo) {
		.name = CharString_createRefCStrConst("OxC3 graphics interface test"),
		.version = 1
	};

	//TODO: Foreach all supported apis

	//Parameter validation: missing instance output

	GraphicsInstanceRef *instRef = NULL;

	Error err = Error_none();
	RefPtrType type = GraphicsInstance_makeType(EGraphicsApi_Count, alloc);

	Test_assert(t, "createNullApp", !GraphicsInstance_create(NULL, EGraphicsApi_Count, 0, alloc, &type, &instRef, &err));
	Test_assert(t, "createNullInst", !GraphicsInstance_create(&appInfo, EGraphicsApi_Count, 0, alloc, &type, NULL, &err));
	Test_assert(t, "createNullType", !GraphicsInstance_create(&appInfo, EGraphicsApi_Count, 0, alloc, NULL, NULL, &err));

	//Real create.
	//Instance creation legitimately fails on machines without a compatible driver/ICD;
	// that's not a test failure, the device tests are simply skipped (like the adapter check below).

	if (!GraphicsInstance_create(&appInfo, EGraphicsApi_Count, 0, alloc, &type, &instRef, &t->err)) {
		Test_print(t, "No compatible graphics driver, skipping instance tests");
		t->err = Error_none();
		return;
	}

	GraphicsInstance *inst = GraphicsInstanceRef_ptr(instRef);

	Test_assert(t, "instNotNull", inst != NULL);
	Test_assert(t, "apiResolved", inst && inst->api == EGraphicsApi_resolve(EGraphicsApi_Count));
	Test_assert(t, "allocStored", inst && inst->alloc == alloc);
	Test_assert(t, "refTypeId", instRef->refPtrType->typeId == (TypeId) EGraphicsTypeId_GraphicsInstance);

	//Object type table invariants: correct typeIds, length can hold the base struct, same alloc, free func set.
	//These types live in the instance so they're guaranteed to outlive the objects created with them.

	if (inst) {

		const GraphicsObjectTypes *types = &inst->types;

		Test_checkObjectType(t, "device", &types->device, (TypeId)EGraphicsTypeId_GraphicsDevice, sizeof(GraphicsDevice), alloc);
		Test_checkObjectType(t, "buffer", &types->buffer, (TypeId)EGraphicsTypeId_DeviceBuffer, sizeof(DeviceBuffer), alloc);

		Test_checkObjectType(
			t, "deviceTexture", &types->deviceTexture,
			(TypeId)EGraphicsTypeId_DeviceTexture, sizeof(DeviceTexture), alloc
		);

		Test_checkObjectType(
			t, "renderTexture", &types->renderTexture,
			(TypeId)EGraphicsTypeId_RenderTexture, sizeof(RenderTexture), alloc
		);

		Test_checkObjectType(
			t, "depthStencil", &types->depthStencil,
			(TypeId)EGraphicsTypeId_DepthStencil, sizeof(DepthStencil), alloc
		);

		Test_checkObjectType(t, "swapchain", &types->swapchain, (TypeId)EGraphicsTypeId_Swapchain, sizeof(Swapchain), alloc);

		//The pipeline kinds share a typeId but each length has to fit its own info block (Pipeline_infoOffset),
		// which is exactly the invariant whose violation used to overrun the heap on graphics pipeline creation

		Test_checkObjectType(
			t, "pipelineCompute", &types->pipelineCompute, (TypeId)EGraphicsTypeId_Pipeline, sizeof(Pipeline), alloc
		);

		Test_checkObjectType(
			t, "pipelineGraphics", &types->pipelineGraphics,
			(TypeId)EGraphicsTypeId_Pipeline, sizeof(Pipeline) + sizeof(PipelineGraphicsInfo), alloc
		);

		Test_checkObjectType(
			t, "pipelineRaytracing", &types->pipelineRaytracing,
			(TypeId)EGraphicsTypeId_Pipeline, sizeof(Pipeline) + sizeof(PipelineRaytracingInfo), alloc
		);

		Test_checkObjectType(t, "sampler", &types->sampler, (TypeId)EGraphicsTypeId_Sampler, sizeof(Sampler), alloc);
		Test_checkObjectType(t, "blas", &types->blas, (TypeId)EGraphicsTypeId_BLASExt, sizeof(BLAS), alloc);
		Test_checkObjectType(t, "tlas", &types->tlas, (TypeId)EGraphicsTypeId_TLASExt, sizeof(TLAS), alloc);

		Test_checkObjectType(
			t, "descriptorLayout", &types->descriptorLayout,
			(TypeId)EGraphicsTypeId_DescriptorLayout, sizeof(DescriptorLayout), alloc
		);

		Test_checkObjectType(
			t, "descriptorTable", &types->descriptorTable,
			(TypeId)EGraphicsTypeId_DescriptorTable, sizeof(DescriptorTable), alloc
		);

		Test_checkObjectType(
			t, "descriptorHeap", &types->descriptorHeap,
			(TypeId)EGraphicsTypeId_DescriptorHeap, sizeof(DescriptorHeap), alloc
		);

		Test_checkObjectType(
			t, "pipelineLayout", &types->pipelineLayout,
			(TypeId)EGraphicsTypeId_PipelineLayout, sizeof(PipelineLayout), alloc
		);

		Test_checkObjectType(
			t, "commandList", &types->commandList,
			(TypeId)EGraphicsTypeId_CommandList, sizeof(CommandList), alloc
		);
	}

	//Free through the generic ref counter; double-dec must be safe (pointer is NULLed)

	RefPtr_dec(&instRef);
	Test_assert(t, "freeNulled", instRef == NULL);

	RefPtr_dec(&instRef);
	Test_assert(t, "doubleDecSafe", instRef == NULL);
}

// -- 7-10. Device, DeviceBuffer and Swapchain ------------------------------------

//One adapter's full run: create the device, run every device scoped module, tear down again

static void Test_graphicsDeviceSingle(Test *t, GraphicsInstanceRef *instRef, const GraphicsDeviceInfo *info) {

	Test_setModule(t, "GraphicsDevice");

	GraphicsInstance *inst = GraphicsInstanceRef_ptr(instRef);
	const Allocator *alloc = inst->alloc;

	GraphicsDeviceRef *deviceRef = NULL;
	DeviceBufferRef *buffer = NULL;
	DeviceBufferRef *cpuBuffer = NULL;

	Log_debugLnx("-- GraphicsDevice: testing %s", info->name);

	//8. Device create / wait

	if(!Test_assert(t, "deviceCreate", GraphicsDeviceRef_create(
		instRef, info, EGraphicsDeviceFlags_None, EGraphicsBufferingMode_Default, NULL, &deviceRef, &t->err
	)))
		return;

	Test_assert(t, "deviceTypeId", deviceRef->refPtrType->typeId == (TypeId) EGraphicsTypeId_GraphicsDevice);
	Test_assert(t, "deviceAlloc", GraphicsDeviceRef_getAlloc(deviceRef) == alloc);
	Test_assert(t, "deviceTypes", GraphicsDeviceRef_getTypes(deviceRef) == &inst->types);
	Test_assert(t, "deviceWait", GraphicsDeviceRef_wait(deviceRef, &t->err));

	//9. DeviceBuffer

	CharString testVertexBuffer = CharString_createRefCStrConst("Test vertex buffer");

	Test_assert(t, "bufferCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_Vertex, EGraphicsResourceFlag_None, NULL,
		&testVertexBuffer, 256, &buffer, &t->err
	));

	Test_assert(t, "bufferTypeId", buffer && buffer->refPtrType->typeId == (TypeId) EGraphicsTypeId_DeviceBuffer);
	Test_assert(t, "bufferSize", buffer && DeviceBufferRef_ptr(buffer)->resource.size == 256);

	//markDirty requires a CPU-backed buffer; the vertex buffer above isn't

	Test_assert(t, "markDirtyNotBacked", !DeviceBufferRef_markDirty(buffer, 0, 0, NULL));
	Test_assert(t, "markDirtyNull", !DeviceBufferRef_markDirty(NULL, 0, 0, NULL));

	CharString testCpuBuffer = CharString_createRefCStrConst("Test cpu buffer");

	Test_assert(t, "cpuBufferCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_CPUBacked, NULL,
		&testCpuBuffer, 128, &cpuBuffer, &t->err
	));

	if(cpuBuffer) {
		Test_assert(t, "markDirty", DeviceBufferRef_markDirty(cpuBuffer, 0, 64, &t->err));
		Test_assert(t, "markDirtyOOB", !DeviceBufferRef_markDirty(cpuBuffer, 256, 1, NULL));
	}

	//10. Swapchain requires a physical window; NULL must be rejected

	SwapchainRef *swapchain = NULL;
	SwapchainInfo swapchainInfo = (SwapchainInfo) { .window = NULL };

	Test_assert(t, "swapchainNullWindow", !GraphicsDeviceRef_createSwapchain(
		deviceRef, swapchainInfo, false, NULL, &swapchain, NULL
	));

	//11-14. Everything that needs a live device but no submission.
	//Each of these owns and frees the objects it creates, so they can run in any order.

	Test_graphicsDescriptorTable(t, deviceRef);
	Test_graphicsBindlessDescriptor(t, deviceRef);
	Test_graphicsBufferBindless(t, deviceRef);
	Test_graphicsCommandList(t, deviceRef);
	Test_graphicsCommandRecording(t, deviceRef);
	Test_graphicsCommandValidation(t, deviceRef);
	Test_graphicsRenderPass(t, deviceRef);
	Test_graphicsTextureRef(t, deviceRef);
	Test_graphicsSamplerAndData(t, deviceRef);
	Test_graphicsPipelineLayout(t, deviceRef);
	Test_graphicsShaderReflection(t, deviceRef);
	Test_graphicsSubmit(t, deviceRef);
	Test_graphicsGpuExecute(t, deviceRef);
	Test_graphicsAccelerationStructures(t, deviceRef);

	//31-33. Shader execution: real dispatches, draws and traces with verified results

	Test_graphicsShaderCompute(t, deviceRef);
	Test_graphicsShaderDraw(t, deviceRef);
	Test_graphicsShaderRays(t, deviceRef);

	Test_graphicsDeviceMemory(t, deviceRef);

	RefPtr_dec(&cpuBuffer);
	RefPtr_dec(&buffer);

	GraphicsDeviceRef_wait(deviceRef, NULL);
	RefPtr_dec(&deviceRef);
}

static void Test_graphicsDeviceForApi(Test *t, EGraphicsApi api) {

	Test_setModule(t, "GraphicsDevice");
	Test_print(t, EGraphicsApi_name[api]);        //Marks which graphics API this device-test run targets

	const Allocator *alloc = Platform_instance->alloc;

	GraphicsApplicationInfo appInfo = (GraphicsApplicationInfo) {
		.name = CharString_createRefCStrConst("OxC3 graphics interface test"),
		.version = 1
	};

	RefPtrType type = GraphicsInstance_makeType(api, alloc);
	GraphicsInstanceRef *instRef = NULL;
	ListGraphicsDeviceInfo infos = (ListGraphicsDeviceInfo) { 0 };

	if (!GraphicsInstance_create(&appInfo, api, 0, alloc, &type, &instRef, &t->err)) {
		Test_print(t, "No compatible graphics driver, skipping device tests");
		t->err = Error_none();
		return;
	}

	GraphicsInstance *inst = GraphicsInstanceRef_ptr(instRef);

	//getDeviceInfos validation

	Test_assert(t, "getDeviceInfosNullResult", !GraphicsInstance_getDeviceInfos(inst, NULL, NULL));
	Test_assert(t, "getDeviceInfosNullInst", !GraphicsInstance_getDeviceInfos(NULL, &infos, NULL));

	//getPreferredDevice validation

	GraphicsDeviceInfo preferred = (GraphicsDeviceInfo) { 0 };

	Test_assert(t, "getPreferredNullInfo", !GraphicsInstance_getPreferredDevice(
		inst, NULL, GraphicsInstance_vendorMaskAll, GraphicsInstance_deviceTypeAll, NULL, NULL
	));

	//A GPU (or software rasterizer like lavapipe) is never guaranteed here, so a machine without one has to stay green.
	//getDeviceInfos tells the two empty results apart through the error it returns.
	//EGenericError_NotFound means the api enumerated no adapter at all, which is nothing this test can hold against it.
	//Any other error means adapters were enumerated but every one of them failed OxC3's requirements.
	//That is a real result to fail on, and the device selection log right above names each requirement that went unmet.

	if (!GraphicsInstance_getDeviceInfos(inst, &infos, &t->err) || !infos.length) {

		if (t->err.genericError == EGenericError_NotFound) {
			Test_print(t, "No graphics adapter present, skipping device tests");
			t->err = Error_none();
			goto clean;
		}

		Test_assert(t, "deviceEnumeration", false);
		goto clean;
	}

	//Device creation loads the prebuilt shaders from the //OxC3_graphics section,
	// which are only packaged when the build has the shader compiler enabled

	const CharString graphicsSection = CharString_createRefCStrConst("//OxC3_graphics");
	const CharString prebuiltShader = CharString_createRefCStrConst("//OxC3_graphics/shaders/image_copy.oiSH");
	const RefPtrType memStreamType = MemoryStream_makeType(alloc);

	if (!File_loadVirtual(&graphicsSection, &memStreamType, NULL, NULL, alloc, NULL) || !File_hasFile(&prebuiltShader, alloc)) {
		Test_print(t, "Prebuilt shaders unavailable (built without shader compiler), skipping device tests");
		goto clean;
	}

	Test_assert(t, "getPreferredDevice", GraphicsInstance_getPreferredDevice(
		inst, NULL, GraphicsInstance_vendorMaskAll, GraphicsInstance_deviceTypeAll, &preferred, &t->err
	));

	//8-14. Every enumerated adapter gets the full battery, so integrated and software devices are exercised
	// on machines that have them rather than only the preferred device

	for(U64 i = 0; i < infos.length; ++i)
		Test_graphicsDeviceSingle(t, instRef, &infos.ptr[i]);

clean:

	ListGraphicsDeviceInfo_free(&infos, alloc);

	//After every device fully shut down, the whole api run has to be validation clean;
	// any error or warning the debug layers reported is a real defect, which is what hard fails CI

	Test_setModule(t, "GraphicsDevice/validation");
	Test_assert(t, "validationErrors", !GraphicsInstance_getValidationErrors(inst));
	Test_assert(t, "validationWarnings", !GraphicsInstance_getValidationWarnings(inst));

	RefPtr_dec(&instRef);
}

//Run the full device test suite for every graphics api the build actually supports, so under dynamic
//linking we exercise e.g. both Direct3D12 and Vulkan (instead of only the platform's default api).

static void Test_graphicsDevice(Test *t) {

	Test_setModule(t, "GraphicsDevice");

	Bool any = false;

	for (EGraphicsApi api = 0; api < EGraphicsApi_Count; ++api) {

		if(!GraphicsInterface_supportsApi(api))
			continue;

		any = true;
		Test_graphicsDeviceForApi(t, api);
	}

	if(!any)
		Test_print(t, "No supported graphics api, skipping device tests");
}

// -- 15. Null device rejection ---------------------------------------------------

//Every creator reaches its RefPtrType through GraphicsDeviceRef_getTypes, which returns NULL for a NULL device.
//A member offset is then applied to that NULL, yielding a small non NULL pointer that RefPtr_create accepts
// and faults on, so a creator missing its device check segfaults instead of returning an error.
//Only the first member of GraphicsObjectTypes sits at offset 0, so every other creator is exposed.
//None of these need a live device, which is why this runs even where no adapter is present.

static void Test_graphicsNullDevice(Test *t) {

	Test_setModule(t, "GraphicsDevice/nullDevice");

	const CharString name = CharString_createRefCStrConst("Null device rejection");

	DeviceBufferRef *buffer = NULL;
	DeviceTextureRef *texture = NULL;
	RenderTextureRef *renderTexture = NULL;
	DepthStencilRef *depthStencil = NULL;
	SwapchainRef *swapchain = NULL;
	CommandListRef *commandList = NULL;

	Buffer empty = Buffer_createNull();

	Test_assert(t, "buffer", !GraphicsDeviceRef_createBuffer(
		NULL, EDeviceBufferUsage_Vertex, EGraphicsResourceFlag_None, NULL, &name, 256, &buffer, NULL
	));

	Test_assert(t, "texture", !GraphicsDeviceRef_createTexture(
		NULL, ETextureType_2D, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		1, 1, 0, NULL, &name, &empty, &texture, NULL
	));

	Test_assert(t, "renderTexture", !GraphicsDeviceRef_createRenderTexture(
		NULL, ETextureType_2D, 1, 1, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &name, &renderTexture, NULL
	));

	Test_assert(t, "depthStencil", !GraphicsDeviceRef_createDepthStencil(
		NULL, 1, 1, EDepthStencilFormat_D32, false, EMSAASamples_Off, NULL, &name, &depthStencil, NULL
	));

	Test_assert(t, "swapchain", !GraphicsDeviceRef_createSwapchain(
		NULL, (SwapchainInfo) { 0 }, false, NULL, &swapchain, NULL
	));

	Test_assert(t, "commandList", !GraphicsDeviceRef_createCommandList(
		NULL, 2 * KIBI, 128, 64, true, &commandList, NULL
	));

	//A rejection must not leave a half built object behind, since the caller has no way to free one.

	Test_assert(t, "nothingAllocated",
		!buffer && !texture && !renderTexture && !depthStencil && !swapchain && !commandList
	);
}

// -- entry point ---------------------------------------------------------------

OXC3_TEST_ENTRY(graphics_interface) {

	Error err = Error_none();
	if (!Platform_create(Platform_argc, Platform_argv, Platform_getData(), NULL, true, &err)) {
		Test_printPlatformCreateFail(&err);
		Platform_return(1);
	}

	Test t = (Test) { .alloc = Platform_instance->alloc };

	U64 allocsBefore = Platform_getActiveAllocations(0);

	Test_graphicsInterface(&t);
	Test_graphicsInstanceType(&t);
	Test_graphicsInstance(&t);
	Test_graphicsFormats(&t);
	Test_bindlessDescriptorPacking(&t);
	Test_tlasTransformSRT(&t);
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
