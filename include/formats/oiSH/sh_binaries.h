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

//formats/oiSH/sh_binaries.h

#pragma once
#include "types/container/list.h"
#include "formats/oiSH/sh_registers.h"

#ifdef __cplusplus
	extern "C" {
#endif

extern const C8 *ESHBinaryType_names[ESHBinaryType_Count];

typedef enum ESHExtension {

	ESHExtension_None                        = 0,

	//These losely map to EGraphicsDataTypes in OxC3 graphics

	ESHExtension_F64                         = 1 << 0,
	ESHExtension_I64                         = 1 << 1,
	ESHExtension_16BitTypes                  = 1 << 2,        //I16, F16

	ESHExtension_AtomicI64                   = 1 << 3,
	ESHExtension_AtomicF32                   = 1 << 4,
	ESHExtension_AtomicF64                   = 1 << 5,

	//Some of them are present in EGraphicsFeatures in OxC3 graphics

	ESHExtension_SubgroupArithmetic          = 1 << 6,
	ESHExtension_SubgroupShuffle             = 1 << 7,

	ESHExtension_RayQuery                    = 1 << 8,
	ESHExtension_RayMicromapOpacity          = 1 << 9,
	ESHExtension_RayTriPosition              = 1 << 10,       //SM6.10 tri vertex position fetch (RayQuery + ray-pipeline)
	//SV_Barycentrics / GetAttributeAtVertex (SM6.1+); SPIR-V BaryCoordKHR via SPV_KHR_fragment_shader_barycentric.
	//Hardware-gated:
	// RDNA1 lacks the Vulkan extension,
	// so a fragment shader that reads barycentrics ships as a variant beside a fallback.
	//Reuses the reserved bit 11 slot; old oiSH files never set it, so dormant masks stay valid.
	ESHExtension_Barycentrics                = 1 << 11,
	ESHExtension_RayReorder                  = 1 << 12,

	ESHExtension_Multiview                   = 1 << 13,
	ESHExtension_ComputeDeriv                = 1 << 14,

	ESHExtension_PAQ                         = 1 << 15,        //Payload access qualifiers

	ESHExtension_MeshTaskTexDeriv            = 1 << 16,

	ESHExtension_WriteMSTexture              = 1 << 17,

	ESHExtension_Bindless                    = 1 << 18,
	ESHExtension_UnboundArraySize            = 1 << 19,

	ESHExtension_SubgroupOperations          = 1 << 20,

	//SM6.10 cooperative vectors (per-thread matrix*vector): DXIL dx::linalg, SPIR-V SPV_NV_cooperative_vector (NV-only).
	ESHExtension_CoopVec                     = 1 << 21,

	//SM6.10 cooperative matrix (subgroup matrix*matrix / GEMM): DXIL dx::linalg,
	// SPIR-V SPV_KHR_cooperative_matrix (cross-vendor).
	ESHExtension_CoopMat                     = 1 << 22,

	//FP8 (e4m3/e5m2) cooperative vector/matrix support: an additive gate on top of CoopVec/CoopMat,
	// whose base tier is FP16 + INT8.
	//FP8 is not universally supported (newer HW on Vulkan; a vendor/EXT extension for KHR cooperative matrix),
	// so it is a separate flag the runtime matches against device cooperative properties.
	//Annotation-driven: NV cooperative-vector FP8 is only an interpretation, not a SPIR-V capability,
	// so it isn't reflection-detectable (hence not in the SpirvNative/DxilNative sets, or it would be demoted).
	ESHExtension_CoopFP8                     = 1 << 23,

	//Cooperative-vector training (D3D12 Tier 1.1 / SPV_NV_cooperative_vector CooperativeVectorTrainingNV):
	// the outer-product / reduce-sum accumulate ops (weight/bias gradients).
	//Additive gate on top of CoopVec, reflection-detectable (a real capability)
	// but kept out of the Native sets so adding it doesn't churn every oiSH's dormant mask.
	ESHExtension_CoopVecTraining             = 1 << 24,

	//Full bindless: direct descriptor heap indexing from shaders (requires the DescriptorHeap device feature).
	//DXIL: SM6.6 dynamic resources (ResourceDescriptorHeap/SamplerDescriptorHeap), detected via the
	// RESOURCE/SAMPLER_DESCRIPTOR_HEAP_INDEXING requires flags.
	//SPIRV: SPV_EXT_descriptor_heap (DescriptorHeapEXT capability).
	ESHExtension_DescriptorHeap              = 1 << 25,

	//An ARRAY of samplers, which is only servable by the bindless _samplers[] array and so needs
	// EGraphicsDeviceFlags_EnableDynamicSamplers on the device.
	//A singular sampler is a plain binding or a static sampler and requires none of this.
	//Reflection derived like Bindless, never annotation settable.

	ESHExtension_DynamicSamplers             = 1 << 26,

	//Barycentrics is native on BOTH backends:
	// D3D_SHADER_REQUIRES_BARYCENTRICS and BaryCoordKHR both map to it,
	// so declared-but-unused demotes to dormant like any other native extension.
	//Adding a native bit churns every oiSH's dormant mask,
	// so the compiler corpus goldens were REGENERATED with it (dormant gains bit 11 everywhere).
	//Old third-party files stay safe:
	// no oiSH written before this bit existed can contain the requires-flag or the capability - the old compiler
	// rejected both, so no old identifier can diverge from a fresh compile.

	ESHExtension_DxilNative =                                  //Extensions that can be found from DXIL natively
		ESHExtension_Barycentrics |
		ESHExtension_RayQuery |
		ESHExtension_16BitTypes |
		ESHExtension_I64 |
		ESHExtension_Multiview |
		ESHExtension_F64 |
		ESHExtension_AtomicI64 |
		ESHExtension_MeshTaskTexDeriv |
		ESHExtension_WriteMSTexture |
		ESHExtension_SubgroupOperations |
		ESHExtension_DescriptorHeap,

	ESHExtension_SpirvNative =                                 //Extensions that map directly to SPIRV capabilities
		ESHExtension_Barycentrics |
		ESHExtension_RayMicromapOpacity |
		ESHExtension_RayQuery |
		ESHExtension_RayTriPosition |
		ESHExtension_RayReorder |
		ESHExtension_AtomicF32 |
		ESHExtension_AtomicF64 |
		ESHExtension_SubgroupArithmetic |
		ESHExtension_SubgroupShuffle |
		ESHExtension_SubgroupOperations |
		ESHExtension_Multiview |
		ESHExtension_16BitTypes |
		ESHExtension_F64 |
		ESHExtension_I64 |
		ESHExtension_AtomicI64 |
		ESHExtension_ComputeDeriv |
		ESHExtension_WriteMSTexture |
		ESHExtension_CoopVec |
		ESHExtension_CoopMat |
		ESHExtension_DescriptorHeap,

	//Which backend DXC can *compile* an extension to, as opposed to the DxilNative / SpirvNative sets above which
	// say which backend it can be *detected* (reflected) from.
	//DXC lowers an HLSL intrinsic to a single backend,
	// but the other backend is still reachable when the shader writes the operation as inline SPIR-V ([[vk::ext_*]]).
	//Inline SPIR-V is itself SPIR-V, so it's an escape hatch onto SPIRV only - never onto DXIL.
	//An extension is therefore listed as single-backend below ONLY when the other backend has no path at all.
	//A DXIL-only *intrinsic* (OMM / SER / triangle position fetch / cooperative vectors) is NOT single-backend
	// because a SPIRV build can still express it inline, so it stays dual (absent from both sets).
	//When adding an extension above: leave it out of both sets to keep it dual (the default), and only add it here
	// if one backend is genuinely impossible.

	//SubgroupArithmetic and SubgroupShuffle deliberately are NOT here: WaveActiveSum / WaveReadLaneAt are
	// plain HLSL, so DXC compiles them to DXIL fine.
	//DXIL reflection just can't DETECT them (one generic wave ops flag), which is a DxilNative matter and
	// keeps them annotation-driven on DXIL, not a reason to refuse the compile.

	ESHExtension_NoDxilCompile =                              //SPIRV-only to compile: no DXIL intrinsic exists
		ESHExtension_AtomicF32 |
		ESHExtension_AtomicF64,

	ESHExtension_NoSpirvCompile =                             //DXIL-only to compile: no SPIR-V intrinsic or inline op
		ESHExtension_MeshTaskTexDeriv,

	ESHExtension_Count                       = 27,

	ESHExtension_All                         = (1 << ESHExtension_Count) - 1

} ESHExtension;

extern const C8 *ESHExtension_defines[ESHExtension_Count];
extern const C8 *ESHExtension_names[ESHExtension_Count];

typedef enum ESHVendor {
	ESHVendor_NV,
	ESHVendor_AMD,
	ESHVendor_ARM,
	ESHVendor_QCOM,
	ESHVendor_INTC,
	ESHVendor_IMGT,
	ESHVendor_MSFT,
	ESHVendor_APPL,
	ESHVendor_SMSG,
	ESHVendor_HWEI,
	ESHVendor_GOGL,
	ESHVendor_MESA,
	ESHVendor_Count
} ESHVendor;

//All bits below a count, which is how a vendorMask spells "any vendor".
//writerCount comes from SHHeader::vendorCount, so this is the everything of whoever wrote the file.

static inline U16 ESHVendor_allMask(U16 count) {
	//0xFFFF spelled out rather than U16_MAX, which lives in constants.h and isn't reachable from here.
	return count >= 16 ? (U16)0xFFFF : (U16)(((U32)1 << count) - 1);
}

//Restates an all-vendors mask in terms of the vendors that exist now.
//A file written before a vendor was added spells everything with fewer bits, and left alone that mask would
// quietly start excluding the new vendor, refusing a binary its author meant to run anywhere.
//A mask naming specific vendors is returned untouched: it never claimed to cover ones that did not exist.

static inline U16 ESHVendor_widenMask(U16 mask, U16 writerCount) {
	return mask == ESHVendor_allMask(writerCount) ? ESHVendor_allMask(ESHVendor_Count) : mask;
}

typedef enum ESHBinaryFlags {

	ESHBinaryFlags_None                     = 0,

	//What type of binaries it includes
	//Must be one at least

	ESHBinaryFlags_HasSPIRV                 = 1 << 0,
	ESHBinaryFlags_HasDXIL                  = 1 << 1,

	//Reserved
	//ESHBinaryFlags_HasAIR                 = 1 << 2,
	//ESHBinaryFlags_HasWGSL                = 1 << 3

	ESHBinaryFlags_HasShaderAnnotation      = 1 << 4,

	ESHBinaryFlags_HasBinary                = ESHBinaryFlags_HasSPIRV | ESHBinaryFlags_HasDXIL,    // | ESHBinaryFlags_HasAIR
	//ESHBinaryFlags_HasText                = ESHBinaryFlags_HasWGSL
	ESHBinaryFlags_HasSource                = ESHBinaryFlags_HasBinary // | ESHBinaryFlags_HasText

} ESHBinaryFlags;

extern const C8 *ESHVendor_names[ESHVendor_Count + 1];

typedef struct SHUniform {
	U16 dataOffset;
	TypeIdShort typeIdShort;        //< ETypeId_Max
	U8 nameId;
} SHUniform;

//Runtime SHEntry with some extra information that is used to decide how to compile
//This is how the SHEntry is found in the shader.
//Afterwards, it is transformed into binaries.
//Then the SHEntry will point to the binaries instead to save space.
typedef struct SHUniformRuntime {

	CharString name;

	U8 pad0;
	TypeIdShort typeIdShort;
	U16 dataOffset;

	U32 pad1;

} SHUniformRuntime;

TList(SHUniformRuntime);

void ListSHUniformRuntime_freeUnderlying(ListSHUniformRuntime *uniforms, const Allocator *alloc);

typedef struct SHBinaryIdentifier {

	CharString entrypoint;         //If it's not a lib, this defines the entrypoint to compile with
	ListCharString defines;        //[defineName, defineValue][]

	ListSHUniformRuntime uniforms;
	ListU8 uniformData;

	//Don't change order, is used for compare (U64)

	ESHExtension extensions;       //Needs 8-byte alignment
	U16 shaderVersion;             //U8 maj, minor (need 4-byte alignment, is compared together with stageType)
	U16 stageType;                 //ESHPipelineStage

} SHBinaryIdentifier;

typedef struct SHBinaryInfo {

	SHBinaryIdentifier identifier;

	ListSHRegisterRuntime registers;

	ESHExtension dormantExtensions;
	U16 vendorMask;
	Bool hasShaderAnnotation;      //If [shader("")] is used rather than [[oxc::stage("")]]
	U8 padding[1];

	Buffer binaries[ESHBinaryType_Count];

} SHBinaryInfo;

TList(SHBinaryIdentifier);
TList(SHBinaryInfo);

//This comparison is laxer than you'd expect, to allow Bindless/UnboundArraySize to be compatible and RT stages too.
//This mechanism is relied on by the shader compiler to merge compatible compiles (and SH combine).
Bool SHBinaryIdentifier_equals(const SHBinaryIdentifier *a, const SHBinaryIdentifier *b);

//This compare has to compare binaries, only use for when it's necessary such as unit tests.
Bool SHBinaryInfo_equalsExact(const SHBinaryInfo *a, const SHBinaryInfo *b);

void SHBinaryInfo_print(const SHBinaryInfo *binary, Bool isVerbose, const Allocator *alloc);
void SHBinaryIdentifier_free(SHBinaryIdentifier *identifier, const Allocator *alloc);
void SHBinaryInfo_free(SHBinaryInfo *info, const Allocator *alloc);

#ifdef __cplusplus
	}
#endif
