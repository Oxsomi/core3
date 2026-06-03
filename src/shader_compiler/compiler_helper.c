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

#include "platforms/ext/listx_impl.h"
#include "types/container/string.h"
#include "types/container/log.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/base/lock.h"
#include "types/base/thread.h"
#include "types/math/math.h"
#include "formats/oiSH/sh_file.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "shader_compiler/compiler.h"
#include "types/base/constants.h"

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

	Allocator alloc;

} ShaderFileRecursion;

const C8 *oiSHCombineSuffix = ".oiSH";        //Suffix when oiSH is combined

const C8 *oiSHSuffixes[] = {
	".spv.oiSH",
	".dxil.oiSH"
};

Bool registerFile(FileInfo file, ShaderFileRecursion *shaderFiles, Error *e_rr) {

	Bool s_uccess = true;
	CharString copy = CharString_createNull();
	CharString tempStr = CharString_createNull();

	Allocator alloc = shaderFiles->alloc;

	if (file.type == EFileType_File) {

		CharString hlsl = CharString_createRefCStrConst(".hlsl");

		if (CharString_endsWithStringInsensitive(file.path, hlsl, 0)) {

			gotoIfError2(clean, CharString_createCopy(file.path, alloc, &copy));

			//Move to allShaders

			gotoIfError2(clean, ListCharString_pushBack(shaderFiles->allShaders, copy, alloc));
			copy = CharString_createNull();

			//Grab subPath

			CharString subPath = CharString_createNull();

			if(!CharString_cut(file.path, CharString_length(shaderFiles->base), 0, &subPath))
				retError(clean, Error_invalidState(0, "registerFile() couldn't get subPath"));

			//Copy subPath

			gotoIfError2(clean, CharString_createCopy(subPath, alloc, &copy));

			//Move subPath into new folder

			gotoIfError2(clean, CharString_insertString(&copy, shaderFiles->output, 0, alloc));

			//Handle multiple modes by inserting .spv.hlsl at the end

			Bool foundFirstMode = false;

			for(U8 i = 0; i < ESHBinaryType_Count; ++i) {

				if(!((shaderFiles->compileModeU64 >> i) & 1))
					continue;

				gotoIfError2(clean, ListU8_pushBack(shaderFiles->allModes, i, alloc));

				//Add double reference to input, so we don't waste memory (besides 24 for CharString struct itself)
				//Because we want to compile it with two different modes
				//The first mode already added one

				if(foundFirstMode) {

					CharString input = *ListCharString_last(*shaderFiles->allShaders);
					input = CharString_createRefStrConst(input);

					gotoIfError2(clean, ListCharString_pushBack(shaderFiles->allShaders, input, alloc));
				}

				//Append .oiSH/.spv.oiSH/etc.

				gotoIfError2(clean, CharString_format(
					alloc, &tempStr, "%.*s%s",
					(int)U64_min(
						CharString_length(copy),
						CharString_findLastStringInsensitive(copy, CharString_createRefCStrConst(".hlsl"), 0, 0)
					),
					copy.ptr,
					shaderFiles->hasCombineFlag ? oiSHCombineSuffix : oiSHSuffixes[i]
				));

				gotoIfError3(clean, File_add(tempStr, EFileType_File, 1 * MS, true, alloc, e_rr));
				gotoIfError2(clean, ListCharString_pushBack(shaderFiles->allOutputs, tempStr, alloc));
				tempStr = CharString_createNull();
				foundFirstMode = true;
			}

			CharString_free(&copy, alloc);
		}
	}

clean:
	CharString_free(&copy, alloc);
	CharString_free(&tempStr, alloc);
	return s_uccess;
}

Bool Compiler_precompileShader(
	Compiler compiler,
	ESHBinaryType outputType,
	Bool isDebug,
	CharString inputPath,
	CharString input,
	ListSHEntryRuntime *shEntriesRuntime,
	CharString includeDir,
	Bool enableLogging,
	Allocator alloc
) {

	CompilerSettings settings = (CompilerSettings) {
		.string = input,
		.path = inputPath,
		.debug = isDebug,
		.format = ECompilerFormat_HLSL,
		.outputType = outputType,
		.infoAboutIncludes = false,
		.includeDir = includeDir
	};
	 
	Error errTemp = Error_none(), *e_rr = &errTemp;
	Bool s_uccess = true;

	CompileResult compileResult = (CompileResult) { 0 };
	gotoIfError3(clean, Compiler_parse(compiler, settings, alloc, &compileResult, e_rr));

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
					gotoIfError2(clean, ListSHUniformRuntime_createCopy(prev, alloc, &entry->uniforms))
				}

				for(U64 j = 0; j < entry->uniforms.length; ++j) {

					CharString str = usePrev ? prev.ptr[j].name : entry->uniforms.ptr[j].name;

					if (usePrev || CharString_isRef(str)) {
						entry->uniforms.ptrNonConst[j].name = CharString_createNull();
						gotoIfError2(clean, CharString_createCopy(str, alloc, &entry->uniforms.ptrNonConst[j].name))
					}
				}

				if (ListU8_isRef(entry->uniformData)) {
					ListU8 tmp = (ListU8) { 0 };
					gotoIfError2(clean, ListU8_createCopy(entry->uniformData, alloc, &tmp))
					entry->uniformData = tmp;
				}

				//Copy define names if needed

				for (U64 j = 0; j < entry->defineNameValues.length; ++j) {

					CharString *curr = &entry->defineNameValues.ptrNonConst[j];
					CharString temp = CharString_createNull();

					if(!CharString_isRef(*curr))
						continue;

					gotoIfError2(clean, CharString_createCopy(*curr, alloc, &temp))
					*curr = temp;
				}

				//Copy name if needed

				if(!CharString_isRef(entry->entry.name))
					continue;

				CharString temp = CharString_createNull();
				gotoIfError2(clean, CharString_createCopy(entry->entry.name, alloc, &temp))
				entry->entry.name = temp;
			}
		}
	}

clean:
	s_uccess &= compileResult.isSuccess;

	if(!s_uccess)
		ListSHEntryRuntime_freeUnderlying(shEntriesRuntime, alloc);

	CompileResult_free(&compileResult, alloc);
	Error_print(alloc, errTemp, ELogLevel_Error, ELogOptions_Default);
	return s_uccess;
}

Bool Compiler_getUniqueCompiles(
	ListSHEntryRuntime runtimeEntries,
	ListU32 *compileCombinations,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListSHBinaryIdentifier identifiers = (ListSHBinaryIdentifier) { 0 };

	//Go through each compile combination.
	//Linking is a different story;
	//Each combination of uniforms and entrypoints will have to be linked later.

	for (U64 i = 0; i < runtimeEntries.length; ++i) {

		if(i >> 15)
			retError(clean, Error_overflow(0, i, 1 << 15, "Compiler_getUniqueCompiles() i out of bounds"))

		SHEntryRuntime runtime = runtimeEntries.ptr[i];

		//Note: Since compiled combinations excludes uniforms, it'll "compile" for uniformId 0 which will later be
		//        explicitly linked for the real uniform information.
		//        This means the compiler will only do 1 compile step, but multiple linking steps for the uniforms.

		for (U64 j = 0; j < SHEntryRuntime_getCombinationsCompiled(runtime); ++j) {

			if(j >> 15)
				retError(clean, Error_overflow(1, j, 1 << 15, "Compiler_getUniqueCompiles() j out of bounds"))

			SHBinaryIdentifier binaryIdentifier = (SHBinaryIdentifier) { 0 };
			gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(&runtime, (U16) j, &binaryIdentifier, e_rr))

			//Find SHBinaryIdentifier or not

			U64 k = 0;

			for(; k < identifiers.length; ++k)    //TODO: This one should combine compilations if isShaderAnnotation
				if(SHBinaryIdentifier_equals(binaryIdentifier, identifiers.ptr[k]))
					break;

			//When it's new, we gotta remember the binary identifier for reuse.
			//Other than that, we need to tell the compiler which runtime id and combination id we need

			if(k == identifiers.length) {

				gotoIfError2(clean, ListSHBinaryIdentifier_pushBack(&identifiers, binaryIdentifier, alloc))

				if(compileCombinations)
					gotoIfError2(clean, ListU32_pushBack(compileCombinations, (U32)(j | (i << 16)), alloc))
			}

			//Update flags of compileCombinations->ptr[k] stored in bit 15 (isRt), 31 (isGfxOrComp)
			
			if(compileCombinations) {

				Bool isRt =
					runtime.entry.stage >= ESHPipelineStage_RtStartExt && runtime.entry.stage <= ESHPipelineStage_RtEndExt;

				if (isRt)
					compileCombinations->ptrNonConst[k] |= 1 << 15;

				if (!isRt && runtime.entry.stage != ESHPipelineStage_WorkgraphExt)
					compileCombinations->ptrNonConst[k] |= 1 << 31;
			}
		}
	}

clean:
	//We will never allocate nested memory into this, so it's fine to just free it (no underlying data)
	ListSHBinaryIdentifier_free(&identifiers, alloc);
	return s_uccess;
}

void Compiler_printErrors(ListCompileError errors, Allocator alloc) {
	
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
	Allocator alloc,
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
	Compiler compiler,
	Buffer binary,
	Bool showAll,
	ListCompilerEntrypoint *uniqueEntrypoints,
	Allocator alloc,
	Error *e_rr
);

Bool Compiler_getUniqueEntrypointsSPIRV(
	Compiler compiler,
	Buffer binary,
	Bool showAll,
	ListCompilerEntrypoint *uniqueEntrypoints,
	Allocator alloc,
	Error *e_rr
);

Bool Compiler_getUniqueEntrypoints(
	Compiler compiler,
	ESHBinaryType binaryType,
	Buffer binary,
	Bool showAll,
	ListCompilerEntrypoint *uniqueEntrypoints,
	Allocator alloc,
	Error *e_rr
) {
	
	Bool s_uccess = true;

	switch (binaryType) {

		case ESHBinaryType_SPIRV:
			gotoIfError3(clean, Compiler_getUniqueEntrypointsSPIRV(compiler, binary, showAll, uniqueEntrypoints, alloc, e_rr))
			break;

		case ESHBinaryType_DXIL:
			gotoIfError3(clean, Compiler_getUniqueEntrypointsDXIL(compiler, binary, showAll, uniqueEntrypoints, alloc, e_rr))
			break;

		default:
			retError(clean, Error_unimplemented(0, "Compiler_getUniqueEntrypoints() has invalid type"))
	}

clean:
	return s_uccess;
}

Bool Compiler_compileShaderSingle(
	Compiler compiler,
	ESHBinaryType binaryType,
	Bool isDebug,
	Bool isRt,
	Bool isGfxOrComp,
	CharString inputPath,
	CharString input,
	CompileResult *dest,
	ListSHEntryRuntime runtimeEntries,
	U16 runtimeEntryId,
	U16 combinationId,
	CharString includeDir,
	Bool enableLogging,
	Allocator alloc
) {

	Error errTemp = Error_none(), *e_rr = &errTemp;
	Bool s_uccess = true;

	if(!dest)
		retError(clean, Error_nullPointer(5, "Compiler_compileShaderSingle()::dest is required"));

	if(dest->binary.ptr || dest->compileErrors.ptr || dest->includeInfo.ptr)
		retError(clean, Error_invalidParameter(5, 0, "Compiler_compileShaderSingle()::dest was present, but not empty"))

	CompilerSettings settings = (CompilerSettings) {
		.string = input,
		.path = inputPath,
		.debug = isDebug,
		.isRt = isRt,
		.containsGfxOrComp = isGfxOrComp,
		.format = ECompilerFormat_HLSL,
		.outputType = binaryType,
		.infoAboutIncludes = true,        //Required to supply oiSH info about includes
		.includeDir = includeDir
	};

	//First we need to go from text with includes and defines to easy to parse text
	//Accessing SHEntryRuntime here without locking is safe, since we don't access these properties from asBinaryIdentifier

	SHEntryRuntime entry = runtimeEntries.ptrNonConst[runtimeEntryId];
	SHBinaryIdentifier binaryIdentifier = (SHBinaryIdentifier) { 0 };
	gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(&entry, combinationId, &binaryIdentifier, e_rr))

	settings.isLib = entry.isShaderAnnotation;
	settings.containsGfxOrComp = entry.containsGfxOrComp;
	settings.isRt = entry.isRt;

	gotoIfError3(clean, Compiler_compile(compiler, settings, binaryIdentifier, alloc, dest, e_rr))

	if(enableLogging)
		Compiler_printErrors(dest->compileErrors, alloc);

clean:

	s_uccess &= dest && dest->isSuccess;

	if(enableLogging)
		Compiler_logStatus(binaryType, "Compile", inputPath, runtimeEntryId, combinationId, alloc, s_uccess);
		
	Error_print(alloc, errTemp, ELogLevel_Error, ELogOptions_Default);
	return s_uccess;
}

Bool Compiler_linkSingle(
	Compiler compiler,
	CharString path,
	U16 runtimeEntryId,
	U16 combinationId,
	ESHBinaryType type,
	ListBuffer inputs,
	ListSHUniformRuntime uniforms,
	Buffer uniformData,
	CharString entrypoint,
	U16 shaderVersion,
	ESHPipelineStage stageType,
	ESHExtension exts,
	Bool enableLogging,
	Buffer *result,
	Allocator alloc
) {

	Error errTemp = Error_none(), *e_rr = &errTemp;
	Bool s_uccess = true;
	ListCompileError errors = (ListCompileError) { 0 };

	if(!result)
		retError(clean, Error_nullPointer(5, "Compiler_linkSingle()::result is required"));

	if(result->ptr)
		retError(clean, Error_invalidParameter(5, 0, "Compiler_linkSingle()::result was present, but not empty"))

	gotoIfError3(clean, Compiler_link(
		compiler, type, inputs, uniforms, uniformData, entrypoint, shaderVersion, stageType, exts, &errors, result,
		alloc, e_rr
	))

	if (enableLogging)
		Compiler_printErrors(errors, alloc);

clean:

	if (enableLogging)
		Compiler_logStatus(type, "Link", path, runtimeEntryId, combinationId, alloc, s_uccess);
		
	Error_print(alloc, errTemp, ELogLevel_Error, ELogOptions_Default);
	ListCompileError_freeUnderlying(&errors, alloc);
	return s_uccess;
}

Bool Compiler_processSingle(
	Compiler compiler,
	CharString path,
	U16 runtimeEntryId,
	U16 combinationId,
	ESHBinaryType binaryType,
	CompileResult *tempResult,
	Bool isDebug,
	SHBinaryIdentifier binaryIdentifier,
	SpinLock *lock,
	ListSHEntryRuntime runtimeEntries,
	Bool isShaderAnnotation,
	Bool enableLogging,
	Allocator alloc,
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
		binaryIdentifier,
		lock,
		runtimeEntries,
		isShaderAnnotation,
		&tempResult->demotion,
		&errors,
		alloc,
		e_rr
	))
		
	if (enableLogging)
		Compiler_printErrors(errors, alloc);

clean:
	
	if (enableLogging)
		Compiler_logStatus(binaryType, "Process", path, runtimeEntryId, combinationId, alloc, s_uccess);
	
	ListCompileError_freeUnderlying(&errors, alloc);
	return s_uccess;
}

TList(ListSHEntryRuntime);
TListImpl(ListSHEntryRuntime);

typedef struct LinkEntry {
	ListU16 runtimeEntries;
	Buffer uniformData;
	U16 combinationId, entrypointId;
	U8 padding[4];
} LinkEntry;

TList(LinkEntry);
TListImpl(LinkEntry);

void ListListSHEntryRuntime_freeUnderlying(ListListSHEntryRuntime *entry, Allocator alloc) {

	if(!entry)
		return;

	for(U64 i = 0; i < entry->length; ++i)
		ListSHEntryRuntime_freeUnderlying(&entry->ptrNonConst[i], alloc);

	ListListSHEntryRuntime_free(entry, alloc);
}

void ListLinkEntry_freeUnderlying(ListLinkEntry* entries, Allocator alloc) {

	if (!entries)
		return;

	for (U64 i = 0; i < entries->length; ++i) {
		LinkEntry* entry = &entries->ptrNonConst[i];
		Buffer_free(&entry->uniformData, alloc);
		ListU16_free(&entry->runtimeEntries, alloc);
	}

	ListLinkEntry_free(entries, alloc);
}

Bool Compiler_getLinkEntries(
	Compiler compiler,
	const ListSHEntryRuntime *runtimeEntries,
	const SHBinaryIdentifier *binaryIdentifier,
	ESHBinaryType binaryType,
	Buffer *binary,
	ListCompilerEntrypoint *entrypoints,
	ListLinkEntry *linkEntries,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListU16 tmpEntries = (ListU16) { 0 };
	Bool freeEntrypoints = false;
	Bool freeLinkEntries = false;

	Bool isRt =
		binaryIdentifier->stageType >= ESHPipelineStage_RtStartExt &&
		binaryIdentifier->stageType <= ESHPipelineStage_RtEndExt;

	Bool isLib =
		binaryIdentifier->stageType == ESHPipelineStage_WorkgraphExt ||
		isRt;

	Bool isLibTarget = isLib;
	isLib = isLibTarget || (binaryIdentifier->stageType == ESHPipelineStage_Count);

	if (!isLib) {
		
		gotoIfError2(clean, ListCompilerEntrypoint_pushBack(
			entrypoints, (CompilerEntrypoint) { .stage = binaryIdentifier->stageType }, alloc
		))

		freeEntrypoints = true;

		gotoIfError2(clean, CharString_createCopy(
			binaryIdentifier->entrypoint,
			alloc,
			&ListCompilerEntrypoint_last(*entrypoints)->name
		))
	}
	
	else gotoIfError3(clean, Compiler_getUniqueEntrypoints(compiler, binaryType, *binary, true, entrypoints, alloc, e_rr))

	freeEntrypoints = true;

	ListCompilerEntrypoint entrypointL = *entrypoints;
	ListSHEntryRuntime runtimeEntryL = *runtimeEntries;
	SHBinaryIdentifier ident = *binaryIdentifier;

	for (U64 i = 0; i < entrypointL.length; ++i) {

		CompilerEntrypoint entrypoint = entrypointL.ptr[i];

		//Find entrypoint in input array and ensure it exists / is the same stage

		U64 j = 0;

		for (; j < runtimeEntryL.length; ++j)
			if (CharString_equalsStringSensitive(entrypoint.name, runtimeEntryL.ptr[j].entry.name))
				break;

		if (j == runtimeEntryL.length)
			retError(clean, Error_invalidState(
				0, "Compiler_getLinkEntries() had an entrypoint that wasn't defined while parsing but is present in reflection"
			))

		SHEntryRuntime entry = runtimeEntryL.ptr[j];

		if(entry.entry.stage != entrypoint.stage)
			retError(clean, Error_invalidState(
				0, "Compiler_getLinkEntries() had a reflection stage type that mismatched with what was parsed"
			))

		//Ensure we're actually present for what we're currently compiling and that we do really need linking (otherwise skip)
		//This is not relevant for single entrypoints, as they're always only compiled with the defines / extensions they need.
		//However, if you have a mix of extensions and defines, then some entrypoints might not need to be linked again.
		//Example: raygen with both SER and no SER. This would only be linked once per compilation,
		//            but any other shaders should exclude this.
		//            (We don't want to have 2x hit shaders included while only raygen needs these compilations)

		//Check extensions

		Bool containsExtension = !ident.extensions && !entry.extensions.length;
		U16 extensionId = 0;

		for(U64 k = 0; k < entry.extensions.length; ++k)
			if (entry.extensions.ptr[k] == (U32) ident.extensions) {
				containsExtension = true;
				extensionId = (U16) k;
				break;
			}

		if (!containsExtension)            //Extension not found
			continue;

		//Check shader versions

		Bool containsShaderVersion = ident.shaderVersion == OISH_SHADER_MODEL(6, 5) && !entry.shaderVersions.length;
		U16 shaderVersion = 0;

		for(U64 k = 0; k < entry.shaderVersions.length; ++k)
			if (entry.shaderVersions.ptr[k] == ident.shaderVersion) {
				containsShaderVersion = true;
				shaderVersion = (U16) k;
				break;
			}

		if (!containsShaderVersion)        //Shader model not found
			continue;

		//Check defines

		Bool containsDefines = !ident.defines.length && !entry.definesPerCompilation.length;
		U16 defineId = 0;

		for (U64 k = 0, l = 0; k < entry.definesPerCompilation.length; ++k) {

			U64 m = entry.definesPerCompilation.ptr[k];

			ListCharString tmp = (ListCharString) { 0 };
			gotoIfError2(clean, ListCharString_createRefConst(entry.defineNameValues.ptr + (l << 1), m << 1, &tmp))

			Bool eq = tmp.length == ident.defines.length;        //TODO: ListCharString_equalsUnderlying

			if (eq)
				for (U64 n = 0; n < tmp.length; ++n)
					if (!CharString_equalsStringSensitive(tmp.ptr[n], ident.defines.ptr[n])) {
						eq = false;
						break;
					}

			if (eq) {
				defineId = (U16) k;
				containsDefines = true;
				break;
			}

			l += m;
		}

		if (!containsDefines)            //Defines not found
			continue;

		//Go through all uniforms defined by the runtime, since there may be multiple

		U16 shaderVersions = (U16)U64_max(entry.shaderVersions.length, 1);
		U16 extensions = (U16)U64_max(entry.extensions.length, 1);
		U16 defines = (U16)U64_max(entry.definesPerCompilation.length, 1);
		U64 uniformCombos = U64_safeDiv(entry.uniformData.length, entry.uniformStride);

		for (U64 k = 0; k < U64_max(1, uniformCombos); ++k) {

			U64 combinationId = ((k * defines + defineId) * extensions + extensionId) * shaderVersions + shaderVersion;

			LinkEntry linkEntry = (LinkEntry) {
				.uniformData = Buffer_createRefConst(entry.uniformData.ptr + entry.uniformStride * k, entry.uniformStride),
				.combinationId = (U16) combinationId
			};

			if (!isLibTarget) {

				linkEntry.entrypointId = (U16)j;

				U64 l = 0;

				for (; l < runtimeEntryL.length; ++l)
					if (CharString_equalsStringSensitive(runtimeEntryL.ptr[l].entry.name, entrypoint.name))
						break;

				if(l == runtimeEntryL.length)
					retError(clean, Error_invalidState(
						0, "Compiler_getLinkEntries() had an entrypoint that couldn't be found in runtime entry"
					))

				//The ptr below is the same as linkEntry.entrypointId, except can be used as ptr to avoid intermediate ListU16
				gotoIfError2(clean, ListU16_createRefConst(&runtimeEntryL.ptr[l].entry.idOrPadding, 1, &linkEntry.runtimeEntries))
			}

			else {

				//If RT/workgraph shader, try to find a previous linkEntry
				//In that case, we just reference the same binary.

				U64 l = 0;

				for (; l < linkEntries->length; ++l) {

					LinkEntry linkEntry2 = linkEntries->ptr[l];

					if (linkEntry2.entrypointId != U16_MAX)
						continue;

					if (!Buffer_eq(linkEntry.uniformData, linkEntry2.uniformData))
						continue;

					break;
				}

				if (l != linkEntries->length) {
					gotoIfError2(clean, ListU16_pushBack(&linkEntries->ptrNonConst[k].runtimeEntries, (U16)j, alloc))
					continue;
				}

				linkEntry.entrypointId = U16_MAX;
				gotoIfError2(clean, ListU16_pushBack(&tmpEntries, (U16)j, alloc))
				linkEntry.runtimeEntries = tmpEntries;
			}

			gotoIfError2(clean, ListLinkEntry_pushBack(linkEntries, linkEntry, alloc))
			tmpEntries = (ListU16) { 0 };    //Moved
			freeLinkEntries = true;
		}
	}

	if (linkEntries->length >> 32)
		retError(clean, Error_invalidState(0, "Compiler_getLinkEntries() must return <32bit entries"));

clean:

	if (!s_uccess) {

		if (freeEntrypoints)
			ListCompilerEntrypoint_freeUnderlying(entrypoints, alloc);

		if (freeLinkEntries)
			ListLinkEntry_freeUnderlying(linkEntries, alloc);
	}

	ListU16_free(&tmpEntries, alloc);
	return s_uccess;
}

typedef struct CompilerJobScheduler {

	ListCharString inputPaths;
	ListCharString inputData;
	ListCharString outputPaths;
	ListU8 compileModes;
	ListCompiler compilers;

	ListListSHEntryRuntime *shEntries;

	//[U32 shEntries[i], U16 shEntries[i][j], U16 shEntries[i][j][k]]

	ListU64 *shEntryIds;
	ListU64 *shEntryIdsIntermediate;
	ListU64 *shEntryIdsIntermediate1;

	ListCompileResult *compileResults;
	ListCompileResult *compileResultsTemp;

	SpinLock *lock;
	U64 *counter;                        //This one is to acquire job ids
	U64 *completedCounter;                //This one is to signal jobs that are done
	U64 *counterCompiledBinaries;        //How many binaries have been compiled
	U64 *counterLinkedBinaries;            //How many binaries have been linked
	U64 *threadCounter;
	Bool *success;

	ECompileType compileType;
	Bool isDebug;
	Bool ignoreEmptyFiles;
	Bool enableLogging;
	U8 padding;

	CharString includeDir;

	Allocator alloc;

} CompilerJobScheduler;

void Compiler_compileJob(CompilerJobScheduler *job) {

	if(!job)
		return;

	Bool s_uccess = true;

	Allocator alloc = job->alloc;
	ELockAcquire acq = ELockAcquire_Invalid;
	U64 threadCounter = U64_MAX;
	CompileResult tmp = (CompileResult) { 0 };
	ListLinkEntry linkEntries = (ListLinkEntry) { 0 };
	ListCompilerEntrypoint uniqueEntrypoints = (ListCompilerEntrypoint) { 0 };
	Compiler compiler = (Compiler) { 0 };

	//Pre-compile
	//TODO: Test something that fails only in pre-compile and see what happens.

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

			ListSHEntryRuntime runtimeEntries = job->shEntries->ptr[lastJobId];
			ListU32 compileCombinations = (ListU32) { 0 };

			if(!Compiler_getUniqueCompiles(runtimeEntries, &compileCombinations, alloc, NULL)) {
				if(job->enableLogging)
					Log_errorLn(alloc, "Compiler_compileJob() failed (Compiler_getUniqueCompiles)");
			}

			//Add all compile combinations, but include our last job id to ensure it can be found again

			else if(compileCombinations.length) {

				if(ListU64_reserve(
					job->shEntryIdsIntermediate, job->shEntryIdsIntermediate->length + compileCombinations.length, alloc
				).genericError) {
					if(job->enableLogging)
						Log_errorLn(alloc, "Compiler_compileJob() failed (ListU64_reserve)");
				}

				else if(ListCompileResult_resize(
					job->compileResultsTemp, job->compileResultsTemp->length + compileCombinations.length, alloc
				).genericError) {
					if(job->enableLogging)
						Log_errorLn(alloc, "Compiler_compileJob() failed (ListCompileResult_resize)");
				}

				else for (U64 i = 0; i < compileCombinations.length; ++i) {

					U32 compileCombination = compileCombinations.ptr[i];

					if (ListU64_pushBack(
						job->shEntryIdsIntermediate, ((U64)lastJobId << 32) | compileCombination, (Allocator) { 0 }
					).genericError) {
						Log_errorLn(alloc, "Compiler_compileJob() failed (ListU64_pushBack)");
						break;
					}
				}
			}

			else if(!job->ignoreEmptyFiles) {

				if(job->enableLogging)
					Log_errorLn(
						alloc, "Precompile couldn't find entrypoints for file \"%.*s\"",
						(int)CharString_length(job->outputPaths.ptr[lastJobId]), job->outputPaths.ptr[lastJobId].ptr
					);

				s_uccess = false;
			}

			ListU32_free(&compileCombinations, alloc);

			//Mark as completed, so other threads will know

			++*job->completedCounter;
		}

		//Assign thread counter, because it might be needed later

		if (threadCounter == U64_MAX) {
			threadCounter = (*job->threadCounter)++;
			compiler = job->compilers.ptr[threadCounter];
		}

		//Remember if we succeeded or not

		if(*job->counter == job->inputData.length) {
			*job->success &= s_uccess;
			break;
		}

		if (*job->counter == U32_MAX) {

			Log_errorLn(
				alloc, "Precompile ran out of counters starting at file \"%.*s\"",
				(int)CharString_length(job->outputPaths.ptr[lastJobId]), job->outputPaths.ptr[lastJobId].ptr
			);

			*job->success = false;
			break;
		}

		U64 ourJobId = (*job->counter)++;

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(job->lock);

		acq = ELockAcquire_Invalid;
		lastJobId = ourJobId;

		//Process next. If compile failed, still continue. Because we want all compiler errors

		CharString input = job->inputPaths.ptr[ourJobId];

		if(!Compiler_precompileShader(
			compiler,
			job->compileModes.ptr[ourJobId],
			job->isDebug,
			input,
			job->inputData.ptr[ourJobId],
			&job->shEntries->ptrNonConst[ourJobId],
			job->includeDir,
			job->enableLogging,
			alloc
		)) {

			if(job->enableLogging)
				Log_errorLn(alloc, "Compile failed for file \"%.*s\"", (int)CharString_length(input), input.ptr);

			s_uccess = false;
		}
	}

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(job->lock);

	acq = ELockAcquire_Invalid;

	//Compile requires another stage where we go over unique binaries that we have to compile

	lastJobId = U64_MAX;

	while(true) {

		acq = SpinLock_lock(job->lock, U64_MAX);

		if(acq < ELockAcquire_Success)
			goto clean;

		//Now that we have the lock, we can write back the result

		if (lastJobId != U64_MAX) {

			//Figure out if linking is even necessary; only for lib targets

			U64 ourLast = job->shEntryIdsIntermediate->ptr[lastJobId];

			U32 ourOldJobId = ourLast >> 32;
			ListSHEntryRuntime entries = job->shEntries->ptr[ourOldJobId];

			U16 runtimeEntryId = (U16)(ourLast >> 16) & (U16)I16_MAX;
			SHEntryRuntime entry = entries.ptrNonConst[runtimeEntryId];

			if (entry.isShaderAnnotation && tmp.isSuccess) {

				CharString input = job->inputPaths.ptr[ourOldJobId];

				if (tmp.type != ECompileResultType_Binary) {

					if (job->enableLogging)
						Log_errorLn(
							alloc, "Compile failed for file \"%.*s\", invalid compile output",
							(int)CharString_length(input), input.ptr
						);

					s_uccess = false;

				} else {

					U16 combinationId = (U16)(ourLast & (U16)I16_MAX);

					SHBinaryIdentifier binaryIdentifier = (SHBinaryIdentifier){ 0 };

					if (!SHEntryRuntime_asBinaryIdentifier(&entry, combinationId, &binaryIdentifier, NULL)) {

						Log_errorLn(
							alloc, "Compile failed for file \"%.*s\", invalid binary identifier",
							(int)CharString_length(input), input.ptr
						);

						s_uccess = false;
					}

					else {
					
						Error errTemp = Error_none();

						if (Compiler_getLinkEntries(
							compiler,
							&entries,
							&binaryIdentifier,
							job->compileModes.ptr[ourOldJobId],
							&tmp.binary,
							&uniqueEntrypoints,
							&linkEntries,
							alloc,
							&errTemp
						)) {

							//Setup next pass

							if (ListU64_reserve(
								job->shEntryIdsIntermediate1, job->shEntryIdsIntermediate1->length + linkEntries.length, alloc
							).genericError) {
								if (job->enableLogging)
									Log_errorLn(alloc, "Compiler_compileJob() failed (ListU64_reserve)");
							}

							else if (ListCompileResult_resize(
								job->compileResults, job->compileResults->length + linkEntries.length, alloc
							).genericError) {
								if (job->enableLogging)
									Log_errorLn(alloc, "Compiler_compileJob() failed (ListCompileResult_resize)");
							}

							else for (U64 i = 0; i < linkEntries.length; ++i) {

								if (ListU64_pushBack(
									job->shEntryIdsIntermediate1, ((U64)lastJobId << 32) | (U32)i, (Allocator) { 0 }
								).genericError) {
									Log_errorLn(alloc, "Compiler_compileJob() failed (ListU64_pushBack)");
									break;
								}
							}

						} else {

							Log_errorLn(
								alloc, "Compile failed for file \"%.*s\", invalid link entries",
								(int)CharString_length(input), input.ptr
							);

							s_uccess = false;
							Error_print(alloc, errTemp, ELogLevel_Error, ELogOptions_Default);
						}

						ListLinkEntry_freeUnderlying(&linkEntries, alloc);
						ListCompilerEntrypoint_freeUnderlying(&uniqueEntrypoints, alloc);
					}
				}
			}

			//Move our binary we compiled to the registry directly without linking.
			//This happens when compiling an oxc::stage or when an error occurs.

			else if (ListU64_pushBack(job->shEntryIds, ourLast, (Allocator) { 0 }).genericError) {
				//TODO: Move to job->compileResults
				Log_errorLn(alloc, "Compiler_compileJob() failed (ListU64_pushBack)");
				break;
			}

			job->compileResultsTemp->ptrNonConst[lastJobId] = tmp;
			tmp = (CompileResult) { 0 };
		}

		//We're done, since all initial threads are done spawning children.
		//And those children have also finished.

		if(
			*job->completedCounter == job->inputData.length &&
			*job->counterCompiledBinaries == job->shEntryIdsIntermediate->length
		)
			break;

		//We currently don't have any pending, more will likely spawn.
		//Come back a bit later to try again.

		if(*job->counterCompiledBinaries == job->shEntryIdsIntermediate->length) {

			if(acq == ELockAcquire_Acquired)        //Release lock to prevent deadlock
				SpinLock_unlock(job->lock);

			acq = ELockAcquire_Invalid;

			Thread_sleep(100 * MU);
			continue;
		}

		//Grab next job id

		U64 ourJobId = (*job->counterCompiledBinaries)++;
		U64 ourNext = job->shEntryIdsIntermediate->ptr[ourJobId];

		U32 ourOldJobId = ourNext >> 32;
		ListSHEntryRuntime entries = job->shEntries->ptr[ourOldJobId];        //Grab entries as shEntries can be modified

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(job->lock);

		acq = ELockAcquire_Invalid;

		//Unpack as ListSHEntryRuntime, then SHEntryRuntime, then the specific job

		U16 runtimeEntryId = (U16)(ourNext >> 16);
		U16 combinationId = (U16) ourNext;

		Bool isRt = combinationId >> 15;
		Bool isGfxOrComp = runtimeEntryId >> 15;

		runtimeEntryId &= (U16)I16_MAX;
		combinationId &= (U16)I16_MAX;

		CharString input = job->inputPaths.ptr[ourOldJobId];

		lastJobId = ourJobId;

		if(!Compiler_compileShaderSingle(
			compiler,
			job->compileModes.ptr[ourOldJobId],
			job->isDebug,
			isRt,
			isGfxOrComp,
			input,
			job->inputData.ptr[ourOldJobId],
			&tmp,
			entries,
			runtimeEntryId,
			combinationId,
			job->includeDir,
			job->enableLogging,
			alloc
		)) {
			s_uccess = false;

			if(job->enableLogging)
				Log_errorLn(alloc, "Compile failed for file \"%.*s\"", (int)CharString_length(input), input.ptr);
		}
	}

	//Linking stage, our previous compile step has hopefully spawned more.
	//If no linking is necessary, it moves shEntryIdsIntermediate -> shEntryIds.
	//If an error occurred in the previous step, it will not execute.
	//Otherwise it'll link per link entry.
	//If errors occur while linking, it'll merge the compile errors.

	lastJobId = U64_MAX;
	
	while (true) {

		//Lock to get next job id

		acq = SpinLock_lock(job->lock, U64_MAX);

		if (acq < ELockAcquire_Success)
			return;

		//Now that we have the lock, we can write back the result

		if (lastJobId != U64_MAX) {
			job->compileResults->ptrNonConst[lastJobId] = tmp;
			tmp = (CompileResult){ 0 };
		}
		
		//We're done, since all initial threads are done spawning children.
		//And those children's children have also finished.

		if(
			*job->completedCounter == job->inputData.length &&
			*job->counterCompiledBinaries == job->shEntryIdsIntermediate->length &&
			*job->counterLinkedBinaries == job->shEntryIdsIntermediate1->length
		)
			break;

		//We currently don't have any pending, more will likely spawn.
		//Come back a bit later to try again.

		if(*job->counterLinkedBinaries == job->shEntryIdsIntermediate1->length) {

			if(acq == ELockAcquire_Acquired)        //Release lock to prevent deadlock
				SpinLock_unlock(job->lock);

			acq = ELockAcquire_Invalid;

			Thread_sleep(100 * MU);
			continue;
		}

		U64 ourJobId = (*job->counterLinkedBinaries)++;
		U64 ourNext = job->shEntryIdsIntermediate1->ptr[ourJobId];        //((U64)lastJobId << 32) | (U32)linkId

		U32 compileJobId = (U32)(ourNext >> 32);
		U32 linkEntryId = (U32)ourNext;

		ourNext = job->shEntryIdsIntermediate->ptr[compileJobId];
		CompileResult result = job->compileResultsTemp->ptr[compileJobId];

		U32 ourOldJobId = ourNext >> 32;
		ListSHEntryRuntime entries = job->shEntries->ptr[ourOldJobId];

		if (acq == ELockAcquire_Acquired)
			SpinLock_unlock(job->lock);

		acq = ELockAcquire_Invalid;
		lastJobId = ourJobId;

		//Link binaries

		U16 runtimeEntryId = (U16)(ourNext >> 16) & (U16)I16_MAX;
		SHEntryRuntime entry = entries.ptrNonConst[runtimeEntryId];

		U16 combinationId = (U16)(ourNext & (U16)I16_MAX);

		SHBinaryIdentifier binaryIdentifier = (SHBinaryIdentifier){ 0 };
		CharString inputPath = job->inputPaths.ptr[ourOldJobId];

		if (!SHEntryRuntime_asBinaryIdentifier(&entry, combinationId, &binaryIdentifier, NULL)) {

			Log_errorLn(
				alloc, "Link failed for file \"%.*s\", invalid binary identifier",
				(int)CharString_length(inputPath), inputPath.ptr
			);

			s_uccess = false;
		}

		else {

			ESHBinaryType binaryType = job->compileModes.ptr[ourOldJobId];

			Error errTemp = Error_none();

			if (Compiler_getLinkEntries(
				compiler,
				&entries,
				&binaryIdentifier,
				binaryType,
				&result.binary,
				&uniqueEntrypoints,
				&linkEntries,
				alloc,
				&errTemp
			)) {

				//Link

				LinkEntry linkEntry = linkEntries.ptr[linkEntryId];
				
				Buffer uniformData = linkEntry.uniformData;

				U16 currentCombinationId = linkEntry.combinationId;

				binaryIdentifier = (SHBinaryIdentifier) { 0 };
				SHEntryRuntime_asBinaryIdentifier(        //TODO: error handling
					&entry, currentCombinationId, &binaryIdentifier, NULL
				);

				ListBuffer inputs = (ListBuffer) { 0 };
				ListBuffer_createRefConst(&result.binary, 1, &inputs);    //TODO: Error handling

				CompilerEntrypoint entrypoint = (CompilerEntrypoint) { 0 };
							
				if (linkEntry.entrypointId != U16_MAX) {

					//entrypointId doesn't map to uniqueEntrypoints as some might be missing there;
					//it maps to our parsed runtimeEntries.

					CharString entrypointName = entries.ptr[linkEntry.entrypointId].entry.name;

					U64 l = 0;

					for (; l < uniqueEntrypoints.length; ++l)
						if (CharString_equalsStringSensitive(entrypointName, uniqueEntrypoints.ptr[l].name))
							break;

					//TODO: Error handling
					//if (l == uniqueEntrypoints.length)
					//    retError(clean, Error_invalidState(
					//        0,
					//        "Compiler_compileShaders() somehow an entrypointId was referenced by a linkEntry "
					//        "that doesn't exist"
					//    ));

					entrypoint = uniqueEntrypoints.ptr[l];
				}

				else entrypoint.stage = ESHPipelineStage_Count;        //Mark as lib

				tmp.type = ECompileResultType_Binary;
				if (!Compiler_linkSingle(
					compiler,
					inputPath,
					runtimeEntryId,
					combinationId,
					binaryType,
					inputs,
					entry.uniforms,
					uniformData,
					entrypoint.name,
					binaryIdentifier.shaderVersion,
					entrypoint.stage,
					binaryIdentifier.extensions,
					job->enableLogging,
					&tmp.binary,
					alloc
				)) {
					Log_errorLn(alloc, "Link failed for file \"%.*s\"", (int)CharString_length(inputPath), inputPath.ptr);
					s_uccess = false;
				}

			} else {

				Log_errorLn(
					alloc, "Compile failed for file \"%.*s\", invalid link entries",
					(int)CharString_length(inputPath), inputPath.ptr
				);

				s_uccess = false;
				Error_print(alloc, errTemp, ELogLevel_Error, ELogOptions_Default);
			}

			ListLinkEntry_freeUnderlying(&linkEntries, alloc);
			ListCompilerEntrypoint_freeUnderlying(&uniqueEntrypoints, alloc);
		}
	}

	if (acq == ELockAcquire_Acquired)
		SpinLock_unlock(job->lock);

	acq = ELockAcquire_Invalid;

clean:
	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(job->lock);

	acq = ELockAcquire_Invalid;
	CompileResult_free(&tmp, alloc);
	ListLinkEntry_freeUnderlying(&linkEntries, alloc);
	ListCompilerEntrypoint_freeUnderlying(&uniqueEntrypoints, alloc);
}

Bool Compiler_registerShaderBinary(
	SHFile *shFile,
	CompileResult *tempResult,
	ESHBinaryType compileMode,
	CharString sourceFile,
	const SHEntryRuntime *runtimeEntry,
	const SHBinaryIdentifier *binaryIdentifier,        //Make sure this binary identifier only contains references
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	CharString tempStr = CharString_createNull();
	SHInclude shInclude = (SHInclude) { 0 };
	SHBinaryInfo binaryInfo = (SHBinaryInfo) { 0 };

	if(tempResult->type != ECompileResultType_Binary)
		retError(clean, Error_invalidState(0, "Compiler_registerShaderBinary() should return binary"))

	gotoIfError3(clean, SHEntryRuntime_asBinaryInfo(
		runtimeEntry, binaryIdentifier, compileMode, tempResult->binary, tempResult->demotion, &binaryInfo, e_rr
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

			gotoIfError3(clean, File_makeRelative(
				Platform_instance->defaultDir, sourceFile, shInclude.relativePath, 256, alloc, &tempStr, e_rr
			))

			CharString_free(&shInclude.relativePath, alloc);
			shInclude.relativePath = tempStr;
			tempStr = CharString_createNull();
		}

		gotoIfError3(clean, SHFile_addInclude(shFile, &shInclude, alloc, e_rr))
	}

	//Move binary there to avoid copying mem if possible

	binaryInfo.registers = tempResult->registers;
	binaryInfo.binaries[compileMode] = tempResult->binary;
	gotoIfError3(clean, SHFile_addBinary(shFile, &binaryInfo, alloc, e_rr))
	tempResult->binary = Buffer_createNull();
	tempResult->registers = (ListSHRegisterRuntime) { 0 };

	CompileResult_free(tempResult, alloc);

clean:
	SHInclude_free(&shInclude, alloc);
	SHBinaryInfo_free(&binaryInfo, alloc);
	CharString_free(&tempStr, alloc);
	return s_uccess;
}

Bool Compiler_registerShaderEntries(
	SHFile *shFile,
	ListSHEntryRuntime entries,
	ListU32 binaryIndices,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListU16 binaryIndicesShort = (ListU16) { 0 };

	for (U64 j = 0, k = 0; j < entries.length; ++j) {

		SHEntryRuntime *runtime = &entries.ptrNonConst[j];

		U32 l = SHEntryRuntime_getCombinations(*runtime);

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
			))

		if((binaryIndices.ptr[k + l - 1] >> 16) != j)
			retError(clean, Error_invalidState(0, "CLI_compileShader() has missing binaries for index j"))

		if(runtime->entry.binaryIds.length)
			retError(clean, Error_invalidOperation(
				0, "CLI_compileShader() runtime already included binaryIds"
			))

		gotoIfError2(clean, ListU16_resize(&binaryIndicesShort, l, alloc))

		for(U64 m = 0; m < l; ++m)
			binaryIndicesShort.ptrNonConst[m] = (U16) binaryIndices.ptr[k + m];

		runtime->entry.binaryIds = ListU16_createRefFromList(binaryIndicesShort);
		gotoIfError3(clean, SHFile_addEntrypoint(shFile, &runtime->entry, alloc, e_rr))

		k += l;
	}

clean:
	ListU16_free(&binaryIndicesShort, alloc);
	return s_uccess;
}

Bool Compiler_getTargetsFromFile(
	CharString input,
	ECompileType compileType,
	U64 compileModeU64,
	Bool multipleModes,
	Bool combineFlag,
	Bool enableLogging,
	Allocator alloc,
	Bool *isFolder,
	CharString *output,
	ListCharString *allFiles,
	ListCharString *allShaderText,
	ListCharString *allOutputs,
	ListU8 *allCompileModes
) {

	Bool s_uccess = true;

	if (!allCompileModes || !allFiles || !allShaderText || !allOutputs) {
		if(enableLogging) Log_debugLn(alloc, "Compiler_getTargetsFromFile one of outputs is missing");
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
		gotoIfError3(clean, File_resolve(input, &isVirtual, 128, Platform_instance->defaultDir, alloc, &resolved, e_rr))
		gotoIfError2(clean, CharString_append(&resolved, '/', alloc))

		if(output) {
			gotoIfError3(clean, File_resolve(*output, &isVirtual, 128, Platform_instance->defaultDir, alloc, &resolved2, e_rr))
			gotoIfError2(clean, CharString_append(&resolved2, '/', alloc))
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
			.compileType = compileType,
			.alloc = alloc
		};

		gotoIfError3(clean, File_foreach(
			input,
			false,
			(FileCallback) registerFile,
			&shaderFileRecursion,
			true,
			e_rr
		))

		//Make sure we can have a folder at output

		if(output)
			gotoIfError3(clean, File_add(resolved2, EFileType_Folder, 1 * SECOND, false, alloc, e_rr))

		if(isFolder) *isFolder = true;
	}

	//We need to add multiple compile modes

	else for(U8 i = 0; i < ESHBinaryType_Count; ++i) {

		if(!((compileModeU64 >> i) & 1))
			continue;

		//Replace output's .hlsl by .spv.hlsl or .dxil.hlsl

		gotoIfError2(clean, CharString_format(
			alloc, &tempStr, "%.*s%s",
			output ? (int)U64_min(
				CharString_length(*output),
				CharString_findLastStringInsensitive(*output, CharString_createRefCStrConst(".hlsl"), 0, 0)
			) : (int)(sizeof("output") - 1),
			output ? output->ptr : "output",
			oiSHSuffixes[i]
		))

		//Register mode and input/output name

		gotoIfError2(clean, ListCharString_pushBack(allFiles, input, alloc))

		gotoIfError2(clean, ListCharString_pushBack(allOutputs, tempStr, alloc))        //Moved here
		tempStr = CharString_createNull();

		gotoIfError2(clean, ListU8_pushBack(allCompileModes, i, alloc))
	}

	//Only continue if there are any files and then fetch all files

	if (!allFiles->length)
		goto clean;

	CharString prevStr = CharString_createNull();

	for (U64 i = 0; i < allFiles->length; ++i) {

		//Grab from cache if we're re-compiling the same file with a different mode

		if (CharString_equalsStringSensitive(prevStr, allFiles->ptr[i])) {

			CharString shader = *ListCharString_last(*allShaderText);
			shader = CharString_createRefStrConst(shader);

			gotoIfError2(clean, ListCharString_pushBack(allShaderText, shader, alloc))
			continue;
		}

		//Otherwise grab from file

		gotoIfError3(clean, File_read(allFiles->ptr[i], 10 * MS, 0, 0, alloc, &temp, e_rr))

		if(!Buffer_length(temp)) {
			gotoIfError2(clean, ListCharString_pushBack(allShaderText, CharString_createNull(), alloc))
			continue;
		}

		gotoIfError2(clean, CharString_createCopy(
			CharString_createRefSizedConst((const C8*)temp.ptr, Buffer_length(temp), false), alloc, &tempStr
		))

		if(!CharString_eraseAllSensitive(&tempStr, '\r', 0, 0))
			retError(clean, Error_invalidState(1, "Compiler_getTargetsFromFile couldn't erase \\rs"))

		gotoIfError2(clean, ListCharString_pushBack(allShaderText, tempStr, alloc))
		tempStr = CharString_createNull();

		Buffer_free(&temp, alloc);

		prevStr = allFiles->ptr[i];
	}

clean:
	Error_print(alloc, errTmp, ELogLevel_Error, ELogOptions_Default);
	CharString_free(&resolved, alloc);
	CharString_free(&resolved2, alloc);
	Buffer_free(&temp, alloc);
	CharString_free(&tempStr, alloc);
	return s_uccess;
}

Bool Compiler_compileShaders(
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
	Bool enableLogging,
	Allocator alloc,
	ListBuffer *allBuffers,
	Error *e_rr
) {
	ListThread threads = (ListThread) { 0 };
	ListCompiler compilers = (ListCompiler) { 0 };
	ListListSHEntryRuntime shEntries = (ListListSHEntryRuntime) { 0 };

	ListU64 shEntryIdsIntermediate = (ListU64) { 0 };                    //Precompile
	ListU64 shEntryIdsIntermediate1 = (ListU64) { 0 };                    //Pre-link
	ListU64 shEntryIds = (ListU64) { 0 };                                //Post-link
	ListU64 shEntryIdsSorted = (ListU64) { 0 };
	ListCompileResult compileResults = (ListCompileResult) { 0 };        //Post-link
	ListCompileResult compileResultsTemp = (ListCompileResult) { 0 };    //Pre-link
	CompileResult tempResult = (CompileResult) { 0 };
	CompileResult tempResult2 = (CompileResult) { 0 };

	Compiler compiler = (Compiler) { 0 };
	ListU32 compileCombinations = (ListU32) { 0 };
	ListU32 binaryIndices = (ListU32) { 0 };
	SHEntry shEntry = (SHEntry) { 0 };

	SHFile shFile = (SHFile) { 0 };
	SHFile previous = (SHFile) { 0 };
	Bool errorInPrevious = false;

	Buffer temp = Buffer_createNull();
	SpinLock lock = (SpinLock) { 0 };

	ListSHEntryRuntime runtimeEntries = (ListSHEntryRuntime) { 0 };
	ListLinkEntry linkEntries = (ListLinkEntry) { 0 };
	ListCompilerEntrypoint uniqueEntrypoints = (ListCompilerEntrypoint) { 0 };

	CharString tempStr = CharString_createNull();
	CharString tempStr2 = CharString_createNull();
	CharString tempStr3 = CharString_createNull();

	Bool s_uccess = true;

	if(allBuffers)
		gotoIfError2(clean, ListBuffer_resize(allBuffers, allOutputs.length, alloc))

	//Spin up threads

	if (threadCount > 1) {

		threadCount = 1;        //No support yet, test TODO: Remove

		U64 counter = 0;
		U64 threadCounter = 0;
		U64 completedCounter = 0;
		U64 counterCompiledBinaries = 0;
		U64 counterLinkedBinaries = 0;

		gotoIfError2(clean, ListThread_resize(&threads, threadCount, alloc))
		gotoIfError2(clean, ListCompiler_resize(&compilers, threadCount, alloc))
		gotoIfError2(clean, ListListSHEntryRuntime_resize(&shEntries, allFiles.length, alloc))

		CompilerJobScheduler jobScheduler = (CompilerJobScheduler) {

			.isDebug = isDebug,
			.ignoreEmptyFiles = ignoreEmptyFiles,
			.enableLogging = enableLogging,
			.inputPaths = allFiles,
			.inputData = allShaderText,
			.outputPaths = allOutputs,
			.compileModes = allCompileOutputs,
			.compilers = compilers,

			.shEntries = &shEntries,
			.shEntryIdsIntermediate = &shEntryIdsIntermediate,
			.shEntryIdsIntermediate1 = &shEntryIdsIntermediate1,
			.shEntryIds = &shEntryIds,
			.compileResults = &compileResults,
			.compileResultsTemp = &compileResultsTemp,

			.lock = &lock,
			.counter = &counter,
			.completedCounter = &completedCounter,
			.counterCompiledBinaries = &counterCompiledBinaries,
			.counterLinkedBinaries = &counterLinkedBinaries,
			.threadCounter = &threadCounter,
			.success = &s_uccess,

			.compileType = compileType,
			.includeDir = includeDir,

			.alloc = alloc
		};

		for(U64 i = 0; i < threadCount; ++i) {

			gotoIfError3(clean, Compiler_create(alloc, &compilers.ptrNonConst[i], e_rr))

			gotoIfError2(clean, Thread_create(
				&alloc, (ThreadCallbackFunction) Compiler_compileJob, &jobScheduler, &threads.ptrNonConst[i]
			))
		}

		for(U64 i = 0; i < threadCount; ++i)
			Thread_waitAndCleanup(&alloc, &threads.ptrNonConst[i]);

		ListThread_free(&threads, alloc);

		gotoIfError2(clean, ListU64_createCopy(shEntryIds, alloc, &shEntryIdsSorted))
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

				//TODO: binaryIndices
				//gotoIfError3(clean, Compiler_getUniqueCompiles(shEntries.ptr[lastJobId], NULL, alloc, e_rr))
				
				/*gotoIfError3(clean, Compiler_registerShaderEntries(
					&shFile, shEntries.ptr[lastJobId], binaryIndices, alloc, e_rr
				))*/

				if (
					lastJobId &&
					CharString_equalsStringSensitive(allOutputs.ptr[lastJobId - 1], allOutputs.ptr[lastJobId])
				) {
					SHFile tmp = (SHFile) { 0 };
					gotoIfError3(clean, SHFile_combine(previous, shFile, alloc, &tmp, e_rr))
					SHFile_free(&previous, alloc);
					previous = tmp;
				}

				else errorInPrevious = false;        //Reset error report

				if(
					lastJobId + 1 == allOutputs.length ||
					!CharString_equalsStringSensitive(allOutputs.ptr[lastJobId + 1], allOutputs.ptr[lastJobId])
				) {

					if(errorInPrevious) {
						if(enableLogging)
							Log_warnLn(alloc, "One of the previous oiSH compilations failed, not producing a binary");
					}

					else {

						if(extraWarnings)
							gotoIfError3(clean, Compiler_handleExtraWarnings(
								previous.entries.length ? previous : shFile, extraWarnings, alloc, e_rr
							))

						if(previous.entries.length)
							gotoIfError3(clean, SHFile_write(previous, alloc, &temp, e_rr))

						else gotoIfError3(clean, SHFile_write(shFile, alloc, &temp, e_rr))

						if(allBuffers) {
							allBuffers->ptrNonConst[lastJobId] = temp;
							temp = Buffer_createNull();        //Moved
						}

						else {
							gotoIfError3(clean, File_write(temp, allOutputs.ptr[lastJobId], 0, 0, 100 * MS, true, alloc, e_rr))
							Buffer_free(&temp, alloc);
						}
					}
				}

				else {
					SHFile_free(&previous, alloc);
					previous = shFile;
					shFile = (SHFile) { 0 };
				}

				SHFile_free(&shFile, alloc);
			}

			//In case we just finished

			if (i == shEntryIdsSorted.length)
				break;

			//Make sure we don't create an invalid SHFile

			U64 unsortedI = ListU64_findFirst(shEntryIds, next, 0, NULL);
			CompileResult *compileResult = &compileResults.ptrNonConst[unsortedI];

			if(!compileResult->isSuccess) {
				SHFile_free(&shFile, alloc);
				continue;
			}

			//Start new SHFile

			if (jobId != lastJobId) {

				U32 crc32c = Buffer_crc32c(CharString_bufferConst(allShaderText.ptr[jobId]));

				gotoIfError3(clean, SHFile_create(
					ESHSettingsFlags_None,
					OXC3_VERSION,
					crc32c,
					alloc,
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

			const SHEntryRuntime *runtimeEntry = &shEntries.ptr[jobId].ptr[runtimeEntryId];

			SHBinaryIdentifier identifier = (SHBinaryIdentifier) { 0 };
			gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(runtimeEntry, combinationId, &identifier, e_rr))

			gotoIfError3(clean, Compiler_registerShaderBinary(
				&shFile,
				compileResult,
				allCompileOutputs.ptr[jobId],
				allFiles.ptr[jobId],
				runtimeEntry,
				&identifier,
				alloc,
				e_rr
			))

			lastJobId = jobId;
		}
	}

	else {

		gotoIfError3(clean, Compiler_create(alloc, &compiler, e_rr))

		for (U64 i = 0; i < allFiles.length; ++i) {

			//Preprocess to get information necessary for real compiles.

			if(!Compiler_precompileShader(
				compiler, allCompileOutputs.ptr[i],
				isDebug,
				allFiles.ptr[i], allShaderText.ptr[i],
				&runtimeEntries,
				includeDir,
				enableLogging,
				alloc
			)) {
				s_uccess = false;

				if(enableLogging)
					Log_errorLn(
						alloc, "Precompile failed for file \"%.*s\"",
						(int)CharString_length(allFiles.ptr[i]), allFiles.ptr[i].ptr
					);

				goto clean;
			}

			//Finish by going over all individual runtime entries and outputting it as a raw binary

			if (!runtimeEntries.length) {

				if(!ignoreEmptyFiles) {

					if(enableLogging)
						Log_errorLn(
							alloc, "Precompile couldn't find entrypoints for file \"%.*s\"",
							(int)CharString_length(allFiles.ptr[i]), allFiles.ptr[i].ptr
						);

					s_uccess = false;
				}

				ListSHEntryRuntime_freeUnderlying(&runtimeEntries, alloc);
				continue;
			}

			U32 crc32c = Buffer_crc32c(CharString_bufferConst(allShaderText.ptr[i]));

			gotoIfError3(clean, SHFile_create(
				ESHSettingsFlags_None,
				OXC3_VERSION,
				crc32c,
				alloc,
				&shFile,
				e_rr
			))

			gotoIfError3(clean, Compiler_getUniqueCompiles(runtimeEntries, &compileCombinations, alloc, e_rr))

			//Only for non lib entries, and then once per lib entry

			Bool didSucceed = true;

			for(U64 j = 0; j < compileCombinations.length; ++j) {

				U16 runtimeEntryId = (U16) (compileCombinations.ptr[j] >> 16);
				U16 combinationId  = (U16) compileCombinations.ptr[j];

				Bool isRt = combinationId >> 15;
				Bool isGfxOrComp = runtimeEntryId >> 15;

				runtimeEntryId &= (U16) I16_MAX;
				combinationId  &= (U16) I16_MAX;

				SHEntryRuntime runtimeEntry = runtimeEntries.ptr[runtimeEntryId];

				//Compile and return error if failed

				ESHBinaryType binaryType = allCompileOutputs.ptr[i];

				if(!Compiler_compileShaderSingle(
					compiler,
					binaryType,
					isDebug,
					isRt,
					isGfxOrComp,
					allFiles.ptr[i],
					allShaderText.ptr[i],
					&tempResult,
					runtimeEntries,
					runtimeEntryId,
					combinationId,
					includeDir,
					enableLogging,
					alloc
				)) {
					s_uccess = false;

					if(enableLogging)
						Log_errorLn(
							alloc, "Compile failed for file \"%.*s\"",
							(int)CharString_length(allFiles.ptr[i]), allFiles.ptr[i].ptr
						);

					didSucceed = false;
					CompileResult_free(&tempResult, alloc);
					break;
				}

				SHBinaryIdentifier binaryIdentifier = (SHBinaryIdentifier){ 0 };
				gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(
					&runtimeEntry, combinationId, &binaryIdentifier, e_rr
				))

				//Lib files need to be specialized per shader annotation if uniforms are present
				//or per entrypoint for graphics shaders.
				//For non libs this wil just loop once and only call process.
				//It uses both the oxc data (uniforms, defines, etc.) and binary data to know the relevant entrypoints.
				//For example; it might compile a function, but it's unused or a function is not present in the final binary.

				gotoIfError3(clean, Compiler_getLinkEntries(
					compiler,
					&runtimeEntries,
					&binaryIdentifier,
					binaryType,
					&tempResult.binary,
					&uniqueEntrypoints,
					&linkEntries,
					alloc,
					e_rr
				))

				for (U64 k = 0; k < linkEntries.length; ++k) {

					LinkEntry linkEntry = linkEntries.ptr[k];
					Buffer uniformData = linkEntry.uniformData;

					U16 currentCombinationId = linkEntry.combinationId;

					binaryIdentifier = (SHBinaryIdentifier){ 0 };
					gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(
						&runtimeEntry, currentCombinationId, &binaryIdentifier, e_rr
					));

					ListBuffer inputs = (ListBuffer) { 0 };
					gotoIfError2(clean, ListBuffer_createRefConst(&tempResult.binary, 1, &inputs));

					Bool isShaderAnnotation = runtimeEntry.isShaderAnnotation;

					if(isShaderAnnotation) {

						CompilerEntrypoint entry = (CompilerEntrypoint) { 0 };
							
						if (linkEntry.entrypointId != U16_MAX) {

							//entrypointId doesn't map to uniqueEntrypoints as some might be missing there;
							//it maps to our parsed runtimeEntries.

							CharString entrypointName = runtimeEntries.ptr[linkEntry.entrypointId].entry.name;

							U64 l = 0;

							for (; l < uniqueEntrypoints.length; ++l)
								if (CharString_equalsStringSensitive(entrypointName, uniqueEntrypoints.ptr[l].name))
									break;

							if(l == uniqueEntrypoints.length)
								retError(clean, Error_invalidState(
									0,
									"Compiler_compileShaders() somehow an entrypointId was referenced by a linkEntry "
									"that doesn't exist"
								))

							entry = uniqueEntrypoints.ptr[l];
						}

						else entry.stage = ESHPipelineStage_Count;        //Mark as lib
							
						tempResult2.type = ECompileResultType_Binary;

						gotoIfError3(clean, Compiler_linkSingle(
							compiler,
							allFiles.ptr[i],
							runtimeEntryId,
							currentCombinationId,
							binaryType,
							inputs,
							runtimeEntry.uniforms,
							uniformData,
							entry.name,
							binaryIdentifier.shaderVersion,
							entry.stage,
							binaryIdentifier.extensions,
							enableLogging,
							&tempResult2.binary,
							alloc
						))

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
						allFiles.ptr[i],
						runtimeEntryId,
						currentCombinationId,
						binaryType,
						tempResult2.binary.ptr ? &tempResult2 : &tempResult,
						isDebug,
						binaryIdentifier,
						&lock,
						runtimeEntries,
						isShaderAnnotation,
						enableLogging,
						alloc,
						e_rr
					))

					if (linkEntry.entrypointId == U16_MAX)
						binaryIdentifier.stageType = isRt ? ESHPipelineStage_RtStartExt : ESHPipelineStage_WorkgraphExt;

					U16 binaryId = (U16) shFile.binaries.length;

					gotoIfError3(clean, Compiler_registerShaderBinary(
						&shFile,
						tempResult2.binary.ptr ? &tempResult2 : &tempResult,
						allCompileOutputs.ptr[i],
						allFiles.ptr[i],
						&runtimeEntry,
						&binaryIdentifier,
						alloc,
						e_rr
					))

					for (U64 l = 0; l < linkEntry.runtimeEntries.length; ++l)        //Link runtime entry to binary
						gotoIfError2(clean, ListU32_pushBack(
							&binaryIndices, binaryId | (((U32)linkEntry.runtimeEntries.ptr[l]) << 16), alloc
						))

					runtimeEntry.isShaderAnnotation = isShaderAnnotation;
					CompileResult_free(&tempResult2, alloc);
				}

				ListLinkEntry_freeUnderlying(&linkEntries, alloc);
				ListCompilerEntrypoint_freeUnderlying(&uniqueEntrypoints, alloc);
				CompileResult_free(&tempResult, alloc);
			}

			if(!ListU32_sort(binaryIndices))
				retError(clean, Error_invalidState(0, "Compiler_compileShaders() sort failed"))

			//Link entrypoint to binaries

			if (didSucceed)
				gotoIfError3(clean, Compiler_registerShaderEntries(&shFile, runtimeEntries, binaryIndices, alloc, e_rr))

			//Write to disk and free temp data

			if (didSucceed) {

				//Merge with previous if present

				if (
					i &&
					CharString_equalsStringSensitive(allOutputs.ptr[i - 1], allOutputs.ptr[i])
				) {
					SHFile tmp = (SHFile) { 0 };
					gotoIfError3(clean, SHFile_combine(previous, shFile, alloc, &tmp, e_rr))
					SHFile_free(&previous, alloc);
					previous = tmp;
				}

				else errorInPrevious = false;        //Reset error report

				if(
					i + 1 == allOutputs.length ||
					!CharString_equalsStringSensitive(allOutputs.ptr[i + 1], allOutputs.ptr[i])
				) {

					if(errorInPrevious) {
						if(enableLogging)
							Log_warnLn(alloc, "One of the previous oiSH compilations failed, not producing a binary");
					}

					else {

						if(extraWarnings)
							gotoIfError3(clean, Compiler_handleExtraWarnings(
								previous.entries.length ? previous : shFile, extraWarnings, alloc, e_rr
							))

						if(previous.entries.length)
							gotoIfError3(clean, SHFile_write(previous, alloc, &temp, e_rr))

						else gotoIfError3(clean, SHFile_write(shFile, alloc, &temp, e_rr))

						if(allBuffers) {
							allBuffers->ptrNonConst[i] = temp;
							temp = Buffer_createNull();        //Moved
						}

						else {
							gotoIfError3(clean, File_write(temp, allOutputs.ptr[i], 0, 0, 100 * MS, true, alloc, e_rr))
							Buffer_free(&temp, alloc);
						}
					}
				}

				else {
					SHFile_free(&previous, alloc);
					previous = shFile;
					shFile = (SHFile) { 0 };
				}
			}

			else errorInPrevious = true;

			ListU32_free(&compileCombinations, alloc);
			ListU32_clear(&binaryIndices);
			SHFile_free(&shFile, alloc);

			ListSHEntryRuntime_freeUnderlying(&runtimeEntries, alloc);
		}
	}

	if(!s_uccess)
		goto clean;

clean:

	for(U64 i = 0; i < threads.length; ++i)
		Thread_waitAndCleanup(&alloc, &threads.ptrNonConst[i]);

	ListThread_free(&threads, alloc);
	ListCompiler_freeUnderlying(&compilers, alloc);
	Compiler_free(&compiler, alloc);
	Buffer_free(&temp, alloc);

	ListSHEntryRuntime_freeUnderlying(&runtimeEntries, alloc);
	ListLinkEntry_freeUnderlying(&linkEntries, alloc);
	ListCompilerEntrypoint_freeUnderlying(&uniqueEntrypoints, alloc);
	SHFile_free(&shFile, alloc);

	SHFile_free(&previous, alloc);
	SHEntry_free(&shEntry, alloc);
	ListU32_free(&compileCombinations, alloc);
	ListU32_free(&binaryIndices, alloc);

	ListListSHEntryRuntime_freeUnderlying(&shEntries, alloc);
	ListU64_free(&shEntryIds, alloc);
	ListU64_free(&shEntryIdsIntermediate, alloc);
	ListU64_free(&shEntryIdsIntermediate1, alloc);
	ListU64_free(&shEntryIdsSorted, alloc);
	ListCompileResult_freeUnderlying(&compileResults, alloc);
	ListCompileResult_freeUnderlying(&compileResultsTemp, alloc);
	CompileResult_free(&tempResult, alloc);
	CompileResult_free(&tempResult2, alloc);

	CharString_free(&tempStr, alloc);
	CharString_free(&tempStr2, alloc);
	CharString_free(&tempStr3, alloc);

	return s_uccess;
}
