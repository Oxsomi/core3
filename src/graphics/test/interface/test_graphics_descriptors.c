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

//graphics/test/interface/test_graphics_descriptors.c

//Descriptor layout, table and bindless modules (11, 12, 13, 40).
//These need a live device and are called once per adapter from the device test loop.

#include "graphics/generic/device.h"
#include "graphics/generic/device_info.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/bindless_descriptor.h"
#include "graphics/generic/render_texture.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/commands.h"
#include "graphics/generic/graphics_types.h"
#include "types/test/test.h"
#include "types/base/string_base.h"
#include "test_graphics_shared.h"

// -- Device dependent, skipped without an adapter --------------------------------

//7 to 10 live in Test_graphicsDeviceForApi below, which brings up the device the rest of these run against.

// -- 11. DescriptorLayout / DescriptorTable --------------------------------------

//Builds a small explicit layout, a heap sized for it and a table on top of both, then checks the invariants the
// headers promise before tearing all of it back down.
//The layout is bindless where the device has the feature and bindful where it doesn't, so whichever adapter CI
// hands us exercises one of the two paths.

void Test_graphicsDescriptorTable(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "DescriptorLayout");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	const Bool hasBindless = (device->info.capabilities.features & EGraphicsFeatures_Bindless) != 0;

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	DescriptorTableRef *secondTable = NULL;
	DescriptorLayoutRef *badLayout = NULL;
	DescriptorTableRef *badTable = NULL;
	DeviceBufferRef *buffer = NULL;
	DeviceBufferRef *routed = NULL;

	//Two arrays and one single descriptor, all byte address buffers so no texture format rules come into play.
	//SPIRV wants unique binding ids within a set and DXIL wants non overlapping ranges per register type;
	// t0-t3, u4-u7 and t8 satisfies both, so the test doesn't have to know which api it's running on.

	CharString bindingNames[3] = {
		CharString_createRefCStrConst("testBuffers"),
		CharString_createRefCStrConst("testRWBuffers"),
		CharString_createRefCStrConst("testBuffer")
	};

	DescriptorBinding bindings[3] = {
		(DescriptorBinding) {
			.registerType = ESHRegisterType_ByteAddressBuffer,
			.count = 4,
			.binding = (SHBinding) { .space = 0, .binding = 0 },
			.visibility = 1 << ESHPipelineStage_Compute
		},
		(DescriptorBinding) {
			.registerType = ESHRegisterType_ByteAddressBuffer | ESHRegisterType_IsWrite,
			.count = 4,
			.binding = (SHBinding) { .space = 0, .binding = 4 },
			.visibility = 1 << ESHPipelineStage_Compute
		},
		(DescriptorBinding) {
			.registerType = ESHRegisterType_ByteAddressBuffer,
			.count = 1,
			.binding = (SHBinding) { .space = 0, .binding = 8 },
			.visibility = 1 << ESHPipelineStage_Compute
		}
	};

	CharString name = CharString_createRefCStrConst("Test descriptor layout");

	//Parameter validation.
	//Every info here refs stack memory, so a rejected create has nothing to leak.

	DescriptorLayoutInfo info = (DescriptorLayoutInfo) {
		.flags = hasBindless ? EDescriptorLayoutFlags_AllowBindlessOnArrays : EDescriptorLayoutFlags_None
	};

	Test_assert(t, "bindingsRef", ListDescriptorBinding_createRefConst(bindings, 3, &info.bindings, &t->err));
	Test_assert(t, "namesRef", ListCharString_createRefConst(bindingNames, 3, &info.bindingNames, &t->err));

	Test_assert(t, "layoutNullDevice", !GraphicsDeviceRef_createDescriptorLayout(NULL, &info, &name, &badLayout, NULL));
	Test_assert(t, "layoutNullInfo", !GraphicsDeviceRef_createDescriptorLayout(deviceRef, NULL, &name, &badLayout, NULL));
	Test_assert(t, "layoutNullOut", !GraphicsDeviceRef_createDescriptorLayout(deviceRef, &info, &name, NULL, NULL));

	//A constant buffer has to declare its size and push constants belong in a pipeline layout, not a descriptor layout.

	DescriptorBinding sizelessCBuffer = (DescriptorBinding) {
		.registerType = ESHRegisterType_ConstantBuffer,
		.count = 1,
		.binding = (SHBinding) { .space = 0, .binding = 0 },
		.visibility = 1 << ESHPipelineStage_Compute
	};

	DescriptorBinding pushConstant = (DescriptorBinding) {
		.registerType = ESHRegisterType_PushConstants,
		.count = 1,
		.binding = (SHBinding) { .space = 0, .binding = 0 },
		.visibility = 1 << ESHPipelineStage_Compute,
		.constantBufferSize = 16
	};

	DescriptorLayoutInfo cbufferInfo = (DescriptorLayoutInfo) { 0 };
	DescriptorLayoutInfo pushInfo = (DescriptorLayoutInfo) { 0 };

	Test_assert(t, "cbufferRef", ListDescriptorBinding_createRefConst(&sizelessCBuffer, 1, &cbufferInfo.bindings, &t->err));
	Test_assert(t, "pushRef", ListDescriptorBinding_createRefConst(&pushConstant, 1, &pushInfo.bindings, &t->err));

	Test_assert(t, "layoutSizelessCBuffer", !GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &cbufferInfo, &name, &badLayout, NULL
	));

	Test_assert(t, "layoutPushConstants", !GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &pushInfo, &name, &badLayout, NULL
	));

	//Bindless and push descriptors exclude each other.
	//On a device without bindless the flag on its own is already illegal, so this is rejected either way.

	DescriptorLayoutInfo mixedInfo = (DescriptorLayoutInfo) {
		.flags = EDescriptorLayoutFlags_AllowBindlessOnArrays | EDescriptorLayoutFlags_HasPushDescriptors
	};

	Test_assert(t, "mixedRef", ListDescriptorBinding_createRefConst(bindings, 3, &mixedInfo.bindings, &t->err));

	Test_assert(t, "layoutBindlessPushDescriptors", !GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &mixedInfo, &name, &badLayout, NULL
	));

	Test_assert(t, "layoutRejectedNothing", !badLayout);

	//A heap of its own, sized for exactly the 9 buffer descriptors the layout declares.

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) {
		.flags = hasBindless ? EDescriptorHeapFlags_AllowBindless : EDescriptorHeapFlags_None,
		.maxBuffersRW = 9,
		.maxDescriptorTables = 1
	};

	DescriptorHeapInfo emptyHeapInfo = (DescriptorHeapInfo) { .maxDescriptorTables = 1 };

	name = CharString_createRefCStrConst("Test descriptor heap");

	Test_assert(t, "heapNullDevice", !GraphicsDeviceRef_createDescriptorHeap(NULL, &heapInfo, &name, &heap, NULL));
	Test_assert(t, "heapNullInfo", !GraphicsDeviceRef_createDescriptorHeap(deviceRef, NULL, &name, &heap, NULL));

	Test_assert(t, "heapNoDescriptors", !GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &emptyHeapInfo, &name, &heap, NULL
	));

	if(!Test_assert(t, "heapCreate", GraphicsDeviceRef_createDescriptorHeap(deviceRef, &heapInfo, &name, &heap, &t->err)))
		goto clean;

	//Real layout create; the info is moved into the layout, which is what the emptied lists below check.

	name = CharString_createRefCStrConst("Test descriptor layout");

	if(!Test_assert(t, "layoutCreate", GraphicsDeviceRef_createDescriptorLayout(deviceRef, &info, &name, &layout, &t->err)))
		goto clean;

	Test_assert(t, "layoutTypeId", layout->refPtrType->typeId == (TypeId) EGraphicsTypeId_DescriptorLayout);
	Test_assert(t, "layoutInfoMoved", !info.bindings.ptr && !info.bindingNames.ptr);

	const DescriptorLayout *layoutPtr = DescriptorLayoutRef_ptr(layout);

	Test_assert(t, "layoutDevice", layoutPtr->device == deviceRef);
	Test_assert(t, "layoutBindingCount", layoutPtr->info.bindings.length == 3);
	Test_assert(t, "layoutNameCount", layoutPtr->info.bindingNames.length == 3);
	Test_assert(t, "layoutOwnsBindings", !ListDescriptorBinding_isRef(layoutPtr->info.bindings));
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
			Test_assert(t, "bindlessMapNonArray", layoutPtr->bindingToBindlessType.ptr[2] == U8_MAX);
		}
	}

	else {
		Test_assert(t, "noBindlessTypes", !layoutPtr->bindlessTypeToBinding.length);
		Test_assert(t, "noBindlessMap", !layoutPtr->bindingToBindlessType.length);
	}

	//Table create + validation.

	Test_setModule(t, "DescriptorTable");

	name = CharString_createRefCStrConst("Test descriptor table");

	Test_assert(t, "tableNullParent", !DescriptorHeapRef_createDescriptorTable(
		NULL, layout, EDescriptorTableFlags_None, &name, &badTable, NULL
	));

	Test_assert(t, "tableNullLayout", !DescriptorHeapRef_createDescriptorTable(
		heap, NULL, EDescriptorTableFlags_None, &name, &badTable, NULL
	));

	if(!Test_assert(t, "tableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	Test_assert(t, "tableTypeId", table->refPtrType->typeId == (TypeId) EGraphicsTypeId_DescriptorTable);

	const DescriptorTable *tablePtr = DescriptorTableRef_ptr(table);

	Test_assert(t, "tableParent", tablePtr->parent == heap);
	Test_assert(t, "tableLayout", tablePtr->layout == layout);
	Test_assert(t, "tableBindingCount", tablePtr->bindings.length == 3);
	Test_assert(t, "tableNoResources", !tablePtr->resources.length);

	//The heap was created with room for a single table, which is what maxDescriptorTables means.

	Test_assert(t, "tableOutOfSlots", !DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &secondTable, NULL
	));

	Test_assert(t, "tableRejectedNothing", !badTable && !secondTable);

	//Register names resolve to their binding index, anything else doesn't resolve at all.

	const CharString missingName = CharString_createRefCStrConst("notARegister");

	Test_assert(t, "resolveFirst", !DescriptorTableRef_resolveRegisterName(table, &bindingNames[0]));
	Test_assert(t, "resolveLast", DescriptorTableRef_resolveRegisterName(table, &bindingNames[2]) == 2);
	Test_assert(t, "resolveMissing", DescriptorTableRef_resolveRegisterName(table, &missingName) == U64_MAX);

	//A buffer to point descriptors at; the read only bindings above require ShaderRead on the resource.

	name = CharString_createRefCStrConst("Test descriptor table buffer");

	if(!Test_assert(t, "bufferCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL, &name, 256, &buffer, &t->err
	)))
		goto clean;

	const Descriptor desc = Descriptor_buffer(buffer, 0, 0, NULL, 0);
	U64 arrayId = U64_MAX;

	//Allocating picks a free slot in an array, so a binding with a count of 1 has nothing to pick from.

	Test_assert(t, "allocOnNonArray", !DescriptorTableRef_allocDescriptor(table, 2, &arrayId, false, &desc, NULL));
	Test_assert(t, "allocOutOfBounds", !DescriptorTableRef_allocDescriptor(table, 3, &arrayId, false, &desc, NULL));
	Test_assert(t, "allocNullArrayId", !DescriptorTableRef_allocDescriptor(table, 0, NULL, false, &desc, NULL));

	//That single descriptor is set through its binding index instead.
	//maintainRef stays false, so the table doesn't take ownership of the resource.

	Test_assert(t, "setSingle", DescriptorTableRef_setDescriptor(table, 2, 0, false, &desc, &t->err));
	Test_assert(t, "setSingleNoRef", !tablePtr->resources.length);
	Test_assert(t, "setSingleOOB", !DescriptorTableRef_setDescriptor(table, 2, 1, false, &desc, NULL));
	Test_assert(t, "unsetSingle", DescriptorTableRef_unsetDescriptors(table, 2, 0, 1, &t->err));

	//The plural setter, which nothing else in the suite reaches directly: setDescriptor and the by name forms
	// all funnel through it with a length fixed at 1, so its range handling was never exercised.
	//Binding 0 is the count 4 array, so arrayId 1 with 2 descriptors fits and arrayId 3 with 2 runs off the end.

	const Descriptor descPair[2] = { desc, desc };
	ListDescriptor many = (ListDescriptor) { 0 };
	ListDescriptor_createRefConst(descPair, 2, &many, NULL);

	Test_assert(t, "setMany", DescriptorTableRef_setDescriptors(table, 0, 1, false, &many, &t->err));
	Test_assert(t, "setManyNoRef", !tablePtr->resources.length);
	Test_assert(t, "setManyOOB", !DescriptorTableRef_setDescriptors(table, 0, 3, false, &many, NULL));
	Test_assert(t, "unsetMany", DescriptorTableRef_unsetDescriptors(table, 0, 1, 2, &t->err));

	//An empty list has nothing to bind and has to be refused on both binding shapes.
	//The array case is the one that mattered: it used to report success while binding nothing, because the
	// comparison loop simply ran zero times.
	//The single case indexed element 0 of a list that has none.

	ListDescriptor none = (ListDescriptor) { 0 };

	Test_assert(t, "setNoneSingle", !DescriptorTableRef_setDescriptors(table, 2, 0, false, &none, NULL));
	Test_assert(t, "setNoneArray", !DescriptorTableRef_setDescriptors(table, 0, 0, false, &none, NULL));

	//The by name variants resolve the register name first, so they're the same operations routed differently
	// and an unknown name has to be refused everywhere rather than defaulting to binding 0.

	Test_assert(t, "setByName", DescriptorTableRef_setDescriptorByName(table, &bindingNames[2], 0, false, &desc, &t->err));
	Test_assert(t, "unsetByName", DescriptorTableRef_unsetDescriptorsByName(table, &bindingNames[2], 0, 1, &t->err));
	Test_assert(t, "setByNameMissing", !DescriptorTableRef_setDescriptorByName(table, &missingName, 0, false, &desc, NULL));

	ListDescriptor one = (ListDescriptor) { 0 };
	ListDescriptor_createRefConst(&desc, 1, &one, NULL);

	Test_assert(t, "setManyByName", DescriptorTableRef_setDescriptorsByName(
		table, &bindingNames[0], 0, false, &one, &t->err
	));

	Test_assert(t, "unsetManyByName", DescriptorTableRef_unsetDescriptorsByName(table, &bindingNames[0], 0, 1, &t->err));

	Test_assert(t, "unsetByNameMissing", !DescriptorTableRef_unsetDescriptorsByName(table, &missingName, 0, 1, NULL));

	Test_assert(t, "allocByName", DescriptorTableRef_allocDescriptorByName(
		table, &bindingNames[0], &arrayId, false, &desc, &t->err
	));

	Test_assert(t, "allocByNameId", arrayId < 4);
	Test_assert(t, "unsetAllocByName", DescriptorTableRef_unsetDescriptorsByName(table, &bindingNames[0], arrayId, 1, &t->err));

	Test_assert(t, "allocByNameNonArray", !DescriptorTableRef_allocDescriptorByName(
		table, &bindingNames[2], &arrayId, false, &desc, NULL
	));

	Test_assert(t, "allocByNameMissing", !DescriptorTableRef_allocDescriptorByName(
		table, &missingName, &arrayId, false, &desc, NULL
	));

	if (hasBindless) {

		//A bindless allocation finds the array whose register type matches, which is the first binding here.

		U16 bindId = U16_MAX;
		U8 bindlessTypeId = U8_MAX;

		Test_assert(t, "allocBindless", DescriptorTableRef_allocDescriptorBindless(
			table, ESHRegisterType_ByteAddressBuffer, 0, &bindId, &bindlessTypeId, &arrayId, false, &desc, &t->err
		));

		Test_assert(t, "allocBindlessBinding", !bindId);
		Test_assert(t, "allocBindlessType", !bindlessTypeId);
		Test_assert(t, "allocBindlessArrayId", !arrayId);
		Test_assert(t, "unsetBindless", DescriptorTableRef_unsetDescriptors(table, bindId, arrayId, 1, &t->err));

		//findBindlessRegister answers "where would a resource like this go", so the type has to match a binding

		U16 foundBind = U16_MAX;
		U8 foundType = U8_MAX;

		Test_assert(t, "findRegister", DescriptorTableRef_findBindlessRegister(
			table, ESHRegisterType_ByteAddressBuffer, 0, &foundBind, &foundType, buffer, 0, &t->err
		));

		Test_assert(t, "findRegisterBinding", !foundBind && !foundType);

		Test_assert(t, "findRegisterMissing", !DescriptorTableRef_findBindlessRegister(
			table, ESHRegisterType_Sampler, 0, &foundBind, &foundType, buffer, 0, NULL
		));

		//Resources can be routed into a table of the caller's own, which is what every creator's
		// bindlessDescriptorTable parameter is for.

		name = CharString_createRefCStrConst("Test routed buffer");

		Test_assert(t, "routedCreate", GraphicsDeviceRef_createBuffer(
			deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderReadBindless, table,
			&name, 256, &routed, &t->err
		));

		if (routed) {

			const DeviceBuffer *routedPtr = DeviceBufferRef_ptr(routed);

			Test_assert(t, "routedTable", routedPtr->bindlessDescriptorTable == table);
			Test_assert(t, "routedHandle", routedPtr->readHandle != BindlessDescriptor_None);
			Test_assert(t, "routedValid", BindlessDescriptor_isValid(deviceRef, table, routedPtr->readHandle));
			Test_assert(t, "routedNoWriteHandle", routedPtr->writeHandle == BindlessDescriptor_None);
		}
	}

clean:

	//The table reads the layout while it's being freed but never took a ref on it, so it has to go first.

	RefPtr_dec(&routed);
	RefPtr_dec(&buffer);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);
}

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

// -- 40. Descriptor allocation: filling a binding, recycling, resources in flight -

//Every descriptor test above allocates a single slot and hands it straight back, so the allocator was only ever
// asked for slot 0 of an array.
//This fills a binding to its declared count instead, which is what real code does the moment a scene holds more
// than a couple of exposed buffers or acceleration structures.
//It runs in two halves, because "the slot came back" and "the slot came back while the gpu still held the
// resource" are separate paths: the first frees descriptors nothing ever touched, the second frees descriptors
// the table owns a reference through and whose buffer is in flight in a submitted command list.

//Allocates until the binding is full, returning a bitmask of the slots it got.
//A slot that repeats or lands outside the array stops the fill, so a full mask is the only way to pass.

static U64 Test_fillDescriptorBinding(
	Test *t,
	DescriptorTableRef *table,
	const Descriptor *desc,
	Bool maintainRef,
	U64 count
) {

	U64 taken = 0;

	for (U64 i = 0; i < count; ++i) {

		U64 arrayId = U64_MAX;

		if(!DescriptorTableRef_allocDescriptor(table, 0, &arrayId, maintainRef, desc, &t->err))
			break;

		if(arrayId >= count || ((taken >> arrayId) & 1))
			break;

		taken |= (U64)1 << arrayId;
	}

	return taken;
}

void Test_graphicsDescriptorAlloc(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "DescriptorTable/alloc");

	DescriptorHeapRef *heap = NULL;
	DescriptorLayoutRef *layout = NULL;
	DescriptorTableRef *table = NULL;
	DeviceBufferRef *buffer = NULL;
	RenderTextureRef *target = NULL;
	CommandListRef *commandList = NULL;

	//One array of 8, small enough to fill exhaustively and big enough that the bitset behind it spans more than
	// the handful of slots the rest of the suite ever touches.
	//Bindless is deliberately left out of both the layout and the heap, so this runs the same on every adapter.

	const U64 count = 8;

	CharString bindingName = CharString_createRefCStrConst("allocBuffers");

	DescriptorBinding binding = (DescriptorBinding) {
		.registerType = ESHRegisterType_ByteAddressBuffer,
		.count = 8,
		.binding = (SHBinding) { .space = 0, .binding = 0 },
		.visibility = 1 << ESHPipelineStage_Compute
	};

	DescriptorLayoutInfo info = (DescriptorLayoutInfo) { 0 };

	Test_assert(t, "allocBindingRef", ListDescriptorBinding_createRefConst(&binding, 1, &info.bindings, &t->err));
	Test_assert(t, "allocNameRef", ListCharString_createRefConst(&bindingName, 1, &info.bindingNames, &t->err));

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .maxBuffersRW = 8, .maxDescriptorTables = 1 };

	CharString name = CharString_createRefCStrConst("Descriptor alloc heap");

	if(!Test_assert(t, "allocHeapCreate", GraphicsDeviceRef_createDescriptorHeap(
		deviceRef, &heapInfo, &name, &heap, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Descriptor alloc layout");

	if(!Test_assert(t, "allocLayoutCreate", GraphicsDeviceRef_createDescriptorLayout(
		deviceRef, &info, &name, &layout, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Descriptor alloc table");

	if(!Test_assert(t, "allocTableCreate", DescriptorHeapRef_createDescriptorTable(
		heap, layout, EDescriptorTableFlags_None, &name, &table, &t->err
	)))
		goto clean;

	name = CharString_createRefCStrConst("Descriptor alloc buffer");

	if(!Test_assert(t, "allocBufferCreate", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL, &name, 256, &buffer, &t->err
	)))
		goto clean;

	const DescriptorTable *tablePtr = DescriptorTableRef_ptr(table);
	const Descriptor desc = Descriptor_buffer(buffer, 0, 0, NULL, 0);

	//Nothing in flight first: the allocator on its own, with no owner of the buffer other than this function.
	//The binding declares 8 slots, so 8 allocations have to succeed and each has to land on a slot of its own.

	Test_assert(t, "fillToCount", Test_fillDescriptorBinding(t, table, &desc, false, count) == 0xFF);

	//Only a full binding may refuse, and refusing has to leave the caller's id alone rather than report a slot
	// off the end of the array.

	U64 overflowId = U64_MAX;

	Test_assert(t, "allocPastCount", !DescriptorTableRef_allocDescriptor(table, 0, &overflowId, false, &desc, NULL));
	Test_assert(t, "allocPastCountNothing", overflowId == U64_MAX);

	//Freeing a slot in the middle proves the search finds a hole rather than tracking a high water mark.

	U64 recycled = U64_MAX;

	Test_assert(t, "freeMiddle", DescriptorTableRef_unsetDescriptors(table, 0, 3, 1, &t->err));
	Test_assert(t, "reallocMiddle", DescriptorTableRef_allocDescriptor(table, 0, &recycled, false, &desc, &t->err));
	Test_assert(t, "reallocMiddleId", recycled == 3);
	Test_assert(t, "allocFullAgain", !DescriptorTableRef_allocDescriptor(table, 0, &overflowId, false, &desc, NULL));

	//Releasing the whole binding has to give all 8 slots back, not only the ones below some internal bound.

	Test_assert(t, "freeAll", DescriptorTableRef_unsetDescriptors(table, 0, 0, count, &t->err));
	Test_assert(t, "refillToCount", Test_fillDescriptorBinding(t, table, &desc, false, count) == 0xFF);
	Test_assert(t, "refillFreeAll", DescriptorTableRef_unsetDescriptors(table, 0, 0, count, &t->err));

	//maintainRef was false throughout, so the table never took a reference of its own to give back.

	Test_assert(t, "refillNoRefs", !tablePtr->resources.length);

	//Now the same binding with the buffer genuinely in flight.
	//maintainRef makes the table hold a reference for as long as the descriptor lives and a submitted command
	// list makes the device hold one until that frame retires, so the descriptors below are freed with two other
	// owners still on the resource, which is what a resource released mid frame actually looks like.
	//The clear on a throwaway render target is the modify op the scope needs: an empty scope is dropped along
	// with its transitions, so the buffer would never reach the device's in flight list.

	name = CharString_createRefCStrConst("Descriptor alloc target");

	if(!Test_assert(t, "allocTargetCreate", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 4, 4, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &name, &target, &t->err
	)))
		goto clean;

	if(!Test_assert(t, "allocListCreate", GraphicsDeviceRef_createCommandList(
		deviceRef, 2 * KIBI, 64, 16, true, &commandList, &t->err
	)))
		goto clean;

	const Transition transition = (Transition) { .resource = buffer, .stage = EPipelineStage_Compute };

	ListTransition transitions = (ListTransition) { 0 };
	ListTransition_createRefConst(&transition, 1, &transitions, NULL);

	const ImageRange all = (ImageRange) { .levelId = U32_MAX, .layerId = U32_MAX };

	const Bool recorded =
		Test_assert(t, "inFlightBegin", CommandListRef_begin(commandList, true, U64_MAX, &t->err)) &&
		Test_assert(t, "inFlightScope", CommandListRef_startScope(commandList, &transitions, 1, NULL, &t->err)) &&
		Test_assert(t, "inFlightClear", CommandListRef_clearImagef(
			commandList, F32x4_create4(0, 0, 0, 1), all, target, &t->err
		)) &&
		Test_assert(t, "inFlightScopeEnd", CommandListRef_endScope(commandList, &t->err)) &&
		Test_assert(t, "inFlightEnd", CommandListRef_end(commandList, &t->err));

	if(!recorded)
		goto clean;

	//The list only keeps what its transitions named, so this is what makes the submit below put the buffer in
	// flight instead of only the render target.

	Test_assert(t, "inFlightTracked", ListRefPtr_contains(CommandListRef_ptr(commandList)->resources, buffer, 0, NULL));

	ListCommandListRef lists = (ListCommandListRef) { 0 };
	ListCommandListRef_createRefConst(&commandList, 1, &lists, NULL);

	Test_assert(t, "inFlightSubmit", GraphicsDeviceRef_submitCommands(deviceRef, &lists, NULL, NULL, 0, 0, &t->err));

	//Every descriptor points at the same buffer, so the table's resource list holds one entry with a count of 8.

	Test_assert(t, "inFlightFill", Test_fillDescriptorBinding(t, table, &desc, true, count) == 0xFF);
	Test_assert(t, "inFlightRef", tablePtr->resources.length == 1);

	//Freed while the submit is still outstanding; the slots have to come back right away and the table has to
	// let go of every reference it took, even though the device is still holding one of its own.

	Test_assert(t, "inFlightFree", DescriptorTableRef_unsetDescriptors(table, 0, 0, count, &t->err));
	Test_assert(t, "inFlightNoRefs", !tablePtr->resources.length);

	U64 duringId = U64_MAX;

	Test_assert(t, "inFlightRealloc", DescriptorTableRef_allocDescriptor(table, 0, &duringId, true, &desc, &t->err));
	Test_assert(t, "inFlightReallocId", !duringId);
	Test_assert(t, "inFlightFreeAgain", DescriptorTableRef_unsetDescriptors(table, 0, duringId, 1, &t->err));

	//And once the frame has actually retired the binding still hands out all 8, so the device releasing its own
	// reference afterwards stranded nothing.

	Test_assert(t, "inFlightWait", GraphicsDeviceRef_wait(deviceRef, &t->err));
	Test_assert(t, "afterWaitFill", Test_fillDescriptorBinding(t, table, &desc, true, count) == 0xFF);
	Test_assert(t, "afterWaitRef", tablePtr->resources.length == 1);

	//Those 8 are deliberately left allocated: freeing the table is what has to release them, which is the last
	// owner the buffer's dec below relies on.

clean:

	RefPtr_dec(&commandList);
	RefPtr_dec(&target);
	RefPtr_dec(&buffer);
	RefPtr_dec(&table);
	RefPtr_dec(&layout);
	RefPtr_dec(&heap);
}
