/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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
#include "platforms/window_manager.h"
#include "platforms/window.h"
#include "platforms/platform.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/errorx.h"
#include "platforms/log.h"
#include "types/error.h"

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#define NOMINMAX
#include <Windows.h>

LRESULT CALLBACK WWindow_onCallback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

Bool WindowManager_createNative(WindowManager *w, Error *e_rr) {

	Bool s_uccess = true;
	gotoIfError2(clean, Buffer_createEmptyBytesx(sizeof(WNDCLASSEXW), &w->platformData))

	WNDCLASSEXW *wc = (WNDCLASSEXW*) w->platformData.ptr;

	const HINSTANCE mainModule = Platform_instance->data;

	*wc = (WNDCLASSEXW) {

		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = WWindow_onCallback,
		.hInstance = mainModule,

		.hIcon = (HICON)LoadImageW(mainModule, L"LOGO", IMAGE_ICON, 32, 32, 0),
		.hIconSm = (HICON)LoadImageW(mainModule, L"LOGO", IMAGE_ICON, 16, 16, 0),

		.hCursor = LoadCursorW(NULL, IDC_ARROW),

		.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH),

		.lpszClassName = L"OxC3: Oxsomi core 3",
		.cbSize = sizeof(*wc),
		.cbWndExtra = sizeof(void*),
	};

	if (!RegisterClassExW(wc))
		retError(clean, Error_platformError(
			0, GetLastError(), "WindowManager_createNative() RegisterClassEx failed"
		))

clean:
	return s_uccess;
}

Bool WindowManager_freeNative(WindowManager *w) {
	const WNDCLASSEXW* wc = (const WNDCLASSEXW*)w->platformData.ptr;
	UnregisterClassW(wc->lpszClassName, wc->hInstance);
	return true;
}

void WindowManager_updateExt(WindowManager *manager) {

	MSG msg = (MSG) { 0 };
	U64 seenWindows[4] = { 0 };
	ListU64 seenWindowsLarge = (ListU64) { 0 };
	Error err = Error_none();

	if(manager->windows.length > 256)
		gotoIfError(clean, ListU64_resizex(&seenWindowsLarge, (manager->windows.length + 63) >> 6))

	else gotoIfError(clean, ListU64_createRefConst(seenWindows, 4, &seenWindowsLarge))

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

			if(duplicatePaint)		//Ensure our manager draw/update happens too for next frame
				break;
		}

		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

clean:
	ListU64_freex(&seenWindowsLarge);
	Error_printLnx(err);
}
