#include "platforms/mouse.h"
#include "platforms/input_device.h"
#include "types/error.h"

#define _button(name)																						\
	if ((err = InputDevice_createButton(																	\
		*result, EMouseButton_##name, String_createRefUnsafeConst("EMouseButton_" #name), &res				\
	)).genericError) {																						\
		InputDevice_free(result);																			\
		return err;																							\
	}

#define _axis(name)																							\
	if ((err = InputDevice_createAxis(																		\
		*result, EMouseAxis_##name, String_createRefUnsafeConst("EMouseAxis_" #name), 0, &res					\
	)).genericError) {																						\
		InputDevice_free(result);																			\
		return err;																							\
	}

Error Mouse_create(Mouse *result) {

	Error err = InputDevice_create(
		EMouseButton_Count, EMouseAxis_Count, EInputDeviceType_Mouse, result
	);

	if(err.genericError)
		return err;

	InputHandle res = 0;

	_button(Left); _button(Middle); _button(Right);
	_button(Back); _button(Forward);

	_axis(X); _axis(Y); 
	_axis(ScrollWheel_X); _axis(ScrollWheel_Y);

	return Error_none();
}
