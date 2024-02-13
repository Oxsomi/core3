/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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

#pragma once
#include "command_structs.h"

Error CommandListRef_clear(CommandListRef *commandList);
Error CommandListRef_begin(CommandListRef *commandList, Bool doClear, U64 lockTimeout);
Error CommandListRef_end(CommandListRef *commandList);

Error CommandListRef_setViewport(CommandListRef *commandList, I32x2 offset, I32x2 size);
Error CommandListRef_setScissor(CommandListRef *commandList, I32x2 offset, I32x2 size);
Error CommandListRef_setViewportAndScissor(CommandListRef *commandList, I32x2 offset, I32x2 size);

Error CommandListRef_setStencil(CommandListRef *commandList, U8 stencilValue);
Error CommandListRef_setBlendConstants(CommandListRef *commandList, F32x4 blendConstants);

//Setting clear parameters and clearing render texture

//Clear entire resource or subresource.
//SwapchainRef or RenderTargetRef

Error CommandListRef_clearImagef(CommandListRef *commandList, F32x4 color, ImageRange range, RefPtr *image);
Error CommandListRef_clearImagei(CommandListRef *commandList, I32x4 color, ImageRange range, RefPtr *image);
Error CommandListRef_clearImageu(CommandListRef *commandList, const U32 color[4], ImageRange range, RefPtr *image);

TList(ClearImageCmd);

Error CommandListRef_clearImages(CommandListRef *commandList, ListClearImageCmd clearImages);

//Copy image
//Only allowed on SwapchainRef, RenderTargetRef, DeviceTextureRef and DepthStencilRef.
//Though DepthStencilRef requires both src and dst to be depth stencil.
//The texture formats need to be the same to allow copying.
//isStencil is only allowed when the input and output both have stencil buffers.
//When depthStencil is used, the stride of the stencil and/or depth needs to match between src and dst.

TList(CopyImageRegion);

Error CommandListRef_copyImageRegions(
	CommandListRef *commandList, RefPtr *src, RefPtr *dst, ECopyType copyType, ListCopyImageRegion regions
);

Error CommandListRef_copyImage(
	CommandListRef *commandList, RefPtr *src, RefPtr *dst, ECopyType copyType, CopyImageRegion region
);

//Draw calls and dispatches

TList(Transition);
TList(CommandScopeDependency);

Error CommandListRef_startScope(
	CommandListRef *commandList,
	ListTransition transitions,
	U32 id,
	ListCommandScopeDependency dependencies
);

Error CommandListRef_endScope(CommandListRef *commandList);

Error CommandListRef_setPipeline(CommandListRef *commandList, PipelineRef *pipeline, EPipelineType type);
Error CommandListRef_setComputePipeline(CommandListRef *commandList, PipelineRef *pipeline);
Error CommandListRef_setGraphicsPipeline(CommandListRef *commandList, PipelineRef *pipeline);

Error CommandListRef_setPrimitiveBuffers(CommandListRef *commandList, SetPrimitiveBuffersCmd buffers);

Error CommandListRef_draw(CommandListRef *commandList, DrawCmd draw);

Error CommandListRef_drawIndexed(CommandListRef *commandList, U32 indexCount, U32 instanceCount);
Error CommandListRef_drawUnindexed(CommandListRef *commandList, U32 vertexCount, U32 instanceCount);

Error CommandListRef_drawIndexedAdv(
	CommandListRef *commandList,
	U32 indexCount,
	U32 instanceCount,
	U32 indexOffset,
	U32 instanceOffset,
	U32 vertexOffset
);

Error CommandListRef_drawUnindexedAdv(
	CommandListRef *commandList,
	U32 vertexCount,
	U32 instanceCount,
	U32 vertexOffset,
	U32 instanceOffset
);

Error CommandListRef_drawIndirect(
	CommandListRef *commandList,
	DeviceBufferRef *buffer,
	U64 bufferOffset,
	U32 bufferStride,
	U32 drawCalls,
	Bool indexed
);

Error CommandListRef_drawIndirectCountExt(
	CommandListRef *commandList,
	DeviceBufferRef *buffer,
	U64 bufferOffset,
	U32 bufferStride,
	DeviceBufferRef *countBuffer,
	U64 countOffset,
	U32 maxDrawCalls,
	Bool indexed
);

Error CommandListRef_dispatch(CommandListRef *commandList, DispatchCmd dispatch);
Error CommandListRef_dispatch1D(CommandListRef *commandList, U32 groupsX);
Error CommandListRef_dispatch2D(CommandListRef *commandList, U32 groupsX, U32 groupsY);
Error CommandListRef_dispatch3D(CommandListRef *commandList, U32 groupsX, U32 groupsY, U32 groupsZ);

Error CommandListRef_dispatchIndirect(CommandListRef *commandList, DeviceBufferRef *buffer, U64 offset);

//DynamicRendering feature

TList(AttachmentInfo);

Error CommandListRef_startRenderExt(
	CommandListRef *commandList,
	I32x2 offset,
	I32x2 size,
	ListAttachmentInfo colors,
	AttachmentInfo depth,
	AttachmentInfo stencil
);

Error CommandListRef_endRenderExt(CommandListRef *commandList);

//DebugMarkers feature

Error CommandListRef_addMarkerDebugExt(CommandListRef *commandList, F32x4 color, CharString name);
Error CommandListRef_startRegionDebugExt(CommandListRef *commandList, F32x4 color, CharString name);
Error CommandListRef_endRegionDebugExt(CommandListRef *commandList);
