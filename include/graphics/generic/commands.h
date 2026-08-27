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

//graphics/generic/commands.h

#pragma once
#include "graphics/generic/command_structs.h"
#include "graphics/generic/descriptor_table.h"
#include "types/math/vec4.h"

#ifdef __cplusplus
	extern "C" {
#endif

Bool CommandListRef_clear(CommandListRef *commandList, Error *e_rr);
Bool CommandListRef_begin(CommandListRef *commandList, Bool doClear, U64 lockTimeout, Error *e_rr);
Bool CommandListRef_end(CommandListRef *commandList, Error *e_rr);

Bool CommandListRef_setViewport(CommandListRef *commandList, I32x2 offset, I32x2 size, Error *e_rr);
Bool CommandListRef_setScissor(CommandListRef *commandList, I32x2 offset, I32x2 size, Error *e_rr);
Bool CommandListRef_setViewportAndScissor(CommandListRef *commandList, I32x2 offset, I32x2 size, Error *e_rr);

Bool CommandListRef_setStencil(CommandListRef *commandList, U8 stencilValue, Error *e_rr);
Bool CommandListRef_setBlendConstants(CommandListRef *commandList, F32x4 blendConstants, Error *e_rr);

//Setting clear parameters and clearing render texture

//Clear entire resource or subresource.
//SwapchainRef or RenderTargetRef

Bool CommandListRef_clearImagef(CommandListRef *commandList, F32x4 color, ImageRange range, RefPtr *image, Error *e_rr);
Bool CommandListRef_clearImagei(CommandListRef *commandList, I32x4 color, ImageRange range, RefPtr *image, Error *e_rr);
Bool CommandListRef_clearImageu(CommandListRef *commandList, const U32 color[4], ImageRange range, RefPtr *image, Error *e_rr);

TList(ClearImageCmd);

Bool CommandListRef_clearImages(CommandListRef *commandList, ListClearImageCmd clearImages, Error *e_rr);

//Copy image
//Only allowed on SwapchainRef, RenderTargetRef, DeviceTextureRef and DepthStencilRef.
//Though DepthStencilRef requires both src and dst to be depth stencil.
//The texture formats need to be the same to allow copying.
//isStencil is only allowed when the input and output both have stencil buffers.
//When depthStencil is used, the stride of the stencil and/or depth needs to match between src and dst.

TList(CopyImageRegion);

Bool CommandListRef_copyImageRegions(
	CommandListRef *commandList, RefPtr *src, RefPtr *dst, ListCopyImageRegion regions, Error *e_rr
);

Bool CommandListRef_copyImage(
	CommandListRef *commandList, RefPtr *src, RefPtr *dst, CopyImageRegion region, Error *e_rr
);

//Draw calls and dispatches

TList(Transition);
TList(CommandScopeDependency);

Bool CommandListRef_startScope(
	CommandListRef *commandList,
	const ListTransition *transitions,
	U32 id,
	const ListCommandScopeDependency *dependencies,
	ECommandScopeFlags flags,
	const CharString *name,
	Error *e_rr
);

Bool CommandListRef_endScope(CommandListRef *commandList, Error *e_rr);

Bool CommandListRef_setPipeline(CommandListRef *commandList, PipelineRef *pipeline, EPipelineType type, Error *e_rr);
Bool CommandListRef_setComputePipeline(CommandListRef *commandList, PipelineRef *pipeline, Error *e_rr);
Bool CommandListRef_setGraphicsPipeline(CommandListRef *commandList, PipelineRef *pipeline, Error *e_rr);
Bool CommandListRef_setRaytracingPipeline(CommandListRef *commandList, PipelineRef *pipeline, Error *e_rr);

Bool CommandListRef_setPrimitiveBuffers(CommandListRef *commandList, const SetPrimitiveBuffersCmd *buffers, Error *e_rr);

//Binds a descriptor table (bindful): pipelines with a custom pipeline layout read their descriptors from it.
//This only SETS state: the work ops (draw/dispatch/dispatchRays) are what validate that the bound table's
// layout matches the bound pipeline's layout, so bind order never matters.
//Pipelines using the device's default (bindless) layout ignore it entirely.
//The table is kept alive by the command list; the resources its descriptors point at follow the usual
// rules (maintainRef or caller kept), and scope transitions for them stay the caller's job for now.

Bool CommandListRef_bindDescriptorTable(CommandListRef *commandList, DescriptorTableRef *table, Error *e_rr);

//Binds a descriptor heap (bindful): any descriptor table bound after this has to belong to it.
//Explicit on purpose: switching heaps can stall the GPU (notably D3D12), so the cost has to be visible in
// the recording rather than implied by whichever table was bound. The work ops validate that the bound
// table's parent is exactly this heap.

Bool CommandListRef_bindDescriptorHeap(CommandListRef *commandList, DescriptorHeapRef *heap, Error *e_rr);

//Writes the push constants the next work op will run with (bindful).
//The data has to be exactly the size the bound pipeline's layout declared: the range is one object to the
// backends, so writing part of it would leave the rest holding whatever the previous pipeline left there.
//Like the binds above this only sets state; the work ops validate it against the pipeline actually bound.

Bool CommandListRef_setPushConstants(CommandListRef *commandList, Buffer data, Error *e_rr);

//Writes every push descriptor the bound pipeline's layout declares, in the layout's own binding order
//(the order detectLayoutFromEntry/Entries produced into pushDescriptorInfo, whose bindingNames name them).
//All of them at once rather than one at a time: a partial set would leave the rest pointing at whatever the
//last pipeline bound, and both backends emit the whole set anyway.

Bool CommandListRef_setPushDescriptors(CommandListRef *commandList, const ListDescriptor *descriptors, Error *e_rr);

Bool CommandListRef_draw(CommandListRef *commandList, const DrawCmd *draw, Error *e_rr);

Bool CommandListRef_drawIndexed(CommandListRef *commandList, U32 indexCount, U32 instanceCount, Error *e_rr);
Bool CommandListRef_drawUnindexed(CommandListRef *commandList, U32 vertexCount, U32 instanceCount, Error *e_rr);

Bool CommandListRef_drawIndexedAdv(
	CommandListRef *commandList,
	U32 indexCount,
	U32 instanceCount,
	U32 indexOffset,
	U32 instanceOffset,
	U32 vertexOffset,
	Error *e_rr
);

Bool CommandListRef_drawUnindexedAdv(
	CommandListRef *commandList,
	U32 vertexCount,
	U32 instanceCount,
	U32 vertexOffset,
	U32 instanceOffset,
	Error *e_rr
);

Bool CommandListRef_drawIndirect(
	CommandListRef *commandList,
	DeviceBufferRef *buffer,
	U64 bufferOffset,
	U32 drawCalls,
	Bool indexed,
	Error *e_rr
);

Bool CommandListRef_drawIndirectCountExt(
	CommandListRef *commandList,
	DeviceBufferRef *buffer,
	U64 bufferOffset,
	DeviceBufferRef *countBuffer,
	U64 countOffset,
	U32 maxDrawCalls,
	Bool indexed,
	Error *e_rr
);

Bool CommandListRef_dispatch(CommandListRef *commandList, DispatchCmd dispatch, Error *e_rr);
Bool CommandListRef_dispatch1D(CommandListRef *commandList, U32 groupsX, Error *e_rr);
Bool CommandListRef_dispatch2D(CommandListRef *commandList, U32 groupsX, U32 groupsY, Error *e_rr);
Bool CommandListRef_dispatch3D(CommandListRef *commandList, U32 groupsX, U32 groupsY, U32 groupsZ, Error *e_rr);

Bool CommandListRef_dispatchIndirect(CommandListRef *commandList, DeviceBufferRef *buffer, U64 offset, Error *e_rr);

//RayPipeline feature

Bool CommandListRef_dispatchRaysExt(CommandListRef *commandList, DispatchRaysExt dispatchRays, Error *e_rr);

//Ray dispatch whose width/height/depth come from a buffer the GPU wrote,
// so a launch sized by a GPU driven pass never has to read its count back to the CPU.
//The buffer holds three U32s (width, height, depth) at offset, needs Indirect usage, and is aligned to 4.
//Vulkan reads the buffer directly; D3D12 writes the pipeline's SBT into a per frame intermediate, copies the
// three counts in beside it and issues an ExecuteIndirect over it, so the caller sees one portable call.

Bool CommandListRef_dispatchRaysIndirectExt(
	CommandListRef *commandList, DeviceBufferRef *buffer, U64 offset, U32 raygenLocalId, Error *e_rr
);
Bool CommandListRef_dispatch1DRaysExt(CommandListRef *commandList, U32 raygenLocalId, U32 raysX, Error *e_rr);
Bool CommandListRef_dispatch2DRaysExt(CommandListRef *commandList, U32 raygenLocalId, U32 raysX, U32 raysY, Error *e_rr);
Bool CommandListRef_dispatch3DRaysExt(
	CommandListRef *commandList,
	U32 raygenLocalId,
	U32 raysX,
	U32 raysY,
	U32 raysZ,
	Error *e_rr
);

//Raytracing feature

typedef RefPtr TLASRef;
typedef RefPtr OpacityMicromapRef;
typedef RefPtr BLASRef;

Bool CommandListRef_updateTLASExt(CommandListRef *commandList, TLASRef *tlas, Error *e_rr);
Bool CommandListRef_updateBLASExt(CommandListRef *commandList, BLASRef *blas, Error *e_rr);

//Builds an opacity micromap, which has to be recorded before any BLAS build that links it.
//A micromap has no update mode, so recording it again after it completed is a no-op.

Bool CommandListRef_updateOmmExt(CommandListRef *commandList, OpacityMicromapRef *micromap, Error *e_rr);

//DynamicRendering feature

TList(AttachmentInfo);

Bool CommandListRef_startRenderExt(
	CommandListRef *commandList,
	I32x2 offset,
	I32x2 size,
	const ListAttachmentInfo *colors,
	const DepthStencilAttachmentInfo *depthStencil,
	Error *e_rr
);

Bool CommandListRef_endRenderExt(CommandListRef *commandList, Error *e_rr);

//DebugMarkers feature

Bool CommandListRef_addMarkerDebugExt(CommandListRef *commandList, F32x4 color, const CharString *name, Error *e_rr);
Bool CommandListRef_startRegionDebugExt(CommandListRef *commandList, F32x4 color, const CharString *name, Error *e_rr);
Bool CommandListRef_endRegionDebugExt(CommandListRef *commandList, Error *e_rr);

//Timestamps feature

//Turns per scope GPU timing on or off for this command list; call before recording begins.
//When on, every scope that does not pass ECommandScopeFlags_DisableTimestamp
// gets a begin and end timestamp keyed by its scopeId.
//The device reads the results back once the frame completes, see GraphicsDeviceRef_getTimings.

Bool CommandListRef_setScopeTimingExt(CommandListRef *commandList, Bool enable, Error *e_rr);

//Turns per scope auto debug regions on or off for this command list; call before recording.
//When on, every scope given a name emits a begin and end debug region labelled by it,
// unless it passes ECommandScopeFlags_DisableDebug.
//The name is the one passed to startScope; a scope with no name emits nothing.

Bool CommandListRef_setScopeDebugExt(CommandListRef *commandList, Bool enable, Error *e_rr);

//A manual named region, nestable like a debug region: a begin and end timestamp whose delta is one timing result.
//id is a hot path key read back without comparing strings, 0 when unused; name is a convenience, NULL when unused.

Bool CommandListRef_startTimingRegionExt(CommandListRef *commandList, U32 id, const CharString *name, Error *e_rr);
Bool CommandListRef_endTimingRegionExt(CommandListRef *commandList, Error *e_rr);

//A single point in time timestamp; two of them read back are diffed by the caller. Same id and name rules.

Bool CommandListRef_insertTimingExt(CommandListRef *commandList, U32 id, const CharString *name, Error *e_rr);

#ifdef __cplusplus
	}
#endif
