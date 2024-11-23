/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "platforms/dynamic_library.h"
#include "platforms/ext/stringx.h"
#include "types/container/string.h"
#include "types/base/error.h"

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#define NOMINMAX
#include <Windows.h>
#include <libloaderapi.h>

Bool DynamicLibrary_isValidPath(CharString str) {
	return CharString_endsWithStringInsensitive(str, CharString_createRefCStrConst(".dll"), 0);
}

Bool DynamicLibrary_load(CharString str, Bool isAppDir, DynamicLibrary *dynamicLib, Error *e_rr) {

	Bool s_uccess = true;
	ListU16 utf16 = (ListU16) { 0 };
	CharString loc = CharString_createNull();

	if(!dynamicLib)
		retError(clean, Error_invalidState(0, "DynamicLibrary_load()::dynamicLib is required"))

	if(*dynamicLib)
		retError(clean, Error_invalidParameter(1, 0, "DynamicLibrary_load()::dynamicLib was already set, indicates memleak"))

	Bool isVirtual = false;

	if(isAppDir)
		gotoIfError3(clean, File_resolve(
			str, &isVirtual, MAX_PATH, Platform_instance->appDirectory, Platform_instance->alloc, &loc, e_rr
		))

	else gotoIfError3(clean, File_resolve(
		str, &isVirtual, MAX_PATH, Platform_instance->workDirectory, Platform_instance->alloc, &loc, e_rr
	))

	gotoIfError2(clean, CharString_toUTF16x(loc, &utf16))

	*dynamicLib = (void*)LoadLibraryW(utf16.ptr);
	if(!*dynamicLib)
		retError(clean, Error_platformError(0, GetLastError(), "DynamicLibrary_load() LoadLibrary failed"))

clean:
	ListU16_freex(&utf16);
	CharString_freex(&loc);
	return s_uccess;
}

Bool DynamicLibrary_loadSymbol(DynamicLibrary dynamicLib, CharString str, void **ptr, Error *e_rr) {

	Bool s_uccess = true;
	CharString tmp = CharString_createNull();

	if(!dynamicLib || !ptr)
		retError(clean, Error_invalidState(!dynamicLib ? 0 : 2, "DynamicLibrary_load()::dynamicLib and ptr are required"))

	if(!CharString_isNullTerminated(str))
		gotoIfError2(clean, CharString_createCopyx(str, &tmp))

	*ptr = (void*)GetProcAddress(dynamicLib, tmp.ptr ? tmp.ptr : str.ptr);

	if(!*ptr)
		retError(clean, Error_platformError(0, GetLastError(), "DynamicLibrary_load() GetProcAddress failed"))

clean:
	CharString_freex(&tmp);
	return s_uccess;
}

void DynamicLibrary_free(DynamicLibrary dynamicLib) {
	if(dynamicLib) FreeLibrary(dynamicLib);
}
