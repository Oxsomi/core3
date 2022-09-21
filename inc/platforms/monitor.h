#pragma once
#include "math/vec.h"
#include "types/error.h"

//Monitor

enum MonitorOrientation {
	MonitorOrientation_Landscape			= 0,
	MonitorOrientation_Portrait				= 90,
	MonitorOrientation_FlippedLandscape		= 180,
	MonitorOrientation_FlippedPortrait		= 270
};

//A monitor is something a physical window is displayed on.
//The window can know this to handle monitor specific processing,
//such as subpixel rendering.

struct Monitor {

	I32x2 offsetPixels, sizePixels;
	I32x2 offsetR, offsetG;
	I32x2 offsetB, sizeInches;

	enum MonitorOrientation orientation;
	F32 gamma, contrast, refreshRate;
};

//Helpers

inline struct Error Monitor_setRect(struct Monitor *m, I32x2 offset, I32x2 size) { 

	if (!m)
		return (struct Error) { .genericError = GenericError_NullPointer };

	m->offsetPixels = offset;
	m->sizePixels = size;
	return Error_none();
}

//Subpixel rendering
//Ex.	RGB is 0,1,2 and BGR is 2,1,0. 
//		Other formats such as pentile might not map properly to this.
//		For those formats all offsets should be set to zero 
//		and the renderer should implement a fallback.

inline struct Error Monitor_setSubpixels(struct Monitor *m, F32x4 offsetR, F32x4 offsetG, F32x4 offsetB) {

	if (!m)
		return (struct Error) { .genericError = GenericError_NullPointer };

	m->offsetR = offsetR;
	m->offsetG = offsetG;
	m->offsetB = offsetB;
	return Error_none();
}