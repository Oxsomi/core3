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

//platforms/linux/lwindow.c

#include "platforms/linux/lwindow_structs.h"
#include "lwindow_internal.h"
#include "platforms/window.h"
#include "platforms/window_manager.h"
#include "platforms/platform.h"
#include "platforms/keyboard.h"
#include "platforms/logx.h"
#include "platforms/mouse.h"
#include "types/container/log.h"
#include "types/container/buffer.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

U32 Window_extSize = sizeof(LWindow);

Bool LWindow_openShmFd(U64 size, I32 *fdOut, Error *e_rr) {

	Bool s_uccess = true;

	C8 base[]       = "/wl_shm";
	C8 randomName[] = "/wl_shm-XXXXXXXXXXX";
	U64 rand = 0;

	if(!Buffer_csprng(Buffer_createRef(&rand, sizeof(rand))))
		retError(clean, Error_invalidState(0, "LWindow_openShmFd() can't generate random name"));

	U64 randOg = rand;
	I32 fd     = -1;

	for(U64 j = 0; j < 4; ++j) {

		for(U8 i = 0; i < 11; ++i) {
			randomName[sizeof(base) + i] = C8_createNyto(rand & 0x3F);
			rand >>= 6;
		}

		fd = shm_open(randomName, O_RDWR | O_CREAT | O_EXCL, 0600);

		if(fd >= 0) {
			shm_unlink(randomName);
			break;
		}

		if(errno != EEXIST)
			retError(clean, Error_stderr(errno, "LWindow_openShmFd() shm_open failed"));

		rand = randOg + j + 1;
	}

	if(fd < 0)
		retError(clean, Error_invalidState(0, "LWindow_openShmFd() couldn't find a free shm name"));

	while(ftruncate(fd, (off_t)size) < 0) {
		if(errno != EINTR) {
			close(fd);
			retError(clean, Error_stderr(errno, "LWindow_openShmFd() ftruncate failed"));
		}
	}

	*fdOut = fd;

clean:
	return s_uccess;
}

static void LWindow_bufferRelease(void *data, struct wl_buffer *buf) {
	LWindow *lwin = (LWindow*) data;
	U64 n = sizeof(lwin->buffers) / sizeof(lwin->buffers[0]);

	for(U64 i = 0; i < n; ++i) {
		if(lwin->buffers[i] == buf) {
			lwin->bufferBusy[i] = false;
			break;
		}
	}
}

static const struct wl_buffer_listener LWindow_bufferListener = {
	.release = LWindow_bufferRelease
};

static void LWindow_frameCallback(void *data, struct wl_callback *cb, U32 time) {
	(void) time;
	Window  *w    = (Window*) data;
	LWindow *lwin = WindowExt(w, LWindow);

	wl_callback_destroy(cb);
	lwin->frameCallback = NULL;
	lwin->frameReady    = true;
}

static const struct wl_callback_listener LWindow_frameListener = {
	.done = LWindow_frameCallback
};

Bool LWindow_initSize(Window *w, I32x2 size, Error *e_rr) {

	Bool s_uccess    = true;
	struct wl_shm_pool *pool = NULL;
	I32  fd          = -1;
	U64  buffersCreated = 0;

	LWindow *lwin = WindowExt(w, LWindow);
	I32 totalWidth  = I32x2_x(size);
	I32 totalHeight = I32x2_y(size);

	//Resize the bar whenever the window width changes.
	gotoIfError3(clean, LWindow_initBar(w, (U32)totalWidth, e_rr));

	//Tell the compositor the full visible extent of the window (including bar).
	//Used for snap, shadows, and maximize geometry, must cover the whole surface.
	//The CSD bar subsurface sits LWINDOW_DECOR_HEIGHT px *above* the content
	// surface's origin (see wl_subsurface_set_position at creation), so the
	// geometry's y-origin and height must include that region or the compositor's
	// maximize / fullscreen-restore / interactive-resize anchoring will be off by
	// the bar height (and drift further with every subsequent resize configure).

	Bool hasBar = lwin->barSurface && !(w->flags & EWindowFlags_IsFullscreen);
	I32  geomY  = hasBar ? -(I32)LWINDOW_DECOR_HEIGHT : 0;
	I32  geomH  = totalHeight + (hasBar ? (I32)LWINDOW_DECOR_HEIGHT : 0);

	xdg_surface_set_window_geometry(lwin->surface, 0, geomY, totalWidth, geomH);

	if(w->hint & EWindowHint_ProvideCPUBuffer) {

		struct wl_shm *shm = ((LWindowManager*)w->owner->platformData.ptr)->shm;

		U32 width  = (U32) totalWidth;
		U32 height = (U32) totalHeight;

		U64 stride   = (U64)width * 4;
		U64 poolSize = stride * height * LWINDOW_BUFFER_COUNT;

		if((width >> 24) || (height >> 24))
			retError(clean, Error_invalidState(0, "LWindow_initSize() max window size of 16777215"));

		gotoIfError3(clean, LWindow_openShmFd(poolSize, &fd, e_rr));

		//Tear down old resources.
		U64 nbuffers = LWINDOW_BUFFER_COUNT;

		for(U64 i = 0; i < nbuffers; ++i) {
			if(lwin->buffers[i]) {
				wl_buffer_destroy(lwin->buffers[i]);
				lwin->buffers[i]    = NULL;
				lwin->bufferBusy[i] = false;
			}
		}

		if(lwin->mainBufferPtr) {
			U32 oldH       = lwin->height | ((U32) lwin->heightHi8 << 16);
			U64 oldPoolSz  = (U64) lwin->pixelStride * oldH * nbuffers;
			munmap(lwin->mainBufferPtr, oldPoolSz);
			lwin->mainBufferPtr = NULL;
		}

		if(lwin->backBuffer) {
			wl_shm_pool_destroy(lwin->backBuffer);
			lwin->backBuffer = NULL;
		}

		if(lwin->fileDescriptor >= 0) {
			close(lwin->fileDescriptor);
			lwin->fileDescriptor = -1;
		}

		lwin->fileDescriptor = fd;
		fd = -1;

		lwin->pixelStride = (U32)(width * 4);
		lwin->height      = (U16) height;
		lwin->heightHi8   = (U8)(height >> 16);

		pool = wl_shm_create_pool(shm, lwin->fileDescriptor, (I32)poolSize);

		if(!pool)
			retError(clean, Error_invalidState(0, "LWindow_initSize() wl_shm_create_pool failed"));

		lwin->backBuffer = pool;
		pool = NULL;

		for(U64 i = 0; i < nbuffers; ++i) {
			lwin->buffers[i] = wl_shm_pool_create_buffer(
				lwin->backBuffer,
				(I32)(stride * height * i),
				(I32)width, (I32)height, (I32)stride,
				w->hint & EWindowHint_Transparency ? WL_SHM_FORMAT_ARGB8888 : WL_SHM_FORMAT_XRGB8888
			);

			if(!lwin->buffers[i])
				retError(clean, Error_invalidState(0, "LWindow_initSize() wl_shm_pool_create_buffer failed"));

			wl_buffer_add_listener(lwin->buffers[i], &LWindow_bufferListener, lwin);
			++buffersCreated;
		}

		lwin->mainBufferPtr = mmap(NULL, poolSize, PROT_READ | PROT_WRITE, MAP_SHARED, lwin->fileDescriptor, 0);

		if(lwin->mainBufferPtr == MAP_FAILED) {
			lwin->mainBufferPtr = NULL;
			retError(clean, Error_stderr(errno, "LWindow_initSize() mmap failed"));
		}

		lwin->backBufferId  = 0;
		lwin->bufferBusy[0] = false;
		lwin->bufferBusy[1] = false;

		//cpuVisibleBuffer points to the content area; user sees nothing of the bar.
		w->cpuVisibleBuffer.ptr              = lwin->mainBufferPtr;
		w->cpuVisibleBuffer.lengthAndRefBits = (stride * height) | ((U64)1 << 63);
	}

	w->size = I32x2_create2(totalWidth, totalHeight);

clean:
	if(!s_uccess) {

		LWindow *l2 = WindowExt(w, LWindow);
		for(U64 i = 0; i < buffersCreated; ++i) {
			if(l2->buffers[i]) {
				wl_buffer_destroy(l2->buffers[i]);
				l2->buffers[i] = NULL;
			}
		}

		if(pool) wl_shm_pool_destroy(pool);
		if(fd >= 0) close(fd);
	}

	return s_uccess;
}

static void LWindow_updateSize(
	void *data,
	struct xdg_toplevel *xdg_toplevel,
	I32 width,
	I32 height,
	struct wl_array *states
) {
	(void) xdg_toplevel;

	Window *w = (Window*) data;
	LWindow *lwin = WindowExt(w, LWindow);

	//Parse the states array to track minimized and suspended flags,
	// matching the EWindowFlags_IsMinimized gate on Windows (WM_SIZE/SIZE_MINIMIZED)
	Bool isMinimized  = false;
	Bool isSuspended  = false;
	Bool isMaximized  = false;
	Bool isFullscreen = false;

	U32 *s = (U32*) states->data;
	U32  n = (U32)(states->size / sizeof(U32));

	for(U32 i = 0; i < n; ++i) {
		switch(s[i]) {
			case XDG_TOPLEVEL_STATE_MAXIMIZED:  isMaximized  = true; break;
			case XDG_TOPLEVEL_STATE_FULLSCREEN: isFullscreen = true; break;
			case XDG_TOPLEVEL_STATE_SUSPENDED:  isSuspended  = true; break;

			//There is no XDG_TOPLEVEL_STATE_MINIMIZED in the core protocol;
			// minimization is inferred from a 0x0 configure with no other states
			default: break;
		}
	}

	//Track whether compositor ever gave us a real size

	if(width > 0 && height > 0)
		lwin->hadCompositorSize = true;

	//Only infer minimized from 0x0 if compositor previously told us a real size.
	//Nested compositors like gamescope send 0x0 repeatedly meaning "deferred to client",
	// not "minimized".

	if(!width && !height && !isMaximized && !isFullscreen && !isSuspended && lwin->configured && lwin->hadCompositorSize)
		isMinimized = true;

	Bool prevMinimized = w->flags & EWindowFlags_IsMinimized;

	if(isMinimized) w->flags |=  EWindowFlags_IsMinimized;
	else            w->flags &= ~EWindowFlags_IsMinimized;

	if(isMaximized) w->flags |=  EWindowFlags_IsMaximized;
	else            w->flags &= ~EWindowFlags_IsMaximized;

	//Sync fullscreen flag with compositor state
	if(isFullscreen) w->flags |=  EWindowFlags_IsFullscreen;
	else             w->flags &= ~EWindowFlags_IsFullscreen;

	//Skip geometry update while minimized or suspended, matching the
	// EWindowHint_AllowBackgroundUpdates gate on Windows

	Bool skip = (isMinimized || isSuspended) && !(w->hint & EWindowHint_AllowBackgroundUpdates);

	Bool stateChanged = isMinimized != prevMinimized || !lwin->configured;

	if(skip) {
		//Still notify if the minimized state changed.
		if(stateChanged && w->callbacks.onResize && (w->flags & EWindowFlags_IsFinalized)) {
			Error err = Error_none();
			if(!w->callbacks.onResize(w, &err))
				Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);
		}
		return;
	}

	//0x0 = compositor defers to client

	lwin->configured = true;

	if(height >= (I32)LWINDOW_DECOR_HEIGHT && lwin->barSurface && !isFullscreen)
		height -= LWINDOW_DECOR_HEIGHT;

	if(width  <= 0) width  = I32x2_x(w->size) ? I32x2_x(w->size) : 1280;
	if(height <= 0) height = I32x2_y(w->size) ? I32x2_y(w->size) : 720;

	I32x2 newContentSize = I32x2_create2(width, height);

	//xdg_toplevel maximize/tiling are allowed by protocol to exceed min/max size hints.
	//Clamp here so CSD maximize matches the Windows backend (which clamps via WM_GETMINMAXINFO).
	//Fullscreen is intentionally exempt.
	if(!isFullscreen && !w->owner->isSingleWindow)
		newContentSize = I32x2_clamp(newContentSize, w->minSize, w->maxSize);

	if(I32x2_eq2(w->size, newContentSize) && !stateChanged)
		return;

	Error err = Error_none();
	if(!LWindow_initSize(w, newContentSize, &err))
		Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);

	else lwin->frameReady = true;

	LWindow_updateMonitors(w);

	if(w->callbacks.onResize && (w->flags & EWindowFlags_IsFinalized)) {
		if(!w->callbacks.onResize(w, &err))
			Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);
	}
}

static void LWindow_close(void *data, struct xdg_toplevel *xdg_toplevel) {
	(void) xdg_toplevel;
	((Window*)data)->flags |= EWindowFlags_ShouldTerminate;
}

Bool WindowManager_supportsFormat(const WindowManager *manager, EWindowFormat format) {
	(void) manager;
	return format == EWindowFormat_BGRA8;   //TODO: HDR
}

void WindowManager_freePhysical(Window *w) {

	LWindow *lwin = WindowExt(w, LWindow);

	if(!lwin)
		return;

	//Drain pending events so in-flight callbacks (e.g. frame callback) complete
	// and dereference lwin before we free it.
	LWindowManager *manager = (LWindowManager*)w->owner->platformData.ptr;
	if(w->nativeHandle && manager && manager->display)
		wl_display_roundtrip(manager->display);

	if(lwin->keyboard)      wl_keyboard_destroy(lwin->keyboard);
	if(lwin->touch)         wl_touch_destroy(lwin->touch);
	if(lwin->barPointer)    wl_pointer_destroy(lwin->barPointer);
	if(lwin->barBuffer)     wl_buffer_destroy(lwin->barBuffer);
	if(lwin->barPixels)     munmap(lwin->barPixels, (U64)lwin->barWidth * LWINDOW_DECOR_HEIGHT * 4);
	if(lwin->barPool)       wl_shm_pool_destroy(lwin->barPool);
	if(lwin->barSubsurface) wl_subsurface_destroy(lwin->barSubsurface);
	if(lwin->barSurface)    wl_surface_destroy(lwin->barSurface);

	for(U64 i = 0; i < LWINDOW_BUFFER_COUNT; ++i)
		if(lwin->buffers[i])
			wl_buffer_destroy(lwin->buffers[i]);

	if(lwin->xkbState)      xkb_state_unref(lwin->xkbState);
	if(lwin->xkbContext)    xkb_context_unref(lwin->xkbContext);
	if(lwin->frameCallback) wl_callback_destroy(lwin->frameCallback);
	if(lwin->topLevel)      xdg_toplevel_destroy(lwin->topLevel);
	if(lwin->surface)       xdg_surface_destroy(lwin->surface);
	if(lwin->backBuffer)    wl_shm_pool_destroy(lwin->backBuffer);

	if(lwin->mainBufferPtr) {
		U32 oldH      = lwin->height | ((U32) lwin->heightHi8 << 16);
		U64 oldPoolSz = (U64) lwin->pixelStride * oldH * LWINDOW_BUFFER_COUNT;
		munmap(lwin->mainBufferPtr, oldPoolSz);
	}

	if(w->nativeHandle)           wl_surface_destroy((struct wl_surface*) w->nativeHandle);
	if(lwin->fileDescriptor >= 0) close(lwin->fileDescriptor);

	w->nativeHandle = NULL;

	*lwin = (LWindow) { 0 };
}

Bool Window_updatePhysicalTitle(Window *w, CharString title, Error *e_rr) {

	Bool s_uccess = true;
	CharString copy = CharString_createNull();

	if(!w || !I32x2_any(w->size) || !title.ptr || !CharString_length(title) || w->type != EWindowType_Physical)
		retError(clean, Error_nullPointer(
			!w || !I32x2_any(w->size) ? 0 : 1, "Window_updatePhysicalTitle()::w and title are required"
		));

	if(!(w->flags & EWindowFlags_IsActive)) {
		Log_warnLnx("Window_updatePhysicalTitle()::w triggered on inactive window. Ignored");
		goto clean;
	}

	gotoIfError3(clean, CharString_createCopy(title, Platform_instance->alloc, &copy, e_rr));

	LWindow *lwin = WindowExt(w, LWindow);
	struct wl_surface *surface = (struct wl_surface*) w->nativeHandle;

	CharString_free(&w->title, Platform_instance->alloc);
	w->title = copy;
	copy = CharString_createNull();

	xdg_toplevel_set_title(lwin->topLevel, w->title.ptr);
	xdg_toplevel_set_app_id(lwin->topLevel, w->title.ptr);

	wl_surface_commit(surface);

	LWindow_redrawBar((Window*)(uintptr_t)w);     //Redraw CSD bar with updated title if present

clean:
	CharString_free(&copy, Platform_instance->alloc);
	return s_uccess;
}

Bool Window_toggleFullScreen(Window *w, Error *e_rr) {

	Bool s_uccess = true;

	if(!w || !I32x2_any(w->size) || w->type != EWindowType_Physical)
		retError(clean, Error_nullPointer(!w || !I32x2_any(w->size) ? 0 : 1, "Window_toggleFullScreen()::w is required"));

	if(!(w->hint & EWindowHint_AllowFullscreen))
		retError(clean, Error_unsupportedOperation(
			0, "Window_toggleFullScreen() isn't allowed if EWindowHint_AllowFullscreen is off"
		));

	if(!(w->flags & EWindowFlags_IsActive)) {
		Log_warnLnx("Window_updatePhysicalTitle()::w triggered on inactive window. Ignored");
		goto clean;
	}

	LWindow *lwin = WindowExt(w, LWindow);

	//Compositor will send a configure with the pre-fullscreen size.
	// w->prevSize is not needed on Wayland; the compositor restores geometry

	if(w->flags & EWindowFlags_IsFullscreen) {

		w->flags &= ~EWindowFlags_IsFullscreen;
		xdg_toplevel_unset_fullscreen(lwin->topLevel);

		if(lwin->barSurface)
			wl_subsurface_set_desync(lwin->barSubsurface);

	} else {

		w->prevSize = w->size;   //Kept for cross-platform consistency; not used to restore
		w->flags |= EWindowFlags_IsFullscreen;
		xdg_toplevel_set_fullscreen(lwin->topLevel, NULL);

		if(lwin->barSurface)
			wl_subsurface_set_sync(lwin->barSubsurface);
	}

clean:
	return s_uccess;
}

Bool Window_presentPhysical(Window *w, Error *e_rr) {

	Bool s_uccess = true;

	if(!w || !I32x2_any(w->size))
		retError(clean, Error_nullPointer(0, "Window_presentPhysical()::w is required"));

	if(!(w->flags & EWindowFlags_IsActive) || !(w->hint & EWindowHint_ProvideCPUBuffer))
		retError(clean, Error_invalidOperation(
			0, "Window_presentPhysical() can only be called if there's a CPU-sided buffer"
		));

	LWindow *lwin = WindowExt(w, LWindow);

	if(!lwin->frameReady)
		goto clean;

	lwin->frameReady = false;

	//Find a free buffer
	U64 chosen = U64_MAX;
	for(U64 i = 0; i < LWINDOW_BUFFER_COUNT; ++i) {
		U64 candidate = ((U64)lwin->backBufferId + i) % LWINDOW_BUFFER_COUNT;
		if(!lwin->bufferBusy[candidate]) {
			chosen = candidate;
			break;
		}
	}

	if(chosen == U64_MAX)
		retError(clean, Error_invalidState(0, "Window_presentPhysical() all buffers held by compositor"));

	struct wl_surface *surface = (struct wl_surface*) w->nativeHandle;

	lwin->bufferBusy[chosen] = true;

	U32 height = lwin->height | ((U32) lwin->heightHi8 << 16);
	U64 stride = (U64) lwin->pixelStride * height;

	//Force Gamescope to treat this as a new buffer (see ValveSoftware/gamescope#1749:
	// wl_shm buffers aren't re-uploaded on repeat commits of the same buffer object).
	if(w->owner->isSingleWindow) {

		if(lwin->buffers[chosen])
			wl_buffer_destroy(lwin->buffers[chosen]);

		lwin->buffers[chosen] = wl_shm_pool_create_buffer(
			lwin->backBuffer,
			(I32)(stride * chosen),
			(I32)lwin->pixelStride / 4, (I32)height, (I32)lwin->pixelStride,
			w->hint & EWindowHint_Transparency ? WL_SHM_FORMAT_ARGB8888 : WL_SHM_FORMAT_XRGB8888
		);

		if(!lwin->buffers[chosen])
			retError(clean, Error_invalidState(0, "Window_presentPhysical() failed to create buffer!"));

		wl_buffer_add_listener(lwin->buffers[chosen], &LWindow_bufferListener, lwin);
	}

	wl_surface_attach(surface, lwin->buffers[chosen], 0, 0);
	wl_surface_damage_buffer(surface, 0, 0, I32_MAX, I32_MAX);

	if(lwin->frameCallback) {
		wl_callback_destroy(lwin->frameCallback);
		lwin->frameCallback = NULL;
	}

	lwin->frameCallback = wl_surface_frame(surface);
	wl_callback_add_listener(lwin->frameCallback, &LWindow_frameListener, w);

	wl_surface_commit(surface);

	lwin->backBufferId      = (U8)((chosen + 1) % LWINDOW_BUFFER_COUNT);

	w->cpuVisibleBuffer.ptr = lwin->mainBufferPtr + stride * lwin->backBufferId;

clean:
	return s_uccess;
}

static void LWindow_confirmExists(void *data, struct xdg_surface *surface, U32 serial) {
	(void) data;
	xdg_surface_ack_configure(surface, serial);
}

Bool WindowManager_createWindowPhysical(Window *w, Error *e_rr) {

	Bool s_uccess = true;
	Bool isActive = false;
	Keyboard builtinKeyboard = (Keyboard) { 0 };
	Mouse    builtinMouse    = (Mouse)    { 0 };

	I32x2 defSize = I32x2_create2(1280, 720);
	I32x2 size    = w->size;

	for(U8 i = 0; i < 2; ++i)
		if(!I32x2_get(size, i))
			I32x2_setRef(&size, i, I32x2_get(defSize, i));

	if(w->owner->isSingleWindow && w->owner->monitors.length == 1 && I32x2_all(w->owner->monitors.ptr[0].sizePixels))
		size = w->owner->monitors.ptr[0].sizePixels;

	w->size = size;

	LWindowManager       *manager    = (LWindowManager*)w->owner->platformData.ptr;
	struct wl_compositor *compositor = manager->compositor;

	//Main surface, the handle passed to Vulkan / vkCreateWaylandSurfaceKHR.
	// Note: Wayland does not expose the window's position in global compositor
	// coordinates to clients (by design, for security). w->offset is always zero
	// on this platform. Use w->monitors[i].offsetPixels for output origins, which
	// is sufficient for subpixel rendering decisions.
	struct wl_surface *surface = wl_compositor_create_surface(compositor);

	if(!surface)
		retError(clean, Error_invalidState(0, "WindowManager_createWindowPhysical() create surface failed"));

	wl_surface_set_user_data(surface, w);
	w->nativeHandle = surface;

	wl_surface_add_listener(surface, &LWindow_surfaceListener, w);

	//Allocate LWindow
	LWindow *lwin = WindowExt(w, LWindow);
	lwin->fileDescriptor = -1;
	lwin->frameReady     = true;

	//xkb context for keyboard input
	lwin->xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if(!lwin->xkbContext)
		retError(clean, Error_invalidState(0, "WindowManager_createWindowPhysical() xkb_context_new failed"));

	lwin->surface = xdg_wm_base_get_xdg_surface(manager->xdgWmBase, surface);

	if(!lwin->surface)
		retError(clean, Error_invalidState(0, "WindowManager_createWindowPhysical() xdg get surface failed"));

	lwin->surfaceCallbacks = (struct xdg_surface_listener) { .configure = LWindow_confirmExists };
	xdg_surface_add_listener(lwin->surface, &lwin->surfaceCallbacks, w);

	lwin->topLevel = xdg_surface_get_toplevel(lwin->surface);

	if(!lwin->topLevel)
		retError(clean, Error_invalidState(0, "WindowManager_createWindowPhysical() xdg get toplevel failed"));

	lwin->topLevelCallbacks = (struct xdg_toplevel_listener) {
		.configure = LWindow_updateSize,
		.close     = LWindow_close
	};

	xdg_toplevel_add_listener(lwin->topLevel, &lwin->topLevelCallbacks, w);

	//Min size: ensure CSD bar has room for its buttons, unless NoBorder or SSD.
	//DisableResize: enforce by setting min == max, matching Windows WS_SIZEBOX absence.

	{
		I32x2 minSize = w->minSize;
		I32x2 maxSize = w->maxSize;

		if(w->hint & EWindowHint_DisableResize)
			maxSize = minSize = size;   //lock to initial size

		xdg_toplevel_set_min_size(lwin->topLevel, I32x2_x(minSize), I32x2_y(minSize));
		xdg_toplevel_set_max_size(lwin->topLevel, I32x2_x(maxSize), I32x2_y(maxSize));
	}

	//Decoration negotiation.
	//NoBorder: skip entirely, no SSD request, no CSD bar.
	//Otherwise: try SSD first; fall back to CSD subsurface on compositors without it

	if(!(w->hint & EWindowHint_NoBorder) && manager->subcompositor) {

		lwin->barSurface = wl_compositor_create_surface(compositor);

		if(!lwin->barSurface)
			retError(clean, Error_invalidState(0, "WindowManager_createWindowPhysical() bar surface failed"));

		lwin->barSubsurface = wl_subcompositor_get_subsurface(
			manager->subcompositor, lwin->barSurface, surface
		);

		if(!lwin->barSubsurface)
			retError(clean, Error_invalidState(0, "WindowManager_createWindowPhysical() bar subsurface failed"));

		wl_subsurface_set_position(lwin->barSubsurface, 0, -LWINDOW_DECOR_HEIGHT);
		wl_subsurface_place_above(lwin->barSubsurface, surface);
		wl_subsurface_set_desync(lwin->barSubsurface);
	}

	//Register built-in keyboard and mouse devices, matching Windows which always
	// has defaultKeyboardId / defaultMouseId available from creation.

	gotoIfError3(clean, ListInputDevice_reserve(&w->devices, 16, Platform_instance->alloc, e_rr));
	gotoIfError3(clean, ListMonitor_reserve(&w->monitors, 16, Platform_instance->alloc, e_rr));

	w->defaultKeyboardId = (U32) w->devices.length;
	gotoIfError3(clean, Keyboard_create(&builtinKeyboard, Platform_instance->alloc, e_rr));
	builtinKeyboard.dataExt = Buffer_createRefConst(lwin, sizeof(LWindow));    //Identification
	gotoIfError3(clean, ListInputDevice_pushBack(&w->devices, builtinKeyboard, Platform_instance->alloc, e_rr));
	builtinKeyboard = (Keyboard) { 0 };

	w->defaultMouseId = (U32) w->devices.length;
	gotoIfError3(clean, Mouse_create(&builtinMouse, Platform_instance->alloc, e_rr));
	gotoIfError3(clean, ListInputDevice_pushBack(&w->devices, builtinMouse, Platform_instance->alloc, e_rr));
	builtinMouse = (Mouse) { 0 };

	//Wire keyboard listener on the seat so we receive key events and text input

	if(manager->seat) {
		
		if((manager->seatCapabilities & WL_SEAT_CAPABILITY_KEYBOARD)) {

			lwin->keyboard = wl_seat_get_keyboard(manager->seat);

			if(lwin->keyboard)
				wl_keyboard_add_listener(lwin->keyboard, &LWindow_kbListener, w);
		}

		if((manager->seatCapabilities & WL_SEAT_CAPABILITY_POINTER)) {

			struct wl_pointer *pointer = wl_seat_get_pointer(manager->seat);
			if(pointer) {
				lwin->barPointer = pointer;
				wl_pointer_add_listener(pointer, &LWindow_barPointerListener, w);
			}
		}

		if((manager->seatCapabilities & WL_SEAT_CAPABILITY_TOUCH)) {
			struct wl_touch *touch = wl_seat_get_touch(manager->seat);
			if(touch) {
				lwin->touch = touch;
				lwin->primaryTouchId = -1;
				wl_touch_add_listener(touch, &LWindow_touchListener, w);
			}
		}
	}

	if(w->hint & EWindowHint_ForceFullscreen)
		gotoIfError3(clean, Window_toggleFullScreen(w, e_rr));

	gotoIfError3(clean, Window_updatePhysicalTitle(w, w->title, e_rr));

	//In case of no monitors, we will try copying the window manager's monitors.
	//This can happen if there's only 1 window available (e.g. SteamOS).

	if(!w->monitors.length) {

		gotoIfError3(clean, ListMonitor_resize(&w->monitors, w->owner->monitors.length, Platform_instance->alloc, e_rr));
		gotoIfError3(clean, ListMonitor_copy(w->owner->monitors, 0, w->monitors, 0, w->monitors.length, e_rr));

		if(w->callbacks.onMonitorChange)
			w->callbacks.onMonitorChange(w);
	}

	//Initial configure round-trip, triggers LWindow_updateSize -> LWindow_initSize,
	// which also allocates the bar buffer via LWindow_initBar

	wl_surface_commit(surface);
	wl_display_roundtrip(manager->display);

	//Arm first frame callback

	lwin->frameCallback = wl_surface_frame(surface);
	wl_callback_add_listener(lwin->frameCallback, &LWindow_frameListener, w);
	wl_surface_commit(surface);

	w->flags |= EWindowFlags_IsActive;
	isActive = true;

	if(w->callbacks.onCreate) {
		if(!w->callbacks.onCreate(w, e_rr))
			goto clean;
	}

	w->flags |= EWindowFlags_IsFinalized;

	if(w->callbacks.onResize) {
		if(!w->callbacks.onResize(w, e_rr))
			goto clean;
	}

clean:
	if(!s_uccess) {

		if(isActive && w->callbacks.onDestroy)
			w->callbacks.onDestroy(w);

		for(U64 i = 0; i < w->devices.length; ++i)
			InputDevice_free(&w->devices.ptrNonConst[i], Platform_instance->alloc);

		ListInputDevice_free(&w->devices, Platform_instance->alloc);
		ListMonitor_free(&w->monitors, Platform_instance->alloc);

		if(w->type == EWindowType_Physical)
			WindowManager_freePhysical(w);

		else if(w->nativeHandle) {
			wl_surface_destroy((struct wl_surface*) w->nativeHandle);
			w->nativeHandle = NULL;
		}
	}

	InputDevice_free(&builtinKeyboard, Platform_instance->alloc);
	InputDevice_free(&builtinMouse,    Platform_instance->alloc);
	return s_uccess;
}
