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
#include "shader_compiler/compiler.h"
#include "cli.h"

#ifdef CLI_SHADER_COMPILER

	typedef enum ECompileType {
		ECompileType_Preprocess,		//Turns shader with includes & defines into an easily parsable string
		ECompileType_Includes,			//Turns shader with includes into a list of their dependencies (direct + indirect)
		ECompileType_Reflect,			//Reflects all shader info (//TODO:)
		ECompileType_Compile,			//Compile all shaders into an oiSH file for consumption
		ECompileType_Symbols			//List all symbols located in the shader or include as a text file
	} ECompileType;

	typedef struct ShaderFileRecursion {

		ListCharString *allShaders;
		ListCharString *allOutputs;
		ListU8 *allModes;

		CharString base, output;

		U64 compileModeU64;
		Bool hasMultipleModes;

		ECompileType compileType;

	} ShaderFileRecursion;

	const C8 *txtSuffix = ".txt";		//Suffix when mode is "includes" (seeing all include info)

	const C8 *fileSuffixes[] = {
		".spv.hlsl",
		".dxil.hlsl"
	};

	const C8 *oiSHSuffixes[] = {
		".spv.oiSH",
		".dxil.oiSH"
	};

	Error registerFile(FileInfo file, ShaderFileRecursion *shaderFiles) {

		Bool isPreprocess = shaderFiles->compileType == ECompileType_Preprocess;

		Error err = Error_none();
		CharString copy = CharString_createNull();
		CharString tempStr = CharString_createNull();

		if (file.type == EFileType_File) {

			CharString hlsl = CharString_createRefCStrConst(".hlsl");

			if (CharString_endsWithStringInsensitive(file.path, hlsl, 0)) {

				gotoIfError(clean, CharString_createCopyx(file.path, &copy))

				//Move to allShaders

				gotoIfError(clean, ListCharString_pushBackx(shaderFiles->allShaders, copy))
				copy = CharString_createNull();

				//Grab subPath

				CharString subPath = CharString_createNull();

				if(!CharString_cut(file.path, CharString_length(shaderFiles->base), 0, &subPath))
					gotoIfError(clean, Error_invalidState(0, "registerFile() couldn't get subPath"))

				//Copy subPath

				gotoIfError(clean, CharString_createCopyx(subPath, &copy))

				//Move subPath into new folder

				gotoIfError(clean, CharString_insertStringx(&copy, shaderFiles->output, 0))

				//Move output file to allOutputs, unless it needs to be renamed

				if(!shaderFiles->hasMultipleModes && isPreprocess) {
					gotoIfError(clean, ListCharString_pushBackx(shaderFiles->allOutputs, copy))
					copy = CharString_createNull();
				}

				//Handle multiple modes by inserting .spv.hlsl at the end

				Bool foundFirstMode = false;

				for(U8 i = 0; i < ESHBinaryType_Count; ++i) {

					if(!((shaderFiles->compileModeU64 >> i) & 1))
						continue;

					gotoIfError(clean, ListU8_pushBackx(shaderFiles->allModes, i))

					//Add double reference to input, so we don't waste memory (besides 24 for CharString struct itself)
					//Because we want to compile it with two different modes
					//The first mode already added one

					if(foundFirstMode) {

						CharString input = *ListCharString_last(*shaderFiles->allShaders);
						input = CharString_createRefStrConst(input);

						gotoIfError(clean, ListCharString_pushBackx(shaderFiles->allShaders, input))
					}

					//Append .spv.hlsl and .dxil.hlsl at the end

					if(shaderFiles->hasMultipleModes || !isPreprocess) {

						gotoIfError(clean, CharString_formatx(
							&tempStr, "%.*s%s",
							(int)U64_min(
								CharString_length(copy),
								CharString_findLastStringInsensitive(copy, CharString_createRefCStrConst(".hlsl"), 0)
							),
							copy.ptr,
							isPreprocess ? fileSuffixes[i] : (
								shaderFiles->compileType == ECompileType_Includes || shaderFiles->compileType == ECompileType_Symbols ? txtSuffix : oiSHSuffixes[i]
							)
						))

						gotoIfError(clean, File_add(tempStr, EFileType_File, 1 * MS, true))
						gotoIfError(clean, ListCharString_pushBackx(shaderFiles->allOutputs, tempStr))
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
		return err;
	}

	Bool CLI_precompileShaderSingle(
		Compiler compiler,
		ESHBinaryType binaryType,
		ParsedArgs args,
		CharString inputPath,
		CharString input,
		CharString outputPath,
		ECompileType compileType,
		ListSHEntryRuntime *shEntriesRuntime,
		CharString includeDir
	) {

		Bool isPreprocess = compileType == ECompileType_Preprocess || compileType == ECompileType_Includes;

		Bool isDebug = args.flags & EOperationFlags_Debug;

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

				gotoIfError2(clean, File_write(CharString_bufferConst(tempStr), outputPath, 10 * MS))
			}

			//Otherwise we can simply output preprocessed blob

			else if(compileResult.type == ECompileResultType_Text)
				gotoIfError2(clean, File_write(CharString_bufferConst(compileResult.text), outputPath, 10 * MS))

			else if(compileResult.type == ECompileResultType_Binary)
				gotoIfError2(clean, File_write(compileResult.binary, outputPath, 10 * MS))
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
					gotoIfError2(clean, ListU32_pushBackx(compileCombinations, (U32)(j | (i << 16))))
				}

				//Insert id of found binary or newly inserted

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
		ParsedArgs args,
		CharString inputPath,
		CharString input,
		CompileResult *dest,
		SHEntryRuntime runtimeEntry,
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
			.debug = (Bool) (args.flags & EOperationFlags_Debug),
			.format = ECompilerFormat_HLSL,
			.outputType = binaryType,
			.infoAboutIncludes = true,		//Required to supply oiSH info about includes
			.includeDir = includeDir
		};

		//First we need to go from text with includes and defines to easy to parse text

		SHBinaryIdentifier binaryIdentifier = (SHBinaryIdentifier) { 0 };
		gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(runtimeEntry, combinationId, &binaryIdentifier, e_rr))
		gotoIfError3(clean, Compiler_compilex(compiler, settings, binaryIdentifier, dest, e_rr))

		for(U64 i = 0; i < dest->compileErrors.length; ++i) {

			CompileError e = dest->compileErrors.ptr[i];

			if((e.typeLineId >> 7) == ECompileErrorType_Warn)
				Log_warnLnx("%s:%"PRIu32":%"PRIu8": %s", e.file.ptr, CompileError_lineId(e), e.lineOffset, e.error.ptr);

			else Log_errorLnx("%s:%"PRIu32":%"PRIu8": %s", e.file.ptr, CompileError_lineId(e), e.lineOffset, e.error.ptr);
		}

	clean:
		s_uccess &= dest && dest->isSuccess;
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

		ParsedArgs args;

		ListCharString inputPaths;
		ListCharString inputData;
		ListCharString outputPaths;
		ListU8 compileModes;
		ListCompiler compilers;

		ListListSHEntryRuntime *shEntries;
		ListU64 *shEntryIds;				//[U32 shEntries[i], U16 shEntries[i][j], U16 shEntries[i][j][k]]

		Lock *lock;
		U64 *counter;						//This one is to acquire job ids
		U64 *completedCounter;				//This one is to signal jobs that are done
		U64 *counterCompiledBinaries;		//How many binaries have been compiled
		U64 *threadCounter;
		Bool *success;

		ECompileType compileType;
		CharString includeDir;

	} CompilerJobScheduler;

	void CLI_compileShaderJob(CompilerJobScheduler *job) {

		if(!job)
			return;

		//TODO: Multi thread
		/*
		Bool s_uccess = true;

		ELockAcquire acq = ELockAcquire_Invalid;
		U64 threadCounter = U64_MAX;

		//Pre-compile

		U64 lastJobId = U64_MAX;

		while(true) {

			//Lock to get next job id

			 acq = Lock_lock(job->lock, U64_MAX);

			if(acq < ELockAcquire_Success)
				return;

			//If we had a last job, we have to add completed counter and parse the output.
			//That parsed output is the shentries and the compiles they have to do.

			if (lastJobId != U64_MAX) {

				//Parse oiSH

				if (job->compileType == ECompileType_Compile) {

					ListSHEntryRuntime runtimeEntries = job->shEntries->ptr[lastJobId];
					ListU32 compileCombinations = (ListU32) { 0 };

					if(!Compiler_getUniqueCompiles(runtimeEntries, &compileCombinations, NULL))
						Log_errorLnx("Compiler_getUniqueCompiles() failed (Compiler_getUniqueCompiles)");

					//Add all compile combinations, but include our last job id to ensure it can be found again

					else {

						if(ListU64_reservex(
							job->shEntryIds, job->shEntryIds->length + compileCombinations.length
						).genericError)
							Log_errorLnx("Compiler_getUniqueCompiles() failed (ListU64_reservex)");

						else for (U64 i = 0; i < compileCombinations.length; ++i) {
						
							U32 compileCombination = compileCombinations.ptr[i];

							ListU64_pushBack(
								job->shEntryIds, ((U64)lastJobId << 32) | compileCombination, (Allocator) { 0 }
							);
						}
					}
				}

				//Mark as completed, so other threads will know

				++*job->completedCounter;
			}

			//Remember if we succeeded or not

			if(*job->counter == job->inputData.length) {
				*job->success &= s_uccess;
				break;
			}

			U64 ourJobId = (*job->counter)++;

			if (threadCounter == U64_MAX)
				threadCounter = (*job->threadCounter)++;

			if(acq == ELockAcquire_Acquired)
				Lock_unlock(job->lock);

			acq = ELockAcquire_Invalid;
			lastJobId = ourJobId;

			//Process next. If compile failed, still continue. Because we want all compiler errors

			CharString input = job->inputPaths.ptr[ourJobId];

			if(!CLI_precompileShaderSingle(
				job->compilers.ptr[threadCounter],
				job->compileModes.ptr[threadCounter],
				job->args,
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
			Lock_unlock(job->lock);
		
		acq = ELockAcquire_Invalid;

		//Compile requires another stage where we go over unique binaries that we have to compile

		if (job->compileType == ECompileType_Compile) {

			while(true) {

				acq = Lock_lock(job->lock, U64_MAX);

				if(acq < ELockAcquire_Success)
					return;

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
						Lock_unlock(job->lock);

					acq = ELockAcquire_Invalid;

					Thread_sleep(100 * MU);
					continue;
				}

				//Grab next job id

				U64 ourNext = job->shEntryIds->ptr[(*job->counterCompiledBinaries)++];

				U32 ourOldJobId = ourNext >> 32;
				ListSHEntryRuntime entries = job->shEntries->ptr[ourOldJobId];		//Grab entries as shEntries can be modified

				if(acq == ELockAcquire_Acquired)
					Lock_unlock(job->lock);

				acq = ELockAcquire_Invalid;

				//Unpack as ListSHEntryRuntime, then SHEntryRuntime, then the specific job

				U16 runtimeEntryId = (U16)(ourNext >> 16);
				U16 combinationId = (U16) ourNext;

				SHEntryRuntime runtimeEntry = entries.ptr[runtimeEntryId];
				CharString input = job->inputPaths.ptr[ourOldJobId];

				if(!CLI_compileShaderSingle(
					job->compilers.ptr[threadCounter],
					job->compileModes.ptr[threadCounter],
					job->args,
					input,
					job->inputData.ptr[ourOldJobId],
					&temp,
					runtimeEntry,
					combinationId,
					job->includeDir
				)) {
					s_uccess = false;
					Log_errorLnx("Compile failed for file \"%.*s\"", (int)CharString_length(input), input.ptr);
				}
			}
		}

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(job->lock);
		
		acq = ELockAcquire_Invalid;*/
	}

	Bool CLI_compileShader(ParsedArgs args) {

		ECompileType compileType = ECompileType_Compile;

		//Get input

		U64 offset = 0;

		CharString input = (CharString) { 0 };
		CharString output = (CharString) { 0 };

		U64 compileModeU64 = 0;
		CharString compileMode = (CharString) { 0 };

		ListCharString splits = (ListCharString) { 0 };

		ListCharString allFiles = (ListCharString) { 0 };
		ListCharString allShaderText = (ListCharString) { 0 };
		ListCharString allOutputs = (ListCharString) { 0 };
		ListU8 allCompileModes = (ListU8) { 0 };
		ListThread threads = (ListThread) { 0 };
		ListCompiler compilers = (ListCompiler) { 0 };
		Compiler compiler = (Compiler) { 0 };
		ListIncludeInfo includeInfo = (ListIncludeInfo) { 0 };
		CharString resolved = CharString_createNull();
		CharString resolved2 = CharString_createNull();
		CharString tempStr = CharString_createNull();
		CharString tempStr2 = CharString_createNull();
		CharString tempStr3 = CharString_createNull();
		ListSHEntryRuntime runtimeEntries = (ListSHEntryRuntime) { 0 };
		SHFile shFile = (SHFile) { 0 };
		ListU32 compileCombinations = (ListU32) { 0 };
		ListU16 binaryIndices = (ListU16) { 0 };
		SHEntry shEntry = (SHEntry) { 0 };
		SHInclude shInclude = (SHInclude) { 0 };
		SHBinaryInfo binaryInfo = (SHBinaryInfo) { 0 };
		Buffer temp = Buffer_createNull();
		Bool isFolder = false;
		Lock lock = (Lock) { 0 };

		ListListSHEntryRuntime shEntries = (ListListSHEntryRuntime) { 0 };
		ListU64 shEntryIds = (ListU64) { 0 };
		CompileResult tempResult = (CompileResult) { 0 };

		Ns start = Time_now();

		Error errTemp = Error_none(), *e_rr = &errTemp;
		Bool s_uccess = true;

		gotoIfError2(clean, ListCharString_get(args.args, offset++, &input))
		gotoIfError2(clean, ListCharString_get(args.args, offset++, &output))

		Bool multipleModes = true;		//Default to 'all' if no argument is provided
		compileModeU64 = U64_MAX;

		if(args.parameters & EOperationHasParameter_ShaderOutputMode) {

			gotoIfError2(clean, ListCharString_get(args.args, offset++, &compileMode))

			//Grab modes

			gotoIfError2(clean, CharString_splitSensitivex(compileMode, ',', &splits));

			CharString modes[] = {
				CharString_createRefCStrConst("spv"),
				CharString_createRefCStrConst("dxil"),
				CharString_createRefCStrConst("all")
			};

			static const U64 modeCount = sizeof(modes) / sizeof(modes[0]);
			multipleModes = splits.length > 1;
			compileModeU64 = 0;

			for (U64 i = 0; i < splits.length; ++i) {

				Bool match = false;

				for(U64 j = 0; j < modeCount; ++j)
					if (CharString_equalsStringInsensitive(splits.ptr[i], modes[j])) {

						if(j == modeCount - 1) {
							compileModeU64 = U64_MAX;
							multipleModes = true;
						}

						else compileModeU64 |= (U64)1 << j;

						match = true;
						break;
					}

				if(!match) {
					Log_errorLnx("Couldn't parse -m x, where x is spv, dxil or all (or for example spv,dxil)");
					s_uccess = false;
					goto clean;
				}
			}

			ListCharString_freex(&splits);
		}

		//Check thread count

		U32 threadCount = Platform_instance.threads;
		Bool defaultThreadCount = true;

		if(args.parameters & EOperationHasParameter_ThreadCount) {

			CharString thread = (CharString) { 0 };

			gotoIfError2(clean, ListCharString_get(args.args, offset++, &thread))

			if(CharString_endsWithSensitive(thread, '%', 0)) {					//-threads 50%

				CharString number = CharString_createRefSizedConst(thread.ptr, CharString_length(thread) - 1, false);
				F64 num = 0;

				if (!CharString_parseDouble(number, &num) || num < 0 || num > 100) {
					Log_errorLnx("Couldn't parse -threads x%, x is expected to be a F64 between (0-100)% or 0 -> threadCount - 1");
					s_uccess = false;
					goto clean;
				}

				threadCount = (U32) F64_max(1, threadCount * num / 100);
				defaultThreadCount = num == 0;
			}

			else {			//-threads x

				U64 num = 0;
				if (!CharString_parseU64(thread, &num) || num > threadCount) {
					Log_errorLnx("Couldn't parse -threads x, where x is expected to be a F64 of (0-100)% or 0 -> threadCount - 1");
					s_uccess = false;
					goto clean;
				}

				threadCount = (U32)num == 0 ? threadCount : (U32)num;
				defaultThreadCount = num == 0;
			}
		}

		//Compile type

		CharString compileTypeStr = (CharString) { 0 };

		if(args.parameters & EOperationHasParameter_ShaderCompileMode) {

			gotoIfError2(clean, ListCharString_get(args.args, offset++, &compileTypeStr));

			if (CharString_equalsStringInsensitive(compileTypeStr, CharString_createRefCStrConst("preprocess")))
				compileType = ECompileType_Preprocess;

			else if (CharString_equalsStringInsensitive(compileTypeStr, CharString_createRefCStrConst("includes")))
				compileType = ECompileType_Includes;

			else if (CharString_equalsStringInsensitive(compileTypeStr, CharString_createRefCStrConst("symbols")))
				compileType = ECompileType_Symbols;

			else if (CharString_equalsStringInsensitive(compileTypeStr, CharString_createRefCStrConst("reflect"))) {
				Log_errorLnx("Shader compiler \"reflect\" mode isn't supported yet");
				s_uccess = false;
				goto clean;
			}

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
			gotoIfError2(clean, ListCharString_get(args.args, offset++, &includeDir));

		//Get all shaders

		if (File_hasFolder(input)) {

			Bool isVirtual;
			gotoIfError2(clean, File_resolvex(input, &isVirtual, 0, &resolved))
			gotoIfError2(clean, CharString_appendx(&resolved, '/'))

			gotoIfError2(clean, File_resolvex(output, &isVirtual, 0, &resolved2))
			gotoIfError2(clean, CharString_appendx(&resolved2, '/'))

			ShaderFileRecursion shaderFileRecursion = (ShaderFileRecursion) {
				.allShaders = &allFiles,
				.allOutputs = &allOutputs,
				.allModes = &allCompileModes,
				.base = resolved,
				.output = resolved2,
				.compileModeU64 = compileModeU64,
				.hasMultipleModes = multipleModes,
				.compileType = compileType
			};

			gotoIfError2(clean, File_foreach(
				input,
				(FileCallback) registerFile,
				&shaderFileRecursion,
				true
			))

			//Make sure we can have a folder at output

			gotoIfError2(clean, File_add(resolved2, EFileType_Folder, 1 * SECOND, false))
			isFolder = true;
		}

		//We need to add multiple compile modes

		else for(U8 i = 0; i < ESHBinaryType_Count; ++i) {

			if(!((compileModeU64 >> i) & 1))
				continue;

			//Replace output's .hlsl by .spv.hlsl or .dxil.hlsl

			if (multipleModes || compileType != ECompileType_Preprocess)
				gotoIfError2(clean, CharString_formatx(
					&tempStr, "%.*s%s",
					(int)U64_min(
						CharString_length(output),
						CharString_findLastStringInsensitive(output, CharString_createRefCStrConst(".hlsl"), 0)
					),
					output.ptr,
					compileType == ECompileType_Preprocess ? fileSuffixes[i] : (
						compileType == ECompileType_Includes || compileType == ECompileType_Symbols ? txtSuffix :
						oiSHSuffixes[i]
					)
				))

			//Otherwise we can safely reuse output, since it's just a ref

			else tempStr = output;

			//Register mode and input/output name

			gotoIfError2(clean, ListCharString_pushBackx(&allFiles, input))

			gotoIfError2(clean, ListCharString_pushBackx(&allOutputs, tempStr))		//Moved here
			tempStr = CharString_createNull();

			gotoIfError2(clean, ListU8_pushBackx(&allCompileModes, i))
		}

		//Only continue if there are any files and then fetch all files

		if (!allFiles.length) {
			Log_debugLnx("No files to process");
			goto clean;
		}

		U64 totalLen = 0;
		CharString prevStr = CharString_createNull();

		for (U64 i = 0; i < allFiles.length; ++i) {

			//Grab from cache if we're re-compiling the same file with a different mode

			if (CharString_equalsStringSensitive(prevStr, allFiles.ptr[i])) {

				CharString shader = *ListCharString_last(allShaderText);
				shader = CharString_createRefStrConst(shader);

				gotoIfError2(clean, ListCharString_pushBackx(&allShaderText, shader))
				totalLen += CharString_length(shader);

				continue;
			}

			//Otherwise grab from file

			gotoIfError2(clean, File_read(allFiles.ptr[i], 10 * MS, &temp))

			if(!Buffer_length(temp)) {
				gotoIfError2(clean, ListCharString_pushBackx(&allShaderText, CharString_createNull()))
				continue;
			}

			gotoIfError2(clean, CharString_createCopyx(
				CharString_createRefSizedConst((const C8*)temp.ptr, Buffer_length(temp), false), &tempStr
			))

			gotoIfError2(clean, ListCharString_pushBackx(&allShaderText, tempStr))
			tempStr = CharString_createNull();

			totalLen += Buffer_length(temp);
			Buffer_freex(&temp);

			prevStr = allFiles.ptr[i];
		}

		//Spin up threads if it's worth it

		if (
			//Default thread count behavior; 64KiB or more (with 8+ files) or 16+ files
			(((totalLen >= 64 * KIBI && allFiles.length >= 8) || allFiles.length >= 16) && defaultThreadCount) ||

			//Or if thread count is forced
			(threadCount > 1 && !defaultThreadCount)
		) {

			lock = Lock_create();
			U64 counter = 0, threadCounter = 0, completedCounter = 0, counterCompiledBinaries = 0;

			gotoIfError2(clean, ListThread_resizex(&threads, threadCount))
			gotoIfError2(clean, ListCompiler_resizex(&compilers, threadCount))

			CompilerJobScheduler jobScheduler = (CompilerJobScheduler) {

				.args = args,
				.inputPaths = allFiles,
				.inputData = allShaderText,
				.outputPaths = allOutputs,
				.compileModes = allCompileModes,
				.compilers = compilers,

				.shEntries = &shEntries,
				.shEntryIds = &shEntryIds,

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
		}

		else {

			gotoIfError3(clean, Compiler_createx(&compiler, e_rr))

			for (U64 i = 0; i < allFiles.length; ++i) {

				//Preprocess to get information necessary for real compiles.
				//Though this is generally enough info for includes, (most) reflection and preprocessing

				if(!CLI_precompileShaderSingle(
					compiler, allCompileModes.ptr[i], args,
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

						if(!(args.flags & EOperationFlags_IgnoreEmptyFiles)) {

							Log_errorLnx(
								"Precompile couldn't find entrypoints for file \"%.*s\"",
								(int)CharString_length(allFiles.ptr[i]), allFiles.ptr[i].ptr
							);

							s_uccess = false;
						}

						ListSHEntryRuntime_freeUnderlyingx(&runtimeEntries);
						continue;
					}

					U32 crc32c = U32_MAX;
					gotoIfError3(clean, Compiler_crc32cx(allShaderText.ptr[i], &crc32c, e_rr))

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
							compiler, allCompileModes.ptr[i], args,
							allFiles.ptr[i], allShaderText.ptr[i],
							&tempResult,
							runtimeEntry,
							combinationId,
							includeDir
						)) {
							s_uccess = false;
							Log_errorLnx(
								"Compile failed for file \"%.*s\"",
								(int)CharString_length(allFiles.ptr[i]), allFiles.ptr[i].ptr
							);

							didSucceed = false;
							break;
						}

						//Add binary to SHFile

						else {

							if(tempResult.type != ECompileResultType_Binary)
								retError(clean, Error_invalidState(0, "CLI_compileShaderSingle() should return binary"))

							ESHBinaryType compileModei = allCompileModes.ptr[i];

							gotoIfError3(clean, SHEntryRuntime_asBinaryInfo(
								runtimeEntry, combinationId, compileModei, tempResult.binary, &binaryInfo, e_rr
							))

							//Add info regarding includes.
							//Merge includes, since different entrypoints can have different includes

							for(U64 k = 0; k < tempResult.includeInfo.length; ++k) {

								IncludeInfo *includeInfok = &tempResult.includeInfo.ptrNonConst[k];
								shInclude = (SHInclude) {
									.crc32c = includeInfok->crc32c,
									.relativePath = includeInfok->file
								};

								includeInfok->file = CharString_createNull();

								//Make sure our includes are relative to source, rather than absolute.
								//Otherwise it's not reproducible

								if (!CharString_startsWithSensitive(shInclude.relativePath, '@', 0)) {

									gotoIfError3(clean, File_makeRelativex(
										allFiles.ptr[i], shInclude.relativePath, 256, &tempStr, e_rr
									))

									CharString_freex(&shInclude.relativePath);
									shInclude.relativePath = tempStr;
									tempStr = CharString_createNull();
								}

								gotoIfError3(clean, SHFile_addIncludex(&shFile, &shInclude, e_rr))
							}

							//Move binary there to avoid copying mem if possible

							binaryInfo.binaries[compileModei] = tempResult.binary;
							gotoIfError3(clean, SHFile_addBinaryx(&shFile, &binaryInfo, e_rr))
							tempResult.binary = Buffer_createNull();

							CompileResult_freex(&tempResult);
						}
					}

					//Link entrypoint to binaries

					if(didSucceed)
						for (U64 j = 0, k = 0; j < runtimeEntries.length; ++j) {

							SHEntryRuntime *runtime = &runtimeEntries.ptrNonConst[j];

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
							gotoIfError3(clean, SHFile_addEntrypointx(&shFile, &runtime->entry, e_rr))

							k += l;
						}

					//Write to disk and free temp data

					if (didSucceed) {
						gotoIfError3(clean, SHFile_writex(shFile, &temp, e_rr))
						gotoIfError2(clean, File_write(temp, allOutputs.ptr[i], 100 * MS))
						Buffer_freex(&temp);
					}

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

		if (compileType == ECompileType_Includes && isFolder) {

			if(compiler.interfaces[0])
				gotoIfError3(clean, Compiler_mergeIncludeInfox(&compiler, &includeInfo, e_rr))

			else for(U64 i = 0; i < compilers.length; ++i)
				gotoIfError3(clean, Compiler_mergeIncludeInfox(&compilers.ptrNonConst[i], &includeInfo, e_rr))

			//Sort IncludeInfo

			ListIncludeInfo_sortCustom(includeInfo, (CompareFunction) IncludeInfo_compare);

			//We won't be needing resolved, so we can safely apppend root.txt after it

			gotoIfError2(clean, CharString_createCopyx(output, &tempStr3))

			if(!CharString_endsWithSensitive(output, '/', 0) && !CharString_endsWithSensitive(output, '\\', 0))
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
					Buffer_crc32c(CharString_bufferConst(allShaderText.ptr[i])), (U32)CharString_length(allShaderText.ptr[i]),
					allFiles.ptr[i].ptr
				))

				gotoIfError2(clean, CharString_appendStringx(&tempStr, tempStr2))
				CharString_freex(&tempStr2);
			}

			gotoIfError2(clean, File_write(CharString_bufferConst(tempStr), tempStr3, 10 * MS))
		}

	clean:

		F64 dt = (F64)(Time_now() - start) / SECOND;

		if(s_uccess)
			Log_debugLnx("-- Compile %.*s success in %fs", (int)CharString_length(input), input.ptr, dt);

		else Log_errorLnx("-- Compile %.*s failed in %fs!", (int)CharString_length(input), input.ptr);

		Error_printx(errTemp, ELogLevel_Error, ELogOptions_Default);

		for(U64 i = 0; i < threads.length; ++i)
			Thread_waitAndCleanupx(&threads.ptrNonConst[i]);

		ListThread_freex(&threads);
		ListCompiler_freeUnderlyingx(&compilers);
		Compiler_freex(&compiler);
		ListIncludeInfo_freeUnderlyingx(&includeInfo);
		Lock_unlock(&lock);
		Buffer_freex(&temp);
		CharString_freex(&resolved);
		CharString_freex(&resolved2);
		CharString_freex(&tempStr);
		CharString_freex(&tempStr2);
		CharString_freex(&tempStr3);
		ListCharString_freeUnderlyingx(&allFiles);
		ListCharString_freeUnderlyingx(&allShaderText);
		ListCharString_freeUnderlyingx(&allOutputs);
		ListU8_freex(&allCompileModes);

		ListSHEntryRuntime_freeUnderlyingx(&runtimeEntries);
		SHFile_freex(&shFile);
		SHEntry_freex(&shEntry);
		SHInclude_freex(&shInclude);
		SHBinaryInfo_freex(&binaryInfo);
		ListU32_freex(&compileCombinations);
		ListU16_freex(&binaryIndices);

		ListListSHEntryRuntime_freeUnderlyingx(&shEntries);
		ListU64_freex(&shEntryIds);
		CompileResult_freex(&tempResult);

		return s_uccess;
	}
#else
	Bool CLI_compileShader(ParsedArgs args) { (void) args; return false; }
#endif
