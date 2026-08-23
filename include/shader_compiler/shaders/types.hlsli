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

//shader_compiler/shaders/types.hlsli
//
//Scalar, vector and matrix typedefs shared by every OxC3 shader, plus the bit casts between them.
//Matrix maths, indirect argument structs and fixed point live in their own headers, included below.

#pragma once

#ifdef __OXC_EXT_16BITTYPES

	typedef float16_t F16;
	typedef float16_t2 F16x2;
	typedef float16_t3 F16x3;
	typedef float16_t4 F16x4;

	//Float16 matrices

	typedef float16_t4x4 F16x4x4;
	typedef float16_t3x4 F16x3x4;
	typedef float16_t2x4 F16x2x4;

	typedef float16_t4x3 F16x4x3;
	typedef float16_t3x3 F16x3x3;
	typedef float16_t2x3 F16x2x3;

	typedef float16_t4x2 F16x4x2;
	typedef float16_t3x2 F16x3x2;
	typedef float16_t2x2 F16x2x2;

	typedef int16_t I16;
	typedef int16_t2 I16x2;
	typedef int16_t3 I16x3;
	typedef int16_t4 I16x4;

	typedef uint16_t U16;
	typedef uint16_t2 U16x2;
	typedef uint16_t3 U16x3;
	typedef uint16_t4 U16x4;

	//Int16 matrices

	typedef int16_t4x4 I16x4x4;
	typedef int16_t3x4 I16x3x4;
	typedef int16_t2x4 I16x2x4;

	typedef int16_t4x3 I16x4x3;
	typedef int16_t3x3 I16x3x3;
	typedef int16_t2x3 I16x2x3;

	typedef int16_t4x2 I16x4x2;
	typedef int16_t3x2 I16x3x2;
	typedef int16_t2x2 I16x2x2;

	//Uint16 matrices

	typedef uint16_t4x4 U16x4x4;
	typedef uint16_t3x4 U16x3x4;
	typedef uint16_t2x4 U16x2x4;

	typedef uint16_t4x3 U16x4x3;
	typedef uint16_t3x3 U16x3x3;
	typedef uint16_t2x3 U16x2x3;

	typedef uint16_t4x2 U16x4x2;
	typedef uint16_t3x2 U16x3x2;
	typedef uint16_t2x2 U16x2x2;

#endif

//AtomicF64 implies the double types: its intrinsic takes a double by reference, so a shader that enables
//the atomic without the type could never call it, and the SPIRV capability check refuses that pairing anyway.

#if defined(__OXC_EXT_F64) || defined(__OXC_EXT_ATOMICF64)

	typedef double F64;
	typedef double2 F64x2;
	typedef double3 F64x3;
	typedef double4 F64x4;

	//Float64 matrices

	typedef double4x4 F64x4x4;
	typedef double3x4 F64x3x4;
	typedef double2x4 F64x2x4;

	typedef double4x3 F64x4x3;
	typedef double3x3 F64x3x3;
	typedef double2x3 F64x2x3;

	typedef double4x2 F64x4x2;
	typedef double3x2 F64x3x2;
	typedef double2x2 F64x2x2;

#endif

typedef uint U32;
typedef float F32;
typedef int I32;

typedef uint2 U32x2;
typedef float2 F32x2;
typedef int2 I32x2;

typedef uint3 U32x3;
typedef float3 F32x3;
typedef int3 I32x3;

typedef uint4 U32x4;
typedef float4 F32x4;
typedef int4 I32x4;

//Matrices

//Float matrices

typedef float4x4 F32x4x4;
typedef float3x4 F32x3x4;
typedef float2x4 F32x2x4;

typedef float4x3 F32x4x3;
typedef float3x3 F32x3x3;
typedef float2x3 F32x2x3;

typedef float4x2 F32x4x2;
typedef float3x2 F32x3x2;
typedef float2x2 F32x2x2;

//Int matrices

typedef int4x4 I32x4x4;
typedef int3x4 I32x3x4;
typedef int2x4 I32x2x4;

typedef int4x3 I32x4x3;
typedef int3x3 I32x3x3;
typedef int2x3 I32x2x3;

typedef int4x2 I32x4x2;
typedef int3x2 I32x3x2;
typedef int2x2 I32x2x2;

//Uint matrices

typedef uint4x4 U32x4x4;
typedef uint3x4 U32x3x4;
typedef uint2x4 U32x2x4;

typedef uint4x3 U32x4x3;
typedef uint3x3 U32x3x3;
typedef uint2x3 U32x2x3;

typedef uint4x2 U32x4x2;
typedef uint3x2 U32x3x2;
typedef uint2x2 U32x2x2;

#ifdef __OXC_EXT_I64

	typedef uint64_t U64;
	typedef uint64_t2 U64x2;
	typedef uint64_t3 U64x3;
	typedef uint64_t4 U64x4;

	typedef int64_t I64;
	typedef int64_t2 I64x2;
	typedef int64_t3 I64x3;
	typedef int64_t4 I64x4;

	//Uint64 matrices

	typedef uint64_t4x4 U64x4x4;
	typedef uint64_t3x4 U64x3x4;
	typedef uint64_t2x4 U64x2x4;

	typedef uint64_t4x3 U64x4x3;
	typedef uint64_t3x3 U64x3x3;
	typedef uint64_t2x3 U64x2x3;

	typedef uint64_t4x2 U64x4x2;
	typedef uint64_t3x2 U64x3x2;
	typedef uint64_t2x2 U64x2x2;

	//Int64 matrices

	typedef int64_t4x4 I64x4x4;
	typedef int64_t3x4 I64x3x4;
	typedef int64_t2x4 I64x2x4;

	typedef int64_t4x3 I64x4x3;
	typedef int64_t3x3 I64x3x3;
	typedef int64_t2x3 I64x2x3;

	typedef int64_t4x2 I64x4x2;
	typedef int64_t3x2 I64x3x2;
	typedef int64_t2x2 I64x2x2;

#endif

//Bool

typedef bool Bool;

typedef bool2 Boolx2;
typedef bool3 Boolx3;
typedef bool4 Boolx4;

//Binding graphics shader outputs (e.g. uv : _bind(0))

#define _bind(x) TEXCOORD##x
#define _flat nointerpolation

//Split out of this header once it got too broad to read. Included at the exact spot the code used
//to sit so declaration order - and therefore the generated SPIR-V ids - is unchanged.

#include "@indirect.hlsli"
#include "@mat.hlsli"

U32 U32_fromF32(F32 f) { return asuint(f); }
U32x2 U32x2_fromF32x2(F32x2 f) { return asuint(f); }
U32x3 U32x3_fromF32x3(F32x3 f) { return asuint(f); }
U32x4 U32x4_fromF32x4(F32x4 f) { return asuint(f); }

F32 F32_fromU32(U32 u) { return asfloat(u); }
F32x2 F32x2_fromU32x2(U32x2 u) { return asfloat(u); }
F32x3 F32x3_fromU32x3(U32x3 u) { return asfloat(u); }
F32x4 F32x4_fromU32x4(U32x4 u) { return asfloat(u); }

//Fused multiply-add (a * b + c)

F32x4 F32x4_fma(F32x4 a, F32x4 b, F32x4 c) { return mad(a, b, c); }

#include "@fixed_point.hlsli"

#ifdef __spirv__

	#define UNKNOWN_FORMAT [[vk::image_format("unknown")]]
	#define PUSH_CONSTANT [[vk::push_constant]] 

	//Dual source blending's two outputs.
	//SPIR-V wants BOTH at location 0, told apart by the Index decoration (location 1 would mean a second attachment instead).
	//DXIL reads plain SV_Target0/1 as src0/src1 whenever an Src1 blend factor is used, so there both expand to nothing.
	//Both outputs must carry a macro, since DXC refuses a mix of explicit and implicit locations.
	//Use as: DUAL_SRC_TARGET0 F32x4 a : SV_Target0; DUAL_SRC_TARGET1 F32x4 b : SV_Target1;

	#define DUAL_SRC_TARGET0 [[vk::location(0)]]
	#define DUAL_SRC_TARGET1 [[vk::location(0)]] [[vk::index(1)]]

#else
	#define UNKNOWN_FORMAT
	#define PUSH_CONSTANT
	#define DUAL_SRC_TARGET0
	#define DUAL_SRC_TARGET1
#endif

//The register space OxC3 reserves for its own per frame globals on DXIL. Anything a shader declares there
//is refused at layout creation, because the runtime binds its own data to it.
//Keep in sync with OXC3_RESERVED_SPACE in graphics/generic/descriptor_layout.h.

#define OXC3_RESERVED_SPACE space195        //0xC3

//Memory scope and semantics, as the SPIR-V atomic instructions take them.
//These are the raw SPIR-V enumerant values rather than an HLSL enum, because they are handed straight to an
//[[vk::ext_instruction]] intrinsic, which takes plain uints.
//A shader that names them reads as what it means instead of as a pair of bare numbers.

namespace oxc {

	//OpAtomic* memory scope: how far the operation is ordered.
	//Device is the one to reach for on a buffer other dispatches read; Workgroup only orders within a group.

	static const U32 MemoryScope_CrossDevice   = 0;
	static const U32 MemoryScope_Device        = 1;
	static const U32 MemoryScope_Workgroup     = 2;
	static const U32 MemoryScope_Subgroup      = 3;
	static const U32 MemoryScope_Invocation    = 4;
	static const U32 MemoryScope_QueueFamily   = 5;

	//OpAtomic* memory semantics: what ordering the operation itself imposes.
	//Relaxed is the plain atomic with no ordering beyond the operation being indivisible, which is what an
	//accumulate wants; the acquire/release forms are for handing data between invocations.

	static const U32 MemorySemantics_Relaxed              = 0x000;
	static const U32 MemorySemantics_Acquire              = 0x002;
	static const U32 MemorySemantics_Release              = 0x004;
	static const U32 MemorySemantics_AcquireRelease       = 0x008;
	static const U32 MemorySemantics_SequentiallyConsistent = 0x010;

	//Storage class qualifiers, OR'd onto the semantics above when the ordering has to name the memory it covers

	static const U32 MemorySemantics_UniformMemory        = 0x040;
	static const U32 MemorySemantics_SubgroupMemory       = 0x080;
	static const U32 MemorySemantics_WorkgroupMemory      = 0x100;
	static const U32 MemorySemantics_CrossWorkgroupMemory = 0x200;
	static const U32 MemorySemantics_ImageMemory          = 0x800;

}
