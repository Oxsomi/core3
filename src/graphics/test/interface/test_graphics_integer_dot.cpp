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

//graphics/test/interface/test_graphics_integer_dot.cpp
//
//GraphicsDevice/integerDot: the packed 4 x 8 bit integer dot product (EGraphicsFeatures2_IntegerDot4x8).
//A device that claims it runs a shader declaring ESHExtension2_IntegerDot4x8 and gets the sums the host computes, signs
// and all; a device that does not refuses the binary by its missing feature and never takes it through first fit.
//D3D12 claims it on every device (shader model 6.4); Vulkan with VK_KHR_shader_integer_dot_product.

#include "graphics/graphics.hpp"
#include "test_graphics_shared.hpp"

namespace oxc { namespace c {
	#include "test_graphics_shared.h"
	#include "platforms/platform.h"
}}

extern "C" void Test_graphicsIntegerDot(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	Test_setModule(t, "GraphicsDevice/integerDot");

	Device dev = Device::share(deviceRef);
	c::Error *e_rr = &t->err;
	const c::Allocator *alloc = c::Platform_instance->alloc;

	const c::Bool supported = !!(dev.info().capabilities.flags.features2 & c::EGraphicsFeatures2_IntegerDot4x8);

	if(dev.api() == c::EGraphicsApi_Direct3D12)
		Test_assert(t, "claimedOnD3D12", supported);

	gfxtest::OwnedSHFile shader(alloc);

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_integer_dot.oiSH", &shader.list)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping");
		return;
	}

	const c::SHFile &file = shader.list;

	if(!Test_assert(t, "oneBinary", file.entries.length == 1 && file.entries.ptr[0].binaryIds.length >= 1))
		return;

	//Every binary declares the extension, in the second mask alone, and it is not dormant

	for(c::U64 i = 0; i < file.entries.ptr[0].binaryIds.length; ++i) {

		const c::SHBinaryInfo &bin = file.binaries.ptr[file.entries.ptr[0].binaryIds.ptr[i]];

		Test_assert(t, "declaresIntegerDot",
			bin.identifier.extensions2 == c::ESHExtension2_IntegerDot4x8 && !bin.dormantExtensions2
		);
	}

	//Without the feature: refused by what is missing, and never a first fit

	if (!supported) {

		c::Bool refused = true;

		for(c::U64 i = 0; i < file.entries.ptr[0].binaryIds.length; ++i) {

			c::Error refusal = c::Error_none();
			const c::SHBinaryInfo &bin = file.binaries.ptr[file.entries.ptr[0].binaryIds.ptr[i]];

			refused &= !c::GraphicsDeviceRef_checkShaderFeatures(dev.handle(), &bin, &file.entries.ptr[0], &refusal);

			const c::CharString why = c::CharString_createRefCStrConst(refusal.errorStr ? refusal.errorStr : "");
			const c::CharString missing = c::CharString_createRefCStrConst("features2");

			refused &= c::CharString_containsStringSensitive(&why, &missing, 0, c::CharString_length(why));
		}

		Test_assert(t, "refusedWithoutFeature", refused);
		Test_assert(t, "noFirstFit", dev.getFirstShaderEntry(file, "main") == c::U32_MAX);
		return;
	}

	if (!dev.hasBindlessTable()) {
		Test_print(t, "Device has no bindless descriptor table, skipping the run");
		return;
	}

	DeviceBuffer output;

	if(!Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag)(c::EGraphicsResourceFlag_ShaderWriteBindless | c::EGraphicsResourceFlag_CPUBacked),
		"Integer dot output", 64 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	Pipeline pipeline;
	PipelineLayout layout;

	if(!gfxtest::computePipelinePush(t, dev, file, pipeline, layout))
		return;

	CommandList commandList, emptyList;

	if(
		!Test_assert(t, "listCreate", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)) ||
		!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr))
	)
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	const c::U32 pushData[4] = { output.writeHandle(), 0, 0, 0 };
	const c::Transition transition = { .resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true };

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		CommandScope scope = commandList.scope({ transition }, 1, {}, e_rr);

		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "push", scope.setPushConstants(pushData, e_rr));
		Test_assert(t, "dispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;
			c::Bool match = true;

			for(c::U32 i = 0; i < 64; ++i) {

				const c::U32 a = i * 0x9E3779B1u + 0x01234567u;
				const c::U32 b = i * 0x85EBCA6Bu + 0x76543210u;

				c::I32 sum = (c::I32) i;

				for(c::U32 lane = 0; lane < 4; ++lane)
					sum += (c::I32) (c::I8) (a >> (lane * 8)) * (c::I32) (c::I8) (b >> (lane * 8));

				match &= values[i] == (c::U32) sum;
			}

			Test_assert(t, "results", match);
		}
}
