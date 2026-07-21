#include "@resources.hlsli"

RWStructuredBuffer<F32> _test;

cbuffer test {
	F32 _a;
	F32 _b;
	F32 _c;
	F32 _d;
	U32 _id;
};

[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void mainCompute() {
	_test[_id] = _a * _b + _c / _d;
}

