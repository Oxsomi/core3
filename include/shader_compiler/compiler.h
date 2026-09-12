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
typedef struct Platform Platform;
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

//The Vulkan target env spelling of each version, shared by the -fspv-target-env the compile passes
// and the spirv-opt spelling the link steps print, so the two can't drift.

static inline const C8 *Compiler_spirvTargetEnvName(ESpirvVersion version) {
	switch (version) {
		case ESpirvVersion_1_6:    return "vulkan1.3";
		case ESpirvVersion_1_4:    return "vulkan1.1spirv1.4";
		default:                   return "vulkan1.1";
	}
}

//The version one final binary's LINK runs its SPIR-V tooling at, from that binary's own stage and
// extensions (a lib link passes EGfxPipelineStage_Count); Compiler_linkSPIRV and the link step
// descriptions both derive it here.

static inline ESpirvVersion Compiler_linkSpirvVersion(EGfxPipelineStage stage, ESHExtension exts) {

	Bool isRt = stage >= EGfxPipelineStage_RtStartExt && stage <= EGfxPipelineStage_RtEndExt;
	isRt |= !!(exts & ESHExtension_RayQuery);

	Bool isCoop = !!(exts & (
		ESHExtension_CoopVec | ESHExtension_CoopMat | ESHExtension_CoopFP8 | ESHExtension_CoopVecTraining
	));

	Bool isMeshTask = stage == EGfxPipelineStage_MeshExt || stage == EGfxPipelineStage_TaskExt;

	return Compiler_requiredSpirvVersion(isRt, isCoop, isMeshTask);
}

//One spelling for the flag that keeps unused resource bindings bound: the compile, the printed link
// steps and (as a wide string literal) the DXIL link all pass it.

#define COMPILER_KEEP_ALL_BINDINGS "-fhlsl-unused-resource-bindings=keep-all"

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
	EGfxBinaryType outputType;

	ListCharString includeDirs; //Optional extra include dirs to search

	Bool debug;
	Bool noOptimization;        //-Od: keep the codegen unoptimized, so debug line info survives per statement
	Bool infoAboutIncludes;     //Saves extra include info, useful for debugging includes or hot shader reload
	Bool isLib;
	Bool containsGfxOrComp;
	Bool isRt;
	Bool keepUnusedRegisters;   //Keep declared but unused resources bound and reflected (stable layouts across variants)

	//Compiler_reflect only: describe what parsed even when the source has errors.
	//A build wants nothing from a source that doesn't compile, but an editor navigating a file being typed
	//wants the declarations that did parse, and refusing on one bad token means no outline at all.
	Bool reflectAllowErrors;

	//Compiler_reflect only: the permutation the reflection parses as. Zero-init keeps the historical
	//behavior (every extension's define on, no $-defines): the everything-enabled parse an editor shows
	//by default. Following one binary instead passes ~its extension set and its uniforms, so `half`,
	//PAQ and friends mean what they mean in THAT permutation.
	ESHExtension reflectDisabledExt;    //extensions whose __OXC_EXT_* define (and gated flags) are omitted
	ListCharString reflectDefines;      //name,value pairs as -D$name[=value]; a $-prefixed name as is ($$name is a uniform)

} CompilerSettings;

typedef enum ECompileErrorType {
	ECompileErrorType_Warn,
	ECompileErrorType_Error,
	ECompileErrorType_Count
} ECompileErrorType;

typedef struct CompileError {

	U32 compileIndex;           //Compile index. % EGfxBinaryType_Count = binaryType, / EGfxBinaryType = i of strings[i]

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
	EGfxPipelineStage stage;
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

//Hand the host's Platform across the module boundary.
//
//A shared shader compiler (DynamicLinkingShaderCompiler) still links OxC3_platforms statically,
// so the module gets its own copy of Platform_instance and it is NULL until told otherwise;
// every Compiler_* call would then dereference NULL.
//This is the same hand-off GraphicsInterface_getTable does for a dynamically loaded graphics backend,
// and platform.h says as much where Platform_instance is declared.
//
//Call it once, after Platform_create and before any other Compiler_* call.
//In a static build it is an inline no-op, so callers don't need to care which linkage they got.

#ifdef SHADER_COMPILER_DYNAMIC
	void Compiler_setPlatform(Platform *instance);
#else
	static inline void Compiler_setPlatform(Platform *instance) { (void) instance; }
#endif

//Built-in includes.
//
//A shader reaches these by name with an @ prefix (#include "@types.hlsli"); they're compiled into the
//binary rather than read from disk, so they resolve identically on every platform and in a sandbox.
//The list is enumerable so a tool can show what's available without hardcoding it,
// an editor's autocomplete, a --list-includes flag, or a web front end serving the sources to a browser.
//
//`name` has no @ prefix. `source` is NUL terminated, owned by the compiler and valid for the process.

typedef struct CompilerBuiltInInclude {
	const C8 *name;
	const C8 *source;
} CompilerBuiltInInclude;

U64 Compiler_builtInIncludeCount();

//NULL if i is out of range

const CompilerBuiltInInclude *Compiler_builtInIncludeAt(U64 i);

//Looks up by name, with or without the leading @, case insensitive (matching #include resolution).
//NULL if there's no such built-in.

const CompilerBuiltInInclude *Compiler_findBuiltInInclude(CharString name);

//A separate Compiler should be created per thread

//CRC32C of a source with every carriage return skipped, so a text hashes the same under either line ending. The
// source hash and every include hash are this, and nothing else hashes source.
U32 Compiler_hashSource(CharString text);

Bool Compiler_create(const Allocator *alloc, Compiler *comp, Error *e_rr);
void Compiler_free(Compiler *comp, const Allocator *alloc);

//Call this on shutdown for a clean exit
void Compiler_shutdown();

//Generate disassembly from buffer

Bool Compiler_disassemble(
	const Compiler *comp, EGfxBinaryType type, Buffer buf, const Allocator *alloc, CharString *result, Error *e_rr
);

//Assemble text back into a binary (SPIRV text via spirv-tools, DXIL LL text via DXC's IDxcAssembler)

Bool Compiler_assemble(
	const Compiler *comp, EGfxBinaryType type, CharString text, const Allocator *alloc, Buffer *result, Error *e_rr
);

//Canonical form for semantic comparison: every id is renumbered compactly in first-use order and the
// generator word is cleared, so two modules that differ only in id numbering or producing tool compare
// byte for byte equal.

Bool Compiler_canonicalizeSPIRV(Buffer binary, const Allocator *alloc, Buffer *result, Error *e_rr);

//Judge a standalone binary before anything trusts it: spirv-val for SPIR-V, DXC's validator for a DXIL container.
//The verdict comes back through valid/errorText, since an invalid binary is an answer about the input rather than a
// failure of this call; errorText carries the validator's own message.

Bool Compiler_validate(
	const Compiler *comp, EGfxBinaryType type, Buffer binary, const Allocator *alloc, Bool *valid, CharString *errorText,
	Error *e_rr
);

//Query entrypoints embedded in a binary

Bool Compiler_getUniqueEntrypoints(
	const Compiler *compiler,
	EGfxBinaryType binaryType,
	Buffer binary,                                   //Must be a lib
	Bool showAll,                                    //true: show all entrypoints, false: only show targets to link
	ListCompilerEntrypoint *uniqueEntrypoints,
	const Allocator *alloc,
	Error *e_rr
);

//Convert assembly (SPIRV and DXIL) to oiSH by using the assembly

Bool Compiler_process(
	const Compiler *compiler,           //To be able to get reflection data
	EGfxBinaryType type,
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
	EGfxBinaryType type,
	const ListBuffer *inputs,              //Input binary/binaries
	const ListSHUniformRuntime *uniforms,  //Uniform descriptions (to index uniformData and to link)
	Buffer uniformData,                    //Contents of the current compilation
	const CharString *entrypoint,          //Entrypoint specialization (empty = keep as lib, otherwise specialize)
	U16 shaderVersion,                     //U8 maj, minor
	EGfxPipelineStage stageType,
	ESHExtension exts,
	Bool keepRegisters,                    //Keep unused resources through the link's own codegen pass too
	ListCompileError *errors,
	Buffer *result,                        //Output binary: Either library or specialized binary (PS/GS/CS/etc.)
	const Allocator *alloc,
	Error *e_rr
);

Bool Compiler_finalizeEntrypoint(       //Push reflection data into final entrypoint
	U32 localSize[3],                   //If compute-adj (mesh shaders too) the local size per group
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
U16 Compiler_minFeatureSetStage(EGfxPipelineStage stage, U16 waveSize);
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
//With settings->reflectAllowErrors the result is partial rather than absent when the source has errors.
//Unlike Compiler_parse (which keeps only annotated entrypoints), this preserves the full node tree
// with source locations, so it can drive editor intelligence (semantic highlighting, outline, go-to-definition).
//Reflection runs on the preprocessed source, so settings->string / defines determine which code paths are visible.
//spirv-val's verdict on arbitrary bytes; valid is the answer, errorText the validator's own message
//(allocated when invalid and errorText is given). The error channel is for the call itself failing.
Bool Compiler_validateSPIRV(Buffer binary, const Allocator *alloc, Bool *valid, CharString *errorText, Error *e_rr);
Bool Compiler_validateDXIL(
	const Compiler *comp, Buffer binary, const Allocator *alloc, Bool *valid, CharString *errorText, Error *e_rr
);

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

//Builds the exact dxc invocation for one binary of one entrypoint.
//args receives every argument Compiler_compile hands IDxcCompiler3, in CLI spelling, so the list is also
// the dxc command line that reproduces the binary.
//When toCompile has uniforms the input source is amended as well: amendedSource receives the full input
// (the spec constant preamble, a #line mapping, then the original text); otherwise it stays empty and
// settings->string is the input as is.
//Compiler_compile builds its own invocation through this function, so the answer can't drift from what ran.
//args and amendedSource must come in empty and are only written on success.
Bool Compiler_buildCompileArgs(
	const CompilerSettings *settings,
	const SHBinaryIdentifier *toCompile,
	ListCharString *args,
	CharString *amendedSource,
	const Allocator *alloc,
	Error *e_rr
);

//The deduped compiles a parsed entry list expands into, exactly as the compile driver spawns them.
//One U32 per unique compile: combinationId in b0..14, runtimeEntryId in b16..30; b15 is set when any
// entry sharing the compile is raytracing and b31 when any is graphics or compute. Both the compile
// (Compiler_describeCompile) and the link follow those aggregates, so a single entry never speaks for
// a shared compile.
//Uniform combinations collapse to one compile here and specialize at link.
Bool Compiler_getUniqueCompiles(
	const ListSHEntryRuntime *runtimeEntries,
	ListU32 *compileCombinations,
	const Allocator *alloc,
	Error *e_rr
);

//The HLSL source the DXIL link compiles as its "uniforms" library for one permutation: one
// export $$specConst_<name>() function per uniform, returning that permutation's value.
//Compiler_linkDXIL builds its own library through this, so a caller printing the link recipe reads
// the code path that runs it. out must come in empty and is only written on success.
Bool Compiler_buildUniformExportsHLSL(
	const ListSHUniformRuntime *uniforms,
	Buffer uniformData,
	ESHExtension exts,
	const Allocator *alloc,
	CharString *out,
	Error *e_rr
);

//The CompilerSettings and SHBinaryIdentifier one (entry, combination) compiles as, built exactly the
// way the compile driver builds them, so an argv query can never drift from the compile it describes.
//isRt and isGfxOrComp are the aggregates Compiler_getUniqueCompiles packs (b15 and b31), never a single
// entry's own flags.
//settings and identifier only reference the entry and the inputs; they own nothing and must not
// outlive them. includeDirs may be NULL for none.
//Feed the results to Compiler_buildCompileArgs for the dxc invocation, or Compiler_compile to run it.
Bool Compiler_describeCompile(
	const SHEntryRuntime *entry,
	U16 combinationId,
	EGfxBinaryType binaryType,
	Bool isDebug,
	Bool noOpt,
	Bool keepRegisters,
	Bool isRt,
	Bool isGfxOrComp,
	CharString inputPath,
	CharString input,
	const ListCharString *includeDirs,
	CompilerSettings *settings,
	SHBinaryIdentifier *identifier,
	Error *e_rr
);

//One link step that turns a compile's output into a final binary: which permutation it freezes, how
// it is targeted, and the arguments each backend's step runs with.
//identifier is the FINAL binary's identifier (entrypoint and stage specialized for gfx/comp [shader]
// entries, uniforms carrying this permutation's values); it references the entries and owns nothing.
//profile, uniformsHlsl and the lists are owned; free with ListCompilerLinkStep_freeUnderlying.

typedef struct CompilerLinkStep {

	SHBinaryIdentifier identifier;

	CharString profile;           //lib_M_m for a lib link, <stage>_M_m for a specialized entrypoint
	CharString uniformsHlsl;      //DXIL: the "uniforms" library source; empty without uniforms

	ListCharString uniformsArgs;  //DXIL: the dxc args compiling that library
	ListCharString libs;          //DXIL: the library names handed to IDxcLinker, in registration order
	ListCharString linkArgs;      //DXIL: extra IDxcLinker arguments
	ListCharString spirvOpt;      //SPIRV: the spirv-opt spelling of this step; empty when the link copies as is

	U16 runtimeEntryId;           //The stored entry the driver resolves this link through
	U16 combinationId;            //Its full combination id, uniform values included
	U32 padding;

} CompilerLinkStep;

TList(CompilerLinkStep);

void ListCompilerLinkStep_freeUnderlying(ListCompilerLinkStep *steps, const Allocator *alloc);

//The profile string a link targets. Compiler_linkDXIL formats its own through this as well, so the
// printed step and the executed link share one spelling.
Bool Compiler_linkProfile(
	Bool isLib, EGfxPipelineStage stage, U16 shaderVersion, const Allocator *alloc, CharString *out, Error *e_rr
);

//The link steps that finish one compile: enumerated by the same matching core the link jobs run
// (Compiler_getLinkEntries over the parsed entries instead of the binary's reflection) and described
// with the same helpers the links execute, so a printed step can't drift from the link it describes.
//compiled is the compile's identifier and storedEntryId the entry the driver stores for it (both come
// out of Compiler_getUniqueCompiles). [[oxc::stage]] entries never link and produce no steps;
// [shader] entries always link.
Bool Compiler_getLinkSteps(
	const ListSHEntryRuntime *entries,
	const SHBinaryIdentifier *compiled,
	U16 storedEntryId,
	EGfxBinaryType type,
	Bool keepRegisters,
	ListCompilerLinkStep *steps,
	const Allocator *alloc,
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
	ListU8 *allCompileModes           //Per file name: EGfxBinaryType
);

Bool Compiler_compileShaders(
	const ListCharString *allFiles,
	const ListCharString *allShaderText,
	const ListCharString *allOutputs,
	const ListU8 *allCompileOutputs,
	U64 threadCount,
	Bool isDebug,
	Bool noOpt,                       //-Od; only meaningful with isDebug, where line info is the point
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
