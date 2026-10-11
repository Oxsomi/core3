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

//formats/ply/perf/ply_perf.c

#include "formats/ply/perf/ply_perf.h"
#include "formats/ply/ply_file.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "types/container/memory_stream.h"
#include "types/container/job_queue.h"
#include "types/container/ref_ptr.h"
#include "types/container/string.h"
#include "types/container/log.h"
#include "types/base/time.h"
#include "types/base/error.h"
#include "types/base/constants.h"

#include <inttypes.h>

//One pass, into resizable sinks (positions and indices only; this file carries neither normals nor uvs, and
//a consumer materializing only geometry is the common case a BLAS builder wants). Returns the wall time.

static Bool PlyPerf_readOnce(
	StreamRef *stream, EMeshFlags flags, JobQueue *jobs, const Allocator *alloc, MeshInfo *info, DNs *took,
	Error *e_rr
) {

	Bool s_uccess = true;
	const RefPtrType type = MemoryStream_makeType(alloc);

	MemoryStreamRef *positions = NULL, *indices = NULL;

	gotoIfError3(clean, MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &positions, e_rr));
	gotoIfError3(clean, MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &indices, e_rr));

	const MeshOutput output = (MeshOutput) {
		.positions = (StreamRef*) positions,
		.indices = (StreamRef*) indices
	};

	*info = (MeshInfo) { 0 };

	const Ns start = Time_now();
	U64 off = 0;

	gotoIfError3(clean, Ply_readParallel(stream, &off, flags, info, &output, jobs, false, alloc, e_rr));

	*took = Time_elapsed(start);

clean:
	RefPtr_dec((RefPtr**) &indices);
	RefPtr_dec((RefPtr**) &positions);
	return s_uccess;
}

Bool Perf_plyParallelRead(const CharString *path, U64 threadCount, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	const RefPtrType fileHandleType = FileHandle_makeType(alloc);
	const RefPtrType streamType = FileStream_makeType(alloc);
	StreamRef *stream = NULL;

	JobQueue jobs = (JobQueue) { 0 };
	Bool queueMade = false;

	if(!threadCount)
		threadCount = Platform_getThreads();

	gotoIfError3(clean, File_openStream(
		path, 10 * SECOND, EFileOpenType_Read, false, &fileHandleType, &streamType, &stream, e_rr
	));

	MeshInfo headerInfo = (MeshInfo) { 0 };
	MeshHeader header = (MeshHeader) { 0 };
	U64 headOff = 0;

	gotoIfError3(clean, Ply_readHeader(stream, &headOff, &headerInfo, &header, alloc, e_rr));

	const OxStream *streamData = RefPtr_data(stream, OxStream);

	Log_debugLn(
		alloc,
		"Perf_plyParallelRead() %.2f MiB, %" PRIu32 " vertices, %" PRIu32 " faces, concurrentRead=%d",
		(F64) streamData->size / (F64) MIBI, header.vertexCount, header.faceCount,
		(int) !!(streamData->streamType & EStreamType_ConcurrentRead)
	);

	MeshInfo serialInfo = (MeshInfo) { 0 };
	DNs serialTook = 0;
	gotoIfError3(clean, PlyPerf_readOnce(stream, EMeshFlags_None, NULL, alloc, &serialInfo, &serialTook, e_rr));

	Log_debugLn(
		alloc, "Ply_read(): %f s (%" PRIu32 " vertices, %" PRIu32 " indices)",
		(F64) serialTook / SECOND, serialInfo.vertexCount, serialInfo.indexCount
	);

	gotoIfError3(clean, JobQueue_create(threadCount, alloc, &jobs, e_rr));
	queueMade = true;

	MeshInfo parallelInfo = (MeshInfo) { 0 };
	DNs parallelTook = 0;

	gotoIfError3(clean, PlyPerf_readOnce(stream, EMeshFlags_None, &jobs, alloc, &parallelInfo, &parallelTook, e_rr));

	Log_debugLn(
		alloc, "Ply_readParallel() on %" PRIu64 " threads: %f s (%" PRIu32 " vertices, %" PRIu32 " indices)",
		threadCount, (F64) parallelTook / SECOND, parallelInfo.vertexCount, parallelInfo.indexCount
	);

	if(serialInfo.vertexCount != parallelInfo.vertexCount || serialInfo.indexCount != parallelInfo.indexCount)
		retError(clean, Error_invalidState(
			0, "Perf_plyParallelRead() Ply_read and Ply_readParallel disagreed on the counts they produced"
		));

	Log_debugLn(alloc, "Speedup: %fx", (F64) serialTook / (F64) parallelTook);

clean:

	if(queueMade)
		JobQueue_free(&jobs);

	RefPtr_dec(&stream);
	return s_uccess;
}
