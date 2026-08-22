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

namespace oxc { namespace c {
	#include "types/base/string_base.h"
	#include "types/container/buffer.h"
	#include "types/container/memory_stream.h"
	#include "types/container/texture_format.h"
	#include "types/test/test.h"
	#include "formats/oiSH/sh_file.h"
	#include "formats/oiSH/sh_registers.h"
	#include "platforms/file.h"
	#include "platforms/logx.h"
	#include "platforms/platform.h"
	#include "graphics/generic/bindless_descriptor.h"
	#include "graphics/generic/blas.h"
	#include "graphics/generic/command_list.h"
	#include "graphics/generic/commands.h"
	#include "graphics/generic/depth_stencil.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_buffer.h"
	#include "graphics/generic/device_info.h"
	#include "graphics/generic/instance.h"
	#include "graphics/generic/opacity_micromap.h"
	#include "graphics/generic/pipeline.h"
	#include "graphics/generic/render_texture.h"
	#include "graphics/generic/texture.h"
	#include "graphics/generic/tlas.h"
	#include "test_graphics_shared.h"
} }

//Same namespace the C headers landed in, so the definitions here match the declarations in
//test_graphics_shared.h and the macros in those headers still expand to names that resolve.

namespace oxc { namespace c {

// -- 31. Compute execution -------------------------------------------------------

//A 64 thread dispatch writes base + thread id per slot, so the readback proves every thread really ran.
//The same shader then runs through dispatchIndirect twice: once from CPU written arguments at an aligned
// offset and once from arguments another dispatch wrote on the GPU, which is the full indirect pipeline.

void Test_graphicsShaderCompute(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Shaders/compute");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!device->defaultDescriptorTable) {
		Test_print(t, "Device has no bindless descriptor table, skipping compute execution tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	SHFile writeFile {};
	SHFile argsFile {};

	if (
		!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_write.oiSH", &writeFile) ||
		!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_write_args.oiSH", &argsFile)
	) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping compute execution tests");
		SHFile_free(&writeFile, alloc);
		return;
	}

	DeviceBufferRef *output = NULL;
	DeviceBufferRef *cpuArgs = NULL;
	DeviceBufferRef *gpuArgs = NULL;
	PipelineRef *pipelineWrite = NULL;
	PipelineRef *pipelineArgs = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;
	PipelineLayoutRef *writeLayout = NULL;
	PipelineLayoutRef *argsLayout = NULL;

	//128 slots, so the GPU written two group dispatch at the end has room for both groups

	CharString name = CharString_createRefCStrConst("Shader compute output");

	Test_assert(t, "createOutput", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		(EGraphicsResourceFlag) (EGraphicsResourceFlag_ShaderWriteBindless | EGraphicsResourceFlag_CPUBacked),
		NULL, &name, 128 * sizeof(U32), &output, &t->err
	));

	if(!output || !TestShaders_computePipelinePush(t, deviceRef, &writeFile, &pipelineWrite, &writeLayout))
		goto clean;

	if(!Test_assert(t, "createList", GraphicsDeviceRef_createCommandList(
		deviceRef, 4 * KIBI, 64, 16, true, &commandList, &t->err
	)))
		goto clean;

	//An empty list completes pulls without re-running work, since replaying a dispatch would write the
	// output buffer a second time

	if(!Test_assert(t, "createEmptyList", GraphicsDeviceRef_createCommandList(
		deviceRef, KIBI, 16, 8, true, &emptyList, &t->err
	)))
		goto clean;

	Test_assert(t, "beginEmptyList", CommandListRef_begin(emptyList, true, U64_MAX, &t->err));
	Test_assert(t, "endEmptyList", CommandListRef_end(emptyList, &t->err));

	//Scoped so the goto above jumps around these rather than into them.
	{
	const Transition outputWrite = {
		.resource = output, .stage = EPipelineStage_Compute, .isWrite = true
	};

	ListTransition outputTransition {};
	ListTransition_createRefConst(&outputWrite, 1, &outputTransition, NULL);

	DeviceBuffer *outputPtr = DeviceBufferRef_ptr(output);
	const U32 *values = (const U32*) outputPtr->cpuData.ptr;

	//Direct dispatch

	//Pushed inside each recording, since the bytes are captured when setPushConstants is recorded rather
	//than when the list is submitted.

	U32 pushData[4] = { outputPtr->writeHandle, 0xC0DE0000u, 0, 0 };
	const Buffer pushRef = Buffer_createRefConst(pushData, sizeof(pushData));

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "scope", CommandListRef_startScope(commandList, &outputTransition, 1, NULL, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipelineWrite, &t->err));
		Test_assert(t, "push", CommandListRef_setPushConstants(commandList, pushRef, &t->err));
	Test_assert(t, "dispatch", CommandListRef_dispatch1D(commandList, 1, &t->err));
	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if(!TestShaders_submitAndWait(t, deviceRef, commandList))
		goto clean;

	if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

		U32 matching = 0;

		for(U32 i = 0; i < 64; ++i)
			matching += values[i] == 0xC0DE0000u + i;

		Test_assert(t, "directDispatchValues", matching == 64);
	}

	//Indirect dispatch from CPU written arguments; the nonzero offset checks aligned addressing into the buffer

	const U32 cpuArgsData[8] = { 0, 0, 0, 0, 1, 1, 1, 0 };
	Buffer cpuArgsRef = Buffer_createRefConst(cpuArgsData, sizeof(cpuArgsData));

	name = CharString_createRefCStrConst("Shader compute indirect args");

	Test_assert(t, "createCpuArgs", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_Indirect, EGraphicsResourceFlag_None, NULL,
		&name, &cpuArgsRef, &cpuArgs, &t->err
	));

	if (cpuArgs) {

		pushData[1] = 0xD15C0000u;

		Test_assert(t, "beginIndirect", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
		Test_assert(t, "scopeIndirect", CommandListRef_startScope(commandList, &outputTransition, 1, NULL, &t->err));
		Test_assert(t, "bindIndirect", CommandListRef_setComputePipeline(commandList, pipelineWrite, &t->err));
		Test_assert(t, "push", CommandListRef_setPushConstants(commandList, pushRef, &t->err));
		Test_assert(t, "dispatchIndirect", CommandListRef_dispatchIndirect(commandList, cpuArgs, 16, &t->err));
		Test_assert(t, "scopeIndirectEnd", CommandListRef_endScope(commandList, &t->err));
		Test_assert(t, "endIndirect", CommandListRef_end(commandList, &t->err));

		if(
			TestShaders_submitAndWait(t, deviceRef, commandList) &&
			TestShaders_pullBuffer(t, deviceRef, emptyList, output)
		) {

			U32 matching = 0;

			for(U32 i = 0; i < 64; ++i)
				matching += values[i] == 0xD15C0000u + i;

			Test_assert(t, "cpuIndirectValues", matching == 64);
		}
	}

	//Indirect dispatch from GPU written arguments: one dispatch writes { 2, 1, 1 }, the next consumes it,
	// so 128 threads have to land and the argument buffer is proven writable and consumable in one submit

	name = CharString_createRefCStrConst("Shader compute GPU args");

	Test_assert(t, "createGpuArgs", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_Indirect,
		EGraphicsResourceFlag_ShaderWriteBindless,
		NULL, &name, 32, &gpuArgs, &t->err
	));

	if (gpuArgs && TestShaders_computePipelinePush(t, deviceRef, &argsFile, &pipelineArgs, &argsLayout)) {

		const Transition argsWrite = {
			.resource = gpuArgs, .stage = EPipelineStage_Compute, .isWrite = true
		};

		ListTransition argsTransition {};
		ListTransition_createRefConst(&argsWrite, 1, &argsTransition, NULL);

		pushData[1] = 0xA53F0000u;
		pushData[2] = DeviceBufferRef_ptr(gpuArgs)->writeHandle;

		Test_assert(t, "beginGpu", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		Test_assert(t, "scopeArgs", CommandListRef_startScope(commandList, &argsTransition, 1, NULL, &t->err));
		Test_assert(t, "bindArgs", CommandListRef_setComputePipeline(commandList, pipelineArgs, &t->err));
		Test_assert(t, "push", CommandListRef_setPushConstants(commandList, pushRef, &t->err));
		Test_assert(t, "dispatchArgs", CommandListRef_dispatch1D(commandList, 1, &t->err));
		Test_assert(t, "scopeArgsEnd", CommandListRef_endScope(commandList, &t->err));

		Test_assert(t, "scopeGpu", CommandListRef_startScope(commandList, &outputTransition, 2, NULL, &t->err));
		Test_assert(t, "bindGpu", CommandListRef_setComputePipeline(commandList, pipelineWrite, &t->err));
		Test_assert(t, "push", CommandListRef_setPushConstants(commandList, pushRef, &t->err));
		Test_assert(t, "dispatchGpu", CommandListRef_dispatchIndirect(commandList, gpuArgs, 16, &t->err));
		Test_assert(t, "scopeGpuEnd", CommandListRef_endScope(commandList, &t->err));

		Test_assert(t, "endGpu", CommandListRef_end(commandList, &t->err));

		if(
			TestShaders_submitAndWait(t, deviceRef, commandList) &&
			TestShaders_pullBuffer(t, deviceRef, emptyList, output)
		) {

			U32 matching = 0;

			for(U32 i = 0; i < 128; ++i)
				matching += values[i] == 0xA53F0000u + i;

			Test_assert(t, "gpuIndirectValues", matching == 128);
		}
	}

	}

clean:
	RefPtr_dec(&writeLayout);
	RefPtr_dec(&argsLayout);

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipelineArgs);
	RefPtr_dec(&pipelineWrite);
	RefPtr_dec(&gpuArgs);
	RefPtr_dec(&cpuArgs);
	RefPtr_dec(&output);

	SHFile_free(&argsFile, alloc);
	SHFile_free(&writeFile, alloc);
}
} }
