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
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/device.h"
#include "platforms/ext/ref_ptrx.h"
#include "types/container/string.h"

Error DescriptorHeapRef_dec(DescriptorHeapRef **heap) {
	return !RefPtr_dec(heap) ?
		Error_invalidOperation(0, "DescriptorHeapRef_dec()::heap is required") : Error_none();
}

Error DescriptorHeapRef_inc(DescriptorHeapRef *heap) {
	return !RefPtr_inc(heap) ?
		Error_invalidOperation(0, "DescriptorHeapRef_inc()::heap is required") : Error_none();
}

Bool DescriptorHeap_free(DescriptorHeap *heap, Allocator alloc) {

	(void)alloc;

	//Log_debugLnx("Destroy: %p", heap);

	Bool success = DescriptorHeap_freeExt(heap, alloc);

	if(!(heap->info.flags & EDescriptorHeapFlags_InternalWeakDeviceRef))
		success &= !GraphicsDeviceRef_dec(&heap->device).genericError;

	return success;
}

Error GraphicsDeviceRef_createDescriptorHeap(
	GraphicsDeviceRef *dev,
	DescriptorHeapInfo info,
	CharString name,
	DescriptorHeapRef **heapRef
) {

	if(!dev || dev->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_createDescriptorHeap()::dev is required");

	U64 srvCbvUav =
		(U64) info.maxAccelerationStructures + info.maxTextures + info.maxConstantBuffers +
		info.maxTexturesRW + info.maxBuffersRW;

	if(srvCbvUav > 1000000)
		return Error_outOfBounds(
			0, srvCbvUav, 1000000,
			"GraphicsDeviceRef_createDescriptorHeap()::info must not exceed over 1M descriptors (CBV, UAV, SRV)"
		);

	if(info.maxSamplers > 2048)
		return Error_outOfBounds(
			0, info.maxSamplers, 2048,
			"GraphicsDeviceRef_createDescriptorHeap()::info must not exceed over 2048 samplers"
		);

	if(!srvCbvUav && !info.maxSamplers)
		return Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorHeap()::info contains no valid descriptors"
		);

	if(!info.maxDescriptorTables)
		return Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorHeap()::info contains maxDescriptorTables of 0"
		);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);

	if((info.flags & EDescriptorHeapFlags_AllowBindless) && !(device->info.capabilities.features & EGraphicsFeatures_Bindless))
		return Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorHeap()::info.flags can't include bindless if bindless feature is missing"
		);

	if(info.maxAccelerationStructures && !(device->info.capabilities.features & EGraphicsFeatures_Raytracing))
		return Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorHeap()::info.maxAccelerationStructures can't be >0 if raytracing is missing"
		);

	Error err = RefPtr_createx(
		(U32)(sizeof(DescriptorHeap) + GraphicsDeviceRef_getObjectSizes(dev)->descriptorHeap),
		(ObjectFreeFunc) DescriptorHeap_free,
		(ETypeId) EGraphicsTypeId_DescriptorHeap,
		heapRef
	);

	if(err.genericError)
		return err;

	if(!(info.flags & EDescriptorHeapFlags_InternalWeakDeviceRef))
		gotoIfError(clean, GraphicsDeviceRef_inc(dev))

	DescriptorHeap *heap = DescriptorHeapRef_ptr(*heapRef);

	//Log_debugLnx("Create: DescriptorHeap %.*s (%p)", (int) CharString_length(name), name.ptr, heap);

	*heap = (DescriptorHeap) { .device = dev, .info = info };

	gotoIfError(clean, GraphicsDeviceRef_createDescriptorHeapExt(dev, heap, name))

clean:

	if(err.genericError)
		DescriptorHeapRef_dec(heapRef);

	return err;
}
