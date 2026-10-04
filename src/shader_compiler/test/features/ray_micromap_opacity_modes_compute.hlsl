#include "@extensions.hlsli"

RaytracingAccelerationStructure tlas;
RWStructuredBuffer<float> buf;

//Two compute entries of one library, compiled into one module, where only one runs a ray query.
//That one names OXC_ENABLE_OPACITY_MICROMAP() in its body, so it alone carries the OpacityMicromapIdKHR execution mode,
// though the ray query itself sits in a helper.

float traceInline(uint id) {
	RayDesc r; r.Origin = float3(id, 0, 5); r.Direction = float3(0, 0, -1); r.TMin = 0; r.TMax = 1e30f;
	RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS> q;
	q.TraceRayInline(tlas, RAY_FLAG_NONE, 0xFF, r);
	q.Proceed();
	return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? q.CommittedRayT() : -1;
}

[[oxc::extension("RayQuery", "RayMicromapOpacity")]]
[[oxc::model("6.9")]]
[shader("compute")]
[numthreads(64, 1, 1)]
void mainTrace(uint id : SV_DispatchThreadID) {
	OXC_ENABLE_OPACITY_MICROMAP();
	buf[id] = traceInline(id);
}

[[oxc::extension("RayQuery", "RayMicromapOpacity")]]
[[oxc::model("6.9")]]
[shader("compute")]
[numthreads(64, 1, 1)]
void mainClear(uint id : SV_DispatchThreadID) {
	buf[id] = 0;
}
