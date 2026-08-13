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

//platforms/linux/lwindow_input.c

#include "platforms/linux/lwindow_structs.h"
#include "lwindow_internal.h"
#include "platforms/window.h"
#include "platforms/window_manager.h"
#include "platforms/keyboard.h"
#include "platforms/mouse.h"

#include <sys/mman.h>
#include <unistd.h>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>
#include <wayland-cursor.h>

static U32 LWindow_edgeIndex(U32 edge) {
	switch(edge) {
		case XDG_TOPLEVEL_RESIZE_EDGE_TOP:          return 1;
		case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM:       return 2;
		case XDG_TOPLEVEL_RESIZE_EDGE_LEFT:         return 3;
		case XDG_TOPLEVEL_RESIZE_EDGE_RIGHT:        return 4;
		case XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT:     return 5;
		case XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT:    return 6;
		case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT:  return 7;
		case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT: return 8;
		default:                                    return 0;
	}
}

static void LWindow_updateCursor(Window *w, U32 resizeEdge) {

	LWindow *lwin = WindowExt(w, LWindow);
	LWindowManager *manager = (LWindowManager*)w->owner->platformData.ptr;

	if(!lwin->barPointer || !lwin->barSurface || !manager->cursorSurface)
		return;

	U32 idx = LWindow_edgeIndex(resizeEdge);

	if (w->flags & (EWindowFlags_IsFullscreen | EWindowFlags_IsMaximized))
		idx = 0;

	struct wl_cursor *cursor = manager->cursors[idx];

	if(!cursor)
		cursor = manager->cursors[0];

	if(!cursor || !cursor->image_count)
		return;

	struct wl_cursor_image *image  = cursor->images[0];
	struct wl_buffer       *buffer = wl_cursor_image_get_buffer(image);

	if(!buffer)
		return;

	wl_pointer_set_cursor(
		lwin->barPointer,
		lwin->lastPointerSerial,
		manager->cursorSurface,
		(I32)image->hotspot_x,
		(I32)image->hotspot_y
	);

	wl_surface_attach(manager->cursorSurface, buffer, 0, 0);
	wl_surface_damage_buffer(manager->cursorSurface, 0, 0, I32_MAX, I32_MAX);
	wl_surface_commit(manager->cursorSurface);
}

static U32 LWindow_resizeEdge(const Window *w, I32 x, I32 y) {

	I32 W = I32x2_x(w->size);
	I32 H = I32x2_y(w->size);

	I32 B = LWINDOW_RESIZE_BORDER;

	Bool left   = x <  B;
	Bool right  = x >= W - B;
	Bool top    = y <  B;
	Bool bottom = y >= H - B;

	if(top    && left)  return XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
	if(top    && right) return XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
	if(bottom && left)  return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
	if(bottom && right) return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
	if(top)             return XDG_TOPLEVEL_RESIZE_EDGE_TOP;
	if(bottom)          return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
	if(left)            return XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
	if(right)           return XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;

	return XDG_TOPLEVEL_RESIZE_EDGE_NONE;
}

static void LWindow_pointerEnterBar(
	void *data, struct wl_pointer *ptr, U32 serial,
	struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy
) {
	(void) ptr; (void) serial;
	Window  *w    = (Window*) data;
	LWindow *lwin = WindowExt(w, LWindow);

	lwin->lastPointerSerial     = serial;
	lwin->pointerCurrentSurface = surface;

	if(surface == lwin->barSurface) {
		lwin->pointerX     = wl_fixed_to_int(sx);
		lwin->pointerY     = wl_fixed_to_int(sy);
		Bool wasInBar      = lwin->pointerInBar;
		lwin->pointerInBar = true;
		if(!wasInBar) LWindow_redrawBar(w);   //Entering bar: always redraw once
		LWindow_updateCursor(w, XDG_TOPLEVEL_RESIZE_EDGE_NONE);
		return;
	}

	//Entering the main surface: seed absolute position, zero relative axes.

	if(surface == (struct wl_surface*)w->nativeHandle) {

		lwin->contentPointerX = wl_fixed_to_int(sx);
		lwin->contentPointerY = wl_fixed_to_int(sy);

		U32 edge = LWindow_resizeEdge(w, lwin->contentPointerX, lwin->contentPointerY);
		LWindow_updateCursor(w, edge);

		if(w->devices.length > w->defaultMouseId) {

			InputDevice *mouse = &w->devices.ptrNonConst[w->defaultMouseId];

			InputHandle hAbsX = InputDevice_createHandle(mouse, (U16)EMouseAxis_Temp0, EInputType_Axis);
			InputHandle hAbsY = InputDevice_createHandle(mouse, (U16)EMouseAxis_Temp1, EInputType_Axis);
			InputHandle hRelX = InputDevice_createHandle(mouse, (U16)EMouseAxis_RX,    EInputType_Axis);
			InputHandle hRelY = InputDevice_createHandle(mouse, (U16)EMouseAxis_RY,    EInputType_Axis);

			w->cursor = I32x2_create2(wl_fixed_to_int(sx), wl_fixed_to_int(sy));
			
			InputDevice_setCurrentAxis(mouse, hAbsX, (F32) I32x2_x(w->cursor));
			InputDevice_setCurrentAxis(mouse, hAbsY, (F32) I32x2_y(w->cursor));
			InputDevice_setCurrentAxis(mouse, hRelX, 0.f);
			InputDevice_setCurrentAxis(mouse, hRelY, 0.f);
		}
	}
}

static void LWindow_pointerLeaveBar(
	void *data, struct wl_pointer *ptr, U32 serial, struct wl_surface *surface
) {
	(void) ptr; (void) serial; (void) surface;
	Window  *w    = (Window*) data;
	LWindow *lwin = WindowExt(w, LWindow);

	lwin->pointerCurrentSurface = NULL;

	if(lwin->pointerInBar) {
		lwin->pointerInBar = false;
		LWindow_redrawBar(w);
	}

	if(surface == (struct wl_surface*)w->nativeHandle) {

		LWindow_updateCursor(w, XDG_TOPLEVEL_RESIZE_EDGE_NONE);

		//Pointer has left the surface; no further motion events are coming.
		//Settle RX/RY back to zero, same reasoning as touch-up/cancel above

		if(w->devices.length > w->defaultMouseId) {

			InputDevice *mouse = &w->devices.ptrNonConst[w->defaultMouseId];

			InputHandle hRelX = InputDevice_createHandle(mouse, (U16)EMouseAxis_RX, EInputType_Axis);
			InputHandle hRelY = InputDevice_createHandle(mouse, (U16)EMouseAxis_RY, EInputType_Axis);

			F32 prevRelX = InputDevice_getCurrentAxis(mouse, hRelX);
			F32 prevRelY = InputDevice_getCurrentAxis(mouse, hRelY);

			if(prevRelX != 0.f) {

				InputDevice_setCurrentAxis(mouse, hRelX, 0.f);

				if(w->callbacks.onDeviceAxis)
					w->callbacks.onDeviceAxis(w, mouse, hRelX, 0.f);
			}

			if(prevRelY != 0.f) {
				
				InputDevice_setCurrentAxis(mouse, hRelY, 0.f);

				if(w->callbacks.onDeviceAxis)
					w->callbacks.onDeviceAxis(w, mouse, hRelY, 0.f);
			}
		}
	}
}

static void LWindow_pointerMotionBar(
	void *data, struct wl_pointer *ptr, U32 time, wl_fixed_t sx, wl_fixed_t sy
) {
	(void) ptr; (void) time;
	Window  *w    = (Window*) data;
	LWindow *lwin = WindowExt(w, LWindow);

	//Bar surface: zone-change-only redraw

	if(lwin->barSurface && lwin->pointerCurrentSurface == lwin->barSurface) {

		I32 nx = wl_fixed_to_int(sx);
		I32 ny = wl_fixed_to_int(sy);

		if(nx == lwin->pointerX && ny == lwin->pointerY)
			return;

		I32 closeX = (I32)lwin->barWidth - LWINDOW_DECOR_BTN_W;
		I32 maxX   = closeX              - LWINDOW_DECOR_BTN_W;
		I32 minX   = maxX                - LWINDOW_DECOR_BTN_W;

		I32 oldZone = lwin->pointerX >= closeX ? 3 : (
			lwin->pointerX >= maxX ? 2 : (
				lwin->pointerX >= minX ? 1 : 0
			)
		);

		I32 newZone = nx >= closeX ? 3 : (
			nx >= maxX ? 2 : (
				nx >= minX ? 1 : 0
			)
		);

		lwin->pointerX = nx;
		lwin->pointerY = ny;

		if(newZone != oldZone)
			LWindow_redrawBar(w);

		return;
	}

	//Main content surface: update mouse device axes

	if(lwin->pointerCurrentSurface && lwin->pointerCurrentSurface != (struct wl_surface*)w->nativeHandle)
		return;

	I32 nx = wl_fixed_to_int(sx);
	I32 ny = wl_fixed_to_int(sy);

	w->cursor = I32x2_create2(nx, ny);

	lwin->contentPointerX = nx;
	lwin->contentPointerY = ny;

	if(w->devices.length <= w->defaultMouseId)
		return;

	InputDevice *mouse = &w->devices.ptrNonConst[w->defaultMouseId];

	//Absolute position -> X, Y

	InputHandle hAbsX = InputDevice_createHandle(mouse, (U16)EMouseAxis_Temp0,    EInputType_Axis);
	InputHandle hAbsY = InputDevice_createHandle(mouse, (U16)EMouseAxis_Temp1,    EInputType_Axis);

	//Relative direction -> RX, RY

	InputHandle hRelX = InputDevice_createHandle(mouse, (U16)EMouseAxis_RX,       EInputType_Axis);
	InputHandle hRelY = InputDevice_createHandle(mouse, (U16)EMouseAxis_RY,       EInputType_Axis);

	F32 prevAbsX = InputDevice_getCurrentAxis(mouse, hAbsX);
	F32 prevAbsY = InputDevice_getCurrentAxis(mouse, hAbsY);

	F32 newAbsX = (F32)nx;
	F32 newAbsY = (F32)ny;

	F32 relX = newAbsX - prevAbsX;
	F32 relY = newAbsY - prevAbsY;

	InputDevice_setCurrentAxis(mouse, hAbsX, newAbsX);
	InputDevice_setCurrentAxis(mouse, hAbsY, newAbsY);
	InputDevice_setCurrentAxis(mouse, hRelX, relX);
	InputDevice_setCurrentAxis(mouse, hRelY, relY);

	U32 edge = LWindow_resizeEdge(w, nx, ny);
	LWindow_updateCursor(w, edge);

	if(w->callbacks.onDeviceAxis) {

		if(newAbsX != prevAbsX) {
			w->callbacks.onDeviceAxis(w, mouse, hAbsX, newAbsX);
			w->callbacks.onDeviceAxis(w, mouse, hRelX, relX);
		}

		if(newAbsY != prevAbsY) {
			w->callbacks.onDeviceAxis(w, mouse, hAbsY, newAbsY);
			w->callbacks.onDeviceAxis(w, mouse, hRelY, relY);
		}
	}
}

static void LWindow_pointerButtonBar(
	void *data, struct wl_pointer *ptr, U32 serial, U32 time,
	U32 button, U32 state
) {
	(void) ptr; (void) time;
	Window  *w    = (Window*) data;
	LWindow *lwin = WindowExt(w, LWindow);

	Bool pressed = state == WL_POINTER_BUTTON_STATE_PRESSED;

	//Bar surface: handle CSD buttons

	if(lwin->barSurface && lwin->pointerCurrentSurface == lwin->barSurface && pressed && button == BTN_LEFT) {

		LWindowManager *manager = (LWindowManager*)w->owner->platformData.ptr;
		I32 x      = lwin->pointerX;
		I32 closeX = (I32)lwin->barWidth - LWINDOW_DECOR_BTN_W;
		I32 maxX   = closeX              - LWINDOW_DECOR_BTN_W;
		I32 minX   = maxX                - LWINDOW_DECOR_BTN_W;

		if(x >= closeX)
			w->flags |= EWindowFlags_ShouldTerminate;

		else if(x >= maxX) {
			if(!(w->hint & EWindowHint_DisableResize)) {

				if(w->flags & EWindowFlags_IsMaximized)
					xdg_toplevel_unset_maximized(lwin->topLevel);
				else
					xdg_toplevel_set_maximized(lwin->topLevel);
			}
		}

		else if(x >= minX)
			xdg_toplevel_set_minimized(lwin->topLevel);

		else xdg_toplevel_move(lwin->topLevel, manager->seat, serial);

		return;
	}

	//Main content surface: forward to Mouse InputDevice

	if(lwin->pointerCurrentSurface && lwin->pointerCurrentSurface != (struct wl_surface*)w->nativeHandle)
		return;

	if(
		pressed && button == BTN_LEFT &&
		lwin->pointerCurrentSurface == (struct wl_surface*)w->nativeHandle &&
		!(w->hint & EWindowHint_DisableResize) &&
		!(w->flags & (EWindowFlags_IsFullscreen | EWindowFlags_IsMaximized))
	) {

		U32 edge = LWindow_resizeEdge(w, lwin->contentPointerX, lwin->contentPointerY);

		if(edge != XDG_TOPLEVEL_RESIZE_EDGE_NONE) {
			LWindowManager *manager = (LWindowManager*)w->owner->platformData.ptr;
			xdg_toplevel_resize(lwin->topLevel, manager->seat, serial, edge);
			return;
		}
	}

	if(w->devices.length <= w->defaultMouseId)
		return;

	InputDevice *mouse = &w->devices.ptrNonConst[w->defaultMouseId];

	//Map linux BTN_ codes to EMouseButton.
	//EMouseButton values start after EMouseAxis_End; mirror the Windows mapping.
	EMouseActions mb;
	switch(button) {
		case BTN_LEFT:    mb = EMouseButton_Left;    break;
		case BTN_RIGHT:   mb = EMouseButton_Right;   break;
		case BTN_MIDDLE:  mb = EMouseButton_Middle;  break;
		case BTN_FORWARD: mb = EMouseButton_Forward; break;
		case BTN_BACK:    mb = EMouseButton_Back;    break;
		default:         return;
	}

	InputHandle handle = InputDevice_createHandle(mouse, (U16)(mb - EMouseAxis_End), EInputType_Button);

	EInputState prev = InputDevice_getState(mouse, handle);
	InputDevice_setCurrentState(mouse, handle, pressed);
	EInputState next = InputDevice_getState(mouse, handle);

	if(prev != next && w->callbacks.onDeviceButton)
		w->callbacks.onDeviceButton(w, mouse, handle, pressed);
}

static void LWindow_pointerAxisBar(
	void *data, struct wl_pointer *ptr, U32 time, U32 axis, wl_fixed_t value
) {
	(void) ptr; (void) time;
	Window  *w    = (Window*) data;
	LWindow *lwin = WindowExt(w, LWindow);

	//Bar surface: ignore scroll

	if(lwin->pointerCurrentSurface && lwin->pointerCurrentSurface != (struct wl_surface*)w->nativeHandle)
		return;

	if(w->devices.length <= w->defaultMouseId)
		return;

	InputDevice *mouse = &w->devices.ptrNonConst[w->defaultMouseId];

	EMouseActions scrollAxis = axis == WL_POINTER_AXIS_VERTICAL_SCROLL ? EMouseAxis_ScrollWheel_Y : EMouseAxis_ScrollWheel_X;

	InputHandle handle = InputDevice_createHandle(mouse, (U16)scrollAxis, EInputType_Axis);

	F32 delta = (F32)wl_fixed_to_double(value);
	InputDevice_setCurrentAxis(mouse, handle, delta);

	if(w->callbacks.onDeviceAxis)
		w->callbacks.onDeviceAxis(w, mouse, handle, delta);
}

//Required stubs for wl_seat version 5, missing entries might cause NULL-dispatch crashes
// on compositors that send these events.
static void LWindow_pointerFrameBar(void *d, struct wl_pointer *p) {
	(void)d; (void)p;
}

static void LWindow_pointerAxisSourceBar(void *d, struct wl_pointer *p, U32 s) {
	(void)d; (void)p; (void)s;
}

static void LWindow_pointerAxisStopBar(void *d, struct wl_pointer *p, U32 t, U32 a) {
	(void)d; (void)p; (void)t; (void)a;
}

static void LWindow_pointerAxisDiscreteBar(void *d, struct wl_pointer *p, U32 a, I32 v) {
	(void)d; (void)p; (void)a; (void)v;
}

static void LWindow_touchDown(
	void *data, struct wl_touch *touch, U32 serial, U32 time,
	struct wl_surface *surface, I32 id, wl_fixed_t x, wl_fixed_t y
) {
	(void) touch; (void) serial; (void) time;

	Window  *w    = (Window*) data;
	LWindow *lwin = WindowExt(w, LWindow);

	if(surface != (struct wl_surface*)w->nativeHandle || lwin->primaryTouchId != -1)
		return;   //Only the first finger drives the mouse-equivalent for now

	lwin->primaryTouchId = id;

	I32 nx = wl_fixed_to_int(x), ny = wl_fixed_to_int(y);
	w->cursor = I32x2_create2(nx, ny);
	lwin->contentPointerX = nx;
	lwin->contentPointerY = ny;

	if(w->devices.length <= w->defaultMouseId)
		return;

	InputDevice *mouse = &w->devices.ptrNonConst[w->defaultMouseId];

	//Fresh contact point: seed absolute position, zero relative axes,
	// same as LWindow_pointerEnterBar does for a pointer entering the surface.
	//There is no "previous position" yet,
	// so RX/RY must not carry over a stale delta from mouse motion or a different finger's last move

	InputHandle hAbsX = InputDevice_createHandle(mouse, (U16)EMouseAxis_Temp0, EInputType_Axis);
	InputHandle hAbsY = InputDevice_createHandle(mouse, (U16)EMouseAxis_Temp1, EInputType_Axis);
	InputHandle hRelX = InputDevice_createHandle(mouse, (U16)EMouseAxis_RX,    EInputType_Axis);
	InputHandle hRelY = InputDevice_createHandle(mouse, (U16)EMouseAxis_RY,    EInputType_Axis);

	InputDevice_setCurrentAxis(mouse, hAbsX, (F32)nx);
	InputDevice_setCurrentAxis(mouse, hAbsY, (F32)ny);
	InputDevice_setCurrentAxis(mouse, hRelX, 0.f);
	InputDevice_setCurrentAxis(mouse, hRelY, 0.f);

	//Note: no onDeviceAxis callbacks here, deliberately, same as pointerEnterBar.
	//Where the finger first lands isn't reportable motion, just the tracking baseline.

	InputHandle hBtn = InputDevice_createHandle(mouse, (U16)(EMouseButton_Left - EMouseAxis_End), EInputType_Button);

	InputDevice_setCurrentState(mouse, hBtn, true);

	if(w->callbacks.onDeviceButton)
		w->callbacks.onDeviceButton(w, mouse, hBtn, true);
}

static void LWindow_touchUp(void *data, struct wl_touch *touch, U32 serial, U32 time, I32 id) {

	(void) touch; (void) serial; (void) time;

	Window  *w    = (Window*) data;
	LWindow *lwin = WindowExt(w, LWindow);

	if(lwin->primaryTouchId != id)
		return;

	lwin->primaryTouchId = -1;

	if(w->devices.length <= w->defaultMouseId)
		return;

	InputDevice *mouse = &w->devices.ptrNonConst[w->defaultMouseId];

	//Contact has ended; no further motion events will arrive for this finger.
	//Settle RX/RY back to zero so nothing reads a stale "still moving" delta,
	// and report the change, same as any other axis update.
	//Abs (Temp0/Temp1) is left untouched: "last known position" stays meaningful after lift-off

	InputHandle hRelX = InputDevice_createHandle(mouse, (U16)EMouseAxis_RX, EInputType_Axis);
	InputHandle hRelY = InputDevice_createHandle(mouse, (U16)EMouseAxis_RY, EInputType_Axis);

	F32 prevRelX = InputDevice_getCurrentAxis(mouse, hRelX);
	F32 prevRelY = InputDevice_getCurrentAxis(mouse, hRelY);

	if(prevRelX != 0.f) {

		InputDevice_setCurrentAxis(mouse, hRelX, 0.f);

		if(w->callbacks.onDeviceAxis)
			w->callbacks.onDeviceAxis(w, mouse, hRelX, 0.f);
	}

	if(prevRelY != 0.f) {

		InputDevice_setCurrentAxis(mouse, hRelY, 0.f);

		if(w->callbacks.onDeviceAxis)
			w->callbacks.onDeviceAxis(w, mouse, hRelY, 0.f);
	}

	InputHandle hBtn = InputDevice_createHandle(mouse, (U16)(EMouseButton_Left - EMouseAxis_End), EInputType_Button);

	InputDevice_setCurrentState(mouse, hBtn, false);

	if(w->callbacks.onDeviceButton)
		w->callbacks.onDeviceButton(w, mouse, hBtn, false);
}

static void LWindow_touchMotion(void *data, struct wl_touch *touch, U32 time, I32 id, wl_fixed_t x, wl_fixed_t y) {

	(void) touch; (void) time;

	Window  *w    = (Window*) data;
	LWindow *lwin = WindowExt(w, LWindow);

	if(lwin->primaryTouchId != id)
		return;

	I32 nx = wl_fixed_to_int(x), ny = wl_fixed_to_int(y);
	w->cursor = I32x2_create2(nx, ny);
	lwin->contentPointerX = nx;
	lwin->contentPointerY = ny;

	if(w->devices.length <= w->defaultMouseId)
		return;

	InputDevice *mouse = &w->devices.ptrNonConst[w->defaultMouseId];

	InputHandle hAbsX = InputDevice_createHandle(mouse, (U16)EMouseAxis_Temp0, EInputType_Axis);
	InputHandle hAbsY = InputDevice_createHandle(mouse, (U16)EMouseAxis_Temp1, EInputType_Axis);
	InputHandle hRelX = InputDevice_createHandle(mouse, (U16)EMouseAxis_RX,    EInputType_Axis);
	InputHandle hRelY = InputDevice_createHandle(mouse, (U16)EMouseAxis_RY,    EInputType_Axis);

	F32 prevAbsX = InputDevice_getCurrentAxis(mouse, hAbsX);
	F32 prevAbsY = InputDevice_getCurrentAxis(mouse, hAbsY);
	F32 relX = (F32)nx - prevAbsX, relY = (F32)ny - prevAbsY;

	InputDevice_setCurrentAxis(mouse, hAbsX, (F32)nx);
	InputDevice_setCurrentAxis(mouse, hAbsY, (F32)ny);
	InputDevice_setCurrentAxis(mouse, hRelX, relX);
	InputDevice_setCurrentAxis(mouse, hRelY, relY);

	if(w->callbacks.onDeviceAxis) {

		if(relX != 0.f) {
			w->callbacks.onDeviceAxis(w, mouse, hAbsX, (F32)nx);
			w->callbacks.onDeviceAxis(w, mouse, hRelX, relX);
		}

		if(relY != 0.f) {
			w->callbacks.onDeviceAxis(w, mouse, hAbsY, (F32)ny);
			w->callbacks.onDeviceAxis(w, mouse, hRelY, relY);
		}
	}
}

static void LWindow_touchFrame(void *d, struct wl_touch *t) { (void)d; (void)t; }

static void LWindow_touchCancel(void *data, struct wl_touch *touch) {

	(void) touch;

	Window  *w    = (Window*) data;
	LWindow *lwin = WindowExt(w, LWindow);

	if(lwin->primaryTouchId == -1)
		return;

	lwin->primaryTouchId = -1;

	if(w->devices.length <= w->defaultMouseId)
		return;

	InputDevice *mouse = &w->devices.ptrNonConst[w->defaultMouseId];

	//Same "contact has ended" settle-and-report as touchUp, cancel is just a
	// different trigger for the same semantics (e.g. compositor took the touch
	// sequence away mid-gesture, such as for a gesture or focus change).

	InputHandle hRelX = InputDevice_createHandle(mouse, (U16)EMouseAxis_RX, EInputType_Axis);
	InputHandle hRelY = InputDevice_createHandle(mouse, (U16)EMouseAxis_RY, EInputType_Axis);

	F32 prevRelX = InputDevice_getCurrentAxis(mouse, hRelX);
	F32 prevRelY = InputDevice_getCurrentAxis(mouse, hRelY);

	if(prevRelX != 0.f) {

		InputDevice_setCurrentAxis(mouse, hRelX, 0.f);

		if(w->callbacks.onDeviceAxis)
			w->callbacks.onDeviceAxis(w, mouse, hRelX, 0.f);
	}

	if(prevRelY != 0.f) {

		InputDevice_setCurrentAxis(mouse, hRelY, 0.f);

		if(w->callbacks.onDeviceAxis)
			w->callbacks.onDeviceAxis(w, mouse, hRelY, 0.f);
	}

	InputHandle hBtn = InputDevice_createHandle(mouse, (U16)(EMouseButton_Left - EMouseAxis_End), EInputType_Button);

	InputDevice_setCurrentState(mouse, hBtn, false);

	if(w->callbacks.onDeviceButton)
		w->callbacks.onDeviceButton(w, mouse, hBtn, false);
}

const struct wl_touch_listener LWindow_touchListener = {
	.down   = LWindow_touchDown,
	.up     = LWindow_touchUp,
	.motion = LWindow_touchMotion,
	.frame  = LWindow_touchFrame,
	.cancel = LWindow_touchCancel,
};

const struct wl_pointer_listener LWindow_barPointerListener = {
	.enter         = LWindow_pointerEnterBar,
	.leave         = LWindow_pointerLeaveBar,
	.motion        = LWindow_pointerMotionBar,
	.button        = LWindow_pointerButtonBar,
	.axis          = LWindow_pointerAxisBar,
	.frame         = LWindow_pointerFrameBar,
	.axis_source   = LWindow_pointerAxisSourceBar,
	.axis_stop     = LWindow_pointerAxisStopBar,
	.axis_discrete = LWindow_pointerAxisDiscreteBar,
};

static void LWindow_kbKeymap(
	void *data, struct wl_keyboard *kb,
	U32 format, I32 fd, U32 size
) {
	(void) kb;
	Window  *w    = (Window*) data;
	LWindow *lwin = WindowExt(w, LWindow);

	if(format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		close(fd);
		return;
	}

	void *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);

	if(map == MAP_FAILED)
		return;

	struct xkb_keymap *keymap = xkb_keymap_new_from_string(
		lwin->xkbContext, (const C8*)map,
		XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS
	);
	munmap(map, size);

	if(!keymap)
		return;

	struct xkb_state *newState = xkb_state_new(keymap);
	xkb_keymap_unref(keymap);

	if(!newState)
		return;

	if(lwin->xkbState)
		xkb_state_unref(lwin->xkbState);

	lwin->xkbState = newState;
}

static void LWindow_kbEnter(
	void *data, struct wl_keyboard *kb,
	U32 serial, struct wl_surface *surface, struct wl_array *keys
) {
	(void) kb; (void) serial; (void) surface; (void) keys;
	Window *w = (Window*) data;
	w->flags |= EWindowFlags_IsFocussed;

	if(w->callbacks.onUpdateFocus)
		w->callbacks.onUpdateFocus(w);
}

static void LWindow_kbLeave(
	void *data, struct wl_keyboard *kb,
	U32 serial, struct wl_surface *surface
) {
	(void) kb; (void) serial; (void) surface;
	Window  *w    = (Window*) data;

	//Reset all key states on focus loss, matching Windows WM_KILLFOCUS behavior
	for(U64 i = 0; i < w->devices.length; ++i) {

		InputDevice *dev = &w->devices.ptrNonConst[i];

		for(U16 j = 0; j < dev->buttons; ++j) {

			InputHandle handle = InputDevice_createHandle(dev, j, EInputType_Button);
			EInputState prevState = InputDevice_getState(dev, handle);

			if(prevState & EInputState_Curr) {

				InputDevice_setCurrentState(dev, handle, false);

				if(w->callbacks.onDeviceButton)
					w->callbacks.onDeviceButton(w, dev, handle, false);
			}
		}

		for(U16 j = 0; j < dev->axes; ++j) {

			InputAxis axis = *InputDevice_getAxis(dev, j);

			if(!axis.resetOnInputLoss)
				continue;

			InputHandle handle = InputDevice_createHandle(dev, j, EInputType_Axis);

			if(InputDevice_getCurrentAxis(dev, handle)) {

				InputDevice_setCurrentAxis(dev, handle, 0);

				if(w->callbacks.onDeviceAxis)
					w->callbacks.onDeviceAxis(w, dev, handle, 0);
			}
		}

		dev->flags = 0;
	}

	w->flags &= ~EWindowFlags_IsFocussed;

	if(w->callbacks.onUpdateFocus)
		w->callbacks.onUpdateFocus(w);
}

static InputHandle LWindow_scancodeToKey(U32 sc) {
	switch(sc) {

		//Row 0
		case KEY_ESC:        return EKey_Escape;

		case KEY_F1:         return EKey_F1;
		case KEY_F2:         return EKey_F2;
		case KEY_F3:         return EKey_F3;
		case KEY_F4:         return EKey_F4;
		case KEY_F5:         return EKey_F5;
		case KEY_F6:         return EKey_F6;
		case KEY_F7:         return EKey_F7;
		case KEY_F8:         return EKey_F8;
		case KEY_F9:         return EKey_F9;
		case KEY_F10:        return EKey_F10;
		case KEY_F11:        return EKey_F11;
		case KEY_F12:        return EKey_F12;

		//Row 1
		case KEY_GRAVE:      return EKey_Backtick;
		case KEY_1:          return EKey_1;
		case KEY_2:          return EKey_2;
		case KEY_3:          return EKey_3;
		case KEY_4:          return EKey_4;
		case KEY_5:          return EKey_5;
		case KEY_6:          return EKey_6;
		case KEY_7:          return EKey_7;
		case KEY_8:          return EKey_8;
		case KEY_9:          return EKey_9;
		case KEY_0:          return EKey_0;
		case KEY_MINUS:      return EKey_Minus;
		case KEY_EQUAL:      return EKey_Equals;
		case KEY_BACKSPACE:  return EKey_Backspace;

		//Row 2
		case KEY_TAB:        return EKey_Tab;
		case KEY_Q:          return EKey_Q;
		case KEY_W:          return EKey_W;
		case KEY_E:          return EKey_E;
		case KEY_R:          return EKey_R;
		case KEY_T:          return EKey_T;
		case KEY_Y:          return EKey_Y;
		case KEY_U:          return EKey_U;
		case KEY_I:          return EKey_I;
		case KEY_O:          return EKey_O;
		case KEY_P:          return EKey_P;
		case KEY_LEFTBRACE:  return EKey_LBracket;
		case KEY_RIGHTBRACE: return EKey_RBracket;

		//Row 3
		case KEY_CAPSLOCK:   return EKey_Caps;
		case KEY_A:          return EKey_A;
		case KEY_S:          return EKey_S;
		case KEY_D:          return EKey_D;
		case KEY_F:          return EKey_F;
		case KEY_G:          return EKey_G;
		case KEY_H:          return EKey_H;
		case KEY_J:          return EKey_J;
		case KEY_K:          return EKey_K;
		case KEY_L:          return EKey_L;
		case KEY_SEMICOLON:  return EKey_Semicolon;
		case KEY_APOSTROPHE: return EKey_Quote;
		case KEY_BACKSLASH:  return EKey_Backslash;
		case KEY_ENTER:      return EKey_Enter;

		//Row 4
		case KEY_LEFTSHIFT:  return EKey_LShift;
		case KEY_102ND:      return EKey_Bar;
		case KEY_Z:          return EKey_Z;
		case KEY_X:          return EKey_X;
		case KEY_C:          return EKey_C;
		case KEY_V:          return EKey_V;
		case KEY_B:          return EKey_B;
		case KEY_N:          return EKey_N;
		case KEY_M:          return EKey_M;
		case KEY_COMMA:      return EKey_Comma;
		case KEY_DOT:        return EKey_Period;
		case KEY_SLASH:      return EKey_Slash;
		case KEY_RIGHTSHIFT: return EKey_RShift;

		//Row 5
		case KEY_LEFTCTRL:   return EKey_LCtrl;
		case KEY_LEFTMETA:   return EKey_LMenu;
		case KEY_LEFTALT:    return EKey_LAlt;
		case KEY_SPACE:      return EKey_Space;
		case KEY_RIGHTALT:   return EKey_RAlt;
		case KEY_RIGHTMETA:  return EKey_RMenu;
		case KEY_COMPOSE:    return EKey_Options;
		case KEY_RIGHTCTRL:  return EKey_RCtrl;

		//Navigation cluster
		case KEY_SYSRQ:      return EKey_PrintScreen;
		case KEY_SCROLLLOCK: return EKey_ScrollLock;
		case KEY_PAUSE:      return EKey_Pause;
		case KEY_INSERT:     return EKey_Insert;
		case KEY_HOME:       return EKey_Home;
		case KEY_PAGEUP:     return EKey_PageUp;
		case KEY_DELETE:     return EKey_Delete;
		case KEY_END:        return EKey_End;
		case KEY_PAGEDOWN:   return EKey_PageDown;

		//Arrow keys
		case KEY_UP:         return EKey_Up;
		case KEY_LEFT:       return EKey_Left;
		case KEY_DOWN:       return EKey_Down;
		case KEY_RIGHT:      return EKey_Right;

		//Numpad
		case KEY_NUMLOCK:    return EKey_NumLock;
		case KEY_KP0:        return EKey_Numpad0;
		case KEY_KP1:        return EKey_Numpad1;
		case KEY_KP2:        return EKey_Numpad2;
		case KEY_KP3:        return EKey_Numpad3;
		case KEY_KP4:        return EKey_Numpad4;
		case KEY_KP5:        return EKey_Numpad5;
		case KEY_KP6:        return EKey_Numpad6;
		case KEY_KP7:        return EKey_Numpad7;
		case KEY_KP8:        return EKey_Numpad8;
		case KEY_KP9:        return EKey_Numpad9;
		case KEY_KPASTERISK: return EKey_NumpadMul;
		case KEY_KPPLUS:     return EKey_NumpadAdd;
		case KEY_KPDOT:      return EKey_NumpadDot;
		case KEY_KPSLASH:    return EKey_NumpadDiv;
		case KEY_KPMINUS:    return EKey_NumpadSub;
		case KEY_KPENTER:    return EKey_Enter;

		//Media / browser
		case KEY_BACK:          return EKey_Back;
		case KEY_FORWARD:       return EKey_Forward;
		case KEY_SLEEP:         return EKey_Sleep;
		case KEY_REFRESH:       return EKey_Refresh;
		case KEY_SEARCH:        return EKey_Search;
		case KEY_MUTE:          return EKey_Mute;
		case KEY_VOLUMEDOWN:    return EKey_VolumeDown;
		case KEY_VOLUMEUP:      return EKey_VolumeUp;
		case KEY_NEXTSONG:      return EKey_Skip;
		case KEY_PREVIOUSSONG:  return EKey_Previous;
		case KEY_HELP:          return EKey_Help;
		case KEY_CLEAR:         return EKey_Clear;

		default: return InputDevice_invalidHandle();
	}
}

static void LWindow_kbKey(
	void *data, struct wl_keyboard *kb,
	U32 serial, U32 time, U32 key, U32 keyState
) {
	(void) kb; (void) serial; (void) time;

	Window  *w    = (Window*) data;
	LWindow *lwin = WindowExt(w, LWindow);

	if(!lwin->xkbState)
		return;

	xkb_keycode_t keycode = key + 8;    //xkb keycode is evdev + 8
	Bool isDown = keyState == WL_KEYBOARD_KEY_STATE_PRESSED;

	//Keep modifier tracking self-consistent with the key stream itself,
	// instead of relying solely on a possibly-delayed wl_keyboard::modifiers event. (SteamOS specific)
	xkb_state_update_key(lwin->xkbState, keycode, isDown ? XKB_KEY_DOWN : XKB_KEY_UP);

	if(!w->devices.ptr)
		return;

	InputDevice *dev = w->devices.ptrNonConst + w->defaultKeyboardId;   //Builtin keyboard

	U32 flags =
		(xkb_state_mod_name_is_active(lwin->xkbState, XKB_MOD_NAME_CAPS, XKB_STATE_MODS_LOCKED) << EKeyboardFlags_Caps) |
		(xkb_state_mod_name_is_active(lwin->xkbState, XKB_MOD_NAME_NUM,  XKB_STATE_MODS_LOCKED) << EKeyboardFlags_NumLock);

	dev->flags = flags;

	InputHandle handle = LWindow_scancodeToKey(key);

	if(InputDevice_isValidHandle(dev, handle)) {

		EInputState prevState = InputDevice_getState(dev, handle);
		InputDevice_setCurrentState(dev, handle, isDown);
		
		EInputState newState = InputDevice_getState(dev, handle);

		if(prevState != newState && w->callbacks.onDeviceButton)
			w->callbacks.onDeviceButton(w, dev, handle, isDown);
	}

	//Text input, only on key press, only when we have a valid UTF-8 sequence.
	//Matches Windows WM_CHAR: fires for printable characters only.
	if(isDown && w->callbacks.onTypeChar) {

		C8 utf8[5] = { 0 };
		I32 len = xkb_state_key_get_utf8(lwin->xkbState, keycode, utf8, sizeof(utf8) - 1);

		//Filter out control characters (< 0x20) and DEL, matching WM_CHAR behaviour.
		if(len > 0 && (U8)utf8[0] >= 0x20 && utf8[0] != 0x7F)
			w->callbacks.onTypeChar(w, CharString_createRefSizedConst(utf8, (U64)len, true));
	}
}

static void LWindow_kbModifiers(
	void *data, struct wl_keyboard *kb,
	U32 serial,
	U32 modsDepressed, U32 modsLatched, U32 modsLocked, U32 group
) {
	(void) kb; (void) serial;
	Window  *w    = (Window*) data;
	LWindow *lwin = WindowExt(w, LWindow);

	if(lwin->xkbState)
		xkb_state_update_mask(lwin->xkbState, modsDepressed, modsLatched, modsLocked, 0, 0, group);
}

static void LWindow_kbRepeatInfo(void *d, struct wl_keyboard *kb, I32 rate, I32 delay) {
	(void)d; (void)kb; (void)rate; (void)delay;
	//TODO: Key repeat is handled at a higher level if needed
}

const struct wl_keyboard_listener LWindow_kbListener = {
	.keymap      = LWindow_kbKeymap,
	.enter       = LWindow_kbEnter,
	.leave       = LWindow_kbLeave,
	.key         = LWindow_kbKey,
	.modifiers   = LWindow_kbModifiers,
	.repeat_info = LWindow_kbRepeatInfo,
};
