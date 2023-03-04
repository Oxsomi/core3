/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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

#pragma once
#include "types/types.h"

typedef struct Error Error;
typedef struct InputDevice Mouse;

typedef enum EMouseActions {

	//Mouse axes

	EMouseAxis_Begin,

	EMouseAxis_X = EMouseAxis_Begin, EMouseAxis_Y, 
	EMouseAxis_ScrollWheel_X, EMouseAxis_ScrollWheel_Y,

	EMouseAxis_End,
	EMouseAxis_Count = EMouseAxis_End - EMouseAxis_Begin,

	//Mouse buttons

	EMouseButton_Begin = EMouseAxis_End,

	EMouseButton_Left = EMouseButton_Begin, EMouseButton_Middle, EMouseButton_Right, 
	EMouseButton_Back, EMouseButton_Forward,

	EMouseButton_End,
	EMouseButton_Count = EMouseButton_End - EMouseButton_Begin,

} MouseActions;

typedef enum EMouseFlag {

	//When this happens, the mouse position is relative, not absolute
	//So, the cursor position shouldn't be taken as absolute
	//This means using the delta functions instead of the absolute functions

	EMouseFlag_IsRelative

} EMouseFlag;

Error Mouse_create(Mouse *result);
