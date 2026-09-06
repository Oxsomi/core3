#include "@types.hlsli"

// A mesh stage replaces the whole vertex chain: this one emits a full-screen quad and the pixel
// stage shades it, which is the smallest complete mesh pipeline.

struct MeshOut {
    F32x4 pos : SV_Position;
    F32x2 uv : TEXCOORD0;
};

[[oxc::stage("mesh")]]
[outputtopology("triangle")]
[numthreads(4, 1, 1)]
void mainMesh(
    U32 tid : SV_GroupThreadID,
    out vertices MeshOut verts[4],
    out indices U32x3 tris[2]
) {
    SetMeshOutputCounts(4, 2);

    const F32x2 uv = F32x2(tid & 1, tid >> 1);
    verts[tid].pos = F32x4(uv * 2 - 1, 0, 1);
    verts[tid].uv = uv;

    if (tid < 2)
        tris[tid] = tid ? U32x3(1, 3, 2) : U32x3(0, 1, 2);
}

[[oxc::stage("pixel")]]
F32x4 mainPixel(MeshOut i) : SV_Target {
    return F32x4(i.uv, 0.5, 1);
}
