//A mesh entry and a compute entry written with [shader()] and the same uniform: one library compile holding both, so
//the compute binary is specialized out of a module the mesh entry made SPIR-V 1.4. Driven by test_shader_compiler_stages.c.

struct Vertex { float4 pos : SV_Position; };

RWByteAddressBuffer o : register(u0);

[[oxc::uniforms(U32 scale = 1)]]
[[oxc::uniforms(U32 scale = 2)]]
[shader("mesh")]
[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void mainMesh(out vertices Vertex verts[3], out indices uint3 tris[1]) {

	float scale = 1;

	#ifdef $$scale
		scale = $$scale;
	#endif

	SetMeshOutputCounts(3, 1);
	verts[0].pos = float4(-1, -1, 0, 1) * scale;
	verts[1].pos = float4( 3, -1, 0, 1) * scale;
	verts[2].pos = float4(-1,  3, 0, 1) * scale;
	tris[0] = uint3(0, 1, 2);
}

[[oxc::uniforms(U32 scale = 1)]]
[[oxc::uniforms(U32 scale = 2)]]
[shader("compute")]
[numthreads(1, 1, 1)]
void mainCompute() {

	uint scale = 1;

	#ifdef $$scale
		scale = $$scale;
	#endif

	o.Store(0, scale);
}
