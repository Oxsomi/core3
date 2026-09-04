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

//formats/gfx_util/gfx_util.h

#pragma once
#include "types/base/types.h"
#include "types/base/constants.h"

#ifdef __cplusplus
	extern "C" {
#endif

//The graphics vocabulary more than one format serializes, owned here so no format has to include another
// just to name a value: oiSH, oiSP and oiPL all store these.

typedef enum EGfxPipelineStage {

	EGfxPipelineStage_Vertex,
	EGfxPipelineStage_Pixel,
	EGfxPipelineStage_Compute,
	EGfxPipelineStage_GeometryExt,        //GeometryShader extension is required
	EGfxPipelineStage_Hull,
	EGfxPipelineStage_Domain,

	//RayPipeline extension is required

	EGfxPipelineStage_RaygenExt,
	EGfxPipelineStage_CallableExt,
	EGfxPipelineStage_MissExt,
	EGfxPipelineStage_ClosestHitExt,
	EGfxPipelineStage_AnyHitExt,
	EGfxPipelineStage_IntersectionExt,

	EGfxPipelineStage_RtStartExt = EGfxPipelineStage_RaygenExt,
	EGfxPipelineStage_RtEndExt   = EGfxPipelineStage_IntersectionExt,

	//MeshShader extension is required

	EGfxPipelineStage_MeshExt,
	EGfxPipelineStage_TaskExt,

	//Reserved, free to reuse.
	//The slot stays because the stage id is serialized in oiSH and indexes SHEntry_stageNames.

	EGfxPipelineStage_Reserved,

	EGfxPipelineStage_Count

} EGfxPipelineStage;

typedef enum EGfxRegisterType {

	EGfxRegisterType_Sampler,
	EGfxRegisterType_SamplerComparisonState,

	EGfxRegisterType_ConstantBuffer,                    //UBO or CBuffer
	EGfxRegisterType_PushConstants,                     //Push constants or CBuffer (DXIL)
	EGfxRegisterType_ByteAddressBuffer,
	EGfxRegisterType_StructuredBuffer,
	EGfxRegisterType_StructuredBufferAtomic,            //SBuffer + atomic counter
	EGfxRegisterType_StorageBuffer,
	EGfxRegisterType_StorageBufferAtomic,
	EGfxRegisterType_AccelerationStructure,

	EGfxRegisterType_Texture1D,
	EGfxRegisterType_Texture2D,
	EGfxRegisterType_Texture3D,
	EGfxRegisterType_TextureCube,
	EGfxRegisterType_Texture2DMS,
	EGfxRegisterType_SubpassInput,

	EGfxRegisterType_Count,

	EGfxRegisterType_BufferStart            = EGfxRegisterType_ConstantBuffer,
	EGfxRegisterType_BufferEnd              = EGfxRegisterType_AccelerationStructure,

	EGfxRegisterType_TextureStart           = EGfxRegisterType_Texture1D,
	EGfxRegisterType_TextureEnd             = EGfxRegisterType_SubpassInput, //>= to see if real texture, > means 'invalid'

	EGfxRegisterType_TypeMask               = 0xF,
	EGfxRegisterType_IsArray                = 1 << 4,    //Only valid on textures
	EGfxRegisterType_IsCombinedSampler      = 1 << 5,    //Only valid on textures

	//Invalid on samplers, AS and CBuffer
	//Required on append/consume buffer
	//Valid on everything else (textures and various buffers)
	EGfxRegisterType_IsWrite                = 1 << 6,

	EGfxRegisterType_Masks                  =
		EGfxRegisterType_IsArray | EGfxRegisterType_IsCombinedSampler | EGfxRegisterType_IsWrite

} EGfxRegisterType;

typedef enum EGfxBinaryType {

	EGfxBinaryType_SPIRV,
	EGfxBinaryType_DXIL,

	//EGfxBinaryType_AIR,
	//EGfxBinaryType_WGSL,

	EGfxBinaryType_Count

} EGfxBinaryType;

typedef struct GfxBinding {
	U32 space;                        //Space or set, depending on binary type
	U32 binding;
} GfxBinding;

//U32_MAX for both space and binding indicates 'not present'
typedef union GfxBindings {
	U64 arrU64[EGfxBinaryType_Count];
	GfxBinding arr[EGfxBinaryType_Count];
} GfxBindings;

//No compound literals or designated initializers here, since C++ wrappers include this header too

static inline GfxBindings GfxBindings_dummy() {

	GfxBindings bindings = { { 0 } };

	for(U8 i = 0; i < EGfxBinaryType_Count; ++i)
		bindings.arrU64[i] = U64_MAX;

	return bindings;
}

//The register space the runtime owns for its bindless set on DXIL (types.hlsli mirrors it as space195).
//A user layout can never bind there.

#define OXC3_RESERVED_SPACE 0xC3

//The DXIL register namespace a register type binds in (t, u, s or b), so a same-numbered binding in another
// namespace is never mistaken for the same register: t0, u0, s0 and b0 coexist.

typedef enum EGfxDxilBindingClass {
	EGfxDxilBindingClass_T,
	EGfxDxilBindingClass_U,
	EGfxDxilBindingClass_S,
	EGfxDxilBindingClass_B
} EGfxDxilBindingClass;

static inline EGfxDxilBindingClass EGfxRegisterType_dxilBindingClass(U32 type) {

	switch (type & EGfxRegisterType_TypeMask) {

		case EGfxRegisterType_Sampler:
		case EGfxRegisterType_SamplerComparisonState:
			return EGfxDxilBindingClass_S;

		case EGfxRegisterType_ConstantBuffer:
		case EGfxRegisterType_PushConstants:
			return EGfxDxilBindingClass_B;

		default:
			return type & EGfxRegisterType_IsWrite ? EGfxDxilBindingClass_U : EGfxDxilBindingClass_T;
	}
}

//Whether two registers of one layout collide, given each one's descriptor count.
//A SPIRV array stays one (set, binding) slot, so only an identical pair collides.
//A DXIL array occupies [binding, binding + count) in its space and namespace, and count 0 is an unbounded
// array that reaches every register above its start.
//An absent pair (space and binding both U32_MAX) never collides; a lone U32_MAX member is a real location.

static inline Bool GfxBinding_overlaps(
	GfxBinding a, U32 registerTypeA, U64 countA,
	GfxBinding b, U32 registerTypeB, U64 countB,
	EGfxBinaryType binaryType
) {

	if((a.space == U32_MAX && a.binding == U32_MAX) || (b.space == U32_MAX && b.binding == U32_MAX))
		return false;

	if(binaryType != EGfxBinaryType_DXIL)
		return a.space == b.space && a.binding == b.binding;

	if(
		a.space != b.space ||
		EGfxRegisterType_dxilBindingClass(registerTypeA) != EGfxRegisterType_dxilBindingClass(registerTypeB)
	)
		return false;

	const U64 endA = !countA || countA > U64_MAX - a.binding ? U64_MAX : a.binding + countA;
	const U64 endB = !countB || countB > U64_MAX - b.binding ? U64_MAX : b.binding + countB;

	return a.binding < endB && b.binding < endA;
}

typedef enum EGfxTexturePrimitive {

	EGfxTexturePrimitive_UInt,
	EGfxTexturePrimitive_SInt,
	EGfxTexturePrimitive_UNorm,
	EGfxTexturePrimitive_SNorm,
	EGfxTexturePrimitive_Float,
	EGfxTexturePrimitive_Double,
	EGfxTexturePrimitive_Count,

	EGfxTexturePrimitive_TypeMask          = 0x0F,

	EGfxTexturePrimitive_ComponentShift    = 4,

	EGfxTexturePrimitive_Component1        = 0x00,        //R
	EGfxTexturePrimitive_Component2        = 0x10,        //RG
	EGfxTexturePrimitive_Component3        = 0x20,        //RGB
	EGfxTexturePrimitive_Component4        = 0x30,        //RGBA

	EGfxTexturePrimitive_CountAll          = 0x40,

	EGfxTexturePrimitive_Unused            = 0xC0

} EGfxTexturePrimitive;

typedef struct GfxTextureFormat {     //Primitive is set for DXIL always and formatId is only for SPIRV (or DXIL RW)
	U8 primitive;                     //Opt for readonly registers: EGfxTexturePrimitive must match format approx
	U8 formatId;                      //Opt for write registers: ETextureFormatId Must match formatPrimitive & uncompressed
} GfxTextureFormat;

typedef enum ECompareOp {
	ECompareOp_Gt,
	ECompareOp_Geq,
	ECompareOp_Eq,
	ECompareOp_Neq,
	ECompareOp_Leq,
	ECompareOp_Lt,
	ECompareOp_Always,
	ECompareOp_Never,
	ECompareOp_Count
} ECompareOp;

#ifdef __cplusplus
	}
#endif
