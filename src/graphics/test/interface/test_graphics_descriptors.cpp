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

//graphics/test/interface/test_graphics_descriptors.cpp
//
//Descriptor layout and table modules (11, 40): the API neutral descriptor machinery.
//Written against the C++ layer (graphics/graphics.hpp) rather than the C API.
//Same coverage as the C module it replaces, minus the teardown: every handle here releases itself, so there
//is no clean label, no goto chain and no ordered list of RefPtr_dec calls to keep in step with the locals.
//What is left is the test.
//Bindless lives in test_graphics_bindless.c, bindful in test_graphics_bindful.c.
//These need a live device and are called once per adapter from the device test loop.
//Modules 7 to 10 live in Test_graphicsDeviceForApi (test_graphics_interface.c), which brings up the device
//the rest of these run against.

#include "graphics/graphics.hpp"

namespace oxc { namespace c {
	#include "test_graphics_shared.h"
}}

namespace {

	//DescriptorBinding is a plain C struct with no wrapper and every binding these modules declare is compute
	//visible, so one maker keeps each declaration to a line rather than four assignments.

	oxc::c::DescriptorBinding Test_makeBinding(
		oxc::c::ESHRegisterType registerType, oxc::c::U32 count, oxc::c::U32 space, oxc::c::U32 binding
	) noexcept {

		oxc::c::DescriptorBinding out{};
		out.registerType = registerType;
		out.count = count;
		out.binding.space = space;
		out.binding.binding = binding;
		out.visibility = 1 << oxc::c::ESHPipelineStage_Compute;
		return out;
	}

	//Allocates until the binding is full, returning a bitmask of the slots it got.
	//A slot that repeats or lands outside the array stops the fill, so a full mask is the only way to pass.

	oxc::c::U64 Test_fillDescriptorBinding(
		oxc::c::Test *t,
		oxc::gfx::DescriptorTable &table,
		const oxc::c::Descriptor &desc,
		oxc::c::Bool maintainRef,
		oxc::c::U64 count
	) noexcept {

		oxc::c::U64 taken = 0;

		for (oxc::c::U64 i = 0; i < count; ++i) {

			oxc::c::U64 arrayId = oxc::c::U64_MAX;

			if(!table.alloc(0, arrayId, desc, maintainRef, &t->err))
				break;

			if(arrayId >= count || ((taken >> arrayId) & 1))
				break;

			taken |= (oxc::c::U64)1 << arrayId;
		}

		return taken;
	}
}

// -- 11. DescriptorLayout / DescriptorTable --------------------------------------

//Builds a small explicit layout, a heap sized for it and a table on top of both, then checks the invariants the
// headers promise before tearing all of it back down.
//The layout is bindless where the device has the feature and bindful where it doesn't, so whichever adapter CI
// hands us exercises one of the two paths.

extern "C" void Test_graphicsDescriptorTable(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Error *e_rr = &t->err;

	Test_setModule(t, "DescriptorLayout");

	//The harness owns this ref, so it is borrowed rather than adopted.

	Device dev = Device::share(deviceRef);

	const c::Bool hasBindless = (dev.info().capabilities.features & c::EGraphicsFeatures_Bindless) != 0;

	//Declared in the order the destructors have to unwind in reverse.
	//The table reads the layout while it's being freed but never took a ref on it, so it has to go first.

	DescriptorHeap heap;
	DescriptorLayout layout;
	DescriptorTable table;
	DeviceBuffer buffer;

	//Two arrays and one single descriptor, all byte address buffers so no texture format rules come into play.
	//SPIRV wants unique binding ids within a set and DXIL wants non overlapping ranges per register type;
	// t0-t3, u4-u7 and t8 satisfies both, so the test doesn't have to know which api it's running on.

	c::CharString bindingNames[3] = { name("testBuffers"), name("testRWBuffers"), name("testBuffer") };

	c::DescriptorBinding bindings[3] = {
		Test_makeBinding(c::ESHRegisterType_ByteAddressBuffer, 4, 0, 0),
		Test_makeBinding((c::ESHRegisterType)(c::ESHRegisterType_ByteAddressBuffer | c::ESHRegisterType_IsWrite), 4, 0, 4),
		Test_makeBinding(c::ESHRegisterType_ByteAddressBuffer, 1, 0, 8)
	};

	const c::CharString layoutName = name("Test descriptor layout");

	//Parameter validation.
	//Every info here refs stack memory, so a rejected create has nothing to leak.

	c::DescriptorLayoutInfo info{};
	info.flags = hasBindless ? c::EDescriptorLayoutFlags_AllowBindlessOnArrays : c::EDescriptorLayoutFlags_None;

	Test_assert(t, "bindingsRef", c::ListDescriptorBinding_createRefConst(bindings, 3, &info.bindings, e_rr));
	Test_assert(t, "namesRef", c::ListCharString_createRefConst(bindingNames, 3, &info.bindingNames, e_rr));

	//A null device, a null info or a null out parameter has no wrapper spelling by design, so these three stay
	// on the C entry point; everything below that only has a bad info goes through the wrapper.

	c::DescriptorLayoutRef *badLayout = nullptr;

	Test_assert(t, "layoutNullDevice", !c::GraphicsDeviceRef_createDescriptorLayout(
		nullptr, &info, &layoutName, &badLayout, nullptr
	));

	Test_assert(t, "layoutNullInfo", !c::GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, nullptr, &layoutName, &badLayout, nullptr
	));

	Test_assert(t, "layoutNullOut", !c::GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &info, &layoutName, nullptr, nullptr
	));

	//A constant buffer has to declare its size and push constants belong in a pipeline layout, not a descriptor layout.

	c::DescriptorBinding sizelessCBuffer = Test_makeBinding(c::ESHRegisterType_ConstantBuffer, 1, 0, 0);

	c::DescriptorBinding pushConstant = Test_makeBinding(c::ESHRegisterType_PushConstants, 1, 0, 0);
	pushConstant.constantBufferSize = 16;

	c::DescriptorLayoutInfo cbufferInfo{};
	c::DescriptorLayoutInfo pushInfo{};

	Test_assert(t, "cbufferRef", c::ListDescriptorBinding_createRefConst(&sizelessCBuffer, 1, &cbufferInfo.bindings, e_rr));

	Test_assert(t, "pushRef", c::ListDescriptorBinding_createRefConst(&pushConstant, 1, &pushInfo.bindings, e_rr));

	DescriptorLayout rejected;

	Test_assert(t, "layoutSizelessCBuffer", !dev.createDescriptorLayout(cbufferInfo, "Test descriptor layout", rejected));
	Test_assert(t, "layoutPushConstants", !dev.createDescriptorLayout(pushInfo, "Test descriptor layout", rejected));

	//Bindless and push descriptors exclude each other.
	//On a device without bindless the flag on its own is already illegal, so this is rejected either way.

	c::DescriptorLayoutInfo mixedInfo{};

	mixedInfo.flags = (c::EDescriptorLayoutFlags)(
		c::EDescriptorLayoutFlags_AllowBindlessOnArrays | c::EDescriptorLayoutFlags_HasPushDescriptors
	);

	Test_assert(t, "mixedRef", c::ListDescriptorBinding_createRefConst(bindings, 3, &mixedInfo.bindings, e_rr));

	Test_assert(t, "layoutBindlessPushDescriptors", !dev.createDescriptorLayout(mixedInfo, "Test descriptor layout", rejected));

	Test_assert(t, "layoutRejectedNothing", !badLayout && !rejected);

	//A heap of its own, sized for exactly the 9 buffer descriptors the layout declares.

	c::DescriptorHeapInfo heapInfo{};
	heapInfo.flags = hasBindless ? c::EDescriptorHeapFlags_AllowBindless : c::EDescriptorHeapFlags_None;
	heapInfo.maxBuffersRW = 9;
	heapInfo.maxDescriptorTables = 1;

	c::DescriptorHeapInfo emptyHeapInfo{};
	emptyHeapInfo.maxDescriptorTables = 1;

	const c::CharString heapName = name("Test descriptor heap");
	c::DescriptorHeapRef *badHeap = nullptr;

	Test_assert(
		t, "heapNullDevice", !c::GraphicsDeviceRef_createDescriptorHeap(nullptr, &heapInfo, &heapName, &badHeap, nullptr)
	);

	Test_assert(
		t, "heapNullInfo", !c::GraphicsDeviceRef_createDescriptorHeap(deviceRef, nullptr, &heapName, &badHeap, nullptr)
	);

	Test_assert(t, "heapNoDescriptors", !dev.createDescriptorHeap(emptyHeapInfo, "Test descriptor heap", heap));

	if(!Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Test descriptor heap", heap, e_rr)))
		return;

	//Real layout create; the info is moved into the layout, which is what the emptied lists below check.

	if(!Test_assert(t, "layoutCreate", dev.createDescriptorLayout(info, "Test descriptor layout", layout, e_rr)))
		return;

	Test_assert(t, "layoutTypeId", layout.handle()->refPtrType->typeId == (c::TypeId) c::EGraphicsTypeId_DescriptorLayout);
	Test_assert(t, "layoutInfoMoved", !info.bindings.ptr && !info.bindingNames.ptr);

	const c::DescriptorLayout *layoutPtr = layout.data();

	Test_assert(t, "layoutDevice", layoutPtr->device == deviceRef);
	Test_assert(t, "layoutBindingCount", layoutPtr->info.bindings.length == 3);
	Test_assert(t, "layoutNameCount", layoutPtr->info.bindingNames.length == 3);
	Test_assert(t, "layoutOwnsBindings", !c::ListDescriptorBinding_isRef(layoutPtr->info.bindings));
	Test_assert(t, "layoutArrayCount", layoutPtr->info.bindings.ptr[0].count == 4);
	Test_assert(t, "layoutSingleCount", layoutPtr->info.bindings.ptr[2].count == 1);
	Test_assert(t, "layoutAnyResource", layoutPtr->anyResource);
	Test_assert(t, "layoutNoSampler", !layoutPtr->anySampler);

	//Only arrays become bindless types, and there can be at most 15 of them because of BindlessDescriptor's layout.
	//Without the feature the layout is bindful, so neither mapping exists at all.

	if (hasBindless) {

		if(Test_assert(t, "bindlessTypeCount", layoutPtr->bindlessTypeToBinding.length == 2)) {
			Test_assert(t, "bindlessType0", !layoutPtr->bindlessTypeToBinding.ptr[0]);
			Test_assert(t, "bindlessType1", layoutPtr->bindlessTypeToBinding.ptr[1] == 1);
		}

		if(Test_assert(t, "bindlessMapCount", layoutPtr->bindingToBindlessType.length == 3)) {
			Test_assert(t, "bindlessMap0", !layoutPtr->bindingToBindlessType.ptr[0]);
			Test_assert(t, "bindlessMap1", layoutPtr->bindingToBindlessType.ptr[1] == 1);
			Test_assert(t, "bindlessMapNonArray", layoutPtr->bindingToBindlessType.ptr[2] == c::U8_MAX);
		}
	}

	else {
		Test_assert(t, "noBindlessTypes", !layoutPtr->bindlessTypeToBinding.length);
		Test_assert(t, "noBindlessMap", !layoutPtr->bindingToBindlessType.length);
	}

	//Table create + validation.

	Test_setModule(t, "DescriptorTable");

	const c::CharString tableName = name("Test descriptor table");
	c::DescriptorTableRef *badTable = nullptr;

	Test_assert(t, "tableNullParent", !c::DescriptorHeapRef_createDescriptorTable(
		nullptr, layout.handle(), c::EDescriptorTableFlags_None, &tableName, &badTable, nullptr
	));

	Test_assert(t, "tableNullLayout", !c::DescriptorHeapRef_createDescriptorTable(
		heap.handle(), nullptr, c::EDescriptorTableFlags_None, &tableName, &badTable, nullptr
	));

	if(!Test_assert(t, "tableCreate", heap.createTable(
		layout, "Test descriptor table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	Test_assert(t, "tableTypeId", table.handle()->refPtrType->typeId == (c::TypeId) c::EGraphicsTypeId_DescriptorTable);

	const c::DescriptorTable *tablePtr = table.data();

	Test_assert(t, "tableParent", tablePtr->parent == heap.handle());
	Test_assert(t, "tableLayout", tablePtr->layout == layout.handle());
	Test_assert(t, "tableBindingCount", tablePtr->bindings.length == 3);
	Test_assert(t, "tableNoResources", !tablePtr->resources.length);

	//The heap was created with room for a single table, which is what maxDescriptorTables means.

	DescriptorTable secondTable;

	Test_assert(t, "tableOutOfSlots", !heap.createTable(layout, "Test descriptor table", secondTable));
	Test_assert(t, "tableRejectedNothing", !badTable && !secondTable);

	//Register names resolve to their binding index, anything else doesn't resolve at all.

	Test_assert(t, "resolveFirst", !table.resolveRegisterName("testBuffers"));
	Test_assert(t, "resolveLast", table.resolveRegisterName("testBuffer") == 2);
	Test_assert(t, "resolveMissing", table.resolveRegisterName("notARegister") == c::U64_MAX);

	//A buffer to point descriptors at; the read only bindings above require ShaderRead on the resource.

	if(!Test_assert(t, "bufferCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead, "Test descriptor table buffer", 256, buffer, nullptr,
		e_rr
	)))
		return;

	const c::Descriptor desc = c::Descriptor_buffer(buffer.handle(), 0, 0, nullptr, 0);
	c::U64 arrayId = c::U64_MAX;

	//Allocating picks a free slot in an array, so a binding with a count of 1 has nothing to pick from.

	Test_assert(t, "allocOnNonArray", !table.alloc(2, arrayId, desc));
	Test_assert(t, "allocOutOfBounds", !table.alloc(3, arrayId, desc));

	//The wrapper hands the slot back through a reference, so a null out parameter is another one that only the
	// C entry point can spell.

	Test_assert(
		t, "allocNullArrayId", !c::DescriptorTableRef_allocDescriptor(table.handle(), 0, nullptr, false, &desc, nullptr)
	);

	//That single descriptor is set through its binding index instead.
	//maintainRef stays false, so the table doesn't take ownership of the resource.

	Test_assert(t, "setSingle", table.set(2, 0, desc, false, e_rr));
	Test_assert(t, "setSingleNoRef", !tablePtr->resources.length);
	Test_assert(t, "setSingleOOB", !table.set(2, 1, desc));
	Test_assert(t, "unsetSingle", table.unset(2, 0, 1, e_rr));

	//The plural setter, which nothing else in the suite reaches directly: setDescriptor and the by name forms
	// all funnel through it with a length fixed at 1, so its range handling was never exercised.
	//Binding 0 is the count 4 array, so arrayId 1 with 2 descriptors fits and arrayId 3 with 2 runs off the end.

	Test_assert(t, "setMany", table.setRange(0, 1, { desc, desc }, false, e_rr));
	Test_assert(t, "setManyNoRef", !tablePtr->resources.length);
	Test_assert(t, "setManyOOB", !table.setRange(0, 3, { desc, desc }));
	Test_assert(t, "unsetMany", table.unset(0, 1, 2, e_rr));

	//An empty list has nothing to bind and has to be refused on both binding shapes.
	//The array case is the one that mattered: it used to report success while binding nothing, because the
	// comparison loop simply ran zero times.
	//The single case indexed element 0 of a list that has none.
	//Spelled out rather than written as {}, so it is the plural overload that is picked here and not the
	// single descriptor one.

	Test_assert(t, "setNoneSingle", !table.setRange(2, 0, {}));
	Test_assert(t, "setNoneArray", !table.setRange(0, 0, {}));

	//The by name variants resolve the register name first, so they're the same operations routed differently
	// and an unknown name has to be refused everywhere rather than defaulting to binding 0.

	Test_assert(t, "setByName", table.setByName("testBuffer", desc, 0, false, e_rr));
	Test_assert(t, "unsetByName", table.unsetByName("testBuffer", 0, 1, e_rr));
	Test_assert(t, "setByNameMissing", !table.setByName("notARegister", desc));

	//Named for the same reason the empty list above is: a one element brace would pick the single descriptor
	// overload rather than the plural one.

	Test_assert(t, "setManyByName", table.setRangeByName("testBuffers", { desc }, 0, false, e_rr));
	Test_assert(t, "unsetManyByName", table.unsetByName("testBuffers", 0, 1, e_rr));
	Test_assert(t, "unsetByNameMissing", !table.unsetByName("notARegister", 0, 1));

	Test_assert(t, "allocByName", table.allocByName("testBuffers", arrayId, desc, false, e_rr));
	Test_assert(t, "allocByNameId", arrayId < 4);
	Test_assert(t, "unsetAllocByName", table.unsetByName("testBuffers", arrayId, 1, e_rr));
	Test_assert(t, "allocByNameNonArray", !table.allocByName("testBuffer", arrayId, desc));
	Test_assert(t, "allocByNameMissing", !table.allocByName("notARegister", arrayId, desc));

	if (hasBindless) {

		//A bindless allocation finds the array whose register type matches, which is the first binding here.

		c::U16 bindId = c::U16_MAX;
		c::U8 bindlessTypeId = c::U8_MAX;

		Test_assert(t, "allocBindless", table.allocBindless(
			c::ESHRegisterType_ByteAddressBuffer, 0, bindId, bindlessTypeId, arrayId, desc, false, e_rr
		));

		Test_assert(t, "allocBindlessBinding", !bindId);
		Test_assert(t, "allocBindlessType", !bindlessTypeId);
		Test_assert(t, "allocBindlessArrayId", !arrayId);
		Test_assert(t, "unsetBindless", table.unset(bindId, arrayId, 1, e_rr));

		//findBindlessRegister answers "where would a resource like this go", so the type has to match a binding

		c::U16 foundBind = c::U16_MAX;
		c::U8 foundType = c::U8_MAX;

		Test_assert(t, "findRegister", table.findBindlessRegister(
			c::ESHRegisterType_ByteAddressBuffer, 0, foundBind, foundType, buffer.handle(), 0, e_rr
		));

		Test_assert(t, "findRegisterBinding", !foundBind && !foundType);

		Test_assert(t, "findRegisterMissing", !table.findBindlessRegister(
			c::ESHRegisterType_Sampler, 0, foundBind, foundType, buffer.handle()
		));

		//Resources can be routed into a table of the caller's own, which is what every creator's trailing
		// bindlessTable parameter is for; leaving it null routes into the device's own table instead.

		DeviceBuffer routed;

		Test_assert(t, "routedCreate", dev.createBuffer(
			c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderReadBindless, "Test routed buffer", 256,
			routed, &table, e_rr
		));

		if (routed) {

			Test_assert(t, "routedTable", routed.data()->bindlessDescriptorTable == table.handle());
			Test_assert(t, "routedHandle", routed.readHandle() != c::BindlessDescriptor_None);
			Test_assert(t, "routedValid", c::BindlessDescriptor_isValid(deviceRef, table.handle(), routed.readHandle()));
			Test_assert(t, "routedNoWriteHandle", routed.writeHandle() == c::BindlessDescriptor_None);
		}
	}
}

// -- 40. Descriptor allocation: filling a binding, recycling, resources in flight -

//Every descriptor test above allocates a single slot and hands it straight back, so the allocator was only ever
// asked for slot 0 of an array.
//This fills a binding to its declared count instead, which is what real code does the moment a scene holds more
// than a couple of exposed buffers or acceleration structures.
//It runs in two halves, because "the slot came back" and "the slot came back while the gpu still held the
// resource" are separate paths: the first frees descriptors nothing ever touched, the second frees descriptors
// the table owns a reference through and whose buffer is in flight in a submitted command list.

extern "C" void Test_graphicsDescriptorAlloc(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Error *e_rr = &t->err;

	Test_setModule(t, "DescriptorTable/alloc");

	//The harness owns this ref, so it is borrowed rather than adopted.

	Device dev = Device::share(deviceRef);

	//Declared in the order the destructors have to unwind in reverse, the buffer before the table that keeps a
	// reference to it.

	DescriptorHeap heap;
	DescriptorLayout layout;
	DescriptorTable table;
	DeviceBuffer buffer;
	RenderTexture target;
	CommandList commandList;

	//One array of 8, small enough to fill exhaustively and big enough that the bitset behind it spans more than
	// the handful of slots the rest of the suite ever touches.
	//Bindless is deliberately left out of both the layout and the heap, so this runs the same on every adapter.

	const c::U64 count = 8;

	c::CharString bindingName = name("allocBuffers");
	c::DescriptorBinding binding = Test_makeBinding(c::ESHRegisterType_ByteAddressBuffer, 8, 0, 0);

	c::DescriptorLayoutInfo info{};

	Test_assert(t, "allocBindingRef", c::ListDescriptorBinding_createRefConst(&binding, 1, &info.bindings, e_rr));
	Test_assert(t, "allocNameRef", c::ListCharString_createRefConst(&bindingName, 1, &info.bindingNames, e_rr));

	c::DescriptorHeapInfo heapInfo{};
	heapInfo.maxBuffersRW = 8;
	heapInfo.maxDescriptorTables = 1;

	if(!Test_assert(t, "allocHeapCreate", dev.createDescriptorHeap(heapInfo, "Descriptor alloc heap", heap, e_rr)))
		return;

	if(!Test_assert(t, "allocLayoutCreate", dev.createDescriptorLayout(info, "Descriptor alloc layout", layout, e_rr)))
		return;

	if(!Test_assert(t, "allocTableCreate", heap.createTable(
		layout, "Descriptor alloc table", table, c::EDescriptorTableFlags_None, e_rr
	)))
		return;

	if(!Test_assert(t, "allocBufferCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead, "Descriptor alloc buffer", 256, buffer, nullptr, e_rr
	)))
		return;

	const c::DescriptorTable *tablePtr = table.data();
	const c::Descriptor desc = c::Descriptor_buffer(buffer.handle(), 0, 0, nullptr, 0);

	//Nothing in flight first: the allocator on its own, with no owner of the buffer other than this function.
	//The binding declares 8 slots, so 8 allocations have to succeed and each has to land on a slot of its own.

	Test_assert(t, "fillToCount", Test_fillDescriptorBinding(t, table, desc, false, count) == 0xFF);

	//Only a full binding may refuse, and refusing has to leave the caller's id alone rather than report a slot
	// off the end of the array.

	c::U64 overflowId = c::U64_MAX;

	Test_assert(t, "allocPastCount", !table.alloc(0, overflowId, desc));
	Test_assert(t, "allocPastCountNothing", overflowId == c::U64_MAX);

	//Freeing a slot in the middle proves the search finds a hole rather than tracking a high water mark.

	c::U64 recycled = c::U64_MAX;

	Test_assert(t, "freeMiddle", table.unset(0, 3, 1, e_rr));
	Test_assert(t, "reallocMiddle", table.alloc(0, recycled, desc, false, e_rr));
	Test_assert(t, "reallocMiddleId", recycled == 3);
	Test_assert(t, "allocFullAgain", !table.alloc(0, overflowId, desc));

	//Releasing the whole binding has to give all 8 slots back, not only the ones below some internal bound.

	Test_assert(t, "freeAll", table.unset(0, 0, count, e_rr));
	Test_assert(t, "refillToCount", Test_fillDescriptorBinding(t, table, desc, false, count) == 0xFF);
	Test_assert(t, "refillFreeAll", table.unset(0, 0, count, e_rr));

	//maintainRef was false throughout, so the table never took a reference of its own to give back.

	Test_assert(t, "refillNoRefs", !tablePtr->resources.length);

	//Now the same binding with the buffer genuinely in flight.
	//maintainRef makes the table hold a reference for as long as the descriptor lives and a submitted command
	// list makes the device hold one until that frame retires, so the descriptors below are freed with two other
	// owners still on the resource, which is what a resource released mid frame actually looks like.
	//The clear on a throwaway render target is the modify op the scope needs: an empty scope is dropped along
	// with its transitions, so the buffer would never reach the device's in flight list.

	if(!Test_assert(t, "allocTargetCreate", dev.createRenderTexture(
		4, 4, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Descriptor alloc target", target,
		c::EMSAASamples_Off, nullptr, e_rr
	)))
		return;

	if(!Test_assert(t, "allocListCreate", dev.createCommandList(2 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	c::Transition transition{};
	transition.resource = buffer.handle();
	transition.stage = c::EPipelineStage_Compute;

	c::ImageRange all{};
	all.levelId = c::U32_MAX;
	all.layerId = c::U32_MAX;

	c::Bool recorded = Test_assert(t, "inFlightBegin", commandList.begin(true, e_rr));

	if (recorded) {

		CommandScope scope = commandList.scope({ transition }, 1, {}, e_rr);

		recorded =
			Test_assert(t, "inFlightScope", (c::Bool) scope) &&
			Test_assert(t, "inFlightClear", scope.clearImagef(c::F32x4_create4(0, 0, 0, 1), all, target.handle(), e_rr)) &&
			Test_assert(t, "inFlightScopeEnd", scope.end(e_rr));
	}

	recorded = recorded && Test_assert(t, "inFlightEnd", commandList.end(e_rr));

	if(!recorded)
		return;

	//The list only keeps what its transitions named, so this is what makes the submit below put the buffer in
	// flight instead of only the render target.

	Test_assert(t, "inFlightTracked", c::ListRefPtr_contains(commandList.data()->resources, buffer.handle(), 0, nullptr));

	Test_assert(t, "inFlightSubmit", dev.submit({ &commandList }, {}, 0, 0, e_rr));

	//Every descriptor points at the same buffer, so the table's resource list holds one entry with a count of 8.

	Test_assert(t, "inFlightFill", Test_fillDescriptorBinding(t, table, desc, true, count) == 0xFF);
	Test_assert(t, "inFlightRef", tablePtr->resources.length == 1);

	//Freed while the submit is still outstanding; the slots have to come back right away and the table has to
	// let go of every reference it took, even though the device is still holding one of its own.

	Test_assert(t, "inFlightFree", table.unset(0, 0, count, e_rr));
	Test_assert(t, "inFlightNoRefs", !tablePtr->resources.length);

	c::U64 duringId = c::U64_MAX;

	Test_assert(t, "inFlightRealloc", table.alloc(0, duringId, desc, true, e_rr));
	Test_assert(t, "inFlightReallocId", !duringId);
	Test_assert(t, "inFlightFreeAgain", table.unset(0, duringId, 1, e_rr));

	//And once the frame has actually retired the binding still hands out all 8, so the device releasing its own
	// reference afterwards stranded nothing.

	Test_assert(t, "inFlightWait", dev.wait(e_rr));
	Test_assert(t, "afterWaitFill", Test_fillDescriptorBinding(t, table, desc, true, count) == 0xFF);
	Test_assert(t, "afterWaitRef", tablePtr->resources.length == 1);

	//Freed explicitly rather than left for the table's own free to release.
	//Releasing on free is what the debug leak detector exists to report, so a test that relied on it warned on
	// every run (8 descriptors, every device); the in flight half above already frees the same way.

	Test_assert(t, "afterWaitFree", table.unset(0, 0, count, e_rr));
	Test_assert(t, "afterWaitNoRefs", !tablePtr->resources.length);
}
