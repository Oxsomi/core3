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

//graphics/test/interface/test_graphics_bindless.c

//The bindless half of the descriptor tests (modules 12, 13): the device's default table and buffers
// exposing themselves into it. Devices without bindless skip with a message; the bindful twin of this
// coverage lives in test_graphics_bindful.c and runs everywhere.

#include "graphics/generic/device.h"
#include "graphics/generic/device_info.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/commands.h"
#include "platforms/platform.h"
#include "formats/oiSH/sh_file.h"
#include "graphics/generic/bindless_descriptor.h"
#include "graphics/generic/graphics_types.h"
#include "types/test/test.h"
#include "types/base/string_base.h"
#include "test_graphics_shared.h"

// -- 12. Bindless descriptors in the device's default table ----------------------

void Test_graphicsBindlessDescriptor(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "BindlessDescriptor");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!device->defaultDescriptorTable) {
		Test_print(t, "Device has no bindless descriptor table, skipping bindless descriptor tests");
		return;
	}

	DeviceBufferRef *buffer = NULL;
	CharString name = CharString_createRefCStrConst("Bindless descriptor test buffer");

	//ShaderRead without ExposeBindlessRead, so the buffer takes no descriptor of its own and this test owns the one
	// it allocates below.

	if(!Test_assert(t, "bufferCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL, &name, 256, &buffer, &t->err
	)))
		return;

	const Descriptor desc = Descriptor_buffer(buffer, 0, 0, NULL, 0);
	BindlessDescriptor handle = BindlessDescriptor_None;

	//NULL as the table means the device's default one.

	Test_assert(t, "allocate", GraphicsDeviceRef_allocateDescriptorBindless(
		deviceRef, NULL, ESHRegisterType_ByteAddressBuffer, 0, false, &desc, &handle, &t->err
	));

	Test_assert(t, "handleNotNone", handle != BindlessDescriptor_None);
	Test_assert(t, "handleHasType", BindlessDescriptor_getBindlessType(handle) != 0);
	Test_assert(t, "handleValid", BindlessDescriptor_isValid(deviceRef, NULL, handle));

	//The default layout has at most 13 bindless arrays, so type 15 can never resolve to one.

	Test_assert(t, "handleTypeOutOfRange", !BindlessDescriptor_isValid(deviceRef, NULL, (BindlessDescriptor)15 << 17));
	Test_assert(t, "handleNoDevice", !BindlessDescriptor_isValid(NULL, NULL, handle));

	//Allocation validation; a descriptor is required and so is somewhere to put the handle.

	Test_assert(t, "allocateNoDevice", !GraphicsDeviceRef_allocateDescriptorBindless(
		NULL, NULL, ESHRegisterType_ByteAddressBuffer, 0, false, &desc, &handle, NULL
	));

	BindlessDescriptor unused = BindlessDescriptor_None;

	Test_assert(t, "allocateNoDescriptor", !GraphicsDeviceRef_allocateDescriptorBindless(
		deviceRef, NULL, ESHRegisterType_ByteAddressBuffer, 0, false, NULL, &unused, NULL
	));

	Test_assert(t, "allocateNothingLeaked", unused == BindlessDescriptor_None);

	//Freeing hands the slot back, so allocating the same descriptor again lands on it.

	Test_assert(t, "free", GraphicsDeviceRef_freeDescriptorBindless(deviceRef, NULL, handle, &t->err));

	BindlessDescriptor reused = BindlessDescriptor_None;

	Test_assert(t, "reallocate", GraphicsDeviceRef_allocateDescriptorBindless(
		deviceRef, NULL, ESHRegisterType_ByteAddressBuffer, 0, false, &desc, &reused, &t->err
	));

	Test_assert(t, "slotReused", reused == handle);

	//Freeing twice and freeing None are both no-ops rather than errors, which is what resource destructors rely on.

	Test_assert(t, "freeAgain", GraphicsDeviceRef_freeDescriptorBindless(deviceRef, NULL, reused, &t->err));
	Test_assert(t, "freeTwice", GraphicsDeviceRef_freeDescriptorBindless(deviceRef, NULL, reused, &t->err));

	Test_assert(t, "freeNone", GraphicsDeviceRef_freeDescriptorBindless(
		deviceRef, NULL, BindlessDescriptor_None, &t->err
	));

	RefPtr_dec(&buffer);
}

// -- 13. DeviceBuffer only takes a bindless descriptor when asked ----------------

void Test_graphicsBufferBindless(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "DeviceBuffer/bindless");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	DeviceBufferRef *plain = NULL;
	DeviceBufferRef *readOnly = NULL;
	DeviceBufferRef *readWrite = NULL;
	DeviceBufferRef *rejected = NULL;

	CharString name = CharString_createRefCStrConst("Bindless flagless buffer");

	//Without either Expose flag there's no descriptor and no table ref, whatever the device supports.

	Test_assert(t, "plainCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL, &name, 256, &plain, &t->err
	));

	if (plain) {

		const DeviceBuffer *plainPtr = DeviceBufferRef_ptr(plain);

		Test_assert(t, "plainNoReadHandle", plainPtr->readHandle == BindlessDescriptor_None);
		Test_assert(t, "plainNoWriteHandle", plainPtr->writeHandle == BindlessDescriptor_None);
		Test_assert(t, "plainNoTable", !plainPtr->bindlessDescriptorTable);
	}

	name = CharString_createRefCStrConst("Bindless read buffer");

	if (!device->defaultDescriptorTable) {

		//Asking to be exposed on a device that has nowhere to expose it has to fail rather than silently do nothing.

		Test_assert(t, "exposeWithoutBindless", !GraphicsDeviceRef_createBuffer(
			deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderReadBindless, NULL, &name, 256, &rejected, NULL
		));

		Test_assert(t, "exposeWithoutBindlessNothing", !rejected);
		Test_print(t, "Device has no bindless descriptor table, skipping exposed buffer tests");
		goto clean;
	}

	//ExposeBindlessRead takes a read descriptor and nothing else.

	Test_assert(t, "readCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderReadBindless, NULL, &name, 256, &readOnly, &t->err
	));

	if (readOnly) {

		const DeviceBuffer *readPtr = DeviceBufferRef_ptr(readOnly);

		Test_assert(t, "readHandle", readPtr->readHandle != BindlessDescriptor_None);
		Test_assert(t, "readNoWriteHandle", readPtr->writeHandle == BindlessDescriptor_None);
		Test_assert(t, "readDefaultTable", readPtr->bindlessDescriptorTable == device->defaultDescriptorTable);
		Test_assert(t, "readHandleValid", BindlessDescriptor_isValid(deviceRef, NULL, readPtr->readHandle));
	}

	//Both Expose flags take two descriptors, one per binding, so they can't be the same handle.

	name = CharString_createRefCStrConst("Bindless read write buffer");

	Test_assert(t, "rwCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRWBindless, NULL, &name, 256, &readWrite, &t->err
	));

	if (readWrite) {

		const DeviceBuffer *rwPtr = DeviceBufferRef_ptr(readWrite);

		Test_assert(t, "rwReadHandle", rwPtr->readHandle != BindlessDescriptor_None);
		Test_assert(t, "rwWriteHandle", rwPtr->writeHandle != BindlessDescriptor_None);
		Test_assert(t, "rwHandlesDiffer", rwPtr->readHandle != rwPtr->writeHandle);
		Test_assert(t, "rwWriteHandleValid", BindlessDescriptor_isValid(deviceRef, NULL, rwPtr->writeHandle));
	}

	//A table without a flag that says the resource may be exposed is a contradiction.

	Test_assert(t, "tableWithoutExposeFlag", !GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_None, device->defaultDescriptorTable,
		&name, 256, &rejected, NULL
	));

	Test_assert(t, "tableWithoutExposeFlagNothing", !rejected);

clean:

	RefPtr_dec(&readWrite);
	RefPtr_dec(&readOnly);
	RefPtr_dec(&plain);
}

// -- 14. Bindful and bindless interleaved in one scope ---------------------------

//Backends emit descriptor state lazily at the work op and remember what the command buffer last saw, so the
// dangerous transitions are custom to default (the default sets or root arguments have to come back) and
// default to custom right after. Three dispatches cross both transitions in a single scope; each writes its
// own buffer, so a stale table or root signature can't hide.
//Lives in this file rather than the bindful one because it NEEDS bindless, which that file must not.

void Test_graphicsBindlessInterleave(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindless/interleave");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!device->defaultDescriptorTable) {
		Test_print(t, "Device has no bindless descriptor table, skipping bindful interleave tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *tableA = NULL;
	DescriptorTableRef *tableB = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *bindfulPipeline = NULL;
	PipelineRef *bindlessPipeline = NULL;
	DeviceBufferRef *src = NULL;
	DeviceBufferRef *dstA = NULL;
	DeviceBufferRef *dstB = NULL;
	DeviceBufferRef *bindlessOut = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile copyFile = (SHFile) { 0 };
	SHFile writeFile = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };

	if (
		!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_copy.oiSH", &copyFile) ||
		!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_write.oiSH", &writeFile)
	) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful interleave tests");
		SHFile_free(&copyFile, alloc);
		return;
	}

	const U32 entryId = TestShaders_entry(t, deviceRef, &copyFile, "main");

	if(entryId == U32_MAX)
		goto clean;

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &copyFile, entryId, EDescriptorLayoutFlags_None, (EDetectDescriptorLayoutFlags) 0,
		NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	CharString name = CharString_createRefCStrConst("Bindful interleave layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 4, .maxDescriptorTables = 2 };
	name = CharString_createRefCStrConst("Bindful interleave heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful interleave table A");

	if(!Test_assert(t, "tableACreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &tableA, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful interleave table B");

	if(!Test_assert(t, "tableBCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &tableB, &t->err
	)))
		goto clean;

	U32 srcData[64];

	for(U32 i = 0; i < 64; ++i)
		srcData[i] = i;

	Buffer srcRef = Buffer_createRefConst(srcData, sizeof(srcData));
	name = CharString_createRefCStrConst("Bindful interleave src");

	if(!Test_assert(t, "srcCreate", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL,
		&name, &srcRef, &src, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful interleave dst A");

	if(!Test_assert(t, "dstACreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &dstA, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful interleave dst B");

	if(!Test_assert(t, "dstBCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &dstB, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful interleave bindless out");

	if(!Test_assert(t, "bindlessOutCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWriteBindless | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &bindlessOut, &t->err
	)))
		goto clean;

	const Descriptor srcDesc = Descriptor_buffer(src, 0, 0, NULL, 0);
	const Descriptor dstADesc = Descriptor_buffer(dstA, 0, 0, NULL, 0);
	const Descriptor dstBDesc = Descriptor_buffer(dstB, 0, 0, NULL, 0);

	const CharString inputName = CharString_createRefCStrConst("input");
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setSrcA", DescriptorTableRef_setDescriptorByName(tableA, &inputName, 0, false, &srcDesc, &t->err));
	Test_assert(t, "setDstA", DescriptorTableRef_setDescriptorByName(tableA, &outputName, 0, false, &dstADesc, &t->err));
	Test_assert(t, "setSrcB", DescriptorTableRef_setDescriptorByName(tableB, &inputName, 0, false, &srcDesc, &t->err));
	Test_assert(t, "setDstB", DescriptorTableRef_setDescriptorByName(tableB, &outputName, 0, false, &dstBDesc, &t->err));

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Bindful interleave pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindful interleave pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &copyFile, &name, entryId, NULL, EPipelineFlags_None, pipelineLayout, &bindfulPipeline, &t->err
	)))
		goto clean;

	if(!TestShaders_computePipeline(t, deviceRef, &writeFile, &bindlessPipeline))
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

	const Transition transitions[4] = {
		(Transition) { .resource = src,         .stage = EPipelineStage_Compute },
		(Transition) { .resource = dstA,        .stage = EPipelineStage_Compute, .isWrite = true },
		(Transition) { .resource = dstB,        .stage = EPipelineStage_Compute, .isWrite = true },
		(Transition) { .resource = bindlessOut, .stage = EPipelineStage_Compute, .isWrite = true }
	};

	ListTransition transitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(transitions, 4, &transitionList, NULL);

	//Custom, then default, then custom again, all in one scope

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 1, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));

	Test_assert(t, "bindTableA", CommandListRef_bindDescriptorTable(commandList, tableA, &t->err));
	Test_assert(t, "bindBindful", CommandListRef_setComputePipeline(commandList, bindfulPipeline, &t->err));
	Test_assert(t, "dispatchA", CommandListRef_dispatch1D(commandList, 1, &t->err));

	Test_assert(t, "bindBindless", CommandListRef_setComputePipeline(commandList, bindlessPipeline, &t->err));
	Test_assert(t, "dispatchBindless", CommandListRef_dispatch1D(commandList, 1, &t->err));

	Test_assert(t, "bindTableB", CommandListRef_bindDescriptorTable(commandList, tableB, &t->err));
	Test_assert(t, "bindBindful2", CommandListRef_setComputePipeline(commandList, bindfulPipeline, &t->err));
	Test_assert(t, "dispatchB", CommandListRef_dispatch1D(commandList, 1, &t->err));

	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	TestShaderAppData appData = (TestShaderAppData) {
		.handles = { DeviceBufferRef_ptr(bindlessOut)->writeHandle, 0xC0DE0000u }
	};

	if (TestShaders_submitAndWait(t, deviceRef, commandList, &appData, sizeof(appData))) {

		Bool okA = TestShaders_pullBuffer(t, deviceRef, emptyList, dstA);
		Bool okOut = TestShaders_pullBuffer(t, deviceRef, emptyList, bindlessOut);
		Bool okB = TestShaders_pullBuffer(t, deviceRef, emptyList, dstB);

		if (okA && okOut && okB) {

			const U32 *a = (const U32*) DeviceBufferRef_ptr(dstA)->cpuData.ptr;
			const U32 *o = (const U32*) DeviceBufferRef_ptr(bindlessOut)->cpuData.ptr;
			const U32 *b = (const U32*) DeviceBufferRef_ptr(dstB)->cpuData.ptr;

			Bool allMatch = true;

			for(U32 i = 0; i < 64; ++i)
				allMatch &= a[i] == i * 2 + 1 && o[i] == 0xC0DE0000u + i && b[i] == i * 2 + 1;

			Test_assert(t, "interleaveResults", allMatch);
		}
	}

clean:

	if(tableA) {
		DescriptorTableRef_unsetDescriptors(tableA, 0, 0, 1, NULL);
		DescriptorTableRef_unsetDescriptors(tableA, 1, 0, 1, NULL);
	}

	if(tableB) {
		DescriptorTableRef_unsetDescriptors(tableB, 0, 0, 1, NULL);
		DescriptorTableRef_unsetDescriptors(tableB, 1, 0, 1, NULL);
	}

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&bindlessPipeline);
	RefPtr_dec(&bindfulPipeline);
	RefPtr_dec(&pipelineLayout);
	RefPtr_dec(&bindlessOut);
	RefPtr_dec(&dstB);
	RefPtr_dec(&dstA);
	RefPtr_dec(&src);
	RefPtr_dec(&tableB);
	RefPtr_dec(&tableA);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);

	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	SHFile_free(&copyFile, alloc);
	SHFile_free(&writeFile, alloc);
}

// -- 15. A custom layout that opts every binding into bindless updates ----------

//AllowBindlessEverywhere drops the array-only restriction and treats every register as dynamic, which is a
//distinct backend shape rather than a hint: Vulkan adds UPDATE_AFTER_BIND and partially bound flags to
//bindings that would not otherwise carry them, and D3D12 relaxes the descriptor volatility flags on the
//range. Tests only ever used AllowBindlessOnArrays, so this shape was created but never executed.
//It requires the bindless feature even though the layout is a custom one, which is why it lives here.

void Test_graphicsBindlessEverywhere(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindless/everywhere");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!(device->info.capabilities.features & EGraphicsFeatures_Bindless)) {
		Test_print(t, "Device has no bindless, skipping bindless everywhere tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	PipelineLayoutRef *pipelineLayout = NULL;
	PipelineRef *pipeline = NULL;
	DeviceBufferRef *output = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };
	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_write.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindless everywhere tests");
		return;
	}

	const U32 entryId = TestShaders_entry(t, deviceRef, &file, "main");

	if(entryId == U32_MAX)
		goto clean;

	//The flag rides on the layout, so detection is asked for it rather than the layout being hand built

	if(!Test_assert(t, "detectLayout", GraphicsDeviceRef_detectLayoutFromEntry(
		deviceRef, &file, entryId, EDescriptorLayoutFlags_AllowBindlessEverywhere,
		(EDetectDescriptorLayoutFlags) 0, NULL, NULL, NULL, &layoutInfo, NULL, &t->err
	)))
		goto clean;

	CharString name = CharString_createRefCStrConst("Bindless everywhere layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &layoutInfo, &name, &layout, &t->err
	)))
		goto clean;

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) {
		.flags = EDescriptorHeapFlags_AllowBindless, .maxBuffersRW = 1, .maxDescriptorTables = 1
	};

	name = CharString_createRefCStrConst("Bindless everywhere heap");

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindless everywhere table");

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindless everywhere output");

	if(!Test_assert(t, "outputCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWrite | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 64 * sizeof(U32), &output, &t->err
	)))
		goto clean;

	const Descriptor outputDesc = Descriptor_buffer(output, 0, 0, NULL, 0);
	const CharString outputName = CharString_createRefCStrConst("output");

	Test_assert(t, "setOutput", DescriptorTableRef_setDescriptorByName(table, &outputName, 0, false, &outputDesc, &t->err));

	PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) { .bindings = layout };
	name = CharString_createRefCStrConst("Bindless everywhere pipeline layout");

	if(!Test_assert(t, "pipelineLayoutCreate", GraphicsDeviceRef_createPipelineLayout(
		deviceRef, &pipelineLayoutInfo, &name, &pipelineLayout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Bindless everywhere pipeline");

	if(!Test_assert(t, "pipelineCreate", GraphicsDeviceRef_createPipelineCompute(
		deviceRef, &file, &name, entryId, NULL, EPipelineFlags_None, pipelineLayout, &pipeline, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "listCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, 2 * KIBI, 32, 16, true, &commandList, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "emptyListCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, KIBI, 16, 8, true, &emptyList, &t->err
	)))
		goto clean;

	Test_assert(t, "beginEmpty", CommandListRef_begin(emptyList, true, U64_MAX, &t->err));
	Test_assert(t, "endEmpty", CommandListRef_end(emptyList, &t->err));

	const Transition transition = (Transition) {
		.resource = output, .stage = EPipelineStage_Compute, .isWrite = true
	};

	ListTransition transitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(&transition, 1, &transitionList, NULL);

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 1, NULL, &t->err));
	Test_assert(t, "bindHeap", CommandListRef_bindDescriptorHeap(commandList, heap, &t->err));
	Test_assert(t, "bindTable", CommandListRef_bindDescriptorTable(commandList, table, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
	Test_assert(t, "dispatch", CommandListRef_dispatch1D(commandList, 1, &t->err));
	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	if (TestShaders_submitAndWait(t, deviceRef, commandList, NULL, 0))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			Bool allMatch = true;

			for(U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == i * 3 + 7;

			Test_assert(t, "everywhereResults", allMatch);
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

// -- 16. The per frame globals other than app data ------------------------------

//Every bindless test reads getAppData, so the app data half of the globals block is covered by the whole
//suite. The rest of it - frame id, time, delta, swapchain count - had no execution coverage at all, which
//is exactly the half a change to where the block lives can break without a single test noticing.
//Two submits, because the interesting property is that these move: the frame id has to advance and the
//clock must not run backwards.

void Test_graphicsFrameGlobals(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Bindless/frameGlobals");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if (!device->defaultDescriptorTable) {
		Test_print(t, "Device has no bindless descriptor table, skipping frame globals tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	DeviceBufferRef *output = NULL;
	PipelineRef *pipeline = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	SHFile file = (SHFile) { 0 };

	if (!TestShaders_loadFile(t, "//OxC3_gtest/test_shaders/test_frame_globals.oiSH", &file)) {
		Test_print(t, "Test shaders unavailable (built without shader compiler), skipping frame globals tests");
		return;
	}

	CharString name = CharString_createRefCStrConst("Frame globals output");

	if(!Test_assert(t, "outputCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None,
		EGraphicsResourceFlag_ShaderWriteBindless | EGraphicsResourceFlag_CPUBacked,
		NULL, &name, 4 * sizeof(U32), &output, &t->err
	)))
		goto clean;

	if(!TestShaders_computePipeline(t, deviceRef, &file, &pipeline))
		goto clean;

	if(!Test_assert(t, "listCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, 2 * KIBI, 32, 16, true, &commandList, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "emptyListCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, KIBI, 16, 8, true, &emptyList, &t->err
	)))
		goto clean;

	Test_assert(t, "beginEmpty", CommandListRef_begin(emptyList, true, U64_MAX, &t->err));
	Test_assert(t, "endEmpty", CommandListRef_end(emptyList, &t->err));

	const Transition transition = (Transition) {
		.resource = output, .stage = EPipelineStage_Compute, .isWrite = true
	};

	ListTransition transitionList = (ListTransition) { 0 };
	ListTransition_createRefConst(&transition, 1, &transitionList, NULL);

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "scope", CommandListRef_startScope(commandList, &transitionList, 1, NULL, &t->err));
	Test_assert(t, "bindPipeline", CommandListRef_setComputePipeline(commandList, pipeline, &t->err));
	Test_assert(t, "dispatch", CommandListRef_dispatch1D(commandList, 1, &t->err));
	Test_assert(t, "scopeEnd", CommandListRef_endScope(commandList, &t->err));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

	TestShaderAppData appData = (TestShaderAppData) {
		.handles = { DeviceBufferRef_ptr(output)->writeHandle }
	};

	U32 firstFrameId = 0;
	F32 firstTime = 0;
	Bool readFirst = false;

	if (TestShaders_submitAndWait(t, deviceRef, commandList, &appData, sizeof(appData)))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			firstFrameId = values[0];
			Buffer_memcpy(Buffer_createRef(&firstTime, sizeof(F32)), Buffer_createRefConst(&values[1], sizeof(U32)));

			F32 deltaTime = 0;
			Buffer_memcpy(Buffer_createRef(&deltaTime, sizeof(F32)), Buffer_createRefConst(&values[2], sizeof(U32)));

			//The shader really read the block rather than zero initialised memory: a submit that has happened
			// has a non zero frame id, and neither clock can be negative

			Test_assert(t, "frameIdNonZero", firstFrameId != 0);
			Test_assert(t, "timeNotNegative", firstTime >= 0);
			Test_assert(t, "deltaNotNegative", deltaTime >= 0);

			//No swapchain exists in this headless suite, so the count the runtime published has to say so

			Test_assert(t, "swapchainCountZero", values[3] == 0);

			readFirst = true;
		}

	//A second submit of the same list: the frame id advances and time cannot go backwards, which is what
	// makes this a test of the live per frame block rather than of one constant

	if (readFirst && TestShaders_submitAndWait(t, deviceRef, commandList, &appData, sizeof(appData)))
		if (TestShaders_pullBuffer(t, deviceRef, emptyList, output)) {

			const U32 *values = (const U32*) DeviceBufferRef_ptr(output)->cpuData.ptr;

			F32 secondTime = 0;
			Buffer_memcpy(Buffer_createRef(&secondTime, sizeof(F32)), Buffer_createRefConst(&values[1], sizeof(U32)));

			Test_assert(t, "frameIdAdvanced", values[0] > firstFrameId);
			Test_assert(t, "timeMovesForward", secondTime >= firstTime);
		}

clean:

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pipeline);
	RefPtr_dec(&output);

	SHFile_free(&file, alloc);
}
