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

//platforms/windows/wwindow.c

#include "types/container/list_impl.h"
#include "platforms/window.h"
#include "platforms/window_manager.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "platforms/input_device.h"
#include "platforms/keyboard.h"
#include "platforms/mouse.h"
#include "types/container/buffer.h"
#include "types/container/string_unicode.h"
#include "types/container/list_basic_types.h"
#include "types/base/mathf.h"
#include "types/base/time.h"
#include "types/base/constants.h"

#include <stdlib.h>

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#define NOMINMAX
#include <Windows.h>
#include <dwmapi.h>

Bool WindowManager_updateMonitorsExt(ListMonitor *monitors, LPCRECT clip, Error *e_rr);

void WWindow_updateMonitors(Window *w) {
 
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	if(!w)
		retError(clean, Error_nullPointer(0, "WWindow_updateMonitors()::w is required"));

	RECT rc;
	GetWindowRect(w->nativeHandle, &rc);

	gotoIfError3(clean, WindowManager_updateMonitorsExt(&w->monitors, &rc, e_rr));

	if(w->callbacks.onMonitorChange)
		w->callbacks.onMonitorChange(w);

	w->owner->monitorsDirty = true;
	return;

clean:
	Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);
}

Bool WWindow_initSize(Window *w, I32x2 size, Error *e_rr) {

	if(w->nativeData) {
		DeleteObject((HGDIOBJ) w->nativeData);
		w->nativeData = NULL;
	}

	HDC screen = NULL;
	Bool s_uccess = true;

	if(w->hint & EWindowHint_ProvideCPUBuffer) {

		screen = GetDC(w->nativeHandle);

		if(!screen)
			retError(clean, Error_platformError(2, GetLastError(), "WWindow_initSize() GetDC failed"));

		//TODO: Support something other than RGBA8

		const BITMAPINFO bmi = (BITMAPINFO) {
			.bmiHeader = {
				.biSize = sizeof(BITMAPINFOHEADER),
				.biWidth = I32x2_x(size),
				.biHeight = I32x2_y(size),
				.biPlanes = 1,
				.biBitCount = 32,
				.biCompression = BI_RGB
			}
		};

		w->nativeData = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, (void**) &w->cpuVisibleBuffer.ptr, NULL, 0);

		if (!w->nativeData)
			retError(clean, Error_platformError(3, GetLastError(), "WWindow_initSize() CreateDIBSection failed"));

		//Manually set it to be a reference
		//This makes it, so we don't free it, because we don't own the memory

		w->cpuVisibleBuffer.lengthAndRefBits = ((U64)bmi.bmiHeader.biWidth * bmi.bmiHeader.biHeight * 4) | ((U64)1 << 63);

		ReleaseDC(w->nativeHandle, screen);
		screen = NULL;
	}

clean:

	if(screen)
		ReleaseDC(w->nativeHandle, screen);

	return s_uccess;
}

LRESULT CALLBACK WWindow_onCallback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

	Window *w = (Window*) GetWindowLongPtrW(hwnd, 0);

	if(!w)
		return DefWindowProc(hwnd, message, wParam, lParam);

	switch (message) {

		case WM_QUIT:
		case WM_CLOSE:
			w->flags |= EWindowFlags_ShouldTerminate;
			return 0;

		case WM_CREATE:
			break;

		//Setting focus

		case WM_SETFOCUS:
		case WM_KILLFOCUS:

			if(message == WM_SETFOCUS)
				w->flags |= EWindowFlags_IsFocussed;

			else {

				//Reset keys to avoid them hanging.

				for (U64 i = 0; i < w->devices.length; ++i) {

					InputDevice *dev = &w->devices.ptrNonConst[i];

					//Reset buttons

					for (U16 j = 0; j < dev->buttons; ++j) {

						InputHandle handle = InputDevice_createHandle(dev, j, EInputType_Button);

						EInputState prevState = InputDevice_getState(dev, handle);

						if(prevState & EInputState_Curr) {

							InputDevice_setCurrentState(dev, handle, false);
							EInputState newState = InputDevice_getState(dev, handle);

							if(prevState != newState && w->callbacks.onDeviceButton)
								w->callbacks.onDeviceButton(w, dev, handle, false);
						}
					}

					//Reset axes

					for (U16 j = 0; j < dev->axes; ++j) {

						InputAxis axis = *InputDevice_getAxis(dev, j);

						if(!axis.resetOnInputLoss)
							continue;

						InputHandle handle = InputDevice_createHandle(dev, j, EInputType_Axis);

						F32 prevStatef = InputDevice_getCurrentAxis(dev, handle);

						if (prevStatef) {

							InputDevice_setCurrentAxis(dev, handle, 0);

							if(w->callbacks.onDeviceAxis)
								w->callbacks.onDeviceAxis(w, dev, handle, 0);
						}
					}

					//Reset flags that might've been set

					dev->flags = 0;
				}

				w->flags &= ~EWindowFlags_IsFocussed;
			}

			if(w->callbacks.onUpdateFocus)
				w->callbacks.onUpdateFocus(w);

			break;

		case WM_DISPLAYCHANGE:
			WWindow_updateMonitors(w);
			break;

		//Input handling

		case WM_CHAR: {

			Buffer buf = Buffer_createRef(w->buffer, sizeof(w->buffer));
			UnicodeCodePoint typed = 0;
			Bool clearBuffer = true;

			//Single char

			if (wParam <= 0xD7FF)
				typed = (UnicodeCodePoint) wParam;

			//First char

			else if (wParam < 0xDC00) {                //Cache for final char
				((U16*)buf.ptrNonConst)[0] = (U16)wParam;
				clearBuffer = false;
			}

			//Second char

			else if(wParam < 0xE000 && buf.ptr[0]) {

				((U16*)buf.ptrNonConst)[1] = (U16)wParam;

				//Now it's time to translate this from UTF16

				UnicodeCodePointInfo codepoint = (UnicodeCodePointInfo) { 0 };
				if (Buffer_readAsUTF16(buf, 0, &codepoint, NULL))
					typed = codepoint.index;
			}

			if(typed && w->callbacks.onTypeChar) {

				//Translate to UTF8

				U8 bytes = 0;
				Bool success = Buffer_writeAsUTF8(buf, 0, typed, &bytes, NULL);
				((C8*)buf.ptr)[bytes] = '\0';

				if(success)
					w->callbacks.onTypeChar(w, CharString_createRefSizedConst((const C8*)buf.ptr, bytes, true));
			}

			if(clearBuffer)
				((C8*)buf.ptr)[0] = '\0';        //Clear buffer

			break;
		}

		case WM_INPUT: {

			RAWINPUT raw;
			U32 rawSiz = (U32) sizeof(raw);
			if (!GetRawInputData((HRAWINPUT)lParam, RID_INPUT, (U8*) &raw, &rawSiz, sizeof(RAWINPUTHEADER))) {
				Error err = Error_platformError(0, GetLastError(), "WWindow_onCallback() GetRawInputData failed");
				Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);
				break;
			}

			RAWINPUT *data = &raw;

			//Grab device from the list
			//TODO: Map

			InputDevice *dev = w->devices.ptrNonConst;
			InputDevice *end = ListInputDevice_end(w->devices);

			for(; dev != end; ++dev)
				if(dev->dataExt.ptr && *(HANDLE*) dev->dataExt.ptr == data->header.hDevice)
					break;

			//hDevice is actually NULL when there's no HID representing this event.
			//This can happen in multiple cases:
			//- Touchpad
			//- SendKeys (unit testing)
			//- Accessibility; e.g. On screen keyboard
			//- Remote desktop / VM
			//- Tablet windows perhaps?
			//In that case, we default to the builtin keyboard/mouse.

			if (!data->header.hDevice || dev == end) {

				switch (raw.header.dwType) {

					case RIM_TYPEKEYBOARD:
						dev = w->devices.ptrNonConst + w->defaultKeyboardId;
						break;

					case RIM_TYPEMOUSE:
						dev = w->devices.ptrNonConst + w->defaultMouseId;
						break;

					default:
						goto cleanup;
				}
			}

			if (dev->type == EInputDeviceType_Keyboard) {

				RAWKEYBOARD keyboardDat = data->data.keyboard;

				Bool isKeyDown = !(keyboardDat.Flags & 1);

				//Ensure the key state is up-to-date
				//It's a shame we have to get the state, but we can't rely on our program to know the exact state
				//Because locks can be toggled from a different program

				U32 flags =
					((GetKeyState(VK_CAPITAL) & 1) << EKeyboardFlags_Caps) |
					((GetKeyState(VK_NUMLOCK) & 1) << EKeyboardFlags_NumLock) |
					((GetKeyState(VK_SCROLL) & 1) << EKeyboardFlags_ScrollLock);

				dev->flags = flags;

				//Translate scan code to our system and set corresponding device flags.
				//In case of special keys, we don't want to use scan codes.
				//Only if it's about our main keyboard or numpad

				InputHandle handle = InputDevice_invalidHandle();

				switch (keyboardDat.VKey) {

					case VK_SNAPSHOT:            handle = EKey_PrintScreen;        break;
					case VK_SCROLL:                handle = EKey_ScrollLock;        break;
					case VK_NUMLOCK:            handle = EKey_NumLock;            break;
					case VK_PAUSE:                handle = EKey_Pause;            break;
					case VK_INSERT:                handle = EKey_Insert;            break;
					case VK_HOME:                handle = EKey_Home;                break;
					case VK_PRIOR:                handle = EKey_PageUp;            break;
					case VK_NEXT:                handle = EKey_PageDown;            break;
					case VK_DELETE:                handle = EKey_Delete;            break;
					case VK_END:                handle = EKey_End;                break;

					case VK_UP:                    handle = EKey_Up;                break;
					case VK_LEFT:                handle = EKey_Left;                break;
					case VK_DOWN:                handle = EKey_Down;                break;
					case VK_RIGHT:                handle = EKey_Right;            break;

					case VK_BROWSER_BACK:        handle = EKey_Back;                break;
					case VK_BROWSER_FORWARD:    handle = EKey_Forward;            break;
					case VK_SLEEP:                handle = EKey_Sleep;            break;
					case VK_BROWSER_REFRESH:    handle = EKey_Refresh;            break;
					case VK_BROWSER_SEARCH:        handle = EKey_Search;            break;
					case VK_VOLUME_MUTE:        handle = EKey_Mute;                break;
					case VK_VOLUME_DOWN:        handle = EKey_VolumeDown;        break;
					case VK_VOLUME_UP:            handle = EKey_VolumeUp;            break;
					case VK_MEDIA_NEXT_TRACK:    handle = EKey_Skip;                break;
					case VK_MEDIA_PREV_TRACK:    handle = EKey_Previous;            break;
					case VK_CLEAR:                handle = EKey_Clear;            break;
					case VK_RETURN:                handle = EKey_Enter;            break;
					case VK_HELP:                handle = EKey_Help;                break;

					case VK_MULTIPLY:            handle = EKey_NumpadMul;        break;
					case VK_ADD:                handle = EKey_NumpadAdd;        break;
					case VK_DECIMAL:            handle = EKey_NumpadDot;        break;
					case VK_DIVIDE:                handle = EKey_NumpadDiv;        break;
					case VK_SUBTRACT:            handle = EKey_NumpadSub;        break;

					default:

						if(keyboardDat.VKey >= VK_NUMPAD0 && keyboardDat.VKey <= VK_NUMPAD9) {
							handle = EKey_Numpad0 + (keyboardDat.VKey - VK_NUMPAD0);
							break;
						}

						//VKey is used; on screen keyboard or SendKeys.
						if (!keyboardDat.MakeCode && keyboardDat.VKey) {

							if (keyboardDat.VKey >= 'A' && keyboardDat.VKey <= 'Z') {
								handle = EKey_A + (keyboardDat.VKey - 'A');
								break;
							}

							if (keyboardDat.VKey >= '0' && keyboardDat.VKey <= '9') {
								handle = EKey_0 + (keyboardDat.VKey - '0');
								break;
							}

							if (keyboardDat.VKey >= VK_F1 && keyboardDat.VKey <= VK_F12) {
								handle = EKey_F1 + (keyboardDat.VKey - VK_F1);
								break;
							}

							switch (keyboardDat.VKey) {

								//Navigation / editing

								case VK_ESCAPE:        handle = EKey_Escape;        break;
								case VK_TAB:        handle = EKey_Tab;            break;
								case VK_SPACE:        handle = EKey_Space;        break;
								case VK_BACK:        handle = EKey_Backspace;    break;

								//Modifiers (ambiguous but OK fallback)

								case VK_SHIFT:
								case VK_LSHIFT:
									handle = EKey_LShift;
									break;

								case VK_RSHIFT:        handle = EKey_RShift;        break;

								case VK_CONTROL:
								case VK_LCONTROL:
									handle = EKey_LCtrl;
									break;

								case VK_RCONTROL:    handle = EKey_RCtrl;        break;

								case VK_MENU:
								case VK_LMENU:
									handle = EKey_LAlt;
									break;

								case VK_RMENU:        handle = EKey_RAlt;            break;

								//Lock keys

								case VK_CAPITAL:    handle = EKey_Caps;            break;

								//OEM / punctuation

								case VK_OEM_1:        handle = EKey_Semicolon;    break; // ;
								case VK_OEM_2:        handle = EKey_Slash;        break; // /
								case VK_OEM_3:        handle = EKey_Backtick;        break; // `
								case VK_OEM_4:        handle = EKey_LBracket;        break; // [
								case VK_OEM_5:        handle = EKey_Backslash;    break; // "\"
								case VK_OEM_6:        handle = EKey_RBracket;        break; // ]
								case VK_OEM_7:        handle = EKey_Quote;        break; // '

								case VK_OEM_PLUS:    handle = EKey_Equals;        break;
								case VK_OEM_MINUS:    handle = EKey_Minus;        break;
								case VK_OEM_COMMA:    handle = EKey_Comma;        break;
								case VK_OEM_PERIOD: handle = EKey_Period;        break;

								default:
									goto cleanup;
							}
						}

						else switch (keyboardDat.MakeCode) {

							//Row 0

							case 0x01:                    handle = EKey_Escape;        break;

							case 0x3B:                    handle = EKey_F1;            break;
							case 0x3C:                    handle = EKey_F2;            break;
							case 0x3D:                    handle = EKey_F3;            break;
							case 0x3E:                    handle = EKey_F4;            break;
							case 0x3F:                    handle = EKey_F5;            break;
							case 0x40:                    handle = EKey_F6;            break;
							case 0x41:                    handle = EKey_F7;            break;
							case 0x42:                    handle = EKey_F8;            break;
							case 0x43:                    handle = EKey_F9;            break;
							case 0x44:                    handle = EKey_F10;            break;
							case 0x57:                    handle = EKey_F11;            break;
							case 0x58:                    handle = EKey_F12;            break;

							//Row 1

							case 0x29:                    handle = EKey_Backtick;        break;

							case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07: case 0x08: case 0x09: case 0x0A:
								handle = EKey_1 + (keyboardDat.MakeCode - 2);
								break;

							case 0xB:                    handle = EKey_0;            break;
							case 0xC:                    handle = EKey_Minus;        break;
							case 0xD:                    handle = EKey_Equals;        break;
							case 0xE:                    handle = EKey_Backspace;    break;

							//Row 2

							case 0x0F:                    handle = EKey_Tab;            break;
							case 0x10:                    handle = EKey_Q;            break;
							case 0x11:                    handle = EKey_W;            break;
							case 0x12:                    handle = EKey_E;            break;
							case 0x13:                    handle = EKey_R;            break;
							case 0x14:                    handle = EKey_T;            break;
							case 0x15:                    handle = EKey_Y;            break;
							case 0x16:                    handle = EKey_U;            break;
							case 0x17:                    handle = EKey_I;            break;
							case 0x18:                    handle = EKey_O;            break;
							case 0x19:                    handle = EKey_P;            break;
							case 0x1A:                    handle = EKey_LBracket;        break;
							case 0x1B:                    handle = EKey_RBracket;        break;

							//Row 3

							case 0x3A:                    handle = EKey_Caps;            break;
							case 0x1E:                    handle = EKey_A;            break;
							case 0x1F:                    handle = EKey_S;            break;
							case 0x20:                    handle = EKey_D;            break;
							case 0x21:                    handle = EKey_F;            break;
							case 0x22:                    handle = EKey_G;            break;
							case 0x23:                    handle = EKey_H;            break;
							case 0x24:                    handle = EKey_J;            break;
							case 0x25:                    handle = EKey_K;            break;
							case 0x26:                    handle = EKey_L;            break;
							case 0x27:                    handle = EKey_Semicolon;    break;
							case 0x28:                    handle = EKey_Quote;        break;
							case 0x2B:                    handle = EKey_Backslash;    break;

							//Row 4

							case 0x2A:                    handle = EKey_LShift;        break;
							case 0x56:                    handle = EKey_Bar;            break;
							case 0x2C:                    handle = EKey_Z;            break;
							case 0x2D:                    handle = EKey_X;            break;
							case 0x2E:                    handle = EKey_C;            break;
							case 0x2F:                    handle = EKey_V;            break;
							case 0x30:                    handle = EKey_B;            break;
							case 0x31:                    handle = EKey_N;            break;
							case 0x32:                    handle = EKey_M;            break;
							case 0x33:                    handle = EKey_Comma;        break;
							case 0x34:                    handle = EKey_Period;        break;
							case 0x35:                    handle = EKey_Slash;        break;
							case 0x36:                    handle = EKey_RShift;        break;

							//Row 5

							case 0x1D:                    handle = EKey_LCtrl;        break;
							case 0xE05B:                handle = EKey_LMenu;        break;
							case 0x38:                    handle = EKey_LAlt;            break;
							case 0x39:                    handle = EKey_Space;        break;
							case 0xE038:                handle = EKey_RAlt;            break;
							case 0xE05C:                handle = EKey_RMenu;        break;
							case 0xE05D:                handle = EKey_Options;        break;
							case 0xE01D:                handle = EKey_RCtrl;        break;

							//Unknown key

							default:
								goto cleanup;
						}

						break;
				}

				//Send keys through interface and update input device

				EInputState prevState = InputDevice_getState(dev, handle);

				InputDevice_setCurrentState(dev, handle, isKeyDown);
				EInputState newState = InputDevice_getState(dev, handle);

				if(prevState != newState && w->callbacks.onDeviceButton)
					w->callbacks.onDeviceButton(w, dev, handle, isKeyDown);

				return 0;

			} else if (dev->type == EInputDeviceType_Mouse) {

				RAWMOUSE mouseDat = data->data.mouse;

				for (U64 i = 0; i < 5; ++i) {

					Bool isDown = mouseDat.usButtonFlags & (1 << (i << 1));
					Bool isUp = mouseDat.usButtonFlags & (2 << (i << 1));

					if(!isDown && !isUp)
						continue;

					U64 j = i == 1 ? 2 : (i == 2 ? 1 : i);

					InputHandle handle = (InputHandle) (EMouseButton_Left + j);
					InputDevice_setCurrentState(dev, handle, isDown);

					if (w->callbacks.onDeviceButton)
						w->callbacks.onDeviceButton(w, dev, handle, isDown);
				}

				if (mouseDat.usButtonFlags & RI_MOUSE_WHEEL) {

					F32 delta = (F32)(I16)mouseDat.usButtonData / WHEEL_DELTA;
					InputDevice_setCurrentAxis(dev, EMouseAxis_ScrollWheel_Y, delta);

					if (w->callbacks.onDeviceAxis)
						w->callbacks.onDeviceAxis(w, dev, EMouseAxis_ScrollWheel_Y, delta);
				}

				if (mouseDat.usButtonFlags & RI_MOUSE_HWHEEL) {

					F32 delta = (F32)(I16)mouseDat.usButtonData / WHEEL_DELTA;
					InputDevice_setCurrentAxis(dev, EMouseAxis_ScrollWheel_X, delta);

					if (w->callbacks.onDeviceAxis)
						w->callbacks.onDeviceAxis(w, dev, EMouseAxis_ScrollWheel_X, delta);
				}

				F32 prevX = InputDevice_getCurrentAxis(dev, EMouseAxis_RX);
				F32 prevY = InputDevice_getCurrentAxis(dev, EMouseAxis_RY);

				F32 nextX, nextY;

				if (mouseDat.usFlags & MOUSE_MOVE_ABSOLUTE) {

					F32 prevAbsX = InputDevice_getCurrentAxis(dev, EMouseAxis_Temp0);
					F32 prevAbsY = InputDevice_getCurrentAxis(dev, EMouseAxis_Temp1);

					nextX = F32_clamp((F32) (mouseDat.lLastX - prevAbsX), -1, 1);
					nextY = F32_clamp((F32) (mouseDat.lLastY - prevAbsY), -1, 1);

					InputDevice_setCurrentAxis(dev, EMouseAxis_Temp0, (F32) mouseDat.lLastX);
					InputDevice_setCurrentAxis(dev, EMouseAxis_Temp1, (F32) mouseDat.lLastY);

				} else {
					nextX = (F32) mouseDat.lLastX;
					nextY = (F32) mouseDat.lLastY;
				}

				POINT p = (POINT) { 0 };
				GetCursorPos(&p);
				ScreenToClient(w->nativeHandle, &p);

				I32x2 oldCursor = w->cursor;
				w->cursor = I32x2_create2(p.x, p.y);

				if (w->callbacks.onCursorMove && I32x2_neq2(oldCursor, w->cursor))
					w->callbacks.onCursorMove(w);

				if (nextX != prevX) {

					InputDevice_setCurrentAxis(dev, EMouseAxis_RX, nextX);

					if (w->callbacks.onDeviceAxis)
						w->callbacks.onDeviceAxis(w, dev, EMouseAxis_RX, nextX);
				}

				if (nextY != prevY) {

					InputDevice_setCurrentAxis(dev, EMouseAxis_RY, nextY);

					if (w->callbacks.onDeviceAxis)
						w->callbacks.onDeviceAxis(w, dev, EMouseAxis_RY, nextY);
				}
			}

		cleanup:
			LRESULT lr = DefRawInputProc(&data, 1, sizeof(*data));
			return lr;
		}

		case WM_MOUSEMOVE: {

			I32x2 oldCursor = w->cursor;
			w->cursor = I32x2_create2((I32)(I16)LOWORD(lParam), (I32)(I16)HIWORD(lParam));

			if (w->callbacks.onCursorMove && I32x2_neq2(oldCursor, w->cursor))
				w->callbacks.onCursorMove(w);

			break;
		}

		//TODO: Handle capture cursor

		case WM_INPUT_DEVICE_CHANGE: {

			RID_DEVICE_INFO deviceInfo = (RID_DEVICE_INFO) { 0 };
			U32 size = sizeof(deviceInfo);
			deviceInfo.cbSize = size;

			if (!GetRawInputDeviceInfoW((HANDLE)lParam, RIDI_DEVICEINFO, &deviceInfo, &size)) {
				Error err = Error_platformError(0, GetLastError(), "WWindow_onCallback() GetRawInputDeviceInfo failed");
				Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);
				break;
			}

			if(deviceInfo.dwType == RIM_TYPEHID)        //Irrelevant for us for now
				break;

			Bool isAdded = wParam == GIDC_ARRIVAL;

			if (isAdded) {

				Bool s_uccess = true;
				Error err = Error_none(), *e_rr = &err;

				Bool isKeyboard = deviceInfo.dwType == RIM_TYPEKEYBOARD;
				Bool pushed = false;

				//Create input device

				InputDevice device = (InputDevice) { 0 };

				if (isKeyboard) {
					gotoIfError3(clean, Keyboard_create((Keyboard*) &device, Platform_instance->alloc, e_rr));
				}

				else gotoIfError3(clean, Mouse_create((Mouse*) &device, Platform_instance->alloc, e_rr));

				gotoIfError3(clean, Buffer_createUninitializedBytes(
					sizeof(HANDLE), Platform_instance->alloc, &device.dataExt, e_rr
				));

				RAWINPUTDEVICE rawDevice = (RAWINPUTDEVICE) {
					0x01,                                //Perhaps 0xD for touchscreen at some point            TODO:
					(U16)(isKeyboard ? 0x06 : 0x02),    //0x4-0x05 for game controllers in the future        TODO:
					0x0,
					hwnd
				};

				//Find free spot in device array
				//If this happens, we don't need to resize the array

				InputDevice *dev = ListInputDevice_begin(w->devices);
				InputDevice *end = ListInputDevice_end(w->devices);

				for(; dev != end; ++dev)
					if(dev->type == EInputDeviceType_Undefined)
						break;

				//Our list isn't big enough, we need to resize

				if(dev == end) {

					//If it fails, we can't create a new device

					gotoIfError3(clean, ListInputDevice_pushBack(
						&w->devices, (InputDevice) { 0 }, Platform_instance->alloc, e_rr
					));

					dev = ListInputDevice_last(w->devices);
					pushed = true;

					//After this, our dev and end pointers are valid still
					//Because our list didn't reallocate, it just changed size
				}

				//Register device

				if (!RegisterRawInputDevices(&rawDevice, 1, sizeof(RAWINPUTDEVICE)))
					retError(clean, Error_platformError(0, GetLastError(), "WWindow_onCallback() RegisterRawInputDevices failed"));

				//Store device and call callback

				*(HANDLE*) device.dataExt.ptrNonConst = (HANDLE)lParam;
				*dev = device;

				if (w->callbacks.onDeviceAdd)
					w->callbacks.onDeviceAdd(w, dev);

			clean:

				Error_print(Platform_instance->alloc, e_rr, ELogLevel_Error, ELogOptions_Default);

				if(!s_uccess) {

					if(pushed)
						ListInputDevice_popBack(&w->devices, NULL, NULL);

					Buffer_free(&device.dataExt, Platform_instance->alloc);
					InputDevice_free(&device, Platform_instance->alloc);
				}

			} else {

				//Find our device

				InputDevice *ours = ListInputDevice_begin(w->devices);
				InputDevice *end = ListInputDevice_end(w->devices);

				for(; ours != end; ++ours)
					if(ours->dataExt.ptr && *(HANDLE*) ours->dataExt.ptr == (HANDLE) lParam)
						break;

				if(ours == end)        //Unrecognized device
					break;

				//Notify our removal

				if (w->callbacks.onDeviceRemove)
					w->callbacks.onDeviceRemove(w, ours);

				//Cleanup our device

				InputDevice_free(ours, Platform_instance->alloc);

				//We need to keep on popping the end of the array until we reach the next element that isn't invalid
				//This is to keep the list as small as possible because we might be looping over it at some point
				//We can of course have devices that aren't initialized in between valid ones,
				//because our list isn't contiguous (to not invalidate handles)

				if(ours == end - 1)
					while(
						w->devices.length &&
						ListInputDevice_last(w->devices)->type == EInputDeviceType_Undefined
					)
						if(!ListInputDevice_popBack(&w->devices, NULL, NULL))
							break;
			}

			return 0;
		}

		//Render

		case WM_PAINT: {

			if(
				!(w->hint & EWindowHint_AllowBackgroundUpdates) &&
				(w->flags & EWindowFlags_IsMinimized) &&
				!(w->flags & EWindowFlags_IsMoving)
			)
				return 0;

			//Update interface

			InputDevice *dit = ListInputDevice_begin(w->devices);
			InputDevice *dend = ListInputDevice_end(w->devices);

			for(; dit != dend; ++dit)
				InputDevice_markUpdate(dit);

			const Ns now = Time_now();

			if (w->callbacks.onUpdate) {
				const F64 dt = w->lastUpdate ? (now - w->lastUpdate) / (F64)SECOND : 0;
				w->callbacks.onUpdate(w, dt);
			}

			w->lastUpdate = now;

			if(w->callbacks.onDraw)
				w->callbacks.onDraw(w);

			if (w->hint & EWindowHint_ProvideCPUBuffer)
				Window_presentPhysical(w, NULL);

			//Call update on all other windows and window manager

			if (w->flags & EWindowFlags_IsMoving)
				WindowManager_step(w->owner, w, NULL);

			return 0;
		}

		case WM_ENTERSIZEMOVE:
			w->flags |= EWindowFlags_IsMoving;
			break;

		case WM_EXITSIZEMOVE:
			w->flags &=~ EWindowFlags_IsMoving;
			return 0;

		case WM_GETMINMAXINFO: {

			MINMAXINFO *lpMMI = (MINMAXINFO*) lParam;
			const DWORD style = (DWORD) GetWindowLongPtrW(hwnd, GWL_STYLE);
			const DWORD exStyle = (DWORD) GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

			RECT minR = { 0, 0, I32x2_x(w->minSize), I32x2_y(w->minSize) };
			AdjustWindowRectEx(&minR, style, FALSE, exStyle);
			lpMMI->ptMinTrackSize.x = minR.right - minR.left;
			lpMMI->ptMinTrackSize.y = minR.bottom - minR.top;

			if (I32x2_any(w->maxSize)) {    // only if a hard maximum is set
				RECT maxR = { 0, 0, I32x2_x(w->maxSize), I32x2_y(w->maxSize) };
				AdjustWindowRectEx(&maxR, style, FALSE, exStyle);
				lpMMI->ptMaxTrackSize.x = maxR.right - maxR.left;
				lpMMI->ptMaxTrackSize.y = maxR.bottom - maxR.top;
			}

			break;
		}

		case WM_SIZE: {

			RECT r;
			GetClientRect(hwnd, &r);
			I32x2 newSize = I32x2_create2(r.right - r.left, r.bottom - r.top);

			Bool prevState = w->flags & EWindowFlags_IsMinimized;

			if (wParam == SIZE_MINIMIZED)
				w->flags |= EWindowFlags_IsMinimized;

			else w->flags &= ~EWindowFlags_IsMinimized;

			if (wParam == SIZE_MAXIMIZED)
				w->flags |= EWindowFlags_IsMaximized;

			else w->flags &= ~EWindowFlags_IsMaximized;

			Bool newState = w->flags & EWindowFlags_IsMinimized;
			Bool validSize = !I32x2_any(I32x2_leq(newSize, I32x2_zero));
			const Bool sizeChanged = validSize && !I32x2_eq2(w->size, newSize);
			const Bool stateChanged = prevState != newState;

			if (!sizeChanged && !stateChanged)
				break;

			Error err = Error_none();

			if (validSize) {

				w->size = newSize;

				if (!WWindow_initSize(w, w->size, &err))
					Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);
			}

			WWindow_updateMonitors(w);

			if (w->callbacks.onResize && !w->callbacks.onResize(w, &err))
				Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);

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

Bool WindowManager_supportsFormat(const WindowManager *manager, EWindowFormat format) {

	(void)manager;

	//TODO: HDR support; ColorSpace
	//    https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-getcontainingoutput
	//    https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_6/ns-dxgi1_6-dxgi_output_desc1

	return format == EWindowFormat_BGRA8;
}

void WindowManager_freePhysical(Window *w) {

	if(w->nativeData)
		DeleteObject((HGDIOBJ) w->nativeData);

	if(w->nativeHandle)
		DestroyWindow(w->nativeHandle);
}

Bool Window_updatePhysicalTitle(Window *w, CharString title, Error *e_rr) {

	Bool s_uccess = true;
	ListU16 name = (ListU16) { 0 };
	const U64 titlel = CharString_length(title);

	if(!w || !I32x2_any(w->size) || !title.ptr || !titlel || w->type != EWindowType_Physical)
		retError(clean, Error_nullPointer(
			!w || !I32x2_any(w->size) ? 0 : 1, "Window_updatePhysicalTitle()::w and title are required"
		));

	if (titlel >= 260)
		retError(clean, Error_outOfBounds(
			1, titlel, 260, "Window_updatePhysicalTitle()::title must be less than 260 characters"
		));

	gotoIfError3(clean, CharString_toUTF16(title, Platform_instance->alloc, &name, e_rr));

	if(!SetWindowTextW(w->nativeHandle, name.ptr))
		retError(clean, Error_platformError(0, GetLastError(), "Window_updatePhysicalTitle() SetWindowText failed"));

clean:
	ListU16_free(&name, Platform_instance->alloc);
	return s_uccess;
}

Bool Window_toggleFullScreen(Window *w, Error *e_rr) {

	Bool s_uccess = true;

	if(!w || !I32x2_any(w->size) || w->type != EWindowType_Physical)
		retError(clean, Error_nullPointer(!w || !I32x2_any(w->size) ? 0 : 1, "Window_toggleFullScreen()::w is required"));

	if(!(w->hint & EWindowHint_AllowFullscreen))
		retError(clean, Error_unsupportedOperation(
			0, "Window_toggleFullScreen() isn't allowed if EWindowHint_AllowFullscreen is off"
		));

	DWORD style = WS_VISIBLE;

	const Bool wasFullScreen = w->flags & EWindowFlags_IsFullscreen;

	w->flags &= ~EWindowFlags_IsFullscreen;

	if(!wasFullScreen) {
		w->flags |= EWindowFlags_IsFullscreen;
		w->prevSize = w->size;
	}

	const Bool isFullScreen = w->flags & EWindowFlags_IsFullscreen;
	const Bool borderless = w->hint & EWindowHint_NoBorder;

	if(!isFullScreen && !borderless) {

		style |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

		if(!(w->hint & EWindowHint_DisableResize))
			style |= WS_SIZEBOX | WS_MAXIMIZEBOX;
	}

	else style |= WS_POPUP;

	I32x2 newSize = I32x2_create2(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));

	SetWindowLongPtrW(w->nativeHandle, GWL_STYLE, style);

	if(!isFullScreen)
		newSize = w->prevSize;

	//Resize

	if (!I32x2_all(I32x2_eq(newSize, w->size)))
		SetWindowPos(
			w->nativeHandle, NULL,
			0, 0, I32x2_x(newSize), I32x2_y(newSize),
			SWP_SHOWWINDOW | SWP_FRAMECHANGED
		);

clean:
	return s_uccess;
}

Bool Window_presentPhysical(Window *w, Error *e_rr) {

	Bool s_uccess = true;
	PAINTSTRUCT ps;
	HDC oldBmp = NULL;
	U32 errId = 0;
	HDC hdc = 0;
	HDC hdcBmp = 0;

	if(!w || !I32x2_any(w->size))
		retError(clean, Error_nullPointer(0, "Window_presentPhysical()::w is required"));

	if(!(w->flags & EWindowFlags_IsActive) || !(w->hint & EWindowHint_ProvideCPUBuffer))
		retError(clean, Error_invalidOperation(0, "Window_presentPhysical() can only be called if there's a CPU-sided buffer"));

	hdc = BeginPaint(w->nativeHandle, &ps);

	if(!hdc)
		retError(clean, Error_platformError(0, GetLastError(), "Window_presentPhysical() BeginPaint failed"));

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

	//DWM reads alpha from the DIB directly during composition.
	//We must clear the destination to transparent black first so
	//GDI doesn't blend our pixels against whatever was there before.
	//The CPU buffer is already in premultiplied BGRA; the app is
	// responsible for premultiplying before writing to cpuVisibleBuffer.
	if(w->hint & EWindowHint_Transparency) {

		BLENDFUNCTION blend = {
			.BlendOp             = AC_SRC_OVER,
			.BlendFlags          = 0,
			.SourceConstantAlpha = 255,
			.AlphaFormat         = AC_SRC_ALPHA
		};

		if(!AlphaBlend(
			hdc, 0, 0, I32x2_x(w->size), I32x2_y(w->size),
			hdcBmp, 0, 0, I32x2_x(w->size), I32x2_y(w->size), blend
		)) {
			errId = 4;
			goto cleanup;
		}
	}

	else if(!BitBlt(hdc, 0, 0, I32x2_x(w->size), I32x2_y(w->size), hdcBmp, 0, 0, SRCCOPY)) {
		errId = 4;
		goto cleanup;
	}

cleanup:

	const DWORD res = GetLastError();

	if(oldBmp)
		SelectObject(hdc, oldBmp);

	if(hdcBmp)
		DeleteDC(hdcBmp);

	EndPaint(w->nativeHandle, &ps);

	if(errId)
		retError(clean, Error_platformError(errId, res, "Window_presentPhysical() failed in WinApi call"));

clean:
	return s_uccess;
}

Bool WindowManager_createWindowPhysical(Window *w, Error *e_rr) {

	//Create native window

	const WNDCLASSEXW wc = *(const WNDCLASSEXW*) w->owner->platformData.ptr;
	const HINSTANCE mainModule = Platform_instance->data;
	HWND nativeWindow = NULL;
	Bool touched = false;
	Bool s_uccess = true;
	Keyboard builtinKeyboard = (Keyboard) { 0 };
	Mouse builtinMouse = (Mouse) { 0 };

	DWORD style = WS_VISIBLE;

	const Bool isFullScreen = w->hint & EWindowHint_ForceFullscreen;
	const Bool borderless = w->hint & EWindowHint_NoBorder;

	if(!isFullScreen && !borderless) {

		style |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

		if(!(w->hint & EWindowHint_DisableResize))
			style |= WS_SIZEBOX | WS_MAXIMIZEBOX;
	}

	else style |= WS_POPUP;

	const I32x2 maxSize = I32x2_create2(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));

	I32x2 size = w->size;
	const I32x2 position = w->offset;

	for (U8 i = 0; i < 2; ++i)
		if (isFullScreen || (!I32x2_get(size, i) || I32x2_get(size, i) >= I32x2_get(maxSize, i)))
			I32x2_setRef(&size, i, I32x2_get(maxSize, i));

	//Our strings are UTF8, but windows wants UTF16

	ListU16 tmp = (ListU16) { 0 };
	gotoIfError3(clean, CharString_toUTF16(w->title, Platform_instance->alloc, &tmp, e_rr));

	RECT adjRect = { 0, 0, I32x2_x(size), I32x2_y(size) };
	AdjustWindowRectEx(&adjRect, style, FALSE, WS_EX_APPWINDOW);

	nativeWindow = CreateWindowExW(
		WS_EX_APPWINDOW, wc.lpszClassName, (const wchar_t*) tmp.ptr, style,
		I32x2_x(position), I32x2_y(position),
		adjRect.right - adjRect.left,
		adjRect.bottom - adjRect.top,
		NULL, NULL, mainModule, NULL
	);

	if(!nativeWindow)
		retError(clean, Error_platformError(1, GetLastError(), "WindowManager_createWindowPhysical() CreateWindowEx failed"));

	//Get real size and position

	RECT r = (RECT) { 0 };

	if(!GetClientRect(nativeWindow, &r))
		retError(clean, Error_platformError(1, GetLastError(), "WindowManager_createWindowPhysical() GetClientRect failed"));

	w->size = I32x2_create2(r.right - r.left, r.bottom - r.top);

	if(!GetWindowRect(nativeWindow, &r))
		retError(clean, Error_platformError(1, GetLastError(), "WindowManager_createWindowPhysical() GetWindowRect failed"));

	w->offset = I32x2_create2(r.left, r.top);

	//Alloc cpu visible buffer if needed

	gotoIfError3(clean, WWindow_initSize(w, w->size, e_rr));

	//Lock for when we are updating this window

	gotoIfError3(clean, ListInputDevice_reserve(&w->devices, 16, Platform_instance->alloc, e_rr));
	gotoIfError3(clean, ListMonitor_reserve(&w->monitors, 16, Platform_instance->alloc, e_rr));

	w->defaultKeyboardId = (U32) w->devices.length;
	gotoIfError3(clean, Keyboard_create(&builtinKeyboard, Platform_instance->alloc, e_rr));
	gotoIfError3(clean, ListInputDevice_pushBack(&w->devices, builtinKeyboard, Platform_instance->alloc, e_rr));
	builtinKeyboard = (Keyboard) { 0 };

	w->defaultMouseId = (U32)w->devices.length;
	gotoIfError3(clean, Mouse_create(&builtinMouse, Platform_instance->alloc, e_rr));
	gotoIfError3(clean, ListInputDevice_pushBack(&w->devices, builtinMouse, Platform_instance->alloc, e_rr));
	builtinMouse = (Mouse) { 0 };

	w->nativeHandle = nativeWindow;
	touched = true;

	//Bind our window

	SetWindowLongPtrW(nativeWindow, 0, (LONG_PTR) w);

	if(w->hint & EWindowHint_ForceFullscreen)
		w->flags |= EWindowFlags_IsFullscreen;

	//Extend DWM frame to cover the entire client area.
	//This makes the compositor respect per-pixel alpha from our DIB.
	if(w->hint & EWindowHint_Transparency) {

		MARGINS margins = { -1, -1, -1, -1 };

		if(FAILED(DwmExtendFrameIntoClientArea(nativeWindow, &margins))) {
			Log_warnLnx("WindowManager_createWindowPhysical(): DwmExtendFrameIntoClientArea failed, transparency unavailable");
			w->hint &= ~EWindowHint_Transparency;
		}
	}

	//Ensure we get all input devices
	//Register for raw input of these types
	//https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/hid-usages#usage-page

	const RAWINPUTDEVICE registerDevices[2] = {
		{                                    //Keyboard
			.dwFlags = RIDEV_DEVNOTIFY,
			.usUsagePage = 1,
			.usUsage = 6,
			.hwndTarget = nativeWindow
		},
		{                                    //Mouse
			.dwFlags = RIDEV_DEVNOTIFY,
			.usUsagePage = 1,
			.usUsage = 2,
			.hwndTarget = nativeWindow
		}
	};

	if (!RegisterRawInputDevices(registerDevices, 2, sizeof(registerDevices[0])))
		retError(clean, Error_invalidState(0, "Window_physicalLoop() RegisterRawInputDevices failed"));

	//Finalize
	
	w->flags |= EWindowFlags_IsActive;

	WWindow_updateMonitors(w);

	if(w->callbacks.onCreate)
		gotoIfError3(clean, w->callbacks.onCreate(w, e_rr));
		
	w->flags |= EWindowFlags_IsFinalized;

	if(w->callbacks.onResize)
		gotoIfError3(clean, w->callbacks.onResize(w, e_rr));

clean:

	if(!s_uccess) {

		if((w->flags & EWindowFlags_IsActive) && w->callbacks.onDestroy)
			w->callbacks.onDestroy(w);

		ListInputDevice_free(&w->devices, Platform_instance->alloc);
		ListMonitor_free(&w->monitors, Platform_instance->alloc);

		if (w->nativeData) {
			DeleteObject((HGDIOBJ) w->nativeData);
			w->nativeData = NULL;
		}

		if(touched && w->nativeHandle) {
			DestroyWindow(w->nativeHandle);
			w->nativeHandle = NULL;
		}

		if(!touched && nativeWindow)
			DestroyWindow(nativeWindow);
	}

	InputDevice_free(&builtinKeyboard, Platform_instance->alloc);
	InputDevice_free(&builtinMouse, Platform_instance->alloc);
	ListU16_free(&tmp, Platform_instance->alloc);
	return s_uccess;
}
