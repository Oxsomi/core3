/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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
#include "platforms/ext/stringx.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/directx12/dx_device.h"
#include "graphics/directx12/dx_buffer.h"
#include "graphics/directx12/directx12.h"

const U64 TLASExt_size = sizeof(DxTLAS);

Bool TLAS_freeExt(TLAS *tlas) { (void)tlas; return true; }		//No-op

Bool TLAS_getInstanceDataCpuInternal(const TLAS *tlas, U64 i, TLASInstanceData **result);

Error TLASRef_flush(void *commandBufferExt, GraphicsDeviceRef *deviceRef, TLASRef *pending) {

	DxCommandBufferState *commandBuffer = (DxCommandBufferState*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	ListRefPtr *currentFlight = &device->resourcesInFlight[(device->submitId - 1) % 3];

	DeviceBufferRef *tempInstances = NULL;		//CPU visible buffer allocated only for staging
	DeviceBufferRef *tempScratch = NULL;		//Scratch buffer

	TLAS *tlas = TLASRef_ptr(pending);

	Error err = Error_none();
	CharString tmp = CharString_createNull();

	if(tlas->base.asConstructionType == ETLASConstructionType_Serialized)
		return Error_unsupportedOperation(0, "TLASRef_flush()::serialized not supported yet");		//TODO:

	U64 instancesU64 = 0;
	U64 stride = sizeof(TLASInstanceStatic);

	if (tlas->useDeviceMemory)
		instancesU64 = tlas->deviceData.len / stride;

	else instancesU64 = tlas->cpuInstancesStatic.length;

	if(instancesU64 >> 24)
		gotoIfError(clean, Error_outOfBounds(
			0, instancesU64, 1 << 24, "TLASRef_flush() only instance count of <U24_MAX is supported"
		))

	//Convert to DXR dependent version

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

	if(tlas->base.flags & ERTASBuildFlags_AllowUpdate)
		flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

	if(tlas->base.flags & ERTASBuildFlags_AllowCompaction)
		flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;

	if(tlas->base.flags & ERTASBuildFlags_FastTrace)
		flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	if(tlas->base.flags & ERTASBuildFlags_FastBuild)
		flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;

	if(tlas->base.flags & ERTASBuildFlags_MinimizeMemory)
		flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY;

	if(tlas->base.flags & ERTASBuildFlags_IsUpdate)
		flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;

	//Get build size to allocate scratch and final buffer

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInfo = (D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS) {
		.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
		.Flags = flags,
		.NumDescs = (U32) instancesU64,
		.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY
	};

	D3D12_BARRIER_GROUP dep = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_BUFFER };

	if(tlas->base.asConstructionType == ETLASConstructionType_Instances) {

		DeviceData instances = tlas->deviceData;

		//We have to temporarily allocate a device mem buffer

		if (!tlas->useDeviceMemory) {

			gotoIfError(clean, CharString_formatx(
				&tmp, "%.*s instances buffer", CharString_length(tlas->base.name), tlas->base.name.ptr
			))

			gotoIfError(clean, GraphicsDeviceRef_createBuffer(
				deviceRef,
				EDeviceBufferUsage_ASReadExt,
				EGraphicsResourceFlag_CPUAllocatedBit,
				tmp,
				stride * instancesU64,
				&tempInstances
			))

			CharString_freex(&tmp);

			//Directly copy the data is allowed, because it's not in flight and it's on the CPU

			void *mem = DeviceBufferRef_ptr(tempInstances)->resource.mappedMemoryExt;

			Buffer_copy(
				Buffer_createRef(mem, stride * instancesU64),
				Buffer_createRefConst(tlas->cpuInstancesStatic.ptr, stride * instancesU64)
			);

			//We have to transform the CPU-sided buffer to a GPU buffer address

			for (U64 i = 0; i < instancesU64; ++i) {

				TLASInstanceData *dat = NULL;
				TLAS_getInstanceDataCpuInternal(tlas, i, &dat);

				if(!dat->blasCpu)
					continue;

				const U64 *blasAddress = &dat->blasDeviceAddress;
				U64 offset = (const U8*)blasAddress - (((const U8*)tlas->cpuInstancesStatic.ptr) + stride * i);

				*(U64*)((U8*)mem + offset) = getDxDeviceAddress((DeviceData) {
					.buffer = BLASRef_ptr(dat->blasCpu)->base.asBuffer
				});
			}

			instances = (DeviceData) { .buffer = tempInstances, .len = stride * instancesU64 };

			gotoIfError(clean, DxDeviceBuffer_transition(
				DeviceBuffer_ext(DeviceBufferRef_ptr(tempInstances), Dx),
				D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
				D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,
				&deviceExt->bufferTransitions,
				&dep
			))
		}

		else gotoIfError(clean, DxDeviceBuffer_transition(
			DeviceBuffer_ext(DeviceBufferRef_ptr(tlas->deviceData.buffer), Dx),
			D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
			D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,
			&deviceExt->bufferTransitions,
			&dep
		))

		buildInfo.InstanceDescs = getDxLocation(instances, 0);
	}

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO sizes =
		(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO) { 0 };

	deviceExt->device->lpVtbl->GetRaytracingAccelerationStructurePrebuildInfo(
		deviceExt->device,
		&buildInfo,
		&sizes
	);

	//Allocate scratch and final buffer

	gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_ASExt,
		EGraphicsResourceFlag_None,
		tlas->base.name,
		sizes.ResultDataMaxSizeInBytes,
		&tlas->base.asBuffer
	))

	gotoIfError(clean, CharString_formatx(
		&tmp, "%.*s scratch buffer", CharString_length(tlas->base.name), tlas->base.name.ptr
	))

	gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_ScratchExt,
		EGraphicsResourceFlag_None,
		tmp,
		tlas->base.flags & ERTASBuildFlags_IsUpdate ? sizes.UpdateScratchDataSizeInBytes : sizes.ScratchDataSizeInBytes,
		&tempScratch
	))

	CharString_freex(&tmp);

	//Delete temporary resource as soon as possible (safely)

	if(tempInstances) {
		gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, tempInstances))
		tempInstances = NULL;
	}

	//Queue build

	gotoIfError(clean, DxDeviceBuffer_transition(
		DeviceBuffer_ext(DeviceBufferRef_ptr(tlas->base.asBuffer), Dx),
		D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
		D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE,
		&deviceExt->bufferTransitions,
		&dep
	))

	gotoIfError(clean, DxDeviceBuffer_transition(
		DeviceBuffer_ext(DeviceBufferRef_ptr(tempScratch), Dx),
		D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
		D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE,
		&deviceExt->bufferTransitions,
		&dep
	))

	commandBuffer->buffer->lpVtbl->Barrier(commandBuffer->buffer, 1, &dep);

	ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions);

	D3D12_GPU_VIRTUAL_ADDRESS dstAS = DeviceBufferRef_ptr(tlas->base.asBuffer)->resource.deviceAddress;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildAs = (D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC) {
		.DestAccelerationStructureData = dstAS,
		.Inputs = buildInfo,
		.ScratchAccelerationStructureData = DeviceBufferRef_ptr(tempScratch)->resource.deviceAddress
	};

	if(tlas->base.parent) {
		TLAS *parent = TLASRef_ptr(tlas->base.parent);
		buildAs.DestAccelerationStructureData = DeviceBufferRef_ptr(parent->base.asBuffer)->resource.deviceAddress;
	}

	commandBuffer->buffer->lpVtbl->BuildRaytracingAccelerationStructure(commandBuffer->buffer, &buildAs, 0, NULL);

	//Add as flight (keep alive extra)

	gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, pending))
	RefPtr_inc(pending);

	//We mark scratch buffer as delete, we do this by pushing it as a current flight resource
	//And losing the reference from our object

	gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, tempScratch))
	tempScratch = NULL;

	//Add as descriptor

	D3D12_SHADER_RESOURCE_VIEW_DESC resourceView = (D3D12_SHADER_RESOURCE_VIEW_DESC) {
		.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.RaytracingAccelerationStructure = (D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV) {
			.Location = dstAS
		}
	};

	const DxHeap heap = deviceExt->heaps[EDescriptorHeapType_Resources];

	U64 id = EDescriptorTypeOffsets_TLASExt + ResourceHandle_getId(tlas->handle);

	deviceExt->device->lpVtbl->CreateShaderResourceView(
		deviceExt->device,
		NULL,
		&resourceView,
		(D3D12_CPU_DESCRIPTOR_HANDLE) { .ptr = heap.cpuHandle.ptr + heap.cpuIncrement * id }
	);

	tlas->base.isCompleted = true;

clean:
	CharString_freex(&tmp);
	DeviceBufferRef_dec(&tempInstances);
	DeviceBufferRef_dec(&tempScratch);
	return err;
}
