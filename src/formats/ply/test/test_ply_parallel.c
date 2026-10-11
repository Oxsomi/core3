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

//formats/ply/test/test_ply_parallel.c
//
//Ply_readParallel against Ply_read on generated meshes, forced to split with forceParallel so a file far
//below PLY_PARALLEL_MIN_BYTES still exercises chunk boundaries: a vertex count a chunk count does not divide
//evenly, a face section with mixed corner counts tall enough to promote Ply_buildFaceLayout to its table, and
// both byte orders. What is compared is bytes, not values close to them: a chunk that landed at the wrong
// offset produces a different vertex, not an imprecise one.

#include "test_ply_shared.h"
#include "formats/ply/ply_file.h"
#include "types/container/job_queue.h"
#include "types/container/memory_stream.h"
#include "types/container/ref_ptr.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/base/error.h"

#include <inttypes.h>

//A few chunks per thread even on a tiny file, and both single and multi threaded contexts: threadCount 1 runs
//every chunk inline on the calling thread (job_queue.h), which keeps a failing case reproducible, and
// threadCount 3 is what actually exercises concurrent chunks.

#define PLY_TEST_THREADS 3

static U64 putU32At(U8 *out, U32 v, Bool bigEndian) {

	for(U8 i = 0; i < 4; ++i)
		out[i] = (U8) (v >> (bigEndian ? (24 - 8 * i) : (8 * i)));

	return 4;
}

static U64 putF32At(U8 *out, F32 v, Bool bigEndian) {

	union { F32 f; U32 u; } cast;
	cast.f = v;

	return putU32At(out, cast.u, bigEndian);
}

typedef struct PlyGenSpec {

	const U8 *faceCorners;   //faceCount entries, each >= 3; NULL means faceCount triangles of 3 each

	U32 vertexCount;
	U32 faceCount;

	Bool bigEndian;
	Bool withNormals;
	Bool withUvs;
	U8 padding[5];

} PlyGenSpec;

//A deterministic binary PLY. Vertex i sits at (i + 1, 2i + 2, 3i + 3), off the origin so a bound left at zero shows.
//Its normal (when asked for) is +z and its uv (when asked for) is (i / vertexCount, 1 - i / vertexCount),
// both distinct per vertex so a chunk that reads the wrong one is visible.
//Faces walk the vertex ring in order, wrapping modulo vertexCount,
// so any corner count sequence is representable without the vertex count having to divide evenly into it.

static Bool Ply_genBuild(Test *t, const PlyGenSpec *spec, Buffer *out, Error *e_rr) {

	Bool s_uccess = true;
	CharString header = CharString_createNull();

	const U8 vertexStride = (U8) (12 + (spec->withNormals ? 12 : 0) + (spec->withUvs ? 8 : 0));

	U64 bodyBytes = (U64) spec->vertexCount * vertexStride;

	for(U32 f = 0; f < spec->faceCount; ++f) {
		const U8 corners = spec->faceCorners ? spec->faceCorners[f] : 3;
		bodyBytes += 1 + (U64) corners * 4;
	}

	Bool built = CharString_appendFormat(
		t->alloc, &header, &t->err,
		"ply\nformat binary_%s_endian 1.0\nelement vertex %" PRIu32 "\nproperty float x\nproperty float y\nproperty float z\n",
		spec->bigEndian ? "big" : "little", spec->vertexCount
	);

	if(built && spec->withNormals)
		built = CharString_appendFormat(
			t->alloc, &header, &t->err, "property float nx\nproperty float ny\nproperty float nz\n"
		);

	if(built && spec->withUvs)
		built = CharString_appendFormat(t->alloc, &header, &t->err, "property float s\nproperty float t\n");

	built = built && CharString_appendFormat(
		t->alloc, &header, &t->err,
		"element face %" PRIu32 "\nproperty list uchar int vertex_indices\nend_header\n", spec->faceCount
	);

	if(!built)
		retError(clean, Error_invalidState(0, "Ply_genBuild() couldn't format the header"));

	gotoIfError3(clean, Buffer_createUninitializedBytes(CharString_length(header) + bodyBytes, t->alloc, out, e_rr));

	U8 *at = out->ptrNonConst;

	Buffer_memcpy(
		Buffer_createRef(at, CharString_length(header)), Buffer_createRefConst(header.ptr, CharString_length(header))
	);

	at += CharString_length(header);

	for(U32 v = 0; v < spec->vertexCount; ++v) {

		at += putF32At(at, (F32) v + 1, spec->bigEndian);
		at += putF32At(at, (F32) v * 2 + 2, spec->bigEndian);
		at += putF32At(at, (F32) v * 3 + 3, spec->bigEndian);

		if(spec->withNormals) {
			at += putF32At(at, 0, spec->bigEndian);
			at += putF32At(at, 0, spec->bigEndian);
			at += putF32At(at, 1, spec->bigEndian);
		}

		if(spec->withUvs) {
			at += putF32At(at, (F32) v / (F32) spec->vertexCount, spec->bigEndian);
			at += putF32At(at, 1.0f - (F32) v / (F32) spec->vertexCount, spec->bigEndian);
		}
	}

	U32 cursor = 0;

	for(U32 f = 0; f < spec->faceCount; ++f) {

		const U8 corners = spec->faceCorners ? spec->faceCorners[f] : 3;
		*(at++) = corners;

		for(U8 c = 0; c < corners; ++c) {
			at += putU32At(at, cursor % spec->vertexCount, spec->bigEndian);
			++cursor;
		}
	}

clean:
	CharString_free(&header, t->alloc);
	return s_uccess;
}

//Reads through Ply_readParallel into resizable sinks, the shape Test_meshRead gives Ply_read; the two share
//everything but which C function they call and the extra parallel-only arguments.

static MeshResult readPlyParallel(
	Test *t, Buffer bytes, EMeshFlags flags, Bool withAttributes, Bool withTriangles,
	JobQueue *jobs, Bool forceParallel, Error *readError
) {

	MeshResult r = (MeshResult) { 0 };
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	MemoryStreamRef *src = NULL, *positions = NULL, *attributes = NULL, *indices = NULL, *triangles = NULL;

	//A non resizable region stream declares EStreamType_ConcurrentRead on its own (memory_stream.c), which is
	//exactly right here: the bytes never change once built, so two chunks reading disjoint parts of them at
	// once cannot tear anything.

	if(!MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(bytes.ptr, Buffer_length(bytes)), 0, Buffer_length(bytes), EMemoryStreamFlags_None,
		&type, &src, &t->err
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

	if(!Ply_readParallel(
		(StreamRef*) src, &off, flags, &r.info, &output, jobs, forceParallel, t->alloc, readError
	))
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

//Ply_read once, then Ply_readParallel both without a queue (which is Ply_read again, a guard against the
//comparison becoming trivially true) and with one under forceParallel, and asserts every output stream and
// every MeshInfo field the two produced is identical.

static void checkMatchesSerial(Test *t, const C8 *name, const PlyGenSpec *spec, EMeshFlags flags) {

	Buffer bytes = Buffer_createNull();

	if(!Test_assert(t, name, Ply_genBuild(t, spec, &bytes, &t->err)))
		return;

	const Bool withAttributes = true, withTriangles = true;

	MeshResult serial = Test_meshRead(
		t, Ply_read, bytes.ptr, Buffer_length(bytes), flags, withAttributes, withTriangles, &t->err
	);

	if(!Test_assert(t, name, serial.ok)) {
		Buffer_free(&bytes, t->alloc);
		return;
	}

	JobQueue jobs = (JobQueue) { 0 };
	Bool queued = JobQueue_create(PLY_TEST_THREADS, t->alloc, &jobs, &t->err);
	Test_assert(t, name, queued);

	MeshResult withoutQueue = readPlyParallel(t, bytes, flags, withAttributes, withTriangles, NULL, true, &t->err);
	MeshResult forced = queued
		? readPlyParallel(t, bytes, flags, withAttributes, withTriangles, &jobs, true, &t->err)
		: (MeshResult) { 0 };

	Test_assert(t, name, withoutQueue.ok);
	Test_assert(t, name, !queued || forced.ok);

	if(withoutQueue.ok) {

		Test_assert(t, name, Buffer_eq(withoutQueue.positions, serial.positions));
		Test_assert(t, name, Buffer_eq(withoutQueue.attributes, serial.attributes));
		Test_assert(t, name, Buffer_eq(withoutQueue.indices, serial.indices));
		Test_assert(t, name, Buffer_eq(withoutQueue.triangles, serial.triangles));
	}

	if(queued && forced.ok) {

		Test_assert(t, name, Buffer_eq(forced.positions, serial.positions));
		Test_assert(t, name, Buffer_eq(forced.attributes, serial.attributes));
		Test_assert(t, name, Buffer_eq(forced.indices, serial.indices));
		Test_assert(t, name, Buffer_eq(forced.triangles, serial.triangles));

		Test_assert(t, name, forced.info.vertexCount == serial.info.vertexCount);
		Test_assert(t, name, forced.info.indexCount == serial.info.indexCount);
		Test_assert(t, name, forced.info.fannedFaces == serial.info.fannedFaces);
		Test_assert(t, name, forced.info.materialCount == serial.info.materialCount);
		Test_assert(t, name, forced.info.hasNormals == serial.info.hasNormals);
		Test_assert(t, name, forced.info.hasUvs == serial.info.hasUvs);
		Test_assert(t, name, forced.info.allNormals == serial.info.allNormals);
		Test_assert(t, name, forced.info.allUvs == serial.info.allUvs);
		Test_assert(t, name, forced.info.maxUvError == serial.info.maxUvError);

		Test_assert(t, name, F32x4_eqExact3(forced.info.aabbMin, serial.info.aabbMin));
		Test_assert(t, name, F32x4_eqExact3(forced.info.aabbMax, serial.info.aabbMax));
	}

	MeshResult_free(t, &forced);
	MeshResult_free(t, &withoutQueue);
	MeshResult_free(t, &serial);
	JobQueue_free(&jobs);
	Buffer_free(&bytes, t->alloc);
}

//Prime-ish counts on purpose, so PLY_TEST_THREADS * 4 chunks never divides them evenly and the last chunk of
//both the vertex and the face split is always a different size than the rest.

void Test_plyParallelTriangles(Test *t) {

	Test_setModule(t, "PLY/parallelTriangles");

	const PlyGenSpec specs[] = {
		{ .vertexCount = 37, .faceCount = 11 },
		{ .vertexCount = 37, .faceCount = 11, .bigEndian = true },
		{ .vertexCount = 509, .faceCount = 163 },
		{ .vertexCount = 509, .faceCount = 163, .withNormals = true },
		{ .vertexCount = 509, .faceCount = 163, .withUvs = true },
		{ .vertexCount = 509, .faceCount = 163, .withNormals = true, .withUvs = true, .bigEndian = true },

		//A face count the vertex ring wraps around several times over, so the same vertex is named by more
		// than one chunk's worth of faces and every one of them had better agree on where it is.

		{ .vertexCount = 17, .faceCount = 97 }
	};

	for(U8 i = 0; i < (U8) (sizeof(specs) / sizeof(specs[0])); ++i)
		checkMatchesSerial(t, "triangles", specs + i, EMeshFlags_None);
}

void Test_plyParallelMixedPolygons(Test *t) {

	Test_setModule(t, "PLY/parallelMixedPolygons");

	//3, 4, 5 and 6 corners cycling, which Ply_buildFaceLayout cannot treat as uniform past the second face,
	//so this is what promotes it to Ply_buildFaceLayoutTable.

	U8 corners[211];

	for(U16 i = 0; i < 211; ++i)
		corners[i] = (U8) (3 + i % 4);

	const PlyGenSpec spec = { .vertexCount = 97, .faceCorners = corners, .faceCount = 211 };
	checkMatchesSerial(t, "mixedPolygons", &spec, EMeshFlags_None);

	const PlyGenSpec specBE = {
		.vertexCount = 97, .faceCorners = corners, .faceCount = 211, .bigEndian = true, .withNormals = true
	};
	checkMatchesSerial(t, "mixedPolygonsBigEndian", &specBE, EMeshFlags_None);

	//Uniform until the very last face, which is the boundary a stride based split would get wrong fastest.

	U8 tailMismatch[64];

	for(U16 i = 0; i < 64; ++i)
		tailMismatch[i] = 3;

	tailMismatch[63] = 4;

	const PlyGenSpec specTail = { .vertexCount = 40, .faceCorners = tailMismatch, .faceCount = 64 };
	checkMatchesSerial(t, "uniformUntilLastFace", &specTail, EMeshFlags_None);
}

void Test_plyParallelFlags(Test *t) {

	Test_setModule(t, "PLY/parallelFlags");

	const PlyGenSpec spec = { .vertexCount = 253, .faceCount = 81, .withNormals = true, .withUvs = true };

	checkMatchesSerial(t, "narrowIndices", &spec, EMeshFlags_NarrowIndices);
	checkMatchesSerial(t, "wideUvs", &spec, EMeshFlags_WideUvs);
}

//ComputeNormals and QuantizePositions both need the whole mesh, so Ply_readParallel has to fall all the way
//back to Ply_read for them even when forced; what is asserted is that forcing changes nothing about the
// result, not that it changes nothing about the code path.

void Test_plyParallelSerialFlagsFallBack(Test *t) {

	Test_setModule(t, "PLY/parallelSerialFlagsFallBack");

	const PlyGenSpec spec = { .vertexCount = 131, .faceCount = 43 };

	checkMatchesSerial(t, "computeNormals", &spec, EMeshFlags_ComputeNormals);
	checkMatchesSerial(t, "quantizePositions", &spec, EMeshFlags_QuantizePositions);
}

//A file under PLY_PARALLEL_MIN_BYTES, read without forceParallel, has to come back byte identical too: this
// is Ply_readParallel choosing Ply_read on its own rather than forceParallel choosing it for the test.

void Test_plyParallelBelowThreshold(Test *t) {

	Test_setModule(t, "PLY/parallelBelowThreshold");

	Buffer bytes = Buffer_createNull();
	const PlyGenSpec spec = { .vertexCount = 23, .faceCount = 7 };

	if(!Test_assert(t, "build", Ply_genBuild(t, &spec, &bytes, &t->err)))
		return;

	MeshResult serial = Test_meshRead(t, Ply_read, bytes.ptr, Buffer_length(bytes), EMeshFlags_None, true, true, &t->err);

	JobQueue jobs = (JobQueue) { 0 };
	const Bool queued = JobQueue_create(PLY_TEST_THREADS, t->alloc, &jobs, &t->err);
	Test_assert(t, "queue", queued);

	MeshResult unforced = queued
		? readPlyParallel(t, bytes, EMeshFlags_None, true, true, &jobs, false, &t->err)
		: (MeshResult) { 0 };

	if(Test_assert(t, "read", serial.ok) && queued && Test_assert(t, "read", unforced.ok)) {
		Test_assert(t, "positions", Buffer_eq(unforced.positions, serial.positions));
		Test_assert(t, "indices", Buffer_eq(unforced.indices, serial.indices));
	}

	MeshResult_free(t, &unforced);
	MeshResult_free(t, &serial);

	if(queued)
		JobQueue_free(&jobs);

	Buffer_free(&bytes, t->alloc);
}
