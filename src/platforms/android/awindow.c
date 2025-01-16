/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "platforms/ext/bufferx.h"
#include "platforms/log.h"
#include "platforms/window.h"
#include "platforms/window_manager.h"
#include "platforms/keyboard.h"
#include "platforms/mouse.h"

#include <android_native_app_glue.h>
#include <android/configuration.h>

void AWindow_onUpdateSize(Window *w) {

	if(
		(w->hint & EWindowHint_ProvideCPUBuffer) &&
		!Buffer_resizex(&w->cpuVisibleBuffer, 4 * I32x2_x(w->size) * I32x2_y(w->size), false, true, NULL)
	) {
		Log_debugLnx("AWindow_onUpdateSize() couldn't update cpuVisibleBuffer size, removing the provide cpu buffer flag");
		Buffer_freex(&w->cpuVisibleBuffer);
		w->hint &=~ EWindowHint_ProvideCPUBuffer;
	}
	
	//TODO: Query monitors

	if (w->callbacks.onMonitorChange)
		w->callbacks.onMonitorChange(w);

	if (w->callbacks.onResize)
		w->callbacks.onResize(w);
}

EWindowOrientation AWindow_getOrientation(struct android_app *app) {
	
	AConfiguration *config = AConfiguration_new();
	AConfiguration_fromAssetManager(config, app->activity->assetManager);

	I32 orientation = AConfiguration_getOrientation(config);
	AConfiguration_delete(config);

	return
		orientation == ACONFIGURATION_ORIENTATION_PORT ? EWindowOrientation_Portrait : (
			orientation == ACONFIGURATION_ORIENTATION_SQUARE ? EWindowOrientation_Square : EWindowOrientation_Landscape
		);
}

void AWindow_onAppCmd(struct android_app *app, I32 cmd){

	Window *w = (Window*) app->userData;

	if(!w)
		return;

	if((w->flags & EWindowFlags_IsFinalized) || cmd == APP_CMD_INIT_WINDOW)
		switch (cmd) {

			case APP_CMD_WINDOW_RESIZED: {

				I32x2 oldSize = w->size;
				w->size = I32x2_create2(ANativeWindow_getWidth(app->window), ANativeWindow_getHeight(app->window));

				if (I32x2_neq2(w->size, oldSize))
					AWindow_onUpdateSize(w);

				break;
			}

			case APP_CMD_CONTENT_RECT_CHANGED: {

				I32x2 oldOffset = w->offset;
				w->offset = I32x2_create2((I32) app->contentRect.left, (I32) app->contentRect.top);

				if (I32x2_neq2(oldOffset, w->offset) && w->callbacks.onWindowMove)
					w->callbacks.onWindowMove(w);

				break;
			}

			case APP_CMD_GAINED_FOCUS:

				w->flags |= EWindowFlags_IsFocussed;

				if (w->callbacks.onUpdateFocus)
					w->callbacks.onUpdateFocus(w);

				break;

			case APP_CMD_LOST_FOCUS:

				w->flags &= ~EWindowFlags_IsFocussed;

				if (w->callbacks.onUpdateFocus)
					w->callbacks.onUpdateFocus(w);

				break;

			case APP_CMD_RESUME:
				
				if (app->savedState && w->callbacks.onLoad) {
					Buffer buf = Buffer_createManagedPtr(app->savedState, app->savedStateSize);
					w->callbacks.onLoad(w, buf);
					Buffer_freex(&buf);
				}

				break;

			case APP_CMD_TERM_WINDOW:
				w->flags |= EWindowFlags_ShouldTerminate;
				break;
			
			case APP_CMD_SAVE_STATE:
		
				if (w->callbacks.onSave) {

					if (app->savedState) {
						Buffer buf = Buffer_createManagedPtr(app->savedState, app->savedStateSize);
						Buffer_freex(&buf);
					}

					Buffer buf = Buffer_createNull();
					w->callbacks.onSave(w, &buf);
					app->savedState = buf.ptrNonConst;
					app->savedStateSize = Buffer_length(buf);
				}
			
				break;
			
			case APP_CMD_INIT_WINDOW:

				w->size = I32x2_create2(ANativeWindow_getWidth(app->window), ANativeWindow_getHeight(app->window));
				w->orientation = AWindow_getOrientation(app);
				AWindow_onUpdateSize(w);

				if (w->callbacks.onUpdateOrientation)
					w->callbacks.onUpdateOrientation(w);

				w->flags |= EWindowFlags_IsFinalized;
				break;

			//On config change can be a lot of things, we only care about orientation for now.
			//We will later care about:
			//keyboardHidden|screenLayout|fontScale|locale
			//Since these might impact language, text rendering or text input

			case APP_CMD_CONFIG_CHANGED: {

				EWindowOrientation old = w->orientation;
				w->orientation = AWindow_getOrientation(app);

				if (w->orientation != old && w->callbacks.onUpdateOrientation)
					w->callbacks.onUpdateOrientation(w);

				break;
			}

			default:
				break;
		}
}

//TODO: Controller https://developer.android.com/games/sdk/game-controller

EKey AWindow_mapKey(I32 keyCode) {

	switch(keyCode) {

		default:
			return EKey_Count;

		case AKEYCODE_0: case AKEYCODE_1: case AKEYCODE_2: case AKEYCODE_3: case AKEYCODE_4:
		case AKEYCODE_5: case AKEYCODE_6: case AKEYCODE_7: case AKEYCODE_8: case AKEYCODE_9:
			return EKey_0 + (keyCode - AKEYCODE_0);
		
		case AKEYCODE_NUMPAD_0: case AKEYCODE_NUMPAD_1: case AKEYCODE_NUMPAD_2: case AKEYCODE_NUMPAD_3: case AKEYCODE_NUMPAD_4:
		case AKEYCODE_NUMPAD_5: case AKEYCODE_NUMPAD_6: case AKEYCODE_NUMPAD_7: case AKEYCODE_NUMPAD_8: case AKEYCODE_NUMPAD_9:
			return EKey_Numpad0 + (keyCode - AKEYCODE_NUMPAD_0);

		case AKEYCODE_A: case AKEYCODE_B: case AKEYCODE_C: case AKEYCODE_D: case AKEYCODE_E:
		case AKEYCODE_F: case AKEYCODE_G: case AKEYCODE_H: case AKEYCODE_I: case AKEYCODE_J:
		case AKEYCODE_K: case AKEYCODE_L: case AKEYCODE_M: case AKEYCODE_N: case AKEYCODE_O:
		case AKEYCODE_P: case AKEYCODE_Q: case AKEYCODE_R: case AKEYCODE_S: case AKEYCODE_T:
		case AKEYCODE_U: case AKEYCODE_V: case AKEYCODE_W: case AKEYCODE_X: case AKEYCODE_Y:
		case AKEYCODE_Z:
			return EKey_A + (keyCode - AKEYCODE_A);

		case AKEYCODE_F1: case AKEYCODE_F2: case AKEYCODE_F3: case AKEYCODE_F4: case AKEYCODE_F5: case AKEYCODE_F6:
		case AKEYCODE_F7: case AKEYCODE_F8: case AKEYCODE_F9: case AKEYCODE_F10: case AKEYCODE_F11: case AKEYCODE_F12:
			return EKey_F1 + (keyCode - AKEYCODE_F1);

		case AKEYCODE_SCREENSHOT:		return EKey_PrintScreen;
		case AKEYCODE_DEL:				return EKey_Backspace;
		case AKEYCODE_SPACE:			return EKey_Space;
		case AKEYCODE_TAB:				return EKey_Tab;

		case AKEYCODE_SHIFT_LEFT:		return EKey_LShift;
		case AKEYCODE_SHIFT_RIGHT:		return EKey_RShift;
		case AKEYCODE_ALT_LEFT:			return EKey_LAlt;
		case AKEYCODE_ALT_RIGHT:		return EKey_RAlt;
		case AKEYCODE_CTRL_LEFT:		return EKey_LCtrl;
		case AKEYCODE_CTRL_RIGHT:		return EKey_RCtrl;
		case AKEYCODE_META_LEFT:		return EKey_LMenu;
		case AKEYCODE_META_RIGHT:		return EKey_RMenu;

		case AKEYCODE_CAPS_LOCK:		return EKey_Caps;
		case AKEYCODE_SCROLL_LOCK:		return EKey_ScrollLock;
		case AKEYCODE_NUM_LOCK:			return EKey_NumLock;

		case AKEYCODE_COMMA:			return EKey_Comma;
		case AKEYCODE_MINUS:			return EKey_Minus;
		case AKEYCODE_EQUALS:			return EKey_Equals;
		case AKEYCODE_PERIOD:			return EKey_Period;
		case AKEYCODE_REFRESH:			return EKey_Refresh;
		case AKEYCODE_MOVE_HOME:		return EKey_Home;
		case AKEYCODE_MOVE_END:		 	return EKey_End;
		case AKEYCODE_ENTER:			return EKey_Enter;
		case AKEYCODE_GRAVE:			return EKey_Backtick;
		case AKEYCODE_BACKSLASH:		return EKey_Backslash;
		case AKEYCODE_SEMICOLON:		return EKey_Semicolon;
		case AKEYCODE_APOSTROPHE:		return EKey_Quote;
		case AKEYCODE_SLASH:			return EKey_Slash;

		case AKEYCODE_LEFT_BRACKET:		return EKey_LBracket;
		case AKEYCODE_RIGHT_BRACKET:	return EKey_RBracket;

		case AKEYCODE_NUMPAD_DIVIDE:	return EKey_NumpadDiv;
		case AKEYCODE_NUMPAD_MULTIPLY:  return EKey_NumpadMul;
		case AKEYCODE_NUMPAD_SUBTRACT:  return EKey_NumpadSub;
		case AKEYCODE_NUMPAD_ADD:		return EKey_NumpadAdd;

		case AKEYCODE_NUMPAD_DOT:		return EKey_NumpadDot;
		case AKEYCODE_NUMPAD_COMMA:		return EKey_NumpadDot;

		case AKEYCODE_VOLUME_UP:		return EKey_VolumeUp;
		case AKEYCODE_VOLUME_DOWN:		return EKey_VolumeDown;

		case AKEYCODE_PAGE_UP:			return EKey_PageUp;
		case AKEYCODE_PAGE_DOWN:		return EKey_PageDown;
		
		case AKEYCODE_VOLUME_MUTE:		return EKey_Mute;
		case AKEYCODE_CLEAR:			return EKey_Clear;
		case AKEYCODE_ESCAPE:			return EKey_Escape;
		case AKEYCODE_INSERT:			return EKey_Insert;
		case AKEYCODE_MEDIA_PLAY_PAUSE: return EKey_Pause;
		case AKEYCODE_FORWARD_DEL:		return EKey_Delete;

		case AKEYCODE_SLEEP:			return EKey_Sleep;
		case AKEYCODE_HELP:				return EKey_Help;
		case AKEYCODE_SEARCH:			return EKey_Search;
		case AKEYCODE_MENU:			 	return EKey_Options;

		case AKEYCODE_DPAD_DOWN:		return EKey_Down;
		case AKEYCODE_DPAD_UP:			return EKey_Up;
		case AKEYCODE_DPAD_LEFT:		return EKey_Left;
		case AKEYCODE_DPAD_RIGHT:		return EKey_Right;

		case AKEYCODE_BACK:			 	return EKey_Back;
		case AKEYCODE_FORWARD:		 	return EKey_Forward;

		case AKEYCODE_MEDIA_NEXT:		return EKey_Skip;
		case AKEYCODE_MEDIA_PREVIOUS:	return EKey_Previous;
	}
}

I32 AWindow_onInput(struct android_app *app, AInputEvent *event){

	Window *w = (Window*) app->userData;

	if(!w || !(w->flags & EWindowFlags_IsFinalized) || (w->flags & EWindowFlags_ShouldTerminate))
		return 0;
	
	Keyboard *keyboard = w->devices.ptrNonConst;
	Mouse *mouse = w->devices.ptrNonConst + 1;

	Bool isDown = AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_DOWN;
	
	I32 type = AInputEvent_getType(event);
	I32 source = AInputEvent_getSource(event);
	
	switch(type) {

		case AINPUT_EVENT_TYPE_MOTION: {

			I32 action = AMotionEvent_getAction(event);
			
			if (source & (AINPUT_SOURCE_TOUCHSCREEN | AINPUT_SOURCE_MOUSE)) {

				InputDevice *dev = mouse;
				
				if (action == AMOTION_EVENT_ACTION_MOVE || action == AMOTION_EVENT_ACTION_HOVER_MOVE) {
					
					F32 x = AMotionEvent_getXOffset(event);
					F32 y = AMotionEvent_getYOffset(event);

					F32x2 pos = F32x2_div(F32x2_create2(x, y), F32x2_fromI32x2(w->size));

					F32 nextX = F32x2_x(pos);
					F32 nextY = 1 - F32x2_y(pos);

					F32 prevX = InputDevice_getCurrentAxis(*dev, EMouseAxis_X);
					F32 prevY = InputDevice_getCurrentAxis(*dev, EMouseAxis_Y);

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
					
				} else if (action == AMOTION_EVENT_ACTION_SCROLL) {

					F32 nextX = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HSCROLL, 0);
					F32 nextY = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_VSCROLL, 0);

					F32 prevX = InputDevice_getCurrentAxis(*dev, EMouseAxis_ScrollWheel_X);
					F32 prevY = InputDevice_getCurrentAxis(*dev, EMouseAxis_ScrollWheel_Y);

					if (nextX != prevX) {

						InputDevice_setCurrentAxis(*dev, EMouseAxis_ScrollWheel_X, nextX);

						if (w->callbacks.onDeviceAxis)
							w->callbacks.onDeviceAxis(w, dev, EMouseAxis_ScrollWheel_X, nextX);
					}

					if (nextY != prevY) {

						InputDevice_setCurrentAxis(*dev, EMouseAxis_ScrollWheel_Y, nextY);

						if (w->callbacks.onDeviceAxis)
							w->callbacks.onDeviceAxis(w, dev, EMouseAxis_ScrollWheel_Y, nextY);
					}

				} else if(
					action == AMOTION_EVENT_ACTION_POINTER_UP || 
					action == AMOTION_EVENT_ACTION_UP
				) {

					I32 buttonState = AMotionEvent_getButtonState(event);

					Bool primary = buttonState & AMOTION_EVENT_BUTTON_PRIMARY;
					Bool secondary = buttonState & AMOTION_EVENT_BUTTON_SECONDARY;
					Bool tertiary = buttonState & AMOTION_EVENT_BUTTON_TERTIARY;
					Bool forward = buttonState & AMOTION_EVENT_BUTTON_FORWARD;
					Bool backward = buttonState & AMOTION_EVENT_BUTTON_BACK;

					Bool states[5] = { primary, secondary, tertiary, forward, backward };
					EMouseActions buttons[5] = { EMouseButton_Left, EMouseButton_Right, EMouseButton_Middle, EMouseButton_Forward, EMouseButton_Back };
					
					for(U64 i = 0; i < 5; ++i) {
						
						//Send keys through interface and update input device

						InputHandle handle = buttons[i];
						EInputState prevState = InputDevice_getState(*dev, handle);

						InputDevice_setCurrentState(*dev, handle, states[i]);
						EInputState newState = InputDevice_getState(*dev, handle);

						if(prevState != newState && w->callbacks.onDeviceButton)
							w->callbacks.onDeviceButton(w, dev, handle, states[i]);
					}
				}
			}

			return 0;
		}

		case AINPUT_EVENT_TYPE_KEY: {

			//TODO: Android, send unicode events :(

			I32 keyCode = AKeyEvent_getKeyCode(event);
			EKey mappedKey = AWindow_mapKey(keyCode);
			
			if (mappedKey != EKey_Count) {

				InputDevice *dev = keyboard;
        
				U32 flags = 0;		//TODO: Caps, numlock, scroll lock
				
				dev->flags = flags;

				//Send keys through interface and update input device

				InputHandle handle = (InputHandle) mappedKey;
				EInputState prevState = InputDevice_getState(*dev, handle);

				InputDevice_setCurrentState(*dev, handle, isDown);
				EInputState newState = InputDevice_getState(*dev, handle);

				if(prevState != newState && w->callbacks.onDeviceButton)
					w->callbacks.onDeviceButton(w, dev, handle, isDown);
					
				return 1;
			}

			return 0;
		}

		default:
			return 0;
	}
}

Bool WindowManager_supportsFormat(const WindowManager *manager, EWindowFormat format) {
	(void) manager;
	return format == EWindowFormat_BGRA8;	//TODO: HDR
}

extern struct android_app *AWindowManager_instance;

Bool WindowManager_freePhysical(Window *w) {
	(void) w;
	AWindowManager_instance->userData = NULL;
	AWindowManager_instance->onAppCmd = NULL;
	AWindowManager_instance->onInputEvent = NULL;
	return true;
}

Bool Window_updatePhysicalTitle(const Window *w, CharString title, Error *e_rr) {

	Bool s_uccess = true;

	if(!w || !I32x2_any(w->size) || !title.ptr || !CharString_length(title) || w->type != EWindowType_Physical)
		retError(clean, Error_nullPointer(
			!w || !I32x2_any(w->size) ? 0 : 1, "Window_updatePhysicalTitle()::w and title are required"
		))

	//Title of the window is handled by activity name instead

clean:
	return s_uccess;
}

Bool Window_toggleFullScreen(Window *w, Error *e_rr) {

	Bool s_uccess = true;

	if(!w || !I32x2_any(w->size) || w->type != EWindowType_Physical)
		retError(clean, Error_nullPointer(!w || !I32x2_any(w->size) ? 0 : 1, "Window_toggleFullScreen()::w is required"))

	if(!(w->hint & EWindowHint_AllowFullscreen))
		retError(clean, Error_unsupportedOperation(0, "Window_toggleFullScreen() isn't allowed if EWindowHint_AllowFullscreen is off"))

	//Fullscreen is a no-op, this is handled by the OS, not our app

clean:
	return s_uccess;
}

Bool Window_presentPhysical(Window *w, Error *e_rr) {

	Bool s_uccess = true;

	if(!w || !I32x2_any(w->size))
		retError(clean, Error_nullPointer(0, "Window_presentPhysical()::w is required"))

	if(!(w->flags & EWindowFlags_IsActive) || !(w->hint & EWindowHint_ProvideCPUBuffer))
		retError(clean, Error_invalidOperation(0, "Window_presentPhysical() can only be called if there's a CPU-sided buffer"))

	ANativeWindow *nativeWindow = AWindowManager_instance->window;

	ANativeWindow_Buffer buffer = (ANativeWindow_Buffer) { 0 };
	ARect rect = (ARect) { .right = I32x2_x(w->size), .bottom = I32x2_y(w->size) };

	if (ANativeWindow_lock(nativeWindow, &buffer, &rect))
		retError(clean, Error_invalidState(0, "Window_presentPhysical() couldn't lock window"))

	enum AHardwareBuffer_Format format[2] = { 0 };
	U8 formats = 0;

	switch(w->format) {

		case EWindowFormat_BGR10A2:
			format[formats++] = AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
			break;

		case EWindowFormat_RGBA16f:
			format[formats++] = AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
			break;

		default:
			format[formats++] = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
			format[formats++] = AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
			break;
	}

	Bool supported = false;

	for(U64 i = 0; i < formats; ++i)
		if((enum AHardwareBuffer_Format)buffer.format == format[i]) {
			supported = true;
			break;
		}

	if(!supported) {
		ANativeWindow_unlockAndPost(nativeWindow);
		retError(clean, Error_invalidState(0, "Window_presentPhysical() couldn't write to window; mismatching formats"))
	}

	if(buffer.width != I32x2_x(w->size) ||  buffer.height != I32x2_y(w->size)) {
		ANativeWindow_unlockAndPost(nativeWindow);
		retError(clean, Error_invalidState(0, "Window_presentPhysical() couldn't write to window; mismatching dimensions"))
	}

	Buffer_memcpy(Buffer_createRef(buffer.bits, Buffer_length(w->cpuVisibleBuffer)), w->cpuVisibleBuffer);
	ANativeWindow_unlockAndPost(nativeWindow);

clean:
	return s_uccess;
}

Bool WindowManager_createWindowPhysical(Window *w, Error *e_rr) {

	Bool s_uccess = true;

	for(U64 i = 0; i < w->owner->windows.length; ++i)
		if(w->owner->windows.ptr[i]->type == EWindowType_Physical)
			retError(clean, Error_invalidState(0, "WindowManager_createWindow() there can be only one window on Android"))

	if(w->format == EWindowFormat_RGBA32f)
		retError(clean, Error_invalidState(0, "WindowManager_createWindow() RGBA32f format not natively supported on Android"))

	gotoIfError2(clean, ListMonitor_reservex(&w->monitors, 16))

	gotoIfError2(clean, ListInputDevice_resizex(&w->devices, 2))
	gotoIfError2(clean, Keyboard_create(w->devices.ptrNonConst))
	gotoIfError2(clean, Mouse_create(w->devices.ptrNonConst + 1))

	AWindowManager_instance->userData = w;
	AWindowManager_instance->onAppCmd = AWindow_onAppCmd;
	AWindowManager_instance->onInputEvent = AWindow_onInput;

clean:
	return s_uccess;
}
