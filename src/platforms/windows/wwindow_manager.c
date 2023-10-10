/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/window_manager.h"
#include "platforms/platform.h"
#include "platforms/monitor.h"
#include "platforms/input_device.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/stringx.h"
#include "types/string.h"
#include "types/error.h"
#include "types/buffer.h"

#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

const U8 WindowManager_MAX_PHYSICAL_WINDOWS = 16;

//Defined in wwindow.c
//
LRESULT CALLBACK WWindow_onCallback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

Error WWindow_initSize(Window *w, I32x2 size);

//Window belongs to calling thread.
//So we create the window in a separate thread.

void Window_physicalLoop(Window *w) {

	Error err = Error_none();
	
	//Create native window

	WNDCLASSEXA wc = (WNDCLASSEXA){ 0 };
	HINSTANCE mainModule = Platform_instance.data;

	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WWindow_onCallback;
	wc.hInstance = mainModule;

	wc.hIcon = (HICON) LoadImageA(mainModule, "LOGO", IMAGE_ICON, 32, 32, 0);
	wc.hIconSm = (HICON) LoadImageA(mainModule, "LOGO", IMAGE_ICON, 16, 16, 0);

	wc.hCursor = LoadCursorA(NULL, IDC_ARROW);

	wc.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);

	wc.lpszClassName = "OxC3: Oxsomi core 3";
	wc.cbSize = sizeof(wc);
	wc.cbWndExtra = sizeof(void*);

	if (!RegisterClassExA(&wc))
		_gotoIfError(clean, Error_platformError(0, GetLastError()));

	DWORD style = WS_VISIBLE;

	Bool isFullScreen = w->hint & EWindowHint_ForceFullscreen;

	if(!isFullScreen) {

		style |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

		if(!(w->hint & EWindowHint_DisableResize))
			style |= WS_SIZEBOX | WS_MAXIMIZEBOX;
	}

	else style |= WS_POPUP;

	I32x2 maxSize = I32x2_create2(
		GetSystemMetrics(SM_CXSCREEN), 
		GetSystemMetrics(SM_CYSCREEN) 
	);

	I32x2 size = w->size;
	I32x2 position = w->offset;

	for (U8 i = 0; i < 2; ++i)
		if (isFullScreen || (!I32x2_get(size, i) || I32x2_get(size, i) >= I32x2_get(maxSize, i)))
			I32x2_set(&size, i, I32x2_get(maxSize, i));

	//Our strings aren't null terminated, so ensure windows doesn't read garbage

	C8 windowName[MAX_PATH + 1];
	Buffer_copy(Buffer_createRef(windowName, sizeof(windowName)), CharString_bufferConst(w->title));

	windowName[CharString_length(w->title)] = '\0';

	HWND nativeWindow = CreateWindowExA(
		WS_EX_APPWINDOW, wc.lpszClassName, windowName, style,
		I32x2_x(position), I32x2_y(position),
		I32x2_x(size), I32x2_y(size),
		NULL, NULL, mainModule, NULL
	);

	if(!nativeWindow) {
		HRESULT hr = GetLastError();
		UnregisterClassA(wc.lpszClassName, wc.hInstance);
		_gotoIfError(clean, Error_platformError(1, hr));
	}

	//Get real size and position

	RECT r = (RECT) { 0 };
	GetClientRect(nativeWindow, &r);
	w->size = I32x2_create2(r.right - r.left, r.bottom - r.top);

	GetWindowRect(nativeWindow, &r);
	w->offset = I32x2_create2(r.left, r.top);

	//Alloc cpu visible buffer if needed

	w->devices = List_createEmpty(sizeof(InputDevice));
	w->monitors = List_createEmpty(sizeof(Monitor));

	_gotoIfError(clean, WWindow_initSize(w, w->size));

	//Lock for when we are updating this window

	_gotoIfError(clean, List_reservex(&w->devices, Window_MAX_DEVICES));
	_gotoIfError(clean, List_reservex(&w->monitors, Window_MAX_MONITORS));

	w->nativeHandle = nativeWindow;

	//Bind our window

	SetWindowLongPtrA(nativeWindow, 0, (LONG_PTR) w);

	if(w->hint & EWindowHint_ForceFullscreen)
		w->flags |= EWindowFlags_IsFullscreen;

	//Signal ready

	if(w->callbacks.onCreate)
		w->callbacks.onCreate(w);

	if(w->callbacks.onResize)
		w->callbacks.onResize(w);

	w->flags |= EWindowFlags_IsActive;

	//Ensure we get all input devices
	//Register for raw input of these types
	//https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/hid-usages#usage-page

	RAWINPUTDEVICE registerDevices[2] = {
		{									//Keyboard
			.dwFlags = RIDEV_DEVNOTIFY,
			.usUsagePage = 1,
			.usUsage = 6,
			.hwndTarget = nativeWindow
		},
		{									//Mouse
			.dwFlags = RIDEV_DEVNOTIFY,
			.usUsagePage = 1,
			.usUsage = 2,
			.hwndTarget = nativeWindow
		}
	};

	if (!RegisterRawInputDevices(registerDevices, 2, sizeof(registerDevices[0])))
		_gotoIfError(clean, Error_invalidState(0));

	//Window loop

	if(w->callbacks.onDraw) {

		MSG msg = (MSG) { 0 };

		while(!(w->flags & EWindowFlags_ShouldThreadTerminate)) {

			if(Lock_lock(&w->lock, 5 * SECOND)) {

				if (PeekMessageA(&msg, w->nativeHandle, 0, 0, PM_REMOVE)) {
					TranslateMessage(&msg);
					DispatchMessageA(&msg);
				}

				else InvalidateRect(w->nativeHandle, NULL, false);

				if(msg.message == WM_QUIT)
					w->flags |= EWindowFlags_ShouldThreadTerminate;

				Lock_unlock(&w->lock);
			}
		}
	}

	//Free the window if we're done with the window loop

	while (!Lock_lock(&Platform_instance.windowManager.lock, 1 * SECOND)) { }

	Lock_lock(&w->lock, 1 * SECOND);
	
	Window *w0 = w;
	
	WindowManager_freePhysical(&Platform_instance.windowManager, &w);
	*w0 = (Window) { 0 };
	
	Lock_unlock(&Platform_instance.windowManager.lock);
	return;

	//When an error happens, we're not the one freeing it, rather the calling thread would be.

clean:
	Lock_lock(&w->lock, 1 * SECOND);
	w->creationError = err;						//Signal error
	Lock_unlock(&w->lock);
}

Error WindowManager_createPhysical(
	WindowManager *manager,
	I32x2 position,
	I32x2 size, 
	I32x2 minSize,
	I32x2 maxSize,
	EWindowHint hint,
	CharString title, 
	WindowCallbacks callbacks,
	EWindowFormat format,
	Window **w
) {

	//Validate state

	if(!manager)
		return Error_nullPointer(0);

	if(!Lock_isLockedForThread(manager->lock))
		return Error_invalidOperation(0);

	if(!w)
		return Error_nullPointer(9);

	if(*w)
		return Error_invalidParameter(9, 0);

	switch (format) {

		case EWindowFormat_bgra8:
		case EWindowFormat_bgr10a2:
		case EWindowFormat_rgba16f:
		case EWindowFormat_rgba32f:

			if(!WindowManager_supportsFormat(*manager, format))
				return Error_unsupportedOperation(0);

			break;

		default:
			return Error_invalidParameter(8, 0);
	}

	//Ensure the sizes are valid

	Error err = WindowManager_adaptSizes(&size, &minSize, &maxSize);

	if(err.genericError)
		return err;

	//Validate path

	if (CharString_length(title) >= MAX_PATH)
		return Error_outOfBounds(6, CharString_length(title), MAX_PATH);

	//Find free spot in physical windows

	Window *win = NULL;
	WindowHandle handle = 0;

	for(U8 i = 0; i < WindowManager_MAX_PHYSICAL_WINDOWS; ++i) {

		Window *wi = (Window*) List_ptr(manager->windows, i);

		if(!wi)
			break;

		if (!(wi->flags & EWindowFlags_IsActive)) {
			win = wi;
			handle = i;
			break;
		}
	}

	if(!win)
		return Error_outOfMemory(WindowManager_MAX_PHYSICAL_WINDOWS);

	//Fill window object so our thread can read from it

	*win = (Window) {

		.offset = position,
		.size = size,
		.prevSize = size,

		.callbacks = callbacks,

		.hint = hint,
		.format = format,

		.minSize = minSize,
		.maxSize = maxSize
	};

	_gotoIfError(clean, CharString_createCopyx(title, &win->title));

	_gotoIfError(clean, Lock_create(&win->lock));

	//Start event loop thread

	_gotoIfError(clean, Thread_create((ThreadCallbackFunction) Window_physicalLoop, win, &win->mainThread));

	Thread_sleep(1 * MS);

	//Wait for window creation...
	//It either sets isActive or the error
	while (win->lock.data) {

		if (Lock_lock(&win->lock, 1 * SECOND)) {

			Bool b = (win->flags & EWindowFlags_IsActive) || win->creationError.genericError;
			Lock_unlock(&win->lock);

			if(b)
				break;
		}

		Thread_sleep(1 * MS);
	}

	_gotoIfError(clean, win->creationError);

	//

	*w = win;

clean:

	if(err.genericError) {

		WindowManager_freePhysical(&Platform_instance.windowManager, &win);

		Lock_free(&win->lock);
		*win = (Window) { 0 };
	}

	return err;
}
