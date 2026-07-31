#include "@extensions.hlsli"

OXC_COOPVEC_MATRIX_BUFFER(matBuf);
OXC_COOPVEC_VECTOR_BUFFER(biasBuf);
OXC_COOPVEC_VECTOR_BUFFER(ioBuf);

//SM6.10 cooperative vector fused y = W*x + b (the LLM MLP / projection layer).
//DXIL lowers to __builtin_MatVecMulAdd; SPIR-V to inline OpCooperativeVectorMatrixMulAddNV (5292). Both behind oxc::.
[[oxc::extension("CoopVec", "16BitTypes")]]
[[oxc::model("6.10")]]
[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main(uint id : SV_DispatchThreadID) {
	OXC_COOPVEC_MATVEC_BIAS_4X4_F16(matBuf, 0, 8, biasBuf, ioBuf);
}
