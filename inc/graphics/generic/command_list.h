/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/ref_ptr.h"

#ifdef __cplusplus
	extern "C" {
#endif

TList(CommandOpInfo);
TList(TransitionInternal);
TList(AttachmentInfoInternal);
TList(CommandScope);

typedef struct CommandList {

	GraphicsDeviceRef *device;

	Buffer data;									//Data for all commands
	ListCommandOpInfo commandOps;
	ListRefPtr resources;							//Resources used by this command list (TODO: HashSet<RefPtr*>)

	ListTransitionInternal transitions;				//Transitions that are pending
	ListCommandScope activeScopes;					//Scopes that were successfully inserted

	U8 padding0[3];
	Bool allowResize;
	ECommandListState state;

	Lock lock;										//Begin locks this, end unlocks this.

	U64 next;

	//Temp state for the last scope

	PipelineRef *pipeline[EPipelineType_Count];

	ImageAndRange boundImages[8];

	U16 tempStateFlags;								//ECommandStateFlags
	U8 debugRegionStack;
	U8 boundImageCount;
	U32 lastCommandId;

	I32x2 currentSize;

	U64 lastOffset;

	U32 lastScopeId;
	EDepthStencilFormat boundDepthFormat;

	EMSAASamples boundSampleCount;
	U32 padding;

	ListTransitionInternal pendingTransitions;

	ListDeviceResourceVersion activeSwapchains;		//Locks swapchain when it's first inserted

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

#ifdef __cplusplus
	}
#endif
