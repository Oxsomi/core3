/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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

#include "platforms/ext/listx_impl.h"
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
#include "platforms/ext/bufferx.h"
#include "platforms/log.h"
#include "platforms/ext/ref_ptrx.h"
#include "types/string.h"
#include "formats/texture.h"
#include "types/math.h"

TListImpl(CommandOpInfo);
TListImpl(TransitionInternal);
TListImpl(CommandScope);
TListImpl(Transition);
TListImpl(CommandScopeDependency);
TListImpl(ClearImageCmd);
TListImpl(AttachmentInfo);
TListImpl(CopyImageRegion);

Error CommandListRef_dec(CommandListRef **cmd) {
	return !RefPtr_dec(cmd) ? Error_invalidOperation(0, "CommandListRef_dec()::cmd invalid") : Error_none();
}

Error CommandListRef_inc(CommandListRef *cmd) {
	return !RefPtr_inc(cmd) ? Error_invalidOperation(0, "CommandListRef_inc()::cmd invalid") : Error_none();
}

//Clear, append, begin and end

#define CommandListRef_validate(v)																				\
																												\
	if(!(v) || (v)->typeId != (ETypeId)EGraphicsTypeId_CommandList)												\
		return Error_nullPointer(0, "CommandListRef_validate() cmdlist is invalid");							\
																												\
	CommandList *commandList = CommandListRef_ptr(v);															\
																												\
	if(!Lock_isLockedForThread(&commandList->lock))																\
		return Error_invalidOperation(0, "CommandListRef_validate() cmdlist isn't locked");						\
																												\
	if(commandList->state != ECommandListState_Open)															\
		return Error_invalidOperation(1, "CommandListRef_validate() cmdlist isn't open")

#define CommandListRef_validateScope(v, label)																	\
																												\
	CommandListRef_validate(v);																					\
	Error err = Error_none();																					\
																												\
	if(!(commandList->tempStateFlags & ECommandStateFlags_HasScope))											\
		gotoIfError(label, Error_invalidOperation(0, "CommandListRef_validateScope() scope isn't open"))

Error CommandListRef_clear(CommandListRef *commandListRef) {

	CommandListRef_validate(commandListRef);

	for (U64 i = 0; i < commandList->resources.length; ++i) {

		RefPtr **ptr = &commandList->resources.ptrNonConst[i];

		if(*ptr)
			RefPtr_dec(ptr);
	}

	Error err;
	gotoIfError(clean, ListCommandOpInfo_clear(&commandList->commandOps))
	gotoIfError(clean, ListRefPtr_clear(&commandList->resources))
	gotoIfError(clean, ListTransitionInternal_clear(&commandList->transitions))
	gotoIfError(clean, ListCommandScope_clear(&commandList->activeScopes))

	gotoIfError(clean, ListDeviceResourceVersion_clear(&commandList->activeSwapchains))

	commandList->next = 0;

clean:
	return err;
}

Error CommandListRef_begin(CommandListRef *commandListRef, Bool doClear, U64 lockTimeout) {

	if(!commandListRef || commandListRef->typeId != (ETypeId)EGraphicsTypeId_CommandList)
		return Error_nullPointer(0, "CommandListRef_begin()::commandListRef invalid");

	CommandList *commandList = CommandListRef_ptr(commandListRef);

	if(Lock_lock(&commandList->lock, lockTimeout) != ELockAcquire_Acquired)
		return Error_invalidOperation(0, "CommandListRef_begin() couldn't acquire lock");

	Error err;

	if(commandList->state == ECommandListState_Open)
		gotoIfError(clean, Error_invalidOperation(1, "CommandListRef_begin() can only be called on non open cmdlist"))

	commandList->state = ECommandListState_Open;
	gotoIfError(clean, doClear ? CommandListRef_clear(commandListRef) : Error_none())

	if (!doClear) {		//Reacquire swapchains to ensure versions are the same

		for (U64 i = 0; i < commandList->activeSwapchains.length; ++i ) {

			const DeviceResourceVersion v = commandList->activeSwapchains.ptr[i];
			Swapchain *swapchain = SwapchainRef_ptr(v.resource);

			if(Lock_lock(&swapchain->lock, U64_MAX) != ELockAcquire_Acquired)
				gotoIfError(clean, Error_invalidOperation(0, "CommandListRef_begin() couldn't re-acquire swapchain locks"))

			const U64 verId = swapchain->versionId;
			Lock_unlock(&swapchain->lock);

			if(verId != v.version)
				gotoIfError(clean, Error_invalidOperation(
					1, "CommandListRef_begin() can't be called without clear if pre-recorded swapchain versionId changed"
				))
		}
	}

clean:

	if(err.genericError) {

		ListDeviceResourceVersion_clear(&commandList->activeSwapchains);

		commandList->state = ECommandListState_Invalid;
		Lock_unlock(&commandList->lock);
	}

	return err;
}

Error CommandListRef_end(CommandListRef *commandListRef) {

	CommandListRef_validate(commandListRef);

	Error err = Error_none();

	if (commandList->tempStateFlags & ECommandStateFlags_HasScope)
		gotoIfError(clean, Error_invalidState(0, "CommandListRef_end() can't be called if a scope is open"))

	gotoIfError(clean, ListRefPtr_reservex(&commandList->resources, commandList->transitions.length))

	for (U64 i = 0; i < commandList->transitions.length; ++i) {

		const TransitionInternal *transitions = &commandList->transitions.ptrNonConst[i];

		if(!ListRefPtr_contains(commandList->resources, transitions->resource, 0, NULL)) {				//TODO: hashSet

			if(RefPtr_inc(transitions->resource))		//CommandList will keep resource alive.
				gotoIfError(clean, ListRefPtr_pushBackx(&commandList->resources, transitions->resource))
		}
	}

	commandList->state = ECommandListState_Closed;

clean:

	if(err.genericError)
		commandList->state = ECommandListState_Invalid;

	Lock_unlock(&commandList->lock);
	return err;
}

Error CommandList_validateGraphicsPipeline(
	Pipeline *pipeline,
	ImageAndRange images[8],
	U8 imageCount,
	EDepthStencilFormat depthFormat,
	EMSAASamples boundSampleCount
) {

	PipelineGraphicsInfo *info = Pipeline_info(pipeline, PipelineGraphicsInfo);

	//Depth stencil state can be set to None to ignore writing to depth stencil

	if (info->depthFormatExt != EDepthStencilFormat_None && depthFormat != info->depthFormatExt)
		return Error_invalidState(0, "CommandList_validateGraphicsPipeline()::depthFormat was incompatible");

	if(info->msaa != boundSampleCount)
		return Error_invalidState(
			0, 
			"CommandList_validateGraphicsPipeline()::boundSampleCount is incompatible with pipeline MSAA setting"
		);

	//Validate attachments

	if (info->attachmentCountExt != imageCount)
		return Error_invalidState(1, "CommandList_validateGraphicsPipeline()::imageCount was incompatible");

	for (U8 i = 0; i < imageCount && i < 8; ++i) {

		//Undefined is used to ignore the currently bound slot and to avoid writing to it

		if (!info->attachmentFormatsExt[i])
			continue;

		//Validate if formats are the same

		RefPtr *ref = images[i].image;

		if (!ref)
			return Error_nullPointer(
				1, "CommandList_validateGraphicsPipeline()::images[i] is required by pipeline"
			);

		if (!TextureRef_isRenderTargetWritable(ref))
			return Error_invalidParameter(
				1, i, "CommandList_validateGraphicsPipeline()::images[i] is invalid type"
			);

		DeviceResourceVersion v;
		const UnifiedTexture tex = TextureRef_getUnifiedTexture(ref, &v);

		if(info->attachmentFormatsExt[i] != tex.textureFormatId)
			return Error_invalidState(
				i + 2, "CommandList_validateGraphicsPipeline()::images[i] is invalid format"
			);

		if(info->msaa != tex.sampleCount)
			return Error_invalidState(
				i + 2, 
				"CommandList_validateGraphicsPipeline()::images[i] has mismatching MSAA "
				"between pipeline and RenderTarget"
			);
	}

	return Error_none();
}

Bool CommandListRef_imageRangeConflicts(RefPtr *image1, ImageRange range1, RefPtr *image2, ImageRange range2) {
	(void)range2; (void)range1;		//TODO:
	return image1 == image2;
}

Bool CommandListRef_bufferRangeConflicts(RefPtr *buffer1, BufferRange range1, RefPtr *buffer2, BufferRange range2) {
	(void)range2; (void)range1;		//TODO:
	return buffer1 == buffer2;
}

//TLAS, BLAS, etc.
Bool CommandListRef_resourceConflicts(RefPtr *res1, RefPtr *res2) {
	return res1 == res2;
}

Bool CommandListRef_isBound(CommandList *commandList, RefPtr *resource, ResourceRange range, TransitionInternal **found) {

	if(!resource)
		return false;

	for(U64 i = 0; i < commandList->pendingTransitions.length; ++i) {

		const TransitionInternal transition = commandList->pendingTransitions.ptr[i];

		if(transition.resource->typeId != resource->typeId)
			continue;

		if(TextureRef_isTexture(resource)) {
			if(CommandListRef_imageRangeConflicts(
				resource, range.image, transition.resource, transition.range.image
			)) {
				if(found) *found = &commandList->pendingTransitions.ptrNonConst[i];
				return true;
			}
		}

		else if(resource->typeId == (ETypeId) EGraphicsTypeId_DeviceBuffer) {
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

Error CommandList_append(CommandList *commandList, ECommandOp op, Buffer buf, U32 extraSkipStacktrace) {

	(void)extraSkipStacktrace;

	const U64 len = Buffer_length(buf);
	Bool didPush = false;
	Error err;

	if(len > U32_MAX)
		gotoIfError(clean, Error_outOfBounds(2, len, U32_MAX, "CommandList_append() singular command can't exceed 32-bit"))

	if((commandList->commandOps.length + 1) >> 32)
		gotoIfError(clean, Error_outOfBounds(0, U32_MAX, U32_MAX, "CommandList_append() command ops can't exceed 32-bit"))

	if(commandList->next + len > Buffer_length(commandList->data)) {

		if(!commandList->allowResize)
			gotoIfError(clean, Error_outOfMemory(0, "CommandList_append() out of bounds"))

		//Resize buffer to allow allocation

		Buffer resized = Buffer_createNull();
		gotoIfError(clean, Buffer_createEmptyBytesx(Buffer_length(commandList->data) * 2 + KIBI + len, &resized))

		Buffer_copy(resized, commandList->data);
		Buffer_freex(&commandList->data);

		commandList->data = resized;
	}

	const CommandOpInfo info = (CommandOpInfo) {
		.op = op,
		.opSize = (U32) len
	};

	gotoIfError(clean, ListCommandOpInfo_pushBackx(&commandList->commandOps, info))
	didPush = true;

	if(len) {
		Buffer_copy(Buffer_createRef((U8*)commandList->data.ptr + commandList->next, len), buf);
		commandList->next += len;
	}

clean:

	if(err.genericError) {

		if(didPush)
			ListCommandOpInfo_popBack(&commandList->commandOps, NULL);

		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;
	}

	return err;
}

Error CommandListRef_transitionBuffer(
	CommandList *commandList,
	DeviceBufferRef *buffer,
	BufferRange range,
	ETransitionType type
) {

	if(!buffer)
		return Error_none();

	TransitionInternal *oldState = NULL;
	if(CommandListRef_isBound(commandList, buffer, (ResourceRange) { .buffer = range }, &oldState)) {

		if(oldState->type != type)
			return Error_invalidOperation(
				4, "CommandListRef_transitionBuffer()::buffer was already transitioned in scope!"
			);

		return Error_none();
	}

	const TransitionInternal transition = (TransitionInternal) {
		.resource = buffer,
		.range = (ResourceRange) { .buffer = range },
		.stage = EPipelineStage_Count,
		.type = type
	};

	return ListTransitionInternal_pushBackx(&commandList->pendingTransitions, transition);
}

//Standard commands

Error CommandListRef_checkBounds(I32x2 offset, I32x2 size, I32 lowerBound1, I32 upperBound1) {

	if(I32x2_any(I32x2_leq(size, I32x2_zero())))
		return Error_invalidParameter(1, 0, "CommandListRef_checkBounds()::size is <=0");

	const I32x2 upperBound = I32x2_xx2(upperBound1);
	const I32x2 lowerBound = I32x2_xx2(lowerBound1);

	if(I32x2_any(I32x2_gt(size, upperBound)))
		return Error_invalidParameter(1, 0, "CommandListRef_checkBounds()::size > upperBound");

	if(I32x2_any(I32x2_lt(offset, lowerBound)))
		return Error_invalidParameter(0, 0, "CommandListRef_checkBounds()::offset < lowerBound");

	if(I32x2_any(I32x2_gt(offset, upperBound)))
		return Error_invalidParameter(0, 1, "CommandListRef_checkBounds()::offset > upperBound");

	const I32x2 end = I32x2_add(offset, size);

	if(I32x2_any(I32x2_gt(end, upperBound)))
	   return Error_invalidParameter(0, 2, "CommandListRef_checkBounds()::offset + size > upperBound");

	return Error_none();
}

Error CommandListRef_setViewportCmd(CommandListRef *commandListRef, I32x2 offset, I32x2 size, ECommandOp op) {

	CommandListRef_validateScope(commandListRef, clean)

	if(I32x2_any(I32x2_eq(commandList->currentSize, I32x2_zero())))
		gotoIfError(clean, Error_invalidOperation(0, "CommandListRef_setViewportCmd() requires startRender(Pass/Ext)"))

	if(I32x2_any(I32x2_geq(offset, commandList->currentSize)))
		gotoIfError(clean, Error_invalidOperation(1, "CommandListRef_setViewportCmd() offset >= currentSize"))

	if(I32x2_eq2(size, I32x2_zero()))
		size = I32x2_sub(commandList->currentSize, offset);

	gotoIfError(clean, CommandListRef_checkBounds(offset, size, -32768, 32767))

	I32x4 values = I32x4_create2_2(offset, size);
	gotoIfError(clean, CommandList_append(commandList, op, Buffer_createRefConst(&values, sizeof(values)), 1))

	if(op == ECommandOp_SetViewport || op == ECommandOp_SetViewportAndScissor)
		commandList->tempStateFlags |= ECommandStateFlags_AnyViewport;

	if(op == ECommandOp_SetScissor  || op == ECommandOp_SetViewportAndScissor)
		commandList->tempStateFlags |= ECommandStateFlags_AnyScissor;

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_setViewport(CommandListRef *commandListRef, I32x2 offset, I32x2 size) {
	return CommandListRef_setViewportCmd(commandListRef, offset, size, ECommandOp_SetViewport);
}

Error CommandListRef_setScissor(CommandListRef *commandListRef, I32x2 offset, I32x2 size) {
	return CommandListRef_setViewportCmd(commandListRef, offset, size, ECommandOp_SetScissor);
}

Error CommandListRef_setViewportAndScissor(CommandListRef *commandListRef, I32x2 offset, I32x2 size) {
	return CommandListRef_setViewportCmd(commandListRef, offset, size, ECommandOp_SetViewportAndScissor);
}

Error CommandListRef_setStencil(CommandListRef *commandListRef, U8 stencilValue) {

	CommandListRef_validateScope(commandListRef, clean)
	gotoIfError(clean, CommandList_append(commandList, ECommandOp_SetStencil, Buffer_createRefConst(&stencilValue, 1), 0))

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_setBlendConstants(CommandListRef *commandListRef, F32x4 blendConstants) {

	CommandListRef_validateScope(commandListRef, clean)

	gotoIfError(clean, CommandList_append(
		commandList, ECommandOp_SetBlendConstants, Buffer_createRefConst(&blendConstants, sizeof(F32x4)), 0
	))

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_clearImages(CommandListRef *commandListRef, ListClearImageCmd clearImages) {

	Buffer buf = Buffer_createNull();
	CommandListRef_validateScope(commandListRef, clean)

	if(!clearImages.length)
		gotoIfError(clean, Error_nullPointer(1, "CommandListRef_clearImages()::clearImages.length is 0"))

	if(clearImages.length > U32_MAX)
		gotoIfError(clean, Error_outOfBounds(
			1, clearImages.length, U32_MAX, "CommandListRef_clearImages()::clearImages.length > U32_MAX"
		))

	GraphicsDeviceRef *device = CommandListRef_ptr(commandList)->device;

	for(U64 i = 0; i < clearImages.length; ++i) {

		ClearImageCmd clearImage = clearImages.ptr[i];
		UnifiedTexture tex = TextureRef_getUnifiedTexture(clearImage.image, NULL);

		if(!tex.resource.device || !TextureRef_isRenderTargetWritable(clearImage.image))
			gotoIfError(clean, Error_nullPointer(1, "CommandListRef_clearImages()::clearImages[i].image is invalid"))

		if(tex.resource.device != device)
			gotoIfError(clean, Error_unsupportedOperation(
				0, "CommandListRef_clearImages()::clearImages[i].image belongs to other device"
			))

		//TODO: Properly support this

		if (clearImage.range.layerId != U32_MAX && clearImage.range.layerId >= 1)
			gotoIfError(clean, Error_outOfBounds(
				1, clearImage.range.layerId, 1, "CommandListRef_clearImages()::clearImages[i].range.layerId is invalid"
			))

		if (clearImage.range.levelId != U32_MAX && clearImage.range.levelId >= 1)
			gotoIfError(clean, Error_outOfBounds(
				2, clearImage.range.levelId, 1, "CommandListRef_clearImages()::clearImages[i].range.levelId is invalid"
			))

		//Add transition

		TransitionInternal *found = NULL;
		if(CommandListRef_isBound(commandList, clearImage.image, (ResourceRange) { .image = clearImage.range }, &found)) {

			if(found->type != ETransitionType_Clear)
				gotoIfError(clean, Error_invalidOperation(
					0, "CommandListRef_clearImages()::clearImages[i].image is already transitioned"
				))

			continue;
		}

		TransitionInternal transition = (TransitionInternal) {
			.resource = clearImage.image,
			.range = (ResourceRange) { .image = clearImage.range },
			.stage = EPipelineStage_Count,
			.type = ETransitionType_Clear
		};

		gotoIfError(clean, ListTransitionInternal_pushBackx(&commandList->pendingTransitions, transition))
	}

	//Copy buffer

	gotoIfError(clean, Buffer_createEmptyBytesx(ListClearImageCmd_bytes(clearImages) + sizeof(U32), &buf))

	*(U32*)buf.ptr = (U32) clearImages.length;
	Buffer_copy(
		Buffer_createRef((U8*) buf.ptr + sizeof(U32), ListClearImageCmd_bytes(clearImages)),
		ListClearImageCmd_bufferConst(clearImages)
	);

	gotoIfError(clean, CommandList_append(commandList, ECommandOp_ClearImages, buf, 0))

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	Buffer_freex(&buf);
	return err;
}

Error CommandListRef_copyImageRegions(
	CommandListRef *commandListRef,
	RefPtr *srcRef,
	RefPtr *dstRef,
	ECopyType copyType,
	ListCopyImageRegion regions
) {

	Buffer buf = Buffer_createNull();
	CommandListRef_validateScope(commandListRef, clean)

	//Validate regions.length to be <0, U32_MAX]

	if(!regions.length)
		gotoIfError(clean, Error_nullPointer(3, "CommandListRef_copyImage()::regions.length is 0"))

	if(regions.length > U32_MAX)
		gotoIfError(clean, Error_outOfBounds(
			4, regions.length, U32_MAX, "CommandListRef_copyImage()::regions.length > U32_MAX"
		))

	//Validate src and dst

	if(!srcRef || !dstRef)
		gotoIfError(clean, Error_outOfBounds(
			!srcRef ? 1 : 2, regions.length, U32_MAX, "CommandListRef_copyImage()::src and dst are required"
		))

	for (U64 i = 0; i < 2; ++i)
		if(!TextureRef_isTexture(i ? dstRef : srcRef))
			gotoIfError(clean, Error_invalidParameter(
				i ? 1 : 2, 0,
				"CommandListRef_copyImage()::src and dst should be a texture "
				"(Swapchain, DepthStencil, RenderTexture, DeviceTexture)"
			))

	//Validate depth stencil

	Bool isDepthStencil = TextureRef_isDepthStencil(dstRef);

	if(isDepthStencil != TextureRef_isDepthStencil(srcRef))
		gotoIfError(clean, Error_invalidParameter(
			1, 0, "CommandListRef_copyImage()::src and dst should be DepthStencil if one of them is to be compatible"
		))

	if(!isDepthStencil && copyType != ECopyType_All)
		gotoIfError(clean, Error_invalidParameter(
			3, 0, "CommandListRef_copyImage()::copyType should be ECopyType_All if DepthStencil isn't copied"
		))

	DeviceResourceVersion v;
	UnifiedTexture src = TextureRef_getUnifiedTexture(srcRef, &v);
	UnifiedTexture dst = TextureRef_getUnifiedTexture(dstRef, &v);

	if (isDepthStencil) {

		EDepthStencilFormat srcFormat = src.depthFormat;
		EDepthStencilFormat dstFormat = dst.depthFormat;

		if(copyType == ECopyType_All && srcFormat != dstFormat)
			gotoIfError(clean, Error_invalidParameter(
				1, 1, "CommandListRef_copyImage()::src and dst don't match in depth stencil format"
			))

		if (
			copyType == ECopyType_StencilOnly && (
				srcFormat < EDepthStencilFormat_StencilStart || dstFormat < EDepthStencilFormat_StencilStart
			)
		)
			gotoIfError(clean, Error_invalidParameter(
				1, 2, "CommandListRef_copyImage()::src and dst both require stencil when using ECopyType_StencilOnly"
			))

		if (copyType == ECopyType_DepthOnly) {

			Bool compatible = srcFormat == dstFormat;

			switch (srcFormat) {

				case EDepthStencilFormat_D32:
				case EDepthStencilFormat_D32S8:
					compatible = dstFormat == EDepthStencilFormat_D32S8 || dstFormat == EDepthStencilFormat_D32;
					break;

				default:
					break;
			}

			if(!compatible)
				gotoIfError(clean, Error_invalidParameter(
					1, 3,
					"CommandListRef_copyImage()::src and dst require the same depth format if ECopyType_DepthOnly is used "
					"(D32/D32S8 is compatible with D32/D32S8)"
				))
		}
	}

	//Ensure both formats are the same

	else if(src.textureFormatId != dst.textureFormatId)
		gotoIfError(clean, Error_invalidParameter(
			1, 5, "CommandListRef_copyImage()::src and dst require the same texture format"
		))

	//Validate devices

	if(src.resource.device != commandList->device || dst.resource.device != commandList->device)
		gotoIfError(clean, Error_invalidParameter(
			1, 6, "CommandListRef_copyImage()::src and dst require the same device as the CommandList"
		))

	//Validate copy

	for(U64 i = 0; i < regions.length; ++i) {

		CopyImageRegion clearImage = regions.ptr[i];

		//Validate levelId

		if(clearImage.dstLevelId || clearImage.srcLevelId)		//TODO: Allow levels
			gotoIfError(clean, Error_invalidParameter(
				1, 6, "CommandListRef_copyImage()::regions[i].src/dstLevelId is out of bounds"
			))
	}

	//TODO: Check if regions are out of bounds (be sure to keep in mind that w,h,l is 0 means src->w,h,l - src->offset

	//TODO: Check if regions intersect in dst

	//TODO: Ensure there's no overlapping src and dst region
	//if(src == dst)
	//	;

	//Add transitions

	ETransitionType types[2] = { ETransitionType_CopyRead, ETransitionType_CopyWrite };
	RefPtr *ptrs[2] = { srcRef, dstRef };

	for(U64 i = 0; i < 2; ++i) {

		TransitionInternal *found = NULL;
		if(CommandListRef_isBound(commandList, ptrs[i], (ResourceRange) { 0 }, &found)) {

			if(found->type != types[i])
				gotoIfError(clean, Error_invalidOperation(
					0, "CommandListRef_clearImages()::src is already transitioned in the same scope"
				))
		}

		else {

			TransitionInternal transition = (TransitionInternal) {
				.resource = ptrs[i],
				.range = (ResourceRange) { 0 },
				.stage = EPipelineStage_Count,
				.type = types[i]
			};

			gotoIfError(clean, ListTransitionInternal_pushBackx(&commandList->pendingTransitions, transition))
		}
	}

	//Copy buffer

	gotoIfError(clean, Buffer_createEmptyBytesx(ListCopyImageRegion_bytes(regions) + sizeof(CopyImageCmd), &buf))

	*(CopyImageCmd*)buf.ptr = (CopyImageCmd) {
		.src = srcRef,
		.dst = dstRef,
		.regionCount = (U32) regions.length,
		.copyType = copyType
	};

	Buffer_copy(
		Buffer_createRef((U8*) buf.ptr + sizeof(CopyImageCmd), ListCopyImageRegion_bytes(regions)),
		ListCopyImageRegion_bufferConst(regions)
	);

	gotoIfError(clean, CommandList_append(commandList, ECommandOp_CopyImage, buf, 0))

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	Buffer_freex(&buf);
	return err;
}

Error CommandListRef_copyImage(
	CommandListRef *commandListRef, RefPtr *src, RefPtr *dst, ECopyType copyType, CopyImageRegion region
) {
	CommandListRef_validateScope(commandListRef, clean)

	ListCopyImageRegion regions = (ListCopyImageRegion) { 0 };
	gotoIfError(clean, ListCopyImageRegion_createRefConst(&region, 1, &regions))

	gotoIfError(clean, CommandListRef_copyImageRegions(commandListRef, src, dst, copyType, regions))

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_clearImageu(CommandListRef *commandListRef, const U32 coloru[4], ImageRange range, RefPtr *image) {

	CommandListRef_validateScope(commandListRef, clean)

	ClearImageCmd clearImage = (ClearImageCmd) {
		.image = image,
		.range = range
	};

	Buffer_copy(Buffer_createRef(&clearImage.color, sizeof(F32x4)), Buffer_createRefConst(coloru, sizeof(F32x4)));

	ListClearImageCmd clearImages = (ListClearImageCmd) { 0 };
	gotoIfError(clean, ListClearImageCmd_createRefConst(&clearImage, 1, &clearImages))

	gotoIfError(clean, CommandListRef_clearImages(commandListRef, clearImages))

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_clearImagei(CommandListRef *commandListRef, I32x4 color, ImageRange range, RefPtr *image) {
	return CommandListRef_clearImageu(commandListRef, (const U32*) &color, range, image);
}

Error CommandListRef_clearImagef(CommandListRef *commandListRef, F32x4 color, ImageRange range, RefPtr *image) {
	return CommandListRef_clearImageu(commandListRef, (const U32*) &color, range, image);
}

//Render calls

Error CommandListRef_startScope(
	CommandListRef *commandListRef,
	ListTransition transitions,
	U32 id,
	ListCommandScopeDependency deps
) {

	CommandListRef_validate(commandListRef);

	GraphicsDeviceRef *device = commandList->device;

	Error err = Error_none();

	if(transitions.length > U32_MAX)
		gotoIfError(clean, Error_outOfBounds(
			1, transitions.length, U32_MAX, "CommandListRef_startScope()::transitions.length > U32_MAX"
		))

	if(commandList->tempStateFlags & ECommandStateFlags_HasScope)		//No nested scopes
		gotoIfError(clean, Error_invalidOperation(
			0, "CommandListRef_startScope() scope is already present. Nested scopes are unsupported"
		))

	gotoIfError(clean, ListTransitionInternal_clear(&commandList->pendingTransitions))
	gotoIfError(clean, ListTransitionInternal_reservex(&commandList->pendingTransitions, transitions.length))

	for(U64 i = 0; i < transitions.length; ++i) {

		Transition transition = transitions.ptr[i];
		RefPtr *res = transition.resource;

		if(!res)
			gotoIfError(clean, Error_nullPointer(1, "CommandListRef_startScope()::transitions[i].resource is NULL"))

		UnifiedTexture tex = TextureRef_getUnifiedTexture(res, NULL);
		Bool isSampler = res->typeId == EGraphicsTypeId_Sampler;

		GraphicsResource resource = tex.resource;

		if (tex.resource.device)
			resource = tex.resource;

		else if (res->typeId == EGraphicsTypeId_DeviceBuffer)
			resource = DeviceBufferRef_ptr(res)->resource;

		else if(isSampler)
			resource = (GraphicsResource) { .device = SamplerRef_ptr(res)->device };		//Only device is required here

		else if (res->typeId == EGraphicsTypeId_TLASExt)									//Get device and mark as readonly
			resource = (GraphicsResource) {
				.device = TLASRef_ptr(res)->base.device, .flags = EGraphicsResourceFlag_ShaderRead
			};

		else if (res->typeId == EGraphicsTypeId_BLASExt)									//Get device and mark as readonly
			resource = (GraphicsResource) {
				.device = BLASRef_ptr(res)->base.device, .flags = EGraphicsResourceFlag_ShaderRead
			};

		else gotoIfError(clean, Error_invalidParameter(
			1, 0, "CommandListRef_startScope()::transitions[i].resource's type is unsupported"
		))

		TransitionInternal transitionDst;

		if(!isSampler) {

			if (transition.isWrite && !(resource.flags & EGraphicsResourceFlag_ShaderWrite))
				gotoIfError(clean, Error_constData(
					0, 0, "CommandListRef_startScope()::transitions[i].resource should be writable"
				))

			if(!transition.isWrite && !(resource.flags & EGraphicsResourceFlag_ShaderRead))
				gotoIfError(clean, Error_unsupportedOperation(
					1, "CommandListRef_startScope()::transitions[i].resource should be readable"
				))

			transitionDst = (TransitionInternal) {
				.resource = res,
				.range = transition.range,
				.stage = transition.stage,
				.type = transition.isWrite ? ETransitionType_ShaderWrite : ETransitionType_ShaderRead
			};
		}

		else transitionDst = (TransitionInternal) { .resource = res, .type = ETransitionType_KeepAlive };

		if(resource.device != device)
			gotoIfError(clean, Error_unsupportedOperation(
				0, "CommandListRef_startScope()::transitions[i].resource's device is incompatible"
			))

		TransitionInternal *found = NULL;
		if(CommandListRef_isBound(commandList, res, transition.range, &found)) {

			if(found->type != transitionDst.type)
				gotoIfError(clean, Error_invalidOperation(
					0, "CommandListRef_startScope()::transitions[i].resource is already transitioned"
				))

			//To combine shader transitions we just take the highest up shader stage it's used

			found->stage = (EPipelineStage) U64_min(found->stage, transitionDst.stage);
			continue;
		}

		gotoIfError(clean, ListTransitionInternal_pushBackx(&commandList->pendingTransitions, transitionDst))
	}

	//Scope has to be unique

	for (U64 i = 0; i < commandList->activeScopes.length; ++i) {

		CommandScope scope = commandList->activeScopes.ptr[i];

		if(scope.scopeId == id)
			gotoIfError(clean, Error_alreadyDefined(0, "CommandListRef_startScope()::id should be unique"))
	}

	//Find deps

	for (U64 j = 0; j < deps.length; ++j) {

		CommandScopeDependency dep = deps.ptr[j];

		if(dep.type == ECommandScopeDependencyType_Unconditional)		//Don't care
			continue;

		Bool found = false;

		for (U64 i = 0; i < commandList->activeScopes.length; ++i) {	//TODO: HashSet

			CommandScope scope = commandList->activeScopes.ptr[i];

			if (scope.scopeId == dep.id) {
				found = true;
				break;
			}
		}

		if(found)
			continue;

		gotoIfError(clean, Error_notFound(0, 0, "CommandListRef_startScope()::deps[j] not found"))
	}

	//TODO: Append deps to array so runtime can use it (U32 only)

	commandList->lastCommandId = (U32) commandList->commandOps.length;
	commandList->lastOffset = commandList->next;
	commandList->lastScopeId = id;

	gotoIfError(clean, CommandList_append(commandList, ECommandOp_StartScope, Buffer_createNull(), 0))

	commandList->tempStateFlags |= ECommandStateFlags_HasScope;

clean:

	if(err.genericError)
		commandList->tempStateFlags = ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_endScope(CommandListRef *commandListRef) {

	CommandListRef_validateScope(commandListRef, clean)

	//Check if scope has to be hidden.

	if(
		(commandList->tempStateFlags & ECommandStateFlags_InvalidState) ||			//Hide scope
		!(commandList->tempStateFlags & ECommandStateFlags_HasModifyOp)
	) {
		//Pretend the last commands didn't happen

		gotoIfError(clean, ListCommandOpInfo_resize(&commandList->commandOps, commandList->lastCommandId, (Allocator) { 0 }))
		commandList->next = commandList->lastOffset;

		goto clean;
	}

	if(commandList->debugRegionStack)
		gotoIfError(clean, Error_invalidOperation(
			0, "CommandListRef_endScope() can't close scope while debugRegion is still active"
		))

	if(!I32x2_eq2(commandList->currentSize, I32x2_zero()))
		gotoIfError(clean, Error_invalidOperation(1, "CommandListRef_endScope() can't close scope while render is active"))

	//Push command, transitions and scope

	if((commandList->transitions.length + commandList->pendingTransitions.length) >> 32)
		gotoIfError(clean, Error_outOfBounds(
			0, commandList->transitions.length + commandList->pendingTransitions.length, U32_MAX,
			"CommandListRef_endScope() transitionCount of command list can't exceed U32_MAX"
		))

	const U32 commandOps = (U32)((commandList->commandOps.length + 1) - commandList->lastCommandId);
	const U64 commandLen = commandList->next - commandList->lastOffset;
	const U32 transitionOffset = (U32) commandList->transitions.length;

	gotoIfError(clean, CommandList_append(commandList, ECommandOp_EndScope, Buffer_createNull(), 0))
	gotoIfError(clean, ListTransitionInternal_pushAllx(&commandList->transitions, commandList->pendingTransitions))

	const CommandScope scope = (CommandScope) {
		.commandBufferOffset = commandList->lastOffset,
		.commandBufferLength = commandLen,
		.commandOpOffset = commandList->lastCommandId,
		.transitionOffset = transitionOffset,
		.commandOps = commandOps,
		.transitionCount = (U32) commandList->pendingTransitions.length,
		.scopeId = commandList->lastScopeId
	};

	gotoIfError(clean, ListCommandScope_pushBackx(&commandList->activeScopes, scope))

clean:

	for(U64 i = 0; i < EPipelineType_Count; ++i)
		commandList->pipeline[i] = NULL;

	ListTransitionInternal_clear(&commandList->pendingTransitions);

	commandList->tempStateFlags = 0;
	commandList->debugRegionStack = 0;
	commandList->currentSize = I32x2_zero();

	if(err.genericError)
		commandList->state = ECommandListState_Invalid;

	return err;
}

Error CommandListRef_setPipeline(CommandListRef *commandListRef, PipelineRef *pipelineRef, EPipelineType type) {

	CommandListRef_validateScope(commandListRef, clean)

	if (!pipelineRef || pipelineRef->typeId != EGraphicsTypeId_Pipeline)
		gotoIfError(clean, Error_nullPointer(1, "CommandListRef_setPipeline()::pipelineRef is required"))

	const Pipeline *pipeline = PipelineRef_ptr(pipelineRef);

	if(pipeline->device != commandList->device)
		gotoIfError(clean, Error_unsupportedOperation(
			0, "CommandListRef_setPipeline()::pipelineRef is owned by different device"
		))

	if(pipeline->type != type)
		gotoIfError(clean, Error_unsupportedOperation(
			1, "CommandListRef_setPipeline()::pipeline's type is incompatible with type"
		))

	ECommandOp op = ECommandOp_SetComputePipeline;

	if(type == EPipelineType_Graphics)
		op = ECommandOp_SetGraphicsPipeline;

	else if(type == EPipelineType_RaytracingExt)
		op = ECommandOp_SetRaytracingPipelineExt;

	gotoIfError(clean, CommandList_append(
		commandList, op, Buffer_createRefConst((const U8*) &pipelineRef, sizeof(pipelineRef)), 0
	))

	if(!ListRefPtr_contains(commandList->resources, pipelineRef, 0, NULL)) {						//TODO: hashSet
		RefPtr_inc(pipelineRef);		//CommandList will keep resource alive.
		gotoIfError(clean, ListRefPtr_pushBackx(&commandList->resources, pipelineRef))
	}

	commandList->pipeline[type] = pipelineRef;

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_setComputePipeline(CommandListRef *commandList, PipelineRef *pipeline) {
	return CommandListRef_setPipeline(commandList, pipeline, EPipelineType_Compute);
}

Error CommandListRef_setGraphicsPipeline(CommandListRef *commandList, PipelineRef *pipeline) {
	return CommandListRef_setPipeline(commandList, pipeline, EPipelineType_Graphics);
}

Error CommandListRef_setRaytracingPipeline(CommandListRef *commandList, PipelineRef *pipeline) {
	return CommandListRef_setPipeline(commandList, pipeline, EPipelineType_RaytracingExt);
}

Error CommandListRef_validateBufferDesc(GraphicsDeviceRef *device, DeviceBufferRef *buffer, EDeviceBufferUsage usage) {

	if(!buffer)
		return Error_none();

	if(buffer->typeId != EGraphicsTypeId_DeviceBuffer)
		return Error_unsupportedOperation(0, "CommandListRef_validateBufferDesc()::buffer has invalid type");

	if(DeviceBufferRef_ptr(buffer)->resource.device != device)
		return Error_unsupportedOperation(
			1, "CommandListRef_validateBufferDesc()::buffer is owned by different device"
		);

	DeviceBuffer *buf = DeviceBufferRef_ptr(buffer);

	if((buf->usage & usage) != usage)
		return Error_unsupportedOperation(
			2, "CommandListRef_validateBufferDesc()::buffer is missing required usage flag"
		);

	return Error_none();
}

Error CommandListRef_setPrimitiveBuffers(CommandListRef *commandListRef, SetPrimitiveBuffersCmd buffers) {

	CommandListRef_validateScope(commandListRef, clean)

	if(I32x2_any(I32x2_eq(commandList->currentSize, I32x2_zero())))
		gotoIfError(clean, Error_invalidOperation(
			0, "CommandListRef_setPrimitiveBuffers() is only available if render is started"
		))

	//Validate index and vertex buffers

	GraphicsDeviceRef *device = commandList->device;
	gotoIfError(clean, CommandListRef_validateBufferDesc(device, buffers.indexBuffer, EDeviceBufferUsage_Index))

	if(err.genericError)
		return err;

	for(U8 i = 0; i < 8; ++i)
		gotoIfError(clean, CommandListRef_validateBufferDesc(device, buffers.vertexBuffers[i], EDeviceBufferUsage_Vertex))

	//Transition

	gotoIfError(clean, CommandListRef_transitionBuffer(
		commandList, buffers.indexBuffer, (BufferRange) { 0 }, ETransitionType_Index
	))

	for(U8 i = 0; i < 8; ++i)
		gotoIfError(clean, CommandListRef_transitionBuffer(
			commandList, buffers.vertexBuffers[i], (BufferRange) { 0 }, ETransitionType_Vertex
		))

	//Issue command

	gotoIfError(clean, CommandList_append(
		commandList, ECommandOp_SetPrimitiveBuffers, Buffer_createRefConst((const U8*) &buffers, sizeof(buffers)), 0
	))

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_drawBase(CommandListRef *commandListRef, Buffer buf, ECommandOp op) {

	CommandListRef_validateScope(commandListRef, clean)

	if(I32x2_any(I32x2_eq(commandList->currentSize, I32x2_zero())))
		gotoIfError(clean, Error_invalidOperation(0, "CommandListRef_drawBase() is only available if render is started"))

	PipelineRef *pipelineRef = commandList->pipeline[EPipelineType_Graphics];

	if (!pipelineRef)
		gotoIfError(clean, Error_invalidOperation(1, "CommandListRef_drawBase() requires bound graphics pipeline"))

	U32 flags = ECommandStateFlags_AnyScissor | ECommandStateFlags_AnyViewport;

	if ((commandList->tempStateFlags & flags) != flags)
		gotoIfError(clean, Error_invalidOperation(2, "CommandListRef_drawBase() requires viewport and scissor"))

	gotoIfError(clean, CommandList_validateGraphicsPipeline(
		PipelineRef_ptr(pipelineRef),
		commandList->boundImages,
		commandList->boundImageCount,
		commandList->boundDepthFormat,
		commandList->boundSampleCount
	))

	gotoIfError(clean, CommandList_append(commandList, op, buf, 1))

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_draw(CommandListRef *commandListRef, DrawCmd draw) {

	if(!draw.count || !draw.instanceCount)		//No-op
		return Error_none();

	return CommandListRef_drawBase(commandListRef, Buffer_createRefConst(&draw, sizeof(draw)), ECommandOp_Draw);
}

Error CommandListRef_drawIndexed(CommandListRef *commandList, U32 indexCount, U32 instanceCount) {
	const DrawCmd draw = (DrawCmd) { .count = indexCount, .instanceCount = instanceCount, .isIndexed = true };
	return CommandListRef_draw(commandList, draw);
}

Error CommandListRef_drawIndexedAdv(
	CommandListRef *commandList,
	U32 indexCount, U32 instanceCount,
	U32 indexOffset, U32 instanceOffset,
	U32 vertexOffset
) {
	const DrawCmd draw = (DrawCmd) {
		.count = indexCount,
		.instanceCount = instanceCount,
		.indexOffset = indexOffset,
		.instanceOffset = instanceOffset,
		.vertexOffset = vertexOffset,
		.isIndexed = true
	};

	return CommandListRef_draw(commandList, draw);
}

Error CommandListRef_drawUnindexed(CommandListRef *commandList, U32 vertexCount, U32 instanceCount) {
	const DrawCmd draw = (DrawCmd) { .count = vertexCount, .instanceCount = instanceCount };
	return CommandListRef_draw(commandList, draw);
}

Error CommandListRef_drawUnindexedAdv(
	CommandListRef *commandList,
	U32 vertexCount, U32 instanceCount,
	U32 vertexOffset, U32 instanceOffset
) {
	const DrawCmd draw = (DrawCmd) {
		.count = vertexCount,
		.instanceCount = instanceCount,
		.vertexOffset = vertexOffset,
		.instanceOffset = instanceOffset
	};

	return CommandListRef_draw(commandList, draw);
}

Error CommandListRef_dispatch(CommandListRef *commandListRef, DispatchCmd dispatch) {

	CommandListRef_validateScope(commandListRef, clean)

	if (!commandList->pipeline[EPipelineType_Compute])
		gotoIfError(clean, Error_invalidOperation(1, "CommandListRef_dispatch() requires bound compute pipeline"))

	const U64 groupCountMax = U64_max(dispatch.groups[0], U64_max(dispatch.groups[1], dispatch.groups[2]));

	if(groupCountMax > U16_MAX)
		gotoIfError(clean, Error_outOfBounds(
			1, groupCountMax, U16_MAX, "CommandListRef_dispatch() groupCountMax out of bounds"
		))

	gotoIfError(clean, CommandList_append(
		commandList, ECommandOp_Dispatch, Buffer_createRefConst(&dispatch, sizeof(dispatch)), 0
	))

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_dispatch1D(CommandListRef *commandList, U32 groupsX) {
	const DispatchCmd dispatch = (DispatchCmd) { .groups = { groupsX, 1, 1 } };
	return CommandListRef_dispatch(commandList, dispatch);
}

Error CommandListRef_dispatch2D(CommandListRef *commandList, U32 groupsX, U32 groupsY) {
	const DispatchCmd dispatch = (DispatchCmd) { .groups = { groupsX, groupsY, 1 } };
	return CommandListRef_dispatch(commandList, dispatch);
}

Error CommandListRef_dispatch3D(CommandListRef *commandList, U32 groupsX, U32 groupsY, U32 groupsZ) {
	const DispatchCmd dispatch = (DispatchCmd) { .groups = { groupsX, groupsY, groupsZ } };
	return CommandListRef_dispatch(commandList, dispatch);
}

//Dispatch rays

Error CommandListRef_dispatchRaysExt(CommandListRef *commandListRef, DispatchRaysExt dispatch) {

	CommandListRef_validateScope(commandListRef, clean)

	PipelineRef *rayPipeline = commandList->pipeline[EPipelineType_RaytracingExt];

	if (!rayPipeline)
		gotoIfError(clean, Error_invalidOperation(1, "CommandListRef_dispatchRaysExt() requires bound raytracing pipeline"))

	U64 total = dispatch.x * dispatch.y;

	if(total < dispatch.x || total * dispatch.z < total)
		gotoIfError(clean, Error_invalidOperation(1, "CommandListRef_dispatchRaysExt() overflow"))

	total *= dispatch.z;

	if(total > 1 * GIBI)
		gotoIfError(clean, Error_invalidOperation(1, "CommandListRef_dispatchRaysExt() is limited to 1Gibi rays"))

	if(!total)
		gotoIfError(clean, Error_invalidOperation(1, "CommandListRef_dispatchRaysExt() can't be executed with 0 count"))

	if(dispatch.raygenId >= Pipeline_info(PipelineRef_ptr(rayPipeline), PipelineRaytracingInfo)->raygenCount)
		gotoIfError(clean, Error_invalidOperation(1, "CommandListRef_dispatchRaysExt() raygen index out of bounds"))

	gotoIfError(clean, CommandList_append(
		commandList, ECommandOp_DispatchRaysExt, Buffer_createRefConst(&dispatch, sizeof(dispatch)), 0
	))

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_dispatch1DRaysExt(CommandListRef *commandList, U32 raygenLocalId, U32 groupsX) {
	return CommandListRef_dispatchRaysExt(commandList, (DispatchRaysExt) { groupsX, 1, 1, raygenLocalId });
}

Error CommandListRef_dispatch2DRaysExt(CommandListRef *commandList, U32 raygenLocalId, U32 groupsX, U32 groupsY) {
	return CommandListRef_dispatchRaysExt(commandList, (DispatchRaysExt) { groupsX, groupsY, 1, raygenLocalId });
}

Error CommandListRef_dispatch3DRaysExt(CommandListRef *commandList, U32 raygenLocalId, U32 groupsX, U32 groupsY, U32 groupsZ) {
	return CommandListRef_dispatchRaysExt(commandList, (DispatchRaysExt) { groupsX, groupsY, groupsZ, raygenLocalId });
}

//Indirect rendering

Error CommandListRef_checkDispatchBuffer(GraphicsDeviceRef *device, DeviceBufferRef *buffer, U64 offset, U64 siz) {

	if(!buffer || buffer->typeId != EGraphicsTypeId_DeviceBuffer)
		return Error_nullPointer(1, "CommandListRef_checkDispatchBuffer()::buffer is required");

	const DeviceBuffer *buf = DeviceBufferRef_ptr(buffer);

	if(buf->resource.device != device)
		return Error_unsupportedOperation(3, "CommandListRef_checkDispatchBuffer()::buffer is owned by different device");

	if(offset + siz > buf->resource.size)
		return Error_outOfBounds(
			1, offset + siz, buf->resource.size, "CommandListRef_checkDispatchBuffer()::offset + size is out of bounds"
		);

	if(!(buf->usage & EDeviceBufferUsage_Indirect))
		return Error_unsupportedOperation(0, "CommandListRef_checkDispatchBuffer()::buffer requires indirect buffer usage");

	return Error_none();
}

Error CommandListRef_dispatchIndirect(CommandListRef *commandListRef, DeviceBufferRef *buffer, U64 offset) {

	CommandListRef_validateScope(commandListRef, clean);
	GraphicsDeviceRef *device = commandList->device;

	if (!commandList->pipeline[EPipelineType_Compute])
		gotoIfError(clean, Error_invalidOperation(1, "CommandListRef_dispatchIndirect() requires bound compute pipeline"))

	if(offset & 15)
		gotoIfError(clean, Error_invalidParameter(
			2, 0, "CommandListRef_dispatchIndirect()::offset has to be 16-byte aligned"
		));

	gotoIfError(clean, CommandListRef_checkDispatchBuffer(device, buffer, offset, sizeof(U32) * 4))

	const BufferRange range = (BufferRange) { .startRange = offset, .endRange = offset + sizeof(U32) * 4 };
	gotoIfError(clean, CommandListRef_transitionBuffer(commandList, buffer, range, ETransitionType_Indirect))

	const DispatchIndirectCmd dispatch = (DispatchIndirectCmd) { .buffer = buffer, .offset = offset };

	gotoIfError(clean, CommandList_append(
		commandList, ECommandOp_DispatchIndirect, Buffer_createRefConst(&dispatch, sizeof(dispatch)), 0
	))

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandList_drawIndirectBase(
	CommandList *commandList,
	DeviceBufferRef *buffer,
	U64 bufferOffset,
	U32 *bufferStride,
	U32 drawCalls,
	Bool indexed
) {

	const U32 minStride = (U32)(indexed ? sizeof(DrawCallIndexed) : sizeof(DrawCallUnindexed));
	const DeviceBuffer *buf = DeviceBufferRef_ptr(buffer);

	if(!buf)
		return Error_nullPointer(0, "CommandList_drawIndirectBase()::buffer is required");

	if(bufferOffset & 15)
		return Error_invalidParameter(
			2, 0, "CommandList_drawIndirectBase()::offset has to be 16-byte aligned"
		);

	if(!*bufferStride)
		*bufferStride = minStride;

	if(*bufferStride < minStride)
		return Error_invalidParameter(
			2, 1, 
			"CommandList_drawIndirectBase()::*bufferStride should be 0 (auto), 16 (unindexed) or 32 (indexed)"
		);

	if (!drawCalls)
		return Error_invalidParameter(
			2, 2, "CommandList_drawIndirectBase() shouldn't be submitted if drawCalls is 0"
		);

	const Error err = CommandListRef_checkDispatchBuffer(
		commandList->device, buffer, bufferOffset, (U64)*bufferStride * drawCalls
	);

	if(err.genericError)
		return err;

	const BufferRange range = (BufferRange) {
		.startRange = bufferOffset,
		.endRange = bufferOffset + *bufferStride * drawCalls
	};

	return CommandListRef_transitionBuffer(commandList, buffer, range, ETransitionType_Indirect);
}

Error CommandListRef_drawIndirect(
	CommandListRef *commandListRef,
	DeviceBufferRef *buffer,
	U64 bufferOffset,
	U32 bufferStride,
	U32 drawCalls,
	Bool indexed
) {

	CommandListRef_validateScope(commandListRef, clean)

	gotoIfError(clean, CommandList_drawIndirectBase(commandList, buffer, bufferOffset, &bufferStride, drawCalls, indexed))

	DrawIndirectCmd draw = (DrawIndirectCmd) {
		.buffer = buffer,
		.bufferOffset = bufferOffset,
		.drawCalls = drawCalls,
		.bufferStride = bufferStride,
		.isIndexed = indexed
	};

	gotoIfError(clean, CommandListRef_drawBase(
		commandListRef, Buffer_createRefConst(&draw, sizeof(draw)), ECommandOp_DrawIndirect
	))

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_drawIndirectCountExt(
	CommandListRef *commandListRef,
	DeviceBufferRef *buffer,
	U64 bufferOffset,
	U32 bufferStride,
	DeviceBufferRef *countBuffer,
	U64 countOffset,
	U32 maxDrawCalls,
	Bool indexed
) {

	CommandListRef_validateScope(commandListRef, clean)

	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);
	gotoIfError(clean, CommandList_drawIndirectBase(commandList, buffer, bufferOffset, &bufferStride, maxDrawCalls, indexed))

	if(!(device->info.capabilities.features & EGraphicsFeatures_MultiDrawIndirectCount))
		gotoIfError(clean, Error_unsupportedOperation(
			0, "CommandListRef_drawIndirectCountExt() requires multiDrawIndirectCount extension, which was missing!"
		))

	gotoIfError(clean, CommandListRef_checkDispatchBuffer(commandList->device, countBuffer, countOffset, sizeof(U32)))

	BufferRange range = (BufferRange) {
		.startRange = countOffset,
		.endRange = countOffset + sizeof(U32)
	};

	gotoIfError(clean, CommandListRef_transitionBuffer(commandList, countBuffer, range, ETransitionType_Indirect))

	DrawIndirectCmd draw = (DrawIndirectCmd) {
		.buffer = buffer,
		.countBufferExt = countBuffer,
		.bufferOffset = bufferOffset,
		.countOffsetExt = countOffset,
		.drawCalls = maxDrawCalls,
		.bufferStride = bufferStride,
		.isIndexed = indexed
	};

	gotoIfError(clean, CommandListRef_drawBase(
		commandListRef, Buffer_createRefConst(&draw, sizeof(draw)), ECommandOp_DrawIndirectCount
	))

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

//Dynamic rendering

Error CommandListRef_startRenderExt(
	CommandListRef *commandListRef,
	I32x2 offset,
	I32x2 size,
	ListAttachmentInfo colors,
	AttachmentInfo depth,
	AttachmentInfo stencil
) {

	Buffer command = Buffer_createNull();
	Lock *toRelease = NULL;

	CommandListRef_validateScope(commandListRef, clean)

	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);

	if(!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering))
		gotoIfError(clean, Error_unsupportedOperation(
			0, "CommandListRef_startRenderExt() requires directRendering extension, which was missing!"
		))

	if(!colors.length && !depth.image && !stencil.image)
		gotoIfError(clean, Error_invalidOperation(
			1, "CommandListRef_startRenderExt() requires DepthStencil and/or colors"
		))

	if(colors.length > 8)
		gotoIfError(clean, Error_outOfBounds(3, colors.length, 8, "CommandListRef_startRenderExt()::colors has to be <=8"))

	I32x2 targetSize = size;
	I32x2 firstSize = I32x2_zero();
	U8 counter = 0;
	EMSAASamples sampleCount = 0;

	for (U64 i = 0; i < colors.length + !!depth.image + !!stencil.image; ++i) {

		Bool isDepth = i == colors.length && depth.image;
		AttachmentInfo info = i < colors.length ? colors.ptr[i] : (isDepth ? depth : stencil);

		if(!info.image)
			continue;

		++counter;
		Bool isStencil = i >= colors.length && !isDepth;

		DeviceResourceVersion version;
		UnifiedTexture texture = TextureRef_getUnifiedTexture(info.image, &version);

		//Swapchain needs to maintain version, so CommandList can be invalidated on resize

		 if(version.resource && !ListDeviceResourceVersion_contains(commandList->activeSwapchains, version, 0, NULL))
			gotoIfError(clean, ListDeviceResourceVersion_pushBackx(&commandList->activeSwapchains, version))

		//TODO: Properly validate this

		if(info.range.levelId >= 1 || info.range.layerId >= 1)
			gotoIfError(clean, Error_outOfBounds(
				4, info.range.levelId >= 1 ? info.range.levelId : info.range.layerId, 1,
				"CommandListRef_startRenderExt() image range.levelId or layerId is invalid"
			))

		//Check generic properties like devices

		if(texture.resource.device != commandList->device)
			gotoIfError(clean, Error_unsupportedOperation(
				4, "CommandListRef_startRenderExt() image belongs to different device"
			))

		if(texture.type != ETextureType_2D)
			gotoIfError(clean, Error_invalidParameter(
				3, (U32)i, "CommandListRef_startRenderExt() image needs to be a 2D texture"
			))

		I32x2 currSize = I32x2_create2(texture.width, texture.height);

		if(counter == 1) {

			firstSize = currSize;

			if(I32x2_any(I32x2_geq(offset, firstSize)))
				gotoIfError(clean, Error_invalidState(
					0, "CommandListRef_startRenderExt() image offset was out of bounds"
				))

			targetSize = I32x2_sub(firstSize, offset);
		}

		else if(I32x2_any(I32x2_lt(currSize, I32x2_add(targetSize, offset))))
			gotoIfError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt() image dimensions are incompatible with others"
			))

		if(TextureRef_isDepthStencil(info.image) != (isDepth || isStencil))
			gotoIfError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt() image had mismatching image types (Color/Depth)"
			))

		if (isDepth && texture.depthFormat == EDepthStencilFormat_S8Ext)
			gotoIfError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt()::depth has format S8, which is not usable for depth attachment"
			))

		if (isStencil && texture.depthFormat < EDepthStencilFormat_StencilStart)
			gotoIfError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt()::stencil has format D16/D32, which is unusable for stencil attachment"
			))

		//Validate MSAA

		if(info.resolveMode >= EMSAAResolveMode_Count)
			gotoIfError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt() image had invalid resolveMode"
			))

		if(info.resolveMode && !info.resolveImage)
			gotoIfError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt() image had resolveMode but no resolveImage"
			))

		if(counter == 1)
			sampleCount = texture.sampleCount;

		else if(sampleCount != texture.sampleCount)
			gotoIfError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt() image had mismatching MSAA setting between others"
			))

		if(info.resolveImage) {

			if(!texture.sampleCount)
				gotoIfError(clean, Error_invalidOperation(
					3, "CommandListRef_startRenderExt() image had resolveImage, while MSAA was off"
				))

			if(TextureRef_isDepthStencil(info.image) != TextureRef_isDepthStencil(info.resolveImage))
				gotoIfError(clean, Error_invalidOperation(
					3, "CommandListRef_startRenderExt() image had resolveImage which didn't match the same type (Color/Depth)"
				))

			DeviceResourceVersion resolveVersion;
			UnifiedTexture resolveTexture = TextureRef_getUnifiedTexture(info.resolveImage, &resolveVersion);

			if (texture.depthFormat) {

				EDepthStencilFormat resolveDF = resolveTexture.depthFormat;
				EDepthStencilFormat texDF = texture.depthFormat;

				if(isStencil && resolveDF < EDepthStencilFormat_StencilStart)
					gotoIfError(clean, Error_invalidOperation(
						3, "CommandListRef_startRenderExt() MSAA resolve image of stencil image needs stencil buffer"
					))

				Bool resolveImageIsCompatible =
					(resolveDF == texDF) || (
						(resolveDF == EDepthStencilFormat_D32S8 || resolveDF == EDepthStencilFormat_D32) &&
						(texDF == EDepthStencilFormat_D32S8 || texDF == EDepthStencilFormat_D32)
					);

				if(isDepth && !resolveImageIsCompatible)
					gotoIfError(clean, Error_invalidOperation(
						3, "CommandListRef_startRenderExt() MSAA resolve image of depth buffer needs compatible depth format"
					))
			}

			else if(texture.textureFormatId != resolveTexture.textureFormatId)
				gotoIfError(clean, Error_invalidOperation(
					3, "CommandListRef_startRenderExt() image and resolveImage have mismatching formats"
				))

			I32x2 resolveSize = I32x2_create2(resolveTexture.width, resolveTexture.height);

			if(I32x2_neq2(resolveSize, targetSize))
				gotoIfError(clean, Error_invalidOperation(
					3, "CommandListRef_startRenderExt() size of MSAA resolve image is incompatible"
				))
		}
	}

	if(!counter)
		gotoIfError(clean, Error_invalidParameter(
			3, 1, "CommandListRef_startRenderExt() didn't provide any render targets"
		))

	if(I32x2_any(I32x2_or(I32x2_leq(targetSize, I32x2_zero()), I32x2_leq(firstSize, I32x2_zero()))))
		gotoIfError(clean, Error_invalidOperation(5, "CommandListRef_startRenderExt() targetSize or firstSize is <=0"))

	if(I32x2_any(I32x2_eq(size, I32x2_zero())))
		size = I32x2_sub(targetSize, offset);

	gotoIfError(clean, CommandListRef_checkBounds(offset, size, 0, 32767))
	gotoIfError(clean, Buffer_createEmptyBytesx(sizeof(StartRenderCmdExt) + sizeof(AttachmentInfoInternal) * 16, &command))

	if(!I32x2_all(I32x2_eq(commandList->currentSize, I32x2_zero())))
		gotoIfError(clean, Error_invalidOperation(
			2, "CommandListRef_startRenderExt() can't already have a render started!"
		))

	if(stencil.color.coloru[0] >> 8)
		gotoIfError(clean, Error_invalidOperation(
			5, "CommandListRef_startRenderExt()::stencil clear color should be 0-255 (0-0xFF)"
		))

	if(!stencil.image && I32x4_any(I32x4_load4(stencil.color.colori)))
		gotoIfError(clean, Error_invalidOperation(
			5, "CommandListRef_startRenderExt()::stencil clear value can't be non zero if there's no stencil bound"
		))

	if(depth.color.colorf[0] < 0 || depth.color.colorf[0] > 1)
		gotoIfError(clean, Error_invalidOperation(
			4, "CommandListRef_startRenderExt()::depth clear color should be 0-1"
		))

	if(!depth.image && F32x4_any(F32x4_load4(depth.color.colorf)))
		gotoIfError(clean, Error_invalidOperation(
			4, "CommandListRef_startRenderExt()::depth clear value can't be non zero if there's no depth buffer bound"
		))

	if(F32x4_any(F32x4_yzw(F32x4_load4(depth.color.colorf))))
		gotoIfError(clean, Error_invalidOperation(
			4, "CommandListRef_startRenderExt()::depth clear value can only contain info in the first float"
		))

	if(I32x4_any(I32x4_yzw(I32x4_load4(stencil.color.colori))))
		gotoIfError(clean, Error_invalidOperation(
			5, "CommandListRef_startRenderExt()::stencil clear value can only contain info in the first uint8"
		))

	StartRenderCmdExt *startRender = (StartRenderCmdExt*)command.ptr;

	*startRender = (StartRenderCmdExt) {
		.offset = offset,
		.size = size,
		.resolveDepthMode = depth.resolveMode,
		.resolveStencilMode = stencil.resolveMode,
		.colorCount = (U8) colors.length,
		.clearStencil = (U8) stencil.color.coloru[0],
		.clearDepth = depth.color.colorf[0],
		.depthRange = depth.range,
		.stencilRange = stencil.range,
		.depth = depth.image,
		.stencil = stencil.image,
		.resolveDepth = depth.resolveImage,
		.resolveStencil = stencil.resolveImage
	};

	if(depth.image)
		startRender->flags |= EStartRenderFlags_Depth;

	if(depth.load == ELoadAttachmentType_Clear)
		startRender->flags |= EStartRenderFlags_ClearDepth;

	else if(depth.load == ELoadAttachmentType_Preserve)
		startRender->flags |= EStartRenderFlags_PreserveDepth;

	if(depth.unusedAfterRender)
		startRender->flags |= EStartRenderFlags_DepthUnusedAfterRender;

	if(stencil.image)
		startRender->flags |= EStartRenderFlags_Stencil;

	if(stencil.load == ELoadAttachmentType_Clear)
		startRender->flags |= EStartRenderFlags_ClearStencil;

	else if(stencil.load == ELoadAttachmentType_Preserve)
		startRender->flags |= EStartRenderFlags_PreserveStencil;

	if(stencil.unusedAfterRender)
		startRender->flags |= EStartRenderFlags_StencilUnusedAfterRender;

	if(!stencil.image && (
		startRender->flags & EStartRenderFlags_StencilFlags || stencil.range.layerId || stencil.range.levelId
	))
		gotoIfError(clean, Error_invalidOperation(
			5, "CommandListRef_startRenderExt()::stencil had values set, but didn't have a valid image"
		))

	if(!depth.image && (
		startRender->flags & EStartRenderFlags_DepthFlags || depth.range.layerId || depth.range.levelId
	))
		gotoIfError(clean, Error_invalidOperation(
			4, "CommandListRef_startRenderExt()::depth had values set, but didn't have a valid image"
		))

	AttachmentInfoInternal *attachments = (AttachmentInfoInternal*)(startRender + 1);
	counter = 0;

	for (U64 i = 0; i < colors.length + !!depth.image + !!stencil.image; ++i) {

		AttachmentInfo info = i == colors.length && depth.image ? depth : (i >= colors.length ? stencil : colors.ptr[i]);

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

			if(state->type != transition.type)
				gotoIfError(clean, Error_invalidOperation(
					4, "CommandListRef_startRenderExt()::colors[i] or depthStencil was already transitioned!"
				))
		}

		else gotoIfError(clean, ListTransitionInternal_pushBackx(&commandList->pendingTransitions, transition))

		//Transition resolve image

		if(info.resolveImage) {

			transition.resource = info.resolveImage;
			transition.range = (ResourceRange) { 0 };				//TODO: Range for resolveImage
			transition.type = ETransitionType_RenderTargetWrite;

			if(CommandListRef_isBound(commandList, transition.resource, transition.range, &state)) {

				if(state->type != transition.type)
					gotoIfError(clean, Error_invalidOperation(
						4,
						"CommandListRef_startRenderExt()::colors[i] or depthStencil resolve target was already resolved"
					))
			}

			else gotoIfError(clean, ListTransitionInternal_pushBackx(&commandList->pendingTransitions, transition))
		}
	}

	gotoIfError(clean, CommandList_append(
		commandList,
		ECommandOp_StartRenderingExt,
		Buffer_createRefConst(startRender, sizeof(StartRenderCmdExt) + sizeof(AttachmentInfoInternal) * counter),
		0
	))

	commandList->currentSize = size;
	commandList->boundImageCount = (U8) colors.length;

	for (U8 i = 0; i < commandList->boundImageCount; ++i) {
		AttachmentInfo info = colors.ptr[i];
		commandList->boundImages[i] = (ImageAndRange) { .image = info.image, .range = info.range };
	}

	//Combine stencil and depth back to one format

	EDepthStencilFormat depthFormat = EDepthStencilFormat_None;

	if(depth.image)
		switch (TextureRef_getUnifiedTexture(depth.image, NULL).depthFormat) {

			//If stencil is missing, we try to make the format stencil-less to indicate we won't need to write.

			case EDepthStencilFormat_D16:
				depthFormat = EDepthStencilFormat_D16;
				break;

			case EDepthStencilFormat_D32:
			case EDepthStencilFormat_D32S8:
				depthFormat = stencil.image ? EDepthStencilFormat_D32S8 : EDepthStencilFormat_D32;
				break;

			//D24S8 can't be made stencil-less

			case EDepthStencilFormat_D24S8Ext:
				depthFormat = EDepthStencilFormat_D24S8Ext;
				break;
		}

	if(!depth.image && stencil.image)
		depthFormat = EDepthStencilFormat_S8Ext;

	commandList->boundDepthFormat = depthFormat;
	commandList->boundSampleCount = sampleCount;

clean:

	if(toRelease)
		Lock_unlock(toRelease);

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	Buffer_freex(&command);
	return err;
}

Error CommandListRef_endRenderExt(CommandListRef *commandListRef) {

	CommandListRef_validateScope(commandListRef, clean)
	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);

	if(!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering))
		gotoIfError(clean, Error_unsupportedOperation(
			0, "CommandListRef_endRenderExt() requires directRendering extension, which was missing!"
		))

	if(I32x2_any(I32x2_eq(commandList->currentSize, I32x2_zero())))
		gotoIfError(clean, Error_invalidOperation(
			1, "CommandListRef_endRenderExt() requires startRenderExt to be called first"
		))

	gotoIfError(clean, CommandList_append(commandList, ECommandOp_EndRenderingExt, Buffer_createNull(), 0))

	commandList->currentSize = I32x2_zero();
	commandList->tempStateFlags &= ~(ECommandStateFlags_AnyScissor | ECommandStateFlags_AnyViewport);

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

//Debug markers

Error CommandList_markerDebugExt(CommandListRef *commandListRef, F32x4 color, CharString name, ECommandOp op) {

	Buffer buf = Buffer_createNull();
	CommandListRef_validateScope(commandListRef, clean)

	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);

	if(!(device->info.capabilities.features & EGraphicsFeatures_DebugMarkers))		//NO-OP
		return Error_none();

	gotoIfError(clean, Buffer_createEmptyBytesx(sizeof(color) + CharString_length(name) + 1, &buf))

	Buffer_copy(buf, Buffer_createRefConst(&color, sizeof(color)));

	Buffer_copy(
		Buffer_createRef((U8*)buf.ptr + sizeof(color), CharString_length(name)), 
		CharString_bufferConst(name)
	);

	gotoIfError(clean, CommandList_append(commandList, op, buf, 1))

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	Buffer_freex(&buf);
	return err;
}

Error CommandListRef_addMarkerDebugExt(CommandListRef *commandListRef, F32x4 color, CharString name) {
	return CommandList_markerDebugExt(commandListRef, color, name, ECommandOp_AddMarkerDebugExt);
}

Error CommandListRef_startRegionDebugExt(CommandListRef *commandListRef, F32x4 color, CharString name) {

	CommandListRef_validateScope(commandListRef, clean)

	if(!CharString_length(name))
		gotoIfError(clean, Error_nullPointer(2, "CommandListRef_startRegionDebugExt()::name is required"))

	if(commandList && commandList->debugRegionStack == U8_MAX)
		gotoIfError(clean, Error_outOfBounds(
			0, U8_MAX, U8_MAX, "CommandListRef_startRegionDebugExt() can only have depth of 255."
		))

	gotoIfError(clean, CommandList_markerDebugExt(commandListRef, color, name, ECommandOp_StartRegionDebugExt))

	++commandList->debugRegionStack;

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_endRegionDebugExt(CommandListRef *commandListRef) {

	CommandListRef_validateScope(commandListRef, clean)

	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);

	if(!(device->info.capabilities.features & EGraphicsFeatures_DebugMarkers))		//NO-OP
		return Error_none();

	if (!commandList->debugRegionStack)
		gotoIfError(clean, Error_invalidOperation(1, "CommandListRef_endRegionDebugExt() requires startRegion first."))

	gotoIfError(clean, CommandList_append(commandList, ECommandOp_EndRegionDebugExt, Buffer_createNull(), 0))

	--commandList->debugRegionStack;

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

//Free and create

Bool CommandList_free(CommandList *cmd, Allocator alloc) {

	(void)alloc;

	Lock_free(&cmd->lock);

	for (U64 i = 0; i < cmd->resources.length; ++i) {

		RefPtr **ptr = cmd->resources.ptrNonConst + i;

		if(*ptr)
			RefPtr_dec(ptr);
	}

	ListCommandOpInfo_freex(&cmd->commandOps);
	ListRefPtr_freex(&cmd->resources);
	ListCommandScope_freex(&cmd->activeScopes);
	ListTransitionInternal_freex(&cmd->transitions);
	ListTransitionInternal_freex(&cmd->pendingTransitions);
	ListDeviceResourceVersion_freex(&cmd->activeSwapchains);
	Buffer_freex(&cmd->data);

	GraphicsDeviceRef_dec(&cmd->device);

	return true;
}

Error GraphicsDeviceRef_createCommandList(
	GraphicsDeviceRef *deviceRef,
	U64 commandListLen,
	U64 estimatedCommandCount,
	U64 estimatedResources,
	Bool allowResize,
	CommandListRef **commandListRef
) {

	Error err = RefPtr_createx(
		(U32) sizeof(CommandList),
		(ObjectFreeFunc) CommandList_free,
		(ETypeId) EGraphicsTypeId_CommandList,
		commandListRef
	);

	if(err.genericError)
		return err;

	CommandList *commandList = CommandListRef_ptr(*commandListRef);

	gotoIfError(clean, Buffer_createEmptyBytesx(commandListLen, &commandList->data))

	gotoIfError(clean, ListCommandOpInfo_reservex(&commandList->commandOps, estimatedCommandCount))
	gotoIfError(clean, ListRefPtr_reservex(&commandList->resources, estimatedResources))
	gotoIfError(clean, ListCommandScope_reservex(&commandList->activeScopes, 16))
	gotoIfError(clean, ListTransitionInternal_reservex(&commandList->transitions, estimatedResources))
	gotoIfError(clean, ListTransitionInternal_reservex(&commandList->pendingTransitions, 32))
	commandList->lock = Lock_create();

	GraphicsDeviceRef_inc(deviceRef);
	commandList->device = deviceRef;
	commandList->allowResize = allowResize;

	goto success;

clean:
	CommandListRef_dec(commandListRef);

success:
	return err;
}
