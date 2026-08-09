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

//shader_compiler/compiler.h

#pragma once
#include "formats/oiSH/sh_file.h"
#include "formats/oiSR/sr_file.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct CharString CharString;
typedef struct SpinLock SpinLock;

typedef enum ECompilerFormat {
	ECompilerFormat_HLSL,
	ECompilerFormat_Count
} ECompilerFormat;

//The SPIR-V version a shader must target, driven purely by the features it uses.
//Kept in one place so the DXC compile target-env, the spirv-opt optimizer and the linker all stay in sync.
//- Cooperative vectors/matrix (linalg) need SPIR-V 1.6 (vulkan1.3): StorageBuffer storage class + Vulkan memory model.
//- Raytracing and mesh/task shaders need SPIR-V 1.4: SPV_KHR_ray_tracing / SPV_EXT_mesh_shader emit >= 1.4 opcodes.
//- Everything else is fine at SPIR-V 1.3 (vulkan1.1).

typedef enum ESpirvVersion {
	ESpirvVersion_1_3,
	ESpirvVersion_1_4,
	ESpirvVersion_1_6
} ESpirvVersion;

static inline ESpirvVersion Compiler_requiredSpirvVersion(Bool isRt, Bool isCoop, Bool isMeshTask) {

	if(isCoop)
		return ESpirvVersion_1_6;

	if(isRt || isMeshTask)
		return ESpirvVersion_1_4;

	return ESpirvVersion_1_3;
}

//Each compiler

typedef struct Compiler {
	void *interfaces[8];        //Data for native compiler interfaces (such as DXC)
} Compiler;

TList(Compiler);

void ListCompiler_freeUnderlying(ListCompiler *compilers, const Allocator *alloc);

typedef struct CompilerSettings {

	CharString string;

	CharString path;

	ECompilerFormat format;
	ESHBinaryType outputType;

	ListCharString includeDirs; //Optional extra include dirs to search

	Bool debug;
	Bool infoAboutIncludes;     //Saves extra include info, useful for debugging includes or hot shader reload
	Bool isLib;
	Bool containsGfxOrComp;
	Bool isRt;
	Bool keepUnusedRegisters;   //Keep declared but unused resources bound and reflected (stable layouts across variants)
	U8 padding[2];

} CompilerSettings;

typedef enum ECompileErrorType {
	ECompileErrorType_Warn,
	ECompileErrorType_Error,
	ECompileErrorType_Count
} ECompileErrorType;

typedef struct CompileError {

	U32 compileIndex;           //Compile index. % ESHBinaryType_Count = binaryType, / ESHBinaryType = i of strings[i]

	U16 lineId;
	U8 typeLineId;              //ECompileErrorType in the top bit and lineId upper 7 bits
	U8 lineOffset;

	CharString error;

	CharString file;

} CompileError;

U32 CompileError_lineId(CompileError err);

TList(CompileError);

void CompileError_free(CompileError *err, const Allocator *alloc);
void ListCompileError_freeUnderlying(ListCompileError *compileErrors, const Allocator *alloc);

typedef struct IncludeInfo {

	U32 fileSize;
	U32 crc32c;

	Ns timestamp;

	U64 counter;

	CharString file;

} IncludeInfo;

TList(IncludeInfo);

ECompareResult IncludeInfo_compare(const IncludeInfo *a, const IncludeInfo *b);
void IncludeInfo_free(IncludeInfo *info, const Allocator *alloc);

void ListIncludeInfo_freeUnderlying(ListIncludeInfo *infos, const Allocator *alloc);

Bool ListIncludeInfo_stringify(const ListIncludeInfo *files, const Allocator *alloc, CharString *output, Error *e_rr);

typedef enum ECompileResultType {
	ECompileResultType_Binary,
	ECompileResultType_SHEntryRuntime,
	ECompileResultType_Count
} ECompileResultType;

typedef struct CompileResult {

	ListCompileError compileErrors;

	ESHExtension demotion;      //Compile result can signal "extension not found" to demote final binary

	Bool isSuccess;
	Bool infoAboutIncludes;
	U8 type;                    //ECompileResultType
	U8 padding;

	union {
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

typedef struct CompilerEntrypoint {
	CharString name;
	ESHPipelineStage stage;
	U32 padding;
} CompilerEntrypoint;

TList(CompilerEntrypoint);

TListNamed(const U16*, ListU16PtrConst);
TListNamed(const U32*, ListU32PtrConst);

void IncludedFile_free(IncludedFile *file, const Allocator *alloc);
void ListIncludedFile_freeUnderlying(ListIncludedFile *file, const Allocator *alloc);

void CompileResult_free(CompileResult *result, const Allocator *alloc);
void ListCompileResult_freeUnderlying(ListCompileResult *result, const Allocator *alloc);

void ListCompilerEntrypoint_freeUnderlying(ListCompilerEntrypoint *entry, const Allocator *alloc);

//A separate Compiler should be created per thread

Bool Compiler_create(const Allocator *alloc, Compiler *comp, Error *e_rr);
void Compiler_free(Compiler *comp, const Allocator *alloc);

//Call this on shutdown for a clean exit
void Compiler_shutdown();

//Generate disassembly from buffer

Bool Compiler_disassemble(
	const Compiler *comp, ESHBinaryType type, Buffer buf, const Allocator *alloc, CharString *result, Error *e_rr
);

//Assemble text back into a binary (SPIRV text via spirv-tools, DXIL LL text via DXC's IDxcAssembler)

Bool Compiler_assemble(
	const Compiler *comp, ESHBinaryType type, CharString text, const Allocator *alloc, Buffer *result, Error *e_rr
);

//Query entrypoints embedded in a binary

Bool Compiler_getUniqueEntrypoints(
	const Compiler *compiler,
	ESHBinaryType binaryType,
	Buffer binary,                                   //Must be a lib
	Bool showAll,                                    //true: show all entrypoints, false: only show targets to link
	ListCompilerEntrypoint *uniqueEntrypoints,
	const Allocator *alloc,
	Error *e_rr
);

//Convert assembly (SPIRV and DXIL) to oiSH by using the assembly

Bool Compiler_process(
	const Compiler *compiler,           //To be able to get reflection data
	ESHBinaryType type,
	Buffer *result,                     //Required; input & output binary
	ListSHRegisterRuntime *registers,   //Required; Output registers
	Bool isDebug,
	Bool keepRegisters,                 //Keep declared but unused resources bound and reflected
	const SHBinaryIdentifier *toCompile,
	SpinLock *lock,                     //If not NULL will be used before writing into entries
	const ListSHEntryRuntime *entries,  //Array contains the current buffer's reflection for the entry and compatibility checks
	Bool isLib,                         //If input file was compiled as lib
	ESHExtension *demotions,            //Required; specifies which extensions aren't used (useful for demoting unused ones)
	ListCompileError *errors,
	const Allocator *alloc,
	Error *e_rr
);

Bool Compiler_link(
	const Compiler *compiler,
	ESHBinaryType type,
	const ListBuffer *inputs,              //Input binary/binaries
	const ListSHUniformRuntime *uniforms,  //Uniform descriptions (to index uniformData and to link)
	Buffer uniformData,                    //Contents of the current compilation
	const CharString *entrypoint,          //Entrypoint specialization (empty = keep as lib, otherwise specialize)
	U16 shaderVersion,                     //U8 maj, minor
	ESHPipelineStage stageType,
	ESHExtension exts,
	ListCompileError *errors,
	Buffer *result,                        //Output binary: Either library or specialized binary (PS/GS/CS/etc.)
	const Allocator *alloc,
	Error *e_rr
);

Bool Compiler_finalizeEntrypoint(       //Push reflection data into final entrypoint
	U32 localSize[3],                   //If compute-adj (workgraph, mesh shaders too) the local size per group
	U8 payloadSize,                     //If miss/hit/callable, the payload size that gets transmitted (bytes)
	U8 intersectSize,                   //If intersection/hit shader, size of intersection attributes (generally 8 bytes)
	U16 waveSize,                       //4 pairs of log2(thread) + 1 where 0 = none. req, min, max, rec
	ESBType inputs[16],                 //Input types for graphics shaders
	ESBType outputs[16],                //Output types for graphics shaders
	U8 uniqueInputSemantics,            //How many unique semantic names there are
	ListCharString *uniqueSemantics,    //All semantic names; e.g. NORMAL. Excluding TEXCOORD or SV_TARGET
	U8 inputSemantics[16],              //U4 each; semanticId and uniqueSemanticOff (0 = TEXCOORD or SV_TARGET)
	U8 outputSemantics[16],             //^ but for output semantics for graphics shaders
	const CharString *entryName,        //Can be NULL/empty in case of RT shaders
	SpinLock *lock,                     //If not NULL will be used before writing/validating against previous entry
	const ListSHEntryRuntime *entries,  //Array contains the current buffer's reflection for the entry and compatibility checks
	const Allocator *alloc,
	Error *e_rr
);

//Append new entries to infos and increase counters.
//This makes it possible to get a list of all includes.
Bool Compiler_mergeIncludeInfo(Compiler *comp, const Allocator *alloc, ListIncludeInfo *infos, Error *e_rr);

//Determine what minimum shader version is required
U16 Compiler_minFeatureSetStage(ESHPipelineStage stage, U16 waveSize);
U16 Compiler_minFeatureSetExtension(ESHExtension ext);

Bool Compiler_validateGroupSize(U32 threads[3], Error *e_rr);

Bool Compiler_parseErrors(CharString errs, const Allocator *alloc, ListCompileError *errors, Bool *hasErrors, Error *e_rr);

//Invoke HLSL reflection, to obtain & parse annotations
Bool Compiler_parse(
	const Compiler *comp,
	const CompilerSettings *settings,
	const Allocator *alloc,
	CompileResult *result,
	Error *e_rr
);

//Walk the frontend HLSL reflection (source-level symbol AST) into a backend-neutral SRFile (oiSR).
//Unlike Compiler_parse (which keeps only annotated entrypoints), this preserves the full node tree
// with source locations, so it can drive editor intelligence (semantic highlighting, outline, go-to-definition).
//Reflection runs on the preprocessed source, so settings->string / defines determine which code paths are visible.
Bool Compiler_reflect(
	const Compiler *comp,
	const CompilerSettings *settings,
	const Allocator *alloc,
	SRFile *reflection,
	Error *e_rr
);

typedef enum ECompileBinaryTypes {
	ECompileBinaryTypes_Shader,           //Shader binary
	ECompileBinaryTypes_Reflection,       //Reflection file for HLSL
	ECompileBinaryTypes_Debugging,        //Debugging file such as a PDB for HLSL
	ECompileBinaryTypes_Count
} ECompileBinaryTypes;

//Compile preprocessed file's entry
Bool Compiler_compile(
	const Compiler *comp,
	const CompilerSettings *settings,
	const SHBinaryIdentifier *toCompile,
	const Allocator *alloc,
	CompileResult *result,
	Error *e_rr
);

//Extra warnings useful for debugging purposes and optimization.

typedef enum ECompilerWarning ECompilerWarning;

Bool Compiler_handleExtraWarnings(const SHFile *file, ECompilerWarning warning, const Allocator *alloc, Error *e_rr);

//Simplied compiler workflow, this is what the CLI calls too; it automatically handles threading and other things.

typedef enum ECompileType {
	ECompileType_Compile              //Compile all shaders into an oiSH file for consumption
} ECompileType;

Bool Compiler_getTargetsFromFile(
	CharString input,
	ECompileType compileType,
	U64 compileModeU64,
	Bool multipleModes,
	Bool combineFlag,
	Bool enableLogging,
	const Allocator *alloc,
	Bool *isFolder,                   //Optional (out); if the input is a folder or not
	CharString *output,               //Optional; the output directory. If NULL, will output file names only (relative to none)
	ListCharString *allFiles,         //Fully resolved file names (may contain duplicates per compile mode)
	ListCharString *allShaderText,    //Per file name: Input shader files
	ListCharString *allOutputs,       //Per file name: Output shader file names
	ListU8 *allCompileModes           //Per file name: ESHBinaryType
);

Bool Compiler_compileShaders(
	const ListCharString *allFiles,
	const ListCharString *allShaderText,
	const ListCharString *allOutputs,
	const ListU8 *allCompileOutputs,
	U64 threadCount,
	Bool isDebug,
	Bool keepRegisters,               //Keep declared but unused resources bound and reflected
	ECompilerWarning extraWarnings,
	Bool ignoreEmptyFiles,
	ECompileType type,
	const ListCharString *includeDirs,   //Optional list of extra include dirs
	Bool enableLogging,
	const Allocator *alloc,
	ListBuffer *allBuffers,           //Optional: buffer outputs (if NULL, outputs to file)
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
