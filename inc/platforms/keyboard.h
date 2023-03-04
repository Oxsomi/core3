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

typedef enum EKey {

	EKey_0, EKey_1, EKey_2, EKey_3, EKey_4, EKey_5, EKey_6, EKey_7, EKey_8, EKey_9,

	EKey_A, EKey_B, EKey_C, EKey_D, EKey_E, EKey_F, EKey_G, EKey_H, EKey_I, EKey_J,
	EKey_K, EKey_L, EKey_M, EKey_N, EKey_O, EKey_P, EKey_Q, EKey_R, EKey_S, EKey_T, 
	EKey_U, EKey_V, EKey_W, EKey_X, EKey_Y, EKey_Z,

	EKey_Backspace,		EKey_Space,			EKey_Tab,			EKey_Shift,			EKey_Ctrl,			EKey_Alt, 
	EKey_Pause,			EKey_Caps,			EKey_Escape,		EKey_PageUp,		EKey_PageDown,		EKey_End,
	EKey_Home,			EKey_Select,		EKey_Print,			EKey_Execute,		EKey_PrintScreen,	EKey_Insert,		
	EKey_Back,			EKey_Forward,		EKey_Sleep,			EKey_Refresh,		EKey_Stop,			EKey_Search,
	EKey_Favorites,		EKey_Start,			EKey_Mute,			EKey_VolumeDown,	EKey_VolumeUp,		EKey_Skip,
	EKey_Previous,		EKey_Clear,			EKey_Zoom,			EKey_Enter,			EKey_Delete,		EKey_Help,
	EKey_NumLock,		EKey_ScrollLock,	EKey_Apps,

	EKey_Left,			EKey_Up,			EKey_Right,			EKey_Down,

	EKey_Numpad0,
	EKey_Numpad1,		EKey_Numpad2,		EKey_Numpad3,
	EKey_Numpad4,		EKey_Numpad5,		EKey_Numpad6,
	EKey_Numpad7,		EKey_Numpad8,		EKey_Numpad9,

	EKey_NumpadMul,		EKey_NumpadAdd,		EKey_NumpadDec,
	EKey_NumpadDiv,		EKey_NumpadSub,

	EKey_F1,			EKey_F2,			EKey_F3,			EKey_F4,			EKey_F5,		
	EKey_F6,			EKey_F7,			EKey_F8,			EKey_F9,			EKey_F10,
	EKey_F11,			EKey_F12,			EKey_F13,			EKey_F14,			EKey_F15,	
	EKey_F16,			EKey_F17,			EKey_F18,			EKey_F19,			EKey_F20,
	EKey_F21,			EKey_F22,			EKey_F23,			EKey_F24,

	EKey_Equals,		EKey_Comma,			EKey_Minus,			EKey_Period,
	EKey_Slash,			EKey_Acute,			EKey_Semicolon,		EKey_LBracket,		EKey_RBracket,
	EKey_Backslash,		EKey_Quote,

	EKey_Count

} EKey;

typedef enum EKeyboardFlags {
	EKeyboardFlags_Caps,
	EKeyboardFlags_NumLock,
	EKeyboardFlags_ScrollLock,
	EKeyboardFlags_Shift,
	EKeyboardFlags_Control,
	EKeyboardFlags_Alt
} EKeyboardFlags;

Error Keyboard_create(Keyboard *result);
