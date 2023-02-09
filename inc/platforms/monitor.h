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
#include "math/vec.h"

//Monitor

typedef enum EMonitorOrientation {
	EMonitorOrientation_Landscape			= 0,
	EMonitorOrientation_Portrait			= 90,
	EMonitorOrientation_FlippedLandscape	= 180,
	EMonitorOrientation_FlippedPortrait		= 270
} MonitorOrientation;

//A monitor is something a physical window is displayed on.
//The window can know this to handle monitor specific processing,
//such as subpixel rendering.

typedef struct Monitor {

	I32x2 offsetPixels, sizePixels;
	I32x2 offsetR, offsetG;
	I32x2 offsetB, sizeInches;

	MonitorOrientation orientation;
	F32 gamma, contrast, refreshRate;

} Monitor;
