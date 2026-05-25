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

//platforms/input_device.h

#pragma once
#include "types/base/types.h"
#include "types/container/string.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum EInputState {

	EInputState_Up,
	EInputState_Pressed,
	EInputState_Released,
	EInputState_Down,

	EInputState_Curr = 1 << 0,
	EInputState_Prev = 1 << 1

} EInputState;

typedef enum EInputType {
	EInputType_Button,
	EInputType_Axis
} EInputType;

typedef U32 InputHandle;		//Don't serialize this, because input devices can change it. Use the name instead.

typedef struct InputButton {
	const C8 *name;
} InputButton;

typedef struct InputAxis {
	const C8 *name;
	F32 deadZone;
	Bool resetOnInputLoss;
	U8 padding[3];
} InputAxis;

typedef enum EInputDeviceType {
	EInputDeviceType_Undefined,
	EInputDeviceType_Keyboard,
	EInputDeviceType_Mouse,
	EInputDeviceType_Controller
} EInputDeviceType;

typedef struct InputDevice {

	//How many buttons and axes this device has

	U16 buttons, axes;
	U32 flags;
	EInputDeviceType type;

	//The names of all handles
	//InputAxis[axes]
	//InputButton[buttons]

	Buffer handles;

	//Bitset for the states of the buttons/axes
	//F32[floatStates*2]
	//U2[booleanStates]

	Buffer states;

	//Platform dependent data, for better identifying this device
	//This is useful for when multiple keyboards are present for example

	Buffer dataExt;

} InputDevice;

//Initializing a device

Bool InputDevice_create(
	U16 buttons,
	U16 axes,
	EInputDeviceType type,
	InputDevice *result,
	const Allocator *alloc,
	Error *e_rr
);

Bool InputDevice_createButton(
	InputDevice *dev,
	U16 localHandle,
	const C8 *keyName,			//The alphaNumeric name (e.g. EKey_1). Should be present until destroy.
	InputHandle *result,
	Error *e_rr
);

Bool InputDevice_createAxis(
	InputDevice *dev,
	U16 localHandle,
	const C8 *axisName,			//The alphaNumeric name (e.g. EKey_1). Should be present until destroy.
	F32 deadZone,
	Bool resetOnInputLoss,
	InputHandle *result,
	Error *e_rr
);

void InputDevice_free(InputDevice *dev, const Allocator *alloc);

static inline InputButton *InputDevice_getButton(const InputDevice *dev, U16 localHandle) {
	return !dev || localHandle >= dev->buttons ? NULL :
		(InputButton*)((InputAxis*) dev->handles.ptr + dev->axes) + localHandle;
}

static inline InputAxis *InputDevice_getAxis(const InputDevice *dev, U16 localHandle) {
	return !dev || localHandle >= dev->axes ? NULL : (InputAxis*) dev->handles.ptr + localHandle;
}

//Simple helpers

static inline U32 InputDevice_getHandles(const InputDevice *d) { return !d ? 0 : (U32)d->axes + d->buttons; }

static inline InputHandle InputDevice_invalidHandle() { return (InputHandle) U64_MAX; }

static inline Bool InputDevice_isValidHandle(const InputDevice *d, InputHandle handle) { return d && handle < InputDevice_getHandles(d); }

static inline Bool InputDevice_isAxis(const InputDevice *d, InputHandle handle) { return d && handle < d->axes; }

static inline Bool InputDevice_isButton(const InputDevice *d, InputHandle handle) {
	return d && !InputDevice_isAxis(d, handle) && handle < (U32)d->axes + d->buttons;
}

static inline InputHandle InputDevice_createHandle(const InputDevice *d, U16 localHandle, EInputType type) {
	return d ? InputDevice_invalidHandle() : localHandle + (InputHandle)(type == EInputType_Axis ? 0 : d->axes);
}

static inline U16 InputDevice_getLocalHandle(const InputDevice *d, InputHandle handle) {
	return !d ? U16_MAX : (U16)(handle - (InputDevice_isAxis(d, handle) ? 0 : d->axes));
}

//Getting previous/current states

static inline Bool InputDevice_hasFlag(const InputDevice *d, U8 flag) {

	if(!d || flag >= 32)
		return false;

	return (d->flags >> flag) & 1;
}

static inline F32 *InputDevice_getAxisValue(const InputDevice *dev, U16 localHandle, Bool isCurrent) {
	return !dev || localHandle >= dev->axes ? NULL : (F32*)dev->states.ptrNonConst + ((U64)localHandle << 1) + isCurrent;
}

static inline BitRef InputDevice_getButtonValue(const InputDevice *dev, U16 localHandle, Bool isCurrent) {

	if(!dev || localHandle >= dev->buttons)
		return (BitRef) { 0 };

	const U64 bitOff = ((U32)localHandle << 1) + isCurrent;
	U8 *off = dev->states.ptrNonConst + dev->axes * 2 * sizeof(F32) + (bitOff >> 3);

	return (BitRef) { .ptr = off, .off = (bitOff & 7) };
}

static inline EInputState InputDevice_getState(const InputDevice *d, InputHandle handle) {

	if(!d || d->type == EInputDeviceType_Undefined || !InputDevice_isButton(d, handle))
		return EInputState_Up;

	U16 i = InputDevice_getLocalHandle(d, handle);
	BitRef old = InputDevice_getButtonValue(d, i, false);

	return (EInputState)((*old.ptr >> old.off) & 3);
}

static inline Bool InputDevice_getCurrentState(const InputDevice *d, InputHandle handle) {
	return InputDevice_getState(d, handle) & EInputState_Curr;
}

static inline Bool InputDevice_getPreviousState(const InputDevice *d, InputHandle handle) {
	return InputDevice_getState(d, handle) & EInputState_Prev;
}

static inline F32 InputDevice_getCurrentAxis(const InputDevice *d, InputHandle handle) {
	return !d || d->type == EInputDeviceType_Undefined || !InputDevice_isAxis(d, handle) ? 0 :
		*InputDevice_getAxisValue(d, InputDevice_getLocalHandle(d, handle), true);
}

static inline F32 InputDevice_getPreviousAxis(const InputDevice *d, InputHandle handle) {
	return !d || d->type == EInputDeviceType_Undefined || !InputDevice_isAxis(d, handle) ? 0 :
		*InputDevice_getAxisValue(d, InputDevice_getLocalHandle(d, handle), false);
}

static inline F32 InputDevice_getDeltaAxis(const InputDevice *d, InputHandle handle) {
	return InputDevice_getCurrentAxis(d, handle) - InputDevice_getPreviousAxis(d, handle);
}

static inline Bool InputDevice_isDown(const InputDevice *d, InputHandle handle) {
	return InputDevice_getState(d, handle) == EInputState_Down;
}

static inline Bool InputDevice_isUp(const InputDevice *d, InputHandle handle) {
	return InputDevice_getState(d, handle) == EInputState_Up;
}

static inline Bool InputDevice_isReleased(const InputDevice *d, InputHandle handle) {
	return InputDevice_getState(d, handle) == EInputState_Released;
}

static inline Bool InputDevice_isPressed(const InputDevice *d, InputHandle handle) {
	return InputDevice_getState(d, handle) == EInputState_Pressed;
}

//For serialization and stuff like that

InputHandle InputDevice_getHandle(const InputDevice *d, CharString name);
CharString InputDevice_getName(const InputDevice *d, InputHandle handle);

static inline F32 InputDevice_getDeadZone(const InputDevice *d, InputHandle handle) {
	return !d || d->type == EInputDeviceType_Undefined || !InputDevice_isAxis(d, handle) ? 0 :
		InputDevice_getAxis(d, InputDevice_getLocalHandle(d, handle))->deadZone;
}

//This should only be handled by platform updating the input device
//First the platform should call markUpdate, then it should start setting new values
//Platform should also unset current axis if focus is lost

//This should only be handled by platform updating the input device

static inline Bool InputDevice_setCurrentState(InputDevice *d, InputHandle handle, Bool v) {

	if(!d || d->type == EInputDeviceType_Undefined || !InputDevice_isButton(d, handle))
		return false;

	const BitRef b = InputDevice_getButtonValue(d, InputDevice_getLocalHandle(d, handle), true);
	BitRef_setTo(b, v);
	return true;
}

static inline Bool InputDevice_setCurrentAxis(InputDevice *d, InputHandle handle, F32 v) {

	if(!d || d->type == EInputDeviceType_Undefined || !InputDevice_isAxis(d, handle))
		return false;

	*InputDevice_getAxisValue(d, InputDevice_getLocalHandle(d, handle), true) = v;
	return true;
}

void InputDevice_markUpdate(InputDevice *d);

static inline Bool InputDevice_setFlag(InputDevice *d, U8 flag) {

	if(flag >= 32 || !d)
		return false;

	d->flags |= (U32)1 << flag;
	return true;
}

static inline Bool InputDevice_resetFlag(InputDevice *d, U8 flag) {

	if(flag >= 32 || !d)
		return false;

	d->flags &= ~((U32)1 << flag);
	return true;
}

static inline Bool InputDevice_setFlagTo(InputDevice *d, U8 flag, Bool value) {
	return value ? InputDevice_setFlag(d, flag) : InputDevice_resetFlag(d, flag);
}

#ifdef __cplusplus
	}
#endif
