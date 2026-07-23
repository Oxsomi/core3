RWStructuredBuffer<float> _buf;

[[oxc::extension("ComputeDeriv")]]
[[oxc::model("6.6")]]
[[oxc::stage("compute")]]
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
	float v = (float)id.x * (float)id.y;
	float d = ddx(v) + ddy(v);
	_buf[id.x + id.y * 8] = d;
}
