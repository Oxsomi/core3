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

//graphics/generic/command_list_cmds.c

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
		commandList, ECommandOp_SetStencil, Buffer_createRefConst(stencilValue, sizeof(stencilValue)), 0, e_rr
	));

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
			!srcRef ? 1 : 2, regions.length, U32_MAX, "CommandListRef_copyImage()::src and dst are required"
		));

	for (U64 i = 0; i < 2; ++i)
		if(!TextureRef_isTexture(i ? dstRef : srcRef))
			retError(clean, Error_invalidParameter(
				i ? 1 : 2, 0,
				"CommandListRef_copyImage()::src and dst should be a texture "
				"(Swapchain, DepthStencil, RenderTexture, DeviceTexture)"
			));

	//Validate depth stencil

	Bool isDepthStencil = TextureRef_isDepthStencil(dstRef);

	if(isDepthStencil != TextureRef_isDepthStencil(srcRef))
		retError(clean, Error_invalidParameter(
			1, 0, "CommandListRef_copyImage()::src and dst should be DepthStencil if one of them is to be compatible"
		));

	DeviceResourceVersion v;
	UnifiedTexture src = TextureRef_getUnifiedTexture(srcRef, &v);
	UnifiedTexture dst = TextureRef_getUnifiedTexture(dstRef, &v);

	if (isDepthStencil)
		retError(clean, Error_invalidParameter(
			1, 0, "CommandListRef_copyImage()::src and dst aren't allowed to be depth stencil"
		));

	//Ensure both formats are the same

	if(src.textureFormatId != dst.textureFormatId)
		retError(clean, Error_invalidParameter(
			1, 5, "CommandListRef_copyImage()::src and dst require the same texture format"
		));

	//Validate devices

	if(src.resource.device != commandList->device || dst.resource.device != commandList->device)
		retError(clean, Error_invalidParameter(
			1, 6, "CommandListRef_copyImage()::src and dst require the same device as the CommandList"
		));

	//Validate copy

	for(U64 i = 0; i < regions.length; ++i) {

		CopyImageRegion clearImage = regions.ptr[i];

		//Validate levelId

		if(clearImage.dstLevelId || clearImage.srcLevelId)        //TODO: Allow levels
			retError(clean, Error_invalidParameter(
				1, 6, "CommandListRef_copyImage()::regions[i].src/dstLevelId is out of bounds"
			));
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
			commandList, ptrs[i], (ImageRange) { 0 }, types[i], EPipelineStage_Count, e_rr
		));

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

//Debug markers

Bool CommandList_markerDebugExt(
	CommandListRef *commandListRef, F32x4 color, const CharString *name, ECommandOp op, Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = commandListRef ? GraphicsDeviceRef_getAlloc(CommandListRef_ptr(commandListRef)->device) : NULL;

	Buffer buf = Buffer_createNull();
	CommandListRef_validateScope(commandListRef, clean)

	U64 namel = name ? CharString_length(*name) : 0;

	U64 len = sizeof(color) + namel + 1;
	len = (len + 15) &~ 15;                                        //Align to 16-byte to not mess up next instruction alignment

	gotoIfError3(clean, Buffer_createUninitializedBytes(len, alloc, &buf, e_rr));

	Buffer_memcpy(buf, Buffer_createRefConst(&color, sizeof(color)));

	if(namel)
		Buffer_memcpy(
			Buffer_createRef(buf.ptrNonConst + sizeof(color), namel),
			CharString_bufferConst(*name)
		);

	buf.ptrNonConst[sizeof(color) + namel] = '\0';

	gotoIfError3(clean, CommandList_append(commandList, op, buf, 1, e_rr));

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	Buffer_free(&buf, alloc);
	return s_uccess;
}

Bool CommandListRef_addMarkerDebugExt(CommandListRef *commandListRef, F32x4 color, const CharString *name, Error *e_rr) {
	return CommandList_markerDebugExt(commandListRef, color, name, ECommandOp_AddMarkerDebugExt, e_rr);
}

Bool CommandListRef_startRegionDebugExt(CommandListRef *commandListRef, F32x4 color, const CharString *name, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	if(!name || !CharString_length(*name))
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
