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

//shader_compiler/compiler_helper_jobs.c

#include "types/container/list_impl.h"
#include "types/container/string.h"
#include "types/container/log.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/base/lock.h"
#include "types/base/thread.h"
#include "types/container/job_queue.h"
#include "types/container/list_basic_types.h"
#include "types/base/string_read_helper.h"
#include "types/container/memory_stream.h"
#include "types/container/ref_ptr.h"
#include "types/base/constants.h"
#include "formats/oiSH/sh_file.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "shader_compiler/compiler.h"
#include "compiler_helper_internal.h"

//Per-file compile job.
//
//Each input file is compiled fully independently
// (precompile -> unique compiles -> compile -> link -> reflection -> register into an SHFile), producing one SHFile per file.
//Only combining SHFiles that share the same output (e.g. DXIL + SPIRV into one oiSH)
// and writing them to disk happens afterwards on the owning thread,
// since that part is inherently sequential and cheap compared to the compiles themselves.
//
//Because every job only writes to its own CompilerShaderFileJob (result, success) no locking is needed between jobs.
//The per thread Compiler instance is selected with the JobQueue's threadId,
// which is guaranteed stable and unique per execution context.
//
//The same job also runs unmodified in the JobQueue's single threaded (inline) mode (threadCount <= 1),
// which keeps a deterministic, easily debuggable flow around.

typedef struct CompilerShaderFileJob {

	ListCharString allFiles;            //Shared (read only)
	ListCharString allShaderText;       //Shared (read only)
	ListU8 allCompileOutputs;           //Shared (read only)

	ListCompiler compilers;             //Shared (read only); one Compiler per JobQueue execution context

	ListCharString includeDirs;         //Shared (read only)

	const Allocator *alloc;

	U64 fileId;

	SHFile result;                      //Output; only written by this job

	Bool success;                       //Output; only written by this job
	Bool isDebug;
	Bool keepRegisters;
	Bool ignoreEmptyFiles;
	Bool enableLogging;
	U8 padding[3];

} CompilerShaderFileJob;

TList(CompilerShaderFileJob);
TListImpl(CompilerShaderFileJob);

Bool Compiler_registerShaderBinary(
	SHFile *shFile,
	CompileResult *tempResult,
	ESHBinaryType compileMode,
	CharString sourceFile,
	const SHEntryRuntime *runtimeEntry,
	const SHBinaryIdentifier *binaryIdentifier,
	const Allocator *alloc,
	Error *e_rr
);

Bool Compiler_registerShaderEntries(
	SHFile *shFile,
	const ListSHEntryRuntime *entries,
	ListU32 binaryIndices,
	const Allocator *alloc,
	Error *e_rr
);

//A file's work runs as a 3-level JobGroup fan-out tree on the shared queue:
//  file latch  ->  per-combination compile jobs  ->  per-linkEntry leaf jobs.
//
//Each combination holds one token of the file latch until *its* leaves drain (released in the combination's finalize),
// so the file context provably outlives every leaf.
//That context owns the shared SHFile, its lock and the binaryIndices every leaf writes.
//A combination's compiled binary is freed the moment its own leaves finish (per-combination finalize),
// bounding peak memory to in-flight combinations.
//The file latch's finalize assembles the oiSH (registerShaderEntries) and moves it into job->result.
//On any failure the finalize is skipped and the context is reclaimed by the group's dataDestructor.
//Every enter is matched by exactly one leave along the normal drain,
// so a single JobQueue_wait completes the whole tree with balanced tokens and no leaks.
//
//Sub-jobs run on arbitrary execution contexts,
// so each selects its own Compiler via job->compilers.ptr[threadId] (lock free, like the old per-file jobs).
//The only shared mutable state is the file's SHFile + binaryIndices, guarded by file->lock.
//The reflection pass (Compiler_processSingle) takes that same lock internally.

typedef struct CompilerFileCtx {

	CompilerShaderFileJob *job;             //Back-ref for outputs (result/success) + shared read-only inputs

	ListSHEntryRuntime runtimeEntries;      //Owned; shared read-only during fan-out
	ListU32 compileCombinations;            //Owned

	SHFile shFile;                          //Shared; mutated by leaves under lock
	ListU32 binaryIndices;                  //Shared; mutated by leaves under lock
	SpinLock lock;                          //Guards shFile + binaryIndices (and the processSingle reflection pass)

	JobGroup group;                         //File latch; finalize = Compiler_finalizeShaderFile

	ESHBinaryType binaryType;
	U8 padding[4];

} CompilerFileCtx;

typedef struct CompilerComboCtx {

	CompilerFileCtx *file;

	CompileResult tempResult;               //This combination's compiled binary; owned, read by its leaves
	ListLinkEntry linkEntries;              //Owned
	ListCompilerEntrypoint uniqueEntrypoints;   //Owned

	JobGroup group;                         //Combination latch; finalize = Compiler_finalizeCombination

	const Allocator *alloc;                        //Self-contained so the destructor never derefs sibling contexts

	U16 runtimeEntryId;
	U16 combinationId;
	Bool isRt;
	Bool isGfxOrComp;

} CompilerComboCtx;

typedef struct CompilerLeafCtx {
	CompilerComboCtx *combo;                //Parent; alive until this combination's finalize (after all leaves)
	const Allocator *alloc;                        //Self-contained so the destructor never derefs sibling contexts
	U64 linkIndex;                          //Index into combo->linkEntries
} CompilerLeafCtx;

//Frees the file context and everything it still owns.
//shFile is moved out by finalize on the success path (left null here).
//Doubles as the group dataDestructor for the shutdown-discard case.
void CompilerFileCtx_free(void *ptr) {

	CompilerFileCtx *ctx = (CompilerFileCtx*) ptr;

	if(!ctx)
		return;

	const Allocator *alloc = ctx->job->alloc;      //job outlives the queue (owned by Compiler_compileShaders), so this is safe

	SHFile_free(&ctx->shFile, alloc);
	ListU32_free(&ctx->binaryIndices, alloc);
	ListSHEntryRuntime_freeUnderlying(&ctx->runtimeEntries, alloc);
	ListU32_free(&ctx->compileCombinations, alloc);

	//Rebuilt from the raw pointer, so the aligned bit the creator set is gone and has to be restored.
	//This context is always allocated aligned, since it embeds an alignas(64) SpinLock.

	Buffer buf = Buffer_createManagedPtr(ctx, sizeof(*ctx));
	Buffer_markAligned(&buf);
	Buffer_free(&buf, alloc);
}

//Frees a combination context and everything it owns (its compiled binary included).
//Doubles as the combination group's dataDestructor for the shutdown-discard case.
void CompilerComboCtx_free(void *ptr) {

	CompilerComboCtx *ctx = (CompilerComboCtx*) ptr;

	if(!ctx)
		return;

	const Allocator *alloc = ctx->alloc;

	CompileResult_free(&ctx->tempResult, alloc);
	ListLinkEntry_freeUnderlying(&ctx->linkEntries, alloc);
	ListCompilerEntrypoint_freeUnderlying(&ctx->uniqueEntrypoints, alloc);

	alloc->free(alloc->ptr, Buffer_createManagedPtr(ctx, sizeof(*ctx)));
}

//Frees a leaf context.
//Only used as the shutdown-discard destructor; a leaf that runs frees itself.
void CompilerLeafCtx_freeDiscarded(void *ptr) {

	CompilerLeafCtx *leaf = (CompilerLeafCtx*) ptr;

	if(!leaf)
		return;

	const Allocator *alloc = leaf->alloc;
	alloc->free(alloc->ptr, Buffer_createManagedPtr(leaf, sizeof(*leaf)));
}

//Leaf: link (for lib/annotation) + reflection + register one binary into the shared SHFile.
//This is the old inner linkEntries-loop body, one iteration per job.
Bool Compiler_compileLinkJob(void *data, U64 threadId, JobQueue *queue) {

	(void) queue;

	CompilerLeafCtx *leaf = (CompilerLeafCtx*) data;

	if(!leaf)
		return false;

	CompilerComboCtx *combo = leaf->combo;
	CompilerFileCtx *file = combo->file;
	CompilerShaderFileJob *job = file->job;

	const Allocator *alloc = job->alloc;
	const Compiler *compiler = &job->compilers.ptr[threadId];

	Error errTmp = Error_none(), *e_rr = &errTmp;
	Bool s_uccess = true;
	Bool locked = false;

	CharString inputPath = job->allFiles.ptr[job->fileId];
	CompileResult tempResult2 = (CompileResult) { 0 };

	//runtimeEntry is a private shallow copy (refs into file->runtimeEntries, alive for the whole file).
	//The annotation branch below mutates it, which is why each leaf needs its own.

	SHEntryRuntime runtimeEntry = file->runtimeEntries.ptr[combo->runtimeEntryId];
	LinkEntry linkEntry = combo->linkEntries.ptr[leaf->linkIndex];
	Buffer uniformData = linkEntry.uniformData;
	U16 currentCombinationId = linkEntry.combinationId;

	SHBinaryIdentifier binaryIdentifier = (SHBinaryIdentifier) { 0 };
	gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(&runtimeEntry, currentCombinationId, &binaryIdentifier, e_rr));

	ListBuffer inputs = (ListBuffer) { 0 };
	gotoIfError3(clean, ListBuffer_createRefConst(&combo->tempResult.binary, 1, &inputs, e_rr));

	Bool isShaderAnnotation = runtimeEntry.isShaderAnnotation;

	if(isShaderAnnotation) {

		CompilerEntrypoint entry = (CompilerEntrypoint) { 0 };

		if (linkEntry.entrypointId != U16_MAX) {

			//entrypointId doesn't map to uniqueEntrypoints as some might be missing there.
			//It maps to our parsed runtimeEntries.

			CharString entrypointName = file->runtimeEntries.ptr[linkEntry.entrypointId].entry.name;

			U64 l = 0;

			for (; l < combo->uniqueEntrypoints.length; ++l)
				if (CharString_equalsStringSensitive(&entrypointName, &combo->uniqueEntrypoints.ptr[l].name))
					break;

			if(l == combo->uniqueEntrypoints.length)
				retError(clean, Error_invalidState(
					0,
					"Compiler_compileLinkJob() somehow an entrypointId was referenced by a linkEntry "
					"that doesn't exist"
				));

			entry = combo->uniqueEntrypoints.ptr[l];
		}

		else entry.stage = ESHPipelineStage_Count;      //Mark as lib

		tempResult2.type = ECompileResultType_Binary;

		gotoIfError3(clean, Compiler_linkSingle(
			compiler,
			inputPath,
			combo->runtimeEntryId,
			currentCombinationId,
			file->binaryType,
			&inputs,
			&runtimeEntry.uniforms,
			uniformData,
			entry.name,
			binaryIdentifier.shaderVersion,
			entry.stage,
			binaryIdentifier.extensions,
			job->enableLogging,
			&tempResult2.binary,
			alloc
		));

		binaryIdentifier.stageType = entry.stage;

		Bool currGfxOrComp = !(
			(entry.stage >= ESHPipelineStage_RtStartExt && entry.stage >= ESHPipelineStage_RtEndExt) ||
			entry.stage >= ESHPipelineStage_Count ||
			entry.stage == ESHPipelineStage_WorkgraphExt
		);

		if(currGfxOrComp)
			binaryIdentifier.entrypoint = CharString_createRefStrConst(entry.name);

		runtimeEntry.isShaderAnnotation = !currGfxOrComp;
	}

	//Process reflection and strip debug/reflection info if necessary

	gotoIfError3(clean, Compiler_processSingle(
		compiler,
		inputPath,
		combo->runtimeEntryId,
		currentCombinationId,
		file->binaryType,
		tempResult2.binary.ptr ? &tempResult2 : &combo->tempResult,
		job->isDebug,
		job->keepRegisters,
		&binaryIdentifier,
		&file->lock,
		&file->runtimeEntries,
		isShaderAnnotation,
		job->enableLogging,
		alloc,
		e_rr
	));

	if (linkEntry.entrypointId == U16_MAX)
		binaryIdentifier.stageType = combo->isRt ? ESHPipelineStage_RtStartExt : ESHPipelineStage_WorkgraphExt;

	//Register the binary and link its runtime entries to it.
	//binaryId is derived from the current const SHFile *size,
	// so the read + registerShaderBinary + binaryIndices push must be one atomic section.

	if(SpinLock_lock(&file->lock, U64_MAX) < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "Compiler_compileLinkJob() couldn't lock SHFile"));

	locked = true;

	U16 binaryId = (U16) file->shFile.binaries.length;

	gotoIfError3(clean, Compiler_registerShaderBinary(
		&file->shFile,
		tempResult2.binary.ptr ? &tempResult2 : &combo->tempResult,
		file->binaryType,
		inputPath,
		&runtimeEntry,
		&binaryIdentifier,
		alloc,
		e_rr
	));

	for (U64 l = 0; l < linkEntry.runtimeEntries.length; ++l)       //Link runtime entry to binary
		gotoIfError3(clean, ListU32_pushBack(
			&file->binaryIndices, binaryId | (((U32)linkEntry.runtimeEntries.ptr[l]) << 16), alloc, e_rr
		));

	SpinLock_unlock(&file->lock);
	locked = false;

clean:

	if(locked)
		SpinLock_unlock(&file->lock);

	if(!s_uccess) {

		if(job->enableLogging)
			Error_print(alloc, &errTmp, ELogLevel_Error, ELogOptions_Default);

		Error e2 = Error_none();
		JobGroup_fail(&file->group, &e2);       //Mark the whole file failed; finalize will be skipped
	}

	CompileResult_free(&tempResult2, alloc);

	alloc->free(alloc->ptr, Buffer_createManagedPtr(leaf, sizeof(*leaf)));

	Error e3 = Error_none();
	JobGroup_leave(&combo->group, &e3);         //Release this leaf's combination token (may fire the combo finalize)

	return s_uccess;
}

//Combination finalize: runs once all of this combination's leaves have drained.
//Frees the combination (its compiled binary included) and releases the combination's file-latch token.
//The combination group is never failed, so this always runs.
//A failing leaf fails the file latch instead, letting the tree still drain and free itself cleanly.
Bool Compiler_finalizeCombination(void *data, U64 threadId, JobQueue *queue) {

	(void) threadId; (void) queue;

	CompilerComboCtx *ctx = (CompilerComboCtx*) data;

	if(!ctx)
		return false;

	//Grab the file group before freeing ctx (which contains the back-ref).
	//The file context stays alive because this token hasn't been released yet.

	JobGroup *fileGroup = &ctx->file->group;

	CompilerComboCtx_free(ctx);

	Error e2 = Error_none();
	JobGroup_leave(fileGroup, &e2);

	return true;
}

//Combination job: compile this combination's binary, discover its link entries,
// then fan out one leaf job per link entry under a combination latch.
//Holds a self token while spawning so the latch can't reach zero mid-spawn (the documented enter(1)/enter(k)/leave pattern).
Bool Compiler_compileCombinationJob(void *data, U64 threadId, JobQueue *queue) {

	CompilerComboCtx *ctx = (CompilerComboCtx*) data;

	if(!ctx)
		return false;

	CompilerFileCtx *file = ctx->file;
	CompilerShaderFileJob *job = file->job;

	const Allocator *alloc = job->alloc;
	const Compiler *compiler = &job->compilers.ptr[threadId];

	Error errTmp = Error_none(), *e_rr = &errTmp;
	Bool s_uccess = true;
	Bool spawning = false;

	CharString inputPath = job->allFiles.ptr[job->fileId];
	CharString inputData = job->allShaderText.ptr[job->fileId];

	SHEntryRuntime runtimeEntry = file->runtimeEntries.ptr[ctx->runtimeEntryId];

	//Compile and return error if failed

	if(!Compiler_compileShaderSingle(
		compiler,
		file->binaryType,
		job->isDebug,
		job->keepRegisters,
		ctx->isRt,
		ctx->isGfxOrComp,
		inputPath,
		inputData,
		&ctx->tempResult,
		&file->runtimeEntries,
		ctx->runtimeEntryId,
		ctx->combinationId,
		&job->includeDirs,
		job->enableLogging,
		alloc
	)) {

		if(job->enableLogging)
			Log_errorLn(
				alloc, "Compile failed for file \"%.*s\"",
				(int)CharString_length(inputPath), inputPath.ptr
			);

		retError(clean, Error_invalidState(2, "Compiler_compileCombinationJob() compile failed"));
	}

	SHBinaryIdentifier binaryIdentifier = (SHBinaryIdentifier) { 0 };
	gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(&runtimeEntry, ctx->combinationId, &binaryIdentifier, e_rr));

	//Bracketed by two lines rather than logged once, because getLinkEntries reflects the binary through
	//SPIRV-Reflect, which can take the process down without saying which shader it was working on.
	//Both lines come from this job, so a "Reflecting" with no matching "Reflected" names the shader that
	//died no matter how the other threads' output interleaves. Pairing against the "Link"/"Process" lines
	//instead does not work: those are logged by the leaf jobs, so their ordering says nothing about this.
	//The pointer, length and ref bit are here because the only crash we have seen is inside SPIRV-Reflect's
	//read of this buffer. Without NO_COPY the only thing it does with our memory is one memcpy of exactly
	//this length, so a fault there means the length outruns the allocation or the pointer is already dead,
	//and "ref" tells those two apart from a genuinely bad module.

	if(job->enableLogging)
		Log_debugLn(
			alloc, "Reflecting entrypoints: %.*s (%s, %"PRIu32":%"PRIu32", %"PRIu64" bytes @ 0x%"PRIx64"%s)",
			(int) CharString_length(inputPath), inputPath.ptr,
			file->binaryType == ESHBinaryType_SPIRV ? "spirv" : "dxil",
			(U32) ctx->runtimeEntryId, (U32) ctx->combinationId,
			Buffer_length(ctx->tempResult.binary), (U64) ctx->tempResult.binary.ptr,
			Buffer_isRef(ctx->tempResult.binary) ? ", ref" : ""
		);

	//Lib files need to be specialized per shader annotation or per entrypoint; non libs loop once.

	gotoIfError3(clean, Compiler_getLinkEntries(
		compiler,
		&file->runtimeEntries,
		&binaryIdentifier,
		file->binaryType,
		&ctx->tempResult.binary,
		&ctx->uniqueEntrypoints,
		&ctx->linkEntries,
		alloc,
		e_rr
	));

	if(job->enableLogging)
		Log_debugLn(
			alloc, "Reflected entrypoints: %.*s (%s, %"PRIu32":%"PRIu32")",
			(int) CharString_length(inputPath), inputPath.ptr,
			file->binaryType == ESHBinaryType_SPIRV ? "spirv" : "dxil",
			(U32) ctx->runtimeEntryId, (U32) ctx->combinationId
		);

	//A non-annotation ([[oxc::stage]]) entry produces exactly one link entry, hence one leaf, and that leaf
	//reflects and rewrites the shared combo->tempResult in place. Two such leaves would race that buffer and
	//corrupt the heap. The invariant holds today because uniforms (the only thing that fans a stage entry into
	//multiple combinations) are rejected on non-annotation entries, but enforce it here so a future change
	//fails loudly rather than silently reintroducing that race.

	if (!runtimeEntry.isShaderAnnotation && ctx->linkEntries.length > 1)
		retError(clean, Error_invalidState(
			0,
			"Compiler_compileCombinationJob() a non-annotation entry produced multiple link entries, "
			"which would race the shared compile result"
		));

	//Activate the combination latch and fan out its leaves.
	//From here cleanup is via the latch: we hold a self token, fail the file latch on error,
	// then release the self token and let the already-pushed leaves drain into Compiler_finalizeCombination,
	// which frees ctx + leaves the file.

	gotoIfError3(clean, JobGroup_create(&ctx->group, queue, Compiler_finalizeCombination, ctx, CompilerComboCtx_free, e_rr));
	gotoIfError3(clean, JobGroup_enter(&ctx->group, 1, e_rr));
	spawning = true;

	for (U64 k = 0; k < ctx->linkEntries.length; ++k) {

		Buffer buf = Buffer_createNull();
		gotoIfError3(cleanSpawn, Buffer_createUninitializedBytes(sizeof(CompilerLeafCtx), alloc, &buf, e_rr));

		CompilerLeafCtx *leaf = (CompilerLeafCtx*) buf.ptrNonConst;
		*leaf = (CompilerLeafCtx) { .combo = ctx, .alloc = alloc, .linkIndex = k };

		gotoIfError3(cleanLeaf, JobGroup_enter(&ctx->group, 1, e_rr));

		if(!JobQueue_pushDestructor(queue, Compiler_compileLinkJob, leaf, CompilerLeafCtx_freeDiscarded, e_rr)) {
			Error e2 = Error_none();
			JobGroup_leave(&ctx->group, &e2);       //Undo this leaf's token; the push never took ownership
			alloc->free(alloc->ptr, Buffer_createManagedPtr(leaf, sizeof(*leaf)));
			retError(cleanSpawn, Error_invalidState(0, "Compiler_compileCombinationJob() couldn't push leaf"));
		}

		continue;

	cleanLeaf:
		alloc->free(alloc->ptr, Buffer_createManagedPtr(leaf, sizeof(*leaf)));
		goto cleanSpawn;
	}

	spawning = false;
	{
		Error e2 = Error_none();
		JobGroup_leave(&ctx->group, &e2);           //Release the self token
	}
	return true;

cleanSpawn:

	//Error after the latch went active: fail the file, release the self token,
	// let pushed leaves drain and free ctx via Compiler_finalizeCombination.

	if(job->enableLogging)
		Error_print(alloc, &errTmp, ELogLevel_Error, ELogOptions_Default);
	{
		Error e2 = Error_none();
		JobGroup_fail(&file->group, &e2);
		if(spawning)
			JobGroup_leave(&ctx->group, &e2);
	}
	return false;

clean:

	//Error before the latch went active (compile / link discovery / latch create).
	//No combination tokens exist yet, so free ctx directly, fail the file and release this combination's file token.

	if(!s_uccess && job->enableLogging)
		Error_print(alloc, &errTmp, ELogLevel_Error, ELogOptions_Default);

	CompilerComboCtx_free(ctx);
	{
		Error e2 = Error_none();
		JobGroup_fail(&file->group, &e2);
		JobGroup_leave(&file->group, &e2);
	}
	return false;
}

//File finalize: runs once every combination has drained and nothing failed.
//Sorts the binary indices, links entrypoints to binaries and hands the finished SHFile to the job's output slot.
Bool Compiler_finalizeShaderFile(void *data, U64 threadId, JobQueue *queue) {

	(void) threadId; (void) queue;

	CompilerFileCtx *ctx = (CompilerFileCtx*) data;

	if(!ctx)
		return false;

	const Allocator *alloc = ctx->job->alloc;
	Error errTmp = Error_none();
	Bool s_uccess = true;

	if(!ListU32_sort(ctx->binaryIndices))
		s_uccess = false;

	else if(!Compiler_registerShaderEntries(&ctx->shFile, &ctx->runtimeEntries, ctx->binaryIndices, alloc, &errTmp))
		s_uccess = false;

	if(s_uccess) {
		ctx->job->result = ctx->shFile;             //Move to the job's output slot
		ctx->shFile = (SHFile) { 0 };
		ctx->job->success = true;
	}

	else if(ctx->job->enableLogging)
		Error_print(alloc, &errTmp, ELogLevel_Error, ELogOptions_Default);

	CompilerFileCtx_free(ctx);
	return s_uccess;
}

//Spawner (per file): preprocess, then fan out one compile job per combination under the file latch.
//Runs as the file's queue job; the actual compiles/links happen in the spawned sub-jobs.
Bool Compiler_compileShaderFile(CompilerShaderFileJob *job, JobQueue *queue, U64 threadId, Error *e_rr) {

	Bool s_uccess = true;

	const Allocator *alloc = job->alloc;
	const U64 i = job->fileId;
	const Compiler *compiler = &job->compilers.ptr[threadId];

	CharString inputPath = job->allFiles.ptr[i];
	CharString inputData = job->allShaderText.ptr[i];
	ESHBinaryType binaryType = (ESHBinaryType) job->allCompileOutputs.ptr[i];

	ListSHEntryRuntime runtimeEntries = (ListSHEntryRuntime) { 0 };
	ListU32 compileCombinations = (ListU32) { 0 };
	SHFile shFile = (SHFile) { 0 };

	CompilerFileCtx *ctx = NULL;
	Bool enteredSelf = false;

	if(job->result.entries.ptr)
		retError(clean, Error_invalidParameter(1, 0, "Compiler_compileShaderFile()::job->result must be empty"));

	//Preprocess to get information necessary for real compiles.

	if(!Compiler_precompileShader(
		compiler, binaryType, job->isDebug, inputPath, inputData, &runtimeEntries, &job->includeDirs, job->enableLogging, alloc
	)) {

		if(job->enableLogging)
			Log_errorLn(
				alloc, "Precompile failed for file \"%.*s\"",
				(int)CharString_length(inputPath), inputPath.ptr
			);

		retError(clean, Error_invalidState(0, "Compiler_compileShaderFile() precompile failed"));
	}

	//No entrypoints; either an allowed empty file (success, empty result) or an error.

	if (!runtimeEntries.length) {

		if(!job->ignoreEmptyFiles) {

			if(job->enableLogging)
				Log_errorLn(
					alloc, "Precompile couldn't find entrypoints for file \"%.*s\"",
					(int)CharString_length(inputPath), inputPath.ptr
				);

			retError(clean, Error_invalidState(1, "Compiler_compileShaderFile() couldn't find entrypoints"));
		}

		job->success = true;        //Allowed empty file
		goto clean;
	}

	U32 crc32c = Buffer_crc32c(CharString_bufferConst(inputData));

	gotoIfError3(clean, SHFile_create(ESHSettingsFlags_None, OXC3_VERSION, crc32c, alloc, &shFile, e_rr));
	gotoIfError3(clean, Compiler_getUniqueCompiles(&runtimeEntries, &compileCombinations, alloc, e_rr));

	//Move the accumulators into a heap file context shared by the whole fan-out tree.

	//Aligned rather than plain: SpinLock is alignas(64) and this struct embeds one, which malloc's 16 bytes don't satisfy.
	//clang stores the initializer below with aligned AVX moves and faults otherwise.

	Buffer buf = Buffer_createNull();

	gotoIfError3(clean, Buffer_createUninitializedBytesAligned(
		sizeof(CompilerFileCtx), alignof(CompilerFileCtx), 0, alloc, &buf, e_rr
	));

	ctx = (CompilerFileCtx*) buf.ptrNonConst;
	*ctx = (CompilerFileCtx) {
		.job = job,
		.runtimeEntries = runtimeEntries,
		.compileCombinations = compileCombinations,
		.shFile = shFile,
		.binaryType = binaryType
	};

	runtimeEntries = (ListSHEntryRuntime) { 0 };        //Moved
	compileCombinations = (ListU32) { 0 };              //Moved
	shFile = (SHFile) { 0 };                            //Moved

	//From here the context is owned by the file latch: finalize frees it on success,
	// dataDestructor (CompilerFileCtx_free) on failure or shutdown-discard.
	//If create/enter fails no tokens exist yet, so free it directly (cleanCtx).

	gotoIfError3(cleanCtx, JobGroup_create(
		&ctx->group, queue, Compiler_finalizeShaderFile, ctx, CompilerFileCtx_free, e_rr
	));

	gotoIfError3(cleanCtx, JobGroup_enter(&ctx->group, 1, e_rr));    //Self token held while spawning
	enteredSelf = true;

	for(U64 j = 0; j < ctx->compileCombinations.length; ++j) {

		U16 runtimeEntryId = (U16) (ctx->compileCombinations.ptr[j] >> 16);
		U16 combinationId  = (U16) ctx->compileCombinations.ptr[j];

		Bool isRt = combinationId >> 15;
		Bool isGfxOrComp = runtimeEntryId >> 15;

		runtimeEntryId &= (U16) I16_MAX;
		combinationId  &= (U16) I16_MAX;

		//Skip compiling this combination entirely if the entry's stage / extensions can't be expressed
		// on the backend we're compiling for (e.g. a workgraph on SPIRV, or an inline-SPIRV atomic on DXIL).
		//This prevents a guaranteed compile failure.
		//Only the stage/extension support is checked here (not the [[oxc::binary(...)]] annotation),
		// because it's identical for every entrypoint sharing this compile.
		//The annotation is applied per-entrypoint later at link time (Compiler_getLinkEntries).

		if (!((SHEntryRuntime_getSupportedBinaryTypes(&ctx->runtimeEntries.ptr[runtimeEntryId]) >> binaryType) & 1))
			continue;

		Buffer cbuf = Buffer_createNull();
		gotoIfError3(cleanSpawn, Buffer_createUninitializedBytes(sizeof(CompilerComboCtx), alloc, &cbuf, e_rr));

		CompilerComboCtx *combo = (CompilerComboCtx*) cbuf.ptrNonConst;
		*combo = (CompilerComboCtx) {
			.file = ctx,
			.alloc = alloc,
			.runtimeEntryId = runtimeEntryId,
			.combinationId = combinationId,
			.isRt = isRt,
			.isGfxOrComp = isGfxOrComp
		};

		gotoIfError3(cleanCombo, JobGroup_enter(&ctx->group, 1, e_rr));      //File token for this combination

		if(!JobQueue_pushDestructor(queue, Compiler_compileCombinationJob, combo, CompilerComboCtx_free, e_rr)) {
			Error e2 = Error_none();
			JobGroup_leave(&ctx->group, &e2);       //Undo this combination's token
			CompilerComboCtx_free(combo);
			retError(cleanSpawn, Error_invalidState(0, "Compiler_compileShaderFile() couldn't push combination job"));
		}

		continue;

	cleanCombo:
		CompilerComboCtx_free(combo);
		goto cleanSpawn;
	}

	enteredSelf = false;
	{
		Error e2 = Error_none();
		JobGroup_leave(&ctx->group, &e2);           //Release the self token; the tree now completes on its own
	}
	return true;

cleanSpawn:

	//Error mid-spawn after the latch went active: fail it, release the self token,
	// let the pushed combinations drain and reclaim the context via finalize/dataDestructor.
	{
		Error e2 = Error_none();
		JobGroup_fail(&ctx->group, &e2);
		if(enteredSelf)
			JobGroup_leave(&ctx->group, &e2);
	}
	return false;

cleanCtx:

	//Latch create/enter failed: no tokens exist, so reclaim the context directly.
	CompilerFileCtx_free(ctx);
	ctx = NULL;

clean:

	SHFile_free(&shFile, alloc);
	ListSHEntryRuntime_freeUnderlying(&runtimeEntries, alloc);
	ListU32_free(&compileCombinations, alloc);

	return s_uccess;
}

//JobQueue entrypoint; a thin wrapper that spawns the file's fan-out tree and logs any spawn error.
//The file's actual success/result is produced asynchronously by the file latch's finalize.

Bool Compiler_compileShaderFileJob(void *data, U64 threadId, JobQueue *queue) {

	CompilerShaderFileJob *job = (CompilerShaderFileJob*) data;

	if(!job)
		return false;

	Error errTmp = Error_none();
	Bool spawned = Compiler_compileShaderFile(job, queue, threadId, &errTmp);

	if(!spawned && job->enableLogging)
		Error_print(job->alloc, &errTmp, ELogLevel_Error, ELogOptions_Default);

	return spawned;
}

Bool Compiler_registerShaderBinary(
	SHFile *shFile,
	CompileResult *tempResult,
	ESHBinaryType compileMode,
	CharString sourceFile,
	const SHEntryRuntime *runtimeEntry,
	const SHBinaryIdentifier *binaryIdentifier,        //Make sure this binary identifier only contains references
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	CharString tempStr = CharString_createNull();
	SHInclude shInclude = (SHInclude) { 0 };
	SHBinaryInfo binaryInfo = (SHBinaryInfo) { 0 };

	if(tempResult->type != ECompileResultType_Binary)
		retError(clean, Error_invalidState(0, "Compiler_registerShaderBinary() should return binary"));

	gotoIfError3(clean, SHEntryRuntime_asBinaryInfo(
		runtimeEntry, binaryIdentifier, compileMode, tempResult->binary, tempResult->demotion, &binaryInfo, e_rr
	));

	//Add info regarding includes.
	//Merge includes, since different entrypoints can have different includes

	for(U64 k = 0; k < tempResult->includeInfo.length; ++k) {

		IncludeInfo *includeInfok = &tempResult->includeInfo.ptrNonConst[k];
		shInclude = (SHInclude) {
			.crc32c = includeInfok->crc32c,
			.relativePath = includeInfok->file
		};

		includeInfok->file = CharString_createNull();

		//Make sure our includes are relative to source, rather than absolute.
		//Otherwise it's not reproducible

		if (!CharString_startsWithSensitive(shInclude.relativePath, '@', 0)) {

			gotoIfError3(clean, File_makeRelative(
				Platform_instance->defaultDir, sourceFile, shInclude.relativePath, 256, alloc, &tempStr, e_rr
			));

			CharString_free(&shInclude.relativePath, alloc);
			shInclude.relativePath = tempStr;
			tempStr = CharString_createNull();
		}

		gotoIfError3(clean, SHFile_addInclude(shFile, &shInclude, alloc, e_rr));
	}

	//Move binary there to avoid copying mem if possible.
	//Ownership is handed to binaryInfo BEFORE the fallible SHFile_addBinary, not after: if the add fails and
	//jumps to clean, binaryInfo is then the sole owner and SHBinaryInfo_free releases the binary and registers
	//exactly once. Nulling tempResult after the add (as before) left both binaryInfo and tempResult owning the
	//same blocks on the failure path, so SHBinaryInfo_free here plus the caller's CompileResult_free double
	//freed them. On success addBinary moves them out of binaryInfo, so this stays a single free either way.

	binaryInfo.registers = tempResult->registers;
	binaryInfo.binaries[compileMode] = tempResult->binary;
	tempResult->binary = Buffer_createNull();
	tempResult->registers = (ListSHRegisterRuntime) { 0 };

	gotoIfError3(clean, SHFile_addBinary(shFile, &binaryInfo, alloc, e_rr));

	CompileResult_free(tempResult, alloc);

clean:
	SHInclude_free(&shInclude, alloc);
	SHBinaryInfo_free(&binaryInfo, alloc);
	CharString_free(&tempStr, alloc);
	return s_uccess;
}

Bool Compiler_registerShaderEntries(
	SHFile *shFile,
	const ListSHEntryRuntime *entries,
	ListU32 binaryIndices,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListU16 binaryIndicesShort = (ListU16) { 0 };

	for (U64 j = 0, k = 0; j < entries->length; ++j) {

		SHEntryRuntime *runtime = &entries->ptrNonConst[j];

		U32 l = SHEntryRuntime_getCombinations(runtime);

		//Skip missing binaries

		if (k == binaryIndices.length)
			break;

		if ((binaryIndices.ptr[k] >> 16) != j)
			continue;

		//Validate that [k, k + l> is valid (points to same binary)

		if(k + l > binaryIndices.length)
			retError(clean, Error_outOfBounds(
				0, k + l, binaryIndices.length,
				"CLI_compileShader() runtime accessed binaryIndices out of bounds"
			));

		if((binaryIndices.ptr[k + l - 1] >> 16) != j)
			retError(clean, Error_invalidState(0, "CLI_compileShader() has missing binaries for index j"));

		if(runtime->entry.binaryIds.length)
			retError(clean, Error_invalidOperation(
				0, "CLI_compileShader() runtime already included binaryIds"
			));

		gotoIfError3(clean, ListU16_resize(&binaryIndicesShort, l, alloc, e_rr));

		for(U64 m = 0; m < l; ++m)
			binaryIndicesShort.ptrNonConst[m] = (U16) binaryIndices.ptr[k + m];

		runtime->entry.binaryIds = ListU16_createRefFromList(binaryIndicesShort);
		gotoIfError3(clean, SHFile_addEntrypoint(shFile, &runtime->entry, alloc, e_rr));

		k += l;
	}

clean:
	ListU16_free(&binaryIndicesShort, alloc);
	return s_uccess;
}

Bool Compiler_compileShaders(
	const ListCharString *allFiles,
	const ListCharString *allShaderText,
	const ListCharString *allOutputs,
	const ListU8 *allCompileOutputs,
	U64 threadCount,
	Bool isDebug,
	Bool keepRegisters,
	ECompilerWarning extraWarnings,
	Bool ignoreEmptyFiles,
	ECompileType compileType,
	const ListCharString *includeDirs,
	Bool enableLogging,
	const Allocator *alloc,
	ListBuffer *allBuffers,
	Error *e_rr
) {
	(void) compileType;

	Bool s_uccess = true;

	JobQueue queue = (JobQueue) { 0 };
	ListCompiler compilers = (ListCompiler) { 0 };
	ListCompilerShaderFileJob jobs = (ListCompilerShaderFileJob) { 0 };

	SHFile previous = (SHFile) { 0 };       //Accumulates SHFiles that share the same output
	Buffer temp = Buffer_createNull();
	Bool errorInPrevious = false;

	MemoryStreamRef *ms = NULL;
	const RefPtrType msType = MemoryStream_makeType(alloc);
	const RefPtrType fileHandleType = FileHandle_makeType(alloc);

	if(allBuffers)
		gotoIfError3(clean, ListBuffer_resize(allBuffers, allOutputs->length, alloc, e_rr));

	//All compiles run as per file jobs on a JobQueue.
	//threadCount <= 1 puts the queue in single threaded mode: no threads are spawned and all jobs run inline
	// (in push order) during JobQueue_wait, which keeps a deterministic flow around for debugging.
	//Higher counts run the same jobs on threadCount execution contexts.

	gotoIfError3(clean, JobQueue_create(threadCount, alloc, &queue, e_rr));

	const U64 contexts = JobQueue_threadCount(&queue);

	//A separate Compiler per execution context, indexed by the job's threadId.

	gotoIfError3(clean, ListCompiler_resize(&compilers, contexts, alloc, e_rr));

	for(U64 i = 0; i < contexts; ++i)
		gotoIfError3(clean, Compiler_create(alloc, &compilers.ptrNonConst[i], e_rr));

	//Kick off one job per file.
	//Jobs only write to their own slot, so no locking is needed.
	//The jobs list is stable for the queue's lifetime (resized up front, never touched after).

	gotoIfError3(clean, ListCompilerShaderFileJob_resize(&jobs, allFiles->length, alloc, e_rr));

	for (U64 i = 0; i < allFiles->length; ++i) {

		jobs.ptrNonConst[i] = (CompilerShaderFileJob) {

			.allFiles = *allFiles,
			.allShaderText = *allShaderText,
			.allCompileOutputs = *allCompileOutputs,
			.compilers = compilers,
			.includeDirs = *includeDirs,
			.alloc = alloc,
			.fileId = i,

			.isDebug = isDebug,
			.keepRegisters = keepRegisters,
			.ignoreEmptyFiles = ignoreEmptyFiles,
			.enableLogging = enableLogging
		};

		gotoIfError3(clean, JobQueue_push(&queue, Compiler_compileShaderFileJob, &jobs.ptrNonConst[i], e_rr));
	}

	gotoIfError3(clean, JobQueue_wait(&queue, e_rr));

	if(!JobQueue_isSuccess(&queue))
		s_uccess = false;       //Report failure, but still write the outputs that did succeed

	//Combine SHFiles that share the same output (e.g. DXIL + SPIRV into a single oiSH) and write them to allBuffers or disk.
	//Files with the same output are adjacent.
	//This stays sequential on purpose; it's cheap compared to compiling and merging is ordered.

	for (U64 i = 0; i < allFiles->length; ++i) {

		CompilerShaderFileJob *job = &jobs.ptrNonConst[i];

		const Bool lastOfGroup =
			i + 1 == allOutputs->length ||
			!CharString_equalsStringSensitive(&allOutputs->ptr[i + 1], &allOutputs->ptr[i]);

		if(!job->success)
			errorInPrevious = true;

		//Merge into the group's accumulator (empty results come from ignored empty files)

		else if (job->result.entries.ptr) {

			if (!previous.entries.ptr) {
				previous = job->result;
				job->result = (SHFile) { 0 };
			}

			else {
				SHFile tmp = (SHFile) { 0 };
				gotoIfError3(clean, SHFile_combine(&previous, &job->result, alloc, &tmp, e_rr));
				SHFile_free(&previous, alloc);
				SHFile_free(&job->result, alloc);
				previous = tmp;
			}
		}

		if(!lastOfGroup)
			continue;

		//Finish up the group's SHFile and write it

		if(errorInPrevious) {
			if(enableLogging)
				Log_warnLn(alloc, "One of the previous oiSH compilations failed, not producing a binary");
		}

		else if (previous.entries.ptr) {

			if(extraWarnings)
				gotoIfError3(clean, Compiler_handleExtraWarnings(&previous, extraWarnings, alloc, e_rr));

			//Serialize through a resizable memory stream, then hand the buffer to the caller or disk

			U64 writeOff = 0;
			gotoIfError3(clean, MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &msType, &ms, e_rr));
			gotoIfError3(clean, SHFile_write((StreamRef*)ms, &writeOff, &previous, alloc, e_rr));
			gotoIfError3(clean, MemoryStream_move(&ms, &temp, e_rr));
			RefPtr_dec(&ms);

			if(allBuffers) {
				allBuffers->ptrNonConst[i] = temp;
				temp = Buffer_createNull();     //Moved
			}

			else {
				gotoIfError3(clean, File_write(&temp, &allOutputs->ptr[i], 0, 0, 100 * MS, true, &fileHandleType, e_rr));
				Buffer_free(&temp, alloc);
			}
		}

		SHFile_free(&previous, alloc);
		errorInPrevious = false;
	}

clean:

	JobQueue_free(&queue);      //Must go first; jobs reference compilers and the jobs list

	for(U64 i = 0; i < jobs.length; ++i)
		SHFile_free(&jobs.ptrNonConst[i].result, alloc);

	ListCompilerShaderFileJob_free(&jobs, alloc);
	ListCompiler_freeUnderlying(&compilers, alloc);

	RefPtr_dec(&ms);
	SHFile_free(&previous, alloc);
	Buffer_free(&temp, alloc);

	return s_uccess;
}
