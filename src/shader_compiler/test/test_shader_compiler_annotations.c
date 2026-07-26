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

//shader_compiler/test/test_shader_compiler_annotations.c

#include "test_shader_compiler_shared.h"
#include "shader_compiler/compiler.h"
#include "formats/oiSH/sh_entries.h"
#include "formats/oiSH/sh_binaries.h"
#include "formats/oiSH/sh_file.h"
#include "platforms/platform.h"
#include "types/container/string.h"
#include "types/container/log.h"
#include "types/base/error.h"
#include "types/base/string_base.h"
#include "types/base/string_read_helper.h"

//Parse `src` as HLSL (this runs real DXC reflection + our oxc:: annotation parser) and hand back the
//reflection. The error is swallowed on purpose so the caller can assert on the boolean result and keep
//iterating over many small shaders without aborting the whole module on the first expected failure.
static Bool parseShader(
	const Compiler *comp, CharString src, const Allocator *alloc, CompileResult *result, Bool failIsOk
) {

	Error err = Error_none();

	CompilerSettings settings = (CompilerSettings) {
		.string = src,
		.path = CharString_createRefCStrConst("test_annot.hlsl"),
		.format = ECompilerFormat_HLSL,
		.outputType = ESHBinaryType_SPIRV
	};

	Bool ok = Compiler_parse(comp, &settings, alloc, result, &err);
	Bool clean = ok && !err.genericError && result->isSuccess;

	if(!failIsOk)
		Error_print(alloc, &err, ELogLevel_Debug, ELogOptions_Default);

	return clean;
}

void Test_shaderCompilerAnnotations(Test *t) {

	Test_setModule(t, "Compiler annotations");

	const Allocator *alloc = Platform_instance->alloc;
	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	Compiler comp = (Compiler) { 0 };
	Bool created = false;

	CharString src = CharString_createNull();
	CompileResult r = (CompileResult) { 0 };

	gotoIfError3(clean, Compiler_create(alloc, &comp, e_rr));
	created = true;

	//--- Every ESHExtension name is accepted by [[oxc::extension(...)]] and mapped to its bit ---

	for (U64 i = 0; i < ESHExtension_Count; ++i) {

		//Skip bits a compute annotation can't set (tested elsewhere / not annotation-settable)
		switch (1 << i) {

			case ESHExtension_Reserved:             //Placeholder bit
			case ESHExtension_RayMotionBlur:        //CPU-side only
			case ESHExtension_Bindless:             //Derived from the binary, not annotation-settable
			case ESHExtension_UnboundArraySize:
			case ESHExtension_RayReorder:           //Requires a raygen stage
			case ESHExtension_PAQ:                  //Requires an RT stage
				continue;
		}

		CharString_free(&src, alloc);
		gotoIfError3(clean, CharString_format(
			alloc, &src, e_rr,
			"RWByteAddressBuffer buf;\n"
			"[[oxc::extension(\"%s\")]]\n"
			"[[oxc::stage(\"compute\")]]\n"
			"[numthreads(1, 1, 1)]\n"
			"void main(uint id : SV_DispatchThreadID) { buf.Store<uint>(id * 4, 1); }\n",
			ESHExtension_names[i]
		));

		CompileResult_free(&r, alloc);
		Bool ok = parseShader(&comp, src, alloc, &r, false);

		Bool mapped =
			ok && r.shEntriesRuntime.length == 1 &&
			r.shEntriesRuntime.ptr[0].extensions.length >= 1 &&
			(r.shEntriesRuntime.ptr[0].extensions.ptr[0] & (1u << i));

		Test_assert(t, ESHExtension_names[i], mapped);
	}

	CharString_free(&src, alloc);

	//--- Stage-constrained extensions on their required stage: RayReorder needs raygen, PAQ needs an
	//--- RT stage (raygen qualifies as one). RT entrypoints use the [shader(...)] intrinsic. ---

	src = CharString_createRefCStrConst(
		"[[oxc::extension(\"RayReorder\")]]\n"
		"[shader(\"raygeneration\")]\n"
		"void main() {}\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(
		t, "RayReorder on raygen",
		parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1 &&
		r.shEntriesRuntime.ptr[0].extensions.length >= 1 &&
		(r.shEntriesRuntime.ptr[0].extensions.ptr[0] & ESHExtension_RayReorder)
	);

	src = CharString_createRefCStrConst(
		"[[oxc::extension(\"PAQ\")]]\n"
		"[shader(\"raygeneration\")]\n"
		"void main() {}\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(
		t, "PAQ on raygen",
		parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1 &&
		r.shEntriesRuntime.ptr[0].extensions.length >= 1 &&
		(r.shEntriesRuntime.ptr[0].extensions.ptr[0] & ESHExtension_PAQ)
	);

	//--- Unknown extension names are rejected ---

	CharString_free(&src, alloc);
	src = CharString_createRefCStrConst(
		"[[oxc::extension(\"ThisIsNotARealExtension\")]]\n"
		"[[oxc::stage(\"compute\")]]\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(t, "unknown extension rejected", !parseShader(&comp, src, alloc, &r, true));

	//--- [[oxc::model(...)]] records the requested shader model ---

	src = CharString_createRefCStrConst(
		"[[oxc::model(\"6.7\")]]\n"
		"[[oxc::stage(\"compute\")]]\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(
		t, "model 6.7 recorded",
		parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1 &&
		r.shEntriesRuntime.ptr[0].shaderVersions.length >= 1 &&
		r.shEntriesRuntime.ptr[0].shaderVersions.ptr[0] == OISH_SHADER_MODEL(6, 7)
	);

	//--- [[oxc::vendor(...)]] records a vendor mask (non-zero for a real vendor) ---

	src = CharString_createRefCStrConst(
		"[[oxc::vendor(\"NV\", \"AMD\")]]\n"
		"[[oxc::stage(\"compute\")]]\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(
		t, "vendor mask recorded",
		parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1 &&
		r.shEntriesRuntime.ptr[0].vendorMask != 0
	);

	//--- [[oxc::binary(...)]] records a per-entrypoint backend mask (bit = 1 << ESHBinaryType) ---

	src = CharString_createRefCStrConst(
		"[[oxc::binary(\"dxil\")]]\n"
		"[[oxc::stage(\"compute\")]]\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(
		t, "binary(dxil) records DXIL-only mask",
		parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1 &&
		r.shEntriesRuntime.ptr[0].binaryTypes == (1 << ESHBinaryType_DXIL)
	);

	//"spv" and "spirv" are both accepted; listing both backends sets both bits

	src = CharString_createRefCStrConst(
		"[[oxc::binary(\"spv\", \"dxil\")]]\n"
		"[[oxc::stage(\"compute\")]]\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(
		t, "binary(spv, dxil) records both bits",
		parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1 &&
		r.shEntriesRuntime.ptr[0].binaryTypes == ((1 << ESHBinaryType_SPIRV) | (1 << ESHBinaryType_DXIL))
	);

	//Absence of the annotation leaves the mask unset (0), which the driver treats as "all supported"

	src = CharString_createRefCStrConst(
		"[[oxc::stage(\"compute\")]]\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(
		t, "binary absent leaves mask unset",
		parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1 &&
		r.shEntriesRuntime.ptr[0].binaryTypes == 0
	);

	//A backend name that isn't a currently-supported binary type is rejected. "air" (Apple IR) is the
	//reserved name for the planned Apple backend; until ESHBinaryType_AIR exists it's rejected here, so this
	//assertion flips (and prompts wiring the mapping) the day AIR support lands.

	src = CharString_createRefCStrConst(
		"[[oxc::binary(\"air\")]]\n"
		"[[oxc::stage(\"compute\")]]\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(t, "binary(air) rejected until AIR is supported", !parseShader(&comp, src, alloc, &r, true));

	//--- Backend auto-restrict: SHEntryRuntime_getSupportedBinaryTypes AND's the stage + extension support,
	//--- independent of the annotation. A backend-exclusive extension (AtomicF32, inline-SPIRV atomics)
	//--- restricts to SPIRV; a plain compute entrypoint supports both. This is the mechanism behind
	//--- "workgraph is DXIL-only", etc. ---

	src = CharString_createRefCStrConst(
		"[[oxc::extension(\"AtomicF32\")]]\n"
		"[[oxc::stage(\"compute\")]]\n"
		"[numthreads(2, 2, 1)]\n"
		"void main(uint3 id : SV_DispatchThreadID) {}\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(
		t, "AtomicF32 entrypoint auto-restricts to SPIRV",
		parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1 &&
		SHEntryRuntime_getSupportedBinaryTypes(&r.shEntriesRuntime.ptr[0]) == (1 << ESHBinaryType_SPIRV)
	);

	//ComputeDeriv is only natively *detected* on SPIRV (ComputeDerivativeGroupQuads) but DXC compiles compute
	//derivatives on DXIL too (SM6.6), so it must NOT auto-restrict - it stays dual-backend.
	src = CharString_createRefCStrConst(
		"[[oxc::extension(\"ComputeDeriv\")]]\n"
		"[[oxc::model(\"6.6\")]]\n"
		"[[oxc::stage(\"compute\")]]\n"
		"[numthreads(2, 2, 1)]\n"
		"void main(uint3 id : SV_DispatchThreadID) {}\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(
		t, "ComputeDeriv entrypoint stays dual-backend (compiles on DXIL too)",
		parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1 &&
		SHEntryRuntime_getSupportedBinaryTypes(&r.shEntriesRuntime.ptr[0]) ==
			((1 << ESHBinaryType_SPIRV) | (1 << ESHBinaryType_DXIL))
	);

	src = CharString_createRefCStrConst(
		"[[oxc::stage(\"compute\")]]\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(
		t, "plain compute entrypoint supports both backends",
		parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1 &&
		SHEntryRuntime_getSupportedBinaryTypes(&r.shEntriesRuntime.ptr[0]) ==
			((1 << ESHBinaryType_SPIRV) | (1 << ESHBinaryType_DXIL))
	);

	//--- [[oxc::defines(...)]] records the define name/value pair (not just "parsed ok") ---

	src = CharString_createRefCStrConst(
		"[[oxc::defines(\"THREAD_COUNT\" = \"16\")]]\n"
		"[[oxc::stage(\"compute\")]]\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n"
	);
	CompileResult_free(&r, alloc);
	{
		Bool ok = parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1;
		const SHEntryRuntime *e = ok ? &r.shEntriesRuntime.ptr[0] : NULL;

		//defineNameValues is a flat [name, value][] list; one define => two entries with the exact strings
		const CharString wantName = CharString_createRefCStrConst("THREAD_COUNT");
		const CharString wantVal  = CharString_createRefCStrConst("16");

		Test_assert(
			t, "defines records name and value",
			e && e->defineNameValues.length == 2 &&
			CharString_equalsStringSensitive(&e->defineNameValues.ptr[0], &wantName) &&
			CharString_equalsStringSensitive(&e->defineNameValues.ptr[1], &wantVal)
		);
	}

	//--- Permutation counts: two [[oxc::defines]] annotations produce two compile combinations; two
	//--- [[oxc::model]] annotations record two shader versions (each multiplies the combination space). ---

	src = CharString_createRefCStrConst(
		"[[oxc::defines(\"MODE\" = \"0\")]]\n"
		"[[oxc::defines(\"MODE\" = \"1\")]]\n"
		"[[oxc::stage(\"compute\")]]\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(
		t, "two define-sets -> two compile combinations",
		parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1 &&
		r.shEntriesRuntime.ptr[0].definesPerCompilation.length == 2 &&
		SHEntryRuntime_getCombinationsCompiled(&r.shEntriesRuntime.ptr[0]) == 2
	);

	src = CharString_createRefCStrConst(
		"[[oxc::model(\"6.6\")]]\n"
		"[[oxc::model(\"6.8\")]]\n"
		"[[oxc::stage(\"compute\")]]\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(
		t, "two models -> two shader versions",
		parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1 &&
		r.shEntriesRuntime.ptr[0].shaderVersions.length == 2 &&
		SHEntryRuntime_getCombinationsCompiled(&r.shEntriesRuntime.ptr[0]) == 2
	);

	//--- [[oxc::uniforms(...)]] records uniform permutations. Uniforms are illegal together with
	//--- [[oxc::stage(...)]] (they compile as a lib), so the entrypoint uses the [shader(...)] intrinsic.

	src = CharString_createRefCStrConst(
		"[[oxc::uniforms(B1 ROTATE = true)]]\n"
		"[shader(\"compute\")]\n"
		"[numthreads(1, 1, 1)]\n"
		"void main() {}\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(
		t, "uniform recorded",
		parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1 &&
		r.shEntriesRuntime.ptr[0].uniforms.length >= 1
	);

	//--- Graphics stages parse with a valid signature ---

	src = CharString_createRefCStrConst(
		"[[oxc::stage(\"vertex\")]]\n"
		"float4 main() : SV_Position { return 0; }\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(
		t, "vertex stage parses",
		parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1
	);

	src = CharString_createRefCStrConst(
		"[[oxc::stage(\"pixel\")]]\n"
		"float4 main() : SV_Target { return 1; }\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(
		t, "pixel stage parses",
		parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1
	);

	//--- Negative validation: each of these is valid HLSL but an illegal OxC3 annotation combination, so
	//--- Compiler_parse must reject it cleanly (return failure, not crash). failIsOk=true keeps them quiet. ---

	{
		static const struct { const C8 *label; const C8 *src; } negatives[] = {

			{ "RayReorder on a non-raygen stage rejected",
				"[[oxc::extension(\"RayReorder\")]]\n[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\nvoid main() {}\n" },

			{ "PAQ on a non-raytracing stage rejected",
				"[[oxc::extension(\"PAQ\")]]\n[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\nvoid main() {}\n" },

			{ "ComputeDeriv on a pixel stage rejected",
				"[[oxc::extension(\"ComputeDeriv\")]]\n[[oxc::model(\"6.6\")]]\n[[oxc::stage(\"pixel\")]]\n"
				"float4 main() : SV_Target { return 0; }\n" },

			{ "shader model below the 6.5 floor rejected",
				"[[oxc::model(\"6.0\")]]\n[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\nvoid main() {}\n" },

			{ "extension needing a higher model than declared rejected",   //AtomicI64 needs 6.6, only 6.5 given
				"[[oxc::extension(\"AtomicI64\")]]\n[[oxc::model(\"6.5\")]]\n[[oxc::stage(\"compute\")]]\n"
				"[numthreads(1,1,1)]\nvoid main() {}\n" },

			{ "oxc::uniforms together with oxc::stage rejected",
				"[[oxc::uniforms(B1 X = true)]]\n[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\nvoid main() {}\n" },

			{ "conflicting oxc::stage + [shader] on one function rejected",
				"[[oxc::stage(\"compute\")]]\n[shader(\"compute\")]\n[numthreads(1,1,1)]\nvoid main() {}\n" },

			{ "unknown stage name rejected",
				"[[oxc::stage(\"foobar\")]]\n[numthreads(1,1,1)]\nvoid main() {}\n" },

			{ "empty oxc::binary() rejected",
				"[[oxc::binary()]]\n[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\nvoid main() {}\n" },

			{ "duplicate binary type in one annotation rejected",
				"[[oxc::binary(\"spv\", \"spv\")]]\n[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\nvoid main() {}\n" }
		};

		for (U64 i = 0; i < sizeof(negatives) / sizeof(negatives[0]); ++i) {
			src = CharString_createRefCStrConst(negatives[i].src);
			CompileResult_free(&r, alloc);
			Test_assert(t, negatives[i].label, !parseShader(&comp, src, alloc, &r, true));
		}
	}

	//--- binary(spv) mirrors binary(dxil): the effective set collapses to SPIRV only ---

	src = CharString_createRefCStrConst(
		"[[oxc::binary(\"spv\")]]\n[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\nvoid main() {}\n"
	);
	CompileResult_free(&r, alloc);
	Test_assert(
		t, "binary(spv) restricts effective set to SPIRV",
		parseShader(&comp, src, alloc, &r, false) && r.shEntriesRuntime.length == 1 &&
		r.shEntriesRuntime.ptr[0].binaryTypes == (1 << ESHBinaryType_SPIRV) &&
		SHEntryRuntime_getBinaryTypes(&r.shEntriesRuntime.ptr[0]) == (1 << ESHBinaryType_SPIRV)
	);

clean:

	Test_assert(t, "annotations module produced no error", s_uccess);

	CompileResult_free(&r, alloc);
	CharString_free(&src, alloc);

	if (created)
		Compiler_free(&comp, alloc);

	Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
}
