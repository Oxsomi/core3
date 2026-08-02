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

//graphics/generic/command_list_internal.h

#pragma once
#include "graphics/generic/command_list.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Internal declarations shared between the command_list*.c translation units.
//These were file-local to command_list.c before it was split and are not part of the public API.

typedef struct Pipeline Pipeline;
typedef RefPtr TextureRef;
typedef RefPtr RTASRef;

#define CommandListRef_validate(v)                                                                              \
																												\
	CommandList *commandList = NULL;                                                                            \
																												\
	if(!(v) || (v)->refPtrType->typeId != (TypeId)EGraphicsTypeId_CommandList)                                 \
		retError(clean, Error_nullPointer(0, "CommandListRef_validate() cmdlist is invalid"));                  \
																												\
	commandList = CommandListRef_ptr(v);                                                                        \
																												\
	if(!SpinLock_isLockedForThread(&commandList->lock))                                                         \
		retError(clean, Error_invalidOperation(0, "CommandListRef_validate() cmdlist isn't locked"));           \
																												\
	if(commandList->state != ECommandListState_Open)                                                            \
		retError(clean, Error_invalidOperation(1, "CommandListRef_validate() cmdlist isn't open"));

#define CommandListRef_validateScope(v, label)                                                                  \
																												\
	CommandListRef_validate(v);                                                                                 \
																												\
	if(!(commandList->tempStateFlags & ECommandStateFlags_HasScope))                                            \
		retError(label, Error_invalidOperation(0, "CommandListRef_validateScope() scope isn't open"));

Bool CommandList_append(CommandList *commandList, ECommandOp op, Buffer buf, U32 extraSkipStacktrace, Error *e_rr);

Bool CommandListRef_isBound(CommandList *commandList, RefPtr *resource, ResourceRange range, TransitionInternal **found);

Bool CommandListRef_transitionBuffer(
	CommandList *commandList,
	DeviceBufferRef *buffer,
	BufferRange range,
	ETransitionType type,
	EPipelineStage stage,
	Error *e_rr
);

Bool CommandListRef_transitionImage(
	CommandList *commandList,
	TextureRef *image,
	ImageRange range,
	ETransitionType type,
	EPipelineStage stage,
	Error *e_rr
);

Bool CommandListRef_transitionRTAS(
	CommandList *commandList,
	RTASRef *rtasPtr,
	ETransitionType type,
	EPipelineStage stage,
	Error *e_rr
);

Bool CommandList_validateGraphicsPipeline(
	Pipeline *pipeline,
	ImageAndRange images[8],
	U8 imageCount,
	EDepthStencilFormat depthFormat,
	EMSAASamples boundSampleCount,
	Error *e_rr
);

Bool CommandListRef_checkBounds(I32x2 offset, I32x2 size, I32 lowerBound1, I32 upperBound1, Error *e_rr);

#ifdef __cplusplus
	}
#endif
