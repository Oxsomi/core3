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

//CoopVec (SM6.10 cooperative vectors: a per-thread matrix * vector, the LLM per-token projection / MLP op).
//DXIL lowers dx::linalg (Matrix<..,Thread>::Load + Multiply / MultiplyAdd) to __builtin_MatVecMul(Add).
//DXC's SPIR-V backend has no such lowering (dx/linalg.h #errors on __spirv__), so there it is inline
//SPV_NV_cooperative_vector - opaque OpTypeCooperativeVectorNV values loaded / matmul'd / stored via ops
//5302/5289/5292/5303. This extension is NVIDIA-only on Vulkan: there is no cross-vendor KHR cooperative vector
//(the cross-vendor primitive is cooperative matrix - see @extension.CoopMat.hlsli). The NV spec requires the Vulkan
//memory model, so each op also declares VulkanMemoryModel (5345); DXC then upgrades OpMemoryModel to Vulkan.
//
//Both backends read matrix / vectors from buffers (a memory-to-memory matvec); the per-backend differences are
//hidden behind macros (OXC_COOPVEC_MATRIX_BUFFER / OXC_COOPVEC_VECTOR_BUFFER, and the OXC_COOPVEC_* op macros).

#pragma once

//dx/linalg.h (the DXIL path) is included once by @extensions.hlsli, since it has no include guard and both CoopVec
//and CoopMat need it.

//Matrix (M*K) and vector (K / M) buffer elements as struct-of-array, so the ops get StorageBuffer array pointers.
struct OxCCoopVecMat4x4F16 {
	float16_t data[16];
};

struct OxCCoopVecVec4F16 {
	float16_t data[4];
};

//Int32 vector buffer (input activations / output accumulator) for the INT8 quantized path.
struct OxCCoopVecVec4I32 {
	int data[4];
};

//Matrix source: a ByteAddressBuffer on DXIL (what dx::linalg loads from); on SPIR-V a RWStructuredBuffer struct-of-
//array, so the op gets a StorageBuffer pointer whose pointee is an array (a read-only SRV is a Uniform/BufferBlock
//buffer the op rejects; a helper-passed buffer would need a Function-class pointer Logical addressing rejects).
#ifdef __spirv__
	#define OXC_COOPVEC_MATRIX_BUFFER(name) RWStructuredBuffer<OxCCoopVecMat4x4F16> name
#else
	#define OXC_COOPVEC_MATRIX_BUFFER(name) ByteAddressBuffer name
#endif

#define OXC_COOPVEC_VECTOR_BUFFER(name) RWStructuredBuffer<OxCCoopVecVec4F16> name
#define OXC_COOPVEC_VECTOR_BUFFER_I32(name) RWStructuredBuffer<OxCCoopVecVec4I32> name

//Training gradient targets are accumulated in place (atomic add), so they're writable. On DXIL the accumulate ops
//take a RWByteAddressBuffer; on SPIR-V the op needs a storage-buffer array pointer (struct-of-array).
#ifdef __spirv__
	#define OXC_COOPVEC_RWMATRIX_BUFFER(name) RWStructuredBuffer<OxCCoopVecMat4x4F16> name
	#define OXC_COOPVEC_RWVECTOR_BUFFER(name) RWStructuredBuffer<OxCCoopVecVec4F16> name
#else
	#define OXC_COOPVEC_RWMATRIX_BUFFER(name) RWByteAddressBuffer name
	#define OXC_COOPVEC_RWVECTOR_BUFFER(name) RWByteAddressBuffer name
#endif

namespace oxc {

#ifdef __spirv__

	//A cooperative vector is an opaque type (OpTypeCooperativeVectorNV): component type + count. The matmul's input
	//and result are cooperative vectors, loaded from / stored to storage-buffer array pointers.
	typedef vk::SpirvOpaqueType</* OpTypeCooperativeVectorNV */ 5288, float16_t, vk::integral_constant<uint, 4> > CoopVecF16x4;

	//OpCooperativeVectorLoadNV: Pointer, Offset.
	[[vk::ext_capability(/* CooperativeVectorNV */ 5394)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_NV_cooperative_vector")]]
	[[vk::ext_instruction(/* OpCooperativeVectorLoadNV */ 5302)]]
	CoopVecF16x4 __coopVecLoadF16x4([[vk::ext_reference]] float16_t src[4], uint byteOffset);

	//OpCooperativeVectorMatrixMulNV: Input, InputInterpretation, Matrix, MatrixOffset, MatrixInterpretation, M, K,
	//MemoryLayout, Transpose, MatrixStride. Interpretations / M / K / layout / transpose are compile-time constants.
	[[vk::ext_capability(/* CooperativeVectorNV */ 5394)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_NV_cooperative_vector")]]
	[[vk::ext_instruction(/* OpCooperativeVectorMatrixMulNV */ 5289)]]
	CoopVecF16x4 __coopVecMatMul4x4F16(
		CoopVecF16x4 input,
		uint inputInterpretation,
		[[vk::ext_reference]] float16_t matrix[16],
		uint matrixOffset,
		uint matrixInterpretation,
		uint m,
		uint k,
		uint memoryLayout,
		bool transpose,
		uint matrixStride
	);

	//OpCooperativeVectorMatrixMulAddNV: as above, with Bias, BiasOffset, BiasInterpretation after MatrixInterpretation.
	//This is the fused y = W*x + b (the LLM MLP / projection layer).
	[[vk::ext_capability(/* CooperativeVectorNV */ 5394)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_NV_cooperative_vector")]]
	[[vk::ext_instruction(/* OpCooperativeVectorMatrixMulAddNV */ 5292)]]
	CoopVecF16x4 __coopVecMatMulAdd4x4F16(
		CoopVecF16x4 input,
		uint inputInterpretation,
		[[vk::ext_reference]] float16_t matrix[16],
		uint matrixOffset,
		uint matrixInterpretation,
		[[vk::ext_reference]] float16_t bias[4],
		uint biasOffset,
		uint biasInterpretation,
		uint m,
		uint k,
		uint memoryLayout,
		bool transpose,
		uint matrixStride
	);

	//OpCooperativeVectorStoreNV: Pointer, Offset, Object.
	[[vk::ext_capability(/* CooperativeVectorNV */ 5394)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_NV_cooperative_vector")]]
	[[vk::ext_instruction(/* OpCooperativeVectorStoreNV */ 5303)]]
	void __coopVecStoreF16x4([[vk::ext_reference]] float16_t dst[4], uint byteOffset, CoopVecF16x4 obj);

	//INT8 quantized (int8 weights * int8 activations -> int32 accumulate). Input/result are int32 cooperative vectors;
	//the SignedInt8 interpretation converts the int32 activations to int8 and reads the matrix bytes as int8. The
	//matrix pointer's pointee type is ignored by the op, so the F16 matrix buffer is reused to hold the int8 bytes.
	typedef vk::SpirvOpaqueType</* OpTypeCooperativeVectorNV */ 5288, int, vk::integral_constant<uint, 4> > CoopVecI32x4;

	[[vk::ext_capability(/* CooperativeVectorNV */ 5394)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_NV_cooperative_vector")]]
	[[vk::ext_instruction(/* OpCooperativeVectorLoadNV */ 5302)]]
	CoopVecI32x4 __coopVecLoadI32x4([[vk::ext_reference]] int src[4], uint byteOffset);

	[[vk::ext_capability(/* CooperativeVectorNV */ 5394)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_NV_cooperative_vector")]]
	[[vk::ext_instruction(/* OpCooperativeVectorMatrixMulNV */ 5289)]]
	CoopVecI32x4 __coopVecMatMulI8toI32(
		CoopVecI32x4 input,
		uint inputInterpretation,
		[[vk::ext_reference]] float16_t matrix[16],
		uint matrixOffset,
		uint matrixInterpretation,
		uint m,
		uint k,
		uint memoryLayout,
		bool transpose,
		uint matrixStride
	);

	[[vk::ext_capability(/* CooperativeVectorNV */ 5394)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_NV_cooperative_vector")]]
	[[vk::ext_instruction(/* OpCooperativeVectorStoreNV */ 5303)]]
	void __coopVecStoreI32x4([[vk::ext_reference]] int dst[4], uint byteOffset, CoopVecI32x4 obj);

	//Training (CooperativeVectorTrainingNV): weight-gradient outer-product and bias-gradient reduce-sum, both atomically
	//accumulated into memory. OpCooperativeVectorOuterProductAccumulateNV: Pointer, Offset, A, B, MemoryLayout,
	//MatrixInterpretation, MatrixStride. OpCooperativeVectorReduceSumAccumulateNV: Pointer, Offset, V.
	[[vk::ext_capability(/* CooperativeVectorNV */ 5394)]]
	[[vk::ext_capability(/* CooperativeVectorTrainingNV */ 5435)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_NV_cooperative_vector")]]
	[[vk::ext_instruction(/* OpCooperativeVectorOuterProductAccumulateNV */ 5290)]]
	void __coopVecOuterProductAccumulate4x4F16([[vk::ext_reference]] float16_t matrix[16], uint offset, CoopVecF16x4 a, CoopVecF16x4 b, uint memoryLayout, uint matrixInterpretation, uint matrixStride);

	[[vk::ext_capability(/* CooperativeVectorNV */ 5394)]]
	[[vk::ext_capability(/* CooperativeVectorTrainingNV */ 5435)]]
	[[vk::ext_capability(/* VulkanMemoryModel */ 5345)]]
	[[vk::ext_extension("SPV_NV_cooperative_vector")]]
	[[vk::ext_instruction(/* OpCooperativeVectorReduceSumAccumulateNV */ 5291)]]
	void __coopVecReduceSumAccumulate4F16([[vk::ext_reference]] float16_t dst[4], uint offset, CoopVecF16x4 v);

#else

	//4x4 F16 matrix * F16 vector, column-major, from matBuf at byte offset/stride.
	vector<half, 4> CoopVecMatVecMul4x4F16(
		ByteAddressBuffer matBuf, uint offset, uint stride, vector<half, 4> input
	) {
		using namespace dx::linalg;
		using MatrixT = Matrix<ComponentType::F16, 4, 4, MatrixUse::A, MatrixScope::Thread>;
		MatrixT mat = MatrixT::Load<MatrixLayoutEnum::ColMajor>(matBuf, offset, stride);
		return Multiply<half>(mat, input);
	}

	//4x4 F16 matrix * F16 vector + F16 bias (y = W*x + b).
	vector<half, 4> CoopVecMatVecMulAdd4x4F16(
		ByteAddressBuffer matBuf, uint offset, uint stride, vector<half, 4> input, vector<half, 4> bias
	) {
		using namespace dx::linalg;
		using MatrixT = Matrix<ComponentType::F16, 4, 4, MatrixUse::A, MatrixScope::Thread>;
		MatrixT mat = MatrixT::Load<MatrixLayoutEnum::ColMajor>(matBuf, offset, stride);
		return MultiplyAdd<half>(mat, input, bias);
	}

	//Quantized: 4x4 FP8 (e4m3) weight matrix * F16 vector -> F16 (FP8-weight inference). Column-major, 1 byte/element.
	vector<half, 4> CoopVecMatVecMulFP8W4x4F16(
		ByteAddressBuffer matBuf, uint offset, vector<half, 4> input
	) {
		using namespace dx::linalg;
		using MatrixT = Matrix<ComponentType::F8_E4M3FN, 4, 4, MatrixUse::A, MatrixScope::Thread>;
		MatrixT mat = MatrixT::Load<MatrixLayoutEnum::ColMajor>(matBuf, offset, /* byte stride = 4 rows * 1 byte */ 4);
		return Multiply<half>(mat, input);
	}

	//Quantized: 4x4 FP8 (e5m2) weight matrix * F16 vector -> F16. Column-major, 1 byte/element.
	vector<half, 4> CoopVecMatVecMulFP8E5M2W4x4F16(
		ByteAddressBuffer matBuf, uint offset, vector<half, 4> input
	) {
		using namespace dx::linalg;
		using MatrixT = Matrix<ComponentType::F8_E5M2, 4, 4, MatrixUse::A, MatrixScope::Thread>;
		MatrixT mat = MatrixT::Load<MatrixLayoutEnum::ColMajor>(matBuf, offset, /* byte stride = 4 rows * 1 byte */ 4);
		return Multiply<half>(mat, input);
	}

	//Quantized: 4x4 INT8 weight matrix * INT8 activations -> INT32 accumulate. Column-major, 1 byte/element.
	vector<int, 4> CoopVecMatVecMulI8(
		ByteAddressBuffer matBuf, uint offset, vector<int, 4> input
	) {
		using namespace dx::linalg;
		using MatrixT = Matrix<ComponentType::I8, 4, 4, MatrixUse::A, MatrixScope::Thread>;
		MatrixT mat = MatrixT::Load<MatrixLayoutEnum::ColMajor>(matBuf, offset, /* byte stride = 4 rows * 1 byte */ 4);
		//I8 is a packed component type (4 per scalar); Convert packs the 4 int32 activations into the I8 interpretation.
		return Multiply<int>(mat, Convert<ComponentType::I8, ComponentType::I32>(input));
	}

	//Training: weight gradient - accumulate the outer product a (x) b into the 4x4 F16 matrix at matBuf/offset.
	void CoopVecOuterProductAccumulate4x4F16(vector<half, 4> a, vector<half, 4> b, RWByteAddressBuffer matBuf, uint offset) {
		using namespace dx::linalg;
		Matrix<ComponentType::F16, 4, 4, MatrixUse::Accumulator, MatrixScope::Thread> grad = OuterProduct<ComponentType::F16>(a, b);
		grad.InterlockedAccumulate(matBuf, offset);
	}

	//Training: bias gradient - atomically accumulate the vector v into the buffer at offset.
	void CoopVecReduceSumAccumulate4F16(vector<half, 4> v, RWByteAddressBuffer buf, uint offset) {
		using namespace dx::linalg;
		InterlockedAccumulate(buf, offset, v);
	}

#endif

}
//Public API: (4x4 F16 matrix from matBuf at byte offset/stride) * (the F16 vector in ioBuf), result written to ioBuf.
//Float16NV = 0 (interpretations), ColumnMajorNV = 1 (layout), transpose = false.
#ifdef __spirv__
	#define OXC_COOPVEC_MATVEC_4X4_F16(matBuf, offset, stride, ioBuf) { oxc::CoopVecF16x4 __oxcIn = oxc::__coopVecLoadF16x4((ioBuf)[0].data, 0); oxc::CoopVecF16x4 __oxcOut = oxc::__coopVecMatMul4x4F16(__oxcIn, 0, (matBuf)[0].data, (offset), 0, 4, 4, 1, false, (stride)); oxc::__coopVecStoreF16x4((ioBuf)[0].data, 0, __oxcOut); }
#else
	#define OXC_COOPVEC_MATVEC_4X4_F16(matBuf, offset, stride, ioBuf) { vector<half, 4> __oxcIn = { (ioBuf)[0].data[0], (ioBuf)[0].data[1], (ioBuf)[0].data[2], (ioBuf)[0].data[3] }; vector<half, 4> __oxcOut = oxc::CoopVecMatVecMul4x4F16((matBuf), (offset), (stride), __oxcIn); (ioBuf)[0].data[0] = __oxcOut[0]; (ioBuf)[0].data[1] = __oxcOut[1]; (ioBuf)[0].data[2] = __oxcOut[2]; (ioBuf)[0].data[3] = __oxcOut[3]; }
#endif

//Public API: fused y = W*x + b - (4x4 F16 matrix from matBuf) * (F16 vector in ioBuf) + (F16 bias in biasBuf) -> ioBuf.
#ifdef __spirv__
	#define OXC_COOPVEC_MATVEC_BIAS_4X4_F16(matBuf, offset, stride, biasBuf, ioBuf) { oxc::CoopVecF16x4 __oxcIn = oxc::__coopVecLoadF16x4((ioBuf)[0].data, 0); oxc::CoopVecF16x4 __oxcOut = oxc::__coopVecMatMulAdd4x4F16(__oxcIn, 0, (matBuf)[0].data, (offset), 0, (biasBuf)[0].data, 0, 0, 4, 4, 1, false, (stride)); oxc::__coopVecStoreF16x4((ioBuf)[0].data, 0, __oxcOut); }
#else
	#define OXC_COOPVEC_MATVEC_BIAS_4X4_F16(matBuf, offset, stride, biasBuf, ioBuf) { vector<half, 4> __oxcIn = { (ioBuf)[0].data[0], (ioBuf)[0].data[1], (ioBuf)[0].data[2], (ioBuf)[0].data[3] }; vector<half, 4> __oxcBias = { (biasBuf)[0].data[0], (biasBuf)[0].data[1], (biasBuf)[0].data[2], (biasBuf)[0].data[3] }; vector<half, 4> __oxcOut = oxc::CoopVecMatVecMulAdd4x4F16((matBuf), (offset), (stride), __oxcIn, __oxcBias); (ioBuf)[0].data[0] = __oxcOut[0]; (ioBuf)[0].data[1] = __oxcOut[1]; (ioBuf)[0].data[2] = __oxcOut[2]; (ioBuf)[0].data[3] = __oxcOut[3]; }
#endif

//Public API (quantized): (4x4 FP8 e4m3 weight matrix from matBuf) * (F16 vector in ioBuf) -> ioBuf. FP8-weight inference.
//SPIR-V reuses the F16 matmul op with MatrixInterpretation = FloatE4M3NV (1000491002); the matrix pointee type is
//ignored by the op, so the raw FP8 bytes are read via a 1-byte-per-element column stride of 4.
#ifdef __spirv__
	#define OXC_COOPVEC_MATVEC_FP8W_4X4_F16(matBuf, offset, ioBuf) { oxc::CoopVecF16x4 __oxcIn = oxc::__coopVecLoadF16x4((ioBuf)[0].data, 0); oxc::CoopVecF16x4 __oxcOut = oxc::__coopVecMatMul4x4F16(__oxcIn, 0, (matBuf)[0].data, (offset), /* FloatE4M3NV */ 1000491002, 4, 4, 1, false, 4); oxc::__coopVecStoreF16x4((ioBuf)[0].data, 0, __oxcOut); }
#else
	#define OXC_COOPVEC_MATVEC_FP8W_4X4_F16(matBuf, offset, ioBuf) { vector<half, 4> __oxcIn = { (ioBuf)[0].data[0], (ioBuf)[0].data[1], (ioBuf)[0].data[2], (ioBuf)[0].data[3] }; vector<half, 4> __oxcOut = oxc::CoopVecMatVecMulFP8W4x4F16((matBuf), (offset), __oxcIn); (ioBuf)[0].data[0] = __oxcOut[0]; (ioBuf)[0].data[1] = __oxcOut[1]; (ioBuf)[0].data[2] = __oxcOut[2]; (ioBuf)[0].data[3] = __oxcOut[3]; }
#endif

//Public API (quantized): as above but FP8 e5m2 weights (FloatE5M2NV = 1000491003 interpretation).
#ifdef __spirv__
	#define OXC_COOPVEC_MATVEC_FP8E5M2W_4X4_F16(matBuf, offset, ioBuf) { oxc::CoopVecF16x4 __oxcIn = oxc::__coopVecLoadF16x4((ioBuf)[0].data, 0); oxc::CoopVecF16x4 __oxcOut = oxc::__coopVecMatMul4x4F16(__oxcIn, 0, (matBuf)[0].data, (offset), /* FloatE5M2NV */ 1000491003, 4, 4, 1, false, 4); oxc::__coopVecStoreF16x4((ioBuf)[0].data, 0, __oxcOut); }
#else
	#define OXC_COOPVEC_MATVEC_FP8E5M2W_4X4_F16(matBuf, offset, ioBuf) { vector<half, 4> __oxcIn = { (ioBuf)[0].data[0], (ioBuf)[0].data[1], (ioBuf)[0].data[2], (ioBuf)[0].data[3] }; vector<half, 4> __oxcOut = oxc::CoopVecMatVecMulFP8E5M2W4x4F16((matBuf), (offset), __oxcIn); (ioBuf)[0].data[0] = __oxcOut[0]; (ioBuf)[0].data[1] = __oxcOut[1]; (ioBuf)[0].data[2] = __oxcOut[2]; (ioBuf)[0].data[3] = __oxcOut[3]; }
#endif

//Public API (quantized): (4x4 INT8 weight matrix from matBuf) * (INT8 activations in ioBuf) -> INT32 accumulate in ioBuf.
//SignedInt8NV = 3 (input + matrix interpretation); ioBuf is an int32 vector buffer (OXC_COOPVEC_VECTOR_BUFFER_I32).
#ifdef __spirv__
	#define OXC_COOPVEC_MATVEC_I8(matBuf, offset, ioBuf) { oxc::CoopVecI32x4 __oxcIn = oxc::__coopVecLoadI32x4((ioBuf)[0].data, 0); oxc::CoopVecI32x4 __oxcOut = oxc::__coopVecMatMulI8toI32(__oxcIn, /* SignedInt8NV */ 3, (matBuf)[0].data, (offset), /* SignedInt8NV */ 3, 4, 4, 1, false, 4); oxc::__coopVecStoreI32x4((ioBuf)[0].data, 0, __oxcOut); }
#else
	#define OXC_COOPVEC_MATVEC_I8(matBuf, offset, ioBuf) { vector<int, 4> __oxcIn = { (ioBuf)[0].data[0], (ioBuf)[0].data[1], (ioBuf)[0].data[2], (ioBuf)[0].data[3] }; vector<int, 4> __oxcOut = oxc::CoopVecMatVecMulI8((matBuf), (offset), __oxcIn); (ioBuf)[0].data[0] = __oxcOut[0]; (ioBuf)[0].data[1] = __oxcOut[1]; (ioBuf)[0].data[2] = __oxcOut[2]; (ioBuf)[0].data[3] = __oxcOut[3]; }
#endif

//Training API: weight-gradient outer-product accumulate - gradW (4x4 F16) += outer(a, b), a/b loaded from vecBuf[0].
#ifdef __spirv__
	#define OXC_COOPVEC_OUTER_PRODUCT_ACCUMULATE_4X4_F16(gradW, vecBuf) { oxc::CoopVecF16x4 __oxcA = oxc::__coopVecLoadF16x4((vecBuf)[0].data, 0); oxc::__coopVecOuterProductAccumulate4x4F16((gradW)[0].data, 0, __oxcA, __oxcA, /* RowMajorNV */ 0, /* Float16NV */ 0, 8); }
#else
	#define OXC_COOPVEC_OUTER_PRODUCT_ACCUMULATE_4X4_F16(gradW, vecBuf) { vector<half, 4> __oxcA = { (vecBuf)[0].data[0], (vecBuf)[0].data[1], (vecBuf)[0].data[2], (vecBuf)[0].data[3] }; oxc::CoopVecOuterProductAccumulate4x4F16(__oxcA, __oxcA, (gradW), 0); }
#endif

//Training API: bias-gradient reduce-sum accumulate - gradB (4 F16) += the vector loaded from vecBuf[0].
#ifdef __spirv__
	#define OXC_COOPVEC_REDUCE_SUM_ACCUMULATE_4_F16(gradB, vecBuf) { oxc::CoopVecF16x4 __oxcV = oxc::__coopVecLoadF16x4((vecBuf)[0].data, 0); oxc::__coopVecReduceSumAccumulate4F16((gradB)[0].data, 0, __oxcV); }
#else
	#define OXC_COOPVEC_REDUCE_SUM_ACCUMULATE_4_F16(gradB, vecBuf) { vector<half, 4> __oxcV = { (vecBuf)[0].data[0], (vecBuf)[0].data[1], (vecBuf)[0].data[2], (vecBuf)[0].data[3] }; oxc::CoopVecReduceSumAccumulate4F16(__oxcV, (gradB), 0); }
#endif
