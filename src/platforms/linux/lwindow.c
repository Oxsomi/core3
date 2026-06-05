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
#include "platforms/window.h"
#include "platforms/window_manager.h"
#include "platforms/platform.h"
#include "types/container/log.h"
#include "types/container/buffer.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input-event-codes.h>

static Bool LWindow_openShmFd(U64 size, I32 *fdOut, Error *e_rr) {

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
	LWindow *lwin = (LWindow*) w->nativeData;

	wl_callback_destroy(cb);
	lwin->frameCallback = NULL;
	lwin->frameReady    = true;
}

static const struct wl_callback_listener LWindow_frameListener = {
	.done = LWindow_frameCallback
};

//TODO: Double check this also. V
// Colors (XRGB8888)
#define DECOR_COL_BG         0xFF2B2B2B
#define DECOR_COL_BG_HOVER   0xFF3C3C3C
#define DECOR_COL_CLOSE      0xFFC42B1C
#define DECOR_COL_CLOSE_HOV  0xFFE81123
#define DECOR_COL_SYM        0xFFCCCCCC
#define DECOR_COL_TITLE      0xFFCCCCCC

static void LDecor_fillRect(U32 *px, U32 stride4, I32 x, I32 y, I32 w, I32 h, U32 col) {
	for(I32 r = y; r < y + h; ++r)
		for(I32 c = x; c < x + w; ++c)
			px[r * (I32)stride4 + c] = col;
}

static void LDecor_drawMinimise(U32 *px, U32 stride4, I32 bx, I32 by, U32 col) {
	I32 sy = by + LWINDOW_DECOR_BTN_H / 2 + 2;
	LDecor_fillRect(px, stride4, bx + 18, sy, 10, 1, col);
}

static void LDecor_drawMaximise(U32 *px, U32 stride4, I32 bx, I32 by, U32 col) {
	I32 sx = bx + 18, sy = by + (LWINDOW_DECOR_BTN_H - 10) / 2;
	LDecor_fillRect(px, stride4, sx,     sy,      10, 1, col);
	LDecor_fillRect(px, stride4, sx,     sy + 9,  10, 1, col);
	LDecor_fillRect(px, stride4, sx,     sy + 1,  1,  8, col);
	LDecor_fillRect(px, stride4, sx + 9, sy + 1,  1,  8, col);
}

static void LDecor_drawClose(U32 *px, U32 stride4, I32 bx, I32 by, U32 col) {
	I32 sx = bx + 18, sy = by + (LWINDOW_DECOR_BTN_H - 10) / 2;
	for(I32 i = 0; i < 10; ++i) {
		px[(sy + i) * (I32)stride4 + sx + i]     = col;
		px[(sy + i) * (I32)stride4 + sx + 9 - i] = col;
	}
}

static const U8 LDecor_font6x8[95][6] = {
	{0x00,0x00,0x00,0x00,0x00,0x00}, // ' '  (32)
	{0x20,0x20,0x20,0x00,0x20,0x00}, // '!'
	{0x50,0x50,0x00,0x00,0x00,0x00}, // '"'
	{0x50,0xF8,0x50,0xF8,0x50,0x00}, // '#'
	{0x20,0x78,0x60,0x18,0xF0,0x20}, // '$'
	{0xC0,0xC8,0x10,0x20,0x4C,0x0C}, // '%'
	{0x60,0x90,0x60,0x90,0x68,0x00}, // '&'
	{0x60,0x20,0x40,0x00,0x00,0x00}, // '''
	{0x10,0x20,0x40,0x40,0x20,0x10}, // '('
	{0x40,0x20,0x10,0x10,0x20,0x40}, // ')'
	{0x00,0x50,0x20,0x50,0x00,0x00}, // '*'
	{0x00,0x20,0x70,0x20,0x00,0x00}, // '+'
	{0x00,0x00,0x00,0x30,0x10,0x20}, // ','
	{0x00,0x00,0x70,0x00,0x00,0x00}, // '-'
	{0x00,0x00,0x00,0x00,0x60,0x00}, // '.'
	{0x08,0x08,0x10,0x20,0x40,0x40}, // '/'
	{0x70,0x88,0x98,0xA8,0xC8,0x70}, // '0'
	{0x20,0x60,0x20,0x20,0x70,0x00}, // '1'
	{0x70,0x88,0x10,0x20,0x40,0xF8}, // '2'
	{0xF0,0x08,0x30,0x08,0x08,0xF0}, // '3'
	{0x10,0x30,0x50,0xF8,0x10,0x10}, // '4'
	{0xF8,0x80,0xF0,0x08,0x08,0xF0}, // '5'
	{0x38,0x40,0xF0,0x88,0x88,0x70}, // '6'
	{0xF8,0x08,0x10,0x20,0x20,0x20}, // '7'
	{0x70,0x88,0x70,0x88,0x88,0x70}, // '8'
	{0x70,0x88,0x78,0x08,0x10,0xE0}, // '9'
	{0x00,0x60,0x00,0x60,0x00,0x00}, // ':'
	{0x00,0x30,0x00,0x30,0x10,0x20}, // ';'
	{0x10,0x20,0x40,0x20,0x10,0x00}, // '<'
	{0x00,0x70,0x00,0x70,0x00,0x00}, // '='
	{0x40,0x20,0x10,0x20,0x40,0x00}, // '>'
	{0x70,0x08,0x10,0x20,0x00,0x20}, // '?'
	{0x70,0x88,0xB8,0xB0,0x80,0x78}, // '@'
	{0x20,0x50,0x88,0xF8,0x88,0x88}, // 'A'
	{0xF0,0x88,0xF0,0x88,0x88,0xF0}, // 'B'
	{0x70,0x88,0x80,0x80,0x88,0x70}, // 'C'
	{0xE0,0x90,0x88,0x88,0x90,0xE0}, // 'D'
	{0xF8,0x80,0xF0,0x80,0x80,0xF8}, // 'E'
	{0xF8,0x80,0xF0,0x80,0x80,0x80}, // 'F'
	{0x70,0x88,0x80,0x98,0x88,0x70}, // 'G'
	{0x88,0x88,0xF8,0x88,0x88,0x88}, // 'H'
	{0x70,0x20,0x20,0x20,0x20,0x70}, // 'I'
	{0x38,0x10,0x10,0x10,0x90,0x60}, // 'J'
	{0x88,0x90,0xE0,0x90,0x88,0x88}, // 'K'
	{0x80,0x80,0x80,0x80,0x80,0xF8}, // 'L'
	{0x88,0xD8,0xA8,0x88,0x88,0x88}, // 'M'
	{0x88,0xC8,0xA8,0x98,0x88,0x88}, // 'N'
	{0x70,0x88,0x88,0x88,0x88,0x70}, // 'O'
	{0xF0,0x88,0x88,0xF0,0x80,0x80}, // 'P'
	{0x70,0x88,0x88,0xA8,0x90,0x68}, // 'Q'
	{0xF0,0x88,0x88,0xF0,0x90,0x88}, // 'R'
	{0x78,0x80,0x70,0x08,0x08,0xF0}, // 'S'
	{0xF8,0x20,0x20,0x20,0x20,0x20}, // 'T'
	{0x88,0x88,0x88,0x88,0x88,0x70}, // 'U'
	{0x88,0x88,0x88,0x88,0x50,0x20}, // 'V'
	{0x88,0x88,0xA8,0xA8,0xD8,0x88}, // 'W'
	{0x88,0x50,0x20,0x50,0x88,0x88}, // 'X'
	{0x88,0x88,0x50,0x20,0x20,0x20}, // 'Y'
	{0xF8,0x08,0x10,0x20,0x40,0xF8}, // 'Z'
	{0x70,0x40,0x40,0x40,0x40,0x70}, // '['
	{0x40,0x40,0x20,0x10,0x08,0x08}, // '\'
	{0x70,0x10,0x10,0x10,0x10,0x70}, // ']'
	{0x20,0x50,0x88,0x00,0x00,0x00}, // '^'
	{0x00,0x00,0x00,0x00,0x00,0xF8}, // '_'
	{0x40,0x20,0x10,0x00,0x00,0x00}, // '`'
	{0x00,0x60,0x10,0x70,0x90,0x68}, // 'a'
	{0x80,0xB0,0xC8,0x88,0xC8,0xB0}, // 'b'
	{0x00,0x70,0x80,0x80,0x80,0x70}, // 'c'
	{0x08,0x68,0x98,0x88,0x98,0x68}, // 'd'
	{0x00,0x70,0x88,0xF8,0x80,0x70}, // 'e'
	{0x30,0x48,0x40,0xF0,0x40,0x40}, // 'f'
	{0x00,0x70,0x88,0x98,0x68,0x08}, // 'g'
	{0x80,0xB0,0xC8,0x88,0x88,0x88}, // 'h'
	{0x20,0x00,0x60,0x20,0x20,0x70}, // 'i'
	{0x10,0x00,0x30,0x10,0x10,0x90}, // 'j'
	{0x80,0x90,0xA0,0xC0,0xA0,0x90}, // 'k'
	{0x60,0x20,0x20,0x20,0x20,0x70}, // 'l'
	{0x00,0xD0,0xA8,0xA8,0xA8,0x88}, // 'm'
	{0x00,0xB0,0xC8,0x88,0x88,0x88}, // 'n'
	{0x00,0x70,0x88,0x88,0x88,0x70}, // 'o'
	{0x00,0xF0,0x88,0xF0,0x80,0x80}, // 'p'
	{0x00,0x68,0x98,0x68,0x08,0x08}, // 'q'
	{0x00,0xB0,0xC8,0x80,0x80,0x80}, // 'r'
	{0x00,0x70,0x80,0x70,0x08,0xF0}, // 's'
	{0x40,0xF0,0x40,0x40,0x48,0x30}, // 't'
	{0x00,0x88,0x88,0x88,0x98,0x68}, // 'u'
	{0x00,0x88,0x88,0x88,0x50,0x20}, // 'v'
	{0x00,0x88,0xA8,0xA8,0xA8,0x50}, // 'w'
	{0x00,0x88,0x50,0x20,0x50,0x88}, // 'x'
	{0x00,0x88,0x98,0x68,0x08,0x70}, // 'y'
	{0x00,0xF8,0x10,0x20,0x40,0xF8}, // 'z'
	{0x30,0x20,0x60,0x20,0x20,0x30}, // '{'
	{0x20,0x20,0x20,0x20,0x20,0x20}, // '|'
	{0x60,0x20,0x30,0x20,0x20,0x60}, // '}'
	{0x40,0xA8,0x10,0x00,0x00,0x00}, // '~'
};

static void LDecor_drawChar(U32 *px, U32 stride4, I32 cx, I32 cy, C8 ch, U32 col) {
	if(ch < 32 || ch > 126) return;
	const U8 *glyph = LDecor_font6x8[ch - 32];
	for(I32 row = 0; row < 6; ++row) {
		U8 bits = glyph[row];
		for(I32 col2 = 0; col2 < 6; ++col2) {
			if(bits & (0x80u >> col2))
				px[(cy + row) * (I32)stride4 + (cx + col2)] = col;
		}
	}
}

static void LDecor_drawString(U32 *px, U32 stride4, I32 x, I32 y, const C8 *str, U32 col, I32 maxW) {
	I32 cx = x;
	while(*str && (cx + 6) <= (x + maxW)) {
		LDecor_drawChar(px, stride4, cx, y, *str, col);
		cx += 7;
		++str;
	}
}

static void LWindow_redrawBar(Window *w) {

	LWindow *lwin = (LWindow*) w->nativeData;

	if(!lwin->barSurface || !lwin->barPixels)
		return;

	U32 width  = lwin->barWidth;
	U32 height = LWINDOW_DECOR_HEIGHT;
	U32 stride = width;

	LDecor_fillRect(lwin->barPixels, stride, 0, 0, (I32)width, (I32)height, DECOR_COL_BG);

	I32 closeX = (I32)width  - LWINDOW_DECOR_BTN_W;
	I32 maxX   = closeX      - LWINDOW_DECOR_BTN_W;
	I32 minX   = maxX        - LWINDOW_DECOR_BTN_W;

	Bool inClose = lwin->pointerInBar && lwin->pointerX >= closeX;
	Bool inMax   = lwin->pointerInBar && lwin->pointerX >= maxX && lwin->pointerX < closeX;
	Bool inMin   = lwin->pointerInBar && lwin->pointerX >= minX && lwin->pointerX < maxX;

	LDecor_fillRect(lwin->barPixels, stride, closeX, 0, LWINDOW_DECOR_BTN_W, LWINDOW_DECOR_BTN_H,
		inClose ? DECOR_COL_CLOSE_HOV : DECOR_COL_BG);
	LDecor_fillRect(lwin->barPixels, stride, maxX, 0, LWINDOW_DECOR_BTN_W, LWINDOW_DECOR_BTN_H,
		inMax ? DECOR_COL_BG_HOVER : DECOR_COL_BG);
	LDecor_fillRect(lwin->barPixels, stride, minX, 0, LWINDOW_DECOR_BTN_W, LWINDOW_DECOR_BTN_H,
		inMin ? DECOR_COL_BG_HOVER : DECOR_COL_BG);

	LDecor_drawMinimise(lwin->barPixels, stride, minX,   0, DECOR_COL_SYM);
	LDecor_drawMaximise(lwin->barPixels, stride, maxX,   0, DECOR_COL_SYM);
	LDecor_drawClose(   lwin->barPixels, stride, closeX, 0, DECOR_COL_SYM);

	I32 titleAreaW = minX - 8;
	I32 titleY     = (LWINDOW_DECOR_HEIGHT - 6) / 2;
	if(titleAreaW > 6 && w->title.ptr)
		LDecor_drawString(lwin->barPixels, stride, 8, titleY,
			w->title.ptr, DECOR_COL_TITLE, titleAreaW);

	wl_surface_attach(lwin->barSurface, lwin->barBuffer, 0, 0);
	wl_surface_damage_buffer(lwin->barSurface, 0, 0, (I32)width, (I32)height);
	wl_surface_commit(lwin->barSurface);
}

static Bool LWindow_initBar(Window *w, U32 width, Error *e_rr) {

	Bool s_uccess = true;
	I32  fd       = -1;

	LWindow        *lwin    = (LWindow*) w->nativeData;
	LWindowManager *manager = (LWindowManager*)w->owner->platformData.ptr;

	if(!lwin->barSurface || lwin->barWidth == width)
		goto clean;

	U64 barSize = (U64)width * LWINDOW_DECOR_HEIGHT * 4;

	if(lwin->barBuffer) {
		wl_buffer_destroy(lwin->barBuffer);
		lwin->barBuffer     = NULL;
		lwin->barBufferBusy = false;
	}

	if(lwin->barPixels) {
		munmap(lwin->barPixels, (U64)lwin->barWidth * LWINDOW_DECOR_HEIGHT * 4);
		lwin->barPixels = NULL;
	}

	if(lwin->barPool) {
		wl_shm_pool_destroy(lwin->barPool);
		lwin->barPool = NULL;
	}

	gotoIfError3(clean, LWindow_openShmFd(barSize, &fd, e_rr))

	lwin->barPool = wl_shm_create_pool(manager->shm, fd, (I32)barSize);

	if(!lwin->barPool)
		retError(clean, Error_invalidState(0, "LWindow_initBar() wl_shm_create_pool failed"))

	lwin->barBuffer = wl_shm_pool_create_buffer(
		lwin->barPool, 0,
		(I32)width, LWINDOW_DECOR_HEIGHT,
		(I32)(width * 4), WL_SHM_FORMAT_XRGB8888
	);

	if(!lwin->barBuffer)
		retError(clean, Error_invalidState(0, "LWindow_initBar() wl_shm_pool_create_buffer failed"))

	// mmap before closing fd — mapping remains valid after close
	lwin->barPixels = mmap(NULL, barSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if(lwin->barPixels == MAP_FAILED) {
		lwin->barPixels = NULL;
		retError(clean, Error_stderr(errno, "LWindow_initBar() mmap failed"))
	}

	lwin->barWidth = width;
	LWindow_redrawBar(w);

clean:
	if(fd >= 0) {
		close(fd);
		fd = -1;
	}

	return s_uccess;
}

static void LWindow_pointerEnterBar(
	void *data, struct wl_pointer *ptr, U32 serial,
	struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy
) {
	(void) ptr; (void) serial; (void) surface;
	Window  *w    = (Window*) data;
	LWindow *lwin = (LWindow*) w->nativeData;
	lwin->pointerX     = wl_fixed_to_int(sx);
	lwin->pointerY     = wl_fixed_to_int(sy);
	lwin->pointerInBar = true;
	LWindow_redrawBar(w);
}

static void LWindow_pointerLeaveBar(
	void *data, struct wl_pointer *ptr, U32 serial, struct wl_surface *surface
) {
	(void) ptr; (void) serial; (void) surface;
	Window  *w    = (Window*) data;
	LWindow *lwin = (LWindow*) w->nativeData;
	lwin->pointerInBar = false;
	LWindow_redrawBar(w);
}

static void LWindow_pointerMotionBar(
	void *data, struct wl_pointer *ptr, U32 time, wl_fixed_t sx, wl_fixed_t sy
) {
	(void) ptr; (void) time;
	Window  *w    = (Window*) data;
	LWindow *lwin = (LWindow*) w->nativeData;
	I32 nx = wl_fixed_to_int(sx);
	I32 ny = wl_fixed_to_int(sy);

	if(nx != lwin->pointerX || ny != lwin->pointerY) {
		lwin->pointerX = nx;
		lwin->pointerY = ny;
		LWindow_redrawBar(w);
	}
}

static void LWindow_pointerButtonBar(
	void *data, struct wl_pointer *ptr, U32 serial, U32 time,
	U32 button, U32 state
) {
	(void) ptr; (void) time;

	if(state != WL_POINTER_BUTTON_STATE_PRESSED || button != BTN_LEFT)
		return;

	Window  *w    = (Window*) data;
	LWindow *lwin = (LWindow*) w->nativeData;

	if(!lwin->pointerInBar)
		return;

	LWindowManager *manager = (LWindowManager*)w->owner->platformData.ptr;
	I32 x      = lwin->pointerX;
	I32 closeX = (I32)lwin->barWidth  - LWINDOW_DECOR_BTN_W;
	I32 maxX   = closeX               - LWINDOW_DECOR_BTN_W;
	I32 minX   = maxX                 - LWINDOW_DECOR_BTN_W;

	if(x >= closeX)
		w->flags |= EWindowFlags_ShouldTerminate;

	else if(x >= maxX) {
		if(w->hint & EWindowHint_AllowFullscreen) {
			Error e = Error_none();
			Window_toggleFullScreen(w, &e);
		}
	}

	else if(x >= minX)
		xdg_toplevel_set_minimized(lwin->topLevel);

	else
		xdg_toplevel_move(lwin->topLevel, manager->seat, serial);
}

static void LWindow_pointerAxisBar(void *d, struct wl_pointer *p, U32 t, U32 a, wl_fixed_t v) {
	(void)d; (void)p; (void)t; (void)a; (void)v;
}

// Required stubs for wl_seat version 5 — missing entries cause NULL-dispatch crashes
// on compositors that send these events.
static void LWindow_pointerFrameBar(void *d, struct wl_pointer *p) {
	(void)d; (void)p;
}

static void LWindow_pointerAxisSourceBar(void *d, struct wl_pointer *p, U32 s) {
	(void)d; (void)p; (void)s;
}

static void LWindow_pointerAxisStopBar(void *d, struct wl_pointer *p, U32 t, U32 a) {
	(void)d; (void)p; (void)t; (void)a;
}

static void LWindow_pointerAxisDiscreteBar(void *d, struct wl_pointer *p, U32 a, I32 v) {
	(void)d; (void)p; (void)a; (void)v;
}

static const struct wl_pointer_listener LWindow_barPointerListener = {
	.enter         = LWindow_pointerEnterBar,
	.leave         = LWindow_pointerLeaveBar,
	.motion        = LWindow_pointerMotionBar,
	.button        = LWindow_pointerButtonBar,
	.axis          = LWindow_pointerAxisBar,
	.frame         = LWindow_pointerFrameBar,
	.axis_source   = LWindow_pointerAxisSourceBar,
	.axis_stop     = LWindow_pointerAxisStopBar,
	.axis_discrete = LWindow_pointerAxisDiscreteBar,
};

static void LWindow_decorConfigure(
	void *data,
	struct zxdg_toplevel_decoration_v1 *deco,
	U32 mode
) {
	(void) deco;
	Window  *w    = (Window*) data;
	LWindow *lwin = (LWindow*) w->nativeData;

	if(mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE && !lwin->barSurface) {
		// Compositor downgraded us to CSD after we requested SSD.
		// This is rare but must be handled. Log it — actual bar creation
		// requires the compositor connection and should be done in the
		// create path once this flag is detected.
		Log_warnLn("LWindow_decorConfigure(): compositor downgraded to CSD — bar will be created");
		// TODO: trigger deferred bar creation via LWindow_createBarSurface(w)
	}
}

//TODO: Check this ^

static const struct zxdg_toplevel_decoration_v1_listener LWindow_decorListener = {
	.configure = LWindow_decorConfigure
};

Bool LWindow_initSize(Window *w, I32x2 size, Error *e_rr) {

	Bool s_uccess    = true;
	struct wl_shm_pool *pool = NULL;
	I32  fd          = -1;
	U64  buffersCreated = 0;

	LWindow *lwin  = (LWindow*) w->nativeData;
	I32 totalWidth  = I32x2_x(size);
	I32 totalHeight = I32x2_y(size);

	//Resize the bar whenever the window width changes

	gotoIfError3(clean, LWindow_initBarCorrect(w, (U32)totalWidth, e_rr));

	// The content height excludes the decoration bar.
	// When using SSD the bar is zero-height from our perspective.
	I32 contentHeight = lwin->barSurface ? totalHeight - LWINDOW_DECOR_HEIGHT : totalHeight;

	if(contentHeight <= 0)
		contentHeight = 1;

	//Tell the compositor where the actual interactive content begins
	xdg_surface_set_window_geometry(lwin->surface, 0, 0, totalWidth, contentHeight);

	if(w->hint & EWindowHint_ProvideCPUBuffer) {

		struct wl_shm *shm = ((LWindowManager*)w->owner->platformData.ptr)->shm;

		U32 width  = (U32) totalWidth;
		U32 height = (U32) contentHeight;

		U64 stride   = (U64)width * 4;
		U64 poolSize = stride * height * LWINDOW_BUFFER_COUNT;

		if((width >> 24) || (height >> 24))
			retError(clean, Error_invalidState(0, "LWindow_initSize() max window size of 16777215"));

		gotoIfError3(clean, LWindow_openShmFd(poolSize, &fd, e_rr));

		//Tear down old resources
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
				WL_SHM_FORMAT_XRGB8888
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

		//cpuVisibleBuffer points to content area, user sees nothing of the bar
		w->cpuVisibleBuffer.ptr              = lwin->mainBufferPtr;
		w->cpuVisibleBuffer.lengthAndRefBits = (stride * height) | ((U64)1 << 63);
	}

clean:
	if(!s_uccess) {

		if(w->nativeData) {
			LWindow *l2 = (LWindow*) w->nativeData;
			for(U64 i = 0; i < buffersCreated; ++i) {
				if(l2->buffers[i]) {
					wl_buffer_destroy(l2->buffers[i]);
					l2->buffers[i] = NULL;
				}
			}
		}

		if(pool) wl_shm_pool_destroy(pool);
		if(fd >= 0) close(fd);
	}

	return s_uccess;
}

void LWindow_updateMonitors(Window *w) {

	LWindowManager *manager = (LWindowManager*)w->owner->platformData.ptr;
	ListMonitor_clear(&w->monitors);

	for(U32 i = 0; i < LWINDOW_MAX_OUTPUTS; ++i) {

		if(!manager->outputs[i])
			continue;

		Monitor m = (Monitor) { 0 };

		//TODO:
		// typedef enum EMonitorOrientation {
		// 	EMonitorOrientation_Landscape            = 0,
		// 	EMonitorOrientation_Portrait             = 90,
		// 	EMonitorOrientation_FlippedLandscape     = 180,
		// 	EMonitorOrientation_FlippedPortrait      = 270
		// } EMonitorOrientation;
		// 
		// //A monitor is something a physical window is displayed on.
		// //The window can know this to handle monitor specific processing,
		// //such as subpixel rendering.
		// 
		// typedef struct Monitor {
		// 
		// 	I32x2 offsetPixels, sizePixels;
		// 	I32x2 offsetR, offsetG;
		// 	I32x2 offsetB, sizeInches;
		// 
		// 	EMonitorOrientation orientation;
		// 	F32 gamma, contrast, refreshRate;
		// 
		// } Monitor;

		// TODO: populate from manager->outputInfo[i] once wl_output listener is wired
		ListMonitor_pushBackx(&w->monitors, m);
	}

	if(w->callbacks.onMonitorChange)
		w->callbacks.onMonitorChange(w);
}

static void LWindow_updateSize(
	void *data,
	struct xdg_toplevel *xdg_toplevel,
	I32 width,
	I32 height,
	struct wl_array *states
) {
	(void) xdg_toplevel;
	(void) states;

	Window *w = (Window*) data;

	//0x0 = compositor defers to client
	if(width  <= 0) width  = I32x2_x(w->size) ? I32x2_x(w->size) : 1280;
	if(height <= 0) height = I32x2_y(w->size) ? I32x2_y(w->size) : 720;

	I32x2 newSize = I32x2_create2(width, height);

	if(I32x2_eq2(w->size, newSize))
		return;

	w->size = newSize;

	Error err = Error_none();
	if(!LWindow_initSize(w, w->size, &err))
		Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);

	LWindow_updateMonitors(w);

	if(w->callbacks.onResize && (w->flags & EWindowFlags_IsFinalized))
		w->callbacks.onResize(w);
}

void LWindow_close(void *data, struct xdg_toplevel *xdg_toplevel) {
	(void) xdg_toplevel;
	((Window*)data)->flags |= EWindowFlags_ShouldTerminate;
}

Bool WindowManager_supportsFormat(const WindowManager *manager, EWindowFormat format) {
	(void) manager;
	return format == EWindowFormat_BGRA8;	//TODO: HDR
}

void WindowManager_freePhysical(Window *w) {

	LWindow *lwin = (LWindow*) w->nativeData;

	if(!lwin)
		return;

	//Drain pending events so in-flight callbacks (e.g. frame callback) fire
	// and dereference lwin before we free it.
	LWindowManager *manager = (LWindowManager*)w->owner->platformData.ptr;
	if(manager && manager->display)
    	wl_display_roundtrip(manager->display);

	if(lwin->barBuffer)     wl_buffer_destroy(lwin->barBuffer);
	if(lwin->barPixels)     munmap(lwin->barPixels, (U64)lwin->barWidth * LWINDOW_DECOR_HEIGHT * 4);
	if(lwin->barPool)       wl_shm_pool_destroy(lwin->barPool);
	if(lwin->barSubsurface) wl_subsurface_destroy(lwin->barSubsurface);
	if(lwin->barSurface)    wl_surface_destroy(lwin->barSurface);

	for(U64 i = 0; i < LWINDOW_BUFFER_COUNT; ++i)
		if(lwin->buffers[i])
			wl_buffer_destroy(lwin->buffers[i]);

	if(lwin->frameCallback)   wl_callback_destroy(lwin->frameCallback);
	if(lwin->decoration)      zxdg_toplevel_decoration_v1_destroy(lwin->decoration);
	if(lwin->topLevel)        xdg_toplevel_destroy(lwin->topLevel);
	if(lwin->surface)         xdg_surface_destroy(lwin->surface);
	if(lwin->backBuffer)      wl_shm_pool_destroy(lwin->backBuffer);

	if(lwin->mainBufferPtr) {
		U32 oldH      = lwin->height | ((U32) lwin->heightHi8 << 16);
		U64 oldPoolSz = (U64) lwin->pixelStride * oldH * LWINDOW_BUFFER_COUNT;
		munmap(lwin->mainBufferPtr, oldPoolSz);
	}

	if(w->nativeHandle)           wl_surface_destroy((struct wl_surface*) w->nativeHandle);
	if(lwin->fileDescriptor >= 0) close(lwin->fileDescriptor);

	Buffer buf = Buffer_createManagedPtr(lwin, sizeof(*lwin));
	Buffer_free(&buf, Platform_instance->alloc);

	w->nativeData   = NULL;
	w->nativeHandle = NULL;
}

Bool Window_updatePhysicalTitle(const Window *w, CharString title, Error *e_rr) {

	Bool s_uccess = true;
	CharString copy = CharString_createNull();

	if(!w || !I32x2_any(w->size) || !title.ptr || !CharString_length(title) || w->type != EWindowType_Physical)
		retError(clean, Error_nullPointer(
			!w || !I32x2_any(w->size) ? 0 : 1, "Window_updatePhysicalTitle()::w and title are required"
		))

	if(!CharString_isNullTerminated(title))
		gotoIfError3(clean, CharString_createCopy(title, Platform_instance->alloc, &copy, e_rr));

	LWindow *lwin = (LWindow*) w->nativeData;
	struct wl_surface *surface = (struct wl_surface*) w->nativeHandle;

	xdg_toplevel_set_title(lwin->topLevel, copy.ptr ? copy.ptr : title.ptr);
	xdg_toplevel_set_app_id(lwin->topLevel, copy.ptr ? copy.ptr : title.ptr);
	wl_surface_commit(surface);

	//Redraw CSD bar with updated title if present
	LWindow_redrawBar((Window*)(uintptr_t)w);

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

	LWindow *lwin = (LWindow*) w->nativeData;

	if(w->flags & EWindowFlags_IsFullscreen) {
		w->flags &= ~EWindowFlags_IsFullscreen;
		xdg_toplevel_unset_fullscreen(lwin->topLevel);
	} else {
		w->flags |= EWindowFlags_IsFullscreen;
		xdg_toplevel_set_fullscreen(lwin->topLevel, NULL);
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

	LWindow *lwin = (LWindow*) w->nativeData;

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
	U32 height  = lwin->height | ((U32) lwin->heightHi8 << 16);
	U64 stride  = (U64) lwin->pixelStride * height;

	lwin->bufferBusy[chosen] = true;

	wl_surface_attach(surface, lwin->buffers[chosen], 0, 0);
	wl_surface_damage_buffer(surface, 0, 0, I32_MAX, I32_MAX);

	lwin->frameCallback = wl_surface_frame(surface);
	wl_callback_add_listener(lwin->frameCallback, &LWindow_frameListener, w);

	wl_surface_commit(surface);

	lwin->backBufferId      = (U8)((chosen + 1) % LWINDOW_BUFFER_COUNT);
	w->cpuVisibleBuffer.ptr = lwin->mainBufferPtr + stride * lwin->backBufferId;

clean:
	return s_uccess;
}

void LWindow_confirmExists(void *data, struct xdg_surface *surface, U32 serial) {
	(void) data;
	xdg_surface_ack_configure(surface, serial);
}

Bool WindowManager_createWindowPhysical(Window *w, Error *e_rr) {

	Bool s_uccess = true;

	I32x2 defSize = I32x2_create2(1280, 720);
	I32x2 size    = w->size;

	for(U8 i = 0; i < 2; ++i)
		if(!I32x2_get(size, i))
			I32x2_set(&size, i, I32x2_get(defSize, i));

	LWindowManager       *manager    = (LWindowManager*)w->owner->platformData.ptr;
	struct wl_compositor *compositor = manager->compositor;

	//Main surface, this is the handle that could be passed to Vulkan

	struct wl_surface *surface = wl_compositor_create_surface(compositor);

	if(!surface)
		retError(clean, Error_invalidState(0, "WindowManager_createWindowPhysical() create surface failed"));

	wl_surface_set_user_data(surface, w);
	w->nativeHandle = surface;

	//Allocate LWindow

	Buffer buf = Buffer_createNull();
	gotoIfError3(clean, Buffer_createEmptyBytes(sizeof(LWindow), Platform_instance->alloc, &buf, e_rr));
	w->nativeData = (void*) buf.ptr;

	LWindow *lwin        = (LWindow*) buf.ptr;
	lwin->fileDescriptor = -1;
	lwin->frameReady     = true;

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

	I32x2 minSize = I32x2_create2(3 * LWINDOW_DECOR_BTN_W, LWINDOW_DECOR_HEIGHT + 1);

	if(manager->xdgDeco)
		minSize = I32x2_zero;

	minSize = I32x2_max(w->minSize, minSize);

	xdg_toplevel_set_min_size(lwin->topLevel, I32x2_x(minSize), I32x2_y(minSize));
	xdg_toplevel_set_max_size(lwin->topLevel, I32x2_x(w->maxSize), I32x2_y(w->maxSize));

	//Decoration negotiation,
	//Try SSD first. If the compositor supports it we get native decorations
	//for free and never create the bar subsurface.
	//If SSD is unavailable (GNOME) we create a subsurface for the bar.
	//Either way the user just gets w->nativeHandle and calls vkCreateWaylandSurfaceKHR.

	Bool useCSD = true;

	if(manager->xdgDeco) {

		lwin->decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(manager->xdgDeco, lwin->topLevel);
		
		lwin->decorationListener = LWindow_decorListener;
		zxdg_toplevel_decoration_v1_add_listener(lwin->decoration, &lwin->decorationListener, w);
		zxdg_toplevel_decoration_v1_set_mode(lwin->decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);

		//TODO:!!!!

		//Do one roundtrip so the compositor can respond to our SSD request
		// before we decide whether to create the CSD bar.
		wl_display_roundtrip(manager->display);

		//If the compositor honoured SSD the decoration configure callback
		// won't have set barSurface. We only fall through to CSD creation
		// if the compositor downgraded us, but in practice that is handled
		// in LWindow_decorConfigure above. Here we just mark CSD not needed
		// when xdgDeco is present and the mode came back as SERVER_SIDE.
		//assume SSD was accepted; decorator configure overrides

		useCSD = false;
	}

	if(useCSD && manager->subcompositor) {

		//Create the decoration bar as a subsurface above the main surface.
		//It has its own shm buffer, completely separate from the Vulkan swapchain
		// or the CPU buffer on the main surface.

		lwin->barSurface = wl_compositor_create_surface(compositor);

		if(!lwin->barSurface)
			retError(clean, Error_invalidState(0, "WindowManager_createWindowPhysical() bar surface failed"));

		lwin->barSubsurface = wl_subcompositor_get_subsurface(manager->subcompositor, lwin->barSurface, surface);

		if(!lwin->barSubsurface)
			retError(clean, Error_invalidState(0, "WindowManager_createWindowPhysical() bar subsurface failed"));

		//Position bar at top-left, above the main surface
		wl_subsurface_set_position(lwin->barSubsurface, 0, 0);
		wl_subsurface_place_above(lwin->barSubsurface, surface);

		//Desync so the bar can be updated independently of the main surface
		wl_subsurface_set_desync(lwin->barSubsurface);

		//Wire up pointer input on the bar surface
		if(manager->seat) {
			struct wl_pointer *pointer = wl_seat_get_pointer(manager->seat);
			wl_pointer_add_listener(pointer, &LWindow_barPointerListener, w);
		}
	}
	
	gotoIfError3(clean, ListInputDevice_reserve(&w->devices, 16, Platform_instance->alloc, e_rr));
	gotoIfError3(clean, ListMonitor_reserve(&w->monitors, 16, Platform_instance->alloc, e_rr));

	if(w->hint & EWindowHint_ForceFullscreen)
		gotoIfError3(clean, Window_toggleFullScreen(w, e_rr));

	gotoIfError3(clean, Window_updatePhysicalTitle(w, w->title, e_rr));

	//Initial configure round-trip, triggers Window_updateSize -> LWindow_initSize
	// which also calls LWindow_initBarCorrect to allocate the bar buffer.
	wl_display_roundtrip(manager->display);
	wl_surface_commit(surface);

	//Arm first frame callback
	lwin->frameCallback = wl_surface_frame(surface);
	wl_callback_add_listener(lwin->frameCallback, &LWindow_frameListener, w);
	wl_surface_commit(surface);

	w->flags |= EWindowFlags_IsActive;

	if(w->callbacks.onCreate)
		w->callbacks.onCreate(w);

	w->flags |= EWindowFlags_IsFinalized;

	if(w->callbacks.onResize)
		w->callbacks.onResize(w);

clean:
	return s_uccess;
}
