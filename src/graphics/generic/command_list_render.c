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

//graphics/generic/command_list_render.c

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

//Dynamic rendering

Bool CommandListRef_startRenderExt(
	CommandListRef *commandListRef,
	I32x2 offset,
	I32x2 size,
	const ListAttachmentInfo *colors,
	const DepthStencilAttachmentInfo *depthStencil,
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

	if((!colors || !colors->length) && (!depthStencil || !depthStencil->image))
		retError(clean, Error_invalidOperation(
			1, "CommandListRef_startRenderExt() requires DepthStencil and/or colors"));

	if(colors && colors->length > 8)
		retError(clean, Error_outOfBounds(3, colors->length, 8, "CommandListRef_startRenderExt()::colors has to be <=8"));

	I32x2 targetSize = size;
	I32x2 firstSize = I32x2_zero;
	U8 counter = 0;
	EMSAASamples sampleCount = 0;

	for (U64 i = 0; i < (colors ? colors->length : 0) + (depthStencil && depthStencil->image); ++i) {

		Bool isDepthStencil = i == (colors ? colors->length : 0);
		AttachmentInfo info;

		if(isDepthStencil)
			info = (AttachmentInfo) {
				.range = depthStencil->range,
				.image = depthStencil->image,
				.resolveImage = depthStencil->resolveImage,
				.resolveRange = depthStencil->resolveImageRange,
				.resolveMode = depthStencil->depthStencilResolve
			};

		else info = colors->ptr[i];

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
				"CommandListRef_startRenderExt() image range.levelId or layerId is invalid"
			));

		if(info.readOnly && info.load == ELoadAttachmentType_Clear)
			retError(clean, Error_unsupportedOperation(
				7, "CommandListRef_startRenderExt() render target is set as clear but also read only"
			));

		//Check generic properties like devices

		if(texture.resource.device != commandList->device)
			retError(clean, Error_unsupportedOperation(
				4, "CommandListRef_startRenderExt() image belongs to different device"
			));

		if(texture.type != ETextureType_2D)
			retError(clean, Error_invalidParameter(
				3, (U32)i, "CommandListRef_startRenderExt() image needs to be a 2D texture"
			));

		I32x2 currSize = I32x2_create2(texture.width, texture.height);

		if(counter == 1) {

			firstSize = currSize;

			if(I32x2_any(I32x2_geq(offset, firstSize)))
				retError(clean, Error_invalidState(
					0, "CommandListRef_startRenderExt() image offset was out of bounds"
				));

			targetSize = I32x2_sub(firstSize, offset);
		}

		else if(I32x2_any(I32x2_lt(currSize, I32x2_add(targetSize, offset))))
			retError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt() image dimensions are incompatible with others"
			));

		if(TextureRef_isDepthStencil(info.image) != isDepthStencil)
			retError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt() image had mismatching image types (Color/Depth)"
			));

		//Validate MSAA

		if(info.resolveMode >= EMSAAResolveMode_Count)
			retError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt() image had invalid resolveMode"
			));

		if(info.resolveMode && !info.resolveImage)
			retError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt() image had resolveMode but no resolveImage"
			));

		if(counter == 1)
			sampleCount = texture.sampleCount;

		else if(sampleCount != texture.sampleCount)
			retError(clean, Error_invalidOperation(
				3, "CommandListRef_startRenderExt() image had mismatching MSAA setting between others"
			));

		if(info.resolveImage) {

			//TODO: Properly validate this

			if(info.resolveRange.levelId >= 1 || info.resolveRange.layerId >= 1)
				retError(clean, Error_outOfBounds(
					4, info.resolveRange.levelId >= 1 ? info.resolveRange.levelId : info.resolveRange.layerId, 1,
					"CommandListRef_startRenderExt() image range.levelId or layerId is invalid"
				));

			if(!texture.sampleCount)
				retError(clean, Error_invalidOperation(
					3, "CommandListRef_startRenderExt() image had resolveImage, while MSAA was off"
				));

			if(TextureRef_isDepthStencil(info.image) != TextureRef_isDepthStencil(info.resolveImage))
				retError(clean, Error_invalidOperation(
					3, "CommandListRef_startRenderExt() image had resolveImage which didn't match the same type (Color/Depth)"
				));

			DeviceResourceVersion resolveVersion;
			UnifiedTexture resolveTexture = TextureRef_getUnifiedTexture(info.resolveImage, &resolveVersion);

			if (texture.depthFormat) {
				if(texture.depthFormat != resolveTexture.depthFormat)
					retError(clean, Error_invalidOperation(
						3, "CommandListRef_startRenderExt() MSAA resolve image of depth buffer needs compatible depth format"
					));
			}

			else if(texture.textureFormatId != resolveTexture.textureFormatId)
				retError(clean, Error_invalidOperation(
					3, "CommandListRef_startRenderExt() image and resolveImage have mismatching formats"
				));

			I32x2 resolveSize = I32x2_create2(resolveTexture.width, resolveTexture.height);

			if(I32x2_neq2(resolveSize, targetSize))
				retError(clean, Error_invalidOperation(
					3, "CommandListRef_startRenderExt() size of MSAA resolve image is incompatible"
				));
		}
	}

	if(!counter)
		retError(clean, Error_invalidParameter(
			3, 1, "CommandListRef_startRenderExt() didn't provide any render targets"
		));

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
			2, "CommandListRef_startRenderExt() can't already have a render started!"
		));

	if(depthStencil && !depthStencil->image && depthStencil->clearStencil)
		retError(clean, Error_invalidOperation(
			5, "CommandListRef_startRenderExt()::stencil clear value can't be non zero if there's no stencil bound"
		));
		
	if(depthStencil && (depthStencil->clearDepth < 0 || depthStencil->clearDepth > 1))
		retError(clean, Error_invalidOperation(4, "CommandListRef_startRenderExt()::depth clear should be 0-1"));

	if(depthStencil && !depthStencil->image && depthStencil->clearDepth)
		retError(clean, Error_invalidOperation(
			4, "CommandListRef_startRenderExt()::depth clear value can't be non zero if there's no depth buffer bound"
		));

	StartRenderCmdExt *startRender = (StartRenderCmdExt*)command.ptr;

	*startRender = (StartRenderCmdExt) {
		.offset = offset,
		.size = size,
		.resolveDepthStencilMode = !depthStencil ? EMSAAResolveMode_Average : depthStencil->depthStencilResolve,
		.colorCount = (U8) (!colors ? 0 : colors->length),
		.clearStencil = (U8) (!depthStencil ? 0 : depthStencil->clearStencil),
		.clearDepth = !depthStencil ? 0 : depthStencil->clearDepth,
		.depthStencilRange = !depthStencil ? (ImageRange) { 0 } : depthStencil->range,
		.depthStencil = !depthStencil ? NULL : depthStencil->image,
		.resolveDepthStencil = !depthStencil ? NULL : depthStencil->resolveImage,
		.resolveDepthStencilRange = !depthStencil ? (ImageRange) { 0 } : depthStencil->resolveImageRange
	};

	if(depthStencil) {

		UnifiedTexture depthStencilImg = TextureRef_getUnifiedTexture(!depthStencil ? NULL : depthStencil->image, NULL);

		if(depthStencilImg.depthFormat != EDepthStencilFormat_S8X24Ext)
			startRender->flags |= EStartRenderFlags_Depth;

		if(depthStencil->depthLoad == ELoadAttachmentType_Clear)
			startRender->flags |= EStartRenderFlags_ClearDepth;

		else if(depthStencil->depthLoad == ELoadAttachmentType_Preserve)
			startRender->flags |= EStartRenderFlags_PreserveDepth;

		if(depthStencil->depthUnusedAfterRender)
			startRender->flags |= EStartRenderFlags_DepthUnusedAfterRender;

		if(depthStencil->depthReadOnly)
			startRender->flags |= EStartRenderFlags_DepthReadOnly;

		if(depthStencilImg.depthFormat >= EDepthStencilFormat_StencilStart)
			startRender->flags |= EStartRenderFlags_Stencil;

		if(depthStencil->stencilLoad == ELoadAttachmentType_Clear)
			startRender->flags |= EStartRenderFlags_ClearStencil;

		else if(depthStencil->stencilLoad == ELoadAttachmentType_Preserve)
			startRender->flags |= EStartRenderFlags_PreserveStencil;

		if(depthStencil->stencilUnusedAfterRender)
			startRender->flags |= EStartRenderFlags_StencilUnusedAfterRender;

		if(depthStencil->stencilReadOnly)
			startRender->flags |= EStartRenderFlags_StencilReadOnly;

		if(depthStencil->stencilReadOnly && depthStencil->stencilLoad == ELoadAttachmentType_Clear)
			retError(clean, Error_invalidOperation(
				6, "CommandListRef_startRenderExt()::stencil was set to clear but was readonly"
			));

		if(depthStencil->depthReadOnly && depthStencil->depthLoad == ELoadAttachmentType_Clear)
			retError(clean, Error_invalidOperation(
				6, "CommandListRef_startRenderExt()::depth was set to clear but was readonly"
			));

		if(!depthStencil->image && (depthStencil->range.layerId || depthStencil->range.levelId || startRender->flags))
			retError(clean, Error_invalidOperation(
				5, "CommandListRef_startRenderExt()::depthStencil had values set, but didn't have a valid image"
			));
	}

	AttachmentInfoInternal *attachments = (AttachmentInfoInternal*)(startRender + 1);
	counter = 0;

	for (U64 i = 0; i < (colors ? colors->length : 0) + (depthStencil && depthStencil->image); ++i) {

		AttachmentInfo info;

		if(i == (colors ? colors->length : 0))
			info = (AttachmentInfo) {
				.range = depthStencil->range,
				.image = depthStencil->image,
				.resolveImage = depthStencil->resolveImage,
				.resolveRange = depthStencil->resolveImageRange
			};

		else info = colors->ptr[i];

		if (!info.image)
			continue;

		if(i < colors->length) {

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

			if(i < (colors ? colors->length : 0) || state->type != transition.type)
				retError(clean, Error_invalidOperation(
					4, "CommandListRef_startRenderExt()::colors[i] or depthStencil was already transitioned!"
				));
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
						4, "CommandListRef_startRenderExt()::colors[i] or depthStencil resolve target was already resolved"
					));
			}

			else gotoIfError3(clean, ListTransitionInternal_pushBack(
				&commandList->pendingTransitions, transition, alloc, e_rr
			));
		}
	}

	gotoIfError3(clean, CommandList_append(
		commandList,
		ECommandOp_StartRenderingExt,
		Buffer_createRefConst(startRender, sizeof(StartRenderCmdExt) + sizeof(AttachmentInfoInternal) * counter),
		0, e_rr));

	//A clear load is a side effect all by itself, so a pass without draws still has to keep its scope alive;
	// otherwise endScope rewinds the scope as untouched and the clear silently never reaches the GPU

	if(startRender->clearMask || (startRender->flags & (EStartRenderFlags_ClearDepth | EStartRenderFlags_ClearStencil)))
		commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

	commandList->currentSize = size;
	commandList->boundImageCount = (U8) (!colors ? 0 : colors->length);

	for (U8 i = 0; i < commandList->boundImageCount; ++i)
		commandList->boundImages[i] = (ImageAndRange) { .image = colors->ptr[i].image, .range = colors->ptr[i].range };

	//Combine stencil and depth back to one format

	EDepthStencilFormat depthFormat = EDepthStencilFormat_None;

	if(depthStencil && depthStencil->image)
		depthFormat = TextureRef_getUnifiedTexture(depthStencil->image, NULL).depthFormat;

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
			0, "CommandListRef_endRenderExt() requires directRendering extension, which was missing!"
		));

	if(I32x2_any(I32x2_eq(commandList->currentSize, I32x2_zero)))
		retError(clean, Error_invalidOperation(
			1, "CommandListRef_endRenderExt() requires startRenderExt to be called first"
		));

	gotoIfError3(clean, CommandList_append(commandList, ECommandOp_EndRenderingExt, Buffer_createNull(), 0, e_rr));

	commandList->currentSize = I32x2_zero;
	commandList->tempStateFlags &= ~(ECommandStateFlags_AnyScissor | ECommandStateFlags_AnyViewport);

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}
