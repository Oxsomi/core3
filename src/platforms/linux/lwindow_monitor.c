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

//platforms/linux/lwindow_monitor.c

#include "platforms/linux/lwindow_structs.h"
#include "lwindow_internal.h"
#include "platforms/window.h"
#include "platforms/window_manager.h"
#include "platforms/platform.h"
#include "types/container/log.h"

Bool WindowManager_updateMonitorsExt(
	LWindowManager *lmanager,
	ListMonitor *monitors,
	const U32 *activeIds,
	U32 activeCount,
	Error *e_rr
);

void LWindow_updateMonitors(Window *w) {
 
	LWindowManager *manager = (LWindowManager*)w->owner->platformData.ptr;
	LWindow *lwin = WindowExt(w, LWindow);

	const U32 *ids   = lwin->activeOutputCount ? lwin->activeOutputIds : NULL;
	U32        count = lwin->activeOutputCount  ? lwin->activeOutputCount : 0;

	Error err = Error_none();
	if(!WindowManager_updateMonitorsExt(manager, &w->monitors, ids, count, &err)) {
		Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);
		return;
	}

	if(w->callbacks.onMonitorChange)
		w->callbacks.onMonitorChange(w);

	w->owner->monitorsDirty = true;
}

//wl_surface::enter fires when the surface (fully or partially) enters an output
static void LWindow_surfaceEnter(void *data, struct wl_surface *surface, struct wl_output *output) {
	(void) surface;
	Window  *w    = (Window*) data;
	LWindow *lwin = WindowExt(w, LWindow);
 
	//Find the output id from the manager's table
	LWindowManager *manager = (LWindowManager*)w->owner->platformData.ptr;
	U32 id = 0;
 
	for(U32 i = 0; i < LWINDOW_MAX_OUTPUTS; ++i) {
		if(manager->outputs[i] == output) {
			id = manager->outputIds[i];
			break;
		}
	}
 
	if(!id)
		return;
 
	//Add to active set if not already present.
	for(U32 i = 0; i < lwin->activeOutputCount; ++i)
		if(lwin->activeOutputIds[i] == id)
			return;
 
	if(lwin->activeOutputCount < LWINDOW_MAX_OUTPUTS)
		lwin->activeOutputIds[lwin->activeOutputCount++] = id;
 
	LWindow_updateMonitors(w);
}
 
//wl_surface::leave fires when the surface fully leaves an output.
static void LWindow_surfaceLeave(void *data, struct wl_surface *surface, struct wl_output *output) {
	(void) surface;
	Window  *w    = (Window*) data;
	LWindow *lwin = WindowExt(w, LWindow);
 
	LWindowManager *manager = (LWindowManager*)w->owner->platformData.ptr;
	U32 id = 0;
 
	for(U32 i = 0; i < LWINDOW_MAX_OUTPUTS; ++i) {
		if(manager->outputs[i] == output) {
			id = manager->outputIds[i];
			break;
		}
	}
 
	if(!id)
		return;
 
	//Remove from active set (swap with last)
	for(U32 i = 0; i < lwin->activeOutputCount; ++i) {
		if(lwin->activeOutputIds[i] == id) {
			lwin->activeOutputIds[i] = lwin->activeOutputIds[lwin->activeOutputCount - 1];
			--lwin->activeOutputCount;
			LWindow_updateMonitors(w);
			return;
		}
	}
}
 
const struct wl_surface_listener LWindow_surfaceListener = {
	.enter = LWindow_surfaceEnter,
	.leave = LWindow_surfaceLeave,
};
