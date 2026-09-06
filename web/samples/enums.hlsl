#include "@resources.hlsli"

enum class Mode : U32 { Off = 0, Linear = 1, Cubic = 2 };

typedef F32x4 Color;

struct Material {
    Color albedo;
    F32 roughness;
    U32 flags;
};

StructuredBuffer<Material> _materials;
RWStructuredBuffer<Color> _out;
cbuffer Params { U32 materialCount; U32 modeRaw; };

// Roughness darkens the albedo: not at all, proportionally, or with a cubic falloff.
Color shade(Material m, Mode mode) {

    switch (mode) {
        case Mode::Linear:  return m.albedo * (1 - m.roughness);
        case Mode::Cubic:   return m.albedo * pow(1 - m.roughness, 3);
        default:            return m.albedo;
    }
}

[[oxc::stage("compute")]]
[numthreads(64, 1, 1)]
void mainEnums(U32 i : SV_DispatchThreadID) {

    if (i >= materialCount)
        return;

    _out[i] = shade(_materials[i], (Mode) modeRaw);
}
