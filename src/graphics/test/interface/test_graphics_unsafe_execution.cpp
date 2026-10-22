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

//graphics/test/interface/test_graphics_unsafe_execution.cpp
//
//The devices that alone take a binary marked UnsafeExperimentExecution or UnsafeExtensionExecution, and the experiment
// tiers a device's driver claims.

#include "graphics/graphics.hpp"
#include "test_graphics_shared.hpp"

namespace oxc { namespace c {
	#include "test_graphics_shared.h"
	#include "platforms/platform.h"
	#include "types/base/string_read_helper.h"
}}

namespace {

	//Each gated binary against the device: run where the device has the flag the binary needs and claims the tier it
	// declares, and otherwise refused by the name of what is missing (the flag first) and never a first fit

	struct UnsafeCase {
		const oxc::c::C8 *path, *flagName, *module;
		oxc::c::EGraphicsDeviceFlags flag;    //The device flag the binary needs, or none
		oxc::c::EGraphicsFeatures2 tier;      //The experiment tier it declares, or none
		oxc::c::U32 scale;                    //The shader writes id * scale + 1
		oxc::c::Bool spirvOnly;
		oxc::c::U8 padding[3];
	};

	void Test_graphicsUnsafeCase(oxc::c::Test *t, oxc::gfx::Device &dev, const UnsafeCase &unsafe) {

		using namespace oxc;
		using namespace oxc::gfx;

		c::Error *e_rr = &t->err;
		const c::Allocator *alloc = c::Platform_instance->alloc;
		const c::GraphicsDevice *device = RefPtr_data(dev.handle(), c::GraphicsDevice);
		const c::Bool hasFlag = !unsafe.flag || (device->flags & unsafe.flag);
		const c::Bool claimsTier = (device->info.capabilities.flags.features2 & unsafe.tier) == unsafe.tier;
		const c::Bool allowed = hasFlag && claimsTier;

		Test_setModule(t, unsafe.module);

		if(unsafe.spirvOnly && RefPtr_data(device->instance, c::GraphicsInstance)->api != c::EGraphicsApi_Vulkan) {
			Test_print(t, "SPIR-V only, skipping on this api");
			return;
		}

		gfxtest::OwnedSHFile shader(alloc);

		if (!TestShaders_loadFile(t, unsafe.path, &shader.list)) {
			Test_print(t, "Test shaders unavailable (built without shader compiler), skipping");
			return;
		}

		const c::SHFile &file = shader.list;

		if(!Test_assert(t, "oneMarkedBinary", file.entries.length == 1 && file.entries.ptr[0].binaryIds.length >= 1))
			return;

		if (!allowed) {

			const c::CharString flagName = c::CharString_createRefCStrConst(hasFlag ? "ExperimentTier" : unsafe.flagName);
			c::Bool refused = true, refusedDormant = true;

			for(c::U64 i = 0; i < file.entries.ptr[0].binaryIds.length; ++i) {

				c::Error refusal = c::Error_none();
				const c::SHBinaryInfo &bin = file.binaries.ptr[file.entries.ptr[0].binaryIds.ptr[i]];

				const c::Bool binRefused = !c::GraphicsDeviceRef_checkShaderFeatures(
					dev.handle(), &bin, &file.entries.ptr[0], &refusal
				);

				const c::CharString why = c::CharString_createRefCStrConst(refusal.errorStr ? refusal.errorStr : "");

				refused &= binRefused && c::CharString_containsStringSensitive(
					&why, &flagName, 0, c::CharString_length(why)
				);

				//The markers are declarations, so marking them dormant doesn't get a binary past them.

				c::SHBinaryInfo dormant = bin;
				dormant.dormantExtensions = dormant.identifier.extensions;
				dormant.dormantExtensions2 = dormant.identifier.extensions2;

				refusedDormant &= !c::GraphicsDeviceRef_checkShaderFeatures(
					dev.handle(), &dormant, &file.entries.ptr[0], nullptr
				);
			}

			Test_assert(t, "refusedByName", refused);
			Test_assert(t, "refusedWhenDormant", refusedDormant);
			Test_assert(t, "noFirstFit", dev.getFirstShaderEntry(file, "main") == c::U32_MAX);
			return;
		}

		if (!dev.hasBindlessTable()) {
			c::Test_print(t, "Device has no bindless descriptor table, skipping the run");
			return;
		}

		DeviceBuffer output;

		if(!Test_assert(t, "outputCreate", dev.createBuffer(
			c::EDeviceBufferUsage_None,
			(c::EGraphicsResourceFlag)(c::EGraphicsResourceFlag_ShaderWriteBindless | c::EGraphicsResourceFlag_CPUBacked),
			"Unsafe execution output", 64 * sizeof(c::U32), output, nullptr, e_rr
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

				for(c::U32 i = 0; i < 64; ++i)
					match &= values[i] == i * unsafe.scale + 1;

				Test_assert(t, "results", match);
			}
	}
}

//Both markers and the experiment tier against whatever flags the device was created with and tiers its driver claims:
// the suite's device refuses both markers, and the config module runs this on a device per flag, each of which takes
// its own marker and refuses the other. A tier binary runs only where the driver claims the tier, which an experiment
// that declares one is held to as well.

extern "C" void Test_graphicsUnsafeExecution(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;

	gfx::Device dev = gfx::Device::share(deviceRef);
	const c::GraphicsDevice *device = RefPtr_data(deviceRef, c::GraphicsDevice);

	const c::Bool experiments = !!(device->flags & c::EGraphicsDeviceFlags_AllowUnsafeExperimentExecution);
	const c::Bool extensions = !!(device->flags & c::EGraphicsDeviceFlags_AllowUnsafeExtensionExecution);

	const c::Bool tier1 = !!(device->info.capabilities.flags.features2 & c::EGraphicsFeatures2_ExperimentTier1);

	const UnsafeCase cases[] = {
		{
			"//OxC3_gtest/test_shaders/test_unsafe_experiment.oiSH", "AllowUnsafeExperimentExecution",
			experiments ? "GraphicsDevice/unsafeExperimentAllowed" : "GraphicsDevice/unsafeExperiment",
			c::EGraphicsDeviceFlags_AllowUnsafeExperimentExecution, c::EGraphicsFeatures2_None, 3, true, {}
		},
		{
			"//OxC3_gtest/test_shaders/test_unsafe_extension.oiSH", "AllowUnsafeExtensionExecution",
			extensions ? "GraphicsDevice/unsafeExtensionAllowed" : "GraphicsDevice/unsafeExtension",
			c::EGraphicsDeviceFlags_AllowUnsafeExtensionExecution, c::EGraphicsFeatures2_None, 5, false, {}
		},
		{
			"//OxC3_gtest/test_shaders/test_experiment_tier.oiSH", "",
			tier1 ? "GraphicsDevice/experimentTierClaimed" : "GraphicsDevice/experimentTier",
			c::EGraphicsDeviceFlags_None, c::EGraphicsFeatures2_ExperimentTier1, 7, false, {}
		},
		{
			"//OxC3_gtest/test_shaders/test_unsafe_experiment_tier.oiSH", "AllowUnsafeExperimentExecution",
			experiments && tier1 ? "GraphicsDevice/unsafeExperimentTierAllowed" : "GraphicsDevice/unsafeExperimentTier",
			c::EGraphicsDeviceFlags_AllowUnsafeExperimentExecution, c::EGraphicsFeatures2_ExperimentTier1, 11, true, {}
		}
	};

	for(const UnsafeCase &unsafe : cases)
		Test_graphicsUnsafeCase(t, dev, unsafe);
}

//The experiment tiers the device's driver claims, which OxC3 only reports (EGraphicsFeatures2_ExperimentTier1). A run
// against a driver known to claim some names them in OXC3_TEST_EXPERIMENT_TIERS, a mask of 1 for the first tier and 2
// for the second, and the device must claim exactly those; unset, the claim is only printed.

extern "C" void Test_graphicsExperimentTiers(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	Test_setModule(t, "GraphicsDevice/experimentTiers");

	Device dev = Device::share(deviceRef);
	const c::U32 features2 = dev.info().capabilities.flags.features2;

	const c::U64 claimed =
		(features2 & c::EGraphicsFeatures2_ExperimentTier1 ? 1 : 0) |
		(features2 & c::EGraphicsFeatures2_ExperimentTier2 ? 2 : 0);

	c::U64 expected = c::U64_MAX;

	Test_assert(
		t, "expectationParses",
		c::Platform_getEnvU64(
			c::CharString_createRefCStrConst("OXC3_TEST_EXPERIMENT_TIERS"), c::Platform_instance->alloc, &expected, nullptr
		)
	);

	if(expected != c::U64_MAX)
		Test_assert(t, "claimedAsExpected", claimed == expected);

	static const c::C8 *claims[4] = { "no tier claimed", "tier 1 claimed", "tier 2 claimed", "tiers 1 and 2 claimed" };
	Test_print(t, claims[claimed]);
}
