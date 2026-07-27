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
//
//What is covered when an adapter is present (skipped with a message otherwise;
//CI Linux may provide lavapipe, but a GPU is never guaranteed):
//  6. Device enumeration - getDeviceInfos / getPreferredDevice (+ validation)
//  7. Device             - create / wait / free
//  8. DeviceBuffer       - create, CPU-backed create, markDirty validation
//  9. CommandList        - create + free
// 10. Swapchain          - rejects NULL / non-physical window
//
//Run in CI, no display, no human interaction required.

#include "graphics/generic/interface.h"
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
#include "types/test/test.h"
#include "types/container/memory_stream.h"
#include "types/container/texture_format.h"
#include "types/base/string_base.h"
#include "types/base/error.h"

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

	Test_assert(t, "typeId", type.typeId == (ETypeId) EGraphicsTypeId_GraphicsInstance);
	Test_assert(t, "length", type.length >= sizeof(GraphicsInstance));
	Test_assert(t, "alloc", type.alloc == alloc);
	Test_assert(t, "freeFunc", type.free);
}

// -- 3/4. Instance create / free + object type table ---------------------------

static void Test_checkObjectType(
	Test *t, const C8 *name, const RefPtrType *type, ETypeId typeId, U64 minLen, const Allocator *alloc
) {
	Test_assert(t, name, type->typeId == typeId && type->length >= minLen && type->alloc == alloc && type->free);
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
	//that's not a test failure, the device tests are simply skipped (like the adapter check below).

	if (!GraphicsInstance_create(&appInfo, EGraphicsApi_Count, 0, alloc, &type, &instRef, &t->err)) {
		Test_print(t, "No compatible graphics driver, skipping instance tests");
		t->err = Error_none();
		return;
	}

	GraphicsInstance *inst = GraphicsInstanceRef_ptr(instRef);

	Test_assert(t, "instNotNull", inst != NULL);
	Test_assert(t, "apiResolved", inst && inst->api == EGraphicsApi_resolve(EGraphicsApi_Count));
	Test_assert(t, "allocStored", inst && inst->alloc == alloc);
	Test_assert(t, "refTypeId", instRef->refPtrType->typeId == (ETypeId) EGraphicsTypeId_GraphicsInstance);

	//Object type table invariants: correct typeIds, length can hold the base struct, same alloc, free func set.
	//These types live in the instance so they're guaranteed to outlive the objects created with them.

	if (inst) {

		const GraphicsObjectTypes *types = &inst->types;

		Test_checkObjectType(t, "device", &types->device, (ETypeId)EGraphicsTypeId_GraphicsDevice, sizeof(GraphicsDevice), alloc);
		Test_checkObjectType(t, "buffer", &types->buffer, (ETypeId)EGraphicsTypeId_DeviceBuffer, sizeof(DeviceBuffer), alloc);

		Test_checkObjectType(
			t, "deviceTexture", &types->deviceTexture,
			(ETypeId)EGraphicsTypeId_DeviceTexture, sizeof(DeviceTexture), alloc
		);

		Test_checkObjectType(
			t, "renderTexture", &types->renderTexture,
			(ETypeId)EGraphicsTypeId_RenderTexture, sizeof(RenderTexture), alloc
		);

		Test_checkObjectType(
			t, "depthStencil", &types->depthStencil,
			(ETypeId)EGraphicsTypeId_DepthStencil, sizeof(DepthStencil), alloc
		);

		Test_checkObjectType(t, "swapchain", &types->swapchain, (ETypeId)EGraphicsTypeId_Swapchain, sizeof(Swapchain), alloc);
		Test_checkObjectType(t, "pipeline", &types->pipeline, (ETypeId)EGraphicsTypeId_Pipeline, sizeof(Pipeline), alloc);
		Test_checkObjectType(t, "sampler", &types->sampler, (ETypeId)EGraphicsTypeId_Sampler, sizeof(Sampler), alloc);
		Test_checkObjectType(t, "blas", &types->blas, (ETypeId)EGraphicsTypeId_BLASExt, sizeof(BLAS), alloc);
		Test_checkObjectType(t, "tlas", &types->tlas, (ETypeId)EGraphicsTypeId_TLASExt, sizeof(TLAS), alloc);

		Test_checkObjectType(
			t, "descriptorLayout", &types->descriptorLayout,
			(ETypeId)EGraphicsTypeId_DescriptorLayout, sizeof(DescriptorLayout), alloc
		);

		Test_checkObjectType(
			t, "descriptorTable", &types->descriptorTable,
			(ETypeId)EGraphicsTypeId_DescriptorTable, sizeof(DescriptorTable), alloc
		);

		Test_checkObjectType(
			t, "descriptorHeap", &types->descriptorHeap,
			(ETypeId)EGraphicsTypeId_DescriptorHeap, sizeof(DescriptorHeap), alloc
		);

		Test_checkObjectType(
			t, "pipelineLayout", &types->pipelineLayout,
			(ETypeId)EGraphicsTypeId_PipelineLayout, sizeof(PipelineLayout), alloc
		);

		Test_checkObjectType(
			t, "commandList", &types->commandList,
			(ETypeId)EGraphicsTypeId_CommandList, sizeof(CommandList), alloc
		);
	}

	//Free through the generic ref counter; double-dec must be safe (pointer is NULLed)

	RefPtr_dec(&instRef);
	Test_assert(t, "freeNulled", instRef == NULL);

	RefPtr_dec(&instRef);
	Test_assert(t, "doubleDecSafe", instRef == NULL);
}

// -- 5. Texture format helpers --------------------------------------------------

static void Test_graphicsFormats(Test *t) {

	Test_setModule(t, "TextureFormat");

	Test_assert(t, "rgba8Size", ETextureFormat_getSize(ETextureFormat_RGBA8, 1, 1, 1) == 4);
	Test_assert(t, "rgba16fSize", ETextureFormat_getSize(ETextureFormat_RGBA16f, 1, 1, 1) == 8);
	Test_assert(t, "rgba32fSize", ETextureFormat_getSize(ETextureFormat_RGBA32f, 1, 1, 1) == 16);

	Test_assert(t, "rgba8Uncompressed", !ETextureFormat_getIsCompressed(ETextureFormat_RGBA8));
	Test_assert(t, "bc7Compressed", ETextureFormat_getIsCompressed(ETextureFormat_BC7));

	//4x4 BC7 block = 16 bytes (misaligned blocks handled the same)

	Test_assert(t, "bc7BlockSize4", ETextureFormat_getSize(ETextureFormat_BC7, 4, 4, 1) == 16);
	Test_assert(t, "bc7BlockSize2", ETextureFormat_getSize(ETextureFormat_BC7, 2, 2, 1) == 16);
	Test_assert(t, "bc7BlockSize1", ETextureFormat_getSize(ETextureFormat_BC7, 1, 1, 1) == 16);

	//Vertex attributes exclude compressed formats
	Test_assert(t, "rgba8VertexAttrib", GraphicsDeviceInfo_supportsFormatVertexAttribute(ETextureFormat_RGBA8));
	Test_assert(t, "bc7NoVertexAttrib", !GraphicsDeviceInfo_supportsFormatVertexAttribute(ETextureFormat_BC7));

	//ETextureFormatId <-> ETextureFormat mapping is consistent
	Test_assert(t, "unpackRGBA8", ETextureFormatId_unpack[ETextureFormatId_RGBA8] == ETextureFormat_RGBA8);
	Test_assert(t, "unpackUndefined", ETextureFormatId_unpack[ETextureFormatId_Undefined] == ETextureFormat_Undefined);
}

// -- 6-10. Device dependent (skipped without an adapter) -------------------------

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
	GraphicsDeviceRef *deviceRef = NULL;
	DeviceBufferRef *buffer = NULL;
	DeviceBufferRef *cpuBuffer = NULL;
	CommandListRef *commandList = NULL;
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

	//A GPU (or software rasterizer like lavapipe) is never guaranteed here; skip if absent

	if (!GraphicsInstance_getDeviceInfos(inst, &infos, NULL) || !infos.length) {
		Test_print(t, "No graphics adapter present, skipping device tests");
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

	//7. Device create / wait

	if(!Test_assert(t, "deviceCreate", GraphicsDeviceRef_create(
		instRef, &preferred, EGraphicsDeviceFlags_None, EGraphicsBufferingMode_Default, &deviceRef, &t->err
	)))
		goto clean;

	Test_assert(t, "deviceTypeId", deviceRef->refPtrType->typeId == (ETypeId) EGraphicsTypeId_GraphicsDevice);
	Test_assert(t, "deviceAlloc", GraphicsDeviceRef_getAlloc(deviceRef) == alloc);
	Test_assert(t, "deviceTypes", GraphicsDeviceRef_getTypes(deviceRef) == &inst->types);
	Test_assert(t, "deviceWait", GraphicsDeviceRef_wait(deviceRef, &t->err));

	//8. DeviceBuffer

	CharString testVertexBuffer = CharString_createRefCStrConst("Test vertex buffer");

	Test_assert(t, "bufferCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_Vertex, EGraphicsResourceFlag_None, NULL,
		&testVertexBuffer, 256, &buffer, &t->err
	));

	Test_assert(t, "bufferTypeId", buffer && buffer->refPtrType->typeId == (ETypeId) EGraphicsTypeId_DeviceBuffer);
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

	//9. CommandList create (recording is validated by functional tests; here just lifecycle)

	Test_assert(t, "commandListCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, 2 * KIBI, 128, 64, true, &commandList, &t->err
	));

	Test_assert(
		t, "commandListTypeId",
		commandList && commandList->refPtrType->typeId == (ETypeId) EGraphicsTypeId_CommandList
	);

	//10. Swapchain requires a physical window; NULL must be rejected

	SwapchainRef *swapchain = NULL;
	SwapchainInfo swapchainInfo = (SwapchainInfo) { .window = NULL };

	Test_assert(t, "swapchainNullWindow", !GraphicsDeviceRef_createSwapchain(
		deviceRef, swapchainInfo, false, NULL, &swapchain, NULL
	));

clean:

	ListGraphicsDeviceInfo_free(&infos, alloc);

	RefPtr_dec(&commandList);
	RefPtr_dec(&cpuBuffer);
	RefPtr_dec(&buffer);

	if(deviceRef)
		GraphicsDeviceRef_wait(deviceRef, NULL);

	RefPtr_dec(&deviceRef);
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

// -- entry point ---------------------------------------------------------------

Platform_defineEntrypoint() {

	Error err = Error_none();
	if (!Platform_create(Platform_argc, Platform_argv, Platform_getData(), NULL, true, &err))
		Platform_return(1);

	Test t = (Test) { .alloc = Platform_instance->alloc };

	U64 allocsBefore = Platform_getActiveAllocations(0);

	Test_graphicsInterface(&t);
	Test_graphicsInstanceType(&t);
	Test_graphicsInstance(&t);
	Test_graphicsFormats(&t);
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
