/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/container/list.h"
#include "types/base/error.h"
#include "types/container/texture_format.h"
#include "platforms/input_device.h"
#include "platforms/monitor.h"

#ifdef __cplusplus
	extern "C" {
#endif

//There are three types of windows;
//Physical windows, virtual windows and extended.
//
//A physical window is optional to support by the runtime and how many can be created is limited.
//Android for example would allow only 1 window, while Windows would allow for N windows.
//A server would have 0 windows.
//
//A virtual window is basically just a render target and can always be used.
//It can be created as a fallback if no API is present, but has to be manually written to.
//
//An extended window is a special type of native window that is only applicable to a different API.
//An example here is an OpenXR window.

//A hint is only used as a *hint* to the impl.
//The runtime is allowed to ignore this if it's not applicable.
typedef enum EWindowHint {

	EWindowHint_AllowFullscreen			= 1 << 0,
	EWindowHint_DisableResize			= 1 << 1,
	EWindowHint_ForceFullscreen			= 1 << 2,
	EWindowHint_AllowBackgroundUpdates	= 1 << 3,
	EWindowHint_ProvideCPUBuffer		= 1 << 4,	//We write from CPU. Useful for physical windows CPU accessible

	EWindowHint_None					= 0,
	EWindowHint_Default					= EWindowHint_AllowFullscreen

} EWindowHint;

//Subset of formats that can be used for windows
//These formats are dependent on the platform too. It's very possible they're not available.
typedef enum EWindowFormat {
	EWindowFormat_AutoRGBA8				= 0,							//BGRA8 or RGBA8, depending on platform
	EWindowFormat_BGRA8					= ETextureFormat_BGRA8,			//Most common format
	EWindowFormat_RGBA8					= ETextureFormat_RGBA8,			//Also common on non desktop
	EWindowFormat_BGR10A2				= ETextureFormat_BGR10A2,
	EWindowFormat_RGBA16f				= ETextureFormat_RGBA16f,
	EWindowFormat_RGBA32f				= ETextureFormat_RGBA32f		//Rarely supported (only CPU)
} EWindowFormat;

//Window flags are set by the implementation
typedef enum EWindowFlags {
	EWindowFlags_None					= 0,
	EWindowFlags_IsFocussed				= 1 << 0,
	EWindowFlags_IsMinimized			= 1 << 1,
	EWindowFlags_IsFullscreen			= 1 << 2,
	EWindowFlags_IsActive				= 1 << 3,
	EWindowFlags_ShouldTerminate		= 1 << 4,
	EWindowFlags_IsMoving				= 1 << 5,		//Move or resize
	EWindowFlags_IsFinalized			= 1 << 6		//Ready to receive events
} EWindowFlags;

#define _RESOLUTION(w, h) (w << 16) | h

//Commonly used resolutions
typedef enum EResolution {
	EResolution_Undefined,
	EResolution_SD						= _RESOLUTION(426, 240),
	EResolution_360						= _RESOLUTION(640, 360),
	EResolution_VGA						= _RESOLUTION(640, 480),
	EResolution_480						= _RESOLUTION(854, 480),
	EResolution_WideScreen				= _RESOLUTION(1280, 544),
	EResolution_HD						= _RESOLUTION(1280, 720),
	EResolution_FWideScreen				= _RESOLUTION(1920, 816),
	EResolution_FHD						= _RESOLUTION(1920, 1080),
	EResolution_QHD						= _RESOLUTION(2560, 1440),
	EResolution_UHD						= _RESOLUTION(3840, 2160),
	EResolution_8K						= _RESOLUTION(7680, 4320),
	EResolution_16K						= _RESOLUTION(15360, 8640)
} EResolution;

I32x2 EResolution_get(EResolution r);
EResolution EResolution_create(I32x2 v);

//Window callbacks

typedef struct Window Window;

typedef void (*WindowCallback)(Window*);
typedef void (*WindowUpdateCallback)(Window*, F64);
typedef void (*WindowDeviceCallback)(Window*, InputDevice*);
typedef void (*WindowDeviceButtonCallback)(Window*, InputDevice*, InputHandle, Bool);
typedef void (*WindowDeviceAxisCallback)(Window*, InputDevice*, InputHandle, F32);
typedef void (*WindowTypeCallback)(Window*, CharString);

//On apps this is called by the OS when memory is needed and you're expected to quit everything to continue
//Another useful thing could be for example switching graphics API on windows during development and reloading the same thing

typedef void (*WindowLoadCallback)(Window*, Buffer buf);
typedef void (*WindowSaveCallback)(Window*, Buffer *buf);

typedef struct WindowCallbacks {
	WindowCallback onCreate, onDestroy, onDraw, onResize, onWindowMove, onMonitorChange, onUpdateFocus, onUpdateOrientation;
	WindowCallback onCursorMove;
	WindowUpdateCallback onUpdate;
	WindowDeviceCallback onDeviceAdd, onDeviceRemove;
	WindowDeviceButtonCallback onDeviceButton;
	WindowDeviceAxisCallback onDeviceAxis;
	WindowTypeCallback onTypeChar;
	WindowSaveCallback onSave;
	WindowLoadCallback onLoad;
} WindowCallbacks;

//Window itself

typedef enum EWindowType {

	EWindowType_Physical,			//Native window of the underlying platform
	EWindowType_Virtual,			//Non-native window, such as headless rendering
	//EWindowType_ExtendedOpenXR,	//Extended physical window; for use with OpenXR
	//EWindowType_External,			//Externally managed window, manually calling the necessary functions

	EWindowType_Count,
	EWindowType_Extended = EWindowType_Virtual + 1

} EWindowType;

typedef U8 WindowType;
typedef U16 WindowHint;

typedef struct WindowManager WindowManager;

TList(InputDevice);
TList(Monitor);

typedef enum EWindowOrientation {
	EWindowOrientation_Landscape			= 0,
	EWindowOrientation_Portrait				= 90,
	EWindowOrientation_FlippedLandscape		= 180,
	EWindowOrientation_FlippedPortrait		= 270
} EWindowOrientation;

typedef U16 WindowOrientation;

typedef struct Window {

	WindowManager *owner;

	WindowHint hint;
	WindowType type;
	Bool requireResize;			//Can be set by for example the graphics API to indicate a resize should be performed

	WindowOrientation orientation;
	U16 padding1;

	EWindowFormat format;
	EWindowFlags flags;

	I32x2 offset, size;
	I32x2 minSize, maxSize;

	I32x2 prevSize;				//For full screen toggle
	I32x2 cursor;

	Buffer cpuVisibleBuffer;

	void *nativeHandle, *nativeData;

	WindowCallbacks callbacks;

	Ns lastUpdate;

	CharString title;

	//TODO: Make this a map at some point

	ListInputDevice devices;
	ListMonitor monitors;

	//Data initialized by onCreate such as extended window data

	Buffer extendedData;

	U64 padding2;

	//Temporary data such as input buffer

	U8 buffer[96];

} Window;

//Implementation dependent aka physical windows

impl Bool Window_updatePhysicalTitle(const Window *w, CharString title, Error *e_rr);
impl Bool Window_toggleFullScreen(Window *w, Error *e_rr);
impl Bool Window_presentPhysical(Window *w, Error *e_rr);

//Virtual windows

//Should be called if virtual or EWindowHint_ProvideCPUBuffer

Bool Window_resizeCPUBuffer(Window *w, Bool copyData, I32x2 newSize, Error *e_rr);
Bool Window_storeCPUBufferToDisk(const Window *w, CharString filePath, Ns maxTimeout, Error *e_rr);

//Simple helper functions

Bool Window_isMinimized(const Window *w);
Bool Window_isFocussed(const Window *w);
Bool Window_isFullScreen(const Window *w);

Bool Window_doesAllowFullScreen(const Window *w);

Bool Window_terminate(Window *w);

#ifdef __cplusplus
	}
#endif
