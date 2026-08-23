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

//graphics/d3d12/generic/dx_opacity_micromap.c

#include "graphics/generic/opacity_micromap.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/d3d12/dx_buffer.h"
#include "graphics/d3d12/direct3d12.h"
#include "graphics/d3d12/dx_device.h"
#include "graphics/d3d12/dx_interface.h"
#include "types/container/list_impl.h"
#include "types/container/string.h"
#include "types/base/error.h"

TListNamedImpl(ListDxOMMHistogram);

Bool DX_WRAP_FUNC(OpacityMicromap_init)(OpacityMicromap *micromap, Error *e_rr) {

	Bool s_uccess = true;

	const Allocator *alloc = GraphicsDeviceRef_getAlloc(micromap->base.device);
	DxOpacityMicromap *micromapExt = OpacityMicromap_ext(micromap, Dx);
	CharString tmp = CharString_createNull();

	//On D3D12 an OMM array IS an acceleration structure: same build call, same prebuild info call, so the
	// histogram is prepared here in the shape the build will hand over.

	gotoIfError3(clean, ListDxOMMHistogram_resize(
		&micromapExt->histogram, micromap->usages.length, alloc, e_rr
	));

	for (U64 i = 0; i < micromap->usages.length; ++i) {

		const OpacityMicromapUsage usage = micromap->usages.ptr[i];

		D3D12_RAYTRACING_OPACITY_MICROMAP_FORMAT format = D3D12_RAYTRACING_OPACITY_MICROMAP_FORMAT_OC1_2_STATE;

		switch ((EOpacityMicromapFormat) usage.format) {

			case EOpacityMicromapFormat_Opacity2State:
				format = D3D12_RAYTRACING_OPACITY_MICROMAP_FORMAT_OC1_2_STATE;
				break;

			case EOpacityMicromapFormat_Opacity4State:
				format = D3D12_RAYTRACING_OPACITY_MICROMAP_FORMAT_OC1_4_STATE;
				break;

			default:
				retError(clean, Error_unsupportedOperation(0, "D3D12OpacityMicromap_init() unsupported format"));
		}

		micromapExt->histogram.ptrNonConst[i] = (D3D12_RAYTRACING_OPACITY_MICROMAP_HISTOGRAM_ENTRY) {
			.Count = usage.count,
			.SubdivisionLevel = usage.subdivisionLevel,
			.Format = format
		};
	}

	micromapExt->array = (D3D12_RAYTRACING_OPACITY_MICROMAP_ARRAY_DESC) {
		.NumOmmHistogramEntries = (U32) micromapExt->histogram.length,
		.pOmmHistogram = micromapExt->histogram.ptr,
		.InputBuffer = getDxLocation(micromap->inputBuffer, 0),
		.PerOmmDescs = (D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE) {
			.StartAddress = getDxLocation(micromap->entryBuffer, 0),
			.StrideInBytes = micromap->entryStride
		}
	};

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags =
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

	if(micromap->base.flags & ERTASBuildFlags_AllowCompaction)
		flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;

	if(micromap->base.flags & ERTASBuildFlags_FastTrace)
		flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	if(micromap->base.flags & ERTASBuildFlags_FastBuild)
		flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;

	//NumDescs counts array descs rather than entries, and this object is always exactly one array

	micromapExt->inputs = (D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS) {
		.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_OPACITY_MICROMAP_ARRAY,
		.Flags = flags,
		.NumDescs = 1,
		.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
		.pOpacityMicromapArrayDesc = &micromapExt->array
	};

	GraphicsDevice *device = GraphicsDeviceRef_ptr(micromap->base.device);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO sizes =
		(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO) { 0 };

	deviceExt->device->lpVtbl->GetRaytracingAccelerationStructurePrebuildInfo(
		deviceExt->device,
		&micromapExt->inputs,
		&sizes
	);

	gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
		micromap->base.device,
		EDeviceBufferUsage_ASExt,
		EGraphicsResourceFlag_None,
		NULL,
		&micromap->base.name,
		sizes.ResultDataMaxSizeInBytes,
		&micromap->base.asBuffer, e_rr
	));

	//A zero scratch size is legal (WARP reports exactly that for an OMM array), and a zero sized buffer is not

	if (sizes.ScratchDataSizeInBytes) {

		gotoIfError3(clean, CharString_format(
			alloc, &tmp, e_rr, "%.*s scratch buffer",
			CharString_length(micromap->base.name), micromap->base.name.ptr
		));

		gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
			micromap->base.device,
			EDeviceBufferUsage_ScratchExt,
			EGraphicsResourceFlag_None,
			NULL,
			&tmp,
			sizes.ScratchDataSizeInBytes,
			&micromap->base.tempScratchBuffer, e_rr
		));
	}

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

void DX_WRAP_FUNC(OpacityMicromap_free)(OpacityMicromap *micromap) {

	const Allocator *alloc = GraphicsDeviceRef_getAlloc(micromap->base.device);
	DxOpacityMicromap *micromapExt = OpacityMicromap_ext(micromap, Dx);

	ListDxOMMHistogram_free(&micromapExt->histogram, alloc);
}

Bool DX_WRAP_FUNC(OpacityMicromapRef_flush)(
	void *commandBufferExt, GraphicsDeviceRef *deviceRef, OpacityMicromapRef *pending, Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	DxCommandBufferState *commandBuffer = (DxCommandBufferState*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->fifId];

	OpacityMicromap *micromap = OpacityMicromapRef_ptr(pending);
	DxOpacityMicromap *micromapExt = OpacityMicromap_ext(micromap, Dx);

	//A micromap has no update mode, so a completed one never rebuilds

	if(micromap->base.isCompleted)
		return s_uccess;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildAs = (D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC) {
		.DestAccelerationStructureData = DeviceBufferRef_ptr(micromap->base.asBuffer)->resource.deviceAddress,
		.Inputs = micromapExt->inputs,
		.ScratchAccelerationStructureData = !micromap->base.tempScratchBuffer ? 0 :
			DeviceBufferRef_ptr(micromap->base.tempScratchBuffer)->resource.deviceAddress
	};

	commandBuffer->buffer->lpVtbl->BuildRaytracingAccelerationStructure(commandBuffer->buffer, &buildAs, 0, NULL);

	//Keep the object alive for the frame; the scratch is never needed again after the build, so the flight
	// takes the reference the object held, exactly like a non updatable BLAS.

	if(!ListRefPtr_contains(*currentFlight, pending, 0, NULL)) {
		gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, pending, alloc, e_rr));
		RefPtr_inc(pending);
	}

	if(
		micromap->base.tempScratchBuffer &&
		!ListRefPtr_contains(*currentFlight, micromap->base.tempScratchBuffer, 0, NULL)
	) {
		gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, micromap->base.tempScratchBuffer, alloc, e_rr));
		micromap->base.tempScratchBuffer = NULL;
	}

	micromap->base.isCompleted = true;

clean:
	return s_uccess;
}
