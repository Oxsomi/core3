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
