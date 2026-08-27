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

//graphics/test/interface/test_graphics_timestamps.cpp
//
//GPU timestamps end to end:
// the command list flags time and debug label a named scope while a manual region wraps the same dispatch,
// the frame is submitted, then several empty frames follow so the framesInFlight-latent result lands,
// and getTimings proves the scope and region came back keyed by their id and name with a real span.

#include "test_graphics_shared.hpp"

namespace oxc { namespace c {
	#include "graphics/generic/device_buffer.h"
}}

// -- 32. GPU timestamps ----------------------------------------------------------

extern "C" void Test_graphicsTimestamps(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;
	using namespace oxc::gfxtest;

	c::Error *e_rr = &t->err;

	c::Test_setModule(t, "GraphicsDevice/timestamps");

	Device dev = Device::share(deviceRef);

	if(!(dev.info().capabilities.features2 & c::EGraphicsFeatures2_Timestamps)) {
		c::Test_print(t, "Device has no Timestamps capability, skipping");
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
		"Timestamp output", 128 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	Pipeline pipelineWrite;
	PipelineLayout writeLayout;

	if(!computePipelinePush(t, dev, writeFile.list, pipelineWrite, writeLayout))
		return;

	CommandList commandList, emptyList;

	if(!c::Test_assert(t, "createList", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	//The empty frames that follow the timed one carry no work, so nothing re-runs while the result is waited out.

	if(!c::Test_assert(t, "createEmptyList", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	c::Test_assert(t, "beginEmptyList", emptyList.begin(true, e_rr));
	c::Test_assert(t, "endEmptyList", emptyList.end(e_rr));

	const c::Transition outputWrite = {
		.resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
	};

	c::U32 pushData[4] = { output.writeHandle(), 0xC0DE0000u, 0, 0 };

	//Scope 41 is timed by the list flag; region 7 ("dispatch") is a manual span around the same dispatch, so the
	// two nest and the region's span can never exceed the scope's.

	c::Test_assert(t, "begin", commandList.begin(true, e_rr));
	c::Test_assert(t, "enableScopeTiming", commandList.setScopeTiming(true, e_rr));
	c::Test_assert(t, "enableScopeDebug", commandList.setScopeDebug(true, e_rr));

	{
		CommandScope scope = commandList.scope({ outputWrite }, 41, {}, c::ECommandScopeFlags_None, "MainScope", e_rr);
		c::Test_assert(t, "scope", (c::Bool) scope);
		c::Test_assert(t, "bindPipeline", scope.setComputePipeline(pipelineWrite, e_rr));
		c::Test_assert(t, "push", scope.setPushConstants(pushData, e_rr));

		{
			CommandTimingRegion region = scope.timingRegion(7, "dispatch", e_rr);
			c::Test_assert(t, "region", (c::Bool) region);
			c::Test_assert(t, "dispatch", scope.dispatch1D(1, e_rr));
			c::Test_assert(t, "regionEnd", region.end(e_rr));
		}

		c::Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	//A scope that opts out with DisableTimestamp spends no query slots and never appears in getTimings, even with
	// list timing on.

	{
		CommandScope scope = commandList.scope(
			{ outputWrite }, 42, {}, c::ECommandScopeFlags_DisableTimestamp, nullptr, e_rr
		);
		c::Test_assert(t, "optOutScope", (c::Bool) scope);
		c::Test_assert(t, "optOutBind", scope.setComputePipeline(pipelineWrite, e_rr));
		c::Test_assert(t, "optOutPush", scope.setPushConstants(pushData, e_rr));
		c::Test_assert(t, "optOutDispatch", scope.dispatch1D(1, e_rr));
		c::Test_assert(t, "optOutScopeEnd", scope.end(e_rr));
	}

	c::Test_assert(t, "end", commandList.end(e_rr));

	//The timed frame, then several empty frames: a timestamp is only readable once its frame's fence has signalled,
	// which is framesInFlight submits later, so the read below sees the timed frame only after the pool it wrote is
	// reused. Five empty frames clear the ring whatever the buffering mode.

	if(!submitAndWait(t, dev, commandList))
		return;

	for(c::U32 i = 0; i < 5; ++i)
		if(!submitAndWait(t, dev, emptyList))
			return;

	c::ListGraphicsTiming timings = (c::ListGraphicsTiming) { 0 };

	if(!c::Test_assert(t, "getTimings", dev.timings(timings, e_rr)))
		return;

	c::Bool sawScope = false, sawRegion = false, sawOptOut = false;
	c::U64 scopeNs = 0, regionNs = 0;

	for(c::U64 i = 0; i < timings.length; ++i) {

		const c::GraphicsTiming g = timings.ptr[i];

		if(g.id == 41) {
			sawScope = true;
			scopeNs = g.gpuNs;
			c::Test_assert(t, "scopeName", c::CharString_equalsCStringSensitive(&g.name, "MainScope"));
		}

		if(g.id == 7) {
			sawRegion = true;
			regionNs = g.gpuNs;
			c::Test_assert(t, "regionName", c::CharString_equalsCStringSensitive(&g.name, "dispatch"));
		}

		if(g.id == 42)
			sawOptOut = true;
	}

	c::Test_assert(t, "timedScope", sawScope);
	c::Test_assert(t, "timedRegion", sawRegion);
	c::Test_assert(t, "scopeMeasured", scopeNs > 0);
	c::Test_assert(t, "regionWithinScope", regionNs <= scopeNs);
	c::Test_assert(t, "optOutNotTimed", !sawOptOut);

	c::ListGraphicsTiming_freeUnderlying(&timings, dev.alloc());

	//Growth: a scope with more inserts than the initial pool holds forces the pool to grow,
	// and getTimings still returns every one.

	{
		CommandList growList;

		if(!c::Test_assert(t, "createGrowList", dev.createCommandList(128 * c::KIBI, 4 * c::KIBI, 16, growList, true, e_rr)))
			return;

		const c::U32 growInserts = GRAPHICS_TIMESTAMP_QUERIES + 4;          //Just past the initial capacity

		c::Test_assert(t, "growBegin", growList.begin(true, e_rr));
		c::Test_assert(t, "growEnableTiming", growList.setScopeTiming(true, e_rr));

		{
			CommandScope scope = growList.scope({ outputWrite }, 100, {}, e_rr);
			c::Test_assert(t, "growScope", (c::Bool) scope);
			c::Test_assert(t, "growBind", scope.setComputePipeline(pipelineWrite, e_rr));
			c::Test_assert(t, "growPush", scope.setPushConstants(pushData, e_rr));
			c::Test_assert(t, "growDispatch", scope.dispatch1D(1, e_rr));

			c::Bool insertsOk = true;

			for(c::U32 i = 0; i < growInserts; ++i)
				insertsOk &= scope.insertTiming(1000 + i, nullptr, e_rr);

			c::Test_assert(t, "growInserts", insertsOk);
			c::Test_assert(t, "growScopeEnd", scope.end(e_rr));
		}

		c::Test_assert(t, "growEnd", growList.end(e_rr));

		if(!submitAndWait(t, dev, growList))
			return;

		for(c::U32 i = 0; i < 5; ++i)
			if(!submitAndWait(t, dev, emptyList))
				return;

		c::ListGraphicsTiming grown = (c::ListGraphicsTiming) { 0 };

		if(c::Test_assert(t, "growGetTimings", dev.timings(grown, e_rr)))
			c::Test_assert(t, "growCount", grown.length >= growInserts);          //Scope plus every insert survived

		c::ListGraphicsTiming_freeUnderlying(&grown, dev.alloc());
	}
}
