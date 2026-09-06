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

//graphics/d3d12/generic/dx_descriptor_heap.c

#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/d3d12/dx_device.h"
#include "types/container/string.h"
#include "platforms/logx.h"

void DX_WRAP_FUNC(DescriptorHeap_free)(DescriptorHeap *heap, const Allocator *alloc) {

	(void) alloc;

	DxDescriptorHeap *heapExt = DescriptorHeap_ext(heap, Dx);

	if(heapExt->samplerHeap.heap)
		heapExt->samplerHeap.heap->lpVtbl->Release(heapExt->samplerHeap.heap);

	if(heapExt->resourcesHeap.heap)
		heapExt->resourcesHeap.heap->lpVtbl->Release(heapExt->resourcesHeap.heap);

	AllocationBuffer_free(&heapExt->allocators[0], alloc);
	AllocationBuffer_free(&heapExt->allocators[1], alloc);
}

Bool DX_WRAP_FUNC(GraphicsDeviceRef_createDescriptorHeap)(
	GraphicsDeviceRef *dev,
	DescriptorHeap *heap,
	const CharString *name,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(dev);

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	DxDescriptorHeap *heapExt = DescriptorHeap_ext(heap, Dx);

	const DescriptorHeapInfo info = heap->info;
	CharString tmpName = CharString_createNull();

	U32 srvCbvUav =
		(U32) info.maxAccelerationStructures + info.maxTextures + info.maxConstantBuffers +
		info.maxTexturesRW + info.maxBuffersRW;

	//The push ring lives past the declared maxima, so the heap is bigger than what allocators[0] covers.
	//That is what keeps a table allocation from ever being handed a slot a push descriptor is using.

	heapExt->pushRingPerFrame = info.maxPushDescriptors;
	heapExt->pushRingBase = srvCbvUav;
	heapExt->pushRingOffset = 0;

	const U32 pushRing = heapExt->pushRingPerFrame * device->framesInFlight;

	if (srvCbvUav || pushRing) {

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = (D3D12_DESCRIPTOR_HEAP_DESC) {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			.NumDescriptors = srvCbvUav + pushRing,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		};

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && name && CharString_length(*name))
			gotoIfError3(clean, CharString_format(
				alloc,
				&tmpName,
				e_rr,
				"%.*s resources heap",
				(int) CharString_length(*name),
				name->ptr
			));

		gotoIfError3(clean, DxGraphicsDevice_createDescriptorHeapSingle(
			deviceExt, heapDesc, &tmpName, &heapExt->resourcesHeap, true, alloc, e_rr
		));

		if (srvCbvUav) {

			AllocationBufferCreate create = (AllocationBufferCreate){
				.size = srvCbvUav,
				.nonLinearAlignment = 1,
				.alloc = alloc,
				.allocationBuffer = &heapExt->allocators[0]
			};

			gotoIfError3(clean, AllocationBuffer_create(&create, true, e_rr));
		}
	}

	if (info.maxSamplers) {

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = (D3D12_DESCRIPTOR_HEAP_DESC) {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
			.NumDescriptors = info.maxSamplers,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		};

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && name && CharString_length(*name))
			gotoIfError3(clean, CharString_format(
				alloc,
				&tmpName,
				e_rr,
				"%.*s sampler heap",
				(int) CharString_length(*name),
				name->ptr
			));

		gotoIfError3(clean, DxGraphicsDevice_createDescriptorHeapSingle(
			deviceExt, heapDesc, &tmpName, &heapExt->samplerHeap, true, alloc, e_rr
		));

		AllocationBufferCreate create = (AllocationBufferCreate){
			.size = info.maxSamplers,
			.nonLinearAlignment = 1,
			.alloc = alloc,
			.allocationBuffer = &heapExt->allocators[1]
		};

		gotoIfError3(clean, AllocationBuffer_create(&create, true, e_rr));
	}

clean:
	CharString_free(&tmpName, alloc);
	return s_uccess;
}

//Bump allocates count transient slots from the heap's push ring (see DescriptorHeapInfo::maxPushDescriptors).
//Each frame in flight owns a window that resets exactly once when its frame comes back around; wrapping
//WITHIN a frame is never safe, because descriptors are written at record time and read at execution, so a
//reused slot makes the earlier command read the newer descriptor. That wrap logs and continues.
//Never binds anything: the caller is responsible for the heap already being bound, since a background
//SetDescriptorHeaps is exactly the cost the explicit heap bind exists to keep visible.

Bool DxDescriptorHeap_allocTransient(DxDescriptorHeap *heapExt, U32 fifId, U32 count, U32 *slot) {

	if(!heapExt || !heapExt->pushRingPerFrame || !slot || count > heapExt->pushRingPerFrame)
		return false;

	if (heapExt->pushRingFrame != fifId) {
		heapExt->pushRingFrame = fifId;
		heapExt->pushRingOffset = 0;
	}

	if (heapExt->pushRingOffset + count > heapExt->pushRingPerFrame) {
		Log_errorLnx("Push descriptor ring wrapped mid frame; raise maxPushDescriptors");
		heapExt->pushRingOffset = 0;
	}

	*slot = heapExt->pushRingBase + fifId * heapExt->pushRingPerFrame + heapExt->pushRingOffset;
	heapExt->pushRingOffset += count;
	return true;
}

Bool DxDescriptorHeap_freeTable(DxDescriptorHeap *heapExt, DxDescriptorTable *table) {

	Bool s_uccess = true;

	SpinLock *lock = NULL;
	ELockAcquire acq = ELockAcquire_Invalid;

	//Free CBV/SRV/UAV and sampler ranges

	for(U8 i = 0; i < 2; ++i)
		if(table->allocationSizes[i]) {

			lock = &heapExt->locks[i];
			acq = SpinLock_lock(lock, 1 * SECOND);

			if(acq < ELockAcquire_Success) {        //No e_rr here; only used from the free path
				s_uccess = false;
				goto clean;
			}

			AllocationBuffer_freeBlock(&heapExt->allocators[i], (const U8*) (const void*) table->allocationLocations[i]);

			table->allocationSizes[i] = 0;

			if(acq == ELockAcquire_Acquired)
				SpinLock_unlock(lock);

			acq = ELockAcquire_Invalid;
		}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(lock);

	return s_uccess;
}

Bool DxDescriptorHeap_allocTable(DxDescriptorHeap *heapExt, DxDescriptorTable *table, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	SpinLock *lock = NULL;
	ELockAcquire acq = ELockAcquire_Invalid;

	U64 ranges[2] = { table->allocationSizes[0], table->allocationSizes[1] };
	table->allocationSizes[1] = table->allocationSizes[0] = 0;

	//Allocate CBV/SRV/UAV and sampler ranges

	for(U8 i = 0; i < 2; ++i)
		if(ranges[i]) {

			lock = &heapExt->locks[i];
			acq = SpinLock_lock(lock, 1 * SECOND);

			if(acq < ELockAcquire_Success)
				retError(clean, Error_invalidState(0, "DxDescriptorHeap_allocTable couldn't lock"));

			const AllocationBufferAllocate allocate = (AllocationBufferAllocate) {
					.allocationBuffer = &heapExt->allocators[i],
					.alignment = 1,
					.isNonLinearResource = false,
					.alloc = alloc
			};

			const U8 *loc = NULL;
			gotoIfError3(clean, AllocationBuffer_allocateBlock(&allocate, ranges[i], &loc, e_rr));

			table->allocationLocations[i] = (U64) (const void*) loc;
			table->allocationSizes[i] = ranges[i];

			if(acq == ELockAcquire_Acquired)
				SpinLock_unlock(lock);

			acq = ELockAcquire_Invalid;
		}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(lock);

	return s_uccess;
}
