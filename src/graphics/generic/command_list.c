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

//graphics/generic/command_list.c

#include "types/container/list_impl.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/commands.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/sampler.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/blas.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/container/texture_format.h"
#include "types/base/string_base.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"
#include "command_list_internal.h"

TListImpl(CommandOpInfo);
TListImpl(TransitionInternal);
TListImpl(CommandScope);
TListImpl(Transition);
TListImpl(CommandScopeDependency);
TListImpl(ClearImageCmd);
TListImpl(AttachmentInfo);
TListImpl(CopyImageRegion);

//Clear, append, begin and end

Bool CommandListRef_clear(CommandListRef *commandListRef, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validate(commandListRef);

	for (U64 i = 0; i < commandList->resources.length; ++i)
		RefPtr_dec(&commandList->resources.ptrNonConst[i]);

	gotoIfError3(clean, ListCommandOpInfo_clear(&commandList->commandOps, e_rr));
	gotoIfError3(clean, ListRefPtr_clear(&commandList->resources, e_rr));
	gotoIfError3(clean, ListTransitionInternal_clear(&commandList->transitions, e_rr));
	gotoIfError3(clean, ListCommandScope_clear(&commandList->activeScopes, e_rr));

	gotoIfError3(clean, ListDeviceResourceVersion_clear(&commandList->activeSwapchains, e_rr));

	commandList->next = 0;

clean:
	return s_uccess;
}

Bool CommandListRef_begin(CommandListRef *commandListRef, Bool doClear, U64 lockTimeout, Error *e_rr) {

	Bool s_uccess = true;
	CommandList *commandList = NULL;

	if(!commandListRef || commandListRef->refPtrType->typeId != (TypeId)EGraphicsTypeId_CommandList)
		retError(clean, Error_nullPointer(0, "CommandListRef_begin()::commandListRef invalid"));

	commandList = CommandListRef_ptr(commandListRef);

	if(SpinLock_lock(&commandList->lock, lockTimeout) != ELockAcquire_Acquired)
		retError(clean, Error_invalidOperation(0, "CommandListRef_begin() couldn't acquire lock"));

	if(commandList->state == ECommandListState_Open)
		retError(clean, Error_invalidOperation(1, "CommandListRef_begin() can only be called on non open cmdlist"));

	commandList->state = ECommandListState_Open;

	if(doClear)
		gotoIfError3(clean, CommandListRef_clear(commandListRef, e_rr));

	if (!doClear) {        //Reacquire swapchains to ensure versions are the same

		for (U64 i = 0; i < commandList->activeSwapchains.length; ++i ) {

			const DeviceResourceVersion v = commandList->activeSwapchains.ptr[i];
			Swapchain *swapchain = SwapchainRef_ptr(v.resource);

			if(SpinLock_lock(&swapchain->lock, U64_MAX) != ELockAcquire_Acquired)
				retError(clean, Error_invalidOperation(0, "CommandListRef_begin() couldn't re-acquire swapchain locks"));

			const U64 verId = swapchain->versionId;
			SpinLock_unlock(&swapchain->lock);

			if(verId != v.version)
				retError(clean, Error_invalidOperation(
					1, "CommandListRef_begin() can't be called without clear if pre-recorded swapchain versionId changed"));
		}
	}

clean:

	if(!s_uccess && commandList) {

		ListDeviceResourceVersion_clear(&commandList->activeSwapchains, e_rr);

		commandList->state = ECommandListState_Invalid;
		SpinLock_unlock(&commandList->lock);
	}

	return s_uccess;
}

Bool CommandListRef_end(CommandListRef *commandListRef, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = commandListRef ? GraphicsDeviceRef_getAlloc(CommandListRef_ptr(commandListRef)->device) : NULL;

	CommandListRef_validate(commandListRef);

	if (commandList->tempStateFlags & ECommandStateFlags_HasScope)
		retError(clean, Error_invalidState(0, "CommandListRef_end() can't be called if a scope is open"));

	gotoIfError3(clean, ListRefPtr_reserve(&commandList->resources, commandList->transitions.length, alloc, e_rr));

	for (U64 i = 0; i < commandList->transitions.length; ++i) {

		const TransitionInternal *transitions = &commandList->transitions.ptrNonConst[i];

		if(!ListRefPtr_contains(commandList->resources, transitions->resource, 0, NULL)) {                //TODO: hashSet

			if(RefPtr_inc(transitions->resource))        //CommandList will keep resource alive.
				gotoIfError3(clean, ListRefPtr_pushBack(&commandList->resources, transitions->resource, alloc, e_rr));
		}
	}

	commandList->state = ECommandListState_Closed;

clean:

	if(!s_uccess && commandList)
		commandList->state = ECommandListState_Invalid;

	if(commandList)
		SpinLock_unlock(&commandList->lock);

	return s_uccess;
}

Bool CommandListRef_imageRangeConflicts(RefPtr *image1, ImageRange range1, RefPtr *image2, ImageRange range2) {
	(void)range2; (void)range1;        //TODO:
	return image1 == image2;
}

Bool CommandListRef_bufferRangeConflicts(RefPtr *buffer1, BufferRange range1, RefPtr *buffer2, BufferRange range2) {
	(void)range2; (void)range1;        //TODO:
	return buffer1 == buffer2;
}

Bool CommandListRef_resourceConflicts(RefPtr *res1, RefPtr *res2) {        //TLAS, BLAS, etc.
	return res1 == res2;
}

Bool CommandListRef_isBound(CommandList *commandList, RefPtr *resource, ResourceRange range, TransitionInternal **found) {

	if(!resource)
		return false;

	for(U64 i = 0; i < commandList->pendingTransitions.length; ++i) {

		const TransitionInternal transition = commandList->pendingTransitions.ptr[i];

		if(transition.resource->refPtrType->typeId != resource->refPtrType->typeId)
			continue;

		if(TextureRef_isTexture(resource)) {
			if(CommandListRef_imageRangeConflicts(
				resource, range.image, transition.resource, transition.range.image
			)) {
				if(found) *found = &commandList->pendingTransitions.ptrNonConst[i];
				return true;
			}
		}

		else if(resource->refPtrType->typeId == (TypeId) EGraphicsTypeId_DeviceBuffer) {
			if(CommandListRef_bufferRangeConflicts(
				resource, range.buffer, transition.resource, transition.range.buffer
			)) {
				if(found) *found = &commandList->pendingTransitions.ptrNonConst[i];
				return true;
			}
		}

		else if(CommandListRef_resourceConflicts(resource, transition.resource)) {
			if(found) *found = &commandList->pendingTransitions.ptrNonConst[i];
			return true;
		}
	}

	return false;
}

Bool CommandList_append(CommandList *commandList, ECommandOp op, Buffer buf, U32 extraSkipStacktrace, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = commandList ? GraphicsDeviceRef_getAlloc(commandList->device) : NULL;

	(void)extraSkipStacktrace;

	const U64 len = Buffer_length(buf);
	Bool didPush = false;

	if(len > U32_MAX)
		retError(clean, Error_outOfBounds(2, len, U32_MAX, "CommandList_append() singular command can't exceed 32-bit"));

	if((commandList->commandOps.length + 1) >> 32)
		retError(clean, Error_outOfBounds(0, U32_MAX, U32_MAX, "CommandList_append() command ops can't exceed 32-bit"));

	if(commandList->next + len > Buffer_length(commandList->data)) {

		if(!commandList->allowResize)
			retError(clean, Error_outOfMemory(0, "CommandList_append() out of bounds"));

		//Resize buffer to allow allocation

		Buffer resized = Buffer_createNull();
		gotoIfError3(clean, Buffer_createEmptyBytes(Buffer_length(commandList->data) * 2 + KIBI + len, alloc, &resized, e_rr));

		Buffer_memcpy(resized, commandList->data);
		Buffer_free(&commandList->data, alloc);

		commandList->data = resized;
	}

	const CommandOpInfo info = (CommandOpInfo) {
		.op = op,
		.opSize = (U32) len
	};

	gotoIfError3(clean, ListCommandOpInfo_pushBack(&commandList->commandOps, info, alloc, e_rr));
	didPush = true;

	if(len) {
		Buffer_memcpy(Buffer_createRef(commandList->data.ptrNonConst + commandList->next, len), buf);
		commandList->next += len;
	}

clean:

	if(!s_uccess) {

		if(didPush)
			ListCommandOpInfo_popBack(&commandList->commandOps, NULL, e_rr);

		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;
	}

	return s_uccess;
}

Bool CommandListRef_transitionBuffer(
	CommandList *commandList,
	DeviceBufferRef *buffer,
	BufferRange range,
	ETransitionType type,
	EPipelineStage stage,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = commandList ? GraphicsDeviceRef_getAlloc(commandList->device) : NULL;

	if(!buffer)
		return s_uccess;

	TransitionInternal *oldState = NULL;
	if(CommandListRef_isBound(commandList, buffer, (ResourceRange) { .buffer = range }, &oldState)) {

		if(oldState->type != type)
			retError(clean, Error_invalidOperation(
				4, "CommandListRef_transitionBuffer()::buffer was already transitioned in scope!"
			));

		oldState->stage = (EPipelineStage) U64_min(oldState->stage, stage);
		return s_uccess;
	}

	const TransitionInternal transition = (TransitionInternal) {
		.resource = buffer,
		.range = (ResourceRange) { .buffer = range },
		.stage = stage,
		.type = type
	};

	gotoIfError3(clean, ListTransitionInternal_pushBack(&commandList->pendingTransitions, transition, alloc, e_rr));

clean:
	return s_uccess;
}

Bool CommandListRef_transitionRTAS(
	CommandList *commandList,
	RTASRef *rtasPtr,
	ETransitionType type,
	EPipelineStage stage,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = commandList ? GraphicsDeviceRef_getAlloc(commandList->device) : NULL;

	if(!rtasPtr)
		return s_uccess;

	Bool isTLAS = rtasPtr->refPtrType->typeId == (TypeId) EGraphicsTypeId_TLASExt;
	RTAS rtas = isTLAS ? TLASRef_ptr(rtasPtr)->base : BLASRef_ptr(rtasPtr)->base;

	if(stage == EPipelineStage_RTASBuild && type == ETransitionType_ShaderWrite) {

		if(rtas.tempScratchBuffer)
			gotoIfError3(clean, CommandListRef_transitionBuffer(
				commandList, rtas.tempScratchBuffer, (BufferRange) { 0 },
				ETransitionType_ShaderWrite, EPipelineStage_RTASBuild, e_rr));

		if (rtas.parent) {

			TransitionInternal *oldState = NULL;
			if(CommandListRef_isBound(commandList, rtas.parent, (ResourceRange) { 0 }, &oldState)) {

				if(oldState->type != type)
					retError(clean, Error_invalidOperation(
						4, "CommandListRef_transitionRTAS()::rtas.parent was already transitioned in scope!"
					));

				oldState->stage = (EPipelineStage) U64_min(oldState->stage, stage);
			}

			else {

				const TransitionInternal transition = (TransitionInternal) {
					.resource = rtas.parent, .stage = stage, .type = type
				};

				gotoIfError3(clean, ListTransitionInternal_pushBack(&commandList->pendingTransitions, transition, alloc, e_rr));
			}
		}

		if(isTLAS) {

			TLAS *tlas = TLASRef_ptr(rtasPtr);

			if(!tlas->useDeviceMemory) {
				gotoIfError3(clean, CommandListRef_transitionBuffer(
					commandList, tlas->tempInstanceBuffer, (BufferRange) { 0 },
					ETransitionType_ShaderRead, EPipelineStage_RTASBuild, e_rr
				));
			}

			else {
				gotoIfError3(clean, CommandListRef_transitionBuffer(
				commandList,
				tlas->deviceData.buffer,
				(BufferRange) {
					.startRange = tlas->deviceData.offset,
					.endRange = tlas->deviceData.offset + tlas->deviceData.len
				},
				ETransitionType_ShaderRead, EPipelineStage_RTASBuild, e_rr));
			}
		}

		else {

			BLAS *blas = BLASRef_ptr(rtasPtr);

			if(blas->base.asConstructionType == EBLASConstructionType_Procedural)
			{
				gotoIfError3(clean, CommandListRef_transitionBuffer(
					commandList,
					blas->aabbBuffer.buffer,
					(BufferRange) {
						.startRange = blas->aabbBuffer.offset + blas->aabbOffset,
						.endRange = blas->aabbBuffer.offset + blas->aabbBuffer.len
					},
					ETransitionType_ShaderRead, EPipelineStage_RTASBuild, e_rr));
			}

			else {

				gotoIfError3(clean, CommandListRef_transitionBuffer(
					commandList,
					blas->indexBuffer.buffer,
					(BufferRange) {
						.startRange = blas->indexBuffer.offset,
						.endRange = blas->indexBuffer.offset + blas->indexBuffer.len
					},
					ETransitionType_ShaderRead, EPipelineStage_RTASBuild, e_rr));

				gotoIfError3(clean, CommandListRef_transitionBuffer(
					commandList,
					blas->positionBuffer.buffer,
					(BufferRange) {
						.startRange = blas->positionBuffer.offset + blas->positionOffset,
						.endRange = blas->indexBuffer.offset + blas->indexBuffer.len
					},
					ETransitionType_ShaderRead, EPipelineStage_RTASBuild, e_rr));
			}
		}
	}

	TransitionInternal *oldState = NULL;
	if(CommandListRef_isBound(commandList, rtasPtr, (ResourceRange) { 0 }, &oldState)) {

		if(oldState->type != type)
			retError(clean, Error_invalidOperation(
				4, "CommandListRef_transitionRTAS()::rtas was already transitioned in scope!"
			));

		oldState->stage = (EPipelineStage) U64_min(oldState->stage, stage);
		return s_uccess;
	}

	const TransitionInternal transition = (TransitionInternal) { .resource = rtasPtr, .stage = stage, .type = type };
	gotoIfError3(clean, ListTransitionInternal_pushBack(&commandList->pendingTransitions, transition, alloc, e_rr));

clean:
	return s_uccess;
}

Bool CommandListRef_transitionImage(
	CommandList *commandList,
	TextureRef *image,
	ImageRange range,
	ETransitionType type,
	EPipelineStage stage,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = commandList ? GraphicsDeviceRef_getAlloc(commandList->device) : NULL;

	if(!image)
		return s_uccess;

	TransitionInternal *oldState = NULL;
	if(CommandListRef_isBound(commandList, image, (ResourceRange) { .image = range }, &oldState)) {

		if(oldState->type != type)
			retError(clean, Error_invalidOperation(
				4, "CommandListRef_transitionImage()::image was already transitioned in scope!"
			));

		switch (type) {

			case ETransitionType_Clear:
			case ETransitionType_CopyWrite:
			case ETransitionType_ShaderWrite:
			case ETransitionType_ResolveTargetWrite:
				retError(clean, Error_invalidOperation(
					4,
					"CommandListRef_transitionImage()::image was used as writable target in the same scope, "
					"this is a write hazard and needs a separate scope to handle synchronization properly."
				));

			default:
				break;
		}

		//To combine shader transitions we just take the highest up shader stage it's used

		oldState->stage = (EPipelineStage) U64_min(oldState->stage, stage);
		return s_uccess;
	}

	const TransitionInternal transition = (TransitionInternal) {
		.resource = image,
		.range = (ResourceRange) { .image = range },
		.stage = stage,
		.type = type
	};

	gotoIfError3(clean, ListTransitionInternal_pushBack(&commandList->pendingTransitions, transition, alloc, e_rr));

clean:
	return s_uccess;
}

//Render calls

Bool CommandListRef_startScope(
	CommandListRef *commandListRef,
	const ListTransition *transitions,
	U32 id,
	const ListCommandScopeDependency *deps,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = commandListRef ? GraphicsDeviceRef_getAlloc(CommandListRef_ptr(commandListRef)->device) : NULL;

	CommandListRef_validate(commandListRef);

	GraphicsDeviceRef *device = commandList->device;

	if(transitions && transitions->length > U32_MAX)
		retError(clean, Error_outOfBounds(
			1, transitions->length, U32_MAX, "CommandListRef_startScope()::transitions.length > U32_MAX"
		));

	if(commandList->tempStateFlags & ECommandStateFlags_HasScope)        //No nested scopes
		retError(clean, Error_invalidOperation(
			0, "CommandListRef_startScope() scope is already present. Nested scopes are unsupported"
		));

	gotoIfError3(clean, ListTransitionInternal_clear(&commandList->pendingTransitions, e_rr));
	gotoIfError3(clean, ListTransitionInternal_reserve(
		&commandList->pendingTransitions, transitions ? transitions->length : 0, alloc, e_rr
	));

	for(U64 i = 0; i < (transitions ? transitions->length : 0); ++i) {

		Transition transition = transitions->ptr[i];
		RefPtr *res = transition.resource;

		if(!res)
			retError(clean, Error_nullPointer(1, "CommandListRef_startScope()::transitions[i].resource is NULL"));

		UnifiedTexture tex = TextureRef_getUnifiedTexture(res, NULL);
		Bool isSampler = res->refPtrType->typeId == (TypeId) EGraphicsTypeId_Sampler;

		GraphicsResource resource = tex.resource;

		if (tex.resource.device)
			resource = tex.resource;

		else if (res->refPtrType->typeId == (TypeId) EGraphicsTypeId_DeviceBuffer)
			resource = DeviceBufferRef_ptr(res)->resource;

		else if(isSampler)
			resource = (GraphicsResource) { .device = SamplerRef_ptr(res)->device };        //Only device is required here

		//Get device and mark as readonly

		else if (res->refPtrType->typeId == (TypeId) EGraphicsTypeId_TLASExt) {

			TLAS *tlas = TLASRef_ptr(res);

			if(!tlas->useDeviceMemory)
				for (U64 j = 0; j < tlas->cpuInstancesStatic.length; ++j) {

					TLASInstanceData dat = (TLASInstanceData) { 0 };
					TLAS_getInstanceDataCpu(tlas, j, &dat);

					if (!dat.blasCpu)
						continue;

					gotoIfError3(clean, CommandListRef_transitionRTAS(
						commandList, dat.blasCpu,
						ETransitionType_ShaderRead, transition.stage, e_rr));
				}

			resource = (GraphicsResource) {
				.device = TLASRef_ptr(res)->base.device, .flags = EGraphicsResourceFlag_ShaderRead
			};
		}

		//Get device and mark as readonly

		else if (res->refPtrType->typeId == (TypeId) EGraphicsTypeId_BLASExt)
			resource = (GraphicsResource) {
				.device = BLASRef_ptr(res)->base.device, .flags = EGraphicsResourceFlag_ShaderRead
			};

		else {
			retError(clean, Error_invalidParameter(
			1, 0, "CommandListRef_startScope()::transitions[i].resource's type is unsupported"));
		}

		TransitionInternal transitionDst;

		if(!isSampler) {

			if (transition.isWrite && !(resource.flags & EGraphicsResourceFlag_ShaderWrite))
				retError(clean, Error_constData(
					0, 0, "CommandListRef_startScope()::transitions[i].resource should be writable"));

			if(!transition.isWrite && !(resource.flags & EGraphicsResourceFlag_ShaderRead))
				retError(clean, Error_unsupportedOperation(
					1, "CommandListRef_startScope()::transitions[i].resource should be readable"));

			transitionDst = (TransitionInternal) {
				.resource = res,
				.range = transition.range,
				.stage = transition.stage,
				.type = transition.isWrite ? ETransitionType_ShaderWrite : ETransitionType_ShaderRead
			};
		}

		else transitionDst = (TransitionInternal) { .resource = res, .type = ETransitionType_KeepAlive };

		if(resource.device != device)
			retError(clean, Error_unsupportedOperation(
				0, "CommandListRef_startScope()::transitions[i].resource's device is incompatible"
			));

		TransitionInternal *found = NULL;
		if(CommandListRef_isBound(commandList, res, transition.range, &found)) {

			if(found->type != transitionDst.type)
				retError(clean, Error_invalidOperation(
					0, "CommandListRef_startScope()::transitions[i].resource is already transitioned"
				));

			//To combine shader transitions we just take the highest up shader stage it's used

			found->stage = (EPipelineStage) U64_min(found->stage, transitionDst.stage);
			continue;
		}

		gotoIfError3(clean, ListTransitionInternal_pushBack(&commandList->pendingTransitions, transitionDst, alloc, e_rr));
	}

	//Scope has to be unique

	for (U64 i = 0; i < commandList->activeScopes.length; ++i) {

		CommandScope scope = commandList->activeScopes.ptr[i];

		if(scope.scopeId == id)
			retError(clean, Error_alreadyDefined(0, "CommandListRef_startScope()::id should be unique"));
	}

	//Find deps

	for (U64 j = 0; j < (deps ? deps->length : 0); ++j) {

		CommandScopeDependency dep = deps->ptr[j];

		if(dep.type == ECommandScopeDependencyType_Unconditional)        //Don't care
			continue;

		Bool found = false;

		for (U64 i = 0; i < commandList->activeScopes.length; ++i) {    //TODO: HashSet

			CommandScope scope = commandList->activeScopes.ptr[i];

			if (scope.scopeId == dep.id) {
				found = true;
				break;
			}
		}

		if(found)
			continue;

		retError(clean, Error_notFound(0, 0, "CommandListRef_startScope()::deps[j] not found"));
	}

	//TODO: Append deps to array so runtime can use it (U32 only)

	commandList->lastCommandId = (U32) commandList->commandOps.length;
	commandList->lastOffset = commandList->next;
	commandList->lastScopeId = id;

	gotoIfError3(clean, CommandList_append(commandList, ECommandOp_StartScope, Buffer_createNull(), 0, e_rr));

	commandList->tempStateFlags |= ECommandStateFlags_HasScope;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags = ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_endScope(CommandListRef *commandListRef, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = commandListRef ? GraphicsDeviceRef_getAlloc(CommandListRef_ptr(commandListRef)->device) : NULL;

	CommandListRef_validateScope(commandListRef, clean)

	//Check if scope has to be hidden.

	if(
		(commandList->tempStateFlags & ECommandStateFlags_InvalidState) ||            //Hide scope
		!(commandList->tempStateFlags & ECommandStateFlags_HasModifyOp)
	) {
		//Pretend the last commands didn't happen

		gotoIfError3(clean, ListCommandOpInfo_resize(&commandList->commandOps, commandList->lastCommandId, NULL, e_rr));
		commandList->next = commandList->lastOffset;

		goto clean;
	}

	if(commandList->debugRegionStack)
		retError(clean, Error_invalidOperation(
			0, "CommandListRef_endScope() can't close scope while debugRegion is still active"
		));

	if(!I32x2_eq2(commandList->currentSize, I32x2_zero))
		retError(clean, Error_invalidOperation(1, "CommandListRef_endScope() can't close scope while render is active"));

	//Push command, transitions and scope

	if((commandList->transitions.length + commandList->pendingTransitions.length) >> 32)
		retError(clean, Error_outOfBounds(
			0, commandList->transitions.length + commandList->pendingTransitions.length, U32_MAX,
			"CommandListRef_endScope() transitionCount of command list can't exceed U32_MAX"
		));

	const U32 commandOps = (U32)((commandList->commandOps.length + 1) - commandList->lastCommandId);
	const U64 commandLen = commandList->next - commandList->lastOffset;
	const U32 transitionOffset = (U32) commandList->transitions.length;

	gotoIfError3(clean, CommandList_append(commandList, ECommandOp_EndScope, Buffer_createNull(), 0, e_rr));
	gotoIfError3(clean, ListTransitionInternal_pushAll(
		&commandList->transitions,
		commandList->pendingTransitions,
		alloc,
		e_rr
	));

	const CommandScope scope = (CommandScope) {
		.commandBufferOffset = commandList->lastOffset,
		.commandBufferLength = commandLen,
		.commandOpOffset = commandList->lastCommandId,
		.transitionOffset = transitionOffset,
		.commandOps = commandOps,
		.transitionCount = (U32) commandList->pendingTransitions.length,
		.scopeId = commandList->lastScopeId
	};

	gotoIfError3(clean, ListCommandScope_pushBack(&commandList->activeScopes, scope, alloc, e_rr));

clean:

	if (commandList) {

		for(U64 i = 0; i < EPipelineType_Count; ++i)
			commandList->pipeline[i] = NULL;

		ListTransitionInternal_clear(&commandList->pendingTransitions, e_rr);

		commandList->tempStateFlags = 0;
		commandList->debugRegionStack = 0;
		commandList->currentSize = I32x2_zero;

		if(!s_uccess)
			commandList->state = ECommandListState_Invalid;
	}

	return s_uccess;
}

//Free and create

void CommandList_free(CommandList *cmd, const Allocator *alloc) {

	(void)alloc;

	SpinLock_lock(&cmd->lock, U64_MAX);

	for (U64 i = 0; i < cmd->resources.length; ++i)
		RefPtr_dec(cmd->resources.ptrNonConst + i);

	ListCommandOpInfo_free(&cmd->commandOps, alloc);
	ListRefPtr_free(&cmd->resources, alloc);
	ListCommandScope_free(&cmd->activeScopes, alloc);
	ListTransitionInternal_free(&cmd->transitions, alloc);
	ListTransitionInternal_free(&cmd->pendingTransitions, alloc);
	ListDeviceResourceVersion_free(&cmd->activeSwapchains, alloc);
	Buffer_free(&cmd->data, alloc);

	RefPtr_dec(&cmd->device);
}

Bool GraphicsDeviceRef_createCommandList(
	GraphicsDeviceRef *deviceRef,
	U64 commandListLen,
	U64 estimatedCommandCount,
	U64 estimatedResources,
	Bool allowResize,
	CommandListRef **commandListRef,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	gotoIfError3(clean, RefPtr_create(&GraphicsDeviceRef_getTypes(deviceRef)->commandList, commandListRef, e_rr));

	CommandList *commandList = CommandListRef_ptr(*commandListRef);

	gotoIfError3(clean, Buffer_createEmptyBytes(commandListLen, alloc, &commandList->data, e_rr));

	gotoIfError3(clean, ListCommandOpInfo_reserve(&commandList->commandOps, estimatedCommandCount, alloc, e_rr));
	gotoIfError3(clean, ListRefPtr_reserve(&commandList->resources, estimatedResources, alloc, e_rr));
	gotoIfError3(clean, ListCommandScope_reserve(&commandList->activeScopes, 16, alloc, e_rr));
	gotoIfError3(clean, ListTransitionInternal_reserve(&commandList->transitions, estimatedResources, alloc, e_rr));
	gotoIfError3(clean, ListTransitionInternal_reserve(&commandList->pendingTransitions, 32, alloc, e_rr));

	RefPtr_inc(deviceRef);
	commandList->device = deviceRef;
	commandList->allowResize = allowResize;

	goto success;

clean:
	RefPtr_dec(commandListRef);

success:
	return s_uccess;
}
