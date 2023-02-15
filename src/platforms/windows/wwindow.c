/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#include "types/buffer.h"
#include "platforms/window.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "platforms/input_device.h"
#include "platforms/keyboard.h"
#include "platforms/mouse.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/bufferx.h"
#include "types/time.h"

#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

const U16 Window_MAX_DEVICES = 64;
const U16 Window_MAX_MONITORS = 64;

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

			w->flags &= ~EWindowFlags_IsActive;
			break;
		}

		case WM_CLOSE:
		case WM_CREATE:
			break;

		//Setting focus

		case WM_SETFOCUS:
		case WM_KILLFOCUS:

			if(message == WM_SETFOCUS)
				w->flags |= EWindowFlags_IsFocussed;

			else {

				//TODO: Reset keys because otherwise they might hang

				w->flags &= ~EWindowFlags_IsFocussed;
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
				(HRAWINPUT)lParam, 
				RID_INPUT, 
				NULL, 
				&size, 
				sizeof(RAWINPUTHEADER)
			);

			Buffer buf = Buffer_createNull(); 
			Error err = Buffer_createUninitializedBytesx(size, &buf);

			if(err.genericError) {
				Error_printx(err, ELogLevel_Error, ELogOptions_Default);
				break;
			}

			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buf.ptr, &size, sizeof(RAWINPUTHEADER)) != size) {
				Error_printx(Error_platformError(0, GetLastError()), ELogLevel_Error, ELogOptions_Default);
				Buffer_freex(&buf);
				Log_error(String_createConstRefUnsafe("Couldn't get raw input"), ELogOptions_Default);
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

			if (dev->type == EInputDeviceType_Keyboard) {

				RAWKEYBOARD keyboardDat = data->data.keyboard;

				Bool isKeyDown = !(keyboardDat.Flags & 1);

				//Ensure the key state is up to date
				//It's a shame we have to get the state, but we can't rely on our program to know the exact state
				//Because locks can be toggled from a different program

				InputDevice_setFlagTo(dev, EKeyboardFlags_Caps, GetKeyState(VK_CAPITAL) & 1);
				InputDevice_setFlagTo(dev, EKeyboardFlags_NumLock, GetKeyState(VK_NUMLOCK) & 1);
				InputDevice_setFlagTo(dev, EKeyboardFlags_ScrollLock, GetKeyState(VK_SCROLL) & 1);

				//Translate key to our system and set corresponding device flags

				InputHandle handle = InputDevice_invalidHandle();

				switch (keyboardDat.VKey) {

					case VK_LSHIFT:
					case VK_RSHIFT:
					case VK_SHIFT:
						InputDevice_setFlagTo(dev, EKeyboardFlags_Shift, isKeyDown);
						handle = EKey_Shift;
						break;

					case VK_LMENU:
					case VK_RMENU:
					case VK_MENU:
						InputDevice_setFlagTo(dev, EKeyboardFlags_Alt, isKeyDown);
						handle = EKey_Alt;
						break;

					case VK_LCONTROL:
					case VK_RCONTROL:
					case VK_CONTROL:
						InputDevice_setFlagTo(dev, EKeyboardFlags_Control, isKeyDown);
						handle = EKey_Ctrl;
						break;

					case VK_NUMLOCK:
						InputDevice_setFlagTo(dev, EKeyboardFlags_NumLock, isKeyDown);
						handle = EKey_NumLock;
						break;

					case VK_SCROLL:
						InputDevice_setFlagTo(dev, EKeyboardFlags_ScrollLock, isKeyDown);
						handle = EKey_ScrollLock;
						break;

					case VK_BACK:				handle = EKey_Backspace;	break;
					case VK_SPACE:				handle = EKey_Space;		break;
					case VK_TAB:				handle = EKey_Tab;			break;
					case VK_PAUSE:				handle = EKey_Pause;		break;
					case VK_CAPITAL:			handle = EKey_Caps;			break;
					case VK_ESCAPE:				handle = EKey_Escape;		break;
					case VK_PRIOR:				handle = EKey_PageUp;		break;
					case VK_NEXT:				handle = EKey_PageDown;		break;
					case VK_END:				handle = EKey_End;			break;
					case VK_HOME:				handle = EKey_Home;			break;
					case VK_SELECT:				handle = EKey_Select;		break;
					case VK_PRINT:				handle = EKey_Print;		break;
					case VK_EXECUTE:			handle = EKey_Execute;		break;
					case VK_SNAPSHOT:			handle = EKey_PrintScreen;	break;
					case VK_INSERT:				handle = EKey_Insert;		break;
					case VK_BROWSER_BACK:		handle = EKey_Back;			break;
					case VK_BROWSER_FORWARD:	handle = EKey_Forward;		break;
					case VK_SLEEP:				handle = EKey_Sleep;		break;
					case VK_BROWSER_REFRESH:	handle = EKey_Refresh;		break;
					case VK_BROWSER_STOP:		handle = EKey_Stop;			break;
					case VK_BROWSER_SEARCH:		handle = EKey_Search;		break;
					case VK_BROWSER_FAVORITES:	handle = EKey_Favorites;	break;
					case VK_BROWSER_HOME:		handle = EKey_Start;		break;
					case VK_VOLUME_MUTE:		handle = EKey_Mute;			break;
					case VK_VOLUME_DOWN:		handle = EKey_VolumeDown;	break;
					case VK_VOLUME_UP:			handle = EKey_VolumeUp;		break;
					case VK_MEDIA_NEXT_TRACK:	handle = EKey_Skip;			break;
					case VK_MEDIA_PREV_TRACK:	handle = EKey_Previous;		break;
					case VK_CLEAR:				handle = EKey_Clear;		break;
					case VK_ZOOM:				handle = EKey_Zoom;			break;
					case VK_RETURN:				handle = EKey_Enter;		break;
					case VK_DELETE:				handle = EKey_Delete;		break;
					case VK_HELP:				handle = EKey_Help;			break;
					case VK_APPS:				handle = EKey_Apps;			break;

					case VK_LEFT:				handle = EKey_Left;			break;
					case VK_UP:					handle = EKey_Up;			break;
					case VK_RIGHT:				handle = EKey_Right;		break;
					case VK_DOWN:				handle = EKey_Down;			break;

					case VK_MULTIPLY:			handle = EKey_NumpadMul;	break;
					case VK_ADD:				handle = EKey_NumpadAdd;	break;
					case VK_DECIMAL:			handle = EKey_NumpadDec;	break;
					case VK_DIVIDE:				handle = EKey_NumpadDiv;	break;
					case VK_SUBTRACT:			handle = EKey_NumpadSub;	break;

					case VK_OEM_PLUS:			handle = EKey_Equals;		break;
					case VK_OEM_COMMA:			handle = EKey_Comma;		break;
					case VK_OEM_MINUS:			handle = EKey_Minus;		break;
					case VK_OEM_PERIOD:			handle = EKey_Period;		break;
					case VK_OEM_1:				handle = EKey_Semicolon;	break;
					case VK_OEM_2:				handle = EKey_Slash;		break;
					case VK_OEM_3:				handle = EKey_Acute;		break;
					case VK_OEM_4:				handle = EKey_LBracket;		break;
					case VK_OEM_6:				handle = EKey_RBracket;		break;
					case VK_OEM_5:				handle = EKey_Backslash;	break;
					case VK_OEM_7:				handle = EKey_Quote;		break;

					//Unknown key or common key

					default:

						if(keyboardDat.VKey >= '0' && keyboardDat.VKey <= '9')
							handle = EKey_0 + (keyboardDat.VKey - '0');

						else if(keyboardDat.VKey >= 'A' && keyboardDat.VKey <= 'Z')
							handle = EKey_A + (keyboardDat.VKey - 'A');

						else if(keyboardDat.VKey >= VK_F1 && keyboardDat.VKey <= VK_F24)
							handle = EKey_F1 + (keyboardDat.VKey - VK_F1);

						else if(keyboardDat.VKey >= VK_NUMPAD0 && keyboardDat.VKey <= VK_NUMPAD9)
							handle = EKey_Numpad0 + (keyboardDat.VKey - VK_NUMPAD0);

						else goto cleanup;

						break;
				}

				//Send keys through interface and update input device

				EInputState prevState = InputDevice_getState(*dev, handle);

				InputDevice_setCurrentState(*dev, handle, isKeyDown);
				EInputState newState = InputDevice_getState(*dev, handle);

				if(prevState != newState && w->callbacks.onDeviceButton)
					w->callbacks.onDeviceButton(w, dev, handle, isKeyDown);

				return 0;

			} else {

				RAWMOUSE mouseDat = data->data.mouse;

				for (U64 i = 0; i < 5; ++i) {

					Bool isDown = mouseDat.usButtonFlags & (1 << (i << 1));
					Bool isUp = mouseDat.usButtonFlags & (2 << (i << 1));

					if(!isDown && !isUp)
						continue;

					InputHandle handle = (InputHandle) (EMouseButton_Left + i);
					InputDevice_setCurrentState(*dev, handle, isDown);

					if (w->callbacks.onDeviceButton)
						w->callbacks.onDeviceButton(w, dev, handle, isDown);
				}

				if (mouseDat.usButtonFlags & RI_MOUSE_WHEEL) {

					F32 delta = (F32)mouseDat.usButtonData / WHEEL_DELTA;
					InputDevice_setCurrentAxis(*dev, EMouseAxis_ScrollWheel_X, delta);

					if (w->callbacks.onDeviceAxis)
						w->callbacks.onDeviceAxis(w, dev, EMouseAxis_ScrollWheel_X, delta);
				}

				if (mouseDat.usButtonFlags & RI_MOUSE_HWHEEL) {

					F32 delta = (F32)mouseDat.usButtonData / WHEEL_DELTA;
					InputDevice_setCurrentAxis(*dev, EMouseAxis_ScrollWheel_Y, delta);

					if (w->callbacks.onDeviceAxis)
						w->callbacks.onDeviceAxis(w, dev, EMouseAxis_ScrollWheel_Y, delta);
				}

				F32 prevX = InputDevice_getCurrentAxis(*dev, EMouseAxis_X);
				F32 prevY = InputDevice_getCurrentAxis(*dev, EMouseAxis_Y);

				F32 nextX, nextY;

				if (mouseDat.usFlags & MOUSE_MOVE_ABSOLUTE) {

					RECT rect;
					GetClientRect(hwnd, &rect);

					InputDevice_resetFlag(dev, EMouseFlag_IsRelative);

					nextX = (F32) (mouseDat.lLastX - rect.left);
					nextY = (F32) (mouseDat.lLastY - rect.top);

				} else {
					InputDevice_setFlag(dev, EMouseFlag_IsRelative);
					nextX = prevX + (F32) mouseDat.lLastX;
					nextY = prevY + (F32) mouseDat.lLastY;
				}

				if (nextX != prevX) {

					InputDevice_setCurrentAxis(*dev, EMouseAxis_X, nextX);

					if (w->callbacks.onDeviceAxis)
						w->callbacks.onDeviceAxis(w, dev, EMouseAxis_X, nextX);
				}

				if (nextY != prevY) {

					InputDevice_setCurrentAxis(*dev, EMouseAxis_Y, nextY);

					if (w->callbacks.onDeviceAxis)
						w->callbacks.onDeviceAxis(w, dev, EMouseAxis_Y, nextY);
				}
			}

		cleanup:
			LRESULT lr = DefRawInputProc(&data, 1, sizeof(*data));
			Buffer_freex(&buf);
			return lr;
		}

		//TODO: Handle capture cursor

		case WM_INPUT_DEVICE_CHANGE: {

			RID_DEVICE_INFO deviceInfo = (RID_DEVICE_INFO) { 0 };
			U32 size = sizeof(deviceInfo);

			if (!GetRawInputDeviceInfoA((HANDLE)lParam, RIDI_DEVICEINFO, &deviceInfo, &size)) {
				Error_printx(Error_platformError(0, GetLastError()), ELogLevel_Error, ELogOptions_Default);
				Log_error(String_createConstRefUnsafe("Invalid data in WM_INPUT_DEVICE_CHANGE"), ELogOptions_Default);
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
					Log_warn(String_createConstRefUnsafe("Possibly unsupported type of keyboard!"), ELogOptions_Default);

				//Create input device

				InputDevice device = (InputDevice) { 0 };

				if (isKeyboard) {

					if ((err = EKeyboard_create((EKeyboard*) &device)).genericError) {
						Error_printx(err, ELogLevel_Error, ELogOptions_Default);
						break;
					}
				}

				else if ((err = Mouse_create((Mouse*) &device)).genericError) {
					Error_printx(err, ELogLevel_Error, ELogOptions_Default);
					break;
				}

				if((err = Buffer_createUninitializedBytesx(sizeof(HANDLE), &device.dataExt)).genericError) {
					InputDevice_free(&device);
					Error_printx(err, ELogLevel_Error, ELogOptions_Default);
					break;
				}

				RAWINPUTDEVICE rawDevice = (RAWINPUTDEVICE) {
					0x01,								//Perhaps 0xD for touchscreen at some point			TODO:
					(U16)(isKeyboard ? 0x06 : 0x02),	//0x4-0x05 for game controllers in the future		TODO:
					0x0,
					hwnd
				};

				//Find free spot in device array
				//If this happens, we don't need to resize the array

				InputDevice *beg = (InputDevice*) List_begin(w->devices);
				InputDevice *end = (InputDevice*) List_end(w->devices);

				InputDevice *dev = beg;

				for(; dev != end; ++dev)
					if(dev->type == EInputDeviceType_Undefined)
						break;

				//Our list isn't big enough, we need to resize

				Bool pushed = false;

				if(dev == end) {

					//Try to allocate without allowing allocation
					//If it fails, we can't create a new device

					err = List_pushBack(&w->devices, Buffer_createNull(), (Allocator) { 0 });
					pushed = true;

					if(err.genericError) {
						Buffer_freex(&device.dataExt);
						InputDevice_free(&device);
						Log_error(String_createConstRefUnsafe("Couldn't register device; "), ELogOptions_Default);
						break;
					}

					//After this, our dev and end pointers are valid still
					//Because our list didn't reallocate, it just changed size
				}

				//Register device

				if (!RegisterRawInputDevices(&rawDevice, 1, sizeof(RAWINPUTDEVICE))) {

					Error_printx(Error_platformError(0, GetLastError()), ELogLevel_Error, ELogOptions_Default);

					if(pushed)
						List_popBack(&w->devices, Buffer_createNull());

					Buffer_freex(&device.dataExt);
					InputDevice_free(&device);

					Log_error(String_createConstRefUnsafe("Couldn't create raw input device"), ELogOptions_Default);
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

				InputDevice_free(ours);

				//We need to keep on popping the end of the array until we reach the next element that isn't invalid
				//This is to keep the list as small as possible because we might be looping over it at some point
				//We can of course have devices that aren't initialized in between valid ones, 
				//because our list isn't contiguous

				if(ours == end - 1)
					while(
						w->devices.length && 
						((InputDevice*)List_last(w->devices))->type == EInputDeviceType_Undefined
					)
						if((List_popBack(&w->devices, Buffer_createNull())).genericError)
							break;

			}

			return 0;
		}

		//Render

		case WM_PAINT: {

			if(!(w->hint & EWindowHint_AllowBackgroundUpdates) && (w->flags & EWindowFlags_IsMinimized))
				return 0;

			//Update interface

			Ns now = Time_now();

			if (w->callbacks.onUpdate) {
				F32 dt = w->lastUpdate ? (now - w->lastUpdate) / (F32)SECOND : 0;
				w->callbacks.onUpdate(w, dt);
			}

			w->lastUpdate = now;

			//Update input

			InputDevice *dit = (InputDevice*) List_begin(w->devices);
			InputDevice *dend = (InputDevice*) List_end(w->devices);

			for(; dit != dend; ++dit)
				InputDevice_markUpdate(*dit);

			//Render (if not minimized)

			if(w->callbacks.onDraw && !(w->flags & EWindowFlags_IsMinimized)) {
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
				w->flags |= EWindowFlags_IsMinimized;

			else w->flags &= ~EWindowFlags_IsMinimized;

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

Bool WindowManager_supportsFormat(WindowManager manager, EWindowFormat format) {

	manager;

	//TODO: HDR support; ColorSpace
	//	https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-getcontainingoutput
	//	https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_6/ns-dxgi1_6-dxgi_output_desc1

	return format == EWindowFormat_rgba8;
}

Bool WindowManager_freePhysical(WindowManager *manager, Window **w) {

	if(!manager || !manager->lock.data)
		return false;

	if(!w || !*w)
		return true;

	if(!Lock_isLockedForThread(manager->lock))
		return false;

	if(*w < (Window*) List_begin(manager->windows) || *w >= (Window*) List_end(manager->windows))
		return false;

	if(!((*w)->flags & (EWindowFlags_IsActive | EWindowFlags_IsVirtual)))
		return false;

	//Ensure our window safely exits

	PostMessageA((HWND) (*w)->nativeHandle, WM_DESTROY, 0, 0);
	*w = NULL;
	return true;
}

Error Window_updatePhysicalTitle(const Window *w, String title) {

	if(!w || !I32x2_any(w->size) || !title.ptr || !title.length)
		return Error_nullPointer(!w || !I32x2_any(w->size) ? 0 : 1, 0);

	if (title.length >= MAX_PATH)
		return Error_outOfBounds(1, 0, title.length, MAX_PATH);

	C8 windowName[MAX_PATH + 1];
	Buffer_copy(Buffer_createRef(windowName, sizeof(windowName)), String_bufferConst(title));

	windowName[title.length] = '\0';

	if(!SetWindowTextA(w->nativeHandle, windowName))
		return Error_platformError(0, GetLastError());

	return Error_none();
}

Error Window_presentPhysical(const Window *w) {

	if(!w || !I32x2_any(w->size))
		return Error_nullPointer(0, 0);

	if(!(w->flags & EWindowFlags_IsActive) || !(w->hint & EWindowHint_ProvideCPUBuffer))
		return Error_invalidOperation(0);

	PAINTSTRUCT ps;
	HDC hdcBmp = NULL, oldBmp = NULL;
	U32 errId = 0;

	HDC hdc = BeginPaint(w->nativeHandle, &ps);

	if(!hdc)
		return Error_platformError(0, GetLastError());

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

	HRESULT res = GetLastError();

	if(oldBmp)
		SelectObject(hdc, oldBmp);

	if(hdcBmp)
		DeleteDC(hdcBmp);

	EndPaint(w->nativeHandle, &ps);
	return Error_platformError(errId, res);
}
