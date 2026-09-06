#include "@types.hlsli"

// Wave intrinsics: stream compaction without a prefix sum pass. Every lane decides whether to keep
// its element, the wave counts the keepers and hands each one a slot, and one lane per wave reserves
// the wave's block with a single atomic instead of one per element.

StructuredBuffer<F32x4>   _particles;   // xyz position, w remaining lifetime
RWStructuredBuffer<F32x4> _alive;
RWByteAddressBuffer       _aliveCount;

struct Compact {
    U32 count;
};

PUSH_CONSTANT Compact _compact;

[[oxc::model("6.6")]]
[[oxc::extension("SubgroupOperations")]]
[shader("compute")]
[numthreads(64, 1, 1)]
void main(U32 id : SV_DispatchThreadID) {

    // Lanes past the end stay in the wave so the counts below see every lane; they just keep nothing.
    bool keep = false;

    if (id < _compact.count)
        keep = _particles[id].w > 0;

    U32 keptInWave = WaveActiveCountBits(keep);
    U32 slot       = WavePrefixCountBits(keep);

    U32 base = 0;

    if (WaveIsFirstLane() && keptInWave > 0)
        _aliveCount.InterlockedAdd(0, keptInWave, base);

    base = WaveReadLaneFirst(base);

    if (keep)
        _alive[base + slot] = _particles[id];
}
