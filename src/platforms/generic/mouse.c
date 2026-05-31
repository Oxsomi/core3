/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

//platforms/mouse.c

#include "platforms/mouse.h"
#include "platforms/input_device.h"
#include "types/base/error.h"

#define BUTTON(name) gotoIfError3(clean, InputDevice_createButton(                                                \
		result, EMouseButton_##name  - EMouseButton_Begin, "EMouseButton_" #name, &res, e_rr                    \
	))

#define AXIS(name, resetOnUnfocus) gotoIfError3(clean, InputDevice_createAxis(                                    \
		result, EMouseAxis_##name - EMouseAxis_Begin, "EMouseAxis_" #name, 0, resetOnUnfocus, &res, e_rr        \
	))

Bool Mouse_create(Mouse *result, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = InputDevice_create(EMouseButton_Count, EMouseAxis_Count, EInputDeviceType_Mouse, result, alloc, e_rr);

	if(!s_uccess)
		return false;

	InputHandle res = 0;

	BUTTON(Left);                BUTTON(Middle);            BUTTON(Right);
	BUTTON(Back);                BUTTON(Forward);

	AXIS(RX, false);            AXIS(RY, false);
	AXIS(ScrollWheel_X, true);    AXIS(ScrollWheel_Y, true);
	AXIS(Temp0, false);            AXIS(Temp1, false);

clean:

	if(!s_uccess)
		InputDevice_free(result, alloc);

	return s_uccess;
}
