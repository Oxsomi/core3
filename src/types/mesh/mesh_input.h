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

//types/mesh/mesh_input.h
//
//Reading the flat mesh form back, which is what a writer does. The layout is the one MeshOutput wrote, so the
// same EMeshFlags describe it, and everything a file wants comes out unpacked: a position in the file's own
// units whether or not it was quantized, a normal as three floats whether or not it was oct packed, a uv as two.

#pragma once
#include "types/mesh/mesh.h"
#include "types/container/stream.h"

typedef struct Allocator Allocator;
typedef struct Error Error;

typedef struct MeshInputReader {

	StreamCursor positions, attributes, indices;

	const Allocator *alloc;

	U64 positionBase, attributeBase, indexBase;

	//center + s / 32767 * halfExtent per axis, which is what a quantized position decodes through

	F32 center[3], halfExtent[3];

	U32 vertexCount, indexCount;

	U8 positionStride, attributeStride, indexStride;

	Bool hasAttributes, quantized, wideUvs;
	U8 padding[4];

} MeshInputReader;

//Refuses an input whose streams are too short for the counts MeshInfo names, so a writer never reads past one.

Bool MeshInputReader_create(
	const MeshInput *input,
	const MeshInfo *info,
	EMeshFlags layout,
	const Allocator *alloc,
	MeshInputReader *reader,
	Error *e_rr
);

void MeshInputReader_free(MeshInputReader *reader);

//Vertex i unpacked. normal and uv are left alone when the input carries no attributes, so a caller zeroes them
// once and passes the same pair every time.

Bool MeshInputReader_vertex(MeshInputReader *reader, U32 i, F32 position[3], F32 normal[3], F32 uv[2], Error *e_rr);

//Triangle t as three vertex indices.

Bool MeshInputReader_triangle(MeshInputReader *reader, U32 t, U32 indices[3], Error *e_rr);
