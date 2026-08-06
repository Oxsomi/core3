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

//shader_compiler/compiler_helper.c

#include "types/container/string.h"
#include "types/container/log.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/container/list_basic_types.h"
#include "formats/oiSH/sh_file.h"
#include "shader_compiler/compiler.h"
#include "compiler_helper_internal.h"

Bool Compiler_precompileShader(
	const Compiler *compiler,
	ESHBinaryType outputType,
	Bool isDebug,
	CharString inputPath,
	CharString input,
	ListSHEntryRuntime *shEntriesRuntime,
	const ListCharString *includeDirs,
	Bool enableLogging,
	const Allocator *alloc
) {

	CompilerSettings settings = (CompilerSettings) {
		.string = input,
		.path = inputPath,
		.debug = isDebug,
		.format = ECompilerFormat_HLSL,
		.outputType = outputType,
		.infoAboutIncludes = false,
		.includeDirs = *includeDirs
	};
	 
	Error errTemp = Error_none(), *e_rr = &errTemp;
	Bool s_uccess = true;

	CompileResult compileResult = (CompileResult) { 0 };
	gotoIfError3(clean, Compiler_parse(compiler, &settings, alloc, &compileResult, e_rr));

	if(enableLogging)
		for(U64 i = 0; i < compileResult.compileErrors.length; ++i) {

			CompileError e = compileResult.compileErrors.ptr[i];

			if((e.typeLineId >> 7) == ECompileErrorType_Warn)
				Log_warnLn(alloc, "%s:%"PRIu32":%"PRIu8": %s", e.file.ptr, CompileError_lineId(e), e.lineOffset, e.error.ptr);

			else Log_errorLn(
				alloc, "%s:%"PRIu32":%"PRIu8": %s", e.file.ptr, CompileError_lineId(e), e.lineOffset, e.error.ptr
			);
		}

	//Write final compile result

	if (compileResult.isSuccess) {

		if (compileResult.type != ECompileResultType_SHEntryRuntime)
			retError(clean, Error_invalidState(0, "Compiler_precompileShader() expected SHEntryRuntime result"));

		//Tell oiSH entries to caller

		if (shEntriesRuntime && compileResult.shEntriesRuntime.length) {

			//Move list with all allocated memory

			*shEntriesRuntime = compileResult.shEntriesRuntime;
			compileResult.shEntriesRuntime = (ListSHEntryRuntime) { 0 };

			//Move as copy, since these are refs to the input file, which will be freed at end of function
			//TODO: This is not true anymore!

			for (U64 i = 0; i < shEntriesRuntime->length; ++i) {

				SHEntryRuntime *entry = &shEntriesRuntime->ptrNonConst[i];
				entry->entry.idOrPadding = (U16) i;

				//Copy uniforms

				ListSHUniformRuntime prev = entry->uniforms;
				Bool usePrev = ListSHUniformRuntime_isRef(prev);

				if (usePrev) {
					entry->uniforms = (ListSHUniformRuntime) { 0 };
					gotoIfError3(clean, ListSHUniformRuntime_createCopy(prev, alloc, &entry->uniforms, e_rr));
				}

				for(U64 j = 0; j < entry->uniforms.length; ++j) {

					CharString str = usePrev ? prev.ptr[j].name : entry->uniforms.ptr[j].name;

					if (usePrev || CharString_isRef(str)) {
						entry->uniforms.ptrNonConst[j].name = CharString_createNull();
						gotoIfError3(clean, CharString_createCopy(str, alloc, &entry->uniforms.ptrNonConst[j].name, e_rr));
					}
				}

				if (ListU8_isRef(entry->uniformData)) {
					ListU8 tmp = (ListU8) { 0 };
					gotoIfError3(clean, ListU8_createCopy(entry->uniformData, alloc, &tmp, e_rr));
					entry->uniformData = tmp;
				}

				//Copy define names if needed

				for (U64 j = 0; j < entry->defineNameValues.length; ++j) {

					CharString *curr = &entry->defineNameValues.ptrNonConst[j];
					CharString temp = CharString_createNull();

					if(!CharString_isRef(*curr))
						continue;

					gotoIfError3(clean, CharString_createCopy(*curr, alloc, &temp, e_rr));
					*curr = temp;
				}

				//Copy name if needed

				if(!CharString_isRef(entry->entry.name))
					continue;

				CharString temp = CharString_createNull();
				gotoIfError3(clean, CharString_createCopy(entry->entry.name, alloc, &temp, e_rr));
				entry->entry.name = temp;
			}
		}
	}

clean:
	s_uccess &= compileResult.isSuccess;

	if(!s_uccess)
		ListSHEntryRuntime_freeUnderlying(shEntriesRuntime, alloc);

	CompileResult_free(&compileResult, alloc);

	//Respect enableLogging: an expected-failure compile (e.g. a unit test provoking a precompile error) passes
	//enableLogging=false to stay quiet, so don't print the parse error unconditionally.
	if(enableLogging)
		Error_print(alloc, &errTemp, ELogLevel_Error, ELogOptions_Default);

	return s_uccess;
}

Bool Compiler_getUniqueCompiles(
	const ListSHEntryRuntime *runtimeEntries,
	ListU32 *compileCombinations,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListSHBinaryIdentifier identifiers = (ListSHBinaryIdentifier) { 0 };

	//Go through each compile combination.
	//Linking is a different story;
	//Each combination of uniforms and entrypoints will have to be linked later.

	for (U64 i = 0; i < runtimeEntries->length; ++i) {

		if(i >> 15)
			retError(clean, Error_overflow(0, i, 1 << 15, "Compiler_getUniqueCompiles() i out of bounds"));

		SHEntryRuntime runtime = runtimeEntries->ptr[i];

		//Note: Since compiled combinations excludes uniforms, it'll "compile" for uniformId 0 which will later be
		//        explicitly linked for the real uniform information.
		//        This means the compiler will only do 1 compile step, but multiple linking steps for the uniforms.

		for (U64 j = 0; j < SHEntryRuntime_getCombinationsCompiled(&runtime); ++j) {

			if(j >> 15)
				retError(clean, Error_overflow(1, j, 1 << 15, "Compiler_getUniqueCompiles() j out of bounds"));

			SHBinaryIdentifier binaryIdentifier = (SHBinaryIdentifier) { 0 };
			gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(&runtime, (U16) j, &binaryIdentifier, e_rr));

			//Find SHBinaryIdentifier or not

			U64 k = 0;

			for(; k < identifiers.length; ++k)    //TODO: This one should combine compilations if isShaderAnnotation
				if(SHBinaryIdentifier_equals(&binaryIdentifier, &identifiers.ptr[k]))
					break;

			//When it's new, we gotta remember the binary identifier for reuse.
			//Other than that, we need to tell the compiler which runtime id and combination id we need

			if(k == identifiers.length) {

				gotoIfError3(clean, ListSHBinaryIdentifier_pushBack(&identifiers, binaryIdentifier, alloc, e_rr));

				if(compileCombinations)
					gotoIfError3(clean, ListU32_pushBack(compileCombinations, (U32)(j | (i << 16)), alloc, e_rr));
			}

			//Update flags of compileCombinations->ptr[k] stored in bit 15 (isRt), 31 (isGfxOrComp)
			
			if(compileCombinations) {

				Bool isRt =
					runtime.entry.stage >= ESHPipelineStage_RtStartExt && runtime.entry.stage <= ESHPipelineStage_RtEndExt;

				if (isRt)
					compileCombinations->ptrNonConst[k] |= 1 << 15;

				//1u, not 1: the list is U32, but a bare 1 is int, and shifting it into the sign bit is
				//undefined rather than merely implementation defined.
				//MSVC happens to give 0x80000000, clang at -O2 is entitled to assume it can't happen.

				if (!isRt && runtime.entry.stage != ESHPipelineStage_WorkgraphExt)
					compileCombinations->ptrNonConst[k] |= 1u << 31;
			}
		}
	}

clean:
	//We will never allocate nested memory into this, so it's fine to just free it (no underlying data)
	ListSHBinaryIdentifier_free(&identifiers, alloc);
	return s_uccess;
}

void Compiler_printErrors(ListCompileError errors, const Allocator *alloc) {
	
	for(U64 i = 0; i < errors.length; ++i) {

		CompileError e = errors.ptr[i];

		if(e.file.ptr) {

			if((e.typeLineId >> 7) == ECompileErrorType_Warn)
				Log_warnLn(alloc, "%s:%"PRIu32":%"PRIu8": %s", e.file.ptr, CompileError_lineId(e), e.lineOffset, e.error.ptr);

			else Log_errorLn(alloc, "%s:%"PRIu32":%"PRIu8": %s", e.file.ptr, CompileError_lineId(e), e.lineOffset, e.error.ptr);
		}

		else if((e.typeLineId >> 7) == ECompileErrorType_Warn)
			Log_warnLn(alloc, "%s", e.error.ptr);

		else Log_errorLn(alloc, "%s", e.error.ptr);
	}
}
void Compiler_logStatus(
	ESHBinaryType binaryType,
	const C8 *type,
	CharString inputPath,
	U16 runtimeEntryId,
	U16 combinationId,
	const Allocator *alloc,
	Bool s_uccess
) {
	
	const C8 *binType = binaryType == ESHBinaryType_SPIRV ? "spirv" : "dxil";

		if(s_uccess)
			Log_debugLn(
				alloc, "%s success: %.*s (%s, %"PRIu32":%"PRIu32")",
				type, (int) CharString_length(inputPath), inputPath.ptr,
				binType, runtimeEntryId, combinationId
			);

		else
			Log_errorLn(
				alloc, "%s failed: %.*s (%s, %"PRIu32":%"PRIu32")",
				type, (int) CharString_length(inputPath), inputPath.ptr,
				binType, runtimeEntryId, combinationId
			);
}

Bool Compiler_getUniqueEntrypointsDXIL(
	const Compiler *compiler,
	Buffer binary,
	Bool showAll,
	ListCompilerEntrypoint *uniqueEntrypoints,
	const Allocator *alloc,
	Error *e_rr
);

Bool Compiler_getUniqueEntrypointsSPIRV(
	const Compiler *compiler,
	Buffer binary,
	Bool showAll,
	ListCompilerEntrypoint *uniqueEntrypoints,
	const Allocator *alloc,
	Error *e_rr
);

Bool Compiler_getUniqueEntrypoints(
	const Compiler *compiler,
	ESHBinaryType binaryType,
	Buffer binary,
	Bool showAll,
	ListCompilerEntrypoint *uniqueEntrypoints,
	const Allocator *alloc,
	Error *e_rr
) {
	
	Bool s_uccess = true;

	switch (binaryType) {

		case ESHBinaryType_SPIRV:
			gotoIfError3(clean, Compiler_getUniqueEntrypointsSPIRV(compiler, binary, showAll, uniqueEntrypoints, alloc, e_rr));
			break;

		case ESHBinaryType_DXIL:
			gotoIfError3(clean, Compiler_getUniqueEntrypointsDXIL(compiler, binary, showAll, uniqueEntrypoints, alloc, e_rr));
			break;

		default:
			retError(clean, Error_unimplemented(0, "Compiler_getUniqueEntrypoints() has invalid type"));
	}

clean:
	return s_uccess;
}

Bool Compiler_compileShaderSingle(
	const Compiler *compiler,
	ESHBinaryType binaryType,
	Bool isDebug,
	Bool keepRegisters,
	Bool isRt,
	Bool isGfxOrComp,
	CharString inputPath,
	CharString input,
	CompileResult *dest,
	const ListSHEntryRuntime *runtimeEntries,
	U16 runtimeEntryId,
	U16 combinationId,
	const ListCharString *includeDirs,
	Bool enableLogging,
	const Allocator *alloc
) {

	Error errTemp = Error_none(), *e_rr = &errTemp;
	Bool s_uccess = true;

	if(!dest)
		retError(clean, Error_nullPointer(5, "Compiler_compileShaderSingle()::dest is required"));

	if(dest->binary.ptr || dest->compileErrors.ptr || dest->includeInfo.ptr)
		retError(clean, Error_invalidParameter(5, 0, "Compiler_compileShaderSingle()::dest was present, but not empty"));

	CompilerSettings settings = (CompilerSettings) {
		.string = input,
		.path = inputPath,
		.debug = isDebug,
		.keepUnusedRegisters = keepRegisters,
		.isRt = isRt,
		.containsGfxOrComp = isGfxOrComp,
		.format = ECompilerFormat_HLSL,
		.outputType = binaryType,
		.infoAboutIncludes = true,        //Required to supply oiSH info about includes
		.includeDirs = *includeDirs
	};

	//First we need to go from text with includes and defines to easy to parse text
	//Accessing SHEntryRuntime here without locking is safe, since we don't access these properties from asBinaryIdentifier

	SHEntryRuntime entry = runtimeEntries->ptrNonConst[runtimeEntryId];
	SHBinaryIdentifier binaryIdentifier = (SHBinaryIdentifier) { 0 };
	gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(&entry, combinationId, &binaryIdentifier, e_rr));

	settings.isLib = entry.isShaderAnnotation;
	settings.containsGfxOrComp = SHEntryRuntime_containsGfxOrComp(entry);
	settings.isRt = SHEntryRuntime_isRt(entry);

	gotoIfError3(clean, Compiler_compile(compiler, &settings, &binaryIdentifier, alloc, dest, e_rr));

	if(enableLogging)
		Compiler_printErrors(dest->compileErrors, alloc);

clean:

	s_uccess &= dest && dest->isSuccess;

	if(enableLogging)
		Compiler_logStatus(binaryType, "Compile", inputPath, runtimeEntryId, combinationId, alloc, s_uccess);
		
	Error_print(alloc, &errTemp, ELogLevel_Error, ELogOptions_Default);
	return s_uccess;
}

Bool Compiler_linkSingle(
	const Compiler *compiler,
	CharString path,
	U16 runtimeEntryId,
	U16 combinationId,
	ESHBinaryType type,
	const ListBuffer *inputs,
	const ListSHUniformRuntime *uniforms,
	Buffer uniformData,
	CharString entrypoint,
	U16 shaderVersion,
	ESHPipelineStage stageType,
	ESHExtension exts,
	Bool enableLogging,
	Buffer *result,
	const Allocator *alloc
) {

	Error errTemp = Error_none(), *e_rr = &errTemp;
	Bool s_uccess = true;
	ListCompileError errors = (ListCompileError) { 0 };

	if(!result)
		retError(clean, Error_nullPointer(5, "Compiler_linkSingle()::result is required"));

	if(result->ptr)
		retError(clean, Error_invalidParameter(5, 0, "Compiler_linkSingle()::result was present, but not empty"));

	gotoIfError3(clean, Compiler_link(
		compiler, type, inputs, uniforms, uniformData, &entrypoint, shaderVersion, stageType, exts, &errors, result,
		alloc, e_rr
	));

	if (enableLogging)
		Compiler_printErrors(errors, alloc);

clean:

	if (enableLogging)
		Compiler_logStatus(type, "Link", path, runtimeEntryId, combinationId, alloc, s_uccess);
		
	Error_print(alloc, &errTemp, ELogLevel_Error, ELogOptions_Default);
	ListCompileError_freeUnderlying(&errors, alloc);
	return s_uccess;
}

Bool Compiler_processSingle(
	const Compiler *compiler,
	CharString path,
	U16 runtimeEntryId,
	U16 combinationId,
	ESHBinaryType binaryType,
	CompileResult *tempResult,
	Bool isDebug,
	Bool keepRegisters,
	const SHBinaryIdentifier *binaryIdentifier,
	SpinLock *lock,
	const ListSHEntryRuntime *runtimeEntries,
	Bool isShaderAnnotation,
	Bool enableLogging,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListCompileError errors = (ListCompileError) { 0 };

	if(!tempResult)
		retError(clean, Error_nullPointer(5, "Compiler_processSingle()::tempResult is required"));
		
	gotoIfError3(clean, Compiler_process(
		compiler,
		binaryType,
		&tempResult->binary,
		&tempResult->registers,
		isDebug,
		keepRegisters,
		binaryIdentifier,
		lock,
		runtimeEntries,
		isShaderAnnotation,
		&tempResult->demotion,
		&errors,
		alloc,
		e_rr
	));
		
	if (enableLogging)
		Compiler_printErrors(errors, alloc);

clean:
	
	if (enableLogging)
		Compiler_logStatus(binaryType, "Process", path, runtimeEntryId, combinationId, alloc, s_uccess);
	
	ListCompileError_freeUnderlying(&errors, alloc);
	return s_uccess;
}
