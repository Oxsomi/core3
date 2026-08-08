#include "@resources.hlsli"

struct Test1 {
    nointerpolation F32 a : COLOR0;
};

[[oxc::stage("vertex")]]
F32x4 mainVS(Test1 test1, nointerpolation F32x2 uv : TEXCOORD0) : SV_POSITION {
	return F32x4(test1.a, uv, 1);
}
