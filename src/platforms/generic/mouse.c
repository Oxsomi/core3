#include "platforms/mouse.h"
#include "platforms/input_device.h"
#include "types/error.h"

struct Error Mouse_free(Mouse *dev) {
	return InputDevice_free(dev);
}

#define _button(name)																						\
	if ((err = InputDevice_createButton(																	\
		*result, MouseButton_##name, String_createRefUnsafeConst("MouseButton_" #name), '\0', '\0', &res	\
	)).genericError) {																						\
		Mouse_free(result);																					\
		return err;																							\
	}

#define _axis(name)																							\
	if ((err = InputDevice_createAxis(																		\
		*result, MouseAxis_##name, String_createRefUnsafeConst("MouseAxis_" #name), 0, &res					\
	)).genericError) {																						\
		Mouse_free(result);																					\
		return err;																							\
	}

struct Error Mouse_create(Mouse *result) {

	struct Error err = InputDevice_create(
		MouseButton_Count, MouseAxis_Count, InputDeviceType_Mouse, result
	);

	if(err.genericError)
		return err;

	InputHandle res = 0;

	_button(Left); _button(Middle); _button(Right);
	_button(Back); _button(Forward);

	_axis(X); _axis(Y); _axis(ScrollWheel);

	return Error_none();
}
