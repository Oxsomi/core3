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

//graphics/test/interface/test_graphics_predication.cpp
//
//Predicated scopes end to end: two dispatches into the same buffer, one behind a zero predicate and one
// behind a nonzero one, prove that a skipped scope leaves its output untouched while its sibling lands,
// and that both still pass their barriers. Recording is the other half of the contract: a predicated
// scope only takes draws and dispatches, so a clear that an ordinary scope accepts must be refused.
//The recording half is pure CPU and keys on the predicate being requested, so it runs on any device;
// only the execution half needs the capability and the bindless write fixture.

#include "test_graphics_shared.hpp"

namespace oxc { namespace c {
	#include "graphics/generic/device_buffer.h"
}}

// -- 33. Predicated scopes -------------------------------------------------------

extern "C" void Test_graphicsPredication(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;
	using namespace oxc::gfxtest;

	c::Error *e_rr = &t->err;

	c::Test_setModule(t, "GraphicsDevice/predication");

	Device dev = Device::share(deviceRef);

	//Slot 0 reads zero, so its scope is skipped; slot 1 reads nonzero, so its scope runs. Written as full
	// U64s, since D3D12 evaluates all 64 bits where Vulkan reads the low word.

	const c::U64 values[2] = { 0, 1 };
	c::Buffer valueData = c::Buffer_createRefConst(values, sizeof(values));

	DeviceBuffer predicate;

	if(!c::Test_assert(t, "createPredicate", dev.createBufferData(
		c::EDeviceBufferUsage_Predicate, c::EGraphicsResourceFlag_None,
		"Predicate values", &valueData, predicate, nullptr, e_rr
	)))
		return;

	//The draw-and-dispatch-only rule, first: the same clear an ordinary scope accepts is refused in a
	// predicated one, on a list that is never submitted. The refusal hides the scope rather than
	// poisoning the list, so the list still ends cleanly and the offender never reaches activeScopes.

	RenderTexture clearTarget;

	if(!c::Test_assert(t, "createClearTarget", dev.createRenderTexture(
		32, 16, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Refusal clear target", clearTarget,
		c::EMSAASamples_Off, nullptr, e_rr
	)))
		return;

	CommandList negList;

	if(!c::Test_assert(t, "createNegList", dev.createCommandList(c::KIBI, 16, 8, negList, true, e_rr)))
		return;

	const c::ImageRange all = (c::ImageRange) { 0 };

	c::Test_assert(t, "negBegin", negList.begin(true, e_rr));

	{
		CommandScope scope = negList.scope({}, 60, {}, e_rr);
		c::Test_assert(t, "plainScope", (c::Bool) scope);
		c::Test_assert(t, "plainClearAllowed", scope.clearImagef(c::F32x4_zero(), all, clearTarget.handle(), e_rr));
		c::Test_assert(t, "plainEnd", scope.end(e_rr));
	}

	{
		CommandScope scope = negList.scope(
			{}, 61, {}, c::ECommandScopeFlags_None, "Refused clear", predicate, 8, e_rr
		);
		c::Test_assert(t, "refusalScope", (c::Bool) scope);
		c::Test_assert(t, "clearRefused", !scope.clearImagef(c::F32x4_zero(), all, clearTarget.handle(), nullptr));
	}

	c::Test_assert(t, "negEnd", negList.end(e_rr));
	c::Test_assert(t, "refusedHidden", negList.data()->activeScopes.length == 1);

	if(!(dev.info().capabilities.features2 & c::EGraphicsFeatures2_Predication)) {
		c::Test_print(t, "Device has no Predication capability, skipping");
		return;
	}

	if(!dev.hasBindlessTable()) {
		c::Test_print(t, "Device has no bindless descriptor table, skipping");
		return;
	}

	OwnedSHFile writeFile(dev.alloc());

	if(!loadFile(t, "//OxC3_gtest/test_shaders/test_write.oiSH", writeFile.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping");
		return;
	}

	DeviceBuffer output;

	if(!c::Test_assert(t, "createOutput", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWriteBindless | c::EGraphicsResourceFlag_CPUBacked),
		"Predication output", 128 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	Pipeline pipelineWrite;
	PipelineLayout writeLayout;

	if(!computePipelinePush(t, dev, writeFile.list, pipelineWrite, writeLayout))
		return;

	CommandList commandList, emptyList;

	if(!c::Test_assert(t, "createList", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	if(!c::Test_assert(t, "createEmptyList", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	c::Test_assert(t, "beginEmptyList", emptyList.begin(true, e_rr));
	c::Test_assert(t, "endEmptyList", emptyList.end(e_rr));

	const c::Transition outputWrite = {
		.resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
	};

	c::U32 pushSkipped[4] = { output.writeHandle(), 0xDEADDEADu, 0, 0 };
	c::U32 pushRan[4] = { output.writeHandle(), 0xC0DE0000u, 0, 0 };

	c::Test_assert(t, "begin", commandList.begin(true, e_rr));

	//The ran scope goes FIRST and the skipped one last: a skip that wrongly executes then clobbers the
	// pattern the assertion checks, so a backend that silently never predicates cannot pass.

	{
		CommandScope scope = commandList.scope(
			{ outputWrite }, 50, {}, c::ECommandScopeFlags_None, "Ran", predicate, 8, e_rr
		);
		c::Test_assert(t, "ranScope", (c::Bool) scope);
		c::Test_assert(t, "ranBind", scope.setComputePipeline(pipelineWrite, e_rr));
		c::Test_assert(t, "ranPush", scope.setPushConstants(pushRan, e_rr));
		c::Test_assert(t, "ranDispatch", scope.dispatch1D(1, e_rr));
		c::Test_assert(t, "ranEnd", scope.end(e_rr));
	}

	{
		CommandScope scope = commandList.scope(
			{ outputWrite }, 51, { { c::ECommandScopeDependencyType_Unconditional, 50 } },
			c::ECommandScopeFlags_None, "Skipped", predicate, 0, e_rr
		);
		c::Test_assert(t, "skippedScope", (c::Bool) scope);
		c::Test_assert(t, "skippedBind", scope.setComputePipeline(pipelineWrite, e_rr));
		c::Test_assert(t, "skippedPush", scope.setPushConstants(pushSkipped, e_rr));
		c::Test_assert(t, "skippedDispatch", scope.dispatch1D(1, e_rr));
		c::Test_assert(t, "skippedEnd", scope.end(e_rr));
	}

	c::Test_assert(t, "end", commandList.end(e_rr));

	if(!submitAndWait(t, dev, commandList))
		return;

	if(gfxtest::pullBuffer(t, dev, emptyList, output)) {

		const c::U32 *v = (const c::U32*) output.data()->cpuData.ptr;

		//The ran scope's pattern must SURVIVE the skipped scope that recorded after it: 0xDEAD here means
		// the zero predicate executed anyway, and anything else means the ran scope never landed.

		c::Test_assert(t, "ranLandedAndSkipHeld", v[0] == 0xC0DE0000u);
	}

}
