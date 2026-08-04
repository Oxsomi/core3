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

//platforms/android/awindow_manager.c

#include "platforms/window_manager.h"
#include "platforms/window.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "platforms/monitor.h"
#include "types/base/error.h"
#include "types/base/thread.h"

#include <android_native_app_glue.h>
#include <android/configuration.h>
#include <android/native_window.h>

void *Platform_getDataImpl(void *ptr) { (void) ptr; return (struct android_app*) Platform_instance->data; }

Bool WindowManager_createNative(WindowManager *w, Error *e_rr) {
	(void) e_rr;
	w->isSingleWindow = true;
	return true;
}

Bool WindowManager_freeNative(WindowManager *w) {
	(void) w;
	return true;
}

I32 APlatform_getDeviceOrientation();
F32 APlatform_getRefreshRate();
void AWindow_onUpdateSize(Window *w);
void AWindow_flushTypeChar(Window *w);

//Android shows the app on exactly one display and the NDK has no enumeration API,
// so there's a single monitor to report.
//The surface only exists after APP_CMD_INIT_WINDOW; until then there's nothing to
// measure, and WindowManager_step calls us again once it's up.

Bool WindowManager_updateMonitors(WindowManager *wm, Error *e_rr) {

	Bool s_uccess = true;
	struct android_app *app = (struct android_app*) Platform_instance->data;

	if(!wm)
		retError(clean, Error_nullPointer(0, "WindowManager_updateMonitors()::wm is required"));

	ListMonitor_clear(&wm->monitors, NULL);

	if(!app || !app->window)
		goto clean;

	const I32 width = ANativeWindow_getWidth(app->window);
	const I32 height = ANativeWindow_getHeight(app->window);

	if(width <= 0 || height <= 0)
		goto clean;

	//AConfiguration density is already in dpi (the ACONFIGURATION_DENSITY_* values are dpi buckets),
	// so physical size is px / dpi * 25.4.
	//Fall back to mdpi, android's reference density.

	I32 density = app->config ? (I32) AConfiguration_getDensity(app->config) : 0;

	if(density <= 0 || density == ACONFIGURATION_DENSITY_NONE || density == ACONFIGURATION_DENSITY_ANY)
		density = 160;

	const I32 orientation = APlatform_getDeviceOrientation();

	const Monitor monitor = (Monitor) {
		.offsetPixels = I32x2_zero,
		.sizePixels   = I32x2_create2(width, height),
		.sizeMm       = I32x2_create2(
			(I32)(width * 25.4f / (F32)density), (I32)(height * 25.4f / (F32)density)
		),
		.orientation  = (EMonitorOrientation)(orientation < 0 ? 0 : orientation),
		.refreshRate  = APlatform_getRefreshRate()
	};

	gotoIfError3(clean, ListMonitor_pushBack(&wm->monitors, monitor, Platform_instance->alloc, e_rr));

clean:
	return s_uccess;
}

void WindowManager_updateExt(WindowManager *manager) {

	(void) manager;

	int ident, events;
	struct android_poll_source *source;
	struct android_app *app = (struct android_app*) Platform_instance->data;
	Window *w = (Window*)app->userData;

repeat:
	while((ident = ALooper_pollOnce(0, NULL, &events, (void**)&source)) >= 0) {
		if(source)
			source->process(app, source);
	}

	//Soft keyboard text queued from the UI thread;
	// run it here so onTypeChar sees the app thread like every other callback.
	//Flushed before the !w early out so the queue can't grow without a window.

	AWindow_flushTypeChar(w);

	//WindowManager_step runs even when no physical window was created
	// (userData is only set by WindowManager_createWindowPhysical), so everything below this point is window dependent.

	if(!w)
		return;

	//It's possible our last update was successful but suboptimal.
	//This happens when the device is rotated in landscape mode which won't trigger any config change or resize.
	//In this case, the compositor will happily rotate for us, but we should still resize to ensure optimal performance.

	if(w->requireResize) {

		I32 orientation = APlatform_getDeviceOrientation();
		
		if(orientation >= 0)
			w->orientation = (U16) orientation;

		AWindow_onUpdateSize(w);
	}

	//In case of initialization, we have to wait until the surface is ready.
	//Afterwards, we can continue

	if(ident == ALOOPER_POLL_TIMEOUT && !(w->flags & EWindowFlags_IsFinalized)) {
		Thread_sleep(100 * MU);
		goto repeat;
	}
}
