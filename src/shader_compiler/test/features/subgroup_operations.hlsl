RWStructuredBuffer<uint> buf;
[[oxc::extension("SubgroupOperations")]]
[[oxc::stage("compute")]]
[numthreads(64,1,1)]
void main(uint id : SV_DispatchThreadID) {
	bool cond = buf[id] != 0;
	uint4 ballot = WaveActiveBallot(cond);
	uint count = WaveActiveCountBits(cond);
	buf[id] = ballot.x + count + (WaveActiveAnyTrue(cond) ? 1u : 0u);
}
