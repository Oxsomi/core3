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
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/pipeline.h"
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

//Clear, append, begin and end

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

	extraSkipStacktrace;

	CommandListRef_validate(commandListRef);

	U64 len = Buffer_length(buf);

	if(len > U32_MAX)
		return Error_outOfBounds(2, len, U32_MAX);

	if(commandList->next + len > Buffer_length(commandList->data)) {

		if(!commandList->allowResize)
			return Error_outOfMemory(0);

		//Resize buffer to allow allocation

		Buffer resized = Buffer_createNull();
		Error err = Buffer_createEmptyBytesx(Buffer_length(commandList->data) * 2 + KIBI + len, &resized);

		if(err.genericError)
			return err;

		Buffer_copy(resized, commandList->data);
		Buffer_freex(&commandList->data);

		commandList->data = resized;
	}

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

			if(RefPtr_inc(ptr))		//CommandList will keep resource alive.
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

//Standard commands

Error CommandListRef_checkBounds(I32x2 offset, I32x2 size, I32 lowerBound1, I32 upperBound1) {
	
	if(I32x2_any(I32x2_lt(size, I32x2_zero())))
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

	Error boundsCheck = CommandListRef_checkBounds(offset, size, -32'768, 32'767);

	if(boundsCheck.genericError)
		return boundsCheck;

	I32x4 values = I32x4_create2_2(offset, size);
	return CommandListRef_append(commandListRef, op, Buffer_createConstRef(&values, sizeof(values)), (List) { 0 }, 1);
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
	return CommandListRef_append(
		commandListRef, ECommandOp_SetStencil, Buffer_createConstRef(&stencilValue, 1), (List) { 0 }, 0
	);
}

Error CommandListRef_setBlendConstants(CommandListRef *commandList, F32x4 blendConstants) {
	return CommandListRef_append(
		commandList, ECommandOp_SetBlendConstants, Buffer_createConstRef(&blendConstants, sizeof(F32x4)), (List) { 0 }, 0
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

		if(SwapchainRef_ptr(image)->device != CommandListRef_ptr(commandList)->device)
			_gotoIfError(clean, Error_unsupportedOperation(0));

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

	_gotoIfError(clean, CommandListRef_append(commandList, ECommandOp_ClearImages, buf, refs, 0));

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

	Buffer_copy(Buffer_createRef(&clearImage.color, sizeof(F32x4)), Buffer_createConstRef(coloru, sizeof(F32x4)));

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

//Render calls

Error CommandListRef_transition(CommandListRef *commandList, List transitions) {

	if(!transitions.length)
		return Error_nullPointer(1);

	if(transitions.stride != sizeof(Transition))
		return Error_invalidParameter(1, 0);

	if(transitions.length > U32_MAX)
		return Error_outOfBounds(1, transitions.length, U32_MAX);

	Buffer buf = Buffer_createNull();

	List refs = List_createEmpty(sizeof(RefPtr*));
	Error err = List_resizex(&refs, transitions.length);

	if(err.genericError)
		return err;

	RefPtr **ptr = (RefPtr**)refs.ptr;

	for(U64 i = 0; i < transitions.length; ++i) {

		Transition transition = ((Transition*)transitions.ptr)[i];
		RefPtr *res = transition.resource;

		if(!res)
			_gotoIfError(clean, Error_nullPointer(1));

		EGraphicsTypeId typeId = (EGraphicsTypeId) res->typeId;

		switch (typeId) {

			case EGraphicsTypeId_Swapchain:

				if(SwapchainRef_ptr(res)->device != CommandListRef_ptr(commandList)->device)
					_gotoIfError(clean, Error_unsupportedOperation(0));

				break;

			case EGraphicsTypeId_DeviceBuffer: {

				DeviceBuffer *devBuf = DeviceBufferRef_ptr(res);

				if (transition.isWrite) {

					if(!(devBuf->usage & EDeviceBufferUsage_ShaderWrite))
						_gotoIfError(clean, Error_constData(0, 0));
				}

				else if(!(devBuf->usage & EDeviceBufferUsage_ShaderRead))
					_gotoIfError(clean, Error_unsupportedOperation(1));

				if(devBuf->device != CommandListRef_ptr(commandList)->device)
					_gotoIfError(clean, Error_unsupportedOperation(2));

				break;
			}

			default:
				_gotoIfError(clean, Error_invalidParameter(1, 0));
		}

		//Check if range conflicts 

		for (U64 j = 0; j < i; ++j) {

			Transition transitionj = ((Transition*)transitions.ptr)[j];
			RefPtr *resj = transitionj.resource;

			if(resj != res)
				continue;

			//TODO: Multiple types of images / buffers.
			//		We have to loop through all subresources that are covered 
			//		and check if they conflict.
			//		For now, only swapchain is supported w/o layers and levels.
			//		So in that case just mentioning the image twice is enough to conflict.

			_gotoIfError(clean, Error_alreadyDefined(0));
		}

		ptr[i] = res;
	}

	//Convert to refs and buffer

	_gotoIfError(clean, Buffer_createEmptyBytesx(List_bytes(transitions) + sizeof(U32), &buf));

	*(U32*)buf.ptr = (U32) transitions.length;
	Buffer_copy(Buffer_createRef((U8*) buf.ptr + sizeof(U32), List_bytes(transitions)), List_bufferConst(transitions));

	_gotoIfError(clean, CommandListRef_append(commandList, ECommandOp_Transition, buf, refs, 0));

clean:
	Buffer_freex(&buf);
	List_freex(&refs);
	return err;
}

Error CommandListRef_setPipeline(CommandListRef *commandList, PipelineRef *pipeline) {

	if (!pipeline)
		return Error_nullPointer(1);

	if (pipeline->typeId != EGraphicsTypeId_Pipeline)
		return Error_invalidParameter(1, 0);

	if(PipelineRef_ptr(pipeline)->device != CommandListRef_ptr(commandList)->device)
		return Error_unsupportedOperation(0);

	List refs = (List) { 0 };
	Error err = List_createConstRef((const U8*) &pipeline, 1, sizeof(pipeline), &refs);

	if (err.genericError)
		return err;

	return CommandListRef_append(
		commandList, ECommandOp_SetPipeline, Buffer_createConstRef((const U8*) &pipeline, sizeof(pipeline)), refs, 0
	);
}

Error validateBufferDesc(GraphicsDeviceRef *device, DeviceBufferRef *buffer, U64 *counter, EDeviceBufferUsage usage) {

	if(!buffer)
		return Error_none();

	if(buffer->typeId != EGraphicsTypeId_DeviceBuffer)
		return Error_unsupportedOperation(0);

	if(DeviceBufferRef_ptr(buffer)->device != device)
		return Error_unsupportedOperation(1);

	DeviceBuffer *buf = DeviceBufferRef_ptr(buffer);

	if((buf->usage & usage) != usage)
		return Error_unsupportedOperation(2);

	++*counter;
	return Error_none();
}

Error CommandListRef_setPrimitiveBuffers(CommandListRef *commandList, PrimitiveBuffers buffers) {

	//Validate index and vertex buffers

	GraphicsDeviceRef *device = CommandListRef_ptr(commandList)->device;

	U64 counter = 0;
	Error err = validateBufferDesc(device, buffers.indexBuffer, &counter, EDeviceBufferUsage_Index);

	if(err.genericError)
		return err;

	for(U8 i = 0; i < 8; ++i)
		if((err = validateBufferDesc(device, buffers.vertexBuffers[i], &counter, EDeviceBufferUsage_Vertex)).genericError)
			return err;

	List refs = List_createEmpty(sizeof(RefPtr*));
	_gotoIfError(clean, List_resizex(&refs, counter));

	RefPtr **refPtrs = (RefPtr**) refs.ptr;

	counter = 0;

	if(buffers.indexBuffer)
		refPtrs[counter++] = buffers.indexBuffer;

	for(U8 i = 0; i < 8; ++i)
		if(buffers.vertexBuffers[i])
			refPtrs[counter++] = buffers.vertexBuffers[i];

	_gotoIfError(clean, CommandListRef_append(
		commandList, ECommandOp_SetPrimitiveBuffers, Buffer_createConstRef((const U8*) &buffers, sizeof(buffers)), refs, 0
	));

clean:
	List_freex(&refs);
	return err;
}

Error CommandListRef_draw(CommandListRef *commandList, Draw draw) {
	return CommandListRef_append(commandList, ECommandOp_Draw, Buffer_createConstRef(&draw, sizeof(draw)), (List) { 0 }, 0);
}

Error CommandListRef_drawIndexed(CommandListRef *commandList, U32 indexCount, U32 instanceCount) {
	Draw draw = (Draw) { .count = indexCount, .instanceCount = instanceCount, .isIndexed = true };
	return CommandListRef_draw(commandList, draw);
}

Error CommandListRef_drawIndexedAdv(
	CommandListRef *commandList, 
	U32 indexCount, U32 instanceCount, 
	U32 indexOffset, U32 instanceOffset,
	U32 vertexOffset
) {
	Draw draw = (Draw) { 
		.count = indexCount, .instanceCount = instanceCount, 
		.indexOffset = indexOffset, .instanceOffset = instanceOffset,
		.vertexOffset = vertexOffset,
		.isIndexed = true 
	};

	return CommandListRef_draw(commandList, draw);
}

Error CommandListRef_drawUnindexed(CommandListRef *commandList, U32 vertexCount, U32 instanceCount) {
	Draw draw = (Draw) { .count = vertexCount, .instanceCount = instanceCount };
	return CommandListRef_draw(commandList, draw);
}

Error CommandListRef_drawUnindexedAdv(
	CommandListRef *commandList, 
	U32 vertexCount, U32 instanceCount, 
	U32 vertexOffset, U32 instanceOffset
) {
	Draw draw = (Draw) { 
		.count = vertexCount, .instanceCount = instanceCount, 
		.vertexOffset = vertexOffset, .instanceOffset = instanceOffset
	};

	return CommandListRef_draw(commandList, draw);
}

Error CommandListRef_dispatch(CommandListRef *commandList, Dispatch dispatch) {
	return CommandListRef_append(
		commandList, ECommandOp_Dispatch, Buffer_createConstRef(&dispatch, sizeof(dispatch)), (List) { 0 }, 0
	);
}

Error CommandListRef_dispatch1D(CommandListRef *commandList, U32 groupsX) {
	Dispatch dispatch = (Dispatch) { .groups = { groupsX, 1, 1 } };
	return CommandListRef_dispatch(commandList, dispatch);
}

Error CommandListRef_dispatch2D(CommandListRef *commandList, U32 groupsX, U32 groupsY) {
	Dispatch dispatch = (Dispatch) { .groups = { groupsX, groupsY, 1 } };
	return CommandListRef_dispatch(commandList, dispatch);
}

Error CommandListRef_dispatch3D(CommandListRef *commandList, U32 groupsX, U32 groupsY, U32 groupsZ) {
	Dispatch dispatch = (Dispatch) { .groups = { groupsX, groupsY, groupsZ } };
	return CommandListRef_dispatch(commandList, dispatch);
}

//Indirect rendering

Error CommandListRef_checkDispatchBuffer(GraphicsDeviceRef *device, DeviceBufferRef *buffer, U64 offset, U64 siz) {

	if(!buffer)
		return Error_nullPointer(1);

	if(buffer->typeId != EGraphicsTypeId_DeviceBuffer)
		return Error_invalidParameter(1, 0);

	DeviceBuffer *buf = DeviceBufferRef_ptr(buffer);

	if(buf->device != device)
		return Error_unsupportedOperation(3);

	if(offset + siz > buf->length)
		return Error_outOfBounds(1, offset + siz, buf->length);

	if(!(buf->usage & EDeviceBufferUsage_Indirect))
		return Error_unsupportedOperation(0);

	return Error_none();
}

Error CommandListRef_dispatchIndirect(CommandListRef *commandList, DeviceBufferRef *buffer, U64 offset) {

	GraphicsDeviceRef *device = CommandListRef_ptr(commandList)->device;

	Error err = CommandListRef_checkDispatchBuffer(device, buffer, offset, sizeof(U32) * 3);

	if(err.genericError)
		return err;

	List refs = (List) { 0 };
	if((err = List_createConstRef((const U8*)&buffer, 1, sizeof(buffer), &refs)).genericError)
		return err;

	DispatchIndirect dispatch = (DispatchIndirect) { .buffer = buffer, .offset = offset };

	return CommandListRef_append(
		commandList, ECommandOp_DispatchIndirect, Buffer_createConstRef(&dispatch, sizeof(dispatch)), refs, 0
	);
}

Error CommandListRef_drawIndirectBase(
	GraphicsDeviceRef *device,
	DeviceBufferRef *buffer,
	U64 bufferOffset,
	U32 *bufferStride,
	U32 drawCalls,
	Bool indexed
) {

	U32 minStride = (U32)(indexed ? sizeof(DrawCallIndexed) : sizeof(DrawCallUnindexed));

	DeviceBuffer *buf = DeviceBufferRef_ptr(buffer);

	if(!buf)
		return Error_nullPointer(0);

	if(!*bufferStride)
		*bufferStride = minStride;

	if(*bufferStride < minStride)
		return Error_invalidParameter(2, 0);

	if (!drawCalls)
		return Error_invalidParameter(2, 1);

	return CommandListRef_checkDispatchBuffer(device, buffer, bufferOffset, (U64)*bufferStride * drawCalls);
}

Error CommandListRef_drawIndirect(
	CommandListRef *commandList, 
	DeviceBufferRef *buffer,
	U64 bufferOffset,
	U32 bufferStride,
	U32 drawCalls,
	Bool indexed
) {

	GraphicsDeviceRef *device = CommandListRef_ptr(commandList)->device;
	Error err = CommandListRef_drawIndirectBase(device, buffer, bufferOffset, &bufferStride, drawCalls, indexed);

	if(err.genericError)
		return err;

	List refs = (List) { 0 };
	if((err = List_createConstRef((const U8*)&buffer, 1, sizeof(buffer), &refs)).genericError)
		return err;

	DrawIndirect draw = (DrawIndirect) {
		.buffer = buffer,
		.bufferOffset = bufferOffset,
		.drawCalls = drawCalls,
		.bufferStride = bufferStride,
		.isIndexed = indexed
	};

	return CommandListRef_append(
		commandList, ECommandOp_DrawIndirect, Buffer_createConstRef(&draw, sizeof(draw)), refs, 0
	);
}

Error CommandListRef_drawIndirectCount(
	CommandListRef *commandList, 
	DeviceBufferRef *buffer,
	U64 bufferOffset,
	U32 bufferStride, 
	DeviceBufferRef *countBuffer,
	U64 countOffset,
	U32 maxDrawCalls,
	Bool indexed
) {

	GraphicsDeviceRef *device = CommandListRef_ptr(commandList)->device;
	Error err = CommandListRef_drawIndirectBase(device, buffer, bufferOffset, &bufferStride, maxDrawCalls, indexed);

	if(err.genericError)
		return err;

	if((err = CommandListRef_checkDispatchBuffer(device, countBuffer, countOffset, sizeof(U32))).genericError)
		return err;

	DeviceBufferRef *refArr[2] = { buffer, countBuffer };
	List refs = (List) { 0 };
	if((err = List_createConstRef((const U8*)refArr, 2, sizeof(refArr[0]), &refs)).genericError)
		return err;

	DrawIndirect draw = (DrawIndirect) {
		.buffer = buffer,
		.countBuffer = countBuffer,
		.bufferOffset = bufferOffset,
		.countOffset = countOffset,
		.drawCalls = maxDrawCalls,
		.bufferStride = bufferStride,
		.isIndexed = indexed
	};

	return CommandListRef_append(
		commandList, ECommandOp_DrawIndirectCount, Buffer_createConstRef(&draw, sizeof(draw)), refs, 0
	);
}

//Dynamic rendering

Error CommandListRef_startRenderExt(
	CommandListRef *commandListRef, 
	I32x2 offset, 
	I32x2 size, 
	List colors, 
	List depthStencil
) {

	if(!commandListRef)
		return Error_nullPointer(0);

	CommandList *commandList = CommandListRef_ptr(commandListRef);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);

	if(!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering))
		return Error_unsupportedOperation(0);

	if(!colors.length && !depthStencil.length)
		return Error_invalidOperation(0);

	if(colors.length > 8)
		return Error_outOfBounds(3, colors.length, 8);

	Error err = CommandListRef_checkBounds(offset, size, 0, 32'767);

	if(err.genericError)
		return err;

	if(colors.length && colors.stride != sizeof(AttachmentInfo))
		return Error_invalidParameter(3, 0);

	if(depthStencil.length)
		return Error_unsupportedOperation(1);			//TODO: Check depthStencil for depth, stencil and/or DS

	if(depthStencil.length && depthStencil.stride != sizeof(AttachmentInfo))
		return Error_invalidParameter(4, 0);

	Buffer command = Buffer_createNull();
	err = Buffer_createEmptyBytesx(sizeof(StartRenderExt) + sizeof(AttachmentInfo) * 10, &command);

	if(err.genericError)
		return err;

	StartRenderExt *startRender = (StartRenderExt*)command.ptr;

	List refs = List_createEmpty(sizeof(RefPtr*));
	_gotoIfError(clean, List_reservex(&refs, 10));

	*startRender = (StartRenderExt) {
		.offset = offset,
		.size = size,
		.colorCount = (U8) colors.length
	};

	AttachmentInfo *attachments = (AttachmentInfo*)(startRender + 1);

	U8 counter = 0;

	for (U64 i = 0; i < colors.length; ++i) {

		AttachmentInfo info = ((AttachmentInfo*) colors.ptr)[i];

		//TODO: Properly validate this

		if(info.range.levelId >= 1 || info.range.layerId >= 1)
			_gotoIfError(clean, Error_outOfBounds(3, info.range.levelId >= 1 ? info.range.levelId : info.range.layerId, 1));

		if(info.image && info.image->typeId != EGraphicsTypeId_Swapchain)		//TODO: Support other types
			_gotoIfError(clean, Error_unsupportedOperation(2));

		if(SwapchainRef_ptr(info.image)->device != commandList->device)
			return Error_unsupportedOperation(3);

		if (info.image) {

			startRender->activeMask |= (U8)1 << i;
			attachments[counter++] = info;

			_gotoIfError(clean, List_pushBackx(&refs, Buffer_createConstRef(&info.image, sizeof(info.image))));
		}
	}

	if(!counter)
		_gotoIfError(clean, Error_invalidParameter(3, 1));

	_gotoIfError(clean, CommandListRef_append(
		commandListRef, 
		ECommandOp_StartRenderingExt, 
		Buffer_createConstRef(startRender, sizeof(StartRenderExt) + sizeof(AttachmentInfo) * counter), 
		refs, 
		0
	));

clean:
	List_freex(&refs);
	Buffer_freex(&command);
	return err;
}

Error CommandListRef_endRenderExt(CommandListRef *commandListRef) {

	if(!commandListRef)
		return Error_nullPointer(0);

	CommandList *commandList = CommandListRef_ptr(commandListRef);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);

	if(!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering))
		return Error_unsupportedOperation(0);

	return CommandListRef_append(commandListRef, ECommandOp_EndRenderingExt, Buffer_createNull(), (List) { 0 }, 0);
}

//Debug markers

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
	return CommandList_markerDebugExt(commandListRef, color, name, ECommandOp_AddMarkerDebugExt);
}

Error CommandListRef_startRegionDebugExt(CommandListRef *commandListRef, F32x4 color, CharString name) {
	return CommandList_markerDebugExt(commandListRef, color, name, ECommandOp_StartRegionDebugExt);
}

Error CommandListRef_endRegionDebugExt(CommandListRef *commandListRef) {

	if(!commandListRef)
		return Error_nullPointer(0);

	CommandList *commandList = CommandListRef_ptr(commandListRef);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);

	if(!(device->info.capabilities.features & EGraphicsFeatures_DebugMarkers))		//NO-OP
		return Error_none();

	return CommandListRef_append(commandListRef, ECommandOp_EndRegionDebugExt, Buffer_createNull(), (List) { 0 }, 0);
}

//Free and create

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
	Bool allowResize,
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
	commandList->allowResize = allowResize;

	goto success;

clean:
	CommandListRef_dec(commandListRef);

success:
	return err;
}
