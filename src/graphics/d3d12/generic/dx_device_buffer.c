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

//graphics/d3d12/generic/dx_device_buffer.c

#include "graphics/d3d12/dx_buffer.h"
#include "graphics/d3d12/dx_device.h"
#include "graphics/generic/interface.h"
#include "graphics/d3d12/dx_interface.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/instance.h"
#include "platforms/logx.h"
#include "types/container/buffer.h"
#include "types/container/string_unicode.h"
#include "types/base/constants.h"

Bool DxDeviceBuffer_transition(
	DxDeviceBuffer *buffer,
	D3D12_BARRIER_SYNC sync,
	D3D12_BARRIER_ACCESS access,
	ListD3D12_BUFFER_BARRIER *bufferBarriers,
	D3D12_BARRIER_GROUP *dependency,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	//Avoid duplicate barriers except in one case:
	//direct3d12.has the concept of UAVBarriers, which always need to be inserted in-between two compute calls.
	//Otherwise, it's not synchronized correctly.

	if(buffer->lastSync == sync && buffer->lastAccess == access && !(access & D3D12BarrierAccess_Write))
		return s_uccess;

	//Handle buffer barrier

	//The first use has SyncBefore NONE, which the spec only allows together with AccessBefore NO_ACCESS

	const D3D12_BUFFER_BARRIER bufferBarrier = (D3D12_BUFFER_BARRIER) {
		.SyncBefore = buffer->lastSync,
		.SyncAfter = sync,
		.AccessBefore = buffer->lastSync == D3D12_BARRIER_SYNC_NONE ? D3D12_BARRIER_ACCESS_NO_ACCESS : buffer->lastAccess,
		.AccessAfter = access,
		.pResource = buffer->buffer,
		.Size = UINT64_MAX            //Sized barrier not allowed
	};

	gotoIfError3(clean, ListD3D12_BUFFER_BARRIER_pushBack(bufferBarriers, bufferBarrier, alloc, e_rr));

	buffer->lastSync = bufferBarrier.SyncAfter;
	buffer->lastAccess = bufferBarrier.AccessAfter;

	dependency->pBufferBarriers = bufferBarriers->ptr;
	dependency->NumBarriers = (U32) bufferBarriers->length;

clean:
	return s_uccess;
}

void DX_WRAP_FUNC(DeviceBuffer_free)(DeviceBuffer *buffer) {

	DxDeviceBuffer *bufferExt = DeviceBuffer_ext(buffer, Dx);

	if(buffer->resource.debugExt)
		((ID3D12ManualWriteTrackingResource*)buffer->resource.debugExt)->lpVtbl->Release(
			(ID3D12ManualWriteTrackingResource*)buffer->resource.debugExt
		);

	if(bufferExt->buffer)
		bufferExt->buffer->lpVtbl->Release(bufferExt->buffer);
}

Bool DX_WRAP_FUNC(GraphicsDeviceRef_createBuffer)(
	GraphicsDeviceRef *dev, DeviceBuffer *buf, const CharString *name, Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(dev);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	DxDeviceBuffer *bufExt = DeviceBuffer_ext(buf, Dx);
	ListU16 name16 = (ListU16) { 0 };

	//Query about alignment and size

	D3D12_RESOURCE_DESC1 resourceDesc = (D3D12_RESOURCE_DESC1) {
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		.Width = buf->resource.size,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = (DXGI_SAMPLE_DESC) { .Count = 1, .Quality = 0 },
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR
	};

	if(buf->resource.flags & EGraphicsResourceFlag_ShaderWrite)
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	if(!(buf->resource.flags & EGraphicsResourceFlag_ShaderRead))
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

	if(buf->usage & EDeviceBufferUsage_ScratchExt)
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	//The spec requires UAV alongside, since builds write the AS through unordered access behind the scenes

	if(buf->usage & EDeviceBufferUsage_ASExt)
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	//Acceleration structure buffers don't honor the tight contract, so requesting it hands the allocator a tight
	// offset while placement still demands the full alignment, which fails resource creation.

	#if D3D12_SDK_VERSION >= 618
		if(
			(device->info.capabilities.featuresExt & EDxGraphicsFeatures_TightAlignment) &&
			!(buf->usage & EDeviceBufferUsage_ASExt)
		) {
			resourceDesc.Flags |= D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT;

			//Tight alignment lets D3D12 pick a smaller alignment; an explicit 64 KiB placement alignment is
			//incompatible with it and makes GetResourceAllocationInfo2 fail (SizeInBytes = U64_MAX), so clear it.
			resourceDesc.Alignment = 0;
		}
	#endif

	D3D12_RESOURCE_ALLOCATION_INFO1 allocInfo = (D3D12_RESOURCE_ALLOCATION_INFO1) { 0 };
	D3D12_RESOURCE_ALLOCATION_INFO retVal = (D3D12_RESOURCE_ALLOCATION_INFO) { 0 };
	D3D12_RESOURCE_ALLOCATION_INFO *res = deviceExt->device->lpVtbl->GetResourceAllocationInfo2(
		deviceExt->device, &retVal, 0, 1, &resourceDesc, &allocInfo
	);

	ELockAcquire acq = ELockAcquire_Invalid;

	//GetResourceAllocationInfo2 returns its (non-null) retVal pointer even on failure, signalling it via
	//SizeInBytes = U64_MAX, so check the sentinel too (the texture path does the same) instead of only !res.
	if(!res || allocInfo.SizeInBytes == U64_MAX)
		retError(clean, Error_invalidState(0, "D3D12GraphicsDeviceRef_createBuffer() couldn't query allocInfo"));

	Bool cpuSided = buf->resource.flags & EGraphicsResourceFlag_CPUAllocatedBit;

	DeviceMemoryBlock block;

	//When a buffer gets "too big" we will give it a dedicated allocation.
	//This means ring buffers are always dedicated resources (they're always >64MiB).

	if (buf->resource.size >= 64 * MIBI && device->info.type == EGraphicsDeviceType_Dedicated) {

		block = (DeviceMemoryBlock) {
			.isActive = true,
			.typeExt = (U32) allocInfo.Alignment,
			.allocationTypeExt = !cpuSided,        //Don't share dedicated and non dedicated allocations
			.isDedicated = true
		};

		if(device->flags & EGraphicsDeviceFlags_IsDebug)
			Error_captureStackTrace(block.stackTrace, (U8)(sizeof(block.stackTrace) / sizeof(void*)), 1);

		if(allocInfo.SizeInBytes > device->info.capabilities.maxAllocationSize)
			retError(clean, Error_invalidState(0, "D3D12UnifiedTexture_create() couldn't allocate resource size!"));

		U64 usedMem = DX_WRAP_FUNC(GraphicsDevice_getMemoryBudget)(device, !cpuSided);
		U64 maxAlloc = cpuSided ? device->info.capabilities.sharedMemory : device->info.capabilities.dedicatedMemory;

		if(usedMem != U64_MAX && usedMem + allocInfo.SizeInBytes > maxAlloc)
			retError(clean, Error_outOfMemory(0, "Dedicated memory block allocation would exceed available memory"));

		acq = SpinLock_lock(&device->allocator.lock, U64_MAX);

		if(device->allocator.blocks.length >= U32_MAX)
			retError(clean, Error_invalidState(0, "D3D12UnifiedTexture_create() couldn't allocate dedicated block"));

		U32 blockId = (U32) device->allocator.blocks.length;
		gotoIfError3(clean, ListDeviceMemoryBlock_pushBack(&device->allocator.blocks, block, alloc, e_rr));

		AllocationBuffer *allocBuf = &device->allocator.blocks.ptrNonConst[blockId].allocations;

		const AllocationBufferCreate allocCreate = (AllocationBufferCreate) {
			.size = allocInfo.SizeInBytes,
			.nonLinearAlignment = 0,
			.alloc = alloc,
			.allocationBuffer = allocBuf
		};

		gotoIfError3(clean, AllocationBuffer_create(&allocCreate, true, e_rr));

		const AllocationBufferAllocate allocBuffer = (AllocationBufferAllocate) {
			.allocationBuffer = allocBuf,
			.alignment = allocInfo.Alignment,
			.isNonLinearResource = false,
			.alloc = alloc
		};

		U8 *dummy = NULL;
		gotoIfError3(clean, AllocationBuffer_allocateBlock(&allocBuffer, allocInfo.SizeInBytes, (const U8**) &dummy, e_rr));

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(&device->allocator.lock);

		acq = ELockAcquire_Invalid;

		D3D12_HEAP_DESC heap = getDxHeapDesc(device, &cpuSided, allocInfo.Alignment, EResourceType_Undefined);

		if(device->flags & EGraphicsDeviceFlags_IsDebug)
			Log_debugLnx(
				"-- Graphics: Allocating dedicated memory block (%"PRIu32" with size %"PRIu64")\n"
				"\t%s (Memory left: %"PRIu64")",
				blockId,
				allocInfo.SizeInBytes,
				cpuSided ? "Cpu sided allocation" : "Gpu sided allocation",
				maxAlloc - usedMem
			);

		gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->CreateCommittedResource3(
			deviceExt->device,
			&heap.Properties,
			heap.Flags,
			&resourceDesc,
			D3D12_BARRIER_LAYOUT_UNDEFINED,
			NULL,
			NULL, 0, NULL,
			&IID_ID3D12Resource,
			(void**)&bufExt->buffer
		), e_rr));

		buf->resource.allocated = true;
		buf->resource.blockId = blockId;
		buf->resource.blockOffset = 0;
	}

	//Allocate as memory block

	else {

		DxBlockRequirements req = (DxBlockRequirements) {
			.flags = EDxBlockFlags_None,
			.alignment = (U32) allocInfo.Alignment,
			.length = allocInfo.SizeInBytes
		};

		gotoIfError3(clean, DX_WRAP_FUNC(DeviceMemoryAllocator_allocate)(
			&device->allocator,
			&req,
			cpuSided,
			&buf->resource.blockId,
			&buf->resource.blockOffset,
			EResourceType_DeviceBuffer,
			name,
			&block, e_rr
		));

		buf->resource.allocated = true;

		//Bind memory

		gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->CreatePlacedResource2(
			deviceExt->device,
			block.ext,
			buf->resource.blockOffset,
			&resourceDesc,
			D3D12_BARRIER_LAYOUT_UNDEFINED,
			NULL,
			0, NULL,
			&IID_ID3D12Resource,
			(void**)&bufExt->buffer
		), e_rr));
	}

	if (!(block.allocationTypeExt & 1) || (device->info.capabilities.featuresExt & EDxGraphicsFeatures_ReBAR))
		gotoIfError3(clean, dxCheck(bufExt->buffer->lpVtbl->Map(
			bufExt->buffer, 0, NULL, (void**) &buf->resource.mappedMemoryExt
		), e_rr));

	//Grab GPU location

	buf->resource.deviceAddress = bufExt->buffer->lpVtbl->GetGPUVirtualAddress(bufExt->buffer);

	if(!buf->resource.deviceAddress)
		retError(clean, Error_invalidState(0, "D3D12GraphicsDeviceRef_createBuffer() Couldn't obtain GPU address"));

	if(
		(device->info.capabilities.featuresExt & EDxGraphicsFeatures_ReallyReportReBARWrites) ==
		EDxGraphicsFeatures_ReallyReportReBARWrites
	)
		gotoIfError3(clean, dxCheck(bufExt->buffer->lpVtbl->QueryInterface(
			bufExt->buffer, &IID_ID3D12ManualWriteTrackingResource, (void**) &buf->resource.debugExt
		), e_rr));

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && name && CharString_length(*name)) {
		gotoIfError3(clean, CharString_toUTF16(*name, alloc, &name16, e_rr));
		gotoIfError3(clean, dxCheck(bufExt->buffer->lpVtbl->SetName(bufExt->buffer, name16.ptr), e_rr));
	}

	bufExt->lastAccess = D3D12_BARRIER_ACCESS_NO_ACCESS;

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->allocator.lock);

	ListU16_free(&name16, alloc);
	return s_uccess;
}

Bool DX_WRAP_FUNC(DeviceBufferRef_flush)(
	void *commandBufferExt,
	GraphicsDeviceRef *deviceRef,
	DeviceBufferRef *pending,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	DxCommandBufferState *commandBuffer = (DxCommandBufferState*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	DeviceBuffer *buffer = DeviceBufferRef_ptr(pending);
	DxDeviceBuffer *bufferExt = DeviceBuffer_ext(buffer, Dx);

	Bool isInFlight = false;
	ListRefPtr *currentFlight = &device->resourcesInFlight[device->fifId];
	DeviceBufferRef *tempStagingResource = NULL;

	gotoIfError3(clean, ListD3D12_BUFFER_BARRIER_reserve(&deviceExt->bufferTransitions, 2, alloc, e_rr));

	for(U64 j = 0; j < sizeof(device->resourcesInFlight) / sizeof(device->resourcesInFlight[0]); ++j) {

		ListRefPtr inFlight = device->resourcesInFlight[j];

		for(U64 i = 0; i < inFlight.length; ++i)
			if (inFlight.ptr[i] == pending) {
				isInFlight = true;
				break;
			}

		if(isInFlight)
			break;
	}

	if (!isInFlight && buffer->resource.mappedMemoryExt) {

		ID3D12ManualWriteTrackingResource *tracking = (ID3D12ManualWriteTrackingResource*) buffer->resource.debugExt;

		for (U64 j = 0; j < buffer->pendingChanges.length; ++j) {

			DevicePendingRange range = buffer->pendingChanges.ptr[j];

			U64 start = range.buffer.startRange;
			U64 len = range.buffer.endRange - range.buffer.startRange;

			Buffer dst = Buffer_createRef((U8*)buffer->resource.mappedMemoryExt + start, len);
			Buffer src = Buffer_createRefConst(buffer->cpuData.ptr + start, len);

			Buffer_memcpy(dst, src);

			if (tracking) {
				D3D12_RANGE rangeD3D12 = (D3D12_RANGE) { .Begin = start, .End = start + len };
				tracking->lpVtbl->TrackWrite(tracking, 0, &rangeD3D12);
			}
		}
	}

	else {

		//TODO: Copy queue

		U64 allocRange = 0;

		for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {
			const BufferRange bufferj = buffer->pendingChanges.ptr[j].buffer;
			allocRange += bufferj.endRange - bufferj.startRange;
		}

		device->pendingBytes += allocRange;

		D3D12_BARRIER_GROUP dependency = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_BUFFER };

		if (allocRange >= DeviceBufferRef_ptr(device->staging)->resource.size / 4) {

			CharString dedicatedStaging = CharString_createRefCStrConst("Dedicated staging buffer");

			gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
				deviceRef,
				EDeviceBufferUsage_None, EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit,
				NULL,
				&dedicatedStaging,
				allocRange, &tempStagingResource, e_rr
			));

			DeviceBuffer *stagingResource = DeviceBufferRef_ptr(tempStagingResource);
			DxDeviceBuffer *stagingResourceExt = DeviceBuffer_ext(stagingResource, Dx);
			U8 *location = stagingResource->resource.mappedMemoryExt;

			ID3D12ManualWriteTrackingResource *tracking = (ID3D12ManualWriteTrackingResource*) stagingResource->resource.debugExt;

			//Copy into our buffer

			allocRange = 0;

			for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {

				const BufferRange bufferj = buffer->pendingChanges.ptr[j].buffer;
				U64 len = bufferj.endRange - bufferj.startRange;

				Buffer_memcpy(
					Buffer_createRef(location + allocRange, len),
					Buffer_createRefConst(buffer->cpuData.ptr + bufferj.startRange, len)
				);

				if (tracking) {
					D3D12_RANGE rangeD3D12 = (D3D12_RANGE) { .Begin = allocRange, .End = allocRange + len };
					tracking->lpVtbl->TrackWrite(tracking, 0, &rangeD3D12);
				}

				gotoIfError3(clean, DxDeviceBuffer_transition(
					stagingResourceExt,
					D3D12_BARRIER_SYNC_COPY,
					D3D12_BARRIER_ACCESS_COPY_SOURCE,
					&deviceExt->bufferTransitions,
					&dependency, alloc, e_rr
				));

				gotoIfError3(clean, DxDeviceBuffer_transition(
					bufferExt,
					D3D12_BARRIER_SYNC_COPY,
					D3D12_BARRIER_ACCESS_COPY_DEST,
					&deviceExt->bufferTransitions,
					&dependency, alloc, e_rr
				));

				if(dependency.NumBarriers) {
					commandBuffer->buffer->lpVtbl->Barrier(commandBuffer->buffer, 1, &dependency);
					ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions, e_rr);
				}

				commandBuffer->buffer->lpVtbl->CopyBufferRegion(
					commandBuffer->buffer,
					bufferExt->buffer,
					bufferj.startRange,
					stagingResourceExt->buffer,
					allocRange,
					len
				);

				allocRange += len;
			}

			//When staging resource is committed to current in flight then we can relinquish ownership.

			gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, tempStagingResource, alloc, e_rr));
			tempStagingResource = NULL;
		}

		//Use staging buffer

		else {

			AllocationBuffer *stagingBuffer = &device->stagingAllocations[device->fifId];
			DeviceBuffer *staging = DeviceBufferRef_ptr(device->staging);
			DxDeviceBuffer *stagingExt = DeviceBuffer_ext(staging, Dx);

			ID3D12ManualWriteTrackingResource *tracking = (ID3D12ManualWriteTrackingResource*) staging->resource.debugExt;

			U8 *defaultLocation = (U8*) 1, *location = defaultLocation;
			Error temp = Error_none();

			AllocationBufferAllocate bufferAlloc = (AllocationBufferAllocate) {
				.allocationBuffer = stagingBuffer,
				.alignment = 4,
				.alloc = alloc
			};

			Bool allocated = AllocationBuffer_allocateBlock(
				&bufferAlloc, allocRange, (const U8**) &location, &temp
			);

			if(!allocated && location == defaultLocation) {        //Something else went wrong
				if(e_rr) *e_rr = temp;
				s_uccess = false;
				goto clean;
			}

			//We re-create the staging buffer to fit the new allocation.

			if (!allocated) {

				U64 prevSize = DeviceBufferRef_ptr(device->staging)->resource.size;

				//Allocate new staging buffer.

				U64 newSize = prevSize * 2 + allocRange * 3;
				gotoIfError3(clean, GraphicsDeviceRef_resizeStagingBuffer(deviceRef, newSize, e_rr));

				bufferAlloc = (AllocationBufferAllocate) {
					.allocationBuffer = stagingBuffer,
					.alignment = 4,
					.alloc = alloc
				};

				gotoIfError3(clean, AllocationBuffer_allocateBlock(
					&bufferAlloc, allocRange, (const U8**) &location, e_rr
				));

				staging = DeviceBufferRef_ptr(device->staging);
				stagingExt = DeviceBuffer_ext(staging, Dx);
			}

			//Copy into our buffer

			allocRange = 0;

			for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {

				const BufferRange bufferj = buffer->pendingChanges.ptr[j].buffer;
				U64 len = bufferj.endRange - bufferj.startRange;

				Buffer_memcpy(
					Buffer_createRef(location + allocRange, len),
					Buffer_createRefConst(buffer->cpuData.ptr + bufferj.startRange, len)
				);

				if (tracking) {
					D3D12_RANGE rangeD3D12 = (D3D12_RANGE) { .Begin = allocRange, .End = allocRange + len };
					tracking->lpVtbl->TrackWrite(tracking, 0, &rangeD3D12);
				}

				gotoIfError3(clean, DxDeviceBuffer_transition(
					bufferExt,
					D3D12_BARRIER_SYNC_COPY,
					D3D12_BARRIER_ACCESS_COPY_DEST,
					&deviceExt->bufferTransitions,
					&dependency, alloc, e_rr
				));

				if(!ListRefPtr_contains(*currentFlight, device->staging, 0, NULL)) {

					gotoIfError3(clean, DxDeviceBuffer_transition(                        //Ensure resource is transitioned
						stagingExt,
						D3D12_BARRIER_SYNC_COPY,
						D3D12_BARRIER_ACCESS_COPY_SOURCE,
						&deviceExt->bufferTransitions,
						&dependency, alloc, e_rr
					));

					RefPtr_inc(device->staging);
					gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, device->staging, alloc, e_rr));        //Add to in flight
				}

				if(dependency.NumBarriers) {
					commandBuffer->buffer->lpVtbl->Barrier(commandBuffer->buffer, 1, &dependency);
					ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions, e_rr);
				}

				commandBuffer->buffer->lpVtbl->CopyBufferRegion(
					commandBuffer->buffer,
					bufferExt->buffer,
					bufferj.startRange,
					stagingExt->buffer,
					allocRange + (location - (const U8*)staging->resource.mappedMemoryExt),        //Resource relative
					len
				);

				allocRange += len;
			}
		}
	}

	if(!(buffer->resource.flags & EGraphicsResourceFlag_CPUBacked))
		Buffer_free(&buffer->cpuData, alloc);

	buffer->isFirstFrame = buffer->isPending = buffer->isPendingFullCopy = false;
	gotoIfError3(clean, ListDevicePendingRange_clear(&buffer->pendingChanges, e_rr));

	if(RefPtr_inc(pending))
		gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, pending, alloc, e_rr));

	if (device->pendingBytes >= device->flushThreshold)
		gotoIfError3(clean, DxGraphicsDevice_flush(deviceRef, commandBuffer, e_rr));

clean:
	ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions, NULL);
	RefPtr_dec(&tempStagingResource);
	return s_uccess;
}
