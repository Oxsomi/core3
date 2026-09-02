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

//graphics/test/interface/test_graphics_config.cpp
//
//Coverage group D: device configuration variants.
//
//  39. GraphicsDevice/configVariants - create devices with non default flags and buffering modes
//
//Every other module runs against one device built with EGraphicsDeviceFlags_None, so the flags that change
// how a device is set up are never exercised at all.
//A flag that cleared the wrong bits, or cleared nothing, would go unnoticed.
//The assertions here are surgical on purpose: each flag is checked to clear exactly the bits it documents
// and to leave every other bit alone, which is what makes them regression tests rather than restatements.
//
//Written against the C++ layer (graphics/graphics.hpp) rather than the C API, so each of the five devices
// this module builds waits and releases itself at the end of the block that made it.
//Device creation itself stays on the C entry point, see Test_createConfigDevice below.

#include "graphics/graphics.hpp"

//Log::debugLn is the C++ front for Log_debugLnx; the x macros name ELogOptions_NewLine unqualified and so
//cannot be reached through the c namespace.

#include "types/container/log.hpp"
#include "test_graphics_shared.hpp"

namespace oxc { namespace c {
	#include "test_graphics_shared.h"
	#include "platforms/platform.h"
}}

//The bits DisableRt documents itself as clearing, kept next to the test rather than derived from the
//implementation, so a change to either side has to be a deliberate change to both.

#define TEST_CONFIG_RT_FEATURES (                        \
	c::EGraphicsFeatures_Raytracing         |            \
	c::EGraphicsFeatures_RayPipeline        |            \
	c::EGraphicsFeatures_RayQuery           |            \
	c::EGraphicsFeatures_RayMicromapOpacity |            \
	c::EGraphicsFeatures_RayReorder         |            \
	c::EGraphicsFeatures_RayValidation      |            \
	c::EGraphicsFeatures_RayTriPosition                  \
)

#define TEST_CONFIG_RT_FEATURES2 (                       \
	c::EGraphicsFeatures2_RayReorderActual   |           \
	c::EGraphicsFeatures2_RayMicromapOpacityActual |     \
	c::EGraphicsFeatures2_RayMicromapOpacityU8 |         \
	c::EGraphicsFeatures2_RayClusterAS       |           \
	c::EGraphicsFeatures2_RayPartitionedTLAS |           \
	c::EGraphicsFeatures2_RayIndirectASBuild             \
)

namespace {

	//Creates a device with the given flags and buffering mode, or reports the failure and returns an empty
	//Device, which the caller tests the same way the C module tested its pointer for NULL.
	//
	//The create itself stays on the C entry point rather than gfx::Device::create: this module is handed a
	// BORROWED GraphicsInstanceRef, and gfx::Instance is immovable, owns the RefPtrType the C side keeps a
	// pointer to, and offers no share() to wrap someone else's instance with.
	//The reference the C factory hands back is then folded into the wrapper, which owns it from there on, so
	// every caller below still gets RAII teardown.

	oxc::gfx::Device Test_createConfigDevice(
		oxc::c::Test *t,
		oxc::c::GraphicsInstanceRef *instRef,
		const oxc::c::GraphicsDeviceInfo *info,
		oxc::c::EGraphicsDeviceFlags flags,
		oxc::c::EGraphicsBufferingMode mode,
		const oxc::c::C8 *assertName,
		const oxc::c::DescriptorHeapInfo *reserve = nullptr
	) noexcept {

		using namespace oxc;

		c::GraphicsDeviceRef *deviceRef = nullptr;

		if(!Test_assert(t, assertName, c::GraphicsDeviceRef_create(
			instRef, info, flags, mode, nullptr, reserve, &deviceRef, &t->err
		)))
			return gfx::Device();

		gfx::Device dev = gfx::Device::share(deviceRef);
		c::RefPtr_dec(&deviceRef);
		return dev;
	}
}

extern "C" void Test_graphicsConfigVariants(
	oxc::c::Test *t, oxc::c::GraphicsInstanceRef *instRef, const oxc::c::GraphicsDeviceInfo *info
) {

	using namespace oxc;
	using namespace oxc::gfx;

	const c::Allocator *alloc = c::Platform_instance->alloc;

	Test_setModule(t, "GraphicsDevice/configVariants");

	//The baseline every variant is compared against.
	//Comparing against info->capabilities directly would not work,
	// since device create is allowed to adjust capabilities for reasons unrelated to these flags.

	Device baseDev = Test_createConfigDevice(
		t, instRef, info, c::EGraphicsDeviceFlags_None, c::EGraphicsBufferingMode_Default, "configBaseCreate"
	);

	if(!baseDev)
		return;

	const c::GraphicsDeviceCapabilities base = baseDev.info().capabilities;

	//39a. DisableRt clears the raytracing bits and nothing else.

	{
		Device rtOff = Test_createConfigDevice(
			t, instRef, info, c::EGraphicsDeviceFlags_DisableRt, c::EGraphicsBufferingMode_Default, "configNoRtCreate"
		);

		if (rtOff) {

			const c::GraphicsDeviceCapabilities caps = rtOff.info().capabilities;

			Test_assert(t, "configNoRtFeatures", !(caps.features & TEST_CONFIG_RT_FEATURES));
			Test_assert(t, "configNoRtFeatures2", !(caps.features2 & TEST_CONFIG_RT_FEATURES2));

			//Everything outside the raytracing set has to survive, which is what makes this a test of the flag
			// rather than a test that the device came up with fewer features.

			Test_assert(
				t, "configNoRtKeepsRest",
				(caps.features  & ~(c::EGraphicsFeatures)  TEST_CONFIG_RT_FEATURES)  ==
				(base.features  & ~(c::EGraphicsFeatures)  TEST_CONFIG_RT_FEATURES)
			);

			Test_assert(
				t, "configNoRtKeepsRest2",
				(caps.features2 & ~(c::EGraphicsFeatures2) TEST_CONFIG_RT_FEATURES2) ==
				(base.features2 & ~(c::EGraphicsFeatures2) TEST_CONFIG_RT_FEATURES2)
			);

			Test_assert(t, "configNoRtKeepsDataTypes", caps.dataTypes == base.dataTypes);

			(void) rtOff.wait();
		}
	}

	//39b. EnableDynamicSamplers puts the bindless _samplers[] array back in the default layout.
	//It is opt in because that array costs a whole descriptor set on Vulkan (set 0, while every resource
	// array shares set 1) and a sampler heap on both backends, and static samplers cover the ordinary case.
	//The suite's own device runs WITHOUT it, so this is the only place the array is exercised at all, which
	// is the point: dropping it by default must not quietly delete a working path.

	{
		Device dynamicSamplers = Test_createConfigDevice(
			t, instRef, info,
			c::EGraphicsDeviceFlags_EnableDynamicSamplers, c::EGraphicsBufferingMode_Default,
			"configDynamicSamplersCreate"
		);

		if (dynamicSamplers)
			Test_graphicsBindlessSampler(t, dynamicSamplers.handle());
	}

	//39c. reservedDescriptors: extra heap capacity on top of the bindless set, so bindful tables can be
	// created from the device's own heap and live beside it, without a second heap and the heap switch a
	// second heap costs.
	//The negative runs on the base device: without a reserve, the default table consumed the heap's single
	// table slot, so the same creation there has to fail.

	{
		c::DescriptorHeapInfo reserve = { .maxTextures = 1, .maxBuffersRW = 1, .maxDescriptorTables = 1 };

		Device reserved = Test_createConfigDevice(
			t, instRef, info,
			c::EGraphicsDeviceFlags_None, c::EGraphicsBufferingMode_Default, "configReserveCreate", &reserve
		);

		if (reserved && (reserved.info().capabilities.features & c::EGraphicsFeatures_Bindless)) {

			//A tiny bindful layout: one texture and one rw buffer, exactly what the reserve holds

			//Plain aggregate init: a compound literal ((c::T) { ... }) is C, which MSVC refuses in C++

			c::DescriptorBinding bindings[2] = {
				{
					.registerType = c::ESHRegisterType_Texture2D,
					.count = 1,
					.visibility = c::U32_MAX
				},
				{
					.registerType = (c::ESHRegisterType)
						(c::ESHRegisterType_ByteAddressBuffer | c::ESHRegisterType_IsWrite),
					.count = 1,
					.binding = { .binding = 1 },
					.visibility = c::U32_MAX
				}
			};

			c::ListDescriptorBinding bindingsRef = {};
			gfxtest::OwnedLayoutInfo layoutInfo(reserved.alloc());

			if (
				Test_assert(t, "configReserveBindingsRef", c::ListDescriptorBinding_createRefConst(
					bindings, 2, &bindingsRef, &t->err
				)) &&
				Test_assert(t, "configReserveBindings", c::ListDescriptorBinding_createCopy(
					bindingsRef, reserved.alloc(), &layoutInfo.list.bindings, &t->err
				))
			) {

				DescriptorLayout layout;
				DescriptorTable table;
				gfxtest::TableGuard tableGuard{ { &table } };

				Test_assert(t, "configReserveLayout", reserved.createDescriptorLayout(
					layoutInfo.list, "Reserve layout", layout, &t->err
				));

				//The whole point: the table allocates from the DEVICE's heap, beside the bindless set

				if(layout)
					Test_assert(t, "configReserveTable", reserved.defaultHeap().createTable(
						layout, "Reserve table", table, c::EDescriptorTableFlags_None, &t->err
					));

				//The base device got no reserve, so its heap holds exactly the bindless set and the one
				// table the default set took; the SAME creation there has to be refused, which is what
				// proves the reserve did anything at all.

				if (layout && (base.features & c::EGraphicsFeatures_Bindless)) {

					DescriptorLayout baseLayout;
					DescriptorTable baseTable;

					Test_assert(t, "configNoReserveLayout", baseDev.createDescriptorLayout(
						layoutInfo.list, "No reserve layout", baseLayout, &t->err
					));

					if(baseLayout)
						Test_assert(t, "configNoReserveTableRefused", !baseDev.defaultHeap().createTable(
							baseLayout, "No reserve table", baseTable, c::EDescriptorTableFlags_None, nullptr
						));
				}
			}
		}
	}

	//39d. DisableBindless clears Bindless and, with it, DescriptorHeap.
	//DescriptorHeap is the interesting half: it is a features2 bit that implies bindless, so a flag that only
	// cleared the obvious bit would leave a device claiming heap indexing it can no longer set up.

	{
		Device noBindless = Test_createConfigDevice(
			t, instRef, info,
			c::EGraphicsDeviceFlags_DisableBindless, c::EGraphicsBufferingMode_Default, "configNoBindlessCreate"
		);

		if (noBindless) {

			const c::GraphicsDeviceCapabilities caps = noBindless.info().capabilities;

			Test_assert(t, "configNoBindlessBit", !(caps.features & c::EGraphicsFeatures_Bindless));
			Test_assert(t, "configNoBindlessHeap", !(caps.features2 & c::EGraphicsFeatures2_DescriptorHeap));

			Test_assert(
				t, "configNoBindlessKeepsRest",
				(caps.features & ~(c::EGraphicsFeatures) c::EGraphicsFeatures_Bindless) ==
				(base.features & ~(c::EGraphicsFeatures) c::EGraphicsFeatures_Bindless)
			);

			Test_assert(t, "configNoBindlessKeepsDataTypes", caps.dataTypes == base.dataTypes);

			//The whole bindful module on a device with bindless OFF: descriptor work must not need it.
			//These are C entry points taking a GraphicsDeviceRef, so they are handed the wrapper's raw handle.

			c::GraphicsDeviceRef *noBindlessRef = noBindless.handle();

			Test_graphicsBindful(t, noBindlessRef);
			Test_graphicsBindfulAdvanced(t, noBindlessRef);
			Test_graphicsBindfulSampler(t, noBindlessRef);
			Test_graphicsBindfulDraw(t, noBindlessRef);
			Test_graphicsBindfulLayoutSwitch(t, noBindlessRef);
			Test_graphicsBindfulCbuffer(t, noBindlessRef);
			Test_graphicsBindfulRwTexture(t, noBindlessRef);
			Test_graphicsBindfulArray(t, noBindlessRef);
			Test_graphicsBindfulSpaces(t, noBindlessRef);
			Test_graphicsBindfulIndirect(t, noBindlessRef);
			Test_graphicsBindfulDrawFixed(t, noBindlessRef);
			Test_graphicsBindfulTableUpdate(t, noBindlessRef);
			Test_graphicsBindfulSharedRegister(t, noBindlessRef);
			Test_graphicsBindfulHeapRecycle(t, noBindlessRef);
			Test_graphicsBindfulPushDescriptorBoundary(t, noBindlessRef);
			Test_graphicsBindfulSamplerCmp(t, noBindlessRef);
			Test_graphicsBindfulStructured(t, noBindlessRef);
			Test_graphicsBindfulAppendCounter(t, noBindlessRef);
			Test_graphicsBindfulAtomicFloat(t, noBindlessRef);
			Test_graphicsBindfulPushConstants(t, noBindlessRef);
			Test_graphicsBindfulPushDescriptors(t, noBindlessRef);
			Test_graphicsBindfulReservedSpace(t, noBindlessRef);

			//The descriptor modules self-branch on hasBindless, so this is their only bindless-off execution

			Test_graphicsDescriptorTable(t, noBindlessRef);
			Test_graphicsDescriptorAlloc(t, noBindlessRef);

			//A device without a bindless layout still has to serve plain resources, since that is the whole
			// point of being able to turn bindless off.

			DeviceBuffer plainBuffer;

			Test_assert(t, "configNoBindlessBuffer", noBindless.createBuffer(
				c::EDeviceBufferUsage_Vertex, c::EGraphicsResourceFlag_None,
				"Config plain buffer", 256, plainBuffer, nullptr, &t->err
			));

			(void) noBindless.wait();
		}
	}

	//39e. Both flags at once, to catch one flag's clearing undoing the other's.

	{
		Device both = Test_createConfigDevice(
			t, instRef, info,
			(c::EGraphicsDeviceFlags)(c::EGraphicsDeviceFlags_DisableRt | c::EGraphicsDeviceFlags_DisableBindless),
			c::EGraphicsBufferingMode_Default, "configBothCreate"
		);

		if (both) {

			const c::GraphicsDeviceCapabilities caps = both.info().capabilities;

			Test_assert(t, "configBothRt", !(caps.features & TEST_CONFIG_RT_FEATURES));
			Test_assert(t, "configBothBindless", !(caps.features & c::EGraphicsFeatures_Bindless));
			Test_assert(t, "configBothFeatures2", !(caps.features2 & TEST_CONFIG_RT_FEATURES2));
			Test_assert(t, "configBothHeap", !(caps.features2 & c::EGraphicsFeatures2_DescriptorHeap));

			(void) both.wait();
		}
	}

	//39f. Buffering modes land on the frame counts they name.
	//Default is deliberately not pinned to a number, since it is device preferred.
	//It only has to be one of the counts the enum offers.

	{
		const c::EGraphicsBufferingMode modes[] = {
			c::EGraphicsBufferingMode_Double, c::EGraphicsBufferingMode_Triple, c::EGraphicsBufferingMode_Default
		};

		const c::U8 expected[] = { 2, 3, 0 };
		const c::C8 *names[] = { "configDouble", "configTriple", "configDefault" };

		//framesInFlight is a raw device field the wrapper does not expose, and GraphicsDeviceRef_ptr is a macro
		// naming its result type unqualified, so the read stays on the C side with that type pulled into scope.

		using c::GraphicsDevice;

		for (c::U64 i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {

			Device modeDev = Test_createConfigDevice(
				t, instRef, info, c::EGraphicsDeviceFlags_None, modes[i], names[i]
			);

			if(!modeDev)
				continue;

			const c::U8 frames = GraphicsDeviceRef_ptr(modeDev.handle())->framesInFlight;

			if(expected[i])
				Test_assert(t, "configFrames", frames == expected[i]);

			else Test_assert(t, "configFramesDefault", frames >= 1 && frames <= 3);

			(void) modeDev.wait();
		}
	}

	//Log_debugLnx is a macro that names ELogOptions_NewLine unqualified, so it cannot be spelled through the c
	// namespace; oxc::Log is the C++ front for the same call and takes the allocator the x suffix implies.

	Log::debugLn(
		*alloc,
		"-- configVariants: base features %08X, verified DisableRt, DisableBindless and buffering modes",
		(c::U32) base.features
	);

	(void) baseDev.wait();
}
