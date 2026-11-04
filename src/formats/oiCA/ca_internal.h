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

//formats/oiCA/ca_internal.h
//
//What the oiCA sources share and nothing else needs. Internal, so it lives beside them rather than in include/.

#pragma once
#include "formats/oiCA/ca_file.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Rebuilds the order of folders and files from the parent links alone: folders breadth first (a folder precedes its
// children, one parent's folders are contiguous) and files grouped by folder in that order, keeping the relative order
// within a parent. Handles are invalidated. Nothing changes unless the whole rebuild can be done.
Bool CAFile_relayout(CAFile *caFile, const Allocator *alloc, Error *e_rr);

#ifdef __cplusplus
	}
#endif
