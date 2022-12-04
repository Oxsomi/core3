#include "types/string.h"
#include "types/error.h"
#include "types/buffer.h"
#include "platforms/input_device.h"
#include "platforms/platform.h"
#include "platforms/ext/bufferx.h"

//Private helpers

//Bitset for the states of the buttons/axes
//F32[floatStates*2]
//u2[booleanStates]

InputButton *InputDevice_getButton(InputDevice dev, U16 localHandle) {
	return localHandle >= dev.buttons ? NULL : (InputButton*)((InputAxis*) dev.handles.ptr + dev.axes) + localHandle;
}

InputAxis *InputDevice_getAxis(InputDevice dev, U16 localHandle) {
	return localHandle >= dev.axes ? NULL : (InputAxis*) dev.handles.ptr + localHandle;
}

U32 InputDevice_getHandles(InputDevice d) { return (U32)d.axes + d.buttons; }

InputHandle InputDevice_invalidHandle() { return (InputHandle) U64_MAX; }

Bool InputDevice_isValidHandle(InputDevice d, InputHandle handle) { return handle < InputDevice_getHandles(d); }

Bool InputDevice_isAxis(InputDevice d, InputHandle handle) { return handle < d.buttons; }
Bool InputDevice_isButton(InputDevice d, InputHandle handle) { return !InputDevice_isAxis(d, handle); }

InputHandle InputDevice_createHandle(InputDevice d, U16 localHandle, EInputType type) { 
	return localHandle + (InputHandle)(type == EInputType_Axis ? 0 : d.axes); 
}

U16 InputDevice_getLocalHandle(InputDevice d, InputHandle handle) {
	return (U16)(handle - (InputDevice_isAxis(d, handle) ? 0 : d.axes));
}

//Getting previous/current states

Bool InputDevice_hasFlag(InputDevice d, U8 flag) {

	if(flag >= 32)
		return false;

	return (d.flags >> flag) & 1;
}

Bool InputDevice_setFlag(InputDevice *d, U8 flag) {

	if(flag >= 32 || !d)
		return false;

	d->flags |= (U32)1 << flag;
	return true;
}

Bool InputDevice_resetFlag(InputDevice *d, U8 flag) {

	if(flag >= 32 || !d)
		return false;

	d->flags &= ~((U32)1 << flag);
	return true;
}

Bool InputDevice_setFlagTo(InputDevice *d, U8 flag, Bool value) {
	return value ? InputDevice_setFlag(d, flag) : InputDevice_resetFlag(d, flag);
}

Bool InputDevice_getCurrentState(InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) & EInputState_Curr; 
}

Bool InputDevice_getPreviousState(InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) & EInputState_Prev; 
}

inline F32 *InputDevice_getAxisValue(InputDevice dev, U16 localHandle, Bool isCurrent) {
	return localHandle >= dev.axes ? NULL : (F32*)dev.states.ptr + ((U64)localHandle << 1) + isCurrent;
}

inline BitRef InputDevice_getButtonValue(InputDevice dev, U16 localHandle, Bool isCurrent) {
	
	if(localHandle >= dev.buttons)
		return (BitRef){ 0 };

	U64 bitOff = ((U32)localHandle << 1) + isCurrent;
	U8 *off = dev.states.ptr + dev.axes * 2 * sizeof(F32) + (bitOff >> 3);

	return (BitRef){ .ptr = off, .off = (bitOff & 7) };
}

//

Error InputDevice_create(U16 buttons, U16 axes, EInputDeviceType type, InputDevice *result) {

	if(!result)
		return Error_nullPointer(3, 0);

	*result = (InputDevice) {
		.buttons = buttons,
		.axes = axes,
		.type = type
	};

	U64 handles = sizeof(InputAxis) * axes + sizeof(InputButton) * buttons;
	U64 states = sizeof(F32) * 2 * axes + (((U64)buttons * 2 + 7) >> 3);

	Error err = Buffer_createEmptyBytesx(handles, &result->handles);

	if (err.genericError) {
		InputDevice_free(result);
		return err;
	}

	err = Buffer_createEmptyBytesx(states, &result->states);

	if (err.genericError) {
		InputDevice_free(result);
		return err;
	}

	return Error_none();
}

#define InputDeviceCreate(EInputType) 													\
																						\
	if(d.type == EInputDeviceType_Undefined)											\
		return Error_invalidOperation(0);												\
																						\
	Input##EInputType *inputType = InputDevice_get##EInputType(d, localHandle);			\
																						\
	if(!res)																			\
		return Error_nullPointer(4, 0);													\
																						\
	if(inputType->name[0])																\
		return Error_alreadyDefined(0);													\
																						\
	if(!inputType)																		\
		return Error_nullPointer(0, 0);													\
																						\
	if(String_isEmpty(keyName))															\
		return Error_invalidParameter(2, 0, 0);											\
																						\
	if(keyName.length >= _LONGSTRING_LEN)												\
		return Error_outOfBounds(2, 0, keyName.length, _LONGSTRING_LEN);				\
																						\
	Buffer_copy(																		\
		Buffer_createRef(inputType->name, _LONGSTRING_LEN), 							\
		Buffer_createConstRef(keyName.ptr, keyName.length)								\
	);																					\
																						\
	inputType->name[U64_min(keyName.length, _LONGSTRING_LEN - 1)] = '\0';

Error InputDevice_createButton(
	InputDevice d, 
	U16 localHandle, 
	String keyName, 
	InputHandle *res
) {
	InputDeviceCreate(Button);
	*res = InputDevice_createHandle(d, localHandle, EInputType_Button);
	return Error_none();
}

Error InputDevice_createAxis(
	InputDevice d, 
	U16 localHandle, 
	String keyName, 
	F32 deadZone, 
	InputHandle *res
) {
	InputDeviceCreate(Axis);
	*res = InputDevice_createHandle(d, localHandle, EInputType_Axis);
	inputType->deadZone = deadZone;
	return Error_none();
}

Bool InputDevice_free(InputDevice *dev) {

	if (!dev || dev->type == EInputDeviceType_Undefined)
		return true;

	Bool freed = true;

	freed &= Buffer_freex(&dev->handles);
	freed &= Buffer_freex(&dev->states);
	freed &= Buffer_freex(&dev->dataExt);

	*dev = (InputDevice){ 0 };
	return freed;
}

EInputState InputDevice_getState(InputDevice d, InputHandle handle) {

	if(d.type == EInputDeviceType_Undefined || !InputDevice_isButton(d, handle))
		return EInputState_Up;

	U16 i = InputDevice_getLocalHandle(d, handle);
	BitRef old = InputDevice_getButtonValue(d, i, false);

	return (EInputState)((*old.ptr >> old.off) & 3);
}

F32 InputDevice_getCurrentAxis(InputDevice d, InputHandle handle) {
	return d.type == EInputDeviceType_Undefined || !InputDevice_isAxis(d, handle) ? 0 : 
		*InputDevice_getAxisValue(d, InputDevice_getLocalHandle(d, handle), true);
}

F32 InputDevice_getPreviousAxis(InputDevice d, InputHandle handle) {
	return d.type == EInputDeviceType_Undefined || !InputDevice_isAxis(d, handle) ? 0 : 
		*InputDevice_getAxisValue(d, InputDevice_getLocalHandle(d, handle), false);
}

Bool InputDevice_isDown(InputDevice d, InputHandle handle) { return InputDevice_getState(d, handle) == EInputState_Down; }
Bool InputDevice_isUp(InputDevice d, InputHandle handle) { return InputDevice_getState(d, handle) == EInputState_Up; }

Bool InputDevice_isReleased(InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == EInputState_Released; 
}

Bool InputDevice_isPressed(InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == EInputState_Pressed;
}

F32 InputDevice_getDeltaAxis(InputDevice d, InputHandle handle) {
	return InputDevice_getCurrentAxis(d, handle) - InputDevice_getPreviousAxis(d, handle);
}

String InputDevice_getName(InputDevice d, InputHandle handle) {

	if(d.type == EInputDeviceType_Undefined)
		return String_createNull();

	U16 localHandle = InputDevice_getLocalHandle(d, handle);

	if(InputDevice_isAxis(d, handle))
		return String_createConstRefLongString(InputDevice_getAxis(d, localHandle)->name);

	return String_createConstRefLongString(InputDevice_getButton(d, localHandle)->name);
}

InputHandle InputDevice_getHandle(InputDevice d, String name) {

	if(d.type == EInputDeviceType_Undefined)
		return InputDevice_invalidHandle();

	//TODO: We probably wanna optimize this at some point like use a hashmap

	for(U16 i = 0; i < d.buttons; ++i)
		if(String_equalsString(
			String_createConstRefLongString(InputDevice_getButton(d, i)->name), 
			name,
			EStringCase_Insensitive
		))
			return InputDevice_createHandle(d, i, EInputType_Button);

	for(U16 i = 0; i < d.axes; ++i)
		if(String_equalsString(
			String_createConstRefLongString(InputDevice_getAxis(d, i)->name), 
			name,
			EStringCase_Insensitive
		))
			return InputDevice_createHandle(d, i, EInputType_Axis);

	return InputDevice_invalidHandle();
}

F32 InputDevice_getDeadZone(InputDevice d, InputHandle handle) {
	return d.type == EInputDeviceType_Undefined || !InputDevice_isAxis(d, handle) ? 0 : 
		InputDevice_getAxis(d, InputDevice_getLocalHandle(d, handle))->deadZone;
}

//This should only be handled by platform updating the input device

Bool InputDevice_setCurrentState(InputDevice d, InputHandle handle, Bool v) {

	if(d.type == EInputDeviceType_Undefined || !InputDevice_isButton(d, handle))
		return false;

	BitRef b = InputDevice_getButtonValue(d, InputDevice_getLocalHandle(d, handle), true);

	BitRef_setTo(b, v);
	return true;
}

Bool InputDevice_setCurrentAxis(InputDevice d, InputHandle handle, F32 v) {

	if(d.type == EInputDeviceType_Undefined || !InputDevice_isAxis(d, handle))
		return false;

	*InputDevice_getAxisValue(d, InputDevice_getLocalHandle(d, handle), true) = v;
	return true;
}

void InputDevice_markUpdate(InputDevice d) {

	if(d.type == EInputDeviceType_Undefined)
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
