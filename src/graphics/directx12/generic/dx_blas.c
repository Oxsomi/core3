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
#include "graphics/generic/blas.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/directx12/dx_device.h"
#include "graphics/directx12/dx_buffer.h"
#include "graphics/directx12/directx12.h"

const U64 BLASExt_size = sizeof(DxBLAS);

Bool BLAS_freeExt(BLAS *blas) { (void)blas; return true; }

Error BLASRef_flush(void *commandBufferExt, GraphicsDeviceRef *deviceRef, BLASRef *pending) {

	DxCommandBuffer *commandBuffer = (DxCommandBuffer*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	DeviceBufferRef *tempScratch = NULL;

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->submitId % 3];

	BLAS *blas = BLASRef_ptr(pending);

	Error err = Error_none();
	CharString tmp = CharString_createNull();

	if(blas->base.asConstructionType == EBLASConstructionType_Serialized)
		return Error_unsupportedOperation(0, "BLASRef_flush()::serialized not supported yet");		//TODO:

	U64 primitives = 0;
	EBLASConstructionType type = (EBLASConstructionType) blas->base.asConstructionType;

	U64 vertexCount = 0;

	switch (type) {

		case EBLASConstructionType_Procedural:
			primitives = blas->aabbBuffer.len / (sizeof(F32) * 3 * 2);
			break;

		default: {

			vertexCount = blas->positionBuffer.len / blas->positionBufferStride;
			U8 stride = blas->indexFormatId == ETextureFormatId_R32u ? 4 : 2;

			if(blas->indexFormatId != ETextureFormatId_Undefined)
				primitives = blas->indexBuffer.len / stride / 3;

			else primitives = vertexCount / 3;

			break;
		}
	}

	if(primitives >> 32)
		gotoIfError(clean, Error_outOfBounds(
			0, primitives, U32_MAX, "BLASRef_flush() only primitive count of <U32_MAX is supported"
		))

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

	D3D12_RAYTRACING_GEOMETRY_DESC geometry = (D3D12_RAYTRACING_GEOMETRY_DESC) { 0 };

	if(blas->base.flagsExt & EBLASFlag_DisableAnyHit)
		geometry.Flags |= D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	if(blas->base.flagsExt & EBLASFlag_AvoidDuplicateAnyHit)
		geometry.Flags |= D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION;

	D3D12_BARRIER_GROUP dep = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_BUFFER };

	if(blas->base.asConstructionType == EBLASConstructionType_Geometry) {

		geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

		D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC *tri = &geometry.Triangles;
		*tri = (D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC) {
			.VertexFormat = DXFormat_toTextureFormatId(ETextureFormatId_unpack[blas->positionFormatId]),
			.VertexBuffer = (D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE) {
				.StartAddress = getDxLocation(blas->positionBuffer, blas->positionOffset),
				.StrideInBytes = blas->positionBufferStride
			},
			.VertexCount = (U32) vertexCount
		};

		gotoIfError(clean, DxDeviceBuffer_transition(
			DeviceBuffer_ext(DeviceBufferRef_ptr(blas->positionBuffer.buffer), Dx),
			D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
			D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,
			0, 0,
			&deviceExt->bufferTransitions,
			&dep
		))

		if (blas->indexFormatId) {

			tri->IndexFormat = DXFormat_toTextureFormatId(ETextureFormatId_unpack[blas->indexFormatId]);
			tri->IndexBuffer = getDxLocation(blas->indexBuffer, 0);

			gotoIfError(clean, DxDeviceBuffer_transition(
				DeviceBuffer_ext(DeviceBufferRef_ptr(blas->indexBuffer.buffer), Dx),
				D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
				D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,
				0, 0,
				&deviceExt->bufferTransitions,
				&dep
			))
		}
	}

	else {

		geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
		geometry.AABBs = (D3D12_RAYTRACING_GEOMETRY_AABBS_DESC) {
			.AABBs = (D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE) {
				.StartAddress = getDxLocation(blas->aabbBuffer, blas->aabbOffset),
				.StrideInBytes = blas->aabbStride
			},
			.AABBCount = (U32) primitives
		};

		gotoIfError(clean, DxDeviceBuffer_transition(
			DeviceBuffer_ext(DeviceBufferRef_ptr(blas->aabbBuffer.buffer), Dx),
			D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
			D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,
			0, 0,
			&deviceExt->bufferTransitions,
			&dep
		))
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInfo = (D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS) {
		.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
		.Flags = flags,
		.NumDescs = (U32) primitives,
		.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
		.pGeometryDescs = &geometry
	};

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
		blas->base.name,
		sizes.ResultDataMaxSizeInBytes,
		&blas->base.asBuffer
	))

	gotoIfError(clean, CharString_formatx(
		&tmp, "%.*s scratch buffer", CharString_length(blas->base.name), blas->base.name.ptr
	))

	gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_ScratchExt,
		EGraphicsResourceFlag_None,
		tmp,
		blas->base.flags & ERTASBuildFlags_IsUpdate ? sizes.UpdateScratchDataSizeInBytes : sizes.ScratchDataSizeInBytes,
		&tempScratch
	))

	//Queue build

	gotoIfError(clean, DxDeviceBuffer_transition(
		DeviceBuffer_ext(DeviceBufferRef_ptr(blas->base.asBuffer), Dx),
		D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
		D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE,
		0, 0,
		&deviceExt->bufferTransitions,
		&dep
	))

	gotoIfError(clean, DxDeviceBuffer_transition(
		DeviceBuffer_ext(DeviceBufferRef_ptr(tempScratch), Dx),
		D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
		D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE,
		0, 0,
		&deviceExt->bufferTransitions,
		&dep
	))

	commandBuffer->lpVtbl->Barrier(commandBuffer, 1, &dep);

	ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions);

	D3D12_GPU_VIRTUAL_ADDRESS dstAS = DeviceBufferRef_ptr(blas->base.asBuffer)->resource.deviceAddress;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildAs = (D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC) {
		.DestAccelerationStructureData = dstAS,
		.Inputs = buildInfo,
		.ScratchAccelerationStructureData = DeviceBufferRef_ptr(tempScratch)->resource.deviceAddress
	};

	if(blas->base.parent) {
		BLAS *parent = BLASRef_ptr(blas->base.parent);
		buildAs.DestAccelerationStructureData = DeviceBufferRef_ptr(parent->base.asBuffer)->resource.deviceAddress;
	}

	commandBuffer->lpVtbl->BuildRaytracingAccelerationStructure(commandBuffer, &buildAs, 0, NULL);

	//Add as flight and ensure flushes are done if too many ASes are queued this frame

	device->pendingBytes += primitives;

	gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, pending))
	RefPtr_inc(pending);

	//We mark scratch buffer as delete, we do this by pushing it as a current flight resource
	//And losing the reference from our object

	gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, tempScratch))
	tempScratch = NULL;

	//Ensure we don't exceed a maximum amount of time spent on the GPU

	if (device->pendingPrimitives >= device->flushThresholdPrimitives)
		gotoIfError(clean, DxGraphicsDevice_flush(deviceRef, commandBuffer))

	blas->base.isCompleted = true;

clean:
	DeviceBufferRef_dec(&tempScratch);
	CharString_freex(&tmp);
	return err;
}
