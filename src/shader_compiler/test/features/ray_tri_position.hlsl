#include "@extensions.hlsli"

RaytracingAccelerationStructure tlas;
RWStructuredBuffer<float> buf;

[[oxc::extension("RayQuery", "RayTriPosition")]]
[[oxc::model("6.10")]]
[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main(uint id : SV_DispatchThreadID) {

	RayDesc r; r.Origin = float3(0, 0, 0); r.Direction = float3(0, 0, 1); r.TMin = 0; r.TMax = 1e30f;
	RayQuery<RAY_FLAG_NONE> q;
	q.TraceRayInline(tlas, 0, 0xFF, r);
	q.Proceed();

	oxc::TrianglePositions p = oxc::CommittedTriangleObjectPositions(q);
	buf[id] = q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? p.p0.x + p.p1.y + p.p2.z : -1;
}
