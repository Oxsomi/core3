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

typedef enum ECommandOp {

	ECommandOp_setViewport,
	ECommandOp_setScissor,
	ECommandOp_setViewportAndScissor,

	ECommandOp_setStencil,

	ECommandOp_clearColor,
	ECommandOp_clearDepthStencil,

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

	U32 pad;
	ECommandListState state;

	U64 next;

} CommandList;

typedef RefPtr CommandListRef;

#define CommandList_ext(ptr, T) ((T##CommandList*)(ptr + 1))		//impl
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

Error CommandListRef_clearColor(CommandListRef *commandList, F32x4 color);
Error CommandListRef_clearDepthStencil(CommandListRef *commandList, F32 depth, U8 stencil);

Error CommandListRef_addMarkerDebugExt(CommandListRef *commandList, F32x4 color, CharString name);
Error CommandListRef_startRegionDebugExt(CommandListRef *commandList, F32x4 color, CharString name);
Error CommandListRef_endRegionDebugExt(CommandListRef *commandList);
