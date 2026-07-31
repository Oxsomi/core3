#include "@extensions.hlsli"

OXC_COOPVEC_MATRIX_BUFFER(matBuf);
OXC_COOPVEC_VECTOR_BUFFER(ioBuf);

//SM6.10 cooperative vector, quantized: a 4x4 FP8 (e4m3) weight matrix * F16 vector -> F16 (FP8-weight LLM inference).
//DXIL uses dx::linalg Matrix<F8_E4M3FN>; SPIR-V uses the NV matmul with the FloatE4M3NV matrix interpretation.
//CoopFP8 gates the FP8 tier (not universally supported); CoopVec is the base (its capability carries the op).
[[oxc::extension("CoopVec", "CoopFP8", "16BitTypes")]]
[[oxc::model("6.10")]]
[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main(uint id : SV_DispatchThreadID) {
	OXC_COOPVEC_MATVEC_FP8W_4X4_F16(matBuf, 0, ioBuf);
}
