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
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/bindless_descriptor.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/commands.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/pipeline.h"
#include "platforms/platform.h"
#include "formats/oiSH/sh_file.h"
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
