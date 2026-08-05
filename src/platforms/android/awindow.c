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

//platforms/android/awindow.c

#include "platforms/logx.h"
#include "platforms/window.h"
#include "platforms/window_manager.h"
#include "platforms/keyboard.h"
#include "platforms/mouse.h"
#include "platforms/platform.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/container/string_unicode.h"
#include "types/base/atomic.h"
#include "types/base/lock.h"
#include "types/base/mathf.h"

#include <android_native_app_glue.h>
#include <android/configuration.h>

U32 Window_extSize = 0;

//awindow_manager.c owns the display query, since that's where WindowManager_updateMonitors needs it too

Bool AWindowManager_getMonitor(Monitor *monitor);

void AWindow_onUpdateSize(Window *w) {

	Error err = Error_none();

	//Window_resizeCPUBuffer is a no-op here; it early outs when w->size already matches (the caller assigns it before calling us)
	// and it can't be told what the OS-side stride is.
	//Android composites through a plain heap buffer that Window_presentPhysical memcpy's into the
	// locked ANativeWindow, so size it directly.

	if(w->hint & EWindowHint_ProvideCPUBuffer) {

		const U64 linSiz = ETextureFormat_getSize(
			(ETextureFormat) w->format, I32x2_x(w->size), I32x2_y(w->size), 1
		);

		if(!linSiz || !Buffer_resize(&w->cpuVisibleBuffer, linSiz, false, true, Platform_instance->alloc, &err)) {

			Log_debugLnx(
				"AWindow_onUpdateSize() couldn't update cpuVisibleBuffer size, removing the provide cpu buffer flag"
			);

			Buffer_free(&w->cpuVisibleBuffer, Platform_instance->alloc);
			w->hint &=~ EWindowHint_ProvideCPUBuffer;
		}
	}

	//A window always covers the whole display here, so its monitor list is the display's single entry.
	//monitorsDirty is what gets the manager's own list refreshed on the next step, the way the other backends do it
	// from their monitor change handlers.

	Monitor monitor = (Monitor) { 0 };
	ListMonitor_clear(&w->monitors, NULL);

	if(AWindowManager_getMonitor(&monitor))
		if(!ListMonitor_pushBack(&w->monitors, monitor, Platform_instance->alloc, &err))
			Log_debugLnx("AWindow_onUpdateSize() couldn't store the monitor");

	if(w->owner)
		w->owner->monitorsDirty = true;

	if (w->callbacks.onMonitorChange)
		w->callbacks.onMonitorChange(w);

	if (w->callbacks.onResize && !w->callbacks.onResize(w, &err))
		Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);

	if (w->callbacks.onUpdateOrientation)
		w->callbacks.onUpdateOrientation(w);

	w->requireResize = false;
}

I32 APlatform_getDeviceOrientation() {

	struct android_app *app = (struct android_app*)Platform_instance->data;
	JavaVM *vm = app->activity->vm;
	JNIEnv *env = app->activity->env;

	Bool attached = false;
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;
	I32 orientation = -1;

	if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
		(*vm)->AttachCurrentThread(vm, &env, NULL);
		attached = true;
	}

	jclass cls = (*env)->GetObjectClass(env, app->activity->clazz);

	if(!cls)
		retError(clean, Error_invalidState(0, "Couldn't find OxC3Activity"));

	jmethodID methodId = (*env)->GetMethodID(env, cls, "getDeviceOrientation", "()I");

	if (!methodId)
		retError(clean, Error_invalidState(0, "Couldn't find OxC3Activity.getDeviceOrientation"));

	orientation = (*env)->CallIntMethod(env, app->activity->clazz, methodId);

clean:

	if(cls)
		(*env)->DeleteLocalRef(env, cls);

	if(!s_uccess)
		Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);

	if(attached)
		(*vm)->DetachCurrentThread(vm);

	return orientation;
}

//Unicode a physical key produces, via OxC3Activity.getKeyUnicode -> KeyCharacterMap.
//The NDK stops at the raw keycode (AKeyEvent_* has no unicode accessor), so this has to go through JNI.
//Returns 0 when the key produces nothing printable.

I32 APlatform_getKeyUnicode(I32 keyCode, I32 metaState, I32 deviceId) {

	struct android_app *app = (struct android_app*)Platform_instance->data;
	JavaVM *vm = app->activity->vm;
	JNIEnv *env = app->activity->env;

	Bool attached = false;
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;
	I32 unicode = 0;

	if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
		(*vm)->AttachCurrentThread(vm, &env, NULL);
		attached = true;
	}

	jclass cls = (*env)->GetObjectClass(env, app->activity->clazz);

	if(!cls)
		retError(clean, Error_invalidState(0, "Couldn't find OxC3Activity"));

	jmethodID methodId = (*env)->GetMethodID(env, cls, "getKeyUnicode", "(III)I");

	if (!methodId)
		retError(clean, Error_invalidState(0, "Couldn't find OxC3Activity.getKeyUnicode"));

	unicode = (*env)->CallIntMethod(env, app->activity->clazz, methodId, keyCode, metaState, deviceId);

clean:

	if(cls)
		(*env)->DeleteLocalRef(env, cls);

	if(!s_uccess)
		Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);

	if(attached)
		(*vm)->DetachCurrentThread(vm);

	return unicode;
}

//Refresh rate of the display, via OxC3Activity.getRefreshRate.
//The NDK only has ANativeWindow_setFrameRate (a setter, API 30+), so this needs JNI too.

F32 APlatform_getRefreshRate() {

	struct android_app *app = (struct android_app*)Platform_instance->data;
	JavaVM *vm = app->activity->vm;
	JNIEnv *env = app->activity->env;

	Bool attached = false;
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;
	F32 refreshRate = 0;

	if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
		(*vm)->AttachCurrentThread(vm, &env, NULL);
		attached = true;
	}

	jclass cls = (*env)->GetObjectClass(env, app->activity->clazz);

	if(!cls)
		retError(clean, Error_invalidState(0, "Couldn't find OxC3Activity"));

	jmethodID methodId = (*env)->GetMethodID(env, cls, "getRefreshRate", "()F");

	if (!methodId)
		retError(clean, Error_invalidState(0, "Couldn't find OxC3Activity.getRefreshRate"));

	refreshRate = (F32) (*env)->CallFloatMethod(env, app->activity->clazz, methodId);

clean:

	if(cls)
		(*env)->DeleteLocalRef(env, cls);

	if(!s_uccess)
		Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);

	if(attached)
		(*vm)->DetachCurrentThread(vm);

	return refreshRate;
}

//Soft keyboard text arrives on the android UI thread (OxC3Activity's TextWatcher), but every other
//callback runs on the app thread. So the JNI side only queues, and AWindow_flushTypeChar drains it
//from WindowManager_updateExt, where the rest of the input pump already runs.

static SpinLock AWindow_typeCharLock = { 0 };
static ListCharString AWindow_typeCharQueue = { 0 };

//Called from the JNI export in include/platforms/android/aoxc3_activity_glue.c, which is where the
//OxC3Activity.onTypeChar binding lives (consumers compile that file into their own .so).
//UTF-16 in, because GetStringUTFChars hands back modified UTF-8, which encodes anything outside the
//BMP (emoji, which a soft keyboard produces readily) as CESU-8 surrogate pairs our reader would reject.

void AWindow_queueTypeChar(const U16 *utf16, U64 len) {

	if(!utf16 || !len || !Platform_instance)
		return;

	CharString str = CharString_createNull();
	Error err = Error_none();

	if(CharString_createFromUTF16(utf16, len, Platform_instance->alloc, &str, &err)) {

		const ELockAcquire acq = SpinLock_lock(&AWindow_typeCharLock, U64_MAX);

		if(acq >= ELockAcquire_Success) {

			if(ListCharString_pushBack(&AWindow_typeCharQueue, str, Platform_instance->alloc, &err))
				str = CharString_createNull();        //The queue owns it now

			if(acq == ELockAcquire_Acquired)
				SpinLock_unlock(&AWindow_typeCharLock);
		}
	}

	CharString_free(&str, Platform_instance->alloc);
}

//Called on the app thread. Swaps the queue out under the lock so onTypeChar runs unlocked and the UI
//thread never blocks on user code. Drains even without a window, so nothing accumulates forever.

void AWindow_flushTypeChar(Window *w) {

	//Move the queue into a local and hand the global a fresh empty one; the struct copy carries
	//capacityAndRefInfo along, so the local owns the buffer and the global no longer refers to it.

	ListCharString queue = (ListCharString) { 0 };
	const ELockAcquire acq = SpinLock_lock(&AWindow_typeCharLock, U64_MAX);

	if(acq >= ELockAcquire_Success) {

		queue = AWindow_typeCharQueue;
		AWindow_typeCharQueue = (ListCharString) { 0 };

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(&AWindow_typeCharLock);
	}

	for(U64 i = 0; i < queue.length; ++i)
		if(w && w->callbacks.onTypeChar && (w->flags & EWindowFlags_IsFinalized))
			w->callbacks.onTypeChar(w, queue.ptr[i]);

	ListCharString_freeUnderlying(&queue, Platform_instance->alloc);
}

//Bind a window to the surface that's already up.
//Normally driven by APP_CMD_INIT_WINDOW, but android only sends that when the surface itself appears, so a window
// created while one is already alive (the previous one having been freed) would never hear it, and
// WindowManager_updateExt would spin waiting to be finalized.
//WindowManager_createWindowPhysical calls this directly in that case.

void AWindow_finalize(Window *w) {

	struct android_app *app = (struct android_app*) Platform_instance->data;

	if(!app || !app->window || (w->flags & EWindowFlags_IsFinalized))
		return;

	w->nativeHandle = app->window;
	w->flags |= EWindowFlags_IsFinalized;

	//A NativeActivity surface keeps whatever format the system handed it until asked otherwise, and that's regularly
	// one ANativeWindow_lock won't produce, so Window_presentPhysical either fails to lock or gets a format it
	// rejects - silently, since the draw loop passes no Error.
	//0, 0 keeps the surface's own dimensions and pins only the format.
	//Only for windows that asked for a cpu buffer: a vulkan swapchain configures this surface itself, and its
	// preTransform is how rotation gets handled there.

	if(w->hint & EWindowHint_ProvideCPUBuffer) {

		I32 nativeFormat = WINDOW_FORMAT_RGBA_8888;

		switch(w->format) {

			case EWindowFormat_BGR10A2:
				nativeFormat = AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
				break;

			case EWindowFormat_RGBA16f:
				nativeFormat = AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
				break;

			default:
				break;
		}

		if(ANativeWindow_setBuffersGeometry(app->window, 0, 0, nativeFormat))
			Log_debugLnx("AWindow_finalize() couldn't set the surface format, present may not reach the screen");
	}

	w->size = I32x2_create2(ANativeWindow_getWidth(app->window), ANativeWindow_getHeight(app->window));

	w->flags |= EWindowFlags_IsActive;

	if(w->callbacks.onCreate) {
		Error err = Error_none();
		if(!w->callbacks.onCreate(w, &err))
			Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);
	}

	AWindow_onUpdateSize(w);
}

void AWindow_onAppCmd(struct android_app *app, I32 cmd) {

	Window *w = (Window*) app->userData;

	if(!w)
		return;

	if((w->flags & EWindowFlags_IsFinalized) || cmd == APP_CMD_INIT_WINDOW)
		switch (cmd) {

			case APP_CMD_WINDOW_RESIZED: {

				I32 orientation = APlatform_getDeviceOrientation();

				if(orientation < 0) {
					Log_errorLnx("-- Error! Orientation couldn't be detected! Defaulting to portrait");
					orientation = 0;
				}

				I32 width = ANativeWindow_getWidth(app->window);
				I32 height = ANativeWindow_getHeight(app->window);

				switch(orientation) {

					case 90:    case 270: {        //Ensure we maintain "no rotation", we'll do it ourselves
						I32 tmp = width;
						width = height;
						height = tmp;
						break;
					}

					default:
						break;
				}
				
				I32x2 oldSize = w->size;
				w->size = I32x2_create2(width, height);

				if ((I32x2_neq2(w->size, oldSize) || w->orientation != (U16) orientation) && w->nativeHandle) {
					w->orientation = (U16) orientation;
					AWindow_onUpdateSize(w);
				}

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

					//savedState stays owned by the glue (android_app_post_exec_cmd frees it right after this returns),
					// so hand the callback a ref rather than taking ownership of it here.

					Buffer buf = Buffer_createRefConst(app->savedState, app->savedStateSize);
					w->callbacks.onLoad(w, buf);
				}

				break;

			case APP_CMD_TERM_WINDOW:

				//The surface is already gone by the time this arrives, and the glue has nulled app->window.
				//Clearing IsFinalized is what keeps the generic step from drawing and presenting into it, since its
				// ShouldTerminate check only runs after the present.
				//AWindow_finalize binds the window again if another surface shows up (resume sends INIT_WINDOW).

				w->flags &= ~(EWindowFlags_IsFinalized | EWindowFlags_IsActive);
				w->nativeHandle = NULL;

				w->flags |= EWindowFlags_ShouldTerminate;
				break;
			
			case APP_CMD_SAVE_STATE:

				if (w->callbacks.onSave) {

					if (app->savedState) {
						Buffer old = Buffer_createManagedPtr(app->savedState, app->savedStateSize);
						Buffer_free(&old, Platform_instance->alloc);
					}

					Buffer buf = Buffer_createNull();
					w->callbacks.onSave(w, &buf);
					app->savedState = buf.ptrNonConst;
					app->savedStateSize = Buffer_length(buf);
				}
			
				break;
			
			case APP_CMD_INIT_WINDOW:
				AWindow_finalize(w);
				break;

			//On config change can be a lot of things, orientation is already handled by onUpdateSize.
			//We will later care about:
			//keyboardHidden|screenLayout|fontScale|locale
			//Since these might impact language, text rendering or text input

			case APP_CMD_CONFIG_CHANGED:
				break;

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

		//AKEYCODE_SCREENSHOT isn't exposed by the NDK (it's a framework-internal key), AKEYCODE_SYSRQ is
		//documented as the "System Request / Print Screen" key and is what an attached keyboard reports.
		case AKEYCODE_SYSRQ:            return EKey_PrintScreen;
		case AKEYCODE_DEL:              return EKey_Backspace;
		case AKEYCODE_SPACE:            return EKey_Space;
		case AKEYCODE_TAB:              return EKey_Tab;

		case AKEYCODE_SHIFT_LEFT:       return EKey_LShift;
		case AKEYCODE_SHIFT_RIGHT:      return EKey_RShift;
		case AKEYCODE_ALT_LEFT:         return EKey_LAlt;
		case AKEYCODE_ALT_RIGHT:        return EKey_RAlt;
		case AKEYCODE_CTRL_LEFT:        return EKey_LCtrl;
		case AKEYCODE_CTRL_RIGHT:       return EKey_RCtrl;
		case AKEYCODE_META_LEFT:        return EKey_LMenu;
		case AKEYCODE_META_RIGHT:       return EKey_RMenu;

		case AKEYCODE_CAPS_LOCK:        return EKey_Caps;
		case AKEYCODE_SCROLL_LOCK:      return EKey_ScrollLock;
		case AKEYCODE_NUM_LOCK:         return EKey_NumLock;

		case AKEYCODE_COMMA:            return EKey_Comma;
		case AKEYCODE_MINUS:            return EKey_Minus;
		case AKEYCODE_EQUALS:           return EKey_Equals;
		case AKEYCODE_PERIOD:           return EKey_Period;
		case AKEYCODE_REFRESH:          return EKey_Refresh;
		case AKEYCODE_MOVE_HOME:        return EKey_Home;
		case AKEYCODE_MOVE_END:         return EKey_End;
		case AKEYCODE_ENTER:            return EKey_Enter;
		case AKEYCODE_GRAVE:            return EKey_Backtick;
		case AKEYCODE_BACKSLASH:        return EKey_Backslash;
		case AKEYCODE_SEMICOLON:        return EKey_Semicolon;
		case AKEYCODE_APOSTROPHE:       return EKey_Quote;
		case AKEYCODE_SLASH:            return EKey_Slash;

		case AKEYCODE_LEFT_BRACKET:     return EKey_LBracket;
		case AKEYCODE_RIGHT_BRACKET:    return EKey_RBracket;

		case AKEYCODE_NUMPAD_DIVIDE:    return EKey_NumpadDiv;
		case AKEYCODE_NUMPAD_MULTIPLY:  return EKey_NumpadMul;
		case AKEYCODE_NUMPAD_SUBTRACT:  return EKey_NumpadSub;
		case AKEYCODE_NUMPAD_ADD:       return EKey_NumpadAdd;

		case AKEYCODE_NUMPAD_DOT:       return EKey_NumpadDot;
		case AKEYCODE_NUMPAD_COMMA:     return EKey_NumpadDot;

		case AKEYCODE_VOLUME_UP:        return EKey_VolumeUp;
		case AKEYCODE_VOLUME_DOWN:      return EKey_VolumeDown;

		case AKEYCODE_PAGE_UP:          return EKey_PageUp;
		case AKEYCODE_PAGE_DOWN:        return EKey_PageDown;
		
		case AKEYCODE_VOLUME_MUTE:      return EKey_Mute;
		case AKEYCODE_CLEAR:            return EKey_Clear;
		case AKEYCODE_ESCAPE:           return EKey_Escape;
		case AKEYCODE_INSERT:           return EKey_Insert;
		case AKEYCODE_MEDIA_PLAY_PAUSE: return EKey_Pause;
		case AKEYCODE_FORWARD_DEL:      return EKey_Delete;

		case AKEYCODE_SLEEP:            return EKey_Sleep;
		case AKEYCODE_HELP:             return EKey_Help;
		case AKEYCODE_SEARCH:           return EKey_Search;
		case AKEYCODE_MENU:             return EKey_Options;

		case AKEYCODE_DPAD_DOWN:        return EKey_Down;
		case AKEYCODE_DPAD_UP:          return EKey_Up;
		case AKEYCODE_DPAD_LEFT:        return EKey_Left;
		case AKEYCODE_DPAD_RIGHT:       return EKey_Right;

		case AKEYCODE_BACK:             return EKey_Back;
		case AKEYCODE_FORWARD:          return EKey_Forward;

		case AKEYCODE_MEDIA_NEXT:       return EKey_Skip;
		case AKEYCODE_MEDIA_PREVIOUS:   return EKey_Previous;
	}
}

//Inverse of AWindow_mapKey, for Keyboard_remap (aplatform.c). Kept next to the switch above on purpose:
//the two have to agree, and nothing enforces that but proximity.
//AWindow_mapKey stays a switch because it's on the input hot path and compiles to a jump table.
//AKEYCODE_UNKNOWN (0) means the key doesn't exist on android and can't be remapped.

const I32 EKey_toAndroidKeyCode[EKey_Count] = {

	/* EKey_0 */            AKEYCODE_0,
	/* EKey_1 */            AKEYCODE_1,
	/* EKey_2 */            AKEYCODE_2,
	/* EKey_3 */            AKEYCODE_3,
	/* EKey_4 */            AKEYCODE_4,
	/* EKey_5 */            AKEYCODE_5,
	/* EKey_6 */            AKEYCODE_6,
	/* EKey_7 */            AKEYCODE_7,
	/* EKey_8 */            AKEYCODE_8,
	/* EKey_9 */            AKEYCODE_9,

	/* EKey_A */            AKEYCODE_A,
	/* EKey_B */            AKEYCODE_B,
	/* EKey_C */            AKEYCODE_C,
	/* EKey_D */            AKEYCODE_D,
	/* EKey_E */            AKEYCODE_E,
	/* EKey_F */            AKEYCODE_F,
	/* EKey_G */            AKEYCODE_G,
	/* EKey_H */            AKEYCODE_H,
	/* EKey_I */            AKEYCODE_I,
	/* EKey_J */            AKEYCODE_J,
	/* EKey_K */            AKEYCODE_K,
	/* EKey_L */            AKEYCODE_L,
	/* EKey_M */            AKEYCODE_M,
	/* EKey_N */            AKEYCODE_N,
	/* EKey_O */            AKEYCODE_O,
	/* EKey_P */            AKEYCODE_P,
	/* EKey_Q */            AKEYCODE_Q,
	/* EKey_R */            AKEYCODE_R,
	/* EKey_S */            AKEYCODE_S,
	/* EKey_T */            AKEYCODE_T,
	/* EKey_U */            AKEYCODE_U,
	/* EKey_V */            AKEYCODE_V,
	/* EKey_W */            AKEYCODE_W,
	/* EKey_X */            AKEYCODE_X,
	/* EKey_Y */            AKEYCODE_Y,
	/* EKey_Z */            AKEYCODE_Z,

	/* EKey_Backspace */    AKEYCODE_DEL,
	/* EKey_Space */        AKEYCODE_SPACE,
	/* EKey_Tab */          AKEYCODE_TAB,

	/* EKey_LShift */       AKEYCODE_SHIFT_LEFT,
	/* EKey_LCtrl */        AKEYCODE_CTRL_LEFT,
	/* EKey_LAlt */         AKEYCODE_ALT_LEFT,
	/* EKey_LMenu */        AKEYCODE_META_LEFT,
	/* EKey_RShift */       AKEYCODE_SHIFT_RIGHT,
	/* EKey_RCtrl */        AKEYCODE_CTRL_RIGHT,
	/* EKey_RAlt */         AKEYCODE_ALT_RIGHT,
	/* EKey_RMenu */        AKEYCODE_META_RIGHT,

	/* EKey_Pause */        AKEYCODE_MEDIA_PLAY_PAUSE,
	/* EKey_Caps */         AKEYCODE_CAPS_LOCK,
	/* EKey_Escape */       AKEYCODE_ESCAPE,
	/* EKey_PageUp */       AKEYCODE_PAGE_UP,
	/* EKey_PageDown */     AKEYCODE_PAGE_DOWN,
	/* EKey_End */          AKEYCODE_MOVE_END,
	/* EKey_Home */         AKEYCODE_MOVE_HOME,
	/* EKey_PrintScreen */  AKEYCODE_SYSRQ,
	/* EKey_Insert */       AKEYCODE_INSERT,
	/* EKey_Enter */        AKEYCODE_ENTER,
	/* EKey_Delete */       AKEYCODE_FORWARD_DEL,
	/* EKey_NumLock */      AKEYCODE_NUM_LOCK,
	/* EKey_ScrollLock */   AKEYCODE_SCROLL_LOCK,

	/* EKey_Back */         AKEYCODE_BACK,
	/* EKey_Forward */      AKEYCODE_FORWARD,
	/* EKey_Sleep */        AKEYCODE_SLEEP,
	/* EKey_Refresh */      AKEYCODE_REFRESH,
	/* EKey_Search */       AKEYCODE_SEARCH,
	/* EKey_Mute */         AKEYCODE_VOLUME_MUTE,
	/* EKey_VolumeDown */   AKEYCODE_VOLUME_DOWN,
	/* EKey_VolumeUp */     AKEYCODE_VOLUME_UP,
	/* EKey_Skip */         AKEYCODE_MEDIA_NEXT,
	/* EKey_Previous */     AKEYCODE_MEDIA_PREVIOUS,
	/* EKey_Clear */        AKEYCODE_CLEAR,
	/* EKey_Help */         AKEYCODE_HELP,

	/* EKey_Left */         AKEYCODE_DPAD_LEFT,
	/* EKey_Up */           AKEYCODE_DPAD_UP,
	/* EKey_Right */        AKEYCODE_DPAD_RIGHT,
	/* EKey_Down */         AKEYCODE_DPAD_DOWN,

	/* EKey_Numpad0 */      AKEYCODE_NUMPAD_0,
	/* EKey_Numpad1 */      AKEYCODE_NUMPAD_1,
	/* EKey_Numpad2 */      AKEYCODE_NUMPAD_2,
	/* EKey_Numpad3 */      AKEYCODE_NUMPAD_3,
	/* EKey_Numpad4 */      AKEYCODE_NUMPAD_4,
	/* EKey_Numpad5 */      AKEYCODE_NUMPAD_5,
	/* EKey_Numpad6 */      AKEYCODE_NUMPAD_6,
	/* EKey_Numpad7 */      AKEYCODE_NUMPAD_7,
	/* EKey_Numpad8 */      AKEYCODE_NUMPAD_8,
	/* EKey_Numpad9 */      AKEYCODE_NUMPAD_9,

	/* EKey_NumpadMul */    AKEYCODE_NUMPAD_MULTIPLY,
	/* EKey_NumpadAdd */    AKEYCODE_NUMPAD_ADD,
	/* EKey_NumpadDot */    AKEYCODE_NUMPAD_DOT,
	/* EKey_NumpadDiv */    AKEYCODE_NUMPAD_DIVIDE,
	/* EKey_NumpadSub */    AKEYCODE_NUMPAD_SUBTRACT,

	/* EKey_F1 */           AKEYCODE_F1,
	/* EKey_F2 */           AKEYCODE_F2,
	/* EKey_F3 */           AKEYCODE_F3,
	/* EKey_F4 */           AKEYCODE_F4,
	/* EKey_F5 */           AKEYCODE_F5,
	/* EKey_F6 */           AKEYCODE_F6,
	/* EKey_F7 */           AKEYCODE_F7,
	/* EKey_F8 */           AKEYCODE_F8,
	/* EKey_F9 */           AKEYCODE_F9,
	/* EKey_F10 */          AKEYCODE_F10,
	/* EKey_F11 */          AKEYCODE_F11,
	/* EKey_F12 */          AKEYCODE_F12,

	/* EKey_Bar */          AKEYCODE_UNKNOWN,    //Scancode 56; android has no keycode for it
	/* EKey_Options */      AKEYCODE_MENU,

	/* EKey_Equals */       AKEYCODE_EQUALS,
	/* EKey_Comma */        AKEYCODE_COMMA,
	/* EKey_Minus */        AKEYCODE_MINUS,
	/* EKey_Period */       AKEYCODE_PERIOD,
	/* EKey_Slash */        AKEYCODE_SLASH,
	/* EKey_Backtick */     AKEYCODE_GRAVE,
	/* EKey_Semicolon */    AKEYCODE_SEMICOLON,
	/* EKey_LBracket */     AKEYCODE_LEFT_BRACKET,
	/* EKey_RBracket */     AKEYCODE_RIGHT_BRACKET,
	/* EKey_Backslash */    AKEYCODE_BACKSLASH,
	/* EKey_Quote */        AKEYCODE_APOSTROPHE
};

//KeyCharacterMap is per input device, so remapping needs to know which keyboard we're talking about.
//Set from the input thread, read from wherever Keyboard_remap is called, hence the atomic.
//KeyCharacterMap.VIRTUAL_KEYBOARD (-1) is the documented fallback and yields a US QWERTY map.

AtomicI64 AWindow_lastKeyboardDeviceId = { -1 };

//Touch emulates the mouse: the first finger down drives the cursor and the left button,
// and any other finger is ignored until it lifts.
//Same model as the wayland/SteamOS path in lwindow_input.c, which is the reference for the semantics here.
//Multi-touch would need an InputDevice type of its own.
//Android is single-window (WindowManager_createNative sets isSingleWindow), so one id is enough.

static I32 AWindow_primaryTouchId = -1;

//Contact ended, by lift-off or by the system taking the gesture away.
//Settle RX/RY back to zero so nothing reads a stale "still moving" delta, and release the button.
//Temp0/Temp1 are deliberately left alone: "last known position" stays meaningful after lift-off.

static void AWindow_touchEnd(Window *w, Mouse *mouse) {

	AWindow_primaryTouchId = -1;

	const F32 prevRelX = InputDevice_getCurrentAxis(mouse, EMouseAxis_RX);
	const F32 prevRelY = InputDevice_getCurrentAxis(mouse, EMouseAxis_RY);

	if(prevRelX != 0) {

		InputDevice_setCurrentAxis(mouse, EMouseAxis_RX, 0);

		if(w->callbacks.onDeviceAxis)
			w->callbacks.onDeviceAxis(w, mouse, EMouseAxis_RX, 0);
	}

	if(prevRelY != 0) {

		InputDevice_setCurrentAxis(mouse, EMouseAxis_RY, 0);

		if(w->callbacks.onDeviceAxis)
			w->callbacks.onDeviceAxis(w, mouse, EMouseAxis_RY, 0);
	}

	if(InputDevice_getState(mouse, EMouseButton_Left) & EInputState_Curr) {

		InputDevice_setCurrentState(mouse, EMouseButton_Left, false);

		if(w->callbacks.onDeviceButton)
			w->callbacks.onDeviceButton(w, mouse, EMouseButton_Left, false);
	}
}

static void AWindow_onTouch(Window *w, Mouse *mouse, AInputEvent *event) {

	const I32 action = AMotionEvent_getAction(event);
	const I32 masked = action & AMOTION_EVENT_ACTION_MASK;

	const size_t changed =
		(size_t)((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);

	switch(masked) {

		case AMOTION_EVENT_ACTION_DOWN:
		case AMOTION_EVENT_ACTION_POINTER_DOWN: {

			if(AWindow_primaryTouchId != -1)        //Already tracking a finger, ignore the rest
				break;

			if(changed >= AMotionEvent_getPointerCount(event))
				break;

			AWindow_primaryTouchId = AMotionEvent_getPointerId(event, changed);

			const F32 x = AMotionEvent_getRawX(event, changed);
			const F32 y = AMotionEvent_getRawY(event, changed);

			//Fresh contact point: seed the absolute position and zero the relative axes. There's no
			//previous position yet, so RX/RY must not carry a delta over from the last gesture.
			//No onDeviceAxis here, deliberately: where a finger lands isn't motion, just the baseline.

			InputDevice_setCurrentAxis(mouse, EMouseAxis_Temp0, x);
			InputDevice_setCurrentAxis(mouse, EMouseAxis_Temp1, y);
			InputDevice_setCurrentAxis(mouse, EMouseAxis_RX, 0);
			InputDevice_setCurrentAxis(mouse, EMouseAxis_RY, 0);

			const I32x2 oldCursor = w->cursor;
			w->cursor = I32x2_create2((I32) x, (I32) y);

			if(w->callbacks.onCursorMove && I32x2_neq2(oldCursor, w->cursor))
				w->callbacks.onCursorMove(w);

			InputDevice_setCurrentState(mouse, EMouseButton_Left, true);

			if(w->callbacks.onDeviceButton)
				w->callbacks.onDeviceButton(w, mouse, EMouseButton_Left, true);

			break;
		}

		case AMOTION_EVENT_ACTION_MOVE: {

			if(AWindow_primaryTouchId == -1)
				break;

			//A move event carries every active pointer, so find the one we're tracking rather than
			//assuming index 0; a second finger going down would otherwise hijack the cursor.

			const size_t count = AMotionEvent_getPointerCount(event);
			size_t index = count;

			for(size_t i = 0; i < count; ++i)
				if(AMotionEvent_getPointerId(event, i) == AWindow_primaryTouchId) {
					index = i;
					break;
				}

			if(index == count)
				break;

			const F32 x = AMotionEvent_getRawX(event, index);
			const F32 y = AMotionEvent_getRawY(event, index);

			const F32 relX = x - InputDevice_getCurrentAxis(mouse, EMouseAxis_Temp0);
			const F32 relY = y - InputDevice_getCurrentAxis(mouse, EMouseAxis_Temp1);

			InputDevice_setCurrentAxis(mouse, EMouseAxis_Temp0, x);
			InputDevice_setCurrentAxis(mouse, EMouseAxis_Temp1, y);
			InputDevice_setCurrentAxis(mouse, EMouseAxis_RX, relX);
			InputDevice_setCurrentAxis(mouse, EMouseAxis_RY, relY);

			const I32x2 oldCursor = w->cursor;
			w->cursor = I32x2_create2((I32) x, (I32) y);

			if(w->callbacks.onCursorMove && I32x2_neq2(oldCursor, w->cursor))
				w->callbacks.onCursorMove(w);

			if(w->callbacks.onDeviceAxis) {

				if(relX != 0) {
					w->callbacks.onDeviceAxis(w, mouse, EMouseAxis_Temp0, x);
					w->callbacks.onDeviceAxis(w, mouse, EMouseAxis_RX, relX);
				}

				if(relY != 0) {
					w->callbacks.onDeviceAxis(w, mouse, EMouseAxis_Temp1, y);
					w->callbacks.onDeviceAxis(w, mouse, EMouseAxis_RY, relY);
				}
			}

			break;
		}

		case AMOTION_EVENT_ACTION_UP:
		case AMOTION_EVENT_ACTION_POINTER_UP:

			if(
				AWindow_primaryTouchId != -1 &&
				changed < AMotionEvent_getPointerCount(event) &&
				AMotionEvent_getPointerId(event, changed) == AWindow_primaryTouchId
			)
				AWindow_touchEnd(w, mouse);

			break;

		case AMOTION_EVENT_ACTION_CANCEL:

			if(AWindow_primaryTouchId != -1)
				AWindow_touchEnd(w, mouse);

			break;

		default:
			break;
	}
}

I32 AWindow_onInput(struct android_app *app, AInputEvent *event) {

	Window *w = (Window*) app->userData;

	if(!w || !(w->flags & EWindowFlags_IsFinalized) || (w->flags & EWindowFlags_ShouldTerminate))
		return 0;

	if(w->devices.length <= w->defaultKeyboardId || w->devices.length <= w->defaultMouseId)
		return 0;

	Keyboard *keyboard = w->devices.ptrNonConst + w->defaultKeyboardId;
	Mouse *mouse = w->devices.ptrNonConst + w->defaultMouseId;

	Bool isDown = (AKeyEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK) == AKEY_EVENT_ACTION_DOWN;
	
	I32 type = AInputEvent_getType(event);
	I32 source = AInputEvent_getSource(event);
	
	switch(type) {

		case AINPUT_EVENT_TYPE_MOTION: {

			I32 action = AMotionEvent_getAction(event);

			//Touch is pointer-id tracked and has no hover,
			// so it gets its own handler rather than sharing the mouse's index-0 path (see AWindow_onTouch).

			if (source & AINPUT_SOURCE_TOUCHSCREEN) {
				AWindow_onTouch(w, mouse, event);
				return 1;
			}

			if (source & AINPUT_SOURCE_MOUSE) {

				InputDevice *dev = mouse;
				
				if (action == AMOTION_EVENT_ACTION_MOVE || action == AMOTION_EVENT_ACTION_HOVER_MOVE) {
					
					F32 x = AMotionEvent_getRawX(event, 0);
					F32 y = AMotionEvent_getRawY(event, 0);

					F32 prevAbsX = InputDevice_getCurrentAxis(dev, EMouseAxis_Temp0);
					F32 prevAbsY = InputDevice_getCurrentAxis(dev, EMouseAxis_Temp1);

					F32 nextX = F32_ceil(F32_clamp((F32) (x - prevAbsX), -1, 1));
					F32 nextY = F32_ceil(F32_clamp((F32) (y - prevAbsY), -1, 1));

					InputDevice_setCurrentAxis(dev, EMouseAxis_Temp0, x);
					InputDevice_setCurrentAxis(dev, EMouseAxis_Temp1, y);

					I32x2 oldCursor = w->cursor;
					w->cursor = I32x2_create2((I32) x, (I32) y);

					if (w->callbacks.onCursorMove && I32x2_neq2(oldCursor, w->cursor))
						w->callbacks.onCursorMove(w);

					F32 prevX = InputDevice_getCurrentAxis(dev, EMouseAxis_RX);
					F32 prevY = InputDevice_getCurrentAxis(dev, EMouseAxis_RY);

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
					
				} else if (action == AMOTION_EVENT_ACTION_SCROLL) {

					F32 nextX = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HSCROLL, 0);
					F32 nextY = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_VSCROLL, 0);

					nextX = F32_clamp(F32_ceil(nextX), -1, 1);
					nextY = F32_clamp(F32_ceil(nextY), -1, 1);

					F32 prevX = InputDevice_getCurrentAxis(dev, EMouseAxis_ScrollWheel_X);
					F32 prevY = InputDevice_getCurrentAxis(dev, EMouseAxis_ScrollWheel_Y);

					if (nextX != prevX) {

						InputDevice_setCurrentAxis(dev, EMouseAxis_ScrollWheel_X, nextX);

						if (w->callbacks.onDeviceAxis)
							w->callbacks.onDeviceAxis(w, dev, EMouseAxis_ScrollWheel_X, nextX);
					}

					if (nextY != prevY) {

						InputDevice_setCurrentAxis(dev, EMouseAxis_ScrollWheel_Y, nextY);

						if (w->callbacks.onDeviceAxis)
							w->callbacks.onDeviceAxis(w, dev, EMouseAxis_ScrollWheel_Y, nextY);
					}

				} else if(
					action == AMOTION_EVENT_ACTION_BUTTON_PRESS ||
					action == AMOTION_EVENT_ACTION_BUTTON_RELEASE
				) {

					I32 buttonState = AMotionEvent_getButtonState(event);
					
					Bool primary = buttonState & AMOTION_EVENT_BUTTON_PRIMARY;
					Bool secondary = buttonState & AMOTION_EVENT_BUTTON_SECONDARY;
					Bool tertiary = buttonState & AMOTION_EVENT_BUTTON_TERTIARY;
					Bool forward = buttonState & AMOTION_EVENT_BUTTON_FORWARD;
					Bool backward = buttonState & AMOTION_EVENT_BUTTON_BACK;

					Bool states[5] = { primary, secondary, tertiary, forward, backward };
					EMouseActions buttons[5] = {
						EMouseButton_Left, EMouseButton_Right, EMouseButton_Middle, EMouseButton_Forward, EMouseButton_Back
					};

					for(U64 i = 0; i < 5; ++i) {

						//Send keys through interface and update input device

						InputHandle handle = buttons[i];
						EInputState prevState = InputDevice_getState(dev, handle);

						InputDevice_setCurrentState(dev, handle, states[i]);
						EInputState newState = InputDevice_getState(dev, handle);

						if(prevState != newState && w->callbacks.onDeviceButton)
							w->callbacks.onDeviceButton(w, dev, handle, states[i]);
					}

				} else if(
					action == AMOTION_EVENT_ACTION_DOWN ||
					action == AMOTION_EVENT_ACTION_UP
				) {
					
					InputHandle handle = EMouseButton_Left;
					EInputState prevState = InputDevice_getState(dev, handle);

					InputDevice_setCurrentState(dev, handle, isDown);
					EInputState newState = InputDevice_getState(dev, handle);

					if(prevState != newState && w->callbacks.onDeviceButton)
						w->callbacks.onDeviceButton(w, dev, handle, isDown);
				}
			}

			return 0;
		}

		case AINPUT_EVENT_TYPE_KEY: {

			I32 keyCode = AKeyEvent_getKeyCode(event);
			I32 metaState = AKeyEvent_getMetaState(event);
			I32 deviceId = AInputEvent_getDeviceId(event);
			EKey mappedKey = AWindow_mapKey(keyCode);

			//Remember which keyboard this came from so Keyboard_remap can load its KeyCharacterMap.
			//Stored even for keys we don't map, since the device is valid regardless.

			AtomicI64_store(&AWindow_lastKeyboardDeviceId, deviceId);

			//Text input for physical keyboards.
			//The EditText only has focus while the soft keyboard is up (see Platform_setKeyboardVisible),
			// so these never reach the TextWatcher and would otherwise produce no text at all.
			//Control characters are filtered to match WM_CHAR/linux.

			if (isDown && w->callbacks.onTypeChar) {

				I32 unicode = APlatform_getKeyUnicode(keyCode, metaState, deviceId);

				if (unicode >= 0x20 && unicode != 0x7F) {

					CharString str = CharString_createNull();
					Error err = Error_none();
					const U32 codepoint = (U32) unicode;

					if(CharString_createFromUTF32(&codepoint, 1, Platform_instance->alloc, &str, &err))
						w->callbacks.onTypeChar(w, str);

					CharString_free(&str, Platform_instance->alloc);
				}
			}

			if (mappedKey != EKey_Count) {

				InputDevice *dev = keyboard;

				dev->flags =
					((U32)((metaState & AMETA_CAPS_LOCK_ON) != 0)   << EKeyboardFlags_Caps) |
					((U32)((metaState & AMETA_NUM_LOCK_ON) != 0)    << EKeyboardFlags_NumLock) |
					((U32)((metaState & AMETA_SCROLL_LOCK_ON) != 0) << EKeyboardFlags_ScrollLock);

				//Send keys through interface and update input device

				InputHandle handle = (InputHandle) mappedKey;
				EInputState prevState = InputDevice_getState(dev, handle);

				InputDevice_setCurrentState(dev, handle, isDown);
				EInputState newState = InputDevice_getState(dev, handle);

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
	return format == EWindowFormat_RGBA8;    //TODO: HDR
}

void WindowManager_freePhysical(Window *w) {
	(void) w;
	struct android_app *app = (struct android_app*) Platform_instance->data;
	app->userData = NULL;
	app->onAppCmd = NULL;
	app->onInputEvent = NULL;
}

Bool Window_updatePhysicalTitle(Window *w, CharString title, Error *e_rr) {

	Bool s_uccess = true;

	if(!w || !I32x2_any(w->size) || !title.ptr || !CharString_length(title) || w->type != EWindowType_Physical)
		retError(clean, Error_nullPointer(
			!w || !I32x2_any(w->size) ? 0 : 1, "Window_updatePhysicalTitle()::w and title are required"
		));

	if(!(w->flags & EWindowFlags_IsActive)) {
		Log_warnLnx("Window_updatePhysicalTitle()::w triggered on inactive window. Ignored");
		goto clean;
	}

	//Title of the window is handled by activity name instead

clean:
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

	if(!(w->flags & EWindowFlags_IsActive)) {
		Log_warnLnx("Window_updatePhysicalTitle()::w triggered on inactive window. Ignored");
		goto clean;
	}

	//Fullscreen is a no-op, this is handled by the OS, not our app

clean:
	return s_uccess;
}

Bool Window_presentPhysical(Window *w, Error *e_rr) {

	Bool s_uccess = true;

	if(!w || !I32x2_any(w->size))
		retError(clean, Error_nullPointer(0, "Window_presentPhysical()::w is required"));

	if(!(w->flags & EWindowFlags_IsActive) || !(w->hint & EWindowHint_ProvideCPUBuffer))
		retError(clean, Error_invalidOperation(0, "Window_presentPhysical() can only be called if there's a CPU-sided buffer"));

	struct android_app *app = (struct android_app*) Platform_instance->data;
	ANativeWindow *nativeWindow = app ? app->window : NULL;

	//Teardown races the draw loop: the surface can go away between a step deciding to present and getting here, and
	// ANativeWindow_lock segfaults on a null window rather than returning an error.

	if(!nativeWindow)
		retError(clean, Error_invalidState(0, "Window_presentPhysical() the surface is gone"));

	//w->size stays in the unrotated orientation on purpose (see APP_CMD_WINDOW_RESIZED), because the vulkan swapchain
	// hands rotation to the display controller through preTransform and reads w->orientation to do it.
	//The surface itself is rotated though, so the region we lock is w->size turned to match it.

	const Bool quarterTurn = w->orientation == 90 || w->orientation == 270;

	const I32 surfaceW = quarterTurn ? I32x2_y(w->size) : I32x2_x(w->size);
	const I32 surfaceH = quarterTurn ? I32x2_x(w->size) : I32x2_y(w->size);

	ANativeWindow_Buffer buffer = (ANativeWindow_Buffer) { 0 };
	ARect rect = (ARect) { .right = surfaceW, .bottom = surfaceH };

	if (ANativeWindow_lock(nativeWindow, &buffer, &rect))
		retError(clean, Error_invalidState(0, "Window_presentPhysical() couldn't lock window"));

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
		retError(clean, Error_invalidState(0, "Window_presentPhysical() couldn't write to window; mismatching formats"));
	}

	if(buffer.width != surfaceW || buffer.height != surfaceH) {
		ANativeWindow_unlockAndPost(nativeWindow);
		retError(clean, Error_invalidState(0, "Window_presentPhysical() couldn't write to window; mismatching dimensions"));
	}

	//The compositor picks the stride, and it's regularly wider than the window (1088 for a 1080px display, say).
	//ANativeWindow_Buffer::stride counts pixels, not bytes, and only matches the width when the width happens to suit
	// the GPU's alignment, so the rows can't go across as one block or each one lands progressively further left.

	const U64 bytesPerPixel = ETextureFormat_getSize((ETextureFormat) w->format, 1, 1, 1);
	const U64 srcStride = (U64) I32x2_x(w->size) * bytesPerPixel;
	const U64 dstStride = (U64) buffer.stride * bytesPerPixel;

	const I32 srcW = I32x2_x(w->size), srcH = I32x2_y(w->size);

	U8 *dst = (U8*) buffer.bits;
	const U8 *src = w->cpuVisibleBuffer.ptr;

	//Upright: rows go across whole, or one block when the strides already agree.
	//Rotated: the buffer is turned into the surface a pixel at a time, since no run of source pixels is contiguous in
	// the destination. That's only for the cpu present path; vulkan gets this free from the swapchain's preTransform,
	// so nothing that actually renders pays for it.

	if(!quarterTurn && w->orientation != 180) {

		if(srcStride == dstStride)
			Buffer_memcpy(Buffer_createRef(dst, Buffer_length(w->cpuVisibleBuffer)), w->cpuVisibleBuffer);

		else for(I32 y = 0; y < srcH; ++y)
			Buffer_memcpy(
				Buffer_createRef(dst + (U64) y * dstStride, srcStride),
				Buffer_createRefConst(src + (U64) y * srcStride, srcStride)
			);
	}

	else for(I32 y = 0; y < surfaceH; ++y)
		for(I32 x = 0; x < surfaceW; ++x) {

			I32 sx, sy;

			switch(w->orientation) {

				case 90:     sx = y;                sy = srcH - 1 - x;    break;
				case 270:    sx = srcW - 1 - y;     sy = x;               break;
				default:     sx = srcW - 1 - x;     sy = srcH - 1 - y;    break;    //180
			}

			Buffer_memcpy(
				Buffer_createRef(dst + (U64) y * dstStride + (U64) x * bytesPerPixel, bytesPerPixel),
				Buffer_createRefConst(src + (U64) sy * srcStride + (U64) sx * bytesPerPixel, bytesPerPixel)
			);
		}

	ANativeWindow_unlockAndPost(nativeWindow);

clean:
	return s_uccess;
}

Bool WindowManager_createWindowPhysical(Window *w, Error *e_rr) {

	Bool s_uccess = true;
	Keyboard builtinKeyboard = (Keyboard) { 0 };
	Mouse builtinMouse = (Mouse) { 0 };

	for(U64 i = 0; i < w->owner->windows.length; ++i) {

		Window *wi = RefPtr_data(w->owner->windows.ptr[i], Window);

		if(wi != w && wi->type == EWindowType_Physical)
			retError(clean, Error_invalidState(0, "WindowManager_createWindow() there can be only one window on Android"));
	}

	if(w->format == EWindowFormat_RGBA32f)
		retError(clean, Error_invalidState(0, "WindowManager_createWindow() RGBA32f format not natively supported on Android"));

	if(w->format == EWindowFormat_BGRA8)
		retError(clean, Error_invalidState(0, "WindowManager_createWindow() BGRA8 format not natively supported on Android"));

	//Register the built-in keyboard and mouse, matching the other backends which always have
	// defaultKeyboardId / defaultMouseId available from creation onwards.

	gotoIfError3(clean, ListMonitor_reserve(&w->monitors, 16, Platform_instance->alloc, e_rr));
	gotoIfError3(clean, ListInputDevice_reserve(&w->devices, 2, Platform_instance->alloc, e_rr));

	w->defaultKeyboardId = (U32) w->devices.length;
	gotoIfError3(clean, Keyboard_create(&builtinKeyboard, Platform_instance->alloc, e_rr));
	gotoIfError3(clean, ListInputDevice_pushBack(&w->devices, builtinKeyboard, Platform_instance->alloc, e_rr));
	builtinKeyboard = (Keyboard) { 0 };

	w->defaultMouseId = (U32) w->devices.length;
	gotoIfError3(clean, Mouse_create(&builtinMouse, Platform_instance->alloc, e_rr));
	gotoIfError3(clean, ListInputDevice_pushBack(&w->devices, builtinMouse, Platform_instance->alloc, e_rr));
	builtinMouse = (Mouse) { 0 };

	struct android_app *app = (struct android_app*) Platform_instance->data;
	app->userData = w;
	app->onAppCmd = AWindow_onAppCmd;
	app->onInputEvent = AWindow_onInput;

	//The surface is already up whenever this isn't the first window of the run, and APP_CMD_INIT_WINDOW won't come a
	// second time, so bind to it now rather than waiting for a command that will never arrive.

	AWindow_finalize(w);

clean:

	if(!s_uccess) {

		InputDevice_free(&builtinKeyboard, Platform_instance->alloc);
		InputDevice_free(&builtinMouse, Platform_instance->alloc);

		for(U64 i = 0; i < w->devices.length; ++i)
			InputDevice_free(&w->devices.ptrNonConst[i], Platform_instance->alloc);

		ListInputDevice_free(&w->devices, Platform_instance->alloc);
		ListMonitor_free(&w->monitors, Platform_instance->alloc);
	}

	return s_uccess;
}
