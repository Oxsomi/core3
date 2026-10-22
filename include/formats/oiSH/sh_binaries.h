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

extern const C8 *EGfxBinaryType_names[EGfxBinaryType_Count];

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
	//SPIRV: SPV_EXT_descriptor_heap (DescriptorHeapEXT capability), detected but not compiled, see ESHExtension_NoSpirvCompile.
	ESHExtension_DescriptorHeap              = 1 << 25,

	//Subgroup quad ops: QuadReadAcrossX/Y/Diagonal and QuadReadLaneAt.
	//SPIRV GroupNonUniformQuad, Vulkan VK_SUBGROUP_FEATURE_QUAD_BIT, part of core 1.1 subgroup properties.
	//Held apart from SubgroupShuffle rather than folded into it because the two are separately reported and
	// drivers exist that expose shuffle without quad.
	//Held apart from ComputeDeriv because they gate different things:
	// a quad is implicit in a pixel shader, so quad ops there need this alone,
	// while in a compute shader DXC also emits a derivative group execution mode and BOTH are required.
	ESHExtension_SubgroupQuad                = 1 << 26,

	//An ARRAY of samplers, which is only servable by the bindless _samplers[] array and so needs
	// EGraphicsDeviceFlags_EnableDynamicSamplers on the device.
	//A singular sampler is a plain binding or a static sampler and requires none of this.
	//Set by the annotation, and reflection also infers it for a binary declaring a sampler array of its own.

	ESHExtension_DynamicSamplers             = 1 << 27,

	//A binary its author vouches for rather than one OxC3 can check, for experiments with instructions no API can
	// report support for (a private extended instruction set in a patched driver, say).
	//Nothing is validated for such a binary, so it may hold modules the validator would refuse, such as a NonSemantic
	// extended instruction whose result is used, the one kind of unknown instruction set SPIRV-Tools passes through
	// without a grammar; its NonSemantic instructions are kept where other binaries have them stripped.
	//At runtime only a device created with EGraphicsDeviceFlags_AllowUnsafeExperimentExecution takes it, and that
	// device then skips its feature checks for it, so the binary also runs where an extension it declares is
	// unsupported. Whether the device can execute it is the caller's to know; an experiment tier the binary declares
	// (ESHExtension2_ExperimentTier1) is the one check it still gets.
	//Annotation only, and SPIR-V only: DXIL has no unvalidated path.

	ESHExtension_UnsafeExperimentExecution   = 1 << 28,

	//A binary that uses extensions OxC3 doesn't recognize: SPIR-V capabilities oiSH has no extension for, any SPIR-V
	// extension DXC knows (no allow list is passed), and DXIL requirement flags oiSH has no extension for. Those pass
	// and map to nothing, while the extensions OxC3 does recognize are reflected and checked as for any binary.
	//At runtime only a device created with EGraphicsDeviceFlags_AllowUnsafeExtensionExecution takes it, as the device
	// cannot check what it doesn't know of.
	//Annotation only.

	ESHExtension_UnsafeExtensionExecution    = 1 << 29,

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
		ESHExtension_SubgroupQuad |
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

	//SubgroupArithmetic, SubgroupShuffle and SubgroupQuad deliberately are NOT here:
	// WaveActiveSum / WaveReadLaneAt / QuadReadAcrossX are plain HLSL, so DXC compiles them to DXIL fine.
	//DXIL reflection just can't DETECT them (one generic wave ops flag), which is a DxilNative matter and
	// keeps them annotation-driven on DXIL, not a reason to refuse the compile.

	ESHExtension_NoDxilCompile =                              //SPIRV-only to compile: no DXIL intrinsic exists
		ESHExtension_AtomicF32 |
		ESHExtension_AtomicF64 |
		ESHExtension_UnsafeExperimentExecution,

	//DXIL-only to compile. PAQ and WriteMSTexture have SPIR-V forms, but DXC's SPIR-V backend refuses both.
	//DescriptorHeap has one too, but DXC leaves the heap array without the stride SPV_EXT_descriptor_heap requires.

	ESHExtension_NoSpirvCompile =
		ESHExtension_MeshTaskTexDeriv |
		ESHExtension_PAQ |
		ESHExtension_WriteMSTexture |
		ESHExtension_DescriptorHeap,

	//Full: 1 << ESHExtension_Count has to stay an int, so an extension past these goes in ESHExtension2.

	ESHExtension_Count                       = 30,

	ESHExtension_All                         = (1 << ESHExtension_Count) - 1

} ESHExtension;

extern const C8 *ESHExtension_defines[ESHExtension_Count];
extern const C8 *ESHExtension_names[ESHExtension_Count];

//The extensions past ESHExtension. A binary carries a mask of each, and an [[oxc::extension(...)]] annotation names
// extensions of either (SHExtensionSet).

typedef enum ESHExtension2 {

	ESHExtension2_None                       = 0,

	//An experiment tier the device's driver claims (EGraphicsFeatures2_ExperimentTier1 and ExperimentTier2), held
	// against the device for every binary that declares one, an UnsafeExperimentExecution binary included.
	//OxC3 gives the tiers no meaning: what a binary at one may use is between the driver claiming it and the
	// binary's author. Annotation only, on both backends, though only a Vulkan device ever claims a tier.

	ESHExtension2_ExperimentTier1            = 1 << 0,
	ESHExtension2_ExperimentTier2            = 1 << 1,

	//Packed 4 x 8 bit integer dot products with a 32 bit accumulate: dot4add_i8packed and dot4add_u8packed.
	//DXIL: SM6.4's Dot4AddI8Packed and Dot4AddU8Packed, which set no requires flag, so reflection can't detect them;
	// every device OxC3 compiles DXIL for runs them, as its shader model floor is above 6.4.
	//SPIR-V: DXC lowers them to OpSDot and OpUDot of PackedVectorFormat4x8Bit and an OpIAdd (SPV_KHR_integer_dot_product),
	// which declares the DotProduct and DotProductInput4x8BitPacked capabilities that map to this extension.
	//The device needs EGraphicsFeatures2_IntegerDot4x8.

	ESHExtension2_IntegerDot4x8              = 1 << 2,

	//What reflection detects, as ESHExtension_DxilNative and ESHExtension_SpirvNative are for the first mask.
	//SPIR-V reflects IntegerDot4x8 from its capabilities, so a binary that declares it and doesn't use it has it dormant.
	//DXIL reflects none of these.

	ESHExtension2_DxilNative                 = ESHExtension2_None,
	ESHExtension2_SpirvNative                = ESHExtension2_IntegerDot4x8,

	ESHExtension2_Count                      = 3,

	ESHExtension2_All                        = (1 << ESHExtension2_Count) - 1

} ESHExtension2;

extern const C8 *ESHExtension2_defines[ESHExtension2_Count];
extern const C8 *ESHExtension2_names[ESHExtension2_Count];

//The extensions one [[oxc::extension(...)]] line names, across both masks.

typedef struct SHExtensionSet {
	ESHExtension extensions;
	ESHExtension2 extensions2;
} SHExtensionSet;

TList(SHExtensionSet);

//Logs the names of the extensions in set as an annotation lists them, quoted and comma separated, without a newline.

void SHExtensionSet_logNames(SHExtensionSet set, const Allocator *alloc);

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
	U16 stageType;                 //EGfxPipelineStage

	ESHExtension2 extensions2;     //Not part of the U64 above, so copied and compared beside it
	U32 padding;

} SHBinaryIdentifier;

typedef struct SHBinaryInfo {

	SHBinaryIdentifier identifier;

	ListSHRegisterRuntime registers;

	ESHExtension dormantExtensions;
	ESHExtension2 dormantExtensions2;

	U16 vendorMask;
	Bool hasShaderAnnotation;      //If [shader("")] is used rather than [[oxc::stage("")]]
	U8 padding[5];

	Buffer binaries[EGfxBinaryType_Count];

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

//The oldest shader model that can express an extension set on DXIL, as OISH_SHADER_MODEL.
//It lives beside the enum because the requirement is a property of the extension itself: several of the
// declarations above already say so in prose (RayTriPosition is "SM6.10 tri vertex position fetch").
//A binary's model is PROMOTED to this when its extensions need more than the entry asked for, so the value
// has to be a true lower bound: too low is caught by the shader compiler's own availability diagnostic,
// while too high costs devices that could have run the binary.
//SPIRV has no shader models and never checks one at runtime, but the identifier is shared by both
// backends, so the promoted value is what the SPIRV binary records too.

U16 ESHExtension_minShaderModel(ESHExtension ext);
void SHBinaryInfo_free(SHBinaryInfo *info, const Allocator *alloc);

#ifdef __cplusplus
	}
#endif
