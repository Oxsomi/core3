struct Vertex { float4 pos : SV_Position; };

[[oxc::stage("mesh")]]
[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void main(out vertices Vertex verts[3], out indices uint3 tris[1]) {
	SetMeshOutputCounts(3, 1);
	verts[0].pos = float4(-1, -1, 0, 1);
	verts[1].pos = float4( 3, -1, 0, 1);
	verts[2].pos = float4(-1,  3, 0, 1);
	tris[0] = uint3(0, 1, 2);
}
