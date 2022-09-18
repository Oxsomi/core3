#include "types/string.h"
#include "types/error.h"
#include "platforms/input_device.h"
#include "platforms/platform.h"
#include "types/bit.h"

//Private helpers

//Bitset for the states of the buttons/axes
//f32[floatStates*2]
//u2[booleanStates]

inline struct InputButton *InputDevice_getButtonName(struct InputDevice dev, u16 localHandle) {

	return localHandle >= dev.buttons ? NULL : 
		(struct InputButton*)((struct InputAxis*) dev.handles.ptr + dev.axes) + localHandle;
}

inline struct InputAxis *InputDevice_getAxisName(struct InputDevice dev, u16 localHandle) {
	return localHandle >= dev.axes ? NULL :
		(struct InputAxis*) dev.handles.ptr + localHandle;
}

inline f32 *InputDevice_getAxisValue(struct InputDevice dev, u16 localHandle, bool isCurrent) {
	return localHandle >= dev.axes ? NULL : 
		(f32*)dev.states.ptr + (localHandle << 1) + isCurrent;
}

inline struct BitRef InputDevice_getButtonValue(struct InputDevice dev, u16 localHandle, bool isCurrent) {
	
	if(localHandle >= dev.buttons)
		return (struct BitRef){ 0 };

	u64 bitOff = (localHandle << 1) + isCurrent;
	u8 *off = dev.states.ptr + dev.axes * 2 * sizeof(f32) + (bitOff >> 3);

	return (struct BitRef){ .ptr = off, .off = (bitOff & 7) };
}

//

struct Error InputDevice_create(u16 buttons, u16 axes, struct InputDevice *result) {

	if(!result)
		return (struct Error) { .genericError = GenericError_NullPointer };

	*result = (struct InputDevice) {
		.buttons = buttons,
		.axes = axes
	};

	u64 handles = sizeof(struct InputAxis) * axes + sizeof(struct InputButton) * buttons;
	u64 states = sizeof(f32) * 2 * axes + (buttons * 2 + 7) >> 3;

	struct Error err = Bit_createEmpty(handles, Platform_instance.alloc, &result->handles);

	if (err.genericError) {
		InputDevice_free(result);
		return err;
	}

	err = Bit_createEmpty(states, Platform_instance.alloc, &result->states);

	if (err.genericError) {
		InputDevice_free(result);
		return err;
	}

	return Error_none();
}

#define InputDeviceCreate(InputType) 																\
																									\
	struct Input##InputType *inputType = InputDevice_get##InputType##Name(d, localHandle);			\
																									\
	if(!res)																						\
		return (struct Error) { .genericError = GenericError_OutOfBounds, .paramId = 1 };			\
																									\
	if(inputType->name[0])																			\
		return (struct Error) { .genericError = GenericError_AlreadyDefined };						\
																									\
	if(!inputType)																					\
		return (struct Error) { .genericError = GenericError_NullPointer };							\
																									\
	if(String_isEmpty(keyName))																		\
		return (struct Error) { .genericError = GenericError_InvalidParameter, .paramId = 2 };		\
																									\
	if(keyName.len >= ShortString_LEN)																\
		return (struct Error) { .genericError = GenericError_OutOfBounds, .paramId = 2 };			\
																									\
	Bit_copy(																						\
		Bit_createRef(inputType->name, ShortString_LEN), 											\
		Bit_createRef(keyName.ptr, keyName.len + 1)													\
	);																								\
																									\
	*res = InputDevice_createHandle(d, localHandle, InputType_##InputType)							\

struct Error InputDevice_createButton(
	struct InputDevice d, 
	u16 localHandle, 
	struct String keyName, 
	InputHandle *res
) {
	InputDeviceCreate(Button);
	return Error_none();
}

struct Error InputDevice_createAxis(
	struct InputDevice d, 
	u16 localHandle, 
	struct String keyName, 
	f32 deadZone, 
	InputHandle *res
) {
	InputDeviceCreate(Axis);
	inputType->deadZone = deadZone;
	return Error_none();
}

struct Error InputDevice_free(struct InputDevice *dev) {

	void *allocator = Platform_instance.alloc.ptr;
	FreeFunc free = Platform_instance.alloc.free;

	struct Error err = free(allocator, dev->handles);

	if(err.genericError) {

		free(allocator, dev->states);		//Still try to free the other
		*dev = (struct InputDevice){ 0 };

		return err;
	}

	err = free(allocator, dev->states);
	*dev = (struct InputDevice){ 0 };

	if(err.genericError)
		return err;

	return Error_none();
}

enum InputState InputDevice_getState(struct InputDevice d, InputHandle handle) {

	if(!InputDevice_isButton(d, handle))
		return InputState_Up;

	u16 i = InputDevice_getLocalHandle(d, handle);
	struct BitRef old = InputDevice_getButtonValue(d, i, false);

	return (enum InputState)((*old.ptr >> old.off) & 3);
}

f32 InputDevice_getCurrentAxis(struct InputDevice d, InputHandle handle) {
	return !InputDevice_isAxis(d, handle) ? 0 : 
		*InputDevice_getAxisValue(d, InputDevice_getLocalHandle(d, handle), true);
}

f32 InputDevice_getPreviousAxis(struct InputDevice d, InputHandle handle) {
	return !InputDevice_isAxis(d, handle) ? 0 : 
		*InputDevice_getAxisValue(d, InputDevice_getLocalHandle(d, handle), false);
}

f32 InputDevice_getDeltaAxis(struct InputDevice d, InputHandle handle) {
	return InputDevice_getCurrentAxis(d, handle) - InputDevice_getPreviousAxis(d, handle);
}

struct String InputDevice_getName(struct InputDevice d, InputHandle handle) {

	u16 localHandle = InputDevice_getLocalHandle(d, handle);

	if(InputDevice_isAxis(d, handle))
		return String_createRefShortStringConst(InputDevice_getAxisName(d, localHandle)->name);

	return String_createRefShortStringConst(InputDevice_getButtonName(d, localHandle)->name);
}

InputHandle InputDevice_getHandle(struct InputDevice d, struct String name) {

	//TODO: We probably wanna optimize this at some point like use a hashmap

	for(u16 i = 0; i < d.buttons; ++i)
		if(String_equalsString(
			String_createRefShortStringConst(InputDevice_getButtonName(d, i)->name), 
			name,
			StringCase_Insensitive
		))
			return InputDevice_createHandle(d, i, InputType_Button);

	for(u16 i = 0; i < d.axes; ++i)
		if(String_equalsString(
			String_createRefShortStringConst(InputDevice_getAxisName(d, i)->name), 
			name,
			StringCase_Insensitive
		))
			return InputDevice_createHandle(d, i, InputType_Axis);

	return InputDevice_invalidHandle(d);
}

f32 InputDevice_getDeadZone(struct InputDevice d, InputHandle handle) {
	return !InputDevice_isAxis(d, handle) ? 0 : 
		InputDevice_getAxisName(d, InputDevice_getLocalHandle(d, handle))->deadZone;
}

//This should only be handled by platform updating the input device

bool InputDevice_setCurrentState(struct InputDevice d, InputHandle handle, bool v) {

	if(!InputDevice_isButton(d, handle))
		return false;

	struct BitRef b = InputDevice_getButtonValue(d, InputDevice_getLocalHandle(d, handle), true);

	BitRef_setTo(b, v);
	return true;
}

bool InputDevice_setCurrentAxis(struct InputDevice d, InputHandle handle, f32 v) {

	if(!InputDevice_isAxis(d, handle))
		return false;

	*InputDevice_getAxisValue(d, InputDevice_getLocalHandle(d, handle), true) = v;
	return true;
}

void InputDevice_markUpdate(struct InputDevice d) {

	for(u16 i = 0; i < d.axes; ++i) {
		f32 *start = InputDevice_getAxisValue(d, i, false);
		start[0] = start[1];
	}

	for (u16 i = 0; i < d.buttons; ++i) {

		struct BitRef old = InputDevice_getButtonValue(d, i, false);
		struct BitRef neo = old;
		++neo.off;						//Allowed since we're always aligned

		BitRef_setTo(old, BitRef_get(neo));
	}
}