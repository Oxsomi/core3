#include "types/bit.h"
#include "platforms/window.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "types/timer.h"

#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

struct WWindow {
	bool running;
};

void WWindow_updateMonitors(struct Window *w) {

	//TODO:

	if(w->callbacks.updateMonitors)
		w->callbacks.updateMonitors(w);
}

LRESULT CALLBACK onCallback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

	struct Window *w = (struct Window*) GetWindowLongPtrA(hwnd, 0);
	struct WWindow *wext = (struct WWindow*) w->nativeData;

	if(!w || !wext)
		return DefWindowProc(hwnd, message, wParam, lParam);

	switch (message) {

		case WM_CREATE:
		case WM_DESTROY:
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

		//TODO:

		case WM_POWERBROADCAST: break;
		case WM_DISPLAYCHANGE:	break;

		//Input handling

		case WM_INPUT: {

			u32 size = 0;
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));

			struct Buffer buf = Bit_bytes(size, Platform_instance.alloc);

			oicAssert(
				"Couldn't read input", 
				GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buf.ptr, &size, sizeof(RAWINPUTHEADER)) == size
			);

			RAWINPUT *data = (RAWINPUT*) buf.ptr;

			RECT rect;
			GetClientRect(hwnd, &rect);

			if(data->header.hDevice)
				if (auto *ptr = (WWindow*)GetWindowLongPtrA(hwnd, 0)) {

					auto *vinterface = ptr->info->vinterface;

					InputDevice *&dvc = ptr->devices[data->header.hDevice];

					if (dvc->getType() == InputDevice::Type::KEYBOARD) {

						auto &keyboardDat = data->data.keyboard;

						usz id = WKey::idByValue(WKey::_E(keyboardDat.VKey));

						//TODO: Keyboard should initialize CAPS, SHIFT, ALT if they get changed or on start/switch

						//Only send recognized keys

						if (id != WKey::count) {

							String keyName = WKey::nameById(id);
							usz keyCode = Key::idByName(keyName);
							bool isKeyDown = !(keyboardDat.Flags & 1);

							bool pressed = dvc->getCurrentState(ButtonHandle(keyCode));
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

						for (usz i = 0; i < 5; ++i) {

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

							f64 delta = i16(mouseDat.usButtonData) / f64(WHEEL_DELTA);
							dvc->setAxis(MouseAxis::Axis_wheel_y, delta);

							if (vinterface)
								vinterface->onInputUpdate(ptr->info, dvc, InputHandle(MouseAxis::Axis_wheel_y) + MouseButton::count, delta != 0);
						}

						if (mouseDat.usButtonFlags & RI_MOUSE_HWHEEL) {

							f64 delta = i16(mouseDat.usButtonData) / f64(WHEEL_DELTA);
							dvc->setAxis(MouseAxis::Axis_wheel_x, delta);

							if (vinterface)
								vinterface->onInputUpdate(ptr->info, dvc, InputHandle(MouseAxis::Axis_wheel_x) + MouseButton::count, delta != 0);
						}

						if (mouseDat.usFlags & MOUSE_MOVE_ABSOLUTE) {

							f64 x = f64(mouseDat.lLastX) - rect.left, y = f64(mouseDat.lLastY) - rect.top;

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

			Bit_free(&buf, Platform_instance.alloc);
			return DefRawInputProc(&data, 1, sizeof(*data));
		}

		case WM_MOUSEMOVE:

			for (auto device : ptr->devices) {

				auto *dvc = device.second;

				if (dvc->getType() != InputDevice::Type::MOUSE)
					continue;

				if (ptr->info->hasHint(ViewportInfo::CAPTURE_CURSOR))
					return false;

				int x = GET_X_LPARAM(lParam); 
				int y = GET_Y_LPARAM(lParam);

				dvc->setAxis(MouseAxis::Axis_x, f64(x));
				dvc->setAxis(MouseAxis::Axis_y, f64(y));

				if (auto *vinterface = ptr->info->vinterface) {
					vinterface->onInputUpdate(ptr->info, dvc, InputHandle(MouseAxis::Axis_x) + MouseButton::count, false);
					vinterface->onInputUpdate(ptr->info, dvc, InputHandle(MouseAxis::Axis_y) + MouseButton::count, false);
				}
			}

			break;

		case WM_INPUT_DEVICE_CHANGE: {

			RID_DEVICE_INFO deviceInfo;
			u32 size = sizeof(deviceInfo);

			oicAssert(
				"Couldn't get raw input device", 
				GetRawInputDeviceInfoA((HANDLE)lParam, RIDI_DEVICEINFO, &deviceInfo, &size)
			);

			bool isAdded = wParam == GIDC_ARRIVAL;
			InputDevice *&dvc = ptr->devices[(HANDLE)lParam];

			oicAssert("Device change was already notified", bool(dvc) != isAdded);

			if (isAdded) {

				bool isKeyboard = deviceInfo.dwType == RIM_TYPEKEYBOARD;

				if (isKeyboard)
					info->devices.push_back(dvc = new Keyboard());
				else
					info->devices.push_back(dvc = new Mouse());

				if (info->vinterface)
					info->vinterface->onDeviceConnect(info, dvc);

				RAWINPUTDEVICE device {
					0x01,
					u16(isKeyboard ? 0x06 : 0x02),
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

			ns now = Timer_now();

			if (w->callbacks.update) {
				f64 dt = w->lastUpdate ? (now - w->lastUpdate) / (f64)seconds : 0;
				w->callbacks.update(w, dt);
			}

			w->lastUpdate = now;

			//Update input

			for (auto dvc : info->devices) {

				for (ButtonHandle i = 0, j = ButtonHandle(dvc->getButtonCount()); i < j; ++i)
					if (dvc->getState(i) == 0x2 /* released */)
						dvc->setPreviousState(i, false);
					else if (dvc->getState(i) == 0x1 /* pressed */)
						dvc->setPreviousState(i, true);

				for (AxisHandle i = 0, j = dvc->getAxisCount(); i < j; ++i)
					dvc->setPreviousAxis(i, dvc->getCurrentAxis(i));
			}

			//Render (if not minimized)

			if(w->callbacks.draw && !(w->flags & WindowFlags_IsMinimized))
				w->callbacks.draw(w);

			return NULL;
		}

		case WM_GETMINMAXINFO: {
			MINMAXINFO *lpMMI = (MINMAXINFO*) lParam;
			lpMMI->ptMinTrackSize.x = lpMMI->ptMinTrackSize.y = 256;
			break;
		}

		case WM_CLOSE:
			wext->running = false;
			break;

		case WM_SIZE: {

			RECT r;
			GetClientRect(hwnd, &r);
			f32x4 newSize = Vec_create2((f32)(r.right - r.left), (f32)(r.bottom - r.top));

			if (wParam == SIZE_MINIMIZED) 
				w->flags |= WindowFlags_IsMinimized;

			else w->flags &= ~WindowFlags_IsMinimized;

			if (!Vec_x(newSize) || !Vec_y(newSize) || Vec_all(Vec_eq(w->size, newSize)))
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
		
			w->offset = Vec_create2((f32)r.left, (f32)r.top);

			WWindow_updateMonitors(w);

			if (w->callbacks.move)
				w->callbacks.move(w);

			break;
		}
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

struct Window *Window_createPhysical(
	f32x4 position, f32x4 size,
	enum WindowHint hint, const c8 *title, struct WindowCallbacks callbacks,
	enum WindowFormat format
) {
	WNDCLASSEXA wc = (WNDCLASSEXA){ 0 };
	HINSTANCE mainModule = Platform_instance.data;

	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = onCallback;
	wc.hInstance = mainModule;

	wc.hIcon = (HICON) LoadImageA(mainModule, "LOGO", IMAGE_ICON, 32, 32, 0);
	wc.hIconSm = (HICON) LoadImageA(mainModule, "LOGO", IMAGE_ICON, 16, 16, 0);

	wc.hCursor = LoadCursorA(NULL, IDC_ARROW);

	wc.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);

	wc.lpszClassName = title;
	wc.cbSize = sizeof(wc);
	wc.cbWndExtra = sizeof(void*);

	if (!RegisterClassExA(&wc))
		Log_fatal("Couldn't create window class", LogOptions_Default);

	DWORD style = WS_VISIBLE;

	if(!(hint & WindowHint_ForceFullScreen)) {

		style |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

		if(!(hint & WindowHint_DisableResize))
			style |= WS_SIZEBOX | WS_MAXIMIZEBOX;
	}

	if(!(hint & WindowHint_HandleInput))
		style |= WS_DISABLED;

	u32 maxSize[2] = { 
		(u32) GetSystemMetrics(SM_CXSCREEN), 
		(u32) GetSystemMetrics(SM_CYSCREEN) 
	};

	for (usz i = 0; i < 2; ++i)
		if (!info->size[i] || info->size[i] >= maxSize[i])
			info->size[i] = maxSize[i];

	HWND hwnd = CreateWindowExA(
		WS_EX_APPWINDOW, wc.lpszClassName, wc.lpszClassName, style,
		(int) Vec_x(position), (int) Vec_y(position),
		(int) Vec_x(size), (int) Vec_y(size),
		NULL, NULL, mainModule, NULL
	);

	if(!hwnd)
		Log_fatal("Couldn't create window", LogOptions_Default);

	//Get real size and position

	RECT r = (RECT){ 0 };
	GetClientRect(hwnd, &r);
	size = Vec_create2((f32)(r.right - r.left), (f32)(r.bottom - r.top));

	GetWindowRect(hwnd, &r);
	position = Vec_create2((f32) r.left, (f32) r.top);

	//Find overlapping monitors

	//TODO:

	usz monitorCount = 0;
	struct Monitor *monitors = NULL;

	//Create our real window

	AllocFunc alloc = Platform_instance.alloc.alloc;
	void* allocator = Platform_instance.alloc.ptr;

	struct Window *wind = alloc(allocator, sizeof(struct Window));
	struct WWindow *wwind = alloc(allocator, sizeof(struct WWindow));

	*wind = (struct Window) {

		.offset = position,
		.size = size,

		.monitorCount = monitorCount,				//Monitors that the window overlaps with
		.monitors =  monitors,

		.nativeHandle = hwnd,
		.nativeData = wwind,

		.callbacks = callbacks,
		.lastUpdate = 0,

		.hint = hint,
		.format = ???,
		.flags = hint & WindowHint_ForceFullScreen ? WindowFlags_IsFullScreen : WindowFlags_None
	};

	*wwind = (struct WWindow) {
		.running = false
	};

	//

	SetWindowLongPtrA(hwnd, 0, (LONG_PTR) wind);
	UpdateWindow(hwnd);
}

void Window_freePhysical(struct Window **w) {

	if(!w || !free)
		Log_fatal("Couldn't free physical window, no free func or invalid window ptr", LogOptions_Default);

	if(!*w || !(*w)->nativeData)
		return;

	if(((struct WWindow*)(*w)->nativeData)->running)
		PostMessageA((*w)->nativeHandle, WM_DESTROY, NULL, NULL);

	FreeFunc free = Platform_instance.alloc.free;
	void* allocator = Platform_instance.alloc.ptr;

	free(allocator, (struct Buffer) { .ptr = (*w)->nativeData, .siz = sizeof(struct WWindow) });
	free(allocator, (struct Buffer) { .ptr = *w, .siz = sizeof(struct Window) });
	*w = NULL;
}

void Window_updatePhysicalTitle(const struct Window *w, const c8 *title) {

	if(!w)
		Log_fatal("Couldn't update physical title; invalid window", LogOptions_Default);

	if(!SetWindowTextA(w->nativeHandle, title))
		Log_fatal("Couldn't update physical title. Window might not have one", LogOptions_Default);
}

void Window_presentPhysical(const struct Window *w, struct Buffer data, enum WindowFormat encodedFormat);