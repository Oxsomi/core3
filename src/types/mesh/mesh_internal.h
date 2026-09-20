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

//types/mesh/mesh_internal.h
//
//What the mesh readers share and nothing else needs: the input window, the chunked sink, and what every
// reader does with a vertex and a triangle once it has them. Internal, so it lives beside the readers rather than
// in include/.

#pragma once
#include "types/mesh/mesh.h"
#include "types/container/stream.h"
#include "types/math/vec4.h"
#include "types/container/list_basic_types.h"
#include "types/base/string_base.h"

typedef struct Allocator Allocator;
typedef struct Error Error;

//The window a reader pulls the file through. A dragon is a 30 MiB text file and its decoded form is a fraction
// of that, so mapping the source whole would be the largest allocation of the read.

#define MESH_WINDOW 65536

//Records leave in chunks of this many bytes, so a sink pays a write per chunk rather than per vertex.
//That matters twice over: a file sink pays a syscall per write, and a resizable memory sink that grows to
// exactly what each write needs would copy itself once per vertex.

#define MESH_CHUNK 65536

//A text line longer than this is refused. Only an n-gon with thousands of corners gets near it.

#define MESH_LINE_CAP 65536

typedef struct MeshSource {

	StreamCursor cursor;          //Its cache is the window, MESH_WINDOW bytes
	const Allocator *alloc;

	U64 size;                     //Of the stream, so the end is known without a read
	U64 pos;                      //Absolute stream offset the reader is at

} MeshSource;

Bool MeshSource_create(StreamRef *stream, U64 off, const Allocator *alloc, MeshSource *src, Error *e_rr);
void MeshSource_free(MeshSource *src);

//How many bytes sit contiguously at the cursor, moving the window when the cursor has left it. Zero at the
// end of the stream, which is not an error here: whether the end was expected is the reader's to decide.
//A scanner keeps this check for itself rather than reading through the cursor each time, since the cursor's own
// read relocates the window whenever the request does not start at its first byte.

Bool MeshSource_available(MeshSource *src, U64 *available, Error *e_rr);

//Only meaningful right after MeshSource_available reported at least one byte

static inline const U8 *MeshSource_ptr(const MeshSource *src) {
	return src ? src->cursor.cacheData.ptr + (src->pos - src->cursor.lastLocation) : NULL;
}

static inline void MeshSource_advance(MeshSource *src, U64 n) { if(src) src->pos += n; }
static inline U64 MeshSource_offset(const MeshSource *src) { return src ? src->pos : 0; }

//Exactly n bytes, across window boundaries. Fewer than n left is an error: a binary body is sized by its header.

Bool MeshSource_readBytes(MeshSource *src, void *dst, U64 n, Error *e_rr);
Bool MeshSource_skip(MeshSource *src, U64 n, Error *e_rr);

//One line without its terminator, either ending accepted. got is false at the end of the stream, which leaves
// line untouched. A line at the cap is refused rather than split.

Bool MeshSource_readLine(MeshSource *src, C8 *line, U64 cap, U64 *len, Bool *got, Error *e_rr);

//The chunked sink: a write cursor whose cache is the chunk, so records leave once per MESH_CHUNK bytes rather
// than once each. A NULL stream is a sink that discards, which is what an output the caller left NULL is, so a
// reader writes unconditionally and the check lives here once.
//
//Capacity is reserved geometrically ahead of the writes on a stream that can reserve, and never on one that
// cannot: a file simply grows, and a memory stream that was handed a fixed buffer refuses at the write itself.

typedef struct MeshSink {

	StreamCursor cursor;          //Unset for the sink that discards
	const Allocator *alloc;

	U64 base;                     //Stream offset the first record sits at
	U64 written;                  //Bytes handed to the cursor past base
	U64 reserved;                 //Capacity asked for past base, on streams that can reserve

} MeshSink;

Bool MeshSink_create(StreamRef *stream, U64 off, const Allocator *alloc, MeshSink *sink, Error *e_rr);
static inline Bool MeshSink_active(const MeshSink *sink) { return sink && sink->cursor.stream; }

Bool MeshSink_write(MeshSink *sink, const void *data, U64 bytes, Error *e_rr);

//One formatted piece, which is what the text forms of a mesh are built out of. The sink's own allocator is
// used, and a sink that discards formats nothing at all rather than formatting and throwing it away.

Bool MeshSink_writeFormatted(MeshSink *sink, Error *e_rr, const C8 *format, ...);
Bool MeshSink_flush(MeshSink *sink, Error *e_rr);
void MeshSink_free(MeshSink *sink);

//Positions leave as they come as three F32s, unless they are being quantized, in which case they are held until
// the bounds are complete and written at the end as four snorm16. The bounds are tracked either way.

typedef struct MeshPositions {

	MeshSink *sink;
	ListF32 held;                 //3 per vertex while holding
	
	F32x4 aabbMin, aabbMax;       //Three lanes used, the fourth stays at zero

	U64 heldCapacity;
	U32 count;
	Bool hold;
	U8 padding[3];

} MeshPositions;

void MeshPositions_create(EMeshFlags flags, MeshSink *sink, MeshPositions *positions);
Bool MeshPositions_push(MeshPositions *p, const F32 *position, const Allocator *alloc, Error *e_rr);
Bool MeshPositions_finish(MeshPositions *p, MeshInfo *info, Error *e_rr);
void MeshPositions_free(MeshPositions *p, const Allocator *alloc);

//What every reader does with a triangle once its corners are known: the indices leave, the packed word leaves
// when asked for, and the area weighted normal is summed into its three corners when normals are being computed.
//The cross product is only taken when either of the last two wants it.

typedef struct MeshTriangles {

	MeshSink *indices;
	MeshSink *words;

	ListF32 normalSums;           //3 per vertex while computing normals, otherwise empty
	Bool computeNormals;
	Bool narrowIndices;           //U16 per index instead of U32
	U8 padding[2];

	U32 count;                    //Triangles emitted

} MeshTriangles;

void MeshTriangles_create(EMeshFlags flags, MeshSink *indices, MeshSink *words, MeshTriangles *triangles);

//Whether a reader has to be able to hand positions to emit, which is what decides if it keeps them.

static inline Bool MeshTriangles_needsPositions(const MeshTriangles *t) {
	return t && (t->computeNormals || MeshSink_active(t->words));
}

Bool MeshTriangles_emit(
	MeshTriangles *t,
	U32 i0, U32 i1, U32 i2,
	U32 material,
	const F32 *p0, const F32 *p1, const F32 *p2,      //Only read when needsPositions
	const Allocator *alloc, Error *e_rr
);

void MeshTriangles_free(MeshTriangles *t, const Allocator *alloc);

//Attributes leave as they come, unless normals are being computed, in which case they are held until the last
// face and finished with the sums.

typedef struct MeshAttributes {

	MeshSink *sink;
	ListF32 held;                 //5 per vertex while holding
	U64 heldCapacity;             //What was reserved, so the doubling never asks the list

	Bool hold;
	Bool wide;                    //MeshAttributeWide records rather than MeshAttribute
	Bool placeholderNormal;       //A computed normal fell back to +z, so some vertex had no triangle with area
	U8 padding;

	F32 maxUvError;               //Worst F16 round trip over every uv written, 0 when wide

} MeshAttributes;

//normal and uv as the file gave them; the record leaves packed. A file that named no normal passes a zero one.

void MeshAttributes_create(EMeshFlags flags, MeshSink *sink, MeshAttributes *attributes);
Bool MeshAttributes_push(MeshAttributes *a, const F32 *normal, const F32 *uv, const Allocator *alloc, Error *e_rr);
Bool MeshAttributes_finish(MeshAttributes *a, const MeshTriangles *t, U32 vertexCount, Error *e_rr);
void MeshAttributes_free(MeshAttributes *a, const Allocator *alloc);
