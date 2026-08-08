#include "@extensions.hlsli"

RaytracingAccelerationStructure tlas;
RWStructuredBuffer<uint> outp;

struct [raypayload] Payload {
	float3 color : read(caller) : write(caller, closesthit, miss);
};

struct Attributes {
	float2 barycentrics;
};

[[oxc::extension("RayReorder", "RayTriPosition")]]
[[oxc::model("6.10")]]
[shader("raygeneration")]
void main() {

	RayDesc ray; ray.Origin = float3(0, 0, 0); ray.Direction = float3(0, 0, 1); ray.TMin = 0; ray.TMax = 1e30f;

	OXC_RAYPAYLOAD Payload payload;
	OXC_HITOBJECT_ATTRIBUTES Attributes attrs;

	payload.color = float3(0, 0, 0);
	attrs.barycentrics = float2(0, 0);

	oxc::HitObject hit = oxc::HitObject::TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
	oxc::MaybeReorderThread(hit);
	hit.GetAttributes(attrs);
	oxc::HitObject::Invoke(hit, payload);

	uint result = (hit.IsHit() ? 1u : 0u) | (hit.IsMiss() ? 2u : 0u) | (hit.IsNop() ? 4u : 0u);
	result += hit.GetHitKind() + hit.GetInstanceIndex() + hit.GetInstanceID() + hit.GetGeometryIndex();
	result += hit.GetPrimitiveIndex() + hit.GetRayFlags() + hit.GetShaderTableIndex();
	result += (uint)(hit.GetRayTMin() + hit.GetRayTCurrent());
	float3 v = hit.GetWorldRayOrigin() + hit.GetWorldRayDirection() +
		hit.GetObjectRayOrigin() + hit.GetObjectRayDirection();
	result += (uint)(v.x + v.y + v.z);
	result += (uint)(hit.GetObjectToWorld3x4()[0][0] + hit.GetWorldToObject3x4()[0][0]);
	result += (uint)(payload.color.x + attrs.barycentrics.x);

	//Cross-extension: hit object triangle vertex positions (needs RayReorder + RayTriPosition).
	oxc::TrianglePositions tp = oxc::HitTrianglePositions(hit);
	result += (uint)(tp.p0.x + tp.p1.y + tp.p2.z);

	outp[DispatchRaysIndex().x] = result;
}
