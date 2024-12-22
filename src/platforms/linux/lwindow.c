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

#include "platforms/ext/listx_impl.h"
#include "types/container/buffer.h"
#include "platforms/window.h"
#include "platforms/window_manager.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "platforms/input_device.h"
#include "platforms/keyboard.h"
#include "platforms/mouse.h"
#include "platforms/monitor.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "types/base/time.h"
#include "platforms/linux/lwindow_structs.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

//https://wayland-book.com/surfaces/shared-memory.html
Bool LWindow_initSize(Window *w, I32x2 size, Error *e_rr) {

	Bool s_uccess = true;
	struct wl_shm_pool *backBuffer = NULL;

	if(w->hint & EWindowHint_ProvideCPUBuffer) {

		LWindow *lwin = (LWindow*) w->nativeData;

		struct wl_shm *shm = ((LWindowManager*)w->owner->platformData.ptr)->shm;

		U32 width = (U32) I32x2_x(size);
		U32 height = (U32) I32x2_y(size);
		U64 stride = (U64)width * height * 4;
		U64 nbuffers = sizeof(lwin->buffers) / sizeof(lwin->buffers[0]);

		if((width >> 24) || (height >> 24))
			retError(clean, Error_invalidState(0, "LWindow_initSize() max window size of 16777215"))

		//Different implementation of https://wayland-book.com/surfaces/shared-memory.html#allocating-a-shared-memory-pool

		//Grab start of random name

		C8 base[] = "/wl_shm";	//sizeof includes null terminator, so no -
		C8 randomName[] = "/wl_shm-XXXXXXXXXXX";		//64-bit number as Nyto
		U64 rand = 0;

		if(!Buffer_csprng(Buffer_createRef(&rand, sizeof(rand))))
			retError(clean, Error_invalidState(0, "LWindow_initSize() can't create file name"))

		U64 randOg = rand;
		I32 fd = -1;

		//Try to find a file handle that's not occupied.
		//Though extremely rare that 4 subsequent 64-bit numbers are taken.
		//Even if 65536 buffers are created, it's still 1/2^48 chance each.
		//Having 4 is probably about 1/2^50 or something like that.

		for(U64 j = 0; j < 4; ++j) {

			for(U8 i = 0; i < 11; ++i) {
				randomName[sizeof(base) + i] = C8_createNyto(rand & 0x3F);
				rand >>= 6;
			}

			fd = shm_open(randomName, O_RDWR | O_CREAT | O_EXCL, 0600);

			if(fd < 0) {

				if(errno != EEXIST)
					retError(clean, Error_stderr(errno, "LWindow_initSize() shm_open failed"))

				rand = randOg + j + 1;
				continue;
			}

			shm_unlink(randomName);
		}
		
		if(fd < 0)
			retError(clean, Error_invalidState(0, "LWindow_initSize() couldn't find valid shm file name"))

		//Creating backing memory

		while(ftruncate(fd, nbuffers * stride) < 0) {
			if(errno != EINTR) {
				close(fd);
				retError(clean, Error_invalidState(0, "LWindow_initSize() couldn't open file"))
			}
		}

		if(lwin->fileDescriptor)
			close(lwin->fileDescriptor);

		lwin->fileDescriptor = fd;

		lwin->pixelStride = (U32) (width * 4);
		lwin->height = (U16) height;
		lwin->heightHi8 = (U8) (height >> 16);

		backBuffer = wl_shm_create_pool(shm, fd, nbuffers * stride);

		if(!backBuffer)
			retError(clean, Error_invalidState(0, "LWindow_initSize() wayland create pool failed"))

		if(lwin->backBuffer)
			wl_shm_pool_destroy(lwin->backBuffer);

		lwin->backBuffer = backBuffer;

		for(U64 i = 0; i < nbuffers; ++i)
			lwin->buffers[i] = wl_shm_pool_create_buffer(
				backBuffer, stride * i, width, height, lwin->pixelStride, WL_SHM_FORMAT_XRGB8888
			);

		//Interestingly, with wayland we don't have to allocate a main buffer.
		//So the manual present will just swap the address to the next buffer id.

		lwin->mainBufferPtr = mmap(NULL, nbuffers * stride, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

		lwin->backBufferId = 0;
		w->cpuVisibleBuffer.ptr = lwin->mainBufferPtr;
		w->cpuVisibleBuffer.lengthAndRefBits = stride | ((U64)1 << 63);

		backBuffer = NULL;	
	}

clean:

	if(backBuffer)
		wl_shm_pool_destroy(backBuffer);

	return s_uccess;
}

void LWindow_updateMonitors(Window *w) {

	//TODO: Query monitors

	if(w->callbacks.onMonitorChange)
		w->callbacks.onMonitorChange(w);
}

//TODO: Update minimized?
//TODO: Update focus
//TODO: Adhere to min/max size
//TODO: Get offset
//TODO: Char input and key/mouse input

void Window_updateSize(
	void *data,
	struct xdg_toplevel *xdg_toplevel,
	I32 width,
	I32 height,
	struct wl_array *states
) {
	(void) xdg_toplevel;
	(void) states;

	Window *w = (Window*) data;

	I32x2 newSize = I32x2_create2(width, height);

	if (I32x2_any(I32x2_leq(newSize, I32x2_zero())) || I32x2_eq2(w->size, newSize))
		return;

	w->size = newSize;
	Error err = Error_none();

	if(!LWindow_initSize(w, w->size, &err))
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);

	LWindow_updateMonitors(w);

	if (w->callbacks.onResize && (w->flags & EWindowFlags_IsFinalized))
		w->callbacks.onResize(w);
}

void Window_close(void *data, struct xdg_toplevel *xdg_toplevel) {

	Window *w = (Window*) data;

	(void) xdg_toplevel;
	w->flags |= EWindowFlags_ShouldTerminate;
}

Bool WindowManager_supportsFormat(const WindowManager *manager, EWindowFormat format) {
	(void) manager;
	return format == EWindowFormat_BGRA8;
}

Bool WindowManager_freePhysical(Window *w) {

	LWindow *lwin = (LWindow*) w->nativeData;

	if(!lwin)
		return true;

	if(lwin->topLevel)
		xdg_toplevel_destroy(lwin->topLevel);

	if(lwin->surface)
		xdg_surface_destroy(lwin->surface);

	if(lwin->backBuffer)
		wl_shm_pool_destroy(lwin->backBuffer);

	if(lwin->fileDescriptor)
		close(lwin->fileDescriptor);

	Buffer buf = Buffer_createManagedPtr(lwin, sizeof(*lwin));
	Buffer_freex(&buf);

	w->nativeData = NULL;
	return true;
}

Bool Window_updatePhysicalTitle(const Window *w, CharString title, Error *e_rr) {

	Bool s_uccess = true;
	CharString copy = CharString_createNull();

	if(!w || !I32x2_any(w->size) || !title.ptr || !CharString_length(title))
		retError(clean, Error_nullPointer(
			!w || !I32x2_any(w->size) ? 0 : 1, "Window_updatePhysicalTitle()::w and title are required"
		))

	if(!CharString_isNullTerminated(title))
		gotoIfError2(clean, CharString_createCopyx(title, &copy));

	LWindow *lwin = (LWindow*) w->nativeData;
	struct wl_surface *surface = (struct wl_surface*) w->nativeHandle;

	xdg_toplevel_set_title(lwin->topLevel, copy.ptr ? copy.ptr : title.ptr);
	xdg_toplevel_set_app_id(lwin->topLevel, copy.ptr ? copy.ptr : title.ptr);
	wl_surface_commit(surface);

clean:
	CharString_freex(&copy);
	return s_uccess;
}

Bool Window_toggleFullScreen(Window *w, Error *e_rr) {

	Bool s_uccess = true;

	if(!w || !I32x2_any(w->size))
		retError(clean, Error_nullPointer(!w || !I32x2_any(w->size) ? 0 : 1, "Window_toggleFullScreen()::w is required"))

	if(!(w->hint & EWindowHint_AllowFullscreen))
		retError(clean, Error_unsupportedOperation(0, "Window_toggleFullScreen() isn't allowed if EWindowHint_AllowFullscreen is off"))

	Bool wasFullScreen = w->flags & EWindowFlags_IsFullscreen;

	if(!wasFullScreen)
		w->flags |= EWindowFlags_IsFullscreen;

	else w->flags &= ~EWindowFlags_IsFullscreen;

	LWindow *lwin = (LWindow*) w->nativeData;

	if(w->flags & EWindowFlags_IsFullscreen)
		xdg_toplevel_set_fullscreen(lwin->topLevel, NULL);

	else xdg_toplevel_unset_fullscreen(lwin->topLevel);

clean:
	return s_uccess;
}

Bool Window_presentPhysical(Window *w, Error *e_rr) {

	Bool s_uccess = true;

	if(!w || !I32x2_any(w->size))
		retError(clean, Error_nullPointer(0, "Window_presentPhysical()::w is required"))

	if(!(w->flags & EWindowFlags_IsActive) || !(w->hint & EWindowHint_ProvideCPUBuffer))
		retError(clean, Error_invalidOperation(0, "Window_presentPhysical() can only be called if there's a CPU-sided buffer"))

	LWindow *lwin = (LWindow*) w->nativeData;
	struct wl_surface *surface = (struct wl_surface*) w->nativeHandle;

	U32 height = lwin->height | ((U32) lwin->heightHi8 << 16);
	U64 stride = (U64) lwin->pixelStride * height;

	wl_surface_attach(surface, lwin->buffers[lwin->backBufferId], 0, 0);
	wl_surface_damage_buffer(surface, 0, 0, I32_MAX, I32_MAX);
	wl_surface_commit(surface);

	lwin->backBufferId ^= 1;
	w->cpuVisibleBuffer.ptr = lwin->mainBufferPtr + stride * lwin->backBufferId;

clean:
	return s_uccess;
}

//https://gist.github.com/nikp123/bebe2d2dc9a8287efa9ba0a5b38ffab4
void LWindow_confirmExists(void *data, struct xdg_surface *surface, uint32_t serial) {
	(void) data;
	xdg_surface_ack_configure(surface, serial);
}

Bool WindowManager_createWindowPhysical(Window *w, Error *e_rr) {

	//Create native window

	Bool s_uccess = true;

	I32x2 defSize = I32x2_create2(1280, 720);
	I32x2 size = w->size;

	for (U8 i = 0; i < 2; ++i)
		if (!I32x2_get(size, i))
			I32x2_set(&size, i, I32x2_get(defSize, i));

	LWindowManager *manager = (LWindowManager*)w->owner->platformData.ptr;
	struct wl_compositor *compositor = manager->compositor;

	struct wl_surface *surface = wl_compositor_create_surface(compositor);
	w->nativeHandle = surface;

	if(!surface)
		retError(clean, Error_invalidState(0, "WindowManager_createWindowPhysical() wayland create surface failed"))

	wl_surface_set_user_data(surface, w);

	Buffer buf = Buffer_createNull();
	gotoIfError2(clean, Buffer_createEmptyBytesx(sizeof(LWindow), &buf))
	w->nativeData = (void*) buf.ptr;		//LWindow

	LWindow *lwin = (LWindow*) buf.ptr;
	lwin->surface = xdg_wm_base_get_xdg_surface(manager->xdgWmBase, surface);

	if(!lwin->surface)
		retError(clean, Error_invalidState(0, "WindowManager_createWindowPhysical() xdg get surface failed"));

	lwin->surfaceCallbacks = (struct xdg_surface_listener) {
		.configure = LWindow_confirmExists
	};

	xdg_surface_add_listener(lwin->surface, &lwin->surfaceCallbacks, NULL);
	lwin->topLevel = xdg_surface_get_toplevel(lwin->surface);

	if(!lwin->topLevel)
		retError(clean, Error_invalidState(0, "WindowManager_createWindowPhysical() xdg get toplevel failed"))

	lwin->topLevelCallbacks = (struct xdg_toplevel_listener) {
		.configure = Window_updateSize,
		.close = Window_close
	};

	xdg_toplevel_add_listener(lwin->topLevel, &lwin->topLevelCallbacks, w);

	//Reserve monitors and input device(s)

	gotoIfError2(clean, ListInputDevice_reservex(&w->devices, 16))
	gotoIfError2(clean, ListMonitor_reservex(&w->monitors, 16))

	//Toggle full screen & set title

	if(w->hint & EWindowHint_ForceFullscreen)
		gotoIfError3(clean, Window_toggleFullScreen(w, e_rr))

	//Builtin server side decorations :) happy times

	if(manager->xdgDeco) {
			
		struct zxdg_toplevel_decoration_v1 *decoration =
			zxdg_decoration_manager_v1_get_toplevel_decoration(manager->xdgDeco, lwin->topLevel);

		zxdg_toplevel_decoration_v1_set_mode(decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}

	gotoIfError3(clean, Window_updatePhysicalTitle(w, w->title, e_rr))

	//Call handlers to set right size

	wl_display_roundtrip(manager->display);
	wl_surface_commit(surface);

clean:
	return s_uccess;
}

