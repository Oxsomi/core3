/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/ext/listx_impl.h"
#include "types/buffer.h"
#include "types/thread.h"
#include "types/math.h"
#include "types/time.h"
#include "platforms/file.h"
#include "platforms/log.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/threadx.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/errorx.h"
#include "shader_compiler/compiler.h"
#include "cli.h"

#ifdef CLI_SHADER_COMPILER

	typedef struct ShaderFileRecursion {

		ListCharString *allShaders;
		ListCharString *allOutputs;
		ListU8 *allModes;

		CharString base, output;

		U64 compileModeU64;

		Bool hasMultipleModes;
		Bool hasCombineFlag;
		U8 padding[2];

		ECompileType compileType;

	} ShaderFileRecursion;

	const C8 *txtSuffix = ".txt";				//Suffix when mode is "includes" (seeing all include info)
	const C8 *oiSHCombineSuffix = ".oiSH";		//Suffix when oiSH is combined

	const C8 *fileSuffixes[] = {
		".spv.hlsl",
		".dxil.hlsl"
	};

	const C8 *oiSHSuffixes[] = {
		".spv.oiSH",
		".dxil.oiSH"
	};

	Bool registerFile(FileInfo file, ShaderFileRecursion *shaderFiles, Error *e_rr) {

		Bool isPreprocess = shaderFiles->compileType == ECompileType_Preprocess;

		Bool s_uccess = true;
		CharString copy = CharString_createNull();
		CharString tempStr = CharString_createNull();

		if (file.type == EFileType_File) {

			CharString hlsl = CharString_createRefCStrConst(".hlsl");

			if (CharString_endsWithStringInsensitive(file.path, hlsl, 0)) {

				gotoIfError2(clean, CharString_createCopyx(file.path, &copy))

				//Move to allShaders

				gotoIfError2(clean, ListCharString_pushBackx(shaderFiles->allShaders, copy))
				copy = CharString_createNull();

				//Grab subPath

				CharString subPath = CharString_createNull();

				if(!CharString_cut(file.path, CharString_length(shaderFiles->base), 0, &subPath))
					retError(clean, Error_invalidState(0, "registerFile() couldn't get subPath"))

				//Copy subPath

				gotoIfError2(clean, CharString_createCopyx(subPath, &copy))

				//Move subPath into new folder

				gotoIfError2(clean, CharString_insertStringx(&copy, shaderFiles->output, 0))

				//Move output file to allOutputs, unless it needs to be renamed

				if(!shaderFiles->hasMultipleModes && isPreprocess) {
					gotoIfError2(clean, ListCharString_pushBackx(shaderFiles->allOutputs, copy))
					copy = CharString_createNull();
				}

				//Handle multiple modes by inserting .spv.hlsl at the end

				Bool foundFirstMode = false;

				for(U8 i = 0; i < ESHBinaryType_Count; ++i) {

					if(!((shaderFiles->compileModeU64 >> i) & 1))
						continue;

					gotoIfError2(clean, ListU8_pushBackx(shaderFiles->allModes, i))

					//Add double reference to input, so we don't waste memory (besides 24 for CharString struct itself)
					//Because we want to compile it with two different modes
					//The first mode already added one

					if(foundFirstMode) {

						CharString input = *ListCharString_last(*shaderFiles->allShaders);
						input = CharString_createRefStrConst(input);

						gotoIfError2(clean, ListCharString_pushBackx(shaderFiles->allShaders, input))
					}

					//Append .spv.hlsl and .dxil.hlsl at the end

					if(shaderFiles->hasMultipleModes || !isPreprocess) {

						gotoIfError2(clean, CharString_formatx(
							&tempStr, "%.*s%s",
							(int)U64_min(
								CharString_length(copy),
								CharString_findLastStringInsensitive(copy, CharString_createRefCStrConst(".hlsl"), 0, 0)
							),
							copy.ptr,
							isPreprocess ? fileSuffixes[i] : (
								shaderFiles->compileType == ECompileType_Includes || 
								shaderFiles->compileType == ECompileType_Symbols ? txtSuffix : (
									shaderFiles->hasCombineFlag ? oiSHCombineSuffix : oiSHSuffixes[i]
								)
							)
						))

						gotoIfError3(clean, File_add(tempStr, EFileType_File, 1 * MS, true, e_rr))
						gotoIfError2(clean, ListCharString_pushBackx(shaderFiles->allOutputs, tempStr))
						tempStr = CharString_createNull();
					}

					foundFirstMode = true;
				}

				CharString_freex(&copy);
			}
		}

	clean:
		CharString_freex(&copy);
		CharString_freex(&tempStr);
		return s_uccess;
	}

	Bool CLI_precompileShaderSingle(
		Compiler compiler,
		ESHBinaryType binaryType,
		Bool isDebug,
		CharString inputPath,
		CharString input,
		CharString outputPath,
		ECompileType compileType,
		ListSHEntryRuntime *shEntriesRuntime,
		CharString includeDir
	) {

		Bool isPreprocess = compileType == ECompileType_Preprocess || compileType == ECompileType_Includes;

		CompilerSettings settings = (CompilerSettings) {
			.string = input,
			.path = inputPath,
			.debug = isDebug,
			.format = ECompilerFormat_HLSL,
			.outputType = binaryType,
			.infoAboutIncludes = compileType == ECompileType_Includes,
			.includeDir = includeDir
		};

		//First we need to go from text with includes and defines to easy to parse text

		Error errTemp = Error_none(), *e_rr = &errTemp;
		Bool s_uccess = true;

		CompileResult compileResult = (CompileResult) { 0 };
		CharString tempStr = CharString_createNull();
		CharString tempStr2 = CharString_createNull();
		gotoIfError3(clean, Compiler_preprocessx(compiler, settings, &compileResult, e_rr))

		for(U64 i = 0; i < compileResult.compileErrors.length; ++i) {

			CompileError e = compileResult.compileErrors.ptr[i];

			if((e.typeLineId >> 7) == ECompileErrorType_Warn)
				Log_warnLnx("%s:%"PRIu32":%"PRIu8": %s", e.file.ptr, CompileError_lineId(e), e.lineOffset, e.error.ptr);

			else Log_errorLnx("%s:%"PRIu32":%"PRIu8": %s", e.file.ptr, CompileError_lineId(e), e.lineOffset, e.error.ptr);
		}

		//Then, we need to parse the text for entrypoints and other misc info that will be important

		if (compileResult.isSuccess && !isPreprocess) {

			tempStr = compileResult.text;
			compileResult.text = CharString_createNull();
			CompileResult_freex(&compileResult);

			settings.string = tempStr;

			gotoIfError3(clean, Compiler_parsex(compiler, settings, compileType == ECompileType_Symbols, &compileResult, e_rr))
		}

		//Write final compile result

		if (compileResult.isSuccess) {

			//Tell oiSH entries to caller

			if (
				compileResult.type == ECompileResultType_SHEntryRuntime && 
				shEntriesRuntime &&
				compileResult.shEntriesRuntime.length
			) {

				//Move list with all allocated memory

				*shEntriesRuntime = compileResult.shEntriesRuntime;
				compileResult.shEntriesRuntime = (ListSHEntryRuntime) { 0 };

				//Move as copy, since these are refs to the input file, which will be freed at end of function

				for (U64 i = 0; i < shEntriesRuntime->length; ++i) {

					SHEntryRuntime *entry = &shEntriesRuntime->ptrNonConst[i];

					//Copy uniform names if needed

					for (U64 j = 0; j < entry->uniformNameValues.length; ++j) {

						CharString *curr = &entry->uniformNameValues.ptrNonConst[j];
						CharString temp = CharString_createNull();

						if(!CharString_isRef(*curr))
							continue;

						gotoIfError2(clean, CharString_createCopyx(*curr, &temp))
						*curr = temp;
					}

					//Copy name if needed

					if(!CharString_isRef(entry->entry.name))
						continue;

					CharString temp = CharString_createNull();
					gotoIfError2(clean, CharString_createCopyx(entry->entry.name, &temp))
					entry->entry.name = temp;
				}
			}

			//Now our preprocessed blob is ready to free

			CharString_freex(&tempStr);

			//Output has to be created as a text file that explains info about the includes

			if (compileType == ECompileType_Includes) {

				gotoIfError3(clean, ListIncludeInfo_stringifyx(compileResult.includeInfo, &tempStr, e_rr))

				//Info about the source file

				gotoIfError2(clean, CharString_formatx(
					&tempStr2,
					"\nSources:\n%08"PRIx32" %05"PRIu32" %s\n",
					Buffer_crc32c(CharString_bufferConst(input)), (U32) CharString_length(input), inputPath.ptr
				))

				gotoIfError2(clean, CharString_appendStringx(&tempStr, tempStr2))
				CharString_freex(&tempStr2);

				gotoIfError3(clean, File_write(CharString_bufferConst(tempStr), outputPath, 10 * MS, e_rr))
			}

			//Otherwise we can simply output preprocessed blob

			else if(compileResult.type == ECompileResultType_Text)
				gotoIfError3(clean, File_write(CharString_bufferConst(compileResult.text), outputPath, 10 * MS, e_rr))

			else if(compileResult.type == ECompileResultType_Binary)
				gotoIfError3(clean, File_write(compileResult.binary, outputPath, 10 * MS, e_rr))
		}

	clean:
		s_uccess &= compileResult.isSuccess;

		if(!s_uccess)
			ListSHEntryRuntime_freeUnderlyingx(shEntriesRuntime);

		CompileResult_freex(&compileResult);
		CharString_freex(&tempStr);
		CharString_freex(&tempStr2);
		Error_printx(errTemp, ELogLevel_Error, ELogOptions_Default);
		return s_uccess;
	}

	Bool Compiler_getUniqueCompiles(
		ListSHEntryRuntime runtimeEntries,
		ListU32 *compileCombinations,
		ListU16 *binaryIndices,
		Error *e_rr
	) {

		Bool s_uccess = true;
		ListSHBinaryIdentifier identifiers = (ListSHBinaryIdentifier) { 0 };

		//Go through each combination

		for (U64 i = 0; i < runtimeEntries.length; ++i) {

			if(i >> 16)
				retError(clean, Error_overflow(0, i, 1 << 16, "Compiler_getUniqueCompiles() i out of bounds"))

			SHEntryRuntime runtime = runtimeEntries.ptr[i];

			for (U64 j = 0; j < SHEntryRuntime_getCombinations(runtime); ++j) {

				if(j >> 16)
					retError(clean, Error_overflow(1, j, 1 << 16, "Compiler_getUniqueCompiles() j out of bounds"))

				SHBinaryIdentifier binaryIdentifier = (SHBinaryIdentifier) { 0 };
				gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(runtime, (U16) j, &binaryIdentifier, e_rr))

				//Find SHBinaryIdentifier or not

				U64 k = 0;

				for(; k < identifiers.length; ++k)
					if(SHBinaryIdentifier_equals(binaryIdentifier, identifiers.ptr[k]))
						break;
				
				//When it's new, we gotta remember the binary identifier for reuse.
				//Other than that, we need to tell the compiler which runtime id and combination id we need

				if(k == identifiers.length) {

					gotoIfError2(clean, ListSHBinaryIdentifier_pushBackx(&identifiers, binaryIdentifier))

					if(compileCombinations)
						gotoIfError2(clean, ListU32_pushBackx(compileCombinations, (U32)(j | (i << 16))))
				}

				//Insert id of found binary or newly inserted

				if(binaryIndices)
					gotoIfError2(clean, ListU16_pushBackx(binaryIndices, (U16)k))
			}
		}

	clean:
		//We will never allocate nested memory into this, so it's fine to just freex it (no underlying data)
		ListSHBinaryIdentifier_freex(&identifiers);
		return s_uccess;
	}

	Bool CLI_compileShaderSingle(
		Compiler compiler,
		ESHBinaryType binaryType,
		Bool isDebug,
		CharString inputPath,
		CharString input,
		CompileResult *dest,
		SpinLock *lock,						//When modifying/reading entry
		ListSHEntryRuntime runtimeEntries,
		U16 runtimeEntryId,
		U16 combinationId,
		CharString includeDir
	) {

		Error errTemp = Error_none(), *e_rr = &errTemp;
		Bool s_uccess = true;

		if(!dest)
			retError(clean, Error_nullPointer(5, "CLI_compileShaderSingle()::dest is required"));

		if(dest->binary.ptr || dest->compileErrors.ptr || dest->includeInfo.ptr)
			retError(clean, Error_invalidParameter(5, 0, "CLI_compileShaderSingle()::dest was present, but not empty"))

		CompilerSettings settings = (CompilerSettings) {
			.string = input,
			.path = inputPath,
			.debug = isDebug,
			.format = ECompilerFormat_HLSL,
			.outputType = binaryType,
			.infoAboutIncludes = true,		//Required to supply oiSH info about includes
			.includeDir = includeDir
		};

		//First we need to go from text with includes and defines to easy to parse text
		//Accessing SHEntryRuntime here without locking is safe, since we don't access these properties from asBinaryIdentifier

		SHEntryRuntime entry = runtimeEntries.ptrNonConst[runtimeEntryId];
		SHBinaryIdentifier binaryIdentifier = (SHBinaryIdentifier) { 0 };
		gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(entry, combinationId, &binaryIdentifier, e_rr))
		gotoIfError3(clean, Compiler_compilex(compiler, settings, binaryIdentifier, lock, runtimeEntries, dest, e_rr))

		for(U64 i = 0; i < dest->compileErrors.length; ++i) {

			CompileError e = dest->compileErrors.ptr[i];

			if(e.file.ptr) {

				if((e.typeLineId >> 7) == ECompileErrorType_Warn)
					Log_warnLnx("%s:%"PRIu32":%"PRIu8": %s", e.file.ptr, CompileError_lineId(e), e.lineOffset, e.error.ptr);

				else Log_errorLnx("%s:%"PRIu32":%"PRIu8": %s", e.file.ptr, CompileError_lineId(e), e.lineOffset, e.error.ptr);
			}

			else if((e.typeLineId >> 7) == ECompileErrorType_Warn)
				Log_warnLnx("%s", e.error.ptr);

			else Log_errorLnx("%s", e.error.ptr);
		}

	clean:

		s_uccess &= dest && dest->isSuccess;

		const C8 *binType = binaryType == ESHBinaryType_SPIRV ? "spirv" : "dxil";

		if(s_uccess)
			Log_debugLnx(
				"Compile success: %.*s (%s, %"PRIu32":%"PRIu32")",
				(int) CharString_length(inputPath), inputPath.ptr,
				binType, runtimeEntryId, combinationId
			);

		else 
			Log_errorLnx(
				"Compile failed: %.*s (%s, %"PRIu32":%"PRIu32")",
				(int) CharString_length(inputPath), inputPath.ptr,
				binType, runtimeEntryId, combinationId
			);

		Error_printx(errTemp, ELogLevel_Error, ELogOptions_Default);
		return s_uccess;
	}

	TList(ListSHEntryRuntime);
	TListImpl(ListSHEntryRuntime);

	void ListListSHEntryRuntime_freeUnderlyingx(ListListSHEntryRuntime *entry) {

		if(!entry)
			return;

		for(U64 i = 0; i < entry->length; ++i)
			ListSHEntryRuntime_freeUnderlyingx(&entry->ptrNonConst[i]);

		ListListSHEntryRuntime_freex(entry);
	}

	typedef struct CompilerJobScheduler {

		ListCharString inputPaths;
		ListCharString inputData;
		ListCharString outputPaths;
		ListU8 compileModes;
		ListCompiler compilers;

		ListListSHEntryRuntime *shEntries;
		ListU64 *shEntryIds;				//[U32 shEntries[i], U16 shEntries[i][j], U16 shEntries[i][j][k]]
		ListCompileResult *compileResults;

		SpinLock *lock;
		U64 *counter;						//This one is to acquire job ids
		U64 *completedCounter;				//This one is to signal jobs that are done
		U64 *counterCompiledBinaries;		//How many binaries have been compiled
		U64 *threadCounter;
		Bool *success;

		ECompileType compileType;
		Bool isDebug;
		Bool ignoreEmptyFiles;
		U8 padding[2];

		CharString includeDir;

	} CompilerJobScheduler;

	void CLI_compileShaderJob(CompilerJobScheduler *job) {

		if(!job)
			return;

		Bool s_uccess = true;

		ELockAcquire acq = ELockAcquire_Invalid;
		U64 threadCounter = U64_MAX;

		//Pre-compile

		U64 lastJobId = U64_MAX;

		while(true) {

			//Lock to get next job id

			 acq = SpinLock_lock(job->lock, U64_MAX);

			if(acq < ELockAcquire_Success)
				return;

			//If we had a last job, we have to add completed counter and parse the output.
			//That parsed output is the shEntries and the compiles they have to do.

			if (lastJobId != U64_MAX) {

				//Parse oiSH

				if (job->compileType == ECompileType_Compile) {

					ListSHEntryRuntime runtimeEntries = job->shEntries->ptr[lastJobId];
					ListU32 compileCombinations = (ListU32) { 0 };

					if(!Compiler_getUniqueCompiles(runtimeEntries, &compileCombinations, NULL, NULL))
						Log_errorLnx("Compiler_getUniqueCompiles() failed (Compiler_getUniqueCompiles)");

					//Add all compile combinations, but include our last job id to ensure it can be found again

					else if(compileCombinations.length) {

						if(ListU64_reservex(
							job->shEntryIds, job->shEntryIds->length + compileCombinations.length
						).genericError)
							Log_errorLnx("Compiler_getUniqueCompiles() failed (ListU64_reservex)");

						else if(ListCompileResult_resizex(
							job->compileResults, job->compileResults->length + compileCombinations.length
						).genericError)
							Log_errorLnx("Compiler_getUniqueCompiles() failed (ListCompileResult_resizex)");

						else for (U64 i = 0; i < compileCombinations.length; ++i) {
						
							U32 compileCombination = compileCombinations.ptr[i];

							if (ListU64_pushBack(
								job->shEntryIds, ((U64)lastJobId << 32) | compileCombination, (Allocator) { 0 }
							).genericError) {
								Log_errorLnx("Compiler_getUniqueCompiles() failed (ListU64_pushBack)");
								break;
							}
						}
					}

					else if(!job->ignoreEmptyFiles) {

						Log_errorLnx(
							"Precompile couldn't find entrypoints for file \"%.*s\"",
							(int)CharString_length(job->outputPaths.ptr[lastJobId]), job->outputPaths.ptr[lastJobId].ptr
						);

						s_uccess = false;
					}

					ListU32_freex(&compileCombinations);
				}

				//Mark as completed, so other threads will know

				++*job->completedCounter;
			}

			//Assign thread counter, because it might be needed later

			if (threadCounter == U64_MAX)
				threadCounter = (*job->threadCounter)++;

			//Remember if we succeeded or not

			if(*job->counter == job->inputData.length) {
				*job->success &= s_uccess;
				break;
			}

			U64 ourJobId = (*job->counter)++;

			if(acq == ELockAcquire_Acquired)
				SpinLock_unlock(job->lock);

			acq = ELockAcquire_Invalid;
			lastJobId = ourJobId;

			//Process next. If compile failed, still continue. Because we want all compiler errors

			CharString input = job->inputPaths.ptr[ourJobId];

			if(!CLI_precompileShaderSingle(
				job->compilers.ptr[threadCounter],
				job->compileModes.ptr[ourJobId],
				job->isDebug,
				input,
				job->inputData.ptr[ourJobId],
				job->outputPaths.ptr[ourJobId],
				job->compileType,
				&job->shEntries->ptrNonConst[ourJobId],
				job->includeDir
			)) {
				Log_errorLnx("Compile failed for file \"%.*s\"", (int)CharString_length(input), input.ptr);
				s_uccess = false;
			}
		}

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(job->lock);
		
		acq = ELockAcquire_Invalid;

		//Compile requires another stage where we go over unique binaries that we have to compile

		if (job->compileType == ECompileType_Compile) {

			CompileResult tmp = (CompileResult) { 0 };
			lastJobId = U64_MAX;

			while(true) {

				acq = SpinLock_lock(job->lock, U64_MAX);

				if(acq < ELockAcquire_Success)
					goto cleanTmp;

				//Now that we have the lock, we can write back the result

				if (lastJobId != U64_MAX) {
					job->compileResults->ptrNonConst[lastJobId] = tmp;
					tmp = (CompileResult) { 0 };
				}

				//We're done, since all initial threads are done spawning children.
				//And those children have also finished.

				if(
					*job->completedCounter == job->inputData.length &&
					*job->counterCompiledBinaries == job->shEntryIds->length
				)
					break;

				//We currently don't have any pending, more will likely spawn.
				//Come back a bit later to try again.

				if(*job->counterCompiledBinaries == job->shEntryIds->length) {

					if(acq == ELockAcquire_Acquired)		//Release lock to prevent deadlock
						SpinLock_unlock(job->lock);

					acq = ELockAcquire_Invalid;

					Thread_sleep(100 * MU);
					continue;
				}

				//Grab next job id

				U64 ourJobId = (*job->counterCompiledBinaries)++;
				U64 ourNext = job->shEntryIds->ptr[ourJobId];

				U32 ourOldJobId = ourNext >> 32;
				ListSHEntryRuntime entries = job->shEntries->ptr[ourOldJobId];		//Grab entries as shEntries can be modified

				if(acq == ELockAcquire_Acquired)
					SpinLock_unlock(job->lock);

				acq = ELockAcquire_Invalid;

				//Unpack as ListSHEntryRuntime, then SHEntryRuntime, then the specific job

				U16 runtimeEntryId = (U16)(ourNext >> 16);
				U16 combinationId = (U16) ourNext;

				CharString input = job->inputPaths.ptr[ourOldJobId];

				lastJobId = ourJobId;

				if(!CLI_compileShaderSingle(
					job->compilers.ptr[threadCounter],
					job->compileModes.ptr[ourOldJobId],
					job->isDebug,
					input,
					job->inputData.ptr[ourOldJobId],
					&tmp,
					job->lock,
					entries,
					runtimeEntryId,
					combinationId,
					job->includeDir
				)) {
					s_uccess = false;
					Log_errorLnx("Compile failed for file \"%.*s\"", (int)CharString_length(input), input.ptr);
				}
			}

		cleanTmp:
			CompileResult_freex(&tmp);
		}

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(job->lock);
		
		acq = ELockAcquire_Invalid;
	}

	Bool CLI_registerShaderBinary(
		SHFile *shFile,
		CompileResult *tempResult,
		ESHBinaryType compileMode,
		CharString sourceFile,
		SHEntryRuntime runtimeEntry,
		U16 combinationId,
		Error *e_rr
	) {

		Bool s_uccess = true;
		CharString tempStr = CharString_createNull();
		SHInclude shInclude = (SHInclude) { 0 };
		SHBinaryInfo binaryInfo = (SHBinaryInfo){ 0 };
		
		if(tempResult->type != ECompileResultType_Binary)
			retError(clean, Error_invalidState(0, "CLI_compileShaderSingle() should return binary"))

		gotoIfError3(clean, SHEntryRuntime_asBinaryInfo(
			runtimeEntry, combinationId, compileMode, tempResult->binary, &binaryInfo, e_rr
		))

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

				gotoIfError3(clean, File_makeRelativex(
					sourceFile, shInclude.relativePath, 256, &tempStr, e_rr
				))

				CharString_freex(&shInclude.relativePath);
				shInclude.relativePath = tempStr;
				tempStr = CharString_createNull();
			}

			gotoIfError3(clean, SHFile_addIncludex(shFile, &shInclude, e_rr))
		}

		//Move binary there to avoid copying mem if possible

		binaryInfo.registers = tempResult->registers;
		binaryInfo.binaries[compileMode] = tempResult->binary;
		gotoIfError3(clean, SHFile_addBinaryx(shFile, &binaryInfo, e_rr))
		tempResult->binary = Buffer_createNull();
		tempResult->registers = (ListSHRegisterRuntime) { 0 };

		CompileResult_freex(tempResult);

	clean:
		SHInclude_freex(&shInclude);
		SHBinaryInfo_freex(&binaryInfo);
		CharString_freex(&tempStr);
		return s_uccess;
	}

	Bool CLI_registerShaderEntries(SHFile *shFile, ListSHEntryRuntime entries, ListU16 binaryIndices, Error *e_rr) {

		Bool s_uccess = true;

		for (U64 j = 0, k = 0; j < entries.length; ++j) {

			SHEntryRuntime *runtime = &entries.ptrNonConst[j];

			U32 l = SHEntryRuntime_getCombinations(*runtime);

			if (k + l > binaryIndices.length)
				retError(clean, Error_outOfBounds(
					0, k + l, binaryIndices.length,
					"CLI_compileShader() runtime accessed binaryIndices out of bounds"
				))

			if(runtime->entry.binaryIds.length)
				retError(clean, Error_invalidOperation(
					0, "CLI_compileShader() runtime already included binaryIds"
				))

			ListU16 tempBinaryIds = (ListU16) { 0 };
			gotoIfError2(clean, ListU16_createSubset(binaryIndices, k, l, &tempBinaryIds))

			runtime->entry.binaryIds = tempBinaryIds;
			gotoIfError3(clean, SHFile_addEntrypointx(shFile, &runtime->entry, e_rr))

			k += l;
		}

	clean:
		return s_uccess;
	}

	Bool CLI_parseThreads(ParsedArgs args, U64 *threadCount, U64 defaultThreadCount) {

		if(!threadCount)
			return false;

		U64 maxThreads = Platform_getThreads();

		if(!(args.parameters & EOperationHasParameter_ThreadCount)) {
			*threadCount = !defaultThreadCount ? maxThreads : defaultThreadCount;
			return true;
		}

		CharString str = (CharString) { 0 };
		if(ParsedArgs_getArg(args, EOperationHasParameter_ThreadCountShift, &str).genericError)
			return false;
		
		if(CharString_endsWithSensitive(str, '%', 0)) {					//-threads 50%

			CharString number = CharString_createRefSizedConst(str.ptr, CharString_length(str) - 1, false);
			F64 num = 0;

			if (!CharString_parseDouble(number, &num) || num < 0 || num > 100) {
				Log_errorLnx("Couldn't parse -threads x%, x is expected to be a F64 between (0-100)% or 0 -> threadCount - 1");
				return false;
			}

			*threadCount = (U32) F64_max(1, maxThreads * num / 100);
			return true;
		}

		//-threads x

		U64 num = 0;
		if (!CharString_parseU64(str, &num) || num > maxThreads) {
			Log_errorLnx("Couldn't parse -threads x, where x is expected to be a F64 of (0-100)% or 0 -> threadCount - 1");
			return false;
		}

		*threadCount = (U32)num == 0 ? maxThreads : (U32)num;
		return true;
	}

	Bool CLI_parseCompileTypes(ParsedArgs args, U64 *maskBinaryType, Bool *multipleModes) {

		if(!maskBinaryType || !multipleModes)
			return false;

		if(!(args.parameters & EOperationHasParameter_ShaderOutputMode)) {
			*multipleModes = true;
			*maskBinaryType = (1 << ESHBinaryType_Count) - 1; 
			return true;
		}

		CharString compileMode = CharString_createNull();
		if(ParsedArgs_getArg(args, EOperationHasParameter_ShaderOutputModeShift, &compileMode).genericError)
			return false;

		ListCharString splits = (ListCharString) { 0 };
		
		if(CharString_splitSensitivex(compileMode, ',', &splits).genericError)
			return false;

		CharString modes[] = {
			CharString_createRefCStrConst("spv"),
			CharString_createRefCStrConst("dxil"),
			CharString_createRefCStrConst("all")
		};

		static const U64 modeCount = sizeof(modes) / sizeof(modes[0]);
		*multipleModes = splits.length > 1;
		U64 compileModeU64 = 0;

		for (U64 i = 0; i < splits.length; ++i) {

			Bool match = false;

			for(U64 j = 0; j < modeCount; ++j)
				if (CharString_equalsStringInsensitive(splits.ptr[i], modes[j])) {

					if(j == modeCount - 1) {
						compileModeU64 = (1 << ESHBinaryType_Count) - 1;
						*multipleModes = true;
					}

					else compileModeU64 |= (U64)1 << j;

					match = true;
					break;
				}

			if(!match) {
				compileModeU64 = U64_MAX;
				Log_errorLnx("Couldn't parse -m x, where x is spv, dxil or all (or for example spv,dxil)");
				goto clean;
			}
		}

		*maskBinaryType = compileModeU64;

	clean:
		ListCharString_freex(&splits);
		return compileModeU64 != U64_MAX;
	}

	ECompilerWarning CLI_getExtraWarnings(ParsedArgs args) {

		ECompilerWarning extraWarnings = ECompilerWarning_None;

		if(args.flags & EOperationFlags_CompilerWarnings) {

			if(args.flags & EOperationFlags_WarnUnusedRegisters)
				extraWarnings |= ECompilerWarning_UnusedRegisters;

			if(args.flags & EOperationFlags_WarnUnusedConstants)
				extraWarnings |= ECompilerWarning_UnusedConstants;

			if(args.flags & EOperationFlags_WarnBufferPadding)
				extraWarnings |= ECompilerWarning_BufferPadding;
		}

		return extraWarnings;
	}

	Bool CLI_getCompileTargetsFromFile(
		CharString input,
		ECompileType compileType,
		U64 compileModeU64,
		Bool multipleModes,
		Bool combineFlag,
		Bool *isFolder,
		CharString *output,
		ListCharString *allFiles,
		ListCharString *allShaderText,
		ListCharString *allOutputs,
		ListU8 *allCompileModes
	) {

		Bool s_uccess = true;

		if (!allCompileModes || !allFiles || !allShaderText || !allOutputs) {
			Log_debugLnx("CLI_getCompileTargetsFromFile one of outputs is missing");
			return false;
		}

		CharString resolved = CharString_createNull();
		CharString resolved2 = CharString_createNull();
		CharString tempStr = CharString_createNull();
		Buffer temp = Buffer_createNull();

		Error errTmp = Error_none(), *e_rr = &errTmp;

		//Get all shaders

		if (File_hasFolder(input)) {

			Bool isVirtual;
			gotoIfError3(clean, File_resolvex(input, &isVirtual, 0, &resolved, e_rr))
			gotoIfError2(clean, CharString_appendx(&resolved, '/'))

			if(output) {
				gotoIfError3(clean, File_resolvex(*output, &isVirtual, 0, &resolved2, e_rr))
				gotoIfError2(clean, CharString_appendx(&resolved2, '/'))
			}

			ShaderFileRecursion shaderFileRecursion = (ShaderFileRecursion) {
				.allShaders = allFiles,
				.allOutputs = allOutputs,
				.allModes = allCompileModes,
				.base = resolved,
				.output = resolved2,
				.compileModeU64 = compileModeU64,
				.hasMultipleModes = multipleModes,
				.hasCombineFlag = combineFlag,
				.compileType = compileType
			};

			gotoIfError3(clean, File_foreach(
				input,
				(FileCallback) registerFile,
				&shaderFileRecursion,
				true,
				e_rr
			))

			//Make sure we can have a folder at output

			if(output)
				gotoIfError3(clean, File_add(resolved2, EFileType_Folder, 1 * SECOND, false, e_rr))

			if(isFolder) *isFolder = true;
		}

		//We need to add multiple compile modes

		else for(U8 i = 0; i < ESHBinaryType_Count; ++i) {

			if(!((compileModeU64 >> i) & 1))
				continue;

			//Replace output's .hlsl by .spv.hlsl or .dxil.hlsl

			gotoIfError2(clean, CharString_formatx(
				&tempStr, "%.*s%s",
				output ? (int)U64_min(
					CharString_length(*output),
					CharString_findLastStringInsensitive(*output, CharString_createRefCStrConst(".hlsl"), 0, 0)
				) : (int)(sizeof("output") - 1),
				output ? output->ptr : "output",
				compileType == ECompileType_Preprocess ? fileSuffixes[i] : (
					compileType == ECompileType_Includes || compileType == ECompileType_Symbols ? txtSuffix :
					oiSHSuffixes[i]
				)
			))

			//Register mode and input/output name

			gotoIfError2(clean, ListCharString_pushBackx(allFiles, input))

			gotoIfError2(clean, ListCharString_pushBackx(allOutputs, tempStr))		//Moved here
			tempStr = CharString_createNull();

			gotoIfError2(clean, ListU8_pushBackx(allCompileModes, i))
		}

		//Only continue if there are any files and then fetch all files

		if (!allFiles->length) {
			Log_debugLnx("No files to process");
			goto clean;
		}

		CharString prevStr = CharString_createNull();

		for (U64 i = 0; i < allFiles->length; ++i) {

			//Grab from cache if we're re-compiling the same file with a different mode

			if (CharString_equalsStringSensitive(prevStr, allFiles->ptr[i])) {

				CharString shader = *ListCharString_last(*allShaderText);
				shader = CharString_createRefStrConst(shader);

				gotoIfError2(clean, ListCharString_pushBackx(allShaderText, shader))

				continue;
			}

			//Otherwise grab from file

			gotoIfError3(clean, File_read(allFiles->ptr[i], 10 * MS, &temp, e_rr))

			if(!Buffer_length(temp)) {
				gotoIfError2(clean, ListCharString_pushBackx(allShaderText, CharString_createNull()))
				continue;
			}

			gotoIfError2(clean, CharString_createCopyx(
				CharString_createRefSizedConst((const C8*)temp.ptr, Buffer_length(temp), false), &tempStr
			))

			if(!CharString_eraseAllSensitive(&tempStr, '\r', 0, 0))
				retError(clean, Error_invalidState(1, "CLI_compileShader couldn't erase \\rs"))

			gotoIfError2(clean, ListCharString_pushBackx(allShaderText, tempStr))
			tempStr = CharString_createNull();

			Buffer_freex(&temp);

			prevStr = allFiles->ptr[i];
		}

	clean:
		Error_printx(errTmp, ELogLevel_Error, ELogOptions_Default);
		CharString_freex(&resolved);
		CharString_freex(&resolved2);
		Buffer_freex(&temp);
		CharString_freex(&tempStr);
		return s_uccess;
	}
	
	Bool CLI_compileShaders(
		ListCharString allFiles,
		ListCharString allShaderText,
		ListCharString allOutputs,
		ListU8 allCompileOutputs,
		U64 threadCount,
		Bool isDebug,
		ECompilerWarning extraWarnings,
		Bool ignoreEmptyFiles,
		ECompileType compileType,
		CharString includeDir,
		CharString outputDir,
		ListBuffer *allBuffers,
		Error *e_rr
	) {
		ListThread threads = (ListThread) { 0 };
		ListCompiler compilers = (ListCompiler) { 0 };
		ListListSHEntryRuntime shEntries = (ListListSHEntryRuntime) { 0 };

		ListU64 shEntryIds = (ListU64) { 0 };
		ListU64 shEntryIdsSorted = (ListU64) { 0 };
		ListCompileResult compileResults = (ListCompileResult) { 0 };
		CompileResult tempResult = (CompileResult) { 0 };

		Compiler compiler = (Compiler) { 0 };
		ListIncludeInfo includeInfo = (ListIncludeInfo) { 0 };
		ListU32 compileCombinations = (ListU32) { 0 };
		ListU16 binaryIndices = (ListU16) { 0 };
		SHEntry shEntry = (SHEntry) { 0 };

		SHFile shFile = (SHFile) { 0 };
		SHFile previous = (SHFile) { 0 };
		Bool errorInPrevious = false;

		Buffer temp = Buffer_createNull();
		SpinLock lock = (SpinLock) { 0 };

		ListSHEntryRuntime runtimeEntries = (ListSHEntryRuntime) { 0 };

		CharString tempStr = CharString_createNull();
		CharString tempStr2 = CharString_createNull();
		CharString tempStr3 = CharString_createNull();

		Bool s_uccess = true;

		if(allBuffers)
			gotoIfError2(clean, ListBuffer_resizex(allBuffers, allOutputs.length))
	
		//Spin up threads

		if (threadCount > 1) {

			U64 counter = 0, threadCounter = 0, completedCounter = 0, counterCompiledBinaries = 0;

			gotoIfError2(clean, ListThread_resizex(&threads, threadCount))
			gotoIfError2(clean, ListCompiler_resizex(&compilers, threadCount))
			gotoIfError2(clean, ListListSHEntryRuntime_resizex(&shEntries, allFiles.length))

			CompilerJobScheduler jobScheduler = (CompilerJobScheduler) {

				.isDebug = isDebug,
				.ignoreEmptyFiles = ignoreEmptyFiles,
				.inputPaths = allFiles,
				.inputData = allShaderText,
				.outputPaths = allOutputs,
				.compileModes = allCompileOutputs,
				.compilers = compilers,

				.shEntries = &shEntries,
				.shEntryIds = &shEntryIds,
				.compileResults = &compileResults,

				.lock = &lock,
				.counter = &counter,
				.completedCounter = &completedCounter,
				.counterCompiledBinaries = &counterCompiledBinaries,
				.threadCounter = &threadCounter,
				.success = &s_uccess,

				.compileType = compileType,
				.includeDir = includeDir
			};

			for(U64 i = 0; i < threadCount; ++i) {

				gotoIfError3(clean, Compiler_createx(&compilers.ptrNonConst[i], e_rr))

				gotoIfError2(clean, Thread_createx(
					(ThreadCallbackFunction) CLI_compileShaderJob, &jobScheduler, &threads.ptrNonConst[i]
				))
			}

			for(U64 i = 0; i < threadCount; ++i)
				Thread_waitAndCleanupx(&threads.ptrNonConst[i]);

			ListThread_freex(&threads);

			gotoIfError2(clean, ListU64_createCopyx(shEntryIds, &shEntryIdsSorted))
			ListU64_sort(shEntryIdsSorted);

			//Now, we have all results and all entry points.
			//We will now write these files in sync, as that's way easier and the majority of the hard work is done.
			//+1 to allow finishing up the last one in a nicer way

			U32 lastJobId = U32_MAX;

			for(U64 i = 0; i < shEntryIdsSorted.length + 1; ++i) {

				U64 next = i == shEntryIdsSorted.length ? U64_MAX : shEntryIdsSorted.ptr[i];

				U32 jobId = next >> 32;

				//Finish up SHFile and throw it to disk

				if (jobId != lastJobId && lastJobId != U32_MAX && shFile.entries.ptr) {

					gotoIfError3(clean, Compiler_getUniqueCompiles(shEntries.ptr[lastJobId], NULL, &binaryIndices, e_rr))
					gotoIfError3(clean, CLI_registerShaderEntries(&shFile, shEntries.ptr[lastJobId], binaryIndices, e_rr))

					if (
						lastJobId &&
						CharString_equalsStringSensitive(allOutputs.ptr[lastJobId - 1], allOutputs.ptr[lastJobId])
					) {
						SHFile tmp = (SHFile) { 0 };
						gotoIfError3(clean, SHFile_combinex(previous, shFile, &tmp, e_rr))
						SHFile_freex(&previous);
						previous = tmp;
					}

					else errorInPrevious = false;		//Reset error report
					
					if(
						lastJobId + 1 == allOutputs.length ||
						!CharString_equalsStringSensitive(allOutputs.ptr[lastJobId + 1], allOutputs.ptr[lastJobId])
					) {

						if(errorInPrevious)
							Log_warnLnx("One of the previous oiSH compilations failed, not producing a binary");

						else {

							if(extraWarnings)
								gotoIfError3(clean, Compiler_handleExtraWarningsx(
									previous.entries.length ? previous : shFile, extraWarnings, e_rr
								))
							
							if(previous.entries.length)
								gotoIfError3(clean, SHFile_writex(previous, &temp, e_rr))

							else gotoIfError3(clean, SHFile_writex(shFile, &temp, e_rr))

							if(allBuffers) {
								allBuffers->ptrNonConst[lastJobId] = temp;
								temp = Buffer_createNull();		//Moved
							}
							
							else {
								gotoIfError3(clean, File_write(temp, allOutputs.ptr[lastJobId], 100 * MS, e_rr))
								Buffer_freex(&temp);
							}
						}
					}

					else {
						SHFile_freex(&previous);
						previous = shFile;
						shFile = (SHFile) { 0 };
					}

					ListU16_freex(&binaryIndices);
					SHFile_freex(&shFile);
				}

				//In case we just finished

				if (i == shEntryIdsSorted.length)
					break;

				//Make sure we don't create an invalid SHFile

				U64 unsortedI = ListU64_findFirst(shEntryIds, next, 0, NULL);
				CompileResult *compileResult = &compileResults.ptrNonConst[unsortedI];

				if(!compileResult->isSuccess) {
					SHFile_freex(&shFile);
					continue;
				}

				//Start new SHFile

				if (jobId != lastJobId) {
					
					U32 crc32c = Buffer_crc32c(CharString_bufferConst(allShaderText.ptr[jobId]));

					gotoIfError3(clean, SHFile_createx(
						ESHSettingsFlags_None,
						OXC3_VERSION,
						crc32c,
						&shFile,
						e_rr
					))
				}

				//One of the previous has failed

				if(!shFile.entries.ptr) {
					errorInPrevious = true;
					continue;
				}

				//Add binary to SHFile

				U16 runtimeEntryId = (U16)(next >> 16);
				U16 combinationId = (U16) next;

				gotoIfError3(clean, CLI_registerShaderBinary(
					&shFile,
					compileResult,
					allCompileOutputs.ptr[jobId],
					allFiles.ptr[jobId],
					shEntries.ptr[jobId].ptr[runtimeEntryId],
					combinationId,
					e_rr
				))

				lastJobId = jobId;
			}
		}

		else {

			gotoIfError3(clean, Compiler_createx(&compiler, e_rr))

			for (U64 i = 0; i < allFiles.length; ++i) {

				//Preprocess to get information necessary for real compiles.
				//Though this is generally enough info for includes, (most) reflection and preprocessing

				if(!CLI_precompileShaderSingle(
					compiler, allCompileOutputs.ptr[i],
					isDebug,
					allFiles.ptr[i], allShaderText.ptr[i], allOutputs.ptr[i],
					compileType,
					&runtimeEntries,
					includeDir
				)) {
					s_uccess = false;
					Log_errorLnx(
						"Precompile failed for file \"%.*s\"", (int)CharString_length(allFiles.ptr[i]), allFiles.ptr[i].ptr
					);
					goto clean;
				}

				//Finish by going over all individual runtime entries and outputting it as a raw binary

				if (compileType == ECompileType_Compile) {

					if (!runtimeEntries.length) {

						if(!ignoreEmptyFiles) {

							Log_errorLnx(
								"Precompile couldn't find entrypoints for file \"%.*s\"",
								(int)CharString_length(allFiles.ptr[i]), allFiles.ptr[i].ptr
							);

							s_uccess = false;
						}

						ListSHEntryRuntime_freeUnderlyingx(&runtimeEntries);
						continue;
					}

					U32 crc32c = Buffer_crc32c(CharString_bufferConst(allShaderText.ptr[i]));

					gotoIfError3(clean, SHFile_createx(
						ESHSettingsFlags_None,
						OXC3_VERSION,
						crc32c,
						&shFile,
						e_rr
					))

					gotoIfError3(clean, Compiler_getUniqueCompiles(runtimeEntries, &compileCombinations, &binaryIndices, e_rr))

					//Only for non lib entries, and then once per lib entry

					Bool didSucceed = true;
					
					for(U64 j = 0; j < compileCombinations.length; ++j) {

						U16 runtimeEntryId = (U16) (compileCombinations.ptr[j] >> 16);
						U16 combinationId  = (U16) compileCombinations.ptr[j];

						SHEntryRuntime runtimeEntry = runtimeEntries.ptr[runtimeEntryId];

						//Compile and return error if failed

						if(!CLI_compileShaderSingle(
							compiler, allCompileOutputs.ptr[i],
							isDebug,
							allFiles.ptr[i], allShaderText.ptr[i],
							&tempResult,
							NULL,
							runtimeEntries,
							runtimeEntryId,
							combinationId,
							includeDir
						)) {
							s_uccess = false;
							Log_errorLnx(
								"Compile failed for file \"%.*s\"",
								(int)CharString_length(allFiles.ptr[i]), allFiles.ptr[i].ptr
							);

							didSucceed = false;
							CompileResult_freex(&tempResult);
							break;
						}

						//Add binary to SHFile

						else gotoIfError3(clean, CLI_registerShaderBinary(
							&shFile, &tempResult, allCompileOutputs.ptr[i], allFiles.ptr[i], runtimeEntry, combinationId, e_rr
						))
					}

					//Link entrypoint to binaries

					if (didSucceed)
						gotoIfError3(clean, CLI_registerShaderEntries(&shFile, runtimeEntries, binaryIndices, e_rr))

					//Write to disk and free temp data

					if (didSucceed) {

						//Merge with previous if present

						if (
							i &&
							CharString_equalsStringSensitive(allOutputs.ptr[i - 1], allOutputs.ptr[i])
						) {
							SHFile tmp = (SHFile) { 0 };
							gotoIfError3(clean, SHFile_combinex(previous, shFile, &tmp, e_rr))
							SHFile_freex(&previous);
							previous = tmp;
						}

						else errorInPrevious = false;		//Reset error report

						if(
							i + 1 == allOutputs.length ||
							!CharString_equalsStringSensitive(allOutputs.ptr[i + 1], allOutputs.ptr[i])
						) {

							if(errorInPrevious)
								Log_warnLnx("One of the previous oiSH compilations failed, not producing a binary");

							else {

								if(extraWarnings)
									gotoIfError3(clean, Compiler_handleExtraWarningsx(
										previous.entries.length ? previous : shFile, extraWarnings, e_rr
									))
							
								if(previous.entries.length)
									gotoIfError3(clean, SHFile_writex(previous, &temp, e_rr))

								else gotoIfError3(clean, SHFile_writex(shFile, &temp, e_rr))
								
								if(allBuffers) {
									allBuffers->ptrNonConst[i] = temp;
									temp = Buffer_createNull();		//Moved
								}
							
								else {
									gotoIfError3(clean, File_write(temp, allOutputs.ptr[i], 100 * MS, e_rr))
									Buffer_freex(&temp);
								}
							}
						}

						else {
							SHFile_freex(&previous);
							previous = shFile;
							shFile = (SHFile) { 0 };
						}
					}

					else errorInPrevious = true;

					ListU32_freex(&compileCombinations);
					ListU16_freex(&binaryIndices);
					SHFile_freex(&shFile);
				}

				ListSHEntryRuntime_freeUnderlyingx(&runtimeEntries);
			}
		}

		if(!s_uccess)
			goto clean;

		//Merge all include info into a root.txt file.

		if (compileType == ECompileType_Includes && CharString_length(outputDir) && !allBuffers) {

			if(compiler.interfaces[0])
				gotoIfError3(clean, Compiler_mergeIncludeInfox(&compiler, &includeInfo, e_rr))

			else for(U64 i = 0; i < compilers.length; ++i)
				gotoIfError3(clean, Compiler_mergeIncludeInfox(&compilers.ptrNonConst[i], &includeInfo, e_rr))

			//Sort IncludeInfo

			ListIncludeInfo_sortCustom(includeInfo, (CompareFunction) IncludeInfo_compare);

			//We won't be needing resolved, so we can safely apppend root.txt after it

			gotoIfError2(clean, CharString_createCopyx(outputDir, &tempStr3))

			if(!CharString_endsWithSensitive(outputDir, '/', 0) && !CharString_endsWithSensitive(outputDir, '\\', 0))
				gotoIfError2(clean, CharString_appendx(&tempStr3, '/'))

			gotoIfError2(clean, CharString_appendStringx(&tempStr3, CharString_createRefCStrConst("root.txt")))

			//Make the string

			gotoIfError3(clean, ListIncludeInfo_stringifyx(includeInfo, &tempStr, e_rr))

			//Info about the source files

			gotoIfError2(clean, CharString_appendStringx(&tempStr, CharString_createRefCStrConst("\nSources:\n")))

			for(U64 i = 0; i < allFiles.length; ++i) {

				if(i && allFiles.ptr[i].ptr == allFiles.ptr[i - 1].ptr)		//Easy check, since we re-use string locations :)
					continue;

				gotoIfError2(clean, CharString_formatx(
					&tempStr2,
					"%08"PRIx32" %05"PRIu32" %s\n",
					Buffer_crc32c(CharString_bufferConst(allShaderText.ptr[i])),
					(U32)CharString_length(allShaderText.ptr[i]),
					allFiles.ptr[i].ptr
				))

				gotoIfError2(clean, CharString_appendStringx(&tempStr, tempStr2))
				CharString_freex(&tempStr2);
			}

			gotoIfError3(clean, File_write(CharString_bufferConst(tempStr), tempStr3, 10 * MS, e_rr))
		}

	clean:

		for(U64 i = 0; i < threads.length; ++i)
			Thread_waitAndCleanupx(&threads.ptrNonConst[i]);

		ListThread_freex(&threads);
		ListCompiler_freeUnderlyingx(&compilers);
		Compiler_freex(&compiler);
		ListIncludeInfo_freeUnderlyingx(&includeInfo);
		SpinLock_unlock(&lock);
		Buffer_freex(&temp);

		ListSHEntryRuntime_freeUnderlyingx(&runtimeEntries);
		SHFile_freex(&shFile);
		SHFile_freex(&previous);
		SHEntry_freex(&shEntry);
		ListU32_freex(&compileCombinations);
		ListU16_freex(&binaryIndices);

		ListListSHEntryRuntime_freeUnderlyingx(&shEntries);
		ListU64_freex(&shEntryIds);
		ListU64_freex(&shEntryIdsSorted);
		ListCompileResult_freeUnderlyingx(&compileResults);
		CompileResult_freex(&tempResult);

		CharString_freex(&tempStr);
		CharString_freex(&tempStr2);
		CharString_freex(&tempStr3);

		return s_uccess;
	}

	Bool CLI_compileShader(ParsedArgs args) {

		//Get input

		ListCharString allFiles = (ListCharString) { 0 };
		ListCharString allShaderText = (ListCharString) { 0 };
		ListCharString allOutputs = (ListCharString) { 0 };
		ListU8 allCompileModes = (ListU8) { 0 };

		Error errTemp = Error_none(), *e_rr = &errTemp;
		Bool s_uccess = true;

		Ns start = Time_now();

		CharString input = (CharString) { 0 };
		gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &input))

		CharString output = (CharString) { 0 };
		gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &output))

		//Get compile types

		Bool multipleModes = false;
		U64 compileModeU64 = 0;
		gotoIfError3(clean, CLI_parseCompileTypes(args, &compileModeU64, &multipleModes))

		//Check thread count

		U64 threadCount = 0;
		gotoIfError3(clean, CLI_parseThreads(args, &threadCount, 0))

		//Compile type

		CharString compileTypeStr = (CharString) { 0 };
		ECompileType compileType = ECompileType_Compile;

		if(args.parameters & EOperationHasParameter_ShaderCompileMode) {

			gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_ShaderCompileModeShift, &compileTypeStr))

			if (CharString_equalsStringInsensitive(compileTypeStr, CharString_createRefCStrConst("preprocess")))
				compileType = ECompileType_Preprocess;

			else if (CharString_equalsStringInsensitive(compileTypeStr, CharString_createRefCStrConst("includes")))
				compileType = ECompileType_Includes;

			else if (CharString_equalsStringInsensitive(compileTypeStr, CharString_createRefCStrConst("symbols")))
				compileType = ECompileType_Symbols;

			else if (CharString_equalsStringInsensitive(compileTypeStr, CharString_createRefCStrConst("compile")))
				compileType = ECompileType_Compile;

			else {
				Log_errorLnx("Unknown shader compile mode passed %s", compileTypeStr.ptr);
				s_uccess = false;
				goto clean;
			}
		}

		//Additional includeDir

		CharString includeDir = (CharString) { 0 };

		if (args.parameters & EOperationHasParameter_IncludeDir)
			gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_IncludeDirShift, &includeDir))

		//Grab all files that need compilation

		Bool isFolder = false;
		gotoIfError3(clean, CLI_getCompileTargetsFromFile(
			input,
			compileType,
			compileModeU64,
			multipleModes,
			!(args.flags & EOperationFlags_Split),
			&isFolder,
			&output,
			&allFiles,
			&allShaderText,
			&allOutputs,
			&allCompileModes
		))

		//Grab info about extra detailed compiler warnings

		ECompilerWarning extraWarnings = CLI_getExtraWarnings(args);

		//Compile

		gotoIfError3(clean, CLI_compileShaders(
			allFiles, allShaderText, allOutputs, allCompileModes,
			threadCount,
			args.flags & EOperationFlags_Debug,
			extraWarnings,
			args.flags & EOperationFlags_IgnoreEmptyFiles,
			ECompileType_Compile,
			includeDir,
			isFolder ? output : CharString_createNull(),
			NULL,
			e_rr
		))

	clean:

		F64 dt = (F64)(Time_now() - start) / SECOND;

		if(s_uccess)
			Log_debugLnx("-- Compile %.*s success in %fs", (int)CharString_length(input), input.ptr, dt);

		else Log_errorLnx("-- Compile %.*s failed in %fs!", (int)CharString_length(input), input.ptr, dt);

		Error_printx(errTemp, ELogLevel_Error, ELogOptions_Default);

		ListCharString_freeUnderlyingx(&allFiles);
		ListCharString_freeUnderlyingx(&allShaderText);
		ListCharString_freeUnderlyingx(&allOutputs);
		ListU8_freex(&allCompileModes);

		return s_uccess;
	}
#else
	Bool CLI_compileShader(ParsedArgs args) { (void) args; return false; }
#endif
