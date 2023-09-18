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
#include "platforms/ext/listx.h"
#include "platforms/ext/bufferx.h"
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

Error CommandListRef_append(CommandListRef *commandListRef, ECommandOp op, Buffer buf, List objects) {

	CommandListRef_validate(commandListRef);

	U64 len = Buffer_length(buf);

	if(len > U32_MAX)
		return Error_outOfBounds(2, len, U32_MAX);

	if(commandList->next + len > Buffer_length(commandList->data))
		return Error_outOfMemory(0);

	if(objects.length && objects.stride != sizeof(RefPtr*))
		return Error_invalidParameter(3, 0);

	Error err = List_pushBackx(&commandList->commandOps, Buffer_createConstRef(&op, sizeof(op)));

	if(err.genericError)
		return err;

	_gotoIfError(clean, List_reservex(&commandList->resources, commandList->resources.length + objects.length));

	for (U64 i = 0; i < objects.length; ++i) {

		RefPtr *ptr = ((RefPtr**) objects.ptr)[i];
		Buffer bufi = Buffer_createConstRef(&ptr, sizeof(ptr));

		if(!List_contains(commandList->resources, bufi, 0))							//TODO: hashSet
			_gotoIfError(clean, List_pushBackx(&commandList->resources, bufi));
	}

	if(len) {
		Buffer_copy(Buffer_createRef((U8*)commandList->data.ptr + commandList->next, len), buf);
		commandList->next += len;
	}

	goto success;

clean:
	List_popBack(&commandList->commandOps, Buffer_createNull());

success:
	return err;
}

#define CommandListRef_appendOpOnly(op)					\
CommandListRef_append(commandListRef, op, Buffer_createNull(), (List) { 0 })

#define CommandListRef_appendData(op, t)				\
CommandListRef_append(commandListRef, op, Buffer_createConstRef(&t, sizeof(t)), (List) { 0 })

#define CommandListRef_setViewportCmd(op)				\
														\
	if(I32x2_any(I32x2_leq(size, I32x2_zero())))		\
		return Error_invalidParameter(2, 0);			\
														\
	if(I32x2_any(I32x2_lt(size, I32x2_xx2(-32'768))))	\
		return Error_invalidParameter(1, 0);			\
														\
	if(I32x2_any(I32x2_gt(size, I32x2_xx2(32'767))))	\
		return Error_invalidParameter(1, 1);			\
														\
	I32x4 values = I32x4_create2_2(offset, size);		\
	return CommandListRef_appendData(op, values)

Error CommandListRef_setViewport(CommandListRef *commandListRef, I32x2 offset, I32x2 size) {
	CommandListRef_setViewportCmd(ECommandOp_setViewport);
}

Error CommandListRef_setScissor(CommandListRef *commandListRef, I32x2 offset, I32x2 size) {
	CommandListRef_setViewportCmd(ECommandOp_setScissor);
}

Error CommandListRef_setViewportAndScissor(CommandListRef *commandListRef, I32x2 offset, I32x2 size) {
	CommandListRef_setViewportCmd(ECommandOp_setViewportAndScissor);
}

Error CommandListRef_setStencil(CommandListRef *commandListRef, U8 stencilValue) {
	return CommandListRef_appendData(ECommandOp_setStencil, stencilValue);
}

Error CommandListRef_clearColor(CommandListRef *commandListRef, F32x4 color) {
	return CommandListRef_appendData(ECommandOp_clearColor, color);
}

Error CommandListRef_clearDepthStencil(CommandListRef *commandListRef, F32 depth, U8 stencil) {

	typedef struct DepthStencil {

		F32 depth;

		U8 stencil;
		U8 pad[3];

	} DepthStencil;

	DepthStencil depthStencil = (DepthStencil) { depth, stencil };

	return CommandListRef_appendData(ECommandOp_clearDepthStencil, depthStencil);
}

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

	_gotoIfError(clean, CommandListRef_append(commandListRef, op, buf, (List) { 0 }));

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

	return CommandListRef_appendOpOnly(ECommandOp_endRegionDebugExt);
}

Bool CommandList_free(CommandList *cmd, Allocator alloc) {

	alloc;

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

	commandList->commandOps = List_createEmpty(sizeof(ECommandOp));
	commandList->resources = List_createEmpty(sizeof(RefPtr*));

	_gotoIfError(clean, Buffer_createEmptyBytesx(commandListLen, &commandList->data));

	_gotoIfError(clean, List_reservex(&commandList->commandOps, estimatedCommandCount));
	_gotoIfError(clean, List_reservex(&commandList->resources, estimatedResources));

	GraphicsDeviceRef_add(deviceRef);
	commandList->device = deviceRef;

	goto success;

clean:
	CommandListRef_dec(commandListRef);

success:
	return err;
}
