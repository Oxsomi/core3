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

//types/mesh/test/shared/mesh_test.c

#include "types/mesh/test/mesh_test.h"
#include "types/container/memory_stream.h"
#include "types/container/ref_ptr.h"
#include "types/math/pack.h"
#include "types/math/flp.h"
#include "types/base/mathf.h"

MeshResult Test_meshRead(
	Test *t, MeshReadFunc fn, const void *bytes, U64 length, EMeshFlags flags,
	Bool withAttributes, Bool withTriangles, Error *readError
) {

	MeshResult r = (MeshResult) { 0 };
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	MemoryStreamRef *src = NULL, *positions = NULL, *attributes = NULL, *indices = NULL, *triangles = NULL;

	//The source is a REF over the caller's bytes, which is the shape a file read hands a reader.

	if(!MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(bytes, length), 0, length, EMemoryStreamFlags_None, &type, &src, &t->err
	))
		goto clean;

	if(!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &positions, &t->err))
		goto clean;

	if(!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &indices, &t->err))
		goto clean;

	if(withAttributes && !MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &attributes, &t->err))
		goto clean;

	if(withTriangles && !MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &triangles, &t->err))
		goto clean;

	const MeshOutput output = (MeshOutput) {
		.positions = (StreamRef*) positions,
		.attributes = (StreamRef*) attributes,
		.indices = (StreamRef*) indices,
		.triangles = (StreamRef*) triangles
	};

	U64 off = 0;

	if(!fn((StreamRef*) src, &off, flags, &r.info, &output, t->alloc, readError))
		goto clean;

	if(!MemoryStream_move(&positions, &r.positions, &t->err) || !MemoryStream_move(&indices, &r.indices, &t->err))
		goto clean;

	if(attributes && !MemoryStream_move(&attributes, &r.attributes, &t->err))
		goto clean;

	if(triangles && !MemoryStream_move(&triangles, &r.triangles, &t->err))
		goto clean;

	r.ok = true;

clean:

	RefPtr_dec((RefPtr**) &src);
	RefPtr_dec((RefPtr**) &positions);
	RefPtr_dec((RefPtr**) &attributes);
	RefPtr_dec((RefPtr**) &indices);
	RefPtr_dec((RefPtr**) &triangles);

	return r;
}

void MeshResult_free(Test *t, MeshResult *r) {
	Buffer_free(&r->positions, t->alloc);
	Buffer_free(&r->attributes, t->alloc);
	Buffer_free(&r->indices, t->alloc);
	Buffer_free(&r->triangles, t->alloc);
	*r = (MeshResult) { 0 };
}

const F32 *MeshResult_position(const MeshResult *r, U32 i) {
	return (const F32*) r->positions.ptr + (U64) i * 3;
}

const I16 *MeshResult_quantized(const MeshResult *r, U32 i) {
	return (const I16*) r->positions.ptr + (U64) i * 4;
}

const MeshAttribute *MeshResult_attribute(const MeshResult *r, U32 i) {
	return (const MeshAttribute*) r->attributes.ptr + i;
}

const U32 *MeshResult_triangle(const MeshResult *r, U32 i) {
	return (const U32*) r->indices.ptr + (U64) i * 3;
}

U32 MeshResult_word(const MeshResult *r, U32 i) {
	return ((const U32*) r->triangles.ptr)[i];
}

Bool Test_near(F32 a, F32 b) {
	return F32_approxEq(a, b, 1e-5f);
}

//The attribute unpacked, so a test compares against what the file said rather than against an encoding.

F32x4 MeshResult_normal(const MeshResult *r, U32 i) {
	return F32x4_unpackOct32(MeshResult_attribute(r, i)->normal);
}

F32 MeshResult_uv(const MeshResult *r, U32 i, U8 axis) {
	const U32 packed = MeshResult_attribute(r, i)->uv;
	return F16_castF32((F16) (axis ? packed >> 16 : packed & 0xFFFF));
}

//Oct32 lands an axis exactly and everything else to a few hundredths of a degree, so the comparison is
// absolute rather than relative. unpackOct32 leaves w at zero, which create3 matches.

Bool Test_nearNormal(F32x4 n, F32 x, F32 y, F32 z) {
	return F32x4_eqApproxAdv4(n, F32x4_create3(x, y, z), 0, 1e-4f);
}

Buffer Test_meshWrite(Test *t, MeshWriteFunc fn, const MeshResult *r, EMeshFlags layout) {

	Buffer out = Buffer_createNull();
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	MemoryStreamRef *positions = NULL, *attributes = NULL, *indices = NULL, *dst = NULL;

	if(!MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(r->positions.ptr, Buffer_length(r->positions)), 0, Buffer_length(r->positions),
		EMemoryStreamFlags_None, &type, &positions, &t->err
	))
		goto clean;

	if(!MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(r->indices.ptr, Buffer_length(r->indices)), 0, Buffer_length(r->indices),
		EMemoryStreamFlags_None, &type, &indices, &t->err
	))
		goto clean;

	if(Buffer_length(r->attributes) && !MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(r->attributes.ptr, Buffer_length(r->attributes)), 0, Buffer_length(r->attributes),
		EMemoryStreamFlags_None, &type, &attributes, &t->err
	))
		goto clean;

	if(!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &dst, &t->err))
		goto clean;

	const MeshInput input = (MeshInput) {
		.positions = (StreamRef*) positions,
		.attributes = (StreamRef*) attributes,
		.indices = (StreamRef*) indices
	};

	U64 off = 0;

	if(!fn(&input, &r->info, layout, (StreamRef*) dst, &off, t->alloc, &t->err))
		goto clean;

	if(!MemoryStream_move(&dst, &out, &t->err))
		out = Buffer_createNull();

clean:

	RefPtr_dec((RefPtr**) &positions);
	RefPtr_dec((RefPtr**) &attributes);
	RefPtr_dec((RefPtr**) &indices);
	RefPtr_dec((RefPtr**) &dst);

	return out;
}

void Test_meshTrianglesMatch(Test *t, const C8 *name, const MeshResult *a, const MeshResult *b) {

	if(!Test_assert(t, name, a->info.indexCount == b->info.indexCount))
		return;

	Bool same = true;

	for(U32 tri = 0; tri < a->info.indexCount / 3 && same; ++tri) {

		const U32 *ia = MeshResult_triangle(a, tri);
		const U32 *ib = MeshResult_triangle(b, tri);

		for(U8 c = 0; c < 3 && same; ++c) {

			const F32 *pa = MeshResult_position(a, ia[c]);
			const F32 *pb = MeshResult_position(b, ib[c]);

			for(U8 axis = 0; axis < 3; ++axis)
				same = same && Test_near(pa[axis], pb[axis]);
		}
	}

	Test_assert(t, name, same);
}
