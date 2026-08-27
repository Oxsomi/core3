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

//formats/ply/ply_file.h

#pragma once
#include "formats/mesh/mesh.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct Error Error;

//Stanford PLY, the format the scanned classics are published in, ascii or binary of either byte order.
//
//A PLY vertex is already a vertex: faces index one array, so unlike OBJ nothing has to be deduplicated and the
// records leave as they are read. The header is what carries the work here, since it describes an arbitrary
// property list per element and the reader has to pick the ones it understands out of it.
//
//What is read: from the vertex element x, y and z, nx, ny and nz when present, and the first of s/t, u/v or
// texture_u/texture_v when present. From the face element the first list property, whatever it is named,
// which every writer calls vertex_indices or vertex_index. Every other property and every other element is
// skipped by its declared size, colours and confidences included, so a file carrying them still loads.
//
//Refused rather than repaired: a format line other than the three the specification defines, a vertex element
// without x, y and z, a face list of fewer than three corners, an index past the vertex count, a scalar type the
// specification does not name and a body shorter than the header promised.

Bool PLY_read(
	StreamRef *stream,
	U64 *off,                     //Left after the last byte consumed
	EMeshReadFlags flags,
	MeshInfo *info,
	const MeshOutput *output,
	const Allocator *alloc,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
