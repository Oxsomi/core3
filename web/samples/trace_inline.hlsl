#include "@types.hlsli"

// Inline ray tracing: a compute shader asks the acceleration structure directly through RayQuery,
// with no ray tracing pipeline, payload or shader table. One shadow ray per texel.

RaytracingAccelerationStructure _tlas;
Texture2D<F32x4>                _position;      // world position per texel, w = 0 where there is none
Texture2D<F32x4>                _geoNormal;     // geometric normal per texel, not the shading one
RWTexture2D<F32>                _shadow;

struct Sun {
    F32x3 dir;
    U32   pad;
    U32x2 res;
};

PUSH_CONSTANT Sun _sun;

// Where a secondary ray starts (RT Gems 1 ch. 6, Waechter and Binder): the origin moves along the
// geometric normal by a fixed number of ULPs of the position itself, so the margin scales with the
// distance from the world origin. A constant offset is too big near the origin and too small far away.
F32x3 offsetRay(F32x3 p, F32x3 geoNormal) {
    
    static const F32 ORIGIN = 1.0 / 32.0;
    static const F32 FLOAT_SCALE = 1.0 / 65536.0;
    static const F32 INT_SCALE = 256.0;

    // Stepping the mantissa away from the surface adds for a positive coordinate and subtracts for a
    // negative one, since the float encoding grows away from zero in both directions.
    const I32x3 ofI = I32x3(geoNormal * INT_SCALE);
    const F32x3 pI = asfloat(asint(p) + select(p < 0, -ofI, ofI));

    // Near the origin the float branch scales with the dominant magnitude, so a near zero component of
    // a hit far along the ray still clears its true error.
    const F32 floatScale = FLOAT_SCALE * max(1.0, max(max(abs(p.x), abs(p.y)), abs(p.z)));
    return select(abs(p) < ORIGIN, p + geoNormal * floatScale, pI);
}

[[oxc::model("6.5")]]
[[oxc::extension("RayQuery")]]
[shader("compute")]
[numthreads(8, 8, 1)]
void main(U32x2 id : SV_DispatchThreadID) {

    if (any(id >= _sun.res))
        return;

    F32x4 p = _position[id];

    if (p.w == 0) {
        _shadow[id] = 1;
        return;
    }

    F32x3 geoNormal = _geoNormal[id].xyz;

    RayDesc ray;
    ray.Origin    = offsetRay(p.xyz, geoNormal);
    ray.Direction = _sun.dir;
    ray.TMin      = 0;
    ray.TMax      = 1e6;

    // Any hit is enough for a shadow, so the query may stop at the first one and never sort.
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_FORCE_OPAQUE> query;
    query.TraceRayInline(_tlas, RAY_FLAG_NONE, 0xFF, ray);
    query.Proceed();

    _shadow[id] = query.CommittedStatus() == COMMITTED_NOTHING ? 1 : 0;
}
