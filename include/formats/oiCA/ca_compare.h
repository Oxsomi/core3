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

#pragma once
#include "types/base/algorithm.h"
#include "formats/oiCA/ca_file.h"

#ifdef __cplusplus
	extern "C" {
#endif

//This will compare the two files at a and b.
// Both files have to be buffers or streams that are seekable, otherwise it'll error.
// Keep in mind that this is a full compare, which could take very long with big files.
// As such, this should only be used in tools that are expected to take a long time.
Bool CAFile_dataEqual(
	const CAFile *a, CAHandle aFile,
	const CAFile *b, CAHandle bFile,
	const Allocator *alloc,
	ECompareResult *equal,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
