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
#include "graphics/generic/command_list.h"
#include "graphics/generic/commands.h"
#include "graphics/generic/render_texture.h"
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

