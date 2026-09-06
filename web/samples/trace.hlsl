#include "@types.hlsli"

// A whole ray tracing pipeline in one lib: raygen, closest hit and miss compile to DXR on DXIL and
// SPV_KHR_ray_tracing on SPIR-V, and land in one oiSH with a binary per entrypoint.

struct Payload { F32x3 color; F32 hitT; };

RaytracingAccelerationStructure _tlas;
RWTexture2D<F32x4>              _out;

[shader("miss")]
void miss(inout Payload p) {
    // Sky: the ray direction itself, remapped to a color.
    p.color = WorldRayDirection() * 0.5 + 0.5;
    p.hitT = -1;
}

[shader("closesthit")]
void hit(inout Payload p, BuiltInTriangleIntersectionAttributes attr) {
    // Barycentrics as color: the classic first triangle.
    p.color = F32x3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics);
    p.hitT = RayTCurrent();
}

// Two extension sets on one entry: the lib compiles once per set, so this oiSH carries two rgen
// binaries and deriving a pipeline asks which one with -entry, exactly like the CLI would.
[[oxc::model("6.5")]]
[[oxc::extension("RayQuery")]]
[[oxc::extension()]]
[shader("raygeneration")]
void rgen() {
    U32x2 id = DispatchRaysIndex().xy;
    F32x2 uv = (F32x2(id) + 0.5) / F32x2(DispatchRaysDimensions().xy);

    RayDesc ray;
    ray.Origin = F32x3(uv * 2 - 1, -1);
    ray.Direction = F32x3(0, 0, 1);
    ray.TMin = 0;
    ray.TMax = 1e6;

    Payload p = (Payload)0;
    TraceRay(_tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, p);

    _out[id] = F32x4(p.color, 1);
}
