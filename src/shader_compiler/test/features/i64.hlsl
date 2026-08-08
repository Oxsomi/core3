RWStructuredBuffer<uint64_t> buf;
[[oxc::extension("I64")]]
[[oxc::stage("compute")]]
[numthreads(1,1,1)]
void main(uint id : SV_DispatchThreadID) { buf[id] = buf[id] * 3ull + 1ull; }
