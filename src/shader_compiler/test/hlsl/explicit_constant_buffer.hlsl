#include "@resources.hlsli"

RWStructuredBuffer<F32> _test;

struct Test {
	F32 _a;
	F32 _b;
	F32 _c;
	F32 _d;
	U32 _id;
};

ConstantBuffer<Test> _buf;

[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void mainCompute() {
	_test[0] = _buf._a * _buf._b + _buf._c / _buf._d;
}

