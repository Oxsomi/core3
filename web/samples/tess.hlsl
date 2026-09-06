#include "@types.hlsli"

// Tessellation: vertex and hull carry object space control points, the domain stage interpolates and
// only then projects, so the new vertices land on the surface rather than on a projected triangle.

struct Transform {
    F32x4x4 mvp;
    F32     tess;
};

PUSH_CONSTANT Transform _transform;

struct ControlPoint { F32x3 pos : POSITION; F32x2 uv : TEXCOORD0; };
struct VSOut { F32x4 pos : SV_Position; F32x2 uv : TEXCOORD0; };
struct PatchConst { F32 edges[3] : SV_TessFactor; F32 inner : SV_InsideTessFactor; };

[[oxc::stage("vertex")]]
ControlPoint mainVert(F32x3 pos : POSITION, F32x2 uv : TEXCOORD0) {
    ControlPoint o;
    o.pos = pos;
    o.uv = uv;
    return o;
}

PatchConst constants(InputPatch<ControlPoint, 3> patch) {
    // A factor of 0 or less culls the patch, so a zeroed push constant would draw nothing.
    F32 tess = max(_transform.tess, 1);
    PatchConst c;
    c.edges[0] = c.edges[1] = c.edges[2] = tess;
    c.inner = tess;
    return c;
}

[[oxc::stage("hull")]]
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("constants")]
ControlPoint mainHull(InputPatch<ControlPoint, 3> patch, U32 i : SV_OutputControlPointID) {
    return patch[i];
}

[[oxc::stage("domain")]]
[domain("tri")]
VSOut mainDomain(PatchConst c, F32x3 bary : SV_DomainLocation, const OutputPatch<ControlPoint, 3> patch) {
    F32x3 pos = patch[0].pos * bary.x + patch[1].pos * bary.y + patch[2].pos * bary.z;
    VSOut o;
    o.pos = mul(_transform.mvp, F32x4(pos, 1));
    o.uv = patch[0].uv * bary.x + patch[1].uv * bary.y + patch[2].uv * bary.z;
    return o;
}

[[oxc::stage("pixel")]]
F32x4 mainPixel(VSOut i) : SV_Target { return F32x4(i.uv, 0, 1); }
