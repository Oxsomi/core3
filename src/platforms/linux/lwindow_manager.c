/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

//platforms/linux/lwindow_manager.c

#include "platforms/window_manager.h"
#include "platforms/window.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "platforms/linux/lwindow_structs.h"
#include "types/container/buffer.h"
#include "types/base/string_read_helper.h"
#include "types/base/error.h"
#include "types/base/time.h"

#include <stdlib.h>
#include <wayland-cursor.h>

void LWindowManager_isAlive(void *data, struct xdg_wm_base *base, U32 serial) {
	(void) data;
	xdg_wm_base_pong(base, serial);
}
 
//wl_output geometry event, fires once per output on connect, and on change.
//Gives us the output's position in global compositor space, physical size in mm,
// and transform (orientation). The pixel mode dimensions come from the mode event
static void LOutput_geometry(
	void *data,
	struct wl_output *output,
	I32 x, I32 y,
	I32 mmWidth, I32 mmHeight,
	I32 subpixel,
	const C8 *make, const C8 *model,
	I32 transform
) {
	(void) output; (void) make; (void) model;
	LOutputInfo *info = (LOutputInfo*) data;
	info->x         = x;
	info->y         = y;
	info->mmWidth   = mmWidth;
	info->mmHeight  = mmHeight;
	info->transform = transform;
	info->subpixel  = subpixel;
}
 
//wl_output mode event, fires for each supported mode; the one with WL_OUTPUT_MODE_CURRENT
// set is the active resolution and refresh rate
static void LOutput_mode(
	void *data,
	struct wl_output *output,
	U32 flags,
	I32 width, I32 height,
	I32 refresh
) {
	(void) output;
 
	if(!(flags & WL_OUTPUT_MODE_CURRENT))
		return;
 
	LOutputInfo *info = (LOutputInfo*) data;
	info->pixelWidth  = (U16)width;
	info->pixelHeight = (U16)height;
	info->refreshRate = refresh;   // mHz
}
 
//done event, compositor signals it has finished sending all properties for this output.
// Nothing to do here; we've already updated info in-place
static void LOutput_done(void *data, struct wl_output *output) {
	(void) data; (void) output;
}
 
//scale event, integer HiDPI scale factor (e.g. 2 for 200% scaling).
// Stored for future use; not wired into Monitor yet.
static void LOutput_scale(void *data, struct wl_output *output, I32 factor) {
	(void) output;
	LOutputInfo *info = (LOutputInfo*) data;
	info->scale = factor;
}
 
static const struct wl_output_listener LOutput_listener = {
	.geometry = LOutput_geometry,
	.mode     = LOutput_mode,
	.done     = LOutput_done,
	.scale    = LOutput_scale,
};
 
void LWindowManager_register(
	void *dataVoid,
	struct wl_registry *registry,
	U32 id,
	const C8 *interface,
	U32 version
) {
	(void) version;
	CharString inter = CharString_createRefCStrConst(interface);

	LWindowManager *data = (LWindowManager*) dataVoid;

	const CharString wlCompositorName    = CharString_createRefCStrConst(wl_compositor_interface.name);
	const CharString wlShmName           = CharString_createRefCStrConst(wl_shm_interface.name);
	const CharString xdgWmBaseName       = CharString_createRefCStrConst(xdg_wm_base_interface.name);
	const CharString zxdgDecorationName  = CharString_createRefCStrConst(zxdg_decoration_manager_v1_interface.name);
	const CharString wlSeatName          = CharString_createRefCStrConst(wl_seat_interface.name);
	const CharString wlOutputName        = CharString_createRefCStrConst(wl_output_interface.name);
	const CharString wlSubcompositorName = CharString_createRefCStrConst(wl_subcompositor_interface.name);

	if(CharString_equalsStringSensitive(&wlCompositorName, &inter)) {
		data->compositor   = wl_registry_bind(registry, id, &wl_compositor_interface, 4);
		data->compositorId = id;
	}

	else if(CharString_equalsStringSensitive(&wlShmName, &inter)) {
		data->shm   = wl_registry_bind(registry, id, &wl_shm_interface, 1);
		data->shmId = id;
	}

	else if(CharString_equalsStringSensitive(&xdgWmBaseName, &inter)) {
		data->xdgWmBase   = wl_registry_bind(registry, id, &xdg_wm_base_interface, 1);
		data->xdgWmBaseId = id;

		data->xdgListener = (struct xdg_wm_base_listener) { .ping = LWindowManager_isAlive };
		xdg_wm_base_add_listener(data->xdgWmBase, &data->xdgListener, NULL);
	}

	else if(CharString_equalsStringSensitive(&zxdgDecorationName, &inter)) {
		data->xdgDeco   = wl_registry_bind(registry, id, &zxdg_decoration_manager_v1_interface, 1);
		data->xdgDecoId = id;
	}

	else if(CharString_equalsStringSensitive(&wlSubcompositorName, &inter)) {
		data->subcompositor   = wl_registry_bind(registry, id, &wl_subcompositor_interface, 1);
		data->subcompositorId = id;
	}

	else if(CharString_equalsStringSensitive(&wlSeatName, &inter)) {
		data->seat   = wl_registry_bind(registry, id, &wl_seat_interface, 5);
		data->seatId = id;
	}

	else if(CharString_equalsStringSensitive(&wlOutputName, &inter)) {
		for(U32 i = 0; i < LWINDOW_MAX_OUTPUTS; ++i) {
			if(data->outputs[i])
				continue;
 
			data->outputs[i]   = wl_registry_bind(registry, id, &wl_output_interface, 2);
			data->outputIds[i] = id;
 
			//Clear the info slot and wire the listener so geometry/mode/done
			// events populate it before the first window is created.
			data->outputInfo[i] = (LOutputInfo) { .scale = 1 };
			wl_output_add_listener(data->outputs[i], &LOutput_listener, &data->outputInfo[i]);
			break;
		}
	}

}

void LWindowManager_unregister(void *dataVoid, struct wl_registry *registry, U32 id) {

	LWindowManager *data = (LWindowManager*) dataVoid;
	(void) registry;

	if(id == data->compositorId)
		data->compositor = NULL;

	else if(id == data->shmId)
		data->shm = NULL;

	else if(id == data->xdgWmBaseId)
		data->xdgWmBase = NULL;

	else if(id == data->xdgDecoId)
		data->xdgDeco = NULL;

	else if(id == data->subcompositorId)
		data->subcompositor = NULL;

	else if(id == data->seatId) {
		data->seat   = NULL;
		data->seatId = 0;
	}

	else for(U32 i = 0; i < LWINDOW_MAX_OUTPUTS; ++i) {
		if(data->outputIds[i] == id) {
			wl_output_destroy(data->outputs[i]);
			data->outputs[i]   = NULL;
			data->outputIds[i] = 0;
			data->outputInfo[i] = (LOutputInfo) { 0 };
			break;
		}
	}
}

static inline Monitor LOutputInfo_toMonitor(const LOutputInfo *info) {

	//Map Wayland wl_output_subpixel to per-channel pixel offsets.
	//All-zero means subpixel rendering is disabled (unknown or no layout).
	//Horizontal: offsets are along X. Vertical: along Y.
	//RGB order: R is at -1, G at 0, B at 1 (offset in that direction).
	//BGR order: R is at 1, G at 0, B at -1.
	I32x2 spR = I32x2_zero, spG = I32x2_zero, spB = I32x2_zero;

	switch (info->subpixel) {

		case WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB:
			spR = I32x2_create2(-1, 0);
			spG = I32x2_create2(0,  0);
			spB = I32x2_create2(1,  0);
			break;

		case WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR:
			spR = I32x2_create2(1,  0);
			spG = I32x2_create2(0,  0);
			spB = I32x2_create2(-1, 0);
			break;

		case WL_OUTPUT_SUBPIXEL_VERTICAL_RGB:
			spR = I32x2_create2(0, -1);
			spG = I32x2_create2(0, 0);
			spB = I32x2_create2(0, 1);
			break;

		case WL_OUTPUT_SUBPIXEL_VERTICAL_BGR:
			spR = I32x2_create2(0, 1);
			spG = I32x2_create2(0, 0);
			spB = I32x2_create2(0, -1);
			break;

		default:   //WL_OUTPUT_SUBPIXEL_UNKNOWN / WL_OUTPUT_SUBPIXEL_NONE
			break;
	}

	return (Monitor) {
		.offsetPixels = I32x2_create2(info->x,          info->y),
		.sizePixels   = I32x2_create2(info->pixelWidth, info->pixelHeight),
		.offsetR      = spR,
		.offsetG      = spG,
		.offsetB      = spB,
		.sizeMm       = I32x2_create2(info->mmWidth,    info->mmHeight),
		.refreshRate  = info->refreshRate > 0 ? (F32)info->refreshRate / 1000.f : 0.f,
		.orientation  = (EMonitorOrientation) info->transform,
	};
}

Bool WindowManager_updateMonitorsExt(
	LWindowManager *lmanager,
	ListMonitor *monitors,
	const U32 *activeIds,
	U32 activeCount,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListMonitor_clear(monitors, NULL);

	for(U32 i = 0; i < LWINDOW_MAX_OUTPUTS; ++i) {

		if(!lmanager->outputs[i])
			continue;

		if(activeIds) {
			
			Bool found = false;

			for(U32 j = 0; j < activeCount && !found; ++j)
				found = activeIds[j] == lmanager->outputIds[i];

			if(!found)
				continue;
		}

		Monitor m = LOutputInfo_toMonitor(&lmanager->outputInfo[i]);
		gotoIfError3(clean, ListMonitor_pushBack(monitors, m, Platform_instance->alloc, e_rr));
	}

clean:
	return s_uccess;
}

Bool WindowManager_updateMonitors(WindowManager *wm, Error *e_rr) {

	Bool s_uccess = true;

	if(!wm)
		retError(clean, Error_nullPointer(0, "WindowManager_updateMonitors()::wm is required"));

	if(!wm->platformData.ptr)     //No monitors for virtual windows
		goto clean;

	LWindowManager *lmanager = (LWindowManager*)wm->platformData.ptr;

	gotoIfError3(clean, WindowManager_updateMonitorsExt(lmanager, &wm->monitors, NULL, 0, e_rr));

	if(wm->callbacks.onMonitorChange)
		wm->callbacks.onMonitorChange(wm);

clean:
	return s_uccess;
}

Bool WindowManager_freeNative(WindowManager *w);

Bool WindowManager_createNative(WindowManager *w, Error *e_rr) {

	Bool s_uccess = true;
	Bool alloc = false;

	gotoIfError3(clean, Buffer_createEmptyBytes(sizeof(LWindowManager), Platform_instance->alloc, &w->platformData, e_rr));
	alloc = true;

	LWindowManager *manager  = (LWindowManager*)w->platformData.ptr;

	const C8 *waylandDisplay = getenv("GAMESCOPE_WAYLAND_DISPLAY");

	if(waylandDisplay)
		w->isSingleWindow = true;

	if(!waylandDisplay)
		waylandDisplay = getenv("WAYLAND_DISPLAY");

	manager->display         = wl_display_connect(waylandDisplay);

	manager->compositorId    = U64_MAX;

	if(!manager->display)
		retError(clean, Error_stderr(0, "WindowManager_createNative() couldn't connect to display"));

	manager->registry = wl_display_get_registry(manager->display);

	if(!manager->registry)
		retError(clean, Error_invalidState(0, "WindowManager_createNative() couldn't get registry"));

	manager->listener = (struct wl_registry_listener) {
		.global        = LWindowManager_register,
		.global_remove = LWindowManager_unregister
	};

	wl_registry_add_listener(manager->registry, &manager->listener, manager);

	wl_display_dispatch(manager->display);
	wl_display_roundtrip(manager->display);

	if(!manager->compositor || !manager->shm || !manager->xdgWmBase)
		retError(clean, Error_invalidState(0, "WindowManager_createNative() couldn't get compositor, shm or xdg"));

	//wl_subcompositor is needed for CSD bar on compositors without SSD support.
	//Not fatal, if it's missing and SSD is also missing, windows will just be undecorated.

	if(!manager->subcompositor)
		Log_warnLnx("WindowManager_createNative(): no wl_subcompositor, CSD unavailable if SSD is also absent");

	if(!manager->seat)
		Log_warnLnx("WindowManager_createNative(): no wl_seat found, input will be unavailable");

	//Read cursor size from environment; default 24.
	U64 cursorSize = 24;
	const C8 *sizeEnv = getenv("XCURSOR_SIZE");
	CharString sizeStr = CharString_createRefCStrConst(sizeEnv);

	if(CharString_length(sizeStr))
		CharString_parseDec(sizeStr, &cursorSize);

	const C8 *themeName = getenv("XCURSOR_THEME");
	manager->cursorTheme = wl_cursor_theme_load(themeName, cursorSize, manager->shm);

	if(!manager->cursorTheme)     //Fallback
		manager->cursorTheme = wl_cursor_theme_load(NULL, (I32) (cursorSize & (U64)I32_MAX), manager->shm);

	if(!manager->cursorTheme)
		Log_warnLnx("WindowManager_createNative(): could not load cursor theme, cursors will be hidden");

	//Pre-load the nine cursor shapes we need.
	//Index mapping: 0=default, 1-8=resize edges matching XDG_TOPLEVEL_RESIZE_EDGE_*.
	//XDG edge values: NONE=0, TOP=1, BOTTOM=2, LEFT=4, RIGHT=8,
	//                 TOP_LEFT=5, TOP_RIGHT=9, BOTTOM_LEFT=6, BOTTOM_RIGHT=10
	//We pack them by a small local index (see LWindow_edgeIndex).
	static const C8 *cursorNames[9] = {
		"left_ptr",         //0: default / interior
		"n-resize",         //1: top
		"s-resize",         //2: bottom
		"w-resize",         //3: left
		"e-resize",         //4: right
		"nw-resize",        //5: top-left
		"ne-resize",        //6: top-right
		"sw-resize",        //7: bottom-left
		"se-resize",        //8: bottom-right
	};

	if(manager->cursorTheme)
		for(U32 i = 0; i < 9; ++i) {

			manager->cursors[i] = wl_cursor_theme_get_cursor(
				manager->cursorTheme, cursorNames[i]
			);

			if(!manager->cursors[i] && i > 0) {

				static const C8 *legacyNames[9] = {
					NULL,
					"top_side",           "bottom_side",
					"left_side",          "right_side",
					"top_left_corner",    "top_right_corner",
					"bottom_left_corner", "bottom_right_corner",
				};

				manager->cursors[i] = wl_cursor_theme_get_cursor(
					manager->cursorTheme, legacyNames[i]
				);
			}
		}

	//Create the shared cursor surface.

	manager->cursorSurface = wl_compositor_create_surface(manager->compositor);

	if(!manager->cursorSurface)
		Log_warnLnx("WindowManager_createNative(): could not create cursor surface");

clean:

	if(!s_uccess && alloc) {
		WindowManager_freeNative(w);
		Buffer_free(&w->platformData, Platform_instance->alloc);
	}

	return s_uccess;
}

Bool WindowManager_freeNative(WindowManager *w) {

	LWindowManager *manager = (LWindowManager*)w->platformData.ptr;

	if(!manager)
		goto clean;

	if(manager->cursorSurface)
		wl_surface_destroy(manager->cursorSurface);

	if(manager->cursorTheme)
		wl_cursor_theme_destroy(manager->cursorTheme);

	if(manager->seat)
		wl_seat_destroy(manager->seat);

	for(U32 i = 0; i < LWINDOW_MAX_OUTPUTS; ++i)
		if(manager->outputs[i])
			wl_output_destroy(manager->outputs[i]);

	if(manager->subcompositor)
		wl_subcompositor_destroy(manager->subcompositor);

	if(manager->xdgWmBase)
		xdg_wm_base_destroy(manager->xdgWmBase);

	if(manager->shm)
		wl_shm_destroy(manager->shm);

	if(manager->compositor)
		wl_compositor_destroy(manager->compositor);

	if(manager->xdgDeco)
		zxdg_decoration_manager_v1_destroy(manager->xdgDeco);

	if(manager->registry)
		wl_registry_destroy(manager->registry);

	if(manager->display) {
		wl_display_flush(manager->display);
		wl_display_disconnect(manager->display);
	}

clean:
	return true;
}

void WindowManager_updateExt(WindowManager *manager) {

	LWindowManager *lmanager = (LWindowManager*)manager->platformData.ptr;

	//Flush outgoing requests before dispatching incoming events.

	if(lmanager) {
		wl_display_flush(lmanager->display);
		wl_display_roundtrip(lmanager->display);
	}

	//Fire manager-level callback once if any window updated its monitors
	if(manager->monitorsDirty) {

		manager->monitorsDirty = false;

		Error err = Error_none();
		if(!WindowManager_updateMonitors(manager, &err))
			Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_NewLine);
	}

	//Drive onUpdate + onDraw for every active window, matching WM_PAINT
	// behaviour on Windows. Without this, windows with no Wayland events
	// pending never render.

	for(U64 i = 0; i < manager->windows.length; ++i) {

		Window *w = manager->windows.ptrNonConst[i];

		if(!w || !(w->flags & EWindowFlags_IsActive))
			continue;

		if(
			!(w->hint & EWindowHint_AllowBackgroundUpdates) &&
			(w->flags & EWindowFlags_IsMinimized)
		)
			continue;

		InputDevice *dit  = ListInputDevice_begin(w->devices);
		InputDevice *dend = ListInputDevice_end(w->devices);

		for(; dit != dend; ++dit)
			InputDevice_markUpdate(dit);

		const Ns now = Time_now();

		if(w->callbacks.onUpdate) {
			const F64 dt = w->lastUpdate ? (now - w->lastUpdate) / (F64)SECOND : 0;
			w->callbacks.onUpdate(w, dt);
		}

		w->lastUpdate = now;

		if(w->callbacks.onDraw)
			w->callbacks.onDraw(w);
	}
}
