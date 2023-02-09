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

#include "platforms/mouse.h"
#include "platforms/input_device.h"
#include "types/error.h"
#include "types/string.h"

#define _button(name)																						\
	if ((err = InputDevice_createButton(																	\
		*result, EMouseButton_##name, String_createConstRefUnsafe("EMouseButton_" #name), &res				\
	)).genericError) {																						\
		InputDevice_free(result);																			\
		return err;																							\
	}

#define _axis(name)																							\
	if ((err = InputDevice_createAxis(																		\
		*result, EMouseAxis_##name, String_createConstRefUnsafe("EMouseAxis_" #name), 0, &res				\
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
