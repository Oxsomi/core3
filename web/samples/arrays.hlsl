#include "@resources.hlsli"

// Resource arrays and array indexing: four terrain layers picked per texel by splat-map weights.
// The STOCHASTIC uniform swaps the four-tap blend for one tap chosen by the weights, at the cost of
// a single layer.

Texture2D<F32x4> _layers[4];
Texture2D<F32x4> _splat;
SamplerState _sampler;
RWTexture2D<F32x4> _output;

struct Terrain {
    U32x2 res;
    F32x2 tiling;
};

PUSH_CONSTANT Terrain _terrain;

U32 hash(U32x2 p) {
    U32 h = p.x * 0x8DA6B343 ^ p.y * 0xD8163841;
    h ^= h >> 15;
    h *= 0x2C1B3C6D;
    h ^= h >> 12;
    h *= 0x297A2D39;
    h ^= h >> 15;
    return h;
}

[[oxc::uniforms(B1 STOCHASTIC = false)]]
[[oxc::uniforms(B1 STOCHASTIC = true)]]
[shader("compute")]
[numthreads(8, 8, 1)]
void mainArrays(U32x2 id : SV_DispatchThreadID) {

    if (any(id >= _terrain.res))
        return;

    F32x2 uv = (F32x2(id) + 0.5) / F32x2(_terrain.res);

    F32x4 weights = _splat.SampleLevel(_sampler, uv, 0);
    weights /= max(weights.x + weights.y + weights.z + weights.w, 1e-4);

    F32x2 tiled = uv * _terrain.tiling;
    F32x4 c = 0;

#ifdef $$STOCHASTIC
    if ($$STOCHASTIC) {

        F32 r = hash(id) * (1.0 / 4294967296.0);
        F32x3 cdf = F32x3(weights.x, weights.x + weights.y, weights.x + weights.y + weights.z);
        U32 pick = (U32)(r >= cdf.x) + (U32)(r >= cdf.y) + (U32)(r >= cdf.z);

        c = _layers[NonUniformResourceIndex(pick)].SampleLevel(_sampler, tiled, 0);
    }
    else
#endif
    {
        [unroll]
        for (U32 i = 0; i < 4; ++i)
            c += weights[i] * _layers[i].SampleLevel(_sampler, tiled, 0);
    }

    _output[id] = c;
}
