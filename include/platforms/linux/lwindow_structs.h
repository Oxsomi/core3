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

//platforms/linux/lwindow_structs.h

#pragma once
#include "types/base/types.h"
#include <wayland-client.h>
#include <xdg_shell_client_protocol.h>
#include <xdg_decoration_client_protocol.h>

#define LWINDOW_MAX_OUTPUTS  8
#define LWINDOW_BUFFER_COUNT 2

//Height in pixels of the client-side decoration bar.
//Only used when the compositor does not support zxdg_decoration_manager_v1 (e.g. GNOME).
//The bar lives in its own subsurface so it is invisible to the user's rendering path,
//whether they use a CPU buffer or a Vulkan swapchain.
#define LWINDOW_DECOR_HEIGHT 32
#define LWINDOW_DECOR_BTN_W  46    // width of each min/max/close button
#define LWINDOW_DECOR_BTN_H  LWINDOW_DECOR_HEIGHT

typedef struct LWindowManager {

	struct wl_registry_listener  listener;
	struct xdg_wm_base_listener  xdgListener;

	struct wl_display          *display;
	struct wl_registry         *registry;
	struct wl_compositor       *compositor;
	struct wl_shm              *shm;
	struct xdg_wm_base         *xdgWmBase;
	struct zxdg_decoration_manager_v1 *xdgDeco;
	struct wl_subcompositor    *subcompositor;   //for CSD bar subsurface

	U32 compositorId,    shmId;
	U32 xdgWmBaseId,     xdgDecoId;
	U32 subcompositorId, subcompositorPad;

	//Input seat
	struct wl_seat *seat;
	U32             seatId;
	U32             seatPad;

	//Output / monitor tracking
	struct wl_output *outputs[LWINDOW_MAX_OUTPUTS];
	U32               outputIds[LWINDOW_MAX_OUTPUTS];

} LWindowManager;

typedef struct Window Window;

typedef struct LWindow {

	struct xdg_surface_listener  surfaceCallbacks;
	struct xdg_toplevel_listener topLevelCallbacks;

	struct xdg_surface  *surface;
	struct xdg_toplevel *topLevel;

	//Main surface shm pool, only used when EWindowHint_ProvideCPUBuffer is set.
	//When Vulkan owns the swapchain this stays NULL.
	struct wl_shm_pool *backBuffer;
	struct wl_buffer   *buffers[LWINDOW_BUFFER_COUNT];
	Bool                bufferBusy[LWINDOW_BUFFER_COUNT];

	//Frame callback pacing
	struct wl_callback *frameCallback;
	Bool                frameReady;

	U8  *mainBufferPtr;
	U32  pixelStride;
	U16  height;
	U8   heightHi8;
	U8   backBufferId;

	//-1 == no fd
	I32  fileDescriptor;

	//Client-side decoration bar subsurface.
	//Present only when zxdg_decoration_manager_v1 is unavailable (e.g. GNOME).
	//Completely transparent to the user, the main wl_surface they hand to
	//Vulkan / the CPU buffer path is unaffected.

	struct wl_surface    *barSurface;      //NULL when using SSD
	struct wl_subsurface *barSubsurface;
	struct wl_shm_pool   *barPool;
	struct wl_buffer     *barBuffer;
	Bool                  barBufferBusy;
	U8                    pad[7];
	U32                  *barPixels;       //CPU mapped, always writable
	U32                   barWidth;        //track to detect horizontal resize

	//Pointer state for bar hit testing (in bar surface local coordinates)
	I32  pointerX, pointerY;
	Bool pointerInBar;
	U8   pad1[3];

	//Decoration listener (needed to handle compositor downgrade SSD->CSD)
	struct zxdg_toplevel_decoration_v1         *decoration;
	struct zxdg_toplevel_decoration_v1_listener decorationListener;

	Window *parent;

} LWindow;