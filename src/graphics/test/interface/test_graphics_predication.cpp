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
//Without the capability the contract is refusal (usage bit, then startScope), verified and skipped;
// with it, the whole test runs bindful, so Predication itself is the only capability it needs.

#include "test_graphics_shared.hpp"

namespace oxc { namespace c {
	#include "graphics/generic/device_buffer.h"
}}

namespace {
	struct TestPredicationPushData {
		oxc::c::U32 scale, bias, xorMask, offset;
	};
}

// -- 33. Predicated scopes -------------------------------------------------------

extern "C" void Test_graphicsPredication(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;
	using namespace oxc::gfxtest;

	c::Error *e_rr = &t->err;

	c::Test_setModule(t, "GraphicsDevice/predication");

	Device dev = Device::share(deviceRef);

	//Without the capability the CONTRACT is refusal, not degradation: the predicate usage bit at buffer
	// creation is the first gate, so that is what a capability-less device has to reject.

	if(!(dev.info().capabilities.features2 & c::EGraphicsFeatures2_Predication)) {

		const c::U64 zero = 0;
		c::Buffer zeroData = c::Buffer_createRefConst(&zero, sizeof(zero));

		DeviceBuffer refused;

		c::Test_assert(t, "predicateUsageRefused", !dev.createBufferData(
			c::EDeviceBufferUsage_Predicate, c::EGraphicsResourceFlag_None,
			"Refused predicate", &zeroData, refused, nullptr, nullptr
		));

		c::Test_print(t, "Device has no Predication capability, refusal verified, skipping the rest");
		return;
	}

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
			{ .id = 61, .name = "Refused clear", .predicate = &predicate, .predicateOffset = 8 }, e_rr
		);
		c::Test_assert(t, "refusalScope", (c::Bool) scope);
		c::Test_assert(t, "clearRefused", !scope.clearImagef(c::F32x4_zero(), all, clearTarget.handle(), nullptr));
	}

	c::Test_assert(t, "negEnd", negList.end(e_rr));
	c::Test_assert(t, "refusedHidden", negList.data()->activeScopes.length == 1);

	OwnedSHFile writeFile(dev.alloc());

	if(!loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_pushconst.oiSH", writeFile.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, writeFile.list, "main");

	if(entryId == c::U32_MAX)
		return;

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());
	c::DescriptorBinding pushConstants{};

	if(!c::Test_assert(t, "detectLayout", dev.detectLayout(
		writeFile.list, entryId, layoutInfo.list, nullptr, &pushConstants, {}, nullptr,
		c::EDescriptorLayoutFlags_None, c::EDetectDescriptorLayoutFlags_AssumePushConstants, e_rr
	)))
		return;

	DescriptorLayout layout;

	if(!c::Test_assert(t, "layoutCreate", dev.createDescriptorLayout(
		layoutInfo.list, "Predication layout", layout, e_rr
	)))
		return;

	c::DescriptorHeapInfo heapInfo{};
	heapInfo.maxBuffersRW = 1;
	heapInfo.maxDescriptorTables = 1;

	DescriptorHeap heap;

	if(!c::Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Predication heap", heap, e_rr)))
		return;

	DescriptorTable table;

	if(!c::Test_assert(t, "tableCreate", heap.createTable(
		layout, "Predication table", table, (c::EDescriptorTableFlags) 0, e_rr
	)))
		return;

	DeviceBuffer output;

	if(!c::Test_assert(t, "createOutput", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Predication output", 64 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	//The table holds no reference of its own, so the descriptor goes back before the buffer does.

	struct TableGuard {
		DescriptorTable &table;
		~TableGuard() { (void) table.unset(0, 0, 1, nullptr); }
	} tableGuard{ table };

	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, nullptr, 0);

	if(!c::Test_assert(t, "setOutput", table.setByName("output", outputDesc, 0, false, e_rr)))
		return;

	c::PipelineLayoutInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.bindings = layout.handle();
	pipelineLayoutInfo.pushConstants = pushConstants;

	PipelineLayout writeLayout;

	if(!c::Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Predication pipeline layout", writeLayout, e_rr
	)))
		return;

	Pipeline pipelineWrite;

	if(!c::Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		writeFile.list, "main", "Predication pipeline", pipelineWrite, {}, &writeLayout, e_rr
	)))
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

	//Scale 0 makes all 64 threads write bias itself, so the whole buffer says which dispatch landed last.

	const TestPredicationPushData pushSkipped = { 0, 0xDEADDEADu, 0, 0 };
	const TestPredicationPushData pushRan = { 0, 0xC0DE0000u, 0, 0 };

	c::Test_assert(t, "begin", commandList.begin(true, e_rr));

	//The ran scope goes FIRST and the skipped one last: a skip that wrongly executes then clobbers the
	// pattern the assertion checks, so a backend that silently never predicates cannot pass.

	{
		CommandScope scope = commandList.scope(
			{ .transitions = { outputWrite }, .id = 50, .name = "Ran", .predicate = &predicate, .predicateOffset = 8 },
			e_rr
		);
		c::Test_assert(t, "ranScope", (c::Bool) scope);
		c::Test_assert(t, "ranHeap", scope.bindDescriptorHeap(heap, e_rr));
		c::Test_assert(t, "ranTable", scope.bindDescriptorTable(table, e_rr));
		c::Test_assert(t, "ranBind", scope.setComputePipeline(pipelineWrite, e_rr));
		c::Test_assert(t, "ranPush", scope.setPushConstants(pushRan, e_rr));
		c::Test_assert(t, "ranDispatch", scope.dispatch1D(1, e_rr));
		c::Test_assert(t, "ranEnd", scope.end(e_rr));
	}

	{
		CommandScope scope = commandList.scope(
			{
				.transitions = { outputWrite }, .deps = { { c::ECommandScopeDependencyType_Unconditional, 50 } },
				.id = 51, .name = "Skipped", .predicate = &predicate
			},
			e_rr
		);
		c::Test_assert(t, "skippedScope", (c::Bool) scope);
		c::Test_assert(t, "skippedHeap", scope.bindDescriptorHeap(heap, e_rr));
		c::Test_assert(t, "skippedTable", scope.bindDescriptorTable(table, e_rr));
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
