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

//types/mesh/mesh_derive.c
//
//The passes that run OVER a finished flat mesh rather than inside a read: vertex normals, triangle words and
//hit records. Each of them exists twice by design, here and on a GPU, because a big mesh wants the parallel
//one and a converter with no device wants this one. These are the reference: a GPU pass is checked against
//what this writes, which is why every value here is derived the same way a reader derives it.

#include "mesh_internal.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/math/vec4f.h"
#include "types/math/pack.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

//---------------------------------------------------------------- Reading the flat form back

static U32 MeshFlat_index(const MeshFlat *mesh, U64 at) {
	return mesh->narrowIndices ? ((const U16*) mesh->indices.ptr)[at] : ((const U32*) mesh->indices.ptr)[at];
}

//A quantized position is the extent the bounds describe, so decoding one needs them; a float position is what
//was read. Both leave as three floats in the mesh's own space.

//As a VECTOR, because every caller wants one: the codec writes three floats, so handing those back made each
//caller load them again and hold an array it never read componentwise.

static F32x4 MeshFlat_position(const MeshFlat *mesh, U32 vertex) {

	const ETextureFormatId id = (ETextureFormatId) mesh->positionFormat;
	const U8 size = MeshAttribute_formatSize(id);

	F32 out[3] = { 0, 0, 0 };
	MeshAttribute_decode(mesh->positions.ptr + (U64) vertex * size, id, out, 3);

	//Quantized positions are the snorm extent of the bounds. Scalar and in this order because the result is
	// packed into a word another pass compares against, so the rounding has to stay the one it was.

	if(MeshPositionFormat_isQuantized(id))
		for(U8 i = 0; i < 3; ++i) {
			const F32 half = (mesh->aabbMax[i] - mesh->aabbMin[i]) * 0.5f;
			out[i] = (mesh->aabbMin[i] + mesh->aabbMax[i]) * 0.5f + out[i] * half;
		}

	return F32x4_load3(out);
}

//Twice the area of the triangle, in the direction it faces. Unnormalized on purpose: its length is exactly the
//weight a smooth normal wants, so a sliver contributes as little as it deserves to.

static F32x4 MeshFlat_faceNormal(const MeshFlat *mesh, U64 triangle, U32 *corners) {

	F32x4 p[3];

	for(U8 c = 0; c < 3; ++c) {
		corners[c] = MeshFlat_index(mesh, triangle * 3 + c);
		p[c] = MeshFlat_position(mesh, corners[c]);
	}

	return F32x4_cross3(F32x4_sub(p[1], p[0]), F32x4_sub(p[2], p[0]));
}

//---------------------------------------------------------------- Description

Bool MeshFlat_fromInfo(
	const MeshInfo *info,
	EMeshFlags flags,
	Buffer positions,
	Buffer indices,
	Buffer attributes,
	Buffer triangles,
	MeshFlat *flat,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!info || !flat)
		retError(clean, Error_nullPointer(0, "MeshFlat_fromInfo()::info and flat are required"));

	*flat = (MeshFlat) {
		.positions = positions,
		.indices = indices,
		.attributes = attributes,
		.triangles = triangles,
		.attributeLayout = MeshAttributeLayout_fromFlags(flags),
		.positionFormat = (U8) MeshPositionFormat_fromFlags(flags),
		.narrowIndices = !!(flags & EMeshFlags_NarrowIndices),
		.vertexCount = info->vertexCount,
		.triangleCount = info->indexCount / 3
	};

	for(U8 i = 0; i < 3; ++i) {
		flat->aabbMin[i] = info->aabbMin[i];
		flat->aabbMax[i] = info->aabbMax[i];
	}

	//Checked here so a pass that derives from this never reads past a stream that was shorter than its count.

	const U64 positionBytes = (U64) flat->vertexCount * MeshAttribute_formatSize((ETextureFormatId) flat->positionFormat);
	const U64 indexBytes = (U64) flat->triangleCount * 3 * (flat->narrowIndices ? sizeof(U16) : sizeof(U32));

	if(Buffer_length(positions) < positionBytes)
		retError(clean, Error_outOfBounds(
			2, Buffer_length(positions), positionBytes, "MeshFlat_fromInfo()::positions is shorter than vertexCount"
		));

	if(Buffer_length(indices) < indexBytes)
		retError(clean, Error_outOfBounds(
			3, Buffer_length(indices), indexBytes, "MeshFlat_fromInfo()::indices is shorter than indexCount"
		));

	if(
		Buffer_length(attributes) &&
		Buffer_length(attributes) < (U64) flat->vertexCount * flat->attributeLayout.stride
	)
		retError(clean, Error_outOfBounds(
			4, Buffer_length(attributes), (U64) flat->vertexCount * flat->attributeLayout.stride,
			"MeshFlat_fromInfo()::attributes is shorter than vertexCount"
		));

	if(Buffer_length(triangles) && Buffer_length(triangles) < (U64) flat->triangleCount * sizeof(U32))
		retError(clean, Error_outOfBounds(
			5, Buffer_length(triangles), (U64) flat->triangleCount * sizeof(U32),
			"MeshFlat_fromInfo()::triangles is shorter than the triangle count"
		));

clean:
	return s_uccess;
}

//---------------------------------------------------------------- Normals

Bool MeshFlat_computeNormals(const MeshFlat *mesh, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	ListF32 sums = (ListF32) { 0 };

	if(!mesh)
		retError(clean, Error_nullPointer(0, "MeshFlat_computeNormals()::mesh is required"));

	const MeshAttributeEntry *entry = MeshAttributeLayout_find(&mesh->attributeLayout, EMeshAttribute_Normal);

	if(!entry)
		retError(clean, Error_invalidState(0, "MeshFlat_computeNormals()::mesh has no normal attribute"));

	if(Buffer_isConstRef(mesh->attributes))
		retError(clean, Error_constData(0, 0, "MeshFlat_computeNormals()::mesh attributes are read only"));

	gotoIfError3(clean, ListF32_resize(&sums, (U64) mesh->vertexCount * 3, alloc, e_rr));

	//A degenerate triangle has no direction to contribute and its cross is the zero vector, so it is skipped
	// rather than summed: adding zero is the same answer, and skipping keeps a mesh of them from deciding a
	// vertex is unreachable when it is not.

	for(U64 t = 0; t < mesh->triangleCount; ++t) {

		U32 corners[3];
		const F32x4 n = MeshFlat_faceNormal(mesh, t, corners);

		if(F32x4_sqLen3(n) <= 0)
			continue;

		for(U8 c = 0; c < 3; ++c) {

			if(corners[c] >= mesh->vertexCount)
				retError(clean, Error_outOfBounds(
					0, corners[c], mesh->vertexCount, "MeshFlat_computeNormals() index past the vertex count"
				));

			F32 *sum = sums.ptrNonConst + (U64) corners[c] * 3;
			F32x4_store3(sum, F32x4_add(F32x4_load3(sum), n));
		}
	}

	//A vertex no triangle with area touched keeps the placeholder Mesh_packNormal gives the zero vector, which
	// is what a reader writes for the same vertex.

	for(U64 v = 0; v < mesh->vertexCount; ++v) {

		U8 *record = mesh->attributes.ptrNonConst + v * mesh->attributeLayout.stride + entry->offset;
		const F32 *sum = sums.ptr + v * 3;

		if(entry->encoding == EMeshAttributeEncoding_Oct) {
			const U32 packed = Mesh_packNormal(sum);
			Buffer_memcpy(Buffer_createRef(record, sizeof(packed)), Buffer_createRefConst(&packed, sizeof(packed)));
			continue;
		}

		F32 unit[3] = { 0, 0, 1 };

		if(F32x4_sqLen3(F32x4_load3(sum)) > 0)
			F32x4_store3(unit, F32x4_normalize3(F32x4_load3(sum)));

		MeshAttribute_encode(record, (ETextureFormatId) entry->format, unit, 3);
	}

clean:
	ListF32_free(&sums, alloc);
	return s_uccess;
}

//---------------------------------------------------------------- Triangle words

Bool MeshFlat_computeWords(const MeshFlat *mesh, U32 material, Error *e_rr) {

	Bool s_uccess = true;

	if(!mesh)
		retError(clean, Error_nullPointer(0, "MeshFlat_computeWords()::mesh is required"));

	if(material >= MeshTriangle_maxMaterials)
		retError(clean, Error_outOfBounds(
			1, material, MeshTriangle_maxMaterials, "MeshFlat_computeWords()::material past what a word holds"
		));

	if(Buffer_length(mesh->triangles) < (U64) mesh->triangleCount * sizeof(U32))
		retError(clean, Error_outOfBounds(
			0, Buffer_length(mesh->triangles), (U64) mesh->triangleCount * sizeof(U32),
			"MeshFlat_computeWords()::mesh has no triangle stream to write"
		));

	if(Buffer_isConstRef(mesh->triangles))
		retError(clean, Error_constData(0, 0, "MeshFlat_computeWords()::mesh triangles are read only"));

	U32 *words = (U32*) mesh->triangles.ptrNonConst;

	for(U64 t = 0; t < mesh->triangleCount; ++t) {

		U32 corners[3];
		const F32x4 n = MeshFlat_faceNormal(mesh, t, corners);
		const F32 len2 = F32x4_sqLen3(n);

		//A degenerate triangle has no side to be on. Its normal packs to the +z encoding, which is what
		// unpacking a zero word yields, and the material still rides above it.

		const U32 oct = len2 > 0 ? U32_packOct18(F32x4_normalize3(n)) : U32_packOct18(F32x4_create3(0, 0, 1));
		words[t] = MeshTriangle_pack(oct, material);
	}

clean:
	return s_uccess;
}
