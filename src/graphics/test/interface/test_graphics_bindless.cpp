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

//graphics/test/interface/test_graphics_bindless.cpp

//The bindless half of the descriptor tests (modules 12, 13): the device's default table and buffers
// exposing themselves into it. Devices without bindless skip with a message; the bindful twin of this
// coverage lives in test_graphics_bindful.c and runs everywhere.

//The shared helpers in terms of the handle types. This header comes BEFORE the block below: a standard
//header included after the C headers landed in oxc::c finds its guard already tripped and leaves its
//symbols in that namespace.

#include "test_graphics_shared.hpp"

namespace oxc { namespace c {
	#include "types/base/string_base.h"
	#include "types/test/test.h"
	#include "formats/oiSH/sh_file.h"
	#include "formats/oiSH/sh_registers.h"
	#include "platforms/platform.h"
	#include "graphics/generic/bindless_descriptor.h"
	#include "graphics/generic/command_list.h"
	#include "graphics/generic/commands.h"
	#include "graphics/generic/descriptor_heap.h"
	#include "graphics/generic/descriptor_layout.h"
	#include "graphics/generic/descriptor_table.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_buffer.h"
	#include "graphics/generic/device_info.h"
	#include "graphics/generic/graphics_types.h"
	#include "graphics/generic/pipeline.h"
	#include "graphics/generic/pipeline_layout.h"
	#include "test_graphics_shared.h"
} }

using namespace oxc;

//Same namespace the C headers landed in, so the definitions here match the declarations in
//test_graphics_shared.h and the macros in those headers still expand to names that resolve.

// -- 12. Bindless descriptors in the device's default table ----------------------

extern "C" void Test_graphicsBindlessDescriptor(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "BindlessDescriptor");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	if (!dev.hasBindlessTable()) {
		c::Test_print(t, "Device has no bindless descriptor table, skipping bindless descriptor tests");
		return;
	}

	gfx::DeviceBuffer buffer;

	//ShaderRead without ExposeBindlessRead, so the buffer takes no descriptor of its own and this test owns the one
	// it allocates below.

	if(!Test_assert(t, "bufferCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
		"Bindless descriptor test buffer", 256, buffer, nullptr, e_rr
	)))
		return;

	//The device level allocator is what this module is about, and a handle can express neither the null
	//device nor the null table these negatives need, so it is called through the C entry points throughout.

	const c::Descriptor desc = c::Descriptor_buffer(buffer.handle(), 0, 0, NULL, 0);
	c::BindlessDescriptor handle = c::BindlessDescriptor_None;

	//NULL as the table means the device's default one.

	Test_assert(t, "allocate", c::GraphicsDeviceRef_allocateDescriptorBindless(
		deviceRef, NULL, c::ESHRegisterType_ByteAddressBuffer, 0, false, &desc, &handle, &t->err
	));

	Test_assert(t, "handleNotNone", handle != c::BindlessDescriptor_None);
	Test_assert(t, "handleHasType", c::BindlessDescriptor_getBindlessType(handle) != 0);
	Test_assert(t, "handleValid", c::BindlessDescriptor_isValid(deviceRef, NULL, handle));

	//The default layout has at most 13 bindless arrays, so type 15 can never resolve to one.

	Test_assert(t, "handleTypeOutOfRange", !c::BindlessDescriptor_isValid(deviceRef, NULL, (c::BindlessDescriptor)15 << 17));
	Test_assert(t, "handleNoDevice", !c::BindlessDescriptor_isValid(NULL, NULL, handle));

	//Allocation validation; a descriptor is required and so is somewhere to put the handle.

	Test_assert(t, "allocateNoDevice", !c::GraphicsDeviceRef_allocateDescriptorBindless(
		NULL, NULL, c::ESHRegisterType_ByteAddressBuffer, 0, false, &desc, &handle, NULL
	));

	c::BindlessDescriptor unused = c::BindlessDescriptor_None;

	Test_assert(t, "allocateNoDescriptor", !c::GraphicsDeviceRef_allocateDescriptorBindless(
		deviceRef, NULL, c::ESHRegisterType_ByteAddressBuffer, 0, false, NULL, &unused, NULL
	));

	Test_assert(t, "allocateNothingLeaked", unused == c::BindlessDescriptor_None);

	//Freeing hands the slot back, so allocating the same descriptor again lands on it.

	Test_assert(t, "free", c::GraphicsDeviceRef_freeDescriptorBindless(deviceRef, NULL, handle, &t->err));

	c::BindlessDescriptor reused = c::BindlessDescriptor_None;

	Test_assert(t, "reallocate", c::GraphicsDeviceRef_allocateDescriptorBindless(
		deviceRef, NULL, c::ESHRegisterType_ByteAddressBuffer, 0, false, &desc, &reused, &t->err
	));

	Test_assert(t, "slotReused", reused == handle);

	//Freeing twice and freeing None are both no-ops rather than errors, which is what resource destructors rely on.

	Test_assert(t, "freeAgain", c::GraphicsDeviceRef_freeDescriptorBindless(deviceRef, NULL, reused, &t->err));
	Test_assert(t, "freeTwice", c::GraphicsDeviceRef_freeDescriptorBindless(deviceRef, NULL, reused, &t->err));

	Test_assert(t, "freeNone", c::GraphicsDeviceRef_freeDescriptorBindless(
		deviceRef, NULL, c::BindlessDescriptor_None, &t->err
	));
}

// -- 13. DeviceBuffer only takes a bindless descriptor when asked ----------------

extern "C" void Test_graphicsBufferBindless(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "DeviceBuffer/bindless");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	gfx::DeviceBuffer plain, readOnly, readWrite, rejected;

	//Without either Expose flag there's no descriptor and no table ref, whatever the device supports.

	Test_assert(t, "plainCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
		"Bindless flagless buffer", 256, plain, nullptr, e_rr
	));

	if (plain) {

		const c::DeviceBuffer *plainPtr = plain.data();

		Test_assert(t, "plainNoReadHandle", plainPtr->readHandle == c::BindlessDescriptor_None);
		Test_assert(t, "plainNoWriteHandle", plainPtr->writeHandle == c::BindlessDescriptor_None);
		Test_assert(t, "plainNoTable", !plainPtr->bindlessDescriptorTable);
	}

	if (!dev.hasBindlessTable()) {

		//Asking to be exposed on a device that has nowhere to expose it has to fail rather than silently do nothing.

		Test_assert(t, "exposeWithoutBindless", !dev.createBuffer(
			c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderReadBindless,
			"Bindless read buffer", 256, rejected, nullptr, nullptr
		));

		Test_assert(t, "exposeWithoutBindlessNothing", !rejected);
		c::Test_print(t, "Device has no bindless descriptor table, skipping exposed buffer tests");
		return;
	}

	//ExposeBindlessRead takes a read descriptor and nothing else.

	Test_assert(t, "readCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderReadBindless,
		"Bindless read buffer", 256, readOnly, nullptr, e_rr
	));

	if (readOnly) {

		const c::DeviceBuffer *readPtr = readOnly.data();

		Test_assert(t, "readHandle", readPtr->readHandle != c::BindlessDescriptor_None);
		Test_assert(t, "readNoWriteHandle", readPtr->writeHandle == c::BindlessDescriptor_None);
		Test_assert(t, "readDefaultTable", readPtr->bindlessDescriptorTable == dev.defaultTable().handle());
		Test_assert(t, "readHandleValid", c::BindlessDescriptor_isValid(deviceRef, NULL, readPtr->readHandle));
	}

	//Both Expose flags take two descriptors, one per binding, so they can't be the same handle.

	Test_assert(t, "rwCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRWBindless,
		"Bindless read write buffer", 256, readWrite, nullptr, e_rr
	));

	if (readWrite) {

		const c::DeviceBuffer *rwPtr = readWrite.data();

		Test_assert(t, "rwReadHandle", rwPtr->readHandle != c::BindlessDescriptor_None);
		Test_assert(t, "rwWriteHandle", rwPtr->writeHandle != c::BindlessDescriptor_None);
		Test_assert(t, "rwHandlesDiffer", rwPtr->readHandle != rwPtr->writeHandle);
		Test_assert(t, "rwWriteHandleValid", c::BindlessDescriptor_isValid(deviceRef, NULL, rwPtr->writeHandle));
	}

	//A table without a flag that says the resource may be exposed is a contradiction.

	const gfx::DescriptorTable defaultTable = dev.defaultTable();

	Test_assert(t, "tableWithoutExposeFlag", !dev.createBuffer(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_None,
		"Bindless read write buffer", 256, rejected, &defaultTable, nullptr
	));

	Test_assert(t, "tableWithoutExposeFlagNothing", !rejected);
}

// -- 14. Bindful and bindless interleaved in one scope ---------------------------

//Backends emit descriptor state lazily at the work op and remember what the command buffer last saw, so the
// dangerous transitions are custom to default (the default sets or root arguments have to come back) and
// default to custom right after. Three dispatches cross both transitions in a single scope; each writes its
// own buffer, so a stale table or root signature can't hide.
//Lives in this file rather than the bindful one because it NEEDS bindless, which that file must not.

extern "C" void Test_graphicsBindlessInterleave(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindless/interleave");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	if (!dev.hasBindlessTable()) {
		c::Test_print(t, "Device has no bindless descriptor table, skipping bindful interleave tests");
		return;
	}

	gfxtest::OwnedSHFile copyFile(dev.alloc()), writeFile(dev.alloc());

	if (
		!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_copy.oiSH", copyFile.list) ||
		!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_write.oiSH", writeFile.list)
	) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful interleave tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, copyFile.list, "main");

	if(entryId == c::U32_MAX)
		return;

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		copyFile.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	gfx::DescriptorLayout layout;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(
		layoutInfo.list, "Bindful interleave layout", layout, e_rr
	)))
		return;

	c::DescriptorHeapInfo heapInfo{};
	heapInfo.maxBuffersRW = 4;
	heapInfo.maxDescriptorTables = 2;

	gfx::DescriptorHeap heap;

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Bindful interleave heap", heap, e_rr)))
		return;

	gfx::DescriptorTable tableA, tableB;

	if(!Test_assert(t, "tableACreate", heap.createTable(
		layout, "Bindful interleave table A", tableA, (c::EDescriptorTableFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "tableBCreate", heap.createTable(
		layout, "Bindful interleave table B", tableB, (c::EDescriptorTableFlags) 0, e_rr
	)))
		return;

	c::U32 srcData[64];

	for(c::U32 i = 0; i < 64; ++i)
		srcData[i] = i;

	c::Buffer srcRef = c::Buffer_createRefConst(srcData, sizeof(srcData));

	gfx::DeviceBuffer src, dstA, dstB, bindlessOut;

	if(!Test_assert(t, "srcCreate", dev.createBufferData(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
		"Bindful interleave src", &srcRef, src, nullptr, e_rr
	)))
		return;

	const c::EGraphicsResourceFlag writeBacked =
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked);

	if(!Test_assert(t, "dstACreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, writeBacked, "Bindful interleave dst A", 64 * sizeof(c::U32), dstA, nullptr, e_rr
	)))
		return;

	if(!Test_assert(t, "dstBCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, writeBacked, "Bindful interleave dst B", 64 * sizeof(c::U32), dstB, nullptr, e_rr
	)))
		return;

	if(!Test_assert(t, "bindlessOutCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWriteBindless | c::EGraphicsResourceFlag_CPUBacked),
		"Bindful interleave bindless out", 64 * sizeof(c::U32), bindlessOut, nullptr, e_rr
	)))
		return;

	//Neither table holds a reference of its own, so their descriptors go back before the buffers do.

	struct TableGuard {

		gfx::DescriptorTable &a, &b;

		~TableGuard() {
			(void) a.unset(0, 0, 1, nullptr);
			(void) a.unset(1, 0, 1, nullptr);
			(void) b.unset(0, 0, 1, nullptr);
			(void) b.unset(1, 0, 1, nullptr);
		}
	} tableGuard{ tableA, tableB };

	const c::Descriptor srcDesc = c::Descriptor_buffer(src.handle(), 0, 0, NULL, 0);
	const c::Descriptor dstADesc = c::Descriptor_buffer(dstA.handle(), 0, 0, NULL, 0);
	const c::Descriptor dstBDesc = c::Descriptor_buffer(dstB.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setSrcA", tableA.setByName("input", srcDesc, 0, false, e_rr));
	Test_assert(t, "setDstA", tableA.setByName("output", dstADesc, 0, false, e_rr));
	Test_assert(t, "setSrcB", tableB.setByName("input", srcDesc, 0, false, e_rr));
	Test_assert(t, "setDstB", tableB.setByName("output", dstBDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.bindings = layout.handle();

	gfx::PipelineLayout pipelineLayout, bindlessLayout;

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Bindful interleave pipeline layout", pipelineLayout, e_rr
	)))
		return;

	gfx::Pipeline bindfulPipeline, bindlessPipeline;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		copyFile.list, "main", "Bindful interleave pipeline", bindfulPipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	if(!gfxtest::computePipelinePush(t, dev, writeFile.list, bindlessPipeline, bindlessLayout))
		return;

	gfx::CommandList commandList, emptyList;

	if(!Test_assert(t, "listCreate", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	const c::Transition transitions[4] = {
		{ .resource = src.handle(),         .stage = c::EPipelineStage_Compute },
		{ .resource = dstA.handle(),        .stage = c::EPipelineStage_Compute, .isWrite = true },
		{ .resource = dstB.handle(),        .stage = c::EPipelineStage_Compute, .isWrite = true },
		{ .resource = bindlessOut.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
	};

	//Custom, then custom again, then default, all in one scope.
	//
	//The bindless dispatch goes last because its shader reads push constants, and those belong to the
	//pipeline layout that declares them: binding a layout without them afterwards leaves state the work op
	//has no way to validate, so it refuses. Ordering it last is the whole of the accommodation; the scope
	//still crosses between a custom layout and the device's default one.

	//The handles the shader reads, in the order its push block declares them.

	const c::U32 bindlessPushData[4] = { bindlessOut.writeHandle(), 0xC0DE0000u, 0, 0 };

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scope(
			{ transitions[0], transitions[1], transitions[2], transitions[3] }, 1, {}, e_rr
		);

		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));

		Test_assert(t, "bindTableA", scope.bindDescriptorTable(tableA, e_rr));
		Test_assert(t, "bindBindful", scope.setComputePipeline(bindfulPipeline, e_rr));
		Test_assert(t, "dispatchA", scope.dispatch1D(1, e_rr));

		Test_assert(t, "bindTableB", scope.bindDescriptorTable(tableB, e_rr));
		Test_assert(t, "bindBindful2", scope.setComputePipeline(bindfulPipeline, e_rr));
		Test_assert(t, "dispatchB", scope.dispatch1D(1, e_rr));

		Test_assert(t, "bindBindless", scope.setComputePipeline(bindlessPipeline, e_rr));
		Test_assert(t, "pushBindless", scope.setPushConstants(bindlessPushData, e_rr));
		Test_assert(t, "dispatchBindless", scope.dispatch1D(1, e_rr));

		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList)) {

		c::Bool okA = gfxtest::pullBuffer(t, dev, emptyList, dstA);
		c::Bool okOut = gfxtest::pullBuffer(t, dev, emptyList, bindlessOut);
		c::Bool okB = gfxtest::pullBuffer(t, dev, emptyList, dstB);

		if (okA && okOut && okB) {

			const c::U32 *a = (const c::U32*) dstA.data()->cpuData.ptr;
			const c::U32 *o = (const c::U32*) bindlessOut.data()->cpuData.ptr;
			const c::U32 *b = (const c::U32*) dstB.data()->cpuData.ptr;

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &= a[i] == i * 2 + 1 && o[i] == 0xC0DE0000u + i && b[i] == i * 2 + 1;

			Test_assert(t, "interleaveResults", allMatch);
		}
	}
}

// -- 15. A custom layout that opts every binding into bindless updates ----------

//AllowBindlessEverywhere drops the array-only restriction and treats every register as dynamic, which is a
//distinct backend shape rather than a hint: Vulkan adds UPDATE_AFTER_BIND and partially bound flags to
//bindings that would not otherwise carry them, and D3D12 relaxes the descriptor volatility flags on the
//range. Tests only ever used AllowBindlessOnArrays, so this shape was created but never executed.
//It requires the bindless feature even though the layout is a custom one, which is why it lives here.

extern "C" void Test_graphicsBindlessEverywhere(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindless/everywhere");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	if (!(dev.info().capabilities.features & c::EGraphicsFeatures_Bindless)) {
		c::Test_print(t, "Device has no bindless, skipping bindless everywhere tests");
		return;
	}

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_write.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindless everywhere tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	//The flag rides on the layout, so detection is asked for it rather than the layout being hand built

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_AllowBindlessEverywhere, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	gfx::DescriptorLayout layout;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(
		layoutInfo.list, "Bindless everywhere layout", layout, e_rr
	)))
		return;

	c::DescriptorHeapInfo heapInfo{};
	heapInfo.flags = c::EDescriptorHeapFlags_AllowBindless;
	heapInfo.maxBuffersRW = 1;
	heapInfo.maxDescriptorTables = 1;

	gfx::DescriptorHeap heap;

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Bindless everywhere heap", heap, e_rr)))
		return;

	gfx::DescriptorTable table;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Bindless everywhere table", table, (c::EDescriptorTableFlags) 0, e_rr
	)))
		return;

	gfx::DeviceBuffer output;

	if(!Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Bindless everywhere output", 64 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	//The table holds no reference of its own, so the descriptor goes back before the buffer does.

	struct TableGuard {
		gfx::DescriptorTable &table;
		~TableGuard() { (void) table.unset(0, 0, 1, nullptr); }
	} tableGuard{ table };

	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setOutput", table.setByName("output", outputDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.bindings = layout.handle();

	gfx::PipelineLayout pipelineLayout;

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Bindless everywhere pipeline layout", pipelineLayout, e_rr
	)))
		return;

	gfx::Pipeline pipeline;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Bindless everywhere pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	gfx::CommandList commandList, emptyList;

	if(!Test_assert(t, "listCreate", dev.createCommandList(2 * c::KIBI, 32, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	const c::Transition transition = {
		.resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
	};

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scope({ transition }, 1, {}, e_rr);
		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == i * 3 + 7;

			Test_assert(t, "everywhereResults", allMatch);
		}
}

// -- 16. The per frame globals ------------------------------

//Frame id, time, delta and swapchain count are the whole of the globals block now, and none of them had
//execution coverage at all, which is exactly what a change to where the block lives can break without a
//single test noticing.
//Two submits, because the interesting property is that these move: the frame id has to advance and the
//clock must not run backwards.

extern "C" void Test_graphicsFrameGlobals(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindless/frameGlobals");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	if (!dev.hasBindlessTable()) {
		c::Test_print(t, "Device has no bindless descriptor table, skipping frame globals tests");
		return;
	}

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_frame_globals.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping frame globals tests");
		return;
	}

	gfx::DeviceBuffer output;

	if(!Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWriteBindless | c::EGraphicsResourceFlag_CPUBacked),
		"Frame globals output", 4 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	//On DXIL a push constant reflects as the implicit $Globals cbuffer, so it can't be found by name; Assume
	// is unambiguous here because detect skips the reserved space, where the globals block lives.

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());
	c::DescriptorBinding pushConstants{};

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, &pushConstants, {}, nullptr,
		c::EDescriptorLayoutFlags_None, c::EDetectDescriptorLayoutFlags_AssumePushConstants, e_rr
	)))
		return;

	//Everything else this shader touches is the runtime's own bindless set, which detect skips.

	Test_assert(t, "noOrdinaryBindings", !layoutInfo.list.bindings.length);

	if(!Test_assert(t, "detectedPushConstants", pushConstants.count != 0))
		return;

	//The output handle rides in a push constant, so this pipeline can't use the device's default layout as
	// is: it needs the same bindless bindings and globals push descriptor WITH a push constant range on top,
	// which is what composing one from the device's own layouts gives.

	c::PipelineLayoutInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.bindings = dev.defaultDescLayout();
	pipelineLayoutInfo.pushDescriptors = dev.defaultCBufferLayout();
	pipelineLayoutInfo.pushConstants = pushConstants;

	gfx::PipelineLayout pipelineLayout;

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Frame globals pipeline layout", pipelineLayout, e_rr
	)))
		return;

	gfx::Pipeline pipeline;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Frame globals pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	gfx::CommandList commandList, emptyList;

	if(!Test_assert(t, "listCreate", dev.createCommandList(2 * c::KIBI, 32, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	const c::Transition transition = {
		.resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
	};

	//The write handle travels as a push constant.

	const c::U32 pushData[4] = { output.writeHandle(), 0, 0, 0 };

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scope({ transition }, 1, {}, e_rr);
		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "setPushConstants", scope.setPushConstants(pushData, e_rr));
		Test_assert(t, "dispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	c::U32 firstFrameId = 0;
	c::F32 firstTime = 0;
	c::Bool readFirst = false;

	if (gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			firstFrameId = values[0];
			c::Buffer_memcpy(
				c::Buffer_createRef(&firstTime, sizeof(c::F32)),
				c::Buffer_createRefConst(&values[1], sizeof(c::U32))
			);

			c::F32 deltaTime = 0;
			c::Buffer_memcpy(
				c::Buffer_createRef(&deltaTime, sizeof(c::F32)),
				c::Buffer_createRefConst(&values[2], sizeof(c::U32))
			);

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

	if (readFirst && gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			c::F32 secondTime = 0;
			c::Buffer_memcpy(
				c::Buffer_createRef(&secondTime, sizeof(c::F32)),
				c::Buffer_createRefConst(&values[1], sizeof(c::U32))
			);

			Test_assert(t, "frameIdAdvanced", values[0] > firstFrameId);
			Test_assert(t, "timeMovesForward", secondTime >= firstTime);
		}
}
