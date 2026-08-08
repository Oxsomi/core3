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

//formats/oiCA/ca_edit.h

#pragma once
#include "formats/oiCA/ca_file.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Rename a file.
//Moves name if not a ref, otherwise copies.
Bool CAFile_rename(CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, CharString *name, Error *e_rr);

Bool CAFile_move(CAFile *caFile, CAHandle fileHandle, CAHandle newParent, const Allocator *alloc, Error *e_rr);

//Adding/removing

CAHandle CAFile_add(
	CAFile *caFile, CAHandle parent, CharString *name, Ns time, Bool isFile, const Allocator *alloc, Error *e_rr
);

static inline CAHandle CAFile_addFile(
CAFile *caFile, CAHandle parent, CharString *name, Ns time, const Allocator *alloc, Error *e_rr
) {
	return CAFile_add(caFile, parent, name, time, true, alloc, e_rr);
}

static inline CAHandle CAFile_addFolder(
	CAFile *caFile, CAHandle parent, CharString *name, const Allocator *alloc, Error *e_rr
) {
	return CAFile_add(caFile, parent, name, 0, false, alloc, e_rr);
}

Bool CAFile_removeFile(CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, Error *e_rr);
Bool CAFile_removeFolder(CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, Error *e_rr);
Bool CAFile_remove(CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, Error *e_rr);

#ifdef __cplusplus
	}
#endif
