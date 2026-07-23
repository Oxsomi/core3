RWStructuredBuffer<uint> buf;
[[oxc::extension("SubgroupArithmetic")]]
[[oxc::stage("compute")]]
[numthreads(64,1,1)]
void main(uint id : SV_DispatchThreadID) { buf[id] = WaveActiveSum(id + 1); }
