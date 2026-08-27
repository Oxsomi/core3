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

//formats/mesh/mesh.h
//
//What every mesh reader hands back, whichever file it read.
//A consumer building a vertex buffer and a BLAS should not care whether the bytes came from an OBJ or a PLY,
// so the streams, the counts and the flags are shared here and each format only decides how to fill them.

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct RefPtr RefPtr;
typedef RefPtr StreamRef;

//Positions travel apart from everything else, because they are what a BLAS reads and what a depth pass reads,
// dense and nothing between them, where the rest is only touched when a hit is shaded.
//Three F32s rather than a padded four, or four snorm16 under EMeshReadFlags_QuantizePositions: the point of a
// reader is to hand the bytes straight to a buffer, and a stride the consumer did not ask for costs a repack.

//8 bytes, everything a vertex carries that is not its position, packed the way a vertex buffer wants it.
//A shading normal at four bytes (U32_packOct32, about 0.005 degrees) and a uv at four (U32_packF16x2): the file
// held floats, but nothing shading a mesh can tell the difference, and the bytes go straight to the GPU.

typedef struct MeshAttribute {
	U32 normal;                   //Oct32; the word for (0, 0, 1) when the file named none and none was computed
	U32 uv;                       //F16x2, zero when the file named none for it
} MeshAttribute;

//12 bytes, the same record with the uv kept as two F32s, written under EMeshReadFlags_WideUvs.
//An F16 holds a uv to about 1/2000 of its range, which is a texel of a 2k texture and less than one of a 4k, and
// a uv that wraps a texture several times spends its bits on the integer part first. A consumer whose textures
// need better than that asks for this shape and selects its vertex layout on the same flag.

typedef struct MeshAttributeWide {
	U32 normal;
	F32 uv[2];
} MeshAttributeWide;

//One U32 per TRIANGLE: the geometric normal as an 18 bit octahedral in the low bits (U32_packOct18 in
// types/math/pack.h, unpackOct18 in @pack.hlsli) and the material index in the 14 above it.
//
//A hit loads this one word for which side of the triangle it is on and what the triangle is made of, rather than
// three positions, a cross product and a separate material fetch. 9 bits an axis is about a third of a degree,
// far more than deciding a side needs and coarse for shading, which is what the attributes are for.

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
	U32 indexCount;               //U32s written to the index stream, a multiple of 3

	//What the FILE supplied, regardless of the flags. A file that names normals on some vertices and not others
	// counts as having them; the vertices it did not cover carry zero.

	Bool hasNormals;
	Bool hasUvs;
	U8 padding[2];

	//How many faces had more than three corners and got fanned, which a consumer that cares about
	// triangulation quality wants to know about: a fan is right for convex polygons and wrong for the rest.

	U32 fannedFaces;

	//Distinct materials the triangles name, numbered in order of first use. A format with no notion of one, PLY,
	// reports 1 with every triangle on material 0.

	U32 materialCount;

	//Of every position read, always, since it costs nothing to keep and a consumer placing the mesh wants it.
	//Under EMeshReadFlags_QuantizePositions it is also the space the positions are in: a stored snorm16 s maps to
	// center + s / 32767 * halfExtent per axis, with center and halfExtent taken from these two.

	F32 aabbMin[3];
	F32 aabbMax[3];

} MeshInfo;

typedef enum EMeshReadFlags {

	EMeshReadFlags_None            = 0,

	//Replace every normal with one accumulated from the faces around the vertex, weighted by face area,
	// whether or not the file supplied any.
	//
	//A normal is a sum over every face touching the vertex, so the attributes cannot leave until the last face
	// has been read. Under this flag they are held until then and written at the end, while positions and
	// indices still leave as the file yields them; without it nothing is held past what the format itself
	// requires.
	//
	//Vertices are shared across faces only where the file shares them. In an OBJ a corner named with a
	// distinct uv is a distinct vertex here and gets its own sum, so a file wanting smooth normals across such
	// seams has to name them, which is what vn is for.

	EMeshReadFlags_ComputeNormals  = 1 << 0,

	//Positions as four snorm16 (x, y, z and a zero w, 8 bytes) over the mesh's own bounds rather than three F32s.
	//
	//Two thirds the bytes at 1/65536 of the extent, and it is the RGBA16s an acceleration structure builder takes
	// directly, with the bounds going into the instance transform rather than into every vertex. Snorm rather
	// than unorm because that is the format the builders accept; the range is [-1, 1] over the AABB either way.
	//
	//The bounds are not known until the last position, so under this flag positions leave at the end rather than
	// as the file yields them. An OBJ holds its positions regardless, since faces name them out of order, so it
	// pays nothing extra; a PLY holds them only under this flag or when a triangle word needs them.

	EMeshReadFlags_QuantizePositions = 1 << 1,

	//Attributes as MeshAttributeWide, the uv as F32s, for textures an F16 uv cannot address. See the struct.

	EMeshReadFlags_WideUvs           = 1 << 2

} EMeshReadFlags;

//Where a reader puts what it read. Two of the four are optional, and a reader asked for neither of those
// never computes what they would have held.
//
//The counts are not known until the end, which is the one way this differs from HDR_read: nothing can be
// reserved up front, so a resizable sink is grown geometrically as it fills rather than sized once.
//A sink that cannot resize, a file say, is simply appended to. Output leaves in chunks of many records,
// never one at a time, so a sink pays a write per chunk rather than per vertex.

typedef struct MeshOutput {

	StreamRef *positions;         //F32[3] per vertex, or I16[4] under QuantizePositions
	U64 positionOffset;

	StreamRef *attributes;        //MeshAttribute per vertex, MeshAttributeWide under WideUvs, or NULL for geometry only
	U64 attributeOffset;

	StreamRef *indices;           //U32 per index, three per triangle
	U64 indexOffset;

	StreamRef *triangles;         //The U32 word per triangle described above, or NULL to skip
	U64 triangleOffset;

} MeshOutput;

#ifdef __cplusplus
	}
#endif
