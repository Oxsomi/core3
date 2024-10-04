/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/directx12/dx_buffer.h"
#include "graphics/directx12/dx_device.h"
#include "platforms/ext/bufferx.h"
#include "platforms/log.h"
#include "platforms/ext/stringx.h"

U64 DeviceBufferExt_size = sizeof(DxDeviceBuffer);

Error DxDeviceBuffer_transition(
	DxDeviceBuffer *buffer,
	D3D12_BARRIER_SYNC sync,
	D3D12_BARRIER_ACCESS access,
	ListD3D12_BUFFER_BARRIER *bufferBarriers,
	D3D12_BARRIER_GROUP *dependency
) {

	//Avoid duplicate barriers except in one case:
	//D3D12 has the concept of UAVBarriers, which always need to be inserted in-between two compute calls.
	//Otherwise, it's not synchronized correctly.

	if(
		buffer->lastSync == sync && buffer->lastAccess == access &&
		access != D3D12_BARRIER_ACCESS_UNORDERED_ACCESS &&
		access != D3D12_BARRIER_ACCESS_STREAM_OUTPUT &&
		access != D3D12_BARRIER_ACCESS_COPY_DEST &&
		access != D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE
	)
		return Error_none();

	//Handle buffer barrier

	const D3D12_BUFFER_BARRIER bufferBarrier = (D3D12_BUFFER_BARRIER) {
		.SyncBefore = buffer->lastSync,
		.SyncAfter = sync,
		.AccessBefore = buffer->lastAccess,
		.AccessAfter = access,
		.pResource = buffer->buffer,
		.Size = UINT64_MAX			//Sized barrier not allowed
	};

	const Error err = ListD3D12_BUFFER_BARRIER_pushBackx(bufferBarriers, bufferBarrier);

	if(err.genericError)
		return err;

	buffer->lastSync = bufferBarrier.SyncAfter;
	buffer->lastAccess = bufferBarrier.AccessAfter;

	dependency->pBufferBarriers = bufferBarriers->ptr;
	dependency->NumBarriers = (U32) bufferBarriers->length;

	return Error_none();
}

Bool DeviceBuffer_freeExt(DeviceBuffer *buffer) {

	DxDeviceBuffer *bufferExt = DeviceBuffer_ext(buffer, Dx);

	if(bufferExt->buffer) {
		bufferExt->buffer->lpVtbl->Release(bufferExt->buffer);
		bufferExt->buffer = NULL;
	}

	return true;
}

Error GraphicsDeviceRef_createBufferExt(GraphicsDeviceRef *dev, DeviceBuffer *buf, CharString name) {

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

	if(buf->usage & EDeviceBufferUsage_ASExt)
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE;

	D3D12_RESOURCE_ALLOCATION_INFO1 allocInfo = (D3D12_RESOURCE_ALLOCATION_INFO1) { 0 };
	D3D12_RESOURCE_ALLOCATION_INFO retVal = (D3D12_RESOURCE_ALLOCATION_INFO) { 0 };
	D3D12_RESOURCE_ALLOCATION_INFO *res = deviceExt->device->lpVtbl->GetResourceAllocationInfo2(
		deviceExt->device, &retVal, 0, 1, &resourceDesc, &allocInfo
	);

	Error err;

	if(!res)
		gotoIfError(clean, Error_invalidState(0, "GraphicsDeviceRef_createBufferExt() couldn't query allocInfo"))

	if(buf->resource.size / 4 != (U32)(buf->resource.size / 4))
		gotoIfError(clean, Error_invalidState(1, "GraphicsDeviceRef_createBufferExt() out of bounds"))

	//Allocate memory

	DxBlockRequirements req = (DxBlockRequirements) {
		.flags = EDxBlockFlags_None,
		.alignment = (U32) allocInfo.Alignment,
		.length = allocInfo.SizeInBytes
	};

	gotoIfError(clean, DeviceMemoryAllocator_allocate(
		&device->allocator,
		&req,
		buf->resource.flags & EGraphicsResourceFlag_CPUAllocatedBit,
		&buf->resource.blockId,
		&buf->resource.blockOffset,
		EResourceType_DeviceBuffer,
		name
	))

	buf->resource.allocated = true;

	DeviceMemoryBlock block = device->allocator.blocks.ptr[buf->resource.blockId];

	//Bind memory

	gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreatePlacedResource2(
		deviceExt->device,
		block.ext,
		buf->resource.blockOffset,
		&resourceDesc,
		D3D12_BARRIER_LAYOUT_UNDEFINED,
		NULL,
		0, NULL,
		&IID_ID3D12Resource,
		(void**)&bufExt->buffer
	)))

	if (!(block.allocationTypeExt & 1) || (device->info.capabilities.featuresExt & EDxGraphicsFeatures_ReBAR))
		gotoIfError(clean, dxCheck(bufExt->buffer->lpVtbl->Map(
			bufExt->buffer, 0, NULL, (void**) &buf->resource.mappedMemoryExt
		)))

	//Grab GPU location

	buf->resource.deviceAddress = bufExt->buffer->lpVtbl->GetGPUVirtualAddress(bufExt->buffer);

	if(!buf->resource.deviceAddress)
		gotoIfError(clean, Error_invalidState(0, "GraphicsDeviceRef_createBufferExt() Couldn't obtain GPU address"))

	//Fill relevant descriptor sets if shader accessible

	EGraphicsResourceFlag flags = buf->resource.flags;

	if(flags & EGraphicsResourceFlag_ShaderRW) {

		const DxHeap heap = deviceExt->heaps[EDescriptorHeapType_Resources];

		//Create readonly buffer

		if (flags & EGraphicsResourceFlag_ShaderRead) {

			D3D12_SHADER_RESOURCE_VIEW_DESC srv = (D3D12_SHADER_RESOURCE_VIEW_DESC) {
				.Format = DXGI_FORMAT_R32_TYPELESS,
				.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
				.Shader4ComponentMapping =  D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Buffer = (D3D12_BUFFER_SRV) {
					.NumElements = (U32)(buf->resource.size / 4),
					.Flags = D3D12_BUFFER_SRV_FLAG_RAW
				}
			};

			U64 offset = EDescriptorTypeOffsets_Buffer + ResourceHandle_getId(buf->readHandle);

			deviceExt->device->lpVtbl->CreateShaderResourceView(
				deviceExt->device,
				bufExt->buffer,
				&srv,
				(D3D12_CPU_DESCRIPTOR_HANDLE) { .ptr = heap.cpuHandle.ptr + heap.cpuIncrement * offset }
			);
		}

		//Create writable buffer

		if (flags & EGraphicsResourceFlag_ShaderWrite) {

			D3D12_UNORDERED_ACCESS_VIEW_DESC uav = (D3D12_UNORDERED_ACCESS_VIEW_DESC) {
				.Format = DXGI_FORMAT_R32_TYPELESS,
				.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
				.Buffer = (D3D12_BUFFER_UAV) {
					.NumElements = (U32)(buf->resource.size / 4),
					.Flags = D3D12_BUFFER_UAV_FLAG_RAW
				}
			};

			U64 offset = EDescriptorTypeOffsets_RWBuffer + ResourceHandle_getId(buf->writeHandle);

			deviceExt->device->lpVtbl->CreateUnorderedAccessView(
				deviceExt->device,
				bufExt->buffer,
				NULL,
				&uav,
				(D3D12_CPU_DESCRIPTOR_HANDLE) { .ptr = heap.cpuHandle.ptr + heap.cpuIncrement * offset }
			);
		}
	}

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && CharString_length(name)) {
		gotoIfError(clean, CharString_toUTF16x(name, &name16))
		gotoIfError(clean, dxCheck(bufExt->buffer->lpVtbl->SetName(bufExt->buffer, name16.ptr)))
	}

	bufExt->lastAccess = D3D12_BARRIER_ACCESS_NO_ACCESS;

clean:
	ListU16_freex(&name16);
	return err;
}

Error DeviceBufferRef_flush(void *commandBufferExt, GraphicsDeviceRef *deviceRef, DeviceBufferRef *pending) {

	DxCommandBufferState *commandBuffer = (DxCommandBufferState*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	DeviceBuffer *buffer = DeviceBufferRef_ptr(pending);
	DxDeviceBuffer *bufferExt = DeviceBuffer_ext(buffer, Dx);

	Error err = Error_none();

	Bool isInFlight = false;
	ListRefPtr *currentFlight = &device->resourcesInFlight[(device->submitId - 1) % 3];
	DeviceBufferRef *tempStagingResource = NULL;

	gotoIfError(clean, ListD3D12_BUFFER_BARRIER_reservex(&deviceExt->bufferTransitions, 2))

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

	if(!isInFlight && buffer->resource.mappedMemoryExt)
		for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {

			DevicePendingRange range = buffer->pendingChanges.ptr[j];

			U64 start = range.buffer.startRange;
			U64 len = range.buffer.endRange - range.buffer.startRange;

			Buffer dst = Buffer_createRef((U8*)buffer->resource.mappedMemoryExt + start, len);
			Buffer src = Buffer_createRefConst(buffer->cpuData.ptr + start, len);

			Buffer_copy(dst, src);
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

		if (allocRange >= 16 * MIBI) {		//Resource is too big, allocate dedicated staging resource

			gotoIfError(clean, GraphicsDeviceRef_createBuffer(
				deviceRef,
				EDeviceBufferUsage_None, EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit,
				CharString_createRefCStrConst("Dedicated staging buffer"),
				allocRange, &tempStagingResource
			))

			DeviceBuffer *stagingResource = DeviceBufferRef_ptr(tempStagingResource);
			DxDeviceBuffer *stagingResourceExt = DeviceBuffer_ext(stagingResource, Dx);
			U8 *location = stagingResource->resource.mappedMemoryExt;

			//Copy into our buffer

			allocRange = 0;

			for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {

				const BufferRange bufferj = buffer->pendingChanges.ptr[j].buffer;
				U64 len = bufferj.endRange - bufferj.startRange;

				Buffer_copy(
					Buffer_createRef(location + allocRange, len),
					Buffer_createRefConst(buffer->cpuData.ptr + bufferj.startRange, len)
				);

				gotoIfError(clean, DxDeviceBuffer_transition(
					stagingResourceExt,
					D3D12_BARRIER_SYNC_COPY,
					D3D12_BARRIER_ACCESS_COPY_SOURCE,
					&deviceExt->bufferTransitions,
					&dependency
				))

				gotoIfError(clean, DxDeviceBuffer_transition(
					bufferExt,
					D3D12_BARRIER_SYNC_COPY,
					D3D12_BARRIER_ACCESS_COPY_DEST,
					&deviceExt->bufferTransitions,
					&dependency
				))

				if(dependency.NumBarriers) {
					commandBuffer->buffer->lpVtbl->Barrier(commandBuffer->buffer, 1, &dependency);
					ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions);
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

			gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, tempStagingResource))
			tempStagingResource = NULL;
		}

		//Use staging buffer

		else {

			AllocationBuffer *stagingBuffer = &device->stagingAllocations[(device->submitId - 1) % 3];
			DeviceBuffer *staging = DeviceBufferRef_ptr(device->staging);
			DxDeviceBuffer *stagingExt = DeviceBuffer_ext(staging, Dx);

			U8 *defaultLocation = (U8*) 1, *location = defaultLocation;
			Error temp = AllocationBuffer_allocateBlockx(stagingBuffer, allocRange, 4, (const U8**) &location);

			if(temp.genericError && location == defaultLocation)		//Something else went wrong
				gotoIfError(clean, temp)

			//We re-create the staging buffer to fit the new allocation.

			if (temp.genericError) {

				U64 prevSize = DeviceBufferRef_ptr(device->staging)->resource.size;

				//Allocate new staging buffer.

				U64 newSize = prevSize * 2 + allocRange * 3;
				gotoIfError(clean, GraphicsDeviceRef_resizeStagingBuffer(deviceRef, newSize))
				gotoIfError(clean, AllocationBuffer_allocateBlockx(stagingBuffer, allocRange, 4, (const U8**) &location))

				staging = DeviceBufferRef_ptr(device->staging);
				stagingExt = DeviceBuffer_ext(staging, Dx);
			}

			//Copy into our buffer

			allocRange = 0;

			for(U64 j = 0; j < buffer->pendingChanges.length; ++j) {

				const BufferRange bufferj = buffer->pendingChanges.ptr[j].buffer;
				U64 len = bufferj.endRange - bufferj.startRange;

				Buffer_copy(
					Buffer_createRef(location + allocRange, len),
					Buffer_createRefConst(buffer->cpuData.ptr + bufferj.startRange, len)
				);

				gotoIfError(clean, DxDeviceBuffer_transition(
					bufferExt,
					D3D12_BARRIER_SYNC_COPY,
					D3D12_BARRIER_ACCESS_COPY_DEST,
					&deviceExt->bufferTransitions,
					&dependency
				))

				if(!ListRefPtr_contains(*currentFlight, device->staging, 0, NULL)) {

					gotoIfError(clean, DxDeviceBuffer_transition(						//Ensure resource is transitioned
						stagingExt,
						D3D12_BARRIER_SYNC_COPY,
						D3D12_BARRIER_ACCESS_COPY_SOURCE,
						&deviceExt->bufferTransitions,
						&dependency
					))

					RefPtr_inc(device->staging);
					gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, device->staging))		//Add to in flight
				}

				if(dependency.NumBarriers) {
					commandBuffer->buffer->lpVtbl->Barrier(commandBuffer->buffer, 1, &dependency);
					ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions);
				}

				commandBuffer->buffer->lpVtbl->CopyBufferRegion(
					commandBuffer->buffer,
					bufferExt->buffer,
					bufferj.startRange,
					stagingExt->buffer,
					allocRange + (location - stagingBuffer->buffer.ptr),
					len
				);

				allocRange += len;
			}
		}
	}

	if(!(buffer->resource.flags & EGraphicsResourceFlag_CPUBacked))
		Buffer_freex(&buffer->cpuData);

	buffer->isFirstFrame = buffer->isPending = buffer->isPendingFullCopy = false;
	gotoIfError(clean, ListDevicePendingRange_clear(&buffer->pendingChanges))

	if(RefPtr_inc(pending))
		gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, pending))

	if (device->pendingBytes >= device->flushThreshold)
		gotoIfError(clean, DxGraphicsDevice_flush(deviceRef, commandBuffer))

clean:
	ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions);
	DeviceBufferRef_dec(&tempStagingResource);
	return err;
}
