#include "@types.hlsli"
#include "include/light.hlsli"

cbuffer Globals {
    F32x4x4 viewProj;
    F32x3 cameraPos;
    U32 lightCount;
    U32x2 res;
};

StructuredBuffer<Light> _lights;
Texture2D<F32x4> _albedo;
SamplerState _sampler;   // never sampled, so Reflection marks it unused
RWTexture2D<F32x4> _output;

[[oxc::uniforms(U32 MAX_LIGHTS = 64)]]
[shader("compute")]
[numthreads(8, 8, 1)]
void main(U32x3 id : SV_DispatchThreadID) {

    if (any(id.xy >= res))
        return;

    F32x4 c = _albedo.Load(int3(id.xy, 0));

    U32 count = lightCount;
#ifdef $$MAX_LIGHTS
    count = min(count, $$MAX_LIGHTS);
#endif

    // Every light per texel is the example, not the advice; a renderer culls or clusters first.
    for (U32 i = 0; i < count; ++i)
        c.rgb += shadeLight(_lights[i], cameraPos);

    _output[id.xy] = c;
}
