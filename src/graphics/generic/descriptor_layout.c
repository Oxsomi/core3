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

#include "platforms/ext/listx_impl.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/device.h"
#include "platforms/ext/ref_ptrx.h"
#include "types/container/string.h"

TListImpl(DescriptorBinding);

Error DescriptorLayoutRef_dec(DescriptorLayoutRef **layout) {
	return !RefPtr_dec(layout) ?
		Error_invalidOperation(0, "DescriptorLayoutRef_dec()::layout is required") : Error_none();
}

Error DescriptorLayoutRef_inc(DescriptorLayoutRef *layout) {
	return !RefPtr_inc(layout) ?
		Error_invalidOperation(0, "DescriptorLayoutRef_inc()::layout is required") : Error_none();
}

void DescriptorLayoutInfo_free(DescriptorLayoutInfo *info, Allocator alloc) {

	if(!info)
		return;

	ListDescriptorBinding_free(&info->bindings, alloc);
}

Bool DescriptorLayout_free(DescriptorLayout *layout, Allocator alloc) {

	(void)alloc;

	//Log_debugLnx("Destroy: %p", layout);

	Bool success = DescriptorLayout_freeExt(layout, alloc);

	if(!(layout->info.flags & EDescriptorLayoutFlags_InternalWeakDeviceRef))
		success &= !GraphicsDeviceRef_dec(&layout->device).genericError;

	DescriptorLayoutInfo_free(&layout->info, alloc);
	return success;
}

Error GraphicsDeviceRef_createDescriptorLayout(
	GraphicsDeviceRef *dev,
	DescriptorLayoutInfo *info,
	CharString name,
	DescriptorLayoutRef **layoutRef
) {

	if(!dev || dev->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_createDescriptorLayout()::dev is required");

	if(!info)
		return Error_nullPointer(0, "GraphicsDeviceRef_createDescriptorLayout()::info is required");

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);

	if(!!(info->flags & EDescriptorLayoutFlags_AllowBindlessAny) && !(device->info.capabilities.features & EGraphicsFeatures_Bindless))
		return Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorLayout()::info.flags can't include bindless if bindless feature is missing"
		);

	if(info->bindings.length >= U16_MAX)
		return Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorLayout()::info.bindings.length is limited to U16_MAX"
		);

	for(U64 i = 0; i < info->bindings.length; ++i)
		if(
			info->bindings.ptr[i].registerType == ESHRegisterType_AccelerationStructure &&
			!(device->info.capabilities.features & EGraphicsFeatures_Raytracing)
		)
			return Error_invalidOperation(
				0, "GraphicsDeviceRef_createDescriptorLayout()::info.bindings has an RTAS, but device doesn't have RT"
			);

	Error err = RefPtr_createx(
		(U32)(sizeof(DescriptorLayout) + GraphicsDeviceRef_getObjectSizes(dev)->descriptorLayout),
		(ObjectFreeFunc) DescriptorLayout_free,
		(ETypeId) EGraphicsTypeId_DescriptorLayout,
		layoutRef
	);

	if(err.genericError)
		return err;

	if(!(info->flags & EDescriptorLayoutFlags_InternalWeakDeviceRef))
		gotoIfError(clean, GraphicsDeviceRef_inc(dev))

	DescriptorLayout *layout = DescriptorLayoutRef_ptr(*layoutRef);

	//Log_debugLnx("Create: DescriptorLayout %.*s (%p)", (int) CharString_length(name), name.ptr, layout);

	*layout = (DescriptorLayout) { .device = dev, .info = *info };
	*info = (DescriptorLayoutInfo) { 0 };

	if(ListDescriptorBinding_isRef(layout->info.bindings)) {
		ListDescriptorBinding tmp = (ListDescriptorBinding) { 0 };
		gotoIfError(clean, ListDescriptorBinding_createCopyx(layout->info.bindings, &tmp))
		layout->info.bindings = tmp;
	}

	gotoIfError(clean, GraphicsDeviceRef_createDescriptorLayoutExt(dev, layout, name))

clean:

	if(err.genericError)
		DescriptorLayoutRef_dec(layoutRef);

	return err;
}
