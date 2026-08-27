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

//graphics/generic/command_structs.h

#pragma once
#include "types/math/vec2.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/pipeline_structs.h"
#include "graphics/generic/resource.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef RefPtr GraphicsDeviceRef;
typedef RefPtr PipelineRef;
typedef RefPtr DepthStencilRef;
typedef RefPtr DeviceBufferRef;
typedef RefPtr DescriptorTableRef;
typedef RefPtr DescriptorHeapRef;

typedef enum ECommandOp {

	//Every command needs to belong to a CommandScope (see Scopes in the api spec)

	ECommandOp_StartScope,
	ECommandOp_EndScope,

	//Setting dynamic graphic pipeline

	ECommandOp_SetViewport,                      //Don't move id, these use op bitmask (0-2)
	ECommandOp_SetScissor,                       //^
	ECommandOp_SetViewportAndScissor,            //^

	ECommandOp_SetStencil,
	ECommandOp_SetBlendConstants,

	//Direct rendering

	ECommandOp_StartRenderingExt,
	ECommandOp_EndRenderingExt,

	//Draw calls and dispatches

	ECommandOp_SetGraphicsPipeline,
	ECommandOp_SetComputePipeline,

	ECommandOp_SetPrimitiveBuffers,

	ECommandOp_Draw,
	ECommandOp_DrawIndirect,
	ECommandOp_DrawIndirectCount,

	ECommandOp_Dispatch,
	ECommandOp_DispatchIndirect,

	//Clearing and copying images

	ECommandOp_ClearImages,
	ECommandOp_CopyImage,

	//Debugging

	ECommandOp_AddMarkerDebugExt,
	ECommandOp_StartRegionDebugExt,
	ECommandOp_EndRegionDebugExt,

	//GPU timestamps

	ECommandOp_StartTimingRegion,
	ECommandOp_EndTimingRegion,
	ECommandOp_InsertTiming,

	//Raytracing

	ECommandOp_DispatchRaysExt,
	ECommandOp_DispatchRaysIndirect,
	ECommandOp_SetRaytracingPipelineExt,
	ECommandOp_UpdateBLASExt,
	ECommandOp_UpdateTLASExt,
	ECommandOp_UpdateOmmExt,

	//Bindful: set descriptor state; backends emit the actual binds lazily at the work op.
	//The heap bind is a separate EXPLICIT command because switching heaps is expensive on some hardware
	// (D3D12 can stall the GPU), so it must be visible in the recording rather than implied by a table.

	ECommandOp_BindDescriptorTable,
	ECommandOp_BindDescriptorHeap,
	ECommandOp_SetPushConstants,
	ECommandOp_SetPushDescriptors

} ECommandOp;

typedef enum ECommandListState {
	ECommandListState_New,           //Never opened.
	ECommandListState_Open,          //No end has been called yet.
	ECommandListState_Closed,        //End has already been called (successfully).
	ECommandListState_Invalid        //Something in the command list has gone wrong and it couldn't be closed.
} ECommandListState;

typedef struct CommandOpInfo {
	ECommandOp op;
	U32 opSize;
} CommandOpInfo;

typedef enum ETransitionType {
	ETransitionType_Clear,
	ETransitionType_Vertex,          //TODO: Generic read?
	ETransitionType_Index,
	ETransitionType_Indirect,
	ETransitionType_ShaderRead,
	ETransitionType_ShaderWrite,
	ETransitionType_RenderTargetRead,
	ETransitionType_RenderTargetWrite,
	ETransitionType_ResolveTargetWrite,
	ETransitionType_CopyRead,
	ETransitionType_CopyWrite,
	ETransitionType_KeepAlive            //If the only reason of this transition is to keep a resource alive
} ETransitionType;

//Push constant payload. Fixed size rather than variable: payloads are packed back to back with no padding
//between them, so an odd sized one would misalign the typed reads that later ops perform on their own data.
//144 bytes keeps the 16 byte payload convention the other commands hold to, so every following payload stays
//as aligned as the buffer itself.

typedef struct SetPushConstantsCmd {

	U32 size;
	U32 padding[3];

	U8 data[128];

} SetPushConstantsCmd;

//Push descriptor payload. Variable length: a 16 byte header followed by exactly count Descriptors, which
//keeps the whole payload a multiple of 16 (a Descriptor is 32 bytes) without a fixed worst case in every
//command. The count rides in the header so the backends can find the array without an opSize.
//How many there can be is OXC3_MAX_PUSH_DESCRIPTORS, in descriptor_layout.h.

typedef struct SetPushDescriptorsCmd {

	U32 count;
	U32 padding[3];

	//Descriptor descriptors[count];

} SetPushDescriptorsCmd;

typedef struct TransitionInternal {      //Transitions issued by a scope.

	RefPtr *resource;                    //Swapchain, RenderTexture, DepthStencil, DeviceBuffer, DeviceTexture, Sampler

	ResourceRange range;

	U32 stageMask;                       //Shader stages that will access this resource (if !type), see above
	ETransitionType type;

} TransitionInternal;

typedef struct Transition {

	RefPtr *resource;                    //Swapchain, RenderTexture, DepthStencil, DeviceBuffer, DeviceTexture, Sampler

	ResourceRange range;

	EPipelineStage stage;                //First shader stage that will access this resource

	U8 padding[3];
	Bool isWrite;

} Transition;

//Dependencies are executed before itself; important for threading.

typedef enum ECommandScopeDependencyType {
	ECommandScopeDependencyType_Unconditional,     //If dependency is present, wait for it to finish, otherwise no-op.
	ECommandScopeDependencyType_Conditional        //If dependency is hidden, give error and hide self.
} ECommandScopeDependencyType;

typedef struct CommandScopeDependency {
	ECommandScopeDependencyType type;
	U32 id;
} CommandScopeDependency;

typedef enum ECommandScopeFlags {

	ECommandScopeFlags_None             = 0,

	//Skip this scope's begin and end timestamp while the list is timing scopes (CommandListRef_setScopeTimingExt):
	// a scope that does little, or one the caller times itself with a manual region, spends no query slots.

	ECommandScopeFlags_DisableTimestamp = 1 << 0,

	//Skip this scope's auto debug region while the list is generating them (CommandListRef_setScopeDebugExt).

	ECommandScopeFlags_DisableDebug     = 1 << 1

} ECommandScopeFlags;

//What a scope decided at endScope, read by the backends. The name for either lives in the StartScope payload.

typedef enum ECommandScopeInternalFlags {
	ECommandScopeInternalFlags_Timed       = 1 << 0,    //Emits a begin and end GPU timestamp keyed by scopeId
	ECommandScopeInternalFlags_DebugRegion = 1 << 1     //Emits a begin and end debug region labelled by its name
} ECommandScopeInternalFlags;

//StartTimingRegion and InsertTiming carry this prefix then a NUL terminated name,
// the whole command padded to 16 bytes like a debug marker; EndTimingRegion carries nothing.
//The id is a hot path key that reads timings back without comparing strings,
// and the name is a convenience that is empty when the caller passes none.

typedef struct TimingRegionCmd {
	U32 id;
	U32 padding;
} TimingRegionCmd;

typedef struct CommandScope {

	U64 commandBufferOffset;

	U64 commandBufferLength;

	U32 commandOpOffset, transitionOffset;

	U32 commandOps;
	U32 flags;                           //ECommandScopeInternalFlags, set at endScope

	U32 transitionCount, scopeId;

} CommandScope;

typedef enum ECommandStateFlags {
	ECommandStateFlags_AnyScissor          = 1 << 0,
	ECommandStateFlags_AnyViewport         = 1 << 1,
	ECommandStateFlags_HasModifyOp         = 1 << 2,       //If it has any op that modifies any resource (draw/dispatch/copy)
	ECommandStateFlags_HasScope            = 1 << 3,       //If it's in a scope
	ECommandStateFlags_InvalidState        = 1 << 4        //One of the commands in the scope was invalid; ignore scope.
} ECommandStateFlags;

typedef union ClearColor {
	U32 coloru[4];
	I32 colori[4];
	F32 colorf[4];
} ClearColor;

//All commands have to be 16-byte aligned

typedef struct ClearImageCmd {
	ClearColor color;
	ImageRange range;
	RefPtr *image;
} ClearImageCmd;

typedef struct SetPrimitiveBuffersCmd {

	DeviceBufferRef *vertexBuffers[16];
	DeviceBufferRef *indexBuffer;

	Bool isIndex32Bit;
	U8 padding[7];

} SetPrimitiveBuffersCmd;

typedef struct DrawCmd {

	U32 count, instanceCount;
	U32 vertexOffset, instanceOffset;

	U32 indexOffset;

	Bool isIndexed;
	U8 padding[11];

} DrawCmd;

typedef struct DrawIndirectCmd {

	DeviceBufferRef *buffer;                //Draw commands (or draw indexed commands)
	DeviceBufferRef *countBufferExt;        //If defined, uses draw indirect count and specifies the buffer that holds counter

	U64 bufferOffset, countOffsetExt;

	U32 drawCalls;

	Bool isIndexed;
	U8 pad[7];

} DrawIndirectCmd;

typedef struct DispatchCmd { U32 groups[3], padding; } DispatchCmd;
typedef struct DispatchIndirectCmd { DeviceBufferRef *buffer; U64 offset; } DispatchIndirectCmd;

typedef enum ELoadAttachmentType {
	ELoadAttachmentType_Any,             //Can preserve or clear depending on implementation. Result will be overwritten.
	ELoadAttachmentType_Preserve,        //Keep the contents. Can have overhead because it has to load it (preserveMask).
	ELoadAttachmentType_Clear            //Clears the contents at start.
} ELoadAttachmentType;

typedef struct ImageAndRange {
	ImageRange range;
	RefPtr *image;
} ImageAndRange;

typedef enum EMSAAResolveMode {
	EMSAAResolveMode_Average,
	EMSAAResolveMode_Min,
	EMSAAResolveMode_Max,
	EMSAAResolveMode_Count
} EMSAAResolveMode;

typedef struct AttachmentInfo {

	ImageRange range;                    //Subresource. Multiple resources isn't allowed (layerId, levelId != U32_MAX)
	RefPtr *image;                       //Swapchain or RenderTexture. Null is allowed to disable it.

	ELoadAttachmentType load;

	Bool unusedAfterRender;
	U8 resolveMode;                      //EMSAAResolveMode
	Bool readOnly;
	U8 pad0;

	RefPtr *resolveImage;                //RenderTexture, DepthStencil or Swapchain. Null is allowed to disable resolving.

	ImageRange resolveRange;             //Subresource. Multiple resources isn't allowed (layerId, levelId != U32_MAX)

	ClearColor color;

	U64 padding;

} AttachmentInfo;

typedef struct DepthStencilAttachmentInfo {

	ImageRange range;

	RefPtr *image;

	ImageRange resolveImageRange;

	RefPtr *resolveImage;                //DepthStencil. Null is allowed to disable resolving.

	U8 depthLoad, stencilLoad;           //ELoadAttachmentType
	U8 depthStencilResolve, padding0;    //EMSAAResolveMode

	F32 clearDepth;

	U8 clearStencil;
	U8 padding1[3];

	Bool depthUnusedAfterRender, stencilUnusedAfterRender;
	Bool depthReadOnly, stencilReadOnly;

} DepthStencilAttachmentInfo;

typedef struct AttachmentInfoInternal {

	ImageRange range;                    //Subresource. Multiple resources isn't allowed (layerId, levelId != U32_MAX)
	RefPtr *image;                       //Swapchain or RenderTexture. Null is allowed to disable it.

	ImageRange resolveRange;             //Subresource. Multiple resources isn't allowed (layerId, levelId != U32_MAX)
	RefPtr *resolveImage;                //RenderTexture, DepthStencil or Swapchain. Null is allowed to disable resolving.

	ClearColor color;

	EMSAAResolveMode resolveMode;
	U32 padding[3];

} AttachmentInfoInternal;

typedef enum EStartRenderFlags {

	EStartRenderFlags_Depth                     = 1 << 0,
	EStartRenderFlags_Stencil                   = 1 << 1,
	EStartRenderFlags_ClearDepth                = 1 << 2,
	EStartRenderFlags_ClearStencil              = 1 << 3,
	EStartRenderFlags_PreserveDepth             = 1 << 4,
	EStartRenderFlags_PreserveStencil           = 1 << 5,
	EStartRenderFlags_StencilUnusedAfterRender  = 1 << 6,        //Possibly discard after render (no need to keep result)
	EStartRenderFlags_DepthUnusedAfterRender    = 1 << 7,        //^

	EStartRenderFlags_StencilReadOnly           = 1 << 8,
	EStartRenderFlags_DepthReadOnly             = 1 << 9,

	EStartRenderFlags_DepthFlags                =
		EStartRenderFlags_Depth | EStartRenderFlags_ClearDepth | EStartRenderFlags_DepthReadOnly |
		EStartRenderFlags_PreserveDepth | EStartRenderFlags_StencilUnusedAfterRender,

	EStartRenderFlags_StencilFlags              =
		EStartRenderFlags_Stencil | EStartRenderFlags_ClearStencil | EStartRenderFlags_StencilReadOnly |
		EStartRenderFlags_PreserveStencil | EStartRenderFlags_StencilUnusedAfterRender,

	EStartRenderFlags_ClearDepthStencil = EStartRenderFlags_ClearDepth | EStartRenderFlags_ClearStencil

} EStartRenderFlags;

typedef struct StartRenderCmdExt {

	I32x2 offset, size;

	EMSAAResolveMode resolveDepthStencilMode;

	EStartRenderFlags flags;

	U8 readOnlyMask;             //Mark which render targets are readonly
	U8 preserveMask;             //Mark which render targets are preserved (ELoadAttachmentType) only if clearMask isn't set.
	U8 clearMask;                //Mark which render targets are for clear (ELoadAttachmentType) take prio over preserve.
	U8 unusedAfterRenderMask;    //Mark which render targets are unused after render

	U8 colorCount;               //Count of render targets (even inactive ones).
	U8 activeMask;               //Which render targets are active.
	U8 clearStencil;
	U8 pad0;

	F32 clearDepth;

	ImageRange depthStencilRange;
	ImageRange resolveDepthStencilRange;

	DepthStencilRef *depthStencil;

	DepthStencilRef *resolveDepthStencil;

	U64 pad1;

	//AttachmentInfoInternal attachments[];        //[ active attachments go here ]

} StartRenderCmdExt;

typedef struct CopyImageRegion {

	U16 srcX, srcY, srcZ;
	U16 srcLevelId;

	U16 dstX, dstY, dstZ;
	U16 dstLevelId;

	U16 width, height, length;
	U16 outputRotation;            //0, 1, 2, 3, useful for mobile (also known as 0, 90, 180, 270)

} CopyImageRegion;

typedef struct CopyImageCmd {

	RefPtr *src, *dst;        //dst = RenderTexture, src = RenderTexture or regular Texture

	U32 regionCount;
	U32 padding;

	//CopyImageRegion regions[regionCount];

} CopyImageCmd;

//Commands

typedef struct DrawCallUnindexed {
	U32 vertexCount, instanceCount;
	U32 vertexOffset, instanceOffset;
} DrawCallUnindexed;

typedef struct DrawCallIndexed {

	U32 indexCount, instanceCount;
	U32 indexOffset; I32 vertexOffset;

	U32 instanceOffset;
	U32 padding[3];            //For alignment

} DrawCallIndexed;

typedef struct Dispatch { U32 x, y, z, pad; } Dispatch;

typedef struct DispatchRaysExt { U32 x, y, z, raygenId; } DispatchRaysExt;        //raygenId can't be set if on GPU

typedef struct DispatchRaysIndirectExt {
	DeviceBufferRef *buffer;
	U64 offset;
	U32 raygenId, padding[3];
} DispatchRaysIndirectExt;

#ifdef __cplusplus
	}
#endif
