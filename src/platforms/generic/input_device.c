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

//platforms/generic/input_device.c

#include "platforms/input_device.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/base/error.h"
#include "types/base/string_read_helper.h"
#include "types/base/constants.h"

Bool InputDevice_create(
	U16 buttons,
	U16 axes,
	EInputDeviceType type,
	InputDevice *result,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocated = false;

	if(!result)
		retError(clean, Error_nullPointer(3, "InputDevice_create()::result is required"));

	if(result->handles.ptr || result->states.ptr)
		retError(clean, Error_invalidOperation(0, "InputDevice_create()::result wasn't empty, indicating possible memleak"));

	if(!buttons && !axes)
		retError(clean, Error_invalidOperation(1, "InputDevice_create()::buttons or axes is required"));

	*result = (InputDevice) {
		.buttons = buttons,
		.axes = axes,
		.type = type
	};

	allocated = true;

	const U64 handles = sizeof(InputAxis) * axes + sizeof(InputButton) * buttons;
	const U64 states = sizeof(F32) * 2 * axes + (((U64)buttons * 2 + 7) >> 3);

	gotoIfError3(clean, Buffer_createEmptyBytes(handles, alloc, &result->handles, e_rr));
	gotoIfError3(clean, Buffer_createEmptyBytes(states, alloc, &result->states, e_rr));

clean:

	if(allocated && !s_uccess)
		InputDevice_free(result, alloc);

	return s_uccess;
}

#define InputDeviceCreate(EInputType)                                                                       \
																											\
	if(!d || d->type == EInputDeviceType_Undefined)                                                         \
		retError(clean, Error_invalidOperation(0, "InputDeviceCreate()::d.type is undefined"));             \
																											\
	Input##EInputType *inputType = InputDevice_get##EInputType(d, localHandle);                             \
																											\
	if(!res)                                                                                                \
		retError(clean, Error_nullPointer(4, "InputDeviceCreate()::res is required"));                      \
																											\
	if(!inputType)                                                                                          \
		retError(clean, Error_nullPointer(0, "InputDeviceCreate() localHandle wasn't found"));              \
																											\
	if(inputType->name)                                                                                     \
		retError(clean, Error_alreadyDefined(0, "InputDeviceCreate() localHandle was already defined"));    \
																											\
	if(!name)                                                                                               \
		retError(clean, Error_invalidParameter(2, 0, "InputDeviceCreate()::keyName is required"));          \
																											\
	inputType->name = name

Bool InputDevice_createButton(
	InputDevice *d,
	U16 localHandle,
	const C8 *name,
	InputHandle *res,
	Error *e_rr
) {
	Bool s_uccess = true;
	InputDeviceCreate(Button);
	*res = InputDevice_createHandle(d, localHandle, EInputType_Button);
clean:
	return s_uccess;
}

Bool InputDevice_createAxis(
	InputDevice *d,
	U16 localHandle,
	const C8 *name,
	F32 deadZone,
	Bool resetOnInputLoss,
	InputHandle *res,
	Error *e_rr
) {
	Bool s_uccess = true;
	InputDeviceCreate(Axis);
	*res = InputDevice_createHandle(d, localHandle, EInputType_Axis);
	inputType->deadZone = deadZone;
	inputType->resetOnInputLoss = resetOnInputLoss;
clean:
	return s_uccess;
}

void InputDevice_free(InputDevice *dev, const Allocator *alloc) {

	if (!dev || dev->type == EInputDeviceType_Undefined)
		return;

	Buffer_free(&dev->handles, alloc);
	Buffer_free(&dev->states, alloc);
	Buffer_free(&dev->dataExt, alloc);
	*dev = (InputDevice) { 0 };
}

CharString InputDevice_getName(const InputDevice *d, InputHandle handle) {

	if(!d || d->type == EInputDeviceType_Undefined || handle >= InputDevice_getHandles(d))
		return CharString_createNull();

	const U16 localHandle = InputDevice_getLocalHandle(d, handle);

	if(InputDevice_isAxis(d, handle))
		return CharString_createRefLongStringConst(InputDevice_getAxis(d, localHandle)->name);

	return CharString_createRefLongStringConst(InputDevice_getButton(d, localHandle)->name);
}

InputHandle InputDevice_getHandle(const InputDevice *d, CharString name) {

	if(!d || d->type == EInputDeviceType_Undefined)
		return InputDevice_invalidHandle();

	//TODO: We probably want to optimize this at some point like use a hashmap

	for(U16 i = 0; i < d->buttons; ++i) {

		CharString str = CharString_createRefLongStringConst(InputDevice_getButton(d, i)->name);

		if(CharString_equalsStringInsensitive(&str, &name))
			return InputDevice_createHandle(d, i, EInputType_Button);
	}

	for(U16 i = 0; i < d->axes; ++i) {

		CharString str = CharString_createRefLongStringConst(InputDevice_getAxis(d, i)->name);

		if(CharString_equalsStringInsensitive(&str, &name))
			return InputDevice_createHandle(d, i, EInputType_Axis);
	}

	return InputDevice_invalidHandle();
}

void InputDevice_markUpdate(InputDevice *d) {

	if(!d || d->type == EInputDeviceType_Undefined)
		return;

	//TODO: Optimize this

	for(U16 i = 0; i < d->axes; ++i) {
		F32 *start = InputDevice_getAxisValue(d, i, false);
		start[0] = start[1];
	}

	for (U16 i = 0; i < d->buttons; ++i) {

		const BitRef old = InputDevice_getButtonValue(d, i, false);
		BitRef neo = old;
		++neo.off;                        //Allowed since we're always aligned

		BitRef_setTo(old, BitRef_get(neo));
	}
}
