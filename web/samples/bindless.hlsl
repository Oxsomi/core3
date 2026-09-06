#include "@resources.hlsli"
#include "@buffer.hlsli"

// OxC3's bindless model: a resource is a U32 handle in a push constant, resolved through the big
// descriptor arrays @resources.hlsli declares. No per-draw descriptor sets, no root signature juggling:
// the same shader binds anything the frame created, on Vulkan and D3D12 alike.

struct DrawData {
    U32 histogram;     // handle of a RWByteAddressBuffer
    U32 texture;       // handle of a Texture2D
    U32 smp;           // handle of a SamplerState
    U32 binCount;
};

PUSH_CONSTANT DrawData _draw;

// vendor narrows which GPUs may run this binary (metadata, no extra compiles); binary narrows which
// backends it is emitted for, ANDed with -compile-output.
// The bindless sampler array is opt in: it owns a whole descriptor set on SPIR-V and forces a sampler
// heap on both backends, so a shader that indexes samplers dynamically asks for it by name. A sampler
// known at layout time is a static sampler instead and costs nothing.
[[oxc::model("6.6")]]
[[oxc::vendor("NV", "AMD")]]
[[oxc::binary("spv", "dxil")]]
[[oxc::extension("DynamicSamplers")]]
[[oxc::stage("compute")]]
[numthreads(64, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {

    if (i >= _draw.binCount)
        return;

    F32x2 uv = F32x2(F32(i) / F32(_draw.binCount), 0.5);
    F32x4 texel = texture2DUniform(_draw.texture).SampleLevel(samplerUniform(_draw.smp), uv, 0);

    // Luma-keyed histogram. Threads share bins, so the increment has to be atomic.
    U32 bin = min(U32(dot(texel.rgb, F32x3(0.2126, 0.7152, 0.0722)) * F32(_draw.binCount)), _draw.binCount - 1);
    rwBufferUniform(_draw.histogram).InterlockedAdd(bin * 4, 1);
}
