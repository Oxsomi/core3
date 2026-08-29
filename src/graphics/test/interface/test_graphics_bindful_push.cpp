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

//graphics/test/interface/test_graphics_bindful_push.cpp
//
//Push descriptors, written against the C++ layer (graphics/graphics.hpp) rather than the C API.
//Same coverage as the C module it replaces, minus the teardown: every handle here releases itself, so there
//is no clean label, no goto chain and no ordered list of RefPtr_dec calls to keep in step with the locals.
//What is left is the test.

#include "graphics/graphics.hpp"

namespace oxc { namespace c {
	#include "test_graphics_shared.h"
	#include "platforms/platform.h"
}}

//SHFile is a plain C struct with no wrapper, so it gets a local guard rather than a manual free on
//every exit path.

namespace {

	//graphics.hpp already has the guard these were: OwnedList frees its list on every exit path, error
	//returns included.

	using OwnedSHFile = oxc::gfx::OwnedList<oxc::c::SHFile, oxc::c::SHFile_free>;
	using OwnedLayoutInfo = oxc::gfx::OwnedList<oxc::c::DescriptorLayoutInfo, oxc::c::DescriptorLayoutInfo_free>;
}

//Root descriptors rather than table entries: both resources ride in the command stream, so this module
//binds no descriptor heap and no descriptor table at all and the pipeline layout carries only the push
//descriptor layout. That also exercises the backends' "push descriptors only" root signature branch.
//Two dispatches against two different constant buffers with nothing rebound between them prove the backends
//re-emit at the work op rather than once at the write.

extern "C" void Test_graphicsBindfulPushDescriptors(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Error *e_rr = &t->err;
	const c::Allocator *alloc = c::Platform_instance->alloc;

	Test_setModule(t, "Bindful/pushDescriptors");

	//The harness owns this ref, so it is borrowed rather than adopted.

	Device dev = Device::share(deviceRef);

	OwnedSHFile shader(alloc);

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_pushdesc.oiSH", &shader.list)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping push descriptor tests");
		return;
	}

	const c::U32 entryId = TestShaders_entry(t, deviceRef, &shader.list, "main");

	if(entryId == c::U32_MAX)
		return;

	//Naming both registers is what splits them out of the ordinary bindings.

	OwnedLayoutInfo layoutInfo(alloc);
	OwnedLayoutInfo pushInfo(alloc);

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		shader.list, entryId, layoutInfo.list, nullptr, nullptr, { "params", "output" }, &pushInfo.list,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	//Everything the shader declares is a push descriptor, so nothing is left for an ordinary layout.

	Test_assert(t, "noOrdinaryBindings", !layoutInfo.list.bindings.length);

	if(!Test_assert(t, "detectedPushDescriptors", pushInfo.list.bindings.length == 2))
		return;

	DescriptorLayout pushLayout;

	//Refused outright on a device without VK_KHR_push_descriptor, which is the documented gap rather than a
	// failure: asserting first would record a red before the skip ever printed.

	if(!dev.createDescriptorLayout(pushInfo.list, "Push descriptor layout", pushLayout, nullptr)) {
		Test_print(t, "Push descriptor layouts unsupported on this device, skipping");
		return;
	}

	Test_assert(t, "pushLayoutCreate", pushLayout.valid());

	//A CBV's length has to be 256-byte aligned and match what reflection reported, exactly like module 47.

	c::U32 firstData[64] = { 0 };
	firstData[0] = 3; firstData[1] = 7; firstData[2] = 0; firstData[3] = 0;

	c::U32 secondData[64] = { 0 };
	secondData[0] = 5; secondData[1] = 1; secondData[2] = 0xFFu; secondData[3] = 64;

	c::Buffer firstRef = c::Buffer_createRefConst(firstData, sizeof(firstData));
	c::Buffer secondRef = c::Buffer_createRefConst(secondData, sizeof(secondData));

	DeviceBuffer first, second, output;

	if(!Test_assert(t, "firstCreate", dev.createBufferData(
		c::EDeviceBufferUsage_Uniform, c::EGraphicsResourceFlag_None, "Push descriptor params A", &firstRef, first, nullptr,
		e_rr
	)))
		return;

	if(!Test_assert(t, "secondCreate", dev.createBufferData(
		c::EDeviceBufferUsage_Uniform, c::EGraphicsResourceFlag_None, "Push descriptor params B", &secondRef, second, nullptr,
		e_rr
	)))
		return;

	if(!Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag)(c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Push descriptor output", 128 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	c::PipelineLayoutInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.pushDescriptors = pushLayout.handle();

	PipelineLayout pipelineLayout;

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Push descriptor pipeline layout", pipelineLayout, e_rr
	)))
		return;

	Pipeline pipeline;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		shader.list, "main", "Push descriptor pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	CommandList commandList, emptyList;

	if(!Test_assert(t, "listCreate", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, nullptr, 0);
	const c::Descriptor firstDesc = c::Descriptor_buffer(first.handle(), 0, 0, nullptr, 0);
	const c::Descriptor secondDesc = c::Descriptor_buffer(second.handle(), 0, 0, nullptr, 0);

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	//Nothing is declared at scope start on purpose: a push descriptor's own transition is what has to put
	// these three in the right state, so an empty list is what proves the work op records them.

	//A dispatch with nothing written is refused: the root descriptors would point at whatever the last
	// pipeline left in them, which is the read this validation exists to prevent.
	//A partial write is refused for the same reason, so the negatives live in their own scope (a refused
	// work op hides the whole scope).

	{
		CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "scopeNeg", (c::Bool) scope);
		Test_assert(t, "bindPipelineNeg", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatchWithoutDescriptors", !scope.dispatch1D(1, nullptr));
		Test_assert(t, "setPartial", scope.setPushDescriptors({ firstDesc }, e_rr));
		Test_assert(t, "dispatchWrongCount", !scope.dispatch1D(1, nullptr));
		Test_assert(t, "scopeNegEnd", scope.end(e_rr));
	}

	//Two dispatches, two different constant buffers, nothing rebound between them: the second set only
	// lands if the backend re-emits at the work op.

	{
		CommandScope scope = commandList.scope({}, 2, {}, e_rr);
		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "setFirst", scope.setPushDescriptors({ firstDesc, outputDesc }, e_rr));
		Test_assert(t, "dispatchFirst", scope.dispatch1D(1, e_rr));
		Test_assert(t, "setSecond", scope.setPushDescriptors({ secondDesc, outputDesc }, e_rr));
		Test_assert(t, "dispatchSecond", scope.dispatch1D(1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (TestShaders_submitAndWait(t, deviceRef, commandList.handle()))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList.handle(), output.handle())) {

			//Disjoint halves, so BOTH pushes have to have landed rather than only the last one.

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			c::Bool firstMatch = true, secondMatch = true;

			for(c::U32 i = 0; i < 64; ++i) {
				firstMatch &= values[i] == ((i * firstData[0] + firstData[1]) ^ firstData[2]);
				secondMatch &= values[i + 64] == ((i * secondData[0] + secondData[1]) ^ secondData[2]);
			}

			Test_assert(t, "pushDescriptorFirstResults", firstMatch);
			Test_assert(t, "pushDescriptorSecondResults", secondMatch);
		}
}
