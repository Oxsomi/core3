/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/ext/listx.h"
#include "graphics/generic/bindless_descriptor.h"
#include "graphics/generic/device.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/descriptor_table.h"

U64 BindlessDescriptor_pack3(BindlessDescriptor a, BindlessDescriptor b, BindlessDescriptor c) {
	return a | ((U64)b << 21) |  ((U64)c << 42);
}

I32x4 BindlessDescriptor_unpack3(U64 v) {

	const U32 a = v & ((1 << 21) - 1);
	const U32 b = (v >> 21) & ((1 << 21) - 1);
	const U32 c = v >> 42;

	return I32x4_create3((I32)a, (I32)b, (I32)c);
}

U8 BindlessDescriptor_getBindlessType(BindlessDescriptor handle) {
	return (U8)(handle >> 17);
}

U32 BindlessDescriptor_getId(BindlessDescriptor handle) {
	return handle & ((1 << 17) - 1);
}

Bool BindlessDescriptor_isValid(GraphicsDeviceRef *deviceRef, DescriptorTableRef *descTableRef, BindlessDescriptor handle) {

	if(!handle)
		return true;

	if(!deviceRef || deviceRef->typeId == (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return false;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(!descTableRef)
		descTableRef = device->defaultDescriptorTable;

	if(!descTableRef || descTableRef->typeId == (ETypeId) EGraphicsTypeId_DescriptorTable)
		return false;

	DescriptorTable *tablePtr = DescriptorTableRef_ptr(descTableRef);
	DescriptorLayout *layout = DescriptorLayoutRef_ptr(tablePtr->layout);

	U64 arrayId = BindlessDescriptor_getId(handle);
	U8 typeId = BindlessDescriptor_getBindlessType(handle);

	if(!typeId || typeId > layout->bindlessTypeToBinding.length)
		return false;

	U16 bindId = layout->bindlessTypeToBinding.ptr[typeId - 1];
	return arrayId < layout->info.bindings.ptr[bindId].count;
}

Bool GraphicsDeviceRef_allocateDescriptorBindless(
	GraphicsDeviceRef *deviceRef,
	DescriptorTableRef *descTableRef,
	ESHRegisterType type,
	U32 strideOrLength,
	Descriptor desc,
	BindlessDescriptor *descriptorHandle,
	Error *e_rr
) {

	Bool s_uccess = true;
	U16 bindId = U16_MAX;
	U64 arrayId = U64_MAX;
	U8 bindlessTypeId = 0;

	if(!deviceRef || deviceRef->typeId == (ETypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_allocateDescriptorBindless() deviceRef is required"))

	if(!descriptorHandle)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_allocateDescriptorBindless() descriptorHandle is required"))

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(!descTableRef)
		descTableRef = device->defaultDescriptorTable;

	gotoIfError3(clean, DescriptorTableRef_allocDescriptorBindless(
		descTableRef, type, strideOrLength, &bindId, &bindlessTypeId, &arrayId, desc, e_rr
	))

	*descriptorHandle = (((BindlessDescriptor) bindlessTypeId + 1) << 17) | (BindlessDescriptor) arrayId;

	if(*descriptorHandle == BindlessDescriptor_InvalidAllocation)
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_allocateDescriptorBindless() can't make descriptor handle"))

clean:

	if(!s_uccess && arrayId != U64_MAX)
		DescriptorTableRef_unsetDescriptors(descTableRef, bindId, arrayId, 1, NULL);

	return s_uccess;
}

Bool GraphicsDeviceRef_freeDescriptorBindless(
	GraphicsDeviceRef *deviceRef,
	DescriptorTableRef *descTableRef,
	BindlessDescriptor descriptor,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!descriptor)
		goto clean;

	if(!deviceRef || deviceRef->typeId == (ETypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_freeDescriptorBindless() deviceRef is required"))
		
	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(!descTableRef)
		descTableRef = device->defaultDescriptorTable;

	if(!descTableRef || descTableRef->typeId != (ETypeId) EGraphicsTypeId_DescriptorTable)
		retError(clean, Error_nullPointer(
			0, "GraphicsDeviceRef_freeDescriptorBindless() descriptor table is missing or invalid"
		))
		
	DescriptorTable *tablePtr = DescriptorTableRef_ptr(descTableRef);
	DescriptorLayout *layout = DescriptorLayoutRef_ptr(tablePtr->layout);

	U64 arrayId = BindlessDescriptor_getId(descriptor);
	U8 typeId = BindlessDescriptor_getBindlessType(descriptor);

	if(!typeId || typeId > layout->bindlessTypeToBinding.length)
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_freeDescriptorBindless() invalid descriptor type"))

	U16 bindId = layout->bindlessTypeToBinding.ptr[typeId - 1];
	gotoIfError3(clean, DescriptorTableRef_unsetDescriptors(descTableRef, bindId, arrayId, 1, e_rr))

clean:
	return s_uccess;
}
