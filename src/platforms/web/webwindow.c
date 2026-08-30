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

//platforms/web/webwindow.c
//
//Headless windowing for the web target: WindowManager_createNative returns false,
// which generic/window_manager.c treats as a supported headless configuration ("Physical windows will be unavailable").
//Everything below is the minimum surface to link.
//A real browser front-end (canvas + emscripten_set_main_loop + html5.h input) can replace this file's behavior later,
// without touching the generic layer.

#include "platforms/platform.h"
#include "platforms/window_manager.h"
#include "platforms/window.h"
#include "platforms/monitor.h"
#include "types/base/error.h"

U32 Window_extSize = 0; //No per-window platform payload headless

Bool WindowManager_createNative(WindowManager *w, Error *e_rr) {
	(void) w; (void) e_rr;
	return false; //Headless: no native windowing (yet) on web
}

Bool WindowManager_freeNative(WindowManager *w) {
	(void) w;
	return true;
}

Bool WindowManager_updateMonitors(WindowManager *wm, Error *e_rr) {
	(void) wm; (void) e_rr;
	return true; //No monitors headless
}

void WindowManager_updateExt(WindowManager *manager) {
	(void) manager;
}

void WindowManager_freePhysical(Window *w) {
	(void) w;
}

Bool WindowManager_createWindowPhysical(Window *w, Error *e_rr) {
	(void) w;
	Bool s_uccess = true;
	retError(clean, Error_unsupportedOperation(0, "WindowManager_createWindowPhysical() no windowing on web yet"));
clean:
	return s_uccess;
}

Bool WindowManager_supportsFormat(const WindowManager *manager, EWindowFormat format) {
	(void) manager; (void) format;
	return false;
}

Bool Window_updatePhysicalTitle(Window *w, CharString title, Error *e_rr) {
	(void) w; (void) title;
	Bool s_uccess = true;
	retError(clean, Error_unsupportedOperation(0, "Window_updatePhysicalTitle() no windowing on web yet"));
clean:
	return s_uccess;
}

Bool Window_toggleFullScreen(Window *w, Error *e_rr) {
	(void) w;
	Bool s_uccess = true;
	retError(clean, Error_unsupportedOperation(0, "Window_toggleFullScreen() no windowing on web yet"));
clean:
	return s_uccess;
}

Bool Window_presentPhysical(Window *w, Error *e_rr) {
	(void) w;
	Bool s_uccess = true;
	retError(clean, Error_unsupportedOperation(0, "Window_presentPhysical() no windowing on web yet"));
clean:
	return s_uccess;
}
