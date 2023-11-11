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
#include "formats/texture.h"

Error CommandListRef_dec(CommandListRef **cmd) {
	return !RefPtr_dec(cmd) ? Error_invalidOperation(0) : Error_none();
}

Error CommandListRef_inc(CommandListRef *cmd) {
	return cmd ? (!RefPtr_inc(cmd) ? Error_invalidOperation(0) : Error_none()) : Error_nullPointer(0);
}

//Clear, append, begin and end

#define CommandListRef_validate(v)										\
																		\
	if(!(v) || (v)->typeId != (ETypeId)EGraphicsTypeId_CommandList)		\
		return Error_nullPointer(0);									\
																		\
	CommandList *commandList = CommandListRef_ptr(v);					\
																		\
	if(commandList->state != ECommandListState_Open)					\
		return Error_invalidOperation(0);

#define CommandListRef_validateScope(v, label)							\
																		\
	CommandListRef_validate(v);											\
	Error err = Error_none();											\
																		\
	if(!(commandList->tempStateFlags & ECommandStateFlags_HasScope))	\
		_gotoIfError(label, Error_invalidOperation(0));

Error CommandListRef_clear(CommandListRef *commandListRef) {

	CommandListRef_validate(commandListRef);

	for (U64 i = 0; i < commandList->resources.length; ++i) {

		RefPtr **ptr = (RefPtr**)commandList->resources.ptr + i;

		if(*ptr)
			RefPtr_dec(ptr);
	}

	List_clear(&commandList->commandOps);
	List_clear(&commandList->resources);
	List_clear(&commandList->transitions);
	List_clear(&commandList->activeScopes);

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

	Error err = Error_none();

	if (commandList->tempStateFlags & ECommandStateFlags_HasScope)
		_gotoIfError(clean, Error_invalidState(0));

	_gotoIfError(clean, List_reservex(&commandList->resources, commandList->transitions.length));

	for (U64 i = 0; i < commandList->transitions.length; ++i) {

		TransitionInternal *transitions = &((TransitionInternal*) commandList->transitions.ptr)[i];

		Buffer bufi = Buffer_createConstRef(&transitions->resource, sizeof(RefPtr*));

		if(!List_contains(commandList->resources, bufi, 0)) {						//TODO: hashSet

			if(RefPtr_inc(transitions->resource))		//CommandList will keep resource alive.
				_gotoIfError(clean, List_pushBackx(&commandList->resources, bufi));
		}
	}

	commandList->state = ECommandListState_Closed;

clean:

	if(err.genericError)
		commandList->state = ECommandListState_Invalid;

	return err;
}

Error CommandList_validateGraphicsPipeline(
	Pipeline *pipeline, 
	ImageAndRange images[8], 
	U8 imageCount, 
	EDepthStencilFormat depthFormat
) {

	PipelineGraphicsInfo *info = (PipelineGraphicsInfo*)pipeline->extraInfo;

	//Depth stencil state can be set to None to ignore writing to depth stencil

	if (info->depthFormatExt != EDepthStencilFormat_None && depthFormat != info->depthFormatExt)
		return Error_invalidState(1);

	//Validate attachments

	if (info->attachmentCountExt != imageCount)
		return Error_invalidState(0);

	for (U8 i = 0; i < imageCount && i < 8; ++i) {

		//Undefined is used to ignore the currently bound slot and to avoid writing to it

		if (!info->attachmentFormatsExt[i])
			continue;

		//Validate if formats are the same

		RefPtr *ref = images[i].image;

		if (!ref)
			return Error_nullPointer(1);

		if (ref->typeId != EGraphicsTypeId_Swapchain)
			return Error_invalidParameter(1, i);

		Swapchain *swapchain = SwapchainRef_ptr(ref);

		if (swapchain->format != ETextureFormatId_unpack[info->attachmentFormatsExt[i]])
			return Error_invalidState(i + 2);
	}

	return Error_none();
}

Bool CommandListRef_imageRangeConflicts(RefPtr *image1, ImageRange range1, RefPtr *image2, ImageRange range2) {

	range2; range1;		//TODO:

	return image1 == image2;
}

Bool CommandListRef_bufferRangeConflicts(RefPtr *buffer1, BufferRange range1, RefPtr *buffer2, BufferRange range2) {

	range2; range1;		//TODO:

	return buffer1 == buffer2;
}

Bool CommandListRef_isBound(CommandList *commandList, RefPtr *resource, ResourceRange range) {

	if(!resource)
		return false;

	for(U64 i = 0; i < commandList->pendingTransitions.length; ++i) {

		const TransitionInternal *transition = (const TransitionInternal*) List_ptrConst(commandList->pendingTransitions, i);

		if(transition->resource->typeId != resource->typeId)
			continue;

		if(resource->typeId == EGraphicsTypeId_Swapchain) {
			if(CommandListRef_imageRangeConflicts(resource, range.image, transition->resource, transition->range.image))
				return true;
		}

		else if(CommandListRef_bufferRangeConflicts(resource, range.buffer, transition->resource, transition->range.buffer))
			return true;
	}

	return false;
}

Error CommandList_append(CommandList *commandList, ECommandOp op, Buffer buf, U32 extraSkipStacktrace) {

	extraSkipStacktrace;

	U64 len = Buffer_length(buf);
	Bool didPush = false;
	Error err = Error_none();

	if(len > U32_MAX)
		_gotoIfError(clean, Error_outOfBounds(2, len, U32_MAX));

	if((commandList->commandOps.length + 1) >> 32)
		_gotoIfError(clean, Error_outOfBounds(0, U32_MAX, U32_MAX));

	if(commandList->next + len > Buffer_length(commandList->data)) {

		if(!commandList->allowResize)
			_gotoIfError(clean, Error_outOfMemory(0));

		//Resize buffer to allow allocation

		Buffer resized = Buffer_createNull();
		_gotoIfError(clean, Buffer_createEmptyBytesx(Buffer_length(commandList->data) * 2 + KIBI + len, &resized));

		Buffer_copy(resized, commandList->data);
		Buffer_freex(&commandList->data);

		commandList->data = resized;
	}

	CommandOpInfo info = (CommandOpInfo) {
		.op = op,
		.opSize = (U32) len
	};

	_gotoIfError(clean, List_pushBackx(&commandList->commandOps, Buffer_createConstRef(&info, sizeof(info))));
	didPush = true;

	if(len) {
		Buffer_copy(Buffer_createRef((U8*)commandList->data.ptr + commandList->next, len), buf);
		commandList->next += len;
	}

clean:

	if(err.genericError) {

		if(didPush)
			List_popBack(&commandList->commandOps, Buffer_createNull());

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

	if(CommandListRef_isBound(commandList, buffer, (ResourceRange) { .buffer = range } ))
		return Error_invalidOperation(4);

	TransitionInternal transition = (TransitionInternal) {
		.resource = buffer,
		.range = (ResourceRange) { .buffer = range },
		.stage = EPipelineStage_Count,
		.type = type
	};

	Buffer transitionBuf = Buffer_createConstRef(&transition, sizeof(transition));

	return List_pushBackx(&commandList->pendingTransitions, transitionBuf);
}

//Standard commands

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

	CommandListRef_validateScope(commandListRef, clean);

	if(I32x2_any(I32x2_eq(commandList->currentSize, I32x2_zero())))
		_gotoIfError(clean, Error_invalidOperation(0));

	if(I32x2_any(I32x2_geq(offset, commandList->currentSize)))
		_gotoIfError(clean, Error_invalidOperation(1));

	if(I32x2_eq2(size, I32x2_zero()))
		size = I32x2_sub(commandList->currentSize, offset);

	_gotoIfError(clean, CommandListRef_checkBounds(offset, size, -32'768, 32'767));

	I32x4 values = I32x4_create2_2(offset, size);
	_gotoIfError(clean, CommandList_append(commandList, op, Buffer_createConstRef(&values, sizeof(values)), 1));

	if(op == ECommandOp_SetViewport || op == ECommandOp_SetViewportAndScissor)
		commandList->tempStateFlags |= ECommandStateFlags_AnyViewport;

	if(op == ECommandOp_SetScissor || op == ECommandOp_SetViewportAndScissor)
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

	CommandListRef_validateScope(commandListRef, clean);

	_gotoIfError(clean, CommandList_append(
		commandList, ECommandOp_SetStencil, Buffer_createConstRef(&stencilValue, 1), 0
	));

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_setBlendConstants(CommandListRef *commandListRef, F32x4 blendConstants) {

	CommandListRef_validateScope(commandListRef, clean);

	_gotoIfError(clean, CommandList_append(
		commandList, ECommandOp_SetBlendConstants, Buffer_createConstRef(&blendConstants, sizeof(F32x4)), 0
	));

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_clearImages(CommandListRef *commandListRef, List clearImages) {

	Buffer buf = Buffer_createNull();
	CommandListRef_validateScope(commandListRef, clean);

	if(!clearImages.length)
		_gotoIfError(clean, Error_nullPointer(1));

	if(clearImages.stride != sizeof(ClearImage))
		_gotoIfError(clean, Error_invalidParameter(1, 0));

	if(clearImages.length > U32_MAX)
		_gotoIfError(clean, Error_outOfBounds(1, clearImages.length, U32_MAX));

	for(U64 i = 0; i < clearImages.length; ++i) {

		ClearImage clearImage = ((ClearImage*)clearImages.ptr)[i];
		RefPtr *image = clearImage.image;

		if(!image)
			_gotoIfError(clean, Error_nullPointer(1));

		if(image->typeId != EGraphicsTypeId_Swapchain)
			_gotoIfError(clean, Error_invalidParameter(1, 0));

		if(SwapchainRef_ptr(image)->device != CommandListRef_ptr(commandList)->device)
			_gotoIfError(clean, Error_unsupportedOperation(0));

		//TODO: Properly support this

		if (clearImage.range.layerId != U32_MAX && clearImage.range.layerId >= 1)
			_gotoIfError(clean, Error_outOfBounds(1, clearImage.range.layerId, 1));

		if (clearImage.range.levelId != U32_MAX && clearImage.range.levelId >= 1)
			_gotoIfError(clean, Error_outOfBounds(2, clearImage.range.levelId, 1));

		//Add transition

		if(CommandListRef_isBound(commandList, image, (ResourceRange) { .image = clearImage.range }))
			_gotoIfError(clean, Error_invalidOperation(0));

		TransitionInternal transition = (TransitionInternal) {
			.resource = image,
			.range = (ResourceRange) { .image = clearImage.range },
			.stage = EPipelineStage_Count,
			.type = ETransitionType_ClearColor
		};

		Buffer transitionBuf = Buffer_createConstRef(&transition, sizeof(transition));

		_gotoIfError(clean, List_pushBackx(&commandList->pendingTransitions, transitionBuf));
	}

	//Copy buffer
	
	_gotoIfError(clean, Buffer_createEmptyBytesx(List_bytes(clearImages) + sizeof(U32), &buf));

	*(U32*)buf.ptr = (U32) clearImages.length;
	Buffer_copy(Buffer_createRef((U8*) buf.ptr + sizeof(U32), List_bytes(clearImages)), List_bufferConst(clearImages));

	_gotoIfError(clean, CommandList_append(commandList, ECommandOp_ClearImages, buf, 0));

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	Buffer_freex(&buf);
	return err;
}

Error CommandListRef_clearImageu(CommandListRef *commandListRef, const U32 coloru[4], ImageRange range, RefPtr *image) {

	CommandListRef_validate(commandListRef);

	ClearImage clearImage = (ClearImage) {
		.image = image,
		.range = range
	};

	Buffer_copy(Buffer_createRef(&clearImage.color, sizeof(F32x4)), Buffer_createConstRef(coloru, sizeof(F32x4)));

	List clearImages = (List) { 0 };
	Error err = List_createConstRef((const U8*) &clearImage, 1, sizeof(ClearImage), &clearImages);

	if(err.genericError) {
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;
		return err;
	}

	return CommandListRef_clearImages(commandListRef, clearImages);
}

Error CommandListRef_clearImagei(CommandListRef *commandListRef, I32x4 color, ImageRange range, RefPtr *image) {
	return CommandListRef_clearImageu(commandListRef, (const U32*) &color, range, image);
}

Error CommandListRef_clearImagef(CommandListRef *commandListRef, F32x4 color, ImageRange range, RefPtr *image) {
	return CommandListRef_clearImageu(commandListRef, (const U32*) &color, range, image);
}

/*TODO:
Error CommandListRef_clearDepthStencil(CommandListRef *commandListRef, F32 depth, U8 stencil, ImageRange image) {

	CommandListRef_validateScope(commandListRef, clean);

	List refs = (List) { 0 };
	Error listErr = List_createConstRef((const U8*) &image.image, sizeof(RefPtr*), 1, &refs);

	if(listErr.genericError)
		return listErr;

	ClearDepthStencil depthStencil = (ClearDepthStencil) { 
		.depth = depth, 
		.stencil = stencil,
		.image = image
	};

	return CommandList_append(
		commandList, ECommandOp_clearDepth, Buffer_createConstRef(&depthStencil, sizeof(depthStencil)), refs
	);
}*/

//Render calls

Error CommandListRef_startScope(CommandListRef *commandListRef, List transitions, U32 id, List deps) {

	CommandListRef_validate(commandListRef);

	GraphicsDeviceRef *device = commandList->device;

	Error err = Error_none();

	if(transitions.length && transitions.stride != sizeof(Transition))
		_gotoIfError(clean, Error_invalidParameter(1, 0));

	if(deps.length && deps.stride != sizeof(CommandScopeDependency))
		_gotoIfError(clean, Error_invalidParameter(3, 0));

	if(transitions.length > U32_MAX)
		_gotoIfError(clean, Error_outOfBounds(1, transitions.length, U32_MAX));

	if(commandList->tempStateFlags & ECommandStateFlags_HasScope)		//No nested scopes
		_gotoIfError(clean, Error_invalidOperation(0));

	_gotoIfError(clean, List_clear(&commandList->pendingTransitions));
	_gotoIfError(clean, List_reservex(&commandList->pendingTransitions, transitions.length));

	for(U64 i = 0; i < transitions.length; ++i) {

		Transition transition = ((Transition*)transitions.ptr)[i];
		RefPtr *res = transition.resource;

		if(!res)
			_gotoIfError(clean, Error_nullPointer(1));

		EGraphicsTypeId typeId = (EGraphicsTypeId) res->typeId;

		switch (typeId) {

			case EGraphicsTypeId_Swapchain: {

				Swapchain *swapchain = SwapchainRef_ptr(res);

				if (transition.isWrite) {

					if(!(swapchain->info.usage & ESwapchainUsage_AllowCompute))
						_gotoIfError(clean, Error_constData(0, 0));
				}

				if(swapchain->device != device)
					_gotoIfError(clean, Error_unsupportedOperation(0));

				break;
			}

			case EGraphicsTypeId_DeviceBuffer: {

				DeviceBuffer *devBuf = DeviceBufferRef_ptr(res);

				if (transition.isWrite) {

					if(!(devBuf->usage & EDeviceBufferUsage_ShaderWrite))
						_gotoIfError(clean, Error_constData(0, 1));
				}

				else if(!(devBuf->usage & EDeviceBufferUsage_ShaderRead))
					_gotoIfError(clean, Error_unsupportedOperation(1));

				if(devBuf->device != device)
					_gotoIfError(clean, Error_unsupportedOperation(2));

				break;
			}

			default:
				_gotoIfError(clean, Error_invalidParameter(1, 0));
		}

		if(CommandListRef_isBound(commandList, res, transition.range))
			_gotoIfError(clean, Error_invalidOperation(0));

		TransitionInternal transitionDst = (TransitionInternal) {
			.resource = res,
			.range = transition.range,
			.stage = transition.stage,
			.type = transition.isWrite ? ETransitionType_ShaderWrite : ETransitionType_ShaderRead
		};

		Buffer transitionDstBuf = Buffer_createConstRef(&transitionDst, sizeof(transitionDst));
		_gotoIfError(clean, List_pushBackx(&commandList->pendingTransitions, transitionDstBuf));
	}

	//Scope has to be unique

	for (U64 i = 0; i < commandList->activeScopes.length; ++i) {

		CommandScope *scope = (CommandScope*) List_ptr(commandList->activeScopes, i);

		if(scope->scopeId == id)
			_gotoIfError(clean, Error_alreadyDefined(0));
	}

	//Find deps

	for (U64 j = 0; j < deps.length; ++j) {

		CommandScopeDependency dep = *(const CommandScopeDependency*) List_ptrConst(deps, j);

		if(dep.type == ECommandScopeDependencyType_Unconditional)		//Don't care
			continue;

		Bool found = false;

		for (U64 i = 0; i < commandList->activeScopes.length; ++i) {	//TODO: HashSet

			CommandScope *scope = (CommandScope*) List_ptr(commandList->activeScopes, i);

			if (scope->scopeId == dep.id) {
				found = true;
				break;
			}
		}

		if(found)
			continue;

		if(dep.type == ECommandScopeDependencyType_Strong)		//Error for strong scopes
			_gotoIfError(clean, Error_notFound(0, 0));

		goto clean;		//Ignore scope for weak refs
	}

	//TODO: Append deps to array so runtime can use it (U32 only)

	commandList->lastCommandId = (U32) commandList->commandOps.length;
	commandList->lastOffset = commandList->next;
	commandList->lastScopeId = id;

	_gotoIfError(clean, CommandList_append(commandList, ECommandOp_StartScope, Buffer_createNull(), 0));

	commandList->tempStateFlags |= ECommandStateFlags_HasScope;

clean:

	if(err.genericError)
		commandList->state = ECommandListState_Invalid;

	return err;
}

Error CommandListRef_endScope(CommandListRef *commandListRef) {

	CommandListRef_validateScope(commandListRef, clean);

	//Check if scope has to be hidden.

	if(
		(commandList->tempStateFlags & ECommandStateFlags_InvalidState) ||			//Hide scope
		!(commandList->tempStateFlags & ECommandStateFlags_HasModifyOp)
	) {
		//Pretend the last commands didn't happen

		_gotoIfError(clean, List_resize(&commandList->commandOps, commandList->lastCommandId, (Allocator) { 0 }));
		commandList->next = commandList->lastOffset;

		goto clean;
	}

	if(commandList->debugRegionStack)
		_gotoIfError(clean, Error_invalidOperation(0));

	if(!I32x2_eq2(commandList->currentSize, I32x2_zero()))
		_gotoIfError(clean, Error_invalidOperation(1));

	//Push command, transitions and scope

	if((commandList->transitions.length + commandList->pendingTransitions.length) >> 32)
		_gotoIfError(clean, Error_outOfBounds(
			0, commandList->transitions.length + commandList->pendingTransitions.length, U32_MAX
		));

	U32 commandOps = (U32)((commandList->commandOps.length + 1) - commandList->lastCommandId);
	U64 commandLen = commandList->next - commandList->lastOffset;
	U32 transitionOffset = (U32) commandList->transitions.length;

	_gotoIfError(clean, CommandList_append(commandList, ECommandOp_EndScope, Buffer_createNull(), 0));
	_gotoIfError(clean, List_pushAllx(&commandList->transitions, commandList->pendingTransitions));

	CommandScope scope = (CommandScope) {
		.commandBufferOffset = commandList->lastOffset,
		.commandBufferLength = commandLen,
		.commandOpOffset = commandList->lastCommandId,
		.transitionOffset = transitionOffset,
		.commandOps = commandOps,
		.transitionCount = (U32) commandList->pendingTransitions.length,
		.scopeId = commandList->lastScopeId
	};

	Buffer scopeBuf = Buffer_createConstRef(&scope, sizeof(scope));
	_gotoIfError(clean, List_pushBackx(&commandList->activeScopes, scopeBuf));

clean:

	for(U64 i = 0; i < EPipelineType_Count; ++i)
		commandList->pipeline[i] = NULL;

	_gotoIfError(clean, List_clear(&commandList->pendingTransitions));

	commandList->tempStateFlags = 0;
	commandList->debugRegionStack = 0;
	commandList->currentSize = I32x2_zero();

	if(err.genericError)
		commandList->state = ECommandListState_Invalid;

	return err;
}

Error CommandListRef_setPipeline(CommandListRef *commandListRef, PipelineRef *pipelineRef, EPipelineType type) {

	CommandListRef_validateScope(commandListRef, clean);

	if (!pipelineRef)
		_gotoIfError(clean, Error_nullPointer(1));

	if (pipelineRef->typeId != EGraphicsTypeId_Pipeline)
		_gotoIfError(clean, Error_invalidParameter(1, 0));

	Pipeline *pipeline = PipelineRef_ptr(pipelineRef);

	if(pipeline->device != commandList->device)
		_gotoIfError(clean, Error_unsupportedOperation(0));

	if(pipeline->type != type)
		_gotoIfError(clean, Error_unsupportedOperation(1));

	ECommandOp op = ECommandOp_SetComputePipeline;

	if(type == EPipelineType_Graphics)
		op = ECommandOp_SetGraphicsPipeline;

	_gotoIfError(clean, CommandList_append(
		commandList, op, Buffer_createConstRef((const U8*) &pipelineRef, sizeof(pipelineRef)), 0
	));

	Buffer bufi = Buffer_createConstRef(&pipelineRef, sizeof(RefPtr*));

	if(!List_contains(commandList->resources, bufi, 0)) {						//TODO: hashSet

		if(RefPtr_inc(pipelineRef))		//CommandList will keep resource alive.
			_gotoIfError(clean, List_pushBackx(&commandList->resources, bufi));
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

Error CommandListRef_validateBufferDesc(GraphicsDeviceRef *device, DeviceBufferRef *buffer, EDeviceBufferUsage usage) {

	if(!buffer)
		return Error_none();

	if(buffer->typeId != EGraphicsTypeId_DeviceBuffer)
		return Error_unsupportedOperation(0);

	if(DeviceBufferRef_ptr(buffer)->device != device)
		return Error_unsupportedOperation(1);

	DeviceBuffer *buf = DeviceBufferRef_ptr(buffer);

	if((buf->usage & usage) != usage)
		return Error_unsupportedOperation(2);

	return Error_none();
}

Error CommandListRef_setPrimitiveBuffers(CommandListRef *commandListRef, PrimitiveBuffers buffers) {

	CommandListRef_validateScope(commandListRef, clean);

	if(I32x2_any(I32x2_eq(commandList->currentSize, I32x2_zero())))
		_gotoIfError(clean, Error_invalidOperation(0));

	//Validate index and vertex buffers

	GraphicsDeviceRef *device = commandList->device;
	_gotoIfError(clean, CommandListRef_validateBufferDesc(device, buffers.indexBuffer, EDeviceBufferUsage_Index));

	if(err.genericError)
		return err;

	for(U8 i = 0; i < 8; ++i)
		_gotoIfError(clean, CommandListRef_validateBufferDesc(device, buffers.vertexBuffers[i], EDeviceBufferUsage_Vertex));

	//Transition

	_gotoIfError(clean, CommandListRef_transitionBuffer(
		commandList, buffers.indexBuffer, (BufferRange) { 0 }, ETransitionType_Index
	));

	for(U8 i = 0; i < 8; ++i)
		_gotoIfError(clean, CommandListRef_transitionBuffer(
			commandList, buffers.vertexBuffers[i], (BufferRange) { 0 }, ETransitionType_Vertex
		));

	//Issue command

	_gotoIfError(clean, CommandList_append(
		commandList, ECommandOp_SetPrimitiveBuffers, Buffer_createConstRef((const U8*) &buffers, sizeof(buffers)), 0
	));

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_drawBase(CommandListRef *commandListRef, Buffer buf, ECommandOp op) {

	CommandListRef_validateScope(commandListRef, clean);

	if(I32x2_any(I32x2_eq(commandList->currentSize, I32x2_zero())))
		_gotoIfError(clean, Error_invalidOperation(0));

	PipelineRef *pipelineRef = commandList->pipeline[EPipelineType_Graphics];

	if (!pipelineRef)
		_gotoIfError(clean, Error_invalidOperation(1));

	U32 flags = ECommandStateFlags_AnyScissor | ECommandStateFlags_AnyViewport;

	if ((commandList->tempStateFlags & flags) != flags)
		return Error_invalidOperation(2);

	_gotoIfError(clean, CommandList_validateGraphicsPipeline(
		PipelineRef_ptr(pipelineRef), 
		commandList->boundImages, 
		commandList->boundImageCount, 
		(EDepthStencilFormat) commandList->boundDepthFormat
	));

	_gotoIfError(clean, CommandList_append(commandList, op, buf, 1));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_draw(CommandListRef *commandListRef, Draw draw) {

	if(!draw.count || !draw.instanceCount)		//No-op
		return Error_none();

	return CommandListRef_drawBase(commandListRef, Buffer_createConstRef(&draw, sizeof(draw)), ECommandOp_Draw);
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

Error CommandListRef_dispatch(CommandListRef *commandListRef, Dispatch dispatch) {

	CommandListRef_validateScope(commandListRef, clean);

	if (!commandList->pipeline[EPipelineType_Compute])
		_gotoIfError(clean, Error_invalidOperation(1));

	U64 groupCountMax = U64_max(dispatch.groups[0], U64_max(dispatch.groups[1], dispatch.groups[2]));

	if(groupCountMax > U16_MAX)
		_gotoIfError(clean, Error_outOfBounds(1, groupCountMax, U16_MAX));

	_gotoIfError(clean, CommandList_append(
		commandList, ECommandOp_Dispatch, Buffer_createConstRef(&dispatch, sizeof(dispatch)), 0
	));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
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

	if(!buffer || buffer->typeId != EGraphicsTypeId_DeviceBuffer)
		return Error_nullPointer(1);

	DeviceBuffer *buf = DeviceBufferRef_ptr(buffer);

	if(buf->device != device)
		return Error_unsupportedOperation(3);

	if(offset + siz > buf->length)
		return Error_outOfBounds(1, offset + siz, buf->length);

	if(!(buf->usage & EDeviceBufferUsage_Indirect))
		return Error_unsupportedOperation(0);

	return Error_none();
}

Error CommandListRef_dispatchIndirect(CommandListRef *commandListRef, DeviceBufferRef *buffer, U64 offset) {

	CommandListRef_validateScope(commandListRef, clean);
	GraphicsDeviceRef *device = CommandListRef_ptr(commandList)->device;

	if (!commandList->pipeline[EPipelineType_Compute])
		_gotoIfError(clean, Error_invalidOperation(1));

	_gotoIfError(clean, CommandListRef_checkDispatchBuffer(device, buffer, offset, sizeof(U32) * 4));

	BufferRange range = (BufferRange) { .startRange = offset, .endRange = offset + sizeof(U32) * 4 };

	_gotoIfError(clean, CommandListRef_transitionBuffer(commandList, buffer, range, ETransitionType_Indirect));

	DispatchIndirect dispatch = (DispatchIndirect) { .buffer = buffer, .offset = offset };

	_gotoIfError(clean, CommandList_append(
		commandList, ECommandOp_DispatchIndirect, Buffer_createConstRef(&dispatch, sizeof(dispatch)), 0
	));

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

	U32 minStride = (U32)(indexed ? sizeof(DrawCallIndexed) : sizeof(DrawCallUnindexed));

	DeviceBuffer *buf = DeviceBufferRef_ptr(buffer);

	if(!buf)
		return Error_nullPointer(0);

	if(bufferOffset & 3)
		return Error_invalidParameter(2, 0);

	if(!*bufferStride)
		*bufferStride = minStride;

	if(*bufferStride < minStride)
		return Error_invalidParameter(2, 1);

	if (!drawCalls)
		return Error_invalidParameter(2, 2);

	Error err = CommandListRef_checkDispatchBuffer(commandList->device, buffer, bufferOffset, (U64)*bufferStride * drawCalls);

	if(err.genericError)
		return err;

	BufferRange range = (BufferRange) {
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

	CommandListRef_validateScope(commandListRef, clean);

	_gotoIfError(clean, CommandList_drawIndirectBase(commandList, buffer, bufferOffset, &bufferStride, drawCalls, indexed));

	DrawIndirect draw = (DrawIndirect) {
		.buffer = buffer,
		.bufferOffset = bufferOffset,
		.drawCalls = drawCalls,
		.bufferStride = bufferStride,
		.isIndexed = indexed
	};

	_gotoIfError(clean, CommandListRef_drawBase(
		commandListRef, Buffer_createConstRef(&draw, sizeof(draw)), ECommandOp_DrawIndirect
	));

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

	CommandListRef_validateScope(commandListRef, clean);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);
	_gotoIfError(clean, CommandList_drawIndirectBase(commandList, buffer, bufferOffset, &bufferStride, maxDrawCalls, indexed));

	if(!(device->info.capabilities.features & EGraphicsFeatures_MultiDrawIndirectCount))
		_gotoIfError(clean, Error_unsupportedOperation(0));

	_gotoIfError(clean, CommandListRef_checkDispatchBuffer(commandList->device, countBuffer, countOffset, sizeof(U32)));

	BufferRange range = (BufferRange) {
		.startRange = countOffset,
		.endRange = countOffset + sizeof(U32)
	};

	_gotoIfError(clean, CommandListRef_transitionBuffer(commandList, countBuffer, range, ETransitionType_Indirect));

	DrawIndirect draw = (DrawIndirect) {
		.buffer = buffer,
		.countBufferExt = countBuffer,
		.bufferOffset = bufferOffset,
		.countOffsetExt = countOffset,
		.drawCalls = maxDrawCalls,
		.bufferStride = bufferStride,
		.isIndexed = indexed
	};

	_gotoIfError(clean, CommandListRef_drawBase(
		commandListRef, Buffer_createConstRef(&draw, sizeof(draw)), ECommandOp_DrawIndirectCount
	));

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
	List colors, 
	List depthStencil
) {

	Buffer command = Buffer_createNull();
	CommandListRef_validateScope(commandListRef, clean);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);

	if(!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering))
		_gotoIfError(clean, Error_unsupportedOperation(0));

	if(!colors.length && !depthStencil.length)
		_gotoIfError(clean, Error_invalidOperation(1));

	if(colors.length > 8)
		_gotoIfError(clean, Error_outOfBounds(3, colors.length, 8));

	if(colors.length && colors.stride != sizeof(AttachmentInfo))
		_gotoIfError(clean, Error_invalidParameter(3, 0));

	if(depthStencil.length)
		_gotoIfError(clean, Error_unsupportedOperation(1));			//TODO: Check depthStencil for depth, stencil and/or DS

	if(depthStencil.length && depthStencil.stride != sizeof(AttachmentInfo))
		_gotoIfError(clean, Error_invalidParameter(4, 0));
		
	I32x2 targetSize = I32x2_zero();

	for (U64 i = 0; i < colors.length + depthStencil.length; ++i) {

		if (i == colors.length) {
			//TODO: Depth stencil
			break;
		}

		AttachmentInfo info = ((AttachmentInfo*) colors.ptr)[i];

		if (info.image && info.image->typeId == (ETypeId)EGraphicsTypeId_Swapchain) {
			targetSize = SwapchainRef_ptr(info.image)->size;
			break;
		}
	}

	if(I32x2_any(I32x2_eq(targetSize, I32x2_zero())))
		_gotoIfError(clean, Error_invalidOperation(5));

	if(I32x2_any(I32x2_geq(offset, targetSize)))
		_gotoIfError(clean, Error_invalidOperation(6));

	if(I32x2_any(I32x2_eq(size, I32x2_zero())))
	   size = I32x2_sub(targetSize, offset);

	_gotoIfError(clean, CommandListRef_checkBounds(offset, size, 0, 32'767));
	_gotoIfError(clean, Buffer_createEmptyBytesx(sizeof(StartRenderExt) + sizeof(AttachmentInfo) * 16, &command));

	if(!I32x2_all(I32x2_eq(commandList->currentSize, I32x2_zero())))
		_gotoIfError(clean, Error_invalidOperation(2));

	StartRenderExt *startRender = (StartRenderExt*)command.ptr;

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

		if(
			(info.range.levelId != U32_MAX && info.range.levelId >= 1) || 
			(info.range.layerId != U32_MAX && info.range.layerId >= 1)
		)
			_gotoIfError(clean, Error_outOfBounds(
				3, 
				info.range.levelId != U32_MAX && info.range.levelId >= 1 ? info.range.levelId : info.range.layerId, 
				1
			));

		if (info.image) {

			if(info.image && info.image->typeId != EGraphicsTypeId_Swapchain)		//TODO: Support other types
				_gotoIfError(clean, Error_unsupportedOperation(2));

			if(!i)
				targetSize = SwapchainRef_ptr(info.image)->size;

			else if(!I32x2_eq2(targetSize, SwapchainRef_ptr(info.image)->size))
				_gotoIfError(clean, Error_invalidOperation(3));

			if(SwapchainRef_ptr(info.image)->device != commandList->device)
				_gotoIfError(clean, Error_unsupportedOperation(3));

			startRender->activeMask |= (U8)1 << i;
			attachments[counter++] = info;

			if(CommandListRef_isBound(commandList, info.image, (ResourceRange) { .image = info.range } ))
				_gotoIfError(clean, Error_invalidOperation(4));

			TransitionInternal transition = (TransitionInternal) {
				.resource = info.image,
				.range = info.range,
				.stage = EPipelineStage_Count,
				.type = info.readOnly ? ETransitionType_RenderTargetRead : ETransitionType_RenderTargetWrite
			};

			Buffer transitionBuf = Buffer_createConstRef(&transition, sizeof(transition));
			_gotoIfError(clean, List_pushBackx(&commandList->pendingTransitions, transitionBuf));
		}
	}

	if(!counter)
		_gotoIfError(clean, Error_invalidParameter(3, 1));

	_gotoIfError(clean, CommandList_append(
		commandList, 
		ECommandOp_StartRenderingExt, 
		Buffer_createConstRef(startRender, sizeof(StartRenderExt) + sizeof(AttachmentInfo) * counter), 
		0
	));

	commandList->currentSize = size;
	commandList->boundImageCount = (U8) colors.length;

	for (U8 i = 0; i < commandList->boundImageCount; ++i) {
		AttachmentInfo info = ((AttachmentInfo*) colors.ptr)[i];
		commandList->boundImages[i] = (ImageAndRange) { .image = info.image, .range = info.range };
	}

	//commandList->boundDepthFormat = depthFormat;	TODO:

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	Buffer_freex(&command);
	return err;
}

Error CommandListRef_endRenderExt(CommandListRef *commandListRef) {

	CommandListRef_validateScope(commandListRef, clean);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);

	if(!(device->info.capabilities.features & EGraphicsFeatures_DirectRendering))
		_gotoIfError(clean, Error_unsupportedOperation(0));

	if(I32x2_any(I32x2_eq(commandList->currentSize, I32x2_zero())))
		_gotoIfError(clean, Error_invalidOperation(1));

	_gotoIfError(clean, CommandList_append(commandList, ECommandOp_EndRenderingExt, Buffer_createNull(), 0));

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
	CommandListRef_validateScope(commandListRef, clean);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);

	if(!(device->info.capabilities.features & EGraphicsFeatures_DebugMarkers))		//NO-OP
		return Error_none();

	_gotoIfError(clean, Buffer_createEmptyBytesx(sizeof(color) + CharString_length(name) + 1, &buf));

	Buffer_copy(buf, Buffer_createConstRef(&color, sizeof(color)));
	Buffer_copy(Buffer_createRef((U8*)buf.ptr + sizeof(color), CharString_length(name)), CharString_bufferConst(name));

	_gotoIfError(clean, CommandList_append(commandList, op, buf, 1));

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

	CommandListRef_validateScope(commandListRef, clean);

	if(!CharString_length(name))
		_gotoIfError(clean, Error_nullPointer(2));

	if(commandList && commandList->debugRegionStack == U8_MAX)
		_gotoIfError(clean, Error_outOfBounds(0, U8_MAX, U8_MAX));

	_gotoIfError(clean, CommandList_markerDebugExt(commandListRef, color, name, ECommandOp_StartRegionDebugExt));

	++commandList->debugRegionStack;

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

Error CommandListRef_endRegionDebugExt(CommandListRef *commandListRef) {

	CommandListRef_validateScope(commandListRef, clean);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);

	if(!(device->info.capabilities.features & EGraphicsFeatures_DebugMarkers))		//NO-OP
		return Error_none();

	if (!commandList->debugRegionStack)
		_gotoIfError(clean, Error_invalidOperation(1));

	_gotoIfError(clean, CommandList_append(commandList, ECommandOp_EndRegionDebugExt, Buffer_createNull(), 0));

	--commandList->debugRegionStack;

clean:

	if(err.genericError)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return err;
}

//Free and create

Bool CommandList_free(CommandList *cmd, Allocator alloc) {

	alloc;

	for (U64 i = 0; i < cmd->resources.length; ++i) {

		RefPtr **ptr = (RefPtr**)cmd->resources.ptr + i;

		if(*ptr)
			RefPtr_dec(ptr);
	}

	List_freex(&cmd->commandOps);
	List_freex(&cmd->resources);
	List_freex(&cmd->activeScopes);
	List_freex(&cmd->transitions);
	List_freex(&cmd->pendingTransitions);
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
	commandList->transitions = List_createEmpty(sizeof(TransitionInternal));
	commandList->activeScopes = List_createEmpty(sizeof(CommandScope));
	commandList->pendingTransitions = List_createEmpty(sizeof(TransitionInternal));

	_gotoIfError(clean, Buffer_createEmptyBytesx(commandListLen, &commandList->data));

	_gotoIfError(clean, List_reservex(&commandList->commandOps, estimatedCommandCount));
	_gotoIfError(clean, List_reservex(&commandList->resources, estimatedResources));
	_gotoIfError(clean, List_reservex(&commandList->activeScopes, 16));
	_gotoIfError(clean, List_reservex(&commandList->transitions, estimatedResources));
	_gotoIfError(clean, List_reservex(&commandList->pendingTransitions, 32));

	GraphicsDeviceRef_inc(deviceRef);
	commandList->device = deviceRef;
	commandList->allowResize = allowResize;

	goto success;

clean:
	CommandListRef_dec(commandListRef);

success:
	return err;
}
