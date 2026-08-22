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

//CoopMat (SM6.10 cooperative matrix: a subgroup-cooperative matrix * matrix / GEMM, the batched attention/FFN op).
//This is the cross-vendor GPU linear-algebra primitive on Vulkan (unlike cooperative vectors, which are NV-only).
//DXIL lowers dx::linalg Matrix<..,Wave>::Load + Multiply (native GEMM intrinsic).
//SPIR-V uses SPV_KHR_cooperative_matrix, emitted as inline SPIR-V (DXC's own vk/khr/cooperative_matrix.h isn't in the
//linked DXC's builtin headers, so the ops are declared here directly - the same way CoopVec declares its NV ops). The
//opaque OpTypeCooperativeMatrixKHR carries the CooperativeMatrixKHR + VulkanMemoryModel capabilities; DXC then upgrades
//OpMemoryModel to Vulkan. It requires SPIR-V 1.6, which OxC3 already targets for cooperative extensions.
//
//This slice is a 16x16x16 F16 GEMM (D = A*B), row-major. The matrix source differs per backend (a ByteAddressBuffer for
//DXIL's dx::linalg, a RWStructuredBuffer<float16_t> for the SPIR-V load whose element pointer the op reads), and the
//load/store stride differs (bytes on DXIL, elements-of-pointee on SPIR-V), both hidden behind the macros.

#pragma once

//dx/linalg.h (the DXIL path) is included once by @extensions.hlsli, since it has no include guard and both CoopVec
//and CoopMat need it. SPIR-V uses the inline ops below instead.

#ifdef __spirv__
	#define OXC_COOPMAT_INPUT_BUFFER(name)  RWStructuredBuffer<float16_t> name
	#define OXC_COOPMAT_OUTPUT_BUFFER(name) RWStructuredBuffer<float16_t> name
	//FP8 matrices are read through a uint pointee (4 FP8 per uint); the KHR op allows pointee != component type.
	#define OXC_COOPMAT_INPUT_BUFFER_FP8(name) RWStructuredBuffer<uint> name
#else
	#define OXC_COOPMAT_INPUT_BUFFER(name)  ByteAddressBuffer name
	#define OXC_COOPMAT_OUTPUT_BUFFER(name) RWByteAddressBuffer name
	#define OXC_COOPMAT_INPUT_BUFFER_FP8(name) ByteAddressBuffer name
#endif

namespace oxc {

#ifdef __spirv__

	//OpTypeCooperativeMatrixKHR operands: Component Type, Scope (Subgroup = 3), Rows, Columns, Use (A=0 / B=1 / Acc=2).
	//Scope/Rows/Columns/Use are constant ids, so they use vk::integral_constant (an OpConstant), never vk::Literal.
	typedef vk::SpirvOpaqueType</* OpTypeCooperativeMatrixKHR */ 4456, float16_t, vk::integral_constant<uint, 3>, vk::integral_constant<uint, 16>, vk::integral_constant<uint, 16>, vk::integral_constant<uint, 0> > CoopMatA16F16;
	typedef vk::SpirvOpaqueType</* OpTypeCooperativeMatrixKHR */ 4456, float16_t, vk::integral_constant<uint, 3>, vk::integral_constant<uint, 16>, vk::integral_constant<uint, 16>, vk::integral_constant<uint, 1> > CoopMatB16F16;
	typedef vk::SpirvOpaqueType</* OpTypeCooperativeMatrixKHR */ 4456, float16_t, vk::integral_constant<uint, 3>, vk::integral_constant<uint, 16>, vk::integral_constant<uint, 16>, vk::integral_constant<uint, 2> > CoopMatAcc16F16;

	//OpCooperativeMatrixLoadKHR: Pointer, MemoryLayout, Stride, MemoryOperand (a literal). One per Use (return type).
	[[vk::ext_capability(/* CooperativeMatrixKHR */ 6022)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_KHR_cooperative_matrix")]]
	[[vk::ext_instruction(/* OpCooperativeMatrixLoadKHR */ 4457)]]
	CoopMatA16F16 __coopMatLoadA16F16([[vk::ext_reference]] float16_t ptr, uint memoryLayout, uint stride, [[vk::ext_literal]] uint memoryOperand);

	[[vk::ext_capability(/* CooperativeMatrixKHR */ 6022)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_KHR_cooperative_matrix")]]
	[[vk::ext_instruction(/* OpCooperativeMatrixLoadKHR */ 4457)]]
	CoopMatB16F16 __coopMatLoadB16F16([[vk::ext_reference]] float16_t ptr, uint memoryLayout, uint stride, [[vk::ext_literal]] uint memoryOperand);

	//OpCooperativeMatrixMulAddKHR: A, B, C, CooperativeMatrixOperands (a literal). D = A*B + C.
	[[vk::ext_capability(/* CooperativeMatrixKHR */ 6022)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_KHR_cooperative_matrix")]]
	[[vk::ext_instruction(/* OpCooperativeMatrixMulAddKHR */ 4459)]]
	CoopMatAcc16F16 __coopMatMulAdd16F16(CoopMatA16F16 a, CoopMatB16F16 b, CoopMatAcc16F16 c, [[vk::ext_literal]] int operands);

	//OpCompositeConstruct on a cooperative matrix with one constituent fills every element with that value.
	[[vk::ext_capability(/* CooperativeMatrixKHR */ 6022)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_KHR_cooperative_matrix")]]
	[[vk::ext_instruction(/* OpCompositeConstruct */ 80)]]
	CoopMatAcc16F16 __coopMatSplatAcc16F16(float16_t value);

	//OpCooperativeMatrixStoreKHR: Pointer, Object, MemoryLayout, Stride, MemoryOperand (a literal). No result.
	[[vk::ext_capability(/* CooperativeMatrixKHR */ 6022)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_KHR_cooperative_matrix")]]
	[[vk::ext_instruction(/* OpCooperativeMatrixStoreKHR */ 4458)]]
	void __coopMatStoreAcc16F16([[vk::ext_reference]] float16_t ptr, CoopMatAcc16F16 obj, uint memoryLayout, uint stride, [[vk::ext_literal]] uint memoryOperand);

	//FP8 (e4m3) GEMM: A/B are FP8 cooperative matrices, accumulator stays F16. The FP8 component type is an inline
	//8-bit OpTypeFloat with the Float8E4M3EXT encoding (SPV_EXT_float8); its cooperative use needs Float8CooperativeMatrixEXT.
	typedef vk::SpirvType</* OpTypeFloat */ 22, /* size */ 1, /* alignment */ 1, vk::Literal<vk::integral_constant<uint, 8> >, vk::Literal<vk::integral_constant<uint, /* Float8E4M3EXT */ 4214> > > OxCFloat8E4M3;

	typedef vk::SpirvOpaqueType</* OpTypeCooperativeMatrixKHR */ 4456, OxCFloat8E4M3, vk::integral_constant<uint, 3>, vk::integral_constant<uint, 16>, vk::integral_constant<uint, 16>, vk::integral_constant<uint, 0> > CoopMatA16FP8;
	typedef vk::SpirvOpaqueType</* OpTypeCooperativeMatrixKHR */ 4456, OxCFloat8E4M3, vk::integral_constant<uint, 3>, vk::integral_constant<uint, 16>, vk::integral_constant<uint, 16>, vk::integral_constant<uint, 1> > CoopMatB16FP8;

	[[vk::ext_capability(/* CooperativeMatrixKHR */ 6022)]]
	[[vk::ext_capability(/* Float8EXT */ 4212)]]
	[[vk::ext_capability(/* Float8CooperativeMatrixEXT */ 4213)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_KHR_cooperative_matrix")]]
	[[vk::ext_extension("SPV_EXT_float8")]]
	[[vk::ext_instruction(/* OpCooperativeMatrixLoadKHR */ 4457)]]
	CoopMatA16FP8 __coopMatLoadA16FP8([[vk::ext_reference]] uint ptr, uint memoryLayout, uint stride, [[vk::ext_literal]] uint memoryOperand);

	[[vk::ext_capability(/* CooperativeMatrixKHR */ 6022)]]
	[[vk::ext_capability(/* Float8EXT */ 4212)]]
	[[vk::ext_capability(/* Float8CooperativeMatrixEXT */ 4213)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_KHR_cooperative_matrix")]]
	[[vk::ext_extension("SPV_EXT_float8")]]
	[[vk::ext_instruction(/* OpCooperativeMatrixLoadKHR */ 4457)]]
	CoopMatB16FP8 __coopMatLoadB16FP8([[vk::ext_reference]] uint ptr, uint memoryLayout, uint stride, [[vk::ext_literal]] uint memoryOperand);

	[[vk::ext_capability(/* CooperativeMatrixKHR */ 6022)]]
	[[vk::ext_capability(/* Float8EXT */ 4212)]]
	[[vk::ext_capability(/* Float8CooperativeMatrixEXT */ 4213)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_KHR_cooperative_matrix")]]
	[[vk::ext_extension("SPV_EXT_float8")]]
	[[vk::ext_instruction(/* OpCooperativeMatrixMulAddKHR */ 4459)]]
	CoopMatAcc16F16 __coopMatMulAddFP8(CoopMatA16FP8 a, CoopMatB16FP8 b, CoopMatAcc16F16 c, [[vk::ext_literal]] int operands);

#else

	//DXIL GEMM: 16x16x16 F16, row-major, D = A*B, stored to bufOut. Wave-scope matrices (compute only).
	//A template so it's only instantiated when a shader actually uses it (keeps it out of unrelated reflection).
	template<uint OxCUnused = 0>
	void CoopMatGemm16F16(ByteAddressBuffer bufA, ByteAddressBuffer bufB, RWByteAddressBuffer bufOut) {
		using namespace dx::linalg;
		using MatA   = Matrix<ComponentType::F16, 16, 16, MatrixUse::A,           MatrixScope::Wave>;
		using MatB   = Matrix<ComponentType::F16, 16, 16, MatrixUse::B,           MatrixScope::Wave>;
		using MatAcc = Matrix<ComponentType::F16, 16, 16, MatrixUse::Accumulator, MatrixScope::Wave>;
		//Row stride = columns * element size = 16 * 2 = 32 bytes (must be a multiple of 16).
		MatA a = MatA::Load(bufA, 0, 32, MatrixLayout::RowMajor);
		MatB b = MatB::Load(bufB, 0, 32, MatrixLayout::RowMajor);
		MatAcc d = Multiply(a, b);
		d.Store(bufOut, 0, 32, MatrixLayout::RowMajor);
	}

	//FP8 (e4m3) GEMM: D = A*B with FP8 inputs and an F16 accumulator. Row stride: FP8 = 16 bytes, F16 out = 32 bytes.
	template<uint OxCUnused = 0>
	void CoopMatGemmFP8_16(ByteAddressBuffer bufA, ByteAddressBuffer bufB, RWByteAddressBuffer bufOut) {
		using namespace dx::linalg;
		using MatA   = Matrix<ComponentType::F8_E4M3FN, 16, 16, MatrixUse::A,           MatrixScope::Wave>;
		using MatB   = Matrix<ComponentType::F8_E4M3FN, 16, 16, MatrixUse::B,           MatrixScope::Wave>;
		using MatAcc = Matrix<ComponentType::F16,       16, 16, MatrixUse::Accumulator, MatrixScope::Wave>;
		MatA a = MatA::Load(bufA, 0, 16, MatrixLayout::RowMajor);
		MatB b = MatB::Load(bufB, 0, 16, MatrixLayout::RowMajor);
		MatAcc d = Multiply<ComponentType::F16>(a, b);
		d.Store(bufOut, 0, 32, MatrixLayout::RowMajor);
	}

#endif

}

//Public API: D = A*B (16x16x16 F16, row-major), stored to bufOut. A cross-vendor cooperative-matrix GEMM.
//RowMajorKHR = 0 (memory layout). SPIR-V stride is in elements of the pointee type (float16_t): 16 for a 16-wide row.
#ifdef __spirv__
	#define OXC_COOPMAT_GEMM_16_F16(bufA, bufB, bufOut) { oxc::CoopMatA16F16 __oxcA = oxc::__coopMatLoadA16F16((bufA)[0], 0, 16, 0); oxc::CoopMatB16F16 __oxcB = oxc::__coopMatLoadB16F16((bufB)[0], 0, 16, 0); oxc::CoopMatAcc16F16 __oxcC = oxc::__coopMatSplatAcc16F16((float16_t)0); oxc::CoopMatAcc16F16 __oxcD = oxc::__coopMatMulAdd16F16(__oxcA, __oxcB, __oxcC, 0); oxc::__coopMatStoreAcc16F16((bufOut)[0], __oxcD, 0, 16, 0); }
#else
	#define OXC_COOPMAT_GEMM_16_F16(bufA, bufB, bufOut) oxc::CoopMatGemm16F16((bufA), (bufB), (bufOut))
#endif

//Public API (quantized): D = A*B with FP8 (e4m3) inputs, F16 accumulator, row-major. Cross-vendor FP8 GEMM.
//SPIR-V FP8 matrices are read through a uint pointee (4 FP8/uint), so the row stride is 4 uint for a 16-wide row.
#ifdef __spirv__
	#define OXC_COOPMAT_GEMM_FP8_16(bufA, bufB, bufOut) { oxc::CoopMatA16FP8 __oxcA = oxc::__coopMatLoadA16FP8((bufA)[0], 0, 4, 0); oxc::CoopMatB16FP8 __oxcB = oxc::__coopMatLoadB16FP8((bufB)[0], 0, 4, 0); oxc::CoopMatAcc16F16 __oxcC = oxc::__coopMatSplatAcc16F16((float16_t)0); oxc::CoopMatAcc16F16 __oxcD = oxc::__coopMatMulAddFP8(__oxcA, __oxcB, __oxcC, 0); oxc::__coopMatStoreAcc16F16((bufOut)[0], __oxcD, 0, 16, 0); }
#else
	#define OXC_COOPMAT_GEMM_FP8_16(bufA, bufB, bufOut) oxc::CoopMatGemmFP8_16((bufA), (bufB), (bufOut))
#endif
