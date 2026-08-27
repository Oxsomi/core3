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

//formats/obj/obj_file.h

#pragma once
#include "formats/mesh/mesh.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct Error Error;

//Wavefront OBJ, the text format every mesh archive ships alongside whatever else it has.
//
//What comes out is what a GPU takes: one vertex per DISTINCT position, normal and uv triple a face names,
// and a triangle list of U32 indices into those. The file itself indexes position, normal and uv separately,
// so a corner sharing a position with its neighbour but not a normal is two vertices here and one there.
//That deduplication is the whole job; the arithmetic of the format is trivial.
//
//Only the geometry is read. Groups, objects, smoothing groups and material assignments are skipped rather
// than refused, since a mesh is still a mesh without them, and a reader that stopped at a usemtl it could
// not resolve would refuse most files in the wild. Lines, points and continuation backslashes are skipped or
// refused the same way.
//
//The input is pulled through a window rather than mapped whole, and vertices leave in chunks as faces first
// name them. What is held is the file's own position, normal and uv arrays and the dedup table over them,
// and that is inherent: a face may name any position in the file, first or last, so the arrays have to exist in
// full before the faces that use them can be resolved.
//
//Refused rather than repaired: an index past the arrays, a zero index, a face with fewer than three corners,
// a corner with more than two slashes and a number that does not parse. Each of those is a file that is broken,
// and a reader that guessed would hand the guess to a BLAS build.

Bool OBJ_read(
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
