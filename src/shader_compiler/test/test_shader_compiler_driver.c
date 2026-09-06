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

//Exercises the Compiler_compileShaders driver in compiler_helper.c: batching many files,
// the JobGroup fan-out under different thread counts, the DXIL backend, and error handling.
//These are the paths the annotation/corpus modules don't isolate.

void Test_shaderCompilerDriver(Test *t) {

	Test_setModule(t, "Compiler driver");

	const Allocator *alloc = Platform_instance->alloc;
	Error err = Error_none();

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
	Bool okSingle = compileInlineShaders(alloc, shaders, n, EGfxBinaryType_SPIRV, 1, "driver_batch", true, &single, &err);

	U64 produced = 0;
	if (okSingle)
		for (U64 i = 0; i < single.length; ++i)
			if (Buffer_length(single.ptr[i]))
				++produced;

	Test_assert(t, "multi-file batch compiles", okSingle && single.length == n && produced == n);

	//--- Same batch, multi-threaded: the JobGroup fan-out must be thread-safe AND produce byte-identical
	//--- output regardless of thread count (deterministic compile). ---

	//A wasm build without -pthread can't spawn threads, so it runs single threaded twice instead.
	//That still checks that repeated compiles are byte-identical, which is the property consumers
	// rely on, just not the fan-out.

	#if _PLATFORM_TYPE == PLATFORM_WEB && !defined(__EMSCRIPTEN_PTHREADS__)
		const U32 driverTestThreads = 1;
	#else
		const U32 driverTestThreads = 4;
	#endif

	ListBuffer multi = (ListBuffer) { 0 };
	Bool okMulti = compileInlineShaders(
		alloc, shaders, n, EGfxBinaryType_SPIRV, driverTestThreads, "driver_batch", true, &multi, &err
	);

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
			compileInlineShaders(alloc, one, 1, EGfxBinaryType_DXIL, 1, "driver_dxil", true, &dxil, &err) &&
			dxil.length == 1 && Buffer_length(dxil.ptr[0]) &&
			readOiSH(alloc, dxil.ptr[0], &sh, &err) && sh.binaries.length >= 1 &&
			Buffer_length(sh.binaries.ptr[0].binaries[EGfxBinaryType_DXIL]);

		Test_assert(t, "DXIL target produces a DXIL binary", hasDxil);

		SHFile_free(&sh, alloc);
		ListBuffer_freeUnderlying(&dxil, alloc);
	}

	//--- [[oxc::binary(...)]] restricts, per-entrypoint, which backend an entrypoint is emitted for ---
	//An RT library with a both-backends raygen and a DXIL-only miss: SPIRV keeps only the raygen, DXIL keeps both.
	//This exercises per-entrypoint filtering inside a shared lib compile (the case the driver must get right)
	// while keeping >=1 entrypoint per backend, so neither side degenerates to an empty oiSH.

	{
		//DXIL: both entrypoints survive.

		ListBuffer d = (ListBuffer) { 0 };
		SHFile sd = (SHFile) { 0 };
		Bool onDxil =
			compileFileShader(alloc, "driver/binary_rt_lib.hlsl", EGfxBinaryType_DXIL, true, false, &d, &err) &&
			d.length == 1 && Buffer_length(d.ptr[0]) &&
			readOiSH(alloc, d.ptr[0], &sd, &err) && sd.entries.length == 2;

		Test_assert(t, "binary: DXIL keeps both raygen + DXIL-only miss", onDxil);

		SHFile_free(&sd, alloc);
		ListBuffer_freeUnderlying(&d, alloc);

		//SPIRV: the DXIL-only miss is filtered out, leaving just the raygen.

		ListBuffer s = (ListBuffer) { 0 };
		SHFile ss = (SHFile) { 0 };
		Bool onSpirv =
			compileFileShader(alloc, "driver/binary_rt_lib.hlsl", EGfxBinaryType_SPIRV, true, false, &s, &err) &&
			s.length == 1 && Buffer_length(s.ptr[0]) &&
			readOiSH(alloc, s.ptr[0], &ss, &err) && ss.entries.length == 1;

		Bool onlyRaygen = onSpirv && CharString_equalsCStringSensitive(&ss.entries.ptr[0].name, "mainRaygen");

		Test_assert(t, "binary: SPIRV drops the DXIL-only miss, keeps raygen", onlyRaygen);

		SHFile_free(&ss, alloc);
		ListBuffer_freeUnderlying(&s, alloc);
	}

	//--- A produced oiSH round-trips: read it into an SHFile, serialize it back, and the bytes are
	//--- identical (canonical, deterministic serialization); the re-read SHFile keeps the same shape. ---

	{
		const C8 *one[1] = { shaders[0] };
		ListBuffer rt = (ListBuffer) { 0 };
		SHFile a = (SHFile) { 0 }, b = (SHFile) { 0 };
		Buffer rewritten = Buffer_createNull();

		Bool readOk =
			compileInlineShaders(alloc, one, 1, EGfxBinaryType_SPIRV, 1, "roundtrip", true, &rt, &err) &&
			rt.length == 1 && Buffer_length(rt.ptr[0]) &&
			readOiSH(alloc, rt.ptr[0], &a, &err);

		Bool rewrote = readOk && writeOiSH(alloc, &a, &rewritten, &err);

		Test_assert(t, "oiSH read -> write is byte-identical", rewrote && Buffer_eq(rt.ptr[0], rewritten));

		Bool reread = rewrote && readOiSH(alloc, rewritten, &b, &err);

		Test_assert(t, "re-read oiSH keeps entry + binary counts",
			reread && a.entries.length == b.entries.length && a.binaries.length == b.binaries.length);

		Buffer_free(&rewritten, alloc);
		SHFile_free(&a, alloc);
		SHFile_free(&b, alloc);
		ListBuffer_freeUnderlying(&rt, alloc);
		err = Error_none();
	}

	//--- Includes are recorded whichever annotation the entrypoint uses.
	//--- A [shader("")] entry compiles as a library and is then linked, and the link reads no files of its
	//--- own, so the binary that gets registered is the link's while the includes are the compile's.
	//--- Builtin includes are used here because they resolve without touching the filesystem. ---

	{
		static const C8 *annotated[2] = {

			"#include \"@types.hlsli\"\n"
			"RWStructuredBuffer<F32> _out;\n"
			"[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
			"void main() { _out[0] = 1; }\n",

			"#include \"@types.hlsli\"\n"
			"RWStructuredBuffer<F32> _out;\n"
			"[shader(\"compute\")]\n[numthreads(1,1,1)]\n"
			"void main() { _out[0] = 1; }\n"
		};

		ListBuffer inc = (ListBuffer) { 0 };
		SHFile direct = (SHFile) { 0 }, lib = (SHFile) { 0 };

		Bool compiledInc =
			compileInlineShaders(alloc, annotated, 2, EGfxBinaryType_SPIRV, 1, "driver_includes", true, &inc, &err) &&
			inc.length == 2 &&
			readOiSH(alloc, inc.ptr[0], &direct, &err) &&
			readOiSH(alloc, inc.ptr[1], &lib, &err);

		Test_assert(t, "[[oxc::stage]] records its includes", compiledInc && direct.includes.length);
		Test_assert(t, "[shader()] records the same includes", compiledInc && lib.includes.length == direct.includes.length);

		SHFile_free(&direct, alloc);
		SHFile_free(&lib, alloc);
		ListBuffer_freeUnderlying(&inc, alloc);
		err = Error_none();
	}

	//--- --no-opt (-Od) exists so the optimizer stops folding what --debug's line info describes. ---
	//--- Asserted on DXIL: rich SPIRV debug info already holds the spirv-opt passes off, so -Od changes ---
	//--- nothing there; DXIL runs LLVM's full pipeline and folds the helper's body lines away unless ---
	//--- -Od keeps them. Strictly more distinct DILocation lines with the flag is the whole contract, ---
	//--- and equal counts would mean -Od never reached DXC. ---

	{
		static const C8 *foldable[1] = {
			"RWStructuredBuffer<float> _out;\n"
			"float helper(float v) {\n"
			"    float a = v * 2;\n"
			"    float b = a + 3;\n"
			"    float c = b * b;\n"
			"    return c - a;\n"
			"}\n"
			"[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
			"void main(uint i : SV_DispatchThreadID) {\n"
			"    float x = 1.5;\n"
			"    float y = helper(x);\n"
			"    float z = helper(y);\n"
			"    _out[i] = z;\n"
			"}\n"
		};

		U64 lines[2] = { 0 };
		Bool okBoth = true;

		for (U64 pass = 0; pass < 2 && okBoth; ++pass) {

			ListCharString files = (ListCharString) { 0 }, texts = (ListCharString) { 0 }, outs = (ListCharString) { 0 };
			ListCharString includeDirs = (ListCharString) { 0 };
			ListU8 modes = (ListU8) { 0 };
			ListBuffer buffers = (ListBuffer) { 0 };
			SHFile sh = (SHFile) { 0 };
			CharString disasm = CharString_createNull();
			Compiler disasmComp = (Compiler) { 0 };
			Error e2 = Error_none();

			okBoth &=
				ListCharString_pushBack(&files, CharString_createRefCStrConst("noopt.hlsl"), alloc, NULL) &&
				ListCharString_pushBack(&texts, CharString_createRefCStrConst(foldable[0]), alloc, NULL) &&
				ListCharString_pushBack(&outs, CharString_createRefCStrConst("noopt.oiSH"), alloc, NULL) &&
				ListU8_pushBack(&modes, (U8) EGfxBinaryType_DXIL, alloc, NULL) &&
				Compiler_compileShaders(
					&files, &texts, &outs, &modes,
					1,
					true,                       //isDebug: the line info -Od exists to protect
					pass == 1,                  //noOpt on the second pass only
					false, (ECompilerWarning) 0, false, ECompileType_Compile,
					&includeDirs, false, alloc, &buffers, &e2
				) &&
				buffers.length == 1 &&
				readOiSH(alloc, buffers.ptr[0], &sh, &e2) &&
				sh.binaries.length >= 1 &&
				Compiler_create(alloc, &disasmComp, &e2) &&
				Compiler_disassemble(
					&disasmComp, EGfxBinaryType_DXIL, sh.binaries.ptr[0].binaries[EGfxBinaryType_DXIL], alloc, &disasm, &e2
				);

			//Distinct `!DILocation(line: N` values, collected into a bitset of the small line numbers this
			// shader can produce.

			if (okBoth) {

				U64 seen = 0;
				CharString needle = CharString_createRefCStrConst("!DILocation(line: ");
				U64 off = 0;

				while (true) {

					U64 at = CharString_findFirstStringSensitive(&disasm, &needle, off, 0);

					if(at == U64_MAX)
						break;

					off = at + CharString_length(needle);

					U64 v = 0;

					for (U64 i = off; i < CharString_length(disasm) && C8_isDec(disasm.ptr[i]); ++i)
						v = v * 10 + (U64)(disasm.ptr[i] - '0');

					if(v && v < 64)
						seen |= (U64) 1 << v;
				}

				U64 count = 0;

				for(U64 i = 0; i < 64; ++i)
					count += (seen >> i) & 1;

				lines[pass] = count;
			}

			CharString_free(&disasm, alloc);
			if(disasmComp.interfaces[0]) Compiler_free(&disasmComp, alloc);
			SHFile_free(&sh, alloc);
			ListBuffer_freeUnderlying(&buffers, alloc);
			ListCharString_free(&files, alloc);
			ListCharString_free(&texts, alloc);
			ListCharString_free(&outs, alloc);
			ListCharString_free(&includeDirs, alloc);
			ListU8_free(&modes, alloc);
		}

		Test_assert(t, "both --no-opt passes compiled and disassembled", okBoth);
		Test_assert(t, "--no-opt keeps strictly more source lines alive", okBoth && lines[1] > lines[0]);
	}

	//--- Invalid HLSL is reported as failure, not a crash ---

	{
		const C8 *bad[1] = { "this is not valid HLSL !!! void main(( {\n" };
		ListBuffer out = (ListBuffer) { 0 };
		Error e2 = Error_none();

		//enableLogging=false: the failure is expected and asserted on below, so keep the compiler quiet
		//instead of printing DXC diagnostics for a shader we deliberately broke.
		Bool compiledBad = compileInlineShaders(alloc, bad, 1, EGfxBinaryType_SPIRV, 1, "driver_invalid", false, &out, &e2);

		Test_assert(t, "invalid shader fails cleanly", !compiledBad);

		ListBuffer_freeUnderlying(&out, alloc);
	}

	Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
}
