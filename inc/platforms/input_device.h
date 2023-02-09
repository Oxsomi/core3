/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#pragma once
#include "types/types.h"
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

Bool InputDevice_free(InputDevice *dev);

InputButton *InputDevice_getButton(InputDevice dev, U16 localHandle);
InputAxis *InputDevice_getAxis(InputDevice dev, U16 localHandle);

//Simple helpers

U32 InputDevice_getHandles(InputDevice d);

InputHandle InputDevice_invalidHandle();

Bool InputDevice_isValidHandle(InputDevice d, InputHandle handle);
Bool InputDevice_isAxis(InputDevice d, InputHandle handle);
Bool InputDevice_isButton(InputDevice d, InputHandle handle);

InputHandle InputDevice_createHandle(InputDevice d, U16 localHandle, EInputType type);
U16 InputDevice_getLocalHandle(InputDevice d, InputHandle handle);

//Getting previous/current states

EInputState InputDevice_getState(InputDevice d, InputHandle handle);

Bool InputDevice_hasFlag(InputDevice d, U8 flag);
Bool InputDevice_setFlag(InputDevice *d, U8 flag);
Bool InputDevice_resetFlag(InputDevice *d, U8 flag);

Bool InputDevice_setFlagTo(InputDevice *d, U8 flag, Bool value);

Bool InputDevice_getCurrentState(InputDevice d, InputHandle handle);
Bool InputDevice_getPreviousState(InputDevice d, InputHandle handle);

F32 InputDevice_getDeltaAxis(InputDevice d, InputHandle handle);
F32 InputDevice_getCurrentAxis(InputDevice d, InputHandle handle);
F32 InputDevice_getPreviousAxis(InputDevice d, InputHandle handle);

Bool InputDevice_isDown(InputDevice d, InputHandle handle);
Bool InputDevice_isUp(InputDevice d, InputHandle handle);
Bool InputDevice_isReleased(InputDevice d, InputHandle handle);
Bool InputDevice_isPressed(InputDevice d, InputHandle handle);

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
