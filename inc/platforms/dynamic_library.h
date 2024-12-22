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

#pragma once
#include "types/base/types.h"

typedef void *DynamicLibrary;

impl Bool DynamicLibrary_isValidPath(CharString str);
impl Bool DynamicLibrary_load(CharString str, Bool isAppDir, DynamicLibrary *dynamicLib, Error *e_rr);
impl Bool DynamicLibrary_loadSymbol(DynamicLibrary dynamicLib, CharString str, void **ptr, Error *e_rr);
impl void DynamicLibrary_free(DynamicLibrary dynamicLib);

#ifdef _MSVC_TRADITIONAL
	#define EXPORT_SYMBOL __declspec(dllexport)
	#define IMPORT_SYMBOL __declspec(dllimport)
#else
	#define EXPORT_SYMBOL __attribute__((visibility("default")))
	#define IMPORT_SYMBOL
#endif
