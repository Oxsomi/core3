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
#include "platforms/dynamic_library.h"
#include "platforms/ext/stringx.h"
#include "types/container/string.h"
#include "types/base/error.h"

#include <dlfcn.h>

Bool DynamicLibrary_isValidPath(CharString str) {
	return CharString_endsWithStringInsensitive(str, CharString_createRefCStrConst(".so"), 0);
}

#ifdef SUPPORTS_DYNAMIC_LINKING

	Bool DynamicLibrary_load(CharString str, Bool isAppDir, DynamicLibrary *dynamicLib, Error *e_rr) {

		Bool s_uccess = true;
		CharString loc = CharString_createNull();

		if(!dynamicLib)
			retError(clean, Error_invalidState(0, "DynamicLibrary_load()::dynamicLib is required"))

		if(*dynamicLib)
			retError(clean, Error_invalidParameter(1, 0, "DynamicLibrary_load()::dynamicLib was already set, indicates memleak"))

		Bool isVirtual = false;

		if(isAppDir)
			gotoIfError3(clean, File_resolve(
				str, &isVirtual, 260, Platform_instance->appDirectory, Platform_instance->alloc, &loc, e_rr
			))

		else gotoIfError3(clean, File_resolve(
			str, &isVirtual, 260, Platform_instance->workDirectory, Platform_instance->alloc, &loc, e_rr
		))

		if(!(*dynamicLib = dlopen(loc.ptr, RTLD_LAZY)))
			retError(clean, Error_invalidState(0, "DynamicLibrary_load() dlopen failed"))

	clean:
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

		if(!(*ptr = dlsym(dynamicLib, tmp.ptr ? tmp.ptr : str.ptr)))
			retError(clean, Error_invalidState(0, "DynamicLibrary_load() dlsym failed"))

	clean:
		CharString_freex(&tmp);
		return s_uccess;
	}

	void DynamicLibrary_free(DynamicLibrary dynamicLib) {
		if(dynamicLib) dlclose(dynamicLib);
	}

#endif
