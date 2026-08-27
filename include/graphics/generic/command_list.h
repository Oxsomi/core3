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

//graphics/generic/command_list.h

#pragma once
#include "graphics/generic/command_structs.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/pipeline_structs.h"
#include "graphics/generic/resource.h"
#include "types/container/ref_ptr.h"
#include "types/container/list.h"
#include "types/base/lock.h"

#ifdef __cplusplus
	extern "C" {
#endif

TList(CommandOpInfo);
TList(TransitionInternal);
TList(AttachmentInfoInternal);
TList(CommandScope);

typedef struct CommandList {

	GraphicsDeviceRef *device;

	Buffer data;                                       //Data for all commands
	ListCommandOpInfo commandOps;
	ListRefPtr resources;                              //Resources used by this command list (TODO: HashSet<RefPtr*>)

	ListTransitionInternal transitions;                //Transitions that are pending
	ListCommandScope activeScopes;                     //Scopes that were successfully inserted

	U8 padding0[3];
	Bool allowResize;
	ECommandListState state;

	SpinLock lock;                                     //Begin locks this, end unlocks this.

	U64 next;

	//Temp state for the last scope

	PipelineRef *pipeline[EPipelineType_Count];

	//Bindful: heap and table state for pipelines with a custom layout; reset per scope like the pipelines
	// above. The work ops validate that the bound table's parent IS the bound heap.

	DescriptorTableRef *boundDescriptorTable;
	DescriptorHeapRef *boundDescriptorHeap;

	//Bindful: the push constant bytes a work op will hand the backend, kept inline because the layout caps
	// them at 128 bytes. Written by setPushConstants and reset per scope like every other bind state.
	//pushConstantSize is what was written, which the work op checks against the pipeline layout's own size:
	// a partial write would leave the rest of the range as whatever the last pipeline left behind.

	U8 pushConstantData[128];
	U8 pushConstantSize;
	U8 padding1[7];

	//Bindful: the push descriptors a work op will hand the backend, written by setPushDescriptors and reset
	// per scope like every other bind state.
	//The count is checked against the pipeline layout's push descriptor layout at the work op, for the same
	// reason a partial push constant write is refused: the rest would be whatever the last pipeline bound.

	Descriptor pushDescriptors[OXC3_MAX_PUSH_DESCRIPTORS];
	U8 pushDescriptorCount;
	U8 padding2[7];

	//The (pipeline, table, heap) triple the last successful work op validation ran against.
	//Bind state validation only depends on those identities, so as long as they match, re-validating is
	// skipped: 5000 draws with nothing rebound between them pay the price once.

	PipelineRef *validatedPipeline;
	DescriptorTableRef *validatedTable;
	DescriptorHeapRef *validatedHeap;

	ImageAndRange boundImages[8];

	U16 tempStateFlags;                                //ECommandStateFlags
	U8 debugRegionStack;
	U8 boundImageCount;
	U32 lastCommandId;

	I32x2 currentSize;

	U64 lastOffset;

	U32 lastScopeId;
	EDepthStencilFormat boundDepthFormat;

	EMSAASamples boundSampleCount;
	U32 padding;

	//Timing: set by CommandListRef_setScopeTimingExt before recording. When set, every scope that does not opt out
	// gets a begin and end GPU timestamp keyed by its scopeId. timingRegionStack balances manual start and end
	// regions the way debugRegionStack balances debug ones, and timingSlotCount counts the query slots this list
	// consumes at submit, which the device sums across the submit to size the timestamp pool.

	Bool timeScopes;
	Bool debugScopes;
	U8 timingRegionStack;
	U8 padding3;
	U32 timingSlotCount;
	U32 lastScopeSlotBase;                             //timingSlotCount at startScope, restored if the scope is hidden
	ECommandScopeFlags lastScopeFlags;                 //Flags of the open scope, carried startScope -> endScope
	Bool lastScopeNamed;                               //Whether the open scope was given a debug name

	//Whether the open scope carries a predicate. Requesting one without the capability is refused at
	// record time, so requested and predicated are the same fact.

	Bool lastScopePredicated;
	U8 padding4[2];

	ListTransitionInternal pendingTransitions;

	ListDeviceResourceVersion activeSwapchains;        //Locks swapchain when it's first inserted

} CommandList;

typedef RefPtr CommandListRef;

#define CommandList_ext(ptr, T) (!ptr ? NULL : (T##CommandList*)(ptr + 1))        //impl
#define CommandListRef_ptr(ptr) RefPtr_data(ptr, CommandList)

Bool GraphicsDeviceRef_createCommandList(
	GraphicsDeviceRef *device,
	U64 commandListLen,
	U64 estimatedCommandCount,
	U64 estimatedResources,
	Bool allowResize,
	CommandListRef **commandList,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
