//A ray generation and a miss entry under a uniform: each row links the library on its own, on both backends, and the
//uniform's value is no entrypoint of it. Driven by test_shader_compiler_stages.c.

struct Payload { float4 color; };

RaytracingAccelerationStructure scene;
RWStructuredBuffer<float4> output;

[[oxc::uniforms(U32 scale = 1)]]
[[oxc::uniforms(U32 scale = 2)]]
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

	float scale = 1;

	#ifdef $$scale
		scale = $$scale;
	#endif

	output[id.y * dim.x + id.x] = payload.color * scale;
}

[[oxc::uniforms(U32 scale = 1)]]
[[oxc::uniforms(U32 scale = 2)]]
[shader("miss")]
void mainMiss(inout Payload payload) {
	payload.color = float4(1, 1, 1, 1);
}
