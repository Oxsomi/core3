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

//shader_compiler/test/test_shader_compiler_args.c

#include "test_shader_compiler_shared.h"
#include "shader_compiler/compiler.h"
#include "formats/oiSH/sh_binaries.h"
#include "formats/gfx_util/gfx_util.h"
#include "platforms/platform.h"
#include "types/container/string.h"
#include "types/base/string_read_helper.h"
#include "types/container/log.h"
#include "types/base/error.h"

//Compiler_buildCompileArgs as a pure query: the argv it returns is the dxc command line Compiler_compile
// runs, buildable without a Compiler instance or a live dxc.
//The full argv is proven through Compiler_compile by every compiling suite (the corpus and sample pins
// are byte exact), so this asserts what makes the query itself trustworthy on its own: the backend
// discriminating flags, the target and entrypoint spelling, and the guards on inputs and outputs.

static Bool argsContain(const ListCharString *args, const C8 *arg) {

	CharString ref = CharString_createRefCStrConst(arg);

	for(U64 i = 0; i < args->length; ++i)
		if(CharString_equalsStringSensitive(&args->ptr[i], &ref))
			return true;

	return false;
}

//A flag and its value are separate argv elements and dxc pairs them by position, so the test requires
// them adjacent rather than merely both present somewhere.

static Bool argsPairStr(const ListCharString *args, const C8 *flag, const CharString *value) {

	CharString flagRef = CharString_createRefCStrConst(flag);

	for(U64 i = 0; i + 1 < args->length; ++i)
		if(
			CharString_equalsStringSensitive(&args->ptr[i], &flagRef) &&
			CharString_equalsStringSensitive(&args->ptr[i + 1], value)
		)
			return true;

	return false;
}

static Bool argsPair(const ListCharString *args, const C8 *flag, const C8 *value) {
	CharString valueRef = CharString_createRefCStrConst(value);
	return argsPairStr(args, flag, &valueRef);
}

void Test_shaderCompilerBuildArgs(Test *t) {

	Test_setModule(t, "Compiler build args");

	const Allocator *alloc = Platform_instance->alloc;
	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	ListCharString args = (ListCharString) { 0 };
	CharString amended = CharString_createNull();
	ListCharString oddDefines = (ListCharString) { 0 };
	Compiler comp = (Compiler) { 0 };
	CompileResult parsed = (CompileResult) { 0 };
	ListU32 compiles = (ListU32) { 0 };
	Bool created = false;

	SHBinaryIdentifier id = (SHBinaryIdentifier) {
		.entrypoint = CharString_createRefCStrConst("main"),
		.shaderVersion = (6 << 8) | 5,
		.stageType = EGfxPipelineStage_Compute
	};

	CompilerSettings settings = (CompilerSettings) {
		.string = CharString_createRefCStrConst("[numthreads(1, 1, 1)] void main() {}"),
		.format = ECompilerFormat_HLSL,
		.outputType = EGfxBinaryType_SPIRV
	};

	//The SPIRV leg, on the flags that pick the backend and name the target and entrypoint

	gotoIfError3(clean, Compiler_buildCompileArgs(&settings, &id, &args, &amended, alloc, e_rr));

	Test_assert(t, "spirv leg spells -spirv", argsContain(&args, "-spirv"));
	Test_assert(t, "spirv leg renames the entrypoint", argsContain(&args, "-fspv-entrypoint-name=main"));
	Test_assert(t, "target profile is spelled", argsPair(&args, "-T", "cs_6_5"));
	Test_assert(t, "entrypoint is spelled", argsPair(&args, "-E", "main"));
	Test_assert(t, "no uniforms leaves the source unamended", !amended.ptr);

	//A prior result must be taken by the caller, not silently appended to

	Test_assert(
		t, "refuses non empty outputs",
		!Compiler_buildCompileArgs(&settings, &id, &args, &amended, alloc, NULL)
	);

	ListCharString_freeUnderlying(&args, alloc);

	//The DXIL leg of the same identifier

	settings.outputType = EGfxBinaryType_DXIL;
	gotoIfError3(clean, Compiler_buildCompileArgs(&settings, &id, &args, &amended, alloc, e_rr));

	Test_assert(t, "dxil leg doesn't spell -spirv", !argsContain(&args, "-spirv"));
	Test_assert(t, "dxil leg scopes the binding space", argsContain(&args, "-auto-binding-space"));

	ListCharString_freeUnderlying(&args, alloc);

	//Input validation: missing pointers and a defines list that isn't name, value pairs

	Test_assert(
		t, "refuses missing inputs",
		!Compiler_buildCompileArgs(NULL, &id, &args, &amended, alloc, NULL)
	);

	gotoIfError3(clean, ListCharString_pushBack(&oddDefines, CharString_createRefCStrConst("X"), alloc, e_rr));
	id.defines = oddDefines;

	Test_assert(
		t, "refuses odd define pairs",
		!Compiler_buildCompileArgs(&settings, &id, &args, &amended, alloc, NULL)
	);

	Test_assert(t, "a refused build hands back nothing", !args.length && !amended.ptr);

	id.defines = (ListCharString) { 0 };

	//The driver's expansion produces the same argv: parse a two entry source, expand it exactly the way
	// the compile does (Compiler_getUniqueCompiles + Compiler_describeCompile), and require the flags
	// that identify each compile. This is also the composition the page's argv query runs.

	gotoIfError3(clean, Compiler_create(alloc, &comp, e_rr));
	created = true;

	static const C8 *twoEntrySrc =
		"RWByteAddressBuffer buf;\n"
		"[[oxc::stage(\"compute\")]]\n"
		"[numthreads(1, 1, 1)]\n"
		"void mainCS(uint id : SV_DispatchThreadID) { buf.Store<uint>(id * 4, 1); }\n"
		"[[oxc::stage(\"pixel\")]]\n"
		"float4 mainPS() : SV_Target { return 1; }\n";

	CompilerSettings parseSettings = (CompilerSettings) {
		.string = CharString_createRefCStrConst(twoEntrySrc),
		.path = CharString_createRefCStrConst("args_two_entries.hlsl"),
		.format = ECompilerFormat_HLSL,
		.outputType = EGfxBinaryType_SPIRV
	};

	gotoIfError3(clean, Compiler_parse(&comp, &parseSettings, alloc, &parsed, e_rr));

	Test_assert(t, "parse finds two entrypoints", parsed.isSuccess && parsed.shEntriesRuntime.length == 2);

	gotoIfError3(clean, Compiler_getUniqueCompiles(&parsed.shEntriesRuntime, &compiles, alloc, e_rr));

	Test_assert(t, "two entrypoints expand to two compiles", compiles.length == 2);

	Bool entriesSpelled = true, profilesMatch = true;

	for (U64 i = 0; i < compiles.length; ++i) {

		U16 entryId = (compiles.ptr[i] >> 16) & (U16) I16_MAX;
		U16 combinationId = compiles.ptr[i] & (U16) I16_MAX;
		Bool aggRt = (compiles.ptr[i] >> 15) & 1;
		Bool aggGfxComp = compiles.ptr[i] >> 31;

		const SHEntryRuntime *runtime = &parsed.shEntriesRuntime.ptr[entryId];

		CompilerSettings settings = (CompilerSettings) { 0 };
		SHBinaryIdentifier identifier = (SHBinaryIdentifier) { 0 };

		gotoIfError3(clean, Compiler_describeCompile(
			runtime, combinationId, EGfxBinaryType_SPIRV,
			false, false, false, aggRt, aggGfxComp,
			parseSettings.path, parseSettings.string, NULL,
			&settings, &identifier, e_rr
		));

		gotoIfError3(clean, Compiler_buildCompileArgs(&settings, &identifier, &args, &amended, alloc, e_rr));

		//-E carries the entry's own name and -T its stage's profile, which is what tells compiles apart

		entriesSpelled &= argsPairStr(&args, "-E", &runtime->entry.name);

		CharString dashT = CharString_createRefCStrConst("-T");

		CharString prefix = CharString_createRefCStrConst(
			runtime->entry.stage == EGfxPipelineStage_Compute ? "cs_" : "ps_"
		);

		Bool profiled = false;

		for (U64 j = 0; j + 1 < args.length; ++j)
			if (CharString_equalsStringSensitive(&args.ptr[j], &dashT))
				profiled = CharString_startsWithStringSensitive(&args.ptr[j + 1], &prefix, 0);

		profilesMatch &= profiled;

		ListCharString_freeUnderlying(&args, alloc);
		CharString_free(&amended, alloc);
	}

	Test_assert(t, "each compile spells its own entrypoint", entriesSpelled);
	Test_assert(t, "each profile follows its entry's stage", profilesMatch);

	//A mixed [shader] file: RT and non RT identifiers never combine (asBinaryIdentifier keeps their
	// stageType apart), so every shared compile is flag homogeneous and the packed aggregates equal the
	// stored entry's own flags. This pins that rule: if combining ever widens (the TODO in
	// Compiler_getUniqueCompiles), these asserts demand the flag semantics be re decided rather than
	// silently following one entry, because -Zi and the raytracing define hang off them.

	CompileResult_free(&parsed, alloc);
	ListU32_free(&compiles, alloc);
	parsed = (CompileResult) { 0 };
	compiles = (ListU32) { 0 };

	static const C8 *mixedLibSrc =
		"RWByteAddressBuffer buf;\n"
		"[shader(\"compute\")]\n"
		"[numthreads(1, 1, 1)]\n"
		"void mainCS(uint id : SV_DispatchThreadID) { buf.Store<uint>(id * 4, 1); }\n"
		"[shader(\"raygeneration\")]\n"
		"void mainRays() { buf.Store<uint>(0, 1); }\n";

	parseSettings.string = CharString_createRefCStrConst(mixedLibSrc);
	parseSettings.path = CharString_createRefCStrConst("args_mixed_lib.hlsl");

	gotoIfError3(clean, Compiler_parse(&comp, &parseSettings, alloc, &parsed, e_rr));

	Test_assert(t, "mixed lib parses two entrypoints", parsed.isSuccess && parsed.shEntriesRuntime.length == 2);

	gotoIfError3(clean, Compiler_getUniqueCompiles(&parsed.shEntriesRuntime, &compiles, alloc, e_rr));

	Test_assert(t, "RT and non RT libs stay separate compiles", compiles.length == 2);

	Bool flagsHomogeneous = true, libsSpellZi = true;

	for (U64 i = 0; i < compiles.length; ++i) {

		U16 entryId = (compiles.ptr[i] >> 16) & (U16) I16_MAX;
		U16 combinationId = compiles.ptr[i] & (U16) I16_MAX;
		Bool aggRt = (compiles.ptr[i] >> 15) & 1;
		Bool aggGfxComp = compiles.ptr[i] >> 31;

		const SHEntryRuntime *runtime = &parsed.shEntriesRuntime.ptr[entryId];

		flagsHomogeneous &=
			aggRt == SHEntryRuntime_isRt(*runtime) &&
			aggGfxComp == SHEntryRuntime_containsGfxOrComp(*runtime);

		CompilerSettings settings = (CompilerSettings) { 0 };
		SHBinaryIdentifier identifier = (SHBinaryIdentifier) { 0 };

		gotoIfError3(clean, Compiler_describeCompile(
			runtime, combinationId, EGfxBinaryType_DXIL,
			false, false, false, aggRt, aggGfxComp,
			parseSettings.path, parseSettings.string, NULL,
			&settings, &identifier, e_rr
		));

		gotoIfError3(clean, Compiler_buildCompileArgs(&settings, &identifier, &args, &amended, alloc, e_rr));

		//A gfx or compute lib links per entrypoint, so its compile carries -Zi for the metadata to
		// survive the link; an RT lib without uniforms stays a lib and doesn't.

		libsSpellZi &= argsContain(&args, "-Zi") == aggGfxComp;

		ListCharString_freeUnderlying(&args, alloc);
		CharString_free(&amended, alloc);
	}

	Test_assert(t, "aggregate flags match each compile's stored entry", flagsHomogeneous);
	Test_assert(t, "a linked lib compile carries -Zi, an RT lib doesn't", libsSpellZi);

clean:

	Test_assert(t, "build args produced no error", s_uccess);

	ListCharString_freeUnderlying(&args, alloc);
	ListCharString_freeUnderlying(&oddDefines, alloc);
	CharString_free(&amended, alloc);
	ListU32_free(&compiles, alloc);
	CompileResult_free(&parsed, alloc);

	if(created)
		Compiler_free(&comp, alloc);

	Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);
}
