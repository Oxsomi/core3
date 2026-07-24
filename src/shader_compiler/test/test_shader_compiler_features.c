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

//Each case compiles a shader that actually *uses* an extension's feature (from test/features/) and asserts
//the produced binary reflects that extension bit. Unlike the annotation module (which only checks an
//extension can be *declared*), this exercises the real DXC/SPIRV|DXIL code path behind each extension.
//These live in test/features (not the hlsl/ corpus) because they're semantic reflection tests, not
//byte-snapshots, and several would otherwise flake or fail the SPIRV-only corpus. Each carries its own
//backend target: SPIRV-native features, plus DXIL-native ones (MeshTaskTexDeriv, PAQ, WriteMSTexture).
//
//Coverage still parked as *.hlsl.disabled in test/features (kept as repros, not asserted here), with cause:
// - RayReorder / RayMicromapOpacity: SM6.9 (dx::HitObject SER / OMM ray flags) > the bundled DXC (~6.8).
//PAQ and WriteMSTexture used to be parked here too but now pass on DXIL after OxC3-side fixes:
// - PAQ needed the payload-access qualifiers declared on *both* entrypoints (raygen + closesthit share the
//   payload), plus Compiler_convertMemberDXIL now reflects the opaque RWStructuredBuffer<float4> $Element
//   (DXC leaves it D3D_SVT_VOID on DXIL) as a raw 32-bit block instead of erroring on "invalid primitive".
// - WriteMSTexture: DxilMapToESHExtension now folds ADVANCED_TEXTURE_OPS (which DXC co-reports with
//   WRITEABLE_MSAA_TEXTURES for an RWTexture2DMS write) into ESHExtension_WriteMSTexture.
//AtomicF32/F64 used to be parked here too but now pass: OxC3 was whitelisting -fspv-extension=
//SPV_EXT_shader_atomic_float_add, which DXC rejects as "unknown extension"; the inline [[vk::ext_extension]]
//already declares it, so that flag was removed (compiler.cpp) and DXC emits OpAtomicFAddEXT.

typedef struct FeatureCase {
	const C8 *file;
	ESHExtension ext;       //Expected reflected extension bit
	U8 target;              //ESHBinaryType
} FeatureCase;

void Test_shaderCompilerFeatures(Test *t) {

	Test_setModule(t, "Compiler features");

	const Allocator *alloc = Platform_instance->alloc;
	Error err = Error_none();

	static const FeatureCase features[] = {

		//SPIRV-native features
		{ "features/i64.hlsl",                 ESHExtension_I64,                ESHBinaryType_SPIRV },
		{ "features/f64.hlsl",                 ESHExtension_F64,                ESHBinaryType_SPIRV },
		{ "features/bit16.hlsl",               ESHExtension_16BitTypes,         ESHBinaryType_SPIRV },
		{ "features/atomic_i64.hlsl",          ESHExtension_AtomicI64,          ESHBinaryType_SPIRV },
		{ "features/subgroup_arithmetic.hlsl", ESHExtension_SubgroupArithmetic, ESHBinaryType_SPIRV },
		{ "features/subgroup_operations.hlsl", ESHExtension_SubgroupOperations, ESHBinaryType_SPIRV },
		{ "features/subgroup_shuffle.hlsl",    ESHExtension_SubgroupShuffle,    ESHBinaryType_SPIRV },
		{ "features/multiview.hlsl",           ESHExtension_Multiview,          ESHBinaryType_SPIRV },
		{ "features/compute_deriv.hlsl",       ESHExtension_ComputeDeriv,       ESHBinaryType_SPIRV },
		{ "features/atomic_f32.hlsl",          ESHExtension_AtomicF32,          ESHBinaryType_SPIRV },
		{ "features/atomic_f64.hlsl",          ESHExtension_AtomicF64,          ESHBinaryType_SPIRV },
		{ "features/ray_query.hlsl",           ESHExtension_RayQuery,           ESHBinaryType_SPIRV },

		//DXIL-native features (DXC's SPIR-V backend can't express them), driven here on DXIL by explicit path
		{ "features/mesh_task_tex_deriv.hlsl", ESHExtension_MeshTaskTexDeriv,   ESHBinaryType_DXIL },
		{ "features/paq.hlsl",                 ESHExtension_PAQ,                ESHBinaryType_DXIL },
		{ "features/write_ms_texture.hlsl",    ESHExtension_WriteMSTexture,     ESHBinaryType_DXIL }
	};

	for (U64 i = 0; i < sizeof(features) / sizeof(features[0]); ++i) {

		const C8 *file = features[i].file;
		ListBuffer out = (ListBuffer) { 0 };
		SHFile shFile = (SHFile) { 0 };

		Bool compiled = compileFileShader(alloc, file, features[i].target, true, &out, &err);
		Bool hasBinary = compiled && out.length == 1 && Buffer_length(out.ptr[0]);

		Bool reflected =
			hasBinary && readOiSH(alloc, out.ptr[0], &shFile, &err) &&
			shFile.binaries.length >= 1 &&
			(shFile.binaries.ptr[0].identifier.extensions & features[i].ext);

		if (!reflected)
			Error_print(alloc, &err, ELogLevel_Debug, ELogOptions_Default);

		Test_assert(t, file, reflected);

		SHFile_free(&shFile, alloc);
		ListBuffer_freeUnderlying(&out, alloc);
		err = Error_none();
	}

	//--- Disassembly: a compiled SPIRV binary round-trips to non-empty, plausible SPIRV text ---

	{
		ListBuffer out = (ListBuffer) { 0 };
		SHFile shFile = (SHFile) { 0 };
		Compiler comp = (Compiler) { 0 };
		CharString disasm = CharString_createNull();
		Bool created = false;

		Bool ok =
			compileFileShader(alloc, "features/i64.hlsl", ESHBinaryType_SPIRV, true, &out, &err) &&
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

	Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
}
