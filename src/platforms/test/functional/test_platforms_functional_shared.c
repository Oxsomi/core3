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

//platforms/test/functional/test_platforms_functional_shared.c

#include "test_platforms_functional_shared.h"
#include "platforms/window_manager.h"
#include "types/test/test.h"
#include "types/base/thread.h"

WindowManager windowManager;
Bool windowManagerReady = false;

Bool Test_functionalSetup(Test *t) {

	WindowManagerCallbacks cbs = (WindowManagerCallbacks) { 0 };

	if (!WindowManager_create(cbs, 0, &windowManager, &t->err)) {
		Test_print(t, "WindowManager_create failed, skipping functional tests");
		return false;
	}

	windowManagerReady = true;
	return true;
}

void Test_functionalShutdown() {
	if (windowManagerReady) {
		WindowManager_free(&windowManager);
		windowManagerReady = false;
	}
}

#if _PLATFORM_TYPE == PLATFORM_LINUX
	#include <stdlib.h>
	Bool hasXdotool() {
		return system("which xdotool > /dev/null 2>&1") == 0;
	}
#endif

WindowRef *createWindow(
	Test *t,
	const C8 *titleStr,
	I32x2 size,
	I32x2 pos,
	EWindowHint hint,
	EWindowFormat fmt
) {
	return createWindowCallback(t, titleStr, pos, size, hint, fmt, (WindowCallbacks) { 0 });
}

WindowRef *createWindowCallback(
	Test *t,
	const C8 *titleStr,
	I32x2 pos,
	I32x2 size,
	EWindowHint hint,
	EWindowFormat fmt,
	WindowCallbacks cbs
) {
	WindowRef *wRef = NULL;

	CharString title = CharString_createRefCStrConst(titleStr);
	I32x2 minSize = EResolution_get(EResolution_SD);
	I32x2 maxSize = I32x2_create2(4096, 4096);

	Bool s_uccess = WindowManager_createWindow(
		&windowManager, EWindowType_Physical, pos, size, minSize, maxSize,
		hint, title, cbs, fmt, 0, &wRef, &t->err
	);

	if (!s_uccess) {

		t->err = Error_none();

		s_uccess = WindowManager_createWindow(
			&windowManager, EWindowType_Virtual, pos, size, minSize, maxSize,
			hint, title, cbs, fmt, 0, &wRef, &t->err
		);

		if (s_uccess)
			Test_print(t, "[fallback] using virtual window");
	}

	return wRef;
}

void pump(Ns ns) {

	if (!windowManagerReady)
		return;

	Ns deadline = ns;

	while (deadline > 0) {

		WindowManager_step(&windowManager, NULL, NULL);

		if(!windowManager.windows.length)
			break;

		Ns step = 16 * MS;
		Thread_sleep(step);
		
		if (deadline <= step)
			break;

		deadline -= step;
	}
}

#if _PLATFORM_TYPE == PLATFORM_WINDOWS

	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h>

	//BeginPaint's clip region (inside Window_presentPhysical) is only as good
	// as the window's current update region. CS_HREDRAW/CS_VREDRAW regenerates
	// that on a real size change, but plenty of state changes we test here
	// (fullscreen toggle without a size delta, a test bumping its own pattern
	// color, etc) don't reliably produce one. Explicitly invalidating before
	// every present is what guarantees onDraw -> presentPhysical actually
	// see a non-empty paint rect every time, instead of only "most of the time".
	void invalidateForRepaint(Window *w) {
		InvalidateRect((HWND) w->nativeHandle, NULL, FALSE);
	}

#else
	void presentQuiet(Window *w) { (void) w; }
#endif

void presentQuiet(Window *w) {

	if(w->type != EWindowType_Physical)
		return;

	invalidateForRepaint(w);
	pump(32 * MS);

	Window_presentPhysical(w, NULL);
}

void present(Test *t, Window *w) {

	if(w->type != EWindowType_Physical)
		return;

	//Ask the OS for a fresh paint cycle so onDraw runs with the test's latest state.
	//We don't write pixels here ourselves; onDraw owns that.
	invalidateForRepaint(w);
	pump(32 * MS);

	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
}
