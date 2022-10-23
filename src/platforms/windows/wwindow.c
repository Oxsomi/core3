#include "types/buffer.h"
#include "platforms/window.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "platforms/input_device.h"
#include "types/timer.h"

#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

//We probably won't need more windows for any other reason. Otherwise just use window in a window
//
const U8 WindowManager_maxTotalPhysicalWindowCount = 16;

void WWindow_updateMonitors(struct Window *w) {

	//TODO: Query monitors
	//EnumDisplayMonitors()

	if(w->callbacks.updateMonitors)
		w->callbacks.updateMonitors(w);
}

LRESULT CALLBACK onCallback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

	struct Window *w = (struct Window*) GetWindowLongPtrA(hwnd, 0);

	if(!w)
		return DefWindowProc(hwnd, message, wParam, lParam);

	switch (message) {

		case WM_DESTROY: {

			//TODO: Properly destroy the window

			w->flags &= ~WindowFlags_IsActive;
			break;
		}

		case WM_CLOSE:
		case WM_CREATE:
			break;

		//Setting focus

		case WM_SETFOCUS:
		case WM_KILLFOCUS:

			if(message == WM_SETFOCUS)
				w->flags |= WindowFlags_IsFocussed;

			else w->flags &= ~WindowFlags_IsFocussed;

			if(w->callbacks.updateFocus)
				w->callbacks.updateFocus(w);

			break;

		case WM_DISPLAYCHANGE:	
			WWindow_updateMonitors(w);
			break;

		//Input handling

		case WM_INPUT: {

			//Grab raw input

			U32 size = 0;
			GetRawInputData(
				(HRAWINPUT)lParam, RID_INPUT, NULL, 
				&size, sizeof(RAWINPUTHEADER)
			);

			struct Buffer buf = Buffer_createNull(); 
			struct Error err = Buffer_createUninitializedBytes(size, Platform_instance.alloc, &buf);

			if(err.genericError) {
				Log_fatal(String_createRefUnsafeConst("Couldn't allocate input data bytes"), LogOptions_Default);
				break;
			}

			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buf.ptr, &size, sizeof(RAWINPUTHEADER)) != size) {
				Log_fatal(String_createRefUnsafeConst("Couldn't get raw input"), LogOptions_Default);
				break;
			}

			RAWINPUT *data = (RAWINPUT*) buf.ptr;

			RECT rect;
			GetClientRect(hwnd, &rect);

			if(data->header.hDevice)
				if (auto *ptr = (WWindow*)GetWindowLongPtrA(hwnd, 0)) {

					auto *vinterface = ptr->info->vinterface;

					InputDevice *&dvc = ptr->devices[data->header.hDevice];

					if (dvc->getType() == InputDevice::Type::KEYBOARD) {

						auto &keyboardDat = data->data.keyboard;

						U64 id = WKey::idByValue(WKey::_E(keyboardDat.VKey));

						//TODO: Keyboard should initialize CAPS, SHIFT, ALT if they get changed or on start/switch

						//Only send recognized keys

						if (id != WKey::count) {

							String keyName = WKey::nameById(id);
							U64 keyCode = Key::idByName(keyName);
							Bool isKeyDown = !(keyboardDat.Flags & 1);

							Bool pressed = dvc->getCurrentState(ButtonHandle(keyCode));
							dvc->setState(ButtonHandle(keyCode), isKeyDown);

							if(pressed != isKeyDown)
								if (vinterface) {

									if (isKeyDown)
										vinterface->onInputActivate(ptr->info, dvc, InputHandle(keyCode));
									else 
										vinterface->onInputDeactivate(ptr->info, dvc, InputHandle(keyCode));

									vinterface->onInputUpdate(ptr->info, dvc, InputHandle(keyCode), isKeyDown);
								};
						}

						return 0;

					} else {

						auto &mouseDat = data->data.mouse;

						for (U64 i = 0; i < 5; ++i) {

							if (mouseDat.usButtonFlags & (1 << (i << 1))) {

								dvc->setPreviousState(ButtonHandle(i), dvc->getPreviousState(ButtonHandle(i)));
								dvc->setState(ButtonHandle(i), true);

								if (vinterface) {
									vinterface->onInputActivate(ptr->info, dvc, InputDevice::Handle(i));
									vinterface->onInputUpdate(ptr->info, dvc, InputHandle(i), true);
								}
							}

							else if (mouseDat.usButtonFlags & (2 << (i << 1))) {

								dvc->setPreviousState(ButtonHandle(i), true);
								dvc->setState(ButtonHandle(i), false);

								if (vinterface) {
									vinterface->onInputDeactivate(ptr->info, dvc, InputDevice::Handle(i));
									vinterface->onInputUpdate(ptr->info, dvc, InputHandle(i), false);
								}
							}

						}

						if (mouseDat.usButtonFlags & RI_MOUSE_WHEEL) {

							F32 delta = I16(mouseDat.usButtonData) / F32(WHEEL_DELTA);
							dvc->setAxis(MouseAxis::Axis_wheel_y, delta);

							if (vinterface)
								vinterface->onInputUpdate(ptr->info, dvc, InputHandle(MouseAxis::Axis_wheel_y) + MouseButton::count, delta != 0);
						}

						if (mouseDat.usButtonFlags & RI_MOUSE_HWHEEL) {

							F32 delta = I16(mouseDat.usButtonData) / F32(WHEEL_DELTA);
							dvc->setAxis(MouseAxis::Axis_wheel_x, delta);

							if (vinterface)
								vinterface->onInputUpdate(ptr->info, dvc, InputHandle(MouseAxis::Axis_wheel_x) + MouseButton::count, delta != 0);
						}

						if (mouseDat.usFlags & MOUSE_MOVE_ABSOLUTE) {

							F32 x = F32(mouseDat.lLastX) - rect.left, y = F64(mouseDat.lLastY) - rect.top;

							dvc->setAxis(MouseAxis::Axis_delta_x, dvc->getCurrentAxis(MouseAxis::Axis_x) - x);
							dvc->setAxis(MouseAxis::Axis_delta_y, dvc->getCurrentAxis(MouseAxis::Axis_y) - y);
							dvc->setAxis(MouseAxis::Axis_x, x);
							dvc->setAxis(MouseAxis::Axis_y, y);

							vinterface->onInputUpdate(ptr->info, dvc, InputHandle(MouseAxis::Axis_delta_x) + MouseButton::count, mouseDat.lLastX != 0);
							vinterface->onInputUpdate(ptr->info, dvc, InputHandle(MouseAxis::Axis_delta_y) + MouseButton::count, mouseDat.lLastY != 0);
							vinterface->onInputUpdate(ptr->info, dvc, InputHandle(MouseAxis::Axis_x) + MouseButton::count, false);
							vinterface->onInputUpdate(ptr->info, dvc, InputHandle(MouseAxis::Axis_y) + MouseButton::count, false);

						} else {

							dvc->setAxis(MouseAxis::Axis_delta_x, mouseDat.lLastX);
							dvc->setAxis(MouseAxis::Axis_delta_y, mouseDat.lLastY);

							if (vinterface) {
								vinterface->onInputUpdate(ptr->info, dvc, InputHandle(MouseAxis::Axis_delta_x) + MouseButton::count, mouseDat.lLastX != 0);
								vinterface->onInputUpdate(ptr->info, dvc, InputHandle(MouseAxis::Axis_delta_y) + MouseButton::count, mouseDat.lLastY != 0);
							}
						}
					}

				}

			LRESULT lr = DefRawInputProc(&data, 1, sizeof(*data));
			Buffer_free(&buf, Platform_instance.alloc);
			return lr;
		}

		/* TODO: Handle capture cursor

		case WM_MOUSEMOVE:

			for (auto device : ptr->devices) {

				auto *dvc = device.second;

				if (dvc->getType() != InputDevice::Type::MOUSE)
					continue;

				if (ptr->info->hasHint(ViewportInfo::CAPTURE_CURSOR))
					return false;

				int x = GET_X_LPARAM(lParam); 
				int y = GET_Y_LPARAM(lParam);

				dvc->setAxis(MouseAxis::Axis_x, F32(x));
				dvc->setAxis(MouseAxis::Axis_y, F32(y));

				if (auto *vinterface = ptr->info->vinterface) {
					vinterface->onInputUpdate(ptr->info, dvc, InputHandle(MouseAxis::Axis_x) + MouseButton::count, false);
					vinterface->onInputUpdate(ptr->info, dvc, InputHandle(MouseAxis::Axis_y) + MouseButton::count, false);
				}
			}

			break;
		*/

		case WM_INPUT_DEVICE_CHANGE: {

			RID_DEVICE_INFO deviceInfo;
			U32 size = sizeof(deviceInfo);

			oicAssert(
				"Couldn't get raw input device", 
				GetRawInputDeviceInfoA((HANDLE)lParam, RIDI_DEVICEINFO, &deviceInfo, &size)
			);

			Bool isAdded = wParam == GIDC_ARRIVAL;
			InputDevice *&dvc = ptr->devices[(HANDLE)lParam];

			oicAssert("Device change was already notified", Bool(dvc) != isAdded);

			if (isAdded) {

				Bool isKeyboard = deviceInfo.dwType == RIM_TYPEKEYBOARD;

				if (isKeyboard)
					info->devices.push_back(dvc = new Keyboard());
				else
					info->devices.push_back(dvc = new Mouse());

				if (info->vinterface)
					info->vinterface->onDeviceConnect(info, dvc);

				RAWINPUTDEVICE device {
					0x01,
					U16(isKeyboard ? 0x06 : 0x02),
					0x0,
					hwnd
				};

				if (!isKeyboard) {
					POINT point;
					GetCursorPos(&point);
					dvc->setAxis(MouseAxis::Axis_x, point.x);
					dvc->setAxis(MouseAxis::Axis_y, point.y);
				}

				oicAssert(
					"Couldn't create raw input device", 
					RegisterRawInputDevices(&device, 1, sizeof(RAWINPUTDEVICE))
				);

			} else {

				if (info->vinterface)
					info->vinterface->onDeviceRemoval(info, dvc);

				info->devices.erase(std::find(ptr->info->devices.begin(), ptr->info->devices.end(), dvc));
				ptr->devices.erase((HANDLE)lParam);
				delete dvc;
			}

			return 0;
		}

		//Render

		case WM_PAINT: {

			if(!(w->hint & WindowHint_AllowBackgroundUpdates) && (w->flags & WindowFlags_IsMinimized))
				return NULL;

			//Update interface

			Ns now = Timer_now();

			if (w->callbacks.update) {
				F32 dt = w->lastUpdate ? (now - w->lastUpdate) / (F32)seconds : 0;
				w->callbacks.update(w, dt);
			}

			w->lastUpdate = now;

			//Update input

			for(U64 i = 0; i < w->deviceCount; ++i)
				InputDevice_markUpdate(w->devices[i]);

			//Render (if not minimized)

			if(w->callbacks.draw && !(w->flags & WindowFlags_IsMinimized))
				w->callbacks.draw(w);

			return NULL;
		}

		case WM_GETMINMAXINFO: {

			//TODO: Minimum window size?
			//TODO: Maximum window size?

			MINMAXINFO *lpMMI = (MINMAXINFO*) lParam;
			lpMMI->ptMinTrackSize.x = lpMMI->ptMinTrackSize.y = 256;
			break;
		}

		case WM_SIZE: {

			RECT r;
			GetClientRect(hwnd, &r);
			I32x2 newSize = I32x2_create2(r.right - r.left, r.bottom - r.top);

			if (wParam == SIZE_MINIMIZED) 
				w->flags |= WindowFlags_IsMinimized;

			else w->flags &= ~WindowFlags_IsMinimized;

			if (!I32x2_any(I32x2_leq(newSize, I32x2_zero())) || I32x2_eq2(w->size, newSize))
				break;

			w->size = newSize;
			
			WWindow_updateMonitors(w);

			if (w->callbacks.resize)
				w->callbacks.resize(w);

			break;
		}

		case WM_MOVE: {
		
			RECT r;
			GetWindowRect(hwnd, &r);
		
			w->offset = I32x2_create2(r.left, r.top);

			WWindow_updateMonitors(w);

			if (w->callbacks.move)
				w->callbacks.move(w);

			break;
		}
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

Bool WindowManager_supportsFormat(struct WindowManager manager, enum WindowFormat format) {

	//TODO: HDR support; ColorSpace
	//	https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-getcontainingoutput
	//	https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_6/ns-dxgi1_6-dxgi_output_desc1

	return format == WindowFormat_rgba8;
}

struct Error WindowManager_createPhysical(
	struct WindowManager *manager,
	I32x2 position,
	I32x2 size, 
	enum WindowHint hint,
	struct String title, 
	struct WindowCallbacks callbacks,
	enum WindowFormat format,
	struct Window **w
) {

	//Validate state

	if(!manager)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if(!Lock_isLockedForThread(manager->lock))
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	if(!w)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = 7 };

	if(*w)
		return (struct Error) { .genericError = GenericError_InvalidParameter, .paramId = 7 };

	switch (format) {

		case WindowFormat_rgba8:
		case WindowFormat_hdr10a2:
		case WindowFormat_rgba16f:
		case WindowFormat_rgba32f:

			if(!WindowManager_supportsFormat(*manager, format))
				return (struct Error) { 
					.genericError = GenericError_UnsupportedOperation, 
					.paramId = 6,
					.paramValue0 = format
				};
			
			break;

		default:
			return (struct Error) { .genericError = GenericError_InvalidParameter, .paramId = 6 };
	}

	if(I32x2_any(I32x2_lt(size, I32x2_zero())))
		return (struct Error) { .genericError = GenericError_InvalidParameter };

	if (title.len >= MAX_PATH)
		return (struct Error) { 
			.genericError = GenericError_OutOfBounds, 
			.paramId = 4, 
			.paramValue0 = title.len,
			.paramValue1 = MAX_PATH
		};
		
	//Find free spot in physical windows

	struct Window *win = NULL;
	WindowHandle handle = 0;

	for(U8 i = 0; i < WindowManager_maxTotalPhysicalWindowCount; ++i) {

		struct Window *w = manager->windowArray + i;

		if (!w->nativeHandle) {
			win = w;
			handle = i;
			break;
		}
	}

	if(!win)
		return (struct Error) { .genericError = GenericError_OutOfMemory };
		
	//Create native window

	WNDCLASSEXA wc = (WNDCLASSEXA){ 0 };
	HINSTANCE mainModule = Platform_instance.data;

	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = onCallback;
	wc.hInstance = mainModule;

	wc.hIcon = (HICON) LoadImageA(mainModule, "LOGO", IMAGE_ICON, 32, 32, 0);
	wc.hIconSm = (HICON) LoadImageA(mainModule, "LOGO", IMAGE_ICON, 16, 16, 0);

	wc.hCursor = LoadCursorA(NULL, IDC_ARROW);

	wc.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);

	wc.lpszClassName = "Oxsomi core v3";
	wc.cbSize = sizeof(wc);
	wc.cbWndExtra = sizeof(void*);

	if (!RegisterClassExA(&wc))
		return (struct Error) { .genericError = GenericError_InvalidOperation, .paramSubId = 1 };

	DWORD style = WS_VISIBLE;

	if(!(hint & WindowHint_ForceFullscreen)) {

		style |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

		if(!(hint & WindowHint_DisableResize))
			style |= WS_SIZEBOX | WS_MAXIMIZEBOX;
	}

	if(!(hint & WindowHint_HandleInput))
		style |= WS_DISABLED;

	I32x2 maxSize = I32x2_create2(
		GetSystemMetrics(SM_CXSCREEN), 
		GetSystemMetrics(SM_CYSCREEN) 
	);

	for (U64 i = 0; i < 2; ++i)
		if (!I32x2_get(size, i) || I32x2_get(size, i) >= I32x2_get(maxSize, i))
			I32x2_set(&size, i, I32x2_get(maxSize, i));

	//Our strings aren't null terminated, so ensure windows doesn't read garbage

	C8 windowName[MAX_PATH + 1];
	Buffer_copy(Buffer_createRef(windowName, sizeof(windowName)), Buffer_createRef(title.ptr, title.len));

	windowName[title.len] = '\0';

	HWND nativeWindow = CreateWindowExA(
		WS_EX_APPWINDOW, wc.lpszClassName, windowName, style,
		I32x2_x(position), I32x2_y(position),
		I32x2_x(size), I32x2_y(size),
		NULL, NULL, mainModule, NULL
	);

	if(!nativeWindow)
		return (struct Error) { .genericError = GenericError_InvalidOperation, .paramSubId = 2 };
		
	//Get real size and position

	RECT r = (RECT){ 0 };
	GetClientRect(nativeWindow, &r);
	size = I32x2_create2(r.right - r.left, r.bottom - r.top);

	GetWindowRect(nativeWindow, &r);
	position = I32x2_create2(r.left, r.top);

	//Bind our window

	SetWindowLongPtrA(nativeWindow, 0, (LONG_PTR) win);

	//Alloc cpu visible buffer if needed

	struct Buffer cpuVisibleBuffer = Buffer_createNull();

	struct Error err = Error_none();

	if(hint & WindowHint_ProvideCPUBuffer) {

		err = Buffer_createEmptyBytes(
			TextureFormat_getSize((enum TextureFormat) format, (U64) I32x2_x(size), (U64) I32x2_y(size)),
			Platform_instance.alloc,
			&cpuVisibleBuffer
		);

		if(err.genericError)
			return err;
	}

	//Lock for when we are updating this window

	struct Lock lock = (struct Lock) { 0 };

	if ((err = Lock_create(&lock)).genericError) {

		if(cpuVisibleBuffer.ptr)
			Buffer_free(&cpuVisibleBuffer, Platform_instance.alloc);

		return err;
	}

	//Fill window object

	*win = (struct Window) {

		.offset = position,
		.size = size,
	
		.cpuVisibleBuffer = cpuVisibleBuffer,

		.nativeHandle = nativeWindow,
		.lock = lock,

		.callbacks = callbacks,

		.handle = handle,
		.hint = hint,
		.format = format,
		.flags = WindowFlags_IsActive
	};

	*w = win;

	if(callbacks.start)
		callbacks.start(w);

	//Find overlapping monitors

	WWindow_updateMonitors(win);

	//Our window is now ready

	UpdateWindow(nativeWindow);

	return Error_none();
}

struct Error WindowManager_freePhysical(struct WindowManager *manager, WindowHandle handle) {

	if(!manager)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if(!Lock_isLockedForThread(manager->lock))
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	if(handle >= WindowManager_maxTotalPhysicalWindowCount)
		return (struct Error) { .genericError = GenericError_OutOfBounds };

	struct Window *w = manager->windowArray + handle;

	if(!(w->flags & WindowFlags_IsActive))
		return (struct Error) { .genericError = GenericError_InvalidOperation, .paramId = 1 };

	//Ensure our window safely exits

	PostMessageA(w->nativeHandle, WM_DESTROY, NULL, NULL);
	return Error_none();
}

struct Error Window_updatePhysicalTitle(
	const struct Window *w,
	struct String title
) {

	if(!w || !title.ptr || !title.len)
		return (struct Error) { .genericError = GenericError_NullPointer, .paramId = !!w };

	if (title.len >= MAX_PATH)
		return (struct Error) { 
			.genericError = GenericError_OutOfBounds, 
			.paramId = 1, 
			.paramValue0 = title.len,
			.paramValue1 = MAX_PATH
		};

	C8 windowName[MAX_PATH + 1];
	Buffer_copy(Buffer_createRef(windowName, sizeof(windowName)), Buffer_createRef(title.ptr, title.len));

	windowName[title.len] = '\0';

	if(!SetWindowTextA(w->nativeHandle, windowName))
		return (struct Error) { .genericError = GenericError_InvalidOperation };

	return Error_none();
}

struct Error Window_presentPhysical(
	const struct Window *w, 
	struct Buffer data, 
	enum WindowFormat encodedFormat
);