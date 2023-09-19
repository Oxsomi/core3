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

	//Clearing targets / depth stencils

	ECommandOp_clearColorf,
	ECommandOp_clearColoru,
	ECommandOp_clearColori,
	ECommandOp_clearDepthStencil,

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

typedef enum ECommandListFlag {
	
	ECommandListFlag_hasBoundColor		= 1 << 0,
	ECommandListFlag_hasBoundDepth		= 1 << 1,
	
	ECommandListFlag_isIntTarget		= 1 << 2,
	ECommandListFlag_isUnsignedTarget	= 1 << 3,

	ECommandListFlag_F32Target			= ECommandListFlag_hasBoundColor,
	ECommandListFlag_I32Target			= ECommandListFlag_hasBoundColor | ECommandListFlag_isIntTarget,
	ECommandListFlag_U64Target			= ECommandListFlag_I32Target | ECommandListFlag_isUnsignedTarget,

	ECommandListFlag_maskTargetType		= ECommandListFlag_U64Target,

	ECommandListFlag_all				= ~0

} ECommandListFlag;

typedef struct CommandList {

	GraphicsDeviceRef *device;

	Buffer data;			//Data for all commands
	List commandOps;		//List<CommandOpInfo>
	List resources;			//List<RefPtr*> resources used by this command list (TODO: HashSet<RefPtr*>)

	ECommandListFlag flags;
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

Error CommandListRef_clearColorf(CommandListRef *commandList, F32x4 color);
Error CommandListRef_clearColori(CommandListRef *commandList, I32x4 color);
Error CommandListRef_clearColoru(CommandListRef *commandList, const U32 color[4]);
Error CommandListRef_clearDepthStencil(CommandListRef *commandList, F32 depth, U8 stencil);

Error CommandListRef_addMarkerDebugExt(CommandListRef *commandList, F32x4 color, CharString name);
Error CommandListRef_startRegionDebugExt(CommandListRef *commandList, F32x4 color, CharString name);
Error CommandListRef_endRegionDebugExt(CommandListRef *commandList);

//Convert command into API dependent instructions
//Don't call manually.

impl Error CommandList_process(GraphicsDevice *device, ECommandOp op, const U8 *data, void *commandListExt);
