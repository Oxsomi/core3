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

#pragma once
#include "types/types.h"

typedef struct Error Error;
typedef struct InputDevice Keyboard;
typedef struct CharString CharString;

//EKey is remapped using scan codes for QWERTY iso layout (https://kbdlayout.info/kbdusx)
// The main part of the keyboard uses scan codes, but the following don't:
// Extended keyboard (numpad, arrows, insert/page down/etc.)
// These keystrokes use virtual keys on windows.
//Don't assume for example Key_W is really the W key! (e.g. AZERTY would be Z instead of W. Might even be non latin char).
//Use Keyboard_remap(keyboard, key) to get the localized keyboard unicode codepoint.
//Only use Keyboard_remap for displaying what kind of key is pressed or in option screens.
//When typing text, please rely on the typeChar callback.

typedef enum EKey {

	EKey_0, EKey_1, EKey_2, EKey_3, EKey_4, EKey_5, EKey_6, EKey_7, EKey_8, EKey_9,

	EKey_A, EKey_B, EKey_C, EKey_D, EKey_E, EKey_F, EKey_G, EKey_H, EKey_I, EKey_J,
	EKey_K, EKey_L, EKey_M, EKey_N, EKey_O, EKey_P, EKey_Q, EKey_R, EKey_S, EKey_T,
	EKey_U, EKey_V, EKey_W, EKey_X, EKey_Y, EKey_Z,

	EKey_Backspace,		EKey_Space,			EKey_Tab,

	EKey_LShift,		EKey_LCtrl,			EKey_LAlt,			EKey_LMenu,
	EKey_RShift,		EKey_RCtrl,			EKey_RAlt,			EKey_RMenu,

	EKey_Pause,			EKey_Caps,			EKey_Escape,		EKey_PageUp,
	EKey_PageDown,		EKey_End,			EKey_Home,			EKey_PrintScreen,
	EKey_Insert,		EKey_Enter,			EKey_Delete,		EKey_NumLock,		EKey_ScrollLock,

	EKey_Select,		EKey_Print,			EKey_Execute,		EKey_Back,			EKey_Forward,
	EKey_Sleep,			EKey_Refresh,		EKey_Stop,			EKey_Search,		EKey_Favorites,
	EKey_Start,			EKey_Mute,			EKey_VolumeDown,	EKey_VolumeUp,		EKey_Skip,
	EKey_Previous,		EKey_Clear,			EKey_Zoom,			EKey_Help,			EKey_Apps,

	EKey_Left,			EKey_Up,			EKey_Right,			EKey_Down,

	EKey_Numpad0,
	EKey_Numpad1,		EKey_Numpad2,		EKey_Numpad3,
	EKey_Numpad4,		EKey_Numpad5,		EKey_Numpad6,
	EKey_Numpad7,		EKey_Numpad8,		EKey_Numpad9,

	EKey_NumpadMul,		EKey_NumpadAdd,		EKey_NumpadDot,
	EKey_NumpadDiv,		EKey_NumpadSub,

	EKey_F1,			EKey_F2,			EKey_F3,			EKey_F4,			EKey_F5,
	EKey_F6,			EKey_F7,			EKey_F8,			EKey_F9,			EKey_F10,
	EKey_F11,			EKey_F12,

	EKey_Bar,			//Scancode 56 (right half of shift if there's another key there)
	EKey_Options,		//Scancode E05D (left of ctrl)

	EKey_Equals,		EKey_Comma,			EKey_Minus,			EKey_Period,
	EKey_Slash,			EKey_Backtick,		EKey_Semicolon,		EKey_LBracket,		EKey_RBracket,
	EKey_Backslash,		EKey_Quote,

	EKey_Count

} EKey;

typedef enum EKeyboardFlags {
	EKeyboardFlags_Caps,
	EKeyboardFlags_NumLock,
	EKeyboardFlags_ScrollLock,
	EKeyboardFlags_Shift,
	EKeyboardFlags_Control,
	EKeyboardFlags_Alt,
	EKeyboardFlags_Count
} EKeyboardFlags;

Error Keyboard_create(Keyboard *result);

//Remap key to unicode codepoint using current language. This is only for debugging keyboard mappings and GUI elements.
//For text boxes, use the typeChar callback of Window; this handles OS-level input such as IME (Japanese) and emojis.
//If there's no remap available it will return an empty string.
//Make sure to free this.
impl CharString Keyboard_remap(EKey key);
