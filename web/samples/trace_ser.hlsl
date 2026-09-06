#include "@types.hlsli"
#include "@extensions.hlsli"

// Shader execution reordering: TraceRay split into record, reorder hint and invoke. Between the
// record and the invoke a device that reorders regroups lanes by what they hit, so the hit shaders
// run on coherent waves; a device that does not simply continues, with the same result.
// SM 6.9 also requires the payload to say who reads and writes each field.

struct [raypayload] Payload {
    F32x3 color : read(caller) : write(closesthit, miss);
};

RaytracingAccelerationStructure _tlas;
RWTexture2D<F32x4>              _out;

[[oxc::model("6.9")]]
[shader("miss")]
void miss(inout Payload p) {
    p.color = WorldRayDirection() * 0.5 + 0.5;
}

[[oxc::model("6.9")]]
[shader("closesthit")]
void hit(inout Payload p, BuiltInTriangleIntersectionAttributes attr) {
    p.color = F32x3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics);
}

// Miss and closest hit compile without RayReorder, where the hit object types do not exist, so the
// raygen vanishes from their parse; the annotation pass sees every extension and still registers it.
#ifdef __OXC_EXT_RAYREORDER

[[oxc::model("6.9")]]
[[oxc::extension("RayReorder")]]
[shader("raygeneration")]
void rgen() {

    U32x2 id = DispatchRaysIndex().xy;
    F32x2 uv = (F32x2(id) + 0.5) / F32x2(DispatchRaysDimensions().xy);

    RayDesc ray;
    ray.Origin    = F32x3(uv * 2 - 1, -1);
    ray.Direction = F32x3(0, 0, 1);
    ray.TMin      = 0;
    ray.TMax      = 1e6;

    OXC_RAYPAYLOAD Payload p;
    p.color = 0;

    OXC_HITOBJECT(hitObj);
    oxc::HitObject_TraceRay(hitObj, _tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, p);
    oxc::MaybeReorderThread(hitObj);
    oxc::HitObject_Invoke(hitObj, p);

    _out[id] = F32x4(p.color, 1);
}

#endif
