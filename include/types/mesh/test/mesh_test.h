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

//types/mesh/test/mesh_test.h
//
//The harness every mesh reader suite reads through. A reader's own suite lives beside that reader; what they
// share is the sink plumbing and the accessors that turn a written record back into what the file said.

#pragma once
#include "types/test/test.h"
#include "types/container/buffer.h"
#include "types/mesh/mesh.h"
#include "types/math/vec4f.h"

typedef struct RefPtr RefPtr;
typedef RefPtr StreamRef;

//Every reader has the same signature, so one harness reads through any of them.

typedef Bool (*MeshReadFunc)(
	StreamRef *stream, U64 *off, EMeshFlags flags, MeshInfo *info, const MeshOutput *output,
	const Allocator *alloc, Error *e_rr
);

//What a read produced, moved out of the sinks. Every Buffer is owned and freed by MeshResult_free.

typedef struct MeshResult {
	MeshInfo info;
	Buffer positions, attributes, indices, triangles;
	Bool ok;
	U8 padding[7];
} MeshResult;

//Reads bytes through fn into resizable sinks. withAttributes and withTriangles leave those outputs NULL
// when false, which is what a consumer reading only geometry does.
//readError is where the READ reports; the harness's own failures go to t->err regardless. NULL is for a read
// that is expected to be refused, so an assert made afterwards has no error pending to count against it.

MeshResult Test_meshRead(
	Test *t, MeshReadFunc fn, const void *bytes, U64 length, EMeshFlags flags,
	Bool withAttributes, Bool withTriangles, Error *readError
);

void MeshResult_free(Test *t, MeshResult *r);

const F32 *MeshResult_position(const MeshResult *r, U32 i);
const I16 *MeshResult_quantized(const MeshResult *r, U32 i);
const MeshAttribute *MeshResult_attribute(const MeshResult *r, U32 i);
F32x4 MeshResult_normal(const MeshResult *r, U32 i);
F32 MeshResult_uv(const MeshResult *r, U32 i, U8 axis);
const U32 *MeshResult_triangle(const MeshResult *r, U32 i);
U32 MeshResult_word(const MeshResult *r, U32 i);

//Every writer has the same signature bar its format argument, so a suite wraps that away and the round trip
// below drives any of them.

typedef Bool (*MeshWriteFunc)(
	const MeshInput *input, const MeshInfo *info, EMeshFlags layout,
	StreamRef *stream, U64 *off, const Allocator *alloc, Error *e_rr
);

//Writes a result back out through fn and hands back the bytes it produced, owned by the caller.
//An empty buffer means the write failed, which the caller asserts on.

Buffer Test_meshWrite(Test *t, MeshWriteFunc fn, const MeshResult *r, EMeshFlags layout);

//A round trip compares TRIANGLE by triangle rather than vertex by vertex: a reader shares vertices in first use
// order, so writing and reading back is free to renumber them even though the surface is identical.

void Test_meshTrianglesMatch(Test *t, const C8 *name, const MeshResult *a, const MeshResult *b);

//The tolerances a decoded record is compared at, so every suite agrees on what near means.

Bool Test_near(F32 a, F32 b);
Bool Test_nearNormal(F32x4 n, F32 x, F32 y, F32 z);
