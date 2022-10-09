#pragma once
#include "types/types.h"

typedef struct InputDevice Mouse;

enum MouseButton {

	MouseButton_Left, MouseButton_Middle, MouseButton_Right, 
	MouseButton_Back, MouseButton_Forward,

	MouseButton_Count
};

enum MouseAxis {

	MouseAxis_X, MouseAxis_Y, MouseAxis_ScrollWheel,

	MouseAxis_Count
};

struct Error Mouse_create(Mouse *result);
struct Error Mouse_free(Mouse *dev);
