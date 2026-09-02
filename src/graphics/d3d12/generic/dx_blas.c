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

//graphics/d3d12/generic/dx_blas.c

#include "graphics/generic/device.h"
#include "types/base/mathi.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/opacity_micromap.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/d3d12/dx_device.h"
#include "graphics/d3d12/dx_buffer.h"
#include "graphics/d3d12/direct3d12.h"
#include "types/container/string.h"
#include "types/base/constants.h"

void DX_WRAP_FUNC(BLAS_free)(BLAS *blas) { (void) blas; }        //No-op

Bool DX_WRAP_FUNC(BLAS_init)(BLAS *blas, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = blas ? GraphicsDeviceRef_getAlloc(blas->base.device) : NULL;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(blas->base.device);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	DxBLAS *blasExt = BLAS_ext(blas, Dx);

	CharString tmp = CharString_createNull();

	if(blas->base.asConstructionType == EBLASConstructionType_Serialized)
		retError(clean, Error_unsupportedOperation(0, "D3D12BLAS_init()::serialized not supported yet"));        //TODO:

	U64 primitives = 0;
	EBLASConstructionType type = (EBLASConstructionType) blas->base.asConstructionType;

	U64 vertexCount = 0;

	switch (type) {

		case EBLASConstructionType_Serialized:
			primitives = Buffer_length(blas->cpuData) / 12;        //Conservative estimate
			break;

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
		retError(clean, Error_outOfBounds(
			0, primitives, U32_MAX, "D3D12BLAS_init() only primitive count of <U32_MAX is supported"
		));

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

	if(blas->base.flags & ERTASBuildFlags_AllowDataAccessExt)
		flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_DATA_ACCESS;

	D3D12_RAYTRACING_GEOMETRY_DESC *geometry = &blasExt->geometry;
	*geometry = (D3D12_RAYTRACING_GEOMETRY_DESC) { 0 };

	if(blas->base.flagsExt & EBLASFlag_DisableAnyHit)
		geometry->Flags |= D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	if(blas->base.flagsExt & EBLASFlag_AvoidDuplicateAnyHit)
		geometry->Flags |= D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION;

	if(blas->base.asConstructionType == EBLASConstructionType_Geometry) {

		geometry->Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

		//D3D12 has no four component 32 bit vertex position format; the w is padding the stride already covers,
		// so the three component DXGI format reads the same memory (Vulkan does the same, RGBA32f is optional there).

		DXGI_FORMAT vertexFormat = ETextureFormatId_toDXFormat(blas->positionFormatId);

		if(blas->positionFormatId == ETextureFormatId_RGBA32f)
			vertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

		D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC *tri = &geometry->Triangles;
		*tri = (D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC) {
			.VertexFormat = vertexFormat,
			.VertexBuffer = (D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE) {
				.StartAddress = getDxLocation(blas->positionBuffer, blas->positionOffset),
				.StrideInBytes = blas->positionBufferStride
			},
			.VertexCount = (U32) vertexCount
		};

		if (blas->indexFormatId) {
			tri->IndexFormat = ETextureFormatId_toDXFormat(blas->indexFormatId);
			tri->IndexBuffer = getDxLocation(blas->indexBuffer, 0);
			tri->IndexCount = (U32)(blas->indexBuffer.len / (blas->indexFormatId == ETextureFormatId_R32u ? 4 : 2));
		}

		//Opacity micromaps, stage 1.
		//D3D12 swaps the whole geometry type rather than chaining, so the triangle desc written above moves
		// into its own storage and the union member becomes the OMM pair pointing at it.
		//OpacityMicromapArray stays null on purpose: that is what says the index buffer holds only special
		// indices instead of referencing a built micromap array.

		if (blas->ommIndexFormatId) {

			blasExt->ommTriangleData = *tri;

			blasExt->ommLinkage = (D3D12_RAYTRACING_GEOMETRY_OMM_LINKAGE_DESC) {
				.OpacityMicromapIndexBuffer = (D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE) {
					.StartAddress = getDxLocation(blas->ommIndexBuffer, 0),
					.StrideInBytes =
						blas->ommIndexFormatId == ETextureFormatId_R32u ? 4 :
						(blas->ommIndexFormatId == ETextureFormatId_R8u ? 1 : 2)
				},
				.OpacityMicromapIndexFormat = ETextureFormatId_toDXFormat(blas->ommIndexFormatId),

				//The built OMM array's address when one is linked; 0 keeps the special index only form

				.OpacityMicromapArray = !blas->ommMicromap ? 0 : DeviceBufferRef_ptr(
					OpacityMicromapRef_ptr(blas->ommMicromap)->base.asBuffer
				)->resource.deviceAddress
			};

			geometry->Type = D3D12_RAYTRACING_GEOMETRY_TYPE_OMM_TRIANGLES;

			geometry->OmmTriangles = (D3D12_RAYTRACING_GEOMETRY_OMM_TRIANGLES_DESC) {
				.pTriangles = &blasExt->ommTriangleData,
				.pOmmLinkage = &blasExt->ommLinkage
			};
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

	gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
		blas->base.device,
		EDeviceBufferUsage_ASExt,
		EGraphicsResourceFlag_None,
		NULL,
		&blas->base.name,
		sizes.ResultDataMaxSizeInBytes,
		&blas->base.asBuffer,
		e_rr
	));

	gotoIfError3(clean, CharString_format(
		alloc,
		&tmp,
		e_rr,
		"%.*s scratch buffer",
		CharString_length(blas->base.name),
		blas->base.name.ptr
	));

	//TODO: cache the scratch buffer on the device rather than allocating one per structure.
	// Scratch is only live between a build and the submit that runs it, so structures built in the same
	// submit could share one buffer sized to the largest of them, and a build that is not a refit could
	// hand it straight back. That is a real saving on a scene of many structures, where the scratch can
	// rival the structures themselves in peak footprint.

	gotoIfError3(clean, GraphicsDeviceRef_createBuffer(

		blas->base.device,
		EDeviceBufferUsage_ScratchExt,
		EGraphicsResourceFlag_None,
		NULL,
		&tmp,

		//One scratch buffer serves both, since the same object now does the full build and every refit after
		// it; the update size is not required to be the smaller of the two, so neither is assumed.

		blas->base.flags & ERTASBuildFlags_AllowUpdate ?
			U64_max(sizes.ScratchDataSizeInBytes, sizes.UpdateScratchDataSizeInBytes) : sizes.ScratchDataSizeInBytes,
		&blas->base.tempScratchBuffer,
		e_rr
	));

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

//Compaction, in the two halves the interface splits it into. See the Vulkan pair for the reasoning; the
//only difference here is that a D3D12 acceleration structure IS its buffer, addressed by that buffer's GPU
//virtual address, so there is no separate structure object and repointing the buffer is the whole swap.

Bool DX_WRAP_FUNC(BLASRef_prepareCompact)(GraphicsDeviceRef *deviceRef, BLASRef *blasRef, Bool *recorded, Error *e_rr) {

	Bool s_uccess = true;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	BLAS *blas = BLASRef_ptr(blasRef);

	const U32 query = blas->base.compactionQuery;
	U32 pool = 0, slot = 0;
	GraphicsDevice_compactionQueryPool(query, DX_COMPACTION_QUERIES_BASE, &pool, &slot);

	const U64 offset = (U64) slot * sizeof(U64);

	if(pool >= deviceExt->compactionReadbackPools.length)
		retError(clean, Error_invalidState(
			0, "D3D12BLASRef_prepareCompact() the compacted size slot has no storage behind it"
		));

	DeviceBufferRef *readbackRef = (DeviceBufferRef*) deviceExt->compactionReadbackPools.ptrNonConst[pool];

	const U64 compactedSize = *(const U64*) (
		(const U8*) DeviceBufferRef_ptr(readbackRef)->resource.mappedMemoryExt + offset
	);

	//The slot has been consumed either way, so it goes back before any early out below can take it out of
	// circulation.

	blas->base.compactionQuery = U32_MAX;
	GraphicsDevice_releaseCompactionQuery(device, query, alloc);

	//A driver is allowed to report no saving. Leaving recorded false keeps a pointless copy out of the
	// command buffer entirely.

	if(!compactedSize || compactedSize >= DeviceBufferRef_ptr(blas->base.asBuffer)->resource.size) {
		blas->base.isCompacted = true;
		goto clean;
	}

	gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_ASExt, EGraphicsResourceFlag_None, NULL,
		&blas->base.name, compactedSize, &blas->base.pendingCompactBuffer, e_rr
	));

	*recorded = true;

clean:

	if(!s_uccess)
		RefPtr_dec(&blas->base.pendingCompactBuffer);

	return s_uccess;
}

Bool DX_WRAP_FUNC(BLASRef_compact)(
	void *commandBufferExt, GraphicsDeviceRef *deviceRef, BLASRef *blasRef, Error *e_rr
) {

	Bool s_uccess = true;

	DxCommandBufferState *commandBuffer = (DxCommandBufferState*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	BLAS *blas = BLASRef_ptr(blasRef);

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->fifId];

	if(!blas->base.pendingCompactBuffer)
		retError(clean, Error_invalidState(0, "D3D12BLASRef_compact() nothing was prepared for this structure"));

	//Reserved before the copy is recorded, because once it is in the command buffer the structure it reads
	// cannot be released and a failed retirement would have no undo.

	gotoIfError3(clean, ListRefPtr_reserve(currentFlight, currentFlight->length + 1, alloc, e_rr));

	commandBuffer->buffer->lpVtbl->CopyRaytracingAccelerationStructure(
		commandBuffer->buffer,
		DeviceBufferRef_ptr(blas->base.pendingCompactBuffer)->resource.deviceAddress,
		DeviceBufferRef_ptr(blas->base.asBuffer)->resource.deviceAddress,
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_COMPACT
	);

	//The structure being replaced cannot go back yet: the copy that reads it has only been recorded. Its
	// reference MOVES to the flight list, which releases it once this submit completes.

	ListRefPtr_pushBack(currentFlight, blas->base.asBuffer, alloc, NULL);

	blas->base.asBuffer = blas->base.pendingCompactBuffer;
	blas->base.isCompacted = true;

	blas->base.pendingCompactBuffer = NULL;

	//The address REALLY changes here, so dependents are marked again: a pending TLAS build riding this same
	// submit may have resolved the old address and cleared the record time mark already.

	gotoIfError3(clean, GraphicsDeviceRef_markTlasesStaleForBLAS(deviceRef, blasRef, true, e_rr));

clean:
	return s_uccess;
}

Bool DX_WRAP_FUNC(BLASRef_flush)(void *commandBufferExt, GraphicsDeviceRef *deviceRef, BLASRef *pending, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	DxCommandBufferState *commandBuffer = (DxCommandBufferState*) commandBufferExt;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->fifId];

	BLAS *blas = BLASRef_ptr(pending);
	DxBLAS *blasExt = BLAS_ext(blas, Dx);

	if(blas->base.isCompleted && !(blas->base.flags & ERTASBuildFlags_AllowUpdate))        //Done
		return s_uccess;

	DeviceBuffer *asBuffer = DeviceBufferRef_ptr(blas->base.asBuffer);
	D3D12_GPU_VIRTUAL_ADDRESS dstAS = asBuffer->resource.deviceAddress;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildAs = (D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC) {
		.DestAccelerationStructureData = dstAS,
		.Inputs = blasExt->inputs,
		.ScratchAccelerationStructureData = DeviceBufferRef_ptr(blas->base.tempScratchBuffer)->resource.deviceAddress
	};

	//A structure that was already built refits itself in place, which both APIs allow and which is what
	// keeps its device address stable across an update.
	//PERFORM_UPDATE goes on the local copy rather than on the stored inputs, since the prebuild sizes
	// were queried without it.

	if (blas->base.isCompleted) {
		buildAs.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		buildAs.SourceAccelerationStructureData = dstAS;
	}

	commandBuffer->buffer->lpVtbl->BuildRaytracingAccelerationStructure(commandBuffer->buffer, &buildAs, 0, NULL);

	//The compacted size is a property of the BUILT structure, so it is emitted right behind the build and
	// read back later, once the submit holding both has completed.

	const Bool wantsCompaction =
		(blas->base.flags & ERTASBuildFlags_AllowCompaction) &&
		!blas->base.isCompacted &&
		blas->base.compactionQuery == U32_MAX;

	if (wantsCompaction) {

		U32 query = U32_MAX;
		Bool needsPool = false;

		const U64 poolBytes =
			(U64) GraphicsDevice_compactionPoolSize(device, DX_COMPACTION_QUERIES_BASE) * sizeof(U64);

		gotoIfError3(clean, GraphicsDevice_claimCompactionQuery(
			device, DX_COMPACTION_QUERIES_BASE, &query, &needsPool, e_rr
		));

		if(needsPool) {

			CharString emitName = CharString_createRefCStrConst("BLAS compacted sizes");
			DeviceBufferRef *emitPool = NULL;

			//InternalWeakDeviceRef, because the DEVICE owns these pools: a strong ref here would be a
			// cycle (pool holds device, device holds pool) that keeps both alive forever, and the pools are
			// already released in the device's own teardown.

			gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
				blas->base.device, EDeviceBufferUsage_ScratchExt, EGraphicsResourceFlag_InternalWeakDeviceRef,
				NULL, &emitName, poolBytes, &emitPool, e_rr
			));

			gotoIfError3(clean, ListRefPtr_pushBack(&deviceExt->compactionEmitPools, emitPool, alloc, e_rr));

			CharString readbackName = CharString_createRefCStrConst("BLAS compacted sizes readback");
			DeviceBufferRef *readbackPool = NULL;

			gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
				blas->base.device, EDeviceBufferUsage_None,
				EGraphicsResourceFlag_CPUAllocatedBit | EGraphicsResourceFlag_CPUReadBit |
				EGraphicsResourceFlag_InternalWeakDeviceRef,
				NULL, &readbackName, poolBytes, &readbackPool, e_rr
			));

			gotoIfError3(clean, ListRefPtr_pushBack(&deviceExt->compactionReadbackPools, readbackPool, alloc, e_rr));
		}

		U32 pool = 0, slot = 0;
		GraphicsDevice_compactionQueryPool(query, DX_COMPACTION_QUERIES_BASE, &pool, &slot);

		const U64 offset = (U64) slot * sizeof(U64);

		DeviceBufferRef *emitRef = (DeviceBufferRef*) deviceExt->compactionEmitPools.ptr[pool];
		DeviceBufferRef *readbackRef = (DeviceBufferRef*) deviceExt->compactionReadbackPools.ptr[pool];

		DxDeviceBuffer *emitExt = DeviceBuffer_ext(DeviceBufferRef_ptr(emitRef), Dx);

		D3D12_BARRIER_GROUP dependency = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_BUFFER };

		//The pool is shared, so the previous structure in this same list left it in COPY_SOURCE. The emit
		// writes as a UAV, so it has to come back before the write and go again before the copy.

		gotoIfError3(clean, DxDeviceBuffer_transition(
			emitExt, D3D12_BARRIER_SYNC_EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO,
			D3D12_BARRIER_ACCESS_UNORDERED_ACCESS, &deviceExt->bufferTransitions, &dependency, alloc, e_rr
		));

		gotoIfError3(clean, DxDeviceBuffer_transition(
			DeviceBuffer_ext(asBuffer, Dx),
			D3D12_BARRIER_SYNC_EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO,
			D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,
			&deviceExt->bufferTransitions, &dependency, alloc, e_rr
		));

		if(dependency.NumBarriers) {
			commandBuffer->buffer->lpVtbl->Barrier(commandBuffer->buffer, 1, &dependency);
			ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions, e_rr);
		}

		const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC postbuild =
			(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC) {
				.DestBuffer = DeviceBufferRef_ptr(emitRef)->resource.deviceAddress + offset,
				.InfoType = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_COMPACTED_SIZE
			};

		commandBuffer->buffer->lpVtbl->EmitRaytracingAccelerationStructurePostbuildInfo(
			commandBuffer->buffer, &postbuild, 1, &dstAS
		);

		dependency = (D3D12_BARRIER_GROUP) { .Type = D3D12_BARRIER_TYPE_BUFFER };

		gotoIfError3(clean, DxDeviceBuffer_transition(
			emitExt, D3D12_BARRIER_SYNC_COPY, D3D12_BARRIER_ACCESS_COPY_SOURCE,
			&deviceExt->bufferTransitions, &dependency, alloc, e_rr
		));

		if(dependency.NumBarriers) {
			commandBuffer->buffer->lpVtbl->Barrier(commandBuffer->buffer, 1, &dependency);
			ListD3D12_BUFFER_BARRIER_clear(&deviceExt->bufferTransitions, e_rr);
		}

		//The readback lives on a readback heap, which is created in COPY_DEST and cannot leave it, so it is
		// the one buffer here that is never transitioned.

		commandBuffer->buffer->lpVtbl->CopyBufferRegion(
			commandBuffer->buffer,
			DeviceBuffer_ext(DeviceBufferRef_ptr(readbackRef), Dx)->buffer, offset,
			emitExt->buffer, offset, sizeof(U64)
		);

		blas->base.compactionQuery = query;
		blas->base.compactionSubmitId = device->submitId;
	}

	//Add as flight and ensure flushes are done if too many ASes are queued this frame

	device->pendingPrimitives += blasExt->primitives;

	if(!ListRefPtr_contains(*currentFlight, pending, 0, NULL)) {
		gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, pending, alloc, e_rr));
		RefPtr_inc(pending);
	}

	//We mark scratch buffer as delete, we do this by pushing it as a current flight resource,
	// and losing the reference from our object.
	//However that's only if allow update is false.

	if(!ListRefPtr_contains(*currentFlight, blas->base.tempScratchBuffer, 0, NULL)) {

		gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, blas->base.tempScratchBuffer, alloc, e_rr));

		if(!(blas->base.flags & ERTASBuildFlags_AllowUpdate))
			blas->base.tempScratchBuffer = NULL;

		else RefPtr_inc(blas->base.tempScratchBuffer);
	}

	//Ensure we don't exceed a maximum amount of time spent on the GPU

	if (device->pendingPrimitives >= device->flushThresholdPrimitives)
		gotoIfError3(clean, DxGraphicsDevice_flush(deviceRef, commandBuffer, e_rr));

	blas->base.isCompleted = true;

clean:
	return s_uccess;
}
