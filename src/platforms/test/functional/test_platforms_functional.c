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

//platforms/test/functional/test_platforms_functional.c

// Platform functional tests.
//
// These tests require a windowing system and, for the OS-layer input section,
// a human operator.  They are NOT intended for headless CI.
//
// What is covered:
//   F1.  CPU buffer -> screen       (draw a colour gradient, verify buffer content)
//   F2.  Fullscreen toggle          (requires EWindowHint_AllowFullscreen)
//   F3.  Window resize / min + max enforcement
//   F4.  Multi-window               (if the platform allows more than one physical window)
//   F5. Keyboard input, OS layer   (interactive: operator presses ESC)
//   F6.  Window_storeCPUBufferToDisk
//   F7.  Window_updatePhysicalTitle
//   F8.  Mouse, OS layer            (interactive: operator left-clicks)
//   F9.  Focus / minimize cycle     (EWindowFlags_IsMinimized + EWindowFlags_IsFocussed)
//   F10. onTypeChar callback        (interactive: operator types "Hello")
//
// For tests that require visual inspection (F1, F2, F3, F4, F7, F9) the window
// stays open for VISUAL_HOLD_NS so a human can look.
//
// Tests that require a physical window fall back to a virtual window on
// platforms that don't support physical windows (e.g. headless servers).

#include "platforms/platform.h"
#include "platforms/window_manager.h"
#include "platforms/window.h"
#include "platforms/keyboard.h"
#include "platforms/mouse.h"
#include "platforms/file.h"
#include "formats/dds/dds_file.h"
#include "types/container/log.h"
#include "types/test/test.h"
#include "types/base/string_read_helper.h"
#include "types/base/buffer_base.h"
#include "types/base/error.h"
#include "types/base/thread.h"

#include <stdio.h>

//How long to hold a visual window open for human inspection
#define VISUAL_HOLD_NS  (3 * SECOND)

//Windows-only: SendInput / ShowWindow
#if _PLATFORM_TYPE == PLATFORM_WINDOWS
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h>
#elif _PLATFORM_TYPE == PLATFORM_LINUX

	#include <stdlib.h>
	#include "platforms/linux/lwindow_structs.h"

	static inline Bool hasXdotool(void) {
		return system("which xdotool > /dev/null 2>&1") == 0;
	}

#endif

// -- Shared window manager -----------------------------------------------------

static WindowManager windowManager;
static Bool windowManagerReady = false;

static Bool setup(Test *t) {

	WindowManagerCallbacks cbs = (WindowManagerCallbacks) { 0 };
	if (!WindowManager_create(cbs, 0, &windowManager, &t->err)) {
		Test_print(t, "WindowManager_create failed, skipping functional tests");
		return false;
	}

	windowManagerReady = true;
	return true;
}

static void shutdown() {
	if (windowManagerReady) {
		WindowManager_free(&windowManager);
		windowManagerReady = false;
	}
}

//Try to open a physical window; fall back to virtual if unavailable.
static Window *createWindow(
	Test *t,
	const C8 *titleStr,
	I32x2 size,
	I32x2 pos,
	EWindowHint hint,
	EWindowFormat fmt
) {
	Window *w = NULL;

	CharString title = CharString_createRefCStrConst(titleStr);
	I32x2 minSize = EResolution_get(EResolution_SD);
	I32x2 maxSize = I32x2_create2(4096, 4096);

	Bool s_uccess = WindowManager_createWindow(
		&windowManager, EWindowType_Physical, pos, size, minSize, maxSize,
		hint, title, (WindowCallbacks) { 0 }, fmt, 0, &w, &t->err
	);

	if (!s_uccess) {

		s_uccess = WindowManager_createWindow(
			&windowManager, EWindowType_Virtual, pos, size, minSize, maxSize,
			hint, title, (WindowCallbacks) { 0 }, fmt, 0, &w, &t->err
		);

		if (s_uccess)
			Test_print(t, "[fallback] using virtual window");
	}

	return w;
}

//Same as createWindow but accepts a custom WindowCallbacks.
static Window *createWindowCallback(
	Test *t,
	const C8 *titleStr,
	I32x2 pos,
	I32x2 size,
	EWindowHint hint,
	EWindowFormat fmt,
	WindowCallbacks cbs
) {
	Window *w = NULL;

	CharString title = CharString_createRefCStrConst(titleStr);
	I32x2 minSize = EResolution_get(EResolution_SD);
	I32x2 maxSize = I32x2_create2(4096, 4096);

	Bool s_uccess = WindowManager_createWindow(
		&windowManager, EWindowType_Physical, pos, size, minSize, maxSize,
		hint, title, cbs, fmt, 0, &w, &t->err
	);

	if (!s_uccess) {

		s_uccess = WindowManager_createWindow(
			&windowManager, EWindowType_Virtual, pos, size, minSize, maxSize,
			hint, title, cbs, fmt, 0, &w, &t->err
		);

		if (s_uccess)
			Test_print(t, "[fallback] using virtual window");
	}

	return w;
}

//Pump the window manager for up to `ns` nanoseconds.
static void pump(Ns ns) {
	if (!windowManagerReady) return;
	Ns deadline = ns;
	while (deadline > 0) {
		WindowManager_step(&windowManager, NULL, NULL);
		Ns step = 16 * MS;
		Thread_sleep(step);
		if (deadline <= step) break;
		deadline -= step;
	}
}

//Shared render code to validate fullscreen / resize behavior

static U8 *Test_renderPattern(Window *w, U8 zxor) {

	U32 W = (U32)I32x2_x(w->size);
	U32 H = (U32)I32x2_y(w->size);

	U8 *px = w->cpuVisibleBuffer.ptrNonConst;

	if(!px)
		return NULL;

	for (U32 y = 0; y < H; ++y)
		for (U32 x = 0; x < W; ++x) {
			U8 *p = px + (y * W + x) * 4;
			p[0] = (U8)x;           //R gradient
			p[1] = (U8)y;           //G gradient
			p[2] = 128 ^ zxor;      //B constant baseline
			p[3] = 255;             //A
		}

	return px;
}

// -- F1. CPU buffer -> screen ---------------------------------------------------

static void Test_cpuBuffer(Test *t) {

	Test_setModule(t, "F1/CPUBuffer");

	I32x2 sz = I32x2_create2(256, 256);

	Window *w = createWindow(t, "F1: CPU Buffer", sz, I32x2_zero, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8);

	if (!Test_assert(t, "windowCreated", w != NULL))
		goto clean;

	sz = w->size;
	if (!Test_assert(t, "resizeCPU", Window_resizeCPUBuffer(w, false, sz, &t->err)))
		goto clean;

	{
		U32 W = (U32)I32x2_x(sz);
		U32 H = (U32)I32x2_y(sz);
		U64 expectedBytes = (U64)W * H * 4;

		Test_assert(t, "bufferSize", Buffer_length(w->cpuVisibleBuffer) == expectedBytes);
		U8 *px = Test_renderPattern(w, 0);

		Test_assert(t, "has px", px);

		if(!px)
			goto clean;

		U8 *tl = px;
		U8 *tr = px + (W - 1) * 4;
		U8 *bl = px + (H - 1) * W * 4;
		U8 *br = px + ((H - 1) * W + (W - 1)) * 4;

		Test_assert(t, "topLeft_R",  tl[0] == 0);
		Test_assert(t, "topLeft_G",  tl[1] == 0);
		Test_assert(t, "topLeft_B",  tl[2] == 128);
		Test_assert(t, "topRight_R", tr[0] == (U8)(W - 1));
		Test_assert(t, "topRight_G", tr[1] == 0);
		Test_assert(t, "botLeft_R",  bl[0] == 0);
		Test_assert(t, "botLeft_G",  bl[1] == (U8)(H - 1));
		Test_assert(t, "botRight_R", br[0] == (U8)(W - 1));
		Test_assert(t, "botRight_G", br[1] == (U8)(H - 1));
	}

	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(VISUAL_HOLD_NS);

clean:
	if (w) WindowManager_freeWindow(&windowManager, &w);
}

// -- F2. Fullscreen toggle -----------------------------------------------------

static void Test_fullScreen(Test *t) {

	Test_setModule(t, "F2/Fullscreen");

	I32x2 sz = I32x2_create2(256, 256);

	Window *w = createWindow(
		t, "F2: Fullscreen", sz, I32x2_zero, EWindowHint_AllowFullscreen | EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8
	);

	if (!Test_assert(t, "windowCreated", w != NULL))
		goto clean;

	Test_assert(t, "notFullscreenInit", !Window_isFullScreen(w));

	Test_renderPattern(w, 0);
	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(1 * SECOND);

	if (!Test_assert(t, "toggleOn", Window_toggleFullScreen(w, &t->err)))
		goto clean;

	Test_renderPattern(w, 0x80);
	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(1 * SECOND);

	Test_assert(t, "isFullscreen", Window_isFullScreen(w));

	if (!Test_assert(t, "toggleOff", Window_toggleFullScreen(w, &t->err)))
		goto clean;

	Test_renderPattern(w, 0xFF);
	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(1 * SECOND);

	Test_assert(t, "notFullscreenAgain", !Window_isFullScreen(w));

clean:
	if (w) WindowManager_freeWindow(&windowManager, &w);
}

// -- F3. Resize / min+max enforcement -----------------------------------------

static void Test_resize(Test *t) {

	Test_setModule(t, "F3/Resize");

	I32x2 sz    = I32x2_create2(640, 480);
	I32x2 minSize = EResolution_get(EResolution_SD);
	I32x2 maxSize = I32x2_create2(1920, 1080);
	Window *w   = NULL;

	CharString title = CharString_createRefCStrConst("F3: Resize");
	I32x2 pos = I32x2_create2(50, 50);

	WindowManager_createWindow(
		&windowManager, EWindowType_Virtual, pos, sz, minSize, maxSize,
		EWindowHint_None, title, (WindowCallbacks) { 0 },
		EWindowFormat_AutoRGBA8, 0, &w, &t->err
	);

	if (!Test_assert(t, "windowCreated", w != NULL))
		goto clean;

	Test_assert(t, "initW", I32x2_x(w->size) == 640);
	Test_assert(t, "initH", I32x2_y(w->size) == 480);
	Test_assert(t, "minW",  I32x2_x(w->minSize) == I32x2_x(EResolution_get(EResolution_SD)));
	Test_assert(t, "maxW",  I32x2_x(w->maxSize) == 1920);

	I32x2 newSz = I32x2_create2(512, 384);
	if (!Test_assert(t, "resizeCPU", Window_resizeCPUBuffer(w, false, newSz, &t->err)))
		goto clean;

	Test_assert(t, "newW", I32x2_x(w->size) == 512);
	Test_assert(t, "newH", I32x2_y(w->size) == 384);

	pump(VISUAL_HOLD_NS);

clean:
	if (w) WindowManager_freeWindow(&windowManager, &w);
}

// -- F4. Multi-window ----------------------------------------------------------

static void Test_multiWindow(Test *t) {

	Test_setModule(t, "F4/MultiWindow");

	Window *w1 = NULL, *w2 = NULL;
	I32x2 sz = I32x2_create2(256, 256);

	w1 = createWindow(t, "F4: Window A", sz, I32x2_zero, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8);
	w2 = createWindow(t, "F4: Window B", sz, sz, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8);

	if (!Test_assert(t, "w1Created", w1 != NULL))
		goto clean;

	if (!Test_assert(t, "w2Created", w2 != NULL))
		goto clean;

	Test_renderPattern(w1, 0x00);
	Test_assert(t, "present", Window_presentPhysical(w1, &t->err));

	Test_renderPattern(w2, 0x80);
	Test_assert(t, "present", Window_presentPhysical(w2, &t->err));

	Test_assert(t, "distinct",    w1 != w2);
	Test_assert(t, "sameOwner",   w1->owner == w2->owner);
	Test_assert(t, "managerHas2", windowManager.windows.length >= 2);

	pump(VISUAL_HOLD_NS);

clean:
	if (w1) WindowManager_freeWindow(&windowManager, &w1);
	if (w2) WindowManager_freeWindow(&windowManager, &w2);
}

// -- F5. Keyboard - OS layer (interactive) -----------------------------------

static volatile Bool escPressed = false;

static void onDeviceButton(Window *w, InputDevice *dev, InputHandle h, Bool down) {
	(void)w;
	if (dev->type == EInputDeviceType_Keyboard && down) {
		U16 local = InputDevice_getLocalHandle(dev, h);
		if (local == (U32)EKey_Escape)
			escPressed = true;
	}
}

static void Test_keyboard(Test *t) {

	Test_setModule(t, "F5/Keyboard");

	Window *w = NULL;

	WindowCallbacks wcbs = (WindowCallbacks) { 0 };
	wcbs.onDeviceButton = onDeviceButton;

	CharString title = CharString_createRefCStrConst("F5b: Press ESC to pass");
	I32x2 pos = I32x2_create2(200, 200);
	I32x2 sz  = I32x2_create2(640, 100);
	I32x2 minSize = EResolution_get(EResolution_SD);
	I32x2 maxSize = I32x2_create2(4096, 4096);

	Bool s_uccess = WindowManager_createWindow(
		&windowManager, EWindowType_Physical, pos, sz, minSize, maxSize,
		EWindowHint_ProvideCPUBuffer, title, wcbs, EWindowFormat_AutoRGBA8, 0, &w, &t->err
	);

	if (!s_uccess) {
		Test_print(t, "OS-layer keyboard test requires a physical window, skipped");
		goto clean;
	}

	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(300 * MS);
	
	#if _PLATFORM_TYPE == PLATFORM_WINDOWS

		{
			INPUT input[2] = { 0 };
			input[0].type = INPUT_KEYBOARD; input[0].ki.wVk = VK_ESCAPE;
			input[1].type = INPUT_KEYBOARD; input[1].ki.wVk = VK_ESCAPE;
			input[1].ki.dwFlags = KEYEVENTF_KEYUP;
			SendInput(2, input, sizeof(INPUT));

			Ns waited = 0;
			while (!escPressed && waited < 1 * SECOND) {
				WindowManager_step(&windowManager, NULL, NULL);
				Thread_sleep(16 * MS);
				waited += 16 * MS;
			}
			Test_assert(t, "syntheticESC", escPressed);
			escPressed = false;
		}

	#elif _PLATFORM_TYPE == PLATFORM_LINUX

		if (hasXdotool()) {

			//Focus the window by its title, then send the key

			system("xdotool search --name 'F5b: Press ESC to pass' windowfocus key Escape");

			Ns waited = 0;

			while (!escPressed && waited < 1 * SECOND) {
				WindowManager_step(&windowManager, NULL, NULL);
				Thread_sleep(16 * MS);
				waited += 16 * MS;
			}

			if (escPressed)
				Test_assert(t, "syntheticESC", true);

			else Test_print(t, "WARN: xdotool ESC injection didn't fire within timeout");

			escPressed = false;
		}

		else Test_print(t, "xdotool not available, skipping synthetic ESC injection");

	#else
		Test_print(t, "SendInput not available on this platform, skipping synthetic OS injection");
	#endif

	Test_print(t, ">>> INTERACTIVE: Press ESC in the window (5s timeout) <<<");
	Ns waited = 0;
	while (!escPressed && waited < 5 * SECOND) {
		WindowManager_step(&windowManager, NULL, NULL);
		Thread_sleep(16 * MS);
		waited += 16 * MS;
	}

	if (!escPressed)
		Test_print(t, "WARN: ESC not received within timeout");

	Test_assert(t, "operatorESC", escPressed);

clean:
	if (w) WindowManager_freeWindow(&windowManager, &w);
}

// -- F6. Window_storeCPUBufferToDisk -------------------------------------------

static void Test_storeCPUBuffer(Test *t) {
	
	Test_setModule(t, "F6/StoreCPUBufferToDisk");
 
	const Allocator *alloc = Platform_instance->alloc;
 
	I32x2 sz = I32x2_create2(512, 512);
 
	Window *w = createWindow(t, "F6: StoreToDisk", sz, I32x2_zero, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8);

	if(w)
		sz = w->size;

	U32 W = (U32)I32x2_x(sz), H = (U32)I32x2_y(sz);

	ListSubResourceData subResources = (ListSubResourceData) { 0 };
	StreamRef *readStream = NULL;

	RefPtrType fileHandleType = FileHandle_makeType(alloc);
	RefPtrType streamType = FileStream_makeType(alloc);
 
	if (!Test_assert(t, "windowCreated", w != NULL))
		goto clean;
 
	// Fill: pixel i -> R=(i&0xFF), G=((i>>1)&0xFF), B=42, A=255
	{
		U8 *px = w->cpuVisibleBuffer.ptrNonConst;
		for (U32 i = 0; i < W * H; ++i) {
			px[i * 4 + 0] = (U8)(i & 0xFF);
			px[i * 4 + 1] = (U8)((i >> 1) & 0xFF);
			px[i * 4 + 2] = 42;
			px[i * 4 + 3] = 255;
		}
	}
 
	CharString outPath = CharString_createRefCStrConst("platform_test_cpu_dump.dds");
	File_remove(&outPath, 1 * SECOND, alloc, NULL);
 
	if (!Test_assert(t, "storeToDisk", Window_storeCPUBufferToDisk(w, outPath, 50 * MS, alloc, &t->err)))
		goto clean;
 
	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(VISUAL_HOLD_NS);

	Test_assert(t, "fileExists", File_hasFile(&outPath, alloc));
 
	//Open the DDS file as a stream and read it back
 
	if (!Test_assert(t, "openStream", File_openStream(
		&outPath,
		50 * MS,
		EFileOpenType_Read,
		false,
		&fileHandleType,
		&streamType,
		&readStream,
		&t->err
	)))
		goto clean;
 
	//DDS_read: verify header (DDSInfo)
 
	DDSInfo info = (DDSInfo){ 0 };
	U64 streamOff = 0;
 
	if (!Test_assert(t, "ddsRead", DDS_read(readStream, &streamOff, &info, alloc, &subResources, &t->err)))
		goto clean;
 
	Test_assert(t, "ddsW",      info.w    == W);
	Test_assert(t, "ddsH",      info.h    == H);
	Test_assert(t, "ddsMips",   info.mips == 1);
	Test_assert(t, "ddsLayers", info.layers == 1);
 
	//Window_storeCPUBufferToDisk maps RGBA8 -> BGRA8 in the DDSInfo
	//(see the switch in the implementation; RGBA8 falls through to the default BGRA8 case).
	Test_assert(t, "ddsFormat",
		info.textureFormatId == ETextureFormatId_BGRA8 ||
		info.textureFormatId == ETextureFormatId_RGBA8
	);
 
	//Pixel spot-check via the sub-resource stream
	//DDS_read returns one SubResourceData per mip/layer.
	// For a 512x 1-mip 1-layer image there is exactly one entry.
 
	Test_assert(t, "oneSubResource", subResources.length == 1);
 
	if (subResources.length >= 1) {
 
		const SubResourceData *sr = subResources.ptr;
		U64 pixelBytes = 4;   // BGRA8 / RGBA8, 4 bytes per pixel
 
		//Read pixel 0 (top-left) from the stream at sr->streamOff
		U32 px0 = 0;
		Buffer px0Buf = Buffer_createRef(&px0, sizeof(px0));
		OxStream *stream = RefPtr_data(sr->stream, OxStream);
 
		//We wrote R=0,G=0,B=42,A=255 at pixel 0.
		//storeCPUBufferToDisk maps the window format to BGRA8 by default,
		//so on-disk order is B,G,R,A -> 42,0,0,255.
		//If the format stayed RGBA8 the order is R,G,B,A -> 0,0,42,255.
		//Accept either.
		if (Test_assert(t, "readPx0", stream->read(stream, sr->streamOff, sizeof(px0), px0Buf, alloc, &t->err)))
			Test_assert(t, "px0", px0 == 0xFF2A0000 || px0 == 0xFF00002A);
 
		//Read pixel 1 and verify R is 1 (in either BGR or RGB)
		U32 px1 = 0;
		Buffer px1Buf = Buffer_createRef(&px1, sizeof(px1));
		if (Test_assert(t, "readPx1", stream->read(stream, sr->streamOff + pixelBytes, sizeof(px1), px1Buf, alloc, &t->err)))
			Test_assert(t, "px1", px1 == 0xFF2A0001 || px1 == 0xFF01002A);
 
		//Verify the reported stream length covers the full image
		U64 expectedBytes = (U64)W * H * pixelBytes;
		Test_assert(t, "streamLen", sr->streamLen == expectedBytes);
	}
 
	File_remove(&outPath, 1 * SECOND, alloc, NULL);
 
clean:
	ListSubResourceData_freeUnderlying(&subResources, alloc);
	RefPtr_dec(&readStream);
	if (w) WindowManager_freeWindow(&windowManager, &w);
}

// -- F7. Window_updatePhysicalTitle --------------------------------------------

static void Test_updateTitle(Test *t) {

	Test_setModule(t, "F7/UpdateTitle");

	Error err = Error_none();
	I32x2 sz = I32x2_create2(480, 80);

	Window *w = createWindow(t, "F7: Title, original", sz, I32x2_zero, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8);

	if (!Test_assert(t, "windowCreated", w != NULL))
		goto clean;

	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(300 * MS);

	CharString t2 = CharString_createRefCStrConst("F7: Title, updated (check me)");
	Test_assert(t, "update1", Window_updatePhysicalTitle(w, t2, &err));
	pump(1 * SECOND);

	CharString t3 = CharString_createRefCStrConst("F7: Title, updated again");
	Test_assert(t, "update2", Window_updatePhysicalTitle(w, t3, &err));
	pump(1 * SECOND);

	Test_assert(t, "nullWindow", !Window_updatePhysicalTitle(NULL, t3, NULL));

clean:
	if (w) WindowManager_freeWindow(&windowManager, &w);
}

// -- F8. Mouse - OS layer (interactive) ---------------------------------------

static volatile Bool leftClicked = false;

static void onMouseButton(Window *w, InputDevice *dev, InputHandle h, Bool down) {

	(void)w;

	if (dev->type != EInputDeviceType_Mouse || !down)
		return;

	U16 local = InputDevice_getLocalHandle(dev, h);
	if (local == (U16)(EMouseButton_Left - EMouseAxis_End))
		leftClicked = true;
}

static void Test_mouse(Test *t) {

	Test_setModule(t, "F8/Mouse/OS");

	Window *w = NULL;

	WindowCallbacks wcbs = (WindowCallbacks) { 0 };
	wcbs.onDeviceButton = onMouseButton;

	CharString title = CharString_createRefCStrConst("F8: Left-click anywhere to pass");
	I32x2 pos = I32x2_create2(200, 350);
	I32x2 sz  = I32x2_create2(640, 100);
	I32x2 minSize = EResolution_get(EResolution_SD);
	I32x2 maxSize = I32x2_create2(4096, 4096);

	Bool s_uccess = WindowManager_createWindow(
		&windowManager, EWindowType_Physical, pos, sz, minSize, maxSize,
		EWindowHint_ProvideCPUBuffer, title, wcbs, EWindowFormat_AutoRGBA8, 0, &w, &t->err
	);

	if (!s_uccess) {
		Test_print(t, "OS-layer mouse test requires a physical window, skipped");
		goto clean;
	}

	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(300 * MS);

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS

		{
			POINT pt = { 200 + 320, 350 + 50 };
			SetCursorPos(pt.x, pt.y);

			INPUT input[2] = { 0 };
			input[0].type = INPUT_MOUSE; input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
			input[1].type = INPUT_MOUSE; input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
			SendInput(2, input, sizeof(INPUT));

			Ns waited = 0;
			while (!leftClicked && waited < 1 * SECOND) {
				WindowManager_step(&windowManager, NULL, NULL);
				Thread_sleep(16 * MS);
				waited += 16 * MS;
			}
			Test_assert(t, "syntheticClick", leftClicked);
			leftClicked = false;
		}

	#elif _PLATFORM_TYPE == PLATFORM_LINUX

		if (hasXdotool()) {

			system("xdotool search --name 'F9: Left-click anywhere to pass' windowfocus click 1");

			Ns waited = 0;

			while (!leftClicked && waited < 1 * SECOND) {
				WindowManager_step(&windowManager, NULL, NULL);
				Thread_sleep(16 * MS);
				waited += 16 * MS;
			}

			if (leftClicked)
				Test_assert(t, "syntheticClick", true);

			else Test_print(t, "WARN: xdotool click injection didn't fire within timeout");

			leftClicked = false;
		}
		
		else Test_print(t, "xdotool not available, skipping synthetic mouse injection");
		
	#else
		Test_print(t, "SendInput not available, skipping synthetic mouse OS injection");
	#endif

	Test_print(t, ">>> INTERACTIVE: Left-click anywhere in the window (5s timeout) <<<");
	Ns waited = 0;
	while (!leftClicked && waited < 5 * SECOND) {
		WindowManager_step(&windowManager, NULL, NULL);
		Thread_sleep(16 * MS);
		waited += 16 * MS;
	}

	if (!leftClicked)
		Test_print(t, "WARN: left click not received within timeout");

	Test_assert(t, "operatorClick", leftClicked);

clean:
	if (w) WindowManager_freeWindow(&windowManager, &w);
}

// -- F9. Focus / minimize cycle -----------------------------------------------
//
// Programmatically minimizes and restores the window using OS calls, then verifies
// EWindowFlags_IsMinimized and EWindowFlags_IsFocussed are updated by the platform
// layer after each pump.
//
// On virtual windows we can only check that the helpers don't crash and that the
// initial state is sane (not minimized, flags consistent), since there is no
// compositor to drive the flag changes.

static void Test_focusMinimize(Test *t) {

	Test_setModule(t, "F9/FocusMinimize");

	I32x2 sz = I32x2_create2(640, 200);

	Window *w = createWindow(
		t, "F9: Minimize / restore, watch me", sz, I32x2_zero, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8
	);

	if (!Test_assert(t, "windowCreated", w != NULL))
		goto clean;

	//Initial state: not minimized

	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(300 * MS);

	Test_assert(t, "notMinimizedInit", !Window_isMinimized(w));

	Bool isPhysical = w->type == EWindowType_Physical;

	if (!isPhysical) {
		//Virtual window: flags can't be driven by the OS.
		//Just verify helpers are stable and non-crashing.
		Test_print(t, "[virtual] skipping OS-driven focus/minimize assertions");
		Test_assert(t, "virtualNotMinimized", !Window_isMinimized(w));
		goto clean;
	}

	// -- Minimize --------------------------------------------------------------
	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		ShowWindow((HWND)w->nativeHandle, SW_MINIMIZE);
	#elif _PLATFORM_TYPE == PLATFORM_LINUX
		{
			LWindow        *lwin    = (LWindow*) w->nativeData;
			LWindowManager *manager = (LWindowManager*)w->owner->platformData.ptr;
			xdg_toplevel_set_minimized(lwin->topLevel);
			wl_display_flush(manager->display);
		}
	#endif

	pump(500 * MS);

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS || _PLATFORM_TYPE == PLATFORM_LINUX
		Test_assert(t, "isMinimized", Window_isMinimized(w));
		//Focus must have left when we minimized
		Test_assert(t, "notFocusedWhileMin", !Window_isFocussed(w));
	#else
		//Soft: log the observed state but don't hard-fail on platforms where
		//the OS may not drive the flag synchronously.
		if (!Window_isMinimized(w))
			Test_print(t, "WARN: IsMinimized not set after minimize request");
	#endif

	// -- Restore ---------------------------------------------------------------
	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		ShowWindow((HWND)w->nativeHandle, SW_RESTORE);
		SetForegroundWindow((HWND)w->nativeHandle);
	#elif _PLATFORM_TYPE == PLATFORM_LINUX
		//There is no xdg_toplevel "unset_minimized". The only way to restore
		//a minimized window is through the compositor UI or a tool like xdotool.
		if (hasXdotool()) {
			system("xdotool search --name 'F9:' windowactivate --sync");
			LWindowManager *manager = (LWindowManager*)w->owner->platformData.ptr;
			wl_display_flush(manager->display);
		}
	#endif

	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(500 * MS);

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS || _PLATFORM_TYPE == PLATFORM_LINUX
		Test_assert(t, "notMinimizedAfterRestore", !Window_isMinimized(w));
		Test_assert(t, "focusedAfterRestore",       Window_isFocussed(w));
	#endif

	//Hold so the operator can see it come back
	pump(VISUAL_HOLD_NS);

clean:
	if (w) WindowManager_freeWindow(&windowManager, &w);
}

// -- F10. onTypeChar callback --------------------------------------------------
//
// Opens a window and waits for the operator to type the word "Hello" (5 chars).
// The onTypeChar callback accumulates each CharString fragment; we concatenate
// them and check the result contains "Hello".
//
// On Windows a synthetic round-trip is attempted first via SendInput VK codes
// (Shift+H, e, l, l, o) so the test is not purely interactive.
//
// NOTE: onTypeChar delivers OS-level text input (after IME / layout mapping),
// not raw scancodes.  The 'H' therefore requires a Shift modifier injected
// alongside it.

// Accumulated typed text across callback invocations.
static CharString typedText;

static void onTypeChar(Window *w, CharString str) {
	(void)w;
	CharString_appendString(&typedText, &str, Platform_instance->alloc, NULL);
}

static void Test_typeChar(Test *t) {

	Test_setModule(t, "F10/TypeChar");

	typedText  = CharString_createNull();

	WindowCallbacks wcbs = (WindowCallbacks) { 0 };
	wcbs.onTypeChar = onTypeChar;

	I32x2 pos = I32x2_create2(200, 500);
	I32x2 sz  = I32x2_create2(640, 100);

	Window *w = createWindowCallback(
		t, "F10: Type \"Hello\" to pass", pos, sz, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, wcbs
	);

	if (!Test_assert(t, "windowCreated", w != NULL))
		goto clean;
	
	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(300 * MS);

	Bool isPhysical = w->type == EWindowType_Physical;

	if (!isPhysical) {
		Test_print(t, "[virtual] onTypeChar requires a physical window, skipped");
		goto clean;
	}

	const CharString hello = CharString_createRefCStrConst("Hello");

	// -- Synthetic injection (Windows) ----------------------------------------
	#if _PLATFORM_TYPE == PLATFORM_WINDOWS

		{
			//Bring our window to the foreground so WM_CHAR is routed to it.
			SetForegroundWindow((HWND)w->nativeHandle);
			pump(200 * MS);

			// H (Shift down, H down, H up, Shift up), e, l, l, o
			// Each key: down then up.
			struct { WORD vk; Bool shift; } keys[] = {
				{ 'H', true  },
				{ 'E', false },
				{ 'L', false },
				{ 'L', false },
				{ 'O', false },
			};

			for (U32 i = 0; i < 5; ++i) {

				INPUT inputs[4] = { 0 };
				U32 count = 0;

				if (keys[i].shift) {
					inputs[count].type   = INPUT_KEYBOARD;
					inputs[count].ki.wVk = VK_SHIFT;
					++count;
				}

				inputs[count].type   = INPUT_KEYBOARD;
				inputs[count].ki.wVk = keys[i].vk;
				++count;

				inputs[count].type         = INPUT_KEYBOARD;
				inputs[count].ki.wVk       = keys[i].vk;
				inputs[count].ki.dwFlags   = KEYEVENTF_KEYUP;
				++count;

				if (keys[i].shift) {
					inputs[count].type         = INPUT_KEYBOARD;
					inputs[count].ki.wVk       = VK_SHIFT;
					inputs[count].ki.dwFlags   = KEYEVENTF_KEYUP;
					++count;
				}

				SendInput(count, inputs, sizeof(INPUT));
			}

			pump(300 * MS);

			Bool syntheticOK = CharString_containsStringSensitive(&typedText, &hello, 0, 0);

			if (!syntheticOK)
				Test_print(t, "WARN: synthetic typeChar didn't produce 'Hello', may be layout-dependent");

			else Test_assert(t, "syntheticHello", syntheticOK);

			//Reset for interactive round
			CharString_free(&typedText, t->alloc);
			typedText = CharString_createNull();
		}

	#elif _PLATFORM_TYPE == PLATFORM_LINUX

		if (hasXdotool()) {

			//SetForegroundWindow equivalent via xdotool, then type

			system("xdotool search --name 'F10: Type' windowfocus type --clearmodifiers 'Hello'");
			pump(300 * MS);

			Bool syntheticOK = CharString_containsStringSensitive(&typedText, &hello, 0, 0);

			if (!syntheticOK)
				Test_print(t, "WARN: xdotool type didn't produce 'Hello', may be layout-dependent");

			else Test_assert(t, "syntheticHello", syntheticOK);

			CharString_free(&typedText, t->alloc);
			typedText = CharString_createNull();
		}
		
		else Test_print(t, "xdotool not available, skipping synthetic typeChar injection");

	#else
		Test_print(t, "Synthetic typeChar injection not implemented for this platform");
	#endif

	// -- Interactive -----------------------------------------------------------
	Test_print(t, ">>> INTERACTIVE: Click the window and type \"Hello\" (8s timeout) <<<");

	Ns waited = 0;
	while (waited < 8 * SECOND) {
		WindowManager_step(&windowManager, NULL, NULL);
		Thread_sleep(16 * MS);
		waited += 16 * MS;

		//Pass as soon as "Hello" appears anywhere in the accumulated text
		if (CharString_containsStringSensitive(&typedText, &hello, 0, 0))
			break;
	}

	Bool gotHello = CharString_containsStringSensitive(&typedText, &hello, 0, 0);

	if (!gotHello)
		Test_print(t, "WARN: 'Hello' not received within timeout");

	Test_assert(t, "operatorHello", gotHello);

clean:
	CharString_free(&typedText, t->alloc);
	if (w) WindowManager_freeWindow(&windowManager, &w);
}

// -- F11. Input - Focus Lost Reset ---------------------------------------------

static volatile Bool focusResetTriggered = false;

static void onButtonReset(Window *w, InputDevice *dev, InputHandle h, Bool down) {
	(void)w; (void)dev; (void) h;
	if (!down) focusResetTriggered = true; //Reset callback fired
}

static void Test_focusReset(Test *t) {

	Test_setModule(t, "F11/FocusReset");

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS || _PLATFORM_TYPE == PLATFORM_LINUX

		WindowCallbacks cbs = (WindowCallbacks) { 0 };
		cbs.onDeviceButton = onButtonReset;

		Window *w = createWindowCallback(
			t, "F11: FocusReset",
			I32x2_create2(200, 200), I32x2_create2(300, 300),
			EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8,
			cbs
		);

		if (!Test_assert(t, "windowCreated", w != NULL))
			goto clean;
	
		Test_assert(t, "present", Window_presentPhysical(w, &t->err));
		pump(300 * MS);

		if (w->type != EWindowType_Physical) {
			Test_print(t, "[virtual] focus reset requires a physical window, skipped");
			goto clean;
		}

		pump(300 * MS);   //Let the window settle and receive focus

		#if _PLATFORM_TYPE == PLATFORM_WINDOWS

			{
				Keyboard *kb = (Keyboard*) &w->devices.ptrNonConst[w->defaultKeyboardId];
				InputHandle hEsc = InputDevice_createHandle(kb, EKey_Escape, EInputType_Button);
				InputDevice_setCurrentState(kb, hEsc, true);

				//Force focus loss
				SendMessageW((HWND)w->nativeHandle, WM_KILLFOCUS, 0, 0);
				WindowManager_step(&windowManager, NULL, NULL);

				Test_assert(t, "resetTriggered", focusResetTriggered);
				Test_assert(t, "stateCleared",   !InputDevice_getCurrentState(kb, hEsc));
			}

		#elif _PLATFORM_TYPE == PLATFORM_LINUX

			if (hasXdotool()) {

				//Press and hold a key via xdotool so the platform layer records it as down.
				//Then steal focus, LWindow_kbLeave should fire and clear all button states.
				//We inject at the Wayland level rather than via InputDevice directly, so the
				//state actually enters through the real event path.

				system("xdotool search --name 'F11:' windowfocus key --clearmodifiers Escape");
				pump(200 * MS);

				Keyboard *kb = (Keyboard*) &w->devices.ptrNonConst[w->defaultKeyboardId];
				InputHandle hEsc = InputDevice_createHandle(kb, EKey_Escape, EInputType_Button);

				Test_assert(t, "stateSet", InputDevice_getCurrentState(kb, hEsc));

				//Now steal focus by activating a different window (the desktop / root)
				system("xdotool key super");   //tap Super to shift focus away
				pump(300 * MS);

				//After focus loss all keys should be cleared by LWindow_kbLeave
				Test_assert(t, "stateCleared", !InputDevice_getCurrentState(kb, hEsc));

				//focusResetTriggered is set by the onDeviceButton(down=false) callback
				//which LWindow_kbLeave fires for every previously-down key
				Test_assert(t, "resetTriggered", focusResetTriggered);
			}
			
			else Test_print(t, "xdotool not available, skipping synthetic focus-reset injection");

		#endif

	clean:
		focusResetTriggered = false;
		if (w) WindowManager_freeWindow(&windowManager, &w);

	#else
		(void) onButtonReset;
	#endif
}

static void Test_monitorInfo(Test *t) {

	Test_setModule(t, "F12/MonitorInfo");

	I32x2 sz = I32x2_create2(320, 240);
	Window *w = createWindow(
		t, "F12: Monitor info - check console output",
		sz, I32x2_zero, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8
	);

	if (!Test_assert(t, "windowCreated", w != NULL))
		goto clean;

	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(300 * MS);

	Test_assert(t, "hasMonitor",   w->monitors.length >= 1);

	for (U64 i = 0; i < w->monitors.length; ++i) {

		const Monitor *m = &w->monitors.ptr[i];
		Test_assert(t, "monitorW",       I32x2_x(m->sizePixels) > 0);
		Test_assert(t, "monitorH",       I32x2_y(m->sizePixels) > 0);
		Test_assert(t, "refreshRate",    m->refreshRate > 0.f);

		//Print for operator to visually verify
		Test_print(t, "Monitor info (verify matches your display settings):");

		#define REMAP_OFF_XY(x, y) (((x) + 1) | (((y) + 1) << 2))

		#define REMAP_RGB_OFF(r, g, b) (                  \
			REMAP_OFF_XY(I32x2_x(r), I32x2_y(r)) |        \
			(REMAP_OFF_XY(I32x2_x(r), I32x2_y(r)) << 4) | \
			(REMAP_OFF_XY(I32x2_x(r), I32x2_y(r)) << 8)   \
		)

		U16 rgb16 = REMAP_RGB_OFF(m->offsetR, m->offsetG, m->offsetB);

		const C8 *spName = "None";

		if(rgb16 == REMAP_RGB_OFF(I32x2_create2(-1, 0), I32x2_create2(0, 0), I32x2_create2(1, 0)))
			spName = "Horizontal RGB";

		else if(rgb16 == REMAP_RGB_OFF(I32x2_create2(1, 0), I32x2_create2(0, 0), I32x2_create2(-1, 0)))
			spName = "Horizontal BGR";

		else if(rgb16 == REMAP_RGB_OFF(I32x2_create2(0, 1), I32x2_create2(0, 0), I32x2_create2(0, -1)))
			spName = "Vertical RGB";

		else if(rgb16 == REMAP_RGB_OFF(I32x2_create2(0, -1), I32x2_create2(0, 0), I32x2_create2(0, 1)))
			spName = "Vertical BGR";

		Log_debugLn(Platform_instance->alloc,
			"-- Monitor %"PRIu64": %"PRIi32"x%"PRIi32" @ %.1f Hz, offset (%"PRIi32",%"PRIi32"), size %"PRIi32"x%"PRIi32" mm\n"
			"--- Subpixel: %s",
			(U64)i,
			I32x2_x(m->sizePixels), I32x2_y(m->sizePixels),
			m->refreshRate,
			I32x2_x(m->offsetPixels), I32x2_y(m->offsetPixels),
			I32x2_x(m->sizeMm), I32x2_y(m->sizeMm),
			spName
		);
	}

	Test_print(t, ">>> INTERACTIVE: Verify monitor count and resolution match your system (5s) <<<");
	pump(5 * SECOND);

clean:
	if (w) WindowManager_freeWindow(&windowManager, &w);
}

static volatile Bool movedToSecondMonitor = false;

//Check if we are now on a monitor with a non-zero X offset,
// which indicates a second monitor to the right (most common layout).
static void onMonitorChange(Window *w) {
	for (U64 i = 0; i < w->monitors.length; ++i)
		if (
			I32x2_x(w->monitors.ptr[i].offsetPixels) != 0 ||
			I32x2_y(w->monitors.ptr[i].offsetPixels) != 0
		) {
			movedToSecondMonitor = true;
			break;
		}
}

static void Test_windowMove(Test *t) {

	Test_setModule(t, "F13/WindowMove");

	WindowCallbacks cbs = (WindowCallbacks) { 0 };
	cbs.onMonitorChange = onMonitorChange;

	I32x2 sz  = I32x2_create2(320, 240);
	I32x2 pos = I32x2_create2(100, 100);

	Window *w = createWindowCallback(
		t, "F13: Drag me to a second monitor",
		pos, sz, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, cbs
	);

	if (!Test_assert(t, "windowCreated", w != NULL))
		goto clean;

	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(300 * MS);

	if (w->monitors.length < 1) {
		Test_print(t, "No monitors detected, skipping move test");
		goto clean;
	}

	//Synthetic: move window to an offset that would land on a second monitor.
	//On Wayland w->offset is always zero (compositor hides position), so we
	// can only try and then rely on onMonitorChange to confirm it worked.
	#if _PLATFORM_TYPE == PLATFORM_WINDOWS

		//Move to 2560, 100, typical second monitor position for 1920-wide primary
		SetWindowPos((HWND)w->nativeHandle, NULL, 2560, 100, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		pump(500 * MS);

		if (movedToSecondMonitor)
			Test_assert(t, "syntheticMoveToMonitor2", movedToSecondMonitor);

		else Test_print(t, "WARN: synthetic move didn't reach a second monitor (may be single-monitor system)");

		movedToSecondMonitor = false;

	#elif _PLATFORM_TYPE == PLATFORM_LINUX

		if (hasXdotool()) {

			system("xdotool search --name 'F13:' windowmove 2560 100");
			pump(500 * MS);

			if (movedToSecondMonitor)
				Test_assert(t, "syntheticMoveToMonitor2", movedToSecondMonitor);

			else Test_print(t, "WARN: xdotool move didn't reach a second monitor");

			movedToSecondMonitor = false;
		}

	#endif

	Test_print(t, ">>> INTERACTIVE: Drag the window to a second monitor (10s timeout) <<<");
	Test_print(t, "    If you only have one monitor, this test will time out and warn.");

	Ns waited = 0;
	while (!movedToSecondMonitor && waited < 10 * SECOND) {
		WindowManager_step(&windowManager, NULL, NULL);
		Thread_sleep(16 * MS);
		waited += 16 * MS;
	}

	if (!movedToSecondMonitor)
		Test_print(t, "WARN: window not moved to second monitor within timeout (single-monitor system?)");

	//Not a hard failure, single monitor systems are valid
	pump(VISUAL_HOLD_NS);

clean:
	if (w) WindowManager_freeWindow(&windowManager, &w);
}

static void Test_maximize(Test *t) {

	Test_setModule(t, "F14/Maximize");

	I32x2 sz = I32x2_create2(640, 480);

	Window *w = createWindow(
		t, "F14: Maximize / restore, watch me",
		sz, I32x2_zero,
		EWindowHint_AllowFullscreen | EWindowHint_ProvideCPUBuffer,
		EWindowFormat_AutoRGBA8
	);

	if (!Test_assert(t, "windowCreated", w != NULL))
		goto clean;

	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(300 * MS);

	if (w->type != EWindowType_Physical) {
		Test_print(t, "[virtual] maximize test requires a physical window, skipped");
		goto clean;
	}

	I32x2 originalSize = w->size;

	// -- Maximize --

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		ShowWindow((HWND)w->nativeHandle, SW_MAXIMIZE);
	#elif _PLATFORM_TYPE == PLATFORM_LINUX
		{
			LWindow *lwin = (LWindow*) w->nativeData;
			xdg_toplevel_set_maximized(lwin->topLevel);
			wl_display_flush(((LWindowManager*)w->owner->platformData.ptr)->display);
		}
	#endif

	pump(1 * SECOND);

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS || _PLATFORM_TYPE == PLATFORM_LINUX
		Test_assert(t, "sizeGreaterAfterMax",
			I32x2_x(w->size) > I32x2_x(originalSize) ||
			I32x2_y(w->size) > I32x2_y(originalSize)
		);
	#endif

	pump(VISUAL_HOLD_NS);

	// -- Restore --

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		ShowWindow((HWND)w->nativeHandle, SW_RESTORE);
	#elif _PLATFORM_TYPE == PLATFORM_LINUX
		{
			LWindow *lwin = (LWindow*) w->nativeData;
			xdg_toplevel_unset_maximized(lwin->topLevel);
			wl_display_flush(((LWindowManager*)w->owner->platformData.ptr)->display);
		}
	#endif

	pump(1 * SECOND);

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS || _PLATFORM_TYPE == PLATFORM_LINUX
		Test_assert(t, "sizeRestoredAfterUnmax",
			I32x2_x(w->size) <= I32x2_x(originalSize) + 10 &&
			I32x2_y(w->size) <= I32x2_y(originalSize) + 10
		);
	#endif

	pump(VISUAL_HOLD_NS);

clean:
	if (w) WindowManager_freeWindow(&windowManager, &w);
}

// -- F15. Keyboard remap ------------------------------------------------------
//
//Calls Keyboard_remap for EKey_Q W E R T Y to get the layout-specific label
// for each physical key, prints them, then waits for the operator to press
// every one of them.  The EKey values received through the OS input path must
// match exactly, proving that the scan-code -> EKey mapping and Keyboard_remap
// agree for whatever physical layout the operator uses (QWERTY, AZERTY, etc.).
//
//Synthetic injection is intentionally absent: injecting fixed scancodes would
// only test that 0x10-0x15 map to EKey_Q-Y, which F5 already covers. The value
// here is the operator pressing the keys their layout labels show.

#define F15_KEY_COUNT 6
static const EKey F15_keys[F15_KEY_COUNT] = {
	EKey_Q, EKey_W, EKey_E, EKey_R, EKey_T, EKey_Y
};

static volatile U32 f15_pressed;   // bitmask, bit i set when F15_keys[i] received

static void F15_onDeviceButton(Window *w, InputDevice *dev, InputHandle h, Bool down) {
	
	(void) w;
	if (dev->type != EInputDeviceType_Keyboard || !down)
		return;

	U16 local = InputDevice_getLocalHandle(dev, h);
	for (U32 i = 0; i < F15_KEY_COUNT; ++i)
		if (local == (U16) F15_keys[i])
			f15_pressed |= (1u << i);
}

static void Test_keyboardRemap(Test *t) {

	Test_setModule(t, "F15/KeyboardRemap");

	f15_pressed = 0;

	WindowCallbacks cbs = (WindowCallbacks) { 0 };
	cbs.onDeviceButton = F15_onDeviceButton;

	Window *w = createWindowCallback(
		t, "F15: Keyboard remap",
		I32x2_create2(200, 600), I32x2_create2(640, 100),
		EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, cbs
	);

	if (!Test_assert(t, "windowCreated", w != NULL))
		goto clean;
	
	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(300 * MS);

	if (w->type != EWindowType_Physical) {
		Test_print(t, "[virtual] keyboard remap test requires a physical window, skipped");
		goto clean;
	}

	if (w->devices.length <= w->defaultKeyboardId) {
		Test_print(t, "No keyboard device found, skipped");
		goto clean;
	}

	pump(300 * MS);

	//Resolve each EKey to its layout label and build the prompt.
	//On AZERTY, EKey_Q -> 'a', EKey_W -> 'z', etc.
	{
		InputDevice *kb  = &w->devices.ptrNonConst[w->defaultKeyboardId];
		U64 startOff = sizeof("You need to press: ") - 1;
		C8 prompt[128]   = "You need to press: ";
		Bool anyFailed   = false;

		for (U64 i = 0, k = startOff; i < F15_KEY_COUNT; ++i) {

			CharString label = CharString_createNull();
			Error err        = Error_none();

			Bool ok = Keyboard_remap((const Keyboard*) kb, F15_keys[i], Platform_instance->alloc, &label, &err);

			if (ok && CharString_length(label) && k + CharString_length(label) + 2 < sizeof(prompt)) {

				for (U64 j = 0; j < CharString_length(label); ++j)
					prompt[k++] = label.ptr[j];

				prompt[k++] = ' ';
				prompt[k]   = '\0';

			} else {

				//Fallback: print the EKey index if remap fails
				if (k + 3 < sizeof(prompt)) {
					prompt[k++] = '?';
					prompt[k++] = ' ';
					prompt[k]   = '\0';
				}

				anyFailed = true;
			}

			CharString_free(&label, Platform_instance->alloc);
		}

		Test_print(t, prompt);

		if (anyFailed)
			Test_print(t, "WARN: Keyboard_remap failed for one or more keys");
	}

	//Interactive only, see comment at top of function.
	Test_print(t, ">>> INTERACTIVE: Press each key shown above (10s timeout) <<<");

	U32 allBits = (1u << F15_KEY_COUNT) - 1;
	Ns  waited  = 0;

	while ((f15_pressed & allBits) != allBits && waited < 10 * SECOND) {
		WindowManager_step(&windowManager, NULL, NULL);
		Thread_sleep(16 * MS);
		waited += 16 * MS;
	}

	Bool allPressed = (f15_pressed & allBits) == allBits;

	if (!allPressed)
		Test_print(t, "WARN: not all remap keys received within timeout");

	Test_assert(t, "operatorRemap", allPressed);

clean:
	f15_pressed = 0;
	if (w) WindowManager_freeWindow(&windowManager, &w);
}

//Requires a physical window with a visible CSD bar (compositor without SSD,
// e.g. GNOME/Mutter). Opens a window and asks the operator to click each
// decoration button in turn.  Synthetic injection via xdotool click on the
// bar surface is attempted first.
//
//Because the bar is a subsurface we can't use xdotool's window-title search
// to click it directly; instead we click at absolute screen coordinates
// computed from the window position + bar button offsets.
//
//The close button terminates the window (EWindowFlags_ShouldTerminate).
//We catch that in the poll loop and treat it as success rather than letting
// WindowManager_step free the window under us.

#if _PLATFORM_TYPE == PLATFORM_LINUX

	static volatile Bool f16MinimizeSeen = false;
	static volatile Bool f16MaximizeSeen = false;

	static Bool F16_onResize(Window *w, Error *e_rr) {
		(void) e_rr;
		if(!f16MaximizeSeen && I32x2_x(w->size) >= 800)  //Maximize on Wayland sends a configure with a larger size.
			f16MaximizeSeen = true;

		return true;
	}

	static void Test_csdButtons(Test *t) {

		Test_setModule(t, "F16/CSDButtons");

		WindowCallbacks cbs = (WindowCallbacks) { 0 };
		cbs.onResize = F16_onResize;

		I32x2 pos = I32x2_create2(200, 200);
		I32x2 sz  = I32x2_create2(640, 400);

		Window *w = createWindowCallback(
			t, "F16: Click min, max, then close button",
			pos, sz,
			EWindowHint_AllowFullscreen | EWindowHint_ProvideCPUBuffer,
			EWindowFormat_AutoRGBA8, cbs
		);

		if(!Test_assert(t, "windowCreated", w != NULL))
			goto clean;

		Test_assert(t, "present", Window_presentPhysical(w, &t->err));
		pump(500 * MS);

		if(w->type != EWindowType_Physical) {
			Test_print(t, "[virtual] CSD button test requires a physical window, skipped");
			goto clean;
		}

		//Check whether we actually have a CSD bar; if the compositor provided SSD
		// (e.g. KWin) there is no barSurface and this test is not applicable.
		{
			LWindow *lwin = (LWindow*) w->nativeData;
			if(!lwin->barSurface) {
				Test_print(t, "[SSD compositor] no CSD bar present, skipping F16");
				goto clean;
			}
		}

		//----- Synthetic: xdotool clicks at absolute bar coordinates -----
		//Bar sits at the top of the window.  Button layout (right to left):
		//   close  = barWidth - BTN_W / 2          (rightmost)
		//   max    = barWidth - BTN_W * 3 / 2
		//   min    = barWidth - BTN_W * 5 / 2
		//We don't know w->offset on Wayland (always 0), but xdotool mousemove
		// accepts coordinates relative to a window id via --window.
		//We use `xdotool getactivewindow` after focusing by title.

		if(hasXdotool()) {

			//Minimize button

			system(
				"xdotool search --name 'F16:' windowfocus && "
				"WIN=$(xdotool search --name 'F16:') && "
				"GEOM=$(xdotool getwindowgeometry $WIN) && "
				//Click at bar-relative coords: x = width - BTN_W * 5 / 2 + BTN_W / 2,
				// y = BTN_H / 2. We use a fixed offset since we know the bar layout.
				"xdotool mousemove --window $WIN "
					"$(($(xdotool getwindowgeometry --shell $WIN | grep WIDTH | cut -d= -f2) - 161)) 16 "
				"&& xdotool click 1"
			);

			pump(600 * MS);

			if(Window_isMinimized(w))
				f16MinimizeSeen = true;

			else Test_print(t, "WARN: minimize click didn't set IsMinimized (compositor-dependent)");

			system("xdotool search --name 'F16:' windowactivate --sync");    //Restore via windowactivate
			pump(500 * MS);

			//Maximize button (x = width - BTN_W * 3 / 2 + BTN_W / 2 = width - BTN_W)

			system(
				"WIN=$(xdotool search --name 'F16:') && "
				"xdotool mousemove --window $WIN "
					"$(($(xdotool getwindowgeometry --shell $WIN | grep WIDTH | cut -d= -f2) - 115)) 16 "
				"&& xdotool click 1"
			);

			pump(600 * MS);

			if(f16MaximizeSeen)
				Test_assert(t, "syntheticMaximize", true);

			else Test_print(t, "WARN: maximize click didn't produce resize event");

			//Un-maximize

			system(
				"WIN=$(xdotool search --name 'F16:') && "
				"xdotool mousemove --window $WIN "
					"$(($(xdotool getwindowgeometry --shell $WIN | grep WIDTH | cut -d= -f2) - 115)) 16 "
				"&& xdotool click 1"
			);

			pump(500 * MS);
		}

		//Interactive fallback

		Test_print(t, ">>> INTERACTIVE: Click MINIMIZE, then MAXIMIZE, then CLOSE in the title bar (15s) <<<");

		Bool closeSeen = false;
		Ns waited = 0;

		while(waited < 15 * SECOND) {
			WindowManager_step(&windowManager, NULL, NULL);
			Thread_sleep(16 * MS);
			waited += 16 * MS;

			if(!f16MinimizeSeen && Window_isMinimized(w))
				f16MinimizeSeen = true;

			if(w->flags & EWindowFlags_ShouldTerminate) {
				closeSeen = true;
				break;
			}
		}

		if(!f16MinimizeSeen)
			Test_print(t, "WARN: minimize not observed (compositor may not report it)");

		Test_assert(t, "closeButton", closeSeen);

		//Don't call WindowManager_freeWindow here, ShouldTerminate will be
		// handled by the step loop on re-entry; just null out w so clean: skips it.
		if(closeSeen) {
			WindowManager_freeWindow(&windowManager, &w);
			w = NULL;
		}

	clean:
		f16MinimizeSeen = false;
		f16MaximizeSeen = false;
		if(w) WindowManager_freeWindow(&windowManager, &w);
	}

#else
	static void Test_csdButtons(Test *t) { (void) t; }
#endif

//Creates a virtual window (no compositor needed) and attempts to resize it
// beyond its declared min and max limits using Window_resizeCPUBuffer.
//The function must clamp to the valid range.
//
//Also creates a physical window and verifies the compositor honours the
// min/max_size hints by trying to resize it programmatically
// (via xdotool windowsize) to a forbidden dimension, then checking w->size
// was not updated outside the allowed range.

static void Test_minMaxSize(Test *t) {

	Test_setModule(t, "F17/MinMaxSize");

	I32x2 minSz = I32x2_create2(640, 360);
	I32x2 maxSz = I32x2_create2(800, 600);
	I32x2 initSz = I32x2_create2(700, 480);

	//Virtual window: resizeCPUBuffer clamping

	{
		Window *w = NULL;

		CharString title = CharString_createRefCStrConst("F17: virtual");
		I32x2 pos = I32x2_zero;

		WindowManager_createWindow(
			&windowManager, EWindowType_Virtual, pos, initSz, minSz, maxSz,
			EWindowHint_None, title, (WindowCallbacks){ 0 },
			EWindowFormat_AutoRGBA8, 0, &w, &t->err
		);

		if(!Test_assert(t, "virtualCreated", w != NULL))
			goto cleanVirtual;

		//Try to shrink below minimum
		I32x2 tooSmall = I32x2_create2(100, 80);
		Test_assert(t, "resizeTooSmall", Window_resizeCPUBuffer(w, false, tooSmall, &t->err));
		Test_assert(t, "clampedToMin_W", I32x2_x(w->size) == I32x2_x(minSz));
		Test_assert(t, "clampedToMin_H", I32x2_y(w->size) == I32x2_y(minSz));

		//Try to grow beyond maximum
		I32x2 tooBig = I32x2_create2(2000, 1500);
		Test_assert(t, "resizeTooBig", Window_resizeCPUBuffer(w, false, tooBig, &t->err));
		Test_assert(t, "clampedToMax_W", I32x2_x(w->size) == I32x2_x(maxSz));
		Test_assert(t, "clampedToMax_H", I32x2_y(w->size) == I32x2_y(maxSz));

		//Resize to a valid value
		I32x2 validSz = I32x2_create2(650, 450);
		Test_assert(t, "resizeValid", Window_resizeCPUBuffer(w, false, validSz, &t->err));
		Test_assert(t, "validW", I32x2_x(w->size) == 650);
		Test_assert(t, "validH", I32x2_y(w->size) == 450);

	cleanVirtual:
		if(w) WindowManager_freeWindow(&windowManager, &w);
	}

	//Physical window: compositor-side enforcement

	{
		Window *w = NULL;

		CharString title = CharString_createRefCStrConst("F17: Resize me, should clamp to 400x300 .. 800x600");
		I32x2 pos = I32x2_create2(100, 100);

		Bool s_uccess = WindowManager_createWindow(
			&windowManager, EWindowType_Physical, pos, initSz, minSz, maxSz,
			EWindowHint_ProvideCPUBuffer, title, (WindowCallbacks){ 0 },
			EWindowFormat_AutoRGBA8, 0, &w, &t->err
		);

		if(!s_uccess) {
			Test_print(t, "Physical window unavailable, skipping physical min/max assertions");
			goto cleanPhysical;
		}

		Test_assert(t, "physPresent", Window_presentPhysical(w, &t->err));
		pump(500 * MS);

		#if _PLATFORM_TYPE == PLATFORM_LINUX

			if(hasXdotool()) {

				//Attempt to resize below minimum, compositor should refuse
				system("xdotool search --name 'F17:' windowsize 100 80");
				pump(500 * MS);
				Test_assert(t, "physNotTooSmallW", I32x2_x(w->size) >= I32x2_x(minSz));
				Test_assert(t, "physNotTooSmallH", I32x2_y(w->size) >= I32x2_y(minSz));

				//Attempt to resize above maximum
				system("xdotool search --name 'F17:' windowsize 2000 1500");
				pump(500 * MS);
				Test_assert(t, "physNotTooBigW", I32x2_x(w->size) <= I32x2_x(maxSz));
				Test_assert(t, "physNotTooBigH", I32x2_y(w->size) <= I32x2_y(maxSz));

				//Resize to a valid size within the range
				system("xdotool search --name 'F17:' windowsize 670 500");
				pump(500 * MS);

				Test_assert(t, "physValidW", I32x2_x(w->size) == 670);
				Test_assert(t, "physValidH", I32x2_y(w->size) == 500);
			}

			else Test_print(t, "xdotool not available, skipping physical resize assertions");

		#elif _PLATFORM_TYPE == PLATFORM_WINDOWS

			// SetWindowPos below minimum, Win32 automatically clamps via WM_GETMINMAXINFO.
			HWND hwnd = (HWND)w->nativeHandle;
			SetWindowPos(hwnd, NULL, 0, 0, 100, 80, SWP_NOMOVE | SWP_NOZORDER);
			pump(300 * MS);

			Test_assert(t, "physNotTooSmallW", I32x2_x(w->size) >= I32x2_x(minSz));
			Test_assert(t, "physNotTooSmallH", I32x2_y(w->size) >= I32x2_y(minSz));

			SetWindowPos(hwnd, NULL, 0, 0, 2000, 1500, SWP_NOMOVE | SWP_NOZORDER);
			pump(300 * MS);
			
			Test_assert(t, "physNotTooBigW", I32x2_x(w->size) <= I32x2_x(maxSz));
			Test_assert(t, "physNotTooBigH", I32x2_y(w->size) <= I32x2_y(maxSz));

		#endif

		//Interactive: operator drags the window border
		Test_print(t, ">>> INTERACTIVE: Try to resize the window below 400x300 and above 800x600 (10s) <<<");
		Test_print(t, "    Drag the window border as small and as large as possible.");

		Ns waited = 0;
		Bool sawTooSmall = false;
		Bool sawTooBig   = false;

		while(waited < 10 * SECOND) {
			WindowManager_step(&windowManager, NULL, NULL);
			Thread_sleep(16 * MS);
			waited += 16 * MS;

			I32 cw = I32x2_x(w->size);
			I32 ch = I32x2_y(w->size);

			//If the compositor ever let the window go outside bounds, that's a
			// platform bug worth knowing about, log but don't hard-fail since
			// some compositors clamp server-side without telling the client.
			if(cw < I32x2_x(minSz) || ch < I32x2_y(minSz)) sawTooSmall = true;
			if(cw > I32x2_x(maxSz) || ch > I32x2_y(maxSz)) sawTooBig   = true;
		}

		if(sawTooSmall)
			Test_print(t, "WARN: compositor allowed window below declared minimum size");

		else Test_print(t, "OK: window never went below minimum during interactive resize");

		if(sawTooBig)
			Test_print(t, "WARN: compositor allowed window above declared maximum size");

		else Test_print(t, "OK: window never exceeded maximum during interactive resize");

		//Soft assertions: a non-compliant compositor is a compositor bug, not
		// a platform code bug, so we warn rather than fail the test suite.
		if(sawTooSmall) Test_assert(t, "interactiveMinEnforced", !sawTooSmall);
		if(sawTooBig)   Test_assert(t, "interactiveMaxEnforced", !sawTooBig);

		pump(VISUAL_HOLD_NS);

	cleanPhysical:
		if(w) WindowManager_freeWindow(&windowManager, &w);
	}
}

//F18. Mouse draw: paint into CPU buffer with left-button drag
//
//Opens a 256x256 window with a CPU buffer, initialised to a solid gray.
//The operator (or synthetic injection) holds left mouse button and drags
// across the window.  The onDeviceAxis callback records the cursor position;
// onDeviceButton records the pressed state.  Each frame we paint a white
// pixel at the current cursor position while the button is held.
//
//After the drag we verify that at least one pixel changed from the initial
// gray (0x80808080) to white (0xFFFFFFFF), proving the full path:
//Pointer events -> Mouse InputDevice axes + button ->
// application reads them -> CPU buffer mutated -> present.

#define F18_INIT_COLOR 0x80u   //Grey channel value

typedef struct F18State {
	volatile Bool  buttonHeld;
	volatile I32   cursorX;
	volatile I32   cursorY;
} F18State;

static F18State f18;

static void F18_onButton(Window *w, InputDevice *dev, InputHandle h, Bool down) {

	(void) w;
	if(dev->type != EInputDeviceType_Mouse)
		return;

	U16 local = InputDevice_getLocalHandle(dev, h);
	if(local == (U16)(EMouseButton_Left - EMouseAxis_End))
		f18.buttonHeld = down;
}

static void F18_onAxis(Window *w, InputDevice *dev, InputHandle h, F32 value) {

	(void) w;
	if(dev->type != EInputDeviceType_Mouse)
		return;

	U16 local = InputDevice_getLocalHandle(dev, h);

	if(local == (U16)EMouseAxis_X)
		f18.cursorX = (I32)value;

	if(local == (U16)EMouseAxis_Y)
		f18.cursorY = (I32)value;
}

static void Test_mouseDraw(Test *t) {

	Test_setModule(t, "F18/MouseDraw");

	f18 = (F18State) { 0 };

	WindowCallbacks cbs = (WindowCallbacks) { 0 };
	cbs.onDeviceButton = F18_onButton;
	cbs.onDeviceAxis   = F18_onAxis;

	I32x2 sz  = I32x2_create2(256, 256);
	I32x2 pos = I32x2_create2(300, 300);

	Window *w = createWindowCallback(
		t, "F18: Hold left-click and drag to draw",
		pos, sz, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, cbs
	);

	if(!Test_assert(t, "windowCreated", w != NULL))
		goto clean;

	//Fill buffer with a known grey so we can detect changes.
	{
		U8 *px = w->cpuVisibleBuffer.ptrNonConst;
		U64 len = Buffer_length(w->cpuVisibleBuffer);
		for(U64 i = 0; i < len; ++i)
			px[i] = F18_INIT_COLOR;
	}

	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(300 * MS);

	if(w->type != EWindowType_Physical) {
		Test_print(t, "[virtual] mouse draw requires a physical window, skipped");
		goto clean;
	}

	//Synthetic injection
	#if _PLATFORM_TYPE == PLATFORM_WINDOWS

		{
			HWND hwnd = (HWND)w->nativeHandle;

			//Move to window centre, press left, drag 100px right, release

			POINT centre = { 300 + 128, 300 + 128 };
			SetCursorPos(centre.x, centre.y);
			Sleep(50);

			INPUT inputs[2] = { 0 };
			inputs[0].type = INPUT_MOUSE; inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
			SendInput(1, inputs, sizeof(INPUT));

			for(I32 dx = 0; dx <= 100; dx += 5) {

				SetCursorPos(centre.x + dx, centre.y);
				Sleep(16);
				WindowManager_step(&windowManager, NULL, NULL);

				//Paint while button held

				if(f18.buttonHeld && w->cpuVisibleBuffer.ptrNonConst) {
					I32 cx = f18.cursorX, cy = f18.cursorY;
					I32 W  = I32x2_x(w->size), H = I32x2_y(w->size);
					if(cx >= 0 && cx < W && cy >= 0 && cy < H) {
						U8 *p = w->cpuVisibleBuffer.ptrNonConst + (cy * W + cx) * 4;
						p[0] = p[1] = p[2] = p[3] = 0xFF;
					}
				}
			}

			inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTUP;
			SendInput(1, inputs, sizeof(INPUT));
			pump(200 * MS);
		}

	#elif _PLATFORM_TYPE == PLATFORM_LINUX

		if(hasXdotool()) {

			//Move to centre, mousedown, drag, mouseup

			system(
				"WIN=$(xdotool search --name 'F18:') && "
				"xdotool mousemove --window $WIN 128 128 && "
				"xdotool mousedown 1"
			);

			for(I32 dx = 0; dx <= 80; dx += 4) {

				C8 cmd[256];
				snprintf(cmd, sizeof(cmd),
					"WIN=$(xdotool search --name 'F18:') && "
					"xdotool mousemove --window $WIN %d 128", 128 + dx
				);

				system(cmd);
				Thread_sleep(16 * MS);
				WindowManager_step(&windowManager, NULL, NULL);

				if(f18.buttonHeld && w->cpuVisibleBuffer.ptrNonConst) {
					I32 cx = f18.cursorX, cy = f18.cursorY;
					I32 W  = I32x2_x(w->size), H = I32x2_y(w->size);
					if(cx >= 0 && cx < W && cy >= 0 && cy < H) {
						U8 *p = w->cpuVisibleBuffer.ptrNonConst + (cy * W + cx) * 4;
						p[0] = p[1] = p[2] = p[3] = 0xFF;
					}
				}
			}

			system("xdotool mouseup 1");
			pump(200 * MS);
		}

		else Test_print(t, "xdotool not available, skipping synthetic mouse draw");

	#endif

	//Verify at least one pixel changed
	{
		const U8 *px = w->cpuVisibleBuffer.ptr;
		I32 W = I32x2_x(w->size), H = I32x2_y(w->size);
		Bool anyChanged = false;

		for(I32 i = 0; i < W * H && !anyChanged; ++i)
			if(px[i * 4] != F18_INIT_COLOR)
				anyChanged = true;

		Test_assert(t, "present", Window_presentPhysical(w, &t->err));
		pump(VISUAL_HOLD_NS);

		if(!anyChanged)
			Test_print(t, "WARN: no pixels changed (synthetic draw didn't fire, try interactive)");

		//Interactive fallback

		if(!anyChanged) {

			Test_print(t, ">>> INTERACTIVE: Hold left-click and drag across the window (8s) <<<");

			Ns waited = 0;
			while(!anyChanged && waited < 8 * SECOND) {
				WindowManager_step(&windowManager, NULL, NULL);
				Thread_sleep(16 * MS);
				waited += 16 * MS;

				if(f18.buttonHeld && w->cpuVisibleBuffer.ptrNonConst) {
					I32 cx = f18.cursorX, cy = f18.cursorY;
					I32 W2 = I32x2_x(w->size), H2 = I32x2_y(w->size);
					if(cx >= 0 && cx < W2 && cy >= 0 && cy < H2) {
						U8 *p = w->cpuVisibleBuffer.ptrNonConst + (cy * W2 + cx) * 4;
						p[0] = p[1] = p[2] = p[3] = 0xFF;
					}
				}

				anyChanged = false;
				for(I32 i = 0; i < W * H && !anyChanged; ++i)
					if(px[i * 4] != F18_INIT_COLOR)
						anyChanged = true;
			}

			Test_assert(t, "operatorDraw", anyChanged);
		}
		
		else Test_assert(t, "syntheticDraw", true);
	}

clean:
	f18 = (F18State){ 0 };
	if(w) WindowManager_freeWindow(&windowManager, &w);
}

//F19. Scroll wheel (vertical + horizontal)
//
//Opens a small window, injects scroll events (buttons 4/5 for
// vertical, 6/7 for horizontal), and verifies that the ScrollWheel_Y and
// ScrollWheel_X axes on the Mouse InputDevice receive non-zero values through
// the onDeviceAxis callback.

typedef struct F19State {
	volatile F32  scrollY;
	volatile F32  scrollX;
	volatile Bool gotScrollY;
	volatile Bool gotScrollX;
} F19State;

static F19State f19;

static void F19_onAxis(Window *w, InputDevice *dev, InputHandle h, F32 value) {
	(void) w;
	if(dev->type != EInputDeviceType_Mouse)
		return;
		
	U16 local = InputDevice_getLocalHandle(dev, h);

	if(local == (U16)EMouseAxis_ScrollWheel_Y) {
		f19.scrollY    = value;
		f19.gotScrollY = true;
	}

	if(local == (U16)EMouseAxis_ScrollWheel_X) {
		f19.scrollX    = value;
		f19.gotScrollX = true;
	}
}

static void Test_scrollWheel(Test *t) {

	Test_setModule(t, "F19/ScrollWheel");

	f19 = (F19State) { 0 };

	WindowCallbacks cbs = (WindowCallbacks) { 0 };
	cbs.onDeviceAxis = F19_onAxis;

	I32x2 sz  = I32x2_create2(400, 300);
	I32x2 pos = I32x2_create2(300, 300);

	Window *w = createWindowCallback(
		t, "F19: Scroll wheel test (vertical + horizontal)",
		pos, sz, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, cbs
	);

	if(!Test_assert(t, "windowCreated", w != NULL))
		goto clean;

	Test_assert(t, "present", Window_presentPhysical(w, &t->err));
	pump(300 * MS);

	if(w->type != EWindowType_Physical) {
		Test_print(t, "[virtual] scroll wheel test requires a physical window, skipped");
		goto clean;
	}

	//Synthetic injection

	#if _PLATFORM_TYPE == PLATFORM_LINUX

		if(hasXdotool()) {

			//Focus and move cursor into the centre of the window so scroll
			// events are routed to our surface, not the compositor desktop

			system(
				"xdotool search --name 'F19:' windowfocus && "
				"xdotool mousemove --window $(xdotool search --name 'F19:') 200 150"
			);

			pump(200 * MS);

			//Vertical scroll down (button 5), then up (button 4)

			system("xdotool click --clearmodifiers 5");
			pump(200 * MS);

			if(f19.gotScrollY)
				Test_assert(t, "syntheticScrollY_down", f19.gotScrollY && f19.scrollY != 0.f);

			else Test_print(t, "WARN: vertical scroll down didn't fire (compositor routing?)");

			f19.gotScrollY = false;
			f19.scrollY    = 0.f;

			system("xdotool click --clearmodifiers 4");
			pump(200 * MS);

			if(f19.gotScrollY)
				Test_assert(t, "syntheticScrollY_up", f19.gotScrollY && f19.scrollY != 0.f);

			else Test_print(t, "WARN: vertical scroll up didn't fire");

			//Horizontal scroll right (button 7), then left (button 6).
			//Not all mice or compositors generate horizontal scroll; treat as soft

			f19.gotScrollX = false;
			f19.scrollX    = 0.f;

			system("xdotool click --clearmodifiers 7");
			pump(200 * MS);

			system("xdotool click --clearmodifiers 6");
			pump(200 * MS);

			if(f19.gotScrollX)
				Test_assert(t, "syntheticScrollX", f19.gotScrollX && f19.scrollX != 0.f);

			else Test_print(t, "WARN: horizontal scroll not received (device/compositor may not support it)");
		}
		
		else Test_print(t, "xdotool not available, skipping synthetic scroll injection");

	#elif _PLATFORM_TYPE == PLATFORM_WINDOWS
		{
			//SetCursorPos to window centre, then send WM_MOUSEWHEEL via SendInput
			POINT centre = { 300 + 200, 300 + 150 };
			SetCursorPos(centre.x, centre.y);
			Sleep(50);

			//Vertical scroll down (negative delta by Windows convention)
			INPUT inp = { 0 };
			inp.type           = INPUT_MOUSE;
			inp.mi.dwFlags     = MOUSEEVENTF_WHEEL;
			inp.mi.mouseData   = (DWORD)(WORD)(-WHEEL_DELTA);
			SendInput(1, &inp, sizeof(INPUT));
			pump(200 * MS);

			Test_assert(t, "syntheticScrollY_down", f19.gotScrollY && f19.scrollY != 0.f);

			f19.gotScrollY = false;
			f19.scrollY    = 0.f;

			inp.mi.mouseData = (DWORD)(WORD)(WHEEL_DELTA);
			SendInput(1, &inp, sizeof(INPUT));
			pump(200 * MS);

			Test_assert(t, "syntheticScrollY_up", f19.gotScrollY && f19.scrollY != 0.f);

			//Horizontal scroll via MOUSEEVENTF_HWHEEL
			inp.mi.dwFlags   = MOUSEEVENTF_HWHEEL;
			inp.mi.mouseData = (DWORD)(WORD)(WHEEL_DELTA);
			SendInput(1, &inp, sizeof(INPUT));
			pump(200 * MS);

			if(f19.gotScrollX)
				Test_assert(t, "syntheticScrollX", f19.scrollX != 0.f);

			else Test_print(t, "WARN: horizontal scroll not received on Windows");
		}
	#else
		Test_print(t, "Synthetic scroll injection not implemented for this platform");
	#endif

	//Interactive fallback for vertical
	if(!f19.gotScrollY) {

		Test_print(t, ">>> INTERACTIVE: Scroll the mouse wheel up/down in the window (8s) <<<");

		Ns waited = 0;
		while(!f19.gotScrollY && waited < 8 * SECOND) {
			WindowManager_step(&windowManager, NULL, NULL);
			Thread_sleep(16 * MS);
			waited += 16 * MS;
		}

		if(!f19.gotScrollY)
			Test_print(t, "WARN: no vertical scroll received within timeout");

		Test_assert(t, "operatorScrollY", f19.gotScrollY && f19.scrollY != 0.f);
	}

	//Horizontal is optional / device-dependent; only interactive-prompt if
	// vertical worked (confirms the path is wired) but horizontal didn't.
	if(f19.gotScrollY && !f19.gotScrollX) {

		Test_print(t, ">>> INTERACTIVE: Scroll horizontally (tilt wheel or Shift+scroll) (5s, optional) <<<");

		Ns waited = 0;
		while(!f19.gotScrollX && waited < 5 * SECOND) {
			WindowManager_step(&windowManager, NULL, NULL);
			Thread_sleep(16 * MS);
			waited += 16 * MS;
		}

		if(!f19.gotScrollX)
			Test_print(t, "WARN: no horizontal scroll received (device may not support it, not a failure)");
	}

	pump(VISUAL_HOLD_NS);

clean:
	f19 = (F19State){ 0 };
	if(w) WindowManager_freeWindow(&windowManager, &w);
}

// -- entry point ---------------------------------------------------------------

Platform_defineEntrypoint() {

	if (!Platform_create(Platform_argc, Platform_argv, Platform_getData(), NULL, false, NULL))
		Platform_return(1);

	Test t = (Test) { .alloc = Platform_instance->alloc };

	U64 allocsBefore = Platform_getActiveAllocations(0);

	if (!setup(&t)) {
		Test_print(&t, "Skipping all functional tests, no WindowManager");
		goto done;
	}

	(void) Test_fullScreen;
	(void) Test_resize;

	(void) Test_cpuBuffer;//(&t);
	//Test_fullScreen(&t);     //TODO: Crashes
	//Test_resize(&t);         //TODO: Broken
	(void) Test_multiWindow;//(&t);
	(void) Test_keyboard;//(&t);
	(void) Test_storeCPUBuffer;//(&t);
	(void) Test_updateTitle;//(&t);
	(void) Test_mouse;//(&t);
	(void) Test_focusMinimize;//(&t);
	(void) Test_typeChar;//(&t);
	(void) Test_focusReset;//(&t);
	(void) Test_monitorInfo;//(&t);
	(void) Test_windowMove;//(&t);
	(void) Test_maximize;//(&t);
	(void) Test_keyboardRemap;//(&t);
	(void) Test_csdButtons;//(&t);
	Test_minMaxSize(&t);
	(void) Test_mouseDraw;//(&t);
	(void) Test_scrollWheel;//(&t);

done:
	shutdown();

	U64 allocsAfter = Platform_getActiveAllocations(0);

	Test_setModule(&t, NULL);
	Test_assert(&t, "NoLeaks", allocsAfter <= allocsBefore);

	int result = Test_end(&t);
	Platform_cleanup();
	Platform_return(result);
}
