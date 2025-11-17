#include "@resources.hlsli"

struct PushConstant {
	U32x2 dims;
};

PUSH_CONSTANT PushConstant _pc;

UNKNOWN_FORMAT RWTexture2D<F32x4> _test[16][32];

[[oxc::stage("compute")]]
[numthreads(8, 8, 1)]
void mainCompute(U32x2 id : SV_DispatchThreadID) {

	if(any(id >= _pc.dims))
		return;

	_test[0][0][id] = F32x4(F32x2(id) / _pc.dims, 0, 1);
}

