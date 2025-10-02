#include "@resources.hlsli"

RWStructuredBuffer<F32> _test;

struct Test {
	F32 _a;
	F32 _b;
	F32 _c;
	F32 _d;
	U32 _id;
};

typedef StructuredBuffer<F32> AnnoyingSBuf;
typedef ConstantBuffer<Test> AnnoyingBuf;

ConstantBuffer<Test> _buf;
StructuredBuffer<F32> _sbuf;
AnnoyingBuf _buf0;
AnnoyingSBuf _sbuf0;

struct A {
	ConstantBuffer<Test> buf;
	StructuredBuffer<F32> sbuf;
	AnnoyingBuf buf0;
	AnnoyingSBuf sbuf0;
};

F32 getValue(ConstantBuffer<Test> buf, A a) {
	return buf._a * a.buf._b  * a.buf0._b * a.sbuf[0] * a.sbuf0[0];
}

[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void mainCompute() {
	A a;
	a.buf = _buf;
	a.sbuf = _sbuf;
	a.buf0 = _buf0;
	a.sbuf0 = _sbuf0;
	_test[0] = getValue(_buf, a);
}
