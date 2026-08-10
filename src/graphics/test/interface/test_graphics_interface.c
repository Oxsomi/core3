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
// 14. CommandList        - create + free, parameter validation
// 16. Submit             - begin/end state machine, empty frame submit (the only path that binds descriptors)
//
//Numbering follows the order the modules were added, not the order they run in.
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
#include "graphics/generic/bindless_descriptor.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/commands.h"
#include "graphics/generic/graphics_types.h"
#include "platforms/platform.h"
#include "platforms/file.h"
#include "types/test/test.h"
#include "types/container/memory_stream.h"
#include "types/container/texture_format.h"
#include "types/container/buffer.h"
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
		Test_checkObjectType(t, "pipeline", &types->pipeline, (TypeId)EGraphicsTypeId_Pipeline, sizeof(Pipeline), alloc);
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

// -- 6. Bindless descriptor packing (pure, no device) ----------------------------

static void Test_bindlessDescriptorPacking(Test *t) {

	Test_setModule(t, "BindlessDescriptor/pack");

	//A handle is a 4 bit bindless type (0 is reserved) followed by a 17 bit array index, so 21 bits in total.
	//Three of them fit in a U64, which is what pack3/unpack3 are for.

	const BindlessDescriptor a = ((BindlessDescriptor)1 << 17) | 1;
	const BindlessDescriptor b = ((BindlessDescriptor)2 << 17) | 1234;
	const BindlessDescriptor c = ((BindlessDescriptor)15 << 17) | ((1 << 17) - 1);

	Test_assert(t, "getBindlessType", BindlessDescriptor_getBindlessType(b) == 2);
	Test_assert(t, "getId", BindlessDescriptor_getId(b) == 1234);
	Test_assert(t, "maxHandleFits", c == (BindlessDescriptor)((1 << 21) - 1));

	const U64 packed = BindlessDescriptor_pack3(a, b, c);
	Test_assert(t, "pack3", packed != U64_MAX);        //U64_MAX marks a component that didn't fit in 21 bits

	const I32x4 unpacked = BindlessDescriptor_unpack3(packed);

	Test_assert(t, "unpack3X", (BindlessDescriptor) I32x4_x(unpacked) == a);
	Test_assert(t, "unpack3Y", (BindlessDescriptor) I32x4_y(unpacked) == b);
	Test_assert(t, "unpack3Z", (BindlessDescriptor) I32x4_z(unpacked) == c);

	//None is a handle like any other to pack3, it just doesn't address a descriptor.

	const U64 none = BindlessDescriptor_pack3(BindlessDescriptor_None, BindlessDescriptor_None, BindlessDescriptor_None);
	Test_assert(t, "pack3None", none == 0);

	const I32x4 unpackedNone = BindlessDescriptor_unpack3(none);

	Test_assert(t, "unpack3None", !I32x4_x(unpackedNone) && !I32x4_y(unpackedNone) && !I32x4_z(unpackedNone));

	//Anything wider than 21 bits can't be packed, whichever component it is.

	const BindlessDescriptor tooWide = (BindlessDescriptor)1 << 21;

	Test_assert(t, "pack3OverflowX", BindlessDescriptor_pack3(tooWide, 0, 0) == U64_MAX);
	Test_assert(t, "pack3OverflowY", BindlessDescriptor_pack3(0, tooWide, 0) == U64_MAX);
	Test_assert(t, "pack3OverflowZ", BindlessDescriptor_pack3(0, 0, tooWide) == U64_MAX);
}

// -- Device dependent, skipped without an adapter --------------------------------

//7 to 10 live in Test_graphicsDeviceForApi below, which brings up the device the rest of these run against.

// -- 11. DescriptorLayout / DescriptorTable --------------------------------------

//Builds a small explicit layout, a heap sized for it and a table on top of both, then checks the invariants the
// headers promise before tearing all of it back down.
//The layout is bindless where the device has the feature and bindful where it doesn't, so whichever adapter CI
// hands us exercises one of the two paths.

static void Test_graphicsDescriptorTable(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "DescriptorLayout");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	const Bool hasBindless = (device->info.capabilities.features & EGraphicsFeatures_Bindless) != 0;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	DescriptorTableRef *secondTable = NULL;
	DescriptorLayoutRef *badLayout = NULL;
	DescriptorTableRef *badTable = NULL;
	DeviceBufferRef *buffer = NULL;
	DeviceBufferRef *routed = NULL;

	//Two arrays and one single descriptor, all byte address buffers so no texture format rules come into play.
	//SPIRV wants unique binding ids within a set and DXIL wants non overlapping ranges per register type;
	// t0-t3, u4-u7 and t8 satisfies both, so the test doesn't have to know which api it's running on.

	CharString bindingNames[3] = {
		CharString_createRefCStrConst("testBuffers"),
		CharString_createRefCStrConst("testRWBuffers"),
		CharString_createRefCStrConst("testBuffer")
	};

	DescriptorBinding bindings[3] = {
		(DescriptorBinding) {
			.registerType = ESHRegisterType_ByteAddressBuffer,
			.count = 4,
			.binding = (SHBinding) { .space = 0, .binding = 0 },
			.visibility = 1 << ESHPipelineStage_Compute
		},
		(DescriptorBinding) {
			.registerType = ESHRegisterType_ByteAddressBuffer | ESHRegisterType_IsWrite,
			.count = 4,
			.binding = (SHBinding) { .space = 0, .binding = 4 },
			.visibility = 1 << ESHPipelineStage_Compute
		},
		(DescriptorBinding) {
			.registerType = ESHRegisterType_ByteAddressBuffer,
			.count = 1,
			.binding = (SHBinding) { .space = 0, .binding = 8 },
			.visibility = 1 << ESHPipelineStage_Compute
		}
	};

	CharString name = CharString_createRefCStrConst("Test descriptor layout");

	//Parameter validation.
	//Every info here refs stack memory, so a rejected create has nothing to leak.

	DescriptorLayoutInfo info = (DescriptorLayoutInfo) {
		.flags = hasBindless ? EDescriptorLayoutFlags_AllowBindlessOnArrays : EDescriptorLayoutFlags_None
	};

	Test_assert(t, "bindingsRef", ListDescriptorBinding_createRefConst(bindings, 3, &info.bindings, &t->err));
	Test_assert(t, "namesRef", ListCharString_createRefConst(bindingNames, 3, &info.bindingNames, &t->err));

	Test_assert(t, "layoutNullDevice", !GraphicsDeviceRef_createDescriptorLayout(NULL, &info, &name, &badLayout, NULL));
	Test_assert(t, "layoutNullInfo", !GraphicsDeviceRef_createDescriptorLayout(deviceRef, NULL, &name, &badLayout, NULL));
	Test_assert(t, "layoutNullOut", !GraphicsDeviceRef_createDescriptorLayout(deviceRef, &info, &name, NULL, NULL));

	//A constant buffer has to declare its size and push constants belong in a pipeline layout, not a descriptor layout.

	DescriptorBinding sizelessCBuffer = (DescriptorBinding) {
		.registerType = ESHRegisterType_ConstantBuffer,
		.count = 1,
		.binding = (SHBinding) { .space = 0, .binding = 0 },
		.visibility = 1 << ESHPipelineStage_Compute
	};

	DescriptorBinding pushConstant = (DescriptorBinding) {
		.registerType = ESHRegisterType_PushConstants,
		.count = 1,
		.binding = (SHBinding) { .space = 0, .binding = 0 },
		.visibility = 1 << ESHPipelineStage_Compute,
		.constantBufferSize = 16
	};

	DescriptorLayoutInfo cbufferInfo = (DescriptorLayoutInfo) { 0 };
	DescriptorLayoutInfo pushInfo = (DescriptorLayoutInfo) { 0 };

	Test_assert(t, "cbufferRef", ListDescriptorBinding_createRefConst(&sizelessCBuffer, 1, &cbufferInfo.bindings, &t->err));
	Test_assert(t, "pushRef", ListDescriptorBinding_createRefConst(&pushConstant, 1, &pushInfo.bindings, &t->err));

	Test_assert(t, "layoutSizelessCBuffer", !GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &cbufferInfo, &name, &badLayout, NULL
	));

	Test_assert(t, "layoutPushConstants", !GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &pushInfo, &name, &badLayout, NULL
	));

	//Bindless and push descriptors exclude each other.
	//On a device without bindless the flag on its own is already illegal, so this is rejected either way.

	DescriptorLayoutInfo mixedInfo = (DescriptorLayoutInfo) {
		.flags = EDescriptorLayoutFlags_AllowBindlessOnArrays | EDescriptorLayoutFlags_HasPushDescriptors
	};

	Test_assert(t, "mixedRef", ListDescriptorBinding_createRefConst(bindings, 3, &mixedInfo.bindings, &t->err));

	Test_assert(t, "layoutBindlessPushDescriptors", !GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &mixedInfo, &name, &badLayout, NULL
	));

	Test_assert(t, "layoutRejectedNothing", !badLayout);

	//A heap of its own, sized for exactly the 9 buffer descriptors the layout declares.

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) {
		.flags = hasBindless ? EDescriptorHeapFlags_AllowBindless : EDescriptorHeapFlags_None,
		.maxBuffersRW = 9,
		.maxDescriptorTables = 1
	};

	DescriptorHeapInfo emptyHeapInfo = (DescriptorHeapInfo) { .maxDescriptorTables = 1 };

	name = CharString_createRefCStrConst("Test descriptor heap");

	Test_assert(t, "heapNullDevice", !GraphicsDeviceRef_createDescriptorHeap(NULL, &heapInfo, &name, &heap, NULL));
	Test_assert(t, "heapNullInfo", !GraphicsDeviceRef_createDescriptorHeap(deviceRef, NULL, &name, &heap, NULL));

	Test_assert(t, "heapNoDescriptors", !GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &emptyHeapInfo, &name, &heap, NULL
	));

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(deviceRef, &heapInfo, &name, &heap, &t->err)))
		goto clean;

	//Real layout create; the info is moved into the layout, which is what the emptied lists below check.

	name = CharString_createRefCStrConst("Test descriptor layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(deviceRef, &info, &name, &layout, &t->err)))
		goto clean;

	Test_assert(t, "layoutTypeId", layout->refPtrType->typeId == (TypeId) EGraphicsTypeId_DescriptorLayout);
	Test_assert(t, "layoutInfoMoved", !info.bindings.ptr && !info.bindingNames.ptr);

	const DescriptorLayout *layoutPtr = DescriptorLayoutRef_ptr(layout);

	Test_assert(t, "layoutDevice", layoutPtr->device == deviceRef);
	Test_assert(t, "layoutBindingCount", layoutPtr->info.bindings.length == 3);
	Test_assert(t, "layoutNameCount", layoutPtr->info.bindingNames.length == 3);
	Test_assert(t, "layoutOwnsBindings", !ListDescriptorBinding_isRef(layoutPtr->info.bindings));
	Test_assert(t, "layoutArrayCount", layoutPtr->info.bindings.ptr[0].count == 4);
	Test_assert(t, "layoutSingleCount", layoutPtr->info.bindings.ptr[2].count == 1);
	Test_assert(t, "layoutAnyResource", layoutPtr->anyResource);
	Test_assert(t, "layoutNoSampler", !layoutPtr->anySampler);

	//Only arrays become bindless types, and there can be at most 15 of them because of BindlessDescriptor's layout.
	//Without the feature the layout is bindful, so neither mapping exists at all.

	if (hasBindless) {

		if(Test_assert(t, "bindlessTypeCount", layoutPtr->bindlessTypeToBinding.length == 2)) {
			Test_assert(t, "bindlessType0", !layoutPtr->bindlessTypeToBinding.ptr[0]);
			Test_assert(t, "bindlessType1", layoutPtr->bindlessTypeToBinding.ptr[1] == 1);
		}

		if(Test_assert(t, "bindlessMapCount", layoutPtr->bindingToBindlessType.length == 3)) {
			Test_assert(t, "bindlessMap0", !layoutPtr->bindingToBindlessType.ptr[0]);
			Test_assert(t, "bindlessMap1", layoutPtr->bindingToBindlessType.ptr[1] == 1);
			Test_assert(t, "bindlessMapNonArray", layoutPtr->bindingToBindlessType.ptr[2] == U8_MAX);
		}
	}

	else {
		Test_assert(t, "noBindlessTypes", !layoutPtr->bindlessTypeToBinding.length);
		Test_assert(t, "noBindlessMap", !layoutPtr->bindingToBindlessType.length);
	}

	//Table create + validation.

	Test_setModule(t, "DescriptorTable");

	name = CharString_createRefCStrConst("Test descriptor table");

	Test_assert(t, "tableNullParent", !DescriptorHeapRef_createDescriptorTable(
		NULL, layout, EDescriptorTableFlags_None, &name, &badTable, NULL
	));

	Test_assert(t, "tableNullLayout", !DescriptorHeapRef_createDescriptorTable(
		heap, NULL, EDescriptorTableFlags_None, &name, &badTable, NULL
	));

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	Test_assert(t, "tableTypeId", table->refPtrType->typeId == (TypeId) EGraphicsTypeId_DescriptorTable);

	const DescriptorTable *tablePtr = DescriptorTableRef_ptr(table);

	Test_assert(t, "tableParent", tablePtr->parent == heap);
	Test_assert(t, "tableLayout", tablePtr->layout == layout);
	Test_assert(t, "tableBindingCount", tablePtr->bindings.length == 3);
	Test_assert(t, "tableNoResources", !tablePtr->resources.length);

	//The heap was created with room for a single table, which is what maxDescriptorTables means.

	Test_assert(t, "tableOutOfSlots", !DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &secondTable, NULL
	));

	Test_assert(t, "tableRejectedNothing", !badTable && !secondTable);

	//Register names resolve to their binding index, anything else doesn't resolve at all.

	const CharString missingName = CharString_createRefCStrConst("notARegister");

	Test_assert(t, "resolveFirst", !DescriptorTableRef_resolveRegisterName(table, &bindingNames[0]));
	Test_assert(t, "resolveLast", DescriptorTableRef_resolveRegisterName(table, &bindingNames[2]) == 2);
	Test_assert(t, "resolveMissing", DescriptorTableRef_resolveRegisterName(table, &missingName) == U64_MAX);

	//A buffer to point descriptors at; the read only bindings above require ShaderRead on the resource.

	name = CharString_createRefCStrConst("Test descriptor table buffer");

	if(!Test_assert(t, "bufferCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL, &name, 256, &buffer, &t->err
	)))
		goto clean;

	const Descriptor desc = Descriptor_buffer(buffer, 0, 0, NULL, 0);
	U64 arrayId = U64_MAX;

	//Allocating picks a free slot in an array, so a binding with a count of 1 has nothing to pick from.

	Test_assert(t, "allocOnNonArray", !DescriptorTableRef_allocDescriptor(table, 2, &arrayId, false, &desc, NULL));
	Test_assert(t, "allocOutOfBounds", !DescriptorTableRef_allocDescriptor(table, 3, &arrayId, false, &desc, NULL));
	Test_assert(t, "allocNullArrayId", !DescriptorTableRef_allocDescriptor(table, 0, NULL, false, &desc, NULL));

	//That single descriptor is set through its binding index instead.
	//maintainRef stays false, so the table doesn't take ownership of the resource.

	Test_assert(t, "setSingle", DescriptorTableRef_setDescriptor(table, 2, 0, false, &desc, &t->err));
	Test_assert(t, "setSingleNoRef", !tablePtr->resources.length);
	Test_assert(t, "setSingleOOB", !DescriptorTableRef_setDescriptor(table, 2, 1, false, &desc, NULL));
	Test_assert(t, "unsetSingle", DescriptorTableRef_unsetDescriptors(table, 2, 0, 1, &t->err));

	if (hasBindless) {

		//A bindless allocation finds the array whose register type matches, which is the first binding here.

		U16 bindId = U16_MAX;
		U8 bindlessTypeId = U8_MAX;

		Test_assert(t, "allocBindless", DescriptorTableRef_allocDescriptorBindless(
			table, ESHRegisterType_ByteAddressBuffer, 0, &bindId, &bindlessTypeId, &arrayId, false, &desc, &t->err
		));

		Test_assert(t, "allocBindlessBinding", !bindId);
		Test_assert(t, "allocBindlessType", !bindlessTypeId);
		Test_assert(t, "allocBindlessArrayId", !arrayId);
		Test_assert(t, "unsetBindless", DescriptorTableRef_unsetDescriptors(table, bindId, arrayId, 1, &t->err));

		//Resources can be routed into a table of the caller's own, which is what every creator's
		// bindlessDescriptorTable parameter is for.

		name = CharString_createRefCStrConst("Test routed buffer");

		Test_assert(t, "routedCreate", GraphicsDeviceRef_createBuffer(
			deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderReadBindless, table,
			&name, 256, &routed, &t->err
		));

		if (routed) {

			const DeviceBuffer *routedPtr = DeviceBufferRef_ptr(routed);

			Test_assert(t, "routedTable", routedPtr->bindlessDescriptorTable == table);
			Test_assert(t, "routedHandle", routedPtr->readHandle != BindlessDescriptor_None);
			Test_assert(t, "routedValid", BindlessDescriptor_isValid(deviceRef, table, routedPtr->readHandle));
			Test_assert(t, "routedNoWriteHandle", routedPtr->writeHandle == BindlessDescriptor_None);
		}
	}

clean:

	//The table reads the layout while it's being freed but never took a ref on it, so it has to go first.

	RefPtr_dec(&routed);
	RefPtr_dec(&buffer);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);
}

// -- 12. Bindless descriptors in the device's default table ----------------------

static void Test_graphicsBindlessDescriptor(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "BindlessDescriptor");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!device->defaultDescriptorTable) {
		Test_print(t, "Device has no bindless descriptor table, skipping bindless descriptor tests");
		return;
	}

	DeviceBufferRef *buffer = NULL;
	CharString name = CharString_createRefCStrConst("Bindless descriptor test buffer");

	//ShaderRead without ExposeBindlessRead, so the buffer takes no descriptor of its own and this test owns the one
	// it allocates below.

	if(!Test_assert(t, "bufferCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL, &name, 256, &buffer, &t->err
	)))
		return;

	const Descriptor desc = Descriptor_buffer(buffer, 0, 0, NULL, 0);
	BindlessDescriptor handle = BindlessDescriptor_None;

	//NULL as the table means the device's default one.

	Test_assert(t, "allocate", GraphicsDeviceRef_allocateDescriptorBindless(
		deviceRef, NULL, ESHRegisterType_ByteAddressBuffer, 0, false, &desc, &handle, &t->err
	));

	Test_assert(t, "handleNotNone", handle != BindlessDescriptor_None);
	Test_assert(t, "handleHasType", BindlessDescriptor_getBindlessType(handle) != 0);
	Test_assert(t, "handleValid", BindlessDescriptor_isValid(deviceRef, NULL, handle));

	//The default layout has at most 13 bindless arrays, so type 15 can never resolve to one.

	Test_assert(t, "handleTypeOutOfRange", !BindlessDescriptor_isValid(deviceRef, NULL, (BindlessDescriptor)15 << 17));
	Test_assert(t, "handleNoDevice", !BindlessDescriptor_isValid(NULL, NULL, handle));

	//Allocation validation; a descriptor is required and so is somewhere to put the handle.

	Test_assert(t, "allocateNoDevice", !GraphicsDeviceRef_allocateDescriptorBindless(
		NULL, NULL, ESHRegisterType_ByteAddressBuffer, 0, false, &desc, &handle, NULL
	));

	BindlessDescriptor unused = BindlessDescriptor_None;

	Test_assert(t, "allocateNoDescriptor", !GraphicsDeviceRef_allocateDescriptorBindless(
		deviceRef, NULL, ESHRegisterType_ByteAddressBuffer, 0, false, NULL, &unused, NULL
	));

	Test_assert(t, "allocateNothingLeaked", unused == BindlessDescriptor_None);

	//Freeing hands the slot back, so allocating the same descriptor again lands on it.

	Test_assert(t, "free", GraphicsDeviceRef_freeDescriptorBindless(deviceRef, NULL, handle, &t->err));

	BindlessDescriptor reused = BindlessDescriptor_None;

	Test_assert(t, "reallocate", GraphicsDeviceRef_allocateDescriptorBindless(
		deviceRef, NULL, ESHRegisterType_ByteAddressBuffer, 0, false, &desc, &reused, &t->err
	));

	Test_assert(t, "slotReused", reused == handle);

	//Freeing twice and freeing None are both no-ops rather than errors, which is what resource destructors rely on.

	Test_assert(t, "freeAgain", GraphicsDeviceRef_freeDescriptorBindless(deviceRef, NULL, reused, &t->err));
	Test_assert(t, "freeTwice", GraphicsDeviceRef_freeDescriptorBindless(deviceRef, NULL, reused, &t->err));

	Test_assert(t, "freeNone", GraphicsDeviceRef_freeDescriptorBindless(
		deviceRef, NULL, BindlessDescriptor_None, &t->err
	));

	RefPtr_dec(&buffer);
}

// -- 13. DeviceBuffer only takes a bindless descriptor when asked ----------------

static void Test_graphicsBufferBindless(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "DeviceBuffer/bindless");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	DeviceBufferRef *plain = NULL;
	DeviceBufferRef *readOnly = NULL;
	DeviceBufferRef *readWrite = NULL;
	DeviceBufferRef *rejected = NULL;

	CharString name = CharString_createRefCStrConst("Bindless flagless buffer");

	//Without either Expose flag there's no descriptor and no table ref, whatever the device supports.

	Test_assert(t, "plainCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL, &name, 256, &plain, &t->err
	));

	if (plain) {

		const DeviceBuffer *plainPtr = DeviceBufferRef_ptr(plain);

		Test_assert(t, "plainNoReadHandle", plainPtr->readHandle == BindlessDescriptor_None);
		Test_assert(t, "plainNoWriteHandle", plainPtr->writeHandle == BindlessDescriptor_None);
		Test_assert(t, "plainNoTable", !plainPtr->bindlessDescriptorTable);
	}

	name = CharString_createRefCStrConst("Bindless read buffer");

	if (!device->defaultDescriptorTable) {

		//Asking to be exposed on a device that has nowhere to expose it has to fail rather than silently do nothing.

		Test_assert(t, "exposeWithoutBindless", !GraphicsDeviceRef_createBuffer(
			deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderReadBindless, NULL, &name, 256, &rejected, NULL
		));

		Test_assert(t, "exposeWithoutBindlessNothing", !rejected);
		Test_print(t, "Device has no bindless descriptor table, skipping exposed buffer tests");
		goto clean;
	}

	//ExposeBindlessRead takes a read descriptor and nothing else.

	Test_assert(t, "readCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderReadBindless, NULL, &name, 256, &readOnly, &t->err
	));

	if (readOnly) {

		const DeviceBuffer *readPtr = DeviceBufferRef_ptr(readOnly);

		Test_assert(t, "readHandle", readPtr->readHandle != BindlessDescriptor_None);
		Test_assert(t, "readNoWriteHandle", readPtr->writeHandle == BindlessDescriptor_None);
		Test_assert(t, "readDefaultTable", readPtr->bindlessDescriptorTable == device->defaultDescriptorTable);
		Test_assert(t, "readHandleValid", BindlessDescriptor_isValid(deviceRef, NULL, readPtr->readHandle));
	}

	//Both Expose flags take two descriptors, one per binding, so they can't be the same handle.

	name = CharString_createRefCStrConst("Bindless read write buffer");

	Test_assert(t, "rwCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRWBindless, NULL, &name, 256, &readWrite, &t->err
	));

	if (readWrite) {

		const DeviceBuffer *rwPtr = DeviceBufferRef_ptr(readWrite);

		Test_assert(t, "rwReadHandle", rwPtr->readHandle != BindlessDescriptor_None);
		Test_assert(t, "rwWriteHandle", rwPtr->writeHandle != BindlessDescriptor_None);
		Test_assert(t, "rwHandlesDiffer", rwPtr->readHandle != rwPtr->writeHandle);
		Test_assert(t, "rwWriteHandleValid", BindlessDescriptor_isValid(deviceRef, NULL, rwPtr->writeHandle));
	}

	//A table without a flag that says the resource may be exposed is a contradiction.

	Test_assert(t, "tableWithoutExposeFlag", !GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_None, device->defaultDescriptorTable,
		&name, 256, &rejected, NULL
	));

	Test_assert(t, "tableWithoutExposeFlagNothing", !rejected);

clean:

	RefPtr_dec(&readWrite);
	RefPtr_dec(&readOnly);
	RefPtr_dec(&plain);
}

// -- 14. CommandList -------------------------------------------------------------

//Recording is what the functional tests cover; this is lifecycle and parameter validation only.

static void Test_graphicsCommandList(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "CommandList");

	CommandListRef *commandList = NULL;

	Test_assert(t, "createNullDevice", !GraphicsDeviceRef_createCommandList(
		NULL, 2 * KIBI, 128, 64, true, &commandList, NULL
	));

	Test_assert(t, "createNullOut", !GraphicsDeviceRef_createCommandList(deviceRef, 2 * KIBI, 128, 64, true, NULL, NULL));
	Test_assert(t, "rejectedNothing", !commandList);

	if(!Test_assert(t, "create", GraphicsDeviceRef_createCommandList(
		deviceRef, 2 * KIBI, 128, 64, true, &commandList, &t->err
	)))
		return;

	Test_assert(t, "typeId", commandList->refPtrType->typeId == (TypeId) EGraphicsTypeId_CommandList);

	const CommandList *listPtr = CommandListRef_ptr(commandList);

	Test_assert(t, "device", listPtr->device == deviceRef);
	Test_assert(t, "size", Buffer_length(listPtr->data) == 2 * KIBI);
	Test_assert(t, "allowResize", listPtr->allowResize);
	Test_assert(t, "stateNew", listPtr->state == ECommandListState_New);
	Test_assert(t, "noCommands", !listPtr->commandOps.length);
	Test_assert(t, "noResources", !listPtr->resources.length);

	RefPtr_dec(&commandList);
	Test_assert(t, "freeNulled", !commandList);
}

// -- 16. Submit ------------------------------------------------------------------

//Submit is the only path that reaches GraphicsDevice_rebindDescriptors, which binds the descriptor tables and the
// globals constant buffer for the frame.
//On Vulkan without VK_KHR_push_descriptor that's also the path that allocates and binds the emulated per frame set,
// so nothing else in the suite exercises it.
//An empty but closed command list is enough, since the descriptors are bound before any recorded command is replayed.

static void Test_graphicsSubmit(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "GraphicsDevice/submit");

	CommandListRef *commandList = NULL;
	ListCommandListRef lists = (ListCommandListRef) { 0 };

	if(!Test_assert(t, "create", GraphicsDeviceRef_createCommandList(
		deviceRef, 2 * KIBI, 64, 16, true, &commandList, &t->err
	)))
		return;

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "stateOpen", CommandListRef_ptr(commandList)->state == ECommandListState_Open);
	Test_assert(t, "beginTwice", !CommandListRef_begin(commandList, true, U64_MAX, NULL));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));
	Test_assert(t, "stateClosed", CommandListRef_ptr(commandList)->state == ECommandListState_Closed);

	//Nothing to submit at all is the one combination that has to be refused

	Test_assert(t, "submitNothing", !GraphicsDeviceRef_submitCommands(deviceRef, NULL, NULL, NULL, 0, 0, NULL));
	Test_assert(t, "submitNullDevice", !GraphicsDeviceRef_submitCommands(NULL, NULL, NULL, NULL, 0, 0, NULL));

	if(Test_assert(t, "listRef", ListCommandListRef_createRefConst(&commandList, 1, &lists, &t->err))) {

		//Submitted twice so a second frame in flight is used, which is what picks a different emulated set

		Test_assert(t, "submit", GraphicsDeviceRef_submitCommands(deviceRef, &lists, NULL, NULL, 0, 0, &t->err));
		Test_assert(t, "submitAgain", GraphicsDeviceRef_submitCommands(deviceRef, &lists, NULL, NULL, 0, 0, &t->err));
		Test_assert(t, "wait", GraphicsDeviceRef_wait(deviceRef, &t->err));
	}

	RefPtr_dec(&commandList);
}

// -- 7-10. Device, DeviceBuffer and Swapchain ------------------------------------

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

	//8. Device create / wait

	if(!Test_assert(t, "deviceCreate", GraphicsDeviceRef_create(
		instRef, &preferred, EGraphicsDeviceFlags_None, EGraphicsBufferingMode_Default, NULL, &deviceRef, &t->err
	)))
		goto clean;

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
	Test_graphicsSubmit(t, deviceRef);

clean:

	ListGraphicsDeviceInfo_free(&infos, alloc);

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
		NULL, ETextureType_2D, 1, 1, 0, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
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
