#include "@resources.hlsli"

struct PushConstant {
	U32x2 dims;
};

PUSH_CONSTANT PushConstant _pc;

[[oxc::stage("vertex")]]
F32x4 mainVS(F32x2 uv : TEXCOORD0) : SV_POSITION {
	return F32x4(uv * 2 - 1, 0, 1);
}

[shader("vertex")]
F32x4 mainVS1(F32x2 uv : TEXCOORD0) : SV_POSITION {
	return F32x4(uv * 2 - 1, 0, 1);
}

[[oxc::stage("pixel")]]
F32x4 mainPS(F32x4 pos : SV_POSITION) : SV_TARGET {
	return F32x4(pos.xy / _pc.dims, 0, 1);
}

[shader("pixel")]
F32x4 mainPS1(F32x4 pos : SV_POSITION) : SV_TARGET {
	return F32x4(pos.xy / _pc.dims, 0, 1);
}

UNKNOWN_FORMAT RWTexture2D<F32x4> _test;

[[oxc::stage("compute")]]
[numthreads(8, 8, 1)]
void mainCompute(U32x2 id : SV_DispatchThreadID) {

	if(any(id >= _pc.dims))
		return;

	_test[id] = F32x4(F32x2(id) / _pc.dims, 0, 1);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void mainCompute1(U32x2 id : SV_DispatchThreadID) {

	if(any(id >= _pc.dims))
		return;

	_test[id] = F32x4(F32x2(id) / _pc.dims, 0, 1);
}

