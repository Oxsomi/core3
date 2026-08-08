#include "@extensions.hlsli"

//Buffers differ per backend (ByteAddressBuffer on DXIL, StructuredBuffer<float16_t> on SPIR-V), hidden by the macros.
OXC_COOPMAT_INPUT_BUFFER(bufA);
OXC_COOPMAT_INPUT_BUFFER(bufB);
OXC_COOPMAT_OUTPUT_BUFFER(bufOut);

//SM6.10 cooperative matrix: a 16x16x16 F16 GEMM (D = A*B), the batched attention/FFN op. Cross-vendor.
//DXIL lowers dx::linalg Wave-scope Multiply; SPIR-V uses SPV_KHR_cooperative_matrix (load / mul-add / store). Both behind oxc::.
[[oxc::extension("CoopMat", "16BitTypes")]]
[[oxc::model("6.10")]]
[[oxc::stage("compute")]]
[numthreads(32, 1, 1)]
void main() {
	OXC_COOPMAT_GEMM_16_F16(bufA, bufB, bufOut);
}
