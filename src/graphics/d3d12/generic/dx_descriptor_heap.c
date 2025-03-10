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
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/d3d12/dx_device.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "types/container/string.h"

Bool DX_WRAP_FUNC(DescriptorHeap_free)(DescriptorHeap *heap, Allocator alloc) {

	(void) alloc;

	DxDescriptorHeap *heapExt = DescriptorHeap_ext(heap, Dx);

	if(heapExt->samplerHeap.heap)
		heapExt->samplerHeap.heap->lpVtbl->Release(heapExt->samplerHeap.heap);

	if(heapExt->resourcesHeap.heap)
		heapExt->resourcesHeap.heap->lpVtbl->Release(heapExt->resourcesHeap.heap);

	AllocationBuffer_freex(&heapExt->allocators[0]);
	AllocationBuffer_freex(&heapExt->allocators[1]);

	return true;
}

Error DX_WRAP_FUNC(GraphicsDeviceRef_createDescriptorHeap)(GraphicsDeviceRef *dev, DescriptorHeap *heap, CharString name) {

	Error err = Error_none();

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	DxDescriptorHeap *heapExt = DescriptorHeap_ext(heap, Dx);

	const DescriptorHeapInfo info = heap->info;
	CharString tmpName = CharString_createNull();

	U32 srvCbvUav =
		(U32) info.maxAccelerationStructures + info.maxTextures + info.maxConstantBuffers +
		info.maxTexturesRW + info.maxBuffersRW;

	if (srvCbvUav) {

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = (D3D12_DESCRIPTOR_HEAP_DESC) {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			.NumDescriptors = srvCbvUav,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		};

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && CharString_length(name))
			gotoIfError(clean, CharString_formatx(&tmpName, "%.*s resources heap", (int) CharString_length(name), name.ptr))

		gotoIfError(clean, DxGraphicsDevice_createDescriptorHeapSingle(
			deviceExt, heapDesc, &tmpName, &heapExt->resourcesHeap, true
		))

		gotoIfError(clean, AllocationBuffer_createx(srvCbvUav, true, 1, &heapExt->allocators[0]))
	}

	if (info.maxSamplers) {

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = (D3D12_DESCRIPTOR_HEAP_DESC) {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
			.NumDescriptors = info.maxSamplers,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		};

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && CharString_length(name))
			gotoIfError(clean, CharString_formatx(&tmpName, "%.*s sampler heap", (int) CharString_length(name), name.ptr))

		gotoIfError(clean, DxGraphicsDevice_createDescriptorHeapSingle(
			deviceExt, heapDesc, &tmpName, &heapExt->samplerHeap, true
		))
		
		gotoIfError(clean, AllocationBuffer_createx(info.maxSamplers, true, 1, &heapExt->allocators[1]))
	}

clean:
	CharString_freex(&tmpName);
	return err;
}

Bool DxDescriptorHeap_freeTable(DxDescriptorHeap *heapExt, DxDescriptorTable *table) {
	
	SpinLock *lock = NULL;
	ELockAcquire acq = ELockAcquire_Invalid;
	Error err = Error_none();

	//Free CBV/SRV/UAV and sampler ranges

	for(U8 i = 0; i < 2; ++i)
		if(table->allocationSizes[i]) {

			lock = &heapExt->locks[i];
			acq = SpinLock_lock(lock, 1 * SECOND);

			if(acq < ELockAcquire_Success)
				gotoIfError(clean, Error_invalidState(0, "DxDescriptorHeap_freeTable couldn't lock"))

			AllocationBuffer_freeBlock(&heapExt->allocators[i], (const U8*) (const void*) table->allocationLocations[i]);

			table->allocationSizes[i] = 0;

			if(acq == ELockAcquire_Acquired)
				SpinLock_unlock(lock);

			acq = ELockAcquire_Invalid;
		}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(lock);

	return !err.genericError;
}

Error DxDescriptorHeap_allocTable(DxDescriptorHeap *heapExt, DxDescriptorTable *table) {

	SpinLock *lock = NULL;
	ELockAcquire acq = ELockAcquire_Invalid;
	Error err = Error_none();

	U64 ranges[2] = { table->allocationSizes[0], table->allocationSizes[1] };
	table->allocationSizes[1] = table->allocationSizes[0] = 0;

	//Allocate CBV/SRV/UAV and sampler ranges

	for(U8 i = 0; i < 2; ++i)
		if(ranges[i]) {

			lock = &heapExt->locks[i];
			acq = SpinLock_lock(lock, 1 * SECOND);

			if(acq < ELockAcquire_Success)
				gotoIfError(clean, Error_invalidState(0, "DxDescriptorHeap_allocTable couldn't lock"))

			const U8 *loc = NULL;
			gotoIfError(clean, AllocationBuffer_allocateBlockx(&heapExt->allocators[i], ranges[i], 1, false, &loc))

			table->allocationLocations[i] = (U64) (const void*) loc;
			table->allocationSizes[i] = ranges[i];

			if(acq == ELockAcquire_Acquired)
				SpinLock_unlock(lock);

			acq = ELockAcquire_Invalid;
		}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(lock);

	return err;
}
