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

//formats/mesh/test/test_mesh_ply.c

#include "test_mesh_shared.h"
#include "formats/ply/ply_file.h"
#include "types/math/pack.h"
#include "types/math/flp.h"
#include "types/base/error.h"

static U64 textLength(const C8 *text) {

	U64 len = 0;

	while(text[len])
		++len;

	return len;
}

static MeshResult readPly(Test *t, const void *bytes, U64 len, EMeshReadFlags flags, Bool attrs, Bool words) {
	return Test_meshRead(t, PLY_read, bytes, len, flags, attrs, words);
}

//A quad and a triangle over four vertices carrying normals and uvs, in the ascii form.

static const C8 *const asciiPly =
	"ply\n"
	"format ascii 1.0\n"
	"comment made by hand\n"
	"element vertex 4\n"
	"property float x\n"
	"property float y\n"
	"property float z\n"
	"property float nx\n"
	"property float ny\n"
	"property float nz\n"
	"property float s\n"
	"property float t\n"
	"element face 2\n"
	"property list uchar int vertex_indices\n"
	"end_header\n"
	"0 0 0 0 0 1 0 0\n"
	"1 0 0 0 0 1 1 0\n"
	"1 1 0 0 0 1 1 1\n"
	"0 1 0 0 0 1 0 1\n"
	"4 0 1 2 3\n"
	"3 0 2 3\n";

void Test_PLYAscii(Test *t) {

	Test_setModule(t, "PLY/ascii");

	MeshResult r = readPly(t, asciiPly, textLength(asciiPly), EMeshReadFlags_None, true, true);

	if(!Test_assert(t, "read", r.ok))
		return;

	Test_assert(t, "vertexCount", r.info.vertexCount == 4);
	Test_assert(t, "indexCount", r.info.indexCount == 9);
	Test_assert(t, "fannedFaces", r.info.fannedFaces == 1);
	Test_assert(t, "hasNormals", r.info.hasNormals);
	Test_assert(t, "hasUvs", r.info.hasUvs);

	const F32 *p = MeshResult_position(&r, 2);
	Test_assert(t, "position", p[0] == 1 && p[1] == 1 && p[2] == 0);

	Test_assert(t, "normal", Test_nearNormal(MeshResult_normal(&r, 2), 0, 0, 1));
	Test_assert(t, "uv", MeshResult_uv(&r, 2, 0) == 1 && MeshResult_uv(&r, 2, 1) == 1);

	const U32 *t1 = MeshResult_triangle(&r, 1), *t2 = MeshResult_triangle(&r, 2);
	Test_assert(t, "fan", t1[0] == 0 && t1[1] == 2 && t1[2] == 3);
	Test_assert(t, "triangle", t2[0] == 0 && t2[1] == 2 && t2[2] == 3);

	const F32x4 n = F32x4_unpackOct18(MeshTriangle_oct18(MeshResult_word(&r, 0)));
	Test_assert(t, "faceNormal", F32x4_z(n) == 1);
	Test_assert(t, "materialZero", !MeshTriangle_material(MeshResult_word(&r, 0)) && r.info.materialCount == 1);

	MeshResult_free(t, &r);
}

//The binary body for the same header shape, assembled a byte at a time so the file's own order is what is
// tested and never the host's. red, green and blue ride along to be skipped.

static U64 putU32(U8 *out, U32 v, Bool bigEndian) {

	for(U8 i = 0; i < 4; ++i)
		out[i] = (U8) (v >> (bigEndian ? (24 - 8 * i) : (8 * i)));

	return 4;
}

static U64 putF32(U8 *out, F32 v, Bool bigEndian) {

	union { F32 f; U32 u; } cast;
	cast.f = v;

	return putU32(out, cast.u, bigEndian);
}

static U64 buildBinaryPly(U8 *out, Bool bigEndian) {

	const C8 *header =
		bigEndian
			? "ply\nformat binary_big_endian 1.0\n"
			: "ply\nformat binary_little_endian 1.0\n";

	const C8 *rest =
		"element vertex 3\n"
		"property float x\nproperty float y\nproperty float z\n"
		"property uchar red\nproperty uchar green\nproperty uchar blue\n"
		"element face 1\n"
		"property list uchar int vertex_indices\n"
		"end_header\n";

	U64 n = 0;

	for(const C8 *c = header; *c; ++c)
		out[n++] = (U8) *c;

	for(const C8 *c = rest; *c; ++c)
		out[n++] = (U8) *c;

	const F32 positions[3][3] = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };

	for(U8 v = 0; v < 3; ++v) {

		for(U8 c = 0; c < 3; ++c)
			n += putF32(out + n, positions[v][c], bigEndian);

		out[n++] = 255; out[n++] = 128; out[n++] = 0;
	}

	out[n++] = 3;
	n += putU32(out + n, 0, bigEndian);
	n += putU32(out + n, 1, bigEndian);
	n += putU32(out + n, 2, bigEndian);

	return n;
}

static void checkBinary(Test *t, Bool bigEndian) {

	U8 bytes[512];
	const U64 len = buildBinaryPly(bytes, bigEndian);

	MeshResult r = readPly(t, bytes, len, EMeshReadFlags_None, true, false);

	if(!Test_assert(t, "read", r.ok))
		return;

	Test_assert(t, "vertexCount", r.info.vertexCount == 3);
	Test_assert(t, "indexCount", r.info.indexCount == 3);
	Test_assert(t, "noNormals", !r.info.hasNormals && !r.info.hasUvs);

	const F32 *p1 = MeshResult_position(&r, 1), *p2 = MeshResult_position(&r, 2);
	Test_assert(t, "position1", p1[0] == 1 && p1[1] == 0 && p1[2] == 0);
	Test_assert(t, "position2", p2[0] == 0 && p2[1] == 1 && p2[2] == 0);

	const U32 *tri = MeshResult_triangle(&r, 0);
	Test_assert(t, "triangle", tri[0] == 0 && tri[1] == 1 && tri[2] == 2);

	MeshResult_free(t, &r);
}

void Test_PLYBinaryLittleEndian(Test *t) {
	Test_setModule(t, "PLY/binaryLittleEndian");
	checkBinary(t, false);
}

void Test_PLYBinaryBigEndian(Test *t) {
	Test_setModule(t, "PLY/binaryBigEndian");
	checkBinary(t, true);
}

void Test_PLYSkipsWhatItDoesNotKnow(Test *t) {

	Test_setModule(t, "PLY/skips");

	//Double positions, a quality the reader has no use for, an extra list on the face and an edge element after
	// it, all consumed by their declared sizes. int8 spelling for the count type.

	const C8 *ply =
		"ply\n"
		"format ascii 1.0\n"
		"obj_info scanned\n"
		"element vertex 3\n"
		"property double x\n"
		"property double y\n"
		"property double z\n"
		"property float quality\n"
		"element face 1\n"
		"property list uint8 int32 vertex_index\n"
		"property list uchar float texcoord\n"
		"element edge 2\n"
		"property int vertex1\n"
		"property int vertex2\n"
		"end_header\n"
		"0 0 0 0.5\n"
		"2.5 0 0 0.5\n"
		"0 2.5 0 0.5\n"
		"3 0 1 2 6 0 0 1 0 0 1\n"
		"0 1\n"
		"1 2\n";

	MeshResult r = readPly(t, ply, textLength(ply), EMeshReadFlags_None, true, false);

	if(!Test_assert(t, "read", r.ok))
		return;

	Test_assert(t, "vertexCount", r.info.vertexCount == 3);
	Test_assert(t, "indexCount", r.info.indexCount == 3);

	const F32 *p = MeshResult_position(&r, 1);
	Test_assert(t, "doublePosition", p[0] == 2.5f);

	MeshResult_free(t, &r);
}

void Test_PLYComputeNormals(Test *t) {

	Test_setModule(t, "PLY/computeNormals");

	MeshResult r = readPly(t, asciiPly, textLength(asciiPly), EMeshReadFlags_ComputeNormals, true, false);

	if(!Test_assert(t, "read", r.ok))
		return;

	//Every face is in the xy plane wound counter clockwise, so every vertex sums to +z, whatever the file said.

	for(U32 i = 0; i < 4; ++i) {
		Test_assert(t, "plusZ", Test_nearNormal(MeshResult_normal(&r, i), 0, 0, 1));
		Test_assert(t, "uvKept", MeshResult_uv(&r, i, 0) == (i == 1 || i == 2 ? 1 : 0));
	}

	MeshResult_free(t, &r);
}

void Test_PLYQuantizedPositions(Test *t) {

	Test_setModule(t, "PLY/quantizedPositions");

	//The ascii quad spans the unit square in xy and is flat in z: corners land on the ends of the range and z on 0.

	MeshResult r = readPly(t, asciiPly, textLength(asciiPly), EMeshReadFlags_QuantizePositions, true, true);

	if(!Test_assert(t, "read", r.ok))
		return;

	Test_assert(t, "bytes", Buffer_length(r.positions) == 4 * 4 * sizeof(I16));
	Test_assert(t, "aabb", r.info.aabbMin[0] == 0 && r.info.aabbMax[0] == 1 && r.info.aabbMax[2] == 0);

	const I16 *q0 = MeshResult_quantized(&r, 0), *q2 = MeshResult_quantized(&r, 2);

	Test_assert(t, "corner0", q0[0] == -32767 && q0[1] == -32767 && q0[2] == 0 && q0[3] == 0);
	Test_assert(t, "corner2", q2[0] == 32767 && q2[1] == 32767 && q2[2] == 0);

	//Holding the positions for the bounds did not stop the triangle words being computed from them.

	Test_assert(t, "wordsStillMade", Buffer_length(r.triangles) == 3 * sizeof(U32));

	MeshResult_free(t, &r);
}

static void expectRefused(Test *t, const C8 *name, const C8 *ply) {

	MeshResult r = readPly(t, ply, textLength(ply), EMeshReadFlags_None, true, true);

	//Cleared before the assert: one made with an error pending counts as failed.

	const Bool refused = !r.ok;
	t->err = Error_none();

	Test_assert(t, name, refused);
	MeshResult_free(t, &r);
}

void Test_PLYValidation(Test *t) {

	Test_setModule(t, "PLY/validation");

	expectRefused(t, "badMagic", "plx\nformat ascii 1.0\nend_header\n");
	expectRefused(t, "badFormat", "ply\nformat binary_middle_endian 1.0\nend_header\n");
	expectRefused(t, "noFormat", "ply\nelement vertex 0\nend_header\n");
	expectRefused(t, "noEnd", "ply\nformat ascii 1.0\nelement vertex 0\n");

	expectRefused(t, "noVertex",
		"ply\nformat ascii 1.0\nelement face 0\nproperty list uchar int vertex_indices\nend_header\n");

	expectRefused(t, "noZ",
		"ply\nformat ascii 1.0\nelement vertex 1\nproperty float x\nproperty float y\nend_header\n0 0\n");

	expectRefused(t, "faceBeforeVertex",
		"ply\nformat ascii 1.0\nelement face 0\nproperty list uchar int vertex_indices\n"
		"element vertex 0\nproperty float x\nproperty float y\nproperty float z\nend_header\n");

	expectRefused(t, "unknownType",
		"ply\nformat ascii 1.0\nelement vertex 1\nproperty half x\nproperty float y\nproperty float z\nend_header\n0 0 0\n");

	expectRefused(t, "floatListCount",
		"ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\n"
		"element face 1\nproperty list float int vertex_indices\nend_header\n0 0 0\n1 0 0\n0 1 0\n3 0 1 2\n");

	expectRefused(t, "indexPastEnd",
		"ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\n"
		"element face 1\nproperty list uchar int vertex_indices\nend_header\n0 0 0\n1 0 0\n0 1 0\n3 0 1 3\n");

	expectRefused(t, "twoCorners",
		"ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\n"
		"element face 1\nproperty list uchar int vertex_indices\nend_header\n0 0 0\n1 0 0\n0 1 0\n2 0 1\n");

	expectRefused(t, "truncatedBody",
		"ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\n"
		"end_header\n0 0 0\n1 0 0\n");

	expectRefused(t, "shortLine",
		"ply\nformat ascii 1.0\nelement vertex 1\nproperty float x\nproperty float y\nproperty float z\n"
		"end_header\n0 0\n");

	//A binary body one byte short of what the header promises.

	U8 bytes[512];
	U64 len = 0;

	{
		const C8 *header =
			"ply\nformat binary_little_endian 1.0\nelement vertex 1\n"
			"property float x\nproperty float y\nproperty float z\nend_header\n";

		for(const C8 *c = header; *c; ++c)
			bytes[len++] = (U8) *c;

		for(U8 i = 0; i < 11; ++i)
			bytes[len++] = 0;
	}

	MeshResult r = readPly(t, bytes, len, EMeshReadFlags_None, false, false);
	const Bool refused = !r.ok;
	t->err = Error_none();
	Test_assert(t, "truncatedBinary", refused);
	MeshResult_free(t, &r);
}
