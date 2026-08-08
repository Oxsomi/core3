#include "@extensions.hlsli"

OXC_COOPVEC_MATRIX_BUFFER(matBuf);
OXC_COOPVEC_VECTOR_BUFFER(ioBuf);

//SM6.10 cooperative vector, quantized: a 4x4 FP8 (e5m2) weight matrix * F16 vector -> F16.
[[oxc::extension("CoopVec", "CoopFP8", "16BitTypes")]]
[[oxc::model("6.10")]]
[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main(uint id : SV_DispatchThreadID) {
	OXC_COOPVEC_MATVEC_FP8E5M2W_4X4_F16(matBuf, 0, ioBuf);
}
