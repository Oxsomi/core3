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

//graphics/test/interface/test_graphics_bindful_table.cpp
//
//Bindful descriptor table mechanics: binding one at record time, switching between them, updating a
//live table, recycling its slots, and where the push descriptor boundary sits.
//Split out of test_graphics_bindful.c, which had grown to 24 modules in one file.

//The shared helpers in terms of the handle types. Both C++ headers come BEFORE the block below: a
//standard header included after the C headers landed in oxc::c finds its guard already tripped and
//leaves its symbols in that namespace.

#include "test_graphics_shared.hpp"

//Log::debugLn is the C++ front for Log_debugLnx; the x macros name ELogOptions_NewLine unqualified and
//so cannot be reached through the c namespace.

#include "types/container/log.hpp"

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

using namespace oxc;

//Same namespace the C headers landed in, so the definitions here match the declarations in
//test_graphics_shared.h and the macros in those headers still expand to names that resolve.

// -- 41. Bindful: descriptor table bound at record time ---------------------------

//Everything else in the suite reaches resources through the device's default bindless table; this is the
// first path where a pipeline brings its OWN layout and the heap and table are bound with commands.
//The layout is auto detected from the shader's reflection and the descriptor set by register name, so the
// test never restates what the shader already declares.
//The shader has a single classic register (u0), so the whole flow works on a device without bindless.

extern "C" void Test_graphicsBindful(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_write.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping bindful tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceBuffer buffer;
	gfx::CommandList commandList, emptyList;

	//The table holds no reference of its own, so its descriptor goes back before the buffer it names does.

	struct TableGuard {

		gfx::DescriptorTable &table;

		~TableGuard() {
			if(table)
				(void) table.unset(0, 0, 1, nullptr);
		}
	} tableGuard{ table };

	//The reflection knows the registers, so the layout comes from the shader rather than being restated

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(layoutInfo.list, "Bindful layout", layout, e_rr)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxBuffersRW = 1, .maxDescriptorTables = 1 };

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Bindful heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Bindful table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	//64 threads each write id * 3 + 7 as a U32, so the pull can prove the dispatch really ran bindful

	if(!Test_assert(t, "bufferCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Bindful output buffer", 64 * sizeof(c::U32), buffer, nullptr, e_rr
	)))
		return;

	const c::Descriptor desc = c::Descriptor_buffer(buffer.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setDescriptor", table.setByName("output", desc, 0, false, e_rr));

	//The pipeline brings its own layout rather than the device's default bindless one

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Bindful pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Bindful pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(2 * c::KIBI, 32, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	const c::Transition transition = {
		.resource = buffer.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
	};

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	//The work op is the validator: without a heap or table the dispatch has to be refused.
	//A refused work op invalidates its scope, which endScope hides wholesale; that is by design, so the real
	// dispatch lives in a scope of its own and rebinds its state (scope end resets pipeline and table binds).

	{
		gfx::CommandScope scope = commandList.scope({ transition }, 1, {}, e_rr);
		Test_assert(t, "scopeNeg", (c::Bool) scope);
		Test_assert(t, "bindPipelineNeg", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatchWithoutHeap", !scope.dispatch1D(1, nullptr));
		Test_assert(t, "bindHeapNeg", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "dispatchWithoutTable", !scope.dispatch1D(1, nullptr));

		//The recorders themselves refuse NULL and refs of the wrong type before any state changes.
		//A handle cannot BE null or hold the wrong kind of object, so these four keep the C entry points,
		// reached through the scope's own recorder rather than by leaving the RAII layer.

		Test_assert(t, "bindHeapNull", !c::CommandListRef_bindDescriptorHeap(scope.raw(), NULL, NULL));

		Test_assert(t, "bindHeapWrongType", !c::CommandListRef_bindDescriptorHeap(
			scope.raw(), buffer.handle(), NULL
		));

		Test_assert(t, "bindTableNull", !c::CommandListRef_bindDescriptorTable(scope.raw(), NULL, NULL));

		Test_assert(t, "bindTableWrongType", !c::CommandListRef_bindDescriptorTable(
			scope.raw(), heap.handle(), NULL
		));

		Test_assert(t, "scopeNegEnd", scope.end(e_rr));
	}

	//The real work; scope state (pipeline, heap and table binds) reset at endScope, so everything binds again

	{
		gfx::CommandScope scope = commandList.scope({ transition }, 2, {}, e_rr);
		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, buffer)) {

			const c::U32 *values = (const c::U32*) buffer.data()->cpuData.ptr;

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == i * 3 + 7;

			Test_assert(t, "bindfulResults", allMatch);
		}
}

// -- 42. Bindful, advanced: multi binding tables, table switching, wrong heap ----

//The copy shader reads t0 and writes u1, so one table carries an SRV and a UAV range together; two tables
// over the same layout then prove switching tables between dispatches, and a table from another heap proves
// the wrong heap negative. Runs everywhere, bindless included but never required.

extern "C" void Test_graphicsBindfulAdvanced(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/advanced");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_copy.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping advanced bindful tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap, otherHeap;
	gfx::DescriptorLayout layout, layoutTwin;
	gfx::DescriptorTable tableA, tableB, tableOther, tableTwin;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceBuffer src, dstA, dstB;
	gfx::CommandList commandList, emptyList;

	//Only the two real tables ever hold descriptors; the twin and the foreign one exist to be refused.

	struct TableGuard {

		gfx::DescriptorTable &a, &b;

		~TableGuard() {

			if (a) {
				(void) a.unset(0, 0, 1, nullptr);
				(void) a.unset(1, 0, 1, nullptr);
			}

			if (b) {
				(void) b.unset(0, 0, 1, nullptr);
				(void) b.unset(1, 0, 1, nullptr);
			}
		}
	} tableGuard{ tableA, tableB };

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc()), layoutTwinInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(
		layoutInfo.list, "Bindful advanced layout", layout, e_rr
	)))
		return;

	//Room for both real tables; the other heap exists only to prove its table can't bind under this one

	c::DescriptorHeapInfo heapInfo = { .maxBuffersRW = 6, .maxDescriptorTables = 3 };

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Bindful advanced heap", heap, e_rr)))
		return;

	c::DescriptorHeapInfo otherHeapInfo = { .maxBuffersRW = 2, .maxDescriptorTables = 1 };

	if(!Test_assert(t, "otherHeapCreate", dev.createDescriptorHeap(
		otherHeapInfo, "Bindful advanced other heap", otherHeap, e_rr
	)))
		return;

	if(!Test_assert(t, "tableACreate", heap.createTable(
		layout, "Bindful advanced table A", tableA, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	if(!Test_assert(t, "tableBCreate", heap.createTable(
		layout, "Bindful advanced table B", tableB, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	//A second detect yields a structurally IDENTICAL layout; the work ops must still refuse a table made
	// from it, because layout compatibility is exact object identity, not structural equality

	if(!Test_assert(t, "detectTwinLayout", dev.detectLayout(
		file.list, entryId, layoutTwinInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutTwinCreate", dev.createDescriptorLayout(
		layoutTwinInfo.list, "Bindful advanced twin layout", layoutTwin, e_rr
	)))
		return;

	if(!Test_assert(t, "tableTwinCreate", heap.createTable(
		layoutTwin, "Bindful advanced twin table", tableTwin, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	if(!Test_assert(t, "tableOtherCreate", otherHeap.createTable(
		layout, "Bindful advanced other table", tableOther, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	//src holds 0..63, the shader writes i * 2 + 1 through whichever table is bound

	c::U32 srcData[64];

	for(c::U32 i = 0; i < 64; ++i)
		srcData[i] = i;

	c::Buffer srcRef = c::Buffer_createRefConst(srcData, sizeof(srcData));

	if(!Test_assert(t, "srcCreate", dev.createBufferData(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
		"Bindful advanced src", &srcRef, src, nullptr, e_rr
	)))
		return;

	const c::EGraphicsResourceFlag writeBacked =
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked);

	if(!Test_assert(t, "dstACreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, writeBacked, "Bindful advanced dst A", 64 * sizeof(c::U32), dstA, nullptr, e_rr
	)))
		return;

	if(!Test_assert(t, "dstBCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, writeBacked, "Bindful advanced dst B", 64 * sizeof(c::U32), dstB, nullptr, e_rr
	)))
		return;

	const c::Descriptor srcDesc = c::Descriptor_buffer(src.handle(), 0, 0, NULL, 0);
	const c::Descriptor dstADesc = c::Descriptor_buffer(dstA.handle(), 0, 0, NULL, 0);
	const c::Descriptor dstBDesc = c::Descriptor_buffer(dstB.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setSrcA", tableA.setByName("input", srcDesc, 0, false, e_rr));
	Test_assert(t, "setDstA", tableA.setByName("output", dstADesc, 0, false, e_rr));
	Test_assert(t, "setSrcB", tableB.setByName("input", srcDesc, 0, false, e_rr));
	Test_assert(t, "setDstB", tableB.setByName("output", dstBDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Bindful advanced pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Bindful advanced pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	const c::Transition transitions[3] = {
		{ .resource = src.handle(),  .stage = c::EPipelineStage_Compute },
		{ .resource = dstA.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true },
		{ .resource = dstB.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
	};

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	//A table from another heap must be refused at the work op; its own scope so the refusal can't hide the
	// real work recorded below

	{
		gfx::CommandScope scope = commandList.scopeSpan(transitions, 3, 1, nullptr, 0, e_rr);
		Test_assert(t, "scopeNeg", (c::Bool) scope);
		Test_assert(t, "bindHeapNeg", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindOtherTable", scope.bindDescriptorTable(tableOther, e_rr));
		Test_assert(t, "bindPipelineNeg", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatchWrongHeap", !scope.dispatch1D(1, nullptr));
		Test_assert(t, "bindTwinTable", scope.bindDescriptorTable(tableTwin, e_rr));
		Test_assert(t, "dispatchTwinLayout", !scope.dispatch1D(1, nullptr));
		Test_assert(t, "scopeNegEnd", scope.end(e_rr));
	}

	//Two dispatches through two tables, switching between them without touching the heap or pipeline

	{
		gfx::CommandScope scope = commandList.scopeSpan(transitions, 3, 2, nullptr, 0, e_rr);
		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));

		Test_assert(t, "bindTableA", scope.bindDescriptorTable(tableA, e_rr));
		Test_assert(t, "dispatchA", scope.dispatch1D(1, e_rr));

		Test_assert(t, "bindTableB", scope.bindDescriptorTable(tableB, e_rr));
		Test_assert(t, "dispatchB", scope.dispatch1D(1, e_rr));

		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList)) {

		c::Bool okA = gfxtest::pullBuffer(t, dev, emptyList, dstA);
		c::Bool okB = gfxtest::pullBuffer(t, dev, emptyList, dstB);

		if (okA && okB) {

			const c::U32 *a = (const c::U32*) dstA.data()->cpuData.ptr;
			const c::U32 *b = (const c::U32*) dstB.data()->cpuData.ptr;

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &= a[i] == i * 2 + 1 && b[i] == i * 2 + 1;

			Test_assert(t, "bindfulSwitchResults", allMatch);
		}
	}
}

// -- 45. Bindful layout switch: two custom layouts in one scope ------------------

//Module 42 switches tables under ONE pipeline layout and the interleave module crosses custom to default;
// neither ever switches between two DIFFERENT custom layouts. On D3D12 that changes the root signature,
// which drops all root arguments, so the second dispatch only works if the lazy emission re-establishes
// everything. Two pipelines with structurally different layouts dispatch back to back; both outputs checked.

extern "C" void Test_graphicsBindfulLayoutSwitch(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/layoutSwitch");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	gfxtest::OwnedSHFile writeFile(dev.alloc()), copyFile(dev.alloc());

	if (
		!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_write.oiSH", writeFile.list) ||
		!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_copy.oiSH", copyFile.list)
	) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping layout switch tests");
		return;
	}

	const c::U32 writeId = gfxtest::entry(t, dev, writeFile.list, "main");
	const c::U32 copyId = gfxtest::entry(t, dev, copyFile.list, "main");

	if(writeId == c::U32_MAX || copyId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layoutW, layoutC;
	gfx::DescriptorTable tableW, tableC;
	gfx::PipelineLayout pipelineLayoutW, pipelineLayoutC;
	gfx::Pipeline pipelineW, pipelineC;
	gfx::DeviceBuffer bufW, src, bufC;
	gfx::CommandList commandList, emptyList;

	struct TableGuard {

		gfx::DescriptorTable &w, &c;

		~TableGuard() {

			if(w)
				(void) w.unset(0, 0, 1, nullptr);

			if (c) {
				(void) c.unset(0, 0, 1, nullptr);
				(void) c.unset(1, 0, 1, nullptr);
			}
		}
	} tableGuard{ tableW, tableC };

	gfxtest::OwnedLayoutInfo layoutWInfo(dev.alloc()), layoutCInfo(dev.alloc());

	if(!Test_assert(t, "detectLayoutW", dev.detectLayout(
		writeFile.list, writeId, layoutWInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "detectLayoutC", dev.detectLayout(
		copyFile.list, copyId, layoutCInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutWCreate", dev.createDescriptorLayout(
		layoutWInfo.list, "Layout switch write layout", layoutW, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCCreate", dev.createDescriptorLayout(
		layoutCInfo.list, "Layout switch copy layout", layoutC, e_rr
	)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxBuffersRW = 3, .maxDescriptorTables = 2 };

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Layout switch heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableWCreate", heap.createTable(
		layoutW, "Layout switch write table", tableW, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	if(!Test_assert(t, "tableCCreate", heap.createTable(
		layoutC, "Layout switch copy table", tableC, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	const c::EGraphicsResourceFlag writeBacked =
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked);

	if(!Test_assert(t, "bufWCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, writeBacked, "Layout switch write output", 64 * sizeof(c::U32),
		bufW, nullptr, e_rr
	)))
		return;

	c::U32 srcData[64];

	for(c::U32 i = 0; i < 64; ++i)
		srcData[i] = i;

	c::Buffer srcRef = c::Buffer_createRefConst(srcData, sizeof(srcData));

	if(!Test_assert(t, "srcCreate", dev.createBufferData(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
		"Layout switch src", &srcRef, src, nullptr, e_rr
	)))
		return;

	if(!Test_assert(t, "bufCCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, writeBacked, "Layout switch copy output", 64 * sizeof(c::U32),
		bufC, nullptr, e_rr
	)))
		return;

	const c::Descriptor bufWDesc = c::Descriptor_buffer(bufW.handle(), 0, 0, NULL, 0);
	const c::Descriptor srcDesc = c::Descriptor_buffer(src.handle(), 0, 0, NULL, 0);
	const c::Descriptor bufCDesc = c::Descriptor_buffer(bufC.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setBufW", tableW.setByName("output", bufWDesc, 0, false, e_rr));
	Test_assert(t, "setSrc", tableC.setByName("input", srcDesc, 0, false, e_rr));
	Test_assert(t, "setBufC", tableC.setByName("output", bufCDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutWInfo = { .bindings = layoutW.handle() };

	if(!Test_assert(t, "pipelineLayoutWCreate", dev.createPipelineLayout(
		pipelineLayoutWInfo, "Layout switch write pipeline layout", pipelineLayoutW, e_rr
	)))
		return;

	c::PipelineLayoutInfo pipelineLayoutCInfo = { .bindings = layoutC.handle() };

	if(!Test_assert(t, "pipelineLayoutCCreate", dev.createPipelineLayout(
		pipelineLayoutCInfo, "Layout switch copy pipeline layout", pipelineLayoutC, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineWCreate", dev.createComputePipeline(
		writeFile.list, "main", "Layout switch write pipeline", pipelineW, {}, &pipelineLayoutW, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCCreate", dev.createComputePipeline(
		copyFile.list, "main", "Layout switch copy pipeline", pipelineC, {}, &pipelineLayoutC, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	const c::Transition transitions[3] = {
		{ .resource = bufW.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true },
		{ .resource = src.handle(),  .stage = c::EPipelineStage_Compute },
		{ .resource = bufC.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
	};

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scopeSpan(transitions, 3, 1, nullptr, 0, e_rr);
		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));

		Test_assert(t, "bindTableW", scope.bindDescriptorTable(tableW, e_rr));
		Test_assert(t, "bindPipelineW", scope.setComputePipeline(pipelineW, e_rr));
		Test_assert(t, "dispatchW", scope.dispatch1D(1, e_rr));

		Test_assert(t, "bindTableC", scope.bindDescriptorTable(tableC, e_rr));
		Test_assert(t, "bindPipelineC", scope.setComputePipeline(pipelineC, e_rr));
		Test_assert(t, "dispatchC", scope.dispatch1D(1, e_rr));

		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList)) {

		c::Bool okW = gfxtest::pullBuffer(t, dev, emptyList, bufW);
		c::Bool okC = gfxtest::pullBuffer(t, dev, emptyList, bufC);

		if (okW && okC) {

			const c::U32 *w = (const c::U32*) bufW.data()->cpuData.ptr;
			const c::U32 *copied = (const c::U32*) bufC.data()->cpuData.ptr;

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &= w[i] == i * 3 + 7 && copied[i] == i * 2 + 1;

			Test_assert(t, "layoutSwitchResults", allMatch);
		}
	}
}

// -- 53. Updating a table between two submits of the same table ------------------

//Descriptor writes land in the backend immediately under a spinlock, with no frame fence of their own, and
// a non bindless Vulkan set has no update-after-bind semantics: rebinding a descriptor a submitted frame
// still references would be a use after free of the descriptor.
//What is legal is updating once that frame retired, which is exactly what this records: submit, wait, point
// the same binding of the same table at another buffer, submit again. Both results are checked, so a stale
// descriptor surviving the update shows up as the first buffer's values coming back twice.

extern "C" void Test_graphicsBindfulTableUpdate(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/tableUpdate");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_copy.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping table update tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceBuffer srcA, srcB, output;
	gfx::CommandList commandList, emptyList;

	struct TableGuard {

		gfx::DescriptorTable &table;

		~TableGuard() {

			if (table) {
				(void) table.unset(0, 0, 1, nullptr);
				(void) table.unset(1, 0, 1, nullptr);
			}
		}
	} tableGuard{ table };

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(layoutInfo.list, "Table update layout", layout, e_rr)))
		return;

	c::DescriptorHeapInfo heapInfo = { .maxBuffersRW = 3, .maxDescriptorTables = 1 };

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Table update heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Table update table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	//Two sources whose results can't be confused: A gives i * 2 + 1, B gives i * 2 + 1001

	c::U32 dataA[64], dataB[64];

	for(c::U32 i = 0; i < 64; ++i) {
		dataA[i] = i;
		dataB[i] = i + 500;
	}

	c::Buffer refA = c::Buffer_createRefConst(dataA, sizeof(dataA));

	if(!Test_assert(t, "srcACreate", dev.createBufferData(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
		"Table update src A", &refA, srcA, nullptr, e_rr
	)))
		return;

	c::Buffer refB = c::Buffer_createRefConst(dataB, sizeof(dataB));

	if(!Test_assert(t, "srcBCreate", dev.createBufferData(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
		"Table update src B", &refB, srcB, nullptr, e_rr
	)))
		return;

	if(!Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Table update output", 64 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	const c::Descriptor srcADesc = c::Descriptor_buffer(srcA.handle(), 0, 0, NULL, 0);
	const c::Descriptor srcBDesc = c::Descriptor_buffer(srcB.handle(), 0, 0, NULL, 0);
	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setSrcA", table.setByName("input", srcADesc, 0, false, e_rr));
	Test_assert(t, "setOutput", table.setByName("output", outputDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Table update pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Table update pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(2 * c::KIBI, 32, 16, commandList, true, e_rr)))
		return;

	if(!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)))
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	//Both sources are declared up front: transitions are recorded against concrete resources, and the second
	// submit replays the same list after the table points at the other one

	const c::Transition transitions[3] = {
		{ .resource = srcA.handle(), .stage = c::EPipelineStage_Compute },
		{ .resource = srcB.handle(), .stage = c::EPipelineStage_Compute },
		{ .resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
	};

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scopeSpan(transitions, 3, 1, nullptr, 0, e_rr);
		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));

	//First submit reads A; submitAndWait returns only once the frame retired, which is what makes the
	// descriptor update below legal

	if (gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == i * 2 + 1;

			Test_assert(t, "firstSubmitValues", allMatch);
		}

	//Same table, same binding, another buffer

	Test_assert(t, "updateSrc", table.setByName("input", srcBDesc, 0, true, e_rr));

	if (gfxtest::submitAndWait(t, dev, commandList))
		if (gfxtest::pullBuffer(t, dev, emptyList, output)) {

			const c::U32 *values = (const c::U32*) output.data()->cpuData.ptr;

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &= values[i] == (i + 500) * 2 + 1;

			Test_assert(t, "secondSubmitValues", allMatch);
		}
}

// -- 55. Descriptor table slot recycling ----------------------------------------

//A heap's table budget is a real allocation, not a counter: freeing a table has to hand its slot back on
// both backends (D3D12 frees the heap region it sub allocated, Vulkan frees the set back to its pool).
//Refusing to allocate past the budget is already covered; what is not is that the slot works again
// afterwards, so this fills the budget, frees, reallocates and then actually dispatches through the
// recycled table rather than only creating it.

extern "C" void Test_graphicsBindfulHeapRecycle(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/heapRecycle");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_write.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping heap recycle tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable tableFirst, tableOverBudget, tableRecycled;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceBuffer output;
	gfx::CommandList commandList, emptyList;

	struct TableGuard {

		gfx::DescriptorTable &table;

		~TableGuard() {
			if(table)
				(void) table.unset(0, 0, 1, nullptr);
		}
	} tableGuard{ tableRecycled };

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(layoutInfo.list, "Heap recycle layout", layout, e_rr)))
		return;

	//Room for exactly one table, so the second allocation has to be refused and the third can only succeed
	// by reusing what the first one gave back

	c::DescriptorHeapInfo heapInfo = { .maxBuffersRW = 1, .maxDescriptorTables = 1 };

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Heap recycle heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableFirstCreate", heap.createTable(
		layout, "Heap recycle first table", tableFirst, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	Test_assert(t, "tableOverBudgetRefused", !heap.createTable(
		layout, "Heap recycle over budget table", tableOverBudget, c::EDescriptorTableFlags_None, nullptr
	));

	Test_assert(t, "tableOverBudgetNull", !tableOverBudget);

	//Freeing a table that never held a descriptor keeps the debug build's leaked descriptor warning quiet,
	// which would otherwise fire for slots still set at free time

	tableFirst.release();

	if(!Test_assert(t, "tableRecycledCreate", heap.createTable(
		layout, "Heap recycle recycled table", tableRecycled, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	//Creating it is not enough: the recycled slot has to survive a real dispatch through it

	if(!Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Heap recycle output", 64 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setOutput", tableRecycled.setByName("output", outputDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Heap recycle pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Heap recycle pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

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
		Test_assert(t, "bindTable", scope.bindDescriptorTable(tableRecycled, e_rr));
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

			Test_assert(t, "recycledTableResults", allMatch);
		}
}

// -- 56. The push descriptor boundary -------------------------------------------

//Push descriptors and push constants are phase 2: no command writes them yet, so a pipeline whose layout
// declares push descriptors would read whatever the backend happened to leave behind. The work ops refuse
// it instead, and this pins that refusal so the day it becomes supported is a deliberate change here
// rather than a silent one.
//The refusal is checked with a heap and a table already bound, since the push descriptor check runs before
// the unbound heap check and would otherwise pass for the wrong reason.

extern "C" void Test_graphicsBindfulPushDescriptorBoundary(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Bindful/pushBoundary");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_spaces.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping push boundary tests");
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX)
		return;

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout, pushLayout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceBuffer output;
	gfx::CommandList commandList;

	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc()), pushInfo(dev.alloc());

	//Naming one register makes it a push descriptor while the other stays a normal binding, which is the
	// mixed layout the work ops have to refuse. The assume-all flag would leave no normal bindings at all.
	//This shader keeps its two registers in different spaces on purpose: Vulkan refuses a pipeline layout
	// whose push descriptors and normal bindings land in the same set.

	if(!Test_assert(t, "detectLayout", dev.detectLayout(
		file.list, entryId, layoutInfo.list, nullptr, nullptr, { "input" }, &pushInfo.list,
		c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
	)))
		return;

	if (!pushInfo.list.bindings.length || !layoutInfo.list.bindings.length) {
		c::Test_print(t, "Shader didn't split into push and normal bindings, skipping push boundary tests");
		return;
	}

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(layoutInfo.list, "Push boundary layout", layout, e_rr)))
		return;

	//A device without VK_KHR_push_descriptor refuses a caller owned push descriptor layout outright, which is
	// the documented gap rather than a failure of this module.
	//Android emulators are the usual case, since gfxstream drops the extension from the guest.

	if(!dev.createDescriptorLayout(pushInfo.list, "Push boundary push layout", pushLayout, nullptr)) {
		c::Test_print(t, "Device has no push descriptor support, skipping push boundary tests");
		return;
	}

	Test_assert(t, "pushLayoutCreate", pushLayout.valid());

	c::DescriptorHeapInfo heapInfo = { .maxBuffersRW = 2, .maxDescriptorTables = 1 };

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Push boundary heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Push boundary table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	if(!Test_assert(t, "outputCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
		"Push boundary output", 64 * sizeof(c::U32), output, nullptr, e_rr
	)))
		return;

	c::PipelineLayoutInfo pipelineLayoutInfo = {
		.bindings = layout.handle(), .pushDescriptors = pushLayout.handle()
	};

	if(!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
		pipelineLayoutInfo, "Push boundary pipeline layout", pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
		file.list, "main", "Push boundary pipeline", pipeline, {}, &pipelineLayout, e_rr
	)))
		return;

	if(!Test_assert(t, "listCreate", dev.createCommandList(2 * c::KIBI, 32, 16, commandList, true, e_rr)))
		return;

	const c::Transition transition = {
		.resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
	};

	//Everything a normal dispatch needs is in place, so the only thing left to refuse it is the layout's
	// push descriptors. The scope stays unsubmitted: a refused work op invalidates it and end() hides it.

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scope({ transition }, 1, {}, e_rr);
		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatchRefused", !scope.dispatch1D(1, nullptr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));
}
