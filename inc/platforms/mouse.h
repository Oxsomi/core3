#pragma once
#include "types/types.h"

typedef struct Error Error;
typedef struct InputDevice Mouse;

typedef enum EMouseActions {

	//Mouse buttons

	EMouseButton_Begin,

	EMouseButton_Left = EMouseButton_Begin, EMouseButton_Middle, EMouseButton_Right, 
	EMouseButton_Back, EMouseButton_Forward,

	EMouseButton_End,
	EMouseButton_Count = EMouseButton_End - EMouseButton_Begin,

	//Mouse axes

	EMouseAxis_Begin = EMouseButton_End,

	EMouseAxis_X = EMouseAxis_Begin, EMouseAxis_Y, 
	EMouseAxis_ScrollWheel_X, EMouseAxis_ScrollWheel_Y,

	EMouseAxis_End,
	EMouseAxis_Count = EMouseAxis_End - EMouseAxis_Begin

} MouseActions;

typedef enum EMouseFlag {

	//When this happens, the mouse position is relative, not absolute
	//So, the cursor position shouldn't be taken as absolute
	//This means using the delta functions instead of the absolute functions

	EMouseFlag_IsRelative

} EMouseFlag;

Error Mouse_create(Mouse *result);
