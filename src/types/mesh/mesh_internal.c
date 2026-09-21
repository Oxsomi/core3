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

//types/mesh/mesh_internal.c

#include "mesh_internal.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/container/string.h"
#include "types/math/vec4f.h"
#include "types/math/vec4i.h"
#include "types/math/vec4f_swizzle.h"
#include "types/math/pack.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/c8.h"
#include "types/base/mathf.h"
#include "types/base/mathi.h"
#include "types/base/string_read.h"

#include <stdarg.h>

//---------------------------------------------------------------- Source

Bool MeshSource_create(StreamRef *stream, U64 off, const Allocator *alloc, MeshSource *src, Error *e_rr) {

	Bool s_uccess = true;

	if(!stream || !src)
		retError(clean, Error_nullPointer(0, "MeshSource_create()::stream and src are required"));

	*src = (MeshSource) { .alloc = alloc, .pos = off };

	const OxStream *s = RefPtr_data(stream, OxStream);

	if(!s->read)
		retError(clean, Error_unsupportedOperation(0, "MeshSource_create()::stream is not readable"));

	if(off > s->size)
		retError(clean, Error_outOfBounds(1, off, s->size, "MeshSource_create()::off is past the stream"));

	src->size = s->size;
	gotoIfError3(clean, StreamCursor_create(stream, MESH_WINDOW, false, alloc, &src->cursor, e_rr));

clean:
	return s_uccess;
}

void MeshSource_free(MeshSource *src) {

	if(!src || !src->cursor.stream)
		return;

	StreamCursor_close(&src->cursor, src->alloc);
	*src = (MeshSource) { 0 };
}

Bool MeshSource_available(MeshSource *src, U64 *available, Error *e_rr) {

	Bool s_uccess = true;

	if(!src || !available)
		retError(clean, Error_nullPointer(0, "MeshSource_available()::src and available are required"));

	if(src->pos >= src->size) {
		*available = 0;
		goto clean;
	}

	const U64 window = Buffer_length(src->cursor.cacheData);
	const U64 start = src->cursor.lastLocation;

	//Outside the window, so it moves to the cursor: an empty destination is the load only form of a read

	if(start == U64_MAX || src->pos < start || src->pos >= start + window)
		gotoIfError3(clean, StreamCursor_read(
			&src->cursor, Buffer_createNull(), src->pos, 0, U64_min(window, src->size - src->pos), false,
			src->alloc, e_rr
		));

	*available = U64_min(src->cursor.lastLocation + window, src->size) - src->pos;

clean:
	return s_uccess;
}

Bool MeshSource_readBytes(MeshSource *src, void *dst, U64 n, Error *e_rr) {

	Bool s_uccess = true;

	if(!src)
		retError(clean, Error_nullPointer(0, "MeshSource_readBytes()::src is required"));

	if(!n)
		goto clean;

	if(n > src->size - src->pos)
		retError(clean, Error_outOfBounds(
			0, src->pos + n, src->size, "MeshSource_readBytes() ran past the end of the stream"
		));

	//The cursor copies what its window holds, reads the rest straight through and moves the window to the tail

	if(dst)
		gotoIfError3(clean, StreamCursor_read(
			&src->cursor, Buffer_createRef(dst, n), src->pos, 0, n, false, src->alloc, e_rr
		));

	src->pos += n;

clean:
	return s_uccess;
}

//A skip never touches the stream: only the offset moves

Bool MeshSource_skip(MeshSource *src, U64 n, Error *e_rr) {
	return MeshSource_readBytes(src, NULL, n, e_rr);
}

Bool MeshSource_readLine(MeshSource *src, C8 *line, U64 cap, U64 *len, Bool *got, Error *e_rr) {

	Bool s_uccess = true;
	U64 n = 0;

	if(!src || !line || !len || !got || !cap)
		retError(clean, Error_nullPointer(0, "MeshSource_readLine()::src, line, cap, len and got are required"));

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

	if(!sink)
		retError(clean, Error_nullPointer(3, "MeshSink_create()::sink is required"));

	*sink = (MeshSink) { .alloc = alloc, .base = off };

	if(!stream)
		goto clean;

	if(!RefPtr_data(stream, OxStream)->write)
		retError(clean, Error_unsupportedOperation(0, "MeshSink_create()::stream is not writable"));

	gotoIfError3(clean, StreamCursor_create(stream, MESH_CHUNK, true, alloc, &sink->cursor, e_rr));

clean:
	return s_uccess;
}

//Capacity doubles ahead of the writes, so a sink that ends at N bytes was reallocated log2(N / chunk) times
// rather than once per chunk. Streams without reserve, a file for one, are left to grow themselves.

static Bool MeshSink_reserveFor(MeshSink *sink, U64 bytes, Error *e_rr) {

	Bool s_uccess = true;

	OxStream *stream = RefPtr_data(sink->cursor.stream, OxStream);
	const U64 needed = sink->written + bytes;

	if(!stream->reserve || needed <= sink->reserved)
		goto clean;

	U64 next = sink->reserved ? sink->reserved : MESH_CHUNK;

	while(next < needed)
		next *= 2;

	gotoIfError3(clean, stream->reserve(stream, sink->base + next, sink->alloc, e_rr));
	sink->reserved = next;

clean:
	return s_uccess;
}

//The one path to the cursor. Everything else stages into the block and comes through here.

static Bool MeshSink_passThrough(MeshSink *sink, const void *data, U64 bytes, Error *e_rr) {

	Bool s_uccess = true;

	if(!bytes)
		goto clean;

	gotoIfError3(clean, MeshSink_reserveFor(sink, bytes, e_rr));

	//The cursor fills its chunk, writes a chunk through once it is full and passes anything larger than a chunk
	// straight on, which is all the chunking there is

	gotoIfError3(clean, StreamCursor_write(
		&sink->cursor, Buffer_createRefConst(data, bytes), 0, sink->base + sink->written, bytes, false,
		sink->alloc, e_rr
	));

	sink->written += bytes;

clean:
	return s_uccess;
}

Bool MeshSink_drain(MeshSink *sink, Error *e_rr) {

	Bool s_uccess = true;

	if(!MeshSink_active(sink) || !sink->filled)
		goto clean;

	gotoIfError3(clean, MeshSink_passThrough(sink, sink->block, sink->filled, e_rr));
	sink->filled = 0;

clean:
	return s_uccess;
}

Bool MeshSink_write(MeshSink *sink, const void *data, U64 bytes, Error *e_rr) {

	Bool s_uccess = true;

	if(!MeshSink_active(sink) || !bytes)
		goto clean;

	if(!data)
		retError(clean, Error_nullPointer(1, "MeshSink_write()::data is required"));

	if(bytes >= MESH_SINK_BLOCK) {
		gotoIfError3(clean, MeshSink_drain(sink, e_rr));
		gotoIfError3(clean, MeshSink_passThrough(sink, data, bytes, e_rr));
		goto clean;
	}

	if(sink->filled + bytes > MESH_SINK_BLOCK)
		gotoIfError3(clean, MeshSink_drain(sink, e_rr));

	Buffer_memcpy(
		Buffer_createRef(sink->block + sink->filled, bytes), Buffer_createRefConst(data, bytes)
	);

	sink->filled += (U32) bytes;

clean:
	return s_uccess;
}

Bool MeshSink_writeFormatted(MeshSink *sink, Error *e_rr, const C8 *format, ...) {

	Bool s_uccess = true;

	const Allocator *alloc = sink ? sink->alloc : NULL;
	CharString line = CharString_createNull();

	if(!MeshSink_active(sink))
		goto clean;

	va_list args;
	va_start(args, format);
	const Bool formatted = CharString_formatVariadic(alloc, &line, e_rr, format, args);
	va_end(args);

	if(!formatted)
		retError(clean, Error_invalidState(0, "MeshSink_writeFormatted() couldn't format"));

	gotoIfError3(clean, MeshSink_write(sink, line.ptr, CharString_length(line), e_rr));

clean:
	CharString_free(&line, alloc);
	return s_uccess;
}

Bool MeshSink_flush(MeshSink *sink, Error *e_rr) {

	Bool s_uccess = true;

	if(!MeshSink_active(sink))
		goto clean;

	gotoIfError3(clean, MeshSink_drain(sink, e_rr));
	gotoIfError3(clean, StreamCursor_flush(&sink->cursor, sink->alloc, e_rr));

clean:
	return s_uccess;
}

void MeshSink_free(MeshSink *sink) {

	if(!MeshSink_active(sink))
		return;

	StreamCursor_close(&sink->cursor, sink->alloc);
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

Bool Mesh_appendF32(ListF32 *list, const F32 *src, U8 n, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!list || !src)
		retError(clean, Error_nullPointer(0, "Mesh_appendF32()::list and src are required"));

	const U64 at = list->length;

	gotoIfError3(clean, ListF32_resize(list, at + n, alloc, e_rr));

	Buffer_memcpy(
		Buffer_createRef(list->ptrNonConst + at, (U64) n * sizeof(F32)),
		Buffer_createRefConst(src, (U64) n * sizeof(F32))
	);

clean:
	return s_uccess;
}

//---------------------------------------------------------------- Positions

void MeshPositions_create(EMeshFlags flags, MeshSink *sink, MeshPositions *positions) {
	*positions = (MeshPositions) {
		.sink = sink,
		.hold = !!(flags & EMeshFlags_QuantizePositions),
		.aabbMin = F32x4_xxxx4(F32_MAX),
		.aabbMax = F32x4_xxxx4(-F32_MAX)
	};
}

Bool MeshPositions_push(MeshPositions *p, const F32 *position, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	const F32x4 pos = F32x4_load3(position);

	++p->count;
	p->aabbMin = F32x4_min(p->aabbMin, pos);
	p->aabbMax = F32x4_max(p->aabbMax, pos);

	if(!p->hold) {
		gotoIfError3(clean, MeshSink_write(p->sink, position, 3 * sizeof(F32), e_rr));
		goto clean;
	}

	gotoIfError3(clean, Mesh_growF32(&p->held, &p->heldCapacity, p->held.length + 3, 3 * 1024, alloc, e_rr));
	gotoIfError3(clean, Mesh_appendF32(&p->held, position, 3, alloc, e_rr));

clean:
	return s_uccess;
}

Bool MeshPositions_finish(MeshPositions *p, MeshInfo *info, Error *e_rr) {

	Bool s_uccess = true;

	//No positions at all leaves the bounds at a point rather than at the sentinels.

	const F32x4 mi = p->count ? p->aabbMin : F32x4_zero();
	const F32x4 ma = p->count ? p->aabbMax : F32x4_zero();

	F32x4_store3(info->aabbMin, mi);
	F32x4_store3(info->aabbMax, ma);

	if(!p->hold)
		goto clean;

	//center + s / 32767 * halfExtent per axis, so -1 and 1 are the bounds exactly.
	//A flat axis has no extent to divide by and quantizes to the center, which is the only value it holds: the
	// mask zeroes its scale, and adding the mask's complement to the divisor keeps that divide finite without an
	// epsilon that would have to be smaller than any real extent.

	//The midpoint is reached from the low corner rather than as half of the sum, which would overflow to an
	// infinite centre on bounds past half of F32_MAX and quantize every vertex to one corner.

	const F32x4 half = F32x4_mul(F32x4_sub(ma, mi), F32x4_xxxx4(0.5f));
	const F32x4 center = F32x4_add(mi, half);
	const F32x4 hasExtent = F32x4_gt(half, F32x4_zero());
	const F32x4 divisor = F32x4_add(half, F32x4_sub(F32x4_one(), hasExtent));
	const F32x4 limit = F32x4_xxxx4(32767);
	const F32x4 inv = F32x4_mul(hasExtent, F32x4_div(limit, divisor));

	for(U32 v = 0; v < p->count; ++v) {

		const F32 *pos = p->held.ptr + (U64) v * 3;

		const F32x4 s = F32x4_clamp(
			F32x4_mul(F32x4_sub(F32x4_load3(pos), center), inv), F32x4_negate(limit), limit
		);

		//Rounded half away from zero on both sides, so the bounds land on -32767 and 32767 and not one step past.
		//Integral by then, so the truncating conversion is exact.

		const F32x4 rounded = F32x4_mul(F32x4_sign(s), F32x4_floor(F32x4_add(F32x4_abs(s), F32x4_xxxx4(0.5f))));
		const I32x4 q = I32x4_fromF32x4(rounded);
		const I16 out[4] = { (I16) I32x4_x(q), (I16) I32x4_y(q), (I16) I32x4_z(q), 0 };

		gotoIfError3(clean, MeshSink_write(p->sink, out, sizeof(out), e_rr));
	}

clean:
	return s_uccess;
}

void MeshPositions_free(MeshPositions *p, const Allocator *alloc) {
	ListF32_free(&p->held, alloc);
}

//---------------------------------------------------------------- Triangles

void MeshTriangles_create(EMeshFlags flags, MeshSink *indices, MeshSink *words, MeshTriangles *triangles) {
	*triangles = (MeshTriangles) {
		.indices = indices,
		.words = words,
		.computeNormals = !!(flags & EMeshFlags_ComputeNormals),
		.narrowIndices = !!(flags & EMeshFlags_NarrowIndices)
	};
}

static Bool MeshTriangles_growSums(MeshTriangles *t, U32 vertexCount, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	const U64 needed = (U64) vertexCount * 3;

	if(needed <= t->normalSums.length)
		goto clean;

	//A resize is the whole of it, since the list grows its capacity geometrically and zeroes what it adds.
	//Reserving a doubled LENGTH first is what must not happen here: the length is the highest vertex index any
	// triangle has named so far and it grows a few elements at a time, so the doubled figure lands just above
	// what is already allocated on almost every triangle, and each of those asks copies the whole array.

	gotoIfError3(clean, ListF32_resize(&t->normalSums, needed, alloc, e_rr));

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

	//Refused at the index that does not fit rather than widened, so what a caller asked for is what it gets.

	if(t->narrowIndices) {

		const U32 highest = U32_max(U32_max(i0, i1), i2);

		if(highest > U16_MAX)
			retError(clean, Error_outOfBounds(
				0, highest, U16_MAX, "MeshTriangles_emit() index past U16 under EMeshFlags_NarrowIndices"
			));

		const U16 narrow[3] = { (U16) i0, (U16) i1, (U16) i2 };
		gotoIfError3(clean, MeshSink_write(t->indices, narrow, sizeof(narrow), e_rr));
	}

	else gotoIfError3(clean, MeshSink_write(t->indices, tri, sizeof(tri), e_rr));

	if(MeshTriangles_needsPositions(t)) {

		//Unnormalized, so its length is twice the area: exactly the weight a smooth normal wants, and a sliver
		// of a triangle contributes as little as it deserves to.

		const F32x4 a = F32x4_load3(p0);
		const F32x4 e1 = F32x4_sub(F32x4_load3(p1), a);
		const F32x4 e2 = F32x4_sub(F32x4_load3(p2), a);
		const F32x4 n = F32x4_cross3(e1, e2);
		const F32 len2 = F32x4_sqLen3(n);

		if(MeshSink_active(t->words)) {

			//A degenerate triangle has no side to be on. Its normal packs to the +z encoding of a zero vector,
			// which is what unpacking a zero word yields, and the material still rides above it.

			const U32 oct = len2 > 0 ? U32_packOct18(F32x4_normalize3(n)) : U32_packOct18(F32x4_create3(0, 0, 1));
			const U32 word = MeshTriangle_pack(oct, material);

			gotoIfError3(clean, MeshSink_write(t->words, &word, sizeof(word), e_rr));
		}

		if(t->computeNormals && len2 > 0) {

			const U32 highest = U32_max(U32_max(i0, i1), i2);

			gotoIfError3(clean, MeshTriangles_growSums(t, highest + 1, alloc, e_rr));

			for(U8 c = 0; c < 3; ++c) {

				F32 *sum = t->normalSums.ptrNonConst + (U64) tri[c] * 3;

				F32x4_store3(sum, F32x4_add(F32x4_load3(sum), n));
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

//The recognized shape, spelled once. Everything about it has to hold for the fast path to be the same bytes as
//the walk: both attributes, both encodings, both offsets and a stride with nothing left over to zero.

static Bool MeshAttributeLayout_isSimple(MeshAttributeLayout l) {
	return
		l.entryCount == 2 && l.stride == sizeof(MeshAttribute) &&
		l.entries[0].attribute == EMeshAttribute_Normal &&
		l.entries[0].encoding == EMeshAttributeEncoding_Oct &&
		l.entries[0].format == ETextureFormatId_R32u &&
		!l.entries[0].offset &&
		l.entries[1].attribute == EMeshAttribute_Uv0 &&
		l.entries[1].encoding == EMeshAttributeEncoding_Raw &&
		l.entries[1].format == ETextureFormatId_RG16f &&
		l.entries[1].offset == sizeof(U32);
}

void MeshAttributes_create(EMeshFlags flags, MeshSink *sink, MeshAttributes *attributes) {

	const MeshAttributeLayout layout = MeshAttributeLayout_fromFlags(flags);

	*attributes = (MeshAttributes) {
		.sink = sink,
		.layout = layout,
		.hold = !!(flags & EMeshFlags_ComputeNormals),
		.simple = MeshAttributeLayout_isSimple(layout)
	};
}

//Either record shape, from the same normal and uv.

//A normal the file did not supply packs as +z rather than as the zero vector, which has no octahedral encoding.

U32 Mesh_packNormal(const F32 *n) {

	const F32x4 v = F32x4_load3(n);

	if(F32x4_sqLen3(v) <= 0)
		return U32_packOct32(F32x4_create3(0, 0, 1));

	return U32_packOct32(F32x4_normalize3(v));
}

static Bool MeshAttributes_writeRecord(MeshAttributes *a, const F32 *normal, const F32 *uv, Error *e_rr) {

	Bool s_uccess = true;

	//Assembled into a local of the layout's stride and written once, so where an attribute sits is the table's
	//business and not the sink's. One the reader has no value for is left at zero rather than refused: a
	// layout naming it is a consumer's request, not a claim the file carried it.

	if(a->simple) {

		const F16 u = F32_castF16(uv[0]), v = F32_castF16(uv[1]);
		const MeshAttribute simple = { .normal = Mesh_packNormal(normal), .uv = (U32) u | ((U32) v << 16) };

		const F32 back[2] = { F16_castF32(u), F16_castF32(v) };

		for(U8 c = 0; c < 2; ++c) {

			const F32 err = F32_abs(back[c] - uv[c]);

			if(err > a->maxUvError)
				a->maxUvError = err;
		}

		return MeshSink_write(a->sink, &simple, sizeof(simple), e_rr);
	}

	//Only the stride is zeroed, not the local: an attribute writes over its own bytes and what is left is the
	// layout's gaps, which is what a consumer is entitled to read as zero.

	U8 record[MeshAttributeLayout_maxEntries * 16];

	Buffer_unsetAllBits(Buffer_createRef(record, a->layout.stride), NULL);

	for(U8 i = 0; i < a->layout.entryCount; ++i) {

		const MeshAttributeEntry ch = a->layout.entries[i];
		U8 *at = record + ch.offset;

		const F32 *src = NULL;
		U8 count = 0;

		switch(ch.attribute) {
			case EMeshAttribute_Normal:  src = normal; count = 3; break;
			case EMeshAttribute_Uv0:     src = uv;     count = 2; break;
			default:                     continue;
		}

		if(ch.encoding == EMeshAttributeEncoding_Oct) {

			const U32 packed = Mesh_packNormal(src);
			Buffer_memcpy(Buffer_createRef(at, sizeof(packed)), Buffer_createRefConst(&packed, sizeof(packed)));
			continue;
		}

		MeshAttribute_encode(at, ch.format, src, count);

		//What a lossy attribute cost, read back from what was written so it holds for any format. Only the uv
		// is reported, since that is the one a consumer addresses a texture with.

		if(ch.attribute == EMeshAttribute_Uv0 && ch.format != ETextureFormatId_RG32f) {

			F32 back[2] = { 0, 0 };
			MeshAttribute_decode(at, ch.format, back, 2);

			for(U8 c = 0; c < 2; ++c) {

				const F32 err = F32_abs(back[c] - src[c]);

				if(err > a->maxUvError)
					a->maxUvError = err;
			}
		}
	}

	gotoIfError3(clean, MeshSink_write(a->sink, record, a->layout.stride, e_rr));

clean:
	return s_uccess;
}

Bool MeshAttributes_push(MeshAttributes *a, const F32 *normal, const F32 *uv, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!MeshSink_active(a->sink))
		goto clean;

	if(!a->hold) {
		gotoIfError3(clean, MeshAttributes_writeRecord(a, normal, uv, e_rr));
		goto clean;
	}

	//Held as five floats a vertex: the normal the file gave, which may be nothing, and the uv. What the file
	//left out is what the finish fills in from the accumulated sums.

	const F32 held[5] = { normal[0], normal[1], normal[2], uv[0], uv[1] };

	gotoIfError3(clean, Mesh_growF32(&a->held, &a->heldCapacity, a->held.length + 5, 5 * 1024, alloc, e_rr));
	gotoIfError3(clean, Mesh_appendF32(&a->held, held, 5, alloc, e_rr));

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

		//The file's own normal wins. Only where it named none does the accumulated sum fill in, and a vertex no
		// triangle touched, or only degenerate ones, has neither and gets the +z a missing one packs as.

		F32 n[3] = { held[0], held[1], held[2] };

		if(!n[0] && !n[1] && !n[2] && (U64) i * 3 + 3 <= t->normalSums.length) {
			const F32 *sum = t->normalSums.ptr + (U64) i * 3;
			n[0] = sum[0]; n[1] = sum[1]; n[2] = sum[2];
		}

		if(!n[0] && !n[1] && !n[2])
			a->placeholderNormal = true;

		gotoIfError3(clean, MeshAttributes_writeRecord(a, n, held + 3, e_rr));
	}

clean:
	return s_uccess;
}

void MeshAttributes_free(MeshAttributes *a, const Allocator *alloc) {
	ListF32_free(&a->held, alloc);
}
