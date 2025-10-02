#include "@types.hlsli"

struct Test1 {

    static const F32 a = 0;

    F32 _a;
    F32 _b;
    F32 _c;
    F32 _d;
};

static const F32 a = 0;

[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void mainCompute() { }

