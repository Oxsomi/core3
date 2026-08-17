#include "@extensions.hlsli"

RaytracingAccelerationStructure tlas;
RWStructuredBuffer<uint> outp;

struct [raypayload] Payload {
	float3 color : read(caller) : write(caller, closesthit, miss);
};

struct Attributes {
	float2 barycentrics;
};

[[oxc::extension("RayReorder", "RayTriPosition", "RayQuery")]]
[[oxc::model("6.10")]]
[shader("raygeneration")]
void main() {

	RayDesc ray; ray.Origin = float3(0, 0, 0); ray.Direction = float3(0, 0, 1); ray.TMin = 0; ray.TMax = 1e30f;

	OXC_RAYPAYLOAD Payload payload;
	OXC_HITOBJECT_ATTRIBUTES Attributes attrs;

	payload.color = float3(0, 0, 0);
	attrs.barycentrics = float2(0, 0);

	OXC_HITOBJECT(hit);
	oxc::HitObject_TraceRay(hit, tlas, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
	oxc::MaybeReorderThread(hit);
	oxc::HitObject_GetAttributes(hit, attrs);
	oxc::HitObject_Invoke(hit, payload);

	uint result =
		(oxc::HitObject_IsHit(hit) ? 1u : 0u) | (oxc::HitObject_IsMiss(hit) ? 2u : 0u) |
		(oxc::HitObject_IsNop(hit) ? 4u : 0u);

	result += oxc::HitObject_GetHitKind(hit) + oxc::HitObject_GetInstanceIndex(hit);
	result += oxc::HitObject_GetInstanceID(hit) + oxc::HitObject_GetGeometryIndex(hit);
	result += oxc::HitObject_GetPrimitiveIndex(hit) + oxc::HitObject_GetRayFlags(hit);
	result += oxc::HitObject_GetShaderTableIndex(hit);
	result += (uint)(oxc::HitObject_GetRayTMin(hit) + oxc::HitObject_GetRayTCurrent(hit));
	float3 v = oxc::HitObject_GetWorldRayOrigin(hit) + oxc::HitObject_GetWorldRayDirection(hit) +
		oxc::HitObject_GetObjectRayOrigin(hit) + oxc::HitObject_GetObjectRayDirection(hit);
	result += (uint)(v.x + v.y + v.z);
	result += (uint)(oxc::HitObject_GetObjectToWorld3x4(hit)[0][0] + oxc::HitObject_GetWorldToObject3x4(hit)[0][0]);
	result += (uint)(payload.color.x + attrs.barycentrics.x);

	//Cross-extension: hit object triangle vertex positions (needs RayReorder + RayTriPosition).
	oxc::TrianglePositions tp = oxc::HitObject_TrianglePositions(hit);
	result += (uint)(tp.p0.x + tp.p1.y + tp.p2.z);

	//Hit objects sourced without a TraceRay: an explicit nop, an explicit miss and an inline ray query
	// converted with FromRayQuery, then retargeted with SetShaderTableIndex.

	OXC_HITOBJECT(nop);
	oxc::HitObject_MakeNop(nop);
	result += oxc::HitObject_IsNop(nop) ? 8u : 0u;

	OXC_HITOBJECT(missed);
	oxc::HitObject_MakeMiss(missed, RAY_FLAG_NONE, 0, ray);
	result += oxc::HitObject_IsMiss(missed) ? 16u : 0u;

	RayQuery<RAY_FLAG_NONE> query;
	query.TraceRayInline(tlas, RAY_FLAG_FORCE_OPAQUE, 0xFF, ray);
	query.Proceed();

	OXC_HITOBJECT_ATTRIBUTES BuiltInTriangleIntersectionAttributes triAttrs;
	triAttrs.barycentrics = float2(0, 0);

	OXC_HITOBJECT(fromQuery);

	if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
		oxc::HitObject_FromRayQuery(fromQuery, query, 1u, triAttrs);

	OXC_HITOBJECT_SET_SHADER_TABLE_INDEX(fromQuery, 2);
	result += oxc::HitObject_GetShaderTableIndex(fromQuery);
	oxc::HitObject_Invoke(fromQuery, payload);
	result += (uint)payload.color.y;

	outp[DispatchRaysIndex().x] = result;
}
