//[WaveSize] in its ranged form (SM6.8): the sizes the entry takes, 16 to 64 lanes, preferring 32.

RWByteAddressBuffer output;

[[oxc::extension("SubgroupOperations")]]
[[oxc::model("6.8")]]
[[oxc::stage("compute")]]
[numthreads(64, 1, 1)]
[WaveSize(16, 64, 32)]
void main(uint id : SV_DispatchThreadID) {
	output.Store(id * 4, WaveGetLaneCount());
}
