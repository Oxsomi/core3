struct VSOut { float3 pos : POSITION; };

struct HSConst {
	float edges[3] : SV_TessFactor;
	float inside   : SV_InsideTessFactor;
};

HSConst patchConst(InputPatch<VSOut, 3> ip) {
	HSConst o;
	o.edges[0] = 1; o.edges[1] = 1; o.edges[2] = 1;
	o.inside = 1;
	return o;
}

[[oxc::stage("hull")]]
[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("patchConst")]
VSOut main(InputPatch<VSOut, 3> ip, uint id : SV_OutputControlPointID) {
	return ip[id];
}
