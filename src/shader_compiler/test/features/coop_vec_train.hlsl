#include "@extensions.hlsli"

OXC_COOPVEC_VECTOR_BUFFER(vecBuf);       //activations (a / b / v)
OXC_COOPVEC_RWMATRIX_BUFFER(gradW);      //weight gradient (outer-product accumulate target)
OXC_COOPVEC_RWVECTOR_BUFFER(gradB);      //bias gradient (reduce-sum accumulate target)

//SM6.10 cooperative-vector training: the backward-pass accumulate ops (weight gradient via outer product, bias
//gradient via reduce-sum). DXIL uses dx::linalg OuterProduct + InterlockedAccumulate; SPIR-V uses the NV training
//ops (5290/5291, CooperativeVectorTrainingNV). Gated behind CoopVecTraining (a distinct device tier, D3D12 Tier 1.1).
[[oxc::extension("CoopVec", "CoopVecTraining", "16BitTypes")]]
[[oxc::model("6.10")]]
[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main(uint id : SV_DispatchThreadID) {
	OXC_COOPVEC_OUTER_PRODUCT_ACCUMULATE_4X4_F16(gradW, vecBuf);
	OXC_COOPVEC_REDUCE_SUM_ACCUMULATE_4_F16(gradB, vecBuf);
}
