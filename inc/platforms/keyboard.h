#pragma once
#include "types/types.h"

typedef struct InputDevice Keyboard;

typedef enum Key {

	Key_0, Key_1, Key_2, Key_3, Key_4, Key_5, Key_6, Key_7, Key_8, Key_9,

	Key_A, Key_B, Key_C, Key_D, Key_E, Key_F, Key_G, Key_H, Key_I, Key_J,
	Key_K, Key_L, Key_M, Key_N, Key_O, Key_P, Key_Q, Key_R, Key_S, Key_T, 
	Key_U, Key_V, Key_W, Key_X, Key_Y, Key_Z,

	Key_Backspace,		Key_Space,			Key_Tab,			Key_Shift,			Key_Ctrl,			Key_Alt, 
	Key_Pause,			Key_Caps,			Key_Escape,			Key_PageUp,			Key_PageDown,		Key_End,
	Key_Home,			Key_Select,			Key_Print,			Key_Execute,		Key_PrintScreen,	Key_Insert,		
	Key_Back,			Key_Forward,		Key_Sleep,			Key_Refresh,		Key_Stop,			Key_Search,
	Key_Favorites,		Key_Start,			Key_Mute,			Key_VolumeDown,		Key_VolumeUp,		Key_Skip,
	Key_Previous,		Key_Clear,			Key_Zoom,			Key_Enter,			Key_Delete,			Key_Help,
	Key_NumLock,		Key_ScrollLock,		Key_Apps,

	Key_Left,			Key_Up,			Key_Right,		Key_Down,

	Key_Numpad0,
	Key_Numpad1,		Key_Numpad2,	Key_Numpad3,
	Key_Numpad4,		Key_Numpad5,	Key_Numpad6,
	Key_Numpad7,		Key_Numpad8,	Key_Numpad9,

	Key_NumpadMul,		Key_NumpadAdd,	Key_NumpadDec,
	Key_NumpadDiv,		Key_NumpadSub,

	Key_F1,				Key_F2,			Key_F3,			Key_F4,			Key_F5,		
	Key_F6,				Key_F7,			Key_F8,			Key_F9,			Key_F10,
	Key_F11,			Key_F12,		Key_F13,		Key_F14,		Key_F15,	
	Key_F16,			Key_F17,		Key_F18,		Key_F19,		Key_F20,
	Key_F21,			Key_F22,		Key_F23,		Key_F24,

	Key_Equals,			Key_Comma,		Key_Minus,		Key_Period,
	Key_Slash,			Key_Acute,		Key_Semicolon,	Key_LBracket,	Key_RBracket,
	Key_Backslash,		Key_Quote,

	Key_Count
} Key;

typedef enum KeyboardFlags {
	KeyboardFlags_Caps,
	KeyboardFlags_NumLock,
	KeyboardFlags_ScrollLock,
	KeyboardFlags_Shift,
	KeyboardFlags_Control,
	KeyboardFlags_Alt
} KeyboardFlags;

typedef struct Error Error;

Error Keyboard_create(Keyboard *result);
