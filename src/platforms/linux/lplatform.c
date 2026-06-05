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

// XKB keycode = Linux evdev keycode + 8
#define XKB(k) ((xkb_keycode_t)((k) + 8))
#define XKB_INVALID ((xkb_keycode_t)0)

static const xkb_keycode_t EKey_toXKB[EKey_Count] = {
	/* EKey_0 */            XKB(KEY_0),
	/* EKey_1 */            XKB(KEY_1),
	/* EKey_2 */            XKB(KEY_2),
	/* EKey_3 */            XKB(KEY_3),
	/* EKey_4 */            XKB(KEY_4),
	/* EKey_5 */            XKB(KEY_5),
	/* EKey_6 */            XKB(KEY_6),
	/* EKey_7 */            XKB(KEY_7),
	/* EKey_8 */            XKB(KEY_8),
	/* EKey_9 */            XKB(KEY_9),
	/* EKey_A */            XKB(KEY_A),
	/* EKey_B */            XKB(KEY_B),
	/* EKey_C */            XKB(KEY_C),
	/* EKey_D */            XKB(KEY_D),
	/* EKey_E */            XKB(KEY_E),
	/* EKey_F */            XKB(KEY_F),
	/* EKey_G */            XKB(KEY_G),
	/* EKey_H */            XKB(KEY_H),
	/* EKey_I */            XKB(KEY_I),
	/* EKey_J */            XKB(KEY_J),
	/* EKey_K */            XKB(KEY_K),
	/* EKey_L */            XKB(KEY_L),
	/* EKey_M */            XKB(KEY_M),
	/* EKey_N */            XKB(KEY_N),
	/* EKey_O */            XKB(KEY_O),
	/* EKey_P */            XKB(KEY_P),
	/* EKey_Q */            XKB(KEY_Q),
	/* EKey_R */            XKB(KEY_R),
	/* EKey_S */            XKB(KEY_S),
	/* EKey_T */            XKB(KEY_T),
	/* EKey_U */            XKB(KEY_U),
	/* EKey_V */            XKB(KEY_V),
	/* EKey_W */            XKB(KEY_W),
	/* EKey_X */            XKB(KEY_X),
	/* EKey_Y */            XKB(KEY_Y),
	/* EKey_Z */            XKB(KEY_Z),
	/* EKey_Backspace */    XKB(KEY_BACKSPACE),
	/* EKey_Space */        XKB(KEY_SPACE),
	/* EKey_Tab */          XKB(KEY_TAB),
	/* EKey_LShift */       XKB(KEY_LEFTSHIFT),
	/* EKey_LCtrl */        XKB(KEY_LEFTCTRL),
	/* EKey_LAlt */         XKB(KEY_LEFTALT),
	/* EKey_LMenu */        XKB(KEY_LEFTMETA),
	/* EKey_RShift */       XKB(KEY_RIGHTSHIFT),
	/* EKey_RCtrl */        XKB(KEY_RIGHTCTRL),
	/* EKey_RAlt */         XKB(KEY_RIGHTALT),
	/* EKey_RMenu */        XKB(KEY_RIGHTMETA),
	/* EKey_Pause */        XKB(KEY_PAUSE),
	/* EKey_Caps */         XKB(KEY_CAPSLOCK),
	/* EKey_Escape */       XKB(KEY_ESC),
	/* EKey_PageUp */       XKB(KEY_PAGEUP),
	/* EKey_PageDown */     XKB(KEY_PAGEDOWN),
	/* EKey_End */          XKB(KEY_END),
	/* EKey_Home */         XKB(KEY_HOME),
	/* EKey_PrintScreen */  XKB(KEY_SYSRQ),
	/* EKey_Insert */       XKB(KEY_INSERT),
	/* EKey_Enter */        XKB(KEY_ENTER),
	/* EKey_Delete */       XKB(KEY_DELETE),
	/* EKey_NumLock */      XKB(KEY_NUMLOCK),
	/* EKey_ScrollLock */   XKB(KEY_SCROLLLOCK),
	/* EKey_Back */         XKB(KEY_BACK),
	/* EKey_Forward */      XKB(KEY_FORWARD),
	/* EKey_Sleep */        XKB(KEY_SLEEP),
	/* EKey_Refresh */      XKB(KEY_REFRESH),
	/* EKey_Search */       XKB(KEY_SEARCH),
	/* EKey_Mute */         XKB(KEY_MUTE),
	/* EKey_VolumeDown */   XKB(KEY_VOLUMEDOWN),
	/* EKey_VolumeUp */     XKB(KEY_VOLUMEUP),
	/* EKey_Skip */         XKB(KEY_NEXTSONG),
	/* EKey_Previous */     XKB(KEY_PREVIOUSSONG),
	/* EKey_Clear */        XKB(KEY_CLEAR),
	/* EKey_Help */         XKB(KEY_HELP),
	/* EKey_Left */         XKB(KEY_LEFT),
	/* EKey_Up */           XKB(KEY_UP),
	/* EKey_Right */        XKB(KEY_RIGHT),
	/* EKey_Down */         XKB(KEY_DOWN),
	/* EKey_Numpad0 */      XKB(KEY_KP0),
	/* EKey_Numpad1 */      XKB(KEY_KP1),
	/* EKey_Numpad2 */      XKB(KEY_KP2),
	/* EKey_Numpad3 */      XKB(KEY_KP3),
	/* EKey_Numpad4 */      XKB(KEY_KP4),
	/* EKey_Numpad5 */      XKB(KEY_KP5),
	/* EKey_Numpad6 */      XKB(KEY_KP6),
	/* EKey_Numpad7 */      XKB(KEY_KP7),
	/* EKey_Numpad8 */      XKB(KEY_KP8),
	/* EKey_Numpad9 */      XKB(KEY_KP9),
	/* EKey_NumpadMul */    XKB(KEY_KPASTERISK),
	/* EKey_NumpadAdd */    XKB(KEY_KPPLUS),
	/* EKey_NumpadDot */    XKB(KEY_KPDOT),
	/* EKey_NumpadDiv */    XKB(KEY_KPSLASH),
	/* EKey_NumpadSub */    XKB(KEY_KPMINUS),
	/* EKey_F1 */           XKB(KEY_F1),
	/* EKey_F2 */           XKB(KEY_F2),
	/* EKey_F3 */           XKB(KEY_F3),
	/* EKey_F4 */           XKB(KEY_F4),
	/* EKey_F5 */           XKB(KEY_F5),
	/* EKey_F6 */           XKB(KEY_F6),
	/* EKey_F7 */           XKB(KEY_F7),
	/* EKey_F8 */           XKB(KEY_F8),
	/* EKey_F9 */           XKB(KEY_F9),
	/* EKey_F10 */          XKB(KEY_F10),
	/* EKey_F11 */          XKB(KEY_F11),
	/* EKey_F12 */          XKB(KEY_F12),
	/* EKey_Bar */          XKB(KEY_102ND),
	/* EKey_Options */      XKB(KEY_COMPOSE),
	/* EKey_Equals */       XKB(KEY_EQUAL),
	/* EKey_Comma */        XKB(KEY_COMMA),
	/* EKey_Minus */        XKB(KEY_MINUS),
	/* EKey_Period */       XKB(KEY_DOT),
	/* EKey_Slash */        XKB(KEY_SLASH),
	/* EKey_Backtick */     XKB(KEY_GRAVE),
	/* EKey_Semicolon */    XKB(KEY_SEMICOLON),
	/* EKey_LBracket */     XKB(KEY_LEFTBRACE),
	/* EKey_RBracket */     XKB(KEY_RIGHTBRACE),
	/* EKey_Backslash */    XKB(KEY_BACKSLASH),
	/* EKey_Quote */        XKB(KEY_APOSTROPHE),
};

#undef XKB

Bool Keyboard_remap(const Keyboard *keyboard, EKey key, const Allocator *alloc, CharString *result, Error *e_rr) {

	(void)keyboard;
	Bool s_uccess = true;

	struct xkb_context *ctx   = NULL;
	struct xkb_keymap *keymap = NULL;
	xkb_keycode_t keycode;
	const xkb_keysym_t *syms  = NULL;
	xkb_keysym_t sym;
	C8 buf[64];
	Bool gotUtf8 = false;
	int nsyms, len;

	if((U32)key >= EKey_Count)
		retError(clean, Error_outOfBounds(0, (U64)key, EKey_Count, "Keyboard_remap() key out of range"));

	keycode = EKey_toXKB[key];

	ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if(!ctx)
		retError(clean, Error_outOfMemory(0, "Keyboard_remap() xkb_context_new failed"));

	keymap = xkb_keymap_new_from_names(ctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if(!keymap)
		retError(clean, Error_invalidState(0, "Keyboard_remap() xkb_keymap_new_from_names failed"));

	nsyms = xkb_keymap_key_get_syms_by_level(keymap, keycode, 0, 0, &syms);

	if(nsyms <= 0 || !syms)
		retError(clean, Error_notFound(0, 0, "Keyboard_remap() no keysym for key"));

	sym = syms[0];

	if(sym != XKB_KEY_NoSymbol) {
		len = xkb_keysym_to_utf8(sym, buf, sizeof(buf));
		if(len > 1) {
			buf[len] = '\0';
			gotUtf8 = true;
		}
	}

	if(!gotUtf8) {
		len = xkb_keysym_get_name(sym, buf, sizeof(buf));
		if(len <= 0)
			retError(clean, Error_notFound(1, 0, "Keyboard_remap() xkb_keysym_get_name failed"));

		buf[len] = '\0';
	}

	gotoIfError3(clean, CharString_createCopy(CharString_createRefCStrConst(buf), alloc, result, e_rr));

clean:
	if(keymap) xkb_keymap_unref(keymap);
	if(ctx)    xkb_context_unref(ctx);
	return s_uccess;
}
