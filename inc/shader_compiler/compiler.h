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

	Bool debug;
	U8 padding[7];

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
	U8 padding[3];

	union {
		CharString text;
		ListBuffer binaries;
		ListSHEntry shEntries;
	};

} CompileResult;

Bool CompileResult_free(CompileResult *result, Allocator alloc);

//A separate Compiler should be created per thread

Error Compiler_create(Allocator alloc, Compiler *comp);
Bool Compiler_free(Compiler *comp, Allocator alloc);

//Process a file with includes and defines to one without (returns text)
//Always CompileResult_free after.
//On success returns result->success; otherwise the compile did happen but returned with errors.
//result->compileErrors.length can still be non zero if warnings are present.
Error Compiler_preprocess(Compiler comp, CompilerSettings settings, Allocator alloc, CompileResult *result);

//Manual tokenization for a preprocessed file, to obtain annotations (returns shEntries)
Error Compiler_parse(Compiler comp, CompilerSettings settings, CompileResult *result);

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

Error Compiler_createx(Compiler *comp);
Bool Compiler_freex(Compiler *comp);
Error Compiler_preprocessx(Compiler comp, CompilerSettings settings, CompileResult *result);

#ifdef __cplusplus
	}
#endif
