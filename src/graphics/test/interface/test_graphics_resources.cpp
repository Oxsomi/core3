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

//graphics/test/interface/test_graphics_resources.cpp
//
//Resource modules: texture predicates, samplers and uploads, pipeline layouts and
// shader reflection (23, 25, 26, 27). These need a live device.
//Written against the C++ layer (graphics/graphics.hpp) rather than the C API.
//Same coverage as the C module it replaces, minus the teardown: every handle here releases itself, so there
//is no clean label, no goto chain and no ordered list of RefPtr_dec calls to keep in step with the locals.
//What is left is the test.

#include "graphics/graphics.hpp"

//memory_stream.h and file.h enter oxc::c in the order types/container/memory_stream.hpp and platforms/file.hpp
//establish for them; graphics.hpp already supplied every pre-include those two headers need.

namespace oxc { namespace c {
	#include "test_graphics_shared.h"
	#include "platforms/platform.h"
	#include "types/container/memory_stream.h"
	#include "platforms/file.h"
}}

//SHFile and DescriptorLayoutInfo are plain C structs with no wrapper, so they get a local guard rather than a
//manual free on every exit path.

namespace {

	//graphics.hpp already has the guard these were: OwnedList frees its list on every exit path, error
	//returns included.

	using OwnedSHFile = oxc::gfx::OwnedList<oxc::c::SHFile, oxc::c::SHFile_free>;
	using OwnedLayoutInfo = oxc::gfx::OwnedList<oxc::c::DescriptorLayoutInfo, oxc::c::DescriptorLayoutInfo_free>;
}

// -- 23. TextureRef predicates and accessors -------------------------------------

//These decide what a resource is allowed to be used as, so they gate real behaviour rather than only describing it.
//isRenderTargetWritable is what clearImages and the colour attachments check, and isDepthStencil is what tells a
// depth attachment apart from a colour one, so getting either wrong silently misroutes a resource.

extern "C" void Test_graphicsTextureRef(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Error *e_rr = &t->err;

	Test_setModule(t, "TextureRef");

	//The harness owns this ref, so it is borrowed rather than adopted.

	Device dev = Device::share(deviceRef);

	//Declared so the destructors unwind in the order the old clean block released them: buffer, then the depth
	//stencil, then the render texture.

	RenderTexture renderTexture;
	DepthStencil depthStencil;
	DeviceBuffer buffer;

	Test_assert(t, "createRenderTexture", dev.createRenderTexture(
		32, 16, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Predicate render texture", renderTexture,
		c::EMSAASamples_Off, nullptr, e_rr
	));

	Test_assert(t, "createDepthStencil", dev.createDepthStencil(
		32, 16, c::EDepthStencilFormat_D32, false, "Predicate depth stencil", depthStencil, c::EMSAASamples_Off, e_rr
	));

	Test_assert(t, "createBuffer", dev.createBuffer(
		c::EDeviceBufferUsage_Vertex, c::EGraphicsResourceFlag_None, "Predicate buffer", 256, buffer, nullptr, e_rr
	));

	if(!renderTexture || !depthStencil || !buffer)
		return;

	//Every predicate below answers for a NULL ref as well, which a wrapper holding a valid handle cannot
	// express, so the whole group stays on the C entry point and borrows each handle().

	//A texture is anything that resolves to a unified texture, which a buffer never does

	Test_assert(t, "renderIsTexture", renderTexture.isTexture());
	Test_assert(t, "depthIsTexture", depthStencil.isTexture());
	Test_assert(t, "bufferIsNotTexture", !c::TextureRef_isTexture(buffer.handle()));
	Test_assert(t, "nullIsNotTexture", !c::TextureRef_isTexture(nullptr));

	//Depth is decided by the object kind, not by the format it happens to carry

	Test_assert(t, "depthIsDepthStencil", depthStencil.isDepthStencil());
	Test_assert(t, "renderIsNotDepthStencil", !renderTexture.isDepthStencil());
	Test_assert(t, "bufferIsNotDepthStencil", !c::TextureRef_isDepthStencil(buffer.handle()));
	Test_assert(t, "nullIsNotDepthStencil", !c::TextureRef_isDepthStencil(nullptr));

	//Only render textures and swapchains can be written as a render target, so a depth stencil is excluded here
	// even though it is a perfectly valid attachment

	Test_assert(t, "renderIsWritable", renderTexture.isRenderTargetWritable());
	Test_assert(t, "depthIsNotWritable", !depthStencil.isRenderTargetWritable());
	Test_assert(t, "bufferIsNotWritable", !c::TextureRef_isRenderTargetWritable(buffer.handle()));
	Test_assert(t, "nullIsNotWritable", !c::TextureRef_isRenderTargetWritable(nullptr));

	//The unified texture is what every command reads dimensions and ownership from

	const c::UnifiedTexture render = renderTexture.unifiedTexture();
	const c::UnifiedTexture depth = depthStencil.unifiedTexture();

	Test_assert(t, "renderSize", render.width == 32 && render.height == 16);
	Test_assert(t, "renderDevice", render.resource.device == deviceRef);
	Test_assert(t, "renderFormat", render.textureFormatId == (c::U8) c::ETextureFormatId_RGBA8 && !render.depthFormat);

	Test_assert(t, "depthSize", depth.width == 32 && depth.height == 16);
	Test_assert(t, "depthFormat", depth.depthFormat == (c::U8) c::EDepthStencilFormat_D32);

	//A resource that isn't a texture resolves to nothing rather than being reinterpreted as one

	const c::UnifiedTexture fromBuffer = c::TextureRef_getUnifiedTexture(buffer.handle(), nullptr);
	const c::UnifiedTexture fromNull = c::TextureRef_getUnifiedTexture(nullptr, nullptr);

	Test_assert(t, "bufferHasNoTexture", !fromBuffer.resource.device && !fromBuffer.width);
	Test_assert(t, "nullHasNoTexture", !fromNull.resource.device && !fromNull.width);

	//Handles come from the per image bindless allocation, and nothing here was exposed

	Test_assert(t, "noReadHandle", renderTexture.readHandle(0, 0) == c::BindlessDescriptor_None);
	Test_assert(t, "noWriteHandle", renderTexture.writeHandle(0, 0) == c::BindlessDescriptor_None);

	Test_assert(t, "currMatchesImage0",
		renderTexture.currReadHandle(0) == renderTexture.readHandle(0, 0)
	);

	//An out of range image, an unsupported subresource and a NULL ref all answer as nothing

	Test_assert(t, "handleImageOOB", renderTexture.readHandle(0, 5) == c::BindlessDescriptor_None);
	Test_assert(t, "handleSubResource", renderTexture.readHandle(1, 0) == c::BindlessDescriptor_None);
	Test_assert(t, "handleNull", c::TextureRef_getReadHandle(nullptr, 0, 0) == c::BindlessDescriptor_None);

	const c::UnifiedTextureImage oob = c::TextureRef_getImage(renderTexture.handle(), 0, 5);
	Test_assert(t, "imageOOBZero", !oob.readHandle && !oob.writeHandle);

	//An exposed render target gets real, distinct handles that resolve in the default table

	if (dev.info().capabilities.features & c::EGraphicsFeatures_Bindless) {

		RenderTexture exposed;

		if(Test_assert(t, "createExposed", dev.createRenderTexture(
			8, 8, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_ShaderRWBindless,
			"Predicate exposed target", exposed, c::EMSAASamples_Off, nullptr, e_rr
		))) {

			const c::BindlessDescriptor read = exposed.readHandle(0, 0);
			const c::BindlessDescriptor write = exposed.writeHandle(0, 0);

			Test_assert(t, "exposedRead",
				read != c::BindlessDescriptor_None && c::BindlessDescriptor_isValid(deviceRef, nullptr, read)
			);

			Test_assert(t, "exposedWrite",
				write != c::BindlessDescriptor_None && c::BindlessDescriptor_isValid(deviceRef, nullptr, write)
			);

			Test_assert(t, "exposedDistinct", read != write);
		}
	}
}

// -- 25. Sampler and initial data uploads ----------------------------------------

//createSampler validates every field before allocating, createBufferData either moves or copies its data based
// on ownership, and a CPU backed texture is what markDirty is for.
//None of these had any coverage, and the move/copy split is exactly the kind of contract that breaks silently.

extern "C" void Test_graphicsSamplerAndData(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Error *e_rr = &t->err;
	const c::Allocator *alloc = c::Platform_instance->alloc;

	Test_setModule(t, "Sampler");

	//The harness owns this ref, so it is borrowed rather than adopted.

	Device dev = Device::share(deviceRef);

	//Declared so the destructors unwind in the order the old clean block released them: texture, the copied
	//buffer, the moved buffer, then the sampler.

	Sampler sampler;
	DeviceBuffer moved;
	DeviceBuffer reffed;
	DeviceTexture texture;

	const c::CharString samplerName = name("Test sampler");

	const c::SamplerInfo valid = c::SamplerInfo{};
	c::SamplerInfo bad;

	//Sampler creation stays on the C entry point: Device::createSampler pins disallowBindlessDescriptor to
	// false, and the whole point of this block is the path that allocates no bindless descriptor at all.
	//The raw result is adopted into the wrapper right after, so teardown is still automatic.
	//A null device has no wrapper spelling either way.

	c::SamplerRef *rawSampler = nullptr;

	Test_assert(t, "nullDevice", !c::GraphicsDeviceRef_createSampler(
		nullptr, valid, true, nullptr, &samplerName, &rawSampler, nullptr
	));

	bad = valid;
	bad.filter = 0xFF;
	Test_assert(t, "badFilter", !c::GraphicsDeviceRef_createSampler(
		deviceRef, bad, true, nullptr, &samplerName, &rawSampler, nullptr
	));

	bad = valid;
	bad.addressU = 0xFF;
	Test_assert(t, "badAddress", !c::GraphicsDeviceRef_createSampler(
		deviceRef, bad, true, nullptr, &samplerName, &rawSampler, nullptr
	));

	bad = valid;
	bad.aniso = 17;
	Test_assert(t, "badAniso", !c::GraphicsDeviceRef_createSampler(
		deviceRef, bad, true, nullptr, &samplerName, &rawSampler, nullptr
	));

	bad = valid;
	bad.borderColor = 0xFF;
	Test_assert(t, "badBorder", !c::GraphicsDeviceRef_createSampler(
		deviceRef, bad, true, nullptr, &samplerName, &rawSampler, nullptr
	));

	bad = valid;
	bad.comparisonFunction = 0xFF;
	Test_assert(t, "badCompare", !c::GraphicsDeviceRef_createSampler(
		deviceRef, bad, true, nullptr, &samplerName, &rawSampler, nullptr
	));

	Test_assert(t, "rejectedNothing", !rawSampler);

	//disallowBindlessDescriptor keeps this identical on devices with and without bindless

	if(Test_assert(t, "create", c::GraphicsDeviceRef_createSampler(
		deviceRef, valid, true, nullptr, &samplerName, &rawSampler, e_rr
	))) {

		sampler = Sampler(RefPtr<c::Sampler>::adopt(rawSampler));

		Test_assert(t, "device", sampler.data()->device == deviceRef);
		Test_assert(t, "noBindless", !sampler.data()->bindlessDescriptorTable);

		//A maxLod of 0 would make every sampler unusable, so it defaults to F16 max instead

		Test_assert(t, "maxLodDefaulted", sampler.data()->info.maxLod);
	}

	//createBufferData moves an owned buffer but copies a ref, since a ref can't be taken over

	Test_setModule(t, "DeviceBuffer/data");

	Buffer owned(*alloc);
	Test_assert(t, "ownedAlloc", owned.createEmptyBytes(64, e_rr));

	if(Test_assert(t, "createMoved", dev.createBufferData(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_None, "Test moved buffer", &owned.handle(), moved, nullptr, e_rr
	))) {
		Test_assert(t, "ownedMoved", !owned.handle().ptr);
		Test_assert(t, "movedSize", moved.data()->resource.size == 64);
	}

	c::U8 stack[32] = { 1, 2, 3, 4 };
	c::Buffer dataRef = c::Buffer_createRefConst(stack, sizeof(stack));

	if(Test_assert(t, "createRef", dev.createBufferData(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_None, "Test copied buffer", &dataRef, reffed, nullptr, e_rr
	))) {
		Test_assert(t, "refIntact", dataRef.ptr == stack);
		Test_assert(t, "refSize", reffed.data()->resource.size == 32);
	}

	Test_assert(t, "nullData", !dev.createBufferData(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_None, "Test copied buffer", nullptr, moved, nullptr, nullptr
	));

	//A CPU backed texture keeps its data around, which is what markDirty re-uploads from

	Test_setModule(t, "DeviceTexture");

	Buffer texData(*alloc);
	Test_assert(t, "texAlloc", texData.createEmptyBytes(4 * 4 * 4, e_rr));

	if(Test_assert(t, "texCreate", dev.createTexture(
		c::ETextureType_2D, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_CPUBacked,
		4, 4, 1, "Test texture", &texData.handle(), texture, nullptr, e_rr
	))) {
		Test_assert(t, "texMarkDirty", texture.markDirty(0, 0, 0, 2, 2, 1, e_rr));
		Test_assert(t, "texMarkDirtyOOB", !texture.markDirty(100, 0, 0, 1, 1, 1, nullptr));
	}
}

// -- 26. PipelineLayout ----------------------------------------------------------

//Push constants have hard limits (4-128 bytes, multiple of 4, at least one stage) and the two descriptor layout
// slots each demand the opposite push descriptor flag, so handing a layout to the wrong slot has to be refused.

extern "C" void Test_graphicsPipelineLayout(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Error *e_rr = &t->err;

	Test_setModule(t, "PipelineLayout");

	//The harness owns this ref, so it is borrowed rather than adopted.

	Device dev = Device::share(deviceRef);

	//Declared so the destructors unwind in the order the old clean block released them: the plain layout, then
	//the push descriptor one.
	//The pipeline layout is released inline on every path that creates one, exactly as the C module did.

	PipelineLayout layout;
	DescriptorLayout pushDescLayout;
	DescriptorLayout plainLayout;

	//PushConstants is the portable register type, since DXIL additionally accepts a constant buffer

	c::PipelineLayoutInfo pc{};
	pc.pushConstants.registerType = c::ESHRegisterType_PushConstants;
	pc.pushConstants.count = 1;
	pc.pushConstants.constantBufferSize = 16;
	pc.pushConstants.visibility = c::U32_MAX;

	c::PipelineLayoutInfo bad;

	//A null device and a null info have no wrapper spelling by design, so these two stay on the C entry point;
	// everything below that only has a bad info goes through the wrapper.

	const c::CharString layoutName = name("Test pipeline layout");
	c::PipelineLayoutRef *badLayout = nullptr;

	Test_assert(t, "nullDevice", !c::GraphicsDeviceRef_createPipelineLayout(nullptr, &pc, &layoutName, &badLayout, nullptr));
	Test_assert(
		t, "nullInfo", !c::GraphicsDeviceRef_createPipelineLayout(deviceRef, nullptr, &layoutName, &badLayout, nullptr)
	);

	bad = pc;
	bad.pushConstants.constantBufferSize = 0;
	Test_assert(t, "pcZeroSize", !dev.createPipelineLayout(bad, "Test pipeline layout", layout, nullptr));

	bad = pc;
	bad.pushConstants.constantBufferSize = 132;
	Test_assert(t, "pcTooBig", !dev.createPipelineLayout(bad, "Test pipeline layout", layout, nullptr));

	bad = pc;
	bad.pushConstants.constantBufferSize = 18;
	Test_assert(t, "pcMisaligned", !dev.createPipelineLayout(bad, "Test pipeline layout", layout, nullptr));

	bad = pc;
	bad.pushConstants.visibility = 0;
	Test_assert(t, "pcNoVisibility", !dev.createPipelineLayout(bad, "Test pipeline layout", layout, nullptr));

	bad = pc;
	bad.pushConstants.count = 2;
	Test_assert(t, "pcTwoRanges", !dev.createPipelineLayout(bad, "Test pipeline layout", layout, nullptr));

	bad = pc;
	bad.pushConstants.registerType = c::ESHRegisterType_Sampler;
	Test_assert(t, "pcWrongType", !dev.createPipelineLayout(bad, "Test pipeline layout", layout, nullptr));

	Test_assert(t, "rejectedNothing", !badLayout && !layout);

	if(Test_assert(t, "pcCreate", dev.createPipelineLayout(pc, "Test pipeline layout", layout, e_rr)))
		layout.release();

	//One push descriptor layout and one plain layout, so each can be offered to the slot meant for the other.
	//Space 3 keeps the constant buffer clear of the default layouts on both apis.

	c::DescriptorBinding cbv{};
	cbv.registerType = c::ESHRegisterType_ConstantBuffer;
	cbv.count = 1;
	cbv.binding.space = 3;
	cbv.binding.binding = 0;
	cbv.visibility = c::U32_MAX;
	cbv.constantBufferSize = 64;

	c::CharString cbvName = name("testPushCBuffer");

	//Both infos only ref stack memory, so neither needs an owning guard and a refused create leaks nothing.

	c::DescriptorLayoutInfo pushInfo{};
	pushInfo.flags = c::EDescriptorLayoutFlags_HasPushDescriptors;
	(void) c::ListDescriptorBinding_createRefConst(&cbv, 1, &pushInfo.bindings, nullptr);
	(void) c::ListCharString_createRefConst(&cbvName, 1, &pushInfo.bindingNames, nullptr);

	c::DescriptorBinding bab{};
	bab.registerType = c::ESHRegisterType_ByteAddressBuffer;
	bab.count = 1;
	bab.binding.space = 0;
	bab.binding.binding = 0;
	bab.visibility = 1 << c::ESHPipelineStage_Compute;

	c::CharString babName = name("testPlainBuffer");

	c::DescriptorLayoutInfo plainInfo{};
	(void) c::ListDescriptorBinding_createRefConst(&bab, 1, &plainInfo.bindings, nullptr);
	(void) c::ListCharString_createRefConst(&babName, 1, &plainInfo.bindingNames, nullptr);

	//A device without VK_KHR_push_descriptor refuses this outright (Android emulators, where gfxstream drops
	// the extension), which is the documented gap rather than something this module is testing.

	if(dev.createDescriptorLayout(pushInfo, "Test push descriptor layout", pushDescLayout, nullptr))
		Test_assert(t, "pushDescLayout", pushDescLayout.valid());

	else Test_print(t, "Device has no push descriptor support, skipping the push descriptor layout");

	Test_assert(t, "plainLayout", dev.createDescriptorLayout(plainInfo, "Test plain layout", plainLayout, e_rr));

	if (pushDescLayout && plainLayout) {

		bad = c::PipelineLayoutInfo{};
		bad.bindings = pushDescLayout.handle();
		Test_assert(t, "pushAsBindings", !dev.createPipelineLayout(bad, "Test pipeline layout combos", layout, nullptr));

		bad = c::PipelineLayoutInfo{};
		bad.pushDescriptors = plainLayout.handle();
		Test_assert(t, "plainAsPushDesc", !dev.createPipelineLayout(bad, "Test pipeline layout combos", layout, nullptr));

		Test_assert(t, "comboRejectedNothing", !layout);

		c::PipelineLayoutInfo good{};
		good.pushDescriptors = pushDescLayout.handle();

		if(Test_assert(t, "pushDescCreate", dev.createPipelineLayout(
			good, "Test pipeline layout combos", layout, e_rr
		)))
			layout.release();
	}
}

// -- 27. Shader reflection and pipeline creation ---------------------------------

//The prebuilt image_copy.oiSH is the one shader every build with the compiler enabled ships, so it's what the
// reflection api can be exercised against without compiling anything in the test itself.
//This is the same route GraphicsDeviceRef_createPrebuiltShaders takes internally, but through the public api and
// with the failure cases the internal path never hits.

extern "C" void Test_graphicsShaderReflection(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Error *e_rr = &t->err;
	const c::Allocator *alloc = c::Platform_instance->alloc;

	Test_setModule(t, "ShaderReflection");

	//The harness owns this ref, so it is borrowed rather than adopted.

	Device dev = Device::share(deviceRef);

	const c::CharString path = name("//OxC3_graphics/shaders/image_copy.oiSH");

	//Both types are read through for the whole life of everything made from them, so they lead the block and
	//outlive every handle below.

	const c::RefPtrType fileHandleType = c::FileHandle_makeType(alloc);
	const c::RefPtrType memStreamType = c::MemoryStream_makeType(alloc);

	//Declared so the destructors unwind in the order the old clean block released them: pipeline, pipeline
	//layout, bindings layout, push descriptor layout, the two detected infos, the oiSH, the stream, the bytes.

	Buffer data(*alloc);
	RefPtr<c::MemoryStream> stream;
	OwnedSHFile shader(alloc);
	OwnedLayoutInfo info(alloc);
	OwnedLayoutInfo pushDesc(alloc);
	DescriptorLayout pushDescLayout;
	DescriptorLayout bindingsLayout;
	PipelineLayout pipelineLayout;
	Pipeline pipeline;

	c::DescriptorBinding pushConst{};

	//The same guard the device tests use; a build without the shader compiler has nothing to reflect

	if (!c::File_hasFile(&path, alloc)) {
		Test_print(t, "Prebuilt shaders unavailable, skipping shader reflection tests");
		return;
	}

	if(!Test_assert(t, "readFile", c::File_read(&path, c::U64_MAX, 0, 0, &fileHandleType, &data.handle(), e_rr)))
		return;

	c::U64 streamOffset = 0;
	c::MemoryStreamRef *rawStream = nullptr;

	Test_assert(t, "createStream", c::MemoryStream_createFromBufferRegion(
		c::Buffer_createRefFromBuffer(data.handle(), true), 0, c::Buffer_length(data.handle()),
		c::EMemoryStreamFlags_None, &memStreamType, &rawStream, e_rr
	));

	stream = RefPtr<c::MemoryStream>::adopt(rawStream);

	//MemoryStreamRef and the StreamRef the oiSH reader consumes are both bare RefPtr typedefs, so the handle
	// crosses without the C module's cast.

	if (
		!stream ||
		!Test_assert(t, "readSHFile", c::SHFile_read(stream.handle(), &streamOffset, false, alloc, &shader.list, e_rr))
	)
		return;

	Test_assert(t, "hasEntries", shader.list.entries.length && shader.list.binaries.length);
	Test_assert(t, "isComplete", c::SHFile_isComplete(&shader.list));

	//ROTATE is a uniform, so the two permutations are found by value and resolve to different binaries

	const c::CharString entryName = name("mainSingle");
	const c::CharString missingName = name("doesNotExist");

	c::CharString uniformsFalse[2] = { name("ROTATE"), name("false") };
	c::CharString uniformsTrue[2] = { name("ROTATE"), name("true") };

	c::ListCharString listFalse{};
	c::ListCharString listTrue{};
	(void) c::ListCharString_createRefConst(uniformsFalse, 2, &listFalse, nullptr);
	(void) c::ListCharString_createRefConst(uniformsTrue, 2, &listTrue, nullptr);

	//Device::getFirstShaderEntry pins defines and uniforms to null, so it cannot select between two variants
	// compiled from the same source, which is exactly what these four look up; they stay on the C entry point.

	const c::U32 idFalse = c::GraphicsDeviceRef_getFirstShaderEntry(
		deviceRef, &shader.list, &entryName, nullptr, &listFalse, c::ESHExtension_None, c::ESHExtension_None
	);

	const c::U32 idTrue = c::GraphicsDeviceRef_getFirstShaderEntry(
		deviceRef, &shader.list, &entryName, nullptr, &listTrue, c::ESHExtension_None, c::ESHExtension_None
	);

	Test_assert(t, "entryFound", idFalse != c::U32_MAX);
	Test_assert(t, "entryFoundTrue", idTrue != c::U32_MAX && idTrue != idFalse);

	Test_assert(t, "entryMissing", c::GraphicsDeviceRef_getFirstShaderEntry(
		deviceRef, &shader.list, &missingName, nullptr, &listFalse, c::ESHExtension_None, c::ESHExtension_None
	) == c::U32_MAX);

	//The copy shader doesn't use ray query, so requiring it can't find anything

	Test_assert(t, "entryWrongExtension", c::GraphicsDeviceRef_getFirstShaderEntry(
		deviceRef, &shader.list, &entryName, nullptr, &listFalse, c::ESHExtension_None, c::ESHExtension_RayQuery
	) == c::U32_MAX);

	Test_assert(t, "entryNullDevice", c::GraphicsDeviceRef_getFirstShaderEntry(
		nullptr, &shader.list, &entryName, nullptr, &listFalse, c::ESHExtension_None, c::ESHExtension_None
	) == c::U32_MAX);

	if(idFalse == c::U32_MAX)
		return;

	//The copy shader is compiled with push constants and push descriptors, which is what detect has to find

	//Device::detectLayout pins the detect flags to none and gates each out parameter on a name list, so the
	// Assume* pair below has no wrapper spelling and this call stays on the C entry point.

	if(!Test_assert(t, "detectLayout", c::GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &shader.list, idFalse,
		c::EDescriptorLayoutFlags_None,
		(c::EDetectDescriptorLayoutFlags)(
			c::EDetectDescriptorLayoutFlags_AssumePushDescriptors | c::EDetectDescriptorLayoutFlags_AssumePushConstants
		),
		nullptr, nullptr,
		&pushConst, &info.list, &pushDesc.list, e_rr
	)))
		return;

	//SPIRV reflects the settings as a real push constant register, DXIL reflects them as a constant buffer that
	// AssumePushDescriptors routes into the push descriptors instead, so both shapes are legal here.

	Test_assert(t, "detectedPushConstantsSane", pushConst.count <= 1);

	if(pushConst.count)
		Test_assert(t, "detectedPushConstantsSize",
			pushConst.constantBufferSize && !(pushConst.constantBufferSize & 3) && pushConst.constantBufferSize <= 128
		);

	Test_assert(t, "detectedPushDescriptors", pushDesc.list.bindings.length >= 1);
	Test_assert(t, "detectedPushFlag", pushDesc.list.flags & c::EDescriptorLayoutFlags_HasPushDescriptors);

	//The detected layouts have to round trip through real creation, since that's what they're for

	if(!Test_assert(t, "createReflectedPushDesc", dev.createDescriptorLayout(
		pushDesc.list, "Reflected push descriptor layout", pushDescLayout, e_rr
	)))
		return;

	if(info.list.bindings.length && !Test_assert(t, "createReflectedBindings", dev.createDescriptorLayout(
		info.list, "Reflected descriptor layout", bindingsLayout, e_rr
	)))
		return;

	c::PipelineLayoutInfo pipelineInfo{};
	pipelineInfo.bindings = bindingsLayout.handle();
	pipelineInfo.pushDescriptors = pushDescLayout.handle();
	pipelineInfo.pushConstants = pushConst;

	if(!Test_assert(t, "createReflectedPipelineLayout", dev.createPipelineLayout(
		pipelineInfo, "Reflected pipeline layout", pipelineLayout, e_rr
	)))
		return;

	//checkShaderFeatures is what refuses an oiSH the device can't run, so the one the device does run has to pass

	const c::SHEntry *entry = &shader.list.entries.ptr[(c::U16) idFalse];
	const c::SHBinaryInfo *binary = &shader.list.binaries.ptr[entry->binaryIds.ptr[idFalse >> 16]];

	Test_assert(t, "checkFeatures", dev.checkShaderFeatures(*binary, *entry, e_rr));

	//A null device has no wrapper spelling, so this one negative stays on the C entry point.

	Test_assert(t, "checkFeaturesNullDevice", !c::GraphicsDeviceRef_checkShaderFeatures(nullptr, binary, entry, nullptr));

	//An actual compute pipeline out of the public api, which nothing in the suite created before

	//Device::createComputePipeline resolves the entry by name itself and pins the SPIRV entrypoint to "main",
	// so it can express neither the reflected id this module already resolved nor the entry name it belongs to;
	// both calls stay on the C entry point and the result is adopted into the wrapper for teardown.

	const c::CharString pipelineName = name("Reflected compute pipeline");
	c::PipelineRef *rawPipeline = nullptr;

	if(Test_assert(t, "createPipeline", c::GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &shader.list, &pipelineName, idFalse, &entryName, c::EPipelineFlags_None,
		pipelineLayout.handle(), &rawPipeline, e_rr
	))) {
		pipeline = Pipeline(RefPtr<c::Pipeline>::adopt(rawPipeline));
		Test_assert(t, "pipelineTypeId", pipeline.handle()->refPtrType->typeId == (c::TypeId) c::EGraphicsTypeId_Pipeline);
	}

	//An out of bounds entry id has to be refused rather than read

	c::PipelineRef *badPipeline = nullptr;

	Test_assert(t, "createPipelineBadEntry", !c::GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &shader.list, &pipelineName, 0xFFFF, &entryName, c::EPipelineFlags_None,
		pipelineLayout.handle(), &badPipeline, nullptr
	));

	Test_assert(t, "badPipelineNothing", !badPipeline);
}
