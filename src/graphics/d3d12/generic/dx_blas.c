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
#include "platforms/ext/stringx.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/d3d12/dx_device.h"
#include "graphics/d3d12/dx_buffer.h"
#include "graphics/d3d12/direct3d12.h"

Bool DX_WRAP_FUNC(BLAS_free)(BLAS *blas) { (void) blas; return true; }		//No-op

Error DX_WRAP_FUNC(BLAS_init)(BLAS *blas) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(blas->base.device);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	DxBLAS *blasExt = BLAS_ext(blas, Dx);

	Error err = Error_none();
	CharString tmp = CharString_createNull();

	if(blas->base.asConstructionType == EBLASConstructionType_Serialized)
		gotoIfError(clean, Error_unsupportedOperation(0, "D3D12BLAS_init()::serialized not supported yet"))		//TODO:

	U64 primitives = 0;
	EBLASConstructionType type = (EBLASConstructionType) blas->base.asConstructionType;

	U64 vertexCount = 0;

	switch (type) {

		case EBLASConstructionType_Procedural:
			primitives = blas->aabbBuffer.len / (sizeof(F32) * 3 * 2);
			break;

		default: {

			vertexCount = blas->positionBuffer.len / blas->positionBufferStride;
			U8 stride = blas->indexFormatId == ETextureFormatId_R32u ? 12 : 6;

			if(blas->indexFormatId != ETextureFormatId_Undefined)
				primitives = blas->indexBuffer.len / stride;

			else primitives = vertexCount / 3;

			break;
		}
	}

	if(primitives >> 32)
		gotoIfError(clean, Error_outOfBounds(
			0, primitives, U32_MAX, "D3D12BLAS_init() only primitive count of <U32_MAX is supported"
		))

	blasExt->primitives = (U32) primitives;

	//Convert to DXR dependent version

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

	if(blas->base.flags & ERTASBuildFlags_AllowUpdate)
		flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

	if(blas->base.flags & ERTASBuildFlags_AllowCompaction)
		flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;

	if(blas->base.flags & ERTASBuildFlags_FastTrace)
		flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	if(blas->base.flags & ERTASBuildFlags_FastBuild)
		flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;

	if(blas->base.flags & ERTASBuildFlags_MinimizeMemory)
		flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY;

	if(blas->base.flags & ERTASBuildFlags_IsUpdate)
		flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;

	D3D12_RAYTRACING_GEOMETRY_DESC *geometry = &blasExt->geometry;
	*geometry = (D3D12_RAYTRACING_GEOMETRY_DESC) { 0 };

	if(blas->base.flagsExt & EBLASFlag_DisableAnyHit)
		geometry->Flags |= D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	if(blas->base.flagsExt & EBLASFlag_AvoidDuplicateAnyHit)
		geometry->Flags |= D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION;

	if(blas->base.asConstructionType == EBLASConstructionType_Geometry) {

		geometry->Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

		D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC *tri = &geometry->Triangles;
		*tri = (D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC) {
			.VertexFormat = ETextureFormatId_toDXFormat(blas->positionFormatId),
			.VertexBuffer = (D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE) {
				.StartAddress = getDxLocation(blas->positionBuffer, blas->positionOffset),
				.StrideInBytes = blas->positionBufferStride
			},
			.VertexCount = (U32) vertexCount
		};

		if (blas->indexFormatId) {
			tri->IndexFormat = ETextureFormatId_toDXFormat(blas->indexFormatId);
			tri->IndexBuffer = getDxLocation(blas->indexBuffer, 0);
			tri->IndexCount = (U32)(blas->indexBuffer.len / (blas->indexFormatId == ETextureFormat_R32u ? 4 : 2));
		}
	}

	else {
		geometry->Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
		geometry->AABBs = (D3D12_RAYTRACING_GEOMETRY_AABBS_DESC) {
			.AABBs = (D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE) {
				.StartAddress = getDxLocation(blas->aabbBuffer, blas->aabbOffset),
				.StrideInBytes = blas->aabbStride
			},
			.AABBCount = (U32) primitives
		};
	}

	blasExt->inputs = (D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS) {
		.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
		.Flags = flags,
		.NumDescs = (U32) 1,
		.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
		.pGeometryDescs = geometry
	};

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO sizes =
		(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO) { 0 };

	deviceExt->device->lpVtbl->GetRaytracingAccelerationStructurePrebuildInfo(
		deviceExt->device,
		&blasExt->inputs,
		&sizes
	);

	//Allocate scratch and final buffer

	gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		blas->base.device,
		EDeviceBufferUsage_ASExt,
		EGraphicsResourceFlag_None,
		blas->base.name,
		sizes.ResultDataMaxSizeInBytes,
		&blas->base.asBuffer
	))

	gotoIfError(clean, CharString_formatx(
		&tmp, "%.*s scratch buffer", CharString_length(blas->base.name), blas->base.name.ptr
	))

	gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		blas->base.device,
		EDeviceBufferUsage_ScratchExt,
		EGraphicsResourceFlag_None,
		tmp,
		blas->base.flags & ERTASBuildFlags_IsUpdate ? sizes.UpdateScratchDataSizeInBytes : sizes.ScratchDataSizeInBytes,
		&blas->base.tempScratchBuffer
	))

clean:
	CharString_freex(&tmp);
	return err;
}

Error DX_WRAP_FUNC(BLASRef_flush)(void *commandBufferExt, GraphicsDeviceRef *deviceRef, BLASRef *pending) {

	DxCommandBufferState *commandBuffer = (DxCommandBufferState*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	ListRefPtr *currentFlight = &device->resourcesInFlight[(device->submitId - 1) % 3];

	BLAS *blas = BLASRef_ptr(pending);
	DxBLAS *blasExt = BLAS_ext(blas, Dx);
	Error err = Error_none();

	if(blas->base.isCompleted && !(blas->base.flags & ERTASBuildFlags_AllowUpdate))		//Done
		return Error_none();

	D3D12_GPU_VIRTUAL_ADDRESS dstAS = DeviceBufferRef_ptr(blas->base.asBuffer)->resource.deviceAddress;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildAs = (D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC) {
		.DestAccelerationStructureData = dstAS,
		.Inputs = blasExt->inputs,
		.ScratchAccelerationStructureData = DeviceBufferRef_ptr(blas->base.tempScratchBuffer)->resource.deviceAddress
	};

	if(blas->base.parent) {
		BLAS *parent = BLASRef_ptr(blas->base.parent);
		buildAs.SourceAccelerationStructureData = DeviceBufferRef_ptr(parent->base.asBuffer)->resource.deviceAddress;
	}

	commandBuffer->buffer->lpVtbl->BuildRaytracingAccelerationStructure(commandBuffer->buffer, &buildAs, 0, NULL);

	//Add as flight and ensure flushes are done if too many ASes are queued this frame

	device->pendingPrimitives += blasExt->primitives;

	if(!ListRefPtr_contains(*currentFlight, pending, 0, NULL)) {
		gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, pending))
		RefPtr_inc(pending);
	}

	//We mark scratch buffer as delete, we do this by pushing it as a current flight resource;
	//And losing the reference from our object. However that's only if allow update is false.

	if(!ListRefPtr_contains(*currentFlight, blas->base.tempScratchBuffer, 0, NULL)) {

		gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, blas->base.tempScratchBuffer))

		if(!(blas->base.flags & ERTASBuildFlags_AllowUpdate))
			blas->base.tempScratchBuffer = NULL;

		else RefPtr_inc(blas->base.tempScratchBuffer);
	}

	//Ensure we don't exceed a maximum amount of time spent on the GPU

	if (device->pendingPrimitives >= device->flushThresholdPrimitives)
		gotoIfError(clean, DxGraphicsDevice_flush(deviceRef, commandBuffer))

	blas->base.isCompleted = true;

clean:
	return err;
}
