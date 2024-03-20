/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

#include "platforms/mouse.h"
#include "platforms/input_device.h"
#include "types/error.h"

#define BUTTON(name)																					\
	if ((err = InputDevice_createButton(																\
		*result, EMouseButton_##name  - EMouseButton_Begin, "EMouseButton_" #name, &res					\
	)).genericError) {																					\
		InputDevice_free(result);																		\
		return err;																						\
	}

#define AXIS(name, resetOnUnfocus)																		\
	if ((err = InputDevice_createAxis(																	\
		*result, EMouseAxis_##name - EMouseAxis_Begin, "EMouseAxis_" #name, 0, resetOnUnfocus, &res		\
	)).genericError) {																					\
		InputDevice_free(result);																		\
		return err;																						\
	}

Error Mouse_create(Mouse *result) {

	Error err = InputDevice_create(
		EMouseButton_Count, EMouseAxis_Count, EInputDeviceType_Mouse, result
	);

	if(err.genericError)
		return err;

	InputHandle res = 0;

	BUTTON(Left);				BUTTON(Middle);			BUTTON(Right);
	BUTTON(Back);				BUTTON(Forward);

	AXIS(X, false);				AXIS(Y, false);
	AXIS(ScrollWheel_X, true);	AXIS(ScrollWheel_Y, true);

	return Error_none();
}
