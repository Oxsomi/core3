#include "@extensions.hlsli"

RWStructuredBuffer<double> buf;

[[oxc::extension("F64", "AtomicF64")]]
[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main(uint id : SV_DispatchThreadID) {
	oxc::AtomicAddF64(buf[0], /* Device */ 1u, /* Relaxed */ 0u, buf[id]);
}
