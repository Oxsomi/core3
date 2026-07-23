RWStructuredBuffer<float16_t> buf;
[[oxc::extension("16BitTypes")]]
[[oxc::stage("compute")]]
[numthreads(1,1,1)]
void main(uint id : SV_DispatchThreadID) { buf[id] = buf[id] + (float16_t)1; }
