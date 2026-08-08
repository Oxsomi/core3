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

//shader_compiler/test/test_shader_compiler_permutations.c

#include "test_shader_compiler_shared.h"
#include "shader_compiler/compiler.h"
#include "formats/oiSH/sh_binaries.h"
#include "formats/oiSH/sh_entries.h"
#include "formats/oiSH/sh_file.h"
#include "platforms/platform.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/list_basic_types.h"
#include "types/container/log.h"
#include "types/base/error.h"
#include "types/base/string_read_helper.h"

//The permutation system (multiple entrypoints in one file, each with its OWN oxc:: annotations that fan out
//into distinct compiled binaries) is easy to get subtly wrong when entrypoints DON'T share annotations - one
//entrypoint's extension / model / backend selection must not leak onto another's binary. These tests compile
//multi-entrypoint files where the entrypoints deliberately disagree and assert each binary reflects only its
//own annotations.

//Does any produced binary carry `ext`? Returns how many of the binaries do.
static U64 countBinariesWithExt(const SHFile *sh, ESHExtension ext) {
	U64 n = 0;
	for (U64 i = 0; i < sh->binaries.length; ++i)
		if (sh->binaries.ptr[i].identifier.extensions & ext)
			++n;
	return n;
}

//Is there an entrypoint named `nm` in the reflected file?
static Bool hasEntry(const SHFile *sh, const C8 *nm) {
	const CharString n = CharString_createRefCStrConst(nm);
	for (U64 i = 0; i < sh->entries.length; ++i)
		if (CharString_equalsStringSensitive(&sh->entries.ptr[i].name, &n))
			return true;
	return false;
}

void Test_shaderCompilerPermutations(Test *t) {

	Test_setModule(t, "Compiler permutations");

	const Allocator *alloc = Platform_instance->alloc;
	Error err = Error_none();

	//--- Per-entrypoint extension isolation: two standalone stages in one file, only the vertex one enables
	//--- 16BitTypes. Exactly one of the two produced binaries may carry that extension bit. ---

	{
		ListBuffer out = (ListBuffer) { 0 };
		SHFile sh = (SHFile) { 0 };

		Bool ok =
			compileFileShader(alloc, "permutations/mixed_extensions.hlsl", ESHBinaryType_SPIRV, true, false, &out, &err) &&
			out.length == 1 && Buffer_length(out.ptr[0]) && readOiSH(alloc, out.ptr[0], &sh, &err);

		Test_assert(t, "per-entrypoint extension isolation (only 1 of 2 binaries has 16BitTypes)",
			ok && sh.entries.length == 2 && sh.binaries.length == 2 &&
			countBinariesWithExt(&sh, ESHExtension_16BitTypes) == 1);

		SHFile_free(&sh, alloc);
		ListBuffer_freeUnderlying(&out, alloc);
		err = Error_none();
	}

	//--- Per-entrypoint model isolation: two stages request different shader models; each binary must keep
	//--- its own requested model rather than collapsing to one. ---

	{
		ListBuffer out = (ListBuffer) { 0 };
		SHFile sh = (SHFile) { 0 };

		Bool ok =
			compileFileShader(alloc, "permutations/mixed_models.hlsl", ESHBinaryType_SPIRV, true, false, &out, &err) &&
			out.length == 1 && Buffer_length(out.ptr[0]) && readOiSH(alloc, out.ptr[0], &sh, &err);

		Bool has67 = false, has65 = false;

		for (U64 i = 0; ok && i < sh.binaries.length; ++i) {
			U16 v = sh.binaries.ptr[i].identifier.shaderVersion;
			has67 |= v == OISH_SHADER_MODEL(6, 7);
			has65 |= v == OISH_SHADER_MODEL(6, 5);
		}

		Test_assert(t, "per-entrypoint model isolation (one binary 6.7, the other 6.5)",
			ok && sh.entries.length == 2 && has67 && has65);

		SHFile_free(&sh, alloc);
		ListBuffer_freeUnderlying(&out, alloc);
		err = Error_none();
	}

	//--- Per-entrypoint backend selection inside one shared lib compile: three RT entrypoints, each restricted
	//--- to a different backend set. This is the trickiest disagreement - all three share the lib's compile,
	//--- so the driver must filter per entrypoint at link time. raygen = both; miss = spv only; closesthit =
	//--- dxil only. So SPIRV keeps {raygen, miss}, DXIL keeps {raygen, closesthit}. ---

	{
		ListBuffer spv = (ListBuffer) { 0 }, dx = (ListBuffer) { 0 };
		SHFile ssh = (SHFile) { 0 }, dsh = (SHFile) { 0 };

		Bool spvOk =
			compileFileShader(alloc, "permutations/mixed_binary_lib.hlsl", ESHBinaryType_SPIRV, true, false, &spv, &err) &&
			spv.length == 1 && Buffer_length(spv.ptr[0]) && readOiSH(alloc, spv.ptr[0], &ssh, &err);

		Test_assert(t, "SPIRV keeps raygen + spv-only miss, drops dxil-only closesthit",
			spvOk && ssh.entries.length == 2 &&
			hasEntry(&ssh, "mainRaygen") && hasEntry(&ssh, "mainMiss") && !hasEntry(&ssh, "mainCH"));

		Bool dxOk =
			compileFileShader(alloc, "permutations/mixed_binary_lib.hlsl", ESHBinaryType_DXIL, true, false, &dx, &err) &&
			dx.length == 1 && Buffer_length(dx.ptr[0]) && readOiSH(alloc, dx.ptr[0], &dsh, &err);

		Test_assert(t, "DXIL keeps raygen + dxil-only closesthit, drops spv-only miss",
			dxOk && dsh.entries.length == 2 &&
			hasEntry(&dsh, "mainRaygen") && hasEntry(&dsh, "mainCH") && !hasEntry(&dsh, "mainMiss"));

		SHFile_free(&ssh, alloc);
		SHFile_free(&dsh, alloc);
		ListBuffer_freeUnderlying(&spv, alloc);
		ListBuffer_freeUnderlying(&dx, alloc);
		err = Error_none();
	}

	//--- Uniforms must be identical across every entrypoint of a compilation unit (they specialize one shared
	//--- linking step). Two entrypoints declaring DIFFERENT uniforms is illegal and must be rejected cleanly. ---

	{
		ListBuffer out = (ListBuffer) { 0 };
		Error e2 = Error_none();

		//enableLogging=false: the failure is expected and asserted on, so keep the compiler quiet.
		Bool compiled = compileFileShader(alloc, "permutations/mixed_uniforms.hlsl", ESHBinaryType_SPIRV, false, false, &out, &e2);

		Test_assert(t, "mismatched uniforms across entrypoints rejected", !compiled);

		ListBuffer_freeUnderlying(&out, alloc);
	}

	Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
}
