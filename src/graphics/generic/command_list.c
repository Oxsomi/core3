/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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

#include "graphics/generic/command_list.h"
#include "graphics/generic/device.h"
#include "graphics/generic/swapchain.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/log.h"
#include "types/buffer.h"
#include "types/string.h"
#include "types/error.h"

Error CommandListRef_dec(CommandListRef **cmd) {
	return !RefPtr_dec(cmd) ? Error_invalidOperation(0) : Error_none();
}

Error CommandListRef_add(CommandListRef *cmd) {
	return cmd ? (!RefPtr_inc(cmd) ? Error_invalidOperation(0) : Error_none()) : Error_nullPointer(0);
}

#define CommandListRef_validate(v)						\
														\
	if(!(v))											\
		return Error_nullPointer(0);					\
														\
	CommandList *commandList = CommandListRef_ptr(v);	\
														\
	if(commandList->state != ECommandListState_Open)	\
		return Error_invalidOperation(0);

Error CommandListRef_clear(CommandListRef *commandListRef) {

	CommandListRef_validate(commandListRef);

	for (U64 i = 0; i < commandList->resources.length; ++i) {

		RefPtr **ptr = (RefPtr**)commandList->resources.ptr + i;

		if(*ptr)
			RefPtr_dec(ptr);
	}

	List_clear(&commandList->callstacks);
	List_clear(&commandList->commandOps);
	List_clear(&commandList->resources);

	commandList->next = 0;

	return Error_none();
}

Error CommandListRef_begin(CommandListRef *commandListRef, Bool doClear) {

	if(!commandListRef)
		return Error_nullPointer(0);

	CommandList *commandList = CommandListRef_ptr(commandListRef);

	if(commandList->state == ECommandListState_Open)
		return Error_invalidOperation(0);

	commandList->state = ECommandListState_Open;
	return doClear ? CommandListRef_clear(commandListRef) : Error_none();
}

Error CommandListRef_end(CommandListRef *commandListRef) {
	CommandListRef_validate(commandListRef);
	commandList->state = ECommandListState_Closed;
	return Error_none();
}

Error CommandListRef_append(CommandListRef *commandListRef, ECommandOp op, Buffer buf, List objects, U32 extraSkipStacktrace) {

	CommandListRef_validate(commandListRef);

	U64 len = Buffer_length(buf);

	if(len > U32_MAX)
		return Error_outOfBounds(2, len, U32_MAX);

	if(commandList->next + len > Buffer_length(commandList->data))
		return Error_outOfMemory(0);

	if(objects.length && objects.stride != sizeof(RefPtr*))
		return Error_invalidParameter(3, 0);

	CommandOpInfo info = (CommandOpInfo) {
		.op = op,
		.opSize = (U32) len
	};

	Error err = List_pushBackx(&commandList->commandOps, Buffer_createConstRef(&info, sizeof(info)));

	if(err.genericError)
		return err;

	_gotoIfError(clean, List_reservex(&commandList->resources, commandList->resources.length + objects.length));

	for (U64 i = 0; i < objects.length; ++i) {

		RefPtr *ptr = ((RefPtr**) objects.ptr)[i];

		Buffer bufi = Buffer_createConstRef(&ptr, sizeof(ptr));

		if(!List_contains(commandList->resources, bufi, 0)) {						//TODO: hashSet
			RefPtr_inc(ptr);		//CommandList will keep resource alive.
			_gotoIfError(clean, List_pushBackx(&commandList->resources, bufi));
		}
	}

	if(len) {
		Buffer_copy(Buffer_createRef((U8*)commandList->data.ptr + commandList->next, len), buf);
		commandList->next += len;
	}

	#ifndef NDEBUG

		void *stackTrace[32];
		Log_captureStackTrace(stackTrace, 32, 1 + extraSkipStacktrace);

		_gotoIfError(clean, List_pushBackx(
			&commandList->callstacks, Buffer_createConstRef(stackTrace, sizeof(stackTrace))
		));

	#endif

	goto success;

clean:
	List_popBack(&commandList->commandOps, Buffer_createNull());

success:
	return err;
}

Error CommandListRef_checkBounds(I32x2 offset, I32x2 size, I32 lowerBound1, I32 upperBound1) {
	
	if(I32x2_any(I32x2_leq(size, I32x2_zero())))
		return Error_invalidParameter(1, 0);
	
	I32x2 upperBound = I32x2_xx2(upperBound1);
	I32x2 lowerBound = I32x2_xx2(lowerBound1);

	if(I32x2_any(I32x2_gt(size, upperBound)))
		return Error_invalidParameter(1, 0);

	if(I32x2_any(I32x2_lt(offset, lowerBound)))
		return Error_invalidParameter(0, 0);

	if(I32x2_any(I32x2_gt(offset, upperBound)))
		return Error_invalidParameter(0, 1);

	I32x2 end = I32x2_add(offset, size);

	if(I32x2_any(I32x2_gt(end, upperBound)))
	   return Error_invalidParameter(0, 2);

	if(I32x2_any(I32x2_lt(end, lowerBound)))
	   return Error_invalidParameter(0, 3);

	return Error_none();
}

Error CommandListRef_setViewportCmd(CommandListRef *commandListRef, I32x2 offset, I32x2 size, ECommandOp op) {

	CommandList *commandList = CommandListRef_ptr(commandListRef);

	if(commandList && !(commandList->flags & ECommandListFlag_hasBoundColor))
		return Error_invalidOperation(0);

	Error boundsCheck = CommandListRef_checkBounds(offset, size, -32'768, 32'767);

	if(boundsCheck.genericError)
		return boundsCheck;

	I32x4 values = I32x4_create2_2(offset, size);
	return CommandListRef_append(commandListRef, op, Buffer_createConstRef(&values, sizeof(values)), (List) { 0 }, 1);
}

Error CommandListRef_setViewport(CommandListRef *commandListRef, I32x2 offset, I32x2 size) {
	CommandListRef_setViewportCmd(commandListRef, offset, size, ECommandOp_setViewport);
}

Error CommandListRef_setScissor(CommandListRef *commandListRef, I32x2 offset, I32x2 size) {
	CommandListRef_setViewportCmd(commandListRef, offset, size, ECommandOp_setScissor);
}

Error CommandListRef_setViewportAndScissor(CommandListRef *commandListRef, I32x2 offset, I32x2 size) {
	CommandListRef_setViewportCmd(commandListRef, offset, size, ECommandOp_setViewportAndScissor);
}

Error CommandListRef_setStencil(CommandListRef *commandListRef, U8 stencilValue) {
	return CommandListRef_append(
		commandListRef, ECommandOp_setStencil, Buffer_createConstRef(&stencilValue, 1), (List) { 0 }, 0
	);
}

Error CommandListRef_clearImages(CommandListRef *commandList, List clearImages) {

	if(!clearImages.length)
		return Error_nullPointer(1);

	if(clearImages.stride != sizeof(ClearImage))
		return Error_invalidParameter(1, 0);

	if(clearImages.length > U32_MAX)
		return Error_outOfBounds(1, clearImages.length, U32_MAX);

	Buffer buf = Buffer_createNull();

	List refs = List_createEmpty(sizeof(RefPtr*));
	Error err = List_resizex(&refs, clearImages.length);

	if(err.genericError)
		return err;

	RefPtr **ptr = (RefPtr**)refs.ptr;

	for(U64 i = 0; i < clearImages.length; ++i) {

		ClearImage clearImage = ((ClearImage*)clearImages.ptr)[i];
		RefPtr *image = clearImage.image;

		if(!image)
			_gotoIfError(clean, Error_nullPointer(1));

		if(image->typeId != EGraphicsTypeId_Swapchain)
			_gotoIfError(clean, Error_invalidParameter(1, 0));

		//Check if range conflicts 

		for (U64 j = 0; j < i; ++j) {

			ClearImage clearImagej = ((ClearImage*)clearImages.ptr)[j];
			RefPtr *imagej = clearImagej.image;

			if(imagej != image)
				continue;

			//TODO: Multiple types of images.
			//		We have to loop through all subresources that are covered 
			//		and check if they conflict.
			//		For now, only swapchain is supported w/o layers and levels.
			//		So in that case just mentioning the image twice is enough to conflict.

			_gotoIfError(clean, Error_alreadyDefined(0));
		}

		ptr[i] = image;
	}

	//Convert to refs and buffer
	
	_gotoIfError(clean, Buffer_createEmptyBytesx(List_bytes(clearImages) + sizeof(U32), &buf));

	*(U32*)buf.ptr = (U32) clearImages.length;
	Buffer_copy(Buffer_createRef((U8*) buf.ptr + sizeof(U32), List_bytes(clearImages)), List_bufferConst(clearImages));

	_gotoIfError(clean, CommandListRef_append(commandList, ECommandOp_clearImages, buf, refs, 0));

clean:

	Buffer_freex(&buf);
	List_freex(&refs);

	return err;
}

Error CommandListRef_clearImageu(CommandListRef *commandListRef, const U32 coloru[4], ImageRange range, RefPtr *image) {

	ClearImage clearImage = (ClearImage) {
		.image = image,
		.range = range
	};

	Buffer_copy(Buffer_createRef(clearImage.color, sizeof(F32x4)), Buffer_createConstRef(coloru, sizeof(F32x4)));

	List clearImages = (List) { 0 };
	Error err = List_createConstRef((const U8*) &clearImage, 1, sizeof(ClearImage), &clearImages);

	if(err.genericError)
		return err;

	return CommandListRef_clearImages(commandListRef, clearImages);
}

Error CommandListRef_clearImagei(CommandListRef *commandListRef, I32x4 color, ImageRange range, RefPtr *image) {
	return CommandListRef_clearImageu(commandListRef, (const U32*) &color, range, image);
}

Error CommandListRef_clearImagef(CommandListRef *commandListRef, F32x4 color, ImageRange range, RefPtr *image) {
	return CommandListRef_clearImageu(commandListRef, (const U32*) &color, range, image);
}

/*
Error CommandListRef_clearDepthStencil(CommandListRef *commandListRef, F32 depth, U8 stencil, ImageRange image) {

	List refs = (List) { 0 };
	Error listErr = List_createConstRef((const U8*) &image.image, sizeof(RefPtr*), 1, &refs);

	if(listErr.genericError)
		return listErr;

	ClearDepthStencil depthStencil = (ClearDepthStencil) { 
		.depth = depth, 
		.stencil = stencil,
		.image = image
	};

	return CommandListRef_append(
		commandListRef, ECommandOp_clearDepth, Buffer_createConstRef(&depthStencil, sizeof(depthStencil)), refs
	);
}*/

Error CommandList_markerDebugExt(CommandListRef *commandListRef, F32x4 color, CharString name, ECommandOp op) {

	CommandList *commandList = CommandListRef_ptr(commandListRef);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);

	if(!(device->info.capabilities.features & EGraphicsFeatures_DebugMarkers))		//NO-OP
		return Error_none();

	Buffer buf = Buffer_createNull();
	Error err = Buffer_createEmptyBytesx(sizeof(color) + CharString_length(name) + 1, &buf);

	if(err.genericError)
		return err;

	Buffer_copy(buf, Buffer_createConstRef(&color, sizeof(color)));
	Buffer_copy(Buffer_createRef((U8*)buf.ptr + sizeof(color), CharString_length(name)), CharString_bufferConst(name));

	_gotoIfError(clean, CommandListRef_append(commandListRef, op, buf, (List) { 0 }, 1));

clean:
	Buffer_freex(&buf);
	return err;
}

Error CommandListRef_addMarkerDebugExt(CommandListRef *commandListRef, F32x4 color, CharString name) {
	return CommandList_markerDebugExt(commandListRef, color, name, ECommandOp_addMarkerDebugExt);
}

Error CommandListRef_startRegionDebugExt(CommandListRef *commandListRef, F32x4 color, CharString name) {
	return CommandList_markerDebugExt(commandListRef, color, name, ECommandOp_startRegionDebugExt);
}

Error CommandListRef_endRegionDebugExt(CommandListRef *commandListRef) {

	if(!commandListRef)
		return Error_nullPointer(0);

	CommandList *commandList = CommandListRef_ptr(commandListRef);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);

	if(!(device->info.capabilities.features & EGraphicsFeatures_DebugMarkers))		//NO-OP
		return Error_none();

	return CommandListRef_append(commandListRef, ECommandOp_endRegionDebugExt, Buffer_createNull(), (List) { 0 }, 0);
}

Bool CommandList_free(CommandList *cmd, Allocator alloc) {

	alloc;

	for (U64 i = 0; i < cmd->resources.length; ++i) {

		RefPtr **ptr = (RefPtr**)cmd->resources.ptr + i;

		if(*ptr)
			RefPtr_dec(ptr);
	}

	List_freex(&cmd->callstacks);
	List_freex(&cmd->commandOps);
	List_freex(&cmd->resources);
	Buffer_freex(&cmd->data);

	GraphicsDeviceRef_dec(&cmd->device);

	return true;
}

Error GraphicsDeviceRef_createCommandList(
	GraphicsDeviceRef *deviceRef, 
	U64 commandListLen, 
	U64 estimatedCommandCount,
	U64 estimatedResources,
	CommandListRef **commandListRef
) {

	Error err = RefPtr_createx(
		(U32) sizeof(CommandList), 
		(ObjectFreeFunc) CommandList_free, 
		EGraphicsTypeId_CommandList, 
		commandListRef
	);

	if(err.genericError)
		return err;

	CommandList *commandList = CommandListRef_ptr(*commandListRef);

	commandList->commandOps = List_createEmpty(sizeof(CommandOpInfo));
	commandList->resources = List_createEmpty(sizeof(RefPtr*));
	commandList->callstacks = List_createEmpty(sizeof(void*) * 32);

	_gotoIfError(clean, Buffer_createEmptyBytesx(commandListLen, &commandList->data));

	_gotoIfError(clean, List_reservex(&commandList->commandOps, estimatedCommandCount));
	_gotoIfError(clean, List_reservex(&commandList->resources, estimatedResources));

	#ifndef NDEBUG
		_gotoIfError(clean, List_reservex(&commandList->callstacks, estimatedCommandCount));
	#endif

	GraphicsDeviceRef_add(deviceRef);
	commandList->device = deviceRef;

	goto success;

clean:
	CommandListRef_dec(commandListRef);

success:
	return err;
}
