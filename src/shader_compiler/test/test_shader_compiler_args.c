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
	ListCompilerLinkStep steps = (ListCompilerLinkStep) { 0 };
	ListBuffer outBufs = (ListBuffer) { 0 };
	SHFile shFile = (SHFile) { 0 };
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

		CompilerSettings entrySettings = (CompilerSettings) { 0 };
		SHBinaryIdentifier identifier = (SHBinaryIdentifier) { 0 };

		gotoIfError3(clean, Compiler_describeCompile(
			runtime, combinationId, EGfxBinaryType_SPIRV,
			false, false, false, ESpirvLayout_Dx, aggRt, aggGfxComp,
			parseSettings.path, parseSettings.string, NULL,
			&entrySettings, &identifier, e_rr
		));

		gotoIfError3(clean, Compiler_buildCompileArgs(&entrySettings, &identifier, &args, &amended, alloc, e_rr));

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

	//A mixed [shader] file: the RT and non RT groups deliberately never combine (see
	// Compiler_getUniqueCompiles), so every shared compile is flag homogeneous and the packed
	// aggregates equal the stored entry's own flags. These asserts pin that decision: anyone changing
	// it has to re decide the flag semantics rather than silently follow one entry, because -Zi and
	// the raytracing define hang off them.

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

	Bool flagsHomogeneous = true, libsSpellZi = true, mixedStepsPin = true;

	for (U64 i = 0; i < compiles.length; ++i) {

		U16 entryId = (compiles.ptr[i] >> 16) & (U16) I16_MAX;
		U16 combinationId = compiles.ptr[i] & (U16) I16_MAX;
		Bool aggRt = (compiles.ptr[i] >> 15) & 1;
		Bool aggGfxComp = compiles.ptr[i] >> 31;

		const SHEntryRuntime *runtime = &parsed.shEntriesRuntime.ptr[entryId];

		flagsHomogeneous &=
			aggRt == SHEntryRuntime_isRt(*runtime) &&
			aggGfxComp == SHEntryRuntime_containsGfxOrComp(*runtime);

		CompilerSettings libSettings = (CompilerSettings) { 0 };
		SHBinaryIdentifier identifier = (SHBinaryIdentifier) { 0 };

		gotoIfError3(clean, Compiler_describeCompile(
			runtime, combinationId, EGfxBinaryType_DXIL,
			false, false, false, ESpirvLayout_Dx, aggRt, aggGfxComp,
			parseSettings.path, parseSettings.string, NULL,
			&libSettings, &identifier, e_rr
		));

		gotoIfError3(clean, Compiler_buildCompileArgs(&libSettings, &identifier, &args, &amended, alloc, e_rr));

		//A gfx or compute lib links per entrypoint, so its compile carries -Zi for the metadata to
		// survive the link; an RT lib without uniforms stays a lib and doesn't.

		libsSpellZi &= argsContain(&args, "-Zi") == aggGfxComp;

		//The link steps come from the driver's own matching: the RT lib links ONCE as a lib (its
		// entries share the binary), the compute [shader] entry links once specialized to itself.

		gotoIfError3(clean, Compiler_getLinkSteps(
			&parsed.shEntriesRuntime, &identifier, entryId, EGfxBinaryType_DXIL, false, &steps, alloc, e_rr
		));

		if (aggRt)
			mixedStepsPin &= steps.length == 1 && !CharString_length(steps.ptr[0].identifier.entrypoint);

		else {
			CharString expectedName = CharString_createRefCStrConst("mainCS");
			mixedStepsPin &=
				steps.length == 1 &&
				CharString_equalsStringSensitive(&steps.ptr[0].identifier.entrypoint, &expectedName);
		}

		ListCompilerLinkStep_freeUnderlying(&steps, alloc);
		steps = (ListCompilerLinkStep) { 0 };

		ListCharString_freeUnderlying(&args, alloc);
		CharString_free(&amended, alloc);
	}

	Test_assert(t, "aggregate flags match each compile's stored entry", flagsHomogeneous);
	Test_assert(t, "a linked lib compile carries -Zi, an RT lib doesn't", libsSpellZi);
	Test_assert(t, "an RT lib links once as a lib, a compute lib once specialized", mixedStepsPin);

	//A uniforms entry compiles once and links per permutation. The DXIL link's "uniforms" library
	// source comes from Compiler_buildUniformExportsHLSL; this pins the expansion counts the link
	// enumeration rests on and the library's spelling for one permutation.

	CompileResult_free(&parsed, alloc);
	ListU32_free(&compiles, alloc);
	parsed = (CompileResult) { 0 };
	compiles = (ListU32) { 0 };

	//Uniforms require the [shader] annotation: they resolve by linking libraries (the parse refuses
	// them on [[oxc::stage]] entries), so this entry is a gfx/comp lib the way arrays.hlsl is.

	static const C8 *uniformSrc =
		"RWByteAddressBuffer buf;\n"
		"[[oxc::uniforms(U32 COUNT = 4)]]\n"
		"[[oxc::uniforms(U32 COUNT = 8)]]\n"
		"[shader(\"compute\")]\n"
		"[numthreads(1, 1, 1)]\n"
		"void mainU(uint id : SV_DispatchThreadID) { buf.Store<uint>(id * 4, 1); }\n";

	parseSettings.string = CharString_createRefCStrConst(uniformSrc);
	parseSettings.path = CharString_createRefCStrConst("args_uniforms.hlsl");

	gotoIfError3(clean, Compiler_parse(&comp, &parseSettings, alloc, &parsed, e_rr));

	Test_assert(t, "uniforms entry parses", parsed.isSuccess && parsed.shEntriesRuntime.length == 1);

	const SHEntryRuntime *uniformEntry = &parsed.shEntriesRuntime.ptr[0];

	Test_assert(
		t, "two permutations share one compile",
		SHEntryRuntime_getCombinations(uniformEntry) == 2 &&
		SHEntryRuntime_getCombinationsCompiled(uniformEntry) == 1
	);

	{
		CompilerSettings uniformSettings = (CompilerSettings) { 0 };
		SHBinaryIdentifier identifier = (SHBinaryIdentifier) { 0 };

		gotoIfError3(clean, Compiler_describeCompile(
			uniformEntry, 0, EGfxBinaryType_DXIL,
			false, false, false, ESpirvLayout_Dx, false, true,
			parseSettings.path, parseSettings.string, NULL,
			&uniformSettings, &identifier, e_rr
		));

		gotoIfError3(clean, Compiler_buildCompileArgs(&uniformSettings, &identifier, &args, &amended, alloc, e_rr));

		Test_assert(t, "a uniforms compile amends its source", amended.ptr != NULL);

		ListCharString_freeUnderlying(&args, alloc);
		CharString_free(&amended, alloc);
	}

	//Each permutation's library pins exactly, so the exports spelling and the value each combination
	// resolves to cannot drift. Combination ids count permutations from the FIRST annotation down, which
	// the two pins encode: 0 returns the first line's value, 1 the second's.

	static const struct ExportPin { U16 combinationId; const C8 *expected; } exportPins[] = {
		{ 0, "export uint $$specConst_COUNT() { return 4; }\n" },
		{ 1, "export uint $$specConst_COUNT() { return 8; }\n" }
	};

	{
		Bool exportsMatch = true;

		for (U64 i = 0; i < sizeof(exportPins) / sizeof(exportPins[0]); ++i) {

			SHBinaryIdentifier fullId = (SHBinaryIdentifier) { 0 };
			gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(uniformEntry, exportPins[i].combinationId, &fullId, e_rr));

			gotoIfError3(clean, Compiler_buildUniformExportsHLSL(
				&fullId.uniforms,
				Buffer_createRefConst(fullId.uniformData.ptr, fullId.uniformData.length),
				fullId.extensions, alloc, &amended, e_rr
			));

			CharString expected = CharString_createRefCStrConst(exportPins[i].expected);
			Bool matches = CharString_equalsStringSensitive(&amended, &expected);

			if (!matches)
				Log_debugLn(
					alloc, "exports library of combination %" PRIu64 " was: %.*s",
					i, (int) CharString_length(amended), amended.ptr
				);

			exportsMatch &= matches;
			CharString_free(&amended, alloc);
		}

		Test_assert(t, "each permutation's exports library pins exactly", exportsMatch);
	}

	//The link steps of the uniforms compile, from the driver's own matching: one step per permutation,
	// specialized to the entry, each carrying the library the exports pins spelled out.

	{
		SHBinaryIdentifier compiledId = (SHBinaryIdentifier) { 0 };
		gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(uniformEntry, 0, &compiledId, e_rr));

		gotoIfError3(clean, Compiler_getLinkSteps(
			&parsed.shEntriesRuntime, &compiledId, 0, EGfxBinaryType_DXIL, false, &steps, alloc, e_rr
		));

		CharString expectedProfile = CharString_createRefCStrConst("cs_6_5");
		CharString expectedName = CharString_createRefCStrConst("mainU");

		Bool stepsPin = steps.length == 2;

		for (U64 i = 0; i < steps.length && stepsPin; ++i) {

			const CompilerLinkStep *step = &steps.ptr[i];
			CharString expectedHlsl = CharString_createRefCStrConst(exportPins[i].expected);

			stepsPin &=
				step->combinationId == exportPins[i].combinationId &&
				CharString_equalsStringSensitive(&step->profile, &expectedProfile) &&
				CharString_equalsStringSensitive(&step->identifier.entrypoint, &expectedName) &&
				CharString_equalsStringSensitive(&step->uniformsHlsl, &expectedHlsl);
		}

		Test_assert(t, "the link steps pin both permutations", stepsPin);
	}

	//Convergence with the real driver: compile the same source and require the stored binaries to be
	// exactly the predicted steps' identifiers, which is what keeps this query honest.

	{
		const C8 *srcs[] = { uniformSrc };

		gotoIfError3(clean, compileInlineShaders(
			alloc, srcs, 1, (U8) EGfxBinaryType_DXIL, 1, "args_uniforms", true, &outBufs, e_rr
		));

		Bool converges =
			outBufs.length == 1 && Buffer_length(outBufs.ptr[0]) &&
			readOiSH(alloc, outBufs.ptr[0], &shFile, e_rr) &&
			shFile.binaries.length == steps.length;

		for (U64 i = 0; converges && i < steps.length; ++i) {

			Bool found = false;

			for (U64 j = 0; j < shFile.binaries.length && !found; ++j)
				found = SHBinaryIdentifier_equals(&steps.ptr[i].identifier, &shFile.binaries.ptr[j].identifier);

			converges &= found;
		}

		Test_assert(t, "the driver stores exactly the predicted binaries", converges);
	}

	//--- SPIRV layout choices: the argv follows the layout, and a DXIL target refuses anything but Dx
	//--- at the one choke point every compile passes through (Compiler_buildCompileArgs) ---

	{
		CompilerSettings laySettings = (CompilerSettings) { 0 };
		SHBinaryIdentifier layId = (SHBinaryIdentifier) { 0 };
		const SHEntryRuntime *layRuntime = &parsed.shEntriesRuntime.ptr[0];

		gotoIfError3(clean, Compiler_describeCompile(
			layRuntime, 0, EGfxBinaryType_SPIRV,
			false, false, false, ESpirvLayout_Scalar, false, true,
			parseSettings.path, parseSettings.string, NULL,
			&laySettings, &layId, e_rr
		));

		gotoIfError3(clean, Compiler_buildCompileArgs(&laySettings, &layId, &args, &amended, alloc, e_rr));

		Test_assert(
			t, "scalar layout swaps the layout flag",
			argsContain(&args, "-fvk-use-scalar-layout") && !argsContain(&args, "-fvk-use-dx-layout")
		);

		ListCharString_freeUnderlying(&args, alloc);
		CharString_free(&amended, alloc);

		gotoIfError3(clean, Compiler_describeCompile(
			layRuntime, 0, EGfxBinaryType_SPIRV,
			false, false, false, ESpirvLayout_Gl, false, true,
			parseSettings.path, parseSettings.string, NULL,
			&laySettings, &layId, e_rr
		));

		gotoIfError3(clean, Compiler_buildCompileArgs(&laySettings, &layId, &args, &amended, alloc, e_rr));

		Test_assert(
			t, "gl layout passes no layout flag",
			!argsContain(&args, "-fvk-use-scalar-layout") && !argsContain(&args, "-fvk-use-dx-layout")
		);

		ListCharString_freeUnderlying(&args, alloc);
		CharString_free(&amended, alloc);

		gotoIfError3(clean, Compiler_describeCompile(
			layRuntime, 0, EGfxBinaryType_DXIL,
			false, false, false, ESpirvLayout_Scalar, false, true,
			parseSettings.path, parseSettings.string, NULL,
			&laySettings, &layId, e_rr
		));

		//The refusal is the expected result, so e_rr stays NULL

		Test_assert(
			t, "a non Dx layout refuses any DXIL target",
			!Compiler_buildCompileArgs(&laySettings, &layId, &args, &amended, alloc, NULL)
		);
	}

	//--- A scalar layout SPIRV compile is a valid oiSH on its own whose buffers drop the shared layout
	//--- claim, and the DXIL twin of the same source refuses to merge with it: the two halves of what
	//--- oiSB's hasSharedLayout flag promises ---

	{
		static const C8 *laySrc =
			"cbuffer CBuf { float a; float4 c; float arr[3]; }\n"
			"RWByteAddressBuffer buf2;\n"
			"[[oxc::stage(\"compute\")]]\n"
			"[numthreads(1, 1, 1)]\n"
			"void mainL(uint id : SV_DispatchThreadID) { buf2.Store<float>(id * 4, a + c.x); }\n";

		const C8 *srcs2[] = { laySrc };

		SHFile spvFile = (SHFile) { 0 };
		SHFile dxilFile = (SHFile) { 0 };
		SHFile merged = (SHFile) { 0 };
		ListBuffer spvBufs = (ListBuffer) { 0 };
		ListBuffer dxilBufs = (ListBuffer) { 0 };

		Bool spvOk =
			compileInlineShadersLayout(
				alloc, srcs2, 1, (U8) EGfxBinaryType_SPIRV, 1, "args_layout_spv", true,
				ESpirvLayout_Scalar, &spvBufs, e_rr
			) &&
			spvBufs.length == 1 && Buffer_length(spvBufs.ptr[0]) &&
			readOiSH(alloc, spvBufs.ptr[0], &spvFile, e_rr) && spvFile.binaries.length == 1;

		Bool dxOk =
			compileInlineShaders(
				alloc, srcs2, 1, (U8) EGfxBinaryType_DXIL, 1, "args_layout_dxil", true, &dxilBufs, e_rr
			) &&
			dxilBufs.length == 1 && Buffer_length(dxilBufs.ptr[0]) &&
			readOiSH(alloc, dxilBufs.ptr[0], &dxilFile, e_rr) && dxilFile.binaries.length == 1;

		const SBFile *spvSB = NULL;
		const SBFile *dxSB = NULL;

		if (spvOk)
			for (U64 i = 0; i < spvFile.binaries.ptr[0].registers.length && !spvSB; ++i)
				if (spvFile.binaries.ptr[0].registers.ptr[i].shaderBuffer.bufferSize)
					spvSB = &spvFile.binaries.ptr[0].registers.ptr[i].shaderBuffer;

		if (dxOk)
			for (U64 i = 0; i < dxilFile.binaries.ptr[0].registers.length && !dxSB; ++i)
				if (dxilFile.binaries.ptr[0].registers.ptr[i].shaderBuffer.bufferSize)
					dxSB = &dxilFile.binaries.ptr[0].registers.ptr[i].shaderBuffer;

		Test_assert(t, "a scalar layout compile produces a valid oiSH", spvOk && spvSB);

		//The array right behind c pins that an unclaimed file sizes its bounds without DX's row padding,
		// which would put arr[3] past the end of the scalar cbuffer.

		Test_assert(
			t, "the scalar cbuffer packs float4 at 4 and its array at 20, without the shared claim",
			spvSB && spvSB->vars.length == 3 &&
			spvSB->vars.ptr[0].offset == 0 && spvSB->vars.ptr[1].offset == 4 && spvSB->vars.ptr[2].offset == 20 &&
			!(spvSB->flags & ESBSettingsFlags_HasSharedLayout)
		);

		Test_assert(
			t, "the DXIL twin keeps the shared claim at dx offsets",
			dxSB && dxSB->vars.length == 3 && dxSB->vars.ptr[1].offset == 16 && dxSB->vars.ptr[2].offset == 32 &&
			(dxSB->flags & ESBSettingsFlags_HasSharedLayout)
		);

		//The refusal is the expected result, so e_rr stays NULL

		Test_assert(
			t, "the DXIL twin refuses to merge with the scalar layout",
			spvOk && dxOk && !SHFile_combine(&spvFile, &dxilFile, alloc, &merged, NULL)
		);

		SHFile_free(&spvFile, alloc);
		SHFile_free(&dxilFile, alloc);
		SHFile_free(&merged, alloc);
		ListBuffer_freeUnderlying(&spvBufs, alloc);
		ListBuffer_freeUnderlying(&dxilBufs, alloc);
	}

	//--- A U64 right after a float in a structured buffer: the bytecode declares offset 8 and stride 24 on every path.
	//--- DXC's linker builds the linked module without the data layout, so reflecting it falls back to LLVM's default
	//--- i64 alignment of 4 and reports the U64 at 4 with stride 20.
	//--- Every [shader] entry is linked, so the shared layout claim refuses [shader] on DXIL while [[oxc::stage]] passes.
	//--- The known bug is pinned as is: the refusal assert turning red means DXC fixed it, and the case joins the
	//--- layout assert above ---

	{
		static const C8 *u64Stage =
			"struct Nicu { float a; uint64_t b; uint16_t c; float d; };\n"
			"RWStructuredBuffer<Nicu> sb;\n"
			"[[oxc::extension(\"16BitTypes\", \"I64\")]]\n"
			"[[oxc::stage(\"compute\")]]\n"
			"[numthreads(1, 1, 1)]\n"
			"void main(uint id : SV_DispatchThreadID) { sb[id].b = 7; }\n";

		static const C8 *u64Shader =
			"struct Nicu { float a; uint64_t b; uint16_t c; float d; };\n"
			"RWStructuredBuffer<Nicu> sb;\n"
			"[[oxc::extension(\"16BitTypes\", \"I64\")]]\n"
			"[shader(\"compute\")]\n"
			"[numthreads(1, 1, 1)]\n"
			"void main(uint id : SV_DispatchThreadID) { sb[id].b = 7; }\n";

		const C8 *const u64Srcs[] = { u64Stage, u64Shader };

		static const struct U64Case {
			U8 type;
			U8 srcId;               //Index into u64Srcs: 0 [[oxc::stage]], 1 [shader]
			const C8 *prefix;
		} u64Cases[] = {
			{ EGfxBinaryType_SPIRV, 0, "args_u64_stage_spv" },
			{ EGfxBinaryType_DXIL,  0, "args_u64_stage_dxil" },
			{ EGfxBinaryType_SPIRV, 1, "args_u64_shader_spv" }
		};
		Bool agrees = true;

		for (U64 i = 0; i < sizeof(u64Cases) / sizeof(u64Cases[0]); ++i) {

			ListBuffer bufs = (ListBuffer) { 0 };
			SHFile file = (SHFile) { 0 };
			const SBFile *sb = NULL;

			const Bool compiled =
				compileInlineShaders(
					alloc, &u64Srcs[u64Cases[i].srcId], 1, u64Cases[i].type, 1, u64Cases[i].prefix, true, &bufs, e_rr
				) &&
				bufs.length == 1 && Buffer_length(bufs.ptr[0]) &&
				readOiSH(alloc, bufs.ptr[0], &file, e_rr) && file.binaries.length >= 1;

			for (U64 j = 0; compiled && j < file.binaries.ptr[0].registers.length && !sb; ++j)
				if (file.binaries.ptr[0].registers.ptr[j].shaderBuffer.bufferSize)
					sb = &file.binaries.ptr[0].registers.ptr[j].shaderBuffer;

			const Bool matches =
				sb && sb->bufferSize == 24 && sb->vars.length == 5 && sb->vars.ptr[2].offset == 8 &&
				(sb->flags & ESBSettingsFlags_HasSharedLayout);

			if (!matches)
				Log_debugLn(alloc, "a U64 after a float reflected differently in %s", u64Cases[i].prefix);

			agrees &= matches;
			SHFile_free(&file, alloc);
			ListBuffer_freeUnderlying(&bufs, alloc);
		}

		Test_assert(t, "a U64 after a float lands at 8 with stride 24 wherever the reflection is sound", agrees);

		//The refusal is the expected result of the bug, so the compile stays quiet into a local error

		ListBuffer libBufs = (ListBuffer) { 0 };
		Error libErr = Error_none();

		const Bool libCompiled = compileInlineShaders(
			alloc, &u64Srcs[1], 1, (U8) EGfxBinaryType_DXIL, 1, "args_u64_shader_dxil", false, &libBufs, &libErr
		);

		Test_assert(
			t, "known DXC bug: a linked DXIL module reflects the U64 at 4, so the shared layout claim refuses [shader]",
			!libCompiled
		);

		ListBuffer_freeUnderlying(&libBufs, alloc);
	}

clean:

	Test_assert(t, "build args produced no error", s_uccess);

	ListCharString_freeUnderlying(&args, alloc);
	ListCharString_freeUnderlying(&oddDefines, alloc);
	CharString_free(&amended, alloc);
	ListU32_free(&compiles, alloc);
	CompileResult_free(&parsed, alloc);
	ListCompilerLinkStep_freeUnderlying(&steps, alloc);
	ListBuffer_freeUnderlying(&outBufs, alloc);
	SHFile_free(&shFile, alloc);

	if(created)
		Compiler_free(&comp, alloc);

	Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_Default);
}
