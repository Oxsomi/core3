#pragma once
#include "types/vec.h"
#include "formats/texture.h"

//There are two types of windows;
//Physical windows and virtual windows.
//
//A physical window is optionaly supported and how many can be created is limited.
//Android for example would allow only 1 window, while Windows would allow for N windows.
//A server would have 0 windows.
//
//A virtual window is basically just a render target and can always be used.
//It can be created as a fallback if no API is present, but has to be manually written to disk.

//A hint is only used as a *hint* to the impl.
//
enum WindowHint {

	WindowHint_HandleInput				= 1 << 0,
	WindowHint_AllowFullscreen			= 1 << 1,
	WindowHint_DisableResize			= 1 << 2,
	WindowHint_ForceFullscreen			= 1 << 3,
	WindowHint_AllowBackgroundUpdates	= 1 << 4,
	WindowHint_ProvideCPUBuffer			= 1 << 5,	//A hint used to indicate we write from CPU. Useful for physical windows CPU accessible

	WindowHint_None						= 0,
	WindowHint_Default					= WindowHint_HandleInput | WindowHint_AllowFullscreen
};

//Subset of formats that can be used for windows
//
enum WindowFormat {
	WindowFormat_rgba8		= TextureFormat_rgba8,
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
	WindowFlags_IsFullscreen	= 1 << 3
};

typedef void (*WindowCallback)(struct Window*);
typedef void (*WindowUpdateCallback)(struct Window*, F32);

struct WindowCallbacks {
	WindowCallback start, end, draw, resize, move, updateMonitors, updateFocus;
	WindowUpdateCallback update;
};

typedef U16 WindowHandle;

struct Window {

	I32x2 offset;
	I32x2 size;

	struct Buffer cpuVisibleBuffer;

	void *nativeHandle;
	void *nativeData;
	void *lock;

	struct WindowCallbacks callbacks;

	WindowHandle handle;

	Ns lastUpdate;

	enum WindowHint hint;
	enum WindowFormat format;
	enum WindowFlags flags;
};

//Implementation dependent aka physical windows

impl struct Error Window_updatePhysicalTitle(
	const struct Window *w,
	struct String title
);

impl struct Error Window_presentPhysical(
	const struct Window *w, 
	struct Buffer data, 
	enum WindowFormat encodedFormat,
	Bool isTiled4
);

//Virtual windows

struct Error Window_presentVirtual(
	const struct Window *w, 
	struct Buffer data, 
	enum WindowFormat encodedFormat,
	Bool isTiled4
);

struct Error Window_resizeVirtual(
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

inline struct Error Window_present(
	const struct Window *w,
	struct Buffer data,
	enum WindowFormat encodedFormat,
	Bool isTiled4
) {

	if (!w)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if (Window_isVirtual(w))
		return Window_presentVirtual(w, data, encodedFormat, isTiled4);

	return Window_presentPhysical(w, data, encodedFormat, isTiled4);
}

struct Error Window_presentCPUBuffer(		//Presenting CPU buffer to a file (when virtual) or window when physical
	struct Window *w,
	struct String file
) {

	if (!w)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if (Window_isVirtual(w))
		return Window_storeCPUBufferToDisk(w, file);

	return Window_presentCPUBufferPhysical(w);
}
