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
//Single window, like android: there is one canvas, addressed by the emscripten default selector.
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

U32 Window_extSize = 0; //One canvas, so no per window platform payload

//Is there a DOM with a usable canvas? False under node, which is where the test bundle runs.
//Kept separate from the blit so the failure is reported once at createNative rather than per frame.

EM_JS(int, WWindow_hasCanvas, (), {
	return (typeof document !== 'undefined' && document.getElementById('canvas')) ? 1 : 0;
});

EM_JS(void, WWindow_resizeCanvas, (int width, int height), {
	const canvas = document.getElementById('canvas');
	canvas.width = width;
	canvas.height = height;
});

EM_JS(void, WWindow_setTitle, (const char *title), {
	document.title = UTF8ToString(title);
});

//putImageData wants a copy it owns, and the wasm heap can move under memory growth, so the view is taken
// fresh each call and sliced rather than cached.

EM_JS(void, WWindow_blit, (const unsigned char *rgba, int width, int height), {
	const canvas = document.getElementById('canvas');
	const ctx = canvas.getContext('2d');
	const start = Number(rgba);
	const pixels = new Uint8ClampedArray(HEAPU8.buffer, start, width * height * 4).slice();
	ctx.putImageData(new ImageData(pixels, width, height), 0, 0);
});

Bool WindowManager_createNative(WindowManager *w, Error *e_rr) {

	Bool s_uccess = true;

	//No DOM (node), or a page without the canvas element: stay headless rather than failing the platform.

	if(!WWindow_hasCanvas())
		return false;

	//platformData has to be non null, since the generic layer uses it as the "native windowing is up" flag
	// (see WindowManager_createWindow's physical branch).

	gotoIfError3(clean, Buffer_createEmptyBytes(1, Platform_instance->alloc, &w->platformData, e_rr));

	w->isSingleWindow = true;

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
	(void) w;       //cpuVisibleBuffer is freed by the generic layer, and the canvas outlives the window
}

Bool WindowManager_supportsFormat(const WindowManager *manager, EWindowFormat format) {
	(void) manager;
	return format == EWindowFormat_RGBA8;   //ImageData is RGBA8, and nothing converts on the way in
}

Bool WindowManager_createWindowPhysical(Window *w, Error *e_rr) {

	Bool s_uccess = true;
	Keyboard builtinKeyboard = (Keyboard) { 0 };
	Mouse builtinMouse = (Mouse) { 0 };

	for(U64 i = 0; i < w->owner->windows.length; ++i) {

		Window *wi = RefPtr_data(w->owner->windows.ptr[i], Window);

		if(wi != w && wi->type == EWindowType_Physical)
			retError(clean, Error_invalidState(0, "WindowManager_createWindow() there can be only one canvas on web"));
	}

	if(w->format != EWindowFormat_RGBA8 && w->format != EWindowFormat_AutoRGBA8)
		retError(clean, Error_invalidState(0, "WindowManager_createWindow() web only supports RGBA8"));

	w->format = EWindowFormat_RGBA8;

	//The platform owns the CPU buffer for a physical window (the windows backend gets one from
	// CreateDIBSection); the generic layer frees it when the hint is set.

	if(w->hint & EWindowHint_ProvideCPUBuffer)
		gotoIfError3(clean, Buffer_createEmptyBytes(
			ETextureFormat_getSize((ETextureFormat) w->format, I32x2_x(w->size), I32x2_y(w->size), 1),
			Platform_instance->alloc,
			&w->cpuVisibleBuffer,
			e_rr
		));

	WWindow_resizeCanvas(I32x2_x(w->size), I32x2_y(w->size));

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

Bool Window_updatePhysicalTitle(Window *w, CharString title, Error *e_rr) {

	Bool s_uccess = true;
	CharString nullTerminated = CharString_createNull();

	if(!w)
		retError(clean, Error_nullPointer(0, "Window_updatePhysicalTitle()::w is required"));

	//document.title goes through UTF8ToString, which needs a terminator CharString doesn't guarantee.

	gotoIfError3(clean, CharString_createCopy(title, Platform_instance->alloc, &nullTerminated, e_rr));
	WWindow_setTitle(nullTerminated.ptr);

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

	WWindow_blit(w->cpuVisibleBuffer.ptr, I32x2_x(w->size), I32x2_y(w->size));

clean:
	return s_uccess;
}
