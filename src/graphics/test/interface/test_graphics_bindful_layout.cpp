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

//graphics/test/interface/test_graphics_bindful_layout.cpp
//
//Pipeline layout state that is not a descriptor: push constants, and the register space OxC3 reserves
//for its own per frame globals.
//Split out of test_graphics_bindful.c, which had grown to 24 modules in one file.

namespace oxc { namespace c {
	#include "types/base/string_base.h"
	#include "types/container/list_basic_types.h"
	#include "types/test/test.h"
	#include "formats/oiSH/sh_file.h"
	#include "formats/oiSH/sh_registers.h"
	#include "platforms/platform.h"
	#include "graphics/generic/blas.h"
	#include "graphics/generic/command_list.h"
	#include "graphics/generic/commands.h"
	#include "graphics/generic/depth_stencil.h"
	#include "graphics/generic/descriptor_heap.h"
	#include "graphics/generic/descriptor_layout.h"
	#include "graphics/generic/descriptor_table.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_buffer.h"
	#include "graphics/generic/device_info.h"
	#include "graphics/generic/device_texture.h"
	#include "graphics/generic/graphics_types.h"
	#include "graphics/generic/instance.h"
	#include "graphics/generic/pipeline.h"
	#include "graphics/generic/pipeline_layout.h"
	#include "graphics/generic/render_texture.h"
	#include "graphics/generic/sampler.h"
	#include "graphics/generic/tlas.h"
	#include "test_graphics_shared.h"
} }

//Same namespace the C headers landed in, so the definitions here match the declarations in
//test_graphics_shared.h and the macros in those headers still expand to names that resolve.

namespace oxc { namespace c {

// -- 63. Push constants ---------------------------------------------------------

//Push constants are root constants rather than descriptors: the values ride in the command stream instead
// of the heap, so nothing about them goes through a table. That also means a root signature switch drops
// them on D3D12, which is why the backends re-emit at the work op rather than at the write.
//Two dispatches with different constants and no rebinding between them prove the re-emit really happens.

typedef struct TestBindfulPushData {
	U32 scale, bias, xorMask, offset;
} TestBindfulPushData;

void Test_graphicsBindfulPushConstants(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/pushConstants");

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *output = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file {};
	DescriptorLayoutInfo layoutInfo {};
	DescriptorBinding pushConstants {};

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_pushconst.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping push constant tests");
		return;
	}

	const U32 entryId = TestShaders_entry(t, deviceRef, &file, "main");

	if(entryId == U32_MAX)
		goto clean;

	//Matched by shape rather than by name, which is what the engine's own copy shaders do: a global struct
	// becomes the implicit $Globals cbuffer on DXIL, so its register never carries the variable's name.
	//Vulkan reflects it as a real push constant register, so both backends land on the same binding here.

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &file, entryId, EDescriptorLayoutFlags_None,
		EDetectDescriptorLayoutFlags_AssumePushConstants,
		NULL, NULL, &pushConstants, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	if (!Test_assert(t, "detectedPushConstants", pushConstants.count != 0))
		goto clean;

	Test_assert(t, "pushConstantSize", pushConstants.constantBufferSize == sizeof(TestBindfulPushData));

	CharString name = CharString_createRefCStrConst("Push constant layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	//Scoped so the goto above jumps around these rather than into them.
	{
	DescriptorHeapInfo heapInfo = { .maxBuffersRW = 1, .maxDescriptorTables = 1 };
	name = CharString_createRefCStrConst("Push constant heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Push constant table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Push constant output");

	if(!Test_assert(t, "outputCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		(EGraphicsResourceFlag) (EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked),
		NULL, &name, 128 * sizeof(U32), &output, &t->err
	)))
		goto clean;

	const Descriptor outputDesc = Descriptor_buffer(output, 0, 0, NULL, 0);
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setOutput", DescriptorTableRef_setDescriptorByName(table, &outputName, 0, false, &outputDesc, &t->err));

	PipelineLayoutInfo pipelineLayoutInfo = {
		.bindings = layout, .pushConstants = pushConstants
	};

	name = CharString_createRefCStrConst("Push constant pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Push constant pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &file, &name, entryId, NULL, EPipelineFlags_None, pipelineLayout, &pipeline, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "listCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, 4 * KIBI, 64, 16, true, &commandList, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "emptyListCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, KIBI, 16, 8, true, &emptyList, &t->err
	)))
		goto clean;

	Test_assert(t, "beginEmpty", CommandListRef_begin(emptyList, true, U64_MAX, &t->err));
	Test_assert(t, "endEmpty", CommandListRef_end(emptyList, &t->err));

	const Transition transition = {
		.resource = output, .stage = EPipelineStage_Compute, .isWrite = true
	};

	ListTransition transitionList {};
	ListTransition_createRefConst(&transition, 1, &transitionList, NULL);

	//A dispatch without the constants written has to be refused: the range would hold whatever the last
	// pipeline left in it, which is exactly the garbage read this validation exists to prevent

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

	Test_assert(t, "scopeNeg", CommandListRef_startScope(commandList, &transitionList, 1, NULL, &t->err));
	Test_assert(t, "bindHeapNeg", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTableNeg", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
	Test_assert(t, "bindPipelineNeg", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
	Test_assert(t, "dispatchWithoutConstants", !CommandListRef_dispatch1D(commandList, 1, NULL));

	//A partial write is refused too, for the same reason: the rest of the range would be stale

	const U32 tooSmall = 4;

	Test_assert(t, "setTooSmall", CommandListRef_setPushConstants(
		commandList, Buffer_createRefConst(&tooSmall, sizeof(tooSmall)), &t->err
	));

	Test_assert(t, "dispatchWrongSize", !CommandListRef_dispatch1D(commandList, 1, NULL));
	Test_assert(t, "scopeNegEnd", CommandListRef_endScope(commandList, &t->err));

	//Two dispatches, different constants, nothing rebound in between: the second set only lands if the
	// backend re-emits at the work op rather than once at the bind

	//Disjoint output ranges, so both results survive and neither dispatch races the other for a slot

	const TestBindfulPushData first = { .scale = 3, .bias = 7, .xorMask = 0, .offset = 0 };
	const TestBindfulPushData second = { .scale = 5, .bias = 1, .xorMask = 0xFFu, .offset = 64 };

	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 2, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTable", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));

	Test_assert(t, "setFirst", CommandListRef_setPushConstants(
		commandList, Buffer_createRefConst(&first, sizeof(first)), &t->err
	));

	Test_assert(t, "dispatchFirst", CommandListRef_dispatch1D(commandList, 1, &t->err));

	Test_assert(t, "setSecond", CommandListRef_setPushConstants(
		commandList, Buffer_createRefConst(&second, sizeof(second)), &t->err
	));

	Test_assert(t, "dispatchSecond", CommandListRef_dispatch1D(commandList, 1, &t->err));

	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			//Each dispatch wrote its own half, so BOTH sets have to be visible: that is what proves the
			// second write reached the GPU instead of the first one being reused. Sharing one range instead
			// would just race the two dispatches, which says nothing about the push constants.

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			Bool allMatch = true;

			for(U32 i = 0; i < 64; ++i) {
				allMatch &= values[i] == ((i * first.scale + first.bias) ^ first.xorMask);
				allMatch &= values[i + 64] == ((i * second.scale + second.bias) ^ second.xorMask);
			}

			Test_assert(t, "pushConstantResults", allMatch);
		}

	}

clean:

	if(table)
		DescriptorTableRef_unsetDescriptors(table, 0, 0, 1, NULL);

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&output);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&file, alloc);
}

// -- 64. The reserved register space --------------------------------------------

//OxC3 binds its own per frame globals (frame id, time, swapchain descriptors) to a register space
// it keeps for itself, so a caller's layout may not put anything there.
//It matters now that anyone can build a layout: the globals used to sit at b0 space0, which is the first
// thing someone writing a constant buffer reaches for, and nothing would have told them they collided.

void Test_graphicsBindfulReservedSpace(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindful/reservedSpace");

	DescriptorLayoutRef *layout = NULL;

	//A layout that is legal in every respect except the space it asks for

	DescriptorBinding reserved = {
		.registerType = (ESHRegisterType) (ESHRegisterType_ByteAddressBuffer | ESHRegisterType_IsWrite),
		.count = 1,
		.binding = { .space = OXC3_RESERVED_SPACE, .binding = 0 },
		.visibility = U32_MAX
	};

	const CharString reservedName = CharString_createRefCStrConst("collides");

	DescriptorLayoutInfo info {};
	ListDescriptorBinding_createRefConst(&reserved, 1, &info.bindings, NULL);
	ListCharString_createRefConst(&reservedName, 1, &info.bindingNames, NULL);

	const CharString name = CharString_createRefCStrConst("Reserved space layout");

	Test_assert(t, "reservedSpaceRefused", !GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &info, &name, &layout, NULL
	));

	Test_assert(t, "reservedSpaceNoLayout", !layout);

	//The very same binding one space over is fine, which is what proves the space is the only objection

	reserved.binding.space = OXC3_RESERVED_SPACE + 1;

	DescriptorLayoutInfo okInfo {};
	ListDescriptorBinding_createRefConst(&reserved, 1, &okInfo.bindings, NULL);
	ListCharString_createRefConst(&reservedName, 1, &okInfo.bindingNames, NULL);

	if(Test_assert(t, "neighbourSpaceAccepted", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &okInfo, &name, &layout, &t->err
	)))
		RefPtr_dec(&layout);
}
} }
