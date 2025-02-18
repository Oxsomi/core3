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
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/device.h"
#include "platforms/ext/ref_ptrx.h"
#include "types/container/string.h"

Error PipelineLayoutRef_dec(PipelineLayoutRef **layout) {
	return !RefPtr_dec(layout) ?
		Error_invalidOperation(0, "PipelineLayoutRef_dec()::layout is required") : Error_none();
}

Error PipelineLayoutRef_inc(PipelineLayoutRef *layout) {
	return !RefPtr_inc(layout) ?
		Error_invalidOperation(0, "PipelineLayoutRef_inc()::layout is required") : Error_none();
}

Bool PipelineLayout_free(PipelineLayout *layout, Allocator alloc) {

	(void)alloc;

	//Log_debugLnx("Destroy: %p", layout);

	Bool success = PipelineLayout_freeExt(layout, alloc);

	if(!(layout->info.flags & EPipelineLayoutFlags_InternalWeakDeviceRef))
		success &= !GraphicsDeviceRef_dec(&layout->device).genericError;

	DescriptorLayoutRef_dec(&layout->info.bindings);
	return success;
}

Error GraphicsDeviceRef_createPipelineLayout(
	GraphicsDeviceRef *dev,
	PipelineLayoutInfo info,
	CharString name,
	PipelineLayoutRef **layoutRef
) {

	if(!dev || dev->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_createPipelineLayout()::dev is required");

	if(!info.bindings || info.bindings->typeId != (ETypeId) EGraphicsTypeId_DescriptorLayout)
		return Error_nullPointer(0, "GraphicsDeviceRef_createPipelineLayout()::info.bindings is required");

	Error err = RefPtr_createx(
		(U32)(sizeof(PipelineLayout) + GraphicsDeviceRef_getObjectSizes(dev)->pipelineLayout),
		(ObjectFreeFunc) PipelineLayout_free,
		(ETypeId) EGraphicsTypeId_PipelineLayout,
		layoutRef
	);

	if(err.genericError)
		return err;

	if(!(info.flags & EPipelineLayoutFlags_InternalWeakDeviceRef))
		gotoIfError(clean, GraphicsDeviceRef_inc(dev))

	PipelineLayout *layout = PipelineLayoutRef_ptr(*layoutRef);

	//Log_debugLnx("Create: PipelineLayout %.*s (%p)", (int) CharString_length(name), name.ptr, layout);

	gotoIfError(clean, DescriptorLayoutRef_inc(layout->info.bindings))

	*layout = (PipelineLayout) { .device = dev, .info = info };
	gotoIfError(clean, GraphicsDeviceRef_createPipelineLayoutExt(dev, layout, name))

clean:

	if(err.genericError)
		PipelineLayoutRef_dec(layoutRef);

	return err;
}
