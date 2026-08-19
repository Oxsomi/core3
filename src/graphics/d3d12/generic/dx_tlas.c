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

//graphics/d3d12/generic/dx_tlas.c

#include "graphics/generic/device.h"
#include "types/base/mathi.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/d3d12/dx_device.h"
#include "graphics/d3d12/dx_buffer.h"
#include "graphics/d3d12/direct3d12.h"
#include "types/container/string.h"

void DX_WRAP_FUNC(TLAS_free)(TLAS *tlas) { (void)tlas; }        //No-op
Bool TLAS_getInstanceDataCpuInternal(const TLAS *tlas, U64 i, TLASInstanceData **result);

//Writes the instance array into its upload buffer and turns every BLAS reference into the device address the
// build actually reads.
//Split out of init because a refit has to do this again: the buffer is filled once at create, so an update
// would otherwise rebuild the structure over the instances it already had.

static void DxTLAS_fillInstances(TLAS *tlas) {

	const U64 instancesU64 = tlas->cpuInstances.length;

	Buffer cpuDat = DeviceBufferRef_ptr(tlas->tempInstanceBuffer)->cpuData;
	Buffer_memcpy(
		cpuDat,
		ListTLASInstance_bufferConst(tlas->cpuInstances)
	);

	//We have to transform the CPU-sided buffer to a GPU buffer address

	U8 *mem = (U8*) cpuDat.ptr;

	for (U64 i = 0; i < instancesU64; ++i) {

		TLASInstanceData *dat = NULL;
		TLAS_getInstanceDataCpuInternal(tlas, i, &dat);

		if(!dat->blasCpu)
			continue;

		U64 off = (const U8*)&dat->blasDeviceAddress - (const U8*)tlas->cpuInstances.ptr;
		*(U64*)(mem + off) = getDxDeviceAddress((DeviceData) { .buffer = BLASRef_ptr(dat->blasCpu)->base.asBuffer });
	}

	//cpuData is a CPU side copy rather than the mapped upload heap, so the instances only reach the GPU
	// once they are copied across.
	//The buffer's own flush would do this, but only for ranges marked dirty before the frame started, and
	// a refit fills this in while that frame is already being recorded.

	DeviceBuffer *instanceBuf = DeviceBufferRef_ptr(tlas->tempInstanceBuffer);

	if (instanceBuf->resource.mappedMemoryExt) {

		const U64 len = sizeof(TLASInstance) * instancesU64;

		Buffer_memcpy(
			Buffer_createRef(instanceBuf->resource.mappedMemoryExt, len),
			Buffer_createRefConst(cpuDat.ptr, len)
		);

		ID3D12ManualWriteTrackingResource *tracking =
			(ID3D12ManualWriteTrackingResource*) instanceBuf->resource.debugExt;

		if (tracking) {
			D3D12_RANGE rangeD3D12 = (D3D12_RANGE) { .Begin = 0, .End = len };
			tracking->lpVtbl->TrackWrite(tracking, 0, &rangeD3D12);
		}
	}

	tlas->instancesDirty = false;
}

Bool DX_WRAP_FUNC(TLAS_init)(TLAS *tlas, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = tlas ? GraphicsDeviceRef_getAlloc(tlas->base.device) : NULL;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(tlas->base.device);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	DxTLAS *tlasExt = TLAS_ext(tlas, Dx);

	CharString tmp = CharString_createNull();

	if(tlas->base.asConstructionType == ETLASConstructionType_Serialized)
		retError(clean, Error_unsupportedOperation(0, "D3D12TLAS_init()::serialized not supported yet"));        //TODO:

	U64 instancesU64 = 0;
	U64 stride = sizeof(TLASInstance);

	if (tlas->useDeviceMemory)
		instancesU64 = tlas->deviceData.len / stride;

	else instancesU64 = tlas->cpuInstances.length;

	if(instancesU64 >> 24)
		retError(clean, Error_outOfBounds(
			0, instancesU64, 1 << 24, "D3D12TLAS_init() only instance count of <U24_MAX is supported"));

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

	//Get build size to allocate scratch and final buffer

	tlasExt->inputs = (D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS) {
		.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
		.Flags = flags,
		.NumDescs = (U32) instancesU64,
		.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY
	};

	if(tlas->base.asConstructionType == ETLASConstructionType_Instances) {

		DeviceData instances = tlas->deviceData;

		//We have to temporarily allocate a device mem buffer

		if (!tlas->useDeviceMemory) {

			gotoIfError3(clean, CharString_format(
				alloc,
				&tmp,
				e_rr,
				"%.*s instances buffer",
				CharString_length(tlas->base.name),
				tlas->base.name.ptr
			));

			gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
				tlas->base.device,
				EDeviceBufferUsage_ASReadExt,
				EGraphicsResourceFlag_CPUAllocated,
				NULL,
				&tmp,
				stride * instancesU64,
				&tlas->tempInstanceBuffer, e_rr
			));

			CharString_free(&tmp, alloc);

			DxTLAS_fillInstances(tlas);

			instances = (DeviceData) { .buffer = tlas->tempInstanceBuffer, .len = stride * instancesU64 };
		}

		tlasExt->inputs.InstanceDescs = getDxLocation(instances, 0);
	}

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO sizes =
		(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO) { 0 };

	deviceExt->device->lpVtbl->GetRaytracingAccelerationStructurePrebuildInfo(
		deviceExt->device,
		&tlasExt->inputs,
		&sizes
	);

	//Allocate scratch and final buffer

	gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
		tlas->base.device,
		EDeviceBufferUsage_ASExt,
		EGraphicsResourceFlag_None,
		NULL,
		&tlas->base.name,
		sizes.ResultDataMaxSizeInBytes,
		&tlas->base.asBuffer,
		e_rr
	));

	gotoIfError3(clean, CharString_format(
		alloc,
		&tmp,
		e_rr,
		"%.*s scratch buffer",
		CharString_length(tlas->base.name),
		tlas->base.name.ptr
	));

	gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
		tlas->base.device,
		EDeviceBufferUsage_ScratchExt,
		EGraphicsResourceFlag_None,
		NULL,
		&tmp,
		//One scratch buffer serves both, since the same object now does the full build and every refit after
		// it; the update size is not required to be the smaller of the two, so neither is assumed.

		tlas->base.flags & ERTASBuildFlags_AllowUpdate ?
			U64_max(sizes.ScratchDataSizeInBytes, sizes.UpdateScratchDataSizeInBytes) : sizes.ScratchDataSizeInBytes,
		&tlas->base.tempScratchBuffer,
		e_rr
	));

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

Bool DX_WRAP_FUNC(TLASRef_flush)(void *commandBufferExt, GraphicsDeviceRef *deviceRef, TLASRef *pending, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	DxCommandBufferState *commandBuffer = (DxCommandBufferState*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->fifId];

	TLAS *tlas = TLASRef_ptr(pending);
	DxTLAS *tlasExt = TLAS_ext(tlas, Dx);

	if(tlas->base.isCompleted && !(tlas->base.flags & ERTASBuildFlags_AllowUpdate))        //Done
		return s_uccess;

	//New instances only reach the GPU here, so a batch of transform changes costs one upload rather than
	// one per change.

	if(tlas->instancesDirty && !tlas->useDeviceMemory)
		DxTLAS_fillInstances(tlas);

	D3D12_GPU_VIRTUAL_ADDRESS dstAS = DeviceBufferRef_ptr(tlas->base.asBuffer)->resource.deviceAddress;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildAs = (D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC) {
		.DestAccelerationStructureData = dstAS,
		.Inputs = tlasExt->inputs,
		.ScratchAccelerationStructureData = DeviceBufferRef_ptr(tlas->base.tempScratchBuffer)->resource.deviceAddress
	};

	//A structure that was already built refits itself in place, which both APIs allow and which is what
	// keeps its device address stable across an update.
	//PERFORM_UPDATE goes on the local copy rather than on the stored inputs, since the prebuild sizes
	// were queried without it.

	if (tlas->base.isCompleted) {
		buildAs.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		buildAs.SourceAccelerationStructureData = dstAS;
	}

	commandBuffer->buffer->lpVtbl->BuildRaytracingAccelerationStructure(commandBuffer->buffer, &buildAs, 0, NULL);

	//Add as flight (keep alive extra)

	if(!ListRefPtr_contains(*currentFlight, pending, 0, NULL)) {
		gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, pending, alloc, e_rr));
		RefPtr_inc(pending);
	}

	//We mark scratch buffer as delete, we do this by pushing it as a current flight resource
	//And losing the reference from our object
	//We do the same thing on the tempInstances, since it's CPU mem only

	if(!ListRefPtr_contains(*currentFlight, tlas->base.tempScratchBuffer, 0, NULL)) {

		gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, tlas->base.tempScratchBuffer, alloc, e_rr));

		if(tlas->base.flags & ERTASBuildFlags_AllowUpdate)        //Maintain reference, rather than clear
			RefPtr_inc(tlas->base.tempScratchBuffer);

		else tlas->base.tempScratchBuffer = NULL;
	}

	if(tlas->tempInstanceBuffer && !ListRefPtr_contains(*currentFlight, tlas->tempInstanceBuffer, 0, NULL)) {

		gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, tlas->tempInstanceBuffer, alloc, e_rr));

		if(tlas->base.flags & ERTASBuildFlags_AllowUpdate)        //Maintain reference, rather than clear
			RefPtr_inc(tlas->tempInstanceBuffer);

		else tlas->tempInstanceBuffer = NULL;
	}

	tlas->base.isCompleted = true;

clean:
	return s_uccess;
}
