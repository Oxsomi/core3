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

//shader_compiler/compiler_helper_internal.h

#pragma once
#include "types/container/list_basic_types.h"
#include "shader_compiler/compiler.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Internal declarations shared between the compiler_helper*.c translation units.
//These were file-local to compiler_helper.c before it was split and are not part of the public API.

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
);

Bool Compiler_getUniqueCompiles(
	const ListSHEntryRuntime *runtimeEntries,
	ListU32 *compileCombinations,
	const Allocator *alloc,
	Error *e_rr
);

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
);

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
);

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
);

typedef struct LinkEntry {
	ListU16 runtimeEntries;
	Buffer uniformData;
	U16 combinationId, entrypointId;
	U8 padding[4];
} LinkEntry;

TList(LinkEntry);

void ListLinkEntry_freeUnderlying(ListLinkEntry* entries, const Allocator *alloc);

Bool Compiler_getLinkEntries(
	const Compiler *compiler,
	const ListSHEntryRuntime *runtimeEntries,
	const SHBinaryIdentifier *binaryIdentifier,
	ESHBinaryType binaryType,
	Buffer *binary,
	ListCompilerEntrypoint *entrypoints,
	ListLinkEntry *linkEntries,
	const Allocator *alloc,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
