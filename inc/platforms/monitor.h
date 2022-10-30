#pragma once
#include "math/vec.h"
#include "types/error.h"

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
