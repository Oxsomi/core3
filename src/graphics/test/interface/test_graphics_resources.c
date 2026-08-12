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

//graphics/test/interface/test_graphics_resources.c

//Resource modules: texture predicates, samplers and uploads, pipeline layouts and
// shader reflection (23, 25, 26, 27). These need a live device.

#include "graphics/generic/device.h"
#include "graphics/generic/device_info.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/render_texture.h"
#include "graphics/generic/depth_stencil.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/sampler.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/bindless_descriptor.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/graphics_types.h"
#include "platforms/platform.h"
#include "platforms/file.h"
#include "formats/oiSH/sh_file.h"
#include "types/test/test.h"
#include "types/container/memory_stream.h"
#include "types/container/texture_format.h"
#include "types/container/buffer.h"
#include "types/base/string_base.h"
#include "test_graphics_shared.h"

// -- 23. TextureRef predicates and accessors -------------------------------------

//These decide what a resource is allowed to be used as, so they gate real behaviour rather than only describing it.
//isRenderTargetWritable is what clearImages and the colour attachments check, and isDepthStencil is what tells a
// depth attachment apart from a colour one, so getting either wrong silently misroutes a resource.

void Test_graphicsTextureRef(Test *t, GraphicsDeviceRef *deviceRef) {

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

void Test_graphicsSamplerAndData(Test *t, GraphicsDeviceRef *deviceRef) {

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

void Test_graphicsPipelineLayout(Test *t, GraphicsDeviceRef *deviceRef) {

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

void Test_graphicsShaderReflection(Test *t, GraphicsDeviceRef *deviceRef) {

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
