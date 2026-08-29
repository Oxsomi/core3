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

//graphics/test/interface/test_graphics_command_list.cpp
//
//CommandList lifecycle, recording and scope tests, written against the C++ layer (graphics/graphics.hpp).
//Split out from the interface test because recording is by far the largest untested surface in the module and
// this file is expected to keep growing.
//Everything here is pure CPU; commands validate and append, and nothing reaches the GPU until submit.
//
//Same coverage as the C module it replaces, minus the teardown: every handle releases itself, so there is no
// clean label and no RefPtr_dec ladder to keep in step with the locals.
//This module is mostly NEGATIVE tests and the wrapper deliberately cannot express some of them: a command
// outside a scope, a draw outside a render pass, an endScope with no scope open.
//Those go through the documented escape hatch (scope.raw(), or the list handle) rather than being softened into
// something the wrapper can say, because the refusal is the test.

#include "graphics/graphics.hpp"

namespace oxc { namespace c {
	#include "test_graphics_shared.h"
}}

namespace {

	//A recording error marks the whole list invalid on purpose, since a broken recording can't be trusted, so
	//every negative case below gets a list of its own instead of sharing one.
	//An empty CommandList comes back when either step failed, which the caller tests to skip that case.

	oxc::gfx::CommandList beginList(oxc::c::Test *t, oxc::gfx::Device &dev, const oxc::c::C8 *label) {

		using namespace oxc;
		using namespace oxc::gfx;

		CommandList commandList;

		if(!Test_assert(t, label, dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, &t->err)))
			return CommandList();

		if(!Test_assert(t, label, commandList.begin(true, &t->err)))
			return CommandList();

		return commandList;
	}
}

// -- 14. CommandList -------------------------------------------------------------

//Recording is what the functional tests cover; this is lifecycle and parameter validation only.

extern "C" void Test_graphicsCommandList(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Error *e_rr = &t->err;

	Test_setModule(t, "CommandList");

	//The harness owns this ref, so it is borrowed rather than adopted.

	Device dev = Device::share(deviceRef);

	//A null device and a null out parameter have no wrapper spelling, which is the point: they are what the C
	// factory has to refuse before it allocates anything.

	c::CommandListRef *raw = nullptr;

	Test_assert(t, "createNullDevice", !c::GraphicsDeviceRef_createCommandList(
		nullptr, 2 * c::KIBI, 128, 64, true, &raw, nullptr
	));

	Test_assert(t, "createNullOut", !c::GraphicsDeviceRef_createCommandList(
		deviceRef, 2 * c::KIBI, 128, 64, true, nullptr, nullptr
	));

	Test_assert(t, "rejectedNothing", !raw);

	CommandList commandList;

	if(!Test_assert(t, "create", dev.createCommandList(2 * c::KIBI, 128, 64, commandList, true, e_rr)))
		return;

	Test_assert(t, "typeId", commandList.handle()->refPtrType->typeId == (c::TypeId) c::EGraphicsTypeId_CommandList);

	const c::CommandList *listPtr = commandList.data();

	Test_assert(t, "device", listPtr->device == deviceRef);
	Test_assert(t, "size", c::Buffer_length(listPtr->data) == 2 * c::KIBI);
	Test_assert(t, "allowResize", listPtr->allowResize);
	Test_assert(t, "stateNew", listPtr->state == c::ECommandListState_New);
	Test_assert(t, "noCommands", !listPtr->commandOps.length);
	Test_assert(t, "noResources", !listPtr->resources.length);

	//release() is the wrapper's RefPtr_dec: it decs and empties the handle in one step.

	commandList.release();
	Test_assert(t, "freeNulled", !commandList.valid());
}

// -- 20. Command recording and scopes --------------------------------------------

//Recording is pure CPU; every command validates its arguments and appends to the list, and nothing reaches the GPU
// until submit, so all of this runs against any device.
//A scope is only kept if it recorded something that modifies a resource.
//An empty or invalidated scope is rewound and never reaches activeScopes, which is what makes dependencies
// interesting: a conditional dependency on a scope that got dropped has to fail rather than silently reorder.

extern "C" void Test_graphicsCommandRecording(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Error *e_rr = &t->err;

	Test_setModule(t, "CommandList/record");

	Device dev = Device::share(deviceRef);

	const c::ImageRange all{ c::U32_MAX, c::U32_MAX };

	//Device::createRenderTexture pins the shape the C call spelled out: 2D, depth 1 rather than 0, no MSAA.

	RenderTexture target;

	if(!Test_assert(t, "createTarget", dev.createRenderTexture(
		16, 16, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Scope test target", target, c::EMSAASamples_Off,
		nullptr, e_rr
	)))
		return;

	//Nothing may be recorded before begin, since the list isn't locked by this thread yet

	if(CommandList commandList = beginList(t, dev, "createUnopened")) {

		(void) commandList.end();        //Closed again, so the recording calls below have no lock

		CommandScope scope = commandList.scope({}, 0);
		Test_assert(t, "scopeBeforeBegin", !scope);

		//Recording only exists on CommandScope, and there is no scope here, so the clear goes at the list raw.

		Test_assert(t, "clearBeforeBegin",
			!c::CommandListRef_clearImagef(commandList.handle(), c::F32x4_zero(), all, target.handle(), nullptr)
		);
	}

	//An empty scope modifies nothing so it's rewound, a scope that clears an image is committed under its id,
	// and end() collects everything the recording touched so the list keeps it alive

	if(CommandList commandList = beginList(t, dev, "createHappy")) {

		const c::CommandList *ptr = commandList.data();

		CommandScope empty = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "startEmpty", (c::Bool) empty);
		Test_assert(t, "endEmpty", empty.end(e_rr));
		Test_assert(t, "emptyHidden", !ptr->activeScopes.length);
		Test_assert(t, "emptyRewound", !ptr->commandOps.length);

		CommandScope scope = commandList.scope({}, 2, {}, e_rr);
		Test_assert(t, "startClear", (c::Bool) scope);
		Test_assert(t, "clear", scope.clearImagef(c::F32x4_zero(), all, target.handle(), e_rr));

		//Transitions are gathered during the scope and only merged into the list when it closes

		Test_assert(t, "transitionPending", ptr->pendingTransitions.length == 1);
		Test_assert(t, "endClear", scope.end(e_rr));
		Test_assert(t, "clearCommitted", ptr->activeScopes.length == 1);
		Test_assert(t, "clearScopeId", ptr->activeScopes.length && ptr->activeScopes.ptr[0].scopeId == 2);
		Test_assert(t, "transitionKept", ptr->transitions.length == 1);

		Test_assert(t, "end", commandList.end(e_rr));
		Test_assert(t, "resourceTracked", ptr->resources.length == 1);
		Test_assert(t, "resourceIsTarget", ptr->resources.length && ptr->resources.ptr[0] == target.handle());
	}

	//A command outside a scope has nothing to be ordered against

	if(CommandList commandList = beginList(t, dev, "createOutside")) {

		Test_assert(t, "clearOutsideScope",
			!c::CommandListRef_clearImagef(commandList.handle(), c::F32x4_zero(), all, target.handle(), nullptr)
		);
	}

	//Closing a scope that was never opened invalidates the list rather than being ignored.
	//There is no CommandScope to end, which is exactly what's under test, so endScope goes in raw.

	if(CommandList commandList = beginList(t, dev, "createUnmatched")) {

		Test_assert(t, "endScopeWithoutScope", !c::CommandListRef_endScope(commandList.handle(), nullptr));
		Test_assert(t, "invalidAfterUnmatched", commandList.data()->state == c::ECommandListState_Invalid);
	}

	//Scopes don't nest, and a failed startScope leaves no half open scope behind.
	//outer stays open, like the C module which dropped the list with the scope open; the only difference is the
	// one endScope ~CommandScope runs afterwards, which the invalidated recording rewinds after the last assert.

	if(CommandList commandList = beginList(t, dev, "createNested")) {

		CommandScope outer = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "startOuter", (c::Bool) outer);

		CommandScope nested = commandList.scope({}, 2);
		Test_assert(t, "nestedScope", !nested);

		Test_assert(t, "invalidAfterNested", commandList.data()->tempStateFlags == c::ECommandStateFlags_InvalidState);

		Test_assert(t, "endRefusesInvalid", !commandList.end());
	}

	//An id may only be used once, and reuse has to be caught while recording rather than at submit

	if(CommandList commandList = beginList(t, dev, "createDuplicate")) {

		CommandScope scope = commandList.scope({}, 2, {}, e_rr);
		Test_assert(t, "startFirst", (c::Bool) scope);
		Test_assert(t, "clearFirst", scope.clearImagef(c::F32x4_zero(), all, target.handle(), e_rr));
		Test_assert(t, "endFirst", scope.end(e_rr));

		CommandScope duplicate = commandList.scope({}, 2);
		Test_assert(t, "duplicateId", !duplicate);
	}

	//Dependencies are resolved against the scopes that survived, which is what makes a hidden one interesting

	if(CommandList commandList = beginList(t, dev, "createDeps")) {

		const c::CommandList *ptr = commandList.data();

		const c::CommandScopeDependency onLive{ c::ECommandScopeDependencyType_Conditional, 10 };
		const c::CommandScopeDependency onHidden{ c::ECommandScopeDependencyType_Conditional, 11 };
		const c::CommandScopeDependency onHiddenLoose{ c::ECommandScopeDependencyType_Unconditional, 11 };

		//10 survives because it clears, 11 is dropped because it does nothing

		CommandScope live = commandList.scope({}, 10, {}, e_rr);
		Test_assert(t, "startLive", (c::Bool) live);
		Test_assert(t, "clearLive", live.clearImagef(c::F32x4_zero(), all, target.handle(), e_rr));
		Test_assert(t, "endLive", live.end(e_rr));

		CommandScope dropped = commandList.scope({}, 11, {}, e_rr);
		Test_assert(t, "startDropped", (c::Bool) dropped);
		Test_assert(t, "endDropped", dropped.end(e_rr));
		Test_assert(t, "onlyLiveKept", ptr->activeScopes.length == 1);

		//Unconditional is a hint, so a dropped scope is simply nothing to wait on

		CommandScope loose = commandList.scope({}, 12, { onHiddenLoose }, e_rr);
		Test_assert(t, "dependsLoose", (c::Bool) loose);
		Test_assert(t, "clearLoose", loose.clearImagef(c::F32x4_zero(), all, target.handle(), e_rr));
		Test_assert(t, "endLoose", loose.end(e_rr));

		//Conditional is a promise about ordering, so a scope that survived is a valid thing to depend on

		CommandScope dep = commandList.scope({}, 13, { onLive }, e_rr);
		Test_assert(t, "dependsLive", (c::Bool) dep);
		Test_assert(t, "clearDep", dep.clearImagef(c::F32x4_zero(), all, target.handle(), e_rr));
		Test_assert(t, "endDep", dep.end(e_rr));
		Test_assert(t, "depCommitted", ptr->activeScopes.length == 3);

		//...and one that got dropped is not, since there's nothing left to order against

		CommandScope broken = commandList.scope({}, 14, { onHidden });
		Test_assert(t, "dependsDropped", !broken);
		Test_assert(t, "invalidAfterDep", ptr->tempStateFlags == c::ECommandStateFlags_InvalidState);
	}
}

// -- 21. Command argument validation ---------------------------------------------

//What each command refuses, and how the list is left afterwards.
//A failing command only raises InvalidState, which endScope consumes by hiding the scope and clearing the flags,
// so one list can host many negative cases as long as it doesn't need a scope to survive in between.
//Only endScope and end mark the whole list invalid, which is why those get lists of their own.

extern "C" void Test_graphicsCommandValidation(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Error *e_rr = &t->err;

	Test_setModule(t, "CommandList/commands");

	Device dev = Device::share(deviceRef);

	const c::ImageRange all{ c::U32_MAX, c::U32_MAX };

	RenderTexture target;
	DeviceBuffer buffer;

	if(!Test_assert(t, "createTarget", dev.createRenderTexture(
		16, 16, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Validation target", target, c::EMSAASamples_Off,
		nullptr, e_rr
	)))
		return;

	Test_assert(t, "createBuffer", dev.createBuffer(
		c::EDeviceBufferUsage_Vertex, c::EGraphicsResourceFlag_None, "Validation buffer", 256, buffer, nullptr, e_rr
	));

	//Anything that draws or sets viewport state needs a render pass, and dispatch needs a bound pipeline.
	//None of those can be satisfied here, so this is the shape of the refusal that matters.

	if(CommandList commandList = beginList(t, dev, "createNoRender")) {

		CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "scope", (c::Bool) scope);

		Test_assert(t, "viewportNeedsRender", !scope.setViewport(xy(0, 0), xy(8, 8)));
		Test_assert(t, "scissorNeedsRender", !scope.setScissor(xy(0, 0), xy(8, 8)));
		Test_assert(t, "viewportAndScissorNeedsRender", !scope.setViewportAndScissor(xy(0, 0), xy(8, 8)));

		//Draws live on CommandRender, so a draw without a render pass can only be spelled raw; that type split
		// is the wrapper's compile time version of this very refusal.

		Test_assert(t, "drawNeedsRender", !c::CommandListRef_drawUnindexed(scope.raw(), 3, 1, nullptr));
		Test_assert(t, "drawIndexedNeedsRender", !c::CommandListRef_drawIndexed(scope.raw(), 3, 1, nullptr));
		Test_assert(t, "dispatchNeedsPipeline", !scope.dispatch1D(1));

		//Stencil and blend constants are pure state, so they record without a render pass...

		Test_assert(t, "setStencil", scope.setStencil(0x7F, e_rr));
		Test_assert(t, "setBlendConstants", scope.setBlendConstants(c::F32x4_zero(), e_rr));

		//...but state alone doesn't modify a resource, so the scope is still dropped

		Test_assert(t, "endScope", scope.end(e_rr));
		Test_assert(t, "stateOnlyHidden", !commandList.data()->activeScopes.length);

		//endScope consumed the invalid state, so the list is usable again

		Test_assert(t, "flagsCleared", !commandList.data()->tempStateFlags);
		Test_assert(t, "end", commandList.end(e_rr));
	}

	//Clear rejects an empty batch, a resource that isn't a texture at all, and a range past what exists

	if(CommandList commandList = beginList(t, dev, "createClear")) {

		const c::ImageRange badLevel{ 1, c::U32_MAX };
		const c::ImageRange badLayer{ c::U32_MAX, 1 };

		CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "scope2", (c::Bool) scope);
		Test_assert(t, "clearEmptyBatch", !scope.clearImages({}));
		Test_assert(t, "clearNullImage", !scope.clearImagef(c::F32x4_zero(), all, nullptr));

		//A buffer isn't a texture, so it resolves to no unified texture rather than being misread as one

		Test_assert(t, "clearBufferAsImage", !buffer.valid() || !scope.clearImagef(c::F32x4_zero(), all, buffer.handle()));

		Test_assert(t, "clearBadLevel", !scope.clearImagef(c::F32x4_zero(), badLevel, target.handle()));
		Test_assert(t, "clearBadLayer", !scope.clearImagef(c::F32x4_zero(), badLayer, target.handle()));
	}

	//Debug regions nest and have to be balanced within the scope that opened them.
	//The explicit start/end pair rather than regionDebug(), since the depth between them is the assertion.

	if(CommandList commandList = beginList(t, dev, "createDebug")) {

		const c::CommandList *ptr = commandList.data();

		CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "scope3", (c::Bool) scope);
		Test_assert(t, "endRegionWithoutStart", !scope.endRegionDebug());

		//A null name, not an empty one: startRegionDebug always hands the C side a CharString, so the missing
		// name refusal needs the raw command.

		c::CommandListRef *l = scope.raw();

		Test_assert(t, "startRegionNeedsName", !c::CommandListRef_startRegionDebugExt(l, c::F32x4_zero(), nullptr, nullptr));

		Test_assert(t, "startRegion", scope.startRegionDebug(c::F32x4_zero(), "Region", e_rr));
		Test_assert(t, "regionDepth1", ptr->debugRegionStack == 1);

		Test_assert(t, "startNested", scope.startRegionDebug(c::F32x4_zero(), "Region", e_rr));
		Test_assert(t, "regionDepth2", ptr->debugRegionStack == 2);

		Test_assert(t, "marker", scope.addMarkerDebug(c::F32x4_zero(), "Region", e_rr));

		Test_assert(t, "endNested", scope.endRegionDebug(e_rr));
		Test_assert(t, "endRegion", scope.endRegionDebug(e_rr));
		Test_assert(t, "regionDepth0", !ptr->debugRegionStack);
	}

	//A scope that modifies something and leaves a region open can't close, since the region would outlive it.
	//Without the clear the scope would be hidden before the region is ever checked, so the modify op is load bearing.

	if(CommandList commandList = beginList(t, dev, "createUnbalanced")) {

		CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "scope4", (c::Bool) scope);
		Test_assert(t, "clear4", scope.clearImagef(c::F32x4_zero(), all, target.handle(), e_rr));
		Test_assert(t, "openRegion", scope.startRegionDebug(c::F32x4_zero(), "Region", e_rr));
		Test_assert(t, "endScopeOpenRegion", !scope.end());
		Test_assert(t, "invalidAfterRegion", commandList.data()->state == c::ECommandListState_Invalid);
	}

	//allowResize decides whether the command buffer grows or the recording is refused once it's full.
	//setStencil writes exactly 16 bytes, so a 64 byte buffer takes four of them and refuses the fifth.
	//Device::createCommandList pins allowResize to true, so these two go through the C factory and adopt the
	// reference straight into the wrapper.

	if(c::CommandListRef *raw = nullptr; Test_assert(t, "createFixed", c::GraphicsDeviceRef_createCommandList(
		deviceRef, 64, 8, 4, false, &raw, e_rr
	))) {

		CommandList commandList{ RefPtr<c::CommandList>::adopt(raw) };
		const c::CommandList *ptr = commandList.data();

		Test_assert(t, "beginFixed", commandList.begin(true, e_rr));

		CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "scope5", (c::Bool) scope);

		c::Bool filled = true;

		for(c::U64 i = 0; i < 4; ++i)
			filled = filled && scope.setStencil((c::U8) i);

		Test_assert(t, "fixedFits", filled);
		Test_assert(t, "fixedFull", ptr->next == 64);
		Test_assert(t, "fixedOverflows", !scope.setStencil(4));
		Test_assert(t, "fixedNotGrown", c::Buffer_length(ptr->data) == 64);
	}

	if(c::CommandListRef *raw = nullptr; Test_assert(t, "createResizable", c::GraphicsDeviceRef_createCommandList(
		deviceRef, 64, 8, 4, true, &raw, e_rr
	))) {

		CommandList commandList{ RefPtr<c::CommandList>::adopt(raw) };
		const c::CommandList *ptr = commandList.data();

		Test_assert(t, "beginResizable", commandList.begin(true, e_rr));

		CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "scope6", (c::Bool) scope);

		c::Bool grew = true;

		for(c::U64 i = 0; i < 8; ++i)
			grew = grew && scope.setStencil((c::U8) i);

		Test_assert(t, "resizableAccepts", grew);
		Test_assert(t, "resizableGrew", c::Buffer_length(ptr->data) > 64);
		Test_assert(t, "resizableKeptAll", ptr->next == 8 * 16);
	}

	//Copies are validated against what the images are, not just that they're images

	RenderTexture target2, fmtOther;
	DepthStencil depth;
	DeviceBuffer indirect;

	Test_assert(t, "createTarget2", dev.createRenderTexture(
		16, 16, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Validation copy target", target2,
		c::EMSAASamples_Off, nullptr, e_rr
	));

	Test_assert(t, "createFmtOther", dev.createRenderTexture(
		16, 16, c::ETextureFormatId_RG16f, c::EGraphicsResourceFlag_None, "Validation format target", fmtOther,
		c::EMSAASamples_Off, nullptr, e_rr
	));

	Test_assert(t, "createDepthCopy", dev.createDepthStencil(
		16, 16, c::EDepthStencilFormat_D32, false, "Validation depth", depth, c::EMSAASamples_Off, e_rr
	));

	Test_assert(t, "createIndirect", dev.createBuffer(
		c::EDeviceBufferUsage_Indirect, c::EGraphicsResourceFlag_None, "Validation indirect buffer", 256, indirect, nullptr,
		e_rr
	));

	if(target2.valid() && fmtOther.valid() && depth.valid())
		if(CommandList commandList = beginList(t, dev, "createCopy")) {

			const c::CopyImageRegion whole{};
			c::CopyImageRegion badLevel{};
			badLevel.srcLevelId = 1;

			//Negatives share a scope, since a failing command invalidates the scope but never the list

			CommandScope neg = commandList.scope({}, 1, {}, e_rr);
			Test_assert(t, "scopeCopy", (c::Bool) neg);

			Test_assert(t, "copyNoRegions", !neg.copyImageRegions(target.handle(), target2.handle(), {}));
			Test_assert(t, "copyNullSrc", !neg.copyImage(nullptr, target2.handle(), whole));
			Test_assert(t, "copyBufferAsSrc", !neg.copyImage(buffer.handle(), target2.handle(), whole));
			Test_assert(t, "copyDepthColorMix", !neg.copyImage(depth.handle(), target2.handle(), whole));
			Test_assert(t, "copyBothDepth", !neg.copyImage(depth.handle(), depth.handle(), whole));
			Test_assert(t, "copyFormatMismatch", !neg.copyImage(target.handle(), fmtOther.handle(), whole));
			Test_assert(t, "copyBadLevel", !neg.copyImage(target.handle(), target2.handle(), badLevel));

			Test_assert(t, "endScopeCopyNeg", neg.end(e_rr));
			Test_assert(t, "copyNegHidden", !commandList.data()->activeScopes.length);

			//A valid copy is a modify op, so its scope is the one that commits

			CommandScope pos = commandList.scope({}, 2, {}, e_rr);
			Test_assert(t, "scopeCopyPos", (c::Bool) pos);
			Test_assert(t, "copyValid", pos.copyImage(target.handle(), target2.handle(), whole, e_rr));
			Test_assert(t, "endScopeCopyPos", pos.end(e_rr));
			Test_assert(t, "copyCommitted", commandList.data()->activeScopes.length == 1);
		}

	//Indirect draws validate their argument buffer before any render state, so the buffer rules are reachable
	// without a pipeline; dispatchIndirect checks its pipeline first, so only that refusal is visible for it.
	//drawIndirect lives on CommandRender, so reaching it without a render pass means the raw command.

	if(indirect.valid())
		if(CommandList commandList = beginList(t, dev, "createIndirectList")) {

			CommandScope scope = commandList.scope({}, 1, {}, e_rr);
			Test_assert(t, "scopeIndirect", (c::Bool) scope);

			c::CommandListRef *l = scope.raw();

			Test_assert(t, "indirectNullBuffer", !c::CommandListRef_drawIndirect(l, nullptr, 0, 1, false, nullptr));
			Test_assert(t, "indirectMisaligned", !c::CommandListRef_drawIndirect(l, indirect.handle(), 8, 1, false, nullptr));
			Test_assert(t, "indirectZeroCalls", !c::CommandListRef_drawIndirect(l, indirect.handle(), 0, 0, false, nullptr));
			Test_assert(t, "indirectWrongUsage", !c::CommandListRef_drawIndirect(l, buffer.handle(), 0, 1, false, nullptr));
			Test_assert(t, "indirectOOB", !c::CommandListRef_drawIndirect(l, indirect.handle(), 256, 1, false, nullptr));
			Test_assert(t, "dispatchIndirectNeedsPipeline", !scope.dispatchIndirect(indirect, 0));
		}
}

// -- 22. Render passes -----------------------------------------------------------

//startRenderExt is what gives a scope a render area, and viewport, scissor and every draw are gated on it.
//That makes this the only place those can be exercised positively, and the place their gating can be shown to
// actually lift and drop again.
//Attachments take a concrete subresource rather than a whole image, so unlike clear, U32_MAX is out of range here.

extern "C" void Test_graphicsRenderPass(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Error *e_rr = &t->err;

	Test_setModule(t, "CommandList/render");

	Device dev = Device::share(deviceRef);

	if(!(dev.info().capabilities.features & c::EGraphicsFeatures_DirectRendering)) {
		Test_print(t, "Device lacks direct rendering, skipping render pass tests");
		return;
	}

	RenderTexture target, smaller;
	DepthStencil depth;

	Test_assert(t, "createTarget", dev.createRenderTexture(
		16, 16, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Render pass target", target, c::EMSAASamples_Off,
		nullptr, e_rr
	));

	Test_assert(t, "createSmaller", dev.createRenderTexture(
		8, 8, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Render pass smaller target", smaller,
		c::EMSAASamples_Off, nullptr, e_rr
	));

	Test_assert(t, "createDepth", dev.createDepthStencil(
		16, 16, c::EDepthStencilFormat_D32, false, "Render pass depth", depth, c::EMSAASamples_Off, e_rr
	));

	//RAII is what turns the C module's staged RefPtr_dec ladders on these two early exits into a plain return.

	if(!target.valid() || !smaller.valid() || !depth.valid())
		return;

	CommandList commandList = beginList(t, dev, "createRender");

	if(!commandList)
		return;

	const c::CommandList *ptr = commandList.data();

	const c::I32x2 zero = xy(0, 0);

	c::AttachmentInfo color{};
	color.image = target.handle();
	color.load = c::ELoadAttachmentType_Clear;

	{
		//A failing command only raises InvalidState, so every rejection below can share one scope.
		//A refused render pass hands back a falsy CommandRender whose destructor has nothing to end, so each of
		// these fits in one expression.

		CommandScope scope = commandList.scope({}, 1, {}, e_rr);
		Test_assert(t, "scope", (c::Bool) scope);

		Test_assert(t, "renderNeedsTargets", !scope.render(zero, zero, {}));

		//A whole image range is what clear takes; an attachment has to name one subresource

		c::AttachmentInfo wholeRange = color;
		wholeRange.range = c::ImageRange{ c::U32_MAX, c::U32_MAX };

		Test_assert(t, "renderRejectsWholeRange", !scope.render(zero, zero, { wholeRange }));

		//Clearing an attachment that is declared read only contradicts itself

		c::AttachmentInfo readOnlyClear = color;
		readOnlyClear.readOnly = true;

		Test_assert(t, "renderRejectsReadOnlyClear", !scope.render(zero, zero, { readOnlyClear }));

		//A depth image is not a colour attachment, and the mismatch has to be caught rather than reinterpreted

		c::AttachmentInfo depthAsColor{};
		depthAsColor.image = depth.handle();

		Test_assert(t, "renderRejectsDepthAsColor", !scope.render(zero, zero, { depthAsColor }));

		//Resolving needs somewhere to resolve to

		c::AttachmentInfo resolveNoImage = color;
		resolveNoImage.resolveMode = 1;

		Test_assert(t, "renderRejectsResolveWithoutImage", !scope.render(zero, zero, { resolveNoImage }));

		//Attachments share one render area, so a second target smaller than the first can't satisfy it

		c::AttachmentInfo pairFirst{}, pairSecond{};
		pairFirst.image = target.handle();
		pairSecond.image = smaller.handle();

		Test_assert(t, "renderRejectsMismatchedSizes", !scope.render(zero, zero, { pairFirst, pairSecond }));

		//An offset outside the first attachment leaves no area to render into

		Test_assert(t, "renderRejectsOffsetOutside", !scope.render(xy(16, 16), zero, { color }));

		//None of that opened a render pass, so the state that depends on one is still refused.
		//No CommandRender was ever handed out here, so the unmatched end is the raw command.

		Test_assert(t, "noRenderAfterFailures", sameXy(ptr->currentSize, zero));
		Test_assert(t, "endRenderWithoutStart", !c::CommandListRef_endRenderExt(scope.raw(), nullptr));

		Test_assert(t, "endScopeNegatives", scope.end(e_rr));
	}

	//A valid render pass sets the area, which is what lifts the gate on viewport, scissor and draws

	CommandScope scope = commandList.scope({}, 2, {}, e_rr);
	Test_assert(t, "scope2", (c::Bool) scope);

	CommandRender render = scope.render(zero, zero, { color }, nullptr, e_rr);
	Test_assert(t, "startRender", (c::Bool) render);
	Test_assert(t, "renderAreaFromTarget", sameXy(ptr->currentSize, xy(16, 16)));

	//Zero size means the whole remaining area rather than an empty one.
	//setViewport and setScissor sit on CommandScope rather than CommandRender (the C API takes them in any
	// scope), so they record through the still open scope while the pass is running.

	Test_assert(t, "viewportFull", scope.setViewport(zero, zero, e_rr));
	Test_assert(t, "scissorPartial", scope.setScissor(zero, xy(8, 8), e_rr));
	Test_assert(t, "viewportAndScissor", scope.setViewportAndScissor(xy(2, 2), xy(4, 4), e_rr));

	Test_assert(t, "viewportFlagsSet",
		(ptr->tempStateFlags & (c::ECommandStateFlags_AnyViewport | c::ECommandStateFlags_AnyScissor)) ==
		(c::ECommandStateFlags_AnyViewport | c::ECommandStateFlags_AnyScissor)
	);

	//An offset at or past the render area has nothing left to cover

	Test_assert(t, "viewportOffsetOutside", !scope.setViewport(xy(16, 16), xy(1, 1)));

	//A draw still needs a pipeline even once the render pass and viewport state are in place

	Test_assert(t, "drawStillNeedsPipeline", !render.drawUnindexed(3, 1));

	//Ending the pass takes the area away again, and with it the state that depended on it

	Test_assert(t, "endRender", render.end(e_rr));
	Test_assert(t, "renderAreaCleared", sameXy(ptr->currentSize, zero));

	Test_assert(t, "viewportFlagsCleared",
		!(ptr->tempStateFlags & (c::ECommandStateFlags_AnyViewport | c::ECommandStateFlags_AnyScissor))
	);

	Test_assert(t, "viewportAfterEndRender", !scope.setViewport(zero, xy(8, 8)));

	//render.end() already gave the pass back, so a second end has to be the raw command.

	Test_assert(t, "endRenderTwice", !c::CommandListRef_endRenderExt(scope.raw(), nullptr));
}
