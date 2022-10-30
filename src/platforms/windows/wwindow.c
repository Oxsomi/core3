#include "types/buffer.h"
#include "platforms/window.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "platforms/input_device.h"
#include "platforms/keyboard.h"
#include "platforms/mouse.h"
#include "platforms/errorx.h"
#include "types/timer.h"

#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

const U16 Window_maxDevices = 64;
const U16 Window_maxMonitors = 64;

void WWindow_updateMonitors(Window *w) {

	//TODO: Query monitors
	//EnumDisplayMonitors()

	if(w->callbacks.onMonitorChange)
		w->callbacks.onMonitorChange(w);
}

LRESULT CALLBACK WWindow_onCallback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

	Window *w = (Window*) GetWindowLongPtrA(hwnd, 0);

	//TODO: Lock + Unlock window

	if(!w)
		return DefWindowProc(hwnd, message, wParam, lParam);

	switch (message) {

		case WM_DESTROY: {

			if(w->callbacks.onDestroy)
				w->callbacks.onDestroy(w);

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

			else {

				//TODO: Reset keys because otherwise they might hang

				w->flags &= ~WindowFlags_IsFocussed;
			}

			if(w->callbacks.onUpdateFocus)
				w->callbacks.onUpdateFocus(w);

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

			Buffer buf = Buffer_createNull(); 
			Error err = Buffer_createUninitializedBytes(size, Platform_instance.alloc, &buf);

			if(err.genericError) {
				Error_printx(err, LogLevel_Error, LogOptions_Default);
				break;
			}

			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buf.ptr, &size, sizeof(RAWINPUTHEADER)) != size) {
				Log_error(String_createRefUnsafeConst("Couldn't get raw input"), LogOptions_Default);
				break;
			}

			//Grab device from the list

			RAWINPUT *data = (RAWINPUT*) buf.ptr;

			InputDevice *beg = (InputDevice*) List_begin(w->devices);
			InputDevice *end = (InputDevice*) List_end(w->devices);

			InputDevice *dev = beg;

			for(; dev != end; ++dev)
				if(*(HANDLE*) dev->dataExt.ptr == (HANDLE) lParam)
					break;

			if(!data->header.hDevice || dev == end)
				goto cleanup;

			if (dev->type == InputDeviceType_Keyboard) {

				RAWKEYBOARD keyboardDat = data->data.keyboard;

				Bool isKeyDown = !(keyboardDat.Flags & 1);

				//Ensure the key state is up to date
				//It's a shame we have to get the state, but we can't rely on our program to know the exact state
				//Because locks can be toggled from a different program

				InputDevice_setFlagTo(dev, KeyboardFlags_Caps, GetKeyState(VK_CAPITAL) & 1);
				InputDevice_setFlagTo(dev, KeyboardFlags_NumLock, GetKeyState(VK_NUMLOCK) & 1);
				InputDevice_setFlagTo(dev, KeyboardFlags_ScrollLock, GetKeyState(VK_SCROLL) & 1);

				//Translate key to our system and set corresponding device flags

				InputHandle handle = InputDevice_invalidHandle();

				switch (keyboardDat.VKey) {

					case VK_LSHIFT:
					case VK_RSHIFT:
					case VK_SHIFT:
						InputDevice_setFlagTo(dev, KeyboardFlags_Shift, isKeyDown);
						handle = Key_Shift;
						break;

					case VK_LMENU:
					case VK_RMENU:
					case VK_MENU:
						InputDevice_setFlagTo(dev, KeyboardFlags_Alt, isKeyDown);
						handle = Key_Alt;
						break;

					case VK_LCONTROL:
					case VK_RCONTROL:
					case VK_CONTROL:
						InputDevice_setFlagTo(dev, KeyboardFlags_Control, isKeyDown);
						handle = Key_Ctrl;
						break;

					case VK_NUMLOCK:
						InputDevice_setFlagTo(dev, KeyboardFlags_NumLock, isKeyDown);
						handle = Key_NumLock;
						break;

					case VK_SCROLL:
						InputDevice_setFlagTo(dev, KeyboardFlags_ScrollLock, isKeyDown);
						handle = Key_ScrollLock;
						break;

					case VK_BACK:				handle = Key_Backspace;		break;
					case VK_SPACE:				handle = Key_Space;			break;
					case VK_TAB:				handle = Key_Tab;			break;
					case VK_PAUSE:				handle = Key_Pause;			break;
					case VK_CAPITAL:			handle = Key_Caps;			break;
					case VK_ESCAPE:				handle = Key_Escape;		break;
					case VK_PRIOR:				handle = Key_PageUp;		break;
					case VK_NEXT:				handle = Key_PageDown;		break;
					case VK_END:				handle = Key_End;			break;
					case VK_HOME:				handle = Key_Home;			break;
					case VK_SELECT:				handle = Key_Select;		break;
					case VK_PRINT:				handle = Key_Print;			break;
					case VK_EXECUTE:			handle = Key_Execute;		break;
					case VK_SNAPSHOT:			handle = Key_PrintScreen;	break;
					case VK_INSERT:				handle = Key_Insert;		break;
					case VK_BROWSER_BACK:		handle = Key_Back;			break;
					case VK_BROWSER_FORWARD:	handle = Key_Forward;		break;
					case VK_SLEEP:				handle = Key_Sleep;			break;
					case VK_BROWSER_REFRESH:	handle = Key_Refresh;		break;
					case VK_BROWSER_STOP:		handle = Key_Stop;			break;
					case VK_BROWSER_SEARCH:		handle = Key_Search;		break;
					case VK_BROWSER_FAVORITES:	handle = Key_Favorites;		break;
					case VK_BROWSER_HOME:		handle = Key_Start;			break;
					case VK_VOLUME_MUTE:		handle = Key_Mute;			break;
					case VK_VOLUME_DOWN:		handle = Key_VolumeDown;	break;
					case VK_VOLUME_UP:			handle = Key_VolumeUp;		break;
					case VK_MEDIA_NEXT_TRACK:	handle = Key_Skip;			break;
					case VK_MEDIA_PREV_TRACK:	handle = Key_Previous;		break;
					case VK_CLEAR:				handle = Key_Clear;			break;
					case VK_ZOOM:				handle = Key_Zoom;			break;
					case VK_RETURN:				handle = Key_Enter;			break;
					case VK_DELETE:				handle = Key_Delete;		break;
					case VK_HELP:				handle = Key_Help;			break;
					case VK_APPS:				handle = Key_Apps;			break;

					case VK_LEFT:				handle = Key_Left;			break;
					case VK_UP:					handle = Key_Up;			break;
					case VK_RIGHT:				handle = Key_Right;			break;
					case VK_DOWN:				handle = Key_Down;			break;

					case VK_MULTIPLY:			handle = Key_NumpadMul;		break;
					case VK_ADD:				handle = Key_NumpadAdd;		break;
					case VK_DECIMAL:			handle = Key_NumpadDec;		break;
					case VK_DIVIDE:				handle = Key_NumpadDiv;		break;
					case VK_SUBTRACT:			handle = Key_NumpadSub;		break;

					case VK_OEM_PLUS:			handle = Key_Equals;		break;
					case VK_OEM_COMMA:			handle = Key_Comma;			break;
					case VK_OEM_MINUS:			handle = Key_Minus;			break;
					case VK_OEM_PERIOD:			handle = Key_Period;		break;
					case VK_OEM_1:				handle = Key_Semicolon;		break;
					case VK_OEM_2:				handle = Key_Slash;			break;
					case VK_OEM_3:				handle = Key_Acute;			break;
					case VK_OEM_4:				handle = Key_LBracket;		break;
					case VK_OEM_6:				handle = Key_RBracket;		break;
					case VK_OEM_5:				handle = Key_Backslash;		break;
					case VK_OEM_7:				handle = Key_Quote;			break;

					//Unknown key or common key

					default:

						if(keyboardDat.VKey >= '0' && keyboardDat.VKey <= '9')
							handle = Key_0 + (keyboardDat.VKey - '0');

						else if(keyboardDat.VKey >= 'A' && keyboardDat.VKey <= 'Z')
							handle = Key_A + (keyboardDat.VKey - 'A');

						else if(keyboardDat.VKey >= VK_F1 && keyboardDat.VKey <= VK_F24)
							handle = Key_F1 + (keyboardDat.VKey - VK_F1);

						else if(keyboardDat.VKey >= VK_NUMPAD0 && keyboardDat.VKey <= VK_NUMPAD9)
							handle = Key_Numpad0 + (keyboardDat.VKey - VK_NUMPAD0);

						else goto cleanup;

						break;
				}

				//Send keys through interface and update input device

				InputState prevState = InputDevice_getState(*dev, handle);

				InputDevice_setCurrentState(*dev, handle, isKeyDown);
				InputState newState = InputDevice_getState(*dev, handle);

				if(prevState != newState && w->callbacks.onDeviceButton)
					w->callbacks.onDeviceButton(w, dev, handle, isKeyDown);

				return 0;

			} else {

				RAWMOUSE mouseDat = data->data.mouse;

				for (U64 i = 0; i < 5; ++i) {

					bool isDown = mouseDat.usButtonFlags & (1 << (i << 1));
					bool isUp = mouseDat.usButtonFlags & (2 << (i << 1));

					if(!isDown && !isUp)
						continue;

					InputHandle handle = (InputHandle) (MouseButton_Left + i);
					InputDevice_setCurrentState(*dev, handle, isDown);

					if (w->callbacks.onDeviceButton)
						w->callbacks.onDeviceButton(w, dev, handle, isDown);
				}

				if (mouseDat.usButtonFlags & RI_MOUSE_WHEEL) {

					F32 delta = (F32)mouseDat.usButtonData / WHEEL_DELTA;
					InputDevice_setCurrentAxis(*dev, MouseAxis_ScrollWheel_X, delta);

					if (w->callbacks.onDeviceAxis)
						w->callbacks.onDeviceAxis(w, dev, MouseAxis_ScrollWheel_X, delta);
				}

				if (mouseDat.usButtonFlags & RI_MOUSE_HWHEEL) {

					F32 delta = (F32)mouseDat.usButtonData / WHEEL_DELTA;
					InputDevice_setCurrentAxis(*dev, MouseAxis_ScrollWheel_Y, delta);

					if (w->callbacks.onDeviceAxis)
						w->callbacks.onDeviceAxis(w, dev, MouseAxis_ScrollWheel_Y, delta);
				}

				F32 prevX = InputDevice_getCurrentAxis(*dev, MouseAxis_X);
				F32 prevY = InputDevice_getCurrentAxis(*dev, MouseAxis_Y);

				F32 nextX, nextY;

				if (mouseDat.usFlags & MOUSE_MOVE_ABSOLUTE) {

					RECT rect;
					GetClientRect(hwnd, &rect);

					InputDevice_resetFlag(dev, MouseFlag_IsRelative);

					nextX = (F32) (mouseDat.lLastX - rect.left);
					nextY = (F32) (mouseDat.lLastY - rect.top);

				} else {
					InputDevice_setFlag(dev, MouseFlag_IsRelative);
					nextX = prevX + (F32) mouseDat.lLastX;
					nextY = prevY + (F32) mouseDat.lLastY;
				}

				if (nextX != prevX) {

					InputDevice_setCurrentAxis(*dev, MouseAxis_X, nextX);

					if (w->callbacks.onDeviceAxis)
						w->callbacks.onDeviceAxis(w, dev, MouseAxis_X, nextX);
				}

				if (nextY != prevY) {

					InputDevice_setCurrentAxis(*dev, MouseAxis_Y, nextY);

					if (w->callbacks.onDeviceAxis)
						w->callbacks.onDeviceAxis(w, dev, MouseAxis_Y, nextY);
				}
			}

		cleanup:
			LRESULT lr = DefRawInputProc(&data, 1, sizeof(*data));
			Buffer_free(&buf, Platform_instance.alloc);
			return lr;
		}

		//TODO: Handle capture cursor

		case WM_INPUT_DEVICE_CHANGE: {

			RID_DEVICE_INFO deviceInfo = (RID_DEVICE_INFO) { 0 };
			U32 size = sizeof(deviceInfo);

			if (!GetRawInputDeviceInfoA((HANDLE)lParam, RIDI_DEVICEINFO, &deviceInfo, &size)) {
				Log_error(String_createRefUnsafeConst("Invalid data in WM_INPUT_DEVICE_CHANGE"), LogOptions_Default);
				break;
			}

			if(deviceInfo.dwType == RIM_TYPEHID)		//Irrelevant for us for now
				break;

			Error err;

			Bool isAdded = wParam == GIDC_ARRIVAL;

			if (isAdded) {

				Bool isKeyboard = deviceInfo.dwType == RIM_TYPEKEYBOARD;

				//Warn because a Japanese/Korean keyboard might not work correctly with the app
				//Because it might be relying on keys that don't exist there

				if(isKeyboard && deviceInfo.keyboard.dwKeyboardMode != 0x4)
					Log_warn(String_createRefUnsafeConst("Possibly unsupported type of keyboard!"), LogOptions_Default);

				//Create input device

				InputDevice device;

				if (isKeyboard) {

					if ((err = Keyboard_create((Keyboard*) &device)).genericError) {
						Error_printx(err, LogLevel_Error, LogOptions_Default);
						break;
					}
				}

				else if ((err = Mouse_create((Mouse*) &device)).genericError) {
					Error_printx(err, LogLevel_Error, LogOptions_Default);
					break;
				}

				if((err = Buffer_createUninitializedBytes(
					sizeof(HANDLE), Platform_instance.alloc, &device.dataExt
				)).genericError) {
					InputDevice_free(&device);
					Error_printx(err, LogLevel_Error, LogOptions_Default);
					break;
				}

				RAWINPUTDEVICE rawDevice = (RAWINPUTDEVICE) {
					0x01,								//Perhaps 0xD for touchscreen at some point
					(U16)(isKeyboard ? 0x06 : 0x02),	//0x4-0x05 for game controllers in the future
					0x0,
					hwnd
				};

				//Find free spot in device array
				//If this happens, we don't need to resize the array

				InputDevice *beg = (InputDevice*) List_begin(w->devices);
				InputDevice *end = (InputDevice*) List_end(w->devices);

				InputDevice *dev = beg;

				for(; dev != end; ++dev)
					if(dev->type == InputDeviceType_Undefined)
						break;

				//Our list isn't big enough, we need to resize

				bool pushed = false;

				if(dev == end) {

					//Try to allocate without allowing allocation
					//If it fails, we can't create a new device

					err = List_pushBack(&w->devices, Buffer_createNull(), (Allocator) { 0 });
					pushed = true;

					if(err.genericError) {

						Buffer_free(&device.dataExt, Platform_instance.alloc);
						InputDevice_free(&device);

						Log_error(String_createRefUnsafeConst("Couldn't register device; "), LogOptions_Default);
						break;
					}

					//After this, our dev and end pointers are valid again
					//Because our list didn't reallocate, it just changed size
				}

				//Register device

				if (!RegisterRawInputDevices(&rawDevice, 1, sizeof(RAWINPUTDEVICE))) {

					if(pushed)
						List_popBack(&w->devices, Buffer_createNull());

					Buffer_free(&device.dataExt, Platform_instance.alloc);
					InputDevice_free(&device);

					Log_error(String_createRefUnsafeConst("Couldn't create raw input device"), LogOptions_Default);
					break;
				}

				//Store device and call callback

				*(HANDLE*) device.dataExt.ptr = (HANDLE)lParam;
				*dev = device;

				if (w->callbacks.onDeviceAdd)
					w->callbacks.onDeviceAdd(w, dev);

			} else {

				//Find our device

				InputDevice *beg = (InputDevice*) List_begin(w->devices);
				InputDevice *end = (InputDevice*) List_end(w->devices);

				InputDevice *ours = beg;

				for(; ours != end; ++ours)
					if(*(HANDLE*) ours->dataExt.ptr == (HANDLE) lParam)
						break;

				if(ours == end)		//Unrecognized device
					break;

				//Notify our removal

				if (w->callbacks.onDeviceRemove)
					w->callbacks.onDeviceRemove(w, ours);

				//Cleanup our device

				if((err = InputDevice_free(ours)).genericError)
					Error_printx(err, LogLevel_Error, LogOptions_Default);

				//We need to keep on popping the end of the array until we reach the next element that isn't invalid
				//This is to keep the list as small as possible because we might be looping over it at some point
				//We can of course have devices that aren't initialized in between valid ones, 
				//because our list isn't contiguous

				if(ours == end - 1)
					while(
						w->devices.length && 
						((InputDevice*)List_last(w->devices))->type == InputDeviceType_Undefined
					)
						if((List_popBack(&w->devices, Buffer_createNull())).genericError)
							break;

			}

			return 0;
		}

		//Render

		case WM_PAINT: {

			if(!(w->hint & WindowHint_AllowBackgroundUpdates) && (w->flags & WindowFlags_IsMinimized))
				return 0;

			//Update interface

			Ns now = Timer_now();

			if (w->callbacks.onUpdate) {
				F32 dt = w->lastUpdate ? (now - w->lastUpdate) / (F32)seconds : 0;
				w->callbacks.onUpdate(w, dt);
			}

			w->lastUpdate = now;

			//Update input

			InputDevice *dit = (InputDevice*) List_begin(w->devices);
			InputDevice *dend = (InputDevice*) List_end(w->devices);

			for(; dit != dend; ++dit)
				InputDevice_markUpdate(*dit);

			//Render (if not minimized)

			if(w->callbacks.onDraw && !(w->flags & WindowFlags_IsMinimized)) {
				w->isDrawing = true;
				w->callbacks.onDraw(w);
				w->isDrawing = false;
			}

			return 0;
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

			if (w->callbacks.onResize)
				w->callbacks.onResize(w);

			break;
		}

		case WM_MOVE: {
		
			RECT r;
			GetWindowRect(hwnd, &r);
		
			w->offset = I32x2_create2(r.left, r.top);

			WWindow_updateMonitors(w);

			if (w->callbacks.onWindowMove)
				w->callbacks.onWindowMove(w);

			break;
		}
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

Bool WindowManager_supportsFormat(WindowManager manager, WindowFormat format) {

	manager;

	//TODO: HDR support; ColorSpace
	//	https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-getcontainingoutput
	//	https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_6/ns-dxgi1_6-dxgi_output_desc1

	return format == WindowFormat_rgba8;
}

Error WindowManager_freePhysical(WindowManager *manager, Window **w) {

	if(!manager)
		return (Error) { .genericError = GenericError_NullPointer };

	if(!Lock_isLockedForThread(manager->lock))
		return (Error) { .genericError = GenericError_InvalidOperation };

	if(!w || !*w)
		return (Error) { .genericError = GenericError_NullPointer, .errorSubId = 1 };

	if(*w < (Window*) List_begin(manager->windows) || *w >= (Window*) List_end(manager->windows))
		return (Error) { .genericError = GenericError_OutOfBounds };

	if(!((*w)->flags & (WindowFlags_IsActive | WindowFlags_IsVirtual)))
		return (Error) { .genericError = GenericError_InvalidOperation, .paramId = 1 };

	//Ensure our window safely exits

	PostMessageA((HWND) (*w)->nativeHandle, WM_DESTROY, 0, 0);
	*w = NULL;
	return Error_none();
}

Error Window_updatePhysicalTitle(
	const Window *w,
	String title
) {

	if(!w || !title.ptr || !title.len)
		return (Error) { .genericError = GenericError_NullPointer, .paramId = !!w };

	if (title.len >= MAX_PATH)
		return (Error) { 
			.genericError = GenericError_OutOfBounds, 
			.paramId = 1, 
			.paramValue0 = title.len,
			.paramValue1 = MAX_PATH
		};

	C8 windowName[MAX_PATH + 1];
	Buffer_copy(Buffer_createRef(windowName, sizeof(windowName)), String_buffer(title));

	windowName[title.len] = '\0';

	if(!SetWindowTextA(w->nativeHandle, windowName))
		return (Error) { .genericError = GenericError_InvalidOperation };

	return Error_none();
}

Error Window_presentPhysical(const Window *w) {

	if(!w || !(w->flags & WindowFlags_IsActive) || !(w->hint & WindowHint_ProvideCPUBuffer))
		return (Error) { .genericError = w ? GenericError_NullPointer : GenericError_InvalidOperation };

	PAINTSTRUCT ps;
	HDC hdcBmp = NULL, oldBmp = NULL;
	U32 errId = 0;

	HDC hdc = BeginPaint(w->nativeHandle, &ps);

	if(!hdc)
		return (Error) { .genericError = GenericError_InvalidOperation, .errorSubId = 1 };

	hdcBmp = CreateCompatibleDC(hdc);

	if(!hdcBmp) {
		errId = 2;
		goto cleanup;
	}

	oldBmp = SelectObject(hdcBmp, w->nativeData);

	if(!oldBmp) {
		errId = 3;
		goto cleanup;
	}

	if(!BitBlt(hdc, 0, 0, I32x2_x(w->size), I32x2_y(w->size), hdcBmp, 0, 0, SRCCOPY)) {
		errId = 4;
		goto cleanup;
	}

	return Error_none();

cleanup:

	if(oldBmp)
		SelectObject(hdc, oldBmp);

	if(hdcBmp)
		DeleteDC(hdcBmp);

	EndPaint(w->nativeHandle, &ps);
	return (Error) { .genericError = GenericError_InvalidOperation, .errorSubId = errId };
}