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

//platforms/test/functional/test_platforms_functional_input.c
//
//F5.  Keyboard input, OS layer   (interactive: operator presses ESC)
//F8.  Mouse, OS layer            (interactive: operator left-clicks)
//F10. onTypeChar callback        (interactive: operator types "Hello world")
//F11. Input, focus lost reset
//F15. Keyboard remap
//F18. Mouse draw: paint into CPU buffer with left-button drag
//F19. Scroll wheel (vertical + horizontal)

#include "test_platforms_functional_shared.h"
#include "platforms/window_manager.h"
#include "platforms/keyboard.h"
#include "platforms/mouse.h"
#include "platforms/platform.h"
#include "types/base/string_read_helper.h"
#include "types/base/thread.h"
#include "types/test/test.h"

#if _PLATFORM_TYPE == PLATFORM_WINDOWS
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h>
#elif _PLATFORM_TYPE == PLATFORM_LINUX
	#include <stdlib.h>
	#include <stdio.h>
	#include "platforms/linux/lwindow_structs.h"
	Bool hasXdotool();

	//xdotool simulations are best-effort; the effect is verified via window/input state, not the exit code.
	static void runXdotool(const C8 *cmd) {
		const int ret = system(cmd);
		(void) ret;
	}
#endif

Bool isSingleWindow();

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

	WindowRef *wRef = NULL;

	WindowCallbacks wcbs = (WindowCallbacks) { 0 };
	wcbs.onDeviceButton = onDeviceButton;

	CharString title = CharString_createRefCStrConst("F5b: Press ESC to pass");
	I32x2 pos = I32x2_create2(200, 200);
	I32x2 sz = I32x2_create2(640, 100);
	I32x2 minSize = EResolution_get(EResolution_SD);
	I32x2 maxSize = I32x2_create2(4096, 4096);

	Bool s_uccess = WindowManager_createWindow(
		&windowManager, EWindowType_Physical, pos, sz, minSize, maxSize,
		EWindowHint_ProvideCPUBuffer, title, wcbs, EWindowFormat_AutoRGBA8, 0, &wRef, &t->err
	);

	if (!s_uccess) {
		Test_print(t, "OS-layer keyboard test requires a physical window, skipped");
		goto clean;
	}

	Window *w = RefPtr_data(wRef, Window);
	present(t, w);
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

				if(!windowManager.windows.length)
					break;

				Thread_sleep(16 * MS);
				waited += 16 * MS;
			}
			Test_assert(t, "syntheticESC", escPressed);
			escPressed = false;
		}

	#elif _PLATFORM_TYPE == PLATFORM_LINUX

		// Wayland has no reliable cross-process input injection mechanism:
		// - xdotool uses X11 XTest, which cannot reach native Wayland surfaces
		// - zwp_virtual_keyboard_v1 is compositor-specific; not supported on GNOME/Mutter
		// - /dev/uinput requires the user to be in the 'input' group
		// Synthetic injection is therefore skipped on Linux; the interactive
		// section below is the authoritative test for this platform.
		Test_print(t,
			"Synthetic input injection not supported on Wayland without elevated privileges or compositor-specific extensions"
		);

	#else
		Test_print(t, "SendInput not available on this platform, skipping synthetic OS injection");
	#endif

	Test_print(t, ">>> INTERACTIVE: Press ESC in the window (5s timeout) <<<");
	Ns waited = 0;
	while (!escPressed && waited < 5 * SECOND) {

		WindowManager_step(&windowManager, NULL, NULL);

		if(!windowManager.windows.length)
			break;

		Thread_sleep(16 * MS);
		waited += 16 * MS;
	}

	if (!escPressed)
		Test_print(t, "WARN: ESC not received within timeout");

	Test_assert(t, "operatorESC", escPressed);

clean:
	RefPtr_dec(&wRef);
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

	WindowRef *wRef = NULL;

	WindowCallbacks wcbs = (WindowCallbacks){ 0 };
	wcbs.onDeviceButton = onMouseButton;

	CharString title = CharString_createRefCStrConst("F8: Left-click anywhere to pass");
	I32x2 pos = I32x2_create2(200, 350);
	I32x2 sz = I32x2_create2(640, 100);
	I32x2 minSize = EResolution_get(EResolution_SD);
	I32x2 maxSize = I32x2_create2(4096, 4096);

	Bool s_uccess = WindowManager_createWindow(
		&windowManager, EWindowType_Physical, pos, sz, minSize, maxSize,
		EWindowHint_ProvideCPUBuffer, title, wcbs, EWindowFormat_AutoRGBA8, 0, &wRef, &t->err
	);

	if (!s_uccess) {
		Test_print(t, "OS-layer mouse test requires a physical window, skipped");
		goto clean;
	}

	Window *w = RefPtr_data(wRef, Window);
	present(t, w);
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

				if(!windowManager.windows.length)
					break;

				Thread_sleep(16 * MS);
				waited += 16 * MS;
			}
			Test_assert(t, "syntheticClick", leftClicked);
			leftClicked = false;
		}

	#elif _PLATFORM_TYPE == PLATFORM_LINUX

		if (!isSingleWindow() && hasXdotool()) {

			runXdotool("xdotool search --name 'F8: Left-click anywhere to pass' windowfocus click 1");

			Ns waited = 0;

			while (!leftClicked && waited < 1 * SECOND) {

				WindowManager_step(&windowManager, NULL, NULL);

				if(!windowManager.windows.length)
					break;

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

		if(!windowManager.windows.length)
			break;

		Thread_sleep(16 * MS);
		waited += 16 * MS;
	}

	if (!leftClicked)
		Test_print(t, "WARN: left click not received within timeout");

	Test_assert(t, "operatorClick", leftClicked);

clean:
	RefPtr_dec(&wRef);
}

// -- F10. onTypeChar callback --------------------------------------------------
//
//Opens a window and waits for the operator to type the word "Hello world" (11 chars).
//The onTypeChar callback accumulates each CharString fragment; we concatenate
// them and check the result contains "Hello world".
//
//On Windows a synthetic round-trip is attempted first via SendInput VK codes
// (Shift+H, e, l, l, o, ...) so the test is not purely interactive.
//
//NOTE: onTypeChar delivers OS-level text input (after IME / layout mapping),
// not raw scancodes.  The 'H' therefore requires a Shift modifier injected
// alongside it.

static CharString typedText;

static void onTypeChar(Window *w, CharString str) {
	(void)w;
	CharString_appendString(&typedText, &str, Platform_instance->alloc, NULL);
}

static void Test_typeChar(Test *t) {

	Test_setModule(t, "F10/TypeChar");

	typedText = CharString_createNull();

	WindowCallbacks wcbs = (WindowCallbacks){ 0 };
	wcbs.onTypeChar = onTypeChar;

	I32x2 pos = I32x2_create2(200, 500);
	I32x2 sz = I32x2_create2(640, 100);

	WindowRef *wRef = createWindowCallback(
		t, "F10: Type \"Hello world\" to pass", pos, sz, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, wcbs
	);

	if (!Test_assert(t, "windowCreated", wRef != NULL))
		goto clean;

	Window *w = RefPtr_data(wRef, Window);
	present(t, w);
	pump(300 * MS);

	Bool isPhysical = w->type == EWindowType_Physical;

	if (!isPhysical) {
		Test_print(t, "[virtual] onTypeChar requires a physical window, skipped");
		goto clean;
	}

	const CharString hello = CharString_createRefCStrConst("Hello world");

	// -- Synthetic injection (Windows) ----------------------------------------
	#if _PLATFORM_TYPE == PLATFORM_WINDOWS

		{
			//Bring our window to the foreground so WM_CHAR is routed to it.
			SetForegroundWindow((HWND)w->nativeHandle);
			pump(200 * MS);

			//H (Shift down, H down, H up, Shift up), e, l, l, o, ...
			//Each key: down then up.
			struct { WORD vk; Bool shift; } keys[] = {
				{ 'H', true  },
				{ 'E', false },
				{ 'L', false },
				{ 'L', false },
				{ 'O', false },
				{ ' ', false  },
				{ 'W', false },
				{ 'O', false },
				{ 'R', false },
				{ 'L', false },
				{ 'D', false }
			};

			for (U64 i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {

				INPUT inputs[4] = { 0 };
				U32 count = 0;

				if (keys[i].shift) {
					inputs[count].type = INPUT_KEYBOARD;
					inputs[count].ki.wVk = VK_SHIFT;
					++count;
				}

				inputs[count].type = INPUT_KEYBOARD;
				inputs[count].ki.wVk = keys[i].vk;
				++count;

				inputs[count].type = INPUT_KEYBOARD;
				inputs[count].ki.wVk = keys[i].vk;
				inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
				++count;

				if (keys[i].shift) {
					inputs[count].type = INPUT_KEYBOARD;
					inputs[count].ki.wVk = VK_SHIFT;
					inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
					++count;
				}

				SendInput(count, inputs, sizeof(INPUT));
			}

			pump(300 * MS);

			Bool syntheticOK = CharString_containsStringSensitive(&typedText, &hello, 0, 0);

			if (!syntheticOK)
				Test_print(t, "WARN: synthetic typeChar didn't produce 'Hello world', may be layout-dependent");

			else Test_assert(t, "syntheticHello", syntheticOK);

			//Reset for interactive round
			CharString_free(&typedText, t->alloc);
			typedText = CharString_createNull();
		}

	#elif _PLATFORM_TYPE == PLATFORM_LINUX

		//Wayland has no input injection (see Test_keyboard)
		Test_print(t,
			"Synthetic input injection not supported on Wayland without elevated privileges or compositor-specific extensions"
		);

	#else
		Test_print(t, "Synthetic typeChar injection not implemented for this platform");
	#endif

	// -- Interactive -----------------------------------------------------------
	Test_print(t, ">>> INTERACTIVE: Click the window and type \"Hello world\" (8s timeout) <<<");

	Ns waited = 0;
	while (waited < 8 * SECOND) {

		WindowManager_step(&windowManager, NULL, NULL);

		if(!windowManager.windows.length)
			break;

		Thread_sleep(16 * MS);
		waited += 16 * MS;

		//Pass as soon as "Hello world" appears anywhere in the accumulated text
		// Hint to user: If simulating azerty on a qwerty keyboard, type "Hello zorld".
		if (CharString_containsStringSensitive(&typedText, &hello, 0, 0))
			break;
	}

	Bool gotHello = CharString_containsStringSensitive(&typedText, &hello, 0, 0);

	if (!gotHello)
		Test_print(t, "WARN: 'Hello world' not received within timeout");

	Test_assert(t, "operatorHello", gotHello);

clean:
	CharString_free(&typedText, t->alloc);
	RefPtr_dec(&wRef);
}

// -- F11. Input - Focus Lost Reset ---------------------------------------------

static volatile Bool focusResetTriggered = false;

static void onButtonReset(Window *w, InputDevice *dev, InputHandle h, Bool down) {
	(void)w; (void)dev; (void)h;
	if (!down) focusResetTriggered = true; //Reset callback fired
}

static void Test_focusReset(Test *t) {

	Test_setModule(t, "F11/FocusReset");

	if(isSingleWindow()) {
		Test_print(t, "Skipped. Device is single window");
		return;
	}

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS || _PLATFORM_TYPE == PLATFORM_LINUX

		WindowCallbacks cbs = (WindowCallbacks) { 0 };
		cbs.onDeviceButton = onButtonReset;

		WindowRef *wRef = createWindowCallback(
			t, "F11: FocusReset",
			I32x2_create2(200, 200), I32x2_create2(300, 300),
			EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8,
			cbs
		);

		if (!Test_assert(t, "windowCreated", wRef != NULL))
			goto clean;

		Window *w = RefPtr_data(wRef, Window);
		present(t, w);
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
				Test_assert(t, "stateCleared", !InputDevice_getCurrentState(kb, hEsc));
			}

		#elif _PLATFORM_TYPE == PLATFORM_LINUX

			{
				Keyboard *kb = (Keyboard*)&w->devices.ptrNonConst[w->defaultKeyboardId];
				InputHandle hEsc = InputDevice_createHandle(kb, EKey_Escape, EInputType_Button);

				//Set state directly, matching the Windows path.
				//F5 covers OS-level key injection; this test is about focus-loss reset only.
				InputDevice_setCurrentState(kb, hEsc, true);
				Test_assert(t, "stateSet", InputDevice_getCurrentState(kb, hEsc));

				//Minimize triggers a compositor wl_keyboard::leave event which
				//LWindow_kbLeave handles, clearing all held keys and firing onDeviceButton.

				LWindow *lwin = WindowExt(w, LWindow);
				LWindowManager *manager = (LWindowManager *)w->owner->platformData.ptr;
				xdg_toplevel_set_minimized(lwin->topLevel);
				wl_display_flush(manager->display);
				pump(400 * MS);

				Test_assert(t, "stateCleared",   !InputDevice_getCurrentState(kb, hEsc));
				Test_assert(t, "resetTriggered",  focusResetTriggered);
			}

		#endif

	clean:
		focusResetTriggered = false;
		RefPtr_dec(&wRef);

	#else
		(void)onButtonReset;
	#endif
}

// -- F15. Keyboard remap -------------------------------------------------------
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

	(void)w;
	if (dev->type != EInputDeviceType_Keyboard || !down)
		return;

	U16 local = InputDevice_getLocalHandle(dev, h);
	for (U32 i = 0; i < F15_KEY_COUNT; ++i)
		if (local == (U16)F15_keys[i])
			f15_pressed |= (1u << i);
}

static void Test_keyboardRemap(Test *t) {

	Test_setModule(t, "F15/KeyboardRemap");

	f15_pressed = 0;

	WindowCallbacks cbs = (WindowCallbacks) { 0 };
	cbs.onDeviceButton = F15_onDeviceButton;

	WindowRef *wRef = createWindowCallback(
		t, "F15: Keyboard remap",
		I32x2_create2(200, 600), I32x2_create2(640, 100),
		EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, cbs
	);

	if (!Test_assert(t, "windowCreated", wRef != NULL))
		goto clean;

	Window *w = RefPtr_data(wRef, Window);
	present(t, w);
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
		InputDevice *kb = &w->devices.ptrNonConst[w->defaultKeyboardId];
		U64 startOff = sizeof("You need to press: ") - 1;
		C8 prompt[128] = "You need to press: ";
		Bool anyFailed = false;

		for (U64 i = 0, k = startOff; i < F15_KEY_COUNT; ++i) {

			CharString label = CharString_createNull();
			Error err = Error_none();

			Bool ok = Keyboard_remap((const Keyboard *)kb, F15_keys[i], Platform_instance->alloc, &label, &err);

			if (ok && CharString_length(label) && k + CharString_length(label) + 2 < sizeof(prompt)) {

				for (U64 j = 0; j < CharString_length(label); ++j)
					prompt[k++] = label.ptr[j];

				prompt[k++] = ' ';
				prompt[k] = '\0';

			}
			else {

				//Fallback: print the EKey index if remap fails
				if (k + 3 < sizeof(prompt)) {
					prompt[k++] = '?';
					prompt[k++] = ' ';
					prompt[k] = '\0';
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
	Ns  waited = 0;

	while ((f15_pressed & allBits) != allBits && waited < 10 * SECOND) {
		
		WindowManager_step(&windowManager, NULL, NULL);

		if(!windowManager.windows.length)
			break;

		Thread_sleep(16 * MS);
		waited += 16 * MS;
	}

	Bool allPressed = (f15_pressed & allBits) == allBits;

	if (!allPressed)
		Test_print(t, "WARN: not all remap keys received within timeout");

	Test_assert(t, "operatorRemap", allPressed);

clean:
	f15_pressed = 0;
	RefPtr_dec(&wRef);
}

// -- F18. Mouse draw: paint into CPU buffer with left-button drag -------------
//
//Opens a 256x256 window with a CPU buffer, initialised to a solid gray.
//The operator (or synthetic injection) holds left mouse button and drags
// across the window.  The onDeviceAxis callback records the cursor position;
// onDeviceButton records the pressed state. The actual painting now happens
// in onDraw rather than inline in the input callbacks or the test body: input
// callbacks only update f18's cursor/button state, onDraw is the single place
// that touches cpuVisibleBuffer, and we request a repaint each time the mouse
// moves while the button is held so onDraw gets a chance to run again.
//
//After the drag we verify that at least one pixel changed from the initial
// gray (0x80808080) to white (0xFFFFFFFF), proving the full path:
//Pointer events -> Mouse InputDevice axes + button ->
// application reads them -> onDraw paints into the CPU buffer -> present.

#define F18_INIT_COLOR 0x80u   //Grey channel value

typedef struct F18State {
	volatile Bool buttonHeld;
	I32x2         points[1024];   //Recorded cursor positions while dragging
	U32           pointCount;
} F18State;

static F18State f18;

static void F18_onDraw(Window *w) {

	if (!w->cpuVisibleBuffer.ptrNonConst)
		return;

	I32 W = I32x2_x(w->size), H = I32x2_y(w->size);

	if (f18.buttonHeld && f18.pointCount < (U32)(sizeof(f18.points) / sizeof(f18.points[0]))) {

		I32x2 p = w->cursor;
		Bool dup = f18.pointCount && I32x2_eq2(f18.points[f18.pointCount - 1], p);

		if(!dup)
			f18.points[f18.pointCount++] = p;
	}

	//Re-establish a known background every frame -- a buffer we haven't
	//drawn into yet (LWINDOW_BUFFER_COUNT rotation, or a fresh allocation
	//from a resize) isn't guaranteed to already be grey -- then replay every
	//recorded point on top, clipped to the *current* size. This keeps every
	//presented buffer identical regardless of rotation, and stays correct
	//across a resize since out-of-bounds points are just skipped.

	U8 *px = w->cpuVisibleBuffer.ptrNonConst;
	U64 len = Buffer_length(w->cpuVisibleBuffer);

	for (U64 i = 0; i < len; ++i)
		px[i] = F18_INIT_COLOR;

	//Scale the brush with the surface, a single pixel is invisible on a phone (~0.05mm on a 1080x2237 panel) so the
	// test looks like it did nothing even when it passes.
	//Stays 1px on a small desktop window, a handful on a high dpi display.

	I32 brush = (W < H ? W : H) / 128;

	if(brush < 1)
		brush = 1;

	for (U32 i = 0; i < f18.pointCount; ++i) {

		const I32 cx = I32x2_x(f18.points[i]);
		const I32 cy = I32x2_y(f18.points[i]);

		for (I32 y = cy - brush + 1; y < cy + brush; ++y)
			for (I32 x = cx - brush + 1; x < cx + brush; ++x)
				if (x >= 0 && x < W && y >= 0 && y < H)
					*(U32*)(px + (y * W + x) * 4) = 0xFFFF00FF;
	}
}

static void F18_onButton(Window *w, InputDevice *dev, InputHandle h, Bool down) {

	(void)w;
	if (dev->type != EInputDeviceType_Mouse)
		return;

	U16 local = InputDevice_getLocalHandle(dev, h);
	if (local == (U16)(EMouseButton_Left - EMouseAxis_End))
		f18.buttonHeld = down;
}

//Drive the window manager for 'ns', presenting on every step while the button
// is held so onDraw gets to paint each new cursor position along the drag.
static void F18_pumpAndPaint(Window *w, Ns ns) {

	Ns waited = 0;
	while (waited < ns) {

		WindowManager_step(&windowManager, NULL, NULL);

		if(!windowManager.windows.length)
			break;

		if (f18.buttonHeld)
			presentQuiet(w);

		Thread_sleep(16 * MS);
		waited += 16 * MS;
	}
}

static void Test_mouseDraw(Test *t) {

	Test_setModule(t, "F18/MouseDraw");

	f18 = (F18State) { 0 };

	WindowCallbacks cbs = (WindowCallbacks){ 0 };
	cbs.onDeviceButton = F18_onButton;
	cbs.onDraw = F18_onDraw;

	I32x2 sz = I32x2_create2(256, 256);
	I32x2 pos = I32x2_create2(300, 300);

	WindowRef *wRef = createWindowCallback(
		t, "F18: Hold left-click and drag to draw",
		pos, sz, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, cbs
	);

	if (!Test_assert(t, "windowCreated", wRef != NULL))
		goto clean;

	Window *w = RefPtr_data(wRef, Window);

	//Fill buffer with a known grey so we can detect changes.
	{
		U8 *px = w->cpuVisibleBuffer.ptrNonConst;
		U64 len = Buffer_length(w->cpuVisibleBuffer);
		for (U64 i = 0; i < len; ++i)
			px[i] = F18_INIT_COLOR;
	}

	present(t, w);
	pump(300 * MS);

	if (w->type != EWindowType_Physical) {
		Test_print(t, "[virtual] mouse draw requires a physical window, skipped");
		goto clean;
	}

	//Synthetic injection
	#if _PLATFORM_TYPE == PLATFORM_WINDOWS

		{
			//Move to window centre, press left, drag 100px right, release

			POINT centre = { 300 + 128, 300 + 128 };
			SetCursorPos(centre.x, centre.y);
			Sleep(50);

			INPUT inputs[2] = { 0 };
			inputs[0].type = INPUT_MOUSE; inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
			SendInput(1, inputs, sizeof(INPUT));

			for (I32 dx = 0; dx <= 100; dx += 5) {

				SetCursorPos(centre.x + dx, centre.y);
				Sleep(16);
				WindowManager_step(&windowManager, NULL, NULL);

				if(!windowManager.windows.length)
					break;

				if (f18.buttonHeld)
					presentQuiet(w);
			}

			inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTUP;
			SendInput(1, inputs, sizeof(INPUT));
			pump(200 * MS);
		}

	#elif _PLATFORM_TYPE == PLATFORM_LINUX

		if (!isSingleWindow() && hasXdotool()) {

			//Move to centre, mousedown, drag, mouseup

			runXdotool(
				"WIN=$(xdotool search --name 'F18:') && "
				"xdotool mousemove --window $WIN 128 128 && "
				"xdotool mousedown 1"
			);

			for (I32 dx = 0; dx <= 80; dx += 4) {

				C8 cmd[256];
				snprintf(cmd, sizeof(cmd),
					"WIN=$(xdotool search --name 'F18:') && "
					"xdotool mousemove --window $WIN %d 128", 128 + dx
				);

				runXdotool(cmd);
				Thread_sleep(16 * MS);
				WindowManager_step(&windowManager, NULL, NULL);

				if(!windowManager.windows.length)
					break;

				if (f18.buttonHeld)
					presentQuiet(w);
			}

			runXdotool("xdotool mouseup 1");
			pump(200 * MS);
		}

		else Test_print(t, "xdotool not available, skipping synthetic mouse draw");

	#endif

	//Verify at least one pixel changed
	{
		const U8 *px = w->cpuVisibleBuffer.ptr;

		if(!px)
			goto clean;

		I32 W = I32x2_x(w->size), H = I32x2_y(w->size);
		Bool anyChanged = false;

		for (I32 i = 0; i < W * H && !anyChanged; ++i)
			if (px[i * 4] != F18_INIT_COLOR)
				anyChanged = true;

		present(t, w);
		pump(VISUAL_HOLD_NS);

		if(!px)
			goto clean;

		if (!anyChanged)
			Test_print(t, "WARN: no pixels changed (synthetic draw didn't fire, try interactive)");

		//Interactive fallback

		if (!anyChanged) {

			Test_print(t, ">>> INTERACTIVE: Hold left-click and drag across the window (8s) <<<");

			F18_pumpAndPaint(w, 8 * SECOND);

			if(!windowManager.windows.length)
				goto clean;

			anyChanged = false;
			for (I32 i = 0; i < W * H && !anyChanged; ++i)
				if (px[i * 4] != F18_INIT_COLOR)
					anyChanged = true;

			Test_assert(t, "operatorDraw", anyChanged);
		}

		else Test_assert(t, "syntheticDraw", true);
	}

clean:
	f18 = (F18State) { 0 };
	RefPtr_dec(&wRef);
}

// -- F19. Scroll wheel (vertical + horizontal) ---------------------------------
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

	(void)w;

	if (dev->type != EInputDeviceType_Mouse)
		return;

	U16 local = InputDevice_getLocalHandle(dev, h);

	if (local == (U16)EMouseAxis_ScrollWheel_Y) {
		f19.scrollY = value;
		f19.gotScrollY = true;
	}

	if (local == (U16)EMouseAxis_ScrollWheel_X) {
		f19.scrollX = value;
		f19.gotScrollX = true;
	}
}

static void Test_scrollWheel(Test *t) {

	Test_setModule(t, "F19/ScrollWheel");

	f19 = (F19State) { 0 };

	WindowCallbacks cbs = (WindowCallbacks) { 0 };
	cbs.onDeviceAxis = F19_onAxis;

	I32x2 sz = I32x2_create2(400, 300);
	I32x2 pos = I32x2_create2(300, 300);

	WindowRef *wRef = createWindowCallback(
		t, "F19: Scroll wheel test (vertical + horizontal)",
		pos, sz, EWindowHint_ProvideCPUBuffer, EWindowFormat_AutoRGBA8, cbs
	);

	if (!Test_assert(t, "windowCreated", wRef != NULL))
		goto clean;

	Window *w = RefPtr_data(wRef, Window);
	present(t, w);
	pump(300 * MS);

	if (w->type != EWindowType_Physical) {
		Test_print(t, "[virtual] scroll wheel test requires a physical window, skipped");
		goto clean;
	}

	//Synthetic injection

	#if _PLATFORM_TYPE == PLATFORM_LINUX

		//xdotool pointer injection uses the X11 XTEST extension and does not
		// reach native Wayland surfaces. Scroll testing is interactive-only on Linux.
		Test_print(t, "Native Wayland: synthetic scroll injection unavailable, interactive-only");

	#elif _PLATFORM_TYPE == PLATFORM_WINDOWS
		{
			//SetCursorPos to window centre, then send WM_MOUSEWHEEL via SendInput
			POINT centre = { 300 + 200, 300 + 150 };
			SetCursorPos(centre.x, centre.y);
			Sleep(50);

			//Vertical scroll down (negative delta by Windows convention)
			INPUT inp = { 0 };
			inp.type = INPUT_MOUSE;
			inp.mi.dwFlags = MOUSEEVENTF_WHEEL;
			inp.mi.mouseData = (DWORD)(WORD)(-WHEEL_DELTA);
			SendInput(1, &inp, sizeof(INPUT));
			pump(200 * MS);

			Test_assert(t, "syntheticScrollY_down", f19.gotScrollY && f19.scrollY != 0.f);

			f19.gotScrollY = false;
			f19.scrollY = 0.f;

			inp.mi.mouseData = (DWORD)(WORD)(WHEEL_DELTA);
			SendInput(1, &inp, sizeof(INPUT));
			pump(200 * MS);

			Test_assert(t, "syntheticScrollY_up", f19.gotScrollY && f19.scrollY != 0.f);

			//Horizontal scroll via MOUSEEVENTF_HWHEEL
			inp.mi.dwFlags = MOUSEEVENTF_HWHEEL;
			inp.mi.mouseData = (DWORD)(WORD)(WHEEL_DELTA);
			SendInput(1, &inp, sizeof(INPUT));
			pump(200 * MS);

			if (f19.gotScrollX)
				Test_assert(t, "syntheticScrollX", f19.scrollX != 0.f);

			else Test_print(t, "WARN: horizontal scroll not received on Windows");
		}
	#else
		Test_print(t, "Synthetic scroll injection not implemented for this platform");
	#endif

	//Interactive fallback for vertical
	if (!f19.gotScrollY) {

		Test_print(t, ">>> INTERACTIVE: Scroll the mouse wheel up/down in the window (8s) <<<");

		Ns waited = 0;
		while (!f19.gotScrollY && waited < 8 * SECOND) {

			WindowManager_step(&windowManager, NULL, NULL);

			if(!windowManager.windows.length)
				break;

			Thread_sleep(16 * MS);
			waited += 16 * MS;
		}

		if (!f19.gotScrollY)
			Test_print(t, "WARN: no vertical scroll received within timeout");

		Test_assert(t, "operatorScrollY", f19.gotScrollY && f19.scrollY != 0.f);
	}

	//Horizontal is optional / device-dependent; only interactive-prompt if
	// vertical worked (confirms the path is wired) but horizontal didn't.
	if (f19.gotScrollY && !f19.gotScrollX) {

		Test_print(t, ">>> INTERACTIVE: Scroll horizontally (tilt wheel or Shift+scroll) (5s, optional) <<<");

		Ns waited = 0;
		while (!f19.gotScrollX && waited < 5 * SECOND) {

			WindowManager_step(&windowManager, NULL, NULL);

			if(!windowManager.windows.length)
				break;

			Thread_sleep(16 * MS);
			waited += 16 * MS;
		}

		if (!f19.gotScrollX)
			Test_print(t, "WARN: no horizontal scroll received (device may not support it, not a failure)");
	}

	pump(VISUAL_HOLD_NS);

clean:
	f19 = (F19State) { 0 };
	RefPtr_dec(&wRef);
}

void Test_functionalInput(Test *t) {
	Test_keyboard(t);
	Test_mouse(t);
	Test_typeChar(t);
	Test_focusReset(t);
	Test_keyboardRemap(t);
	Test_mouseDraw(t);
	Test_scrollWheel(t);
}
