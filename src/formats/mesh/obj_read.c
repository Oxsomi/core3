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

//formats/mesh/obj_read.c

#include "formats/obj/obj_file.h"
#include "mesh_internal.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/c8.h"
#include "types/base/string_read.h"

//The dedup table. A corner is a position, uv and normal index, 1 based the way the file counts and 0 where the
// corner named none, and the table maps each distinct triple to the output vertex it became.
//
//Open addressing over a power of two, kept below half full, so a lookup is a hash and a short probe rather
// than a chase through a chained structure. The keys sit in their own list indexed by output vertex, which is
// also what a triangle later needs to find its positions by.

typedef struct OBJDedup {
	ListU32 slots;                //Output vertex + 1, or 0 for empty
	ListU32 keys;                 //v, vt, vn per output vertex
	U64 mask;
	U32 count;
} OBJDedup;

static U64 OBJDedup_hash(U32 v, U32 vt, U32 vn) {

	//Three odd constants, so the three indices land apart even when they equal each other,
	// which they do for every corner of a file with matched vt and vn counts.

	U64 h = (U64) v * 0x9E3779B97F4A7C15ull;
	h ^= (U64) vt * 0xC2B2AE3D27D4EB4Full;
	h ^= (U64) vn * 0x165667B19E3779F9ull;

	h ^= h >> 32;
	h *= 0xD6E8FEB86659FD93ull;
	h ^= h >> 32;

	return h;
}

static Bool OBJDedup_grow(OBJDedup *d, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	const U64 capacity = d->slots.length ? d->slots.length * 2 : 4096;
	ListU32 slots = (ListU32) { 0 };

	gotoIfError3(clean, ListU32_resize(&slots, capacity, alloc, e_rr));

	for(U64 i = 0; i < capacity; ++i)
		slots.ptrNonConst[i] = 0;

	const U64 mask = capacity - 1;

	//Every held vertex is rehashed into the wider table from its key.

	for(U32 i = 0; i < d->count; ++i) {

		const U32 *key = d->keys.ptr + (U64) i * 3;
		U64 slot = OBJDedup_hash(key[0], key[1], key[2]) & mask;

		while(slots.ptr[slot])
			slot = (slot + 1) & mask;

		slots.ptrNonConst[slot] = i + 1;
	}

	ListU32_free(&d->slots, alloc);
	d->slots = slots;
	d->mask = mask;

clean:

	if(!s_uccess)
		ListU32_free(&slots, alloc);

	return s_uccess;
}

//The output vertex for a corner, made on first sight. isNew tells the caller the vertex has to be written.

static Bool OBJDedup_find(
	OBJDedup *d, U32 v, U32 vt, U32 vn, U32 *result, Bool *isNew, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;

	if((U64) d->count * 2 >= d->slots.length)
		gotoIfError3(clean, OBJDedup_grow(d, alloc, e_rr));

	U64 slot = OBJDedup_hash(v, vt, vn) & d->mask;

	while(d->slots.ptr[slot]) {

		const U32 *key = d->keys.ptr + (U64) (d->slots.ptr[slot] - 1) * 3;

		if(key[0] == v && key[1] == vt && key[2] == vn) {
			*result = d->slots.ptr[slot] - 1;
			*isNew = false;
			goto clean;
		}

		slot = (slot + 1) & d->mask;
	}

	if(d->count == U32_MAX)
		retError(clean, Error_overflow(0, d->count, U32_MAX, "OBJ_read() more distinct vertices than U32 indices"));

	gotoIfError3(clean, ListU32_pushBack(&d->keys, v, alloc, e_rr));
	gotoIfError3(clean, ListU32_pushBack(&d->keys, vt, alloc, e_rr));
	gotoIfError3(clean, ListU32_pushBack(&d->keys, vn, alloc, e_rr));

	d->slots.ptrNonConst[slot] = d->count + 1;
	*result = d->count++;
	*isNew = true;

clean:
	return s_uccess;
}

static void OBJDedup_free(OBJDedup *d, const Allocator *alloc) {
	ListU32_free(&d->slots, alloc);
	ListU32_free(&d->keys, alloc);
}

//The materials a file names, in order of first use, so usemtl can be mapped back to an index a triangle word
// holds. The names themselves go nowhere: what a name means is the .mtl file's business and a consumer's, and this
// reader has neither.

typedef struct OBJMaterials {
	ListCharString names;
	U32 current;                  //What the faces since the last usemtl belong to
	Bool usedDefault;             //A face was emitted before any usemtl
	U8 padding[3];
} OBJMaterials;

static Bool OBJMaterials_use(OBJMaterials *m, CharString name, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	//Faces emitted before the first usemtl were on a material of their own, so it keeps index 0 under an empty
	// name and the first named material takes 1 rather than sharing with them.

	if(!m->names.length && m->usedDefault) {

		CharString empty = CharString_createNull();
		gotoIfError3(clean, CharString_createCopy(CharString_createRefSizedConst("", 0, true), alloc, &empty, e_rr));

		if(!ListCharString_pushBack(&m->names, empty, alloc, e_rr)) {
			CharString_free(&empty, alloc);
			s_uccess = false;
			goto clean;
		}
	}

	for(U64 i = 0; i < m->names.length; ++i)
		if(CharString_equalsString(&m->names.ptr[i], &name, EStringCase_Sensitive)) {
			m->current = (U32) i;
			goto clean;
		}

	if(m->names.length >= MeshTriangle_maxMaterials)
		retError(clean, Error_outOfBounds(
			0, m->names.length, MeshTriangle_maxMaterials, "OBJ_read() more materials than the triangle word holds"
		));

	CharString copy = CharString_createNull();
	gotoIfError3(clean, CharString_createCopy(name, alloc, &copy, e_rr));

	if(!ListCharString_pushBack(&m->names, copy, alloc, e_rr)) {
		CharString_free(&copy, alloc);
		s_uccess = false;
		goto clean;
	}

	m->current = (U32) (m->names.length - 1);

clean:
	return s_uccess;
}

static void OBJMaterials_free(OBJMaterials *m, const Allocator *alloc) {
	ListCharString_freeUnderlying(&m->names, alloc);
}

//A file index into an array with count entries, resolved to 1 based or refused.
//Negative counts back from the end, which is what a file written incrementally uses.

static Bool OBJ_resolveIndex(I64 raw, U64 count, U32 *result, const C8 *what, Error *e_rr) {

	Bool s_uccess = true;

	if(!raw)
		retError(clean, Error_invalidParameter(0, 0, "OBJ_read() a face index of 0 names nothing"));

	const I64 resolved = raw < 0 ? (I64) count + 1 + raw : raw;

	if(resolved < 1 || (U64) resolved > count)
		retError(clean, Error_outOfBounds(0, (U64) raw, count, what));

	*result = (U32) resolved;

clean:
	return s_uccess;
}

//v, v/vt, v//vn or v/vt/vn. Anything else is a broken corner.

static Bool OBJ_parseCorner(
	CharString token, U64 vCount, U64 vtCount, U64 vnCount, U32 *v, U32 *vt, U32 *vn, Error *e_rr
) {

	Bool s_uccess = true;

	const C8 *s = token.ptr;
	const U64 len = CharString_length(token);

	U64 slash[2] = { len, len };
	U8 slashes = 0;

	for(U64 i = 0; i < len; ++i)
		if(s[i] == '/') {

			if(slashes == 2)
				retError(clean, Error_invalidParameter(0, 1, "OBJ_read() a face corner has more than two slashes"));

			slash[slashes++] = i;
		}

	const CharString vStr = CharString_createRefSizedConst(s, slash[0], false);

	I64 raw = 0;

	if(!Mesh_parseI64(vStr, &raw))
		retError(clean, Error_invalidParameter(0, 2, "OBJ_read() a face corner's position index doesn't parse"));

	gotoIfError3(clean, OBJ_resolveIndex(raw, vCount, v, "OBJ_read() a face names a position that doesn't exist", e_rr));

	*vt = *vn = 0;

	if(slashes >= 1 && slash[1] > slash[0] + 1) {

		const CharString vtStr = CharString_createRefSizedConst(s + slash[0] + 1, slash[1] - slash[0] - 1, false);

		if(!Mesh_parseI64(vtStr, &raw))
			retError(clean, Error_invalidParameter(0, 3, "OBJ_read() a face corner's uv index doesn't parse"));

		gotoIfError3(clean, OBJ_resolveIndex(raw, vtCount, vt, "OBJ_read() a face names a uv that doesn't exist", e_rr));
	}

	if(slashes == 2 && len > slash[1] + 1) {

		const CharString vnStr = CharString_createRefSizedConst(s + slash[1] + 1, len - slash[1] - 1, false);

		if(!Mesh_parseI64(vnStr, &raw))
			retError(clean, Error_invalidParameter(0, 4, "OBJ_read() a face corner's normal index doesn't parse"));

		gotoIfError3(clean, OBJ_resolveIndex(raw, vnCount, vn, "OBJ_read() a face names a normal that doesn't exist", e_rr));
	}

clean:
	return s_uccess;
}

//n floats off a line, refusing fewer than required. Extra tokens are ignored: v takes an optional w and vt an
// optional third.

static Bool OBJ_parseFloats(const C8 *line, U64 len, U64 *pos, F32 *out, U8 n, U8 required, Error *e_rr) {

	Bool s_uccess = true;
	CharString token;

	for(U8 i = 0; i < n; ++i) {

		out[i] = 0;

		if(!Mesh_nextToken(line, len, pos, &token)) {

			if(i < required)
				retError(clean, Error_invalidParameter(0, 5, "OBJ_read() a vertex line has too few components"));

			break;
		}

		if(!Mesh_parseF32(token, out + i))
			retError(clean, Error_invalidParameter(0, 6, "OBJ_read() a vertex component doesn't parse"));
	}

clean:
	return s_uccess;
}

static Bool OBJ_keywordIs(CharString keyword, const C8 *literal) {
	return CharString_equalsCString(&keyword, literal, EStringCase_Sensitive);
}

Bool OBJ_read(
	StreamRef *stream,
	U64 *off,
	EMeshReadFlags flags,
	MeshInfo *info,
	const MeshOutput *output,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	MeshSource src = (MeshSource) { 0 };
	MeshSink positionSink = (MeshSink) { 0 }, attributeSink = (MeshSink) { 0 };
	MeshSink indexSink = (MeshSink) { 0 }, wordSink = (MeshSink) { 0 };
	MeshPositions positions = (MeshPositions) { 0 };
	MeshTriangles triangles = (MeshTriangles) { 0 };
	MeshAttributes attrs = (MeshAttributes) { 0 };
	OBJDedup dedup = (OBJDedup) { 0 };
	OBJMaterials materials = (OBJMaterials) { 0 };

	//The file's own arrays, which a face may name from anywhere and so have to be whole before it is resolved.

	ListF32 v = (ListF32) { 0 }, vt = (ListF32) { 0 }, vn = (ListF32) { 0 };
	ListU32 face = (ListU32) { 0 };

	Buffer lineBuf = Buffer_createNull();

	if(!stream || !off || !info || !output)
		retError(clean, Error_nullPointer(0, "OBJ_read()::stream, off, info and output are required"));

	if(!output->positions || !output->indices)
		retError(clean, Error_nullPointer(4, "OBJ_read()::output->positions and indices are required"));

	*info = (MeshInfo) { 0 };

	gotoIfError3(clean, MeshSource_create(stream, *off, alloc, &src, e_rr));
	gotoIfError3(clean, MeshSink_create(output->positions, output->positionOffset, alloc, &positionSink, e_rr));
	gotoIfError3(clean, MeshSink_create(output->attributes, output->attributeOffset, alloc, &attributeSink, e_rr));
	gotoIfError3(clean, MeshSink_create(output->indices, output->indexOffset, alloc, &indexSink, e_rr));
	gotoIfError3(clean, MeshSink_create(output->triangles, output->triangleOffset, alloc, &wordSink, e_rr));

	MeshPositions_create(flags, &positionSink, &positions);
	MeshTriangles_create(flags, &indexSink, &wordSink, &triangles);
	MeshAttributes_create(flags, &attributeSink, &attrs);

	gotoIfError3(clean, Buffer_createUninitializedBytes(MESH_LINE_CAP, alloc, &lineBuf, e_rr));
	C8 *line = (C8*) lineBuf.ptrNonConst;

	for(;;) {

		U64 len = 0;
		Bool got = false;

		gotoIfError3(clean, MeshSource_readLine(&src, line, MESH_LINE_CAP, &len, &got, e_rr));

		if(!got)
			break;

		U64 pos = 0;
		CharString keyword;

		if(!Mesh_nextToken(line, len, &pos, &keyword) || keyword.ptr[0] == '#')
			continue;

		if(OBJ_keywordIs(keyword, "v")) {

			F32 p[3];
			gotoIfError3(clean, OBJ_parseFloats(line, len, &pos, p, 3, 3, e_rr));

			for(U8 i = 0; i < 3; ++i)
				gotoIfError3(clean, ListF32_pushBack(&v, p[i], alloc, e_rr));
		}

		else if(OBJ_keywordIs(keyword, "vn")) {

			F32 n[3];
			gotoIfError3(clean, OBJ_parseFloats(line, len, &pos, n, 3, 3, e_rr));

			for(U8 i = 0; i < 3; ++i)
				gotoIfError3(clean, ListF32_pushBack(&vn, n[i], alloc, e_rr));
		}

		else if(OBJ_keywordIs(keyword, "vt")) {

			F32 uv[2];
			gotoIfError3(clean, OBJ_parseFloats(line, len, &pos, uv, 2, 1, e_rr));

			for(U8 i = 0; i < 2; ++i)
				gotoIfError3(clean, ListF32_pushBack(&vt, uv[i], alloc, e_rr));
		}

		else if(OBJ_keywordIs(keyword, "usemtl")) {

			CharString name;

			//A usemtl with no name is treated as naming the empty string rather than refused.

			if(!Mesh_nextToken(line, len, &pos, &name))
				name = CharString_createRefSizedConst("", 0, true);

			gotoIfError3(clean, OBJMaterials_use(&materials, name, alloc, e_rr));
		}

		else if(OBJ_keywordIs(keyword, "f")) {

			gotoIfError3(clean, ListU32_clear(&face, e_rr));

			CharString corner;

			while(Mesh_nextToken(line, len, &pos, &corner)) {

				U32 iv, ivt, ivn;

				gotoIfError3(clean, OBJ_parseCorner(
					corner, v.length / 3, vt.length / 2, vn.length / 3, &iv, &ivt, &ivn, e_rr
				));

				U32 vertex = 0;
				Bool isNew = false;

				gotoIfError3(clean, OBJDedup_find(&dedup, iv, ivt, ivn, &vertex, &isNew, alloc, e_rr));

				if(isNew) {

					//Straight from the file's arrays into the sinks: the position as the file gave it, the
					// attributes as named or zero where the corner named none.

					gotoIfError3(clean, MeshPositions_push(&positions, v.ptr + (U64) (iv - 1) * 3, alloc, e_rr));

					const F32 none[3] = { 0, 0, 0 };
					const F32 *n = none, *uv = none;

					if(ivn) {
						n = vn.ptr + (U64) (ivn - 1) * 3;
						info->hasNormals = true;
					}

					if(ivt) {
						uv = vt.ptr + (U64) (ivt - 1) * 2;
						info->hasUvs = true;
					}

					gotoIfError3(clean, MeshAttributes_push(&attrs, n, uv, alloc, e_rr));
				}

				gotoIfError3(clean, ListU32_pushBack(&face, vertex, alloc, e_rr));
			}

			if(face.length < 3)
				retError(clean, Error_invalidParameter(0, 7, "OBJ_read() a face has fewer than three corners"));

			if(face.length > 3)
				++info->fannedFaces;

			if(!materials.names.length)
				materials.usedDefault = true;

			//Fanned from the first corner, which is right for the convex polygons a modeller emits.

			for(U64 k = 1; k + 1 < face.length; ++k) {

				const U32 i0 = face.ptr[0], i1 = face.ptr[k], i2 = face.ptr[k + 1];

				const F32 *p0 = v.ptr + (U64) (dedup.keys.ptr[(U64) i0 * 3] - 1) * 3;
				const F32 *p1 = v.ptr + (U64) (dedup.keys.ptr[(U64) i1 * 3] - 1) * 3;
				const F32 *p2 = v.ptr + (U64) (dedup.keys.ptr[(U64) i2 * 3] - 1) * 3;

				gotoIfError3(clean, MeshTriangles_emit(
					&triangles, i0, i1, i2, materials.current, p0, p1, p2, alloc, e_rr
				));
			}
		}

		//Everything else, groups, objects, smoothing, the material library, lines and points, is not geometry.
	}

	gotoIfError3(clean, MeshPositions_finish(&positions, info, e_rr));
	gotoIfError3(clean, MeshAttributes_finish(&attrs, &triangles, dedup.count, e_rr));

	gotoIfError3(clean, MeshSink_flush(&positionSink, e_rr));
	gotoIfError3(clean, MeshSink_flush(&attributeSink, e_rr));
	gotoIfError3(clean, MeshSink_flush(&indexSink, e_rr));
	gotoIfError3(clean, MeshSink_flush(&wordSink, e_rr));

	info->vertexCount = dedup.count;
	info->indexCount = triangles.count * 3;
	info->materialCount = materials.names.length ? (U32) materials.names.length : 1;

	*off = MeshSource_offset(&src);

clean:

	ListF32_free(&v, alloc);
	ListF32_free(&vt, alloc);
	ListF32_free(&vn, alloc);
	ListU32_free(&face, alloc);
	OBJDedup_free(&dedup, alloc);
	OBJMaterials_free(&materials, alloc);
	MeshAttributes_free(&attrs, alloc);
	MeshTriangles_free(&triangles, alloc);
	MeshPositions_free(&positions, alloc);
	MeshSink_free(&positionSink);
	MeshSink_free(&attributeSink);
	MeshSink_free(&indexSink);
	MeshSink_free(&wordSink);
	MeshSource_free(&src);
	Buffer_free(&lineBuf, alloc);

	return s_uccess;
}
