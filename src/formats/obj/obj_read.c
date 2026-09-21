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

//formats/obj/obj_read.c

#include "mesh_internal.h"
#include "formats/obj/obj_file.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/c8.h"
#include "types/base/string_read.h"
#include "types/base/string_read_helper.h"

//The dedup table. A corner is a position, uv and normal index, 1 based the way the file counts and 0 where the
// corner named none, and the table maps each distinct triple to the output vertex it became.
//
//Open addressing over a power of two, kept below half full, so a lookup is a hash and a short probe rather
// than a chase through a chained structure. The keys sit in their own list indexed by output vertex, which is
// also what a triangle later needs to find its positions by.

typedef struct ObjDedup {
	ListU32 slots;                //Output vertex + 1, or 0 for empty
	ListU32 keys;                 //v, vt, vn per output vertex
	U64 mask;
	U32 count;
} ObjDedup;

static U64 ObjDedup_hash(U32 v, U32 vt, U32 vn) {

	//The tree's own hash rather than a private one. v and vt ride one word so the chain is two steps for three
	// indices, and they cannot collapse into each other the way three equal indices would under an xor.

	U64 h = Buffer_fnv1a64Single(v | ((U64) vt << 32), Buffer_fnv1a64Offset);
	h = Buffer_fnv1a64Single(vn, h);

	//FNV1a avalanches weakly in its LOW bits and the table masks exactly those, so the finalizer stays.

	h ^= h >> 32;
	h *= 0xD6E8FEB86659FD93ull;
	h ^= h >> 32;

	return h;
}

static Bool ObjDedup_grow(ObjDedup *d, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	const U64 capacity = d->slots.length ? d->slots.length * 2 : 4096;
	ListU32 slots = (ListU32) { 0 };

	//resize rather than reserve: the table is indexed straight away and its LENGTH is the capacity, and the
	// empty sentinel is 0, which resize has already written.

	gotoIfError3(clean, ListU32_resize(&slots, capacity, alloc, e_rr));

	const U64 mask = capacity - 1;

	//Every held vertex is rehashed into the wider table from its key.

	for(U32 i = 0; i < d->count; ++i) {

		const U32 *key = d->keys.ptr + (U64) i * 3;
		U64 slot = ObjDedup_hash(key[0], key[1], key[2]) & mask;

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

static Bool ObjDedup_find(
	ObjDedup *d, U32 v, U32 vt, U32 vn, U32 *result, Bool *isNew, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;

	if((U64) d->count * 2 >= d->slots.length)
		gotoIfError3(clean, ObjDedup_grow(d, alloc, e_rr));

	U64 slot = ObjDedup_hash(v, vt, vn) & d->mask;

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
		retError(clean, Error_overflow(0, d->count, U32_MAX, "Obj_read() more distinct vertices than U32 indices"));

	//One append, so a failure halfway cannot leave a partial key behind for the rehash to read.

	const U32 key[3] = { v, vt, vn };
	ListU32 keyRef = (ListU32) { 0 };

	gotoIfError3(clean, ListU32_createRefConst(key, 3, &keyRef, e_rr));
	gotoIfError3(clean, ListU32_pushAll(&d->keys, keyRef, alloc, e_rr));

	d->slots.ptrNonConst[slot] = d->count + 1;
	*result = d->count++;
	*isNew = true;

clean:
	return s_uccess;
}

static void ObjDedup_free(ObjDedup *d, const Allocator *alloc) {
	ListU32_free(&d->slots, alloc);
	ListU32_free(&d->keys, alloc);
}

//The materials a file names, in order of first use, so usemtl can be mapped back to an index a triangle word
// holds. The names themselves go nowhere: what a name means is the .mtl file's business and a consumer's, and this
// reader has neither.

typedef struct ObjMaterials {
	ListCharString names;
	U32 current;                  //What the faces since the last usemtl belong to
	Bool usedDefault;             //A face was emitted before any usemtl
	U8 padding[3];
} ObjMaterials;

static Bool ObjMaterials_use(ObjMaterials *m, CharString name, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	//Faces emitted before the first usemtl were on a material of their own, so it keeps index 0 under an empty
	// name and the first named material takes 1 rather than sharing with them.

	if(!m->names.length && m->usedDefault) {

		//A null string, not a copy of one: copying an empty source allocates nothing and returns exactly this,
		// so the entry is the placeholder itself. It is load bearing, since it is what index 0 names.

		gotoIfError3(clean, ListCharString_pushBack(&m->names, CharString_createNull(), alloc, e_rr));
	}

	for(U64 i = 0; i < m->names.length; ++i)
		if(CharString_equalsStringSensitive(&m->names.ptr[i], &name)) {
			m->current = (U32) i;
			goto clean;
		}

	if(m->names.length >= MeshTriangle_maxMaterials)
		retError(clean, Error_outOfBounds(
			0, m->names.length, MeshTriangle_maxMaterials, "Obj_read() more materials than the triangle word holds"
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

static void ObjMaterials_free(ObjMaterials *m, const Allocator *alloc) {
	ListCharString_freeUnderlying(&m->names, alloc);
}

//A file index into an array with count entries, resolved to 1 based or refused.
//Negative counts back from the end, which is what a file written incrementally uses.

static Bool Obj_resolveIndex(I64 raw, U64 count, U32 *result, const C8 *what, Error *e_rr) {

	Bool s_uccess = true;

	if(!raw)
		retError(clean, Error_invalidParameter(0, 0, "Obj_read() a face index of 0 names nothing"));

	const I64 resolved = raw < 0 ? (I64) count + 1 + raw : raw;

	if(resolved < 1 || (U64) resolved > count)
		retError(clean, Error_outOfBounds(0, (U64) raw, count, what));

	*result = (U32) resolved;

clean:
	return s_uccess;
}

//v, v/vt, v//vn or v/vt/vn. Anything else is a broken corner.

static Bool Obj_parseCorner(
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
				retError(clean, Error_invalidParameter(0, 1, "Obj_read() a face corner has more than two slashes"));

			slash[slashes++] = i;
		}

	const CharString vStr = CharString_createRefSizedConst(s, slash[0], false);

	I64 raw = 0;

	if(!CharString_parseDecSigned(vStr, &raw))
		retError(clean, Error_invalidParameter(0, 2, "Obj_read() a face corner's position index doesn't parse"));

	gotoIfError3(clean, Obj_resolveIndex(raw, vCount, v, "Obj_read() a face names a position that doesn't exist", e_rr));

	*vt = *vn = 0;

	if(slashes >= 1 && slash[1] > slash[0] + 1) {

		const CharString vtStr = CharString_createRefSizedConst(s + slash[0] + 1, slash[1] - slash[0] - 1, false);

		if(!CharString_parseDecSigned(vtStr, &raw))
			retError(clean, Error_invalidParameter(0, 3, "Obj_read() a face corner's uv index doesn't parse"));

		gotoIfError3(clean, Obj_resolveIndex(raw, vtCount, vt, "Obj_read() a face names a uv that doesn't exist", e_rr));
	}

	if(slashes == 2 && len > slash[1] + 1) {

		const CharString vnStr = CharString_createRefSizedConst(s + slash[1] + 1, len - slash[1] - 1, false);

		if(!CharString_parseDecSigned(vnStr, &raw))
			retError(clean, Error_invalidParameter(0, 4, "Obj_read() a face corner's normal index doesn't parse"));

		gotoIfError3(clean, Obj_resolveIndex(raw, vnCount, vn, "Obj_read() a face names a normal that doesn't exist", e_rr));
	}

clean:
	return s_uccess;
}

//n floats off a line, refusing fewer than required. Extra tokens are ignored: v takes an optional w and vt an
// optional third.

static Bool Obj_parseFloats(CharString line, U64 *pos, F32 *out, U8 n, U8 required, Error *e_rr) {

	Bool s_uccess = true;
	CharString token;

	for(U8 i = 0; i < n; ++i) {

		out[i] = 0;

		if(!CharString_nextToken(line, pos, &token)) {

			if(i < required)
				retError(clean, Error_invalidParameter(0, 5, "Obj_read() a vertex line has too few components"));

			break;
		}

		if(!CharString_parseFloat(token, out + i))
			retError(clean, Error_invalidParameter(0, 6, "Obj_read() a vertex component doesn't parse"));
	}

clean:
	return s_uccess;
}

static Bool Obj_keywordIs(CharString keyword, const C8 *literal) {
	return CharString_equalsCStringSensitive(&keyword, literal);
}

Bool Obj_read(
	StreamRef *stream,
	U64 *off,
	EMeshFlags flags,
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
	ObjDedup dedup = (ObjDedup) { 0 };
	ObjMaterials materials = (ObjMaterials) { 0 };

	//The file's own arrays, which a face may name from anywhere and so have to be whole before it is resolved.

	ListF32 v = (ListF32) { 0 }, vt = (ListF32) { 0 }, vn = (ListF32) { 0 };
	ListU32 face = (ListU32) { 0 };

	Buffer lineBuf = Buffer_createNull();

	if(!stream || !off || !info || !output)
		retError(clean, Error_nullPointer(0, "Obj_read()::stream, off, info and output are required"));

	if(!output->positions || !output->indices)
		retError(clean, Error_nullPointer(4, "Obj_read()::output->positions and indices are required"));

	*info = (MeshInfo) { 0 };

	//Cleared by the first vertex that names none, so a file with no vn at all ends up false rather than vacuous.

	info->allNormals = true;
	info->allUvs = true;

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

		const CharString lineStr = CharString_createRefSizedConst(line, len, false);
		U64 pos = 0;
		CharString keyword;

		if(!CharString_nextToken(lineStr, &pos, &keyword) || keyword.ptr[0] == '#')
			continue;

		if(Obj_keywordIs(keyword, "v")) {

			F32 p[3];
			gotoIfError3(clean, Obj_parseFloats(lineStr, &pos, p, 3, 3, e_rr));

			gotoIfError3(clean, Mesh_appendF32(&v, p, 3, alloc, e_rr));
		}

		else if(Obj_keywordIs(keyword, "vn")) {

			F32 n[3];
			gotoIfError3(clean, Obj_parseFloats(lineStr, &pos, n, 3, 3, e_rr));

			gotoIfError3(clean, Mesh_appendF32(&vn, n, 3, alloc, e_rr));
		}

		else if(Obj_keywordIs(keyword, "vt")) {

			F32 uv[2];
			gotoIfError3(clean, Obj_parseFloats(lineStr, &pos, uv, 2, 1, e_rr));

			gotoIfError3(clean, Mesh_appendF32(&vt, uv, 2, alloc, e_rr));
		}

		else if(Obj_keywordIs(keyword, "usemtl")) {

			CharString name;

			//A usemtl with no name is treated as naming the empty string rather than refused.

			if(!CharString_nextToken(lineStr, &pos, &name))
				name = CharString_createRefSizedConst("", 0, true);

			gotoIfError3(clean, ObjMaterials_use(&materials, name, alloc, e_rr));
		}

		else if(Obj_keywordIs(keyword, "f")) {

			gotoIfError3(clean, ListU32_clear(&face, e_rr));

			CharString corner;

			while(CharString_nextToken(lineStr, &pos, &corner)) {

				U32 iv, ivt, ivn;

				gotoIfError3(clean, Obj_parseCorner(
					corner, v.length / 3, vt.length / 2, vn.length / 3, &iv, &ivt, &ivn, e_rr
				));

				U32 vertex = 0;
				Bool isNew = false;

				gotoIfError3(clean, ObjDedup_find(&dedup, iv, ivt, ivn, &vertex, &isNew, alloc, e_rr));

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

					else info->allNormals = false;

					if(ivt) {
						uv = vt.ptr + (U64) (ivt - 1) * 2;
						info->hasUvs = true;
					}

					else info->allUvs = false;

					gotoIfError3(clean, MeshAttributes_push(&attrs, n, uv, alloc, e_rr));
				}

				gotoIfError3(clean, ListU32_pushBack(&face, vertex, alloc, e_rr));
			}

			if(face.length < 3)
				retError(clean, Error_invalidParameter(0, 7, "Obj_read() a face has fewer than three corners"));

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

	//A computed normal replaces every one the file named, so what the file covered stops deciding this and the
	// only placeholders left are the vertices no triangle with area touched.

	if(flags & EMeshFlags_ComputeNormals)
		info->allNormals = !attrs.placeholderNormal;

	info->maxUvError = attrs.maxUvError;

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
	ObjDedup_free(&dedup, alloc);
	ObjMaterials_free(&materials, alloc);
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
