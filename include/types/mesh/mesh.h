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

//types/mesh/mesh.h
//
//What every mesh reader hands back, whichever file it read.
//A consumer building a vertex buffer and a BLAS should not care which format the bytes came from, so each
// format only decides how to fill these.

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct RefPtr RefPtr;
typedef RefPtr StreamRef;

//Positions travel apart from the rest: they are what a BLAS and a depth pass read, dense with nothing between
// them, where the rest is touched only when a hit is shaded.
//Three F32s, or four snorm16 under QuantizePositions, so no consumer pays for a stride it did not ask for.

//8 bytes, everything a vertex carries that is not its position. Shading normal as oct32, about 0.005 degrees,
// and uv as F16x2: the file held floats, but nothing shading a mesh can tell, and these go straight to the GPU.

typedef struct MeshAttribute {
	U32 normal;                   //Oct32; the word for (0, 0, 1) when the file named none and none was computed
	U32 uv;                       //F16x2, zero when the file named none for it
} MeshAttribute;

//12 bytes, the same record with the uv as two F32s, under WideUvs.
//An F16 uv holds about 1/2000 of its range, a texel of a 2k texture, and one that wraps several times spends its
// bits on the integer part first.

typedef struct MeshAttributeWide {
	U32 normal;
	F32 uv[2];
} MeshAttributeWide;

//One U32 per TRIANGLE: an 18 bit octahedral geometric normal in the low bits (U32_packOct18 in
// types/math/pack.h, unpackOct18 in @pack.hlsli) and the material index in the 14 above.
//A hit reads one word for which side it is on and what it is made of, rather than three positions, a cross and a
// material fetch. 9 bits an axis is ample for a side test and deliberately coarse for shading, which the
// attributes carry.

#define MeshTriangle_normalBits 18
#define MeshTriangle_normalMask ((1u << MeshTriangle_normalBits) - 1)
#define MeshTriangle_maxMaterials (1u << (32 - MeshTriangle_normalBits))

static inline U32 MeshTriangle_pack(U32 oct18, U32 material) {
	return (oct18 & MeshTriangle_normalMask) | (material << MeshTriangle_normalBits);
}

static inline U32 MeshTriangle_material(U32 word) { return word >> MeshTriangle_normalBits; }
static inline U32 MeshTriangle_oct18(U32 word) { return word & MeshTriangle_normalMask; }

typedef struct MeshInfo {

	U32 vertexCount;              //Positions and attributes written, one each per vertex
	U32 indexCount;               //Indices written, a multiple of 3; U32 each, or U16 under NarrowIndices

	//What the FILE supplied, regardless of flags. A file naming normals on only some vertices counts as having
	// them; the rest carry zero. Read these to decide whether asking for ComputeNormals is worth it.

	Bool hasNormals;
	Bool hasUvs;

	//Whether every written record carries a REAL one, which is what a consumer binding a texture or lighting a
	// surface actually needs: a missing normal packs as +z and a missing uv as (0, 0), and both are values a
	// file could legitimately have named, so nothing downstream can tell them apart.
	//allUvs is simply whether the file named one for every vertex, since nothing ever computes a uv.
	//allNormals is that too, unless ComputeNormals is asked for, in which case it is whether every vertex was
	// touched by a triangle with area: one touched by none, or only by degenerate ones, still takes the
	// placeholder.

	Bool allNormals;
	Bool allUvs;
	U8 padding[4];

	//Of every position read, always. Under QuantizePositions it is also the space positions are in: a stored
	// snorm16 s maps to center + s / 32767 * halfExtent per axis, both taken from these two.

	F32 aabbMin[3];

	//Faces with more than three corners that got fanned, which is right for convex polygons and wrong otherwise.
	U32 fannedFaces;

	F32 aabbMax[3];

	//Distinct materials the triangles name, numbered by first use. A format with no notion of one, PLY, reports 1
	// with every triangle on material 0.
	U32 materialCount;

	//Largest absolute difference between a uv the file named and the uv written, over every vertex and both
	// components. Zero under WideUvs, and zero where the file named none.
	//The reader cannot know whether a loss matters, since that depends on a texture size it never sees, so it
	// reports the magnitude and the caller compares: a texel of a 4k texture is 1/4096, about 0.000244.
	//An F16 uv loses nothing near zero and everything past a few thousand, because it spends its bits on the
	// integer part first, so this is driven by how far the uvs wrap rather than by how fine they are.
	//
	//Positions need no equivalent: quantised ones are snorm16 over the AABB, so the worst error is
	// (aabbMax - aabbMin) / 2 / 32767 per axis and both are right here, in whatever units the file used.
	//Indices are not this kind of loss at all. A narrow index that does not fit is not approximate, it names a
	// different vertex, so NarrowIndices REFUSES at the index that crosses rather than reporting a magnitude.

	F32 maxUvError;

} MeshInfo;

//What the flat mesh form is, and what a reader does to produce it. Both directions take these: a reader is told
//what to make, a writer what it was handed. ComputeNormals is the one that only means something on the way in,
//since it asks for work rather than describing the form.

typedef enum EMeshFlags {

	EMeshFlags_None            = 0,

	//Replace every normal with one accumulated from the faces around the vertex, weighted by face area, whether
	// or not the file supplied any.
	//A normal sums over every face touching the vertex, so attributes are HELD until the last face and written at
	// the end; positions and indices still leave as the file yields them.
	//Vertices are shared only where the file shares them: an OBJ corner named with a distinct uv is a distinct
	// vertex here and gets its own sum, which is what vn is for.

	EMeshFlags_ComputeNormals  = 1 << 0,

	//Positions as four snorm16 (x, y, z and a zero w, 8 bytes) over the mesh's own bounds rather than three F32s.
	//Two thirds the bytes at 1/65536 of the extent, and it is the RGBA16s a BLAS builder takes directly, with the
	// bounds going into the instance transform. Snorm because that is the format the builders accept.
	//The bounds are not known until the last position, so positions are HELD and leave at the end. An OBJ holds
	// them regardless; a PLY only under this flag or when a triangle word needs them.

	EMeshFlags_QuantizePositions = 1 << 1,

	//Attributes as MeshAttributeWide, the uv as F32s, for textures an F16 uv cannot address. See the struct.

	EMeshFlags_WideUvs           = 1 << 2,

	//Indices as U16 rather than U32, halving the index stream for a mesh that fits in 65536 vertices.
	//Asked for rather than detected: a reader emits as it meets triangles and the vertex count is not known until
	// the file ends. A file that does not fit is REFUSED at the index that crosses, never silently widened.

	EMeshFlags_NarrowIndices     = 1 << 3

} EMeshFlags;

//Where a reader puts what it read. Two of the four are optional, and a reader asked for neither never computes
// what they would have held.
//The counts are not known until the end, so a resizable sink grows geometrically as it fills and one that cannot
// resize is simply appended to. Output leaves in chunks, so a sink pays a write per chunk rather than per vertex.

typedef struct MeshOutput {

	StreamRef *positions;         //F32[3] per vertex, or I16[4] under QuantizePositions
	U64 positionOffset;

	StreamRef *attributes;        //MeshAttribute per vertex, MeshAttributeWide under WideUvs, or NULL for geometry only
	U64 attributeOffset;

	StreamRef *indices;           //U32 per index, or U16 under NarrowIndices, three per triangle
	U64 indexOffset;

	StreamRef *triangles;         //The U32 word per triangle described above, or NULL to skip
	U64 triangleOffset;

} MeshOutput;

//What a writer reads: the mirror of MeshOutput over the same flat form.
//The EMeshFlags the read was given describe this layout too, and a writer is handed them as `layout`:
// QuantizePositions says the positions are snorm16 over the bounds in MeshInfo, NarrowIndices that the indices
// are U16, WideUvs that the attributes are the wide record. ComputeNormals has no meaning on the way out.
//There is no triangle stream here: the packed per triangle word has no spelling in OBJ or PLY.

typedef struct MeshInput {

	StreamRef *positions;         //F32[3] per vertex, or I16[4] under QuantizePositions
	U64 positionOffset;

	StreamRef *attributes;        //MeshAttribute per vertex, MeshAttributeWide under WideUvs, or NULL for none
	U64 attributeOffset;

	StreamRef *indices;           //U32 per index, or U16 under NarrowIndices, three per triangle
	U64 indexOffset;

} MeshInput;

#ifdef __cplusplus
	}
#endif
