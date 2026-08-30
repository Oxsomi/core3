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
#include "platforms/logx.h"
#include "types/container/string.h"
#include "types/base/error.h"

#include <dirent.h>
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

Bool Keyboard_remap(const Keyboard *keyboard, EKey key, const Allocator *alloc, CharString *result, Error *e_rr) {

	(void) alloc;

	Bool s_uccess = true;

	if(!keyboard)
		retError(clean, Error_nullPointer(0, "Keyboard_remap()::keyboard is required"));

	if((U32) key >= EKey_Count)
		retError(clean, Error_outOfBounds(1, (U64) key, EKey_Count, "Keyboard_remap()::key out of range"));

	if(!result)
		retError(clean, Error_nullPointer(3, "Keyboard_remap()::result is required"));

	//No layout API without a real keyboard event source; headless web runs don't have one.

	retError(clean, Error_unsupportedOperation(0, "Keyboard_remap() isn't supported on web yet"));

clean:
	return s_uccess;
}
