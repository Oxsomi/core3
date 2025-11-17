#include "@resources.hlsli"

struct PushConstant {
	U32x2 dims;
};

PUSH_CONSTANT PushConstant _pc;

void testFunction(inout uint a, out uint b, in uint c) {
	b = a | c;
}

[[oxc::stage("vertex")]]
F32x4 mainVS(F32x2 uv : TEXCOORD0, nointerpolation uint test : KAAS) : SV_POSITION {
	return F32x4(uv * 2 - 1, 0, 1);
}

[[oxc::stage("pixel")]]
F32x4 mainPS(F32x4 pos : SV_POSITION) : SV_TARGET {
	return F32x4(pos.xy / _pc.dims, 0, 1);
}

