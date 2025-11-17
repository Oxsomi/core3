#include "@resources.hlsli"

RWStructuredBuffer<F32> _test;

struct Test1
{
    F32 _a;
    F32 _b;
    F32 _c;
    F32 _d;
};

struct Test {
    Test1 _test1;
	U32 _id;
};

StructuredBuffer<Test> _buf;

[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void mainCompute() {
    _test[0] = _buf[0]._test1._a * _buf[0]._test1._b + _buf[0]._test1._c / _buf[0]._test1._d;
}

