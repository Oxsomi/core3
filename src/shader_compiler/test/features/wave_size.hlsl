//[WaveSize] in its first form (SM6.6): one size the entry requires, which DXIL reflects and SPIR-V can't carry.

RWByteAddressBuffer output;

[[oxc::extension("SubgroupOperations")]]
[[oxc::model("6.6")]]
[[oxc::stage("compute")]]
[numthreads(64, 1, 1)]
[WaveSize(32)]
void main(uint id : SV_DispatchThreadID) {
	output.Store(id * 4, WaveGetLaneCount());
}
