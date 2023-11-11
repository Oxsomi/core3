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
#include "pipeline_structs.h"

typedef struct CharString CharString;
typedef struct GraphicsDevice GraphicsDevice;
typedef enum EPipelineType EPipelineType;

typedef struct CommandList {

	GraphicsDeviceRef *device;

	Buffer data;						//Data for all commands
	List commandOps;					//List<CommandOpInfo>
	List resources;						//List<RefPtr*> resources used by this command list (TODO: HashSet<RefPtr*>)

	List transitions;					//<TransitionInternal> Transitions that are pending
	List activeScopes;					//<CommandScope> Scopes that were successfully inserted

	U8 padding0[3];
	Bool allowResize;
	ECommandListState state;

	U64 next;

	//Temp state for the last scope

	PipelineRef *pipeline[EPipelineType_Count];

	ImageAndRange boundImages[8];

	U16 tempStateFlags;					//ECommandStateFlags
	U8 debugRegionStack, boundImageCount;
	U32 lastCommandId;

	I32x2 currentSize;

	U64 lastOffset;

	U32 lastScopeId;
	EDepthStencilFormat boundDepthFormat;
	
	List pendingTransitions;			//<TransitionInternal>

} CommandList;

typedef RefPtr CommandListRef;

#define CommandList_ext(ptr, T) (!ptr ? NULL : (T##CommandList*)(ptr + 1))		//impl
#define CommandListRef_ptr(ptr) RefPtr_data(ptr, CommandList)

Error CommandListRef_dec(CommandListRef **cmd);
Error CommandListRef_inc(CommandListRef *cmd);

Error GraphicsDeviceRef_createCommandList(
	GraphicsDeviceRef *device, 
	U64 commandListLen, 
	U64 estimatedCommandCount,
	U64 estimatedResources,
	Bool allowResize,
	CommandListRef **commandList
);

Error CommandListRef_clear(CommandListRef *commandList);
Error CommandListRef_begin(CommandListRef *commandList, Bool doClear);
Error CommandListRef_end(CommandListRef *commandList);

Error CommandListRef_setViewport(CommandListRef *commandList, I32x2 offset, I32x2 size);
Error CommandListRef_setScissor(CommandListRef *commandList, I32x2 offset, I32x2 size);
Error CommandListRef_setViewportAndScissor(CommandListRef *commandList, I32x2 offset, I32x2 size);

Error CommandListRef_setStencil(CommandListRef *commandList, U8 stencilValue);
Error CommandListRef_setBlendConstants(CommandListRef *commandList, F32x4 blendConstants);

//Setting clear parameters and clearing render texture

//Clear entire resource or subresource .
//Color: SwapchainRef or RenderTargetRef, Depth: RenderTargetRef

Error CommandListRef_clearImagef(CommandListRef *commandList, F32x4 color, ImageRange range, RefPtr *image);
Error CommandListRef_clearImagei(CommandListRef *commandList, I32x4 color, ImageRange range, RefPtr *image);
Error CommandListRef_clearImageu(CommandListRef *commandList, const U32 color[4], ImageRange range, RefPtr *image);

Error CommandListRef_clearImages(CommandListRef *commandList, List clearImages);		//<ClearImage>

//Error CommandListRef_clearDepthStencil(CommandListRef *commandList, F32 depth, U8 stencil, ImageRange image);

//Draw calls and dispatches

//List<Transition>, List<CommandScopeDependency>
Error CommandListRef_startScope(CommandListRef *commandList, List transitions, U32 id, List dependencies);
Error CommandListRef_endScope(CommandListRef *commandList);

Error CommandListRef_setPipeline(CommandListRef *commandList, PipelineRef *pipeline, EPipelineType type);
Error CommandListRef_setComputePipeline(CommandListRef *commandList, PipelineRef *pipeline);
Error CommandListRef_setGraphicsPipeline(CommandListRef *commandList, PipelineRef *pipeline);

Error CommandListRef_setPrimitiveBuffers(CommandListRef *commandList, PrimitiveBuffers buffers);

Error CommandListRef_draw(CommandListRef *commandList, Draw draw);

Error CommandListRef_drawIndexed(CommandListRef *commandList, U32 indexCount, U32 instanceCount);
Error CommandListRef_drawIndexedAdv(
	CommandListRef *commandList, 
	U32 indexCount, U32 instanceCount, 
	U32 indexOffset, U32 instanceOffset,
	U32 vertexOffset
);

Error CommandListRef_drawUnindexed(CommandListRef *commandList, U32 vertexCount, U32 instanceCount);
Error CommandListRef_drawUnindexedAdv(
	CommandListRef *commandList, 
	U32 vertexCount, U32 instanceCount, 
	U32 vertexOffset, U32 instanceOffset
);

Error CommandListRef_drawIndirect(
	CommandListRef *commandList, 
	DeviceBufferRef *buffer, U64 bufferOffset, U32 bufferStride, 
	U32 drawCalls, Bool indexed
);

Error CommandListRef_drawIndirectCountExt(
	CommandListRef *commandList, 
	DeviceBufferRef *buffer, U64 bufferOffset, U32 bufferStride, 
	DeviceBufferRef *countBuffer, U64 countOffset,
	U32 maxDrawCalls, Bool indexed
);

Error CommandListRef_dispatch(CommandListRef *commandList, Dispatch dispatch);
Error CommandListRef_dispatch1D(CommandListRef *commandList, U32 groupsX);
Error CommandListRef_dispatch2D(CommandListRef *commandList, U32 groupsX, U32 groupsY);
Error CommandListRef_dispatch3D(CommandListRef *commandList, U32 groupsX, U32 groupsY, U32 groupsZ);

Error CommandListRef_dispatchIndirect(CommandListRef *commandList, DeviceBufferRef *buffer, U64 offset);

//DynamicRendering feature

Error CommandListRef_startRenderExt(
	CommandListRef *commandList, 
	I32x2 offset, 
	I32x2 size, 
	List colors,				//<AttachmentInfo>
	List depthStencil			//<AttachmentInfo>
);

Error CommandListRef_endRenderExt(CommandListRef *commandList);

//DebugMarkers feature

Error CommandListRef_addMarkerDebugExt(CommandListRef *commandList, F32x4 color, CharString name);
Error CommandListRef_startRegionDebugExt(CommandListRef *commandList, F32x4 color, CharString name);
Error CommandListRef_endRegionDebugExt(CommandListRef *commandList);
