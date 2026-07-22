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

//shader_compiler/test/test_shader_compiler_features.c

#include "test_shader_compiler_shared.h"
#include "shader_compiler/compiler.h"
#include "formats/oiSH/sh_binaries.h"
#include "formats/oiSH/sh_file.h"
#include "platforms/platform.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/list_basic_types.h"
#include "types/container/log.h"
#include "types/base/string_read_helper.h"
#include "types/base/error.h"

//Unlike the annotation module (which only checks an extension can be *declared*), this module compiles
//shaders that actually *use* each feature and verifies (a) the feature compiles end-to-end through DXC,
//and (b) the resulting binary reflects the extension. So it exercises the real code paths behind each
//extension, not just the annotation parser.

typedef struct FeatureCase {
	const C8 *name;
	ESHExtension ext;
	const C8 *src;
} FeatureCase;

void Test_shaderCompilerFeatures(Test *t) {

	Test_setModule(t, "Compiler features");

	const Allocator *alloc = Platform_instance->alloc;
	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	static const FeatureCase features[] = {

		{ "I64 (uint64 arithmetic)", ESHExtension_I64,
			"RWStructuredBuffer<uint64_t> buf;\n"
			"[[oxc::extension(\"I64\")]]\n[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
			"void main(uint id : SV_DispatchThreadID) { buf[id] = buf[id] * 3ull + 1ull; }\n" },

		{ "F64 (double arithmetic)", ESHExtension_F64,
			"RWStructuredBuffer<double> buf;\n"
			"[[oxc::extension(\"F64\")]]\n[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
			"void main(uint id : SV_DispatchThreadID) { buf[id] = buf[id] * buf[id] + buf[id]; }\n" },

		{ "16BitTypes (float16 arithmetic)", ESHExtension_16BitTypes,
			"RWStructuredBuffer<float16_t> buf;\n"
			"[[oxc::extension(\"16BitTypes\")]]\n[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
			"void main(uint id : SV_DispatchThreadID) { buf[id] = buf[id] + (float16_t)1; }\n" },

		//AtomicI64 via RWStructuredBuffer<int64_t> (NOT RWByteAddressBuffer.InterlockedAdd64, which is
		//impossible in SPIRV logical addressing: DXC #5965, closed wontfix). SpvCapabilityInt64Atomics
		//reflects as ESHExtension_(I64 | AtomicI64), so both are declared. Needs SM6.6 for 64-bit atomics.
		{ "AtomicI64 (uint64 InterlockedAdd)", ESHExtension_AtomicI64,
			"RWStructuredBuffer<uint64_t> buf;\n"
			"[[oxc::extension(\"I64\", \"AtomicI64\")]]\n[[oxc::model(\"6.6\")]]\n[[oxc::stage(\"compute\")]]\n"
			"[numthreads(1,1,1)]\n"
			"void main(uint id : SV_DispatchThreadID) {\n"
			"	uint64_t prev;\n"
			"	InterlockedAdd(buf[0], (uint64_t)(id + 1), prev);\n"
			"	buf[id + 1] = prev;\n"
			"}\n" },

		{ "RayQuery (inline raytracing)", ESHExtension_RayQuery,
			"RaytracingAccelerationStructure tlas;\n"
			"RWStructuredBuffer<float> buf;\n"
			"[[oxc::extension(\"RayQuery\")]]\n[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
			"void main(uint id : SV_DispatchThreadID) {\n"
			"	RayDesc r; r.Origin = float3(0,0,0); r.Direction = float3(0,0,1); r.TMin = 0; r.TMax = 1e30f;\n"
			"	RayQuery<RAY_FLAG_NONE> q;\n"
			"	q.TraceRayInline(tlas, 0, 0xFF, r);\n"
			"	q.Proceed();\n"
			"	buf[id] = q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? q.CommittedRayT() : -1;\n"
			"}\n" },

		{ "SubgroupArithmetic (WaveActiveSum)", ESHExtension_SubgroupArithmetic,
			"RWStructuredBuffer<uint> buf;\n"
			"[[oxc::extension(\"SubgroupArithmetic\")]]\n[[oxc::stage(\"compute\")]]\n[numthreads(64,1,1)]\n"
			"void main(uint id : SV_DispatchThreadID) { buf[id] = WaveActiveSum(id + 1); }\n" }
	};

	for (U64 i = 0; i < sizeof(features) / sizeof(features[0]); ++i) {

		const C8 *srcs[1] = { features[i].src };
		ListBuffer out = (ListBuffer) { 0 };
		SHFile shFile = (SHFile) { 0 };

		Bool compiled = compileInlineShaders(alloc, srcs, 1, ESHBinaryType_SPIRV, 1, true, &out, &err);
		Bool hasBinary = compiled && out.length == 1 && Buffer_length(out.ptr[0]);

		Bool reflected =
			hasBinary && readOiSH(alloc, out.ptr[0], &shFile, &err) &&
			shFile.binaries.length >= 1 &&
			(shFile.binaries.ptr[0].identifier.extensions & features[i].ext);

		if (!reflected)
			Error_print(alloc, &err, ELogLevel_Debug, ELogOptions_Default);

		Test_assert(t, features[i].name, reflected);

		SHFile_free(&shFile, alloc);
		ListBuffer_freeUnderlying(&out, alloc);
		err = Error_none();
	}

	//--- Disassembly: a compiled SPIRV binary round-trips to non-empty, plausible SPIRV text ---

	{
		const C8 *srcs[1] = {
			"RWStructuredBuffer<uint> buf;\n"
			"[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n"
			"void main(uint id : SV_DispatchThreadID) { buf[id] = id; }\n"
		};

		ListBuffer out = (ListBuffer) { 0 };
		SHFile shFile = (SHFile) { 0 };
		Compiler comp = (Compiler) { 0 };
		CharString disasm = CharString_createNull();
		Bool created = false;

		Bool ok =
			compileInlineShaders(alloc, srcs, 1, ESHBinaryType_SPIRV, 1, true, &out, &err) &&
			out.length == 1 && Buffer_length(out.ptr[0]) &&
			readOiSH(alloc, out.ptr[0], &shFile, &err) && shFile.binaries.length >= 1;

		if (ok && Compiler_create(alloc, &comp, &err)) {

			created = true;
			Buffer spirv = shFile.binaries.ptr[0].binaries[ESHBinaryType_SPIRV];

			Bool disOk =
				Buffer_length(spirv) &&
				Compiler_disassemble(&comp, ESHBinaryType_SPIRV, spirv, alloc, &disasm, &err) &&
				CharString_length(disasm) > 0;

			//SPIRV disassembly (SPIRV-Tools) always contains an entrypoint op
			CharString needle = CharString_createRefCStrConst("OpEntryPoint");
			Bool looksLikeSpirv = disOk && CharString_containsStringSensitive(&disasm, &needle, 0, 0);

			Test_assert(t, "SPIRV disassembly is valid", looksLikeSpirv);
		}

		else Test_assert(t, "SPIRV disassembly is valid", false);

		CharString_free(&disasm, alloc);
		if (created)
			Compiler_free(&comp, alloc);
		SHFile_free(&shFile, alloc);
		ListBuffer_freeUnderlying(&out, alloc);
		err = Error_none();
	}

	(void) s_uccess; (void) e_rr;
	Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
}
