Texture2D<float4> tex;
SamplerState samp;

struct Vertex { float4 pos : SV_Position; };

[[oxc::extension("MeshTaskTexDeriv")]]
[[oxc::model("6.6")]]
[[oxc::stage("mesh")]]
[outputtopology("triangle")]
[numthreads(2, 2, 1)]
void main(out vertices Vertex verts[3], out indices uint3 tris[1]) {
	SetMeshOutputCounts(3, 1);
	verts[0].pos = tex.Sample(samp, float2(0.5, 0.5)); // implicit tex derivative in mesh stage
	verts[1].pos = float4(0, 0, 0, 1);
	verts[2].pos = float4(0, 0, 0, 1);
	tris[0] = uint3(0, 1, 2);
}
