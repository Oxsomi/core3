//The output UAV is normally bound, so the heap side only reads a texture and a sampler.

RWStructuredBuffer<float4> outBuf : register(u0);

[[oxc::extension("DescriptorHeap")]]
[[oxc::model("6.6")]]
[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main(uint id : SV_DispatchThreadID) {
	Texture2D<float4> tex = ResourceDescriptorHeap[1];
	SamplerState samp = SamplerDescriptorHeap[2];
	outBuf[id] = tex.SampleLevel(samp, float2(0.5, 0.5), 0);
}
