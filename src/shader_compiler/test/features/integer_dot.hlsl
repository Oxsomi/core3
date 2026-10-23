RWStructuredBuffer<uint> buf;

//Packed 4 x 8 bit integer dot products: DXIL's Dot4AddI8Packed (shader model 6.4), SPIRV's OpSDot over
// PackedVectorFormat4x8Bit plus an integer add (SPV_KHR_integer_dot_product).
//The DotProduct and DotProductInput4x8BitPacked capabilities the SPIR-V declares map back to the shader extension.

[[oxc::extension("IntegerDot4x8")]]
[[oxc::model("6.5")]]
[[oxc::stage("compute")]]
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
	const uint a = buf[id.x * 3], b = buf[id.x * 3 + 1];
	buf[id.x * 3 + 2] = asuint(dot4add_i8packed(a, b, asint(buf[id.x * 3 + 2])));
}
