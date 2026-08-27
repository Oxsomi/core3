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

//formats/mesh/test/test_mesh_obj.c

#include "test_mesh_shared.h"
#include "formats/obj/obj_file.h"
#include "types/math/pack.h"
#include "types/math/flp.h"
#include "types/base/error.h"
#include "types/base/mathf.h"
#include "types/container/memory_stream.h"
#include "types/container/ref_ptr.h"

static MeshResult readObj(Test *t, const C8 *text, EMeshReadFlags flags, Bool attrs, Bool words) {

	U64 len = 0;

	while(text[len])
		++len;

	return Test_meshRead(t, OBJ_read, text, len, flags, attrs, words);
}

static F32x4 wordNormal(const MeshResult *r, U32 i) {
	return F32x4_unpackOct18(MeshTriangle_oct18(MeshResult_word(r, i)));
}

//A unit cube with six quads, each corner naming its own uv and the face's normal. Every corner is a distinct
// triple, so the 8 positions become 24 vertices, and every quad fans into two triangles.

static const C8 *const cubeObj =
	"# a cube\n"
	"o cube\n"
	"v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
	"v 0 0 1\nv 1 0 1\nv 1 1 1\nv 0 1 1\n"
	"vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
	"vn 0 0 -1\nvn 0 0 1\nvn -1 0 0\nvn 1 0 0\nvn 0 -1 0\nvn 0 1 0\n"
	"usemtl paint\n"
	"s off\n"
	"f 1/1/1 4/2/1 3/3/1 2/4/1\n"
	"f 5/1/2 6/2/2 7/3/2 8/4/2\n"
	"f 1/1/3 5/2/3 8/3/3 4/4/3\n"
	"f 2/1/4 3/2/4 7/3/4 6/4/4\n"
	"f 1/1/5 2/2/5 6/3/5 5/4/5\n"
	"f 4/1/6 8/2/6 7/3/6 3/4/6\n";

void Test_OBJCubeWithEverything(Test *t) {

	Test_setModule(t, "OBJ/cube");

	MeshResult r = readObj(t, cubeObj, EMeshReadFlags_None, true, true);

	if(!Test_assert(t, "read", r.ok))
		return;

	Test_assert(t, "vertexCount", r.info.vertexCount == 24);
	Test_assert(t, "indexCount", r.info.indexCount == 36);
	Test_assert(t, "fannedFaces", r.info.fannedFaces == 6);
	Test_assert(t, "hasNormals", r.info.hasNormals);
	Test_assert(t, "hasUvs", r.info.hasUvs);

	Test_assert(t, "positionBytes", Buffer_length(r.positions) == 24 * 3 * sizeof(F32));
	Test_assert(t, "attributeBytes", Buffer_length(r.attributes) == 24 * sizeof(MeshAttribute));
	Test_assert(t, "indexBytes", Buffer_length(r.indices) == 36 * sizeof(U32));
	Test_assert(t, "wordBytes", Buffer_length(r.triangles) == 12 * sizeof(U32));
	Test_assert(t, "oneMaterial", r.info.materialCount == 1);

	//The first quad fans into (0, 1, 2) and (0, 2, 3): corners are numbered as first met.

	const U32 *t0 = MeshResult_triangle(&r, 0), *t1 = MeshResult_triangle(&r, 1);
	Test_assert(t, "fan0", t0[0] == 0 && t0[1] == 1 && t0[2] == 2);
	Test_assert(t, "fan1", t1[0] == 0 && t1[1] == 2 && t1[2] == 3);

	//Vertex 2 is corner 3/3/1 of the first face: position 3, uv 3, normal 1.

	const F32 *p = MeshResult_position(&r, 2);

	Test_assert(t, "position", p[0] == 1 && p[1] == 1 && p[2] == 0);
	Test_assert(t, "uv", MeshResult_uv(&r, 2, 0) == 1 && MeshResult_uv(&r, 2, 1) == 1);
	Test_assert(t, "normal", Test_nearNormal(MeshResult_normal(&r, 2), 0, 0, -1));

	//The first face is wound to face -z, and 9 bits an axis carries an axis exactly.

	const F32x4 fn = wordNormal(&r, 0);
	Test_assert(t, "faceNormal", F32x4_x(fn) == 0 && F32x4_y(fn) == 0 && F32x4_z(fn) == -1);
	Test_assert(t, "wordMaterial", MeshTriangle_material(MeshResult_word(&r, 0)) == 0);

	MeshResult_free(t, &r);
}

void Test_OBJDedup(Test *t) {

	Test_setModule(t, "OBJ/dedup");

	//Two triangles sharing an edge with identical triples share the two vertices on it.

	const C8 *obj =
		"v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\n"
		"vn 0 0 1\n"
		"f 1//1 2//1 3//1\n"
		"f 2//1 4//1 3//1\n";

	MeshResult r = readObj(t, obj, EMeshReadFlags_None, true, false);

	if(!Test_assert(t, "read", r.ok))
		return;

	Test_assert(t, "vertexCount", r.info.vertexCount == 4);
	Test_assert(t, "indexCount", r.info.indexCount == 6);
	Test_assert(t, "fannedFaces", r.info.fannedFaces == 0);

	const U32 *t1 = MeshResult_triangle(&r, 1);
	Test_assert(t, "sharedEdge", t1[0] == 1 && t1[1] == 3 && t1[2] == 2);

	MeshResult_free(t, &r);

	//The same positions with a different normal on the second face are new vertices: the file didn't share them.

	const C8 *split =
		"v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\n"
		"vn 0 0 1\nvn 0 1 0\n"
		"f 1//1 2//1 3//1\n"
		"f 2//2 4//2 3//2\n";

	r = readObj(t, split, EMeshReadFlags_None, true, false);

	if(!Test_assert(t, "readSplit", r.ok))
		return;

	Test_assert(t, "splitVertexCount", r.info.vertexCount == 6);

	MeshResult_free(t, &r);
}

void Test_OBJNegativeIndices(Test *t) {

	Test_setModule(t, "OBJ/negativeIndices");

	//-1 is the most recent position, which is how an incrementally written file names what it just emitted.

	const C8 *obj =
		"v 0 0 0\nv 1 0 0\nv 0 1 0\n"
		"f -3 -2 -1\n"
		"v 1 1 0\n"
		"f -3 -1 -2\n";

	MeshResult r = readObj(t, obj, EMeshReadFlags_None, false, false);

	if(!Test_assert(t, "read", r.ok))
		return;

	Test_assert(t, "vertexCount", r.info.vertexCount == 4);

	const U32 *t0 = MeshResult_triangle(&r, 0), *t1 = MeshResult_triangle(&r, 1);
	Test_assert(t, "first", t0[0] == 0 && t0[1] == 1 && t0[2] == 2);

	//The second face is positions 2, 4, 3 in file terms: 2 was already vertex 1, 4 and 3 become 3 and 2.

	Test_assert(t, "second", t1[0] == 1 && t1[1] == 3 && t1[2] == 2);

	MeshResult_free(t, &r);
}

void Test_OBJCornerForms(Test *t) {

	Test_setModule(t, "OBJ/cornerForms");

	//v/vt without a normal, v//vn without a uv and plain v, each reporting only what it named.

	const C8 *withUv = "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nf 1/1 2/2 3/3\n";
	MeshResult r = readObj(t, withUv, EMeshReadFlags_None, true, false);

	if(Test_assert(t, "readUv", r.ok)) {
		Test_assert(t, "uvOnlyHasUvs", r.info.hasUvs && !r.info.hasNormals);
		Test_assert(t, "uvValue", MeshResult_uv(&r, 1, 0) == 1 && MeshResult_uv(&r, 1, 1) == 0);
		Test_assert(t, "uvNormalPlusZ", Test_nearNormal(MeshResult_normal(&r, 1), 0, 0, 1));
	}

	MeshResult_free(t, &r);

	const C8 *withNormal = "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2//1 3//1\n";
	r = readObj(t, withNormal, EMeshReadFlags_None, true, false);

	if(Test_assert(t, "readNormal", r.ok)) {
		Test_assert(t, "normalOnlyHasNormals", r.info.hasNormals && !r.info.hasUvs);
		Test_assert(t, "normalValue", Test_nearNormal(MeshResult_normal(&r, 0), 0, 0, 1) && MeshResult_uv(&r, 0, 0) == 0);
	}

	MeshResult_free(t, &r);

	const C8 *plain = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
	r = readObj(t, plain, EMeshReadFlags_None, true, false);

	if(Test_assert(t, "readPlain", r.ok))
		Test_assert(t, "plainHasNeither", !r.info.hasNormals && !r.info.hasUvs);

	MeshResult_free(t, &r);

	//The wide shape keeps a uv an F16 would have rounded: 1/3 has no short encoding, and a value past a few
	// thousand has no F16 encoding at all.

	const C8 *wide = "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0.33333334 70000\nf 1/1 2/1 3/1\n";
	r = readObj(t, wide, EMeshReadFlags_WideUvs, true, false);

	if(Test_assert(t, "readWide", r.ok)) {
		Test_assert(t, "wideBytes", Buffer_length(r.attributes) == 3 * sizeof(MeshAttributeWide));
		const MeshAttributeWide *w = (const MeshAttributeWide*) r.attributes.ptr;
		Test_assert(t, "wideUv", w->uv[0] == 0.33333334f && w->uv[1] == 70000);
	}

	MeshResult_free(t, &r);
}

void Test_OBJTextTolerance(Test *t) {

	Test_setModule(t, "OBJ/textTolerance");

	//CRLF endings, tabs, blank lines, comments, an exponent, a sign, a w on a position and a third uv component:
	// all of it appears in files from real exporters.

	const C8 *obj =
		"# comment\r\n"
		"\r\n"
		"v\t1.5e1\t-2.0E-1\t+3 1.0\r\n"
		"v 0 0 0\r\n"
		"v 0 1 0   \r\n"
		"vt 0.25 0.75 0\r\n"
		"g group\r\n"
		"f 1/1 2/1 3/1\r\n";

	MeshResult r = readObj(t, obj, EMeshReadFlags_None, true, false);

	if(!Test_assert(t, "read", r.ok))
		return;

	const F32 *p = MeshResult_position(&r, 0);
	Test_assert(t, "exponent", Test_near(p[0], 15) && Test_near(p[1], -0.2f) && Test_near(p[2], 3));

	Test_assert(t, "uvThird", MeshResult_uv(&r, 0, 0) == 0.25f && MeshResult_uv(&r, 0, 1) == 0.75f);

	MeshResult_free(t, &r);
}

void Test_OBJComputeNormals(Test *t) {

	Test_setModule(t, "OBJ/computeNormals");

	//One triangle in the xy plane wound counter clockwise: every corner gets exactly +z.

	const C8 *tri = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
	MeshResult r = readObj(t, tri, EMeshReadFlags_ComputeNormals, true, false);

	if(Test_assert(t, "readTri", r.ok)) {

		Test_assert(t, "fileHadNone", !r.info.hasNormals);

		for(U32 i = 0; i < 3; ++i)
			Test_assert(t, "plusZ", Test_nearNormal(MeshResult_normal(&r, i), 0, 0, 1));
	}

	MeshResult_free(t, &r);

	//Two triangles meeting at a right angle along the x axis, one facing +z and one +y: the shared vertices average
	// to (0, 1, 1) / sqrt2, and the sums are area weighted, which two equal triangles keep symmetric.

	const C8 *fold =
		"v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 -1\n"
		"f 1 2 3\n"
		"f 1 2 4\n";

	r = readObj(t, fold, EMeshReadFlags_ComputeNormals, true, false);

	if(Test_assert(t, "readFold", r.ok)) {

		const F32 s = 0.70710677f;

		Test_assert(t, "sharedAveraged", Test_nearNormal(MeshResult_normal(&r, 0), 0, s, s));
		Test_assert(t, "unsharedPlain", Test_nearNormal(MeshResult_normal(&r, 2), 0, 0, 1));
	}

	MeshResult_free(t, &r);

	//A file that supplies its own normals has them replaced under the flag, and still reports having supplied them.

	const C8 *supplied = "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 1 0 0\nf 1//1 2//1 3//1\n";
	r = readObj(t, supplied, EMeshReadFlags_ComputeNormals, true, false);

	if(Test_assert(t, "readSupplied", r.ok)) {
		Test_assert(t, "reportsSupplied", r.info.hasNormals);
		Test_assert(t, "replaced", Test_nearNormal(MeshResult_normal(&r, 0), 0, 0, 1));
	}

	MeshResult_free(t, &r);
}

void Test_OBJTriangleWords(Test *t) {

	Test_setModule(t, "OBJ/triangleWords");

	//One packed word per triangle, from the winding. The quad's two fan triangles both carry the quad's normal,
	// a degenerate triangle carries +z, and an off axis normal survives to within the encoding's resolution.

	const C8 *obj =
		"v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 2 0 0\nv 0 0 1\n"
		"f 1 2 3 4\n"
		"f 1 2 5\n"
		"f 2 4 6\n";

	MeshResult r = readObj(t, obj, EMeshReadFlags_None, false, true);

	if(!Test_assert(t, "read", r.ok))
		return;

	Test_assert(t, "count", Buffer_length(r.triangles) == 4 * sizeof(U32));

	Test_assert(t, "quadFirst", F32x4_z(wordNormal(&r, 0)) == 1);
	Test_assert(t, "quadSecond", F32x4_z(wordNormal(&r, 1)) == 1);
	Test_assert(t, "degenerate", F32x4_z(wordNormal(&r, 2)) == 1);

	//(1, 1, 1) / sqrt3 for the tilted one, to about a third of a degree.

	const F32x4 tilted = wordNormal(&r, 3);
	const F32 s = 0.57735f;

	Test_assert(t, "tilted",
		F32_abs(F32x4_x(tilted) - s) < 0.01f && F32_abs(F32x4_y(tilted) - s) < 0.01f && F32_abs(F32x4_z(tilted) - s) < 0.01f
	);

	//The material bits are clear on a file that names no material.

	Test_assert(t, "materialClear", !MeshTriangle_material(MeshResult_word(&r, 0)));

	MeshResult_free(t, &r);
}

void Test_OBJMaterials(Test *t) {

	Test_setModule(t, "OBJ/materials");

	//Numbered in order of first use, a name used again mapping back to its number, and faces before any usemtl on
	// a default of their own so they never share a number with the first named one.

	const C8 *obj =
		"v 0 0 0\nv 1 0 0\nv 0 1 0\n"
		"f 1 2 3\n"
		"usemtl red\n"
		"f 1 2 3\n"
		"usemtl green\n"
		"f 1 2 3\n"
		"usemtl red\n"
		"f 1 2 3\n";

	MeshResult r = readObj(t, obj, EMeshReadFlags_None, false, true);

	if(!Test_assert(t, "read", r.ok))
		return;

	Test_assert(t, "materialCount", r.info.materialCount == 3);
	Test_assert(t, "default", MeshTriangle_material(MeshResult_word(&r, 0)) == 0);
	Test_assert(t, "red", MeshTriangle_material(MeshResult_word(&r, 1)) == 1);
	Test_assert(t, "green", MeshTriangle_material(MeshResult_word(&r, 2)) == 2);
	Test_assert(t, "redAgain", MeshTriangle_material(MeshResult_word(&r, 3)) == 1);

	MeshResult_free(t, &r);

	//With a usemtl before the first face there is no default, and the first name is 0.

	const C8 *named = "v 0 0 0\nv 1 0 0\nv 0 1 0\nusemtl a\nf 1 2 3\nusemtl b\nf 1 2 3\n";
	r = readObj(t, named, EMeshReadFlags_None, false, true);

	if(Test_assert(t, "readNamed", r.ok)) {
		Test_assert(t, "namedCount", r.info.materialCount == 2);
		Test_assert(t, "namedFirst", MeshTriangle_material(MeshResult_word(&r, 0)) == 0);
		Test_assert(t, "namedSecond", MeshTriangle_material(MeshResult_word(&r, 1)) == 1);
	}

	MeshResult_free(t, &r);
}

void Test_OBJQuantizedPositions(Test *t) {

	Test_setModule(t, "OBJ/quantizedPositions");

	//Bounds from (-1, -2, -3) to (3, 2, 1): the corners land on -32767 and 32767 exactly, the center on 0, and a
	// flat axis, z on the last file, quantizes to 0 throughout since it has no extent to divide by.

	const C8 *obj =
		"v -1 -2 -3\nv 3 2 1\nv 1 0 -1\n"
		"f 1 2 3\n";

	MeshResult r = readObj(t, obj, EMeshReadFlags_QuantizePositions, false, false);

	if(!Test_assert(t, "read", r.ok))
		return;

	Test_assert(t, "bytes", Buffer_length(r.positions) == 3 * 4 * sizeof(I16));
	Test_assert(t, "aabbMin", r.info.aabbMin[0] == -1 && r.info.aabbMin[1] == -2 && r.info.aabbMin[2] == -3);
	Test_assert(t, "aabbMax", r.info.aabbMax[0] == 3 && r.info.aabbMax[1] == 2 && r.info.aabbMax[2] == 1);

	const I16 *lo = MeshResult_quantized(&r, 0), *hi = MeshResult_quantized(&r, 1), *mid = MeshResult_quantized(&r, 2);

	Test_assert(t, "min", lo[0] == -32767 && lo[1] == -32767 && lo[2] == -32767 && lo[3] == 0);
	Test_assert(t, "max", hi[0] == 32767 && hi[1] == 32767 && hi[2] == 32767 && hi[3] == 0);
	Test_assert(t, "center", mid[0] == 0 && mid[1] == 0 && mid[2] == 0);

	MeshResult_free(t, &r);

	const C8 *flat = "v 0 0 5\nv 1 0 5\nv 0 1 5\nf 1 2 3\n";
	r = readObj(t, flat, EMeshReadFlags_QuantizePositions, false, false);

	if(Test_assert(t, "readFlat", r.ok)) {
		Test_assert(t, "flatAabb", r.info.aabbMin[2] == 5 && r.info.aabbMax[2] == 5);
		Test_assert(t, "flatZero", MeshResult_quantized(&r, 1)[2] == 0 && MeshResult_quantized(&r, 1)[0] == 32767);
	}

	MeshResult_free(t, &r);

	//Without the flag the bounds are still reported and the positions are still three F32s.

	r = readObj(t, obj, EMeshReadFlags_None, false, false);

	if(Test_assert(t, "readPlain", r.ok)) {
		Test_assert(t, "plainBytes", Buffer_length(r.positions) == 3 * 3 * sizeof(F32));
		Test_assert(t, "plainAabb", r.info.aabbMax[0] == 3 && r.info.aabbMin[2] == -3);
	}

	MeshResult_free(t, &r);
}

void Test_OBJGeometryOnly(Test *t) {

	Test_setModule(t, "OBJ/geometryOnly");

	//Neither attributes nor face normals asked for: the normals in the file are read and dropped, and nothing
	// is computed that no sink wants.

	MeshResult r = readObj(t, cubeObj, EMeshReadFlags_None, false, false);

	if(!Test_assert(t, "read", r.ok))
		return;

	Test_assert(t, "positions", Buffer_length(r.positions) == 24 * 3 * sizeof(F32));
	Test_assert(t, "indices", Buffer_length(r.indices) == 36 * sizeof(U32));
	Test_assert(t, "noAttributes", !Buffer_length(r.attributes));
	Test_assert(t, "noWords", !Buffer_length(r.triangles));
	Test_assert(t, "stillReportsWhatTheFileHad", r.info.hasNormals && r.info.hasUvs);

	MeshResult_free(t, &r);
}

static void expectRefused(Test *t, const C8 *name, const C8 *obj) {

	MeshResult r = readObj(t, obj, EMeshReadFlags_None, true, true);

	//A refusal leaves the error set, and an assert made with one pending counts as failed, so it is cleared
	// first and the refusal itself is what gets asserted.

	const Bool refused = !r.ok;
	t->err = Error_none();

	Test_assert(t, name, refused);
	MeshResult_free(t, &r);
}

void Test_OBJValidation(Test *t) {

	Test_setModule(t, "OBJ/validation");

	expectRefused(t, "zeroIndex", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 0 1 2\n");
	expectRefused(t, "indexPastEnd", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n");
	expectRefused(t, "negativePastStart", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf -4 1 2\n");
	expectRefused(t, "uvPastEnd", "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/1 2/2 3/1\n");
	expectRefused(t, "normalPastEnd", "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2//2 3//1\n");
	expectRefused(t, "twoCorners", "v 0 0 0\nv 1 0 0\nf 1 2\n");
	expectRefused(t, "threeSlashes", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1/1/1/1 2 3\n");
	expectRefused(t, "badFloat", "v 0 abc 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
	expectRefused(t, "shortPosition", "v 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
	expectRefused(t, "badIndex", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf a 2 3\n");

	//The required sinks are required.

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	MemoryStreamRef *src = NULL;
	const C8 *obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";

	if(MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(obj, 33), 0, 33, EMemoryStreamFlags_None, &type, &src, &t->err
	)) {

		const MeshOutput output = (MeshOutput) { 0 };
		MeshInfo info;
		U64 off = 0;

		Test_assert(t, "positionsRequired", !OBJ_read((StreamRef*) src, &off, EMeshReadFlags_None, &info, &output, t->alloc, NULL));
		Test_assert(t, "nullOutput", !OBJ_read((StreamRef*) src, &off, EMeshReadFlags_None, &info, NULL, t->alloc, NULL));
	}

	RefPtr_dec((RefPtr**) &src);
}
