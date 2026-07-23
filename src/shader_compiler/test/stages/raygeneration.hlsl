struct Payload { float4 color; };

RaytracingAccelerationStructure scene;
RWStructuredBuffer<float4> output;

[shader("raygeneration")]
void main() {
	uint2 id = DispatchRaysIndex().xy;
	uint2 dim = DispatchRaysDimensions().xy;
	RayDesc ray;
	ray.Origin = float3(id, 0);
	ray.Direction = float3(0, 0, 1);
	ray.TMin = 0.0;
	ray.TMax = 1e6;
	Payload payload;
	payload.color = float4(0, 0, 0, 0);
	TraceRay(scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
	output[id.y * dim.x + id.x] = payload.color;
}
