#include "@extensions.hlsli"

OXC_COOPVEC_MATRIX_BUFFER(matBuf);
OXC_COOPVEC_VECTOR_BUFFER_I32(ioBuf);

//SM6.10 cooperative vector, quantized: a 4x4 INT8 weight matrix * INT8 activations -> INT32 (INT8 LLM inference).
//DXIL uses dx::linalg Matrix<I8> + MakeInterpretedVector<I8>; SPIR-V uses the NV matmul with SignedInt8 interpretation.
[[oxc::extension("CoopVec", "16BitTypes")]]
[[oxc::model("6.10")]]
[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main(uint id : SV_DispatchThreadID) {
	OXC_COOPVEC_MATVEC_I8(matBuf, 0, ioBuf);
}
