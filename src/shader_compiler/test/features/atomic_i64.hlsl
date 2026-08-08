RWStructuredBuffer<uint64_t> buf;
[[oxc::extension("I64", "AtomicI64")]]
[[oxc::model("6.6")]]
[[oxc::stage("compute")]]
[numthreads(1,1,1)]
void main(uint id : SV_DispatchThreadID) {
	uint64_t prev;
	InterlockedAdd(buf[0], (uint64_t)(id + 1), prev);
	buf[id + 1] = prev;
}
