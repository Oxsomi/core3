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
#include "platforms/ext/bufferx.h"
#include "types/error.h"

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

LRESULT CALLBACK WWindow_onCallback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

Error WindowManager_createNative(WindowManager *w) {

	Error err = Buffer_createEmptyBytesx(sizeof(WNDCLASSEXW), &w->platformData);

	if (err.genericError)
		return err;

	WNDCLASSEXW *wc = (WNDCLASSEXW*) w->platformData.ptr;

	HINSTANCE mainModule = Platform_instance.data;

	*wc = (WNDCLASSEXW) {

		.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
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
		return Error_platformError(0, GetLastError(), "WindowManager_createNative() RegisterClassEx failed");

	return Error_none();
}

Bool WindowManager_freeNative(WindowManager *w) {
	WNDCLASSEXW* wc = (WNDCLASSEXW*)w->platformData.ptr;
	UnregisterClassW(wc->lpszClassName, wc->hInstance);
	return true;
}

void WindowManager_updateExt() {

	MSG msg = (MSG) { 0 };
	Bool didPaint = false;

	while(PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {

		if (msg.message == WM_PAINT) {

			if (didPaint) {

				//Paint is dispatched if there's no more messages left,
				//so after this, we need to return to the main thread so we can process other windows
				//We do this by checking if the next message is also paint. If not, we continue

				MSG msgCheck = (MSG) { 0 };
				PeekMessageW(&msgCheck, NULL, 0, 0, PM_NOREMOVE);

				if (msgCheck.message == msg.message)
					break;
			}

			didPaint = true;
		}

		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}
