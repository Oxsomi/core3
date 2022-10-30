#include "types/string.h"
#include "types/error.h"
#include "platforms/input_device.h"
#include "platforms/platform.h"
#include "types/buffer.h"

//Private helpers

//Bitset for the states of the buttons/axes
//F32[floatStates*2]
//u2[booleanStates]

InputButton *InputDevice_getButton(InputDevice dev, U16 localHandle) {
	return localHandle >= dev.buttons ? NULL : 
		(InputButton*)((InputAxis*) dev.handles.ptr + dev.axes) + localHandle;
}

InputAxis *InputDevice_getAxis(InputDevice dev, U16 localHandle) {
	return localHandle >= dev.axes ? NULL :
		(InputAxis*) dev.handles.ptr + localHandle;
}

inline F32 *InputDevice_getAxisValue(InputDevice dev, U16 localHandle, Bool isCurrent) {
	return localHandle >= dev.axes ? NULL : 
		(F32*)dev.states.ptr + ((U64)localHandle << 1) + isCurrent;
}

inline BitRef InputDevice_getButtonValue(InputDevice dev, U16 localHandle, Bool isCurrent) {
	
	if(localHandle >= dev.buttons)
		return (BitRef){ 0 };

	U64 bitOff = (localHandle << 1) + isCurrent;
	U8 *off = dev.states.ptr + dev.axes * 2 * sizeof(F32) + (bitOff >> 3);

	return (BitRef){ .ptr = off, .off = (bitOff & 7) };
}

//

Error InputDevice_create(U16 buttons, U16 axes, InputDeviceType type, InputDevice *result) {

	if(!result)
		return (Error) { .genericError = GenericError_NullPointer };

	*result = (InputDevice) {
		.buttons = buttons,
		.axes = axes,
		.type = type
	};

	U64 handles = sizeof(InputAxis) * axes + sizeof(InputButton) * buttons;
	U64 states = sizeof(F32) * 2 * axes + (((U64)buttons * 2 + 7) >> 3);

	Error err = Buffer_createZeroBits(handles, Platform_instance.alloc, &result->handles);

	if (err.genericError) {
		InputDevice_free(result);
		return err;
	}

	err = Buffer_createZeroBits(states, Platform_instance.alloc, &result->states);

	if (err.genericError) {
		InputDevice_free(result);
		return err;
	}

	return Error_none();
}

#define InputDeviceCreate(InputType) 																\
																									\
	Input##InputType *inputType = InputDevice_get##InputType(d, localHandle);						\
																									\
	if(!res)																						\
		return (Error) { .genericError = GenericError_OutOfBounds, .paramId = 1 };					\
																									\
	if(inputType->name[0])																			\
		return (Error) { .genericError = GenericError_AlreadyDefined };								\
																									\
	if(!inputType)																					\
		return (Error) { .genericError = GenericError_NullPointer };								\
																									\
	if(String_isEmpty(keyName))																		\
		return (Error) { .genericError = GenericError_InvalidParameter, .paramId = 2 };				\
																									\
	if(keyName.len >= LongString_LEN)																\
		return (Error) { .genericError = GenericError_OutOfBounds, .paramId = 2 };					\
																									\
	Buffer_copy(																					\
		Buffer_createRef(inputType->name, LongString_LEN), 											\
		Buffer_createRef(keyName.ptr, keyName.len + 1)												\
	);

Error InputDevice_createButton(
	InputDevice d, 
	U16 localHandle, 
	String keyName, 
	InputHandle *res
) {
	if(d.type == InputDeviceType_Undefined)
		return (Error) { .genericError = GenericError_InvalidOperation };

	InputDeviceCreate(Button);
	*res = InputDevice_createHandle(d, localHandle, InputType_Button);
	return Error_none();
}

Error InputDevice_createAxis(
	InputDevice d, 
	U16 localHandle, 
	String keyName, 
	F32 deadZone, 
	InputHandle *res
) {
	if(d.type == InputDeviceType_Undefined)
		return (Error) { .genericError = GenericError_InvalidOperation };

	InputDeviceCreate(Axis);
	*res = InputDevice_createHandle(d, localHandle, InputType_Axis);
	inputType->deadZone = deadZone;
	return Error_none();
}

Error InputDevice_free(InputDevice *dev) {

	if(dev->type == InputDeviceType_Undefined)
		return Error_none();

	void *allocator = Platform_instance.alloc.ptr;
	FreeFunc free = Platform_instance.alloc.free;

	Error err = free(allocator, dev->handles), errTemp;

	if((errTemp = free(allocator, dev->states)).genericError)
		err = errTemp;

	if((errTemp = free(allocator, dev->dataExt)).genericError)
		err = errTemp;

	*dev = (InputDevice){ 0 };
	return err;
}

InputState InputDevice_getState(InputDevice d, InputHandle handle) {

	if(d.type == InputDeviceType_Undefined || !InputDevice_isButton(d, handle))
		return InputState_Up;

	U16 i = InputDevice_getLocalHandle(d, handle);
	BitRef old = InputDevice_getButtonValue(d, i, false);

	return (InputState)((*old.ptr >> old.off) & 3);
}

F32 InputDevice_getCurrentAxis(InputDevice d, InputHandle handle) {
	return d.type == InputDeviceType_Undefined || !InputDevice_isAxis(d, handle) ? 0 : 
		*InputDevice_getAxisValue(d, InputDevice_getLocalHandle(d, handle), true);
}

F32 InputDevice_getPreviousAxis(InputDevice d, InputHandle handle) {
	return d.type == InputDeviceType_Undefined || !InputDevice_isAxis(d, handle) ? 0 : 
		*InputDevice_getAxisValue(d, InputDevice_getLocalHandle(d, handle), false);
}

F32 InputDevice_getDeltaAxis(InputDevice d, InputHandle handle) {
	return InputDevice_getCurrentAxis(d, handle) - InputDevice_getPreviousAxis(d, handle);
}

String InputDevice_getName(InputDevice d, InputHandle handle) {

	if(d.type == InputDeviceType_Undefined)
		return String_createEmpty();

	U16 localHandle = InputDevice_getLocalHandle(d, handle);

	if(InputDevice_isAxis(d, handle))
		return String_createRefLongStringConst(InputDevice_getAxis(d, localHandle)->name);

	return String_createRefLongStringConst(InputDevice_getButton(d, localHandle)->name);
}

InputHandle InputDevice_getHandle(InputDevice d, String name) {

	if(d.type == InputDeviceType_Undefined)
		return InputDevice_invalidHandle();

	//TODO: We probably wanna optimize this at some point like use a hashmap

	for(U16 i = 0; i < d.buttons; ++i)
		if(String_equalsString(
			String_createRefLongStringConst(InputDevice_getButton(d, i)->name), 
			name,
			StringCase_Insensitive
		))
			return InputDevice_createHandle(d, i, InputType_Button);

	for(U16 i = 0; i < d.axes; ++i)
		if(String_equalsString(
			String_createRefLongStringConst(InputDevice_getAxis(d, i)->name), 
			name,
			StringCase_Insensitive
		))
			return InputDevice_createHandle(d, i, InputType_Axis);

	return InputDevice_invalidHandle();
}

F32 InputDevice_getDeadZone(InputDevice d, InputHandle handle) {
	return d.type == InputDeviceType_Undefined || !InputDevice_isAxis(d, handle) ? 0 : 
		InputDevice_getAxis(d, InputDevice_getLocalHandle(d, handle))->deadZone;
}

//This should only be handled by platform updating the input device

Bool InputDevice_setCurrentState(InputDevice d, InputHandle handle, Bool v) {

	if(d.type == InputDeviceType_Undefined || !InputDevice_isButton(d, handle))
		return false;

	BitRef b = InputDevice_getButtonValue(d, InputDevice_getLocalHandle(d, handle), true);

	BitRef_setTo(b, v);
	return true;
}

Bool InputDevice_setCurrentAxis(InputDevice d, InputHandle handle, F32 v) {

	if(d.type == InputDeviceType_Undefined || !InputDevice_isAxis(d, handle))
		return false;

	*InputDevice_getAxisValue(d, InputDevice_getLocalHandle(d, handle), true) = v;
	return true;
}

void InputDevice_markUpdate(InputDevice d) {

	if(d.type == InputDeviceType_Undefined)
		return;

	for(U16 i = 0; i < d.axes; ++i) {
		F32 *start = InputDevice_getAxisValue(d, i, false);
		start[0] = start[1];
	}

	for (U16 i = 0; i < d.buttons; ++i) {

		BitRef old = InputDevice_getButtonValue(d, i, false);
		BitRef neo = old;
		++neo.off;						//Allowed since we're always aligned

		BitRef_setTo(old, BitRef_get(neo));
	}
}
