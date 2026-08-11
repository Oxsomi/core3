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

//graphics/test/interface/test_graphics_command_list.c

//CommandList lifecycle, recording and scope tests.
//Split out from the interface test because recording is by far the largest untested surface in the module and
// this file is expected to keep growing.
//Everything here is pure CPU; commands validate and append, and nothing reaches the GPU until submit.

#include "types/container/string.h"
#include "types/base/string_base.h"
#include "types/base/constants.h"
#include "types/math/vec2i.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/commands.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/render_texture.h"
#include "graphics/generic/depth_stencil.h"
#include "graphics/generic/texture.h"
#include "test_graphics_shared.h"

// -- 14. CommandList -------------------------------------------------------------

//Recording is what the functional tests cover; this is lifecycle and parameter validation only.

void Test_graphicsCommandList(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "CommandList");

	CommandListRef *commandList = NULL;

	Test_assert(t, "createNullDevice", !GraphicsDeviceRef_createCommandList(
		NULL, 2 * KIBI, 128, 64, true, &commandList, NULL
	));

	Test_assert(t, "createNullOut", !GraphicsDeviceRef_createCommandList(deviceRef, 2 * KIBI, 128, 64, true, NULL, NULL));
	Test_assert(t, "rejectedNothing", !commandList);

	if(!Test_assert(t, "create", GraphicsDeviceRef_createCommandList(
		deviceRef, 2 * KIBI, 128, 64, true, &commandList, &t->err
	)))
		return;

	Test_assert(t, "typeId", commandList->refPtrType->typeId == (TypeId) EGraphicsTypeId_CommandList);

	const CommandList *listPtr = CommandListRef_ptr(commandList);

	Test_assert(t, "device", listPtr->device == deviceRef);
	Test_assert(t, "size", Buffer_length(listPtr->data) == 2 * KIBI);
	Test_assert(t, "allowResize", listPtr->allowResize);
	Test_assert(t, "stateNew", listPtr->state == ECommandListState_New);
	Test_assert(t, "noCommands", !listPtr->commandOps.length);
	Test_assert(t, "noResources", !listPtr->resources.length);

	RefPtr_dec(&commandList);
	Test_assert(t, "freeNulled", !commandList);
}

// -- 20. Command recording and scopes --------------------------------------------

//Recording is pure CPU; every command validates its arguments and appends to the list, and nothing reaches the GPU
// until submit, so all of this runs against any device.
//A scope is only kept if it recorded something that modifies a resource.
//An empty or invalidated scope is rewound and never reaches activeScopes, which is what makes dependencies
// interesting: a conditional dependency on a scope that got dropped has to fail rather than silently reorder.

//A recording error marks the whole list invalid on purpose, since a broken recording can't be trusted, so every
// negative case below gets a list of its own instead of sharing one.

static CommandListRef *Test_beginList(Test *t, GraphicsDeviceRef *deviceRef, const C8 *name) {

	CommandListRef *commandList = NULL;

	if(!Test_assert(t, name, GraphicsDeviceRef_createCommandList(
		deviceRef, 4 * KIBI, 64, 16, true, &commandList, &t->err
	)))
		return NULL;

	if(!Test_assert(t, name, CommandListRef_begin(commandList, true, U64_MAX, &t->err)))
		RefPtr_dec(&commandList);

	return commandList;
}

void Test_graphicsCommandRecording(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "CommandList/record");

	CommandListRef *commandList = NULL;
	RenderTextureRef *target = NULL;

	const CharString targetName = CharString_createRefCStrConst("Scope test target");
	const ImageRange all = (ImageRange) { .levelId = U32_MAX, .layerId = U32_MAX };

	//length is the depth, so it's 1 rather than 0 for a 2D target

	if(!Test_assert(t, "createTarget", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 16, 16, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &targetName, &target, &t->err
	)))
		return;

	//Nothing may be recorded before begin, since the list isn't locked by this thread yet

	if((commandList = Test_beginList(t, deviceRef, "createUnopened"))) {

		CommandListRef_end(commandList, NULL);        //Closed again, so the recording calls below have no lock

		Test_assert(t, "scopeBeforeBegin", !CommandListRef_startScope(commandList, NULL, 0, NULL, NULL));
		Test_assert(t, "clearBeforeBegin", !CommandListRef_clearImagef(commandList, F32x4_zero(), all, target, NULL));

		RefPtr_dec(&commandList);
	}

	//An empty scope modifies nothing so it's rewound, a scope that clears an image is committed under its id,
	// and end() collects everything the recording touched so the list keeps it alive

	if((commandList = Test_beginList(t, deviceRef, "createHappy"))) {

		const CommandList *ptr = CommandListRef_ptr(commandList);

		Test_assert(t, "startEmpty", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
		Test_assert(t, "endEmpty", CommandListRef_endScope(commandList, &t->err));
		Test_assert(t, "emptyHidden", !ptr->activeScopes.length);
		Test_assert(t, "emptyRewound", !ptr->commandOps.length);

		Test_assert(t, "startClear", CommandListRef_startScope(commandList, NULL, 2, NULL, &t->err));
		Test_assert(t, "clear", CommandListRef_clearImagef(commandList, F32x4_zero(), all, target, &t->err));

		//Transitions are gathered during the scope and only merged into the list when it closes

		Test_assert(t, "transitionPending", ptr->pendingTransitions.length == 1);
		Test_assert(t, "endClear", CommandListRef_endScope(commandList, &t->err));
		Test_assert(t, "clearCommitted", ptr->activeScopes.length == 1);
		Test_assert(t, "clearScopeId", ptr->activeScopes.length && ptr->activeScopes.ptr[0].scopeId == 2);
		Test_assert(t, "transitionKept", ptr->transitions.length == 1);

		Test_assert(t, "end", CommandListRef_end(commandList, &t->err));
		Test_assert(t, "resourceTracked", ptr->resources.length == 1);
		Test_assert(t, "resourceIsTarget", ptr->resources.length && ptr->resources.ptr[0] == target);

		RefPtr_dec(&commandList);
	}

	//A command outside a scope has nothing to be ordered against

	if((commandList = Test_beginList(t, deviceRef, "createOutside"))) {

		Test_assert(t, "clearOutsideScope", !CommandListRef_clearImagef(commandList, F32x4_zero(), all, target, NULL));
		RefPtr_dec(&commandList);
	}

	//Closing a scope that was never opened invalidates the list rather than being ignored

	if((commandList = Test_beginList(t, deviceRef, "createUnmatched"))) {

		Test_assert(t, "endScopeWithoutScope", !CommandListRef_endScope(commandList, NULL));
		Test_assert(t, "invalidAfterUnmatched", CommandListRef_ptr(commandList)->state == ECommandListState_Invalid);
		RefPtr_dec(&commandList);
	}

	//Scopes don't nest, and a failed startScope leaves no half open scope behind

	if((commandList = Test_beginList(t, deviceRef, "createNested"))) {

		Test_assert(t, "startOuter", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
		Test_assert(t, "nestedScope", !CommandListRef_startScope(commandList, NULL, 2, NULL, NULL));
		Test_assert(t, "invalidAfterNested",
			CommandListRef_ptr(commandList)->tempStateFlags == ECommandStateFlags_InvalidState
		);
		Test_assert(t, "endRefusesInvalid", !CommandListRef_end(commandList, NULL));
		RefPtr_dec(&commandList);
	}

	//An id may only be used once, and reuse has to be caught while recording rather than at submit

	if((commandList = Test_beginList(t, deviceRef, "createDuplicate"))) {

		Test_assert(t, "startFirst", CommandListRef_startScope(commandList, NULL, 2, NULL, &t->err));
		Test_assert(t, "clearFirst", CommandListRef_clearImagef(commandList, F32x4_zero(), all, target, &t->err));
		Test_assert(t, "endFirst", CommandListRef_endScope(commandList, &t->err));
		Test_assert(t, "duplicateId", !CommandListRef_startScope(commandList, NULL, 2, NULL, NULL));
		RefPtr_dec(&commandList);
	}

	//Dependencies are resolved against the scopes that survived, which is what makes a hidden one interesting

	if((commandList = Test_beginList(t, deviceRef, "createDeps"))) {

		const CommandList *ptr = CommandListRef_ptr(commandList);

		const CommandScopeDependency onLive =
			(CommandScopeDependency) { .type = ECommandScopeDependencyType_Conditional, .id = 10 };

		const CommandScopeDependency onHidden =
			(CommandScopeDependency) { .type = ECommandScopeDependencyType_Conditional, .id = 11 };

		const CommandScopeDependency onHiddenLoose =
			(CommandScopeDependency) { .type = ECommandScopeDependencyType_Unconditional, .id = 11 };

		ListCommandScopeDependency live = (ListCommandScopeDependency) { 0 };
		ListCommandScopeDependency hidden = (ListCommandScopeDependency) { 0 };
		ListCommandScopeDependency loose = (ListCommandScopeDependency) { 0 };

		ListCommandScopeDependency_createRefConst(&onLive, 1, &live, NULL);
		ListCommandScopeDependency_createRefConst(&onHidden, 1, &hidden, NULL);
		ListCommandScopeDependency_createRefConst(&onHiddenLoose, 1, &loose, NULL);

		//10 survives because it clears, 11 is dropped because it does nothing

		Test_assert(t, "startLive", CommandListRef_startScope(commandList, NULL, 10, NULL, &t->err));
		Test_assert(t, "clearLive", CommandListRef_clearImagef(commandList, F32x4_zero(), all, target, &t->err));
		Test_assert(t, "endLive", CommandListRef_endScope(commandList, &t->err));

		Test_assert(t, "startDropped", CommandListRef_startScope(commandList, NULL, 11, NULL, &t->err));
		Test_assert(t, "endDropped", CommandListRef_endScope(commandList, &t->err));
		Test_assert(t, "onlyLiveKept", ptr->activeScopes.length == 1);

		//Unconditional is a hint, so a dropped scope is simply nothing to wait on

		Test_assert(t, "dependsLoose", CommandListRef_startScope(commandList, NULL, 12, &loose, &t->err));
		Test_assert(t, "clearLoose", CommandListRef_clearImagef(commandList, F32x4_zero(), all, target, &t->err));
		Test_assert(t, "endLoose", CommandListRef_endScope(commandList, &t->err));

		//Conditional is a promise about ordering, so a scope that survived is a valid thing to depend on

		Test_assert(t, "dependsLive", CommandListRef_startScope(commandList, NULL, 13, &live, &t->err));
		Test_assert(t, "clearDep", CommandListRef_clearImagef(commandList, F32x4_zero(), all, target, &t->err));
		Test_assert(t, "endDep", CommandListRef_endScope(commandList, &t->err));
		Test_assert(t, "depCommitted", ptr->activeScopes.length == 3);

		//...and one that got dropped is not, since there's nothing left to order against

		Test_assert(t, "dependsDropped", !CommandListRef_startScope(commandList, NULL, 14, &hidden, NULL));
		Test_assert(t, "invalidAfterDep", ptr->tempStateFlags == ECommandStateFlags_InvalidState);

		RefPtr_dec(&commandList);
	}

	RefPtr_dec(&target);
}

// -- 21. Command argument validation ---------------------------------------------

//What each command refuses, and how the list is left afterwards.
//A failing command only raises InvalidState, which endScope consumes by hiding the scope and clearing the flags,
// so one list can host many negative cases as long as it doesn't need a scope to survive in between.
//Only endScope and end mark the whole list invalid, which is why those get lists of their own.

void Test_graphicsCommandValidation(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "CommandList/commands");

	CommandListRef *commandList = NULL;
	RenderTextureRef *target = NULL;
	DeviceBufferRef *buffer = NULL;

	const CharString targetName = CharString_createRefCStrConst("Validation target");
	const CharString bufferName = CharString_createRefCStrConst("Validation buffer");
	const CharString regionName = CharString_createRefCStrConst("Region");

	const ImageRange all = (ImageRange) { .levelId = U32_MAX, .layerId = U32_MAX };

	if(!Test_assert(t, "createTarget", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 16, 16, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &targetName, &target, &t->err
	)))
		return;

	Test_assert(t, "createBuffer", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_Vertex, EGraphicsResourceFlag_None, NULL, &bufferName, 256, &buffer, &t->err
	));

	//Anything that draws or sets viewport state needs a render pass, and dispatch needs a bound pipeline.
	//None of those can be satisfied here, so this is the shape of the refusal that matters.

	if((commandList = Test_beginList(t, deviceRef, "createNoRender"))) {

		Test_assert(t, "scope", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));

		Test_assert(t, "viewportNeedsRender",
			!CommandListRef_setViewport(commandList, I32x2_zero, I32x2_create2(8, 8), NULL)
		);

		Test_assert(t, "scissorNeedsRender",
			!CommandListRef_setScissor(commandList, I32x2_zero, I32x2_create2(8, 8), NULL)
		);

		Test_assert(t, "viewportAndScissorNeedsRender",
			!CommandListRef_setViewportAndScissor(commandList, I32x2_zero, I32x2_create2(8, 8), NULL)
		);

		Test_assert(t, "drawNeedsRender", !CommandListRef_drawUnindexed(commandList, 3, 1, NULL));
		Test_assert(t, "drawIndexedNeedsRender", !CommandListRef_drawIndexed(commandList, 3, 1, NULL));
		Test_assert(t, "dispatchNeedsPipeline", !CommandListRef_dispatch1D(commandList, 1, NULL));

		//Stencil and blend constants are pure state, so they record without a render pass...

		Test_assert(t, "setStencil", CommandListRef_setStencil(commandList, 0x7F, &t->err));
		Test_assert(t, "setBlendConstants", CommandListRef_setBlendConstants(commandList, F32x4_zero(), &t->err));

		//...but state alone doesn't modify a resource, so the scope is still dropped

		Test_assert(t, "endScope", CommandListRef_endScope(commandList, &t->err));
		Test_assert(t, "stateOnlyHidden", !CommandListRef_ptr(commandList)->activeScopes.length);

		//endScope consumed the invalid state, so the list is usable again

		Test_assert(t, "flagsCleared", !CommandListRef_ptr(commandList)->tempStateFlags);
		Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

		RefPtr_dec(&commandList);
	}

	//Clear rejects an empty batch, a resource that isn't a texture at all, and a range past what exists

	if((commandList = Test_beginList(t, deviceRef, "createClear"))) {

		const ListClearImageCmd empty = (ListClearImageCmd) { 0 };

		Test_assert(t, "scope2", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
		Test_assert(t, "clearEmptyBatch", !CommandListRef_clearImages(commandList, empty, NULL));
		Test_assert(t, "clearNullImage", !CommandListRef_clearImagef(commandList, F32x4_zero(), all, NULL, NULL));

		//A buffer isn't a texture, so it resolves to no unified texture rather than being misread as one

		Test_assert(t, "clearBufferAsImage",
			!buffer || !CommandListRef_clearImagef(commandList, F32x4_zero(), all, buffer, NULL)
		);

		const ImageRange badLevel = (ImageRange) { .levelId = 1, .layerId = U32_MAX };
		const ImageRange badLayer = (ImageRange) { .levelId = U32_MAX, .layerId = 1 };

		Test_assert(t, "clearBadLevel", !CommandListRef_clearImagef(commandList, F32x4_zero(), badLevel, target, NULL));
		Test_assert(t, "clearBadLayer", !CommandListRef_clearImagef(commandList, F32x4_zero(), badLayer, target, NULL));

		RefPtr_dec(&commandList);
	}

	//Debug regions nest and have to be balanced within the scope that opened them

	if((commandList = Test_beginList(t, deviceRef, "createDebug"))) {

		const CommandList *ptr = CommandListRef_ptr(commandList);

		Test_assert(t, "scope3", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
		Test_assert(t, "endRegionWithoutStart", !CommandListRef_endRegionDebugExt(commandList, NULL));
		Test_assert(t, "startRegionNeedsName", !CommandListRef_startRegionDebugExt(commandList, F32x4_zero(), NULL, NULL));

		Test_assert(t, "startRegion", CommandListRef_startRegionDebugExt(commandList, F32x4_zero(), &regionName, &t->err));
		Test_assert(t, "regionDepth1", ptr->debugRegionStack == 1);

		Test_assert(t, "startNested", CommandListRef_startRegionDebugExt(commandList, F32x4_zero(), &regionName, &t->err));
		Test_assert(t, "regionDepth2", ptr->debugRegionStack == 2);

		Test_assert(t, "marker", CommandListRef_addMarkerDebugExt(commandList, F32x4_zero(), &regionName, &t->err));

		Test_assert(t, "endNested", CommandListRef_endRegionDebugExt(commandList, &t->err));
		Test_assert(t, "endRegion", CommandListRef_endRegionDebugExt(commandList, &t->err));
		Test_assert(t, "regionDepth0", !ptr->debugRegionStack);

		RefPtr_dec(&commandList);
	}

	//A scope that modifies something and leaves a region open can't close, since the region would outlive it.
	//Without the clear the scope would be hidden before the region is ever checked, so the modify op is load bearing.

	if((commandList = Test_beginList(t, deviceRef, "createUnbalanced"))) {

		Test_assert(t, "scope4", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
		Test_assert(t, "clear4", CommandListRef_clearImagef(commandList, F32x4_zero(), all, target, &t->err));
		Test_assert(t, "openRegion", CommandListRef_startRegionDebugExt(commandList, F32x4_zero(), &regionName, &t->err));
		Test_assert(t, "endScopeOpenRegion", !CommandListRef_endScope(commandList, NULL));
		Test_assert(t, "invalidAfterRegion", CommandListRef_ptr(commandList)->state == ECommandListState_Invalid);

		RefPtr_dec(&commandList);
	}

	//allowResize decides whether the command buffer grows or the recording is refused once it's full.
	//setStencil writes exactly 16 bytes, so a 64 byte buffer takes four of them and refuses the fifth.

	commandList = NULL;

	if(Test_assert(t, "createFixed", GraphicsDeviceRef_createCommandList(
		deviceRef, 64, 8, 4, false, &commandList, &t->err
	))) {

		Test_assert(t, "beginFixed", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
		Test_assert(t, "scope5", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));

		Bool filled = true;

		for(U64 i = 0; i < 4; ++i)
			filled = filled && CommandListRef_setStencil(commandList, (U8) i, NULL);

		Test_assert(t, "fixedFits", filled);
		Test_assert(t, "fixedFull", CommandListRef_ptr(commandList)->next == 64);
		Test_assert(t, "fixedOverflows", !CommandListRef_setStencil(commandList, 4, NULL));
		Test_assert(t, "fixedNotGrown", Buffer_length(CommandListRef_ptr(commandList)->data) == 64);

		RefPtr_dec(&commandList);
	}

	commandList = NULL;

	if(Test_assert(t, "createResizable", GraphicsDeviceRef_createCommandList(
		deviceRef, 64, 8, 4, true, &commandList, &t->err
	))) {

		Test_assert(t, "beginResizable", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
		Test_assert(t, "scope6", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));

		Bool grew = true;

		for(U64 i = 0; i < 8; ++i)
			grew = grew && CommandListRef_setStencil(commandList, (U8) i, NULL);

		Test_assert(t, "resizableAccepts", grew);
		Test_assert(t, "resizableGrew", Buffer_length(CommandListRef_ptr(commandList)->data) > 64);
		Test_assert(t, "resizableKeptAll", CommandListRef_ptr(commandList)->next == 8 * 16);

		RefPtr_dec(&commandList);
	}

	//Copies are validated against what the images are, not just that they're images

	RenderTextureRef *target2 = NULL;
	RenderTextureRef *fmtOther = NULL;
	DepthStencilRef *depth = NULL;
	DeviceBufferRef *indirect = NULL;

	const CharString target2Name = CharString_createRefCStrConst("Validation copy target");
	const CharString fmtOtherName = CharString_createRefCStrConst("Validation format target");
	const CharString depthName = CharString_createRefCStrConst("Validation depth");
	const CharString indirectName = CharString_createRefCStrConst("Validation indirect buffer");

	Test_assert(t, "createTarget2", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 16, 16, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &target2Name, &target2, &t->err
	));

	Test_assert(t, "createFmtOther", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 16, 16, 1, ETextureFormatId_RG16f, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &fmtOtherName, &fmtOther, &t->err
	));

	Test_assert(t, "createDepthCopy", GraphicsDeviceRef_createDepthStencil(
		deviceRef, 16, 16, EDepthStencilFormat_D32, false, EMSAASamples_Off, NULL, &depthName, &depth, &t->err
	));

	Test_assert(t, "createIndirect", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_Indirect, EGraphicsResourceFlag_None, NULL, &indirectName, 256, &indirect, &t->err
	));

	if(target2 && fmtOther && depth && (commandList = Test_beginList(t, deviceRef, "createCopy"))) {

		const ListCopyImageRegion noRegions = (ListCopyImageRegion) { 0 };
		const CopyImageRegion whole = (CopyImageRegion) { 0 };

		//Negatives share a scope, since a failing command invalidates the scope but never the list

		Test_assert(t, "scopeCopy", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));

		Test_assert(t, "copyNoRegions", !CommandListRef_copyImageRegions(commandList, target, target2, noRegions, NULL));
		Test_assert(t, "copyNullSrc", !CommandListRef_copyImage(commandList, NULL, target2, whole, NULL));
		Test_assert(t, "copyBufferAsSrc", !CommandListRef_copyImage(commandList, buffer, target2, whole, NULL));
		Test_assert(t, "copyDepthColorMix", !CommandListRef_copyImage(commandList, depth, target2, whole, NULL));
		Test_assert(t, "copyBothDepth", !CommandListRef_copyImage(commandList, depth, depth, whole, NULL));
		Test_assert(t, "copyFormatMismatch", !CommandListRef_copyImage(commandList, target, fmtOther, whole, NULL));

		const CopyImageRegion badLevel = (CopyImageRegion) { .srcLevelId = 1 };

		Test_assert(t, "copyBadLevel", !CommandListRef_copyImage(commandList, target, target2, badLevel, NULL));

		Test_assert(t, "endScopeCopyNeg", CommandListRef_endScope(commandList, &t->err));
		Test_assert(t, "copyNegHidden", !CommandListRef_ptr(commandList)->activeScopes.length);

		//A valid copy is a modify op, so its scope is the one that commits

		Test_assert(t, "scopeCopyPos", CommandListRef_startScope(commandList, NULL, 2, NULL, &t->err));
		Test_assert(t, "copyValid", CommandListRef_copyImage(commandList, target, target2, whole, &t->err));
		Test_assert(t, "endScopeCopyPos", CommandListRef_endScope(commandList, &t->err));
		Test_assert(t, "copyCommitted", CommandListRef_ptr(commandList)->activeScopes.length == 1);

		RefPtr_dec(&commandList);
	}

	//Indirect draws validate their argument buffer before any render state, so the buffer rules are reachable
	// without a pipeline; dispatchIndirect checks its pipeline first, so only that refusal is visible for it

	if(indirect && (commandList = Test_beginList(t, deviceRef, "createIndirectList"))) {

		Test_assert(t, "scopeIndirect", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));

		Test_assert(t, "indirectNullBuffer", !CommandListRef_drawIndirect(commandList, NULL, 0, 1, false, NULL));
		Test_assert(t, "indirectMisaligned", !CommandListRef_drawIndirect(commandList, indirect, 8, 1, false, NULL));
		Test_assert(t, "indirectZeroCalls", !CommandListRef_drawIndirect(commandList, indirect, 0, 0, false, NULL));
		Test_assert(t, "indirectWrongUsage", !CommandListRef_drawIndirect(commandList, buffer, 0, 1, false, NULL));
		Test_assert(t, "indirectOOB", !CommandListRef_drawIndirect(commandList, indirect, 256, 1, false, NULL));
		Test_assert(t, "dispatchIndirectNeedsPipeline", !CommandListRef_dispatchIndirect(commandList, indirect, 0, NULL));

		RefPtr_dec(&commandList);
	}

	RefPtr_dec(&indirect);
	RefPtr_dec(&depth);
	RefPtr_dec(&fmtOther);
	RefPtr_dec(&target2);
	RefPtr_dec(&buffer);
	RefPtr_dec(&target);
}

// -- 22. Render passes -----------------------------------------------------------

//startRenderExt is what gives a scope a render area, and viewport, scissor and every draw are gated on it.
//That makes this the only place those can be exercised positively, and the place their gating can be shown to
// actually lift and drop again.
//Attachments take a concrete subresource rather than a whole image, so unlike clear, U32_MAX is out of range here.

void Test_graphicsRenderPass(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "CommandList/render");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering)) {
		Test_print(t, "Device lacks direct rendering, skipping render pass tests");
		return;
	}

	CommandListRef *commandList = NULL;
	RenderTextureRef *target = NULL;
	RenderTextureRef *smaller = NULL;
	DepthStencilRef *depth = NULL;

	const CharString targetName = CharString_createRefCStrConst("Render pass target");
	const CharString smallerName = CharString_createRefCStrConst("Render pass smaller target");
	const CharString depthName = CharString_createRefCStrConst("Render pass depth");

	Test_assert(t, "createTarget", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 16, 16, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &targetName, &target, &t->err
	));

	Test_assert(t, "createSmaller", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 8, 8, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &smallerName, &smaller, &t->err
	));

	Test_assert(t, "createDepth", GraphicsDeviceRef_createDepthStencil(
		deviceRef, 16, 16, EDepthStencilFormat_D32, false, EMSAASamples_Off, NULL, &depthName, &depth, &t->err
	));

	if(!target || !smaller || !depth) {
		RefPtr_dec(&depth);
		RefPtr_dec(&smaller);
		RefPtr_dec(&target);
		return;
	}

	if(!(commandList = Test_beginList(t, deviceRef, "createRender"))) {
		RefPtr_dec(&depth);
		RefPtr_dec(&smaller);
		RefPtr_dec(&target);
		return;
	}

	const CommandList *ptr = CommandListRef_ptr(commandList);

	//A failing command only raises InvalidState, so every rejection below can share one scope

	Test_assert(t, "scope", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));

	const ListAttachmentInfo noColors = (ListAttachmentInfo) { 0 };

	Test_assert(t, "renderNeedsTargets",
		!CommandListRef_startRenderExt(commandList, I32x2_zero, I32x2_zero, &noColors, NULL, NULL)
	);

	AttachmentInfo color = (AttachmentInfo) { .image = target, .load = ELoadAttachmentType_Clear };
	ListAttachmentInfo colors = (ListAttachmentInfo) { 0 };
	ListAttachmentInfo_createRefConst(&color, 1, &colors, NULL);

	//A whole image range is what clear takes; an attachment has to name one subresource

	AttachmentInfo wholeRange = color;
	wholeRange.range = (ImageRange) { .levelId = U32_MAX, .layerId = U32_MAX };
	ListAttachmentInfo wholeRangeList = (ListAttachmentInfo) { 0 };
	ListAttachmentInfo_createRefConst(&wholeRange, 1, &wholeRangeList, NULL);

	Test_assert(t, "renderRejectsWholeRange",
		!CommandListRef_startRenderExt(commandList, I32x2_zero, I32x2_zero, &wholeRangeList, NULL, NULL)
	);

	//Clearing an attachment that is declared read only contradicts itself

	AttachmentInfo readOnlyClear = color;
	readOnlyClear.readOnly = true;
	ListAttachmentInfo readOnlyList = (ListAttachmentInfo) { 0 };
	ListAttachmentInfo_createRefConst(&readOnlyClear, 1, &readOnlyList, NULL);

	Test_assert(t, "renderRejectsReadOnlyClear",
		!CommandListRef_startRenderExt(commandList, I32x2_zero, I32x2_zero, &readOnlyList, NULL, NULL)
	);

	//A depth image is not a colour attachment, and the mismatch has to be caught rather than reinterpreted

	AttachmentInfo depthAsColor = (AttachmentInfo) { .image = depth };
	ListAttachmentInfo depthAsColorList = (ListAttachmentInfo) { 0 };
	ListAttachmentInfo_createRefConst(&depthAsColor, 1, &depthAsColorList, NULL);

	Test_assert(t, "renderRejectsDepthAsColor",
		!CommandListRef_startRenderExt(commandList, I32x2_zero, I32x2_zero, &depthAsColorList, NULL, NULL)
	);

	//Resolving needs somewhere to resolve to

	AttachmentInfo resolveNoImage = color;
	resolveNoImage.resolveMode = 1;
	ListAttachmentInfo resolveList = (ListAttachmentInfo) { 0 };
	ListAttachmentInfo_createRefConst(&resolveNoImage, 1, &resolveList, NULL);

	Test_assert(t, "renderRejectsResolveWithoutImage",
		!CommandListRef_startRenderExt(commandList, I32x2_zero, I32x2_zero, &resolveList, NULL, NULL)
	);

	//Attachments share one render area, so a second target smaller than the first can't satisfy it

	AttachmentInfo pair[2] = {
		(AttachmentInfo) { .image = target },
		(AttachmentInfo) { .image = smaller }
	};

	ListAttachmentInfo pairList = (ListAttachmentInfo) { 0 };
	ListAttachmentInfo_createRefConst(pair, 2, &pairList, NULL);

	Test_assert(t, "renderRejectsMismatchedSizes",
		!CommandListRef_startRenderExt(commandList, I32x2_zero, I32x2_zero, &pairList, NULL, NULL)
	);

	//An offset outside the first attachment leaves no area to render into

	Test_assert(t, "renderRejectsOffsetOutside",
		!CommandListRef_startRenderExt(commandList, I32x2_create2(16, 16), I32x2_zero, &colors, NULL, NULL)
	);

	//None of that opened a render pass, so the state that depends on one is still refused

	Test_assert(t, "noRenderAfterFailures", I32x2_eq2(ptr->currentSize, I32x2_zero));
	Test_assert(t, "endRenderWithoutStart", !CommandListRef_endRenderExt(commandList, NULL));

	Test_assert(t, "endScopeNegatives", CommandListRef_endScope(commandList, &t->err));

	//A valid render pass sets the area, which is what lifts the gate on viewport, scissor and draws

	Test_assert(t, "scope2", CommandListRef_startScope(commandList, NULL, 2, NULL, &t->err));

	Test_assert(t, "startRender",
		CommandListRef_startRenderExt(commandList, I32x2_zero, I32x2_zero, &colors, NULL, &t->err)
	);

	Test_assert(t, "renderAreaFromTarget", I32x2_eq2(ptr->currentSize, I32x2_create2(16, 16)));

	//Zero size means the whole remaining area rather than an empty one

	Test_assert(t, "viewportFull", CommandListRef_setViewport(commandList, I32x2_zero, I32x2_zero, &t->err));
	Test_assert(t, "scissorPartial", CommandListRef_setScissor(commandList, I32x2_zero, I32x2_create2(8, 8), &t->err));

	Test_assert(t, "viewportAndScissor",
		CommandListRef_setViewportAndScissor(commandList, I32x2_create2(2, 2), I32x2_create2(4, 4), &t->err)
	);

	Test_assert(t, "viewportFlagsSet",
		(ptr->tempStateFlags & (ECommandStateFlags_AnyViewport | ECommandStateFlags_AnyScissor)) ==
		(ECommandStateFlags_AnyViewport | ECommandStateFlags_AnyScissor)
	);

	//An offset at or past the render area has nothing left to cover

	Test_assert(t, "viewportOffsetOutside",
		!CommandListRef_setViewport(commandList, I32x2_create2(16, 16), I32x2_create2(1, 1), NULL)
	);

	//A draw still needs a pipeline even once the render pass and viewport state are in place

	Test_assert(t, "drawStillNeedsPipeline", !CommandListRef_drawUnindexed(commandList, 3, 1, NULL));

	//Ending the pass takes the area away again, and with it the state that depended on it

	Test_assert(t, "endRender", CommandListRef_endRenderExt(commandList, &t->err));
	Test_assert(t, "renderAreaCleared", I32x2_eq2(ptr->currentSize, I32x2_zero));

	Test_assert(t, "viewportFlagsCleared",
		!(ptr->tempStateFlags & (ECommandStateFlags_AnyViewport | ECommandStateFlags_AnyScissor))
	);

	Test_assert(t, "viewportAfterEndRender",
		!CommandListRef_setViewport(commandList, I32x2_zero, I32x2_create2(8, 8), NULL)
	);

	Test_assert(t, "endRenderTwice", !CommandListRef_endRenderExt(commandList, NULL));

	RefPtr_dec(&commandList);
	RefPtr_dec(&depth);
	RefPtr_dec(&smaller);
	RefPtr_dec(&target);
}
