#include "@extensions.hlsli"

RWStructuredBuffer<float> buf;

[[oxc::extension("AtomicF32")]]
[[oxc::model("6.5")]]
[[oxc::stage("compute")]]
[numthreads(64, 1, 1)]
void main(uint id : SV_DispatchThreadID) {
	buf[id + 1] = oxc::AtomicAddF32(buf[0], /* Device */ 1, /* Relaxed */ 0, (float)(id + 1));
}
