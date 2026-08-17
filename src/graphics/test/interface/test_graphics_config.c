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

//graphics/test/interface/test_graphics_config.c
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

#include "types/test/test.h"
#include "types/container/string.h"
#include "types/base/error.h"
#include "platforms/logx.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_info.h"
#include "graphics/generic/instance.h"
#include "test_graphics_shared.h"

//The bits DisableRt documents itself as clearing, kept next to the test rather than derived from the
//implementation, so a change to either side has to be a deliberate change to both.

#define TEST_CONFIG_RT_FEATURES (                 \
	EGraphicsFeatures_Raytracing         |        \
	EGraphicsFeatures_RayPipeline        |        \
	EGraphicsFeatures_RayQuery           |        \
	EGraphicsFeatures_RayMicromapOpacity |        \
	EGraphicsFeatures_RayMotionBlur      |        \
	EGraphicsFeatures_RayReorder         |        \
	EGraphicsFeatures_RayValidation      |        \
	EGraphicsFeatures_RayTriPosition              \
)

#define TEST_CONFIG_RT_FEATURES2 (                \
	EGraphicsFeatures2_RayReorderActual   |       \
	EGraphicsFeatures2_RayClusterAS       |       \
	EGraphicsFeatures2_RayPartitionedTLAS |       \
	EGraphicsFeatures2_RayIndirectASBuild         \
)

//Creates a device with the given flags and buffering mode, or reports the failure and returns NULL.

static GraphicsDeviceRef *Test_createConfigDevice(
	Test *t,
	GraphicsInstanceRef *instRef,
	const GraphicsDeviceInfo *info,
	EGraphicsDeviceFlags flags,
	EGraphicsBufferingMode mode,
	const C8 *assertName
) {

	GraphicsDeviceRef *deviceRef = NULL;

	if(!Test_assert(t, assertName, GraphicsDeviceRef_create(instRef, info, flags, mode, NULL, &deviceRef, &t->err)))
		return NULL;

	return deviceRef;
}

void Test_graphicsConfigVariants(Test *t, GraphicsInstanceRef *instRef, const GraphicsDeviceInfo *info) {

	Test_setModule(t, "GraphicsDevice/configVariants");

	//The baseline every variant is compared against.
	//Comparing against info->capabilities directly would not work,
	// since device create is allowed to adjust capabilities for reasons unrelated to these flags.

	GraphicsDeviceRef *baseRef = Test_createConfigDevice(
		t, instRef, info, EGraphicsDeviceFlags_None, EGraphicsBufferingMode_Default, "configBaseCreate"
	);

	if(!baseRef)
		return;

	const GraphicsDeviceCapabilities base = GraphicsDeviceRef_ptr(baseRef)->info.capabilities;

	//39a. DisableRt clears the raytracing bits and nothing else.

	{
		GraphicsDeviceRef *rtOffRef = Test_createConfigDevice(
			t, instRef, info, EGraphicsDeviceFlags_DisableRt, EGraphicsBufferingMode_Default, "configNoRtCreate"
		);

		if (rtOffRef) {

			const GraphicsDeviceCapabilities caps = GraphicsDeviceRef_ptr(rtOffRef)->info.capabilities;

			Test_assert(t, "configNoRtFeatures", !(caps.features & TEST_CONFIG_RT_FEATURES));
			Test_assert(t, "configNoRtFeatures2", !(caps.features2 & TEST_CONFIG_RT_FEATURES2));

			//Everything outside the raytracing set has to survive, which is what makes this a test of the flag
			// rather than a test that the device came up with fewer features.

			Test_assert(
				t, "configNoRtKeepsRest",
				(caps.features  & ~(EGraphicsFeatures)  TEST_CONFIG_RT_FEATURES)  ==
				(base.features  & ~(EGraphicsFeatures)  TEST_CONFIG_RT_FEATURES)
			);

			Test_assert(
				t, "configNoRtKeepsRest2",
				(caps.features2 & ~(EGraphicsFeatures2) TEST_CONFIG_RT_FEATURES2) ==
				(base.features2 & ~(EGraphicsFeatures2) TEST_CONFIG_RT_FEATURES2)
			);

			Test_assert(t, "configNoRtKeepsDataTypes", caps.dataTypes == base.dataTypes);

			GraphicsDeviceRef_wait(rtOffRef, NULL);
			RefPtr_dec(&rtOffRef);
		}
	}

	//39b. DisableBindless clears Bindless and, with it, DescriptorHeap.
	//DescriptorHeap is the interesting half: it is a features2 bit that implies bindless, so a flag that only
	// cleared the obvious bit would leave a device claiming heap indexing it can no longer set up.

	{
		GraphicsDeviceRef *noBindlessRef = Test_createConfigDevice(
			t, instRef, info,
			EGraphicsDeviceFlags_DisableBindless, EGraphicsBufferingMode_Default, "configNoBindlessCreate"
		);

		if (noBindlessRef) {

			const GraphicsDeviceCapabilities caps = GraphicsDeviceRef_ptr(noBindlessRef)->info.capabilities;

			Test_assert(t, "configNoBindlessBit", !(caps.features & EGraphicsFeatures_Bindless));
			Test_assert(t, "configNoBindlessHeap", !(caps.features2 & EGraphicsFeatures2_DescriptorHeap));

			Test_assert(
				t, "configNoBindlessKeepsRest",
				(caps.features & ~(EGraphicsFeatures) EGraphicsFeatures_Bindless) ==
				(base.features & ~(EGraphicsFeatures) EGraphicsFeatures_Bindless)
			);

			Test_assert(t, "configNoBindlessKeepsDataTypes", caps.dataTypes == base.dataTypes);

			//A device without a bindless layout still has to serve plain resources, since that is the whole
			// point of being able to turn bindless off.

			DeviceBufferRef *plainBuffer = NULL;
			const CharString plainName = CharString_createRefCStrConst("Config plain buffer");

			if (Test_assert(t, "configNoBindlessBuffer", GraphicsDeviceRef_createBuffer(
				noBindlessRef, EDeviceBufferUsage_Vertex, EGraphicsResourceFlag_None,
				NULL, &plainName, 256, &plainBuffer, &t->err
			)))
				RefPtr_dec(&plainBuffer);

			GraphicsDeviceRef_wait(noBindlessRef, NULL);
			RefPtr_dec(&noBindlessRef);
		}
	}

	//39c. Both flags at once, to catch one flag's clearing undoing the other's.

	{
		GraphicsDeviceRef *bothRef = Test_createConfigDevice(
			t, instRef, info,
			EGraphicsDeviceFlags_DisableRt | EGraphicsDeviceFlags_DisableBindless,
			EGraphicsBufferingMode_Default, "configBothCreate"
		);

		if (bothRef) {

			const GraphicsDeviceCapabilities caps = GraphicsDeviceRef_ptr(bothRef)->info.capabilities;

			Test_assert(t, "configBothRt", !(caps.features & TEST_CONFIG_RT_FEATURES));
			Test_assert(t, "configBothBindless", !(caps.features & EGraphicsFeatures_Bindless));
			Test_assert(t, "configBothFeatures2", !(caps.features2 & TEST_CONFIG_RT_FEATURES2));
			Test_assert(t, "configBothHeap", !(caps.features2 & EGraphicsFeatures2_DescriptorHeap));

			GraphicsDeviceRef_wait(bothRef, NULL);
			RefPtr_dec(&bothRef);
		}
	}

	//39d. Buffering modes land on the frame counts they name.
	//Default is deliberately not pinned to a number, since it is device preferred.
	//It only has to be one of the counts the enum offers.

	{
		const EGraphicsBufferingMode modes[] = {
			EGraphicsBufferingMode_Double, EGraphicsBufferingMode_Triple, EGraphicsBufferingMode_Default
		};

		const U8 expected[] = { 2, 3, 0 };
		const C8 *names[] = { "configDouble", "configTriple", "configDefault" };

		for (U64 i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {

			GraphicsDeviceRef *modeRef = Test_createConfigDevice(
				t, instRef, info, EGraphicsDeviceFlags_None, modes[i], names[i]
			);

			if(!modeRef)
				continue;

			const U8 frames = GraphicsDeviceRef_ptr(modeRef)->framesInFlight;

			if(expected[i])
				Test_assert(t, "configFrames", frames == expected[i]);

			else Test_assert(t, "configFramesDefault", frames >= 1 && frames <= 3);

			GraphicsDeviceRef_wait(modeRef, NULL);
			RefPtr_dec(&modeRef);
		}
	}

	Log_debugLnx("-- configVariants: base features %08X, verified DisableRt, DisableBindless and buffering modes",
		(U32) base.features
	);

	GraphicsDeviceRef_wait(baseRef, NULL);
	RefPtr_dec(&baseRef);
}
