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

//platforms/windows/wdynamic_library.c

#include "platforms/dynamic_library.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "types/container/list_basic_types.h"
#include "types/container/string_unicode.h"
#include "types/base/string_base.h"
#include "types/base/string_read_helper.h"
#include "types/base/error.h"

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#define NOMINMAX
#include <Windows.h>
#include <libloaderapi.h>

Bool DynamicLibrary_isValidPath(CharString str) {
	const CharString dll = CharString_createRefCStrConst(".dll");
	return CharString_endsWithStringInsensitive(&str, &dll, 0);
}

Bool CharString_toLongPath(wchar_t *buf, const CharString *str, Error *e_rr);

Bool DynamicLibrary_load(CharString str, Bool isAppDir, DynamicLibrary *dynamicLib, Error *e_rr) {

	Bool s_uccess = true;
	CharString loc = CharString_createNull();
	wchar_t path[MAX_OXC_PATH + 1];

	if(!dynamicLib)
		retError(clean, Error_invalidState(0, "DynamicLibrary_load()::dynamicLib is required"));

	if(*dynamicLib)
		retError(clean, Error_invalidParameter(1, 0, "DynamicLibrary_load()::dynamicLib was already set, indicates memleak"));

	Bool isVirtual = false;

	if(isAppDir) {
		gotoIfError3(clean, File_resolve(
			&str, &isVirtual, 0, &Platform_instance->appDirectory, Platform_instance->alloc, &loc, e_rr
		));
	}

	else gotoIfError3(clean, File_resolve(
		&str, &isVirtual, 0, &Platform_instance->workDirectory, Platform_instance->alloc, &loc, e_rr
	));

	gotoIfError3(clean, CharString_toLongPath(path, &loc, e_rr));

	*dynamicLib = (void*)LoadLibraryW(path);
	if(!*dynamicLib)
		retError(clean, Error_platformError(0, GetLastError(), "DynamicLibrary_load() LoadLibrary failed"));

clean:
	CharString_free(&loc, Platform_instance->alloc);
	return s_uccess;
}

Bool DynamicLibrary_loadSystem(CharString str, DynamicLibrary *dynamicLib, Error *e_rr) {

	Bool s_uccess = true;

	if(!dynamicLib)
		retError(clean, Error_invalidState(0, "DynamicLibrary_loadSystem()::dynamicLib is required"));

	if(*dynamicLib)
		retError(clean, Error_invalidParameter(
			1, 0, "DynamicLibrary_loadSystem()::dynamicLib was already set, indicates memleak"
		));

	if(!CharString_isNullTerminated(str))
		retError(clean, Error_invalidParameter(0, 0, "DynamicLibrary_loadSystem()::str must be null-terminated"));

	//Only allow a bare library name so this can't be abused to load an arbitrary DLL from a caller-chosen path;
	//reject any path separator, leaving resolution to the OS's standard (system) search path.
	for(U64 i = 0; i < CharString_length(str); ++i)
		if(str.ptr[i] == '/' || str.ptr[i] == '\\')
			retError(clean, Error_invalidParameter(
				0, 0, "DynamicLibrary_loadSystem()::str must be a bare library name (no path separators)"
			));

	//A bare name lets Windows resolve the DLL via its standard search path (System32, PATH, ...).
	*dynamicLib = (void*)LoadLibraryA(str.ptr);

	if(!*dynamicLib)
		retError(clean, Error_platformError(0, GetLastError(), "DynamicLibrary_loadSystem() LoadLibrary failed"));

clean:
	return s_uccess;
}

Bool DynamicLibrary_loadSymbol(DynamicLibrary dynamicLib, CharString str, void **ptr, Error *e_rr) {

	Bool s_uccess = true;
	CharString tmp = CharString_createNull();

	if(!dynamicLib || !ptr)
		retError(clean, Error_invalidState(!dynamicLib ? 0 : 2, "DynamicLibrary_load()::dynamicLib and ptr are required"));

	if(!CharString_isNullTerminated(str))
		gotoIfError3(clean, CharString_createCopy(str, Platform_instance->alloc, &tmp, e_rr));

	*ptr = (void*)GetProcAddress(dynamicLib, tmp.ptr ? tmp.ptr : str.ptr);

	if(!*ptr)
		retError(clean, Error_platformError(0, GetLastError(), "DynamicLibrary_load() GetProcAddress failed"));

clean:
	CharString_free(&tmp, Platform_instance->alloc);
	return s_uccess;
}

void DynamicLibrary_free(DynamicLibrary dynamicLib) {
	if(dynamicLib) FreeLibrary(dynamicLib);
}
