#pragma once
#include "types/string.h"

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
	LongString name;
} InputButton;

typedef struct InputAxis {
	LongString name;
	F32 deadZone;
} InputAxis;

typedef enum EInputDeviceType {
	EInputDeviceType_Undefined,
	EInputDeviceType_EKeyboard,
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
	//u2[booleanStates]

	Buffer states;

	//Platform dependent data, for better identifying this device
	//This is useful for when multiple keyboards are present for example

	Buffer dataExt;

} InputDevice;

//Initializing a device

Error InputDevice_create(U16 buttons, U16 axes, EInputDeviceType type, InputDevice *result);

Error InputDevice_createButton(
	InputDevice dev, 
	U16 localHandle, 
	String keyName,			//The alphaNumeric name (e.g. EKey_1)
	InputHandle *result
);

Error InputDevice_createAxis(
	InputDevice dev, 
	U16 localHandle, 
	String keyName, 
	F32 deadZone, 
	InputHandle *result
);

Error InputDevice_free(InputDevice *dev);

InputButton *InputDevice_getButton(InputDevice dev, U16 localHandle);
InputAxis *InputDevice_getAxis(InputDevice dev, U16 localHandle);

//Simple helpers

inline U32 InputDevice_getHandles(InputDevice d) { return (U32)d.axes + d.buttons; }

inline InputHandle InputDevice_invalidHandle() { return (InputHandle) U64_MAX; }

inline Bool InputDevice_isValidHandle(InputDevice d, InputHandle handle) { 
	return handle < InputDevice_getHandles(d); 
}

inline Bool InputDevice_isAxis(InputDevice d, InputHandle handle) {
	return handle < d.buttons;
}

inline Bool InputDevice_isButton(InputDevice d, InputHandle handle) {
	return !InputDevice_isAxis(d, handle);
}

inline InputHandle InputDevice_createHandle(InputDevice d, U16 localHandle, EInputType type) { 
	return localHandle + (InputHandle)(type == EInputType_Axis ? 0 : d.axes); 
}

inline U16 InputDevice_getLocalHandle(InputDevice d, InputHandle handle) {
	return (U16)(handle - (InputDevice_isAxis(d, handle) ? 0 : d.axes));
}

//Getting previous/current states

EInputState InputDevice_getState(InputDevice d, InputHandle handle);

inline Bool InputDevice_hasFlag(InputDevice d, U8 flag) {

	if(flag >= 32)
		return false;

	return (d.flags >> flag) & 1;
}

inline Bool InputDevice_setFlag(InputDevice *d, U8 flag) {

	if(flag >= 32 || !d)
		return false;

	d->flags |= (U32)1 << flag;
	return true;
}

inline Bool InputDevice_resetFlag(InputDevice *d, U8 flag) {

	if(flag >= 32 || !d)
		return false;

	d->flags &= ~((U32)1 << flag);
	return true;
}

inline Bool InputDevice_setFlagTo(InputDevice *d, U8 flag, Bool value) {
	return value ? InputDevice_setFlag(d, flag) : InputDevice_resetFlag(d, flag);
}

inline Bool InputDevice_getCurrentState(InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) & EInputState_Curr; 
}

inline Bool InputDevice_getPreviousState(InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) & EInputState_Prev; 
}

F32 InputDevice_getDeltaAxis(InputDevice d, InputHandle handle);
F32 InputDevice_getCurrentAxis(InputDevice d, InputHandle handle);
F32 InputDevice_getPreviousAxis(InputDevice d, InputHandle handle);

inline Bool InputDevice_isDown(InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == EInputState_Down; 
}

inline Bool InputDevice_isUp(InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == EInputState_Up; 
}

inline Bool InputDevice_isReleased(InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == EInputState_Released; 
}

inline Bool InputDevice_isPressed(InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == EInputState_Pressed;
}

//For serialization and stuff like that

InputHandle InputDevice_getHandle(InputDevice d, String name);
String InputDevice_getName(InputDevice d, InputHandle handle);

F32 InputDevice_getDeadZone(InputDevice d, InputHandle handle);

//This should only be handled by platform updating the input device
//First the platform should call markUpdate, then it should start setting new values
//Platform should also unset current axis if focus is lost

Bool InputDevice_setCurrentState(InputDevice d, InputHandle handle, Bool v);
Bool InputDevice_setCurrentAxis(InputDevice d, InputHandle handle, F32 v);

void InputDevice_markUpdate(InputDevice d);
