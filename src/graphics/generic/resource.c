/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "graphics/generic/resource.h"
#include "graphics/generic/allocator.h"
#include "graphics/generic/device.h"

TListImpl(DeviceResourceVersion);

U16 TextureRange_width(TextureRange r) { return r.endRange[0] - r.startRange[0]; }
U16 TextureRange_height(TextureRange r) { return r.endRange[1] - r.startRange[1]; }
U16 TextureRange_length(TextureRange r) { return r.endRange[2] - r.startRange[2]; }

Bool GraphicsResource_free(GraphicsResource *resource, RefPtr *resourceRef) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(resource->device);
	Bool success = true;

	if(resource->allocated) {
		success &= DeviceMemoryAllocator_freeAllocation(&device->allocator, resource->blockId, resource->blockOffset);
		resource->allocated = false;
	}

	success &= GraphicsDeviceRef_removePending(resource->device, resourceRef);

	if(!(resource->flags & EGraphicsResourceFlag_InternalWeakDeviceRef))
		success &= !GraphicsDeviceRef_dec(&resource->device).genericError;

	return success;
}
