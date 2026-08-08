#include "@extensions.hlsli"

OXC_COOPMAT_INPUT_BUFFER_FP8(bufA);
OXC_COOPMAT_INPUT_BUFFER_FP8(bufB);
OXC_COOPMAT_OUTPUT_BUFFER(bufOut);

//SM6.10 cooperative matrix, quantized: a 16x16x16 FP8 (e4m3) GEMM with F16 accumulator (cross-vendor FP8 GEMM).
//DXIL uses dx::linalg Matrix<F8_E4M3FN, Wave>; SPIR-V uses SPV_KHR_cooperative_matrix + SPV_EXT_float8 (Float8CooperativeMatrixEXT).
[[oxc::extension("CoopMat", "CoopFP8", "16BitTypes")]]
[[oxc::model("6.10")]]
[[oxc::stage("compute")]]
[numthreads(32, 1, 1)]
void main() {
	OXC_COOPMAT_GEMM_FP8_16(bufA, bufB, bufOut);
}
