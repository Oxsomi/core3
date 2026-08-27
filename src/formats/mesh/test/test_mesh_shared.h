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

//formats/mesh/test/test_mesh_shared.h

#pragma once
#include "types/test/test.h"
#include "types/container/buffer.h"
#include "formats/mesh/mesh.h"
#include "types/math/vec4f.h"

typedef struct RefPtr RefPtr;
typedef RefPtr StreamRef;

//Both readers have the same signature, so one harness reads through either.

typedef Bool (*MeshReadFunc)(
	StreamRef *stream, U64 *off, EMeshReadFlags flags, MeshInfo *info, const MeshOutput *output,
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

MeshResult Test_meshRead(
	Test *t, MeshReadFunc fn, const void *bytes, U64 length, EMeshReadFlags flags,
	Bool withAttributes, Bool withTriangles
);

void MeshResult_free(Test *t, MeshResult *r);

const F32 *MeshResult_position(const MeshResult *r, U32 i);
const I16 *MeshResult_quantized(const MeshResult *r, U32 i);
const MeshAttribute *MeshResult_attribute(const MeshResult *r, U32 i);
F32x4 MeshResult_normal(const MeshResult *r, U32 i);
F32 MeshResult_uv(const MeshResult *r, U32 i, U8 axis);
Bool Test_nearNormal(F32x4 n, F32 x, F32 y, F32 z);
const U32 *MeshResult_triangle(const MeshResult *r, U32 i);
U32 MeshResult_word(const MeshResult *r, U32 i);

Bool Test_near(F32 a, F32 b);

void Test_OBJCubeWithEverything(Test *t);
void Test_OBJDedup(Test *t);
void Test_OBJNegativeIndices(Test *t);
void Test_OBJCornerForms(Test *t);
void Test_OBJTextTolerance(Test *t);
void Test_OBJComputeNormals(Test *t);
void Test_OBJTriangleWords(Test *t);
void Test_OBJMaterials(Test *t);
void Test_OBJQuantizedPositions(Test *t);
void Test_OBJGeometryOnly(Test *t);
void Test_OBJValidation(Test *t);

void Test_PLYAscii(Test *t);
void Test_PLYBinaryLittleEndian(Test *t);
void Test_PLYBinaryBigEndian(Test *t);
void Test_PLYSkipsWhatItDoesNotKnow(Test *t);
void Test_PLYComputeNormals(Test *t);
void Test_PLYQuantizedPositions(Test *t);
void Test_PLYValidation(Test *t);
