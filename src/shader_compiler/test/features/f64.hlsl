RWStructuredBuffer<double> buf;
[[oxc::extension("F64")]]
[[oxc::stage("compute")]]
[numthreads(1,1,1)]
void main(uint id : SV_DispatchThreadID) { buf[id] = buf[id] * buf[id] + buf[id]; }
