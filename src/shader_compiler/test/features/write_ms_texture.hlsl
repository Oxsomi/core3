RWTexture2DMS<float4> outMS;

[[oxc::extension("WriteMSTexture")]]
[[oxc::model("6.7")]]
[[oxc::stage("compute")]]
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
	outMS.sample[0][id.xy] = float4(id.x, id.y, 0, 1);
}
