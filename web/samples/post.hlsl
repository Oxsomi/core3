#include "@types.hlsli"

struct PostCmd {
    F32x3 tint;
    F32   exposure;
};

PUSH_CONSTANT PostCmd cmd;

Texture2D<F32x4> _color;
SamplerState     _sampler;

struct VSOut {
    F32x4 pos : SV_Position;
    F32x2 uv  : TEXCOORD0;
};

[[oxc::stage("vertex")]]
VSOut vsMain(U32 vid : SV_VertexID) {
    VSOut o;
    o.uv  = F32x2((vid << 1) & 2, vid & 2);
    o.pos = F32x4(o.uv * F32x2(2, -2) + F32x2(-1, 1), 0, 1);
    return o;
}

[[oxc::defines("TONEMAP_ACES")]]
[[oxc::defines()]]
[[oxc::stage("pixel")]]
F32x4 psMain(VSOut i) : SV_Target {

    F32x4 c = _color.SampleLevel(_sampler, i.uv, 0);
    c.rgb *= cmd.exposure * cmd.tint;

#ifdef $TONEMAP_ACES
    // ACES filmic curve fit, Narkowicz 2015.
    c.rgb = saturate((c.rgb * (2.51 * c.rgb + 0.03)) / (c.rgb * (2.43 * c.rgb + 0.59) + 0.14));
#endif

    return c;
}
