#pragma once
#include "types/string.h"

enum InputState {

	InputState_Up,
	InputState_Pressed,
	InputState_Released,
	InputState_Down,

	InputState_Curr = 1 << 0,
	InputState_Prev = 1 << 1
};

enum InputType {
	InputType_Button,
	InputType_Axis
};

typedef U32 InputHandle;		//Don't serialize this, because input devices can change it. Use the name instead.

struct InputButton {
	LongString name;
	C8 contents;
	C8 altContents;
};

struct InputAxis {
	LongString name;
	F32 deadZone;
};

enum InputDeviceType {
	InputDeviceType_Keyboard,
	InputDeviceType_Mouse,
	InputDeviceType_Controller
};

struct InputDevice {

	//How many buttons and axes this device has

	U16 buttons, axes;
	U32 flags;
	enum InputDeviceType type;

	//The names of all handles
	//InputAxis[axes]
	//InputButton[buttons]

	struct Buffer handles;

	//Bitset for the states of the buttons/axes
	//F32[floatStates*2]
	//u2[booleanStates]

	struct Buffer states;
};

//Initializing a device

struct Error InputDevice_create(U16 buttons, U16 axes, enum InputDeviceType type, struct InputDevice *result);

struct Error InputDevice_createButton(
	struct InputDevice dev, 
	U16 localHandle, 
	struct String keyName,			//The alphaNumeric name (e.g. Key_1)
	C8 keyContents,					//For example '1' for Keyboard
	C8 keyAltContents,				//For example '!' for Keyboard
	InputHandle *result
);

struct Error InputDevice_createAxis(
	struct InputDevice dev, 
	U16 localHandle, 
	struct String keyName, 
	F32 deadZone, 
	InputHandle *result
);

struct Error InputDevice_free(struct InputDevice *dev);

struct InputButton *InputDevice_getButton(struct InputDevice dev, U16 localHandle);
struct InputAxis *InputDevice_getAxis(struct InputDevice dev, U16 localHandle);

//Simple helpers

inline U32 InputDevice_getHandles(struct InputDevice d) { return (U32)d.axes + d.buttons; }
inline InputHandle InputDevice_invalidHandle(struct InputDevice d) { return (InputHandle) InputDevice_getHandles(d); }

inline Bool InputDevice_isValidHandle(struct InputDevice d, InputHandle handle) { 
	return handle < InputDevice_getHandles(d); 
}

inline Bool InputDevice_isAxis(struct InputDevice d, InputHandle handle) {
	return handle < d.buttons;
}

inline Bool InputDevice_isButton(struct InputDevice d, InputHandle handle) {
	return !InputDevice_isAxis(d, handle);
}

inline InputHandle InputDevice_createHandle(struct InputDevice d, U16 localHandle, enum InputType type) { 
	return localHandle + (InputHandle)(type == InputType_Axis ? 0 : d.axes); 
}

inline U16 InputDevice_getLocalHandle(struct InputDevice d, InputHandle handle) {
	return (U16)(handle - (InputDevice_isAxis(d, handle) ? 0 : d.axes));
}

//Getting previous/current states

enum InputState InputDevice_getState(struct InputDevice d, InputHandle handle);

inline Bool InputDevice_hasFlag(struct InputDevice d, U8 flag) {

	if(flag >= 32)
		return false;

	return (d.flags >> flag) & 1;
}

inline Bool InputDevice_setFlag(struct InputDevice *d, U8 flag) {

	if(flag >= 32 || !d)
		return false;

	d->flags |= (U32)1 << flag;
	return true;
}

inline Bool InputDevice_resetFlag(struct InputDevice *d, U8 flag) {

	if(flag >= 32 || !d)
		return false;

	d->flags &= ~((U32)1 << flag);
	return true;
}

inline Bool InputDevice_setFlagTo(struct InputDevice *d, U8 flag, Bool value) {
	return value ? InputDevice_setFlag(d, flag) : InputDevice_resetFlag(d, flag);
}

inline Bool InputDevice_getCurrentState(struct InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) & InputState_Curr; 
}

inline Bool InputDevice_getPreviousState(struct InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) & InputState_Prev; 
}

F32 InputDevice_getDeltaAxis(struct InputDevice d, InputHandle handle);
F32 InputDevice_getCurrentAxis(struct InputDevice d, InputHandle handle);
F32 InputDevice_getPreviousAxis(struct InputDevice d, InputHandle handle);

inline Bool InputDevice_isDown(struct InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == InputState_Down; 
}

inline Bool InputDevice_isUp(struct InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == InputState_Up; 
}

inline Bool InputDevice_isReleased(struct InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == InputState_Released; 
}

inline Bool InputDevice_isPressed(struct InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == InputState_Pressed;
}

//For serialization and stuff like that

InputHandle InputDevice_getHandle(struct InputDevice d, struct String name);
struct String InputDevice_getName(struct InputDevice d, InputHandle handle);

F32 InputDevice_getDeadZone(struct InputDevice d, InputHandle handle);
C8 InputDevice_getResolvedChar(struct InputDevice d, InputHandle handle);
C8 InputDevice_getResolvedAltChar(struct InputDevice d, InputHandle handle);

//This should only be handled by platform updating the input device
//First the platform should call markUpdate, then it should start setting new values
//Platform should also unset current axis if focus is lost

Bool InputDevice_setCurrentState(struct InputDevice d, InputHandle handle, Bool v);
Bool InputDevice_setCurrentAxis(struct InputDevice d, InputHandle handle, F32 v);

void InputDevice_markUpdate(struct InputDevice d);
