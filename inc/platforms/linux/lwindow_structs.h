/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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
#pragma once
#include "types/base/types.h"
#include <wayland-client.h>
#include <xdg_shell_client_protocol.h>

typedef struct LWindowManager {

	struct wl_registry_listener listener;
	struct xdg_wm_base_listener xdgListener;

	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct xdg_wm_base *xdgWmBase;

	U32 compositorId, shmId;
	U32 xdgWmBaseId, padding;

} LWindowManager;

typedef struct Window Window;

typedef struct LWindow {
	
	struct xdg_surface_listener surfaceCallbacks;
	struct xdg_toplevel_listener topLevelCallbacks;

	struct xdg_surface *surface;
	struct xdg_toplevel *topLevel;
	struct wl_region *region;

	struct wl_shm_pool *backBuffer;
	struct wl_buffer *buffers[2];	//Double buffering

	U8 *mainBufferPtr;

	U32 pixelStride;
	U16 height;
	U8 heightHi8;
	U8 backBufferId;				//0 or 1

	I32 fileDescriptor;
	U32 padding;

	Window *parent;

} LWindow;
