#include "@extensions.hlsli"

RaytracingAccelerationStructure tlas;
RWStructuredBuffer<float> buf;

struct [raypayload] Payload {
	float t : read(caller) : write(caller, miss, closesthit);
};

//A raytracing library where only the raygen runs an inline ray query against micromap geometry.
//The raygen names OXC_ENABLE_OPACITY_MICROMAP() in its body, so it alone carries the OpacityMicromapIdKHR execution
// mode; the closest hit only declares the capabilities through oxc::EnableOpacityMicromap(), which adds no mode.

float traceInline(float3 origin) {
	RayDesc r; r.Origin = origin; r.Direction = float3(0, 0, -1); r.TMin = 0; r.TMax = 1e30f;
	RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS> q;
	q.TraceRayInline(tlas, RAY_FLAG_NONE, 0xFF, r);
	q.Proceed();
	return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? q.CommittedRayT() : -1;
}

[[oxc::extension("RayQuery", "RayMicromapOpacity")]]
[[oxc::model("6.9")]]
[shader("raygeneration")]
void mainRaygen() {
	OXC_ENABLE_OPACITY_MICROMAP();
	buf[DispatchRaysIndex().x] = traceInline(float3(DispatchRaysIndex().xy, 5));
}

[[oxc::extension("RayQuery", "RayMicromapOpacity")]]
[[oxc::model("6.9")]]
[shader("miss")]
void mainMiss(inout Payload p) {
	p.t = -1;
}

[[oxc::extension("RayQuery", "RayMicromapOpacity")]]
[[oxc::model("6.9")]]
[shader("closesthit")]
void mainClosestHit(inout Payload p, BuiltInTriangleIntersectionAttributes attr) {
	oxc::EnableOpacityMicromap();
	p.t = RayTCurrent();
}
