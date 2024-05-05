/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/ext/listx.h"
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
#include "shader_compiler/compiler.h"
#include "cli.h"

typedef struct ShaderFileRecursion {

	ListCharString *allShaders;
	ListCharString *allOutputs;
	ListU8 *allModes;

	CharString base, output;

	U64 compileModeU64;
	Bool hasMultipleModes;

} ShaderFileRecursion;

const C8 *fileSuffixes[] = {
	".spv.hlsl",
	".dxil.hlsl"
};

Error registerFile(FileInfo file, ShaderFileRecursion *shaderFiles) {

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
			
			if(!shaderFiles->hasMultipleModes) {
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

				if(!foundFirstMode) {

					CharString input = *ListCharString_last(*shaderFiles->allShaders);
					input = CharString_createRefSizedConst(input.ptr, CharString_length(input), false);

					gotoIfError(clean, ListCharString_pushBackx(shaderFiles->allShaders, input))
				}

				//Append .spv.hlsl and .dxil.hlsl at the end
				
				gotoIfError(clean, CharString_formatx(
					&tempStr, "%.*s%s", 
					(int)U64_min(
						CharString_length(copy), 
						CharString_findLastStringInsensitive(copy, CharString_createRefCStrConst(".hlsl"), 0)
					),
					copy.ptr,
					fileSuffixes[i]
				))

				gotoIfError(clean, ListCharString_pushBackx(shaderFiles->allOutputs, tempStr))
				tempStr = CharString_createNull();
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

Bool CLI_compileShaderSingle(
	Compiler compiler,
	ESHBinaryType binaryType,
	ParsedArgs args,
	CharString inputPath,
	CharString input,
	CharString outputPath
) {

	Bool isDebug = args.flags & EOperationFlags_Debug;

	CompilerSettings settings = (CompilerSettings) {
		.string = input,
		.path = inputPath,
		.debug = isDebug,
		.format = ECompilerFormat_HLSL,
		.outputType = binaryType,
	};

	Error err = Error_none();
	CompileResult compileResult = (CompileResult) { 0 };
	gotoIfError(clean, Compiler_preprocessx(compiler, settings, &compileResult))

	for(U64 i = 0; i < compileResult.compileErrors.length; ++i) {

		CompileError e = compileResult.compileErrors.ptr[i];

		if((e.typeLineId >> 7) == ECompileErrorType_Warn)
			Log_warnLnx("%s:%"PRIu32":%"PRIu8": %s", e.file.ptr, CompileError_lineId(e), e.lineOffset, e.error.ptr);

		else Log_errorLnx("%s:%"PRIu32":%"PRIu8": %s", e.file.ptr, CompileError_lineId(e), e.lineOffset, e.error.ptr);
	}

	if (compileResult.isSuccess)
		gotoIfError(clean, File_write(CharString_bufferConst(compileResult.text), outputPath, 10 * MS))

clean:
	CompileResult_freex(&compileResult);
	Error_printx(err, ELogLevel_Error, ELogOptions_Default);
	return !err.genericError && compileResult.isSuccess;
}

typedef struct CompilerJobScheduler {

	ParsedArgs args;

	ListCharString inputPaths;
	ListCharString inputData;
	ListCharString outputPaths;
	ListU8 compileModes;
	ListCompiler compilers;

	Lock *lock;
	U64 *counter;
	U64 *threadCounter;
	Bool *success;

} CompilerJobScheduler;

void CLI_compileShaderJob(CompilerJobScheduler *job) {

	if(!job)
		return;

	Error err = Error_none();
	ELockAcquire acq = ELockAcquire_Invalid;
	U64 threadCounter = U64_MAX;

	while(true) {

		//Lock to get next job id

		 acq = Lock_lock(job->lock, U64_MAX);

		if(acq < ELockAcquire_Success)
			gotoIfError(clean, Error_invalidState(0, "CLI_compileShaderJob() couldn't lock"))

		if(*job->counter == job->inputData.length) {
			*job->success &= !err.genericError;
			break;
		}

		U64 ourJobId = (*job->counter)++;

		if (threadCounter == U64_MAX)
			threadCounter = (*job->threadCounter)++;

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(job->lock);

		acq = ELockAcquire_Invalid;

		//Process next. If compile failed, still continue. Because we want all compiler errors

		CharString input = job->inputPaths.ptr[ourJobId];

		if(!CLI_compileShaderSingle(
			job->compilers.ptr[threadCounter],
			job->compileModes.ptr[threadCounter],
			job->args,
			input,
			job->inputData.ptr[ourJobId],
			job->outputPaths.ptr[ourJobId]
		)) {
			Log_errorLnx("Compile failed for file \"%.*s\"", (int)CharString_length(input), input.ptr);
			err = Error_invalidState(1, "One of CLI_compileShaderJob() failed");
		}
	}

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(job->lock);
}

Bool CLI_compileShader(ParsedArgs args) {

	if (!(args.flags & EOperationFlags_Preprocess)) {		//TODO:
		Log_errorLnx("Currently only supporting OxC3 compile shaders --preprocess");
		return false;
	}

	//Get input

	U64 offset = 0;
	CharString input = (CharString) { 0 };
	Error err = ListCharString_get(args.args, offset++, &input);

	if (err.genericError) {
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
		return false;
	}

	//Check if output is valid

	CharString output = (CharString) { 0 };

	if ((err = ListCharString_get(args.args, offset++, &output)).genericError) {
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
		return false;
	}

	//Grab modes
	
	U64 compileModeU64 = 0;
	CharString compileMode = (CharString) { 0 };

	if ((err = ListCharString_get(args.args, offset++, &compileMode)).genericError) {
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
		return false;
	}

	ListCharString splits = (ListCharString) { 0 };
	if((err = CharString_splitSensitivex(compileMode, ',', &splits)).genericError) {
		Log_errorLnx("Couldn't parse -m x, where x is spv, dxil or all (or for example spv,dxil)");
		return false;
	}

	CharString modes[] = {
		CharString_createRefCStrConst("spv"),
		CharString_createRefCStrConst("dxil"),
		CharString_createRefCStrConst("all")
	};

	static const U64 modeCount = sizeof(modes) / sizeof(modes[0]);
	Bool multipleModes = splits.length > 1;

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
			ListCharString_freex(&splits);
			Log_errorLnx("Couldn't parse -m x, where x is spv, dxil or all (or for example spv,dxil)");
			return false;
		}
	}

	ListCharString_freex(&splits);

	//Check thread count

	U32 threadCount = Platform_instance.threads;
	Bool defaultThreadCount = true;

	if(args.parameters & EOperationHasParameter_ThreadCount) {

		CharString thread = (CharString) { 0 };

		if ((err = ListCharString_get(args.args, offset++, &thread)).genericError) {
			Error_printx(err, ELogLevel_Error, ELogOptions_Default);
			return false;
		}

		if(CharString_endsWithSensitive(thread, '%', 0)) {					//-t 50%

			CharString number = CharString_createRefSizedConst(thread.ptr, CharString_length(thread) - 1, false);
			F64 num = 0;
			
			if (!CharString_parseDouble(number, &num) || num < 0 || num > 100) {
				Log_errorLnx("Couldn't parse -t x%, where x is expected to be a F64 between (0-100)% or 0 -> threadCount - 1");
				return false;
			}

			threadCount = (U32) F64_max(1, threadCount * num / 100);
			defaultThreadCount = num == 0;
		}

		else {			//-t x

			U64 num = 0;
			if (!CharString_parseU64(thread, &num) || num > threadCount) {
				Log_errorLnx("Couldn't parse -t x, where x is expected to be a F64 of (0-100)% or 0 -> threadCount - 1");
				return false;
			}

			threadCount = (U32)num == 0 ? threadCount : (U32)num;
			defaultThreadCount = num == 0;
		}
	}

	ListCharString allFiles = (ListCharString) { 0 };
	ListCharString allShaderText = (ListCharString) { 0 };
	ListCharString allOutputs = (ListCharString) { 0 };
	ListU8 allCompileModes = (ListU8) { 0 };
	ListThread threads = (ListThread) { 0 };
	ListCompiler compilers = (ListCompiler) { 0 };
	Compiler compiler = (Compiler) { 0 };
	CharString resolved = CharString_createNull();
	CharString tempStr = CharString_createNull();
	Buffer temp = Buffer_createNull();
	Bool isFolder = false;
	Lock lock = (Lock) { 0 };
	Ns start = Time_now();

	//Get all shaders

	if (File_hasFolder(input)) {

		Bool isVirtual;
		gotoIfError(clean, File_resolvex(input, &isVirtual, 0, &resolved))
		gotoIfError(clean, CharString_appendx(&resolved, '/'))

		ShaderFileRecursion shaderFileRecursion = (ShaderFileRecursion) {
			.allShaders = &allFiles,
			.allOutputs = &allOutputs,
			.allModes = &allCompileModes,
			.base = resolved,
			.output = output,
			.compileModeU64 = compileModeU64,
			.hasMultipleModes = multipleModes
		};

		gotoIfError(clean, File_foreach(
			input,
			(FileCallback) registerFile,
			&shaderFileRecursion,
			true
		))

		//Make sure we can have a folder at output

		gotoIfError(clean, File_add(output, EFileType_Folder, 1 * SECOND))
		isFolder = true;
	}

	//We need to 

	else for(U8 i = 0; i < ESHBinaryType_Count; ++i) {

		if(!((compileModeU64 >> i) & 1))
			continue;

		//Replace output's .hlsl by .spv.hlsl or .dxil.hlsl

		if (multipleModes)
			gotoIfError(clean, CharString_formatx(
				&tempStr, "%.*s%s", 
				(int)U64_min(
					CharString_length(output), 
					CharString_findLastStringInsensitive(output, CharString_createRefCStrConst(".hlsl"), 0)
				),
				output.ptr,
				fileSuffixes[i]
			))

		//Otherwise we can safely reuse output, since it's just a ref

		else tempStr = output;

		//Register mode and input/output name
		
		gotoIfError(clean, ListCharString_pushBackx(&allFiles, input))
		
		gotoIfError(clean, ListCharString_pushBackx(&allOutputs, tempStr))		//Moved here
		tempStr = CharString_createNull();

		gotoIfError(clean, ListU8_pushBackx(&allCompileModes, i))
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
			shader = CharString_createRefSizedConst(shader.ptr, CharString_length(shader), false);

			gotoIfError(clean, ListCharString_pushBackx(&allShaderText, shader))
			totalLen += CharString_length(shader);

			continue;
		}

		//Otherwise grab from file
		
		gotoIfError(clean, File_read(allFiles.ptr[i], 10 * MS, &temp))

		if(!Buffer_length(temp)) {
			gotoIfError(clean, ListCharString_pushBackx(&allShaderText, CharString_createNull()))
			continue;
		}

		gotoIfError(clean, CharString_createCopyx(
			CharString_createRefSizedConst((const C8*)temp.ptr, Buffer_length(temp), false), &tempStr
		))

		gotoIfError(clean, ListCharString_pushBackx(&allShaderText, tempStr))
		tempStr = CharString_createNull();

		totalLen += Buffer_length(temp);
		Buffer_freex(&temp);

		prevStr = allFiles.ptr[i];
	}

	//Spin up threads if it's worth it

	Bool success = true;

	if (
		//Default thread count behavior; 64KiB or more (with 8+ files) or 16+ files
		(((totalLen >= 64 * KIBI && allFiles.length >= 8) || allFiles.length >= 16) && defaultThreadCount) ||

		//Or if thread count is forced
		(threadCount > 1 && !defaultThreadCount)
	) {

		lock = Lock_create();
		U64 counter = 0;
		U64 threadCounter = 0;

		gotoIfError(clean, ListThread_resizex(&threads, threadCount))
		gotoIfError(clean, ListCompiler_resizex(&compilers, threadCount))

		CompilerJobScheduler jobScheduler = (CompilerJobScheduler) {

			.args = args,
			.inputPaths = allFiles,
			.inputData = allShaderText,
			.outputPaths = allOutputs,
			.compileModes = allCompileModes,
			.compilers = compilers,

			.lock = &lock,
			.counter = &counter,
			.threadCounter = &threadCounter,
			.success = &success
		};

		for(U64 i = 0; i < threadCount; ++i) {

			gotoIfError(clean, Compiler_createx(&compilers.ptrNonConst[i]))

			gotoIfError(clean, Thread_createx(
				(ThreadCallbackFunction) CLI_compileShaderJob, &jobScheduler, &threads.ptrNonConst[i]
			))
		}

		Bool success2 = true;

		for(U64 i = 0; i < threadCount; ++i)
			success2 &= !Thread_waitAndCleanupx(&threads.ptrNonConst[i]).genericError;

		ListThread_freex(&threads);
		success &= success2;
	}

	else {

		gotoIfError(clean, Compiler_createx(&compiler))

		for (U64 i = 0; i < allFiles.length; ++i)
			if(!CLI_compileShaderSingle(
				compiler, allCompileModes.ptr[i], args,
				allFiles.ptr[i], allShaderText.ptr[i], allOutputs.ptr[i]
			)) {
				success = false;
				Log_errorLnx("Compile failed for file \"%.*s\"", (int)CharString_length(allFiles.ptr[i]), allFiles.ptr[i].ptr);
			}
	}

	if(!success)
		gotoIfError(clean, Error_invalidState(0, "CLI_compileShader() compile failed"))

clean:

	F64 dt = (F64)(Time_now() - start) / SECOND;

	if(!err.genericError)
		Log_debugLnx("-- Compile %.*s success in %fs", (int)CharString_length(input), input.ptr, dt);

	else Log_errorLnx("-- Compile %.*s failed in %fs!", (int)CharString_length(input), input.ptr);

	for(U64 i = 0; i < threads.length; ++i)
		Thread_waitAndCleanupx(&threads.ptrNonConst[i]);

	ListThread_freex(&threads);
	ListCompiler_freeUnderlyingx(&compilers);
	Compiler_freex(&compiler);
	Lock_unlock(&lock);
	Buffer_freex(&temp);
	CharString_freex(&resolved);
	CharString_freex(&tempStr);
	ListCharString_freeUnderlyingx(&allFiles);
	ListCharString_freeUnderlyingx(&allShaderText);
	ListCharString_freeUnderlyingx(&allOutputs);
	ListU8_freex(&allCompileModes);
	return !err.genericError;
}
