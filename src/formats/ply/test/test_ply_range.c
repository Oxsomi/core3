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

//formats/ply/test/test_ply_range.c

#include "test_ply_shared.h"
#include "formats/ply/ply_file.h"
#include "types/container/memory_stream.h"
#include "types/container/ref_ptr.h"
#include "types/container/buffer.h"
#include "types/math/type_cast.h"
#include "types/base/string_base.h"
#include "types/base/error.h"

//A body read in SPANS has to produce what reading it once produces, byte for byte, or nothing downstream can
//use a span. That is the whole of what these check: the header alone says where the body is and how wide a
// record is, and the spans then reproduce the file the ordinary reader produces.

#define RANGE_VERTICES 9

static U64 putLE32(U8 *out, U32 v) {
	out[0] = (U8) v; out[1] = (U8) (v >> 8); out[2] = (U8) (v >> 16); out[3] = (U8) (v >> 24);
	return 4;
}

static U64 putLEF32(U8 *out, F32 v) {
	return putLE32(out, U32_fromF32Bits(v));
}

//Nine vertices of x y z nx ny nz s t, so one record is 32 bytes and nine of them split three ways evenly.
//Every component differs per vertex, so a span that lands at the wrong offset cannot look right by accident.

static U64 buildRangePly(U8 *out) {

	const C8 *header =
		"ply\n"
		"format binary_little_endian 1.0\n"
		"element vertex 9\n"
		"property float x\nproperty float y\nproperty float z\n"
		"property float nx\nproperty float ny\nproperty float nz\n"
		"property float s\nproperty float t\n"
		"element face 1\n"
		"property list uchar int vertex_indices\n"
		"end_header\n";

	U64 n = 0;

	for(const C8 *c = header; *c; ++c)
		out[n++] = (U8) *c;

	for(U8 v = 0; v < RANGE_VERTICES; ++v) {

		const F32 f = (F32) v;

		n += putLEF32(out + n, f);           n += putLEF32(out + n, f * 2);   n += putLEF32(out + n, f * 3);
		n += putLEF32(out + n, 0);           n += putLEF32(out + n, 0);       n += putLEF32(out + n, 1);
		n += putLEF32(out + n, f * 0.125f);  n += putLEF32(out + n, f * 0.25f);
	}

	out[n++] = 3;
	n += putLE32(out + n, 0);
	n += putLE32(out + n, 1);
	n += putLE32(out + n, 2);

	return n;
}

void Test_plyHeader(Test *t) {

	Test_setModule(t, "PLY/header");

	U8 bytes[1024];
	const U64 len = buildRangePly(bytes);

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	MemoryStreamRef *src = NULL;

	if(!MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(bytes, len), 0, len, EMemoryStreamFlags_None, &type, &src, &t->err
	)) {
		Test_assert(t, "source", false);
		return;
	}

	MeshInfo info = (MeshInfo) { 0 };
	MeshHeader header = (MeshHeader) { 0 };
	U64 off = 0;

	if(Test_assert(t, "readHeader", Ply_readHeader((StreamRef*) src, &off, &info, &header, t->alloc, &t->err))) {

		Test_assert(t, "vertexCount", header.vertexCount == RANGE_VERTICES);
		Test_assert(t, "faceCount", header.faceCount == 1);
		Test_assert(t, "vertexStride", header.vertexStride == 32);
		Test_assert(t, "littleEndian", !header.bigEndian);
		Test_assert(t, "declaredSemantics", info.hasNormals && info.hasUvs);

		//The body begins where the header stopped, and off is left there so the same stream can be read on.

		Test_assert(t, "bodyOffset", header.bodyOffset == off && header.bodyOffset < len);
		Test_assert(t, "bodyFits", header.bodyOffset + (U64) header.vertexCount * header.vertexStride <= len);

		//Eight wanted properties, each one scalar of one semantic, in declaration order.

		Test_assert(t, "planCount", header.planCount == 8);

		const MeshPlanEntry *x = MeshHeader_find(&header, EMeshVertexSemantic_Position, 0);
		const MeshPlanEntry *nz = MeshHeader_find(&header, EMeshVertexSemantic_Normal, 2);
		const MeshPlanEntry *v = MeshHeader_find(&header, EMeshVertexSemantic_Uv0, 1);

		Test_assert(t, "planPosition", x && !x->offset && x->type == EMeshScalarType_F32);
		Test_assert(t, "planNormal", nz && nz->offset == 20);
		Test_assert(t, "planUv", v && v->offset == 28);
		Test_assert(t, "planMisses", !MeshHeader_find(&header, EMeshVertexSemantic_Uv0, 2));

		//The faces sit straight after the vertex records, and one of them is a uchar count and three U32s.

		Test_assert(
			t, "faceOffset",
			header.faceOffset == header.bodyOffset + (U64) RANGE_VERTICES * header.vertexStride
		);

		Test_assert(t, "faceCountType", header.faceCountType == EMeshScalarType_U8);
		Test_assert(t, "faceIndexType", header.faceIndexType == EMeshScalarType_I32);        //"int", which is signed
		Test_assert(t, "faceStride", MeshHeader_faceStride(&header, 3) == 1 + 3 * 4);
		Test_assert(t, "faceStrideNeedsCorners", !MeshHeader_faceStride(&header, 0));
	}

	RefPtr_dec((RefPtr**) &src);
}

//A vertex element that declares a LIST has no fixed record, so no vertex can be seeked to and the header says
//so rather than reporting a width that only holds until the first list is read.

void Test_plyHeaderNotFixedStride(Test *t) {

	Test_setModule(t, "PLY/headerNotFixedStride");

	const C8 *ply =
		"ply\n"
		"format binary_little_endian 1.0\n"
		"element vertex 2\n"
		"property float x\nproperty float y\nproperty float z\n"
		"property list uchar float weights\n"
		"element face 1\n"
		"property list uchar int vertex_indices\n"
		"end_header\n";

	const U64 len = CharString_calcStrLen(ply, U64_MAX);

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	MemoryStreamRef *src = NULL;

	if(!MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(ply, len), 0, len, EMemoryStreamFlags_None, &type, &src, &t->err
	)) {
		Test_assert(t, "source", false);
		return;
	}

	MeshInfo info = (MeshInfo) { 0 };
	MeshHeader header = (MeshHeader) { 0 };
	U64 off = 0;

	if(Test_assert(t, "readHeader", Ply_readHeader((StreamRef*) src, &off, &info, &header, t->alloc, &t->err))) {

		Test_assert(t, "notFixedStride", !header.vertexStride);
		Test_assert(t, "countsStillKnown", header.vertexCount == 2 && header.faceCount == 1);

		//A vertex element of unknown width shifts the faces by an unknown amount, so their offset is unknown
		// too rather than being computed from a stride that does not exist.

		Test_assert(t, "faceOffsetUnknown", !header.faceOffset);
	}

	//And a span of it is refused rather than reading a width that is not there.

	MeshOutput out = (MeshOutput) { 0 };
	Error err = Error_none();
	Test_assert(
		t, "rangeRefused",
		!Ply_readRange((StreamRef*) src, &header, EMeshFlags_None, 0, 2, &info, &out, t->alloc, &err)
	);

	RefPtr_dec((RefPtr**) &src);
}

//Ascii has no record boundary whatever its properties say, so it reports no stride either.

void Test_plyHeaderAsciiNoStride(Test *t) {

	Test_setModule(t, "PLY/headerAscii");

	const C8 *ply =
		"ply\nformat ascii 1.0\n"
		"element vertex 1\n"
		"property float x\nproperty float y\nproperty float z\n"
		"element face 0\n"
		"property list uchar int vertex_indices\n"
		"end_header\n0 0 0\n";

	const U64 len = CharString_calcStrLen(ply, U64_MAX);

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	MemoryStreamRef *src = NULL;

	if(!MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(ply, len), 0, len, EMemoryStreamFlags_None, &type, &src, &t->err
	)) {
		Test_assert(t, "source", false);
		return;
	}

	MeshInfo info = (MeshInfo) { 0 };
	MeshHeader header = (MeshHeader) { 0 };
	U64 off = 0;

	if(Test_assert(t, "readHeader", Ply_readHeader((StreamRef*) src, &off, &info, &header, t->alloc, &t->err)))
		Test_assert(t, "asciiNoStride", !header.vertexStride && header.vertexCount == 1);

	RefPtr_dec((RefPtr**) &src);
}

void Test_plyRange(Test *t) {

	Test_setModule(t, "PLY/range");

	U8 bytes[1024];
	const U64 len = buildRangePly(bytes);

	//What the ordinary reader produces, which is the answer the spans have to reproduce.

	MeshResult whole = Test_meshRead(t, Ply_read, bytes, len, EMeshFlags_None, true, false, &t->err);

	if(!Test_assert(t, "readWhole", whole.ok))
		return;

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	MemoryStreamRef *src = NULL, *positions = NULL, *attributes = NULL;

	Bool built =
		MemoryStream_createFromBufferRegion(
			Buffer_createRefConst(bytes, len), 0, len, EMemoryStreamFlags_None, &type, &src, &t->err
		) &&
		MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &positions, &t->err) &&
		MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &attributes, &t->err);

	if(!Test_assert(t, "streams", built))
		goto clean;

	MeshInfo info = (MeshInfo) { 0 };
	MeshHeader header = (MeshHeader) { 0 };
	U64 off = 0;

	if(!Test_assert(t, "readHeader", Ply_readHeader((StreamRef*) src, &off, &info, &header, t->alloc, &t->err)))
		goto clean;

	//Zeroed before the first span, merged by each, read after the last.

	info = (MeshInfo) { 0 };

	const U32 spans[3] = { 2, 4, 3 };
	U32 first = 0;
	Bool ok = true;

	for(U8 i = 0; i < 3 && ok; ++i) {

		//Each span writes where the ones before it stopped, which is the caller's business and not the reader's.

		const MeshOutput out = (MeshOutput) {
			.positions = (StreamRef*) positions,
			.positionOffset = (U64) first * sizeof(F32) * 3,
			.attributes = (StreamRef*) attributes,
			.attributeOffset = (U64) first * sizeof(MeshAttribute)
		};

		ok = Ply_readRange(
			(StreamRef*) src, &header, EMeshFlags_None, first, spans[i], &info, &out, t->alloc, &t->err
		);

		first += spans[i];
	}

	if(!Test_assert(t, "readSpans", ok))
		goto clean;

	Test_assert(t, "spanVertexCount", info.vertexCount == RANGE_VERTICES);

	//The bounds have to be the file's, not the last span's, which is what merging is for.

	Test_assert(
		t, "mergedBounds",
		info.aabbMin[0] == whole.info.aabbMin[0] && info.aabbMax[0] == whole.info.aabbMax[0] &&
		info.aabbMin[1] == whole.info.aabbMin[1] && info.aabbMax[1] == whole.info.aabbMax[1] &&
		info.aabbMin[2] == whole.info.aabbMin[2] && info.aabbMax[2] == whole.info.aabbMax[2]
	);

	Buffer spanPositions = Buffer_createNull(), spanAttributes = Buffer_createNull();

	if(
		!Test_assert(t, "movePositions", MemoryStream_move(&positions, &spanPositions, &t->err)) ||
		!Test_assert(t, "moveAttributes", MemoryStream_move(&attributes, &spanAttributes, &t->err))
	)
		goto clean;

	//BYTE identical, not merely close: a span that decoded through a different path would round the same way
	// and still differ, and a comparison by value would not see it.

	Test_assert(t, "positionsIdentical", Buffer_eq(spanPositions, whole.positions));
	Test_assert(t, "attributesIdentical", Buffer_eq(spanAttributes, whole.attributes));

	Buffer_free(&spanPositions, t->alloc);
	Buffer_free(&spanAttributes, t->alloc);

clean:
	RefPtr_dec((RefPtr**) &attributes);
	RefPtr_dec((RefPtr**) &positions);
	RefPtr_dec((RefPtr**) &src);
	MeshResult_free(t, &whole);
}
