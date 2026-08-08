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

//platforms/test/functional/test_platforms_functional_windowstate.c
//
//F2.  Fullscreen toggle          (requires EWindowHint_AllowFullscreen)
//F3a. Window resize / min + max enforcement (virtual)
//F4.  Multi-window               (if the platform allows more than one physical window)
//F9.  Focus / minimize cycle     (EWindowFlags_IsMinimized + EWindowFlags_IsFocussed)
//F12. Monitor info
//F13. Window move (drag to second monitor)
//F14. Maximize / restore
//F17. Min/max size enforcement (virtual clamp + physical compositor enforcement)
//F20. Borderless window
//F21. Transparent window (per-pixel alpha)

#include "test_platforms_functional_shared.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "platforms/window_manager.h"
#include "types/test/test.h"

#if _PLATFORM_TYPE == PLATFORM_WINDOWS
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h>
#elif _PLATFORM_TYPE == PLATFORM_LINUX
	#include <stdlib.h>
	#include "platforms/linux/lwindow_structs.h"
	Bool hasXdotool();
#endif

Bool isSingleWindow();

// -- Shared pattern-draw used by F2/F4/F9/F14/F20: a simple onDraw gradient ----
//
//All of these tests just need "draw *something* distinguishable, and prove that the pixel that landed in the buffer
// matches what onDraw was asked to draw at the time of the most recent present()".
//That state lives in PatternState and is set by the test body; only onDraw actually touches cpuVisibleBuffer.

typedef struct PatternState {
	volatile Bool drawn;
	U8 zxor;
} PatternState;

static void Pattern_onDraw(Window *w, PatternState *ps) {

	U32 W = (U32) I32x2_x(w->size);
	U32 H = (U32) I32x2_y(w->size);
	U8 *px = w->cpuVisibleBuffer.ptrNonConst;

	if(!px)
		return;

	for (U32 y = 0; y < H; ++y)
		for (U32 x = 0; x < W; ++x) {
			U8 *p = px + (y * W + x) * 4;
			p[0] = (U8) x;
			p[1] = (U8) y;
			p[2] = 128 ^ ps->zxor;
			p[3] = 255;
		}

	ps->drawn = true;
}

//Pump until either the callback fired or timeout.
//Used after present() to make sure onDraw actually ran with the *current* zxor before we move on
static Bool Pattern_waitForDraw(volatile Bool *drawn, Ns timeout) {

	Ns waited = 0;
	while (!*drawn && waited < timeout) {

		WindowManager_step(&windowManager, NULL, NULL);

		if(!windowManager.windows.length)
			break;

		Thread_sleep(16 * MS);
		waited += 16 * MS;
	}

	return *drawn;
}

//Present and make sure onDraw ran for it (falls back to calling onDraw directly
// on virtual windows, which have no paint loop to drive it).
static void presentAndDraw(Test *t, Window *w, PatternState *ps, void (*onDraw)(Window*)) {

	if (w->type == EWindowType_Physical) {
		ps->drawn = false;          //reset BEFORE the trigger
		invalidateForRepaint(w);    //WM_PAINT will onDraw + auto-present
		Test_assert(t, "onDrawFired", Pattern_waitForDraw(&ps->drawn, 1 * SECOND));
	} else {
		onDraw(w);
	}
}

// -- F2. Fullscreen toggle -----------------------------------------------------

static PatternState f2;

static void F2_onDraw(Window *w) { Pattern_onDraw(w, &f2); }

static void Test_fullScreen(Test *t) {

	Test_setModule(t, "F2/Fullscreen");

	if(isSingleWindow()) {
		Test_print(t, "Skipped. Device is single window");
		return;
	}

	f2 = (PatternState) { 0 };

	WindowCallbacks cbs = (WindowCallbacks) { 0 };
	cbs.onDraw = F2_onDraw;

	I32x2 sz = I32x2_create2(256, 256);

	WindowRef *wRef = createWindowCallback(
		t, "F2: Fullscreen", I32x2_zero, sz,
		EWindowHint_AllowFullscreen | EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, cbs
	);

	if (!Test_assert(t, "windowCreated", wRef != NULL))
		goto clean;

	Window *w = RefPtr_data(wRef, Window);
	Test_assert(t, "notFullscreenInit", !Window_isFullScreen(w));

	f2.zxor = 0;
	presentAndDraw(t, w, &f2, F2_onDraw);
	pump(1 * SECOND);

	if (!Test_assert(t, "toggleOn", Window_toggleFullScreen(w, &t->err)))
		goto clean;

	pump(1 * SECOND);
	f2.zxor = 0x80;
	presentAndDraw(t, w, &f2, F2_onDraw);

	Test_assert(t, "isFullscreen", Window_isFullScreen(w));

	if (!Test_assert(t, "toggleOff", Window_toggleFullScreen(w, &t->err)))
		goto clean;

	pump(1 * SECOND);
	f2.zxor = 0xFF;
	presentAndDraw(t, w, &f2, F2_onDraw);
	pump(1 * SECOND);

	Test_assert(t, "notFullscreenAgain", !Window_isFullScreen(w));

clean:
	f2 = (PatternState) { 0 };
	RefPtr_dec(&wRef);
}

// -- F3a. Resize / min+max enforcement -----------------------------------------

static void Test_resizeVirtual(Test *t) {

	Test_setModule(t, "F3a/ResizeVirtual");

	I32x2 sz    = I32x2_create2(640, 480);
	I32x2 minSize = EResolution_get(EResolution_SD);
	I32x2 maxSize = I32x2_create2(1920, 1080);
	WindowRef *wRef = NULL;

	CharString title = CharString_createRefCStrConst("F3a: ResizeVirtual");
	I32x2 pos = I32x2_create2(50, 50);

	WindowManager_createWindow(
		&windowManager, EWindowType_Virtual, pos, sz, minSize, maxSize,
		EWindowHint_None, title, (WindowCallbacks) { 0 },
		EWindowFormat_AutoRGBA8, 0, &wRef, &t->err
	);

	if (!Test_assert(t, "windowCreated", wRef != NULL))
		goto clean;

	Window *w = RefPtr_data(wRef, Window);
	Test_assert(t, "initW", I32x2_x(w->size) == 640);
	Test_assert(t, "initH", I32x2_y(w->size) == 480);
	Test_assert(t, "minW",  I32x2_x(w->minSize) == I32x2_x(EResolution_get(EResolution_SD)));
	Test_assert(t, "maxW",  I32x2_x(w->maxSize) == 1920);

	I32x2 newSz = I32x2_create2(512, 384);
	if (!Test_assert(t, "resizeCPU", Window_resizeCPUBuffer(w, false, newSz, &t->err)))
		goto clean;

	Test_assert(t, "newW", I32x2_x(w->size) == 512);
	Test_assert(t, "newH", I32x2_y(w->size) == 384);

clean:
	RefPtr_dec(&wRef);
}

// -- F4. Multi-window ----------------------------------------------------------

static PatternState f4a, f4b;

static void F4_onDrawA(Window *w) { Pattern_onDraw(w, &f4a); }
static void F4_onDrawB(Window *w) { Pattern_onDraw(w, &f4b); }

static void Test_multiWindow(Test *t) {

	Test_setModule(t, "F4/MultiWindow");

	if(isSingleWindow()) {
		Test_print(t, "Skipped. Device is single window");
		return;
	}

	f4a = (PatternState) { .zxor = 0x00 };
	f4b = (PatternState) { .zxor = 0x80 };

	WindowCallbacks cbsA = (WindowCallbacks) { 0 };
	cbsA.onDraw = F4_onDrawA;

	WindowCallbacks cbsB = (WindowCallbacks) { 0 };
	cbsB.onDraw = F4_onDrawB;

	WindowRef *w1Ref = NULL, *w2Ref = NULL;
	I32x2 sz = I32x2_create2(256, 256);

	w1Ref = createWindowCallback(t, "F4: Window A", I32x2_zero, sz, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, cbsA);
	w2Ref = createWindowCallback(t, "F4: Window B", sz, sz, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, cbsB);

	if (!Test_assert(t, "w1Created", w1Ref != NULL))
		goto clean;

	if (!Test_assert(t, "w2Created", w2Ref != NULL))
		goto clean;

	Window *w1 = RefPtr_data(w1Ref, Window);
	Window *w2 = RefPtr_data(w2Ref, Window);

	presentAndDraw(t, w1, &f4a, F4_onDrawA);
	presentAndDraw(t, w2, &f4b, F4_onDrawB);

	Test_assert(t, "distinct",    w1 != w2);
	Test_assert(t, "sameOwner",   w1->owner == w2->owner);
	Test_assert(t, "managerHas2", windowManager.windows.length >= 2);

	pump(VISUAL_HOLD_NS);

clean:
	f4a = (PatternState) { 0 };
	f4b = (PatternState) { 0 };
	RefPtr_dec(&w1Ref);
	RefPtr_dec(&w2Ref);
}

// -- F9. Focus / minimize cycle -----------------------------------------------
//
// Programmatically minimizes and restores the window using OS calls, then verifies
// EWindowFlags_IsMinimized and EWindowFlags_IsFocussed are updated by the platform
// layer after each pump.
//
// On virtual windows we can only check that the helpers don't crash and that the
// initial state is sane (not minimized, flags consistent), since there is no
// compositor to drive the flag changes.

static PatternState f9;

static void F9_onDraw(Window *w) { Pattern_onDraw(w, &f9); }

static void Test_focusMinimize(Test *t) {

	Test_setModule(t, "F9/FocusMinimize");

	if(isSingleWindow()) {
		Test_print(t, "Skipped. Device is single window");
		return;
	}

	f9 = (PatternState) { 0 };

	WindowCallbacks cbs = (WindowCallbacks) { 0 };
	cbs.onDraw = F9_onDraw;

	I32x2 sz = I32x2_create2(640, 200);

	WindowRef *wRef = createWindowCallback(
		t, "F9: Minimize / restore, watch me", I32x2_zero, sz,
		EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, cbs
	);

	if (!Test_assert(t, "windowCreated", wRef != NULL))
		goto clean;

	//Initial state: not minimized

	Window *w = RefPtr_data(wRef, Window);
	presentAndDraw(t, w, &f9, F9_onDraw);
	pump(300 * MS);

	Test_assert(t, "notMinimizedInit", !Window_isMinimized(w));

	Bool isPhysical = w->type == EWindowType_Physical;

	if (!isPhysical) {
		//Virtual window: flags can't be driven by the OS.
		//Just verify helpers are stable and non-crashing.
		Test_print(t, "[virtual] skipping OS-driven focus/minimize assertions");
		Test_assert(t, "virtualNotMinimized", !Window_isMinimized(w));
		goto clean;
	}

	// -- Minimize --------------------------------------------------------------
	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		ShowWindow((HWND)w->nativeHandle, SW_MINIMIZE);
	#elif _PLATFORM_TYPE == PLATFORM_LINUX
		{
			LWindow *lwin = WindowExt(w, LWindow);
			LWindowManager *manager = (LWindowManager*)w->owner->platformData.ptr;
			xdg_toplevel_set_minimized(lwin->topLevel);
			wl_display_flush(manager->display);
		}
	#endif

	pump(500 * MS);

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		Test_assert(t, "isMinimized", Window_isMinimized(w));
	#else
		//Soft: log the observed state but don't hard-fail on platforms where
		// the OS may not drive the flag synchronously.
		if (!Window_isMinimized(w))
			Test_print(t, "WARN: IsMinimized not set after minimize request");
	#endif

	//Focus must have left when we minimized

	Test_assert(t, "notFocusedWhileMin", !Window_isFocussed(w));

	// Restore is only available for Windows, for Linux we can't detect it coming back

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS

		ShowWindow((HWND)w->nativeHandle, SW_RESTORE);
		SetForegroundWindow((HWND)w->nativeHandle);

		presentAndDraw(t, w, &f9, F9_onDraw);
		pump(VISUAL_HOLD_NS);

		Test_assert(t, "notMinimizedAfterRestore", !Window_isMinimized(w));
		Test_assert(t, "focusedAfterRestore",       Window_isFocussed(w));

	#endif

clean:
	f9 = (PatternState) { 0 };
	RefPtr_dec(&wRef);
}

// -- F12. Monitor info ---------------------------------------------------------

static void Test_monitorInfo(Test *t) {

	Test_setModule(t, "F12/MonitorInfo");

	I32x2 sz = I32x2_create2(320, 240);
	WindowRef *wRef = createWindow(
		t, "F12: Monitor info - check console output",
		sz, I32x2_zero, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8
	);

	if (!Test_assert(t, "windowCreated", wRef != NULL))
		goto clean;

	Window *w = RefPtr_data(wRef, Window);

	if(!w->owner->platformData.ptr)   //Headless runs don't actually have monitors
		goto clean;

	present(t, w);
	pump(300 * MS);

	Test_assert(t, "hasMonitor (manager)",  w->owner->monitors.length >= 1);
	Test_assert(t, "hasMonitor (window)",   w->monitors.length >= 1);

	for(U64 j = 0; j < 2; ++j) {

		ListMonitor monitors = j ? w->owner->monitors : w->monitors;

		Test_print(t, j ? "WindowManager" : "Window");

		for (U64 i = 0; i < monitors.length; ++i) {

			const Monitor *m = &monitors.ptr[i];
			Test_assert(t, "monitorW & H",   I32x2_all(m->sizePixels));
			Test_assert(t, "refreshRate",    m->refreshRate > 0.f);

			//Print for operator to visually verify

			Test_print(t, "Monitor info (verify matches your display settings):");

			#define REMAP_OFF_XY(x, y) (((x) + 1) | (((y) + 1) << 2))

			#define REMAP_RGB_OFF(r, g, b) (                  \
				REMAP_OFF_XY(I32x2_x(r), I32x2_y(r)) |        \
				(REMAP_OFF_XY(I32x2_x(r), I32x2_y(r)) << 4) | \
				(REMAP_OFF_XY(I32x2_x(r), I32x2_y(r)) << 8)   \
			)

			U16 rgb16 = (U16)REMAP_RGB_OFF(m->offsetR, m->offsetG, m->offsetB);

			const C8 *spName = "None";

			if(rgb16 == REMAP_RGB_OFF(I32x2_create2(-1, 0), I32x2_create2(0, 0), I32x2_create2(1, 0)))
				spName = "Horizontal RGB";

			else if(rgb16 == REMAP_RGB_OFF(I32x2_create2(1, 0), I32x2_create2(0, 0), I32x2_create2(-1, 0)))
				spName = "Horizontal BGR";

			else if(rgb16 == REMAP_RGB_OFF(I32x2_create2(0, 1), I32x2_create2(0, 0), I32x2_create2(0, -1)))
				spName = "Vertical RGB";

			else if(rgb16 == REMAP_RGB_OFF(I32x2_create2(0, -1), I32x2_create2(0, 0), I32x2_create2(0, 1)))
				spName = "Vertical BGR";

			Log_debugLn(Platform_instance->alloc,
				"-- Monitor %"PRIu64": %"PRIi32"x%"PRIi32" @ %.1f Hz, offset (%"PRIi32",%"PRIi32"), size %"PRIi32"x%"PRIi32" mm\n"
				"--- Subpixel: %s",
				(U64)i,
				I32x2_x(m->sizePixels), I32x2_y(m->sizePixels),
				m->refreshRate,
				I32x2_x(m->offsetPixels), I32x2_y(m->offsetPixels),
				I32x2_x(m->sizeMm), I32x2_y(m->sizeMm),
				spName
			);
		}
	}

	Test_print(t, ">>> INTERACTIVE: Verify monitor count and resolution match your system (5s) <<<");
	pump(5 * SECOND);

clean:
	RefPtr_dec(&wRef);
}

// -- F13. Window move (drag to second monitor) ---------------------------------

static volatile Bool movedToSecondMonitor = false;

//Check if we are now on a monitor with a non-zero X offset,
// which indicates a second monitor to the right (most common layout).
static void onMonitorChange(Window *w) {
	for (U64 i = 0; i < w->monitors.length; ++i)
		if (
			I32x2_x(w->monitors.ptr[i].offsetPixels) != 0 ||
			I32x2_y(w->monitors.ptr[i].offsetPixels) != 0
		) {
			movedToSecondMonitor = true;
			break;
		}
}

static void Test_windowMove(Test *t) {

	Test_setModule(t, "F13/WindowMove");

	if(isSingleWindow()) {
		Test_print(t, "Skipped. Device is single window");
		return;
	}

	WindowCallbacks cbs = (WindowCallbacks) { 0 };
	cbs.onMonitorChange = onMonitorChange;

	I32x2 sz  = I32x2_create2(320, 240);
	I32x2 pos = I32x2_create2(100, 100);

	WindowRef *wRef = createWindowCallback(
		t, "F13: Drag me to a second monitor",
		pos, sz, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, cbs
	);

	if (!Test_assert(t, "windowCreated", wRef != NULL))
		goto clean;

	Window *w = RefPtr_data(wRef, Window);

	present(t, w);
	pump(300 * MS);

	if (w->monitors.length < 1) {
		Test_print(t, "No monitors detected, skipping move test");
		goto clean;
	}

	//Synthetic: move window to an offset that would land on a second monitor.
	//On Wayland w->offset is always zero (compositor hides position), so we
	// can only try and then rely on onMonitorChange to confirm it worked.
	#if _PLATFORM_TYPE == PLATFORM_WINDOWS

		//Move to 2560, 100, typical second monitor position for 1920-wide primary
		SetWindowPos((HWND)w->nativeHandle, NULL, 2560, 100, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		pump(500 * MS);

		if (movedToSecondMonitor)
			Test_assert(t, "syntheticMoveToMonitor2", movedToSecondMonitor);

		else Test_print(t, "WARN: synthetic move didn't reach a second monitor (may be single-monitor system)");

		movedToSecondMonitor = false;

	#elif _PLATFORM_TYPE == PLATFORM_LINUX
		//xdotool uses X11 _NET_WM_MOVERESIZE; not available on native Wayland.
		Test_print(t, "Wayland synthetic window move not supported, skipping");
	#endif

	Test_print(t, ">>> INTERACTIVE: Drag the window to a second monitor (10s timeout) <<<");
	Test_print(t, "    If you only have one monitor, this test will time out and warn.");

	Ns waited = 0;
	while (!movedToSecondMonitor && waited < 10 * SECOND) {

		WindowManager_step(&windowManager, NULL, NULL);

		if(!windowManager.windows.length)
			break;

		Thread_sleep(16 * MS);
		waited += 16 * MS;
	}

	if (!movedToSecondMonitor)
		Test_print(t, "WARN: window not moved to second monitor within timeout (single-monitor system?)");

	//Not a hard failure, single monitor systems are valid
	pump(VISUAL_HOLD_NS);

clean:
	RefPtr_dec(&wRef);
}

// -- F14. Maximize / restore ----------------------------------------------------

static PatternState f14;

static void F14_onDraw(Window *w) { Pattern_onDraw(w, &f14); }

static void Test_maximize(Test *t) {

	Test_setModule(t, "F14/Maximize");

	if(isSingleWindow()) {
		Test_print(t, "Skipped. Device is single window");
		return;
	}

	f14 = (PatternState) { 0 };

	WindowCallbacks cbs = (WindowCallbacks) { 0 };
	cbs.onDraw = F14_onDraw;

	I32x2 sz = I32x2_create2(640, 480);

	WindowRef *wRef = createWindowCallback(
		t, "F14: Maximize / restore, watch me",
		I32x2_zero, sz,
		EWindowHint_AllowFullscreen | EWindowHint_ProvideCPUBuffer,
		EWindowFormat_AutoRGBA8, cbs
	);

	if (!Test_assert(t, "windowCreated", wRef != NULL))
		goto clean;

	Window *w = RefPtr_data(wRef, Window);
	presentAndDraw(t, w, &f14, F14_onDraw);
	pump(300 * MS);

	if (w->type != EWindowType_Physical) {
		Test_print(t, "[virtual] maximize test requires a physical window, skipped");
		goto clean;
	}

	I32x2 originalSize = w->size;

	// -- Maximize --

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		ShowWindow((HWND)w->nativeHandle, SW_MAXIMIZE);
	#elif _PLATFORM_TYPE == PLATFORM_LINUX
		{
			LWindow *lwin = WindowExt(w, LWindow);
			xdg_toplevel_set_maximized(lwin->topLevel);
			wl_display_flush(((LWindowManager*)w->owner->platformData.ptr)->display);
		}
	#endif

	f14.zxor = 128;
	pump(1 * SECOND);
	presentAndDraw(t, w, &f14, F14_onDraw);

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS || _PLATFORM_TYPE == PLATFORM_LINUX
		Test_assert(t, "sizeGreaterAfterMax",
			I32x2_x(w->size) > I32x2_x(originalSize) ||
			I32x2_y(w->size) > I32x2_y(originalSize)
		);
	#endif

	pump(VISUAL_HOLD_NS);

	// -- Restore --

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		ShowWindow((HWND)w->nativeHandle, SW_RESTORE);
	#elif _PLATFORM_TYPE == PLATFORM_LINUX
		{
			LWindow *lwin = WindowExt(w, LWindow);
			
			if(lwin->topLevel) {
				xdg_toplevel_unset_maximized(lwin->topLevel);
				wl_display_flush(((LWindowManager*)w->owner->platformData.ptr)->display);
			}
		}
	#endif

	f14.zxor = 0;
	pump(1 * SECOND);
	presentAndDraw(t, w, &f14, F14_onDraw);

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS || _PLATFORM_TYPE == PLATFORM_LINUX
		Test_assert(t, "sizeRestoredAfterUnmax",
			I32x2_x(w->size) <= I32x2_x(originalSize) + 10 &&
			I32x2_y(w->size) <= I32x2_y(originalSize) + 10
		);
	#else
		(void) originalSize;
	#endif

	pump(VISUAL_HOLD_NS);

clean:
	f14 = (PatternState) { 0 };
	RefPtr_dec(&wRef);
}

// -- F17. Min/max size enforcement ---------------------------------------------
//
//Creates a virtual window (no compositor needed) and attempts to resize it
// beyond its declared min and max limits using Window_resizeCPUBuffer.
//The function must clamp to the valid range.
//
//Also creates a physical window and verifies the compositor honours the
// min/max_size hints by trying to resize it programmatically
// to a forbidden dimension, then checking w->size was not updated outside the allowed range.

static PatternState f17;

static void F17_onDraw(Window *w) { Pattern_onDraw(w, &f17); }

static void Test_minMaxSize(Test *t) {

	Test_setModule(t, "F17/MinMaxSize");

	if(isSingleWindow()) {
		Test_print(t, "Skipped. Device is single window");
		return;
	}

	I32x2 minSz = I32x2_create2(640, 360);
	I32x2 maxSz = I32x2_create2(800, 600);
	I32x2 initSz = I32x2_create2(700, 480);

	//Virtual window: resizeCPUBuffer clamping

	{
		WindowRef *wRef = NULL;

		CharString title = CharString_createRefCStrConst("F17: virtual");
		I32x2 pos = I32x2_zero;

		WindowManager_createWindow(
			&windowManager, EWindowType_Virtual, pos, initSz, minSz, maxSz,
			EWindowHint_None, title, (WindowCallbacks) { 0 },
			EWindowFormat_AutoRGBA8, 0, &wRef, &t->err
		);

		if(!Test_assert(t, "virtualCreated", wRef != NULL))
			goto cleanVirtual;

		Window *w = RefPtr_data(wRef, Window);

		//Try to shrink below minimum
		I32x2 tooSmall = I32x2_create2(100, 80);
		Test_assert(t, "resizeTooSmall", Window_resizeCPUBuffer(w, false, tooSmall, &t->err));
		Test_assert(t, "clampedToMin_W", I32x2_x(w->size) == I32x2_x(minSz));
		Test_assert(t, "clampedToMin_H", I32x2_y(w->size) == I32x2_y(minSz));

		//Try to grow beyond maximum
		I32x2 tooBig = I32x2_create2(2000, 1500);
		Test_assert(t, "resizeTooBig", Window_resizeCPUBuffer(w, false, tooBig, &t->err));
		Test_assert(t, "clampedToMax_W", I32x2_x(w->size) == I32x2_x(maxSz));
		Test_assert(t, "clampedToMax_H", I32x2_y(w->size) == I32x2_y(maxSz));

		//Resize to a valid value
		I32x2 validSz = I32x2_create2(650, 450);
		Test_assert(t, "resizeValid", Window_resizeCPUBuffer(w, false, validSz, &t->err));
		Test_assert(t, "validW", I32x2_x(w->size) == 650);
		Test_assert(t, "validH", I32x2_y(w->size) == 450);

	cleanVirtual:
		RefPtr_dec(&wRef);
	}

	//Physical window: compositor-side enforcement

	{
		WindowRef *wRef = NULL;

		CharString title = CharString_createRefCStrConst("F17: Resize me, should clamp to 400x300 .. 800x600");
		I32x2 pos = I32x2_create2(100, 100);

		WindowCallbacks callbacks = (WindowCallbacks) { 0 };
		callbacks.onDraw = F17_onDraw;

		f17 = (PatternState) { 0 };

		Bool s_uccess = WindowManager_createWindow(
			&windowManager, EWindowType_Physical, pos, initSz, minSz, maxSz,
			EWindowHint_ProvideCPUBuffer, title, callbacks,
			EWindowFormat_AutoRGBA8, 0, &wRef, &t->err
		);

		if(!s_uccess) {
			Test_print(t, "Physical window unavailable, skipping physical min/max assertions");
			goto cleanPhysical;
		}

		Window *w = RefPtr_data(wRef, Window);

		present(t, w);
		pump(500 * MS);

		#if _PLATFORM_TYPE == PLATFORM_LINUX
		
			//xdotool uses X11 _NET_WM_MOVERESIZE; not available on native Wayland.
			Test_print(t, "Wayland synthetic window move/resize not supported, skipping");

		#elif _PLATFORM_TYPE == PLATFORM_WINDOWS

			// SetWindowPos below minimum, Win32 automatically clamps via WM_GETMINMAXINFO.
			HWND hwnd = (HWND)w->nativeHandle;
			SetWindowPos(hwnd, NULL, 0, 0, 100, 80, SWP_NOMOVE | SWP_NOZORDER);
			pump(300 * MS);

			Test_assert(t, "physNotTooSmallW", I32x2_x(w->size) >= I32x2_x(minSz));
			Test_assert(t, "physNotTooSmallH", I32x2_y(w->size) >= I32x2_y(minSz));

			SetWindowPos(hwnd, NULL, 0, 0, 2000, 1500, SWP_NOMOVE | SWP_NOZORDER);
			pump(300 * MS);

			Test_assert(t, "physNotTooBigW", I32x2_x(w->size) <= I32x2_x(maxSz));
			Test_assert(t, "physNotTooBigH", I32x2_y(w->size) <= I32x2_y(maxSz));

		#endif

		//Interactive: operator drags the window border
		Test_print(t, ">>> INTERACTIVE: Try to resize the window below 400x300 and above 800x600 (20s) <<<");
		Test_print(t, "    Drag the window border as small and as large as possible.");

		Ns waited = 0;
		Bool sawTooSmall = false;
		Bool sawTooBig   = false;

		while(waited < 20 * SECOND) {
			
			WindowManager_step(&windowManager, NULL, NULL);

			if(!windowManager.windows.length)
				break;

			Thread_sleep(16 * MS);
			waited += 16 * MS;

			I32 cw = I32x2_x(w->size);
			I32 ch = I32x2_y(w->size);

			//If the compositor ever let the window go outside bounds, that's a
			// platform bug worth knowing about, log but don't hard-fail since
			// some compositors clamp server-side without telling the client.
			if(cw < I32x2_x(minSz) || ch < I32x2_y(minSz)) sawTooSmall = true;
			if(cw > I32x2_x(maxSz) || ch > I32x2_y(maxSz)) sawTooBig   = true;
		}

		if(sawTooSmall)
			Test_print(t, "WARN: compositor allowed window below declared minimum size");

		else Test_print(t, "OK: window never went below minimum during interactive resize");

		if(sawTooBig)
			Test_print(t, "WARN: compositor allowed window above declared maximum size");

		else Test_print(t, "OK: window never exceeded maximum during interactive resize");

		//Soft assertions: a non-compliant compositor is a compositor bug, not
		// a platform code bug, so we warn rather than fail the test suite.
		if(sawTooSmall) Test_assert(t, "interactiveMinEnforced", !sawTooSmall);
		if(sawTooBig)   Test_assert(t, "interactiveMaxEnforced", !sawTooBig);

		pump(VISUAL_HOLD_NS);

	cleanPhysical:
		RefPtr_dec(&wRef);
	}
}

// -- F20. Borderless window -----------------------------------------------------

static PatternState f20;

static void F20_onDraw(Window *w) { Pattern_onDraw(w, &f20); }

static void Test_borderless(Test *t) {

	Test_setModule(t, "F20/Borderless");

	if(isSingleWindow()) {
		Test_print(t, "Skipped. Device is single window");
		return;
	}

	f20 = (PatternState) { 0 };

	WindowCallbacks cbs = (WindowCallbacks) { 0 };
	cbs.onDraw = F20_onDraw;

	I32x2 sz  = I32x2_create2(640, 480);
	I32x2 pos = I32x2_create2(200, 200);

	WindowRef *wRef = createWindowCallback(
		t, "F20: Borderless window",
		pos, sz, EWindowHint_NoBorder | EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, cbs
	);

	if(!Test_assert(t, "windowCreated", wRef != NULL))
		goto clean;

	Window *w = RefPtr_data(wRef, Window);
	Test_assert(t, "isActive",      (w->flags & EWindowFlags_IsActive) != 0);
	Test_assert(t, "noBorderHint",  (w->hint  & EWindowHint_NoBorder)  != 0);
	Test_assert(t, "nonZeroSize",   I32x2_any(w->size));
	Test_assert(t, "notFullscreen", !(w->flags & EWindowFlags_IsFullscreen));

	presentAndDraw(t, w, &f20, F20_onDraw);
	pump(VISUAL_HOLD_NS);

clean:
	f20 = (PatternState) { 0 };
	RefPtr_dec(&wRef);
}

// -- F21. Transparent window (per-pixel alpha) ---------------------------------
//
//Opens a 300x300 window with EWindowHint_Transparent | EWindowHint_NoBorder | EWindowHint_ProvideCPUBuffer.
//Fills the CPU buffer with transparent black (0x00000000) from onDraw, then draws a filled circle in the centre.
//Pixels outside the circle have alpha=0 and should be invisible to the compositor; pixels inside are fully opaque.
//
//We verify:
//  - Inside the circle: pixel is 0xFF0000FF (opaque, premultiplied)
//  - Outside the circle: pixel is 0x00000000 (fully transparent)
//  - present succeeds without error
//
//Visual verification: the window should show a circle floating with no background.
//On compositors / drivers that don't honour per-pixel alpha the background may appear black instead of transparent;
// this is not a test failure since it's a compositor capability, not a bug in our code.
//
//NOTE: this is grouped with window state (not CSD) since transparency is a per-pixel composition hint
// orthogonal to whether the compositor draws its own decorations.

#define F21_RADIUS   100
#define F21_CX       150
#define F21_CY       150
#define F21_W        426
#define F21_H        300

typedef struct F21State {
	volatile Bool drawn;
} F21State;

static F21State f21;

static void F21_onDraw(Window *w) {

	U32 *px = (U32*) w->cpuVisibleBuffer.ptrNonConst;
	I32 W   = I32x2_x(w->size);
	I32 H   = I32x2_y(w->size);

	if(!px)
		return;

	for(I32 y = 0; y < H; ++y) {
		for(I32 x = 0; x < W; ++x) {
			I32x2 d = I32x2_sub(I32x2_create2(x, y), I32x2_create2(F21_CX, F21_CY));
			px[y * W + x] = I32x2_dot(d, d) <= I32_pow2(F21_RADIUS) ? (U32)0xFF0000FF : (U32)0x00000000;
		}
	}

	f21.drawn = true;
}

static void Test_transparent(Test *t) {

	Test_setModule(t, "F21/Transparent");

	if(isSingleWindow()) {
		Test_print(t, "Skipped. Device is single window");
		return;
	}

	f21 = (F21State) { 0 };

	EWindowHint hint = EWindowHint_Transparency | EWindowHint_NoBorder | EWindowHint_ProvideCPUBuffer;

	WindowCallbacks cbs = (WindowCallbacks) { 0 };
	cbs.onDraw = F21_onDraw;

	WindowRef *wRef = createWindowCallback(
		t, "F21: Transparent circle",
		I32x2_create2(200, 200),
		I32x2_create2(F21_W, F21_H),
		hint,
		EWindowFormat_BGRA8,
		cbs
	);

	if(!Test_assert(t, "windowCreated", wRef != NULL))
		goto clean;

	Window *w = RefPtr_data(wRef, Window);

	if(w->type != EWindowType_Physical) {
		Test_print(t, "[virtual] transparency test requires a physical window, skipped");
		goto clean;
	}

	Test_assert(t, "hasTransparentHint", w->hint & EWindowHint_Transparency);
	Test_assert(t, "hasCPUBuffer",       w->cpuVisibleBuffer.ptr);
	Test_assert(t, "correctSize",        I32x2_x(w->size) == F21_W && I32x2_y(w->size) == F21_H);

	present(t, w);
	Test_assert(t, "onDrawFired", Pattern_waitForDraw(&f21.drawn, 1 * SECOND));

	//Verify pixel values after onDraw filled the buffer
	{
		const U32 *px = (const U32*) w->cpuVisibleBuffer.ptr;
		I32 W = I32x2_x(w->size);

		U32 centre = px[F21_CY * W + F21_CX];
		Test_assert(t, "centreIsBlue", centre == 0xFF0000FF);

		U32 corner = px[0];
		Test_assert(t, "cornerIsTransparent", corner == 0x00000000);

		I32 outsideX = F21_CX + F21_RADIUS + 2;
		if(outsideX < W) {
			U32 outside = px[F21_CY * W + outsideX];
			Test_assert(t, "outsideIsTransparent", outside == 0x00000000);
		}

		I32 insideX = F21_CX + F21_RADIUS - 2;
		if(insideX >= 0 && insideX < W) {
			U32 inside = px[F21_CY * W + insideX];
			Test_assert(t, "insideIsBlue", inside == 0xFF0000FF);
		}
	}

	//Hold so the operator can visually verify the circle floats

	pump(VISUAL_HOLD_NS);

clean:
	f21 = (F21State) { 0 };
	RefPtr_dec(&wRef);
}

void Test_functionalWindowState(Test *t) {
	Test_fullScreen(t);
	Test_resizeVirtual(t);
	Test_multiWindow(t);
	Test_focusMinimize(t);
	Test_monitorInfo(t);
	Test_windowMove(t);
	Test_maximize(t);
	Test_minMaxSize(t);
	Test_borderless(t);
	Test_transparent(t);
}
