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
#include "types/container/buffer.h"
#include "types/container/texture_format.h"
#include "types/math/flp.h"
#include "types/base/types.h"
#include "types/base/c8.h"
#include "types/base/mathf.h"

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

//What an attribute MEANS, separate from how it is stored. A reader fills the ones it recognizes and a
//consumer reads the ones it wants, and neither has to agree on a record shape to do it.

typedef enum EMeshAttribute {

	EMeshAttribute_Normal,
	EMeshAttribute_Tangent,
	EMeshAttribute_Uv0,
	EMeshAttribute_Uv1,             //A lightmap's second set, which a raster pipeline still wants
	EMeshAttribute_Color,

	EMeshAttribute_Count

} EMeshAttribute;

//How the bits are read beyond what the format says. A unit vector fits in one 32 bit word octahedrally, which
//no texture format can express on its own, so the encoding expresses it instead.

typedef enum EMeshAttributeEncoding {

	EMeshAttributeEncoding_Raw,     //The format's components, in order
	EMeshAttributeEncoding_Oct,     //A unit vector in one unsigned 32 bit word, through U32_packOct32

	EMeshAttributeEncoding_Count

} EMeshAttributeEncoding;

//One row of the table that describes a record: which attribute it is, how it is stored, where it sits.

typedef struct MeshAttributeEntry {
	U8 attribute;                 //EMeshAttribute
	U8 encoding;                  //EMeshAttributeEncoding
	U8 format;                    //ETextureFormatId, Count is under 0xF0 so it fits
	U8 offset;                    //Bytes into the record
} MeshAttributeEntry;

#define MeshAttributeLayout_maxEntries 8

//The shape of an attribute record, described rather than compiled in. The record shapes EMeshFlags selects are
//constants of this type, so the flags are sugar over the table and not a switch the writer contains.
//Adding a tangent, a vertex color or a second uv is a row here. It is not a new struct, a new flag bit and a
//new arm in every function that touches a record.

typedef struct MeshAttributeLayout {
	MeshAttributeEntry entries[MeshAttributeLayout_maxEntries];
	U8 entryCount;
	U8 stride;
	U8 padding[6];
} MeshAttributeLayout;

//Where an attribute sits in a layout, or NULL when the layout does not carry it.

static inline const MeshAttributeEntry *MeshAttributeLayout_find(const MeshAttributeLayout *l, EMeshAttribute c) {

	if(!l)
		return NULL;

	for(U8 i = 0; i < l->entryCount; ++i)
		if(l->entries[i].attribute == (U8) c)
			return l->entries + i;

	return NULL;
}

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

	//FILL a vertex's normal from the faces around it, weighted by face area, where the file supplied none.
	//One the file DID supply is KEPT, so a consumer can ask for this unconditionally and pay nothing on a file
	// that already has them.
	//A sum spans every face touching the vertex, so attributes are HELD until the last face; positions and
	// indices still leave as the file yields them, and a header that already names normals skips the holding.
	//Vertices are shared only where the FILE shares them, so an OBJ corner with a distinct uv gets its own sum.

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

//A layout from the attributes it carries: they land in the order given, each after the last, and the stride is
//what that comes to. Packed tight and never padded, since the codec goes through memcpy and a byte address
// buffer has no alignment to satisfy. An offset the caller filled in is recomputed, and anything the walk
// below refuses takes the whole layout with it rather than half building one.

static inline MeshAttributeLayout MeshAttributeLayout_create(const MeshAttributeEntry *entries, U8 count) {

	MeshAttributeLayout layout = (MeshAttributeLayout) { 0 };

	if(!entries || !count || count > MeshAttributeLayout_maxEntries)
		return layout;

	U32 at = 0;

	for(U8 i = 0; i < count; ++i) {

		MeshAttributeEntry e = entries[i];

		if(e.attribute >= EMeshAttribute_Count || e.encoding >= EMeshAttributeEncoding_Count)
			return (MeshAttributeLayout) { 0 };

		//An oct encoded attribute is one U32 whatever format the caller named, because the width is the
		// ENCODING's and not the format's.

		if(e.encoding == EMeshAttributeEncoding_Oct)
			e.format = ETextureFormatId_R32u;

		if(!ETextureFormatId_canCodec((ETextureFormatId) e.format))
			return (MeshAttributeLayout) { 0 };

		for(U8 k = 0; k < i; ++k)
			if(layout.entries[k].attribute == e.attribute)
				return (MeshAttributeLayout) { 0 };

		e.offset = (U8) at;
		at += ETextureFormatId_texelBytes((ETextureFormatId) e.format);

		if(at > 0xFF)
			return (MeshAttributeLayout) { 0 };

		layout.entries[i] = e;
	}

	layout.entryCount = count;
	layout.stride = (U8) at;

	return layout;
}

//The layout EMeshFlags asks for, which is one shape of the many above. A consumer wanting a normal with no uv,
//a tangent or a second uv builds its own; the flags stay what a READER is told, since no file format spells
// the rest.

static inline MeshAttributeLayout MeshAttributeLayout_fromFlags(EMeshFlags flags) {

	const MeshAttributeEntry entries[2] = {
		{
			.attribute = EMeshAttribute_Normal,
			.encoding = EMeshAttributeEncoding_Oct,
			.format = ETextureFormatId_R32u
		},
		{
			.attribute = EMeshAttribute_Uv0,
			.encoding = EMeshAttributeEncoding_Raw,
			.format = (U8) ((flags & EMeshFlags_WideUvs) ? ETextureFormatId_RG32f : ETextureFormatId_RG16f)
		}
	};

	return MeshAttributeLayout_create(entries, 2);
}

//The position stream's format, which the flags select between today and a descriptor can name outright.
//An SNorm or UNorm format is the quantized case: the values are the extent the MeshInfo bounds describe, so a
//consumer decodes them through those bounds. A float format carries positions as they were read.

static inline ETextureFormatId MeshPositionFormat_fromFlags(EMeshFlags flags) {
	return (flags & EMeshFlags_QuantizePositions) ? ETextureFormatId_RGBA16s : ETextureFormatId_RGB32f;
}

static inline Bool MeshPositionFormat_isQuantized(ETextureFormatId id) {
	const ETexturePrimitive p = ETextureFormat_getPrimitive(ETextureFormatId_unpack[id]);
	return p == ETexturePrimitive_SNorm || p == ETexturePrimitive_UNorm;
}

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

//The flat form a read produced, as ONE value: the streams plus what shape they are in. Everything derived from
//a mesh takes this rather than a dozen parameters that can disagree about what they describe, and a caller
//builds it from what the reader handed back with MeshFlat_fromInfo.
//
//The buffers are REFERENCED, not owned. Deriving never resizes them, so a caller's own allocations stay its own.
//
//This form is RESIDENT, where the reader's own output is streams throughout, and only POSITIONS make it so: a
//triangle names three arbitrary vertices. The other three are sequential, so the residency is a
// simplification rather than a requirement, and docs/roadmap.md carries what it would take to lift.
//A caller handing a mesh to a device builds none of this and derives there instead.

typedef struct MeshFlat {

	Buffer positions;             //F32[3] per vertex, or the positionFormat's record
	Buffer indices;               //U32 per index, or U16 under narrowIndices
	Buffer attributes;            //attributeLayout.stride per vertex, empty where the mesh has none
	Buffer triangles;             //One U32 word per triangle, empty where the mesh has none

	MeshAttributeLayout attributeLayout;

	U8 positionFormat;            //ETextureFormatId
	Bool narrowIndices;
	U8 padding[6];

	U32 vertexCount;
	U32 triangleCount;

	//The space quantized positions are in, and what a consumer needs to decode them. Copied from the MeshInfo.

	F32 aabbMin[3];
	F32 aabbMax[3];

} MeshFlat;

//The flat form from what a reader returned. The FLAGS the read was given are what say which shape the streams
//are in, so passing the same flags is what makes the description true rather than a second place to keep right.
//Lengths are checked against the counts: a stream shorter than its count is refused here rather than read past
// by whatever derives from it.

Bool MeshFlat_fromInfo(
	const MeshInfo *info,
	EMeshFlags flags,
	Buffer positions,
	Buffer indices,
	Buffer attributes,
	Buffer triangles,
	MeshFlat *flat,
	Error *e_rr
);

//FILL every vertex normal from the faces around it, weighted by face area, over a mesh that has already been
//read. Exactly what EMeshFlags_ComputeNormals does inside a reader, as a pass a caller can run instead, or
// hand to a GPU and run this only to check that one.
//Asking a reader for the flag makes it HOLD every attribute until the last face; running this instead lets the
// read stream straight out and costs one more pass over an array that is already in memory.
//Unlike the reader's, this REPLACES what the attributes hold, since a pass over a finished mesh cannot know
// which normals a file supplied. Run it when MeshInfo said the file named none.
//The attributes buffer is written in place and must not be a const reference.

Bool MeshFlat_computeNormals(const MeshFlat *mesh, const Allocator *alloc, Error *e_rr);

//Fill the per triangle word: an oct18 of the geometric normal and the material above it, for every triangle.
//The same relationship to a reader as MeshFlat_computeNormals: asking a reader for a triangle stream makes it
// hold every POSITION until the last face, where this reads the positions back out of the finished mesh.
//The triangles buffer is written in place and must not be a const reference.

Bool MeshFlat_computeWords(const MeshFlat *mesh, U32 material, Error *e_rr);

#ifdef __cplusplus
	}
#endif
