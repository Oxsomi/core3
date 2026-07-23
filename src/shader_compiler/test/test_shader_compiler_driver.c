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

//shader_compiler/test/test_shader_compiler_driver.c

#include "test_shader_compiler_shared.h"
#include "shader_compiler/compiler.h"
#include "formats/oiSH/sh_binaries.h"
#include "formats/oiSH/sh_entries.h"
#include "formats/oiSH/sh_file.h"
#include "platforms/platform.h"
#include "types/container/buffer.h"
#include "types/container/list_basic_types.h"
#include "types/container/log.h"
#include "types/base/error.h"
#include "types/base/string_read_helper.h"

//Exercises the Compiler_compileShaders driver in compiler_helper.c: batching many files, the JobGroup
//fan-out under different thread counts, the DXIL backend, and error handling. These are the paths the
//annotation/corpus modules don't isolate.

void Test_shaderCompilerDriver(Test *t) {

	Test_setModule(t, "Compiler driver");

	const Allocator *alloc = Platform_instance->alloc;
	Error err = Error_none();
	Bool s_uccess = true;

	//A small batch of distinct, self-contained compute shaders (auto-bound resources, no includes).

	static const C8 *shaders[] = {

		"RWStructuredBuffer<uint> a;\n"
		"[[oxc::stage(\"compute\")]]\n[numthreads(8,1,1)]\n"
		"void main(uint i : SV_DispatchThreadID) { a[i] = i * 2 + 1; }\n",

		"RWStructuredBuffer<float> b;\n"
		"[[oxc::stage(\"compute\")]]\n[numthreads(4,4,1)]\n"
		"void main(uint2 i : SV_DispatchThreadID) { b[i.x + i.y] = (float)(i.x * i.y); }\n",

		"RWByteAddressBuffer c;\n"
		"[[oxc::stage(\"compute\")]]\n[numthreads(16,1,1)]\n"
		"void main(uint i : SV_DispatchThreadID) { c.Store<uint>(i * 4, i ^ 0xABCD); }\n",

		"StructuredBuffer<float> src; RWStructuredBuffer<float> dst;\n"
		"[[oxc::stage(\"compute\")]]\n[numthreads(32,1,1)]\n"
		"void main(uint i : SV_DispatchThreadID) { dst[i] = src[i] * 3; }\n"
	};

	const U64 n = sizeof(shaders) / sizeof(shaders[0]);

	//--- Multi-file batch (single-threaded) ---

	ListBuffer single = (ListBuffer) { 0 };
	Bool okSingle = compileInlineShaders(alloc, shaders, n, ESHBinaryType_SPIRV, 1, "driver_batch", true, &single, &err);

	U64 produced = 0;
	if (okSingle)
		for (U64 i = 0; i < single.length; ++i)
			if (Buffer_length(single.ptr[i]))
				++produced;

	Test_assert(t, "multi-file batch compiles", okSingle && single.length == n && produced == n);

	//--- Same batch, multi-threaded: the JobGroup fan-out must be thread-safe AND produce byte-identical
	//--- output regardless of thread count (deterministic compile). ---

	ListBuffer multi = (ListBuffer) { 0 };
	Bool okMulti = compileInlineShaders(alloc, shaders, n, ESHBinaryType_SPIRV, 4, "driver_batch", true, &multi, &err);

	Bool deterministic = okMulti && multi.length == single.length;

	for (U64 i = 0; deterministic && i < multi.length; ++i)
		deterministic = Buffer_eq(single.ptr[i], multi.ptr[i]);

	Test_assert(t, "output is independent of thread count", deterministic);

	ListBuffer_freeUnderlying(&single, alloc);
	ListBuffer_freeUnderlying(&multi, alloc);

	//--- DXIL backend produces a DXIL binary ---

	{
		const C8 *one[1] = { shaders[0] };
		ListBuffer dxil = (ListBuffer) { 0 };
		SHFile sh = (SHFile) { 0 };

		Bool hasDxil =
			compileInlineShaders(alloc, one, 1, ESHBinaryType_DXIL, 1, "driver_dxil", true, &dxil, &err) &&
			dxil.length == 1 && Buffer_length(dxil.ptr[0]) &&
			readOiSH(alloc, dxil.ptr[0], &sh, &err) && sh.binaries.length >= 1 &&
			Buffer_length(sh.binaries.ptr[0].binaries[ESHBinaryType_DXIL]);

		Test_assert(t, "DXIL target produces a DXIL binary", hasDxil);

		SHFile_free(&sh, alloc);
		ListBuffer_freeUnderlying(&dxil, alloc);
	}

	//--- [[oxc::binary(...)]] restricts, per-entrypoint, which backend an entrypoint is emitted for ---
	//An RT library with a both-backends raygen and a DXIL-only miss: SPIRV keeps only the raygen, DXIL keeps
	//both. This exercises per-entrypoint filtering inside a shared lib compile (the case the driver must get
	//right) while keeping >=1 entrypoint per backend, so neither side degenerates to an empty oiSH.

	{
		const C8 *lib[1] = {
			"RWStructuredBuffer<float> o;\n"
			"struct Payload { float v; };\n"
			"[shader(\"raygeneration\")]\n"
			"void mainRaygen() { o[0] = 1; }\n"
			"[[oxc::binary(\"dxil\")]]\n"
			"[shader(\"miss\")]\n"
			"void mainMiss(inout Payload p) { p.v = 0; }\n"
		};

		//DXIL: both entrypoints survive.

		ListBuffer d = (ListBuffer) { 0 };
		SHFile sd = (SHFile) { 0 };
		Bool onDxil =
			compileInlineShaders(alloc, lib, 1, ESHBinaryType_DXIL, 1, "binary_dxil", true, &d, &err) &&
			d.length == 1 && Buffer_length(d.ptr[0]) &&
			readOiSH(alloc, d.ptr[0], &sd, &err) && sd.entries.length == 2;

		Test_assert(t, "binary: DXIL keeps both raygen + DXIL-only miss", onDxil);

		SHFile_free(&sd, alloc);
		ListBuffer_freeUnderlying(&d, alloc);

		//SPIRV: the DXIL-only miss is filtered out, leaving just the raygen.

		ListBuffer s = (ListBuffer) { 0 };
		SHFile ss = (SHFile) { 0 };
		Bool onSpirv =
			compileInlineShaders(alloc, lib, 1, ESHBinaryType_SPIRV, 1, "binary_spv", true, &s, &err) &&
			s.length == 1 && Buffer_length(s.ptr[0]) &&
			readOiSH(alloc, s.ptr[0], &ss, &err) && ss.entries.length == 1;

		Bool onlyRaygen = onSpirv && CharString_equalsCStringSensitive(&ss.entries.ptr[0].name, "mainRaygen");

		Test_assert(t, "binary: SPIRV drops the DXIL-only miss, keeps raygen", onlyRaygen);

		SHFile_free(&ss, alloc);
		ListBuffer_freeUnderlying(&s, alloc);
	}

	//--- Invalid HLSL is reported as failure, not a crash ---

	{
		const C8 *bad[1] = { "this is not valid HLSL !!! void main(( {\n" };
		ListBuffer out = (ListBuffer) { 0 };
		Error e2 = Error_none();

		//enableLogging=false: the failure is expected and asserted on below, so keep the compiler quiet
		//instead of printing DXC diagnostics for a shader we deliberately broke.
		Bool compiledBad = compileInlineShaders(alloc, bad, 1, ESHBinaryType_SPIRV, 1, "driver_invalid", false, &out, &e2);

		Test_assert(t, "invalid shader fails cleanly", !compiledBad);

		ListBuffer_freeUnderlying(&out, alloc);
	}

	(void) s_uccess;
	Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
}
