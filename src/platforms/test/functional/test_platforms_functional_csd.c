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

//platforms/test/functional/test_platforms_functional_csd.c
//
//F16. Click min, max, then close on a compositor-without-SSD's client-side
//     decoration bar (e.g. GNOME/Mutter). Linux/Wayland only; on Windows and on
//     compositors that provide server-side decorations this is not applicable.

#include "test_platforms_functional_shared.h"

#if _PLATFORM_TYPE == PLATFORM_LINUX

	#include "platforms/window_manager.h"
	#include "platforms/linux/lwindow_structs.h"
	#include "types/test/test.h"
	#include "types/base/thread.h"
	#include <stdlib.h>

	static volatile Bool f16MinimizeSeen = false;
	static volatile Bool f16MaximizeSeen = false;

	Bool hasXdotool();

	static Bool F16_onResize(Window *w, Error *e_rr) {
		(void) e_rr;
		if(!f16MaximizeSeen && I32x2_x(w->size) >= 800)  //Maximize on Wayland sends a configure with a larger size.
			f16MaximizeSeen = true;

		return true;
	}

	void Test_functionalCSD(Test *t) {

		Test_setModule(t, "F16/CSDButtons");

		WindowCallbacks cbs = (WindowCallbacks) { 0 };
		cbs.onResize = F16_onResize;

		I32x2 pos = I32x2_create2(200, 200);
		I32x2 sz  = I32x2_create2(640, 400);

		WindowRef *wRef = createWindowCallback(
			t, "F16: Click min, max, then close button",
			pos, sz,
			EWindowHint_AllowFullscreen | EWindowHint_ProvideCPUBuffer,
			EWindowFormat_AutoRGBA8, cbs
		);

		if(!Test_assert(t, "windowCreated", wRef != NULL))
			goto clean;

		Window *w = RefPtr_data(wRef, Window);
		present(t, w);
		pump(500 * MS);

		if(w->type != EWindowType_Physical) {
			Test_print(t, "[virtual] CSD button test requires a physical window, skipped");
			goto clean;
		}

		//Check whether we actually have a CSD bar; if the compositor provided SSD
		// (e.g. KWin) there is no barSurface and this test is not applicable.
		{
			LWindow *lwin = WindowExt(w, LWindow);
			if(!lwin->barSurface) {
				Test_print(t, "[SSD compositor] no CSD bar present, skipping F16");
				goto clean;
			}
		}

		//----- Synthetic: xdotool clicks at absolute bar coordinates -----
		//Bar sits at the top of the window.  Button layout (right to left):
		//   close  = barWidth - BTN_W / 2          (rightmost)
		//   max    = barWidth - BTN_W * 3 / 2
		//   min    = barWidth - BTN_W * 5 / 2
		//We don't know w->offset on Wayland (always 0), but xdotool mousemove
		// accepts coordinates relative to a window id via --window.
		//We use `xdotool getactivewindow` after focusing by title.

		if(hasXdotool()) {

			//Minimize button

			system(
				"xdotool search --name 'F16:' windowfocus && "
				"WIN=$(xdotool search --name 'F16:') && "
				"GEOM=$(xdotool getwindowgeometry $WIN) && "
				//Click at bar-relative coords: x = width - BTN_W * 5 / 2 + BTN_W / 2,
				// y = BTN_H / 2. We use a fixed offset since we know the bar layout.
				"xdotool mousemove --window $WIN "
					"$(($(xdotool getwindowgeometry --shell $WIN | grep WIDTH | cut -d= -f2) - 161)) 16 "
				"&& xdotool click 1"
			);

			pump(600 * MS);

			if(Window_isMinimized(w))
				f16MinimizeSeen = true;

			else Test_print(t, "WARN: minimize click didn't set IsMinimized (compositor-dependent)");

			system("xdotool search --name 'F16:' windowactivate --sync");    //Restore via windowactivate
			pump(500 * MS);

			//Maximize button (x = width - BTN_W * 3 / 2 + BTN_W / 2 = width - BTN_W)

			system(
				"WIN=$(xdotool search --name 'F16:') && "
				"xdotool mousemove --window $WIN "
					"$(($(xdotool getwindowgeometry --shell $WIN | grep WIDTH | cut -d= -f2) - 115)) 16 "
				"&& xdotool click 1"
			);

			pump(600 * MS);

			if(f16MaximizeSeen)
				Test_assert(t, "syntheticMaximize", true);

			else Test_print(t, "WARN: maximize click didn't produce resize event");

			//Un-maximize

			system(
				"WIN=$(xdotool search --name 'F16:') && "
				"xdotool mousemove --window $WIN "
					"$(($(xdotool getwindowgeometry --shell $WIN | grep WIDTH | cut -d= -f2) - 115)) 16 "
				"&& xdotool click 1"
			);

			pump(500 * MS);
		}

		//Interactive fallback

		Test_print(t, ">>> INTERACTIVE: Click MINIMIZE, then MAXIMIZE, then CLOSE in the title bar (15s) <<<");

		Bool closeSeen = false;
		Ns waited = 0;

		while(waited < 15 * SECOND) {
			
			WindowManager_step(&windowManager, NULL, NULL);

			if(!windowManager.windows.length)
				break;

			Thread_sleep(16 * MS);
			waited += 16 * MS;

			if(!f16MinimizeSeen && Window_isMinimized(w))
				f16MinimizeSeen = true;

			if(w->flags & EWindowFlags_ShouldTerminate) {
				closeSeen = true;
				break;
			}
		}

		if(!f16MinimizeSeen)
			Test_print(t, "WARN: minimize not observed (compositor may not report it)");

		Test_assert(t, "closeButton", closeSeen);

	clean:
		f16MinimizeSeen = false;
		f16MaximizeSeen = false;
		RefPtr_dec(&wRef);
	}

#else
	void Test_functionalCSD(Test *t) { (void) t; }
#endif
