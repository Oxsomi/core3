//[WaveSize] whose arguments are constant expressions rather than literals: DXC evaluates them, so the parse reads the
// sizes they come to, 16 to 64 lanes preferring 32, as for wave_size_range.hlsl.

RWByteAddressBuffer output;

enum EWave { EWave_Small = 16, EWave_Big = 64 };

static const uint PREFERRED = 2 * EWave_Small;

#define TWICE(x) ((x) << 1)

[[oxc::extension("SubgroupOperations")]]
[[oxc::model("6.8")]]
[[oxc::stage("compute")]]
[numthreads(64, 1, 1)]
[wavesize(EWave_Small, TWICE(PREFERRED), PREFERRED)]
void main(uint id : SV_DispatchThreadID) {
	output.Store(id * 4, WaveGetLaneCount());
}
