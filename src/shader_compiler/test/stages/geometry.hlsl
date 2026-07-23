struct GSVertex { float4 pos : SV_Position; };

[[oxc::stage("geometry")]]
[maxvertexcount(3)]
void main(triangle GSVertex input[3], inout TriangleStream<GSVertex> output) {
	[unroll]
	for (int i = 0; i < 3; i++) {
		GSVertex v;
		v.pos = input[i].pos;
		output.Append(v);
	}
	output.RestartStrip();
}
