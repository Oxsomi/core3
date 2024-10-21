/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/window_manager.h"
#include "types/base/error.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "platforms/ext/bufferx.h"
#include "platforms/linux/lwindow_structs.h"

void LWindowManager_isAlive(void *data, struct xdg_wm_base *base, U32 serial) {
	(void) data;
	xdg_wm_base_pong(base, serial);
}

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

	if(CharString_equalsStringSensitive(CharString_createRefCStrConst(wl_compositor_interface.name), inter)) {
		data->compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 4);
		data->compositorId = id;
	}

	else if(CharString_equalsStringSensitive(CharString_createRefCStrConst(wl_shm_interface.name), inter)) {
		data->shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
		data->shmId = id;
	}

	else if(CharString_equalsStringSensitive(CharString_createRefCStrConst(xdg_wm_base_interface.name), inter)) {

		data->xdgWmBase = wl_registry_bind(registry, id, &xdg_wm_base_interface, 1);
		data->xdgWmBaseId = id;

		data->xdgListener = (struct xdg_wm_base_listener) { .ping = LWindowManager_isAlive };
		xdg_wm_base_add_listener(data->xdgWmBase, &data->xdgListener, NULL);
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
}

Bool WindowManager_createNative(WindowManager *w, Error *e_rr) {

	Bool s_uccess = true;
	gotoIfError2(clean, Buffer_createEmptyBytesx(sizeof(LWindowManager), &w->platformData))

	LWindowManager *manager = (LWindowManager*)w->platformData.ptr;
	manager->display = wl_display_connect(NULL);
	manager->compositorId = U64_MAX;

	if(!manager->display)
		retError(clean, Error_invalidState(0, "WindowManager_createNative() couldn't connect to display"))

    manager->registry = wl_display_get_registry(manager->display);

	if(!manager->registry)
		retError(clean, Error_invalidState(0, "WindowManager_createNative() couldn't get registry"))

	manager->listener = (struct wl_registry_listener) {
		.global = LWindowManager_register,
		.global_remove = LWindowManager_unregister
	};

    wl_registry_add_listener(manager->registry, &manager->listener, manager);

    wl_display_dispatch(manager->display);
    wl_display_roundtrip(manager->display);

	if(!manager->compositor || !manager->shm || !manager->xdgWmBase)
		retError(clean, Error_invalidState(0, "WindowManager_createNative() couldn't get compositor, shm or xdg"))

clean:
	return s_uccess;
}

Bool WindowManager_freeNative(WindowManager *w) {

	LWindowManager *manager = (LWindowManager*)w->platformData.ptr;

	if(manager->xdgWmBase)
		xdg_wm_base_destroy(manager->xdgWmBase);

	if (manager->shm)
		wl_shm_destroy(manager->shm);

	if (manager->compositor)
		wl_compositor_destroy(manager->compositor);

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
	wl_display_dispatch_pending(lmanager->display);
}
