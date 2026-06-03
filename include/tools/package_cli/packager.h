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

//tools/package_cli/packager.h

#pragma once
#include "types/base/string_base.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef U8 CompilerWarning;
typedef struct Allocator Allocator;
typedef struct CharString CharString;

typedef struct PackageSettings {

	CharString input;
	CharString output;

	const U32 (*encryptionKey)[8];        //Pass NULL to disable AES256GCM

	U32 compileMode;
	U32 threadCount;

	CharString includeDir;

	CompilerWarning extraWarnings;
	Bool merge;
	Bool enableLogging;
	Bool isDebug;

	Bool ignoreEmptyFiles;
	Bool multipleModes;
	U8 pad[2];

} PackageSettings;

Bool Packager_package(const PackageSettings *settings, const Allocator *alloc, Error *e_rr);

#ifdef __cplusplus
	}
#endif
