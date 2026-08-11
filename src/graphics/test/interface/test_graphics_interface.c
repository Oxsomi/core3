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
//
//CommandList lifecycle, recording and scopes live in test_graphics_command_list.c; they're the largest untested
// surface in the module, so they get their own file to grow in.
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
#include "formats/oiSH/sh_file.h"
#include "types/test/test.h"
#include "types/container/memory_stream.h"
#include "types/container/texture_format.h"
#include "types/container/buffer.h"
#include "types/base/string_base.h"
#include "types/base/string_read_helper.h"
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

	//The support queries only read capabilities.dataTypes, so a synthesised info exercises both answers without
	// needing the adapter to happen to have or lack the type.

	GraphicsDeviceInfo none = (GraphicsDeviceInfo) { 0 };
	GraphicsDeviceInfo all = (GraphicsDeviceInfo) { 0 };

	all.capabilities.dataTypes =
		EGraphicsDataTypes_BCn | EGraphicsDataTypes_ASTC | EGraphicsDataTypes_RGB32f |
		EGraphicsDataTypes_D24S8 | EGraphicsDataTypes_S8;

	//Anything not gated by a data type is supported regardless of the device

	Test_assert(t, "rgba8Always", GraphicsDeviceInfo_supportsFormat(&none, ETextureFormat_RGBA8));

	Test_assert(t, "bc7NeedsBCn",
		!GraphicsDeviceInfo_supportsFormat(&none, ETextureFormat_BC7) &&
		GraphicsDeviceInfo_supportsFormat(&all, ETextureFormat_BC7)
	);

	Test_assert(t, "rgb32fNeedsType",
		!GraphicsDeviceInfo_supportsFormat(&none, ETextureFormat_RGB32f) &&
		GraphicsDeviceInfo_supportsFormat(&all, ETextureFormat_RGB32f)
	);

	//Render targets additionally exclude compressed formats, even where the device can sample them

	Test_assert(t, "bc7NoRenderTarget", !GraphicsDeviceInfo_supportsRenderTextureFormat(&all, ETextureFormat_BC7));
	Test_assert(t, "rgba8RenderTarget", GraphicsDeviceInfo_supportsRenderTextureFormat(&none, ETextureFormat_RGBA8));

	//Plain depth needs nothing, the stencil combinations each need their own data type

	Test_assert(t, "d32Always", GraphicsDeviceInfo_supportsDepthStencilFormat(&none, EDepthStencilFormat_D32));

	Test_assert(t, "d24s8NeedsType",
		!GraphicsDeviceInfo_supportsDepthStencilFormat(&none, EDepthStencilFormat_D24S8Ext) &&
		GraphicsDeviceInfo_supportsDepthStencilFormat(&all, EDepthStencilFormat_D24S8Ext)
	);

	Test_assert(t, "d32s8NeedsType", !GraphicsDeviceInfo_supportsDepthStencilFormat(&all, EDepthStencilFormat_D32S8X24Ext));

	Test_assert(t, "supportsNullDevice",
		!GraphicsDeviceInfo_supportsFormat(NULL, ETextureFormat_RGBA8) &&
		!GraphicsDeviceInfo_supportsDepthStencilFormat(NULL, EDepthStencilFormat_D32)
	);
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

// -- 17. TLASTransformSRT (pure, no device) --------------------------------------

//The SRT layout is VkSRTDataNV, where scale, pivot and shearing are interleaved rather than kept as three vectors
// (sx, a, b / pvx, sy, c / pvy, sz, pvz).
//Setting all five with distinct values and reading all five back is what catches a crossed over field;
// round tripping one component at a time would pass even if two of them shared a slot.
//The struct is handed to the driver as is, so the raw fields are pinned here too.
//Compared exactly rather than approximately, since every value here is stored and loaded without any arithmetic.

static void Test_tlasTransformSRT(Test *t) {

	Test_setModule(t, "TLASTransformSRT");

	Test_assert(t, "size", sizeof(TLASTransformSRT) == 16 * sizeof(F32));

	TLASTransformSRT srt = (TLASTransformSRT) { 0 };

	TLASTransformSRT_setScale(&srt, F32x4_create3(1, 2, 3));
	TLASTransformSRT_setPivot(&srt, F32x4_create3(4, 5, 6));
	TLASTransformSRT_setTranslate(&srt, F32x4_create3(7, 8, 9));
	TLASTransformSRT_setShearing(&srt, F32x4_create3(10, 11, 12));
	TLASTransformSRT_setQuat(&srt, QuatF32_create(0.25f, 0.5f, 0.125f, 0.75f));

	Test_assert(t, "getScale", F32x4_eqExact4(TLASTransformSRT_getScale(&srt), F32x4_create3(1, 2, 3)));
	Test_assert(t, "getPivot", F32x4_eqExact4(TLASTransformSRT_getPivot(&srt), F32x4_create3(4, 5, 6)));
	Test_assert(t, "getTranslate", F32x4_eqExact4(TLASTransformSRT_getTranslate(&srt), F32x4_create3(7, 8, 9)));
	Test_assert(t, "getShearing", F32x4_eqExact4(TLASTransformSRT_getShearing(&srt), F32x4_create3(10, 11, 12)));

	Test_assert(t, "getQuat",
		F32x4_eqExact4(TLASTransformSRT_getQuat(&srt), QuatF32_create(0.25f, 0.5f, 0.125f, 0.75f))
	);

	//Pinned against VkSRTDataNV, since a wrong field here reaches the driver silently

	Test_assert(t, "layoutScale", srt.sx == 1 && srt.sy == 2 && srt.sz == 3);
	Test_assert(t, "layoutPivot", srt.pvx == 4 && srt.pvy == 5 && srt.pvz == 6);
	Test_assert(t, "layoutTranslate", srt.tx == 7 && srt.ty == 8 && srt.tz == 9);
	Test_assert(t, "layoutShearing", srt.a == 10 && srt.b == 11 && srt.c == 12);

	//createSimple leaves the two it doesn't take at zero, create fills all five

	const TLASTransformSRT simple = TLASTransformSRT_createSimple(
		F32x4_create3(2, 2, 2), F32x4_create3(1, 0, 0), QuatF32_identity()
	);

	Test_assert(t, "createSimpleScale", F32x4_eqExact4(TLASTransformSRT_getScale(&simple), F32x4_create3(2, 2, 2)));
	Test_assert(t, "createSimpleTranslate", F32x4_eqExact4(TLASTransformSRT_getTranslate(&simple), F32x4_create3(1, 0, 0)));
	Test_assert(t, "createSimpleNoPivot", F32x4_eqExact4(TLASTransformSRT_getPivot(&simple), F32x4_zero()));
	Test_assert(t, "createSimpleNoShearing", F32x4_eqExact4(TLASTransformSRT_getShearing(&simple), F32x4_zero()));

	const TLASTransformSRT full = TLASTransformSRT_create(
		F32x4_create3(1, 2, 3), F32x4_create3(4, 5, 6), F32x4_create3(7, 8, 9),
		QuatF32_identity(), F32x4_create3(10, 11, 12)
	);

	Test_assert(t, "createPivot", F32x4_eqExact4(TLASTransformSRT_getPivot(&full), F32x4_create3(4, 5, 6)));
	Test_assert(t, "createShearing", F32x4_eqExact4(TLASTransformSRT_getShearing(&full), F32x4_create3(10, 11, 12)));

	//NULL is a query on nothing rather than a crash, and the setters report that they did nothing

	Test_assert(t, "setNull",
		!TLASTransformSRT_setScale(NULL, F32x4_zero()) && !TLASTransformSRT_setPivot(NULL, F32x4_zero()) &&
		!TLASTransformSRT_setTranslate(NULL, F32x4_zero()) && !TLASTransformSRT_setShearing(NULL, F32x4_zero()) &&
		!TLASTransformSRT_setQuat(NULL, QuatF32_identity())
	);

	Test_assert(t, "getNullZero",
		F32x4_eqExact4(TLASTransformSRT_getScale(NULL), F32x4_zero()) &&
		F32x4_eqExact4(TLASTransformSRT_getPivot(NULL), F32x4_zero()) &&
		F32x4_eqExact4(TLASTransformSRT_getTranslate(NULL), F32x4_zero()) &&
		F32x4_eqExact4(TLASTransformSRT_getShearing(NULL), F32x4_zero())
	);

	//A zeroed quat would be a degenerate rotation, so NULL gives identity instead

	Test_assert(t, "getNullQuatIdentity", F32x4_eqExact4(TLASTransformSRT_getQuat(NULL), QuatF32_identity()));
}

// -- 18. Descriptor packing (pure, no device) ------------------------------------

//A buffer descriptor keeps a 48 bit start and end region, and hides the 32 bit counter offset in the top 16 bits of
// each of those two U64s.
//A counter offset with both halves set is what proves the split and the masking agree; one that fits in 16 bits
// would pass even if the high half were dropped.

static void Test_descriptorPacking(Test *t) {

	Test_setModule(t, "Descriptor/pack");

	const U64 start = 0x1000, end = 0x5000;
	const U32 counterOffset = 0xABCD1234;

	const Descriptor buf = Descriptor_buffer(NULL, start, end, NULL, counterOffset);

	Test_assert(t, "start", Descriptor_startBuffer(&buf) == start);
	Test_assert(t, "end", Descriptor_endBuffer(&buf) == end);
	Test_assert(t, "length", Descriptor_bufferLength(&buf) == end - start);
	Test_assert(t, "counterOffset", Descriptor_counterOffset(&buf) == counterOffset);

	//48 bits is the widest region that survives sharing its U64 with the counter offset

	const U64 max48 = ((U64)1 << 48) - 1;
	const Descriptor wide = Descriptor_buffer(NULL, 0, max48, NULL, counterOffset);

	Test_assert(t, "max48End", Descriptor_endBuffer(&wide) == max48);
	Test_assert(t, "max48Length", Descriptor_bufferLength(&wide) == max48);
	Test_assert(t, "max48Counter", Descriptor_counterOffset(&wide) == counterOffset);

	//Anything wider is refused with a null descriptor rather than silently truncated into the counter offset

	const Descriptor tooWideStart = Descriptor_buffer(NULL, (U64)1 << 48, end, NULL, 0);
	const Descriptor tooWideEnd = Descriptor_buffer(NULL, start, (U64)1 << 48, NULL, 0);

	Test_assert(t, "tooWideStart", !Descriptor_startBuffer(&tooWideStart) && !Descriptor_endBuffer(&tooWideStart));
	Test_assert(t, "tooWideEnd", !Descriptor_startBuffer(&tooWideEnd) && !Descriptor_endBuffer(&tooWideEnd));

	//Texture descriptors share the same union, so their fields have to land where the range isn't

	const Descriptor tex = Descriptor_texture(NULL, 1, 2, 3, 4, 5, 6);

	Test_assert(t, "textureMip", tex.texture.mipId == 1 && tex.texture.mipCount == 2);
	Test_assert(t, "texturePlane", tex.texture.planeId == 3 && tex.texture.imageId == 4);
	Test_assert(t, "textureArray", tex.texture.arrayId == 5 && tex.texture.arrayCount == 6);

	//tlas and sampler carry only the resource, so nothing may read back as a range

	const Descriptor tlas = Descriptor_tlas(NULL);
	const Descriptor sampler = Descriptor_sampler(NULL);

	Test_assert(t, "tlasEmpty", !Descriptor_startBuffer(&tlas) && !Descriptor_endBuffer(&tlas) && !tlas.resource);
	Test_assert(t, "samplerEmpty", !Descriptor_bufferLength(&sampler) && !sampler.resource);

	Test_assert(t, "nullDescriptor",
		!Descriptor_startBuffer(NULL) && !Descriptor_endBuffer(NULL) &&
		!Descriptor_bufferLength(NULL) && !Descriptor_counterOffset(NULL)
	);
}

// -- 19. TextureRange (pure, no device) ------------------------------------------

static void Test_textureRange(Test *t) {

	Test_setModule(t, "TextureRange");

	const TextureRange r = (TextureRange) { .startRange = { 1, 2, 3 }, .endRange = { 11, 22, 33 }, .levelId = 4 };

	Test_assert(t, "width", TextureRange_width(r) == 10);
	Test_assert(t, "height", TextureRange_height(r) == 20);
	Test_assert(t, "length", TextureRange_length(r) == 30);

	//An empty range is legal and measures zero, it isn't an error value

	const TextureRange empty = (TextureRange) { 0 };

	Test_assert(t, "emptyWidth", !TextureRange_width(empty));
	Test_assert(t, "emptyHeight", !TextureRange_height(empty));
	Test_assert(t, "emptyLength", !TextureRange_length(empty));
}

// -- 24. Default bindless layout (pure, no device) -------------------------------

//What OxC3's own shaders are compiled against, so the numbers here are a contract with resources.hlsli rather
// than an implementation detail that may drift.
//The DXIL offsets form a chain where each register range starts exactly where the previous one ends, which pins
// the exact numbers without this test needing the private count enums.

static void Test_graphicsDefaultBindlessLayout(Test *t) {

	Test_setModule(t, "GraphicsDevice/defaultLayout");

	const Allocator *alloc = Platform_instance->alloc;

	GraphicsDeviceInfo info = (GraphicsDeviceInfo) { 0 };
	GraphicsDeviceInfo infoRt = (GraphicsDeviceInfo) { 0 };
	infoRt.capabilities.features = EGraphicsFeatures_Raytracing;

	DescriptorLayoutInfo result = (DescriptorLayoutInfo) { 0 };

	Test_assert(t, "nullInfo", !GraphicsDevice_defaultBindlessLayout(NULL, ESHBinaryType_DXIL, &result, alloc, NULL));
	Test_assert(t, "nullResult", !GraphicsDevice_defaultBindlessLayout(&info, ESHBinaryType_DXIL, NULL, alloc, NULL));

	//A binary type without binding numbers is refused rather than silently handed DXIL's registers

	Test_assert(t, "unknownBinary", !GraphicsDevice_defaultBindlessLayout(
		&info, ESHBinaryType_Count, &result, alloc, NULL
	));

	Test_assert(t, "unknownLeftEmpty", !result.bindings.ptr && !result.bindingNames.ptr);

	//SPIRV without raytracing: samplers alone in set 0, everything else packed into set 1 in declaration order

	if(Test_assert(t, "spirv", GraphicsDevice_defaultBindlessLayout(
		&info, ESHBinaryType_SPIRV, &result, alloc, &t->err
	))) {

		Test_assert(t, "spirvCount", result.bindings.length == 12 && result.bindingNames.length == 12);
		Test_assert(t, "spirvFlags", result.flags == EDescriptorLayoutFlags_AllowBindlessOnArrays);

		//A non empty result is a leak about to happen, so it's refused before anything is written

		Test_assert(t, "nonEmptyRefused", !GraphicsDevice_defaultBindlessLayout(
			&info, ESHBinaryType_SPIRV, &result, alloc, NULL
		));

		const DescriptorBinding *b = result.bindings.ptr;

		Test_assert(t, "spirvSampler",
			b[0].registerType == ESHRegisterType_Sampler && !b[0].binding.space && !b[0].binding.binding
		);

		Test_assert(t, "spirvSamplerName", CharString_equalsCStringSensitive(&result.bindingNames.ptr[0], "_samplers"));

		Bool packed = true;
		Bool hasTlas = false;

		for(U64 i = 1; i < result.bindings.length; ++i) {
			packed &= b[i].binding.space == 1 && b[i].binding.binding == i - 1 && b[i].visibility == U32_MAX && b[i].count;
			hasTlas |= b[i].registerType == ESHRegisterType_AccelerationStructure;
		}

		Test_assert(t, "spirvPacked", packed);
		Test_assert(t, "spirvNoTlas", !hasTlas);

		DescriptorLayoutInfo_free(&result, alloc);
	}

	//Raytracing adds exactly one binding at the end rather than reshuffling anything before it

	if(Test_assert(t, "spirvRt", GraphicsDevice_defaultBindlessLayout(
		&infoRt, ESHBinaryType_SPIRV, &result, alloc, &t->err
	))) {

		Test_assert(t, "spirvRtCount", result.bindings.length == 13);

		const DescriptorBinding last = result.bindings.ptr[12];

		Test_assert(t, "spirvRtTlas",
			last.registerType == ESHRegisterType_AccelerationStructure &&
			last.binding.space == 1 && last.binding.binding == 11
		);

		Test_assert(t, "spirvRtTlasName", CharString_equalsCStringSensitive(&result.bindingNames.ptr[12], "_tlasExt"));

		DescriptorLayoutInfo_free(&result, alloc);
	}

	//DXIL: everything lives in space 0 and the ranges chain, SRVs through t and UAVs through u

	if(Test_assert(t, "dxil", GraphicsDevice_defaultBindlessLayout(
		&infoRt, ESHBinaryType_DXIL, &result, alloc, &t->err
	))) {

		const DescriptorBinding *b = result.bindings.ptr;

		Bool space0 = true;

		for(U64 i = 0; i < result.bindings.length; ++i)
			space0 &= !b[i].binding.space;

		Test_assert(t, "dxilSpace0", space0 && result.bindings.length == 13);
		Test_assert(t, "dxilSampler", !b[0].binding.binding);

		//[1] textures2D, [2] cubes, [3] 3D, [4] buffer, [12] tlas share the t namespace

		Test_assert(t, "dxilSrvChain",
			!b[1].binding.binding &&
			b[2].binding.binding == b[1].binding.binding + b[1].count &&
			b[3].binding.binding == b[2].binding.binding + b[2].count &&
			b[4].binding.binding == b[3].binding.binding + b[3].count &&
			b[12].binding.binding == b[4].binding.binding + b[4].count
		);

		//[5] rwBuffer then the six rw textures share the u namespace

		Bool uavChain = !b[5].binding.binding;

		for(U64 i = 6; i <= 11; ++i)
			uavChain &= b[i].binding.binding == b[i - 1].binding.binding + b[i - 1].count;

		Test_assert(t, "dxilUavChain", uavChain);

		DescriptorLayoutInfo_free(&result, alloc);
	}
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

	//The by name variants resolve the register name first, so they're the same operations routed differently
	// and an unknown name has to be refused everywhere rather than defaulting to binding 0.

	Test_assert(t, "setByName", DescriptorTableRef_setDescriptorByName(table, &bindingNames[2], 0, false, &desc, &t->err));
	Test_assert(t, "unsetByName", DescriptorTableRef_unsetDescriptorsByName(table, &bindingNames[2], 0, 1, &t->err));
	Test_assert(t, "setByNameMissing", !DescriptorTableRef_setDescriptorByName(table, &missingName, 0, false, &desc, NULL));

	ListDescriptor one = (ListDescriptor) { 0 };
	ListDescriptor_createRefConst(&desc, 1, &one, NULL);

	Test_assert(t, "setManyByName", DescriptorTableRef_setDescriptorsByName(
		table, &bindingNames[0], 0, false, &one, &t->err
	));

	Test_assert(t, "unsetManyByName", DescriptorTableRef_unsetDescriptorsByName(table, &bindingNames[0], 0, 1, &t->err));

	Test_assert(t, "unsetByNameMissing", !DescriptorTableRef_unsetDescriptorsByName(table, &missingName, 0, 1, NULL));

	Test_assert(t, "allocByName", DescriptorTableRef_allocDescriptorByName(
		table, &bindingNames[0], &arrayId, false, &desc, &t->err
	));

	Test_assert(t, "allocByNameId", arrayId < 4);
	Test_assert(t, "unsetAllocByName", DescriptorTableRef_unsetDescriptorsByName(table, &bindingNames[0], arrayId, 1, &t->err));

	Test_assert(t, "allocByNameNonArray", !DescriptorTableRef_allocDescriptorByName(
		table, &bindingNames[2], &arrayId, false, &desc, NULL
	));

	Test_assert(t, "allocByNameMissing", !DescriptorTableRef_allocDescriptorByName(
		table, &missingName, &arrayId, false, &desc, NULL
	));

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

		//findBindlessRegister answers "where would a resource like this go", so the type has to match a binding

		U16 foundBind = U16_MAX;
		U8 foundType = U8_MAX;

		Test_assert(t, "findRegister", DescriptorTableRef_findBindlessRegister(
			table, ESHRegisterType_ByteAddressBuffer, 0, &foundBind, &foundType, buffer, 0, &t->err
		));

		Test_assert(t, "findRegisterBinding", !foundBind && !foundType);

		Test_assert(t, "findRegisterMissing", !DescriptorTableRef_findBindlessRegister(
			table, ESHRegisterType_Sampler, 0, &foundBind, &foundType, buffer, 0, NULL
		));

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

// -- 23. TextureRef predicates and accessors -------------------------------------

//These decide what a resource is allowed to be used as, so they gate real behaviour rather than only describing it.
//isRenderTargetWritable is what clearImages and the colour attachments check, and isDepthStencil is what tells a
// depth attachment apart from a colour one, so getting either wrong silently misroutes a resource.

static void Test_graphicsTextureRef(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "TextureRef");

	RenderTextureRef *renderTexture = NULL;
	DepthStencilRef *depthStencil = NULL;
	DeviceBufferRef *buffer = NULL;

	const CharString renderName = CharString_createRefCStrConst("Predicate render texture");
	const CharString depthName = CharString_createRefCStrConst("Predicate depth stencil");
	const CharString bufferName = CharString_createRefCStrConst("Predicate buffer");

	Test_assert(t, "createRenderTexture", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 32, 16, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &renderName, &renderTexture, &t->err
	));

	Test_assert(t, "createDepthStencil", GraphicsDeviceRef_createDepthStencil(
		deviceRef, 32, 16, EDepthStencilFormat_D32, false, EMSAASamples_Off, NULL, &depthName, &depthStencil, &t->err
	));

	Test_assert(t, "createBuffer", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_Vertex, EGraphicsResourceFlag_None, NULL, &bufferName, 256, &buffer, &t->err
	));

	if(!renderTexture || !depthStencil || !buffer) {
		RefPtr_dec(&buffer);
		RefPtr_dec(&depthStencil);
		RefPtr_dec(&renderTexture);
		return;
	}

	//A texture is anything that resolves to a unified texture, which a buffer never does

	Test_assert(t, "renderIsTexture", TextureRef_isTexture(renderTexture));
	Test_assert(t, "depthIsTexture", TextureRef_isTexture(depthStencil));
	Test_assert(t, "bufferIsNotTexture", !TextureRef_isTexture(buffer));
	Test_assert(t, "nullIsNotTexture", !TextureRef_isTexture(NULL));

	//Depth is decided by the object kind, not by the format it happens to carry

	Test_assert(t, "depthIsDepthStencil", TextureRef_isDepthStencil(depthStencil));
	Test_assert(t, "renderIsNotDepthStencil", !TextureRef_isDepthStencil(renderTexture));
	Test_assert(t, "bufferIsNotDepthStencil", !TextureRef_isDepthStencil(buffer));
	Test_assert(t, "nullIsNotDepthStencil", !TextureRef_isDepthStencil(NULL));

	//Only render textures and swapchains can be written as a render target, so a depth stencil is excluded here
	// even though it is a perfectly valid attachment

	Test_assert(t, "renderIsWritable", TextureRef_isRenderTargetWritable(renderTexture));
	Test_assert(t, "depthIsNotWritable", !TextureRef_isRenderTargetWritable(depthStencil));
	Test_assert(t, "bufferIsNotWritable", !TextureRef_isRenderTargetWritable(buffer));
	Test_assert(t, "nullIsNotWritable", !TextureRef_isRenderTargetWritable(NULL));

	//The unified texture is what every command reads dimensions and ownership from

	const UnifiedTexture render = TextureRef_getUnifiedTexture(renderTexture, NULL);
	const UnifiedTexture depth = TextureRef_getUnifiedTexture(depthStencil, NULL);

	Test_assert(t, "renderSize", render.width == 32 && render.height == 16);
	Test_assert(t, "renderDevice", render.resource.device == deviceRef);
	Test_assert(t, "renderFormat", render.textureFormatId == (U8) ETextureFormatId_RGBA8 && !render.depthFormat);

	Test_assert(t, "depthSize", depth.width == 32 && depth.height == 16);
	Test_assert(t, "depthFormat", depth.depthFormat == (U8) EDepthStencilFormat_D32);

	//A resource that isn't a texture resolves to nothing rather than being reinterpreted as one

	const UnifiedTexture fromBuffer = TextureRef_getUnifiedTexture(buffer, NULL);
	const UnifiedTexture fromNull = TextureRef_getUnifiedTexture(NULL, NULL);

	Test_assert(t, "bufferHasNoTexture", !fromBuffer.resource.device && !fromBuffer.width);
	Test_assert(t, "nullHasNoTexture", !fromNull.resource.device && !fromNull.width);

	//Handles come from the per image bindless allocation, and nothing here was exposed

	Test_assert(t, "noReadHandle", TextureRef_getReadHandle(renderTexture, 0, 0) == BindlessDescriptor_None);
	Test_assert(t, "noWriteHandle", TextureRef_getWriteHandle(renderTexture, 0, 0) == BindlessDescriptor_None);

	Test_assert(t, "currMatchesImage0",
		TextureRef_getCurrReadHandle(renderTexture, 0) == TextureRef_getReadHandle(renderTexture, 0, 0)
	);

	//An out of range image, an unsupported subresource and a NULL ref all answer as nothing

	Test_assert(t, "handleImageOOB", TextureRef_getReadHandle(renderTexture, 0, 5) == BindlessDescriptor_None);
	Test_assert(t, "handleSubResource", TextureRef_getReadHandle(renderTexture, 1, 0) == BindlessDescriptor_None);
	Test_assert(t, "handleNull", TextureRef_getReadHandle(NULL, 0, 0) == BindlessDescriptor_None);

	const UnifiedTextureImage oob = TextureRef_getImage(renderTexture, 0, 5);
	Test_assert(t, "imageOOBZero", !oob.readHandle && !oob.writeHandle);

	//An exposed render target gets real, distinct handles that resolve in the default table

	if (GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.features & EGraphicsFeatures_Bindless) {

		RenderTextureRef *exposed = NULL;
		const CharString exposedName = CharString_createRefCStrConst("Predicate exposed target");

		if(Test_assert(t, "createExposed", GraphicsDeviceRef_createRenderTexture(
			deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8,
			EGraphicsResourceFlag_ShaderRWBindless, EMSAASamples_Off, NULL, &exposedName, &exposed, &t->err
		))) {

			const BindlessDescriptor read = TextureRef_getReadHandle(exposed, 0, 0);
			const BindlessDescriptor write = TextureRef_getWriteHandle(exposed, 0, 0);

			Test_assert(t, "exposedRead",
				read != BindlessDescriptor_None && BindlessDescriptor_isValid(deviceRef, NULL, read)
			);

			Test_assert(t, "exposedWrite",
				write != BindlessDescriptor_None && BindlessDescriptor_isValid(deviceRef, NULL, write)
			);

			Test_assert(t, "exposedDistinct", read != write);
		}

		RefPtr_dec(&exposed);
	}

	RefPtr_dec(&buffer);
	RefPtr_dec(&depthStencil);
	RefPtr_dec(&renderTexture);
}

// -- 25. Sampler and initial data uploads ----------------------------------------

//createSampler validates every field before allocating, createBufferData either moves or copies its data based
// on ownership, and a CPU backed texture is what markDirty is for.
//None of these had any coverage, and the move/copy split is exactly the kind of contract that breaks silently.

static void Test_graphicsSamplerAndData(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Sampler");

	const Allocator *alloc = Platform_instance->alloc;

	SamplerRef *sampler = NULL;
	DeviceBufferRef *moved = NULL;
	DeviceBufferRef *reffed = NULL;
	DeviceTextureRef *texture = NULL;

	CharString name = CharString_createRefCStrConst("Test sampler");

	const SamplerInfo valid = (SamplerInfo) { 0 };
	SamplerInfo bad;

	Test_assert(t, "nullDevice", !GraphicsDeviceRef_createSampler(NULL, valid, true, NULL, &name, &sampler, NULL));

	bad = valid;
	bad.filter = 0xFF;
	Test_assert(t, "badFilter", !GraphicsDeviceRef_createSampler(deviceRef, bad, true, NULL, &name, &sampler, NULL));

	bad = valid;
	bad.addressU = 0xFF;
	Test_assert(t, "badAddress", !GraphicsDeviceRef_createSampler(deviceRef, bad, true, NULL, &name, &sampler, NULL));

	bad = valid;
	bad.aniso = 17;
	Test_assert(t, "badAniso", !GraphicsDeviceRef_createSampler(deviceRef, bad, true, NULL, &name, &sampler, NULL));

	bad = valid;
	bad.borderColor = 0xFF;
	Test_assert(t, "badBorder", !GraphicsDeviceRef_createSampler(deviceRef, bad, true, NULL, &name, &sampler, NULL));

	bad = valid;
	bad.comparisonFunction = 0xFF;
	Test_assert(t, "badCompare", !GraphicsDeviceRef_createSampler(deviceRef, bad, true, NULL, &name, &sampler, NULL));

	Test_assert(t, "rejectedNothing", !sampler);

	//disallowBindlessDescriptor keeps this identical on devices with and without bindless

	if(Test_assert(t, "create", GraphicsDeviceRef_createSampler(deviceRef, valid, true, NULL, &name, &sampler, &t->err))) {

		const Sampler *samplerPtr = SamplerRef_ptr(sampler);

		Test_assert(t, "device", samplerPtr->device == deviceRef);
		Test_assert(t, "noBindless", !samplerPtr->bindlessDescriptorTable);

		//A maxLod of 0 would make every sampler unusable, so it defaults to F16 max instead

		Test_assert(t, "maxLodDefaulted", samplerPtr->info.maxLod);
	}

	//createBufferData moves an owned buffer but copies a ref, since a ref can't be taken over

	Test_setModule(t, "DeviceBuffer/data");

	name = CharString_createRefCStrConst("Test moved buffer");

	Buffer owned = Buffer_createNull();
	Test_assert(t, "ownedAlloc", Buffer_createEmptyBytes(64, alloc, &owned, &t->err));

	if(Test_assert(t, "createMoved", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_None, NULL, &name, &owned, &moved, &t->err
	))) {
		Test_assert(t, "ownedMoved", !owned.ptr);
		Test_assert(t, "movedSize", DeviceBufferRef_ptr(moved)->resource.size == 64);
	}

	U8 stack[32] = { 1, 2, 3, 4 };
	Buffer dataRef = Buffer_createRefConst(stack, sizeof(stack));

	name = CharString_createRefCStrConst("Test copied buffer");

	if(Test_assert(t, "createRef", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_None, NULL, &name, &dataRef, &reffed, &t->err
	))) {
		Test_assert(t, "refIntact", dataRef.ptr == stack);
		Test_assert(t, "refSize", DeviceBufferRef_ptr(reffed)->resource.size == 32);
	}

	Test_assert(t, "nullData", !GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_None, NULL, &name, NULL, &moved, NULL
	));

	//A CPU backed texture keeps its data around, which is what markDirty re-uploads from

	Test_setModule(t, "DeviceTexture");

	name = CharString_createRefCStrConst("Test texture");

	Buffer texData = Buffer_createNull();
	Test_assert(t, "texAlloc", Buffer_createEmptyBytes(4 * 4 * 4, alloc, &texData, &t->err));

	if(Test_assert(t, "texCreate", GraphicsDeviceRef_createTexture(
		deviceRef, ETextureType_2D, ETextureFormatId_RGBA8, EGraphicsResourceFlag_CPUBacked,
		4, 4, 1, NULL, &name, &texData, &texture, &t->err
	))) {
		Test_assert(t, "texMarkDirty", DeviceTextureRef_markDirty(texture, 0, 0, 0, 2, 2, 1, &t->err));
		Test_assert(t, "texMarkDirtyOOB", !DeviceTextureRef_markDirty(texture, 100, 0, 0, 1, 1, 1, NULL));
	}

	Buffer_free(&texData, alloc);

	RefPtr_dec(&texture);
	RefPtr_dec(&reffed);
	RefPtr_dec(&moved);
	RefPtr_dec(&sampler);
}

// -- 26. PipelineLayout ----------------------------------------------------------

//Push constants have hard limits (4-128 bytes, multiple of 4, at least one stage) and the two descriptor layout
// slots each demand the opposite push descriptor flag, so handing a layout to the wrong slot has to be refused.

static void Test_graphicsPipelineLayout(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "PipelineLayout");

	PipelineLayoutRef *layout = NULL;
	DescriptorLayoutRef *pushDescLayout = NULL;
	DescriptorLayoutRef *plainLayout = NULL;

	CharString name = CharString_createRefCStrConst("Test pipeline layout");

	//PushConstants is the portable register type, since DXIL additionally accepts a constant buffer

	const PipelineLayoutInfo pc = (PipelineLayoutInfo) {
		.pushConstants = (DescriptorBinding) {
			.registerType = ESHRegisterType_PushConstants,
			.count = 1,
			.constantBufferSize = 16,
			.visibility = U32_MAX
		}
	};

	PipelineLayoutInfo bad;

	Test_assert(t, "nullDevice", !GraphicsDeviceRef_createPipelineLayout(NULL, &pc, &name, &layout, NULL));
	Test_assert(t, "nullInfo", !GraphicsDeviceRef_createPipelineLayout(deviceRef, NULL, &name, &layout, NULL));

	bad = pc;
	bad.pushConstants.constantBufferSize = 0;
	Test_assert(t, "pcZeroSize", !GraphicsDeviceRef_createPipelineLayout(deviceRef, &bad, &name, &layout, NULL));

	bad = pc;
	bad.pushConstants.constantBufferSize = 132;
	Test_assert(t, "pcTooBig", !GraphicsDeviceRef_createPipelineLayout(deviceRef, &bad, &name, &layout, NULL));

	bad = pc;
	bad.pushConstants.constantBufferSize = 18;
	Test_assert(t, "pcMisaligned", !GraphicsDeviceRef_createPipelineLayout(deviceRef, &bad, &name, &layout, NULL));

	bad = pc;
	bad.pushConstants.visibility = 0;
	Test_assert(t, "pcNoVisibility", !GraphicsDeviceRef_createPipelineLayout(deviceRef, &bad, &name, &layout, NULL));

	bad = pc;
	bad.pushConstants.count = 2;
	Test_assert(t, "pcTwoRanges", !GraphicsDeviceRef_createPipelineLayout(deviceRef, &bad, &name, &layout, NULL));

	bad = pc;
	bad.pushConstants.registerType = ESHRegisterType_Sampler;
	Test_assert(t, "pcWrongType", !GraphicsDeviceRef_createPipelineLayout(deviceRef, &bad, &name, &layout, NULL));

	Test_assert(t, "rejectedNothing", !layout);

	if(Test_assert(t, "pcCreate", GraphicsDeviceRef_createPipelineLayout(deviceRef, &pc, &name, &layout, &t->err)))
		RefPtr_dec(&layout);

	//One push descriptor layout and one plain layout, so each can be offered to the slot meant for the other.
	//Space 3 keeps the constant buffer clear of the default layouts on both apis.

	DescriptorBinding cbv = (DescriptorBinding) {
		.registerType = ESHRegisterType_ConstantBuffer,
		.count = 1,
		.binding = (SHBinding) { .space = 3, .binding = 0 },
		.visibility = U32_MAX,
		.constantBufferSize = 64
	};

	CharString cbvName = CharString_createRefCStrConst("testPushCBuffer");

	DescriptorLayoutInfo pushInfo = (DescriptorLayoutInfo) { .flags = EDescriptorLayoutFlags_HasPushDescriptors };
	ListDescriptorBinding_createRefConst(&cbv, 1, &pushInfo.bindings, NULL);
	ListCharString_createRefConst(&cbvName, 1, &pushInfo.bindingNames, NULL);

	DescriptorBinding bab = (DescriptorBinding) {
		.registerType = ESHRegisterType_ByteAddressBuffer,
		.count = 1,
		.binding = (SHBinding) { .space = 0, .binding = 0 },
		.visibility = 1 << ESHPipelineStage_Compute
	};

	CharString babName = CharString_createRefCStrConst("testPlainBuffer");

	DescriptorLayoutInfo plainInfo = (DescriptorLayoutInfo) { 0 };
	ListDescriptorBinding_createRefConst(&bab, 1, &plainInfo.bindings, NULL);
	ListCharString_createRefConst(&babName, 1, &plainInfo.bindingNames, NULL);

	name = CharString_createRefCStrConst("Test push descriptor layout");
	Test_assert(t, "pushDescLayout", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &pushInfo, &name, &pushDescLayout, &t->err
	));

	name = CharString_createRefCStrConst("Test plain layout");
	Test_assert(t, "plainLayout", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &plainInfo, &name, &plainLayout, &t->err
	));

	name = CharString_createRefCStrConst("Test pipeline layout combos");

	if (pushDescLayout && plainLayout) {

		bad = (PipelineLayoutInfo) { .bindings = pushDescLayout };
		Test_assert(t, "pushAsBindings", !GraphicsDeviceRef_createPipelineLayout(deviceRef, &bad, &name, &layout, NULL));

		bad = (PipelineLayoutInfo) { .pushDescriptors = plainLayout };
		Test_assert(t, "plainAsPushDesc", !GraphicsDeviceRef_createPipelineLayout(deviceRef, &bad, &name, &layout, NULL));

		Test_assert(t, "comboRejectedNothing", !layout);

		const PipelineLayoutInfo good = (PipelineLayoutInfo) { .pushDescriptors = pushDescLayout };

		if(Test_assert(t, "pushDescCreate", GraphicsDeviceRef_createPipelineLayout(
			deviceRef, &good, &name, &layout, &t->err
		)))
			RefPtr_dec(&layout);
	}

	RefPtr_dec(&plainLayout);
	RefPtr_dec(&pushDescLayout);
}

// -- 27. Shader reflection and pipeline creation ---------------------------------

//The prebuilt image_copy.oiSH is the one shader every build with the compiler enabled ships, so it's what the
// reflection api can be exercised against without compiling anything in the test itself.
//This is the same route GraphicsDeviceRef_createPrebuiltShaders takes internally, but through the public api and
// with the failure cases the internal path never hits.

static void Test_graphicsShaderReflection(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "ShaderReflection");

	const Allocator *alloc = Platform_instance->alloc;

	Buffer data = Buffer_createNull();
	MemoryStreamRef *stream = NULL;
	SHFile file = (SHFile) { 0 };
	DescriptorBinding pushConst = (DescriptorBinding) { 0 };
	DescriptorLayoutInfo info = (DescriptorLayoutInfo) { 0 };
	DescriptorLayoutInfo pushDesc = (DescriptorLayoutInfo) { 0 };
	DescriptorLayoutRef *bindingsLayout = NULL;
	DescriptorLayoutRef *pushDescLayout = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;

	const CharString path = CharString_createRefCStrConst("//OxC3_graphics/shaders/image_copy.oiSH");
	const RefPtrType fileHandleType = FileHandle_makeType(alloc);
	const RefPtrType memStreamType = MemoryStream_makeType(alloc);

	//The same guard the device tests use; a build without the shader compiler has nothing to reflect

	if (!File_hasFile(&path, alloc)) {
		Test_print(t, "Prebuilt shaders unavailable, skipping shader reflection tests");
		return;
	}

	if(!Test_assert(t, "readFile", File_read(&path, U64_MAX, 0, 0, &fileHandleType, &data, &t->err)))
		return;

	U64 streamOffset = 0;

	Test_assert(t, "createStream", MemoryStream_createFromBufferRegion(
		Buffer_createRefFromBuffer(data, true), 0, Buffer_length(data),
		EMemoryStreamFlags_None, &memStreamType, &stream, &t->err
	));

	if(!stream || !Test_assert(t, "readSHFile", SHFile_read((StreamRef*)stream, &streamOffset, false, alloc, &file, &t->err)))
		goto clean;

	Test_assert(t, "hasEntries", file.entries.length && file.binaries.length);
	Test_assert(t, "isComplete", SHFile_isComplete(&file));

	//ROTATE is a uniform, so the two permutations are found by value and resolve to different binaries

	const CharString entryName = CharString_createRefCStrConst("mainSingle");
	const CharString missingName = CharString_createRefCStrConst("doesNotExist");

	CharString uniformsFalse[2] = { CharString_createRefCStrConst("ROTATE"), CharString_createRefCStrConst("false") };
	CharString uniformsTrue[2] = { CharString_createRefCStrConst("ROTATE"), CharString_createRefCStrConst("true") };

	ListCharString listFalse = (ListCharString) { 0 };
	ListCharString listTrue = (ListCharString) { 0 };
	ListCharString_createRefConst(uniformsFalse, 2, &listFalse, NULL);
	ListCharString_createRefConst(uniformsTrue, 2, &listTrue, NULL);

	const U32 idFalse = GraphicsDeviceRef_getFirstShaderEntry(
		deviceRef, &file, &entryName, NULL, &listFalse, ESHExtension_None, ESHExtension_None
	);

	const U32 idTrue = GraphicsDeviceRef_getFirstShaderEntry(
		deviceRef, &file, &entryName, NULL, &listTrue, ESHExtension_None, ESHExtension_None
	);

	Test_assert(t, "entryFound", idFalse != U32_MAX);
	Test_assert(t, "entryFoundTrue", idTrue != U32_MAX && idTrue != idFalse);

	Test_assert(t, "entryMissing", GraphicsDeviceRef_getFirstShaderEntry(
		deviceRef, &file, &missingName, NULL, &listFalse, ESHExtension_None, ESHExtension_None
	) == U32_MAX);

	//The copy shader doesn't use ray query, so requiring it can't find anything

	Test_assert(t, "entryWrongExtension", GraphicsDeviceRef_getFirstShaderEntry(
		deviceRef, &file, &entryName, NULL, &listFalse, ESHExtension_None, ESHExtension_RayQuery
	) == U32_MAX);

	Test_assert(t, "entryNullDevice", GraphicsDeviceRef_getFirstShaderEntry(
		NULL, &file, &entryName, NULL, &listFalse, ESHExtension_None, ESHExtension_None
	) == U32_MAX);

	if(idFalse == U32_MAX)
		goto clean;

	//The copy shader is compiled with push constants and push descriptors, which is what detect has to find

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &file, idFalse,
		EDescriptorLayoutFlags_None,
		EDetectDescriptorLayoutFlags_AssumePushDescriptors | EDetectDescriptorLayoutFlags_AssumePushConstants,
		NULL, NULL,
		&pushConst, &info, &pushDesc, &t->err
	)))
		goto clean;

	//SPIRV reflects the settings as a real push constant register, DXIL reflects them as a constant buffer that
	// AssumePushDescriptors routes into the push descriptors instead, so both shapes are legal here.

	Test_assert(t, "detectedPushConstantsSane", pushConst.count <= 1);

	if(pushConst.count)
		Test_assert(t, "detectedPushConstantsSize",
			pushConst.constantBufferSize && !(pushConst.constantBufferSize & 3) && pushConst.constantBufferSize <= 128
		);

	Test_assert(t, "detectedPushDescriptors", pushDesc.bindings.length >= 1);
	Test_assert(t, "detectedPushFlag", pushDesc.flags & EDescriptorLayoutFlags_HasPushDescriptors);

	//The detected layouts have to round trip through real creation, since that's what they're for

	CharString name = CharString_createRefCStrConst("Reflected push descriptor layout");

	if(!Test_assert(t, "createReflectedPushDesc", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &pushDesc, &name, &pushDescLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Reflected descriptor layout");

	if(info.bindings.length && !Test_assert(t, "createReflectedBindings", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &info, &name, &bindingsLayout, &t->err
	)))
		goto clean;

	const PipelineLayoutInfo pipelineInfo = (PipelineLayoutInfo) {
		.bindings = bindingsLayout,
		.pushDescriptors = pushDescLayout,
		.pushConstants = pushConst
	};

	name = CharString_createRefCStrConst("Reflected pipeline layout");

	if(!Test_assert(t, "createReflectedPipelineLayout", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	//checkShaderFeatures is what refuses an oiSH the device can't run, so the one the device does run has to pass

	const SHEntry *entry = &file.entries.ptr[(U16) idFalse];
	const SHBinaryInfo *binary = &file.binaries.ptr[entry->binaryIds.ptr[idFalse >> 16]];

	Test_assert(t, "checkFeatures", GraphicsDeviceRef_checkShaderFeatures(deviceRef, binary, entry, &t->err));
	Test_assert(t, "checkFeaturesNullDevice", !GraphicsDeviceRef_checkShaderFeatures(NULL, binary, entry, NULL));

	//An actual compute pipeline out of the public api, which nothing in the suite created before

	name = CharString_createRefCStrConst("Reflected compute pipeline");

	if(Test_assert(t, "createPipeline", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &file, &name, idFalse, &entryName, EPipelineFlags_None, pipelineLayout, &pipeline, &t->err
	)))
		Test_assert(t, "pipelineTypeId", pipeline->refPtrType->typeId == (TypeId) EGraphicsTypeId_Pipeline);

	//An out of bounds entry id has to be refused rather than read

	PipelineRef *badPipeline = NULL;

	Test_assert(t, "createPipelineBadEntry", !GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &file, &name, 0xFFFF, &entryName, EPipelineFlags_None, pipelineLayout, &badPipeline, NULL
	));

	Test_assert(t, "badPipelineNothing", !badPipeline);

clean:

	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&bindingsLayout);
	RefPtr_dec(&pushDescLayout);

	DescriptorLayoutInfo_free(&pushDesc, alloc);
	DescriptorLayoutInfo_free(&info, alloc);

	SHFile_free(&file, alloc);
	RefPtr_dec(&stream);
	Buffer_free(&data, alloc);
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

// -- 28. Memory budget, staging buffer and frame upkeep --------------------------

//Runs after submit on purpose: pendingResources has been flushed by then, which is what makes calling
// handleNextFrame without a real command buffer safe, and the staging buffer has actually seen use.

static void Test_graphicsDeviceMemory(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "GraphicsDevice/memory");

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	Test_assert(t, "budgetNullDevice", GraphicsDeviceRef_getMemoryBudget(NULL, false) == U64_MAX);

	const U64 shared = GraphicsDeviceRef_getMemoryBudget(deviceRef, false);
	const U64 local = GraphicsDeviceRef_getMemoryBudget(deviceRef, true);

	Test_assert(t, "budgetShared", shared && shared != U64_MAX);

	//Non dedicated devices report 0 device local by contract, since everything is the same memory

	if(device->info.type == EGraphicsDeviceType_Dedicated)
		Test_assert(t, "budgetLocalDedicated", local && local != U64_MAX);

	else Test_assert(t, "budgetLocalShared", !local);

	//The staging buffer can be resized at runtime; the size is aligned to three whole pages, one per frame

	Test_assert(t, "stagingNullDevice", !GraphicsDeviceRef_resizeStagingBuffer(NULL, 12288, NULL));
	Test_assert(t, "stagingExists", device->staging != NULL);

	const U64 oldSize = device->staging ? DeviceBufferRef_ptr(device->staging)->resource.size : 0;

	if(Test_assert(t, "stagingResize", GraphicsDeviceRef_resizeStagingBuffer(deviceRef, 12288, &t->err)))
		Test_assert(t, "stagingResized",
			device->staging && DeviceBufferRef_ptr(device->staging)->resource.size == 12288
		);

	//Restored so later frames keep the size the device was tuned with at creation

	if(oldSize)
		Test_assert(t, "stagingRestore", GraphicsDeviceRef_resizeStagingBuffer(deviceRef, oldSize, &t->err));

	//A resize that can't be satisfied fails the resize, not the device: the replacement is built before the swap,
	// so the old staging buffer has to still be there and still be its old size.
	//A PiB stays comfortably impossible even on machines with TiBs of memory.

	Test_assert(t, "stagingResizeFailsSafe", !GraphicsDeviceRef_resizeStagingBuffer(deviceRef, PEBI, NULL));

	Test_assert(t, "stagingSurvivesFailure",
		device->staging && DeviceBufferRef_ptr(device->staging)->resource.size == oldSize
	);

	//handleNextFrame is a frame step the submit path drives, so it demands the device lock is already held

	Test_assert(t, "frameNullDevice", !GraphicsDeviceRef_handleNextFrame(NULL, NULL, NULL));
	Test_assert(t, "frameNeedsLock", !GraphicsDeviceRef_handleNextFrame(deviceRef, NULL, NULL));

	//After submit and wait nothing is pending, so no flush runs and no command buffer is needed

	Test_assert(t, "nothingPending", !device->pendingResources.length);

	if (!device->pendingResources.length) {

		const ELockAcquire acq = SpinLock_lock(&device->lock, U64_MAX);

		Test_assert(t, "frameLocked", GraphicsDeviceRef_handleNextFrame(deviceRef, NULL, &t->err));
		Test_assert(t, "inFlightReleased", !device->resourcesInFlight[device->fifId].length);

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(&device->lock);
	}
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
	Test_graphicsCommandRecording(t, deviceRef);
	Test_graphicsCommandValidation(t, deviceRef);
	Test_graphicsRenderPass(t, deviceRef);
	Test_graphicsTextureRef(t, deviceRef);
	Test_graphicsSamplerAndData(t, deviceRef);
	Test_graphicsPipelineLayout(t, deviceRef);
	Test_graphicsShaderReflection(t, deviceRef);
	Test_graphicsSubmit(t, deviceRef);
	Test_graphicsDeviceMemory(t, deviceRef);

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
