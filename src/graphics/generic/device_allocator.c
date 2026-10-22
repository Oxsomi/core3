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

//graphics/generic/device_allocator.c

#include "types/container/list_impl.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/device_allocator.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_internal.h"
#include "platforms/logx.h"
#include "types/base/error.h"
#include "types/base/constants.h"
#include "types/base/mathi.h"
#include "types/container/string.h"
#include "graphics/generic/resource.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/pipeline.h"
#include "formats/json/json_writer.h"

TListImpl(DeviceMemoryBlock);

Bool DeviceMemoryAllocator_freeAllocation(DeviceMemoryAllocator *allocator, U32 blockId, U64 blockOffset) {

	const Allocator *alloc = allocator ? GraphicsDevice_getAlloc(allocator->device) : NULL;

	Bool s_uccess = true;

	if(!allocator)
		return false;

	ELockAcquire acq = SpinLock_lock(&allocator->lock, U64_MAX);

	if (acq < ELockAcquire_Success)
		return false;        //Can't free.

	Error *e_rr = NULL;

	if(blockId >= allocator->blocks.length)
		retError(clean, Error_outOfBounds(
			1, blockId, allocator->blocks.length, "DeviceMemoryAllocator_freeAllocation() blockId out of bounds"
		));

	DeviceMemoryBlock *block = &allocator->blocks.ptrNonConst[blockId];

	AllocationBuffer_freeBlock(&block->allocations, (const U8*) blockOffset);

	if (!block->allocations.allocations.length) {

		if(allocator->device->flags & EGraphicsDeviceFlags_IsDebug)
			Log_debugLnx("-- Graphics: Freeing block %"PRIu32, blockId);

		AllocationBuffer_free(&block->allocations, alloc);
		DeviceMemoryAllocator_freeAllocationExt(allocator->device, block->ext);
		block->ext = NULL;
		block->mappedMemoryExt = NULL;
		block->isActive = false;
	}

	if(blockId + 1 == allocator->blocks.length)
		while(allocator->blocks.length && !ListDeviceMemoryBlock_last(allocator->blocks)->isActive)
			ListDeviceMemoryBlock_popBack(&allocator->blocks, NULL, e_rr);

clean:

	if(acq == ELockAcquire_Acquired)    //Only release if it wasn't already active
		SpinLock_unlock(&allocator->lock);

	return s_uccess;
}

//Memory stats

void DescriptorHeap_trackMemory(DescriptorHeap *heap, U64 gpuBytes) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(heap->device);

	heap->gpuBytes = gpuBytes;
	heap->statsTracked = true;

	AtomicI64_add(&device->descriptorHeapBytes, (I64) gpuBytes);
	AtomicI64_inc(&device->descriptorHeapCount);
}

void Pipeline_trackMemory(Pipeline *pipeline, U64 gpuBytes, U64 irBytes, Bool isEstimate) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(pipeline->device);

	pipeline->gpuBytes = (U32) U64_min(gpuBytes, U32_MAX);
	pipeline->irBytes = (U32) U64_min(irBytes, U32_MAX);
	pipeline->gpuBytesEstimated = isEstimate;

	if(!isEstimate)
		AtomicI64_add(&device->pipelineKnownIRBytes, (I64) pipeline->irBytes);

	pipeline->statsTracked = true;

	AtomicI64_add(isEstimate ? &device->pipelineEstimateBytes : &device->pipelineBytes, (I64) pipeline->gpuBytes);
	AtomicI64_inc(isEstimate ? &device->pipelineEstimateCount : &device->pipelineCount);
}

typedef struct DeviceMemoryBlockStats {
	U64 size, used, largestFree;
	U64 entryStart;                     //Its allocated entries in the entry list
	U32 entryCount;
	U32 allocations, freeRanges;
	U32 typeExt;
	U16 allocationTypeExt;
	Bool isDedicated, isActive;
	U8 padding[4];
} DeviceMemoryBlockStats;

typedef struct DeviceMemoryEntry {
	U64 start, aligned, end;            //Aligned is where the resource starts, start includes the padding before it
} DeviceMemoryEntry;

TList(DeviceMemoryBlockStats);
TListImpl(DeviceMemoryBlockStats);
TList(DeviceMemoryEntry);
TListImpl(DeviceMemoryEntry);

//What a resource is used as, for the summary: the first that applies, since a buffer can carry several usages

typedef enum EDeviceMemoryCategory {
	EDeviceMemoryCategory_AccelerationStructure,
	EDeviceMemoryCategory_ASScratch,
	EDeviceMemoryCategory_ShaderBindingTable,
	EDeviceMemoryCategory_Mesh,                 //Vertex, index or acceleration structure build input
	EDeviceMemoryCategory_Indirect,
	EDeviceMemoryCategory_Uniform,
	EDeviceMemoryCategory_CPU,                  //Staging, readback and other CPU side buffers
	EDeviceMemoryCategory_ShaderWrite,          //UAV
	EDeviceMemoryCategory_ShaderRead,
	EDeviceMemoryCategory_OtherBuffer,
	EDeviceMemoryCategory_Texture2D,
	EDeviceMemoryCategory_Texture3D,
	EDeviceMemoryCategory_TextureCube,
	EDeviceMemoryCategory_RenderTarget,
	EDeviceMemoryCategory_DepthStencil,
	EDeviceMemoryCategory_Swapchain,
	EDeviceMemoryCategory_DescriptorHeap,       //Not in a block: the driver allocates them
	EDeviceMemoryCategory_Pipeline,             //Code size the driver reported
	EDeviceMemoryCategory_PipelineEstimate,     //IR size standing in where the driver didn't say
	EDeviceMemoryCategory_Count
} EDeviceMemoryCategory;

static const C8 *EDeviceMemoryCategory_names[EDeviceMemoryCategory_Count] = {
	"accelerationStructure", "asScratch", "shaderBindingTable", "mesh", "indirect", "uniform", "cpu", "shaderWrite",
	"shaderRead", "otherBuffer", "texture2D", "texture3D", "textureCube", "renderTarget", "depthStencil", "swapchain",
	"descriptorHeap", "pipeline", "pipelineEstimate"
};

static EDeviceMemoryCategory DeviceMemoryStats_category(const LiveResource *live, const GraphicsResource *res) {

	if(res->type != EResourceType_DeviceBuffer) {

		const UnifiedTexture *tex = TextureRef_getUnifiedTextureFast(live->resource);

		if(res->type == EResourceType_Swapchain)
			return EDeviceMemoryCategory_Swapchain;

		if(res->type == EResourceType_RenderTargetOrDepthStencil)
			return tex && tex->depthFormat ? EDeviceMemoryCategory_DepthStencil : EDeviceMemoryCategory_RenderTarget;

		return
			!tex || tex->type == ETextureType_2D ? EDeviceMemoryCategory_Texture2D :
			(tex->type == ETextureType_3D ? EDeviceMemoryCategory_Texture3D : EDeviceMemoryCategory_TextureCube);
	}

	const EDeviceBufferUsage usage = DeviceBufferRef_ptr(live->resource)->usage;

	if(usage & EDeviceBufferUsage_ASExt)                return EDeviceMemoryCategory_AccelerationStructure;
	if(usage & EDeviceBufferUsage_ScratchExt)           return EDeviceMemoryCategory_ASScratch;
	if(usage & EDeviceBufferUsage_SBTExt)               return EDeviceMemoryCategory_ShaderBindingTable;

	if(usage & (EDeviceBufferUsage_Vertex | EDeviceBufferUsage_Index | EDeviceBufferUsage_ASReadExt))
		return EDeviceMemoryCategory_Mesh;

	if(usage & EDeviceBufferUsage_Indirect)             return EDeviceMemoryCategory_Indirect;
	if(usage & EDeviceBufferUsage_Uniform)              return EDeviceMemoryCategory_Uniform;

	if(res->flags & (EGraphicsResourceFlag_CPUAllocatedBit | EGraphicsResourceFlag_CPUReadBit))
		return EDeviceMemoryCategory_CPU;

	if(res->flags & EGraphicsResourceFlag_ShaderWrite)  return EDeviceMemoryCategory_ShaderWrite;
	if(res->flags & EGraphicsResourceFlag_ShaderRead)   return EDeviceMemoryCategory_ShaderRead;

	return EDeviceMemoryCategory_OtherBuffer;
}

//The bytes a resource takes in its block: its allocator entry, from where it starts, so alignment padding before it
// stays unattributed. A swapchain whose images DXGI or the presentation engine own is estimated from its images.

static U64 DeviceMemoryStats_footprint(
	const ListDeviceMemoryBlockStats *stats, const ListDeviceMemoryEntry *entries, const LiveResource *live,
	const GraphicsResource *res
) {

	if(res->type == EResourceType_Swapchain && !res->allocated) {

		const UnifiedTexture *tex = TextureRef_getUnifiedTextureFast(live->resource);

		return !tex || tex->textureFormatId >= ETextureFormatId_Count ? 0 :
			ETextureFormat_getSize(ETextureFormatId_unpack[tex->textureFormatId], tex->width, tex->height, 1) *
			tex->images;
	}

	if(!res->allocated || res->blockId >= stats->length)
		return 0;

	const DeviceMemoryBlockStats s = stats->ptr[res->blockId];

	for(U64 i = s.entryStart; i < s.entryStart + s.entryCount; ++i) {

		const DeviceMemoryEntry e = entries->ptr[i];

		if(e.start == res->blockOffset || e.aligned == res->blockOffset)
			return e.end - res->blockOffset;
	}

	return 0;
}

Bool GraphicsDeviceRef_getMemoryStats(GraphicsDeviceRef *deviceRef, const Allocator *alloc, CharString *json, Error *e_rr) {

	Bool s_uccess = true;
	SpinLock *held = NULL;
	Bool writing = false;                       //Only what this call wrote is freed on failure
	ListDeviceMemoryBlockStats stats = (ListDeviceMemoryBlockStats) { 0 };
	ListDeviceMemoryEntry entries = (ListDeviceMemoryEntry) { 0 };
	JsonWriter w = JsonWriter_create(json, true, alloc);

	if(!deviceRef || deviceRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice || !json)
		retError(clean, Error_nullPointer(
			!json ? 2 : 0, "GraphicsDeviceRef_getMemoryStats()::deviceRef and json are required"
		));

	if(CharString_length(*json))
		retError(clean, Error_invalidParameter(2, 0, "GraphicsDeviceRef_getMemoryStats()::json isn't empty"));

	writing = true;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	//Blocks under the allocator lock, then resources under the device lock, never both at once

	ELockAcquire acq = SpinLock_lock(&device->allocator.lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_getMemoryStats() couldn't lock the allocator"));

	if(acq == ELockAcquire_Acquired)
		held = &device->allocator.lock;

	gotoIfError3(clean, ListDeviceMemoryBlockStats_resize(&stats, device->allocator.blocks.length, alloc, e_rr));

	for(U64 i = 0; i < stats.length; ++i) {

		const DeviceMemoryBlock *block = &device->allocator.blocks.ptr[i];
		const ListAllocationBufferBlock *blockEntries = &block->allocations.allocations;
		const U64 size = Buffer_length(block->allocations.buffer);

		DeviceMemoryBlockStats s = (DeviceMemoryBlockStats) {
			.size = size,
			.entryStart = entries.length,
			.typeExt = block->typeExt,
			.allocationTypeExt = block->allocationTypeExt,
			.isDedicated = block->isDedicated,
			.isActive = block->isActive
		};

		U64 cursor = 0;

		for(U64 j = 0; j < blockEntries->length; ++j) {

			const AllocationBufferBlock e = blockEntries->ptr[j];
			const U64 start = e.startAndNonLinearAndFree & (((U64)1 << 62) - 1);
			const Bool isFree = (e.startAndNonLinearAndFree >> 63) & 1;

			if(start > cursor) {        //The ring buffer's front, before its first entry
				++s.freeRanges;
				s.largestFree = U64_max(s.largestFree, start - cursor);
			}

			if(isFree) {
				++s.freeRanges;
				s.largestFree = U64_max(s.largestFree, e.end - start);
			}

			else {

				++s.allocations;
				s.used += e.end - start;

				const U64 aligned = e.alignment ? (start + e.alignment - 1) / e.alignment * e.alignment : start;
				const DeviceMemoryEntry entry = (DeviceMemoryEntry) { .start = start, .aligned = aligned, .end = e.end };

				gotoIfError3(clean, ListDeviceMemoryEntry_pushBack(&entries, entry, alloc, e_rr));
				++s.entryCount;
			}

			cursor = e.end;
		}

		if(size > cursor) {
			++s.freeRanges;
			s.largestFree = U64_max(s.largestFree, size - cursor);
		}

		stats.ptrNonConst[i] = s;
	}

	if(held)
		SpinLock_unlock(held);

	held = NULL;

	acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_getMemoryStats() couldn't lock the device"));

	if(acq == ELockAcquire_Acquired)
		held = &device->lock;

	//Summary first: what the memory is used as

	U64 categoryCount[EDeviceMemoryCategory_Count] = { 0 }, categoryBytes[EDeviceMemoryCategory_Count] = { 0 };
	U64 blockBytes = 0, usedBytes = 0, attributedBytes = 0;

	for(U64 i = 0; i < device->liveResources.length; ++i) {

		const LiveResource *live = &device->liveResources.ptr[i];
		const GraphicsResource *res = LiveResource_res(live);
		const U64 footprint = DeviceMemoryStats_footprint(&stats, &entries, live, res);
		const EDeviceMemoryCategory category = DeviceMemoryStats_category(live, res);

		++categoryCount[category];
		categoryBytes[category] += footprint;
		attributedBytes += footprint;
	}

	categoryCount[EDeviceMemoryCategory_DescriptorHeap] = (U64) AtomicI64_load(&device->descriptorHeapCount);
	categoryBytes[EDeviceMemoryCategory_DescriptorHeap] = (U64) AtomicI64_load(&device->descriptorHeapBytes);
	categoryCount[EDeviceMemoryCategory_Pipeline] = (U64) AtomicI64_load(&device->pipelineCount);
	categoryBytes[EDeviceMemoryCategory_Pipeline] = (U64) AtomicI64_load(&device->pipelineBytes);
	categoryCount[EDeviceMemoryCategory_PipelineEstimate] = (U64) AtomicI64_load(&device->pipelineEstimateCount);
	categoryBytes[EDeviceMemoryCategory_PipelineEstimate] = (U64) AtomicI64_load(&device->pipelineEstimateBytes);

	for(U64 i = 0; i < stats.length; ++i)
		if(stats.ptr[i].isActive) {
			blockBytes += stats.ptr[i].size;
			usedBytes += stats.ptr[i].used;
		}

	gotoIfError3(clean, JsonWriter_beginObject(&w, e_rr));

	gotoIfError3(clean, JsonWriter_keyObject(&w, "usage", e_rr));
	gotoIfError3(clean, JsonWriter_keyU64(&w, "deviceLocal", GraphicsDeviceRef_getMemoryBudget(deviceRef, true), e_rr));
	gotoIfError3(clean, JsonWriter_keyU64(&w, "shared", GraphicsDeviceRef_getMemoryBudget(deviceRef, false), e_rr));
	gotoIfError3(clean, JsonWriter_keyU64(&w, "blocks", blockBytes, e_rr));
	gotoIfError3(clean, JsonWriter_keyU64(&w, "used", usedBytes, e_rr));
	gotoIfError3(clean, JsonWriter_keyU64(&w, "resources", attributedBytes, e_rr));

	gotoIfError3(clean, JsonWriter_keyU64(
		&w, "outsideBlocks",
		categoryBytes[EDeviceMemoryCategory_DescriptorHeap] + categoryBytes[EDeviceMemoryCategory_Pipeline] +
			categoryBytes[EDeviceMemoryCategory_PipelineEstimate],
		e_rr
	));

	gotoIfError3(clean, JsonWriter_endObject(&w, e_rr));

	gotoIfError3(clean, JsonWriter_keyArray(&w, "summary", e_rr));

	for(U64 i = 0; i < EDeviceMemoryCategory_Count; ++i)
		if(categoryCount[i]) {
			gotoIfError3(clean, JsonWriter_beginObject(&w, e_rr));
			gotoIfError3(clean, JsonWriter_keyCstr(&w, "category", EDeviceMemoryCategory_names[i], e_rr));
			gotoIfError3(clean, JsonWriter_keyU64(&w, "count", categoryCount[i], e_rr));
			gotoIfError3(clean, JsonWriter_keyU64(&w, "bytes", categoryBytes[i], e_rr));

			//What the estimate would have said for the same pipelines

			if(i == EDeviceMemoryCategory_Pipeline)
				gotoIfError3(clean, JsonWriter_keyU64(
					&w, "irBytes", (U64) AtomicI64_load(&device->pipelineKnownIRBytes), e_rr
				));

			gotoIfError3(clean, JsonWriter_endObject(&w, e_rr));
		}

	gotoIfError3(clean, JsonWriter_endArray(&w, e_rr));
	gotoIfError3(clean, JsonWriter_keyArray(&w, "blocks", e_rr));

	for(U64 i = 0; i < stats.length; ++i) {

		const DeviceMemoryBlockStats s = stats.ptr[i];

		if(!s.isActive)
			continue;

		gotoIfError3(clean, JsonWriter_beginObject(&w, e_rr));
		gotoIfError3(clean, JsonWriter_keyU64(&w, "id", i, e_rr));
		gotoIfError3(clean, JsonWriter_keyU64(&w, "size", s.size, e_rr));
		gotoIfError3(clean, JsonWriter_keyU64(&w, "used", s.used, e_rr));
		gotoIfError3(clean, JsonWriter_keyU64(&w, "allocations", s.allocations, e_rr));
		gotoIfError3(clean, JsonWriter_keyU64(&w, "freeRanges", s.freeRanges, e_rr));
		gotoIfError3(clean, JsonWriter_keyU64(&w, "largestFree", s.largestFree, e_rr));
		gotoIfError3(clean, JsonWriter_keyBool(&w, "dedicated", s.isDedicated, e_rr));
		gotoIfError3(clean, JsonWriter_keyU64(&w, "typeExt", s.typeExt, e_rr));
		gotoIfError3(clean, JsonWriter_keyU64(&w, "allocationTypeExt", s.allocationTypeExt, e_rr));
		gotoIfError3(clean, JsonWriter_endObject(&w, e_rr));
	}

	gotoIfError3(clean, JsonWriter_endArray(&w, e_rr));
	gotoIfError3(clean, JsonWriter_keyArray(&w, "resources", e_rr));

	for(U64 i = 0; i < device->liveResources.length; ++i) {

		const LiveResource *live = &device->liveResources.ptr[i];
		const GraphicsResource *res = LiveResource_res(live);

		gotoIfError3(clean, JsonWriter_beginObject(&w, e_rr));
		gotoIfError3(clean, JsonWriter_keyStr(&w, "name", live->name, e_rr));

		gotoIfError3(clean, JsonWriter_keyCstr(
			&w, "category", EDeviceMemoryCategory_names[DeviceMemoryStats_category(live, res)], e_rr
		));

		gotoIfError3(clean, JsonWriter_keyU64(&w, "bytes", DeviceMemoryStats_footprint(&stats, &entries, live, res), e_rr));
		gotoIfError3(clean, JsonWriter_keyU64(&w, "size", res->size, e_rr));
		gotoIfError3(clean, JsonWriter_keyI64(&w, "block", res->allocated ? (I64) res->blockId : -1, e_rr));
		gotoIfError3(clean, JsonWriter_keyU64(&w, "offset", res->blockOffset, e_rr));
		gotoIfError3(clean, JsonWriter_endObject(&w, e_rr));
	}

	if(held)
		SpinLock_unlock(held);

	held = NULL;

	gotoIfError3(clean, JsonWriter_endArray(&w, e_rr));
	gotoIfError3(clean, JsonWriter_endObject(&w, e_rr));

clean:

	if(held)
		SpinLock_unlock(held);

	ListDeviceMemoryBlockStats_free(&stats, alloc);
	ListDeviceMemoryEntry_free(&entries, alloc);

	if(!s_uccess && writing)
		CharString_free(json, alloc);

	return s_uccess;
}
