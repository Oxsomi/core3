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

//Each case compiles a shader that actually *uses* an extension's feature (from test/features/)
// and asserts the produced binary reflects that extension bit.
//Unlike the annotation module (which only checks an extension can be *declared*),
// this exercises the real DXC/SPIRV|DXIL code path behind each extension.
//These live in test/features (not the hlsl/ corpus) because they're semantic reflection tests, not byte-snapshots.
//Each carries its own backend target: SPIRV-native features, plus DXIL-native ones.
//
//A few backend-specific quirks worth knowing:
// - PAQ declares the payload-access qualifiers on *both* entrypoints (raygen + closesthit share the payload);
//   Compiler_convertMemberDXIL reflects the opaque RWStructuredBuffer<float4> $Element (D3D_SVT_VOID on DXIL)
//   as a raw 32-bit block.
// - WriteMSTexture: DxilMapToESHExtension folds ADVANCED_TEXTURE_OPS (which DXC co-reports with
//   WRITEABLE_MSAA_TEXTURES for an RWTexture2DMS write) into ESHExtension_WriteMSTexture.
// - AtomicF32/F64: the inline [[vk::ext_extension]] on the atomic function declares the extension, so OxC3
//   doesn't also pass -fspv-extension=SPV_EXT_shader_atomic_float_add (DXC rejects it as unknown); DXC emits
//   OpAtomicFAddEXT.

typedef struct FeatureCase {
	const C8 *file;
	ESHExtension ext;       //Expected reflected extension bit
	U8 backends;            //Mask of (1 << ESHBinaryType) this feature can be expressed on
} FeatureCase;

#define B_SPV  (1 << ESHBinaryType_SPIRV)
#define B_DXIL (1 << ESHBinaryType_DXIL)
#define B_BOTH (B_SPV | B_DXIL)

void Test_shaderCompilerFeatures(Test *t) {

	Test_setModule(t, "Compiler features");

	const Allocator *alloc = Platform_instance->alloc;
	Error err = Error_none();

	static const FeatureCase features[] = {

		//Expressible on BOTH backends (DXC emits the capability for SPIRV and DXIL alike)
		{ "features/i64.hlsl",                 ESHExtension_I64,                B_BOTH },
		{ "features/f64.hlsl",                 ESHExtension_F64,                B_BOTH },
		{ "features/bit16.hlsl",               ESHExtension_16BitTypes,         B_BOTH },
		{ "features/atomic_i64.hlsl",          ESHExtension_AtomicI64,          B_BOTH },
		{ "features/multiview.hlsl",           ESHExtension_Multiview,          B_BOTH },
		{ "features/subgroup_operations.hlsl", ESHExtension_SubgroupOperations, B_BOTH },
		{ "features/ray_query.hlsl",           ESHExtension_RayQuery,           B_BOTH },

		//Plain HLSL wave intrinsics, so both backends compile them.
		//DXIL reflection can't detect either (one generic wave ops flag), so on DXIL they are annotation-driven.
		{ "features/subgroup_arithmetic.hlsl", ESHExtension_SubgroupArithmetic, B_BOTH },
		{ "features/subgroup_shuffle.hlsl",    ESHExtension_SubgroupShuffle,    B_BOTH },

		//SPIRV-only: inline-SPIRV atomics (DXIL has no float atomic intrinsics)
		{ "features/atomic_f32.hlsl",          ESHExtension_AtomicF32,          B_SPV },
		{ "features/atomic_f64.hlsl",          ESHExtension_AtomicF64,          B_SPV },

		//ComputeDeriv (derivatives in a compute shader) compiles on both backends, even though it's only
		//natively detected on SPIRV (see SHEntryRuntime_getSupportedBinaryTypes).
		{ "features/compute_deriv.hlsl",       ESHExtension_ComputeDeriv,       B_BOTH },

		//SM6.6 dynamic resources (full bindless): native on DXIL, lowered to SPV_EXT_descriptor_heap on SPIRV.
		{ "features/descriptor_heap.hlsl",     ESHExtension_DescriptorHeap,     B_BOTH },

		//DXIL-only: DXC's SPIR-V backend genuinely fails to compile these (verified: SPIRV compile error)
		{ "features/mesh_task_tex_deriv.hlsl", ESHExtension_MeshTaskTexDeriv,   B_DXIL },
		{ "features/paq.hlsl",                 ESHExtension_PAQ,                B_DXIL },
		{ "features/write_ms_texture.hlsl",    ESHExtension_WriteMSTexture,     B_DXIL },

		//SM6.9 OMM: native intrinsic on DXIL; on SPIRV the ray query stays native HLSL and only the opacity-micromap
		//capability is added via inline SPIR-V, so it works on both behind #ifdef __spirv__.
		{ "features/ray_micromap_opacity.hlsl", ESHExtension_RayMicromapOpacity, B_BOTH },

		//SM6.9 SER: native dx::HitObject on DXIL; on SPIRV the hit object + reorder are inline SPIR-V
		//(SPV_EXT_shader_invocation_reorder, opaque OpTypeHitObjectEXT via vk::SpirvOpaqueType), so it works on both.
		{ "features/ray_reorder.hlsl",          ESHExtension_RayReorder,         B_BOTH },

		//SM6.1 fragment barycentrics:
		// native SV_Barycentrics semantic on both backends (SPIRV:
		// BaryCoordKHR through SPV_KHR_fragment_shader_barycentric).
		{ "features/barycentrics.hlsl",         ESHExtension_Barycentrics,       B_BOTH },

		//SM6.10 ray tri vertex position fetch: native intrinsic on DXIL; on SPIRV the ray query stays native HLSL
		// and only the position-fetch op is inline SPIR-V (a valid RayQuery<> can be handed to it by reference),
		// so it works on both.
		//The fully-inline opaque-type route (SER / linalg) instead fails OxC3's spirv-opt.
		{ "features/ray_tri_position.hlsl",     ESHExtension_RayTriPosition,     B_BOTH },

		//Ray-pipeline (closesthit) form of position fetch via the HitTriangleVertexPositionsKHR builtin / DXIL global.
		{ "features/ray_tri_position_pipeline.hlsl", ESHExtension_RayTriPosition, B_BOTH },

		//SM6.10 cooperative vectors (per-thread matvec): native __builtin_MatVecMul(Add) on DXIL; on SPIRV inline
		// SPV_NV_cooperative_vector (opaque OpTypeCooperativeVectorNV loaded / matmul'd / stored via ops 5302/5289/5292/5303).
		//NV-only on Vulkan (no cross-vendor KHR form).
		//Compiled at vulkan1.3 for the StorageBuffer class + Vulkan mem model.
		{ "features/coop_vec.hlsl",             ESHExtension_CoopVec,            B_BOTH },
		{ "features/coop_vec_bias.hlsl",        ESHExtension_CoopVec,            B_BOTH },

		//Quantized FP8-weight matvec (e4m3/e5m2 weights * F16 activations): the modern LLM weight-quantization path.
		//FP8 is gated behind the additive CoopFP8 extension (not universally supported); the test asserts that bit.
		{ "features/coop_vec_fp8.hlsl",         ESHExtension_CoopFP8,            B_BOTH },
		{ "features/coop_vec_fp8_e5m2.hlsl",    ESHExtension_CoopFP8,            B_BOTH },

		//Quantized INT8 matvec (int8 weights * int8 activations -> int32): the classic quantized-inference path.
		{ "features/coop_vec_int8.hlsl",        ESHExtension_CoopVec,            B_BOTH },

		//Training: outer-product (weight grad) + reduce-sum (bias grad) accumulate.
		//Gated behind CoopVecTraining (Tier 1.1).
		{ "features/coop_vec_train.hlsl",       ESHExtension_CoopVecTraining,    B_BOTH },

		//SM6.10 cooperative matrix (subgroup GEMM): native dx::linalg Wave Multiply on DXIL; on SPIRV the cross-vendor
		// SPV_KHR_cooperative_matrix via DXC's vk::khr::CooperativeMatrix (load / mul-add / store).
		//Works on both.
		{ "features/coop_mat.hlsl",             ESHExtension_CoopMat,            B_BOTH },

		//FP8 (e4m3) cooperative-matrix GEMM (F16 accumulate): DXIL Matrix<F8_E4M3FN,Wave>; SPIRV SPV_EXT_float8 +
		// Float8CooperativeMatrixEXT (an inline FP8 OpTypeFloat).
		//Gated behind CoopFP8.
		{ "features/coop_mat_fp8.hlsl",         ESHExtension_CoopFP8,            B_BOTH }
	};

	static const struct { U8 mode; const C8 *name; } backends[] = {
		{ ESHBinaryType_SPIRV, "spirv" },
		{ ESHBinaryType_DXIL,  "dxil"  }
	};

	//Each feature is compiled on every backend it supports.
	//The produced binary must reflect the expected extension AND round-trip through oiSH read/write (byte-identical) -
	// so a DXC/reflection change that breaks a backend or perturbs the serialized oiSH is caught here
	// rather than by manual inspection.

	for (U64 i = 0; i < sizeof(features) / sizeof(features[0]); ++i)
		for (U64 b = 0; b < sizeof(backends) / sizeof(backends[0]); ++b) {

			if (!(features[i].backends & (1 << backends[b].mode)))
				continue;

			ListBuffer out = (ListBuffer) { 0 };
			SHFile shFile = (SHFile) { 0 };
			CharString label = CharString_createNull();

			Bool compiled = compileFileShader(alloc, features[i].file, backends[b].mode, true, false, &out, &err);
			Bool hasBinary = compiled && out.length == 1 && Buffer_length(out.ptr[0]);

			Bool reflected =
				hasBinary && readOiSH(alloc, out.ptr[0], &shFile, &err) &&
				shFile.binaries.length >= 1 &&
				(shFile.binaries.ptr[0].identifier.extensions & features[i].ext) &&
				oiSHRoundtrips(alloc, out.ptr[0], &err);

			if (!reflected)
				Error_print(alloc, &err, ELogLevel_Debug, ELogOptions_Default);

			if (!CharString_format(alloc, &label, &err, "%s (%s)", features[i].file, backends[b].name))
				label = CharString_createRefCStrConst(features[i].file);

			Test_assert(t, label.ptr, reflected);

			CharString_free(&label, alloc);
			SHFile_free(&shFile, alloc);
			ListBuffer_freeUnderlying(&out, alloc);
			err = Error_none();
		}

	#undef B_SPV
	#undef B_DXIL
	#undef B_BOTH

	//--- Full bindless (DescriptorHeap): active vs dormant + register reflection, on both backends ---
	//The table above already asserts the used shader reflects the bit; this block additionally asserts
	// 1) using the heaps does NOT mark the extension dormant (native detection sees the usage),
	// 2) heap accesses create no named register entries (full bindless bypasses the binding table),
	// 3) declaring the extension without using it DOES mark it dormant (native detection on both backends).

	for (U64 b = 0; b < sizeof(backends) / sizeof(backends[0]); ++b) {

		ListBuffer out = (ListBuffer) { 0 };
		SHFile shFile = (SHFile) { 0 };
		CharString label = CharString_createNull();

		//Used: active extension; the only register is the normally bound output UAV (heap accesses create none)

		Bool usedOk =
			compileFileShader(alloc, "features/descriptor_heap.hlsl", backends[b].mode, true, false, &out, &err) &&
			out.length == 1 && Buffer_length(out.ptr[0]) &&
			readOiSH(alloc, out.ptr[0], &shFile, &err) && shFile.binaries.length >= 1 &&
			(shFile.binaries.ptr[0].identifier.extensions & ESHExtension_DescriptorHeap) &&
			!(shFile.binaries.ptr[0].dormantExtensions & ESHExtension_DescriptorHeap) &&
			shFile.binaries.ptr[0].registers.length == 1;

		if (!usedOk)
			Error_print(alloc, &err, ELogLevel_Debug, ELogOptions_Default);

		if (!CharString_format(alloc, &label, &err, "descriptor heap used stays active (%s)", backends[b].name))
			label = CharString_createRefCStrConst("descriptor heap used stays active");

		Test_assert(t, label.ptr, usedOk);

		CharString_free(&label, alloc);
		SHFile_free(&shFile, alloc);
		ListBuffer_freeUnderlying(&out, alloc);
		err = Error_none();

		//Declared but unused: demoted to dormant by native detection on DXIL (no requires flags emitted).
		//On SPIRV the heap mode declares the DescriptorHeapEXT capability even without a heap access,
		// so the binary genuinely requires the feature and the extension stays active there.

		Bool expectDormant = backends[b].mode == ESHBinaryType_DXIL;

		Bool unusedOk =
			compileFileShader(alloc, "features/descriptor_heap_unused.hlsl", backends[b].mode, true, false, &out, &err) &&
			out.length == 1 && Buffer_length(out.ptr[0]) &&
			readOiSH(alloc, out.ptr[0], &shFile, &err) && shFile.binaries.length >= 1 &&
			(shFile.binaries.ptr[0].identifier.extensions & ESHExtension_DescriptorHeap) &&
			!(shFile.binaries.ptr[0].dormantExtensions & ESHExtension_DescriptorHeap) == !expectDormant;

		if (!unusedOk)
			Error_print(alloc, &err, ELogLevel_Debug, ELogOptions_Default);

		if (!CharString_format(alloc, &label, &err, "descriptor heap unused goes dormant (%s)", backends[b].name))
			label = CharString_createRefCStrConst("descriptor heap unused goes dormant");

		Test_assert(t, label.ptr, unusedOk);

		CharString_free(&label, alloc);
		SHFile_free(&shFile, alloc);
		ListBuffer_freeUnderlying(&out, alloc);
		err = Error_none();
	}

	//--- keep-all (--keep-registers): unused resources stay bound and reflected, on both backends ---
	//keep_registers.hlsl declares three used and six unused resources across every register class.
	//Without keep-all the six unused ones must be stripped from reflection; with keep-all all nine must be
	// present by name (DXIL -fhlsl-unused-resource-bindings=keep-all, SPIRV keep-all + -fspv-preserve-bindings).

	for (U64 b = 0; b < sizeof(backends) / sizeof(backends[0]); ++b)
		for (U64 keep = 0; keep < 2; ++keep) {

			static const C8 *usedNames[]   = { "usedTex", "usedSamp", "usedOut" };
			static const C8 *unusedNames[] = { "unusedTex", "unusedTexArr", "unusedSB", "unusedSamp", "unusedUAV" };

			ListBuffer out = (ListBuffer) { 0 };
			SHFile shFile = (SHFile) { 0 };
			CharString label = CharString_createNull();

			Bool ok =
				compileFileShader(alloc, "features/keep_registers.hlsl", backends[b].mode, true, (Bool) keep, &out, &err) &&
				out.length == 1 && Buffer_length(out.ptr[0]) &&
				readOiSH(alloc, out.ptr[0], &shFile, &err) && shFile.binaries.length >= 1 &&
				oiSHRoundtrips(alloc, out.ptr[0], &err);

			if (ok) {

				const ListSHRegisterRuntime *regs = &shFile.binaries.ptr[0].registers;

				Bool hasName[sizeof(usedNames) / sizeof(usedNames[0]) + sizeof(unusedNames) / sizeof(unusedNames[0])] = { 0 };

				for (U64 i = 0; i < regs->length; ++i)
					for (U64 j = 0; j < sizeof(hasName) / sizeof(hasName[0]); ++j) {

						const C8 *name = j < 3 ? usedNames[j] : unusedNames[j - 3];

						if (CharString_equalsCStringSensitive(&regs->ptr[i].name, name))
							hasName[j] = true;
					}

				for (U64 j = 0; j < 3; ++j)                    //Used resources always reflect
					ok &= hasName[j];

				for (U64 j = 3; j < sizeof(hasName) / sizeof(hasName[0]); ++j)
					ok &= keep ? hasName[j] : !hasName[j];     //Unused only reflect under keep-all
			}

			if (!ok)
				Error_print(alloc, &err, ELogLevel_Debug, ELogOptions_Default);

			if (!CharString_format(
				alloc, &label, &err, "keep registers %s (%s)", keep ? "keeps unused" : "strips unused", backends[b].name
			))
				label = CharString_createRefCStrConst("keep registers");

			Test_assert(t, label.ptr, ok);

			CharString_free(&label, alloc);
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
			compileFileShader(alloc, "features/i64.hlsl", ESHBinaryType_SPIRV, true, false, &out, &err) &&
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
