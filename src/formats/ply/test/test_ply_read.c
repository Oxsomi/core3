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

//formats/ply/test/test_ply_read.c

#include "test_ply_shared.h"
#include "formats/ply/ply_file.h"
#include "types/math/pack.h"
#include "types/math/flp.h"
#include "types/container/string.h"
#include "types/base/error.h"

#include <stdarg.h>

static MeshResult readPly(Test *t, const void *bytes, U64 len, EMeshFlags flags, Bool attrs, Bool words) {
	return Test_meshRead(t, Ply_read, bytes, len, flags, attrs, words, &t->err);
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

void Test_plyAscii(Test *t) {

	Test_setModule(t, "PLY/ascii");

	MeshResult r = readPly(t, asciiPly, CharString_calcStrLen(asciiPly, U64_MAX), EMeshFlags_None, true, true);

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

	MeshResult r = readPly(t, bytes, len, EMeshFlags_None, true, false);

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

void Test_plyBinaryLittleEndian(Test *t) {
	Test_setModule(t, "PLY/binaryLittleEndian");
	checkBinary(t, false);
}

void Test_plyBinaryBigEndian(Test *t) {
	Test_setModule(t, "PLY/binaryBigEndian");
	checkBinary(t, true);
}

void Test_plySkipsWhatItDoesNotKnow(Test *t) {

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

	MeshResult r = readPly(t, ply, CharString_calcStrLen(ply, U64_MAX), EMeshFlags_None, true, false);

	if(!Test_assert(t, "read", r.ok))
		return;

	Test_assert(t, "vertexCount", r.info.vertexCount == 3);
	Test_assert(t, "indexCount", r.info.indexCount == 3);

	const F32 *p = MeshResult_position(&r, 1);
	Test_assert(t, "doublePosition", p[0] == 2.5f);

	MeshResult_free(t, &r);
}

void Test_plyComputeNormals(Test *t) {

	Test_setModule(t, "PLY/computeNormals");

	MeshResult r = readPly(t, asciiPly, CharString_calcStrLen(asciiPly, U64_MAX), EMeshFlags_ComputeNormals, true, false);

	if(!Test_assert(t, "read", r.ok))
		return;

	//Every face is in the xy plane wound counter clockwise, so every vertex sums to +z, whatever the file said.

	for(U32 i = 0; i < 4; ++i) {
		Test_assert(t, "plusZ", Test_nearNormal(MeshResult_normal(&r, i), 0, 0, 1));
		Test_assert(t, "uvKept", MeshResult_uv(&r, i, 0) == (i == 1 || i == 2 ? 1 : 0));
	}

	MeshResult_free(t, &r);
}

void Test_plyQuantizedPositions(Test *t) {

	Test_setModule(t, "PLY/quantizedPositions");

	//The ascii quad spans the unit square in xy and is flat in z: corners land on the ends of the range and z on 0.

	MeshResult r = readPly(t, asciiPly, CharString_calcStrLen(asciiPly, U64_MAX), EMeshFlags_QuantizePositions, true, true);

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

	//The read reports into NULL: a refusal is the expected outcome, not an error to leave pending

	MeshResult r = Test_meshRead(
		t, Ply_read, ply, CharString_calcStrLen(ply, U64_MAX), EMeshFlags_None, true, true, NULL
	);

	Test_assert(t, name, !r.ok);
	MeshResult_free(t, &r);
}

//Appends a formatted piece to the text being built. CharString_format refuses a result that is not empty, so
// the piece is freed between calls.

static Bool Test_plyAppend(Test *t, CharString *text, const C8 *format, ...) {

	CharString part = CharString_createNull();

	va_list args;
	va_start(args, format);
	const Bool formatted = CharString_formatVariadic(t->alloc, &part, &t->err, format, args);
	va_end(args);

	const Bool appended = formatted && CharString_appendString(text, &part, t->alloc, &t->err);
	CharString_free(&part, t->alloc);
	return appended;
}

//A gaussian splat's vertex element carries 62 properties: xyz, a normal, 3 f_dc, 45 f_rest for spherical
// harmonics of degree 3, opacity, 3 scale and 4 rot. Degree 2 still needs 41.
//Every one of them has to be kept, both to find the positions among them and to know the widths to step over,
// so this is the case that decides how the header stores them.

void Test_plyManyProperties(Test *t) {

	Test_setModule(t, "PLY/manyProperties");

	CharString text = CharString_createNull();

	Bool built =
		Test_plyAppend(t, &text, "ply\nformat ascii 1.0\nelement vertex 3\n") &&
		Test_plyAppend(t, &text, "property float x\nproperty float y\nproperty float z\n") &&
		Test_plyAppend(t, &text, "property float nx\nproperty float ny\nproperty float nz\n");

	for(U8 i = 0; i < 3 && built; ++i)
		built = Test_plyAppend(t, &text, "property float f_dc_%u\n", (U32) i);

	for(U8 i = 0; i < 45 && built; ++i)
		built = Test_plyAppend(t, &text, "property float f_rest_%u\n", (U32) i);

	built = built && Test_plyAppend(t, &text, "property float opacity\n");

	for(U8 i = 0; i < 3 && built; ++i)
		built = Test_plyAppend(t, &text, "property float scale_%u\n", (U32) i);

	for(U8 i = 0; i < 4 && built; ++i)
		built = Test_plyAppend(t, &text, "property float rot_%u\n", (U32) i);

	built = built && Test_plyAppend(
		t, &text, "element face 1\nproperty list uchar int vertex_indices\nend_header\n"
	);

	//The first three values of a vertex are its position; the other 59 are carried and dropped

	const U32 positions[3][3] = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };

	for(U8 v = 0; v < 3 && built; ++v) {

		built = Test_plyAppend(t, &text, "%u %u %u", positions[v][0], positions[v][1], positions[v][2]);

		for(U8 i = 0; i < 59 && built; ++i)
			built = Test_plyAppend(t, &text, " 0.5");

		built = built && Test_plyAppend(t, &text, "\n");
	}

	built = built && Test_plyAppend(t, &text, "3 0 1 2\n");

	if(!Test_assert(t, "built", built)) {
		CharString_free(&text, t->alloc);
		return;
	}

	MeshResult r = readPly(t, text.ptr, CharString_length(text), EMeshFlags_None, true, false);
	CharString_free(&text, t->alloc);

	if(!Test_assert(t, "read", r.ok))
		return;

	Test_assert(t, "vertexCount", r.info.vertexCount == 3);
	Test_assert(t, "indexCount", r.info.indexCount == 3);

	const F32 *p = MeshResult_position(&r, 2);
	Test_assert(t, "position", Test_near(p[0], 0) && Test_near(p[1], 1) && Test_near(p[2], 0));

	MeshResult_free(t, &r);
}

void Test_plyValidation(Test *t) {

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

	MeshResult r = Test_meshRead(t, Ply_read, bytes, len, EMeshFlags_None, false, false, NULL);
	Test_assert(t, "truncatedBinary", !r.ok);
	MeshResult_free(t, &r);
}
