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

//platforms/linux/lplatform.c

#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE

#include "types/container/list_impl.h"
#include "platforms/platform.h"
#include "platforms/window.h"
#include "platforms/linux/lwindow_structs.h"
#include "platforms/keyboard.h"
#include "types/container/string.h"
#include "types/container/file_base.h"
#include "types/base/thread.h"
#include "types/base/error.h"
#include "types/base/string_read_helper.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <elf.h>
#include <xkbcommon/xkbcommon.h>
#include <linux/input-event-codes.h>

Bool Platform_initUnixExt(Error *e_rr) {

	Bool s_uccess = true;
	CharString tmpStr = CharString_createNull();

	//Grab exe name first, so we can find all sections that exist

	C8 exeName[MAX_OXC_PATH + 1];
	I32 fd = -1;
	C8 *ptr = NULL;
	U64 fileSize = 0;
	I32 exeNameLen = readlink("/proc/self/exe", exeName, sizeof(exeName) - 1);

	if(exeNameLen < 0)
		retError(clean, Error_invalidState(0, "Platform_initUnixExt() couldn't find out executable name"));

	exeName[exeNameLen] = '\0';

	Bool containedSlash = false;

	for(U64 i = exeNameLen - 1; i != U64_MAX; --i)
		if(exeName[i] == '/') {
			containedSlash = true;
			exeNameLen = i + 1;
			break;
		}

	if(!containedSlash)
		retError(clean, Error_invalidState(0, "Platform_initUnixExt() couldn't find app base path"));

	CharString appDir = CharString_createRefSizedConst(exeName, (U64)exeNameLen, false);

	gotoIfError3(clean, CharString_createCopy(appDir, Platform_instance->alloc, &Platform_instance->appDirectory, e_rr));

	//Try to open the main executable within 1s, if it fails we can't init

	U64 i = 0;

	for(; i < 1000 && (fd = open(exeName, O_RDONLY)) < 0; ++i) {

		if(errno != EINTR)
			retError(clean, Error_stderr(0, "Platform_initUnixExt() open failed on executable"));

		Thread_sleep(MS);
	}

	if(i == 1000)
		retError(clean, Error_invalidState(0, "Platform_initUnixExt() executable couldn't be opened in time"));

	//Grab file data

	fileSize = lseek(fd, 0, SEEK_END);
	ptr = (C8*) mmap(NULL, fileSize, PROT_READ, MAP_SHARED, fd, 0);

	if(ptr == (const C8*) MAP_FAILED)
		retError(clean, Error_invalidState(0, "Platform_initUnixExt() executable couldn't be mapped"));

	//Read sections

	Bool anySection = false;

	const Elf64_Ehdr *elf = (const Elf64_Ehdr*) ptr;
	const Elf64_Shdr *shdr = (const Elf64_Shdr*) (ptr + elf->e_shoff);
	const C8 *strings = (const C8*) (ptr + shdr[elf->e_shstrndx].sh_offset);

	const CharString packages = CharString_createRefCStrConst("packages/");

	for(U64 i = 0; i < elf->e_shnum; ++i) {

		CharString sectionName = CharString_createRefCStrConst(&strings[shdr[i].sh_name]);

		if(!CharString_startsWithStringSensitive(&sectionName, &packages, 0))
			continue;

		sectionName.ptr += sizeof("packages");        //sizeof includes null terminator so no need for packages/
		sectionName.lenAndNullTerminated -= sizeof("packages");

		gotoIfError3(clean, CharString_createCopy(sectionName, Platform_instance->alloc, &tmpStr, e_rr));

		VirtualSection section = (VirtualSection) { .path = tmpStr };
		section.lenExt = shdr[i].sh_size;
		section.dataExt = ptr + shdr[i].sh_offset;

		gotoIfError3(clean, ListVirtualSection_pushBack(
			&Platform_instance->virtualSections, section, Platform_instance->alloc, e_rr
		));

		tmpStr = CharString_createNull();
		anySection = true;
	}

	//Keep file open until end of program.
	//Unless there's no need (when there's no sections present).
	//This doesn't keep anything in memory, until we actually load the sections.

	if(anySection) {
		Platform_instance->data = (void*) (U64) fd;
		Platform_instance->data1 = ptr;
		Platform_instance->size1 = fileSize;
		fd = -1;
		ptr = NULL;
	}

clean:

	if(fd >= 0)
		close(fd);

	if(ptr)
		munmap(ptr, fileSize);

	CharString_free(&tmpStr, Platform_instance->alloc);
	return s_uccess;
}

void Platform_cleanupUnixExt() {
	if(Platform_instance->data1) {
		munmap(Platform_instance->data1, Platform_instance->size1);
		close((I32)(U64) Platform_instance->data);
	}
}

static const U16 EKey_toScanCode[EKey_Count] = {
	/* EKey_0 */            KEY_0,
	/* EKey_1 */            KEY_1,
	/* EKey_2 */            KEY_2,
	/* EKey_3 */            KEY_3,
	/* EKey_4 */            KEY_4,
	/* EKey_5 */            KEY_5,
	/* EKey_6 */            KEY_6,
	/* EKey_7 */            KEY_7,
	/* EKey_8 */            KEY_8,
	/* EKey_9 */            KEY_9,
	/* EKey_A */            KEY_A,
	/* EKey_B */            KEY_B,
	/* EKey_C */            KEY_C,
	/* EKey_D */            KEY_D,
	/* EKey_E */            KEY_E,
	/* EKey_F */            KEY_F,
	/* EKey_G */            KEY_G,
	/* EKey_H */            KEY_H,
	/* EKey_I */            KEY_I,
	/* EKey_J */            KEY_J,
	/* EKey_K */            KEY_K,
	/* EKey_L */            KEY_L,
	/* EKey_M */            KEY_M,
	/* EKey_N */            KEY_N,
	/* EKey_O */            KEY_O,
	/* EKey_P */            KEY_P,
	/* EKey_Q */            KEY_Q,
	/* EKey_R */            KEY_R,
	/* EKey_S */            KEY_S,
	/* EKey_T */            KEY_T,
	/* EKey_U */            KEY_U,
	/* EKey_V */            KEY_V,
	/* EKey_W */            KEY_W,
	/* EKey_X */            KEY_X,
	/* EKey_Y */            KEY_Y,
	/* EKey_Z */            KEY_Z,
	/* EKey_Backspace */    KEY_BACKSPACE,
	/* EKey_Space */        KEY_SPACE,
	/* EKey_Tab */          KEY_TAB,
	/* EKey_LShift */       KEY_LEFTSHIFT,
	/* EKey_LCtrl */        KEY_LEFTCTRL,
	/* EKey_LAlt */         KEY_LEFTALT,
	/* EKey_LMenu */        KEY_LEFTMETA,
	/* EKey_RShift */       KEY_RIGHTSHIFT,
	/* EKey_RCtrl */        KEY_RIGHTCTRL,
	/* EKey_RAlt */         KEY_RIGHTALT,
	/* EKey_RMenu */        KEY_RIGHTMETA,
	/* EKey_Pause */        KEY_PAUSE,
	/* EKey_Caps */         KEY_CAPSLOCK,
	/* EKey_Escape */       KEY_ESC,
	/* EKey_PageUp */       KEY_PAGEUP,
	/* EKey_PageDown */     KEY_PAGEDOWN,
	/* EKey_End */          KEY_END,
	/* EKey_Home */         KEY_HOME,
	/* EKey_PrintScreen */  KEY_SYSRQ,
	/* EKey_Insert */       KEY_INSERT,
	/* EKey_Enter */        KEY_ENTER,
	/* EKey_Delete */       KEY_DELETE,
	/* EKey_NumLock */      KEY_NUMLOCK,
	/* EKey_ScrollLock */   KEY_SCROLLLOCK,
	/* EKey_Back */         KEY_BACK,
	/* EKey_Forward */      KEY_FORWARD,
	/* EKey_Sleep */        KEY_SLEEP,
	/* EKey_Refresh */      KEY_REFRESH,
	/* EKey_Search */       KEY_SEARCH,
	/* EKey_Mute */         KEY_MUTE,
	/* EKey_VolumeDown */   KEY_VOLUMEDOWN,
	/* EKey_VolumeUp */     KEY_VOLUMEUP,
	/* EKey_Skip */         KEY_NEXTSONG,
	/* EKey_Previous */     KEY_PREVIOUSSONG,
	/* EKey_Clear */        KEY_CLEAR,
	/* EKey_Help */         KEY_HELP,
	/* EKey_Left */         KEY_LEFT,
	/* EKey_Up */           KEY_UP,
	/* EKey_Right */        KEY_RIGHT,
	/* EKey_Down */         KEY_DOWN,
	/* EKey_Numpad0 */      KEY_KP0,
	/* EKey_Numpad1 */      KEY_KP1,
	/* EKey_Numpad2 */      KEY_KP2,
	/* EKey_Numpad3 */      KEY_KP3,
	/* EKey_Numpad4 */      KEY_KP4,
	/* EKey_Numpad5 */      KEY_KP5,
	/* EKey_Numpad6 */      KEY_KP6,
	/* EKey_Numpad7 */      KEY_KP7,
	/* EKey_Numpad8 */      KEY_KP8,
	/* EKey_Numpad9 */      KEY_KP9,
	/* EKey_NumpadMul */    KEY_KPASTERISK,
	/* EKey_NumpadAdd */    KEY_KPPLUS,
	/* EKey_NumpadDot */    KEY_KPDOT,
	/* EKey_NumpadDiv */    KEY_KPSLASH,
	/* EKey_NumpadSub */    KEY_KPMINUS,
	/* EKey_F1 */           KEY_F1,
	/* EKey_F2 */           KEY_F2,
	/* EKey_F3 */           KEY_F3,
	/* EKey_F4 */           KEY_F4,
	/* EKey_F5 */           KEY_F5,
	/* EKey_F6 */           KEY_F6,
	/* EKey_F7 */           KEY_F7,
	/* EKey_F8 */           KEY_F8,
	/* EKey_F9 */           KEY_F9,
	/* EKey_F10 */          KEY_F10,
	/* EKey_F11 */          KEY_F11,
	/* EKey_F12 */          KEY_F12,
	/* EKey_Bar */          KEY_102ND,
	/* EKey_Options */      KEY_COMPOSE,
	/* EKey_Equals */       KEY_EQUAL,
	/* EKey_Comma */        KEY_COMMA,
	/* EKey_Minus */        KEY_MINUS,
	/* EKey_Period */       KEY_DOT,
	/* EKey_Slash */        KEY_SLASH,
	/* EKey_Backtick */     KEY_GRAVE,
	/* EKey_Semicolon */    KEY_SEMICOLON,
	/* EKey_LBracket */     KEY_LEFTBRACE,
	/* EKey_RBracket */     KEY_RIGHTBRACE,
	/* EKey_Backslash */    KEY_BACKSLASH,
	/* EKey_Quote */        KEY_APOSTROPHE
};

Bool Keyboard_remap(const Keyboard *keyboard, EKey key, const Allocator *alloc, CharString *result, Error *e_rr) {

	(void)keyboard;
	Bool s_uccess = true;

	if((U32)key >= EKey_Count)
		retError(clean, Error_outOfBounds(0, (U64)key, EKey_Count, "Keyboard_remap() key out of range"));

	U16 scanCode = EKey_toScanCode[key];

	if(!keyboard->dataExt.ptr)
		retError(clean, Error_invalidOperation(0, "Keyboard_remap() called on InputDevice without owner (Window)"));

	struct xkb_state *xkbState = ((LWindow*)(keyboard->dataExt.ptr))->xkbState;

	if(!xkbState)
		retError(clean, Error_invalidOperation(0, "Keyboard_remap() called on Window without xkbState"));

	C8 utf8[8] = { 0 };
	I32 len = xkb_state_key_get_utf8(xkbState, (xkb_keycode_t)(scanCode + 8), utf8, sizeof(utf8) - 1);

	if(len <= 0 || !utf8[0])
		retError(clean, Error_notFound(0, 0, "Keyboard_remap() couldn't be translated"));

	gotoIfError3(clean, CharString_createCopy(CharString_createRefCStrConst(utf8), alloc, result, e_rr));

clean:
	return s_uccess;
}
