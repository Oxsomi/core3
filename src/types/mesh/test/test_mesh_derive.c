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

//types/mesh/test/test_mesh_derive.c

#include "test_mesh_shared.h"
#include "types/container/memory_stream.h"
#include "types/container/ref_ptr.h"
#include "types/math/pack.h"
#include "types/base/error.h"

//A unit square in z = 0 as two triangles, which is the smallest mesh where a gathered corner can be WRONG and
//still look plausible: the four vertices carry four different normals and four different uvs, so any corner a
//builder picks out of order shows up as a mismatched word rather than as the same value twice.

#define DERIVE_VERTICES 4
#define DERIVE_TRIANGLES 2

typedef struct DeriveMesh {
	F32 positions[DERIVE_VERTICES * 3];
	U32 indices[DERIVE_TRIANGLES * 3];
	MeshAttribute attributes[DERIVE_VERTICES];
	U32 words[DERIVE_TRIANGLES];
	MeshInfo info;
} DeriveMesh;

static const F32 deriveNormals[DERIVE_VERTICES][3] = {
	{ 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { -1, 0, 0 }
};

static const F32 deriveUvs[DERIVE_VERTICES][2] = {
	{ 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 }
};

static void DeriveMesh_create(DeriveMesh *m) {

	const F32 positions[DERIVE_VERTICES * 3] = { 0, 0, 0,   1, 0, 0,   1, 1, 0,   0, 1, 0 };
	const U32 indices[DERIVE_TRIANGLES * 3] = { 0, 1, 2,   0, 2, 3 };

	*m = (DeriveMesh) { 0 };

	for(U8 i = 0; i < DERIVE_VERTICES * 3; ++i)
		m->positions[i] = positions[i];

	for(U8 i = 0; i < DERIVE_TRIANGLES * 3; ++i)
		m->indices[i] = indices[i];

	for(U8 v = 0; v < DERIVE_VERTICES; ++v) {

		F32 uv[2] = { deriveUvs[v][0], deriveUvs[v][1] };

		m->attributes[v].normal = U32_packOct32(F32x4_load3(deriveNormals[v]));
		ETextureFormatId_encode((U8*) &m->attributes[v].uv, ETextureFormatId_RG16f, uv, 2);
	}

	m->info = (MeshInfo) {
		.vertexCount = DERIVE_VERTICES,
		.indexCount = DERIVE_TRIANGLES * 3,
		.hasNormals = true,
		.hasUvs = true,
		.allNormals = true,
		.allUvs = true,
		.aabbMin = { 0, 0, 0 },
		.aabbMax = { 1, 1, 0 }
	};
}

static Bool DeriveMesh_flat(Test *t, DeriveMesh *m, MeshFlat *flat) {
	return MeshFlat_fromInfo(
		&m->info, EMeshFlags_None,
		Buffer_createRef(m->positions, sizeof(m->positions)),
		Buffer_createRef(m->indices, sizeof(m->indices)),
		Buffer_createRef(m->attributes, sizeof(m->attributes)),
		Buffer_createRef(m->words, sizeof(m->words)),
		flat, &t->err
	);
}

//---------------------------------------------------------------- Normals and words

static void Test_meshDeriveNormals(Test *t) {

	Test_setModule(t, "mesh/derive normals");

	DeriveMesh m;
	MeshFlat flat;

	DeriveMesh_create(&m);

	if(!Test_assert(t, "flat", DeriveMesh_flat(t, &m, &flat)))
		return;

	if(!Test_assert(t, "computeNormals", MeshFlat_computeNormals(&flat, t->alloc, &t->err)))
		return;

	//Both triangles face +z and every vertex is touched by at least one of them, so every normal REPLACED the
	// distinct one the mesh was built with. That is the difference from the reader's flag, which fills only
	// what a file left out.

	for(U8 v = 0; v < DERIVE_VERTICES; ++v)
		Test_assert(t, "normal is +z", Test_nearNormal(F32x4_unpackOct32(m.attributes[v].normal), 0, 0, 1));

	//The uv sits in the same record and is not the normal's to touch.

	for(U8 v = 0; v < DERIVE_VERTICES; ++v) {

		F32 uv[2];
		ETextureFormatId_decode((const U8*) &m.attributes[v].uv, ETextureFormatId_RG16f, uv, 2);

		Test_assert(t, "uv kept", Test_near(uv[0], deriveUvs[v][0]) && Test_near(uv[1], deriveUvs[v][1]));
	}
}

static void Test_meshDeriveWords(Test *t) {

	Test_setModule(t, "mesh/derive words");

	DeriveMesh m;
	MeshFlat flat;

	DeriveMesh_create(&m);

	if(!Test_assert(t, "flat", DeriveMesh_flat(t, &m, &flat)))
		return;

	if(!Test_assert(t, "computeWords", MeshFlat_computeWords(&flat, 5, &t->err)))
		return;

	for(U8 i = 0; i < DERIVE_TRIANGLES; ++i) {
		Test_assert(t, "word material", MeshTriangle_material(m.words[i]) == 5);
		Test_assert(t, "word normal is +z", Test_nearNormal(F32x4_unpackOct18(MeshTriangle_oct18(m.words[i])), 0, 0, 1));
	}

	//A material the word's 14 bits cannot hold is refused rather than wrapping into the normal.

	Error err = Error_none();
	Test_assert(t, "material past the word refused", !MeshFlat_computeWords(&flat, MeshTriangle_maxMaterials, &err));
}

void Test_meshDerive(Test *t) {
	Test_meshDeriveNormals(t);
	Test_meshDeriveWords(t);
}
