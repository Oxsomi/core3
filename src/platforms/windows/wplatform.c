/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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
#include "platforms/platform.h"
#include "platforms/log.h"
#include "platforms/keyboard.h"
#include "types/container/string.h"
#include "platforms/ext/stringx.h"

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#define NOMINMAX
#include <Windows.h>

#include <stdlib.h>

U64 Platform_getThreads() {
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	return systemInfo.dwNumberOfProcessors;
}

void *Platform_allocate(void *allocator, U64 length) { (void)allocator; return malloc(length); }
void Platform_free(void *allocator, void *ptr, U64 length) { (void) allocator; (void)length; free(ptr); }

void Platform_cleanupExt() { }

typedef struct EnumerateFiles {
	ListVirtualSection *sections;
	Bool b;
} EnumerateFiles;

BOOL enumerateFiles(HMODULE mod, LPWSTR unused, LPWSTR name, EnumerateFiles *sections) {

	mod; unused;

	CharString str = CharString_createNull();
	Error err = Error_none();
	gotoIfError(clean, CharString_createFromUTF16x((const U16*)name, U64_MAX, &str))

	if(CharString_countAllSensitive(str, '/', 0) != 1)
		Log_warnLnx("Executable contained unrecognized RCDATA. Ignoring it...");

	else {
		const VirtualSection section = (VirtualSection) { .path = str };
		gotoIfError(clean, ListVirtualSection_pushBackx(sections->sections, section))
	}

clean:

	if(err.genericError) {
		sections->b = true;			//Signal that we failed
		CharString_freex(&str);
	}

	return !err.genericError;
}

void *Platform_getDataImpl(void *ptr) {
	(void) ptr;
	return GetModuleHandleW(NULL);
}

Bool Platform_initExt(Error *e_rr) {

	Bool s_uccess = true;

	//Init app dir

	wchar_t buff[MAX_PATH + 1];
	U32 chars = GetModuleFileNameW(NULL, buff, MAX_PATH);

	if(!chars || chars >= MAX_PATH)
		retError(clean, Error_platformError(
			0, GetLastError(), "Platform_initExt() GetModuleFileName failed"
		))

	buff[chars] = 0;

	U64 lastSlash = U64_MAX;

	for (U64 i = 0; i < chars; ++i) {

		Bool slash = buff[i] == '\\';

		if (slash)
			buff[i] = '/';

		else if (buff[i] == '/')
			slash = true;

		if (slash)
			lastSlash = i;
	}

	if(lastSlash == U64_MAX)
		retError(clean, Error_invalidState(0, "Platform_initExt() couldn't find exe name"))

	buff[lastSlash + 1] = '\0';
	gotoIfError2(clean, CharString_createFromUTF16x((const U16*)buff, lastSlash + 1, &Platform_instance->appDirectory))

	SetDllDirectoryW(buff);

	//Init working dir

	chars = GetCurrentDirectoryW(MAX_PATH + 1, buff);

	if(!chars || chars >= MAX_PATH)
		retError(clean, Error_platformError(
			0, GetLastError(), "Platform_initExt() GetCurrentDirectory failed"
		))

	buff[chars] = 0;

	gotoIfError2(clean, CharString_createFromUTF16x((const U16*)buff, chars, &Platform_instance->workDirectory))

	CharString_replaceAllSensitive(&Platform_instance->workDirectory, '\\', '/', 0, 0);
	gotoIfError2(clean, CharString_appendx(&Platform_instance->workDirectory, '/'))

	//Make default path

	Platform_instance->defaultDir = CharString_createRefStrConst(
		Platform_instance->useWorkingDir ? Platform_instance->workDirectory : Platform_instance->appDirectory
	);

	//Init virtual files

	EnumerateFiles files = (EnumerateFiles) { .sections = &Platform_instance->virtualSections };

	if (!EnumResourceNamesW(
		NULL, RT_RCDATA,
		(ENUMRESNAMEPROCW)enumerateFiles,
		(LONG_PTR)&files
	)) {

		//Enum resource names also fails if we don't have any resources.
		//To counter this, enumerateFiles sets stride to 0 if the reason it returned false was because of the function.

		if(files.b)
			retError(clean, Error_invalidState(1, "Platform_initExt() EnumResourceNames failed"))
	}

clean:
	return s_uccess;
}

CharString Keyboard_remap(EKey key) {

	U32 vkey = 0, scanCode = 0;
	const C8 *raw = NULL;

	switch(key) {

		case EKey_PrintScreen:	raw = "Print screen";			break;		//TODO: Weird behavior
		case EKey_ScrollLock:	vkey = VK_SCROLL;				break;
		case EKey_NumLock:		vkey = VK_NUMLOCK;				break;
		case EKey_Pause:		raw = "Break";					break;		//TODO: Weird behavior
		case EKey_Insert:		vkey = VK_INSERT;				break;
		case EKey_Home:			vkey = VK_HOME;					break;
		case EKey_PageUp:		vkey = VK_PRIOR;				break;
		case EKey_PageDown:		vkey = VK_NEXT;					break;
		case EKey_Delete:		vkey = VK_DELETE;				break;
		case EKey_End:			vkey = VK_END;					break;

		case EKey_Up:			vkey = VK_UP;					break;
		case EKey_Left:			vkey = VK_LEFT;					break;
		case EKey_Down:			vkey = VK_DOWN;					break;
		case EKey_Right:		vkey = VK_RIGHT;				break;

		case EKey_Select:		vkey = VK_SELECT;				break;
		case EKey_Print:		vkey = VK_PRINT;				break;
		case EKey_Execute:		vkey = VK_EXECUTE;				break;
		case EKey_Back:			vkey = VK_BROWSER_BACK;			break;
		case EKey_Forward:		vkey = VK_BROWSER_FORWARD;		break;
		case EKey_Sleep:		vkey = VK_SLEEP;				break;
		case EKey_Refresh:		vkey = VK_BROWSER_REFRESH;		break;
		case EKey_Stop:			vkey = VK_BROWSER_STOP;			break;
		case EKey_Search:		vkey = VK_BROWSER_SEARCH;		break;
		case EKey_Favorites:	vkey = VK_BROWSER_FAVORITES;	break;
		case EKey_Start:		vkey = VK_BROWSER_HOME;			break;
		case EKey_Mute:			vkey = VK_VOLUME_MUTE;			break;
		case EKey_VolumeDown:	vkey = VK_VOLUME_DOWN;			break;
		case EKey_VolumeUp:		vkey = VK_VOLUME_UP;			break;
		case EKey_Skip:			vkey = VK_MEDIA_NEXT_TRACK;		break;
		case EKey_Previous:		vkey = VK_MEDIA_PREV_TRACK;		break;
		case EKey_Clear:		vkey = VK_CLEAR;				break;
		case EKey_Zoom:			vkey = VK_ZOOM;					break;
		case EKey_Enter:		vkey = VK_RETURN;				break;
		case EKey_Help:			vkey = VK_HELP;					break;
		case EKey_Apps:			vkey = VK_APPS;					break;

		case EKey_NumpadMul:	vkey = VK_MULTIPLY;				break;
		case EKey_NumpadAdd:	vkey = VK_ADD;					break;
		case EKey_NumpadDot:	vkey = VK_DECIMAL;				break;
		case EKey_NumpadDiv:	vkey = VK_DIVIDE;				break;
		case EKey_NumpadSub:	vkey = VK_SUBTRACT;				break;

		case EKey_Numpad0:		vkey = VK_NUMPAD0;				break;
		case EKey_Numpad1:		vkey = VK_NUMPAD1;				break;
		case EKey_Numpad2:		vkey = VK_NUMPAD2;				break;
		case EKey_Numpad3:		vkey = VK_NUMPAD3;				break;
		case EKey_Numpad4:		vkey = VK_NUMPAD4;				break;
		case EKey_Numpad5:		vkey = VK_NUMPAD5;				break;
		case EKey_Numpad6:		vkey = VK_NUMPAD6;				break;
		case EKey_Numpad7:		vkey = VK_NUMPAD7;				break;
		case EKey_Numpad8:		vkey = VK_NUMPAD8;				break;
		case EKey_Numpad9:		vkey = VK_NUMPAD9;				break;

		//Row 0

		case EKey_Escape:		scanCode = 0x01;				break;

		case EKey_F1:			scanCode = 0x3B;				break;
		case EKey_F2:			scanCode = 0x3C;				break;
		case EKey_F3:			scanCode = 0x3D;				break;
		case EKey_F4:			scanCode = 0x3E;				break;
		case EKey_F5:			scanCode = 0x3F;				break;
		case EKey_F6:			scanCode = 0x40;				break;
		case EKey_F7:			scanCode = 0x41;				break;
		case EKey_F8:			scanCode = 0x42;				break;
		case EKey_F9:			scanCode = 0x43;				break;
		case EKey_F10:			scanCode = 0x44;				break;
		case EKey_F11:			scanCode = 0x57;				break;
		case EKey_F12:			scanCode = 0x58;				break;

		//Row 1

		case EKey_Backtick:		scanCode = 0x29;				break;
		case EKey_1:			scanCode = 0x02;				break;
		case EKey_2:			scanCode = 0x03;				break;
		case EKey_3:			scanCode = 0x04;				break;
		case EKey_4:			scanCode = 0x05;				break;
		case EKey_5:			scanCode = 0x06;				break;
		case EKey_6:			scanCode = 0x07;				break;
		case EKey_7:			scanCode = 0x08;				break;
		case EKey_8:			scanCode = 0x09;				break;
		case EKey_9:			scanCode = 0x0A;				break;

		case EKey_0:			scanCode = 0xB;					break;
		case EKey_Minus:		scanCode = 0xC;					break;
		case EKey_Equals:		scanCode = 0xD;					break;
		case EKey_Backspace:	scanCode = 0xE;					break;

		//Row 2

		case EKey_Tab:			scanCode = 0x0F;				break;
		case EKey_Q:			scanCode = 0x10;				break;
		case EKey_W:			scanCode = 0x11;				break;
		case EKey_E:			scanCode = 0x12;				break;
		case EKey_R:			scanCode = 0x13;				break;
		case EKey_T:			scanCode = 0x14;				break;
		case EKey_Y:			scanCode = 0x15;				break;
		case EKey_U:			scanCode = 0x16;				break;
		case EKey_I:			scanCode = 0x17;				break;
		case EKey_O:			scanCode = 0x18;				break;
		case EKey_P:			scanCode = 0x19;				break;
		case EKey_LBracket:		scanCode = 0x1A;				break;
		case EKey_RBracket:		scanCode = 0x1B;				break;

		//Row 3

		case EKey_Caps:			scanCode = 0x3A;				break;
		case EKey_A:			scanCode = 0x1E;				break;
		case EKey_S:			scanCode = 0x1F;				break;
		case EKey_D:			scanCode = 0x20;				break;
		case EKey_F:			scanCode = 0x21;				break;
		case EKey_G:			scanCode = 0x22;				break;
		case EKey_H:			scanCode = 0x23;				break;
		case EKey_J:			scanCode = 0x24;				break;
		case EKey_K:			scanCode = 0x25;				break;
		case EKey_L:			scanCode = 0x26;				break;
		case EKey_Semicolon:	scanCode = 0x27;				break;
		case EKey_Quote:		scanCode = 0x28;				break;
		case EKey_Backslash:	scanCode = 0x2B;				break;

		//Row 4

		case EKey_LShift:		scanCode = 0x2A;				break;
		case EKey_Bar:			scanCode = 0x56;				break;
		case EKey_Z:			scanCode = 0x2C;				break;
		case EKey_X:			scanCode = 0x2D;				break;
		case EKey_C:			scanCode = 0x2E;				break;
		case EKey_V:			scanCode = 0x2F;				break;
		case EKey_B:			scanCode = 0x30;				break;
		case EKey_N:			scanCode = 0x31;				break;
		case EKey_M:			scanCode = 0x32;				break;
		case EKey_Comma:		scanCode = 0x33;				break;
		case EKey_Period:		scanCode = 0x34;				break;
		case EKey_Slash:		scanCode = 0x35;				break;
		case EKey_RShift:		scanCode = 0x36;				break;

		//Row 5

		case EKey_LCtrl:		scanCode = 0x1D;				break;
		case EKey_LMenu:		scanCode = 0xE05B;				break;
		case EKey_LAlt:			scanCode = 0x38;				break;
		case EKey_Space:		scanCode = 0x39;				break;
		case EKey_RAlt:			scanCode = 0xE038;				break;
		case EKey_RMenu:		scanCode = 0xE05C;				break;
		case EKey_Options:		scanCode = 0xE05D;				break;
		case EKey_RCtrl:		scanCode = 0xE01D;				break;

		//Unknown key

		default:				break;
	}

	if(raw)
		return CharString_createRefCStrConst(raw);

	if(!scanCode && vkey)
		scanCode = MapVirtualKeyExW(vkey, MAPVK_VK_TO_VSC_EX, 0);

	//Special keys that need some hackery with the scan codes
	//https://www.setnode.com/blog/mapvirtualkey-getkeynametext-and-a-story-of-how-to/
	switch (vkey) {

		case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
		case VK_PRIOR: case VK_NEXT:
		case VK_END: case VK_HOME:
		case VK_INSERT: case VK_DELETE:
		case VK_DIVIDE:
		case VK_NUMLOCK:
			scanCode |= 0x100;
			break;

		default:
			break;
	}

	if(!scanCode)
		return CharString_createNull();

	wchar_t name[32];
	I32 res = GetKeyNameTextW(scanCode << 16, name, sizeof(name));

	if(res > 0) {

		CharString tmp = CharString_createNull();
		Error err = CharString_createFromUTF16x((const U16*)name, res, &tmp);

		if(!err.genericError)
			return tmp;
	}

	return CharString_createNull();
}
