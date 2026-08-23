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

//graphics/test/interface/test_graphics_shaders_compute.cpp
//
//Compute execution: the smallest end to end path, a dispatch whose result is pulled back and checked.
//Split out of test_graphics_shaders.c, which had grown past 2300 lines.
//
//Written against the C++ layer (graphics/graphics.hpp): every handle releases itself, so there is no clean
//label, no goto chain and no ordered list of RefPtr_dec calls to keep in step with the locals.

#include "test_graphics_shared.hpp"

namespace oxc { namespace c {
	#include "graphics/generic/device_buffer.h"
}}

// -- 31. Compute execution -------------------------------------------------------

//A 64 thread dispatch writes base + thread id per slot, so the readback proves every thread really ran.
//The same shader then runs through dispatchIndirect twice: once from CPU written arguments at an aligned
// offset and once from arguments another dispatch wrote on the GPU, which is the full indirect pipeline.

extern "C" void Test_graphicsShaderCompute(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;
	using namespace oxc::gfxtest;

	c::Error *e_rr = &t->err;

	c::Test_setModule(t, "Shaders/compute");

	//The harness owns this ref, so it is borrowed rather than adopted.

	Device dev = Device::share(deviceRef);

	if (!dev.hasBindlessTable()) {
		c::Test_print(t, "Device has no bindless descriptor table, skipping compute execution tests");
		return;
	}

	OwnedSHFile writeFile(dev.alloc());
	OwnedSHFile argsFile(dev.alloc());

	if (
		!loadFile(t, "//OxC3_gtest/test_shaders/test_write.oiSH", writeFile.list) ||
		!loadFile(t, "//OxC3_gtest/test_shaders/test_write_args.oiSH", argsFile.list)
	) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping compute execution tests");
		return;
	}

	//128 slots, so the GPU written two group dispatch at the end has room for both groups

	DeviceBuffer output;

	if(!c::Test_assert(t, "createOutput", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWriteBindless | c::EGraphicsResourceFlag_CPUBacked),
		"Shader compute output", 128 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	Pipeline pipelineWrite;
	PipelineLayout writeLayout;

	if(!computePipelinePush(t, dev, writeFile.list, pipelineWrite, writeLayout))
		return;

	CommandList commandList, emptyList;

	if(!c::Test_assert(t, "createList", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	//An empty list completes pulls without re-running work, since replaying a dispatch would write the
	// output buffer a second time

	if(!c::Test_assert(t, "createEmptyList", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	c::Test_assert(t, "beginEmptyList", emptyList.begin(true, e_rr));
	c::Test_assert(t, "endEmptyList", emptyList.end(e_rr));

	const c::Transition outputWrite = {
		.resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
	};

	const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

	//Direct dispatch

	//Pushed inside each recording, since the bytes are captured when setPushConstants is recorded rather
	//than when the list is submitted.

	c::U32 pushData[4] = { output.writeHandle(), 0xC0DE0000u, 0, 0 };

	c::Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		CommandScope scope = commandList.scope({ outputWrite }, 1, {}, e_rr);
		c::Test_assert(t, "scope", (c::Bool) scope);
		c::Test_assert(t, "bindPipeline", scope.setComputePipeline(pipelineWrite, e_rr));
		c::Test_assert(t, "push", scope.setPushConstants(pushData, e_rr));
		c::Test_assert(t, "dispatch", scope.dispatch1D(1, e_rr));
		c::Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	c::Test_assert(t, "end", commandList.end(e_rr));

	if(!submitAndWait(t, dev, commandList))
		return;

	if (pullBuffer(t, dev, emptyList, output)) {

		c::U32 matching = 0;

		for(c::U32 i = 0; i < 64; ++i)
			matching += values[i] == 0xC0DE0000u + i;

		c::Test_assert(t, "directDispatchValues", matching == 64);
	}

	//Indirect dispatch from CPU written arguments; the nonzero offset checks aligned addressing into the buffer

	const c::U32 cpuArgsData[8] = { 0, 0, 0, 0, 1, 1, 1, 0 };
	c::Buffer cpuArgsRef = c::Buffer_createRefConst(cpuArgsData, sizeof(cpuArgsData));

	DeviceBuffer cpuArgs;

	c::Test_assert(t, "createCpuArgs", dev.createBufferData(
		c::EDeviceBufferUsage_Indirect, c::EGraphicsResourceFlag_None,
		"Shader compute indirect args", &cpuArgsRef, cpuArgs, nullptr, e_rr
	));

	if (cpuArgs) {

		pushData[1] = 0xD15C0000u;

		c::Test_assert(t, "beginIndirect", commandList.begin(true, e_rr));

		{
			CommandScope scope = commandList.scope({ outputWrite }, 1, {}, e_rr);
			c::Test_assert(t, "scopeIndirect", (c::Bool) scope);
			c::Test_assert(t, "bindIndirect", scope.setComputePipeline(pipelineWrite, e_rr));
			c::Test_assert(t, "push", scope.setPushConstants(pushData, e_rr));
			c::Test_assert(t, "dispatchIndirect", scope.dispatchIndirect(cpuArgs, 16, e_rr));
			c::Test_assert(t, "scopeIndirectEnd", scope.end(e_rr));
		}

		c::Test_assert(t, "endIndirect", commandList.end(e_rr));

		if(submitAndWait(t, dev, commandList) && pullBuffer(t, dev, emptyList, output)) {

			c::U32 matching = 0;

			for(c::U32 i = 0; i < 64; ++i)
				matching += values[i] == 0xD15C0000u + i;

			c::Test_assert(t, "cpuIndirectValues", matching == 64);
		}
	}

	//Indirect dispatch from GPU written arguments: one dispatch writes { 2, 1, 1 }, the next consumes it,
	// so 128 threads have to land and the argument buffer is proven writable and consumable in one submit

	DeviceBuffer gpuArgs;

	c::Test_assert(t, "createGpuArgs", dev.createBuffer(
		c::EDeviceBufferUsage_Indirect, c::EGraphicsResourceFlag_ShaderWriteBindless,
		"Shader compute GPU args", 32, gpuArgs, nullptr, e_rr
	));

	Pipeline pipelineArgs;
	PipelineLayout argsLayout;

	if (gpuArgs && computePipelinePush(t, dev, argsFile.list, pipelineArgs, argsLayout)) {

		const c::Transition argsWrite = {
			.resource = gpuArgs.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
		};

		pushData[1] = 0xA53F0000u;
		pushData[2] = gpuArgs.writeHandle();

		c::Test_assert(t, "beginGpu", commandList.begin(true, e_rr));

		{
			CommandScope scope = commandList.scope({ argsWrite }, 1, {}, e_rr);
			c::Test_assert(t, "scopeArgs", (c::Bool) scope);
			c::Test_assert(t, "bindArgs", scope.setComputePipeline(pipelineArgs, e_rr));
			c::Test_assert(t, "push", scope.setPushConstants(pushData, e_rr));
			c::Test_assert(t, "dispatchArgs", scope.dispatch1D(1, e_rr));
			c::Test_assert(t, "scopeArgsEnd", scope.end(e_rr));
		}

		{
			CommandScope scope = commandList.scope({ outputWrite }, 2, {}, e_rr);
			c::Test_assert(t, "scopeGpu", (c::Bool) scope);
			c::Test_assert(t, "bindGpu", scope.setComputePipeline(pipelineWrite, e_rr));
			c::Test_assert(t, "push", scope.setPushConstants(pushData, e_rr));
			c::Test_assert(t, "dispatchGpu", scope.dispatchIndirect(gpuArgs, 16, e_rr));
			c::Test_assert(t, "scopeGpuEnd", scope.end(e_rr));
		}

		c::Test_assert(t, "endGpu", commandList.end(e_rr));

		if(submitAndWait(t, dev, commandList) && pullBuffer(t, dev, emptyList, output)) {

			c::U32 matching = 0;

			for(c::U32 i = 0; i < 128; ++i)
				matching += values[i] == 0xA53F0000u + i;

			c::Test_assert(t, "gpuIndirectValues", matching == 128);
		}
	}
}
