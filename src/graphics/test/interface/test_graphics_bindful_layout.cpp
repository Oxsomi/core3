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

//graphics/test/interface/test_graphics_bindful_layout.cpp
//
//Pipeline layout state that is not a descriptor: push constants, and the register space OxC3 reserves
//for its own per frame globals.
//Split out of test_graphics_bindful.c, which had grown to 24 modules in one file.
//
//Written against the C++ layer (graphics/graphics.hpp): every handle releases itself, so there is no clean
//label, no goto chain and no ordered list of RefPtr_dec calls to keep in step with the locals.

#include "test_graphics_shared.hpp"

namespace oxc { namespace c {
	#include "graphics/generic/device_buffer.h"
}}

// -- 63. Push constants ---------------------------------------------------------

//Push constants are root constants rather than descriptors: the values ride in the command stream instead
// of the heap, so nothing about them goes through a table. That also means a root signature switch drops
// them on D3D12, which is why the backends re-emit at the work op rather than at the write.
//Two dispatches with different constants and no rebinding between them prove the re-emit really happens.

namespace {

	struct TestBindfulPushData {
		oxc::c::U32 scale, bias, xorMask, offset;
	};
}

extern "C" void Test_graphicsBindfulPushConstants(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;
	using namespace oxc::gfxtest;

	c::Error *e_rr = &t->err;

	c::Test_setModule(t, "Bindful/pushConstants");

	Device dev = Device::share(deviceRef);

	OwnedSHFile file(dev.alloc());

	if (!loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_pushconst.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping push constant tests");
		return;
	}

	const c::U32 entryId = entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	//Matched by shape rather than by name, which is what the engine's own copy shaders do: a global struct
	// becomes the implicit $Globals cbuffer on DXIL, so its register never carries the variable's name.
	//Vulkan reflects it as a real push constant register, so both backends land on the same binding here.

	OwnedLayoutInfo layoutInfo(dev.alloc());
	c::DescriptorBinding pushConstants{};

	if(!c::Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, &pushConstants, {}, nullptr,
		c::EDescriptorLayoutFlags_None, c::EDetectDescriptorLayoutFlags_AssumePushConstants, e_rr
	)))
		return;

	if (!c::Test_assert(t, "detectedPushConstants", pushConstants.count != 0))
		return;

	c::Test_assert(t, "pushConstantSize", pushConstants.constantBufferSize == sizeof(TestBindfulPushData));

	DescriptorLayout layout;

	if(!c::Test_assert(t, "layoutCreate", dev.createDescriptorLayout(
		layoutInfo.list, "Push constant layout", layout, e_rr
	)))
		return;

	c::DescriptorHeapInfo heapInfo{};
	heapInfo.maxBuffersRW = 1;
	heapInfo.maxDescriptorTables = 1;

	DescriptorHeap heap;

	if(!c::Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Push constant heap", heap, e_rr)))
		return;

	DescriptorTable table;

	if(!c::Test_assert(t, "tableCreate", heap.createTable(
		layout, "Push constant table", table, (c::EDescriptorTableFlags) 0, e_rr
	)))
		return;

	DeviceBuffer output;

	if(!c::Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Push constant output", 128 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	//The table holds no reference of its own, so the descriptor goes back before the buffer does.

	gfxtest::TableGuard tableGuard{ { &table } };

	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, nullptr, 0);

	c::Test_assert(t, "setOutput", table.setByName("output", outputDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.bindings = layout.handle();
	pipelineLayoutInfo.pushConstants = pushConstants;

	PipelineLayout pipelineLayout;

	if(!c::Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Push constant pipeline layout", pipelineLayout, e_rr
	)))
		return;

	Pipeline pipeline;

	if(!c::Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Push constant pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	CommandList commandList, emptyList;

	if(!c::Test_assert(t, "listCreate", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	if(!c::Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	c::Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	c::Test_assert(t, "endEmpty", emptyList.end(e_rr));

	const c::Transition transition = {
		.resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
	};

	c::Test_assert(t, "begin", commandList.begin(true, e_rr));

	//A dispatch without the constants written has to be refused: the range would hold whatever the last
	// pipeline left in it, which is exactly the garbage read this validation exists to prevent.
	//A partial write is refused for the same reason, and both live in their own scope because a refused
	// work op hides the whole scope.

	{
		CommandScope scope = commandList.scope({ transition }, 1, {}, e_rr);
		c::Test_assert(t, "scopeNeg", (c::Bool) scope);
		c::Test_assert(t, "bindHeapNeg", scope.bindDescriptorHeap(heap, e_rr));
		c::Test_assert(t, "bindTableNeg", scope.bindDescriptorTable(table, e_rr));
		c::Test_assert(t, "bindPipelineNeg", scope.setComputePipeline(pipeline, e_rr));
		c::Test_assert(t, "dispatchWithoutConstants", !scope.dispatch1D(1, nullptr));

		const c::U32 tooSmall = 4;

		c::Test_assert(t, "setTooSmall", scope.setPushConstants(tooSmall, e_rr));
		c::Test_assert(t, "dispatchWrongSize", !scope.dispatch1D(1, nullptr));
		c::Test_assert(t, "scopeNegEnd", scope.end(e_rr));
	}

	//Two dispatches, different constants, nothing rebound in between: the second set only lands if the
	// backend re-emits at the work op rather than once at the bind

	//Disjoint output ranges, so both results survive and neither dispatch races the other for a slot

	const TestBindfulPushData first = { .scale = 3, .bias = 7, .xorMask = 0, .offset = 0 };
	const TestBindfulPushData second = { .scale = 5, .bias = 1, .xorMask = 0xFFu, .offset = 64 };

	{
		CommandScope scope = commandList.scope({ transition }, 2, {}, e_rr);
		c::Test_assert(t, "scope", (c::Bool) scope);
		c::Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		c::Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		c::Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		c::Test_assert(t, "setFirst", scope.setPushConstants(first, e_rr));
		c::Test_assert(t, "dispatchFirst", scope.dispatch1D(1, e_rr));
		c::Test_assert(t, "setSecond", scope.setPushConstants(second, e_rr));
		c::Test_assert(t, "dispatchSecond", scope.dispatch1D(1, e_rr));
		c::Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	c::Test_assert(t, "end", commandList.end(e_rr));

	if (submitAndWait(t, dev, commandList))
		if (pullBuffer(t, dev, emptyList, output)) {

			//Each dispatch wrote its own half, so BOTH sets have to be visible: that is what proves the
			// second write reached the GPU instead of the first one being reused. Sharing one range instead
			// would just race the two dispatches, which says nothing about the push constants.

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i) {
				allMatch &= values[i] == ((i * first.scale + first.bias) ^ first.xorMask);
				allMatch &= values[i + 64] == ((i * second.scale + second.bias) ^ second.xorMask);
			}

			c::Test_assert(t, "pushConstantResults", allMatch);
		}
}

// -- 64. The reserved register space --------------------------------------------

//OxC3 binds its own per frame globals (frame id, time, swapchain descriptors) to a register space
// it keeps for itself, so a caller's layout may not put anything there.
//It matters now that anyone can build a layout: the globals used to sit at b0 space0, which is the first
// thing someone writing a constant buffer reaches for, and nothing would have told them they collided.

extern "C" void Test_graphicsBindfulReservedSpace(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Test_setModule(t, "Bindful/reservedSpace");

	Device dev = Device::share(deviceRef);

	//A layout that is legal in every respect except the space it asks for

	c::DescriptorBinding reserved = {
		.registerType = (c::ESHRegisterType) (c::ESHRegisterType_ByteAddressBuffer | c::ESHRegisterType_IsWrite),
		.count = 1,
		.binding = { .space = OXC3_RESERVED_SPACE, .binding = 0 },
		.visibility = c::U32_MAX
	};

	const c::CharString reservedName = c::CharString_createRefCStrConst("collides");

	c::DescriptorLayoutInfo info{};
	c::ListDescriptorBinding_createRefConst(&reserved, 1, &info.bindings, nullptr);
	c::ListCharString_createRefConst(&reservedName, 1, &info.bindingNames, nullptr);

	DescriptorLayout layout;

	c::Test_assert(t, "reservedSpaceRefused", !dev.createDescriptorLayout(
		info, "Reserved space layout", layout, nullptr
	));

	c::Test_assert(t, "reservedSpaceNoLayout", !layout.valid());

	//The very same binding in another space is fine, which is what proves the space is the only objection.
	//On Vulkan a space is a descriptor set index bounded by the device's maxBoundDescriptorSets, so "one
	// space over" from 0xC3 would be refused there for a reason that has nothing to do with the reservation.
	//Set 3 is the highest every device is required to be able to bind, so it proves the same thing there.

	reserved.binding.space = dev.api() == c::EGraphicsApi_Vulkan ? 3 : OXC3_RESERVED_SPACE + 1;

	c::DescriptorLayoutInfo okInfo{};
	c::ListDescriptorBinding_createRefConst(&reserved, 1, &okInfo.bindings, nullptr);
	c::ListCharString_createRefConst(&reservedName, 1, &okInfo.bindingNames, nullptr);

	c::Test_assert(t, "neighbourSpaceAccepted", dev.createDescriptorLayout(
		okInfo, "Reserved space layout", layout, &t->err
	));
}

// -- 63. What a push descriptor and a copy are allowed to be --------------------

//Two rules that were each enforced in only half the stack, so a caller could build something the recorder
//would always refuse, or ask for something both backends quietly ignored.

extern "C" void Test_graphicsBindfulPushClass(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Test_setModule(t, "Bindful/pushClass");

	Device dev = Device::share(deviceRef);

	//A sampler has no root descriptor form on D3D12 and is refused where the layout is built.
	//A TEXTURE is
	// deliberately NOT refused there: the device builds a texture push itself for its rotated copy shaders,
	// so the layout has to keep accepting one, and the work op is where a texture push actually stops.
	//Pinning both halves keeps that split honest, since it is easy to read as an oversight and "fix" by
	// tightening the layout, which breaks device creation.

	const c::CharString pushName = c::CharString_createRefCStrConst("pushed");

	c::DescriptorBinding push = {
		.registerType = c::ESHRegisterType_Sampler,
		.count = 1,
		.binding = { .space = 0, .binding = 0 },
		.visibility = c::U32_MAX
	};

	c::DescriptorLayoutInfo sampInfo{};
	sampInfo.flags = c::EDescriptorLayoutFlags_HasPushDescriptors;
	c::ListDescriptorBinding_createRefConst(&push, 1, &sampInfo.bindings, nullptr);
	c::ListCharString_createRefConst(&pushName, 1, &sampInfo.bindingNames, nullptr);

	DescriptorLayout layout;

	c::Test_assert(t, "samplerPushRefused", !dev.createDescriptorLayout(sampInfo, "Push class layout", layout, nullptr));
	c::Test_assert(t, "samplerPushNoLayout", !layout.valid());

	//A texture push builds, which is what the device's own copy layout depends on

	push.registerType = c::ESHRegisterType_Texture2D;

	c::DescriptorLayoutInfo texInfo{};
	texInfo.flags = c::EDescriptorLayoutFlags_HasPushDescriptors;
	c::ListDescriptorBinding_createRefConst(&push, 1, &texInfo.bindings, nullptr);
	c::ListCharString_createRefConst(&pushName, 1, &texInfo.bindingNames, nullptr);

	c::Test_assert(t, "texturePushAccepted", dev.createDescriptorLayout(texInfo, "Push class layout", layout, &t->err));

	//As does the buffer class the recorder can actually emit

	push.registerType = (c::ESHRegisterType) (c::ESHRegisterType_ByteAddressBuffer | c::ESHRegisterType_IsWrite);

	c::DescriptorLayoutInfo bufInfo{};
	bufInfo.flags = c::EDescriptorLayoutFlags_HasPushDescriptors;
	c::ListDescriptorBinding_createRefConst(&push, 1, &bufInfo.bindings, nullptr);
	c::ListCharString_createRefConst(&pushName, 1, &bufInfo.bindingNames, nullptr);

	c::Test_assert(t, "bufferPushAccepted", dev.createDescriptorLayout(bufInfo, "Push class layout", layout, &t->err));

	//A rotation is out of range rather than merely unsupported past 3

	CommandList commandList;
	RenderTexture src, dst, readWrite;
	DescriptorHeap pushHeap;
	c::Error *e_rr = &t->err;

	if(
		!c::Test_assert(t, "srcCreate", dev.createRenderTexture(
			8, 8, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_ShaderRead, "Push class src", src,
			c::EMSAASamples_Off, nullptr, e_rr
		)) ||
		!c::Test_assert(t, "dstCreate", dev.createRenderTexture(
			8, 8, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_ShaderWrite, "Push class dst", dst,
			c::EMSAASamples_Off, nullptr, e_rr
		)) ||
		!c::Test_assert(t, "readWriteCreate", dev.createRenderTexture(
			8, 8, c::ETextureFormatId_RGBA8,
			(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderRead | c::EGraphicsResourceFlag_ShaderWrite),
			"Push class read write", readWrite, c::EMSAASamples_Off, nullptr, e_rr
		)) ||
		!c::Test_assert(t, "listCreate", dev.createCommandList(4 * c::KIBI, 32, 16, commandList, true, e_rr))
	)
		return;

	//A rotated copy takes its two transient descriptors from the push ring of the heap the CALLER bound;
	// nothing binds one behind the caller's back, since a hidden SetDescriptorHeaps is exactly the cost the
	// explicit bind exists to keep visible.

	c::DescriptorHeapInfo pushHeapInfo = { .maxDescriptorTables = 1, .maxPushDescriptors = 4 };

	if(!c::Test_assert(t, "pushHeapCreate", dev.createDescriptorHeap(pushHeapInfo, "Push class heap", pushHeap, e_rr)))
		return;

	c::Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		c::Test_assert(t, "scope", (c::Bool) scope);

		//Out of range and merely unimplemented are different refusals, and both have to be refusals: a
		// rotation that fell through would hand back a silently unrotated image.

		c::Test_assert(t, "rotationOutOfRange", !scope.copyImage(
			src.handle(), dst.handle(), { .outputRotation = 4 }, nullptr
		));

		//A rotation with no heap bound yet is refused rather than binding one silently

		c::Test_assert(t, "rotationNeedsHeap", !scope.copyImage(
			src.handle(), dst.handle(), { .outputRotation = 2 }, nullptr
		));

		c::Test_assert(t, "bindPushHeap", scope.bindDescriptorHeap(pushHeap, e_rr));

		//A rotation reads src through a sampled descriptor and writes dst through a storage one, which a
		// texture only has if it was created asking for it.
		//Both backends gate that on the flag, so a texture without it has to be refused HERE.
		//Left to the backend, D3D12 builds a UAV over a resource that never allowed one and only the debug
		// layer notices, while Vulkan cannot create the view at all and the copy silently does not happen.
		//dst carries ShaderWrite alone and src ShaderRead alone, so each is exactly the wrong way round for
		// one of the two checks.

		c::Test_assert(t, "rotationSrcNeedsShaderRead", !scope.copyImage(
			dst.handle(), readWrite.handle(), { .outputRotation = 2 }, nullptr
		));

		c::Test_assert(t, "rotationDstNeedsShaderWrite", !scope.copyImage(
			readWrite.handle(), src.handle(), { .outputRotation = 2 }, nullptr
		));

		c::Test_assert(t, "copyUnrotated", scope.copyImage(src.handle(), dst.handle(), { 0 }, e_rr));

		c::Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	{
		//The same pair unrotated is a plain transfer on both backends and stays allowed, neither shader flag
		// being needed for one.
		//It copies the other way round, which is why it cannot share the scope above: a scope puts an image
		// in ONE state, so src being a copy destination here and a copy source there is the transition
		// conflict the recorder refuses.

		CommandScope unrotated = commandList.scope({}, 2, {}, e_rr);
		c::Test_assert(t, "unrotatedScope", (c::Bool) unrotated);

		c::Test_assert(t, "copyUnrotatedNoShaderFlags", unrotated.copyImage(
			dst.handle(), src.handle(), { 0 }, e_rr
		));

		c::Test_assert(t, "unrotatedScopeEnd", unrotated.end(e_rr));
	}

	c::Test_assert(t, "end", commandList.end(e_rr));

	//And a rotation that actually runs.
	//An image copy cannot express one on either API, so the recorder puts
	// the command on the device's own copy shader instead; this is the only thing that exercises that path,
	// and the only thing that proves the shader's rotation math rather than that a dispatch happened.
	//180 degrees is the case needing no reasoning about which axis swapped: every texel lands diagonally
	// opposite, so dst(x, y) has to be src(7 - x, 7 - y).

	DeviceTexture pattern;
	CommandList emptyList;

	c::U32 texels[64];

	for(c::U32 y = 0; y < 8; ++y)
		for(c::U32 x = 0; x < 8; ++x)
			texels[y * 8 + x] = 0xFF000000u | (y << 8) | x;        //R = x, G = y, so a flip shows up per axis

	c::Buffer texelRef = c::Buffer_createRefConst(texels, sizeof(texels));

	if(
		!c::Test_assert(t, "patternCreate", dev.createTexture(
			c::ETextureType_2D, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_ShaderRead, 8, 8, 1,
			"Push class pattern", &texelRef, pattern, nullptr, e_rr
		)) ||
		!c::Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr))
	)
		return;

	c::Test_assert(t, "beginEmptyList", emptyList.begin(true, e_rr));
	c::Test_assert(t, "endEmptyList", emptyList.end(e_rr));

	c::Test_assert(t, "beginRotate", commandList.begin(true, e_rr));

	{
		//No transitions declared here: copyImage records its own for both images, and naming them again is a
		// write hazard in the same scope.
		//It picks shader read/write rather than transfer ones precisely
		// because a rotation is replayed as a dispatch.

		CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		c::Test_assert(t, "rotateScope", (c::Bool) scope);

		c::Test_assert(t, "rotateBindHeap", scope.bindDescriptorHeap(pushHeap, e_rr));

		c::Test_assert(t, "copyRotated180", scope.copyImage(
			pattern.handle(), dst.handle(), { .outputRotation = 2 }, e_rr
		));

		c::Test_assert(t, "rotateScopeEnd", scope.end(e_rr));
	}

	c::Test_assert(t, "endRotate", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList)) {

		c::TestShaderPixels pixels{};

		if (gfxtest::pullPixels(t, dev, emptyList, dst.handle(), pixels)) {

			c::U32 matching = 0;

			for(c::U32 y = 0; y < 8; ++y)
				for(c::U32 x = 0; x < 8; ++x)
					matching += pixels.pixels[y * 8 + x] == texels[(7 - y) * 8 + (7 - x)];

			c::Test_assert(t, "rotated180Pixels", matching == 64);
		}
	}
}
