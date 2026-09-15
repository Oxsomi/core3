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

//platforms/web/webwindow.c
//
//Windowing for the web target, backed by a browser <canvas>.
//There is no GPU path yet (no WebGPU backend), so this presents the window's CPU buffer:
// Window.cpuVisibleBuffer is blitted to a 2D canvas context with putImageData every present.
//That is the same buffer the other backends expose, so software rendering written against
// EWindowHint_ProvideCPUBuffer works unchanged here.
//RGBA8 is the only format offered, because that is exactly what ImageData stores.
//Windows exposes BGRA8 instead, because that is what CreateDIBSection hands back.
//Every window owns one canvas, addressed by the CSS selector in its platform payload.
//The first window keeps #canvas, the emscripten default, so a page written for a single canvas keeps working,
// and every next window takes #canvas1, #canvas2 and so on.
//The page provides those elements; a window whose selector matches nothing fails to create,
// since there is nothing this backend could create a canvas out of on its own.
//Under node there is no DOM at all, so createNative reports headless and the generic layer keeps working
// the way it does today ("Physical windows will be unavailable").

#include "platforms/platform.h"
#include "platforms/window_manager.h"
#include "platforms/window.h"
#include "platforms/monitor.h"
#include "platforms/input_device.h"
#include "platforms/keyboard.h"
#include "platforms/mouse.h"
#include "types/base/error.h"
#include "types/container/string.h"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

//"#canvas" plus the ten digits a U32 can reach and the null terminator.

#define WEBWINDOW_SELECTOR_SIZE 24

//Per window platform payload.
//The selector is stored instead of rebuilt per call, because the JS side reads it as a null terminated
// string straight out of wasm memory, so it has to outlive the call.
//canvasId is what the selector was built from, which keeps claiming an id for a new window a comparison
// of numbers instead of strings.

typedef struct WebWindow {
	C8 selector[WEBWINDOW_SELECTOR_SIZE];
	U32 canvasId;
} WebWindow;

U32 Window_extSize = sizeof(WebWindow);

//An EM_JS body is plain JS, so a C string arrives as an address into wasm memory and UTF8ToString decodes it.
//Pointers are 64 bit on wasm64 and reach JS as a BigInt, so they are taken as unsigned long long
// and passed through Number() before anything indexes with them (same as host_crypto.c).

//Is there a DOM at all? False under node, which is where the test bundle runs.
//Kept separate from the per window lookup below, so the headless case is reported once at createNative
// rather than per window or per frame.

EM_JS(int, WebWindow_hasDocument, (), {
	return typeof document !== 'undefined' ? 1 : 0;
});

//Does the page carry the element this window asks for? Answered at creation, so a resize or a blit
// never has to deal with a missing canvas.

EM_JS(int, WebWindow_hasCanvas, (unsigned long long selector), {
	if (typeof document === 'undefined')
		return 0;
	const el = document.querySelector(UTF8ToString(Number(selector)));
	//Not just "an element exists": a div with that id would pass, then resize would silently do nothing
	// and the first blit would throw getContext is not a function out of an EM_JS body.
	return (el && typeof el.getContext === 'function') ? 1 : 0;
});

EM_JS(void, WebWindow_resizeCanvas, (unsigned long long selector, int width, int height), {
	const canvas = document.querySelector(UTF8ToString(Number(selector)));
	canvas.width = width;
	canvas.height = height;
});

EM_JS(void, WebWindow_setTitle, (unsigned long long title), {
	document.title = UTF8ToString(Number(title));
});

//putImageData wants a copy it owns, and the wasm heap can move under memory growth, so the view is taken
// fresh each call and sliced rather than cached.

EM_JS(void, WebWindow_blit, (unsigned long long selector, unsigned long long rgba, int width, int height), {
	const canvas = document.querySelector(UTF8ToString(Number(selector)));
	const ctx = canvas.getContext('2d');
	const start = Number(rgba);
	const pixels = new Uint8ClampedArray(HEAPU8.buffer, start, width * height * 4).slice();
	ctx.putImageData(new ImageData(pixels, width, height), 0, 0);
});

//Writes the selector for a canvas id: id 0 is the bare default, every other id appends its digits.

static void WebWindow_canvasSelector(U32 canvasId, C8 selector[WEBWINDOW_SELECTOR_SIZE]) {

	const C8 prefix[] = "#canvas";
	U64 len = sizeof(prefix) - 1;

	for(U64 i = 0; i < len; ++i)
		selector[i] = prefix[i];

	//Dividing hands back the least significant digit first, so the digits are staged and then reversed in.
	//Id 0 writes none of them, which is what keeps the first window on the bare default id.

	C8 digits[10];
	U8 digitCount = 0;

	for(U32 rest = canvasId; rest; rest /= 10)
		digits[digitCount++] = C8_createDec((U8)(rest % 10));

	while(digitCount)
		selector[len++] = digits[--digitCount];

	selector[len] = '\0';
}

//The lowest id no other live window holds, so a window that is dropped hands its canvas back to the next one.
//The window is already in the manager's list here, so at most windows.length - 1 ids are taken and one of
// the first windows.length ids is always free.

static U32 WebWindow_claimCanvasId(const Window *w) {

	const U64 count = w->owner->windows.length;

	for(U64 id = 0; id < count; ++id) {

		Bool taken = false;

		for(U64 i = 0; i < count && !taken; ++i) {

			Window *wi = RefPtr_data(w->owner->windows.ptr[i], Window);

			if(wi != w && wi->type == EWindowType_Physical)
				taken = WindowExt(wi, WebWindow)->canvasId == id;
		}

		if(!taken)
			return (U32) id;
	}

	return (U32) count;
}

//isSingleWindow stays false: a page can declare as many canvases as it likes and every window binds to
// its own, so the generic layer's one physical window rule doesn't apply here.

Bool WindowManager_createNative(WindowManager *w, Error *e_rr) {

	Bool s_uccess = true;

	//No DOM (node): stay headless rather than failing the platform.
	//Whether the page actually has the element a window wants is decided per window at creation.

	if(!WebWindow_hasDocument())
		return false;

	//platformData has to be non null, since the generic layer uses it as the "native windowing is up" flag
	// (see WindowManager_createWindow's physical branch).

	gotoIfError3(clean, Buffer_createEmptyBytes(1, Platform_instance->alloc, &w->platformData, e_rr));

clean:
	return s_uccess;
}

Bool WindowManager_freeNative(WindowManager *w) {
	Buffer_free(&w->platformData, Platform_instance->alloc);
	return true;
}

Bool WindowManager_updateMonitors(WindowManager *wm, Error *e_rr) {
	(void) wm; (void) e_rr;
	return true;    //The canvas is the whole surface; no monitor enumeration to mirror
}

void WindowManager_updateExt(WindowManager *manager) {
	(void) manager;
}

void WindowManager_freePhysical(Window *w) {
	(void) w;       //cpuVisibleBuffer is freed by the generic layer, and the canvas element belongs to the page
}

Bool WindowManager_supportsFormat(const WindowManager *manager, EWindowFormat format) {
	(void) manager;
	return format == EWindowFormat_RGBA8;   //ImageData is RGBA8, and nothing converts on the way in
}

Bool WindowManager_createWindowPhysical(Window *w, Error *e_rr) {

	Bool s_uccess = true;
	Keyboard builtinKeyboard = (Keyboard) { 0 };
	Mouse builtinMouse = (Mouse) { 0 };
	WebWindow *wwin = WindowExt(w, WebWindow);

	if(w->format != EWindowFormat_RGBA8 && w->format != EWindowFormat_AutoRGBA8)
		retError(clean, Error_invalidState(0, "WindowManager_createWindow() web only supports RGBA8"));

	w->format = EWindowFormat_RGBA8;

	wwin->canvasId = WebWindow_claimCanvasId(w);
	WebWindow_canvasSelector(wwin->canvasId, wwin->selector);

	if(!WebWindow_hasCanvas((unsigned long long) wwin->selector))
		retError(clean, Error_notFound(
			0, 0, "WindowManager_createWindow() the page has no element matching the window's canvas selector"
		));

	//The platform owns the CPU buffer for a physical window (the windows backend gets one from
	// CreateDIBSection); the generic layer frees it when the hint is set.

	if(w->hint & EWindowHint_ProvideCPUBuffer)
		gotoIfError3(clean, Buffer_createEmptyBytes(
			ETextureFormat_getSize((ETextureFormat) w->format, I32x2_x(w->size), I32x2_y(w->size), 1),
			Platform_instance->alloc,
			&w->cpuVisibleBuffer,
			e_rr
		));

	WebWindow_resizeCanvas((unsigned long long) wwin->selector, I32x2_x(w->size), I32x2_y(w->size));

	//Same built in keyboard and mouse the other backends register at creation, so defaultKeyboardId and
	// defaultMouseId are valid. No events are delivered into them yet; html5.h input is future work.

	gotoIfError3(clean, ListInputDevice_reserve(&w->devices, 2, Platform_instance->alloc, e_rr));

	w->defaultKeyboardId = (U32) w->devices.length;
	gotoIfError3(clean, Keyboard_create(&builtinKeyboard, Platform_instance->alloc, e_rr));
	gotoIfError3(clean, ListInputDevice_pushBack(&w->devices, builtinKeyboard, Platform_instance->alloc, e_rr));
	builtinKeyboard = (Keyboard) { 0 };

	w->defaultMouseId = (U32) w->devices.length;
	gotoIfError3(clean, Mouse_create(&builtinMouse, Platform_instance->alloc, e_rr));
	gotoIfError3(clean, ListInputDevice_pushBack(&w->devices, builtinMouse, Platform_instance->alloc, e_rr));
	builtinMouse = (Mouse) { 0 };

	w->flags |= EWindowFlags_IsActive;

clean:

	if(!s_uccess) {

		InputDevice_free(&builtinKeyboard, Platform_instance->alloc);
		InputDevice_free(&builtinMouse, Platform_instance->alloc);

		for(U64 i = 0; i < w->devices.length; ++i)
			InputDevice_free(&w->devices.ptrNonConst[i], Platform_instance->alloc);

		ListInputDevice_free(&w->devices, Platform_instance->alloc);
		Buffer_free(&w->cpuVisibleBuffer, Platform_instance->alloc);
	}

	return s_uccess;
}

//A page has one document.title however many canvases it hosts, so w is validated but does not select
// what gets titled (see the note in platforms/window.h).

Bool Window_updatePhysicalTitle(Window *w, CharString title, Error *e_rr) {

	Bool s_uccess = true;
	CharString nullTerminated = CharString_createNull();

	if(!w)
		retError(clean, Error_nullPointer(0, "Window_updatePhysicalTitle()::w is required"));

	//document.title goes through UTF8ToString, which needs a terminator CharString doesn't guarantee.

	gotoIfError3(clean, CharString_createCopy(title, Platform_instance->alloc, &nullTerminated, e_rr));
	WebWindow_setTitle((unsigned long long) nullTerminated.ptr);

clean:
	CharString_free(&nullTerminated, Platform_instance->alloc);
	return s_uccess;
}

Bool Window_toggleFullScreen(Window *w, Error *e_rr) {

	(void) w;
	Bool s_uccess = true;

	//Browsers only grant fullscreen from inside a user gesture handler, so a programmatic call here would
	// be rejected by the page rather than by us. Refuse it honestly instead of appearing to work.

	retError(clean, Error_unsupportedOperation(
		0, "Window_toggleFullScreen() needs a user gesture on web; request it from an input handler"
	));

clean:
	return s_uccess;
}

Bool Window_presentPhysical(Window *w, Error *e_rr) {

	Bool s_uccess = true;

	if(!w || !I32x2_any(w->size))
		retError(clean, Error_nullPointer(0, "Window_presentPhysical()::w is required"));

	if(!(w->flags & EWindowFlags_IsActive) || !(w->hint & EWindowHint_ProvideCPUBuffer))
		retError(clean, Error_invalidOperation(0, "Window_presentPhysical() can only be called if there's a CPU-sided buffer"));

	//A virtual window carries no platform payload, so WindowExt is NULL there. It reaches this far
	// because the generic layer forces ProvideCPUBuffer and IsActive on virtual windows too.

	if(w->type != EWindowType_Physical)
		retError(clean, Error_invalidOperation(1, "Window_presentPhysical() requires a physical window"));

	WebWindow_blit(
		(unsigned long long) WindowExt(w, WebWindow)->selector,
		(unsigned long long) w->cpuVisibleBuffer.ptr,
		I32x2_x(w->size),
		I32x2_y(w->size)
	);

clean:
	return s_uccess;
}
