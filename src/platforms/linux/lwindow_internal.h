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

//platforms/linux/lwindow_internal.h

#pragma once
#include "platforms/linux/lwindow_structs.h"
#include "types/base/error.h"

//Symbols shared between the lwindow*.c translation units only.

//lwindow.c; creates an anonymous shm fd of the requested size.
Bool LWindow_openShmFd(U64 size, I32 *fdOut, Error *e_rr);

//lwindow_decor.c; client-side decoration bar drawing and (re)allocation.
void LWindow_redrawBar(Window *w);
Bool LWindow_initBar(Window *w, U32 width, Error *e_rr);

//lwindow_input.c; seat listeners wired up by WindowManager_createWindowPhysical.
extern const struct wl_keyboard_listener LWindow_kbListener;
extern const struct wl_pointer_listener LWindow_barPointerListener;
extern const struct wl_touch_listener LWindow_touchListener;

//lwindow_monitor.c; tracks which outputs the window is currently on.
extern const struct wl_surface_listener LWindow_surfaceListener;
void LWindow_updateMonitors(Window *w);
