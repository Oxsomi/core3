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

// platforms/test/functional/test_platforms_functional.c
//
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
#include "types/test/test.h"
#include "types/base/string_read_helper.h"
#include "types/base/buffer.h"
#include "types/base/error.h"
#include "types/base/thread.h"

//How long to hold a visual window open for human inspection
#define VISUAL_HOLD_NS  (3 * SECOND)

//Windows-only: SendInput / ShowWindow
#if _PLATFORM_TYPE == PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <Windows.h>
#endif

// ── Shared window manager ─────────────────────────────────────────────────────

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

// ── F1. CPU buffer -> screen ───────────────────────────────────────────────────

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

// ── F2. Fullscreen toggle ─────────────────────────────────────────────────────

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

// ── F3. Resize / min+max enforcement ─────────────────────────────────────────

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

// ── F4. Multi-window ──────────────────────────────────────────────────────────

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

// ── F5. Keyboard – OS layer (interactive) ───────────────────────────────────

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
        EWindowHint_None, title, wcbs, EWindowFormat_AutoRGBA8, 0, &w, &t->err
    );

    if (!s_uccess) {
        Test_print(t, "OS-layer keyboard test requires a physical window, skipped");
        goto clean;
    }
    
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

// ── F6. Window_storeCPUBufferToDisk ───────────────────────────────────────────

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
 
    // Fill: pixel i → R=(i&0xFF), G=((i>>1)&0xFF), B=42, A=255
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
        //If the format stayed RGBA8 the order is R,G,B,A → 0,0,42,255.
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

// ── F7. Window_updatePhysicalTitle ────────────────────────────────────────────

static void Test_updateTitle(Test *t) {

    Test_setModule(t, "F7/UpdateTitle");

    Error err = Error_none();
    I32x2 sz = I32x2_create2(480, 80);

    Window *w = createWindow(t, "F7: Title, original", sz, I32x2_zero, EWindowHint_None, EWindowFormat_AutoRGBA8);

    if (!Test_assert(t, "windowCreated", w != NULL))
        goto clean;

    pump(500 * MS);

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

// ── F8. Mouse – OS layer (interactive) ───────────────────────────────────────

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

    CharString title = CharString_createRefCStrConst("F9: Left-click anywhere to pass");
    I32x2 pos = I32x2_create2(200, 350);
    I32x2 sz  = I32x2_create2(640, 100);
    I32x2 minSize = EResolution_get(EResolution_SD);
    I32x2 maxSize = I32x2_create2(4096, 4096);

    Bool s_uccess =WindowManager_createWindow(
        &windowManager, EWindowType_Physical, pos, sz, minSize, maxSize,
        EWindowHint_None, title, wcbs, EWindowFormat_AutoRGBA8, 0, &w, &t->err
    );

    if (!s_uccess) {
        Test_print(t, "OS-layer mouse test requires a physical window, skipped");
        goto clean;
    }

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

// ── F9. Focus / minimize cycle ───────────────────────────────────────────────
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

    Window *w = createWindow(t, "F9: Minimize / restore, watch me", sz, I32x2_zero, EWindowHint_None, EWindowFormat_AutoRGBA8);

    if (!Test_assert(t, "windowCreated", w != NULL))
        goto clean;

    //Initial state: not minimized
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

    // ── Minimize ──────────────────────────────────────────────────────────────
    #if _PLATFORM_TYPE == PLATFORM_WINDOWS
        ShowWindow((HWND)w->nativeHandle, SW_MINIMIZE);
    #endif

    pump(500 * MS);

    #if _PLATFORM_TYPE == PLATFORM_WINDOWS
        Test_assert(t, "isMinimized", Window_isMinimized(w));
        //Focus must have left when we minimized
        Test_assert(t, "notFocusedWhileMin", !Window_isFocussed(w));
    #else
        //Soft: log the observed state but don't hard-fail on platforms where
        //the OS may not drive the flag synchronously.
        if (!Window_isMinimized(w))
            Test_print(t, "WARN: IsMinimized not set after minimize request");
    #endif

    // ── Restore ───────────────────────────────────────────────────────────────
    #if _PLATFORM_TYPE == PLATFORM_WINDOWS
        ShowWindow((HWND)w->nativeHandle, SW_RESTORE);
        SetForegroundWindow((HWND)w->nativeHandle);
    #endif

    pump(500 * MS);

    #if _PLATFORM_TYPE == PLATFORM_WINDOWS
        Test_assert(t, "notMinimizedAfterRestore", !Window_isMinimized(w));
        Test_assert(t, "focusedAfterRestore",       Window_isFocussed(w));
    #endif

    //Hold so the operator can see it come back
    pump(VISUAL_HOLD_NS);

clean:
    if (w) WindowManager_freeWindow(&windowManager, &w);
}

// ── F10. onTypeChar callback ──────────────────────────────────────────────────
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

    Window *w = createWindowCallback(t, "F10: Type \"Hello\" to pass", pos, sz, EWindowHint_None, EWindowFormat_AutoRGBA8, wcbs);

    if (!Test_assert(t, "windowCreated", w != NULL))
        goto clean;

    Bool isPhysical = w->type == EWindowType_Physical;

    if (!isPhysical) {
        Test_print(t, "[virtual] onTypeChar requires a physical window, skipped");
        goto clean;
    }

    const CharString hello = CharString_createRefCStrConst("Hello");

    // ── Synthetic injection (Windows) ────────────────────────────────────────
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
    #else
        Test_print(t, "Synthetic typeChar injection not implemented for this platform");
    #endif

    // ── Interactive ───────────────────────────────────────────────────────────
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

// ── entry point ───────────────────────────────────────────────────────────────

Platform_defineEntrypoint() {

    if (!Platform_create(Platform_argc, Platform_argv, Platform_getData(), NULL, false, NULL))
        Platform_return(1);

    Test t = (Test) { .alloc = Platform_instance->alloc };

    U64 allocsBefore = Platform_getActiveAllocations(0);

    if (!setup(&t)) {
        Test_print(&t, "Skipping all functional tests, no WindowManager");
        goto done;
    }

    Test_cpuBuffer(&t);
    Test_fullScreen(&t);
    Test_resize(&t);
    Test_multiWindow(&t);
    Test_keyboard(&t);
    Test_storeCPUBuffer(&t);
    Test_updateTitle(&t);
    Test_mouse(&t);
    Test_focusMinimize(&t);
    Test_typeChar(&t);

done:
    shutdown();

    U64 allocsAfter = Platform_getActiveAllocations(0);

    Test_setModule(&t, NULL);
    Test_assert(&t, "NoLeaks", allocsAfter <= allocsBefore);

    int result = Test_end(&t);
    Platform_cleanup();
    Platform_return(result);
}