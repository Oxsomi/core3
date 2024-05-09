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

Bool ListCompiler_freeUnderlying(ListCompiler *compilers, Allocator alloc);

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

Bool CompileError_free(CompileError *err, Allocator alloc);
Bool ListCompileError_freeUnderlying(ListCompileError *compileErrors, Allocator alloc);

typedef struct IncludeInfo {

	U32 fileSize;
	U32 crc32c;

	Ns timestamp;

	U64 counter;

	CharString file;

} IncludeInfo;

TList(IncludeInfo);

ECompareResult IncludeInfo_compare(const IncludeInfo *a, const IncludeInfo *b);
Bool IncludeInfo_free(IncludeInfo *info, Allocator alloc);

Bool ListIncludeInfo_freeUnderlying(ListIncludeInfo *infos, Allocator alloc);

Error ListIncludeInfo_stringify(ListIncludeInfo files, Allocator alloc, CharString *output);

typedef enum ECompileResultType {
	ECompileResultType_Text,
	ECompileResultType_Binaries,
	ECompileResultType_SHEntries,
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
		ListBuffer binaries;
		ListSHEntry shEntries;
	};

	ListIncludeInfo includeInfo;

} CompileResult;

typedef struct IncludedFile {

	IncludeInfo includeInfo;

	U64 globalCounter;

	CharString data;

} IncludedFile;

TList(IncludedFile);

Bool IncludedFile_free(IncludedFile *file, Allocator alloc);
Bool ListIncludedFile_freeUnderlying(ListIncludedFile *file, Allocator alloc);

Bool CompileResult_free(CompileResult *result, Allocator alloc);

//A separate Compiler should be created per thread

Error Compiler_create(Allocator alloc, Compiler *comp);
Bool Compiler_free(Compiler *comp, Allocator alloc);

//Append new entries to infos and increase counters.
//This makes it possible to get a list of all includes.

Error Compiler_mergeIncludeInfo(Compiler *comp, Allocator alloc, ListIncludeInfo *infos);

//Process a file with includes and defines to one without (returns text)
//Always CompileResult_free after.
//On success returns result->success; otherwise the compile did happen but returned with errors.
//result->compileErrors.length can still be non zero if warnings are present.
Error Compiler_preprocess(Compiler comp, CompilerSettings settings, Allocator alloc, CompileResult *result);

//Manual tokenization for a preprocessed file, to obtain annotations (returns shEntries)
Error Compiler_parse(Compiler comp, CompilerSettings settings, Allocator alloc, CompileResult *result);

typedef enum ECompileBinaryTypes {
	ECompileBinaryTypes_Shader,			//Shader binary
	ECompileBinaryTypes_Reflection,		//Reflection file for HLSL
	ECompileBinaryTypes_Debugging,		//Debugging file such as a PDB for HLSL
	ECompileBinaryTypes_Count
} ECompileBinaryTypes;

//Compile preprocessed file's entry (entry of U64_MAX indicates all, for RT entrypoints) (returns binaries)
Error Compiler_compile(Compiler comp, CompilerSettings settings, ListSHEntry entries, U64 entry, CompileResult *result);

//Extended functions for basic allocators

Bool CompileResult_freex(CompileResult *result);
Bool ListCompiler_freeUnderlyingx(ListCompiler *compilers);

Bool CompileError_freex(CompileError *err);
Bool ListCompileError_freeUnderlyingx(ListCompileError *compileErrors);

Bool IncludeInfo_freex(IncludeInfo *info);
Bool ListIncludeInfo_freeUnderlyingx(ListIncludeInfo *infos);
Error ListIncludeInfo_stringifyx(ListIncludeInfo files, CharString *tempStr);

Bool IncludedFile_freex(IncludedFile *file);
Bool ListIncludedFile_freeUnderlyingx(ListIncludedFile *file);

Error Compiler_createx(Compiler *comp);
Bool Compiler_freex(Compiler *comp);

Error Compiler_preprocessx(Compiler comp, CompilerSettings settings, CompileResult *result);
Error Compiler_parsex(Compiler comp, CompilerSettings settings, CompileResult *result);
Error Compiler_mergeIncludeInfox(Compiler *comp, ListIncludeInfo *infos);

#ifdef __cplusplus
	}
#endif
