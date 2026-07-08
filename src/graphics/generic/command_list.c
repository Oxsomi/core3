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
#include "platforms/logx.h"
#include "types/container/ref_ptr.h"
#include "types/container/string.h"
#include "types/container/texture_format.h"
#include "types/base/mathi.h"
#include "types/base/mathf.h"
#include "types/base/constants.h"

TListImpl(CommandOpInfo);
TListImpl(TransitionInternal);
TListImpl(CommandScope);
TListImpl(Transition);
TListImpl(CommandScopeDependency);
TListImpl(ClearImageCmd);
TListImpl(AttachmentInfo);
TListImpl(CopyImageRegion);

//Clear, append, begin and end

#define CommandListRef_validate(v)                                                                                \
																												\
	CommandList *commandList = NULL;                                                                            \
																												\
	if(!(v) || (v)->refPtrType->typeId != (ETypeId)EGraphicsTypeId_CommandList)                                                \
		retError(clean, Error_nullPointer(0, "CommandListRef_validate() cmdlist is invalid"));                            \
																												\
	commandList = CommandListRef_ptr(v);                                                                        \
																												\
	if(!SpinLock_isLockedForThread(&commandList->lock))                                                            \
		retError(clean, Error_invalidOperation(0, "CommandListRef_validate() cmdlist isn't locked"));                        \
																												\
	if(commandList->state != ECommandListState_Open)                                                            \
		retError(clean, Error_invalidOperation(1, "CommandListRef_validate() cmdlist isn't open"));

#define CommandListRef_validateScope(v, label)                                                                    \
																												\
	CommandListRef_validate(v);                                                                                    \
																												\
	if(!(commandList->tempStateFlags & ECommandStateFlags_HasScope))                                            \
		retError(label, Error_invalidOperation(0, "CommandListRef_validateScope() scope isn't open"));

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

	if(!commandListRef || commandListRef->refPtrType->typeId != (ETypeId)EGraphicsTypeId_CommandList)
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

Bool CommandList_validateGraphicsPipeline(
	Pipeline *pipeline,
	ImageAndRange images[8],
	U8 imageCount,
	EDepthStencilFormat depthFormat,
	EMSAASamples boundSampleCount,
	Error *e_rr
) {

	Bool s_uccess = true;
	PipelineGraphicsInfo *info = Pipeline_info(pipeline, PipelineGraphicsInfo);

	//Depth stencil state can be set to None to ignore writing to depth stencil

	if (info->depthFormatExt != EDepthStencilFormat_None && depthFormat != info->depthFormatExt)
		retError(clean, Error_invalidState(0, "CommandList_validateGraphicsPipeline()::depthFormat was incompatible"));

	if(info->msaa != boundSampleCount)
		retError(clean, Error_invalidState(
			0,
			"CommandList_validateGraphicsPipeline()::boundSampleCount is incompatible with pipeline MSAA setting"
		));

	//Validate attachments

	if (info->attachmentCountExt != imageCount)
		retError(clean, Error_invalidState(1, "CommandList_validateGraphicsPipeline()::imageCount was incompatible"));

	for (U8 i = 0; i < imageCount && i < 8; ++i) {

		//Undefined is used to ignore the currently bound slot and to avoid writing to it

		if (!info->attachmentFormatsExt[i])
			continue;

		//Validate if formats are the same

		RefPtr *ref = images[i].image;

		if (!ref)
			retError(clean, Error_nullPointer(1, "CommandList_validateGraphicsPipeline()::images[i] is required by pipeline"));

		if (!TextureRef_isRenderTargetWritable(ref))
			retError(clean, Error_invalidParameter(1, i, "CommandList_validateGraphicsPipeline()::images[i] is invalid type"));

		DeviceResourceVersion v;
		const UnifiedTexture tex = TextureRef_getUnifiedTexture(ref, &v);

		if(info->attachmentFormatsExt[i] != tex.textureFormatId)
			retError(clean, Error_invalidState(i + 2, "CommandList_validateGraphicsPipeline()::images[i] is invalid format"));

		if(info->msaa != tex.sampleCount)
			retError(clean, Error_invalidState(
				i + 2,
				"CommandList_validateGraphicsPipeline()::images[i] has mismatching MSAA between pipeline and RenderTarget"
			));
	}

clean:
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

		else if(resource->refPtrType->typeId == (ETypeId) EGraphicsTypeId_DeviceBuffer) {
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

	Bool isTLAS = rtasPtr->refPtrType->typeId == (ETypeId) EGraphicsTypeId_TLASExt;
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

			if(!tlas->useDeviceMemory)
			{
				gotoIfError3(clean, CommandListRef_transitionBuffer(
					commandList, tlas->tempInstanceBuffer, (BufferRange) { 0 },
					ETransitionType_ShaderRead, EPipelineStage_RTASBuild, e_rr));
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

//Standard commands

Bool CommandListRef_checkBounds(I32x2 offset, I32x2 size, I32 lowerBound1, I32 upperBound1, Error *e_rr) {

	Bool s_uccess = true;

	if(I32x2_any(I32x2_leq(size, I32x2_zero)))
		retError(clean, Error_invalidParameter(1, 0, "CommandListRef_checkBounds()::size is <=0"));

	const I32x2 upperBound = I32x2_xx2(upperBound1);
	const I32x2 lowerBound = I32x2_xx2(lowerBound1);

	if(I32x2_any(I32x2_gt(size, upperBound)))
		retError(clean, Error_invalidParameter(1, 0, "CommandListRef_checkBounds()::size > upperBound"));

	if(I32x2_any(I32x2_lt(offset, lowerBound)))
		retError(clean, Error_invalidParameter(0, 0, "CommandListRef_checkBounds()::offset < lowerBound"));

	if(I32x2_any(I32x2_gt(offset, upperBound)))
		retError(clean, Error_invalidParameter(0, 1, "CommandListRef_checkBounds()::offset > upperBound"));

	const I32x2 end = I32x2_add(offset, size);

	if(I32x2_any(I32x2_gt(end, upperBound)))
		retError(clean, Error_invalidParameter(0, 2, "CommandListRef_checkBounds()::offset + size > upperBound"));

clean:
	return s_uccess;
}

Bool CommandListRef_setViewportCmd(CommandListRef *commandListRef, I32x2 offset, I32x2 size, ECommandOp op, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	if(I32x2_any(I32x2_eq(commandList->currentSize, I32x2_zero)))
		retError(clean, Error_invalidOperation(0, "CommandListRef_setViewportCmd() requires startRender(Pass/Ext)"));

	if(I32x2_any(I32x2_geq(offset, commandList->currentSize)))
		retError(clean, Error_invalidOperation(1, "CommandListRef_setViewportCmd() offset >= currentSize"));

	if(I32x2_eq2(size, I32x2_zero))
		size = I32x2_sub(commandList->currentSize, offset);

	gotoIfError3(clean, CommandListRef_checkBounds(offset, size, -32768, 32767, e_rr));

	I32x4 values = I32x4_create2_2(offset, size);
	gotoIfError3(clean, CommandList_append(commandList, op, Buffer_createRefConst(&values, sizeof(values)), 1, e_rr));

	if(op == ECommandOp_SetViewport || op == ECommandOp_SetViewportAndScissor)
		commandList->tempStateFlags |= ECommandStateFlags_AnyViewport;

	if(op == ECommandOp_SetScissor  || op == ECommandOp_SetViewportAndScissor)
		commandList->tempStateFlags |= ECommandStateFlags_AnyScissor;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_setViewport(CommandListRef *commandListRef, I32x2 offset, I32x2 size, Error *e_rr) {
	return CommandListRef_setViewportCmd(commandListRef, offset, size, ECommandOp_SetViewport, e_rr);
}

Bool CommandListRef_setScissor(CommandListRef *commandListRef, I32x2 offset, I32x2 size, Error *e_rr) {
	return CommandListRef_setViewportCmd(commandListRef, offset, size, ECommandOp_SetScissor, e_rr);
}

Bool CommandListRef_setViewportAndScissor(CommandListRef *commandListRef, I32x2 offset, I32x2 size, Error *e_rr) {
	return CommandListRef_setViewportCmd(commandListRef, offset, size, ECommandOp_SetViewportAndScissor, e_rr);
}

Bool CommandListRef_setStencil(CommandListRef *commandListRef, U8 stencilValueU8, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	U64 stencilValue[2] = { stencilValueU8, 0 };        //Has to be padded to 16-byte
	gotoIfError3(clean, CommandList_append(
		commandList, ECommandOp_SetStencil, Buffer_createRefConst(stencilValue, sizeof(stencilValue)), 0, e_rr));

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_setBlendConstants(CommandListRef *commandListRef, F32x4 blendConstants, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	gotoIfError3(clean, CommandList_append(
		commandList, ECommandOp_SetBlendConstants, Buffer_createRefConst(&blendConstants, sizeof(F32x4)), 0, e_rr));

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_clearImages(CommandListRef *commandListRef, ListClearImageCmd clearImages, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = commandListRef ? GraphicsDeviceRef_getAlloc(CommandListRef_ptr(commandListRef)->device) : NULL;

	Buffer buf = Buffer_createNull();
	CommandListRef_validateScope(commandListRef, clean)

	if(!clearImages.length)
		retError(clean, Error_nullPointer(1, "CommandListRef_clearImages()::clearImages.length is 0"));

	if(clearImages.length > U32_MAX)
		retError(clean, Error_outOfBounds(
			1, clearImages.length, U32_MAX, "CommandListRef_clearImages()::clearImages.length > U32_MAX"));

	GraphicsDeviceRef *device = commandList->device;

	for(U64 i = 0; i < clearImages.length; ++i) {

		ClearImageCmd clearImage = clearImages.ptr[i];
		UnifiedTexture tex = TextureRef_getUnifiedTexture(clearImage.image, NULL);

		if(!tex.resource.device || !TextureRef_isRenderTargetWritable(clearImage.image))
			retError(clean, Error_nullPointer(1, "CommandListRef_clearImages()::clearImages[i].image is invalid"));

		if(tex.resource.device != device)
			retError(clean, Error_unsupportedOperation(
				0, "CommandListRef_clearImages()::clearImages[i].image belongs to other device"));

		//TODO: Properly support this

		if (clearImage.range.layerId != U32_MAX && clearImage.range.layerId >= 1)
			retError(clean, Error_outOfBounds(
				1, clearImage.range.layerId, 1, "CommandListRef_clearImages()::clearImages[i].range.layerId is invalid"));

		if (clearImage.range.levelId != U32_MAX && clearImage.range.levelId >= 1)
			retError(clean, Error_outOfBounds(
				2, clearImage.range.levelId, 1, "CommandListRef_clearImages()::clearImages[i].range.levelId is invalid"));

		//Add transition

		gotoIfError3(clean, CommandListRef_transitionImage(
			commandList, clearImage.image, clearImage.range, ETransitionType_Clear, EPipelineStage_Count, e_rr));
	}

	//Copy buffer

	gotoIfError3(clean, Buffer_createEmptyBytes(ListClearImageCmd_bytes(clearImages) + sizeof(U64) * 2, alloc, &buf, e_rr));

	*(U64*)buf.ptrNonConst = (U64) clearImages.length;
	Buffer_memcpy(
		Buffer_createRef(buf.ptrNonConst + sizeof(U64) * 2, ListClearImageCmd_bytes(clearImages)),
		ListClearImageCmd_bufferConst(clearImages)
	);

	gotoIfError3(clean, CommandList_append(commandList, ECommandOp_ClearImages, buf, 0, e_rr));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	Buffer_free(&buf, alloc);
	return s_uccess;
}

Bool CommandListRef_copyImageRegions(
	CommandListRef *commandListRef,
	RefPtr *srcRef,
	RefPtr *dstRef,
	ListCopyImageRegion regions,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = commandListRef ? GraphicsDeviceRef_getAlloc(CommandListRef_ptr(commandListRef)->device) : NULL;

	Buffer buf = Buffer_createNull();
	CommandListRef_validateScope(commandListRef, clean)

	//Validate regions.length to be <0, 128]

	if(!regions.length)
		retError(clean, Error_nullPointer(3, "CommandListRef_copyImage()::regions.length is 0"));

	if(regions.length > 128)
		retError(clean, Error_outOfBounds(
			4, regions.length, 128, "CommandListRef_copyImage()::regions.length should be less than 128"));

	//Validate src and dst

	if(!srcRef || !dstRef)
		retError(clean, Error_outOfBounds(
			!srcRef ? 1 : 2, regions.length, U32_MAX, "CommandListRef_copyImage()::src and dst are required"));

	for (U64 i = 0; i < 2; ++i)
		if(!TextureRef_isTexture(i ? dstRef : srcRef))
			retError(clean, Error_invalidParameter(
				i ? 1 : 2, 0,
				"CommandListRef_copyImage()::src and dst should be a texture "
				"(Swapchain, DepthStencil, RenderTexture, DeviceTexture)"));

	//Validate depth stencil

	Bool isDepthStencil = TextureRef_isDepthStencil(dstRef);

	if(isDepthStencil != TextureRef_isDepthStencil(srcRef))
		retError(clean, Error_invalidParameter(
			1, 0, "CommandListRef_copyImage()::src and dst should be DepthStencil if one of them is to be compatible"));

	DeviceResourceVersion v;
	UnifiedTexture src = TextureRef_getUnifiedTexture(srcRef, &v);
	UnifiedTexture dst = TextureRef_getUnifiedTexture(dstRef, &v);

	if (isDepthStencil)
		retError(clean, Error_invalidParameter(
			1, 0, "CommandListRef_copyImage()::src and dst aren't allowed to be depth stencil"));

	//Ensure both formats are the same

	if(src.textureFormatId != dst.textureFormatId)
		retError(clean, Error_invalidParameter(
			1, 5, "CommandListRef_copyImage()::src and dst require the same texture format"));

	//Validate devices

	if(src.resource.device != commandList->device || dst.resource.device != commandList->device)
		retError(clean, Error_invalidParameter(
			1, 6, "CommandListRef_copyImage()::src and dst require the same device as the CommandList"));

	//Validate copy

	for(U64 i = 0; i < regions.length; ++i) {

		CopyImageRegion clearImage = regions.ptr[i];

		//Validate levelId

		if(clearImage.dstLevelId || clearImage.srcLevelId)        //TODO: Allow levels
			retError(clean, Error_invalidParameter(
				1, 6, "CommandListRef_copyImage()::regions[i].src/dstLevelId is out of bounds"));
	}

	//TODO: Check if regions are out of bounds (be sure to keep in mind that w,h,l is 0 means src->w,h,l - src->offset

	//TODO: Check if regions intersect in dst

	//TODO: Ensure there's no overlapping src and dst region
	//if(src == dst)
	//    ;

	//Add transitions

	ETransitionType types[2] = { ETransitionType_CopyRead, ETransitionType_CopyWrite };
	RefPtr *ptrs[2] = { srcRef, dstRef };

	for(U64 i = 0; i < 2; ++i)
		gotoIfError3(clean, CommandListRef_transitionImage(
			commandList, ptrs[i], (ImageRange) { 0 }, types[i], EPipelineStage_Count, e_rr));

	//Copy buffer

	gotoIfError3(clean, Buffer_createEmptyBytes(ListCopyImageRegion_bytes(regions) + sizeof(CopyImageCmd), alloc, &buf, e_rr));

	*(CopyImageCmd*)buf.ptr = (CopyImageCmd) {
		.src = srcRef,
		.dst = dstRef,
		.regionCount = (U32) regions.length
	};

	Buffer_memcpy(
		Buffer_createRef(buf.ptrNonConst + sizeof(CopyImageCmd), ListCopyImageRegion_bytes(regions)),
		ListCopyImageRegion_bufferConst(regions)
	);

	gotoIfError3(clean, CommandList_append(commandList, ECommandOp_CopyImage, buf, 0, e_rr));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	Buffer_free(&buf, alloc);
	return s_uccess;
}

Bool CommandListRef_copyImage(
	CommandListRef *commandListRef, RefPtr *src, RefPtr *dst, CopyImageRegion region,
	Error *e_rr
) {

	Bool s_uccess = true;
	CommandListRef_validateScope(commandListRef, clean)

	ListCopyImageRegion regions = (ListCopyImageRegion) { 0 };
	gotoIfError3(clean, ListCopyImageRegion_createRefConst(&region, 1, &regions, e_rr));

	gotoIfError3(clean, CommandListRef_copyImageRegions(commandListRef, src, dst, regions, e_rr));

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_clearImageu(
	CommandListRef *commandListRef,
	const U32 coloru[4],
	ImageRange range,
	RefPtr *image,
	Error *e_rr
) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	ClearImageCmd clearImage = (ClearImageCmd) {
		.image = image,
		.range = range
	};

	Buffer_memcpy(Buffer_createRef(&clearImage.color, sizeof(F32x4)), Buffer_createRefConst(coloru, sizeof(F32x4)));

	ListClearImageCmd clearImages = (ListClearImageCmd) { 0 };
	gotoIfError3(clean, ListClearImageCmd_createRefConst(&clearImage, 1, &clearImages, e_rr));

	gotoIfError3(clean, CommandListRef_clearImages(commandListRef, clearImages, e_rr));

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_clearImagei(CommandListRef *commandListRef, I32x4 color, ImageRange range, RefPtr *image, Error *e_rr) {
	return CommandListRef_clearImageu(commandListRef, (const U32*) &color, range, image, e_rr);
}

Bool CommandListRef_clearImagef(CommandListRef *commandListRef, F32x4 color, ImageRange range, RefPtr *image, Error *e_rr) {
	return CommandListRef_clearImageu(commandListRef, (const U32*) &color, range, image, e_rr);
}

//Render calls

Bool CommandListRef_startScope(
	CommandListRef *commandListRef,
	ListTransition transitions,
	U32 id,
	ListCommandScopeDependency deps,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = commandListRef ? GraphicsDeviceRef_getAlloc(CommandListRef_ptr(commandListRef)->device) : NULL;

	CommandListRef_validate(commandListRef);

	GraphicsDeviceRef *device = commandList->device;

	if(transitions.length > U32_MAX)
		retError(clean, Error_outOfBounds(
			1, transitions.length, U32_MAX, "CommandListRef_startScope()::transitions.length > U32_MAX"));

	if(commandList->tempStateFlags & ECommandStateFlags_HasScope)        //No nested scopes
		retError(clean, Error_invalidOperation(
			0, "CommandListRef_startScope() scope is already present. Nested scopes are unsupported"));

	gotoIfError3(clean, ListTransitionInternal_clear(&commandList->pendingTransitions, e_rr));
	gotoIfError3(clean, ListTransitionInternal_reserve(&commandList->pendingTransitions, transitions.length, alloc, e_rr));

	for(U64 i = 0; i < transitions.length; ++i) {

		Transition transition = transitions.ptr[i];
		RefPtr *res = transition.resource;

		if(!res)
			retError(clean, Error_nullPointer(1, "CommandListRef_startScope()::transitions[i].resource is NULL"));

		UnifiedTexture tex = TextureRef_getUnifiedTexture(res, NULL);
		Bool isSampler = res->refPtrType->typeId == (ETypeId) EGraphicsTypeId_Sampler;

		GraphicsResource resource = tex.resource;

		if (tex.resource.device)
			resource = tex.resource;

		else if (res->refPtrType->typeId == (ETypeId) EGraphicsTypeId_DeviceBuffer)
			resource = DeviceBufferRef_ptr(res)->resource;

		else if(isSampler)
			resource = (GraphicsResource) { .device = SamplerRef_ptr(res)->device };        //Only device is required here

		//Get device and mark as readonly

		else if (res->refPtrType->typeId == (ETypeId) EGraphicsTypeId_TLASExt) {

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

		else if (res->refPtrType->typeId == (ETypeId) EGraphicsTypeId_BLASExt)
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
				0, "CommandListRef_startScope()::transitions[i].resource's device is incompatible"));

		TransitionInternal *found = NULL;
		if(CommandListRef_isBound(commandList, res, transition.range, &found)) {

			if(found->type != transitionDst.type)
				retError(clean, Error_invalidOperation(
					0, "CommandListRef_startScope()::transitions[i].resource is already transitioned"));

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

	for (U64 j = 0; j < deps.length; ++j) {

		CommandScopeDependency dep = deps.ptr[j];

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
			0, "CommandListRef_endScope() can't close scope while debugRegion is still active"));

	if(!I32x2_eq2(commandList->currentSize, I32x2_zero))
		retError(clean, Error_invalidOperation(1, "CommandListRef_endScope() can't close scope while render is active"));

	//Push command, transitions and scope

	if((commandList->transitions.length + commandList->pendingTransitions.length) >> 32)
		retError(clean, Error_outOfBounds(
			0, commandList->transitions.length + commandList->pendingTransitions.length, U32_MAX,
			"CommandListRef_endScope() transitionCount of command list can't exceed U32_MAX"));

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

Bool CommandListRef_setPipeline(CommandListRef *commandListRef, PipelineRef *pipelineRef, EPipelineType type, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = commandListRef ? GraphicsDeviceRef_getAlloc(CommandListRef_ptr(commandListRef)->device) : NULL;

	CommandListRef_validateScope(commandListRef, clean)

	if (!pipelineRef || pipelineRef->refPtrType->typeId != (ETypeId) EGraphicsTypeId_Pipeline)
		retError(clean, Error_nullPointer(1, "CommandListRef_setPipeline()::pipelineRef is required"));

	const Pipeline *pipeline = PipelineRef_ptr(pipelineRef);

	if(pipeline->device != commandList->device)
		retError(clean, Error_unsupportedOperation(
			0, "CommandListRef_setPipeline()::pipelineRef is owned by different device"));

	if(pipeline->type != type)
		retError(clean, Error_unsupportedOperation(
			1, "CommandListRef_setPipeline()::pipeline's type is incompatible with type"));

	ECommandOp op = ECommandOp_SetComputePipeline;

	if(type == EPipelineType_Graphics)
		op = ECommandOp_SetGraphicsPipeline;

	else if(type == EPipelineType_RaytracingExt)
		op = ECommandOp_SetRaytracingPipelineExt;

	PipelineRef *commandOp[2] = { pipelineRef, NULL };        //Padding to 16-byte

	gotoIfError3(clean, CommandList_append(
		commandList, op, Buffer_createRefConst(commandOp, sizeof(commandOp)), 0, e_rr));

	if(!ListRefPtr_contains(commandList->resources, pipelineRef, 0, NULL)) {                        //TODO: hashSet
		RefPtr_inc(pipelineRef);        //CommandList will keep resource alive.
		gotoIfError3(clean, ListRefPtr_pushBack(&commandList->resources, pipelineRef, alloc, e_rr));
	}

	commandList->pipeline[type] = pipelineRef;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_setComputePipeline(CommandListRef *commandList, PipelineRef *pipeline, Error *e_rr) {
	return CommandListRef_setPipeline(commandList, pipeline, EPipelineType_Compute, e_rr);
}

Bool CommandListRef_setGraphicsPipeline(CommandListRef *commandList, PipelineRef *pipeline, Error *e_rr) {
	return CommandListRef_setPipeline(commandList, pipeline, EPipelineType_Graphics, e_rr);
}

Bool CommandListRef_setRaytracingPipeline(CommandListRef *commandList, PipelineRef *pipeline, Error *e_rr) {
	return CommandListRef_setPipeline(commandList, pipeline, EPipelineType_RaytracingExt, e_rr);
}

Bool CommandListRef_validateBufferDesc(
	GraphicsDeviceRef *device,
	DeviceBufferRef *buffer,
	EDeviceBufferUsage usage,
	U64 maxSize,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!buffer)
		return s_uccess;

	if(buffer->refPtrType->typeId != (ETypeId) EGraphicsTypeId_DeviceBuffer)
		retError(clean, Error_unsupportedOperation(0, "CommandListRef_validateBufferDesc()::buffer has invalid type"));

	DeviceBuffer *buf = DeviceBufferRef_ptr(buffer);

	if(buf->resource.device != device)
		retError(clean, Error_unsupportedOperation(
			1, "CommandListRef_validateBufferDesc()::buffer is owned by different device"
		));

	if((buf->usage & usage) != usage)
		retError(clean, Error_unsupportedOperation(
			2, "CommandListRef_validateBufferDesc()::buffer is missing required usage flag"
		));

	if(buf->resource.size > maxSize)
		retError(clean, Error_outOfBounds(
			1, buf->resource.size, maxSize,
			"CommandListRef_validateBufferDesc()::buffer is bigger than max limit"
		));

clean:
	return s_uccess;
}

Bool CommandListRef_setPrimitiveBuffers(CommandListRef *commandListRef, SetPrimitiveBuffersCmd buffers, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	if(I32x2_any(I32x2_eq(commandList->currentSize, I32x2_zero)))
		retError(clean, Error_invalidOperation(
			0, "CommandListRef_setPrimitiveBuffers() is only available if render is started"));

	//Validate index and vertex buffers

	GraphicsDeviceRef *device = commandList->device;
	gotoIfError3(clean, CommandListRef_validateBufferDesc(
		device,
		buffers.indexBuffer,
		EDeviceBufferUsage_Index,
		U32_MAX,
		e_rr
	));

	if(!s_uccess)
		return s_uccess;

	for(U8 i = 0; i < 8; ++i)
		gotoIfError3(clean, CommandListRef_validateBufferDesc(
			device, buffers.vertexBuffers[i], EDeviceBufferUsage_Vertex, U32_MAX, e_rr));

	//Transition

	gotoIfError3(clean, CommandListRef_transitionBuffer(
		commandList, buffers.indexBuffer, (BufferRange) { 0 }, ETransitionType_Index, EPipelineStage_Count, e_rr));

	for(U8 i = 0; i < 8; ++i)
		gotoIfError3(clean, CommandListRef_transitionBuffer(
			commandList, buffers.vertexBuffers[i], (BufferRange) { 0 }, ETransitionType_Vertex, EPipelineStage_Count, e_rr));

	//Issue command

	gotoIfError3(clean, CommandList_append(
		commandList, ECommandOp_SetPrimitiveBuffers, Buffer_createRefConst(&buffers, sizeof(buffers)), 0, e_rr));

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_drawBase(CommandListRef *commandListRef, Buffer buf, ECommandOp op, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	if(I32x2_any(I32x2_eq(commandList->currentSize, I32x2_zero)))
		retError(clean, Error_invalidOperation(0, "CommandListRef_drawBase() is only available if render is started"));

	PipelineRef *pipelineRef = commandList->pipeline[EPipelineType_Graphics];

	if (!pipelineRef)
		retError(clean, Error_invalidOperation(1, "CommandListRef_drawBase() requires bound graphics pipeline"));

	U32 flags = ECommandStateFlags_AnyScissor | ECommandStateFlags_AnyViewport;

	if ((commandList->tempStateFlags & flags) != flags)
		retError(clean, Error_invalidOperation(2, "CommandListRef_drawBase() requires viewport and scissor"));

	gotoIfError3(clean, CommandList_validateGraphicsPipeline(
		PipelineRef_ptr(pipelineRef),
		commandList->boundImages,
		commandList->boundImageCount,
		commandList->boundDepthFormat,
		commandList->boundSampleCount, e_rr));

	gotoIfError3(clean, CommandList_append(commandList, op, buf, 1, e_rr));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_draw(CommandListRef *commandListRef, DrawCmd draw, Error *e_rr) {

	Bool s_uccess = true;

	if(!draw.count || !draw.instanceCount)        //No-op
		return s_uccess;

	if(draw.vertexOffset >> 31)
		retError(clean, Error_outOfBounds(
			1, draw.vertexOffset, (U32)I32_MAX, "CommandListRef_draw() vertexOffset out of bounds"
		));

	gotoIfError3(clean, CommandListRef_drawBase(
		commandListRef,
		Buffer_createRefConst(&draw, sizeof(draw)),
		ECommandOp_Draw,
		e_rr
	));

clean:
	return s_uccess;
}

Bool CommandListRef_drawIndexed(CommandListRef *commandList, U32 indexCount, U32 instanceCount, Error *e_rr) {
	const DrawCmd draw = (DrawCmd) { .count = indexCount, .instanceCount = instanceCount, .isIndexed = true };
	return CommandListRef_draw(commandList, draw, e_rr);
}

Bool CommandListRef_drawIndexedAdv(
	CommandListRef *commandList,
	U32 indexCount, U32 instanceCount,
	U32 indexOffset, U32 instanceOffset,
	U32 vertexOffset,
	Error *e_rr
) {
	const DrawCmd draw = (DrawCmd) {
		.count = indexCount,
		.instanceCount = instanceCount,
		.indexOffset = indexOffset,
		.instanceOffset = instanceOffset,
		.vertexOffset = vertexOffset,
		.isIndexed = true
	};

	return CommandListRef_draw(commandList, draw, e_rr);
}

Bool CommandListRef_drawUnindexed(CommandListRef *commandList, U32 vertexCount, U32 instanceCount, Error *e_rr) {
	const DrawCmd draw = (DrawCmd) { .count = vertexCount, .instanceCount = instanceCount };
	return CommandListRef_draw(commandList, draw, e_rr);
}

Bool CommandListRef_drawUnindexedAdv(
	CommandListRef *commandList,
	U32 vertexCount, U32 instanceCount,
	U32 vertexOffset, U32 instanceOffset,
	Error *e_rr
) {
	const DrawCmd draw = (DrawCmd) {
		.count = vertexCount,
		.instanceCount = instanceCount,
		.vertexOffset = vertexOffset,
		.instanceOffset = instanceOffset
	};

	return CommandListRef_draw(commandList, draw, e_rr);
}

Bool CommandListRef_dispatch(CommandListRef *commandListRef, DispatchCmd dispatch, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	if (!commandList->pipeline[EPipelineType_Compute])
		retError(clean, Error_invalidOperation(1, "CommandListRef_dispatch() requires bound compute pipeline"));

	const U64 groupCountMax = U64_max(dispatch.groups[0], U64_max(dispatch.groups[1], dispatch.groups[2]));

	if(groupCountMax > U16_MAX)
		retError(clean, Error_outOfBounds(
			1, groupCountMax, U16_MAX, "CommandListRef_dispatch() groupCountMax out of bounds"));

	gotoIfError3(clean, CommandList_append(
		commandList, ECommandOp_Dispatch, Buffer_createRefConst(&dispatch, sizeof(dispatch)), 0, e_rr));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_dispatch1D(CommandListRef *commandList, U32 groupsX, Error *e_rr) {
	const DispatchCmd dispatch = (DispatchCmd) { .groups = { groupsX, 1, 1 } };
	return CommandListRef_dispatch(commandList, dispatch, e_rr);
}

Bool CommandListRef_dispatch2D(CommandListRef *commandList, U32 groupsX, U32 groupsY, Error *e_rr) {
	const DispatchCmd dispatch = (DispatchCmd) { .groups = { groupsX, groupsY, 1 } };
	return CommandListRef_dispatch(commandList, dispatch, e_rr);
}

Bool CommandListRef_dispatch3D(CommandListRef *commandList, U32 groupsX, U32 groupsY, U32 groupsZ, Error *e_rr) {
	const DispatchCmd dispatch = (DispatchCmd) { .groups = { groupsX, groupsY, groupsZ } };
	return CommandListRef_dispatch(commandList, dispatch, e_rr);
}

//Dispatch rays

Bool CommandListRef_dispatchRaysExt(CommandListRef *commandListRef, DispatchRaysExt dispatch, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	PipelineRef *rayPipeline = commandList->pipeline[EPipelineType_RaytracingExt];

	if (!rayPipeline)
		retError(clean, Error_invalidOperation(1, "CommandListRef_dispatchRaysExt() requires bound raytracing pipeline"));

	U64 total = dispatch.x * dispatch.y;

	if(total < dispatch.x || total * dispatch.z < total)
		retError(clean, Error_invalidOperation(1, "CommandListRef_dispatchRaysExt() overflow"));

	total *= dispatch.z;

	if(total > 1 * GIBI)
		retError(clean, Error_invalidOperation(1, "CommandListRef_dispatchRaysExt() is limited to 1Gibi rays"));

	if(!total)
		retError(clean, Error_invalidOperation(1, "CommandListRef_dispatchRaysExt() can't be executed with 0 count"));

	if(dispatch.raygenId >= Pipeline_info(PipelineRef_ptr(rayPipeline), PipelineRaytracingInfo)->raygenCount)
		retError(clean, Error_invalidOperation(1, "CommandListRef_dispatchRaysExt() raygen index out of bounds"));

	gotoIfError3(clean, CommandList_append(
		commandList, ECommandOp_DispatchRaysExt, Buffer_createRefConst(&dispatch, sizeof(dispatch)), 0, e_rr));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_dispatch1DRaysExt(CommandListRef *commandList, U32 raygenLocalId, U32 groupsX, Error *e_rr) {
	return CommandListRef_dispatchRaysExt(commandList, (DispatchRaysExt) { groupsX, 1, 1, raygenLocalId }, e_rr);
}

Bool CommandListRef_dispatch2DRaysExt(CommandListRef *commandList, U32 raygenLocalId, U32 groupsX, U32 groupsY, Error *e_rr) {
	return CommandListRef_dispatchRaysExt(commandList, (DispatchRaysExt) { groupsX, groupsY, 1, raygenLocalId }, e_rr);
}

Bool CommandListRef_dispatch3DRaysExt(
	CommandListRef *commandList,
	U32 raygenLocalId,
	U32 groupsX,
	U32 groupsY,
	U32 groupsZ,
	Error *e_rr
) {
	return CommandListRef_dispatchRaysExt(commandList, (DispatchRaysExt) { groupsX, groupsY, groupsZ, raygenLocalId }, e_rr);
}

//Indirect rendering

Bool CommandListRef_checkDispatchBuffer(GraphicsDeviceRef *device, DeviceBufferRef *buffer, U64 offset, U64 siz, Error *e_rr) {

	Bool s_uccess = true;

	if(!buffer || buffer->refPtrType->typeId != (ETypeId) EGraphicsTypeId_DeviceBuffer)
		retError(clean, Error_nullPointer(1, "CommandListRef_checkDispatchBuffer()::buffer is required"));

	const DeviceBuffer *buf = DeviceBufferRef_ptr(buffer);

	if(buf->resource.device != device)
		retError(clean, Error_unsupportedOperation(
			3,
			"CommandListRef_checkDispatchBuffer()::buffer is owned by different device"
		));

	if(offset + siz > buf->resource.size)
		retError(clean, Error_outOfBounds(
			1, offset + siz, buf->resource.size, "CommandListRef_checkDispatchBuffer()::offset + size is out of bounds"
		));

	if(!(buf->usage & EDeviceBufferUsage_Indirect))
		retError(clean, Error_unsupportedOperation(
			0,
			"CommandListRef_checkDispatchBuffer()::buffer requires indirect buffer usage"
		));

clean:
	return s_uccess;
}

Bool CommandListRef_dispatchIndirect(CommandListRef *commandListRef, DeviceBufferRef *buffer, U64 offset, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean);
	GraphicsDeviceRef *device = commandList->device;

	if (!commandList->pipeline[EPipelineType_Compute])
		retError(clean, Error_invalidOperation(1, "CommandListRef_dispatchIndirect() requires bound compute pipeline"));

	if(offset & 15)
		retError(clean, Error_invalidParameter(
			2, 0, "CommandListRef_dispatchIndirect()::offset has to be 16-byte aligned"));

	gotoIfError3(clean, CommandListRef_checkDispatchBuffer(device, buffer, offset, sizeof(U32) * 4, e_rr));

	const BufferRange range = (BufferRange) { .startRange = offset, .endRange = offset + sizeof(U32) * 4 };
	gotoIfError3(clean, CommandListRef_transitionBuffer(
		commandList, buffer, range, ETransitionType_Indirect, EPipelineStage_Count, e_rr));

	const DispatchIndirectCmd dispatch = (DispatchIndirectCmd) { .buffer = buffer, .offset = offset };

	gotoIfError3(clean, CommandList_append(
		commandList, ECommandOp_DispatchIndirect, Buffer_createRefConst(&dispatch, sizeof(dispatch)), 0, e_rr));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandList_drawIndirectBase(
	CommandList *commandList,
	DeviceBufferRef *buffer,
	U64 bufferOffset,
	U32 drawCalls,
	Bool indexed,
	Error *e_rr
) {

	Bool s_uccess = true;

	const U32 bufferStride = (U32)(indexed ? sizeof(DrawCallIndexed) : sizeof(DrawCallUnindexed));
	const DeviceBuffer *buf = DeviceBufferRef_ptr(buffer);

	if(!buf)
		retError(clean, Error_nullPointer(0, "CommandList_drawIndirectBase()::buffer is required"));

	if(bufferOffset & 15)
		retError(clean, Error_invalidParameter(
			2, 0, "CommandList_drawIndirectBase()::offset has to be 16-byte aligned"
		));

	if (!drawCalls)
		retError(clean, Error_invalidParameter(
			2, 2, "CommandList_drawIndirectBase() shouldn't be submitted if drawCalls is 0"
		));

	gotoIfError3(clean, CommandListRef_checkDispatchBuffer(
		commandList->device, buffer, bufferOffset, (U64)bufferStride * drawCalls, e_rr));

	const BufferRange range = (BufferRange) {
		.startRange = bufferOffset,
		.endRange = bufferOffset + bufferStride * drawCalls
	};

	gotoIfError3(clean, CommandListRef_transitionBuffer(
		commandList,
		buffer,
		range,
		ETransitionType_Indirect,
		EPipelineStage_Count,
		e_rr
	));

clean:
	return s_uccess;
}

Bool CommandListRef_drawIndirect(
	CommandListRef *commandListRef,
	DeviceBufferRef *buffer,
	U64 bufferOffset,
	U32 drawCalls,
	Bool indexed,
	Error *e_rr
) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	gotoIfError3(clean, CommandList_drawIndirectBase(commandList, buffer, bufferOffset, drawCalls, indexed, e_rr));

	DrawIndirectCmd draw = (DrawIndirectCmd) {
		.buffer = buffer,
		.bufferOffset = bufferOffset,
		.drawCalls = drawCalls,
		.isIndexed = indexed
	};

	gotoIfError3(clean, CommandListRef_drawBase(
		commandListRef, Buffer_createRefConst(&draw, sizeof(draw)), ECommandOp_DrawIndirect, e_rr));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_drawIndirectCountExt(
	CommandListRef *commandListRef,
	DeviceBufferRef *buffer,
	U64 bufferOffset,
	DeviceBufferRef *countBuffer,
	U64 countOffset,
	U32 maxDrawCalls,
	Bool indexed,
	Error *e_rr
) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);
	gotoIfError3(clean, CommandList_drawIndirectBase(commandList, buffer, bufferOffset, maxDrawCalls, indexed, e_rr));

	if(!(device->info.capabilities.features & EGraphicsFeatures_MultiDrawIndirectCount))
		retError(clean, Error_unsupportedOperation(
			0, "CommandListRef_drawIndirectCountExt() requires multiDrawIndirectCount extension, which was missing!"));

	gotoIfError3(clean, CommandListRef_checkDispatchBuffer(commandList->device, countBuffer, countOffset, sizeof(U32), e_rr));

	BufferRange range = (BufferRange) {
		.startRange = countOffset,
		.endRange = countOffset + sizeof(U32)
	};

	gotoIfError3(clean, CommandListRef_transitionBuffer(
		commandList, countBuffer, range, ETransitionType_Indirect, EPipelineStage_Count, e_rr));

	DrawIndirectCmd draw = (DrawIndirectCmd) {
		.buffer = buffer,
		.countBufferExt = countBuffer,
		.bufferOffset = bufferOffset,
		.countOffsetExt = countOffset,
		.drawCalls = maxDrawCalls,
		.isIndexed = indexed
	};

	gotoIfError3(clean, CommandListRef_drawBase(
		commandListRef, Buffer_createRefConst(&draw, sizeof(draw)), ECommandOp_DrawIndirectCount, e_rr));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

//Feature RaytracingExt

Bool CommandListRef_updateRTASExt(CommandListRef *commandListRef, RTASRef *rtas, Bool isBLAS, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	if(!I32x2_all(I32x2_eq(commandList->currentSize, I32x2_zero)))
		retError(clean, Error_invalidOperation(
			0,
			"CommandListRef_updateRTASExt() is disallowed during render calls, as flushing might cause invalid render state"
		));

	if(!rtas || rtas->refPtrType->typeId != (ETypeId)(isBLAS ? EGraphicsTypeId_BLASExt : EGraphicsTypeId_TLASExt))
		retError(clean, Error_unsupportedOperation(0, "CommandListRef_updateRTASExt() requires BLAS or TLAS"));

	gotoIfError3(clean, CommandListRef_transitionRTAS(
		commandList,
		rtas,
		ETransitionType_ShaderWrite,
		EPipelineStage_RTASBuild,
		e_rr
	));

	if (!isBLAS) {

		TLAS *tlas = TLASRef_ptr(rtas);

		if(!tlas->useDeviceMemory)
			for (U64 j = 0; j < tlas->cpuInstancesStatic.length; ++j) {

				TLASInstanceData dat = (TLASInstanceData) { 0 };
				TLAS_getInstanceDataCpu(tlas, j, &dat);

				if (!dat.blasCpu)
					continue;

				gotoIfError3(clean, CommandListRef_transitionRTAS(
					commandList, dat.blasCpu,
					ETransitionType_ShaderRead, EPipelineStage_RTASBuild, e_rr));
			}
	}

	RTASRef *args[2] = { rtas, NULL };

	gotoIfError3(clean, CommandList_append(
		commandList,
		isBLAS ? ECommandOp_UpdateBLASExt : ECommandOp_UpdateTLASExt,
		Buffer_createRefConst(args, sizeof(args)),
		0, e_rr));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_updateTLASExt(CommandListRef *commandList, TLASRef *tlas, Error *e_rr) {
	return CommandListRef_updateRTASExt(commandList, tlas, false, e_rr);
}

Bool CommandListRef_updateBLASExt(CommandListRef *commandList, BLASRef *blas, Error *e_rr) {
	return CommandListRef_updateRTASExt(commandList, blas, true, e_rr);
}

//Dynamic rendering

Bool CommandListRef_startRenderExt(
	CommandListRef *commandListRef,
	I32x2 offset,
	I32x2 size,
	ListAttachmentInfo colors,
	DepthStencilAttachmentInfo depthStencil,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = commandListRef ? GraphicsDeviceRef_getAlloc(CommandListRef_ptr(commandListRef)->device) : NULL;

	Buffer command = Buffer_createNull();
	SpinLock *toRelease = NULL;

	CommandListRef_validateScope(commandListRef, clean)

	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);

	if(!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering))
		retError(clean, Error_unsupportedOperation(
			0, "CommandListRef_startRenderExt() requires directRendering extension, which was missing!"));

	if(!colors.length && !depthStencil.image)
		retError(clean, Error_invalidOperation(
			1, "CommandListRef_startRenderExt() requires DepthStencil and/or colors"));

	if(colors.length > 8)
		retError(clean, Error_outOfBounds(3, colors.length, 8, "CommandListRef_startRenderExt()::colors has to be <=8"));

	I32x2 targetSize = size;
	I32x2 firstSize = I32x2_zero;
	U8 counter = 0;
	EMSAASamples sampleCount = 0;

	for (U64 i = 0; i < colors.length + !!depthStencil.image; ++i) {

		Bool isDepthStencil = i == colors.length;
		AttachmentInfo info;

		if(isDepthStencil)
			info = (AttachmentInfo) {
				.range = depthStencil.range,
				.image = depthStencil.image,
				.resolveImage = depthStencil.resolveImage,
				.resolveRange = depthStencil.resolveImageRange,
				.resolveMode = depthStencil.depthStencilResolve
			};

		else info = colors.ptr[i];

		if(!info.image)
			continue;

		++counter;

		DeviceResourceVersion version;
		UnifiedTexture texture = TextureRef_getUnifiedTexture(info.image, &version);

		//Swapchain needs to maintain version, so CommandList can be invalidated on resize

		if(version.resource && !ListDeviceResourceVersion_contains(commandList->activeSwapchains, version, 0, NULL))
			gotoIfError3(clean, ListDeviceResourceVersion_pushBack(&commandList->activeSwapchains, version, alloc, e_rr));

		//TODO: Properly validate this

		if(info.range.levelId >= 1 || info.range.layerId >= 1)
			retError(clean, Error_outOfBounds(
				4, info.range.levelId >= 1 ? info.range.levelId : info.range.layerId, 1,
				"CommandListRef_startRenderExt() image range.levelId or layerId is invalid"));

		if(info.readOnly && info.load == ELoadAttachmentType_Clear)
			retError(clean, Error_unsupportedOperation(
				7, "CommandListRef_startRenderExt() render target is set as clear but also read only"));

		//Check generic properties like devices

		if(texture.resource.device != commandList->device)
			retError(clean, Error_unsupportedOperation(
				4, "CommandListRef_startRenderExt() image belongs to different device"));

		if(texture.type != ETextureType_2D)
			retError(clean, Error_invalidParameter(
				3, (U32)i, "CommandListRef_startRenderExt() image needs to be a 2D texture"));

		I32x2 currSize = I32x2_create2(texture.width, texture.height);

		if(counter == 1) {

			firstSize = currSize;

			if(I32x2_any(I32x2_geq(offset, firstSize)))
				retError(clean, Error_invalidState(
					0, "CommandListRef_startRenderExt() image offset was out of bounds"));

			targetSize = I32x2_sub(firstSize, offset);
		}

		else if(I32x2_any(I32x2_lt(currSize, I32x2_add(targetSize, offset))))
			retError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt() image dimensions are incompatible with others"));

		if(TextureRef_isDepthStencil(info.image) != isDepthStencil)
			retError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt() image had mismatching image types (Color/Depth)"));

		//Validate MSAA

		if(info.resolveMode >= EMSAAResolveMode_Count)
			retError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt() image had invalid resolveMode"));

		if(info.resolveMode && !info.resolveImage)
			retError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt() image had resolveMode but no resolveImage"));

		if(counter == 1)
			sampleCount = texture.sampleCount;

		else if(sampleCount != texture.sampleCount)
			retError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt() image had mismatching MSAA setting between others"));

		if(info.resolveImage) {

			//TODO: Properly validate this

			if(info.resolveRange.levelId >= 1 || info.resolveRange.layerId >= 1)
				retError(clean, Error_outOfBounds(
					4, info.resolveRange.levelId >= 1 ? info.resolveRange.levelId : info.resolveRange.layerId, 1,
					"CommandListRef_startRenderExt() image range.levelId or layerId is invalid"));

			if(!texture.sampleCount)
				retError(clean, Error_invalidOperation(
					3, "CommandListRef_startRenderExt() image had resolveImage, while MSAA was off"));

			if(TextureRef_isDepthStencil(info.image) != TextureRef_isDepthStencil(info.resolveImage))
				retError(clean, Error_invalidOperation(
					3,
					"CommandListRef_startRenderExt() image had resolveImage which didn't match the same type (Color/Depth)"
				));

			DeviceResourceVersion resolveVersion;
			UnifiedTexture resolveTexture = TextureRef_getUnifiedTexture(info.resolveImage, &resolveVersion);

			if (texture.depthFormat) {
				if(texture.depthFormat != resolveTexture.depthFormat)
					retError(clean, Error_invalidOperation(
						3, "CommandListRef_startRenderExt() MSAA resolve image of depth buffer needs compatible depth format"));
			}

			else if(texture.textureFormatId != resolveTexture.textureFormatId)
				retError(clean, Error_invalidOperation(
					3, "CommandListRef_startRenderExt() image and resolveImage have mismatching formats"));

			I32x2 resolveSize = I32x2_create2(resolveTexture.width, resolveTexture.height);

			if(I32x2_neq2(resolveSize, targetSize))
				retError(clean, Error_invalidOperation(
					3, "CommandListRef_startRenderExt() size of MSAA resolve image is incompatible"));
		}
	}

	if(!counter)
		retError(clean, Error_invalidParameter(
			3, 1, "CommandListRef_startRenderExt() didn't provide any render targets"));

	if(I32x2_any(I32x2_or(I32x2_leq(targetSize, I32x2_zero), I32x2_leq(firstSize, I32x2_zero))))
		retError(clean, Error_invalidOperation(5, "CommandListRef_startRenderExt() targetSize or firstSize is <=0"));

	if(I32x2_any(I32x2_eq(size, I32x2_zero)))
		size = I32x2_sub(targetSize, offset);

	gotoIfError3(clean, CommandListRef_checkBounds(offset, size, 0, 32767, e_rr));
	gotoIfError3(clean, Buffer_createEmptyBytes(
		sizeof(StartRenderCmdExt) + sizeof(AttachmentInfoInternal) * 16,
		alloc,
		&command,
		e_rr
	));

	if(!I32x2_all(I32x2_eq(commandList->currentSize, I32x2_zero)))
		retError(clean, Error_invalidOperation(
			2, "CommandListRef_startRenderExt() can't already have a render started!"));

	if(!depthStencil.image && depthStencil.clearStencil)
		retError(clean, Error_invalidOperation(
			5, "CommandListRef_startRenderExt()::stencil clear value can't be non zero if there's no stencil bound"));

	if(depthStencil.clearDepth < 0 || depthStencil.clearDepth > 1)
		retError(clean, Error_invalidOperation(4, "CommandListRef_startRenderExt()::depth clear should be 0-1"));

	if(!depthStencil.image && depthStencil.clearDepth)
		retError(clean, Error_invalidOperation(
			4, "CommandListRef_startRenderExt()::depth clear value can't be non zero if there's no depth buffer bound"));

	StartRenderCmdExt *startRender = (StartRenderCmdExt*)command.ptr;

	*startRender = (StartRenderCmdExt) {
		.offset = offset,
		.size = size,
		.resolveDepthStencilMode = depthStencil.depthStencilResolve,
		.colorCount = (U8) colors.length,
		.clearStencil = (U8) depthStencil.clearStencil,
		.clearDepth = depthStencil.clearDepth,
		.depthStencilRange = depthStencil.range,
		.depthStencil = depthStencil.image,
		.resolveDepthStencil = depthStencil.resolveImage,
		.resolveDepthStencilRange = depthStencil.resolveImageRange
	};

	UnifiedTexture depthStencilImg = TextureRef_getUnifiedTexture(depthStencil.image, NULL);

	if(depthStencilImg.depthFormat != EDepthStencilFormat_S8X24Ext)
		startRender->flags |= EStartRenderFlags_Depth;

	if(depthStencil.depthLoad == ELoadAttachmentType_Clear)
		startRender->flags |= EStartRenderFlags_ClearDepth;

	else if(depthStencil.depthLoad == ELoadAttachmentType_Preserve)
		startRender->flags |= EStartRenderFlags_PreserveDepth;

	if(depthStencil.depthUnusedAfterRender)
		startRender->flags |= EStartRenderFlags_DepthUnusedAfterRender;

	if(depthStencil.depthReadOnly)
		startRender->flags |= EStartRenderFlags_DepthReadOnly;

	if(depthStencilImg.depthFormat >= EDepthStencilFormat_StencilStart)
		startRender->flags |= EStartRenderFlags_Stencil;

	if(depthStencil.stencilLoad == ELoadAttachmentType_Clear)
		startRender->flags |= EStartRenderFlags_ClearStencil;

	else if(depthStencil.stencilLoad == ELoadAttachmentType_Preserve)
		startRender->flags |= EStartRenderFlags_PreserveStencil;

	if(depthStencil.stencilUnusedAfterRender)
		startRender->flags |= EStartRenderFlags_StencilUnusedAfterRender;

	if(depthStencil.stencilReadOnly)
		startRender->flags |= EStartRenderFlags_StencilReadOnly;

	if(depthStencil.stencilReadOnly && depthStencil.stencilLoad == ELoadAttachmentType_Clear)
		retError(clean, Error_invalidOperation(
			6, "CommandListRef_startRenderExt()::stencil was set to clear but was readonly"));

	if(depthStencil.depthReadOnly && depthStencil.depthLoad == ELoadAttachmentType_Clear)
		retError(clean, Error_invalidOperation(
			6, "CommandListRef_startRenderExt()::depth was set to clear but was readonly"));

	if(!depthStencil.image && (depthStencil.range.layerId || depthStencil.range.levelId || startRender->flags))
		retError(clean, Error_invalidOperation(
			5, "CommandListRef_startRenderExt()::depthStencil had values set, but didn't have a valid image"));

	AttachmentInfoInternal *attachments = (AttachmentInfoInternal*)(startRender + 1);
	counter = 0;

	for (U64 i = 0; i < colors.length + !!depthStencil.image; ++i) {

		AttachmentInfo info;

		if(i == colors.length)
			info = (AttachmentInfo) {
				.range = depthStencil.range,
				.image = depthStencil.image,
				.resolveImage = depthStencil.resolveImage,
				.resolveRange = depthStencil.resolveImageRange
			};

		else info = colors.ptr[i];

		if (!info.image)
			continue;

		if(i < colors.length) {

			startRender->activeMask |= (U8)1 << i;

			if(info.unusedAfterRender)
				startRender->unusedAfterRenderMask |= (U8)1 << i;

			if(info.readOnly)
				startRender->readOnlyMask |= (U8)1 << i;

			if(info.load == ELoadAttachmentType_Clear)
				startRender->clearMask |= (U8)1 << i;

			else if(info.load == ELoadAttachmentType_Preserve)
				startRender->preserveMask |= (U8)1 << i;

			attachments[counter++] = (AttachmentInfoInternal) {
				.color = info.color,
				.image = info.image,
				.range = info.range,
				.resolveImage = info.resolveImage,
				.resolveMode = info.resolveMode
			};
		}

		//Transition image

		TransitionInternal transition = (TransitionInternal) {
			.resource = info.image,
			.range = (ResourceRange) { .image = info.range },
			.stage = EPipelineStage_Count,
			.type = info.readOnly ? ETransitionType_RenderTargetRead : ETransitionType_RenderTargetWrite
		};

		TransitionInternal *state = NULL;
		if(CommandListRef_isBound(commandList, transition.resource, transition.range, &state)) {

			//Depth stencil is allowed to transition twice as Depth & Stencil.
			//However, you're not allowed to use an RTV twice.

			if(i < colors.length || state->type != transition.type)
				retError(clean, Error_invalidOperation(
					4, "CommandListRef_startRenderExt()::colors[i] or depthStencil was already transitioned!"));
		}

		else {
			gotoIfError3(clean, ListTransitionInternal_pushBack(&commandList->pendingTransitions, transition, alloc, e_rr));
		}

		//Transition resolve image

		if(info.resolveImage) {

			transition.resource = info.resolveImage;
			transition.range = (ResourceRange) { 0 };                //TODO: Range for resolveImage
			transition.type = ETransitionType_ResolveTargetWrite;

			if(CommandListRef_isBound(commandList, transition.resource, transition.range, &state)) {

				if(state->type != transition.type)
					retError(clean, Error_invalidOperation(
						4,
						"CommandListRef_startRenderExt()::colors[i] or depthStencil resolve target was already resolved"));
			}

			else {
				gotoIfError3(clean, ListTransitionInternal_pushBack(&commandList->pendingTransitions, transition, alloc, e_rr));
			}
		}
	}

	gotoIfError3(clean, CommandList_append(
		commandList,
		ECommandOp_StartRenderingExt,
		Buffer_createRefConst(startRender, sizeof(StartRenderCmdExt) + sizeof(AttachmentInfoInternal) * counter),
		0, e_rr));

	commandList->currentSize = size;
	commandList->boundImageCount = (U8) colors.length;

	for (U8 i = 0; i < commandList->boundImageCount; ++i)
		commandList->boundImages[i] = (ImageAndRange) { .image = colors.ptr[i].image, .range = colors.ptr[i].range };

	//Combine stencil and depth back to one format

	EDepthStencilFormat depthFormat = EDepthStencilFormat_None;

	if(depthStencil.image)
		depthFormat = TextureRef_getUnifiedTexture(depthStencil.image, NULL).depthFormat;

	commandList->boundDepthFormat = depthFormat;
	commandList->boundSampleCount = sampleCount;

clean:

	if(toRelease)
		SpinLock_unlock(toRelease);

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	Buffer_free(&command, alloc);
	return s_uccess;
}

Bool CommandListRef_endRenderExt(CommandListRef *commandListRef, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)
	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);

	if(!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering))
		retError(clean, Error_unsupportedOperation(
			0, "CommandListRef_endRenderExt() requires directRendering extension, which was missing!"));

	if(I32x2_any(I32x2_eq(commandList->currentSize, I32x2_zero)))
		retError(clean, Error_invalidOperation(
			1, "CommandListRef_endRenderExt() requires startRenderExt to be called first"));

	gotoIfError3(clean, CommandList_append(commandList, ECommandOp_EndRenderingExt, Buffer_createNull(), 0, e_rr));

	commandList->currentSize = I32x2_zero;
	commandList->tempStateFlags &= ~(ECommandStateFlags_AnyScissor | ECommandStateFlags_AnyViewport);

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

//Debug markers

Bool CommandList_markerDebugExt(CommandListRef *commandListRef, F32x4 color, CharString name, ECommandOp op, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = commandListRef ? GraphicsDeviceRef_getAlloc(CommandListRef_ptr(commandListRef)->device) : NULL;

	Buffer buf = Buffer_createNull();
	CommandListRef_validateScope(commandListRef, clean)

	U64 len = sizeof(color) + CharString_length(name) + 1;
	len = (len + 15) &~ 15;                                        //Align to 16-byte to not mess up next instruction alignment

	gotoIfError3(clean, Buffer_createUninitializedBytes(len, alloc, &buf, e_rr));

	Buffer_memcpy(buf, Buffer_createRefConst(&color, sizeof(color)));

	Buffer_memcpy(
		Buffer_createRef(buf.ptrNonConst + sizeof(color), CharString_length(name)),
		CharString_bufferConst(name)
	);

	buf.ptrNonConst[sizeof(color) + CharString_length(name)] = '\0';

	gotoIfError3(clean, CommandList_append(commandList, op, buf, 1, e_rr));

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	Buffer_free(&buf, alloc);
	return s_uccess;
}

Bool CommandListRef_addMarkerDebugExt(CommandListRef *commandListRef, F32x4 color, CharString name, Error *e_rr) {
	return CommandList_markerDebugExt(commandListRef, color, name, ECommandOp_AddMarkerDebugExt, e_rr);
}

Bool CommandListRef_startRegionDebugExt(CommandListRef *commandListRef, F32x4 color, CharString name, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	if(!CharString_length(name))
		retError(clean, Error_nullPointer(2, "CommandListRef_startRegionDebugExt()::name is required"));

	if(commandList && commandList->debugRegionStack == U8_MAX)
		retError(clean, Error_outOfBounds(
			0, U8_MAX, U8_MAX, "CommandListRef_startRegionDebugExt() can only have depth of 255."));

	gotoIfError3(clean, CommandList_markerDebugExt(commandListRef, color, name, ECommandOp_StartRegionDebugExt, e_rr));

	++commandList->debugRegionStack;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_endRegionDebugExt(CommandListRef *commandListRef, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	if (!commandList->debugRegionStack)
		retError(clean, Error_invalidOperation(1, "CommandListRef_endRegionDebugExt() requires startRegion first."));

	gotoIfError3(clean, CommandList_append(commandList, ECommandOp_EndRegionDebugExt, Buffer_createNull(), 0, e_rr));

	--commandList->debugRegionStack;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

//Free and create

void CommandList_free(CommandList *cmd, const Allocator *alloc) {

	(void)alloc;

	SpinLock_lock(&cmd->lock, U64_MAX);

	//Log_debugLnx("Destroy: CommandList %p", cmd);

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

	//Log_debugLnx("Create: CommandList %p", commandList);
	RefPtr_inc(deviceRef);
	commandList->device = deviceRef;
	commandList->allowResize = allowResize;

	goto success;

clean:
	RefPtr_dec(commandListRef);

success:
	return s_uccess;
}
