RWStructuredBuffer<uint> buf;

//Declares the packed 4 x 8 bit integer dot products but never calls one.
//The SPIR-V then has no DotProduct capability, so its reflection demotes the extension to dormant; DXIL can't tell.

[[oxc::extension("IntegerDot4x8")]]
[[oxc::model("6.5")]]
[[oxc::stage("compute")]]
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
	buf[id.x * 3 + 2] = buf[id.x * 3] + buf[id.x * 3 + 1];
}
