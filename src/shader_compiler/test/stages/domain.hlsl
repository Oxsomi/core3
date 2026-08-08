struct ControlPoint { float3 pos : POSITION; };

struct PatchConstant {
	float edges[3] : SV_TessFactor;
	float inside : SV_InsideTessFactor;
};

[[oxc::stage("domain")]]
[domain("tri")]
float4 main(PatchConstant pc, float3 loc : SV_DomainLocation, const OutputPatch<ControlPoint, 3> patch) : SV_Position {
	float3 p = patch[0].pos * loc.x + patch[1].pos * loc.y + patch[2].pos * loc.z;
	return float4(p, 1);
}
