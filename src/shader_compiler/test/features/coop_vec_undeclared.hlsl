//coop_vec.hlsl's matrix vector multiply without declaring CoopVec or CoopMat.
//DXIL reports the linear algebra it uses, so this has to fail to compile.

#include <dx/linalg.h>

ByteAddressBuffer matBuf;
RWStructuredBuffer<half4> ioBuf;

[[oxc::extension("16BitTypes")]]
[[oxc::model("6.10")]]
[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main() {
	using namespace dx::linalg;
	using MatrixT = Matrix<ComponentType::F16, 4, 4, MatrixUse::A, MatrixScope::Thread>;
	MatrixT mat = MatrixT::Load<MatrixLayoutEnum::ColMajor>(matBuf, 0, 8);
	ioBuf[0] = Multiply<half>(mat, ioBuf[0]);
}
