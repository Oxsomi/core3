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

#pragma once
#include "formats/oiSH.h"

#ifdef __cplusplus
	extern "C" {
#endif

TList(SHFile);

typedef struct CharString CharString;

typedef enum ECompilerFormat {
	ECompilerFormat_HLSL,
	ECompilerFormat_Count
} ECompilerFormat;

//Each compiler

typedef struct Compiler {
	void *interfaces[8];		//Data for native compiler interfaces (such as DXC)
} Compiler;

TList(Compiler);

void ListCompiler_freeUnderlying(ListCompiler *compilers, Allocator alloc);

typedef struct CompilerSettings {

	CharString string;

	CharString path;

	ECompilerFormat format;
	ESHBinaryType outputType;

	CharString includeDir;		//Optional extra includeDir to search for

	Bool debug;
	Bool infoAboutIncludes;		//Saves extra include info, useful for debugging includes or hot shader reload
	U8 padding[6];

} CompilerSettings;

typedef enum ECompileErrorType {
	ECompileErrorType_Warn,
	ECompileErrorType_Error,
	ECompileErrorType_Count
} ECompileErrorType;

typedef struct CompileError {

	U32 compileIndex;		//Compile index. % ESHBinaryType_Count = binaryType, / ESHBinaryType = i of strings[i]

	U16 lineId;
	U8 typeLineId;			//ECompileErrorType in the top bit and lineId upper 7 bits
	U8 lineOffset;

	CharString error;

	CharString file;

} CompileError;

U32 CompileError_lineId(CompileError err);

TList(CompileError);

void CompileError_free(CompileError *err, Allocator alloc);
void ListCompileError_freeUnderlying(ListCompileError *compileErrors, Allocator alloc);

typedef struct IncludeInfo {

	U32 fileSize;
	U32 crc32c;

	Ns timestamp;

	U64 counter;

	CharString file;

} IncludeInfo;

TList(IncludeInfo);

ECompareResult IncludeInfo_compare(const IncludeInfo *a, const IncludeInfo *b);
void IncludeInfo_free(IncludeInfo *info, Allocator alloc);

void ListIncludeInfo_freeUnderlying(ListIncludeInfo *infos, Allocator alloc);

Bool ListIncludeInfo_stringify(ListIncludeInfo files, Allocator alloc, CharString *output, Error *e_rr);

typedef enum ECompileResultType {
	ECompileResultType_Text,
	ECompileResultType_Binary,
	ECompileResultType_SHEntryRuntime,
	ECompileResultType_Count
} ECompileResultType;

typedef struct CompileResult {

	ListCompileError compileErrors;

	ECompileResultType type;

	Bool isSuccess;
	Bool infoAboutIncludes;
	U8 padding[2];

	union {
		CharString text;
		Buffer binary;
		ListSHEntryRuntime shEntriesRuntime;
	};

	ListIncludeInfo includeInfo;

} CompileResult;

TList(CompileResult);

typedef struct IncludedFile {

	IncludeInfo includeInfo;

	U64 globalCounter;

	CharString data;

} IncludedFile;

TList(IncludedFile);

TList(ListU16);
TListNamed(const U16*, ListU16PtrConst);

TList(ListU32);
TListNamed(const U32*, ListU32PtrConst);

void IncludedFile_free(IncludedFile *file, Allocator alloc);
void ListIncludedFile_freeUnderlying(ListIncludedFile *file, Allocator alloc);

void CompileResult_free(CompileResult *result, Allocator alloc);
void ListCompileResult_freeUnderlying(ListCompileResult *result, Allocator alloc);

void ListListU16_freeUnderlying(ListListU16 *list, Allocator alloc);
void ListListU32_freeUnderlying(ListListU32 *list, Allocator alloc);

//A separate Compiler should be created per thread

Bool Compiler_create(Allocator alloc, Compiler *comp, Error *e_rr);
void Compiler_free(Compiler *comp, Allocator alloc);

//Call this on shutdown for a clean exit

void Compiler_shutdown();

//Append new entries to infos and increase counters.
//This makes it possible to get a list of all includes.

Bool Compiler_mergeIncludeInfo(Compiler *comp, Allocator alloc, ListIncludeInfo *infos, Error *e_rr);

//Process a file with includes and defines to one without (returns text)
//Always CompileResult_free after.
//On success returns result->success; otherwise the compile did happen but returned with errors.
//result->compileErrors.length can still be non zero if warnings are present.
Bool Compiler_preprocess(Compiler comp, CompilerSettings settings, Allocator alloc, CompileResult *result, Error *e_rr);

//Determine what minimum shader version is required
U16 Compiler_minFeatureSetStage(ESHPipelineStage stage, U16 waveSize);
U16 Compiler_minFeatureSetExtension(ESHExtension ext);

Bool Compiler_parseErrors(CharString errs, Allocator alloc, ListCompileError *errors, Bool *hasErrors, Error *e_rr);

//Manual tokenization for a preprocessed file, to obtain annotations (returns shEntries if !symbolsOnly, otherwise text)
Bool Compiler_parse(Compiler comp, CompilerSettings settings, Bool symbolsOnly, Allocator alloc, CompileResult *result, Error *e_rr);

typedef enum ECompileBinaryTypes {
	ECompileBinaryTypes_Shader,			//Shader binary
	ECompileBinaryTypes_Reflection,		//Reflection file for HLSL
	ECompileBinaryTypes_Debugging,		//Debugging file such as a PDB for HLSL
	ECompileBinaryTypes_Count
} ECompileBinaryTypes;

//Compile preprocessed file's entry
Bool Compiler_compile(
	Compiler comp,
	CompilerSettings settings,
	SHBinaryIdentifier toCompile,
	Allocator alloc,
	CompileResult *result,
	Error *e_rr
);

//Extended functions for basic allocators

void CompileResult_freex(CompileResult *result);
void ListCompileResult_freeUnderlyingx(ListCompileResult *result);

void ListCompiler_freeUnderlyingx(ListCompiler *compilers);

void CompileError_freex(CompileError *err);
void ListCompileError_freeUnderlyingx(ListCompileError *compileErrors);

void ListListU16_freeUnderlyingx(ListListU16 *list);
void ListListU32_freeUnderlyingx(ListListU32 *list);

void IncludeInfo_freex(IncludeInfo *info);
void ListIncludeInfo_freeUnderlyingx(ListIncludeInfo *infos);
Bool ListIncludeInfo_stringifyx(ListIncludeInfo files, CharString *tempStr, Error *e_rr);

void IncludedFile_freex(IncludedFile *file);
void ListIncludedFile_freeUnderlyingx(ListIncludedFile *file);

Bool Compiler_createx(Compiler *comp, Error *e_rr);
void Compiler_freex(Compiler *comp);

Bool Compiler_preprocessx(Compiler comp, CompilerSettings settings, CompileResult *result, Error *e_rr);
Bool Compiler_parsex(Compiler comp, CompilerSettings settings, Bool symbolsOnly, CompileResult *result, Error *e_rr);
Bool Compiler_mergeIncludeInfox(Compiler *comp, ListIncludeInfo *infos, Error *e_rr);

Bool Compiler_compilex(
	Compiler comp,
	CompilerSettings settings,
	SHBinaryIdentifier toCompile,
	CompileResult *result,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
