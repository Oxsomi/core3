#include "@types.hlsli"

// Full bindless, SM6.6 style: resources come straight off the descriptor heap by index, no descriptor
// layout at all, through ResourceDescriptorHeap/SamplerDescriptorHeap.
//
// DXIL only for now, hence [[oxc::binary("dxil")]]: the compiler refuses the SPIRV leg of any
// DescriptorHeap shader until DXC's SPV_EXT_descriptor_heap lowering is integrated upstream (see
// Compiler_buildCompileArgs), and the annotation is what lets the DXIL leg compile alone. The annotation comes
// off, and the output UAV can move onto the heap too, once that lands.

RWStructuredBuffer<F32x4> _out : register(u0);

struct Blur {
    U32x2 res;
};

PUSH_CONSTANT Blur _blur;

// $BLUR_TAPS is a preprocessor define: each [[oxc::defines]] annotation is one full variant compile,
// so this oiSH carries a 5-tap and a 9-tap binary of the same entrypoint.

[[oxc::binary("dxil")]]
[[oxc::extension("DescriptorHeap")]]
[[oxc::model("6.6")]]
[[oxc::defines("BLUR_TAPS" = "5")]]
[[oxc::defines("BLUR_TAPS" = "9")]]
[[oxc::stage("compute")]]
[numthreads(8, 8, 1)]
void main(U32x2 id : SV_DispatchThreadID) {

    if (any(id >= _blur.res))
        return;

    Texture2D<F32x4> src = ResourceDescriptorHeap[0];
    SamplerState samp = SamplerDescriptorHeap[0];

    // $BLUR_TAPS only exists per variant, so the annotation-scan pass needs the guarded fallback.
#ifdef $BLUR_TAPS
    static const I32 taps = $BLUR_TAPS;
#else
    static const I32 taps = 5;
#endif

    F32x2 uv = (F32x2(id) + 0.5) / F32x2(_blur.res);
    F32x4 acc = 0;

    [unroll]
    for (I32 i = 0; i < taps; ++i)
        acc += src.SampleLevel(samp, uv + F32x2(i - taps / 2, 0) / F32(_blur.res.x), 0);

    _out[id.y * _blur.res.x + id.x] = acc / taps;
}
