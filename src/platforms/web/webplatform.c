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

//platforms/web/webplatform.c
//
//The web (emscripten/wasm64) flavor of the unix platform.
//No app bundle and no readable executable here (lplatform mmaps /proc/self/exe, aplatform reads the apk),
// so virtual file discovery follows the android model instead: scan packages/<target>/<name>.oiCA
// on the mounted filesystem (NODERAWFS under node, MEMFS/preloaded in a browser) and stream them on demand.
//See webfile.c for the streaming half.
//cwd doubles as the app directory: bundle and packages/ ship side by side.

#include "platforms/platform.h"
#include "platforms/keyboard.h"
#include "platforms/input_device.h"
#include "platforms/logx.h"
#include "types/container/string.h"
#include "types/base/error.h"

#include <dirent.h>
#include <emscripten.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

Bool Platform_initUnixExt(Error *e_rr) {

	Bool s_uccess = true;
	DIR *dir = NULL;
	DIR *subDir = NULL;
	CharString tmpPath = CharString_createNull();
	CharString filePath = CharString_createNull();
	CharString sectionPath = CharString_createNull();

	//uplatform.c already set workDirectory to cwd; the web target has no separate install location.

	gotoIfError3(clean, CharString_createCopy(
		Platform_instance->workDirectory, Platform_instance->alloc, &Platform_instance->appDirectory, e_rr
	));

	//Discover virtual sections.
	//Same layout the packager writes everywhere: packages/<target>/<name>.oiCA,
	// section path stored extension-less as "<target>/<name>" (matches lplatform/aplatform).

	dir = opendir("packages");

	if(!dir)
		Log_warnLnx("OxC3 packages folder not found (missing virtual files), OK if not built through OxC3 completely");

	else {

		struct dirent *ent = NULL;

		while ((ent = readdir(dir)) != NULL) {

			if(ent->d_name[0] == '.')
				continue;

			CharString_free(&tmpPath, Platform_instance->alloc);
			gotoIfError3(clean, CharString_format(
				Platform_instance->alloc, &tmpPath, e_rr, "packages/%s", ent->d_name
			));

			subDir = opendir(tmpPath.ptr);

			if(!subDir) //Plain file at the packages root; not ours
				continue;

			struct dirent *sub = NULL;

			while ((sub = readdir(subDir)) != NULL) {

				const U64 nameLen = CharString_calcStrLen(sub->d_name, U64_MAX);

				if(sub->d_name[0] == '.' || nameLen < 5 || strcasecmp(sub->d_name + nameLen - 5, ".oica"))
					continue;

				CharString_free(&filePath, Platform_instance->alloc);
				gotoIfError3(clean, CharString_format(
					Platform_instance->alloc, &filePath, e_rr, "packages/%s/%s", ent->d_name, sub->d_name
				));

				struct stat st;

				if(stat(filePath.ptr, &st) || st.st_size < 0)
					retError(clean, Error_invalidState(0, "Platform_initUnixExt() couldn't stat package"));

				gotoIfError3(clean, CharString_format(
					Platform_instance->alloc, &sectionPath, e_rr, "%s/%.*s",
					ent->d_name, (int)(nameLen - 5), sub->d_name
				));

				VirtualSection virtualSection = (VirtualSection) { .path = sectionPath };
				virtualSection.lenExt = (U64) st.st_size;

				gotoIfError3(clean, ListVirtualSection_pushBack(
					&Platform_instance->virtualSections, virtualSection, Platform_instance->alloc, e_rr
				));

				sectionPath = CharString_createNull(); //Owned by the list now
			}

			closedir(subDir);
			subDir = NULL;
		}
	}

clean:

	if(!s_uccess) {
		Log_errorLnx("Couldn't initialize app, encountered an issue");
		if(e_rr) Error_print(Platform_instance->alloc, e_rr, ELogLevel_Error, ELogOptions_Default);
	}

	if(subDir) closedir(subDir);
	if(dir) closedir(dir);
	CharString_free(&sectionPath, Platform_instance->alloc);
	CharString_free(&filePath, Platform_instance->alloc);
	CharString_free(&tmpPath, Platform_instance->alloc);
	return s_uccess;
}

void Platform_cleanupUnixExt() { }

//The Keyboard Map API is the only way a page can read the physical layout:
// KeyboardEvent.key alone reports what a keystroke produced, not what the key is labelled with.
//navigator.keyboard.getLayoutMap() maps a KeyboardEvent.code to that label, but it is Chromium only,
// it is a promise, and navigator.keyboard isn't exposed inside a Worker.
//So the map is fetched once and cached on Module, the way host_crypto.c caches its bridge state,
// and Keyboard_remap stays a synchronous lookup that never waits on it.
//Module is per thread (every emscripten pthread is its own Worker with its own Module),
// which makes the worker case fall out for free: the probe there finds no API and the caller keeps the US label.

//Starts the one time fetch and reports whether the API exists at all.
//Idempotent, since the cache on Module is the guard, so callers don't track whether it already ran.

EM_JS(int, oxc3_keyboardLayoutInit, (), {

	if (Module._oxc3KeyboardLayout !== undefined)
		return Module._oxc3KeyboardLayout ? 1 : 0;

	Module._oxc3KeyboardLayout = null;

	try {

		//Missing under node (no DOM), in Firefox and Safari, and in any Worker.
		if (typeof navigator === 'undefined' || !navigator.keyboard || !navigator.keyboard.getLayoutMap)
			return 0;

		const st = { map: null };
		Module._oxc3KeyboardLayout = st;

		//Nothing awaits this: lookups before it resolves fall back to the US label,
		// and a rejection simply leaves the map null so they keep doing so.
		navigator.keyboard.getLayoutMap().then(function(map) { st.map = map; }, function() { });

		return 1;

	} catch (e) {
		Module._oxc3KeyboardLayout = null;
		return 0;
	}
});

//Writes the localized label for a KeyboardEvent.code into out and returns how many bytes it wrote.
//Returns 0 when the map hasn't resolved yet or doesn't carry that code; the caller then uses the US label.

EM_JS(int, oxc3_keyboardLayoutLabel, (unsigned long long codePtr, unsigned long long outPtr, unsigned int outLen), {

	const st = Module._oxc3KeyboardLayout;

	if (!st || !st.map)
		return 0;

	//EM_JS emits the body verbatim, so a wasm64 pointer arrives as a BigInt and has to become a Number
	// before anything indexes the heap with it (same reason host_crypto.c does it).

	const label = st.map.get(UTF8ToString(Number(codePtr)));

	if (!label)
		return 0;

	//Encoded before it is written, rather than streamed out, so a label that doesn't fit is dropped
	// instead of being truncated halfway through a codepoint.

	const bytes = new TextEncoder().encode(label);

	if (!bytes.length || bytes.length > outLen)
		return 0;

	HEAPU8.set(bytes, Number(outPtr));
	return bytes.length;
});

//EKey is defined by US QWERTY ISO scan code positions and KeyboardEvent.code is positional too,
// so the two line up one to one and no layout is involved in this table.
//An empty string marks an EKey with no matching code (Clear has no position of its own),
// which skips the layout map and lands on the label below.
static const C8 *EKey_toEventCode[EKey_Count] = {
	/* EKey_0 */            "Digit0",
	/* EKey_1 */            "Digit1",
	/* EKey_2 */            "Digit2",
	/* EKey_3 */            "Digit3",
	/* EKey_4 */            "Digit4",
	/* EKey_5 */            "Digit5",
	/* EKey_6 */            "Digit6",
	/* EKey_7 */            "Digit7",
	/* EKey_8 */            "Digit8",
	/* EKey_9 */            "Digit9",
	/* EKey_A */            "KeyA",
	/* EKey_B */            "KeyB",
	/* EKey_C */            "KeyC",
	/* EKey_D */            "KeyD",
	/* EKey_E */            "KeyE",
	/* EKey_F */            "KeyF",
	/* EKey_G */            "KeyG",
	/* EKey_H */            "KeyH",
	/* EKey_I */            "KeyI",
	/* EKey_J */            "KeyJ",
	/* EKey_K */            "KeyK",
	/* EKey_L */            "KeyL",
	/* EKey_M */            "KeyM",
	/* EKey_N */            "KeyN",
	/* EKey_O */            "KeyO",
	/* EKey_P */            "KeyP",
	/* EKey_Q */            "KeyQ",
	/* EKey_R */            "KeyR",
	/* EKey_S */            "KeyS",
	/* EKey_T */            "KeyT",
	/* EKey_U */            "KeyU",
	/* EKey_V */            "KeyV",
	/* EKey_W */            "KeyW",
	/* EKey_X */            "KeyX",
	/* EKey_Y */            "KeyY",
	/* EKey_Z */            "KeyZ",
	/* EKey_Backspace */    "Backspace",
	/* EKey_Space */        "Space",
	/* EKey_Tab */          "Tab",
	/* EKey_LShift */       "ShiftLeft",
	/* EKey_LCtrl */        "ControlLeft",
	/* EKey_LAlt */         "AltLeft",
	/* EKey_LMenu */        "MetaLeft",
	/* EKey_RShift */       "ShiftRight",
	/* EKey_RCtrl */        "ControlRight",
	/* EKey_RAlt */         "AltRight",
	/* EKey_RMenu */        "MetaRight",
	/* EKey_Pause */        "Pause",
	/* EKey_Caps */         "CapsLock",
	/* EKey_Escape */       "Escape",
	/* EKey_PageUp */       "PageUp",
	/* EKey_PageDown */     "PageDown",
	/* EKey_End */          "End",
	/* EKey_Home */         "Home",
	/* EKey_PrintScreen */  "PrintScreen",
	/* EKey_Insert */       "Insert",
	/* EKey_Enter */        "Enter",
	/* EKey_Delete */       "Delete",
	/* EKey_NumLock */      "NumLock",
	/* EKey_ScrollLock */   "ScrollLock",
	/* EKey_Back */         "BrowserBack",
	/* EKey_Forward */      "BrowserForward",
	/* EKey_Sleep */        "Sleep",
	/* EKey_Refresh */      "BrowserRefresh",
	/* EKey_Search */       "BrowserSearch",
	/* EKey_Mute */         "AudioVolumeMute",
	/* EKey_VolumeDown */   "AudioVolumeDown",
	/* EKey_VolumeUp */     "AudioVolumeUp",
	/* EKey_Skip */         "MediaTrackNext",
	/* EKey_Previous */     "MediaTrackPrevious",
	/* EKey_Clear */        "",
	/* EKey_Help */         "Help",
	/* EKey_Left */         "ArrowLeft",
	/* EKey_Up */           "ArrowUp",
	/* EKey_Right */        "ArrowRight",
	/* EKey_Down */         "ArrowDown",
	/* EKey_Numpad0 */      "Numpad0",
	/* EKey_Numpad1 */      "Numpad1",
	/* EKey_Numpad2 */      "Numpad2",
	/* EKey_Numpad3 */      "Numpad3",
	/* EKey_Numpad4 */      "Numpad4",
	/* EKey_Numpad5 */      "Numpad5",
	/* EKey_Numpad6 */      "Numpad6",
	/* EKey_Numpad7 */      "Numpad7",
	/* EKey_Numpad8 */      "Numpad8",
	/* EKey_Numpad9 */      "Numpad9",
	/* EKey_NumpadMul */    "NumpadMultiply",
	/* EKey_NumpadAdd */    "NumpadAdd",
	/* EKey_NumpadDot */    "NumpadDecimal",
	/* EKey_NumpadDiv */    "NumpadDivide",
	/* EKey_NumpadSub */    "NumpadSubtract",
	/* EKey_F1 */           "F1",
	/* EKey_F2 */           "F2",
	/* EKey_F3 */           "F3",
	/* EKey_F4 */           "F4",
	/* EKey_F5 */           "F5",
	/* EKey_F6 */           "F6",
	/* EKey_F7 */           "F7",
	/* EKey_F8 */           "F8",
	/* EKey_F9 */           "F9",
	/* EKey_F10 */          "F10",
	/* EKey_F11 */          "F11",
	/* EKey_F12 */          "F12",
	/* EKey_Bar */          "IntlBackslash",
	/* EKey_Options */      "ContextMenu",
	/* EKey_Equals */       "Equal",
	/* EKey_Comma */        "Comma",
	/* EKey_Minus */        "Minus",
	/* EKey_Period */       "Period",
	/* EKey_Slash */        "Slash",
	/* EKey_Backtick */     "Backquote",
	/* EKey_Semicolon */    "Semicolon",
	/* EKey_LBracket */     "BracketLeft",
	/* EKey_RBracket */     "BracketRight",
	/* EKey_Backslash */    "Backslash",
	/* EKey_Quote */        "Quote"
};

//What a US QWERTY key is printed with, used whenever the layout map can't answer.
//These are what an options screen shows, so they name the key ("Left shift") rather than what it types.
Bool Keyboard_remap(const Keyboard *keyboard, EKey key, const Allocator *alloc, CharString *result, Error *e_rr) {

	Bool s_uccess = true;
	C8 label[64] = { 0 };

	if(!keyboard)
		retError(clean, Error_nullPointer(0, "Keyboard_remap()::keyboard is required"));

	if((U32) key >= EKey_Count)
		retError(clean, Error_outOfBounds(1, (U64) key, EKey_Count, "Keyboard_remap()::key out of range"));

	if(!result)
		retError(clean, Error_nullPointer(3, "Keyboard_remap()::result is required"));

	//Same guard the other backends carry: the fallback path assigns a ref straight over *result, so
	// without this a caller's existing string would be dropped rather than rejected.

	if(result->ptr)
		retError(clean, Error_invalidParameter(3, 0, "Keyboard_remap()::result is non empty, indicating memleak"));

	//The layout map only exists in Chromium and only covers the keys it can label,
	// so a miss is normal and resolves to the US label rather than to an error.
	//That keeps a missing browser API out of the error channel: only bad arguments fail here.

	const C8 *code = EKey_toEventCode[key];

	if(code[0] && oxc3_keyboardLayoutInit()) {

		const I32 len = oxc3_keyboardLayoutLabel(
			(unsigned long long) code, (unsigned long long) label, (unsigned int)(sizeof(label) - 1)
		);

		if(len > 0) {

			//Owned copy, matching windows/linux/android; the caller frees it

			gotoIfError3(clean, CharString_createCopy(
				CharString_createRefSizedConst(label, (U64) len, true), alloc, result, e_rr
			));

			goto clean;
		}
	}

	//No layout map, so there is nothing localized to report. The key's own name is handed back instead
	// of a US label: "EKey_Q" reads as a placeholder, where "Q" would tell an AZERTY user that the key
	// they use to type A is Q, which is worse than saying nothing.
	//The names come from the buttons generic/keyboard.c registers, so they cannot drift from EKey.
	//Copied rather than referenced so both paths return an owned string and free the same way.

	const InputHandle handle = InputDevice_createHandle(keyboard, (U16) key, EInputType_Button);
	const CharString name = InputDevice_getName(keyboard, handle);

	gotoIfError3(clean, CharString_createCopy(name, alloc, result, e_rr));

clean:
	return s_uccess;
}
