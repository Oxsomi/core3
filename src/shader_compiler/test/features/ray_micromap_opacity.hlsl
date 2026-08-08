#include "@extensions.hlsli"

RaytracingAccelerationStructure tlas;
RWStructuredBuffer<float> buf;

[[oxc::extension("RayQuery", "RayMicromapOpacity")]]
[[oxc::model("6.9")]]
[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main(uint id : SV_DispatchThreadID) {

	oxc::EnableOpacityMicromap();

	RayDesc r; r.Origin = float3(0, 0, 0); r.Direction = float3(0, 0, 1); r.TMin = 0; r.TMax = 1e30f;
	RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS> q;
	q.TraceRayInline(tlas, RAY_FLAG_NONE, 0xFF, r);
	q.Proceed();
	buf[id] = q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? q.CommittedRayT() : -1;
}
