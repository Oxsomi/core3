#include "@extensions.hlsli"

//The matrix source differs per backend (ByteAddressBuffer on DXIL, a struct-of-array storage buffer on SPIR-V), hidden.
OXC_COOPVEC_MATRIX_BUFFER(matBuf);
OXC_COOPVEC_VECTOR_BUFFER(ioBuf);

//SM6.10 cooperative vector: a 4x4 F16 matrix from matBuf times the F16 vector in ioBuf, written back (a per-token matvec).
//DXIL lowers to __builtin_MatVecMul; SPIR-V to inline SPV_NV_cooperative_vector load / matmul / store. Both behind oxc::.
[[oxc::extension("CoopVec", "16BitTypes")]]
[[oxc::model("6.10")]]
[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main(uint id : SV_DispatchThreadID) {
	OXC_COOPVEC_MATVEC_4X4_F16(matBuf, 0, 8, ioBuf);
}
