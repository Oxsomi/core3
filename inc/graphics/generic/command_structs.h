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
#include "types/list.h"
#include "math/vec.h"
#include "platforms/ref_ptr.h"
#include "graphics/generic/device.h"

typedef RefPtr GraphicsDeviceRef;
typedef RefPtr PipelineRef;
typedef RefPtr DeviceBufferRef;

typedef enum ECommandOp {

	//Setting dynamic graphic pipeline

	ECommandOp_SetViewport,						//Don't move, these use op bitmask (0-2)
	ECommandOp_SetScissor,						//Don't move, these use bitmask (0-2)
	ECommandOp_SetViewportAndScissor,			//Don't move, these use bitmask (0-2)

	ECommandOp_SetStencil,
	ECommandOp_SetBlendConstants,

	//Direct rendering

	ECommandOp_StartRenderingExt,
	ECommandOp_EndRenderingExt,

	//Draw calls and dispatches

	ECommandOp_SetGraphicsPipeline,
	ECommandOp_SetComputePipeline,
	ECommandOp_StartScope,
	ECommandOp_EndScope,

	ECommandOp_SetPrimitiveBuffers,

	ECommandOp_Draw,
	ECommandOp_DrawIndirect,
	ECommandOp_DrawIndirectCount,

	ECommandOp_Dispatch,
	ECommandOp_DispatchIndirect,

	//Clearing depth + color views

	ECommandOp_ClearImages,
	//ECommandOp_ClearDepths,

	//Debugging

	ECommandOp_AddMarkerDebugExt,
	ECommandOp_StartRegionDebugExt,
	ECommandOp_EndRegionDebugExt

} ECommandOp;

typedef enum ECommandListState {

	ECommandListState_New,			//Never opened.
	ECommandListState_Open,			//No end has been called yet.
	ECommandListState_Closed,		//End has already been called (successfully).
	ECommandListState_Invalid		//Something in the command list has gone wrong and it couldn't be closed.

} ECommandListState;

typedef struct CommandOpInfo {

	ECommandOp op;
	U32 opSize;

} CommandOpInfo;

typedef enum EPipelineStage EPipelineStage;

typedef struct ImageRange {

	U32 levelId;		//Set to U32_MAX to indicate all levels, otherwise indicates specific index.
	U32 layerId;		//Set to U32_MAX to indicate all layers, otherwise indicates specific index.

} ImageRange;

typedef union ResourceRange {

	ImageRange image;
	BufferRange buffer;

} ResourceRange;

typedef enum ETransitionType {

	ETransitionType_ClearColor,
	ETransitionType_Vertex,
	ETransitionType_Index,
	ETransitionType_Indirect,
	ETransitionType_ShaderRead,
	ETransitionType_ShaderWrite,
	ETransitionType_RenderTargetRead,
	ETransitionType_RenderTargetWrite

} ETransitionType;

typedef struct TransitionInternal {		//Transitions issued by a scope.

	RefPtr *resource;					//Currently only supports Swapchain or DeviceBuffer

	ResourceRange range;

	EPipelineStage stage;				//First shader stage that will access this resource (if !usage)

	U8 padding[3];
	U8 type;							//ETransitionType

} TransitionInternal;

typedef struct Transition {

	RefPtr *resource;					//Currently only supports Swapchain or DeviceBuffer

	ResourceRange range;

	EPipelineStage stage;				//First shader stage that will access this resource

	U8 padding[3];
	Bool isWrite;

} Transition;

//Dependencies are executed before itself; important for threading.

typedef enum ECommandScopeDependencyType {

	ECommandScopeDependencyType_Unconditional,	//If dependency is present, wait for it to finish, otherwise no-op.
	ECommandScopeDependencyType_Weak,			//If dependency is hidden then hide self.
	ECommandScopeDependencyType_Strong			//If dependency is hidden, give error and hide self.

} ECommandScopeDependencyType;

typedef struct CommandScopeDependency {

	ECommandScopeDependencyType type;
	U32 id;

} CommandScopeDependency;

typedef struct CommandScope {

	U64 commandBufferOffset;

	U64 commandBufferLength;

	U32 commandOpOffset, transitionOffset;

	U32 commandOps, padding;

	U32 transitionCount, scopeId;

} CommandScope;

typedef enum ECommandStateFlags {

	ECommandStateFlags_AnyScissor		= 1 << 0,
	ECommandStateFlags_AnyViewport		= 1 << 1,
	ECommandStateFlags_HasModifyOp		= 1 << 2,		//If it has any op that modifies any resource (clear/copy/shader/etc.)
	ECommandStateFlags_HasScope			= 1 << 3,		//If it's in a scope
	ECommandStateFlags_InvalidState		= 1 << 4		//One of the commands in the scope was invalid; ignore scope.

} ECommandStateFlags;

typedef union ClearColor {

	U32 coloru[4];
	I32 colori[4];
	F32 colorf[4];

} ClearColor;

typedef struct ClearImage {

	ClearColor color;

	ImageRange range;

	RefPtr *image;

} ClearImage;

/* typedef struct ClearDepthStencil {		TODO:

	F32 depth;

	U8 stencil, padding[3];

	ImageRange image;

} ClearDepthStencil; */

typedef struct PrimitiveBuffers {

	DeviceBufferRef *vertexBuffers[16];
	DeviceBufferRef *indexBuffer;

	Bool isIndex32Bit;
	U8 padding[3];

} PrimitiveBuffers;

typedef struct Draw {

	U32 count, instanceCount;
	U32 vertexOffset, instanceOffset;

	U32 indexOffset;

	Bool isIndexed;
	U8 padding[3];

} Draw;

typedef struct DrawIndirect {

	DeviceBufferRef *buffer;				//Draw commands (or draw indexed commands)
	DeviceBufferRef *countBufferExt;		//If defined, uses draw indirect count and specifies the buffer that holds counter

	U64 bufferOffset, countOffsetExt;

	U32 drawCalls, bufferStride;

	Bool isIndexed;
	U8 pad[7];

} DrawIndirect;

typedef struct DrawCallUnindexed {

	U32 vertexCount, instanceCount;
	U32 vertexOffset, instanceOffset;

} DrawCallUnindexed;

typedef struct DrawCallIndexed {

	U32 indexCount, instanceCount;
	U32 indexOffset; I32 vertexOffset;

	U32 instanceOffset;
	U32 padding[3];			//For alignment

} DrawCallIndexed;

typedef struct Dispatch { U32 groups[3]; } Dispatch;
typedef struct DispatchIndirect { DeviceBufferRef *buffer; U64 offset; } DispatchIndirect;

typedef enum ELoadAttachmentType {

	ELoadAttachmentType_Preserve,		//Don't modify the contents. Can have overhead because it has to load it.
	ELoadAttachmentType_Clear,			//Clears the contents at start.
	ELoadAttachmentType_Any				//Can preserve or clear depending on implementation. Result will be overwritten.

} ELoadAttachmentType;

typedef struct ImageAndRange {

	ImageRange range;
	RefPtr *image;

} ImageAndRange;

typedef struct AttachmentInfo {

	ImageRange range;			//Subresource. Multiple resources isn't allowed (layerId, levelId != U32_MAX)
	RefPtr *image;				//Swapchain or RenderTexture. Null is allowed to disable it.

	ELoadAttachmentType load;

	U8 padding[3];
	Bool readOnly;				//If true, this attachment will only be an input, output is ignored

	ClearColor color;

} AttachmentInfo;

typedef struct StartRenderExt {

	I32x2 offset, size;

	U8 colorCount;			//Count of render targets (even inactive ones).
	U8 activeMask;			//Which render targets are active.
	U8 depthStencil;		//& 1 = depth, & 2 = stencil
	U8 pad0;

	U32 pad1;

	//AttachmentInfo attachments[];	//[ active attachments go here ]

} StartRenderExt;
