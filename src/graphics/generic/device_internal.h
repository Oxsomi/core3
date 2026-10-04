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

//graphics/generic/device_internal.h

#pragma once
#include "graphics/generic/device.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Internal declarations shared between the generic device and the backends; not part of the public API.

//Device loss diagnostics (docs/graphics_api.md, "Device loss").
//A backend registers each buffer and texture it creates; GraphicsResource_free unregisters it. apiObject is the D3D12
// resource's IUnknown or the Vulkan handle.

Bool GraphicsDevice_registerResource(
	GraphicsDevice *device,
	RefPtr *resource,                           //DeviceBufferRef or TextureRef
	U64 apiObject,
	const CharString *name,
	Error *e_rr
);

void GraphicsDevice_unregisterResource(GraphicsDevice *device, const RefPtr *resource);

//Appends what an address or an API object resolves to: the resource, its kind and the offset, a freed one and how
// long ago, or the memory block when only that is known. Returns whether anything matched.

Bool GraphicsDevice_describeResource(
	GraphicsDevice *device, U64 address, U64 apiObject, CharString *str, const Allocator *alloc, Error *e_rr
);

//When nothing holds address: the registered ranges (live or recently freed) nearest below and above it and how far
// away, since a read past a buffer's end lands right after it. Returns whether either side had one.

Bool GraphicsDevice_describeNearest(
	GraphicsDevice *device, U64 address, CharString *str, const Allocator *alloc, Error *e_rr
);

void GraphicsDevice_freeResourceRegistry(GraphicsDevice *device, const Allocator *alloc);      //Last, at device free

//Counts a descriptor heap or pipeline's GPU memory in the device's memory stats once, after it was created; its
// free takes it out again. A pipeline's is an estimate when only its IR size was available.

typedef struct DescriptorHeap DescriptorHeap;
typedef struct Pipeline Pipeline;

void DescriptorHeap_trackMemory(DescriptorHeap *heap, U64 gpuBytes);
void Pipeline_trackMemory(Pipeline *pipeline, U64 gpuBytes, U64 irBytes, Bool isEstimate);

//The size of a new shared memory block: the first blocks of a kind are an eighth, a quarter and a half of the full
// size, as VMA's are, since most kinds (readback, acceleration structures, a host memory type) only ever hold a few
// small resources. Grown to fit the request, which is at most half the full size.

static inline U64 DeviceMemoryAllocator_newBlockSize(U64 fullSize, U32 blocksOfKind, U64 request) {

	U64 size = fullSize >> (blocksOfKind < 3 ? 3 - blocksOfKind : 0);

	while(size < request && size < fullSize)
		size <<= 1;

	return size;
}

//The driver's code size of the pipeline with this key, if one was recorded (this run or a loaded pipeline cache).
//The key is a hash of everything a backend hands the driver to build the pipeline.

Bool GraphicsDevice_findPipelineSize(GraphicsDevice *device, U64 key, U64 *bytes);

//Builds such a key, starting from Buffer_fnv1a64Offset.

U64 Pipeline_hash(U64 hash, const void *data, U64 length);
U64 Pipeline_hashString(U64 hash, const C8 *str);

//The pipeline's layout as part of its key: flags, push constants and both descriptor layouts' bindings

typedef RefPtr PipelineLayoutRef;
U64 Pipeline_hashLayout(U64 hash, PipelineLayoutRef *layout);
Bool GraphicsDevice_recordPipelineSize(GraphicsDevice *device, U64 key, U64 bytes, Error *e_rr);

//An upload too large for the staging buffer, which goes through a temporary one instead. Counted towards the staging
// peak, so the staging buffer isn't shrunk below what such uploads need.

void GraphicsDevice_noteStagingBypass(GraphicsDevice *device, U64 bytes);

//The GraphicsResource behind a registered buffer or texture; only valid while the device lock is held.
typedef struct GraphicsResource GraphicsResource;
const GraphicsResource *LiveResource_res(const LiveResource *live);

//Logs, once per device, what the backend can tell about a loss: the reason, the fault and the resources it hit.
//Submit and wait call it when they fail; it does nothing for a device that is not lost.

void GraphicsDeviceRef_reportLoss(GraphicsDeviceRef *deviceRef);

//The backends' side of breadcrumbs: the next slot of the frame being recorded, for a scope, or U32_MAX when there are
// no breadcrumbs or the frame used them all. The first claim of a frame returns 0.

U32 GraphicsDevice_claimBreadcrumb(GraphicsDevice *device, U32 scopeId);

//Starts the frame being recorded over, once its previous submit provably completed: its slots back to 0, and kept
// first when not every scope of that submit had ended (see breadcrumbUnfinishedScopes), unless that submit failed
// without a loss (breadcrumbFailedFrames). Before anything is recorded.

void GraphicsDevice_startBreadcrumbs(GraphicsDevice *device);

//GraphicsDeviceRef_wait for a backend's flush, which splits a submit that is still being recorded.
//The submit being recorded is not marked complete (completedSubmitId stops one short of submitId) and keeps the
// resources it holds in flight, except internal staging buffers nothing else references.

Bool GraphicsDeviceRef_waitFlush(GraphicsDeviceRef *deviceRef, Error *e_rr);

#ifdef __cplusplus
	}
#endif
