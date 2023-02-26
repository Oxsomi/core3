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

Error WindowManager_createPhysical(
	WindowManager *manager,
	I32x2 position,
	I32x2 size, 
	EWindowHint hint,
	String title, 
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
		return Error_nullPointer(7);

	if(*w)
		return Error_invalidParameter(7, 0);

	switch (format) {

		case EWindowFormat_rgba8:
		case EWindowFormat_hdr10a2:
		case EWindowFormat_rgba16f:
		case EWindowFormat_rgba32f:

			if(!WindowManager_supportsFormat(*manager, format))
				return Error_unsupportedOperation(0);

			break;

		default:
			return Error_invalidParameter(6, 0);
	}

	if(I32x2_any(I32x2_lt(size, I32x2_zero())))
		return Error_invalidParameter(2, 0);

	if (String_length(title) >= MAX_PATH)
		return Error_outOfBounds(4, String_length(title), MAX_PATH);

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
		return Error_platformError(0, GetLastError());

	DWORD style = WS_VISIBLE;

	if(!(hint & EWindowHint_ForceFullscreen)) {

		style |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

		if(!(hint & EWindowHint_DisableResize))
			style |= WS_SIZEBOX | WS_MAXIMIZEBOX;
	}

	if(!(hint & EWindowHint_HandleInput))
		style |= WS_DISABLED;

	I32x2 maxSize = I32x2_create2(
		GetSystemMetrics(SM_CXSCREEN), 
		GetSystemMetrics(SM_CYSCREEN) 
	);

	for (U8 i = 0; i < 2; ++i)
		if (!I32x2_get(size, i) || I32x2_get(size, i) >= I32x2_get(maxSize, i))
			I32x2_set(&size, i, I32x2_get(maxSize, i));

	//Our strings aren't null terminated, so ensure windows doesn't read garbage

	C8 windowName[MAX_PATH + 1];
	Buffer_copy(Buffer_createRef(windowName, sizeof(windowName)), String_bufferConst(title));

	windowName[String_length(title)] = '\0';

	HWND nativeWindow = CreateWindowExA(
		WS_EX_APPWINDOW, wc.lpszClassName, windowName, style,
		I32x2_x(position), I32x2_y(position),
		I32x2_x(size), I32x2_y(size),
		NULL, NULL, mainModule, NULL
	);

	if(!nativeWindow) {
		HRESULT hr = GetLastError();
		UnregisterClassA(wc.lpszClassName, wc.hInstance);
		return Error_platformError(1, hr);
	}

	//Get real size and position

	RECT r = (RECT){ 0 };
	GetClientRect(nativeWindow, &r);
	size = I32x2_create2(r.right - r.left, r.bottom - r.top);

	GetWindowRect(nativeWindow, &r);
	position = I32x2_create2(r.left, r.top);

	//Bind our window

	SetWindowLongPtrA(nativeWindow, 0, (LONG_PTR) win);

	//Alloc cpu visible buffer if needed

	Buffer cpuVisibleBuffer = Buffer_createNull();
	void *nativeData = NULL;
	HDC screen = NULL;

	Lock lock = (Lock) { 0 };

	List devices = List_createEmpty(sizeof(InputDevice)), monitors = List_createEmpty(sizeof(Monitor));

	Error err = Error_none();

	if(hint & EWindowHint_ProvideCPUBuffer) {

		screen = GetDC(nativeWindow);

		if(!screen)
			_gotoIfError(clean, Error_platformError(2, GetLastError()));

		//TODO: Support something other than RGBA8

		BITMAPINFO bmi = (BITMAPINFO) {
			.bmiHeader = {
				.biSize = sizeof(BITMAPINFOHEADER),
				.biWidth = (DWORD) I32x2_x(size),
				.biHeight = (DWORD) I32x2_y(size),
				.biPlanes = 1,
				.biBitCount = 32,
				.biCompression = BI_RGB
			}
		};

		nativeData = CreateDIBSection(
			screen, &bmi, DIB_RGB_COLORS, (void**) &cpuVisibleBuffer.ptr, 
			NULL, 0
		);

		if(!screen)
			_gotoIfError(clean, Error_platformError(3, GetLastError()));

		//Manually set it to be a reference
		//This makes it so we don't free it, because we don't own the memory

		cpuVisibleBuffer.lengthAndRefBits = ((U64)bmi.bmiHeader.biWidth * bmi.bmiHeader.biHeight * 4) | ((U64)1 << 31);

		ReleaseDC(nativeWindow, screen);
		screen = NULL;
	}

	//Lock for when we are updating this window

	_gotoIfError(clean, Lock_create(&lock));

	_gotoIfError(clean, List_reservex(&devices, Window_MAX_DEVICES));
	_gotoIfError(clean, List_reservex(&monitors, Window_MAX_MONITORS));

	//Fill window object

	*win = (Window) {

		.offset = position,
		.size = size,

		.cpuVisibleBuffer = cpuVisibleBuffer,

		.nativeHandle = nativeWindow,
		.nativeData = nativeData,
		.lock = lock,

		.callbacks = callbacks,

		.hint = hint,
		.format = format,
		.flags = EWindowFlags_IsActive,

		.devices = devices,
		.monitors = monitors
	};

	if(hint & EWindowHint_ForceFullscreen)
		win->flags |= EWindowFlags_IsFullscreen;

	*w = win;

	//Signal ready

	if(callbacks.onCreate)
		callbacks.onCreate(win);

	//Our window is now ready, start window loop

	UpdateWindow(nativeWindow);
	return Error_none();

clean:

	List_freex(&devices);
	List_freex(&monitors);

	Lock_free(&lock);

	if(screen)
		ReleaseDC(nativeWindow, screen);

	if(nativeData)
		DeleteObject((HGDIOBJ) nativeData);

	UnregisterClassA(wc.lpszClassName, wc.hInstance);
	DestroyWindow(nativeWindow);
	return err;
}
