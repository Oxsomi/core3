RWStructuredBuffer<uint> buf;

[[oxc::extension("SubgroupShuffle")]]
[[oxc::model("6.5")]]
[[oxc::stage("compute")]]
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
	uint v = buf[id.x];
	buf[id.x] = WaveReadLaneAt(v, (id.x + 1) & 63);   //pure subgroup shuffle -> OpGroupNonUniformShuffle
}
