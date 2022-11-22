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
typedef enum EWindowHint {

	EWindowHint_HandleInput				= 1 << 0,
	EWindowHint_AllowFullscreen			= 1 << 1,
	EWindowHint_DisableResize			= 1 << 2,
	EWindowHint_ForceFullscreen			= 1 << 3,
	EWindowHint_AllowBackgroundUpdates	= 1 << 4,
	EWindowHint_ProvideCPUBuffer		= 1 << 5,	//We write from CPU. Useful for physical windows CPU accessible

	EWindowHint_None					= 0,
	EWindowHint_Default					= EWindowHint_HandleInput | EWindowHint_AllowFullscreen

} EWindowHint;

//Subset of formats that can be used for windows
//These formats are dependent on the platform too. It's very possible they're not available.
//
typedef enum EWindowFormat {
	EWindowFormat_rgba8		= ETextureFormat_rgba8,			//Most common format
	EWindowFormat_hdr10a2	= ETextureFormat_rgb10a2,
	EWindowFormat_rgba16f	= ETextureFormat_rgba16f,
	EWindowFormat_rgba32f	= ETextureFormat_rgba32f
} EWindowFormat;

//Window flags are set by the implementation
//
typedef enum EWindowFlags {
	EWindowFlags_None			= 0,
	EWindowFlags_IsFocussed		= 1 << 0,
	EWindowFlags_IsMinimized	= 1 << 1,
	EWindowFlags_IsVirtual		= 1 << 2,
	EWindowFlags_IsFullscreen	= 1 << 3,
	EWindowFlags_IsActive		= 1 << 4
} EWindowFlags;

#define _RESOLUTION(w, h) (w << 16) | h

//Commonly used resolutions
//
typedef enum EResolution {
	EResolution_Undefined,
	EResolution_SD			= _RESOLUTION(426, 240),
	EResolution_360			= _RESOLUTION(640, 360),
	EResolution_VGA			= _RESOLUTION(640, 480),
	EResolution_480			= _RESOLUTION(854, 480),
	EResolution_WideScreen	= _RESOLUTION(1280, 544),
	EResolution_HD			= _RESOLUTION(1280, 720),
	EResolution_FWideScreen	= _RESOLUTION(1920, 816),
	EResolution_FHD			= _RESOLUTION(1920, 1080),
	EResolution_QHD			= _RESOLUTION(2560, 1440),
	EResolution_UHD			= _RESOLUTION(3840, 2160),
	EResolution_8K			= _RESOLUTION(7680, 4320),
	EResolution_16K			= _RESOLUTION(15360, 8640)
} EResolution;

inline I32x2 EResolution_get(EResolution r) { return I32x2_create2(r >> 16, r & U16_MAX); }

inline EResolution EResolution_create(I32x2 v) { 

	if(I32x2_neq2(I32x2_clamp(v, I32x2_zero(), I32x2_xx2(U16_MAX)), v)) 
		return EResolution_Undefined;

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

	EWindowHint hint;
	EWindowFormat format;
	EWindowFlags flags;

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

Error Window_resizeCPUBuffer(		//Should be called if virtual or EWindowHint_ProvideCPUBuffer
	Window *w, 
	Bool copyData, 
	I32x2 newSize
);

Error Window_storeCPUBufferToDisk(const Window *w, String filePath, Ns maxTimeout);

//Simple helper functions

inline Bool Window_isVirtual(const Window *w) { return w && w->flags & EWindowFlags_IsVirtual; }
inline Bool Window_isMinimized(const Window *w) { return w && w->flags & EWindowFlags_IsMinimized; }
inline Bool Window_isFocussed(const Window *w) { return w && w->flags & EWindowFlags_IsFocussed; }
inline Bool Window_isFullScreen(const Window *w) { return w && w->flags & EWindowFlags_IsFullscreen; }

inline Bool Window_doesHandleInput(const Window *w) { return w && w->hint & EWindowHint_HandleInput; }
inline Bool Window_doesAllowFullScreen(const Window *w) { return w && w->hint & EWindowHint_AllowFullscreen; }

//Presenting CPU buffer to a file (when virtual) or window when physical
//This can only be called in a draw function!

inline Error Window_presentCPUBuffer(
	Window *w,
	String file,
	Ns maxTimeout
) {

	if (!w)
		return Error_nullPointer(0, 0);

	if (!w->isDrawing)
		return Error_invalidOperation(0);

	if (Window_isVirtual(w))
		return Window_storeCPUBufferToDisk(w, file, maxTimeout);

	return Window_presentPhysical(w);
}

Error Window_waitForExit(Window *w, Ns maxTimeout);
Bool Window_terminateVirtual(Window *w);
