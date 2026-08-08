RaytracingAccelerationStructure tlas;
RWStructuredBuffer<float> buf;
[[oxc::extension("RayQuery")]]
[[oxc::stage("compute")]]
[numthreads(1,1,1)]
void main(uint id : SV_DispatchThreadID) {
	RayDesc r; r.Origin = float3(0,0,0); r.Direction = float3(0,0,1); r.TMin = 0; r.TMax = 1e30f;
	RayQuery<RAY_FLAG_NONE> q;
	q.TraceRayInline(tlas, 0, 0xFF, r);
	q.Proceed();
	buf[id] = q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? q.CommittedRayT() : -1;
}
