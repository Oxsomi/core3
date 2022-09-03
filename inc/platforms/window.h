#pragma once
#include "math/vec.h"
#include "types/allocator.h"
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
	WindowHint_AllowFullScreen			= 1 << 1,
	WindowHint_DisableResize			= 1 << 2,
	WindowHint_ForceFullScreen			= 1 << 3,
	WindowHint_AllowBackgroundUpdates	= 1 << 4,

	WindowHint_None					= 0,
	WindowHint_Default				= WindowHint_HandleInput | WindowHint_AllowFullScreen
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
	WindowFlags_IsFullScreen	= 1 << 3
};

typedef void (*WindowCallback)(struct Window*);
typedef void (*WindowUpdateCallback)(struct Window*, f64);

struct WindowCallbacks {
	WindowCallback start, end, draw, resize, move, updateMonitors, updateFocus;
	WindowUpdateCallback update;
};

struct Window {

	f32x4 offset;
	f32x4 size;

	usz monitorCount;				//Monitors that the window overlaps with
	struct Monitor *monitors;

	void *nativeHandle;
	void *nativeData;

	struct WindowCallbacks callbacks;

	ns lastUpdate;

	enum WindowHint hint;
	enum WindowFormat format;
	enum WindowFlags flags;
};

//Implementation dependent aka physical windows

impl struct Window *Window_createPhysical(
	f32x4 position, f32x4 size, 
	enum WindowHint hint, const c8 *title, struct WindowCallbacks callbacks,
	enum WindowFormat format
);

impl void Window_freePhysical(struct Window **w);

impl void Window_updatePhysicalTitle(const struct Window *w, const c8 *title);

impl void Window_presentPhysical(const struct Window *w, struct Buffer data, enum WindowFormat encodedFormat);

impl void Window_waitForExitPhysical(const struct Window *w);

//Virtual windows

struct Window *Window_createVirtual(
	f32x4 size, struct WindowCallbacks callbacks, enum WindowFormat format
);

void Window_freeVirtual(struct Window **w);
void Window_presentVirtual(const struct Window *w, struct Buffer data, enum WindowFormat encodedFormat);

void Window_resizeVirtual(struct Window *w, bool copyData);

struct Buffer Window_getVirtualData(const struct Window *w);

//Simple helper functions

inline bool Window_isVirtual(const struct Window *w) { return w && w->flags & WindowFlags_IsVirtual; }
inline bool Window_isMinimized(const struct Window *w) { return w && w->flags & WindowFlags_IsMinimized; }
inline bool Window_isFocussed(const struct Window *w) { return w && w->flags & WindowFlags_IsMinimized; }
inline bool Window_isFullScreen(const struct Window *w) { return w && w->flags & WindowFlags_IsFullScreen; }

inline bool Window_doesHandleInput(const struct Window *w) { return w && w->hint & WindowHint_HandleInput; }
inline bool Window_doesAllowFullScreen(const struct Window *w) { return w && w->hint & WindowHint_AllowFullScreen; }

inline struct Window *Window_create(
	f32x4 position, f32x4 size, enum WindowHint hint, const c8 *title, 
	struct WindowCallbacks callbacks, enum WindowFormat format
) {

	struct Window *w = Window_createPhysical(
		position, size, hint, title, callbacks, format
	);

	if (w)
		return w;

	return Window_createVirtual(size, callbacks, format);
}

inline void Window_free(struct Window **w) {

	if (!w)
		return;

	if (Window_isVirtual(*w))
		Window_freeVirtual(w);

	else Window_freePhysical(w);
}

inline void Window_present(const struct Window *w, struct Buffer data, enum WindowFormat encodedFormat) {

	if (!w)
		return;

	if (Window_isVirtual(w))
		Window_presentVirtual(w, data, encodedFormat);

	else Window_presentPhysical(w, data, encodedFormat);
}

//Stall main thread until the window is closed manually

inline void Window_waitForExit(const struct Window *w) {
	if(!Window_isVirtual(w))
		Window_waitForExitPhysical(w);
}

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

	f32x4 offset_size;
	f32x4 offsetR_G;
	f32x4 offsetB_sizeIn;

	enum MonitorOrientation orientation;
	f32 gamma, contrast, refreshRate;
};

//Helpers

inline f32x4 Monitor_getOffset(const struct Monitor *m) { return m ? Vec_xy(m->offset_size) : Vec_zero(); }
inline f32x4 Monitor_getSizePixels(const struct Monitor *m) { return m ? Vec_zw(m->offset_size) : Vec_zero(); }
inline f32x4 Monitor_getOffsetR(const struct Monitor *m) { return m ? Vec_xy(m->offsetR_G) : Vec_zero(); }
inline f32x4 Monitor_getOffsetG(const struct Monitor *m) { return m ? Vec_zw(m->offsetR_G) : Vec_zero(); }
inline f32x4 Monitor_getOffsetB(const struct Monitor *m) { return m ? Vec_xy(m->offsetB_sizeIn) : Vec_zero(); }
inline f32x4 Monitor_getSizeInches(const struct Monitor *m) { return m ? Vec_zw(m->offsetB_sizeIn) : Vec_zero(); }

inline void Monitor_setRect(struct Monitor *m, f32x4 offset, f32x4 size) { 
	if(m) m->offset_size = Vec_combine2(offset, size); 
}

inline void Monitor_setSizeIn(struct Monitor *m, f32x4 sizeIn) { 
	if(m) 
		m->offsetB_sizeIn = Vec_combine2(Vec_zw(m->offsetB_sizeIn), sizeIn); 
}

//Subpixel rendering
//Ex.	RGB is 0,1,2 and BGR is 2,1,0. 
//		Other formats such as pentile might not map properly to this.
//		For those formats all offsets should be set to zero 
//		and the renderer should implement a fallback.

inline void Monitor_setSubpixels(struct Monitor *m, f32x4 offsetR, f32x4 offsetG, f32x4 offsetB) { 

	if (!m)
		return; 
	
	m->offsetR_G = Vec_combine2(offsetR, offsetG);
	m->offsetB_sizeIn = Vec_combine2(offsetB, Vec_zw(m->offsetB_sizeIn));
}