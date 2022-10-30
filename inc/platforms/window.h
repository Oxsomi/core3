#pragma once
#include "math/vec.h"
#include "types/list.h"
#include "formats/texture.h"
#include "lock.h"
#include "input_device.h"

//There are two types of windows;
//Physical windows and virtual windows.
//
//A physical window is optional to support by the runtime and how many can be created is limited.
//Android for example would allow only 1 window, while Windows would allow for N windows.
//A server would have 0 windows.
//
//A virtual window is basically just a render target and can always be used.
//It can be created as a fallback if no API is present, but has to be manually written to disk (Window_presentCPUBuffer).

//A hint is only used as a *hint* to the impl.
//The runtime is allowed to ignore this if it's not applicable.
//
typedef enum WindowHint {

	WindowHint_HandleInput				= 1 << 0,
	WindowHint_AllowFullscreen			= 1 << 1,
	WindowHint_DisableResize			= 1 << 2,
	WindowHint_ForceFullscreen			= 1 << 3,
	WindowHint_AllowBackgroundUpdates	= 1 << 4,
	WindowHint_ProvideCPUBuffer			= 1 << 5,	//We write from CPU. Useful for physical windows CPU accessible

	WindowHint_None						= 0,
	WindowHint_Default					= WindowHint_HandleInput | WindowHint_AllowFullscreen

} WindowHint;

//Subset of formats that can be used for windows
//These formats are dependent on the platform too. It's very possible they're not available.
//
typedef enum WindowFormat {
	WindowFormat_rgba8		= TextureFormat_rgba8,			//Most common format
	WindowFormat_hdr10a2	= TextureFormat_rgb10a2,
	WindowFormat_rgba16f	= TextureFormat_rgba16f,
	WindowFormat_rgba32f	= TextureFormat_rgba32f
} WindowFormat;

//Window flags are set by the implementation
//
typedef enum WindowFlags {
	WindowFlags_None			= 0,
	WindowFlags_IsFocussed		= 1 << 0,
	WindowFlags_IsMinimized		= 1 << 1,
	WindowFlags_IsVirtual		= 1 << 2,
	WindowFlags_IsFullscreen	= 1 << 3,
	WindowFlags_IsActive		= 1 << 4
} WindowFlags;

#define _RESOLUTION(w, h) (w << 16) | h

//Commonly used resolutions
//
typedef enum Resolution {
	Resolution_Undefined,
	Resolution_SD			= _RESOLUTION(426, 240),
	Resolution_360			= _RESOLUTION(640, 360),
	Resolution_VGA			= _RESOLUTION(640, 480),
	Resolution_480			= _RESOLUTION(854, 480),
	Resolution_WideScreen	= _RESOLUTION(1280, 544),
	Resolution_HD			= _RESOLUTION(1280, 720),
	Resolution_FWideScreen	= _RESOLUTION(1920, 816),
	Resolution_FHD			= _RESOLUTION(1920, 1080),
	Resolution_QHD			= _RESOLUTION(2560, 1440),
	Resolution_UHD			= _RESOLUTION(3840, 2160),
	Resolution_8K			= _RESOLUTION(7680, 4320),
	Resolution_16K			= _RESOLUTION(15360, 8640)
} Resolution;

inline I32x2 Resolution_get(Resolution r) { return I32x2_create2(r >> 16, r & U16_MAX); }

inline Resolution Resolution_create(I32x2 v) { 

	if(I32x2_neq2(I32x2_clamp(v, I32x2_zero(), I32x2_xx2(U16_MAX)), v)) 
		return Resolution_Undefined;

	return _RESOLUTION(I32x2_x(v), I32x2_y(v));
}

//Window callbacks

typedef struct Window Window;

typedef void (*WindowCallback)(Window*);
typedef void (*WindowUpdateCallback)(Window*, F32);
typedef void (*WindowDeviceCallback)(Window*, InputDevice*);
typedef void (*WindowDeviceButtonCallback)(Window*, InputDevice*, InputHandle, Bool);
typedef void (*WindowDeviceAxisCallback)(Window*, InputDevice*, InputHandle, F32);

typedef struct WindowCallbacks {
	WindowCallback onCreate, onDestroy, onDraw, onResize, onWindowMove, onMonitorChange, onUpdateFocus;
	WindowUpdateCallback onUpdate;
	WindowDeviceCallback onDeviceAdd, onDeviceRemove;
	WindowDeviceButtonCallback onDeviceButton;
	WindowDeviceAxisCallback onDeviceAxis;
} WindowCallbacks;

//Hard limits; after this, the runtime won't support new devices/monitors

impl extern const U16 Window_maxDevices;
impl extern const U16 Window_maxMonitors;

//Window itself

typedef U16 WindowHandle;

typedef struct Window {

	I32x2 offset;
	I32x2 size;

	Buffer cpuVisibleBuffer;

	void *nativeHandle;
	void *nativeData;
	Lock lock;

	WindowCallbacks callbacks;

	Ns lastUpdate;
	Bool isDrawing;

	WindowHint hint;
	WindowFormat format;
	WindowFlags flags;

	List devices;				//TODO: Make this a map at some point
	List monitors;

} Window;

//Implementation dependent aka physical windows

impl Error Window_updatePhysicalTitle(
	const Window *w,
	String title
);

impl Error Window_presentPhysical(
	const Window *w
);

//Virtual windows

Error Window_resizeCPUBuffer(		//Should be called if virtual or WindowHint_ProvideCPUBuffer
	Window *w, 
	Bool copyData, 
	I32x2 newSize
);

Error Window_storeCPUBufferToDisk(const Window *w, String filePath);

//Simple helper functions

inline Bool Window_isVirtual(const Window *w) { return w && w->flags & WindowFlags_IsVirtual; }
inline Bool Window_isMinimized(const Window *w) { return w && w->flags & WindowFlags_IsMinimized; }
inline Bool Window_isFocussed(const Window *w) { return w && w->flags & WindowFlags_IsFocussed; }
inline Bool Window_isFullScreen(const Window *w) { return w && w->flags & WindowFlags_IsFullscreen; }

inline Bool Window_doesHandleInput(const Window *w) { return w && w->hint & WindowHint_HandleInput; }
inline Bool Window_doesAllowFullScreen(const Window *w) { return w && w->hint & WindowHint_AllowFullscreen; }

//Presenting CPU buffer to a file (when virtual) or window when physical
//This can only be called in a draw function!

inline Error Window_presentCPUBuffer(
	Window *w,
	String file
) {

	if (!w)
		return (Error) { .genericError = GenericError_NullPointer };

	if (!w->isDrawing)
		return (Error) { .genericError = GenericError_InvalidOperation };

	if (Window_isVirtual(w))
		return Window_storeCPUBufferToDisk(w, file);

	return Window_presentPhysical(w);
}

Error Window_waitForExit(Window *w, Ns maxTimeout);
Bool Window_terminateVirtual(Window *w);
