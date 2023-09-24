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

typedef RefPtr GraphicsDeviceRef;
typedef struct CharString CharString;
typedef struct GraphicsDevice GraphicsDevice;

typedef enum ECommandOp {

	ECommandOp_nop,

	//Setting dynamic graphic pipeline

	ECommandOp_setViewport,
	ECommandOp_setScissor,
	ECommandOp_setViewportAndScissor,

	ECommandOp_setStencil,

	//Direct rendering

	ECommandOp_startRenderingExt,
	ECommandOp_endRenderingExt,

	//Clearing depth + color views

	ECommandOp_clearImages,
	//ECommandOp_clearDepths,

	//Debugging

	ECommandOp_addMarkerDebugExt,
	ECommandOp_startRegionDebugExt,
	ECommandOp_endRegionDebugExt

} ECommandOp;

typedef enum ECommandListState {

	ECommandListState_New,			//Never opened
	ECommandListState_Open,			//No end has been called yet
	ECommandListState_Closed		//End has already been called

} ECommandListState;

typedef struct CommandOpInfo {

	ECommandOp op;
	U32 opSize;

} CommandOpInfo;

typedef struct CommandList {

	GraphicsDeviceRef *device;

	Buffer data;			//Data for all commands
	List commandOps;		//List<CommandOpInfo>
	List resources;			//List<RefPtr*> resources used by this command list (TODO: HashSet<RefPtr*>)

	List callstacks;		//Used to handle error handling (Only has call stack depth 32 to save some space)

	U32 padding;
	ECommandListState state;

	U64 next;

} CommandList;

typedef RefPtr CommandListRef;

#define CommandList_ext(ptr, T) (!ptr ? NULL : (T##CommandList*)(ptr + 1))		//impl
#define CommandListRef_ptr(ptr) RefPtr_data(ptr, CommandList)

Error CommandListRef_dec(CommandListRef **cmd);
Error CommandListRef_add(CommandListRef *cmd);

Error GraphicsDeviceRef_createCommandList(
	GraphicsDeviceRef *device, 
	U64 commandListLen, 
	U64 estimatedCommandCount,
	U64 estimatedResources,
	CommandListRef **commandList
);

Error CommandListRef_clear(CommandListRef *commandList);
Error CommandListRef_begin(CommandListRef *commandList, Bool doClear);
Error CommandListRef_end(CommandListRef *commandList);

Error CommandListRef_setViewport(CommandListRef *commandList, I32x2 offset, I32x2 size);
Error CommandListRef_setScissor(CommandListRef *commandList, I32x2 offset, I32x2 size);
Error CommandListRef_setViewportAndScissor(CommandListRef *commandList, I32x2 offset, I32x2 size);

Error CommandListRef_setStencil(CommandListRef *commandList, U8 stencilValue);

//Setting clear parameters and clearing render texture

typedef struct ImageRange {

	U32 levelId;		//Set to U32_MAX to indicate all levels, otherwise indicates specific index.
	U32 layerId;		//Set to U32_MAX to indicate all layers, otherwise indicates specific index.

} ImageRange;

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

/* typedef struct ClearDepthStencil {

	F32 depth;

	U8 stencil, padding[3];

	ImageRange image;

} ClearDepthStencil; */

//Clear entire resource or subresource .
//Color: SwapchainRef or RenderTargetRef, Depth: RenderTargetRef

Error CommandListRef_clearImagef(CommandListRef *commandList, F32x4 color, ImageRange range, RefPtr *image);
Error CommandListRef_clearImagei(CommandListRef *commandList, I32x4 color, ImageRange range, RefPtr *image);
Error CommandListRef_clearImageu(CommandListRef *commandList, const U32 color[4], ImageRange range, RefPtr *image);

Error CommandListRef_clearImages(CommandListRef *commandList, List clearImages);		//<ClearImage>

//Error CommandListRef_clearDepthStencil(CommandListRef *commandList, F32 depth, U8 stencil, ImageRange image);

//DynamicRendering feature

typedef enum ELoadAttachmentType {
	
	ELoadAttachmentType_Preserve,		//Don't modify the contents. Can have overhead because it has to load it.
	ELoadAttachmentType_Clear,			//Clears the contents at start.
	ELoadAttachmentType_Any				//Can preserve or clear depending on implementation. Result will be overwritten.

} ELoadAttachmentType;

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

//Convert command into API dependent instructions
//Don't call manually.

impl Error CommandList_process(GraphicsDevice *device, ECommandOp op, const U8 *data, void *commandListExt);
