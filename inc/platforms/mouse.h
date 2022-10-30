#pragma once
#include "types/types.h"

typedef struct InputDevice Mouse;

typedef enum MouseActions {

	//Mouse buttons

	MouseButton_Begin,

	MouseButton_Left = MouseButton_Begin, MouseButton_Middle, MouseButton_Right, 
	MouseButton_Back, MouseButton_Forward,

	MouseButton_End,
	MouseButton_Count = MouseButton_End - MouseButton_Begin,

	//Mouse axes

	MouseAxis_Begin = MouseButton_End,

	MouseAxis_X = MouseAxis_Begin, MouseAxis_Y, 
	MouseAxis_ScrollWheel_X, MouseAxis_ScrollWheel_Y,

	MouseAxis_End,
	MouseAxis_Count = MouseAxis_End - MouseAxis_Begin

} MouseActions;

typedef enum MouseFlag {

	//When this happens, the mouse position is relative, not absolute
	//So, the cursor position shouldn't be taken as absolute
	//This means using the delta functions instead of the absolute functions

	MouseFlag_IsRelative

} MouseFlag;

typedef struct Error Error;

Error Mouse_create(Mouse *result);
