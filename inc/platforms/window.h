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
enum WindowHint {

	WindowHint_HandleInput				= 1 << 0,
	WindowHint_AllowFullscreen			= 1 << 1,
	WindowHint_DisableResize			= 1 << 2,
	WindowHint_ForceFullscreen			= 1 << 3,
	WindowHint_AllowBackgroundUpdates	= 1 << 4,
	WindowHint_ProvideCPUBuffer			= 1 << 5,	//We write from CPU. Useful for physical windows CPU accessible

	WindowHint_None						= 0,
	WindowHint_Default					= WindowHint_HandleInput | WindowHint_AllowFullscreen
};

//Subset of formats that can be used for windows
//These formats are dependent on the platform too. It's very possible they're not available.
//
enum WindowFormat {
	WindowFormat_rgba8		= TextureFormat_rgba8,			//Most common format
	WindowFormat_hdr10a2	= TextureFormat_rgb10a2,
	WindowFormat_rgba16f	= TextureFormat_rgba16f,
	WindowFormat_rgba32f	= TextureFormat_rgba32f
};

//Window flags are set by the implementation
//
enum WindowFlags {
	WindowFlags_None			= 0,
	WindowFlags_IsFocussed		= 1 << 0,
	WindowFlags_IsMinimized		= 1 << 1,
	WindowFlags_IsVirtual		= 1 << 2,
	WindowFlags_IsFullscreen	= 1 << 3,
	WindowFlags_IsActive		= 1 << 4
};

#define _RESOLUTION(w, h) (w << 16) | h

//Commonly used resolutions
//
enum Resolution {
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
};

inline I32x2 Resolution_get(enum Resolution r) { return I32x2_create2(r >> 16, r & U16_MAX); }

inline enum Resolution Resolution_create(I32x2 v) { 

	if(I32x2_neq2(I32x2_clamp(v, I32x2_zero(), I32x2_xx2(U16_MAX)), v)) 
		return Resolution_Undefined;

	return _RESOLUTION(I32x2_x(v), I32x2_y(v));
}

//Window callbacks

struct Window;

typedef void (*WindowCallback)(struct Window*);
typedef void (*WindowUpdateCallback)(struct Window*, F32);
typedef void (*WindowDeviceCallback)(struct Window*, struct InputDevice*);
typedef void (*WindowDeviceButtonCallback)(struct Window*, struct InputDevice*, InputHandle, Bool);
typedef void (*WindowDeviceAxisCallback)(struct Window*, struct InputDevice*, InputHandle, F32);

struct WindowCallbacks {
	WindowCallback onCreate, onDestroy, onDraw, onResize, onWindowMove, onMonitorChange, onUpdateFocus;
	WindowUpdateCallback onUpdate;
	WindowDeviceCallback onDeviceAdd, onDeviceRemove;
	WindowDeviceButtonCallback onDeviceButton;
	WindowDeviceAxisCallback onDeviceAxis;
};

//Hard limits; after this, the runtime won't support new devices/monitors

impl extern const U16 Window_maxDevices;
impl extern const U16 Window_maxMonitors;

//Window itself

typedef U16 WindowHandle;

struct Window {

	I32x2 offset;
	I32x2 size;

	struct Buffer cpuVisibleBuffer;

	void *nativeHandle;
	void *nativeData;
	struct Lock lock;

	struct WindowCallbacks callbacks;

	Ns lastUpdate;
	Bool isDrawing;

	enum WindowHint hint;
	enum WindowFormat format;
	enum WindowFlags flags;

	struct List devices;				//TODO: Make this a map at some point
	struct List monitors;
};

//Implementation dependent aka physical windows

impl struct Error Window_updatePhysicalTitle(
	const struct Window *w,
	struct String title
);

impl struct Error Window_presentPhysical(
	const struct Window *w
);

//Virtual windows

struct Error Window_resizeCPUBuffer(		//Should be called if virtual or WindowHint_ProvideCPUBuffer
	struct Window *w, 
	Bool copyData, 
	I32x2 newSize
);

struct Error Window_storeCPUBufferToDisk(const struct Window *w, struct String filePath);

//Simple helper functions

inline Bool Window_isVirtual(const struct Window *w) { return w && w->flags & WindowFlags_IsVirtual; }
inline Bool Window_isMinimized(const struct Window *w) { return w && w->flags & WindowFlags_IsMinimized; }
inline Bool Window_isFocussed(const struct Window *w) { return w && w->flags & WindowFlags_IsFocussed; }
inline Bool Window_isFullScreen(const struct Window *w) { return w && w->flags & WindowFlags_IsFullscreen; }

inline Bool Window_doesHandleInput(const struct Window *w) { return w && w->hint & WindowHint_HandleInput; }
inline Bool Window_doesAllowFullScreen(const struct Window *w) { return w && w->hint & WindowHint_AllowFullscreen; }

//Presenting CPU buffer to a file (when virtual) or window when physical
//This can only be called in a draw function!

inline struct Error Window_presentCPUBuffer(
	struct Window *w,
	struct String file
) {

	if (!w)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if (!w->isDrawing)
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	if (Window_isVirtual(w))
		return Window_storeCPUBufferToDisk(w, file);

	return Window_presentPhysical(w);
}

struct Error Window_waitForExit(struct Window *w, Ns maxTimeout);
Bool Window_terminateVirtual(struct Window *w);
