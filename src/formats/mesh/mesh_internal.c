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

//formats/mesh/mesh_internal.c

#include "mesh_internal.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/container/string.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/c8.h"
#include "types/base/mathf.h"
#include "types/base/string_read.h"
#include "types/math/vec4f.h"
#include "types/math/vec4f_swizzle.h"
#include "types/math/pack.h"

//---------------------------------------------------------------- Source

Bool MeshSource_create(StreamRef *stream, U64 off, const Allocator *alloc, MeshSource *src, Error *e_rr) {

	Bool s_uccess = true;

	if(!stream || !src)
		retError(clean, Error_nullPointer(0, "MeshSource_create()::stream and src are required"));

	*src = (MeshSource) { .stream = RefPtr_data(stream, OxStream), .alloc = alloc, .base = off };

	if(!src->stream->read)
		retError(clean, Error_unsupportedOperation(0, "MeshSource_create()::stream is not readable"));

	if(off > src->stream->size)
		retError(clean, Error_outOfBounds(1, off, src->stream->size, "MeshSource_create()::off is past the stream"));

	Buffer window = Buffer_createNull();
	gotoIfError3(clean, Buffer_createUninitializedBytes(MESH_WINDOW, alloc, &window, e_rr));
	src->buf = window.ptrNonConst;

clean:
	return s_uccess;
}

void MeshSource_free(MeshSource *src) {

	if(!src || !src->buf)
		return;

	Buffer window = Buffer_createManagedPtr(src->buf, MESH_WINDOW);
	Buffer_free(&window, src->alloc);
	*src = (MeshSource) { 0 };
}

Bool MeshSource_available(MeshSource *src, U64 *available, Error *e_rr) {

	Bool s_uccess = true;

	if(src->pos >= src->fill) {

		src->base += src->fill;
		src->pos = 0;
		src->fill = 0;

		if(src->base < src->stream->size) {

			const U64 remaining = src->stream->size - src->base;
			src->fill = remaining < MESH_WINDOW ? remaining : MESH_WINDOW;

			gotoIfError3(clean, src->stream->read(
				src->stream, src->base, src->fill, Buffer_createRef(src->buf, src->fill), src->alloc, e_rr
			));
		}
	}

	*available = src->fill - src->pos;

clean:
	return s_uccess;
}

Bool MeshSource_readBytes(MeshSource *src, void *dst, U64 n, Error *e_rr) {

	Bool s_uccess = true;
	U8 *out = (U8*) dst;

	while(n) {

		U64 available = 0;
		gotoIfError3(clean, MeshSource_available(src, &available, e_rr));

		if(!available)
			retError(clean, Error_outOfBounds(
				0, MeshSource_offset(src), src->stream->size, "MeshSource_readBytes() ran past the end of the stream"
			));

		const U64 take = available < n ? available : n;

		if(out) {
			Buffer_memcpy(Buffer_createRef(out, take), Buffer_createRefConst(MeshSource_ptr(src), take));
			out += take;
		}

		src->pos += take;
		n -= take;
	}

clean:
	return s_uccess;
}

Bool MeshSource_skip(MeshSource *src, U64 n, Error *e_rr) {
	return MeshSource_readBytes(src, NULL, n, e_rr);
}

Bool MeshSource_readLine(MeshSource *src, C8 *line, U64 cap, U64 *len, Bool *got, Error *e_rr) {

	Bool s_uccess = true;
	U64 n = 0;

	*got = false;

	for(;;) {

		U64 available = 0;
		gotoIfError3(clean, MeshSource_available(src, &available, e_rr));

		if(!available)
			break;

		//Scan the window for the terminator rather than pulling a byte at a time through a call.

		const U8 *p = MeshSource_ptr(src);
		U64 i = 0;

		while(i < available && p[i] != '\n')
			++i;

		if(n + i >= cap)
			retError(clean, Error_outOfBounds(0, n + i, cap, "MeshSource_readLine() line is too long"));

		Buffer_memcpy(Buffer_createRef(line + n, i), Buffer_createRefConst(p, i));
		n += i;
		*got = true;

		if(i < available) {                 //The terminator itself
			src->pos += i + 1;
			break;
		}

		src->pos += i;
	}

	if(n && line[n - 1] == '\r')
		--n;

	line[n] = '\0';
	*len = n;

clean:
	return s_uccess;
}

//---------------------------------------------------------------- Sink

Bool MeshSink_create(StreamRef *stream, U64 off, const Allocator *alloc, MeshSink *sink, Error *e_rr) {

	Bool s_uccess = true;

	*sink = (MeshSink) { .alloc = alloc, .base = off };

	if(!stream)
		goto clean;

	OxStream *s = RefPtr_data(stream, OxStream);

	if(!s->write)
		retError(clean, Error_unsupportedOperation(0, "MeshSink_create()::stream is not writable"));

	Buffer chunk = Buffer_createNull();
	gotoIfError3(clean, Buffer_createUninitializedBytes(MESH_CHUNK, alloc, &chunk, e_rr));

	sink->stream = s;
	sink->buf = chunk.ptrNonConst;

clean:
	return s_uccess;
}

//Capacity doubles ahead of the writes, so a sink that ends at N bytes was reallocated log2(N / chunk) times
// rather than once per chunk. Streams without reserve, a file for one, are left to grow themselves.

static Bool MeshSink_reserveFor(MeshSink *sink, U64 bytes, Error *e_rr) {

	Bool s_uccess = true;

	const U64 needed = sink->written + bytes;

	if(!sink->stream->reserve || needed <= sink->reserved)
		goto clean;

	U64 next = sink->reserved ? sink->reserved : MESH_CHUNK;

	while(next < needed)
		next *= 2;

	gotoIfError3(clean, sink->stream->reserve(sink->stream, sink->base + next, sink->alloc, e_rr));
	sink->reserved = next;

clean:
	return s_uccess;
}

static Bool MeshSink_push(MeshSink *sink, const void *data, U64 bytes, Error *e_rr) {

	Bool s_uccess = true;

	gotoIfError3(clean, MeshSink_reserveFor(sink, bytes, e_rr));

	gotoIfError3(clean, sink->stream->write(
		sink->stream, sink->base + sink->written, bytes, Buffer_createRefConst(data, bytes), sink->alloc, e_rr
	));

	sink->written += bytes;

clean:
	return s_uccess;
}

Bool MeshSink_flush(MeshSink *sink, Error *e_rr) {

	Bool s_uccess = true;

	if(!sink->stream || !sink->fill)
		goto clean;

	gotoIfError3(clean, MeshSink_push(sink, sink->buf, sink->fill, e_rr));
	sink->fill = 0;

clean:
	return s_uccess;
}

Bool MeshSink_write(MeshSink *sink, const void *data, U64 bytes, Error *e_rr) {

	Bool s_uccess = true;

	if(!sink->stream)
		goto clean;

	//A write bigger than the chunk goes straight through rather than being cut into chunk sized pieces.

	if(bytes >= MESH_CHUNK) {
		gotoIfError3(clean, MeshSink_flush(sink, e_rr));
		gotoIfError3(clean, MeshSink_push(sink, data, bytes, e_rr));
		goto clean;
	}

	if(sink->fill + bytes > MESH_CHUNK)
		gotoIfError3(clean, MeshSink_flush(sink, e_rr));

	Buffer_memcpy(Buffer_createRef(sink->buf + sink->fill, bytes), Buffer_createRefConst(data, bytes));
	sink->fill += bytes;

clean:
	return s_uccess;
}

void MeshSink_free(MeshSink *sink) {

	if(!sink || !sink->buf)
		return;

	Buffer chunk = Buffer_createManagedPtr(sink->buf, MESH_CHUNK);
	Buffer_free(&chunk, sink->alloc);
	*sink = (MeshSink) { 0 };
}

//A list that doubles ahead of a push, since the lists here grow once per vertex and a reserve that fit exactly
// would copy the list once per vertex too.

static Bool Mesh_growF32(ListF32 *list, U64 *capacity, U64 needed, U64 initial, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(needed <= *capacity)
		goto clean;

	U64 next = *capacity ? *capacity : initial;

	while(next < needed)
		next *= 2;

	gotoIfError3(clean, ListF32_reserve(list, next, alloc, e_rr));
	*capacity = next;

clean:
	return s_uccess;
}

//Divided by the real length rather than F32x4_normalize3, which is the approximate rsqrt on SSE and turns an axis
// into 0.9998 of one. A normal written to a file is compared against by whatever reads it, so it has to be exact.

static F32x4 Mesh_normalize3(F32x4 v) {
	return F32x4_div(v, F32x4_xxxx4(F32x4_len3(v)));
}

//---------------------------------------------------------------- Positions

void MeshPositions_create(EMeshReadFlags flags, MeshSink *sink, MeshPositions *positions) {

	*positions = (MeshPositions) {
		.sink = sink,
		.hold = !!(flags & EMeshReadFlags_QuantizePositions),
		.aabbMin = { F32_MAX, F32_MAX, F32_MAX },
		.aabbMax = { -F32_MAX, -F32_MAX, -F32_MAX }
	};
}

Bool MeshPositions_push(MeshPositions *p, const F32 *position, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	for(U8 i = 0; i < 3; ++i) {
		p->aabbMin[i] = position[i] < p->aabbMin[i] ? position[i] : p->aabbMin[i];
		p->aabbMax[i] = position[i] > p->aabbMax[i] ? position[i] : p->aabbMax[i];
	}

	++p->count;

	if(!p->hold) {
		gotoIfError3(clean, MeshSink_write(p->sink, position, 3 * sizeof(F32), e_rr));
		goto clean;
	}

	gotoIfError3(clean, Mesh_growF32(&p->held, &p->heldCapacity, p->held.length + 3, 3 * 1024, alloc, e_rr));

	for(U8 i = 0; i < 3; ++i)
		gotoIfError3(clean, ListF32_pushBack(&p->held, position[i], alloc, e_rr));

clean:
	return s_uccess;
}

Bool MeshPositions_finish(MeshPositions *p, MeshInfo *info, Error *e_rr) {

	Bool s_uccess = true;

	//No positions at all leaves the bounds at a point rather than at the sentinels.

	for(U8 i = 0; i < 3; ++i) {
		info->aabbMin[i] = p->count ? p->aabbMin[i] : 0;
		info->aabbMax[i] = p->count ? p->aabbMax[i] : 0;
	}

	if(!p->hold)
		goto clean;

	//center + s / 32767 * halfExtent per axis, so -1 and 1 are the bounds exactly.
	//A flat axis has no extent to divide by and quantizes to the center, which is the only value it holds.

	F32 center[3], inv[3];

	for(U8 i = 0; i < 3; ++i) {
		center[i] = (info->aabbMin[i] + info->aabbMax[i]) * 0.5f;
		const F32 half = (info->aabbMax[i] - info->aabbMin[i]) * 0.5f;
		inv[i] = half > 0 ? 32767.f / half : 0;
	}

	for(U32 v = 0; v < p->count; ++v) {

		const F32 *pos = p->held.ptr + (U64) v * 3;
		I16 q[4] = { 0, 0, 0, 0 };

		//Rounded half away from zero on both sides, so the bounds land on -32767 and 32767 and not one step past.

		for(U8 i = 0; i < 3; ++i) {
			const F32 s = F32_clamp((pos[i] - center[i]) * inv[i], -32767, 32767);
			q[i] = (I16) (s >= 0 ? F32_floor(s + 0.5f) : -F32_floor(-s + 0.5f));
		}

		gotoIfError3(clean, MeshSink_write(p->sink, q, sizeof(q), e_rr));
	}

clean:
	return s_uccess;
}

void MeshPositions_free(MeshPositions *p, const Allocator *alloc) {
	ListF32_free(&p->held, alloc);
}

//---------------------------------------------------------------- Triangles

void MeshTriangles_create(EMeshReadFlags flags, MeshSink *indices, MeshSink *words, MeshTriangles *triangles) {
	*triangles = (MeshTriangles) {
		.indices = indices,
		.words = words,
		.computeNormals = !!(flags & EMeshReadFlags_ComputeNormals)
	};
}

static Bool MeshTriangles_growSums(MeshTriangles *t, U32 vertexCount, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	const U64 needed = (U64) vertexCount * 3;
	const U64 had = t->normalSums.length;

	if(needed <= had)
		goto clean;

	//Doubled rather than grown to fit, since this is called per triangle.

	U64 capacity = had ? had : 3 * 1024;

	while(capacity < needed)
		capacity *= 2;

	gotoIfError3(clean, ListF32_reserve(&t->normalSums, capacity, alloc, e_rr));
	gotoIfError3(clean, ListF32_resize(&t->normalSums, needed, alloc, e_rr));

	for(U64 i = had; i < needed; ++i)
		t->normalSums.ptrNonConst[i] = 0;

clean:
	return s_uccess;
}

Bool MeshTriangles_emit(
	MeshTriangles *t,
	U32 i0, U32 i1, U32 i2,
	U32 material,
	const F32 *p0, const F32 *p1, const F32 *p2,
	const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;

	if(material >= MeshTriangle_maxMaterials)
		retError(clean, Error_outOfBounds(
			3, material, MeshTriangle_maxMaterials, "MeshTriangles_emit() more materials than the triangle word holds"
		));

	const U32 tri[3] = { i0, i1, i2 };
	gotoIfError3(clean, MeshSink_write(t->indices, tri, sizeof(tri), e_rr));

	if(MeshTriangles_needsPositions(t)) {

		//Unnormalized, so its length is twice the area: exactly the weight a smooth normal wants, and a sliver
		// of a triangle contributes as little as it deserves to.

		const F32x4 a = F32x4_create3(p0[0], p0[1], p0[2]);
		const F32x4 e1 = F32x4_sub(F32x4_create3(p1[0], p1[1], p1[2]), a);
		const F32x4 e2 = F32x4_sub(F32x4_create3(p2[0], p2[1], p2[2]), a);
		const F32x4 n = F32x4_cross3(e1, e2);
		const F32 len2 = F32x4_sqLen3(n);

		if(MeshSink_active(t->words)) {

			//A degenerate triangle has no side to be on. Its normal packs to the +z encoding of a zero vector,
			// which is what unpacking a zero word yields, and the material still rides above it.

			const U32 oct = len2 > 0 ? U32_packOct18(Mesh_normalize3(n)) : U32_packOct18(F32x4_create3(0, 0, 1));
			const U32 word = MeshTriangle_pack(oct, material);

			gotoIfError3(clean, MeshSink_write(t->words, &word, sizeof(word), e_rr));
		}

		if(t->computeNormals && len2 > 0) {

			U32 highest = i0 > i1 ? i0 : i1;
			highest = highest > i2 ? highest : i2;

			gotoIfError3(clean, MeshTriangles_growSums(t, highest + 1, alloc, e_rr));

			for(U8 c = 0; c < 3; ++c) {

				F32 *sum = t->normalSums.ptrNonConst + (U64) tri[c] * 3;

				sum[0] += F32x4_x(n);
				sum[1] += F32x4_y(n);
				sum[2] += F32x4_z(n);
			}
		}
	}

	++t->count;

clean:
	return s_uccess;
}

void MeshTriangles_free(MeshTriangles *t, const Allocator *alloc) {
	ListF32_free(&t->normalSums, alloc);
}

//---------------------------------------------------------------- Attributes

void MeshAttributes_create(EMeshReadFlags flags, MeshSink *sink, MeshAttributes *attributes) {
	*attributes = (MeshAttributes) {
		.sink = sink,
		.hold = !!(flags & EMeshReadFlags_ComputeNormals),
		.wide = !!(flags & EMeshReadFlags_WideUvs)
	};
}

//Either record shape, from the same normal and uv.

static Bool MeshAttributes_writeRecord(MeshAttributes *a, U32 normal, const F32 *uv, Error *e_rr) {

	if(a->wide) {
		const MeshAttributeWide attr = { .normal = normal, .uv = { uv[0], uv[1] } };
		return MeshSink_write(a->sink, &attr, sizeof(attr), e_rr);
	}

	const MeshAttribute attr = { .normal = normal, .uv = U32_packF16x2(uv[0], uv[1]) };
	return MeshSink_write(a->sink, &attr, sizeof(attr), e_rr);
}

//A normal the file did not supply packs as +z rather than as the zero vector, which has no octahedral encoding.

static U32 Mesh_packNormal(const F32 *n) {

	const F32x4 v = F32x4_create3(n[0], n[1], n[2]);

	if(F32x4_sqLen3(v) <= 0)
		return U32_packOct32(F32x4_create3(0, 0, 1));

	return U32_packOct32(Mesh_normalize3(v));
}

Bool MeshAttributes_push(MeshAttributes *a, const F32 *normal, const F32 *uv, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!MeshSink_active(a->sink))
		goto clean;

	if(!a->hold) {
		gotoIfError3(clean, MeshAttributes_writeRecord(a, Mesh_packNormal(normal), uv, e_rr));
		goto clean;
	}

	//Held as five floats a vertex, the uv and room for the normal the finish fills in.

	const F32 held[5] = { 0, 0, 0, uv[0], uv[1] };

	gotoIfError3(clean, Mesh_growF32(&a->held, &a->heldCapacity, a->held.length + 5, 5 * 1024, alloc, e_rr));

	for(U8 i = 0; i < 5; ++i)
		gotoIfError3(clean, ListF32_pushBack(&a->held, held[i], alloc, e_rr));

clean:
	return s_uccess;
}

Bool MeshAttributes_finish(MeshAttributes *a, const MeshTriangles *t, U32 vertexCount, Error *e_rr) {

	Bool s_uccess = true;

	if(!MeshSink_active(a->sink) || !a->hold)
		goto clean;

	if(a->held.length != (U64) vertexCount * 5)
		retError(clean, Error_invalidState(0, "MeshAttributes_finish() held attributes don't match the vertex count"));

	for(U32 i = 0; i < vertexCount; ++i) {

		const F32 *held = a->held.ptr + (U64) i * 5;

		//A vertex no triangle touched, or only degenerate ones, has no normal to speak of and gets the +z a
		// missing one packs as.

		F32 n[3] = { 0, 0, 0 };

		if((U64) i * 3 + 3 <= t->normalSums.length) {
			const F32 *sum = t->normalSums.ptr + (U64) i * 3;
			n[0] = sum[0]; n[1] = sum[1]; n[2] = sum[2];
		}

		gotoIfError3(clean, MeshAttributes_writeRecord(a, Mesh_packNormal(n), held + 3, e_rr));
	}

clean:
	return s_uccess;
}

void MeshAttributes_free(MeshAttributes *a, const Allocator *alloc) {
	ListF32_free(&a->held, alloc);
}

//---------------------------------------------------------------- Text

Bool Mesh_nextToken(const C8 *line, U64 len, U64 *pos, CharString *token) {

	U64 i = *pos;

	while(i < len && C8_isWhitespace(line[i]))
		++i;

	if(i >= len)
		return false;

	const U64 start = i;

	while(i < len && !C8_isWhitespace(line[i]))
		++i;

	*token = CharString_createRefSizedConst(line + start, i - start, false);
	*pos = i;
	return true;
}

Bool Mesh_parseF32(CharString token, F32 *result) {
	return CharString_parseFloat(token, result);
}

Bool Mesh_parseI64(CharString token, I64 *result) {
	return CharString_parseDecSigned(token, result);
}
