RWStructuredBuffer<uint> buf;

//SubgroupOperations rides along for the DXIL variant: any wave intrinsic sets the one generic WAVE_OPS
// reflection flag, which maps to SubgroupOperations and has to be declared for processDXIL to accept it.

[[oxc::extension("SubgroupShuffle", "SubgroupOperations")]]
[[oxc::model("6.5")]]
[[oxc::stage("compute")]]
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
	uint v = buf[id.x];
	buf[id.x] = WaveReadLaneAt(v, (id.x + 1) & 63);   //pure subgroup shuffle -> OpGroupNonUniformShuffle
}
