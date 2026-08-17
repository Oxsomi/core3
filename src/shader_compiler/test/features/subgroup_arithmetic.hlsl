RWStructuredBuffer<uint> buf;

//SubgroupOperations rides along for the DXIL variant: any wave intrinsic sets the one generic WAVE_OPS
// reflection flag, which maps to SubgroupOperations and has to be declared for processDXIL to accept it.

[[oxc::extension("SubgroupArithmetic", "SubgroupOperations")]]
[[oxc::stage("compute")]]
[numthreads(64,1,1)]
void main(uint id : SV_DispatchThreadID) { buf[id] = WaveActiveSum(id + 1); }
