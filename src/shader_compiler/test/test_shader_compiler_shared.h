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

//shader_compiler/test/test_shader_compiler_shared.h

#pragma once
#include "types/test/test.h"

typedef struct Buffer Buffer;
typedef struct ListBuffer ListBuffer;
typedef struct SHFile SHFile;

//Each test module is one Test_* function invoked from test_shader_compiler_main.c.
//They compile HLSL through the real DXC/SPIRV pipeline, so they require a live Platform_instance.
//Declared in invocation order (see test_shader_compiler_main.c).

void Test_shaderCompilerParse(Test *t);           //Parse annotations -> SHEntryRuntime reflection
void Test_shaderCompilerBuiltInIncludes(Test *t); //The enumerable @-prefixed built-in include table
void Test_shaderCompilerAnnotations(Test *t);     //oxc:: extensions / model / vendor / defines / uniforms / stages / binary
void Test_shaderCompilerFeatures(Test *t);        //Shaders *using* extension features -> compiled + reflected
void Test_shaderCompilerStages(Test *t);          //One shader per pipeline stage -> reflected stage matches
void Test_shaderCompilerReflection(Test *t);      //Resource registers reflect with correct type/write/array/stride
void Test_shaderCompilerDriver(Test *t);          //compiler_helper compile driver: threads / modes / errors / round-trip
void Test_shaderCompilerPermutations(Test *t);    //Multi-entrypoint files with disagreeing oxc:: annotations
void Test_shaderCompilerCorpus(Test *t);          //Compile the whole test/hlsl corpus (SPIRV snapshot + DXIL coverage)

//--- Shared helpers (test_shader_compiler_util.c) ---

//Compile `count` inline HLSL sources (each into its own oiSH, all with the same binary type `mode`)
//through the real Compiler_compileShaders driver with `threadCount` execution contexts. Uses dummy file
//names and pre-loaded text, so it never touches the filesystem. Fills `out` with one oiSH buffer per
//source (empty for shaders that produced nothing). Caller frees `out`.
//Pass enableLogging=false when a failure is *expected* and the caller asserts on it itself (the compiler
//then stays quiet instead of printing diagnostics/status for a failure the test is deliberately provoking).
Bool compileInlineShaders(
	const Allocator *alloc,
	const C8 *const *srcs,
	U64 count,
	U8 mode,                //ESHBinaryType
	U64 threadCount,
	const C8 *namePrefix,   //Names each shader "<prefix>N.hlsl" so logs/errors are identifiable
	Bool enableLogging,
	ListBuffer *out,
	Error *e_rr
);

//Compile a single HLSL shader loaded from `path` (relative to the working dir) into an oiSH of binary type
//`mode`. Self-contained shaders only (@virtual includes). Fills `out` with one oiSH buffer. Caller frees.
//keepRegisters keeps declared but unused resources bound and reflected (--keep-registers).
Bool compileFileShader(
	const Allocator *alloc,
	const C8 *path,
	U8 mode,                //ESHBinaryType
	Bool enableLogging,
	Bool keepRegisters,
	ListBuffer *out,
	Error *e_rr
);

//Parse an in-memory oiSH buffer into an SHFile (buffer must stay valid for the read; SHFile is owned).
Bool readOiSH(const Allocator *alloc, Buffer buf, SHFile *out, Error *e_rr);

//Serialize an SHFile back into a freshly-allocated oiSH buffer (caller frees `out`).
Bool writeOiSH(const Allocator *alloc, const SHFile *file, Buffer *out, Error *e_rr);

//Verify a produced oiSH buffer round-trips: read it, serialize it back, and require byte-for-byte identity.
Bool oiSHRoundtrips(const Allocator *alloc, Buffer produced, Error *e_rr);
