#include "platforms/window_manager.h"
#include "platforms/platform.h"
#include "platforms/input_device.h"
#include "platforms/ext/listx.h"
#include "types/string.h"

#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

const U8 WindowManager_maxTotalPhysicalWindowCount = 16;

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
		return (Error) { .genericError = EGenericError_NullPointer };

	if(!Lock_isLockedForThread(manager->lock))
		return (Error) { .genericError = EGenericError_InvalidOperation };

	if(!w)
		return (Error) { .genericError = EGenericError_NullPointer, .paramId = 7 };

	if(*w)
		return (Error) { .genericError = EGenericError_InvalidParameter, .paramId = 7 };

	switch (format) {

		case EWindowFormat_rgba8:
		case EWindowFormat_hdr10a2:
		case EWindowFormat_rgba16f:
		case EWindowFormat_rgba32f:

			if(!WindowManager_supportsFormat(*manager, format))
				return (Error) { 
					.genericError = EGenericError_UnsupportedOperation, 
					.paramId = 6,
					.paramValue0 = format
				};

			break;

		default:
			return (Error) { .genericError = EGenericError_InvalidParameter, .paramId = 6 };
	}

	if(I32x2_any(I32x2_lt(size, I32x2_zero())))
		return (Error) { .genericError = EGenericError_InvalidParameter };

	if (title.len >= MAX_PATH)
		return (Error) { 
			.genericError = EGenericError_OutOfBounds, 
			.paramId = 4, 
			.paramValue0 = title.len,
			.paramValue1 = MAX_PATH
		};

	//Find free spot in physical windows

	Window *win = NULL;
	WindowHandle handle = 0;

	for(U8 i = 0; i < WindowManager_maxTotalPhysicalWindowCount; ++i) {

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
		return (Error) { .genericError = EGenericError_OutOfMemory };

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
		return (Error) { .genericError = EGenericError_InvalidOperation, .paramSubId = 1 };

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
	Buffer_copy(Buffer_createRef(windowName, sizeof(windowName)), String_buffer(title));

	windowName[title.len] = '\0';

	HWND nativeWindow = CreateWindowExA(
		WS_EX_APPWINDOW, wc.lpszClassName, windowName, style,
		I32x2_x(position), I32x2_y(position),
		I32x2_x(size), I32x2_y(size),
		NULL, NULL, mainModule, NULL
	);

	if(!nativeWindow) {
		UnregisterClassA(wc.lpszClassName, wc.hInstance);
		return (Error) { .genericError = EGenericError_InvalidOperation, .paramSubId = 2 };
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

	Error err = Error_none();

	if(hint & EWindowHint_ProvideCPUBuffer) {

		HDC screen = GetDC(nativeWindow);

		if(!screen) {
			DestroyWindow(nativeWindow);
			UnregisterClassA(wc.lpszClassName, wc.hInstance);
			return (Error) { .genericError = EGenericError_InvalidOperation, .paramSubId = 3 };
		}

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

		if(!screen) {
			ReleaseDC(nativeWindow, screen);
			DestroyWindow(nativeWindow);
			UnregisterClassA(wc.lpszClassName, wc.hInstance);
			return (Error) { .genericError = EGenericError_InvalidOperation, .paramSubId = 3 };
		}

		cpuVisibleBuffer.siz = (U64) bmi.bmiHeader.biWidth * bmi.bmiHeader.biHeight * 4;
		ReleaseDC(nativeWindow, screen);
	}

	//Lock for when we are updating this window

	Lock lock = (Lock) { 0 };

	if ((err = Lock_create(&lock)).genericError) {

		if(nativeData)
			DeleteObject((HGDIOBJ) nativeData);

		UnregisterClassA(wc.lpszClassName, wc.hInstance);
		DestroyWindow(nativeWindow);
		return err;
	}

	List devices = List_createEmpty(sizeof(InputDevice)), monitors = List_createEmpty(sizeof(Monitor));

	if ((err = List_reservex(&devices, Window_maxDevices)).genericError) {

		Lock_free(&lock);

		if(nativeData)
			DeleteObject((HGDIOBJ) nativeData);

		UnregisterClassA(wc.lpszClassName, wc.hInstance);
		DestroyWindow(nativeWindow);

		return err;
	}

	if ((err = List_reservex(&monitors, Window_maxMonitors)).genericError) {

		List_freex(&devices);
		Lock_free(&lock);

		if(nativeData)
			DeleteObject((HGDIOBJ) nativeData);

		UnregisterClassA(wc.lpszClassName, wc.hInstance);
		DestroyWindow(nativeWindow);

		return err;
	}

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

	*w = win;

	//Signal ready

	if(callbacks.onCreate)
		callbacks.onCreate(win);

	//Our window is now ready, start window loop

	UpdateWindow(nativeWindow);
	return Error_none();
}
