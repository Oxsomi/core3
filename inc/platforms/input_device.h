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

typedef u32 InputHandle;		//Don't serialize this, because input devices can change it. Use the name instead.

struct InputButton {
	ShortString name;
};

struct InputAxis {
	ShortString name;
	f32 deadZone;
};

struct InputDevice {

	//How many buttons and axes this device has

	u16 buttons, axes;

	//The names of all handles
	//InputAxis[axes]
	//const c8 names[32][buttons]

	struct Buffer handles;

	//Bitset for the states of the buttons/axes
	//f32[floatStates*2]
	//u2[booleanStates]

	struct Buffer states;

};

//Initializing a device

struct Error InputDevice_create(u16 buttons, u16 axes, struct InputDevice *result);
struct Error InputDevice_createButton(struct InputDevice dev, u16 localHandle, struct String keyName, InputHandle *result);
struct Error InputDevice_createAxis(struct InputDevice dev, u16 localHandle, struct String keyName, f32 deadZone, InputHandle *result);
struct Error InputDevice_free(struct InputDevice *dev);

//Simple helpers

inline u32 InputDevice_getHandles(struct InputDevice d) { return (u32)d.axes + d.buttons; }
inline InputHandle InputDevice_invalidHandle(struct InputDevice d) { return (InputHandle) InputDevice_getHandles(d); }

inline bool InputDevice_isValidHandle(struct InputDevice d, InputHandle handle) { 
	return handle < InputDevice_getHandles(d); 
}

inline bool InputDevice_isAxis(struct InputDevice d, InputHandle handle) {
	return handle < d.buttons;
}

inline bool InputDevice_isButton(struct InputDevice d, InputHandle handle) {
	return !InputDevice_isAxis(d, handle);
}

inline InputHandle InputDevice_createHandle(struct InputDevice d, u16 localHandle, enum InputType type) { 
	return localHandle + (InputHandle)(type == InputType_Axis ? 0 : d.axes); 
}

inline u16 InputDevice_getLocalHandle(struct InputDevice d, InputHandle handle) {
	return (u16)(handle - (InputDevice_isAxis(d, handle) ? 0 : d.axes));
}

//Getting previous/current states

enum InputState InputDevice_getState(struct InputDevice d, InputHandle handle);

bool InputDevice_getCurrentState(struct InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) & InputState_Curr; 
}

bool InputDevice_getPreviousState(struct InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) & InputState_Prev; 
}

f32 InputDevice_getDeltaAxis(struct InputDevice d, InputHandle handle);
f32 InputDevice_getCurrentAxis(struct InputDevice d, InputHandle handle);
f32 InputDevice_getPreviousAxis(struct InputDevice d, InputHandle handle);

inline bool InputDevice_isDown(struct InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == InputState_Down; 
}

inline bool InputDevice_isUp(struct InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == InputState_Up; 
}

inline bool InputDevice_isReleased(struct InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == InputState_Released; 
}

inline bool InputDevice_isPressed(struct InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == InputState_Pressed;
}

//For serialization and stuff like that

InputHandle InputDevice_getHandle(struct InputDevice d, struct String name);
struct String InputDevice_getName(struct InputDevice d, InputHandle handle);

f32 InputDevice_getDeadZone(struct InputDevice d, InputHandle handle);

//This should only be handled by platform updating the input device
//First the platform should call markUpdate, then it should start setting new values
//Platform should also unset current axis if focus is lost

bool InputDevice_setCurrentState(struct InputDevice d, InputHandle handle, bool v);
bool InputDevice_setCurrentAxis(struct InputDevice d, InputHandle handle, f32 v);

void InputDevice_markUpdate(struct InputDevice d);