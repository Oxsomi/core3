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

//platforms/windows/wwindow_manager.c

#include "types/container/list_impl.h"
#include "types/container/list_basic_types.h"
#include "platforms/window_manager.h"
#include "platforms/window.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "types/container/log.h"
#include "types/base/error.h"

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#define NOMINMAX
#include <Windows.h>
#include <dwrite.h>

LRESULT CALLBACK WWindow_onCallback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

typedef struct WndClassExExW {
	WNDCLASSEXW wnd;
	HICON icon, iconSm;
} WndClassExExW;

Bool WindowManager_createNative(WindowManager *w, Error *e_rr) {

	Bool s_uccess = true;

	HANDLE hIcon = NULL, hIconSm = NULL;

	gotoIfError3(clean, Buffer_createEmptyBytes(sizeof(WndClassExExW), Platform_instance->alloc, &w->platformData, e_rr));

	WndClassExExW *wc = (WndClassExExW*) w->platformData.ptr;

	const HINSTANCE mainModule = Platform_instance->data;

	hIcon = LoadImageW(mainModule, L"LOGO", IMAGE_ICON, 32, 32, 0);

	if (!hIcon) {

		Log_warnLnx("LoadIconW for the LOGO failed, falling back to default...");

		if((hIcon = LoadIconW(NULL, IDI_APPLICATION)) == NULL)
			retError(clean, Error_platformError(0, GetLastError(), "LoadIconW fallback failed"));
	}

	else hIconSm = LoadImageW(mainModule, L"LOGO", IMAGE_ICON, 16, 16, 0);

	HANDLE hCursor = NULL;
	if((hCursor = LoadCursorW(NULL, IDC_ARROW)) == NULL)
		retError(clean, Error_platformError(0, GetLastError(), "LoadCursorW failed for standard cursor"));

	wc->wnd = (WNDCLASSEXW) {

		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = WWindow_onCallback,
		.hInstance = mainModule,

		.hIcon = (HICON) hIcon,
		.hIconSm = (HICON) (hIconSm ? hIconSm : hIcon),

		.hCursor = (HCURSOR) hCursor,

		.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH),

		.lpszClassName = L"OxC3: Oxsomi core 3",
		.cbSize = sizeof(WNDCLASSEXW),
		.cbWndExtra = sizeof(void*),
	};

	if (!RegisterClassExW(&wc->wnd)) {
		wc->wnd.hInstance = NULL;
		retError(clean, Error_platformError(
			0, GetLastError(), "WindowManager_createNative() RegisterClassEx failed"
		));
	}

	gotoIfError3(clean, WindowManager_updateMonitors(w, e_rr));

	wc->icon = (HICON) hIcon;
	wc->iconSm = (HICON) hIconSm;
	hIcon = NULL;
	hIconSm = NULL;

clean:

	if(hIconSm)
		DestroyIcon((HICON)hIconSm);

	if(hIcon)
		DestroyIcon((HICON)hIcon);

	return s_uccess;
}

Bool WindowManager_freeNative(WindowManager *w) {

	const WndClassExExW *wc = (const WndClassExExW*)w->platformData.ptr;

	if(wc->wnd.hInstance)
		UnregisterClassW(wc->wnd.lpszClassName, wc->wnd.hInstance);

	if(wc->iconSm)
		DestroyIcon(wc->iconSm);

	if(wc->icon)
		DestroyIcon(wc->icon);

	return true;
}

void WindowManager_updateExt(WindowManager *manager) {

	MSG msg = (MSG) { 0 };
	U64 seenWindows[4] = { 0 };
	ListU64 seenWindowsLarge = (ListU64) { 0 };
	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	if (manager->windows.length > 256) {
		gotoIfError3(clean, ListU64_resize(
			&seenWindowsLarge, (manager->windows.length + 63) >> 6, Platform_instance->alloc, e_rr
		));
	}

	else gotoIfError3(clean, ListU64_createRefConst(seenWindows, 4, &seenWindowsLarge, e_rr));

	while(PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {

		if (msg.message == WM_PAINT) {

			//Find physical window

			Bool duplicatePaint = false;

			for(U64 i = 0; i < manager->windows.length; ++i) {

				Window *w = manager->windows.ptr[i];

				if (msg.hwnd == w->nativeHandle && w->type == EWindowType_Physical) {

					if ((seenWindowsLarge.ptrNonConst[i >> 6] >> (i & 63)) & 1) {
						duplicatePaint = true;
						break;
					}

					seenWindowsLarge.ptrNonConst[i >> 6] |= (U64)1 << (i & 63);
					break;
				}
			}

			if(duplicatePaint)        //Ensure our manager draw/update happens too for next frame
				break;
		}

		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	if(manager->monitorsDirty) {
		manager->monitorsDirty = false;
		gotoIfError3(clean, WindowManager_updateMonitors(manager, e_rr));
	}

clean:
	ListU64_free(&seenWindowsLarge, Platform_instance->alloc);
	Error_print(Platform_instance->alloc, e_rr, ELogLevel_Error, ELogOptions_NewLine);
}

static I32x2 rotateOffset(I32x2 p, EMonitorOrientation o) {
	switch (o) {
		default:
		case EMonitorOrientation_Landscape:          return p;
		case EMonitorOrientation_Portrait:           return I32x2_create2(p.y, p.x);
		case EMonitorOrientation_FlippedLandscape:   return I32x2_create2(-p.x, p.y);
		case EMonitorOrientation_FlippedPortrait:    return I32x2_create2(p.y, -p.x);
	}
}

typedef struct DataEnumDisplayMonitors {
	IDWriteFactory *factory;
	ListMonitor *monitors;
} DataEnumDisplayMonitors;

//Callback passed to EnumDisplayMonitors. Called once per monitor the WWindow
// overlaps (or all monitors when called from WindowManager).
//Populates w->monitors with geometry, refresh rate, and orientation
BOOL WWindowManager_enumMonitor(HMONITOR hmon, HDC hdc, LPRECT rect, LPARAM lParam) {

	(void) hdc; (void) rect;
	DataEnumDisplayMonitors *dat = (DataEnumDisplayMonitors*) lParam;
 
	MONITORINFOEXW info = (MONITORINFOEXW) { .cbSize = sizeof(MONITORINFOEXW) };
 
	if(!GetMonitorInfoW(hmon, (MONITORINFO*) &info))
		return TRUE;   //Skip this monitor but continue enumeration
 
	DEVMODEW dm = (DEVMODEW) { .dmSize = sizeof(DEVMODEW) };
	F32 refreshRate = 0.f;
	EMonitorOrientation orientation = EMonitorOrientation_Landscape;
 
	if(EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &dm)) {
		refreshRate = dm.dmDisplayFrequency > 1 ? (F32)dm.dmDisplayFrequency : 0.f;
 
		switch(dm.dmDisplayOrientation) {
			case DMDO_DEFAULT: orientation = EMonitorOrientation_Landscape;         break;
			case DMDO_90:      orientation = EMonitorOrientation_Portrait;          break;
			case DMDO_180:     orientation = EMonitorOrientation_FlippedLandscape;  break;
			case DMDO_270:     orientation = EMonitorOrientation_FlippedPortrait;   break;
		}
	}
 
	//Physical size in mm via GetDeviceCaps, requires a DC for the monitor.
	//We open a temporary DC on the device name for this.
	I32x2 sizeMm = I32x2_zero;
	HDC mdc = CreateDCW(L"DISPLAY", info.szDevice, NULL, NULL);
 
	if(mdc) {
		sizeMm = I32x2_create2(GetDeviceCaps(mdc, HORZSIZE), GetDeviceCaps(mdc, VERTSIZE));
		DeleteDC(mdc);
	}
 
	const RECT *work = &info.rcMonitor;

	I32x2 offsetR = I32x2_zero;
	I32x2 offsetG = I32x2_zero;
	I32x2 offsetB = I32x2_zero;

	IDWriteRenderingParams *params = NULL;
	DWRITE_PIXEL_GEOMETRY geometry = DWRITE_PIXEL_GEOMETRY_FLAT;

	if (SUCCEEDED(dat->factory->CreateMonitorRenderingParams(hmon, &params))) {
		geometry = params->GetPixelGeometry();
		params->Release();
	}

	switch (geometry) {

		case DWRITE_PIXEL_GEOMETRY_RGB:
			offsetR = I32x2_create2(-1, 0);
			offsetG = I32x2_create2( 0, 0);
			offsetB = I32x2_create2( 1, 0);
			break;

		case DWRITE_PIXEL_GEOMETRY_BGR:
			offsetR = I32x2_create2( 1, 0);
			offsetG = I32x2_create2( 0, 0);
			offsetB = I32x2_create2(-1, 0);
			break;

		default:
			break;
	}
 
	Monitor m = (Monitor) {
		.offsetPixels = I32x2_create2(work->left,                work->top),
		.sizePixels   = I32x2_create2(work->right  - work->left, work->bottom - work->top),
		.offsetR      = rotateOffset(offsetR, orientation),
		.offsetG      = rotateOffset(offsetG, orientation),
		.offsetB      = rotateOffset(offsetB, orientation),
		.sizeMm       = sizeMm,
		.refreshRate  = refreshRate,
		.orientation  = orientation
	};
 
	Error err = Error_none();
	if(!ListMonitor_pushBack(dat->monitors, m, Platform_instance->alloc, &err))
		Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);
 
	return TRUE;   //continue enumeration
}

//Called from WWindow with a clip rect
Bool WindowManager_updateMonitorsExt(ListMonitor *monitors, LPCRECT clip, Error *e_rr) {

	Bool s_uccess = true;
	IDWriteFactory *factory = NULL;

	ListMonitor_clear(monitors, NULL);

	HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown**)&factory);

	if(FAILED(hr))
		retError(clean, Error_platformError(0, GetLastError(), "WindowManager_updateMonitors() failed to create factory"));

	DataEnumDisplayMonitors data = (DataEnumDisplayMonitors) { .factory = factory, .monitors = monitors };

	if(!EnumDisplayMonitors(NULL, clip, WWindowManager_enumMonitor, (LPARAM) &data))
		retError(clean, Error_platformError(0, GetLastError(), "WindowManager_updateMonitors() failed to EnumDisplayMonitors"));

clean:
	if(factory) IDWriteFactory_Release(factory);
	return s_uccess;
}

Bool WindowManager_updateMonitors(WindowManager *wm, Error *e_rr) {

	Bool s_uccess = true;

	if(!wm)
		retError(clean, Error_nullPointer(0, "WindowManager_updateMonitors()::wm is required"));

	gotoIfError3(clean, WindowManager_updateMonitorsExt(&wm->monitors, NULL, e_rr));

	if(wm->callbacks.onMonitorChange)
		wm->callbacks.onMonitorChange(wm);

clean:
	return s_uccess;
}
