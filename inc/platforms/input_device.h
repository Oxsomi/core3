#pragma once
#include "types/types.h"

enum InputState {
	InputState_Down,
	InputState_Pressed,
	InputState_Released,
	InputState_Down
};

enum InputType {
	InputType_Button,
	InputType_Axis
};

typedef u32 InputHandle;

typedef c8 InputButton[32];

struct InputAxis {
	c8 name[32];
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

struct InputDevice InputDevice_create(u16 buttons, u16 axes);
InputHandle InputDevice_createButton(struct InputDevice dev, u16 localHandle, const c8 *keyName);
InputHandle InputDevice_createAxis(struct InputDevice dev, u16 localHandle, const c8 *keyName, f32 deadZone);
void InputDevice_free(struct InputDevice dev);

//Simple helpers

inline u16 InputDevice_getButtons(struct InputDevice d) { return d.buttons; }
inline u16 InputDevice_getAxes(struct InputDevice d) { return d.axes; }
inline u32 InputDevice_getHandles(struct InputDevice d) { return (u32)d.axes + d.buttons; }

inline bool InputDevice_isValidHandle(struct InputDevice d, InputHandle handle) { 
	return handle < InputDevice_getHandles(d); 
}

inline bool InputDevice_isAxis(struct InputDevice d, InputHandle handle) {
	return handle < InputDevice_getButtons(d);
}

inline bool InputDevice_isButton(struct InputDevice d, InputHandle handle) {
	return !InputDevice_isAxis(d, handle);
}

inline InputHandle InputDevice_createHandle(struct InputDevice d, u16 localHandle, enum InputType type) { 
	return localHandle + (u32)(type == InputType_Axis ? d.buttons : 0); 
}

inline u16 InputDevice_getLocalHandle(struct InputDevice d, InputHandle handle) {
	return (u16)(handle - (InputDevice_isAxis(d, handle) ? d.buttons : 0));
}

//Getting previous/current states

enum InputState InputDevice_getState(struct InputDevice d, InputHandle handle);
bool InputDevice_getCurrentState(struct InputDevice d, InputHandle handle);
bool InputDevice_getPreviousState(struct InputDevice d, InputHandle handle);

f32 InputDevice_getDeltaAxis(struct InputDevice d, InputHandle handle);
f32 InputDevice_getCurrentAxis(struct InputDevice d, InputHandle handle);
f32 InputDevice_getPreviousAxis(struct InputDevice d, InputHandle handle);

inline bool InputDevice_isDown(struct InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == InputState_Down; 
}

inline bool InputDevice_isUp(struct InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == InputState_Down; 
}

inline bool InputDevice_isReleased(struct InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == InputState_Released; 
}

inline bool InputDevice_isPressed(struct InputDevice d, InputHandle handle) { 
	return InputDevice_getState(d, handle) == InputState_Pressed;
}

//For serialization and stuff like that

InputHandle InputDevice_getHandle(struct InputDevice d, const c8 *name);
const c8 *InputDevice_getName(struct InputDevice d, InputHandle handle);

f32 InputDevice_getDeadZone(struct InputDevice d, InputHandle handle);

//This should only be handled by platform updating the input device

void InputDevice_setCurrentState(struct InputDevice d, InputHandle handle);
void InputDevice_setCurrentAxis(struct InputDevice d, InputHandle handle);

void InputDevice_markUpdate(struct InputDevice d);