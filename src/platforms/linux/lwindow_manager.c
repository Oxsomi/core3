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
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "platforms/linux/lwindow_structs.h"
#include "types/container/buffer.h"
#include "types/base/string_read_helper.h"
#include "types/base/error.h"

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
	(void) output; (void) subpixel; (void) make; (void) model;
	LOutputInfo *info = (LOutputInfo*) data;
	info->x         = x;
	info->y         = y;
	info->mmWidth   = mmWidth;
	info->mmHeight  = mmHeight;
	info->transform = transform;
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
 
	LOutputInfo *info    = (LOutputInfo*) data;
	info->pixelWidth  = width;
	info->pixelHeight = height;
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

Bool WindowManager_createNative(WindowManager *w, Error *e_rr) {

	Bool s_uccess = true;
	gotoIfError3(clean, Buffer_createEmptyBytes(sizeof(LWindowManager), Platform_instance->alloc, &w->platformData, e_rr));

	LWindowManager *manager  = (LWindowManager*)w->platformData.ptr;
	manager->display         = wl_display_connect(NULL);
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

clean:
	return s_uccess;
}

Bool WindowManager_freeNative(WindowManager *w) {

	LWindowManager *manager = (LWindowManager*)w->platformData.ptr;

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

	return true;
}

void WindowManager_updateExt(WindowManager *manager) {

	LWindowManager *lmanager = (LWindowManager*)manager->platformData.ptr;

	//Flush outgoing requests before dispatching incoming events.
	wl_display_flush(lmanager->display);
	wl_display_dispatch_pending(lmanager->display);
}
