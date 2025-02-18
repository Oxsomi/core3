/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "formats/oiSH/sh_file.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct CharString CharString;
typedef struct SpinLock SpinLock;

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

	ESHExtension demotion;		//Compile result can signal "extension not found" to demote final binary

	Bool isSuccess;
	Bool infoAboutIncludes;
	U8 type;					//ECompileResultType
	U8 padding;

	union {
		CharString text;
		ListSHEntryRuntime shEntriesRuntime;
		Buffer binary;
	};

	ListIncludeInfo includeInfo;

	ListSHRegisterRuntime registers;

} CompileResult;

TList(CompileResult);

typedef struct IncludedFile {

	IncludeInfo includeInfo;

	U64 globalCounter;

	CharString data;

} IncludedFile;

TList(IncludedFile);

TListNamed(const U16*, ListU16PtrConst);
TListNamed(const U32*, ListU32PtrConst);

void IncludedFile_free(IncludedFile *file, Allocator alloc);
void ListIncludedFile_freeUnderlying(ListIncludedFile *file, Allocator alloc);

void CompileResult_free(CompileResult *result, Allocator alloc);
void ListCompileResult_freeUnderlying(ListCompileResult *result, Allocator alloc);

//A separate Compiler should be created per thread

Bool Compiler_create(Allocator alloc, Compiler *comp, Error *e_rr);
void Compiler_free(Compiler *comp, Allocator alloc);

//Call this on shutdown for a clean exit
void Compiler_shutdown();

//Generate disassembly from buffer

Bool Compiler_disassembleSPIRV(Buffer buf, Allocator alloc, CharString *result, Error *e_rr);
Bool Compiler_disassembleDXIL(Compiler comp, Buffer buf, Allocator alloc, CharString *result, Error *e_rr);

Bool Compiler_createDisassembly(Compiler comp, ESHBinaryType type, Buffer buf, Allocator alloc, CharString *res, Error *e_rr);

//Convert assembly (SPIRV and DXIL) to oiSH by using the assembly

Bool Compiler_processSPIRV(
	Buffer *result,						//Required; input & output SPIRV (will be optimized)
	ListSHRegisterRuntime *registers,	//Required; Output registers
	CompilerSettings settings,
	SHBinaryIdentifier toCompile,
	SpinLock *lock,						//If not NULL will be used before writing into entries
	ListSHEntryRuntime entries,			//Array contains the current buffer's reflection for the entry and compatibility checks
	ESHExtension *demotions,			//Required; specifies which extensions aren't used (useful for demoting unused ones)
	Allocator alloc,
	Error *e_rr
);

Bool Compiler_processDXIL(
	Compiler compiler,					//To be able to get reflection data
	Buffer *result,						//Required; input & output DXIL
	ListSHRegisterRuntime *registers,	//Required; Output registers
	Buffer reflectionData,				//If not supplied, will try to get it from DXIL, if both are missing it will fail!
	SHBinaryIdentifier toCompile,
	SpinLock *lock,						//If not NULL will be used before writing into entries
	ListSHEntryRuntime entries,			//Array contains the current buffer's reflection for the entry and compatibility checks
	ESHExtension *demotions,			//Required; specifies which extensions aren't used (useful for demoting unused ones)
	Allocator alloc,
	Error *e_rr
);

Bool Compiler_finalizeEntrypoint(		//Push reflection data into final entrypoint
	U32 localSize[3],					//If compute-adj (workgraph, mesh shaders too) the local size per group
	U8 payloadSize,						//If miss/hit/callable, the payload size that gets transmitted (bytes)
	U8 intersectSize,					//If intersection/hit shader, size of intersection attributes (generally 8 bytes)
	U16 waveSize,						//4 pairs of log2(thread) + 1 where 0 = none. req, min, max, rec
	ESBType inputs[16],					//Input types for graphics shaders
	ESBType outputs[16],				//Output types for graphics shaders
	U8 uniqueInputSemantics,			//How many unique semantic names there are
	ListCharString *uniqueSemantics,	//All semantic names; e.g. NORMAL. Excluding TEXCOORD or SV_TARGET
	U8 inputSemantics[16],				//U4 each; semanticId and uniqueSemanticOff (0 = TEXCOORD or SV_TARGET)
	U8 outputSemantics[16],				//^ but for output semantics for graphics shaders
	CharString entryName,				//Can be empty in case of RT shaders
	SpinLock *lock,						//If not NULL will be used before writing/validating against previous entry
	ListSHEntryRuntime entries,			//Array contains the current buffer's reflection for the entry and compatibility checks
	Allocator alloc,
	Error *e_rr
);

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

Bool Compiler_validateGroupSize(U32 threads[3], Error *e_rr);

Bool Compiler_parseErrors(CharString errs, Allocator alloc, ListCompileError *errors, Bool *hasErrors, Error *e_rr);

//Manual tokenization for a preprocessed file, to obtain annotations (returns shEntries if !symbolsOnly, otherwise text)
Bool Compiler_parse(
	Compiler comp,
	CompilerSettings settings,
	Bool symbolsOnly,
	Allocator alloc,
	CompileResult *result,
	Error *e_rr
);

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
	SpinLock *lock,					//Locked when entries are changed
	ListSHEntryRuntime entries,		//Writes into to update reflected properties
	Allocator alloc,
	CompileResult *result,
	Error *e_rr
);

//Extra warnings useful for debugging purposes and optimization.

typedef enum ECompilerWarning {
	ECompilerWarning_None				= 0,
	ECompilerWarning_UnusedRegisters	= 1 << 0,
	ECompilerWarning_UnusedConstants	= 1 << 1,
	ECompilerWarning_BufferPadding		= 1 << 2
} ECompilerWarning;

Bool Compiler_handleExtraWarnings(SHFile file, ECompilerWarning warning, Allocator alloc, Error *e_rr);

//Simplied compiler workflow, this is what the CLI calls too; it automatically handles threading and other things.

typedef enum ECompileType {
	ECompileType_Preprocess,		//Turns shader with includes & defines into an easily parsable string
	ECompileType_Includes,			//Turns shader with includes into a list of their dependencies (direct + indirect)
	ECompileType_Compile,			//Compile all shaders into an oiSH file for consumption
	ECompileType_Symbols			//List all symbols located in the shader or include as a text file
} ECompileType;

Bool Compiler_getTargetsFromFile(
	CharString input,
	ECompileType compileType,
	U64 compileModeU64,
	Bool multipleModes,
	Bool combineFlag,
	Bool enableLogging,
	Allocator alloc,
	Bool *isFolder,					//Optional (out); if the input is a folder or not
	CharString *output,				//Optional; the output directory. If NULL, will output file names only (relative to none)
	ListCharString *allFiles,		//Fully resolved file names (may contain duplicates per compile mode)
	ListCharString *allShaderText,	//Per file name: Input shader files
	ListCharString *allOutputs,		//Per file name: Output shader file names
	ListU8 *allCompileModes			//Per file name: ESHBinaryType
);

Bool Compiler_compileShaders(
	ListCharString allFiles,
	ListCharString allShaderText,
	ListCharString allOutputs,
	ListU8 allCompileOutputs,
	U64 threadCount,
	Bool isDebug,
	ECompilerWarning extraWarnings,
	Bool ignoreEmptyFiles,
	ECompileType type,
	CharString includeDir,			//Optional
	CharString outputDir,			//Optional outputDir,
	Bool enableLogging,
	Allocator alloc,
	ListBuffer *allBuffers,			//Optional: buffer outputs (if NULL, outputs to file)
	Error *e_rr
);

//Extended functions for basic allocators

void CompileResult_freex(CompileResult *result);
void ListCompileResult_freeUnderlyingx(ListCompileResult *result);

void ListCompiler_freeUnderlyingx(ListCompiler *compilers);

void CompileError_freex(CompileError *err);
void ListCompileError_freeUnderlyingx(ListCompileError *compileErrors);

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
Bool Compiler_createDisassemblyx(Compiler comp, ESHBinaryType type, Buffer buf, CharString *result, Error *e_rr);

Bool Compiler_compilex(
	Compiler comp,
	CompilerSettings settings,
	SHBinaryIdentifier toCompile,
	SpinLock *lock,					//Locked when entries are changed
	ListSHEntryRuntime entries,		//Writes into to update reflected properties
	CompileResult *result,
	Error *e_rr
);

Bool Compiler_handleExtraWarningsx(SHFile file, ECompilerWarning warning, Error *e_rr);

#ifdef __cplusplus
	}
#endif
