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

//graphics/generic/descriptor_heap.c

#include "graphics/generic/interface.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/device.h"
#include "types/container/ref_ptr.h"
#include "types/base/string_base.h"

void DescriptorHeap_free(DescriptorHeap *heap, const Allocator *alloc) {

	(void)alloc;

	DescriptorHeap_freeExt(heap, alloc);

	if(!(heap->info.flags & EDescriptorHeapFlags_InternalWeakDeviceRef))
		RefPtr_dec(&heap->device);
}

Bool GraphicsDeviceRef_createDescriptorHeap(
	GraphicsDeviceRef *dev,
	const DescriptorHeapInfo *info,
	const CharString *name,
	DescriptorHeapRef **heapRef,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!dev || dev->refPtrType->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_createDescriptorHeap()::dev is required"));

	U64 srvCbvUav =
		(U64) info->maxAccelerationStructures + info->maxTextures + info->maxConstantBuffers +
		info->maxTexturesRW + info->maxBuffersRW;

	if(srvCbvUav > 1000000)
		retError(clean, Error_outOfBounds(
			0, srvCbvUav, 1000000,
			"GraphicsDeviceRef_createDescriptorHeap()::info must not exceed over 1M descriptors (CBV, UAV, SRV)"
		));

	if(info->maxSamplers > 2048)
		retError(clean, Error_outOfBounds(
			0, info->maxSamplers, 2048,
			"GraphicsDeviceRef_createDescriptorHeap()::info must not exceed over 2048 samplers"
		));

	if(!srvCbvUav && !info->maxSamplers)
		retError(clean, Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorHeap()::info contains no valid descriptors"
		));

	if(!info->maxDescriptorTables)
		retError(clean, Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorHeap()::info contains maxDescriptorTables of 0"
		));

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);

	if(
		(info->flags & EDescriptorHeapFlags_AllowBindless) &&
		!(device->info.capabilities.features & EGraphicsFeatures_Bindless)
	)
		retError(clean, Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorHeap()::info.flags can't include bindless if bindless feature is missing"
		));

	if(info->maxAccelerationStructures && !(device->info.capabilities.features & EGraphicsFeatures_Raytracing))
		retError(clean, Error_invalidOperation(
			0, "GraphicsDeviceRef_createDescriptorHeap()::info.maxAccelerationStructures can't be >0 if raytracing is missing"
		));

	gotoIfError3(clean, RefPtr_create(&GraphicsDeviceRef_getTypes(dev)->descriptorHeap, heapRef, e_rr));

	if(!(info->flags & EDescriptorHeapFlags_InternalWeakDeviceRef))
		gotoIfError3(clean, RefPtr_inc(dev));

	DescriptorHeap *heap = DescriptorHeapRef_ptr(*heapRef);

	*heap = (DescriptorHeap) { .device = dev, .info = info };

	gotoIfError3(clean, GraphicsDeviceRef_createDescriptorHeapExt(dev, heap, name, e_rr));

clean:

	if(!s_uccess)
		RefPtr_dec(heapRef);

	return s_uccess;
}
